#pragma once
#include <thread>
#include <functional>
#include <iostream>
#include <queue>
#include <atomic>
#include <mutex>

class ThreadPool {
	int numThreads;
	std::vector<std::thread> threads;
	std::queue<std::function<void()>> tasks;
	std::mutex mtx;
	std::mutex idle;
	std::condition_variable condition;
	std::atomic<int> numTask = 0;

	bool shutdown = false;

public:

	ThreadPool(int numThreads) :numThreads(numThreads) {
		for (int i = 0; i < numThreads; i++) {
			threads.emplace_back([this, i] {
				while (true) {
					std::unique_lock<std::mutex> lock(mtx);

					condition.wait(lock, [this] {return !tasks.empty() || shutdown; });

					if (shutdown)break;

					auto task(std::move(tasks.front()));
					tasks.pop();
					lock.unlock();

					task();

					numTask--;
				}
				});
		}
	}
	~ThreadPool() {
		mtx.lock();
		shutdown = true;
		mtx.unlock();

		condition.notify_all();
		for (int i = 0; i < numThreads; i++) {
			threads[i].join();
		}
	}

	template<class F, class ...Args>
	void addTask(F&& f, Args&&... args) {
		std::function<void()>task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

		mtx.lock();
		numTask++;
		tasks.emplace(std::move(task));
		mtx.unlock();

		condition.notify_one();
	}
	void barrier() {
		while (numTask > 0) {
			idle.lock();
			idle.unlock();
		}
	}
};

