#include "ThreadPool.hpp"

#include <iostream>
#include <chrono>

ThreadPool::ThreadPool(int n) {
	threads.resize(n);
	for(auto &thread:threads) {
		thread.http = std::make_shared<HTTP>();
		static_cast<std::jthread&>(thread) = std::jthread([&](std::stop_token stop) {
			Thread &self = thread;
			
			while(!stop.stop_requested()) {
				job_t job;
				{
					std::unique_lock<std::mutex> lock{mutex};
					cond.wait(lock, [&] {return !jobs.empty() || stop.stop_requested();});
					if(stop.stop_requested()) return;
					if(!jobs.empty()) {
						job = std::move(*jobs.begin());
						jobs.pop_front();
					} else continue;
				}
				
				job(self);
				
				thread.active_job.clear();
				thread.status.clear();
			}
		});
	}
}

ThreadPool::~ThreadPool() {
	for(auto &thread:threads)
		thread.request_stop();
	cond.notify_all();
}

void ThreadPool::run(job_t job) {
	{
		std::unique_lock<std::mutex> lock{mutex};
		jobs.push_back(job);
	}
	cond.notify_one();
}

void ThreadPool::run(std::function<void(void)> job) {
	run([job = std::move(job)](Thread&) {
		job();
	});
}

void ThreadPool::await_jobs() {
	while(true) {
		{
			std::unique_lock lock{mutex};
			if(jobs.empty())
				break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
