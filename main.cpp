#include "HTTP.hpp"
#include "AuthManager.hpp"

#include <iostream>

int main() {
	http_t http = std::make_shared<HTTP>();
	AuthManager auth;
	std::cout << auth.get_access_token(http) << std::endl;
}
