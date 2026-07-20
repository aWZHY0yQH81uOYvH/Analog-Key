#include "Range.hpp"

FilterRange::FilterRange(): range_re{
	"^(.+)\\s*[-~]\\s*(.+)"
} {
	columns = {
		{"min", TYPE_REAL},
		{"max", TYPE_REAL}
	};
}

FilterRange::parse_list FilterRange::parse(const std::string &val) const {
	std::smatch m;
	if(std::regex_search(val, m, range_re)) {
		auto val1 = parse_si(m[1]);
		auto val2 = parse_si(m[2]);
		if(val1.has_value() && val2.has_value())
			return {val1, val2};
	}
	return {};
}
