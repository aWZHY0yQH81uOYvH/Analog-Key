#include "HTTP.hpp"
#include "AuthManager.hpp"
#include "ThreadPool.hpp"

#include <iostream>

int main() {
	AuthManager auth;
	ThreadPool pool;
	std::cout << auth.get_access_token(pool.threads[0].http) << std::endl;
	
	
}
