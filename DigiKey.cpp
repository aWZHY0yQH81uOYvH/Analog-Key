#include "DigiKey.hpp"

#include <stdexcept>
#include <iostream>

using nlohmann::json;

DigiKey::DigiKey(std::shared_ptr<ThreadPool> pool, std::shared_ptr<Database> db): pool(pool), db(db) {}

void DigiKey::update_categories() {
	pool->run([&](ThreadPool::Thread &t) {
		t.job = "Update categories";
		try {
			t.status = "Querying API";
			auto j = t.http->request_json(HTTP::GET, "https://api.digikey.com/products/v4/search/categories", "", auth.get_headers(t.http));
			
			t.status = "Updating database";
			std::unique_lock<std::mutex> lock(db->mutex);
			SQLite::Transaction transaction(*db);
			
			db->exec("DROP TABLE IF EXISTS categories;");
			db->exec(
			R"(CREATE TABLE categories (
				id            INTEGER PRIMARY KEY,
				parent        INTEGER,
				name          TEXT NOT NULL,
				product_count INTEGER,
				depth         INTEGER,
				display_order INTEGER,
				last_updated  INTEGER
			);)");
			
			SQLite::Statement insert{*db, "INSERT INTO categories (id, parent, name, product_count, depth, display_order) VALUES (?, ?, ?, ?, ?, ?)"};
			
			int count = 0;
			std::function<void(json,int)> traverse = [&](json j, int depth) {
				for(auto &category:j) {
					insert.bind(1, category["CategoryId"].get<int>());
					insert.bind(2, category["ParentId"].get<int>());
					insert.bind(3, category["Name"].get<std::string>());
					insert.bind(4, category["ProductCount"].get<int>());
					insert.bind(5, depth);
					insert.bind(6, count++);
					insert.exec();
					insert.reset();
					
					traverse(category["Children"], depth + 1);
				}
			};
			
			traverse(j["Categories"], 0);
			
			transaction.commit();
			db->updated = true;
		} catch(std::exception &e) {
			std::cerr << e.what() << std::endl;
		}
	});
}
