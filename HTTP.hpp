#pragma once

#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <memory>
#include <string>
#include <vector>
#include <atomic>

class HTTP {
public:
	HTTP();
	~HTTP();
	
	enum method_t {
		GET,
		POST
	};
	
	std::string request(method_t method, std::string url, std::string body = {}, std::vector<std::string> headers = {});
	nlohmann::json request_json(method_t method, std::string url, std::string body = {}, std::vector<std::string> headers = {});
	
	std::string response() const;
	nlohmann::json response_json() const;
	
	std::string escape(const std::string &in);
	
	CURL *curl;
	
private:
	std::string buffer;
	
	static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);
};

using http_t = std::shared_ptr<HTTP>;
