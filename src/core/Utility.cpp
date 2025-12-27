#include "core/Utility.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

std::string Utility::LoadFileToString(const char* path) {
	// Try the path as given first
	std::ifstream file(path, std::ios::in | std::ios::binary);
	
	if (!file) {
		// Try relative to current working directory + path
		std::string fallbackPath = std::string("../") + path;
		file.open(fallbackPath, std::ios::in | std::ios::binary);
		
		if (!file) {
			std::cerr << "Failed to open file: " << path << std::endl;
			std::cerr << "Also tried: " << fallbackPath << std::endl;
			std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
			return {};
		}
	}

	std::ostringstream contents;
	contents << file.rdbuf();
	return contents.str();
}
