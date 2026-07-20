#pragma once

#include "Database.hpp"
#include "ParamFilter.hpp"

#include <nlohmann/json.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

#include <string>
#include <functional>
#include <variant>
#include <map>
#include <memory>
#include <optional>
#include <vector>

// A parameter that can be filtered by
struct Parameter {
	Parameter(int id, std::string name);
	virtual ~Parameter() = default;
	
	virtual std::string parse(const nlohmann::json &j) const;
	
	virtual int insert(std::shared_ptr<Database> db, const nlohmann::json &j, std::optional<int> part_id = {}) const;
	
	void update_filters_from_db(std::shared_ptr<Database> db);
	void reprocess_parameters(std::shared_ptr<Database> db);
	
	std::string name;
	int id;
	std::vector<const ParamFilter*> filters;
	
	// Load normal parameters into db
	// Maintain list of parameter objects
	static void load(std::shared_ptr<Database> db, const nlohmann::json &j, std::map<int, Parameter> &param_list);
	
	static int get_part_id(const nlohmann::json &j);
	static void create_parameter_table(std::shared_ptr<Database> db, int id, std::string name = {});
	
	static std::string id_to_table(int id);
};

// A parameter that DigiKey doesn't call a parameter but is a parameter
struct SpecialParameter: public Parameter {
	using accessor_func = std::function<std::string(const nlohmann::json&)>;
	using accessor_t = std::variant<nlohmann::json::json_pointer, accessor_func>;
	const accessor_t accessor;
	
	SpecialParameter(int id, std::string name, accessor_t accessor);
	
	virtual std::string parse(const nlohmann::json &j) const override;
	
	// Generate database tables for special parameters
	static void gen_tables(std::shared_ptr<Database> db, std::map<int, Parameter> &param_list);
	
	// Load special parameters into db
	static void load(std::shared_ptr<Database> db, const nlohmann::json &j);
	
private:
	static std::multimap<std::string, SpecialParameter> sp_list;
	
	friend struct Parameter;
};
