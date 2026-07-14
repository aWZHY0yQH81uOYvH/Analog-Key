#include "GUI.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <format>
#include <stdexcept>

void glfw_error_callback(int error, const char *description) {
	std::cerr << std::format("GLFW error {}: {}\n", error, description);
}

GUI::GUI(std::shared_ptr<Database> db, std::shared_ptr<DigiKey> dk): db(db), dk(dk) {
	// GLFW
	glfwSetErrorCallback(glfw_error_callback);
	if(!glfwInit()) throw std::runtime_error("Failed to init GLFW");
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_TRUE);
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	
	window = glfwCreateWindow(1280, 720, "Analog-Key", nullptr, nullptr);
	if(!window) {
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}
	
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	
	float xscale = 1, yscale = 1;
	glfwGetWindowContentScale(window, &xscale, &yscale);
	ui_scale = xscale;
	
	// ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;
	
	// Font
	ImFontConfig cfg;
	cfg.SizePixels = 12 * ui_scale;
	cfg.OversampleH = 2;
	io.Fonts->AddFontDefault(&cfg);
	io.FontGlobalScale = 1 / ui_scale;
	
	// Style
	ImGui::StyleColorsDark();
	
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void GUI::run() {
	while(!glfwWindowShouldClose(window)) {
		db->check_update();
		
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		
		// Full size window
		int w, h;
		glfwGetWindowSize(window, &w, &h);
		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize({(float)w, (float)h});
		ImGui::Begin("Analog-Key",
		             nullptr,
		             ImGuiWindowFlags_NoResize    |
		             ImGuiWindowFlags_NoMove      |
		             ImGuiWindowFlags_NoCollapse  |
		             ImGuiWindowFlags_NoTitleBar);
		
		{
			std::lock_guard<std::mutex> lock{db->display_mutex};
			auto &data = db->display_data[db->buffer];
			
			static Database::Category selected_category; // TODO: bad
			std::string preview = (selected_category.id >= 0) ? selected_category.name : "-- None --";
			
			if(ImGui::BeginCombo("Category", preview.c_str(), ImGuiComboFlags_HeightLargest)) {
				if(ImGui::BeginChild("CategoryClipper", ImVec2(0, 200))) {
					ImGuiListClipper clipper;
					clipper.Begin(data.categories.size());
					
					while(clipper.Step()) {
						for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
							auto &item = data.categories[i];
							bool selected = (selected_category.id == item.id);
							
							std::string name = std::string(item.depth * 3, ' ') + item.name;
							
							if(ImGui::Selectable(name.c_str(), selected)) {
								selected_category = item;
								ImGui::CloseCurrentPopup();
							}
							
							if(selected) {
								selected_category = item;
								ImGui::SetItemDefaultFocus();
							}
						}
					}
					ImGui::EndChild();
				}
				ImGui::EndCombo();
			}
		}
		
		ImGui::End();
		ImGui::Render();
		
		int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
	}
	
	ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}
