#pragma once

#include "HTTP.hpp"
#include "Auth.hpp"
#include "ThreadPool.hpp"
#include "Database.hpp"

#include <memory>

class DigiKey {
public:
	DigiKey(std::shared_ptr<ThreadPool> pool, std::shared_ptr<Database> db);
	
	void update_categories();
	void load_categories();
	
protected:
	Auth auth;
	
	std::shared_ptr<ThreadPool> pool;
	std::shared_ptr<Database> db;
};
