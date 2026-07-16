#pragma once

#include "HTTP.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <string>
#include <memory>
#include <atomic>

class ThreadPool {
public:
	ThreadPool(int n = 4);
	~ThreadPool();
	
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool &operator=(const ThreadPool&) = delete;
	
	struct Thread: public std::jthread {
		http_t http;
		
		std::string job;
		std::string status;
	};
	
	using job_t = std::function<void(Thread&)>;
	
	void run(job_t job);
	void run(std::function<void(void)>);
	
	void await_jobs();
	
	std::deque<Thread> threads;
	
protected:
	std::mutex mutex;
	std::condition_variable cond;
	std::deque<job_t> jobs;
	std::atomic<int> active_jobs;
};
