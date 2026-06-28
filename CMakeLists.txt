cmake_minimum_required(VERSION 3.16)

project(UltraHawk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES
	src/*.cpp
)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_executable(UltraHawk ${SOURCES})

target_include_directories(UltraHawk PRIVATE
	src
)

target_link_libraries(UltraHawk PRIVATE Qt6::Widgets)

target_compile_options(UltraHawk PRIVATE
	-O3
)

if(MINGW)
	target_link_options(UltraHawk PRIVATE
		-static
		-static-libgcc
		-static-libstdc++
	)
endif()
