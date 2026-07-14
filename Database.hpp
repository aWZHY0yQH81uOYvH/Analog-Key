#pragma once

#include "ThreadPool.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <array>
#include <string>
#include <deque>

class Database: public SQLite::Database {
public:
	Database(std::shared_ptr<ThreadPool> pool, std::filesystem::path path = {});
	
	void check_update();
	void update_filters(ThreadPool::Thread &t);
	
	using clock = std::chrono::steady_clock;
	
	struct Category {
		int id = -1;
		int parent_id;
		std::string name;
		long product_count;
		int depth;
	};
	
	struct DisplayData {
		std::deque<Category> categories;
		unsigned long update_time_ms = 0;
	};
	
	std::mutex display_mutex;
	std::array<DisplayData, 2> display_data;
	std::atomic<char> buffer = 0;
	
	std::mutex mutex;
	std::atomic<bool> updated = true;
	std::atomic<bool> update_queued = false;
	
protected:
	std::shared_ptr<ThreadPool> pool;
	
private:
	static std::filesystem::path get_path(std::filesystem::path path);
};
