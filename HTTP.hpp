#pragma once

#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <stdexcept>

class HTTP {
public:
	HTTP();
	~HTTP();
	
	enum method_t {
		GET,
		POST
	};
	
	struct rate_limit_error: public std::runtime_error {
		using std::runtime_error::runtime_error;
	};
	
	std::string request(method_t method, std::string url, std::string body = {}, std::vector<std::string> headers = {}, bool fight_rate_limit = true);
	nlohmann::json request_json(method_t method, std::string url, std::string body = {}, std::vector<std::string> headers = {}, bool fight_rate_limit = true);
	
	std::string response() const;
	nlohmann::json response_json() const;
	
	std::string escape(const std::string &in);
	
	CURL *curl;
	
private:
	std::string buffer;
	
	static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
};

using http_t = std::shared_ptr<HTTP>;
