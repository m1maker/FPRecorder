#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

class user_config {
public:
	user_config(const std::string& filename) : filename(filename), data() {}

	int load() {
		try {
			std::ifstream file(filename);
			if (!file.is_open()) {
				return -1;
			}
			std::string line;
			while (std::getline(file, line)) {
				size_t pos = line.find('=');
				if (pos != std::string::npos) {
					std::string key = line.substr(0, pos);
					std::string value = line.substr(pos + 1);
					data[key] = value;
				}
			}
			return 0;
		}
		catch (const std::exception& e) {
			return -1;
		}
	}

	void save() {
		std::ofstream file(filename);
		for (const auto& pair : data) {
			file << pair.first << "=" << pair.second << "\n";
		}
	}

	std::string read(const std::string& key) {
		auto it = data.find(key);
		if (it != data.end()) {
			return it->second;
		}
		return "";
	}

	void write(const std::string& key, const std::string& value) {
		data[key] = value;
	}

private:
	std::string filename;
	std::unordered_map<std::string, std::string> data;
};
