#include "HTTP.hpp"

#include <lz-string.hpp>

#include <cstdlib>
#include <chrono>
#include <thread>
#include <iostream>

using nlohmann::json;

HTTP::HTTP() {
	curl = curl_easy_init();
	if(!curl)
		throw std::runtime_error("Failed to init cURL");
}

HTTP::~HTTP() {
	if(curl) curl_easy_cleanup(curl);
}

std::string HTTP::request(method_t method, std::string url, std::string body, std::vector<std::string> headers, bool fight_rate_limit) {
	int attempts = 12;
	std::chrono::minutes backoff{1};
	
	for(; attempts > 0; attempts--) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
		buffer.clear();
		
		switch(method) {
			case GET:
				// Get with body is nonstandard
				if(body.size()) {
					curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
					curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
				} else curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
				break;
			case POST:
				curl_easy_setopt(curl, CURLOPT_POST, 1L);
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
				break;
		}
		
		struct curl_slist *headers_slist = nullptr;
		for(auto &header:headers)
			headers_slist = curl_slist_append(headers_slist, header.c_str());
		if(headers_slist)
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_slist);
		
		CURLcode res = curl_easy_perform(curl);
		
		if(headers_slist)
			curl_slist_free_all(headers_slist);
		
		if(res != CURLE_OK)
			throw std::runtime_error(std::format("cURL error {}: {}\n", (int)res, curl_easy_strerror(res)));
		
		long code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if(code == 429 || code == 403) {
			std::string error = std::format("Rate limit exceeded from {}", url);
			if(fight_rate_limit) {
				std::cerr << std::format("{}: retrying in {} minute{}\n", error, backoff.count(), backoff.count() == 1 ? "" : "s");
				std::this_thread::sleep_for(backoff);
				backoff *= 2;
				continue;
			}
			throw rate_limit_error(error);
		}
		if(code < 200 || code >= 300)
			throw std::runtime_error(std::format("Received response code {} from {}:\n\t{}\n", code, url, response()));
		
		break;
	}
	
	if(attempts == 0)
		throw rate_limit_error(std::format("Giving up on rate limit for {}", url));
	
	return response();
}

json HTTP::request_json(method_t method, std::string url, std::string body, std::vector<std::string> headers, bool fight_rate_limit) {
	return json::parse(request(method, url, body, headers, fight_rate_limit));
}

std::string HTTP::response() const {
	return buffer;
}

nlohmann::json HTTP::response_json() const {
	return json::parse(response());
}

std::string HTTP::escape(const std::string &in) {
	char *ret = curl_easy_escape(curl, in.c_str(), in.length());
	std::string str = ret;
	free(ret);
	return str;
}

std::string HTTP::lzuri(const json &j) {
	std::string jj = j.dump(-1, ' ', true);

	// Annoying u16string conversions
	lzstring::string jjj;
	jjj.reserve(jj.size());
	for(unsigned char c:jj)
		jjj.push_back(static_cast<char16_t>(c));

	// Same bitstream as compressToEncodedURIComponent, just via the
	// base64 codepath (the library doesn't expose the URI-safe variant directly).
	auto out16 = lzstring::compressToBase64(jjj);

	std::string out;
	out.reserve(out16.size());
	for(char16_t c:out16)
		out.push_back(static_cast<char>(c));

	// lz-string's URI-safe alphabet differs from base64 only at index 63
	// ('-' instead of '/'); index 62 ('+') is shared between both alphabets.
	for(char &c:out)
		if(c == '/') c = '-';

	// Delete base64 =
	while(!out.empty() && out.back() == '=')
		out.pop_back();
	
	// URL encode any +
	return escape(out);
}

size_t HTTP::write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
	auto *out = static_cast<std::string *>(userdata);
	out->append(ptr, size * nmemb);
	return size * nmemb;
}
