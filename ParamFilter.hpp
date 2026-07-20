#pragma once

#include <map>
#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <utility>
#include <variant>

// Parse parameter text into useful data to be filtered
struct ParamFilter {
	enum param_type {
		TYPE_INT,
		TYPE_REAL,
		TYPE_TEXT
	};
	
	virtual ~ParamFilter() = default;
	
	// Make copy that can have settings
	virtual ParamFilter *clone() const = 0;
	#define PARAMFILTER_CLONE(name) virtual ParamFilter *clone() const override {return new name{*this};}
	
	// Parse some data
	using parse_variant = std::variant<int, double, std::string>;
	using parse_list = std::vector<std::optional<parse_variant>>;
	virtual parse_list parse(const std::string &val) const = 0;
	
	// List of database columns for this filter
	std::vector<std::pair<std::string, param_type>> columns;
	
	// Render this filter's options
	virtual bool render_options();
	
	// Take over rendering of this list
	virtual bool render_list(/* TODO */);
	
	static const std::map<std::string, std::shared_ptr<ParamFilter>> all_filters;
	
	static std::string type2str(param_type type);
	static param_type str2type(const std::string &str);
};
