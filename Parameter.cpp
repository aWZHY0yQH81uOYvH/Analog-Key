#include "Parameter.hpp"

#include <iostream>
#include <format>

using nlohmann::json;

// First MUST be MPN
// Second MUST be manufacturer
std::vector<SpecialParameter> SpecialParameter::sp_list{
	{"MPN",                  TYPE_TEXT, "/ManufacturerProductNumber"_json_pointer      },
	{"Manufacturer",         TYPE_INT,  "/Manufacturer/Id"_json_pointer                },
	{"Category",             TYPE_INT,  parse_category                                 },
	{"Datasheet",            TYPE_TEXT, "/DatasheetUrl"_json_pointer                   },
	{"Description",          TYPE_TEXT, "/Description/ProductDescription"_json_pointer },
	{"Detailed Description", TYPE_TEXT, "/Description/DetailedDescription"_json_pointer},
	{"Normally Stocking",    TYPE_BOOL, "/NormallyStocking"_json_pointer               },
	{"Photo",                TYPE_TEXT, "/PhotoUrl"_json_pointer                       },
	{"Status",               TYPE_TEXT, "/ProductStatus/Status"_json_pointer           },
	{"URL",                  TYPE_TEXT, "/ProductUrl"_json_pointer                     },
	{"Available",            TYPE_INT,  "/QuantityAvailable"_json_pointer              },
	{"Series",               TYPE_TEXT, "/Series/Name"_json_pointer                    }
};

Parameter::Parameter(std::string name, param_type type): type(type), name(name) {}

int Parameter::insert(std::shared_ptr<Database> db, const json &j, int ind, std::optional<int> id) const {
	SQLite::Statement insert{*db, std::format("INSERT OR REPLACE INTO {} (part_id, val) VALUES (?, ?) RETURNING part_id;", id_to_table(ind))};
	if(id) insert.bind(1, id.value());
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
	return insert.getColumn(0);
}

int Parameter::get_part_id(std::shared_ptr<Database> db, const json &j) {
	// Check if this component already has an ID
	// Check MPN and manufacturer match (first two special parameters)
	auto table_mpn = id_to_table(SpecialParameter::i_to_id(0));
	auto table_mfr = id_to_table(SpecialParameter::i_to_id(1));
	SQLite::Statement check_exists{*db, std::format(
	R"(SELECT sp1.part_id
		FROM {} AS sp1
		JOIN {} AS sp2
		ON sp1.part_id = sp2.part_id
		WHERE sp1.val = ?
		  AND sp2.val = ?
	;)", table_mpn, table_mpn)};
	
	std::string mpn;
	check_exists.bind(1, mpn = std::get<std::string>(SpecialParameter::sp_list[0].parse(j)));
	check_exists.bind(2,       std::get<int>        (SpecialParameter::sp_list[1].parse(j)));
	int id = -1;
	int count = 0;
	for(auto &&row:check_exists) {
		id = row.getColumn(0);
		count++;
	}
	
	if(count > 1)
		std::cout << std::format("Warning: multiple matches for part number {}\n", mpn);
	
	if(count > 0)
		return id;
	
	// Part does not exist, add it to MPN and manufacturer tables
	id = SpecialParameter::sp_list[0].insert(db, j, SpecialParameter::i_to_id(0));
	     SpecialParameter::sp_list[1].insert(db, j, SpecialParameter::i_to_id(1));
	return id;
}

SpecialParameter::SpecialParameter(std::string name, param_type type, accessor_t accessor): Parameter(name, type), accessor(accessor) {}

void SpecialParameter::gen_tables(std::shared_ptr<Database> db) {
	SQLite::Statement insert_param{*db, "INSERT INTO parameters (id, name) VALUES (?, ?);"};
	
	for(int i = 0; i < (int)sp_list.size(); i++) {
		auto &sp = sp_list[i];
		// Special parameters have negative IDs
		db->exec(std::format(
		R"(CREATE TABLE {} (
			part_id       INTEGER PRIMARY KEY,
			val           TEXT
		);)", id_to_table(i_to_id(i))));
		
		// Add to global table of parameters
		insert_param.bind(1, i_to_id(i));
		insert_param.bind(2, sp.name);
		insert_param.exec();
		insert_param.reset();
	}
}

void SpecialParameter::load(std::shared_ptr<Database> db, const json &j) {
	// If not already in first two special parameter tables, it will be added by this call
	const int id = get_part_id(db, j);
	for(int i = 2; i < (int)sp_list.size(); i++)
		sp_list[i].insert(db, j, i_to_id(i), id);
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

Parameter::parse_variant SpecialParameter::parse_category(const json &j) {
	// Find highest-depth category
	auto category = j["Category"];
	while(category["ChildCategories"].size()) {
		if(category["ChildCategories"].size() > 1)
			std::cerr << std::format("Warning: Part {} is in multiple categories\n", (std::string)j["ManufacturerProductNumber"]);
		category = category["ChildCategories"][0];
	}
	return category["CategoryId"].get<int>();
}

int SpecialParameter::i_to_id(int i) {
	return -i-1;
}

std::string Parameter::id_to_table(int id) {
	if(id < 0) return std::format("param_sp{}", -id);
	else return std::format("param_{}", id);
}
