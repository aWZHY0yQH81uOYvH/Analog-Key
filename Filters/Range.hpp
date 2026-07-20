#pragma once

#include "SIValue.hpp"

struct FilterRange: public FilterSI {
	FilterRange();
	PARAMFILTER_CLONE(FilterRange);
	virtual parse_list parse(const std::string &val) const override;
	
private:
	std::regex range_re;
};
