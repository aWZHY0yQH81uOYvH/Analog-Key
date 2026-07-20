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
	
	using parse_variant = std::variant<int, float, std::string>;
	virtual std::vector<std::optional<parse_variant>> parse(const std::string &val) const = 0;
	
	const std::vector<std::pair<std::string, param_type>> columns;
	
	static const std::map<std::string, std::unique_ptr<ParamFilter>> all_filters;
	
	static std::string type2str(param_type type);
	static param_type str2type(const std::string &str);
};
