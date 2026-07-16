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
#include <map>

class Database: public SQLite::Database {
public:
	Database(std::shared_ptr<ThreadPool> pool, std::filesystem::path path = {});
	
	// Update display data if needed
	void check_update();
	void update_filters(ThreadPool::Thread &t);
	
	// Reprocess all collected search API json responses and update internal db
	void reprocess_api_search_results(long since = 0);
	
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
	// Add a single API response to the internal db
	void process_api_search_result(const nlohmann::json &j);

	std::shared_ptr<ThreadPool> pool;
	
private:
	static std::filesystem::path get_path(std::filesystem::path path);
};
