#include <mutex>
#include <condition_variable>
#include <deque>

template<typename T>
class ts_queue {
public:
	using value_type = T;
private:
	std::deque<T> m_data;
	mutable std::mutex m_mutex;
	mutable std::condition_variable m_cond;
public:
	ts_queue() = default;
	ts_queue(size_t sz) : m_data(sz) {}
	ts_queue(const ts_queue&) = delete;
	ts_queue(ts_queue&& rhs) = delete;
	ts_queue operator=(const ts_queue&) = delete;
	ts_queue& operator=(ts_queue&& rhs) = delete;
	~ts_queue() = default;
public:
	void push(const T& val) {		
		std::lock_guard lk{ m_mutex };
		m_data.emplace_back(val);		
		m_cond.notify_one();
	}
	
    bool front(T& obj) const {
		std::lock_guard lk{ m_mutex };
		if (m_data.empty())
			return false;
		obj = m_data.front();
		return true;
	}

	bool try_pop(T& obj) {
		std::lock_guard lk{ m_mutex };
		if (m_data.empty()) return false;
		obj = m_data.front();
		m_data.pop_front();
		return true;
	}

	T wait_and_pop() {
		std::unique_lock lk{m_mutex};
		m_cond.wait(lk, [this]() { return !m_data.empty(); });
		T tmp = m_data.front();
		m_data.pop_front();
		return tmp;
	}

	bool empty() const {
		std::lock_guard lk{ m_mutex };
		return m_data.empty();
	}

	size_t size() const {
		std::lock_guard lk{ m_mutex };
		return m_data.size();
	}
};

#include <iostream>
#include <chrono>
#include <thread>
#include <format>
#include <vector>

constexpr size_t THREAD_COUNT = 4;
constexpr size_t ITER_COUNT = 1000000;
constexpr bool QUEUE_WAIT = false; // use ts_queue::wait_and_pop or ts_queue::try_pop

std::mutex gmut;
size_t THREADS_ACTIVE{};

void set_active(size_t v) {
	std::lock_guard lk{ gmut };
	THREADS_ACTIVE = v;
}

void dec_active() {
	std::lock_guard lk{ gmut };
	THREADS_ACTIVE -= 1;
}

size_t get_active() {
	std::lock_guard lk{ gmut };
	return THREADS_ACTIVE;
}

template<typename T>
void produce(ts_queue<T>& ts) {
	for (size_t i = 0; i < ITER_COUNT / THREAD_COUNT; i++) {
		ts.push(i);
	}
	dec_active();
}

template<typename T>
void consume(ts_queue<T>& ts) {
	for (size_t i = 0; i < ITER_COUNT / THREAD_COUNT;) {
		if constexpr (QUEUE_WAIT) {
			ts.wait_and_pop();
			i++;
		} else {
			T val;
			if (ts.try_pop(val)) i++;
		}
	}
	dec_active();
}


int main() {
	
	std::cout << std::format("Hardware concurrency: {}\n", std::thread::hardware_concurrency());

	using Type = int;
	ts_queue<Type> q{};

	while (true) {

		set_active(THREAD_COUNT * 2);

		std::vector<std::thread> threads;

		auto start = std::chrono::steady_clock::now();

		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads.emplace_back(produce<Type>, std::ref(q));
			threads.emplace_back(consume<Type>, std::ref(q));
		}

		for (auto&& t : threads)
			t.detach();

		while (get_active() != 0) std::this_thread::yield();

		auto end = std::chrono::steady_clock::now();

		auto time_ns = end - start;
		auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_ns);


		std::cout << "Lost " << q.size() << " tasks for " << time_ms.count() << " ms" << std::endl;

	}

	return 0;
}
