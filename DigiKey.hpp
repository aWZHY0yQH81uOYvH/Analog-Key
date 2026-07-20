#pragma once

#include "HTTP.hpp"
#include "Auth.hpp"
#include "ThreadPool.hpp"
#include "Database.hpp"
#include "Parameter.hpp"

#include <memory>
#include <limits>
#include <map>

class DigiKey {
public:
	DigiKey(std::shared_ptr<ThreadPool> pool, std::shared_ptr<Database> db);
	
	// Update list of categories from DigiKey API
	void update_categories();
	
	// Download all component information from a certain category from the DigiKey API
	void update_category(int id, int start = 0, int stop = std::numeric_limits<int>::max());
	
	// Reprocess all collected json responses and update internal db
	void reprocess_api_search_results(long since = 0);
	
	// Remove all cached json search responses
	void clear_api_cache();
	
protected:
	// Add a single API response to the internal db
	// Caller must already have a lock on the db
	void process_api_search_result(const nlohmann::json &j);

	Auth auth;
	
	std::map<int, Parameter> parameters;
	
	std::shared_ptr<ThreadPool> pool;
	std::shared_ptr<Database> db;
};
