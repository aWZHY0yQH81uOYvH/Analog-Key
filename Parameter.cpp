#include "Parameter.hpp"

#include <iostream>
#include <format>
#include <cassert>

using nlohmann::json;

std::multimap<std::string, SpecialParameter> SpecialParameter::sp_list{
	{"compare",       {-1, "DKPN",         "/value/productNumber"_json_pointer           }},
	{"productDetail", {-2, "MPN",          "/value/productNumber"_json_pointer           }},
	{"productDetail", {-3, "Manufacturer", "/value/manufacturer/value/label"_json_pointer}},
	{"productDetail", {-4, "Datasheet",    "/value/datasheetUrl"_json_pointer            }},
	{"productDetail", {-5, "Description",  "/value/description"_json_pointer             }},
	{"productDetail", {-6, "Photo",        "/value/image/thumb"_json_pointer             }},
	{"productDetail", {-7, "URL",          "/value/detailUrl"_json_pointer               }},
	{"qtyAvailable",  {-8, "Available",    "/value/0/quantity"_json_pointer              }}
};

Parameter::Parameter(int id, std::string name): name(name), id(id) {}

int Parameter::insert(std::shared_ptr<Database> db, const json &j, std::optional<int> part_id) const {
	SQLite::Statement insert{*db, std::format("INSERT OR REPLACE INTO {} (part_id, val) VALUES (?, ?) RETURNING part_id;", id_to_table(id))};
	if(part_id) insert.bind(1, part_id.value());
	else insert.bind(1);
		
	auto parsed = parse(j);
	insert.bind(2, parsed);
	
	insert.executeStep();
	int ret = insert.getColumn(0);
	insert.reset();
	return ret;
}

void Parameter::update_filters_from_db(std::shared_ptr<Database> db) {
	SQLite::Statement stmt{*db, "SELECT filters FROM parameters WHERE id == ?;"};
	stmt.bind(1, id);
	if(stmt.executeStep()) {
		json j = json::parse(stmt.getColumn(0).getString());
		for(auto &filter:j) {
			auto filter_it = ParamFilter::all_filters.find(filter);
			if(filter_it != ParamFilter::all_filters.end())
				filters.emplace_back(filter_it->second->clone());
		}
	}
}

void Parameter::reprocess_parameters(std::shared_ptr<Database> db) {
	const std::string table = id_to_table(id);
	SQLite::Statement query{*db, std::format("SELECT part_id, val FROM {};", table)};
	filters.clear();
	
	// Count how many values each filter can successfully parse
	std::vector<int> success(ParamFilter::all_filters.size(), 0);
	int count = 0;
	const int limit = 10000;
	for(auto &&row:query) {
		std::string value = row.getColumn(1);
		
		// Don't count rows with '-' for or against
		if(value == "-")
			continue;
		
		if(count++ >= limit)
			break;
		
		for(int i = 0; auto &[_, filter]:ParamFilter::all_filters) {
			// If any result column has a value, consider it a successful parse
			for(auto &result:filter->parse(value))
				if(result.has_value()) {
					success[i]++;
					break;
				}
			i++;
		}
	}
	query.reset();
	
	if(count == 0) return;
	
	// Determine what columns we already have
	std::string query_cols;
	std::vector<std::string> relevant_filters;
	std::map<std::string, ParamFilter::param_type> columns;
	for(auto &&row:SQLite::Statement{*db, std::format("SELECT name, type FROM pragma_table_info('{}');", table)})
		columns.emplace(row.getColumn(0), ParamFilter::str2type(row.getColumn(1)));
	
	// For each filter that successfully parses this data, create its relevant columns
	const float threshold = 0.5;
	for(int i = 0; auto &[name, filter]:ParamFilter::all_filters) {
		// Skip filters that don't parse this data well
		if(success[i++] < count * threshold)
			continue;
		
		// Make list of relevant filters for saving in parameters table
		relevant_filters.push_back(name);
		
		// Prepend filter name to its required column names
		auto required_cols = filter->columns;
		for(auto &col:required_cols)
			col.first = name + "_" + col.first;
		
		// Create required columns
		for(auto &col:required_cols) {
			// Build insert statement strings
			if(!query_cols.empty())
				query_cols += ',';
			query_cols += col.first + "=?";
			
			// Check if column exists
			auto col_it = columns.find(col.first);
			bool exists = (col_it != columns.end());
			
			// Check if the type has changed
			if(exists && col_it->second != col.second) {
				db->exec(std::format("ALTER TABLE {} DROP COLUMN {};", table, col.first));
				exists = false;
			}
			
			if(!exists) {
				columns[col.first] = col.second;
				db->exec(std::format("ALTER TABLE {} ADD COLUMN {} {};", table, col.first, ParamFilter::type2str(col.second)));
			}
		}
		
		filters.emplace_back(filter->clone());
	}
	
	// Save filters in parameters table
	SQLite::Statement relevant_filters_update{*db, "UPDATE parameters SET filters=? WHERE id=?;"};
	relevant_filters_update.bind(1, json(relevant_filters).dump());
	relevant_filters_update.bind(2, id);
	relevant_filters_update.exec();
	
	if(query_cols.empty())
		return;
	
	// Parse all data
	SQLite::Statement insert_stmt{*db, std::format("UPDATE {} SET {} WHERE part_id=?;", table, query_cols)};
	for(auto &&row:query) {
		int id = row.getColumn(0);
		std::string value = row.getColumn(1);
		
		int col_idx = 1;
		for(auto &filter:filters) {
			for(size_t i = 0; i < filter->columns.size(); i++) {
				auto results = filter->parse(value);
				if(i < results.size() && results[i].has_value()) {
					auto &result = results[i].value();
					switch(filter->columns[i].second) {
						case ParamFilter::TYPE_INT:
							insert_stmt.bind(col_idx, std::get<int>(result));
							break;
						case ParamFilter::TYPE_REAL:
							insert_stmt.bind(col_idx, std::get<double>(result));
							break;
						case ParamFilter::TYPE_TEXT:
							insert_stmt.bind(col_idx, std::get<std::string>(result));
							break;
						default:
							insert_stmt.bind(col_idx);
							assert(0);
					}
				} else insert_stmt.bind(col_idx);
				col_idx++;
			}
		}
		
		insert_stmt.bind(col_idx, id);
		insert_stmt.exec();
		insert_stmt.reset();
	}
}

std::string Parameter::parse(const nlohmann::json &j) const {
	return j.at("value").at("value").get<std::string>();
}

void Parameter::load(std::shared_ptr<Database> db, const nlohmann::json &product, std::map<int, Parameter> &param_list) {
	SQLite::Statement check{*db, "SELECT name FROM parameters WHERE id == ?;"};
	
	const int part_id = get_part_id(product);
	if(part_id < 0) return;
	
	for(auto &section:product) {
		const int id = std::atoi(section.at("id").get<std::string>().c_str());
		if(id < 0)
			continue;
		
		auto param_it = param_list.find(id);
		if(param_it == param_list.end()) {
			std::string name;
			
			// Check if parameter is in database
			check.bind(1, id);
			if(check.executeStep())
				name = check.getColumn(0).getString();
			else create_parameter_table(db, id);
			check.reset();
			
			param_it = param_list.emplace(id, Parameter{id, name}).first;
			param_it->second.update_filters_from_db(db);
		}
		
		param_it->second.insert(db, section, part_id);
	}
}

int Parameter::get_part_id(const json &product) {
	try {
		for(auto &node:product)
			if(node.at("type") == "productDetail")
				return std::atoi(node.at("value").at("productId").get<std::string>().c_str());
	} catch(...) {}
	std::cout << "Product does not have ID!\n";
	std::cout << product.dump(2) << std::endl;
	return -1;
}

void Parameter::create_parameter_table(std::shared_ptr<Database> db, int id, std::string name) {
	// Create table
	db->exec(std::format(
	R"(CREATE TABLE {} (
		part_id       INTEGER PRIMARY KEY,
		val           TEXT
	);)", id_to_table(id)));
	
	// Add to global table of parameters
	SQLite::Statement insert_param{*db, "INSERT INTO parameters (id, name) VALUES (?, ?);"};
	insert_param.bind(1, id);
	insert_param.bind(2, name);
	insert_param.exec();
	insert_param.reset();
}

SpecialParameter::SpecialParameter(int id, std::string name, accessor_t accessor): Parameter(id, name), accessor(accessor) {}

void SpecialParameter::gen_tables(std::shared_ptr<Database> db, std::map<int, Parameter> &param_list) {
	for(auto &sp_pair:sp_list) {
		auto &sp = sp_pair.second;
		param_list.emplace(sp.id, sp);
		create_parameter_table(db, sp.id, sp.name);
	}
}

void SpecialParameter::load(std::shared_ptr<Database> db, const json &product) {
	const int part_id = get_part_id(product);
	if(part_id < 0) return;
	for(auto &section:product) {
		if(std::atoi(section.at("id").get<std::string>().c_str()) < 0) {
			auto matches = sp_list.equal_range(section.at("type"));
			if(matches.first != sp_list.end())
				for(auto it = matches.first; it != matches.second; it++)
					it->second.insert(db, section, part_id);
		}
	}
}

std::string SpecialParameter::parse(const nlohmann::json &j) const {
	if(auto *ptr = std::get_if<json::json_pointer>(&accessor)) {
		auto &jj = j.at(*ptr);
		return jj.get<std::string>();
	}
	else return std::get<accessor_func>(accessor).operator()(j);
}

std::string Parameter::id_to_table(int id) {
	if(id < 0) return std::format("param_sp{}", -id);
	else return std::format("param_{}", id);
}
