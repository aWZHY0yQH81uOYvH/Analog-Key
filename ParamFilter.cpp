#include "ParamFilter.hpp"

#include "Filters/SIValue.hpp"
#include "Filters/Range.hpp"

#include <cassert>

const std::map<std::string, std::shared_ptr<ParamFilter>> ParamFilter::all_filters {
	{"si",    std::make_shared<FilterSI>()   },
	{"range", std::make_shared<FilterRange>()}
};

bool ParamFilter::render_options() {
	return false;
}

bool ParamFilter::render_list() {
	return false;
}

std::string ParamFilter::type2str(param_type type) {
	switch(type) {
		case TYPE_INT:
			return "INTEGER";
		case TYPE_REAL:
			return "REAL";
		case TYPE_TEXT:
			return "TEXT";
		default:
			assert(0);
	}
}

ParamFilter::param_type ParamFilter::str2type(const std::string &str) {
	static const std::map<std::string, param_type> m{
		{"INTEGER", TYPE_INT},
		{"REAL",    TYPE_REAL},
		{"TEXT",    TYPE_TEXT}
	};
	
	auto it = m.find(str);
	assert(it != m.end());
	return it->second;
}
