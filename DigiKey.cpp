#include "DigiKey.hpp"
#include "Parameter.hpp"

#include <stdexcept>
#include <iostream>
#include <cstdlib>

using nlohmann::json;

DigiKey::DigiKey(std::shared_ptr<ThreadPool> pool, std::shared_ptr<Database> db): pool(pool), db(db) {
	// Load list of parameters from db
	if(db->tableExists("parameters"))
		for(auto &&row:SQLite::Statement{*db, "SELECT id, name FROM parameters;"}) {
			const int id = row.getColumn(0);
			const std::string name = row.getColumn(1);
			parameters.emplace(id, Parameter{id, name, Parameter::TYPE_TEXT});
		}
}

void DigiKey::update_categories() {
	pool->run([&](ThreadPool::Thread &t) {
		t.job = "Update categories";
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
	});
}

void DigiKey::update_category(int id, int start, int stop) {
	pool->run([&, id, start, stop](ThreadPool::Thread &t) {
		int product_count = 0;
		int offset = start;
		const int limit = 50;
		
		{
			std::unique_lock<std::mutex> lock(db->mutex);
			// SQLite::Statement query{*db, "SELECT name, product_count FROM categories WHERE id = ?"};
			// query.bind(1, id);
			// query.executeStep();
			// std::string name = query.getColumn(0);
			// product_count = query.getColumn(1);
			// t.job = std::format("Updating category \"{}\"", name);
			
			db->exec(
			R"(CREATE TABLE IF NOT EXISTS keywordsearch_json (
				id INTEGER PRIMARY KEY,
				time INTEGER,
				json TEXT
			);)");
		}
		
		if(stop - start < product_count)
			product_count = stop - start;
		
		const char *cookie = std::getenv("DIGIKEY_COOKIES");

		while(true) {
			// t.status = std::format("Querying API ({}/{} parts)", offset - start, product_count);
			std::cout << std::format("Part index {}\n", offset);
			
			const int nparts = 100;
			auto api_query = json::parse(std::format("{{\"5\":{{\"p\":{},\"pp\":{}}}}}", offset / nparts + 1, nparts));
			
			std::string url = std::format("https://www.digikey.com/products/api/v5/filter-page/{}?s={}", id, t.http->lzuri(api_query));
			
			std::vector<std::string> headers{
				"Accept: application/json",
				"Authorization: Bearer",
				"Sec-Fetch-Site: same-origin",
				"Accept-Language: en-US,en;q=0.9",
				"Cache-Control: no-cache",
				"Sec-Fetch-Mode: cors",
				"Accept-Encoding: gzip, deflate, br",
				"Referer: https://www.digikey.com",
				"User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.5 Safari/605.1.15",
				"Sec-Fetch-Dest: empty",
				"x-currency: USD",
				"Priority: u=3, i",
				"lang: en",
				"site: us"
			};
			
			if(cookie)
				headers.push_back(std::format("Cookie: {}", cookie));
			
			auto j = t.http->request_json(HTTP::GET, url, "", headers);
			
			std::unique_lock<std::mutex> lock(db->mutex);
			SQLite::Transaction transaction(*db);
			SQLite::Statement insert{*db, "INSERT INTO keywordsearch_json (time, json) VALUES (?, ?);"};

			insert.bind(1, std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
			insert.bind(2, j.dump());
			insert.exec();
			
			// process_api_search_result(j);
			transaction.commit();
			
			offset += nparts;
			if(!nparts || nparts < limit || offset > stop)
				break;
		}
	});
}

void DigiKey::reprocess_api_search_results(long since) {
	pool->run([&, since](ThreadPool::Thread &t) {
		t.job = "Processing API results";
		
		std::unique_lock<std::mutex> lock(db->mutex);
		SQLite::Transaction transaction(*db);
		
		// Delete all existing parameter tables
		std::vector<int> table_ids;
		if(db->tableExists("parameters"))
			for(auto &&row:SQLite::Statement{*db, "SELECT id FROM parameters;"})
				table_ids.push_back(row.getColumn(0));
		for(int id:table_ids)
			db->exec(std::format("DROP TABLE IF EXISTS {};", Parameter::id_to_table(id)));
		
		// Re-create parameter index table
		db->exec("DROP TABLE IF EXISTS parameters;");
		db->exec(
		R"(CREATE TABLE parameters (
			id            INTEGER PRIMARY KEY,
			name          TEXT NOT NULL,
			filters       TEXT DEFAULT '{}'
		);)");
		parameters.clear();
		
		// Generate all special parameter tables
		SpecialParameter::gen_tables(db);
		
		// Load all previous json
		SQLite::Statement query{*db, "SELECT json FROM keywordsearch_json WHERE time > ?"};
		query.bind(1, (int64_t)since);
		
		for(auto &&row:query) {
			auto j = json::parse(row.getColumn(0).getString());
			process_api_search_result(j);
		}
		
		transaction.commit();
	});
	
	// TODO: determine and run parsers
}

void DigiKey::process_api_search_result(const json &j) {
	for(auto &product:j["data"]["products"]) {
		// Load special parameters
		SpecialParameter::load(db, product);
		
		// Load normal parameters
		Parameter::load(db, product, parameters);
	}
	
	// Update parameter names
	std::map<int, std::string> filter_names;
	auto parse_filter_names = [&](const json &j) {
		for(auto &filter:j)
			filter_names[std::atoi(filter["key"].get<std::string>().c_str())] = filter["label"];
	};
	
	parse_filter_names(j["data"]["commonFilters"]);
	parse_filter_names(j["data"]["filters"]);
	
	SQLite::Statement update{*db, "UPDATE parameters SET name = ? WHERE id = ?;"};
	
	for(auto &&row:SQLite::Statement{*db, "SELECT id FROM parameters;"}) {
		const int id = row.getColumn(0);
		
		// Don't use DigiKey's names for special parameters (they're different)
		if(id < 0)
			continue;
		
		auto name_it = filter_names.find(id);
		if(name_it == filter_names.end())
			continue;
		
		const auto &name = name_it->second;
		update.bind(1, name);
		update.bind(2, id);
		update.exec();
		update.reset();
		auto param_it = parameters.find(id);
		if(param_it != parameters.end())
			param_it->second.name = name;
	}
}

void DigiKey::clear_api_cache() {
	pool->run([&] {
		std::unique_lock<std::mutex> lock(db->mutex);
		db->exec("DELETE FROM keywordsearch_json;");
	});
}

/*

table of json shit
	autoincrement id, time, text
	
for each parameter (including mpn, manufacturer, tariff, etc)
	part id, text value (other stuff is shit), post processed cols
	
dkpn table
	each dkpn gets unique id
	dkpn id, dkpn, part id
pricing table
	dkpn id, moq, unitprice
	
mfr table
	mfr id, name
packaging table
	package id, name
	
parameter table
	parameter id (in name of parameter tables), name, parser/filter objects
	include hardcoded values (negative?) for stuff dk doesn't call parameters
category table (already have)
	category id, list of parameter ids

image cache

*/

