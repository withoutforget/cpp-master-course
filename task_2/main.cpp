#include <iostream>
#include <format>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

// unbounded lock-free MPMC stack
// TODO: add atomic counter for stack's size
class lf_stack {
private:
	struct Node {
		std::shared_ptr<Node> next;
		int value;
	};
	std::atomic<std::shared_ptr<Node>> m_head;
public:
	lf_stack() {}

	void push(int value) {	
		std::shared_ptr<Node> old = m_head.load();
		std::shared_ptr<Node> tmp;
		do {
			tmp = std::make_shared<Node>(old, value);
		} while (!m_head.compare_exchange_weak(old, tmp));
	}

	bool pop(int& value) {
		std::shared_ptr<Node> node = m_head.load();
		while (node != nullptr && !m_head.compare_exchange_weak(node, node->next));
		if (node == nullptr) return false;
		value = node->value;
		return true;
	}

	size_t size() const {
		size_t s = 0;
		std::shared_ptr<Node> head = m_head.load();
		while (head != nullptr) { head = head->next; s++; }
		return s;
	}
};

constexpr size_t ITER_COUNT = 1000;
constexpr size_t THREAD_COUNT = 2;

void producer(lf_stack& stack, std::atomic<size_t>& counter) try {
	for (size_t i = 0; i < ITER_COUNT / THREAD_COUNT; i++)
		stack.push(1);
	counter.fetch_add(1);
}
catch (std::exception& e) {
	std::cout << "Caught exception: " << e.what() << std::endl;
}

void consumer(lf_stack& stack, std::atomic<size_t>& counter) try {
	for (size_t i = 0; i < ITER_COUNT / THREAD_COUNT;)
	{
		int tmp;
		if (stack.pop(tmp)) i++;
	}
	counter.fetch_add(1);
}
catch (std::exception& e) {
	std::cout << "Caught exception: " << e.what() << std::endl;
}

int main() {
	while (true) {
		lf_stack stack;
		std::atomic<size_t> counter;
		
		std::vector<std::thread> threads;

		auto start = std::chrono::steady_clock::now();

		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads.emplace_back(producer, std::ref(stack), std::ref(counter));
			threads.emplace_back(consumer, std::ref(stack), std::ref(counter));
		}

		for (auto&& thread : threads)
			thread.detach();
		while (counter.load() != THREAD_COUNT * 2)
			std::this_thread::yield();

		auto end = std::chrono::steady_clock::now();

		auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		std::cout << std::format("Lost {} tasks for {} ms ", stack.size(), time_ms) << std::endl;
		if (stack.size() != 0)
			break;
	}

	return 0;
}