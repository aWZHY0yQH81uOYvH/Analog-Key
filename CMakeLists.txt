cmake_minimum_required(VERSION 3.16)
project(Analog-Key CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Download build some dependencies
include(FetchContent)

# GLFW
FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG        3.4
	GIT_SHALLOW    TRUE
)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# Dear ImGui
FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG        v1.92.8
	GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
	${imgui_SOURCE_DIR}
	${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw)

# curlcpp
FetchContent_Declare(
	curlcpp
	GIT_REPOSITORY https://github.com/JosephP91/curlcpp.git
	GIT_TAG        3.1
	GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(curlcpp)

# Analog-Key
add_executable(Analog-Key
	main.cpp
	AuthManager.cpp
)

target_link_libraries(Analog-Key PRIVATE
	curlcpp
	nlohmann_json::nlohmann_json
	Threads::Threads
	imgui
	sqlite3
)

target_compile_options(Analog-Key PRIVATE
    -Wall -Wextra -Wpedantic
)
