#pragma once

#include "ParamFilter.hpp"

#include <regex>
#include <optional>

struct FilterSI: public ParamFilter {
	FilterSI();
	PARAMFILTER_CLONE(FilterSI);
	virtual parse_list parse(const std::string &val) const override;
	
protected:
	std::optional<double> parse_si(const std::string &val) const;
	static std::optional<double> char2magnitude(const std::string &c);
	
private:
	std::regex si_re;
};
