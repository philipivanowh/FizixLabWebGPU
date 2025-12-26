#include "core/Utility.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

std::string Utility::LoadFileToString(const char* path) {
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file) {
		std::cerr << "Failed to open file: " << path << std::endl;
		return {};
	}

	std::ostringstream contents;
	contents << file.rdbuf();
	return contents.str();
}
