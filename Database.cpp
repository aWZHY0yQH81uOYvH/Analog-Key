#include "Database.hpp"

#include <cstdlib>

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
