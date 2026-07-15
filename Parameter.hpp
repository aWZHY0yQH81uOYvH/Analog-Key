#pragma once

#include "Database.hpp"

#include <nlohmann/json.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

#include <string>
#include <functional>
#include <variant>
#include <vector>
#include <memory>
#include <optional>

// A parameter that can be filtered by
struct Parameter {
	enum param_type {
		TYPE_BOOL,
		TYPE_INT,
		TYPE_REAL,
		TYPE_TEXT
	};
	
	Parameter(std::string name, param_type type);
	virtual ~Parameter() = default;
	
	param_type type;
	using parse_variant = std::variant<int, float, std::string>;
	
	virtual parse_variant parse(const nlohmann::json &j) const = 0;
	virtual int insert(std::shared_ptr<Database> db, const nlohmann::json &j, int ind, std::optional<int> id = {}) const;
	
	const std::string name;
	
	static int get_part_id(std::shared_ptr<Database> db, const nlohmann::json &j);
	
	static std::string id_to_table(int id);
};

// A parameter that DigiKey doesn't call a parameter but is a parameter
struct SpecialParameter: public Parameter {
	using accessor_func = std::function<parse_variant(const nlohmann::json&)>;
	using accessor_t = std::variant<nlohmann::json::json_pointer, accessor_func>;
	const accessor_t accessor;
	
	SpecialParameter(std::string name, param_type type, accessor_t accessor);
	
	virtual parse_variant parse(const nlohmann::json &j) const override;
	
	// Generate database tables for special parameters
	static void gen_tables(std::shared_ptr<Database> db);
	
	// Load special parameters into db
	static void load(std::shared_ptr<Database> db, const nlohmann::json &j);
	
	// Index in sp_list to parameter number
	static int i_to_id(int i);
	
private:
	// Special function for parsing category
	static parse_variant parse_category(const nlohmann::json &j);
	
	static std::vector<SpecialParameter> sp_list;
	
	friend struct Parameter;
};
