#include "ThreadPool.hpp"
#include "Database.hpp"
#include "DigiKey.hpp"
#include "GUI.hpp"

#include <iostream>
#include <memory>

int main() {
	curl_global_init(CURL_GLOBAL_DEFAULT);

	auto pool = std::make_shared<ThreadPool>(4);
	auto db   = std::make_shared<Database>(pool);
	auto slow = std::make_shared<DigiKey>(pool, db);
	auto gui  = std::make_shared<GUI>(db, slow);
	
	gui->run();
	pool->await_jobs();

	curl_global_cleanup();
}
