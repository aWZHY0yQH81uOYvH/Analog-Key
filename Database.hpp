#pragma once

#include "ThreadPool.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <filesystem>
#include <memory>
#include <mutex>

class Database: public SQLite::Database {
public:
	Database(std::shared_ptr<ThreadPool> pool, std::filesystem::path path = {});
	
	std::mutex mutex;
	
protected:
	std::shared_ptr<ThreadPool> pool;
	
private:
	static std::filesystem::path get_path(std::filesystem::path path);
};
