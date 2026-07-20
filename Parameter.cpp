#include "Parameter.hpp"

#include <iostream>
#include <format>
#include <cassert>

using nlohmann::json;

std::multimap<std::string, SpecialParameter> SpecialParameter::sp_list{
	{"compare",       {-1, "DKPN",         TYPE_TEXT, "/value/productNumber"_json_pointer           }},
	{"productDetail", {-2, "MPN",          TYPE_TEXT, "/value/productNumber"_json_pointer           }},
	{"productDetail", {-3, "Manufacturer", TYPE_TEXT, "/value/manufacturer/value/label"_json_pointer}},
	{"productDetail", {-4, "Datasheet",    TYPE_TEXT, "/value/datasheetUrl"_json_pointer            }},
	{"productDetail", {-5, "Description",  TYPE_TEXT, "/value/description"_json_pointer             }},
	{"productDetail", {-6, "Photo",        TYPE_TEXT, "/value/image/thumb"_json_pointer             }},
	{"productDetail", {-7, "URL",          TYPE_TEXT, "/value/detailUrl"_json_pointer               }},
	{"qtyAvailable",  {-8, "Available",    TYPE_TEXT, "/value/0/quantity"_json_pointer              }}
};

Parameter::Parameter(int id, std::string name, param_type type): type(type), name(name), id(id) {}

int Parameter::insert(std::shared_ptr<Database> db, const json &j, std::optional<int> part_id) const {
	SQLite::Statement insert{*db, std::format("INSERT OR REPLACE INTO {} (part_id, val) VALUES (?, ?) RETURNING part_id;", id_to_table(id))};
	if(part_id) insert.bind(1, part_id.value());
	else insert.bind(1);
		
	auto parsed = parse(j);
	switch(type) {
		case TYPE_BOOL:
		case TYPE_INT:
			insert.bind(2, std::get<int>(parsed));
			break;
		case TYPE_REAL:
			insert.bind(2, std::get<float>(parsed));
			break;
		case TYPE_TEXT:
			insert.bind(2, std::get<std::string>(parsed));
			break;
	};
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
		for(auto &filter:j)
			; // TODO: something with filter names
	}
}

Parameter::parse_variant Parameter::parse(const nlohmann::json &j) const {
	assert(type == TYPE_TEXT);
	return j["value"]["value"].get<std::string>();
}

void Parameter::load(std::shared_ptr<Database> db, const nlohmann::json &product, std::map<int, Parameter> &param_list) {
	SQLite::Statement check{*db, "SELECT name FROM parameters WHERE id == ?;"};
	
	const int part_id = get_part_id(product);
	
	for(auto &section:product) {
		const int id = std::atoi(section["id"].get<std::string>().c_str());
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
			
			param_it = param_list.emplace(id, Parameter{id, name, Parameter::TYPE_TEXT}).first;
			param_it->second.update_filters_from_db(db);
		}
		
		param_it->second.insert(db, section, part_id);
	}
}

int Parameter::get_part_id(const json &product) {
	try {
		for(auto &node:product)
			if(node["type"] == "productDetail")
				return std::atoi(node["value"]["productId"].get<std::string>().c_str());
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

SpecialParameter::SpecialParameter(int id, std::string name, param_type type, accessor_t accessor): Parameter(id, name, type), accessor(accessor) {}

void SpecialParameter::gen_tables(std::shared_ptr<Database> db) {
	for(auto &sp_pair:sp_list) {
		auto &sp = sp_pair.second;
		create_parameter_table(db, sp.id, sp.name);
	}
}

void SpecialParameter::load(std::shared_ptr<Database> db, const json &product) {
	const int part_id = get_part_id(product);
	for(auto &section:product) {
		if(std::atoi(section["id"].get<std::string>().c_str()) < 0) {
			auto matches = sp_list.equal_range(section["type"]);
			if(matches.first != sp_list.end())
				for(auto it = matches.first; it != matches.second; it++)
					it->second.insert(db, section, part_id);
		}
	}
}

Parameter::parse_variant SpecialParameter::parse(const nlohmann::json &j) const {
	if(auto *ptr = std::get_if<json::json_pointer>(&accessor)) {
		auto &jj = j[*ptr];
		switch(type) {
			case TYPE_BOOL:
				return jj.get<bool>();
			case TYPE_INT:
				return jj.get<int>();
			case TYPE_REAL:
				return jj.get<float>();
			case TYPE_TEXT:
				return jj.get<std::string>();
		}
	}
	else return std::get<accessor_func>(accessor).operator()(j);
}

std::string Parameter::id_to_table(int id) {
	if(id < 0) return std::format("param_sp{}", -id);
	else return std::format("param_{}", id);
}
