#pragma once

#include "HTTP.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <mutex>
#include <filesystem>

class AuthManager {
public:
	AuthManager();

	std::string get_access_token(http_t http);
	std::vector<std::string> get_headers(http_t http);

private:
	void authorize(http_t http);
	void refresh(http_t http);
	void post_token(http_t http, const std::string &body);
	void save_to_disk() const;
	void load_from_disk();

	std::mutex mutex;
	
	std::filesystem::path json_path;

	bool tokens_valid = false;
	std::string client_id;
	std::string client_secret;
	std::string access_token;
	std::string refresh_token;
	std::chrono::system_clock::time_point expires_at;
};
