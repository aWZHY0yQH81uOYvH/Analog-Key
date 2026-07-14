#include "AuthManager.hpp"

#include <nlohmann/json.hpp>
#include <curl_easy.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <format>
#include <cassert>

using nlohmann::json;
namespace fs = std::filesystem;

AuthManager::AuthManager() {
	const char *home = std::getenv("HOME");
	if(!home) throw std::runtime_error("$HOME not set");
	
	json_path = home;
	json_path /= ".analog_key_tokens.json";
}

void AuthManager::post_token(http_t http, const std::string &body) {
	http->add<CURLOPT_URL>("https://api.digikey.com/v1/oauth2/token");
	http->add<CURLOPT_POST>(1L);
	http->add<CURLOPT_POSTFIELDS>(body.c_str());
	http->add<CURLOPT_TIMEOUT>(15L);
	http->perform();
	
	long code = http->get_info<CURLINFO_RESPONSE_CODE>().get();
	if(code < 200 || code >= 300)
		throw std::runtime_error(std::format("Received response code {} during auth: {}\n", code, http->response()));
	
	json j = http->response_json();
	
	access_token = j.at("access_token").get<std::string>();
	refresh_token = j.at("refresh_token").get<std::string>();
	http->escape(access_token);
	http->escape(refresh_token);

	const int expires_in = j.at("expires_in").get<int>();
	expires_at = std::chrono::system_clock::now() +
	             std::chrono::seconds(std::max(0, expires_in - 60));
	
	tokens_valid = true;
	save_to_disk();
}

void AuthManager::authorize(http_t http) {
	std::cout << "Perform Digi-Key API authorization\n";
	
	const char *client_id_env = std::getenv("DIGIKEY_CLIENT_ID");
	if(client_id_env)
		client_id = client_id_env;
	else {
		std::cout << "\tEnter Digi-Key client ID: ";
		std::cin >> client_id;
	}
	http->escape(client_id);
	
	const char *client_secret_env = std::getenv("DIGIKEY_CLIENT_SECRET");
	if(client_secret_env)
		client_secret = client_secret_env;
	else {
		std::cout << "\tEnter Digi-Key client secret: ";
		std::cin >> client_secret;
	}
	http->escape(client_secret);
	
	std::string redirect_uri = "https://localhost";
	http->escape(redirect_uri);
	
	std::string auth_url = std::format("https://api.digikey.com/v1/oauth2/authorize?response_type=code&client_id={}&redirect_uri={}",
		client_id,
		redirect_uri
	);
	
	std::cout << std::format("Go to this URL in a browser and paste the ?code=... value here\n\tURL: {}\n\tCode: ", auth_url);
	std::string code;
	std::cin >> code;
	http->escape(code);
	
	std::string body = std::format("code={}&client_id={}&client_secret={}&redirect_uri={}&grant_type=authorization_code",
		code,
		client_id,
		client_secret,
		redirect_uri
	);
	
	post_token(http, body);
}

void AuthManager::refresh(http_t http) {
	assert(tokens_valid);
	
	std::string body = std::format("client_id={}&client_secret={}&refresh_token={}&grant_type=refresh_token",
		client_id,
		client_secret,
		refresh_token
	);
	
	post_token(http, body);
}

std::string AuthManager::get_access_token(http_t http) {
	std::unique_lock<std::mutex> lock{mutex};

	// Try to load from disk
	if(!tokens_valid)
		load_from_disk();
	
	// If that didn't work, get the user to help create a new token set
	if(!tokens_valid)
		authorize(http);
	
	if(std::chrono::system_clock::now() >= expires_at)
		refresh(http);
	
	return "Bearer " + access_token;
}

void AuthManager::save_to_disk() const {
	if(!tokens_valid) return;

	json j;
	j["client_id"] = client_id;
	j["client_secret"] = client_secret;
	j["access_token"] = access_token;
	j["refresh_token"] = refresh_token;
	j["expires"] = std::chrono::duration_cast<std::chrono::seconds>(expires_at.time_since_epoch()).count();

	std::ofstream out{json_path};
	if(!out) std::cerr << std::format("Failed to create token file: {}\n", json_path.string());
	out << j.dump(2);
	out.close();

	try {
		fs::perms perms = fs::perms::owner_read | fs::perms::owner_write;
		fs::permissions(json_path, perms, fs::perm_options::replace);
	} catch (...) {}
}

void AuthManager::load_from_disk() {
	std::ifstream in{json_path};
	if(!in) return;

	try {
		json j;
		in >> j;
		client_id = j.at("client_id").get<std::string>();
		client_secret = j.at("client_secret").get<std::string>();
		access_token = j.at("access_token").get<std::string>();
		refresh_token = j.at("refresh_token").get<std::string>();
		expires_at = std::chrono::system_clock::time_point(std::chrono::seconds(j.at("expires").get<long long>()));
		tokens_valid = true;
	} catch (const std::exception &e) {
		std::cerr << std::format("Error: couldn't parse token file: {}\n", e.what());
		tokens_valid = false;
	}
}
