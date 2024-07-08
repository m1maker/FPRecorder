#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

class user_config {
public:
	user_config(std::string filename) {
		this->filename = filename;
		this->data = std::unordered_map<std::string, std::string>();
	}

	int load() {
		try {
			std::ifstream file(this->filename);
			std::string line;
			while (std::getline(file, line)) {
				size_t pos = line.find('=');
				if (pos != std::string::npos) {
					std::string key = line.substr(0, pos);
					std::string value = line.substr(pos + 1);
					this->data[key] = value;
				}
			}
			return 0;
		}
		catch (const std::exception& e) {
			return -1;
		}
	}

	void save() {
		std::ofstream file(this->filename);
		for (const auto& pair : this->data) {
			file << pair.first << "=" << pair.second << "\n";
		}
	}

	std::string read(const std::string& key) {
		auto it = this->data.find(key);
		if (it != this->data.end()) {
			return it->second;
		}
		return "";
	}

	void write(const std::string& key, const std::string& value) {
		this->data[key] = value;
	}

private:
	std::string filename;
	std::unordered_map<std::string, std::string> data;
};

