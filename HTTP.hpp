#pragma once

#include <curl_easy.h>
#include <sstream>
#include <nlohmann/json.hpp>
#include <memory>

class HTTP: private std::ostringstream, private curl::curl_ios<std::ostringstream>, public curl::curl_easy {
public:
	HTTP(): curl::curl_ios<std::ostringstream>(static_cast<std::ostringstream&>(*this)), curl::curl_easy(static_cast<curl::curl_ios<std::ostringstream>&>(*this)) {}
	
	std::string response() const {
		return this->str();
	}
	
	nlohmann::json response_json() const {
		return nlohmann::json::parse(response());
	}
};

using http_t = std::shared_ptr<HTTP>;
