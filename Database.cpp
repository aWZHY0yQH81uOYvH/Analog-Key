#include "Database.hpp"

#include <cstdlib>
#include <functional>
#include <format>

using nlohmann::json;

Database::Database(std::shared_ptr<ThreadPool> pool, std::filesystem::path path): SQLite::Database(get_path(path), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE), pool(pool) {}

std::filesystem::path Database::get_path(std::filesystem::path path) {
	if(path.empty()) {
		const char *home = std::getenv("HOME");
		if(!home) throw std::runtime_error("$HOME not set");
		
		path = home;
		path /= ".analog_key.db";
	}
	
	return path;
}

void Database::check_update() {
	if(updated) {
		bool update_queued_local = update_queued.exchange(true);
		if(!update_queued_local)
			pool->run(std::bind_front(&Database::update_filters, this));
	}
}

void Database::update_filters(ThreadPool::Thread &t) {
	t.job = "Apply filters";
	
	std::lock_guard<std::mutex> lock{mutex};
	auto start = clock::now();
	
	auto &data = display_data[!buffer];
	
	// Update category information
	if(!tableExists("categories"))
		return;
	
	data.categories.clear();
	for(auto &&row:SQLite::Statement{*this,
		R"(SELECT id, parent, name, product_count, depth
			FROM categories
			ORDER BY display_order
		;)"}) {
		
		long product_count = row.getColumn(3).getInt64();
		std::string name = std::format("{} ({})", row.getColumn(2).getString(), product_count);
		
		data.categories.push_back(Category{
			.id            = row.getColumn(0),
			.parent_id     = row.getColumn(1),
			.name          = name,
			.product_count = product_count,
			.depth         = row.getColumn(4)
		});
	}
	
	auto end = clock::now();
	data.update_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	
	std::unique_lock<std::mutex> display_lock{display_mutex};
	buffer ^= 1;
}
