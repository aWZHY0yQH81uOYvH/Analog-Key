#pragma once

#include "HTTP.hpp"
#include "AuthManager.hpp"
#include "ThreadPool.hpp"
#include "Database.hpp"

#include <memory>

class DigiKey {
public:
	DigiKey(std::shared_ptr<ThreadPool> pool, std::shared_ptr<Database> db);
	
	void update_categories();
	
protected:
	AuthManager auth;
	
	std::shared_ptr<ThreadPool> pool;
	std::shared_ptr<Database> db;
};
