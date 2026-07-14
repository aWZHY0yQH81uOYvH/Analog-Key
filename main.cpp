#include "DigiKey.hpp"
#include "ThreadPool.hpp"
#include "Database.hpp"

#include <iostream>
#include <memory>

int main() {
	curl_global_init(CURL_GLOBAL_DEFAULT);

	auto pool = std::make_shared<ThreadPool>(1);
	auto db   = std::make_shared<Database>(pool);
	DigiKey slow{pool, db};
	
	slow.update_categories();
	
	pool->await_jobs();

	curl_global_cleanup();
}
