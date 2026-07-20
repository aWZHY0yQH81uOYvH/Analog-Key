#include "SIValue.hpp"

#include <cstdlib>

FilterSI::FilterSI(): si_re{
	"^([-0-9.]+)\\s*([YZEPTGMkhdcmunpfazy]|\u00B5|\u03BC)?(?:.*?\\/([YZEPTGMkhdcmunpfazy]|\u00B5|\u03BC))?"
} {
	columns = {
		{"val", TYPE_REAL}
	};
}

FilterSI::parse_list FilterSI::parse(const std::string &val) const {
	return {parse_si(val)};
}

std::optional<double> FilterSI::parse_si(const std::string &val) const {
	std::smatch m;
	
	if(std::regex_search(val, m, si_re)) {
		double value = std::atof(m[1].str().c_str());
		auto prefix1 = char2magnitude(m[2].str());
		auto prefix2 = m[3].matched ? char2magnitude(m[3].str()) : std::nullopt;
		
		if(prefix1.has_value()) {
			value *= prefix1.value();
			if(prefix2.has_value())
				value /= prefix2.value();
		}
		return value;
	}
	return std::nullopt;
}

std::optional<double> FilterSI::char2magnitude(const std::string &c) {
	if(c == "\u00B5" || c == "\u03BC") return 1e-6f;
	if(c.size() != 1) return std::nullopt;
	switch(c[0]) {
		case 'Y': return 1e24f;
		case 'Z': return 1e21f;
		case 'E': return 1e18f;
		case 'P': return 1e15f;
		case 'T': return 1e12f;
		case 'G': return 1e9f;
		case 'M': return 1e6f;
		case 'k': return 1e3f;
		case 'h': return 1e2f;
		case 'd': return 1e-1f;
		case 'c': return 1e-2f;
		case 'm': return 1e-3f;
		case 'u': return 1e-6f;
		case 'n': return 1e-9f;
		case 'p': return 1e-12f;
		case 'f': return 1e-15f;
		case 'a': return 1e-18f;
		case 'z': return 1e-21f;
		case 'y': return 1e-24f;
		default:  return std::nullopt;
    }
}
