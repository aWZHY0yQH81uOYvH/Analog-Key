#pragma once

#include "Database.hpp"
#include "DigiKey.hpp"

#include <GLFW/glfw3.h>

#include <memory>

class GUI {
public:
	GUI(std::shared_ptr<Database> db, std::shared_ptr<DigiKey> dk);
	
	void run();
	
protected:
	float ui_scale;
	GLFWwindow *window;

	std::shared_ptr<Database> db;
	std::shared_ptr<DigiKey> dk;
};
