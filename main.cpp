#include "ThreadPool.hpp"
#include "Database.hpp"
#include "DigiKey.hpp"
#include "GUI.hpp"

#include <iostream>
#include <memory>

int main() {
	curl_global_init(CURL_GLOBAL_DEFAULT);

	int threads = 32;

	auto pool = std::make_shared<ThreadPool>(threads);
	auto db   = std::make_shared<Database>(pool);
	auto slow = std::make_shared<DigiKey>(pool, db);
// 	auto gui  = std::make_shared<GUI>(db, slow);
	
// 	gui->run();
// 	slow->update_category(402); // "foil connectors" only 33 items
// 	slow->update_category(687, 0, 50); // opamps
	// slow->reprocess_api_search_results();

	std::vector<int> checkpoint = {
		22100,
		54902,
		92677,
		135425,
		168227,
		206002,
		243777,
		281552,
		319327,
		361575,
		399450,
		436925,
		470427,
		512575,
		545977,
		583752,
		621527,
		663275,
		697077,
		734852,
		776100,
		813975,
		848177,
		885952,
		923727,
		971175,
		1013750,
		1037052,
		1074827,
		1112602,
		1150377,
		1188152
	};

	int n = 1208800;
	int n_per = n/threads;
	for(int i = 0; i < threads; i++) {
		// slow->update_category(52, i*n_per, (i+1)*n_per);
		slow->update_category(52, checkpoint[i], (i+1)*n_per);
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}

	pool->await_jobs();

	curl_global_cleanup();
}
