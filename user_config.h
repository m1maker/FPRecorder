#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

__declspec(allocate("CONFIG")) class user_config {
public:
	std::string last_value;
	std::string last_name;
	// Constructor that initializes the configuration with a filename
	user_config(const std::string& filename) : filename(filename), data() {}

	// Load configuration from the specified file
	int load() {
		try {
			std::ifstream file(filename);
			if (!file.is_open()) {
				return -1; // File could not be opened
			}

			std::string line;
			std::string current_section;

			while (std::getline(file, line)) {
				// Trim the line
				line = trim(line);
				if (line.empty() || line[0] == '#') {
					continue; // Skip empty lines and comments
				}

				// Check for section header
				if (line.front() == '[' && line.back() == ']') {
					current_section = line.substr(1, line.size() - 2); // Extract section name
					continue;
				}

				// Find the position of the '=' separator
				size_t pos = line.find('=');
				if (pos != std::string::npos) {
					// Extract key and value, trimming spaces
					std::string key = trim(line.substr(0, pos));
					std::string value = trim(line.substr(pos + 1));
					data[current_section][key] = value; // Store in the nested unordered_map
				}
			}
			return 0; // Success
		}
		catch (const std::exception& e) {
			return -1; // Exception occurred
		}
	}

	// Save the current configuration to the specified file
	void save() {
		std::ofstream file(filename);
		for (const auto& section : data) {
			file << "[" << section.first << "]\n"; // Write section header
			for (const auto& pair : section.second) {
				file << pair.first << " = " << pair.second << "\n"; // Write key-value pairs
			}
			file << "\n"; // Add a newline after each section for readability
		}
	}

	// Read a value by key from the specified section
	std::string read(const std::string& section, const std::string& key) {
		auto sec_it = data.find(section);
		if (sec_it != data.end()) {
			auto key_it = sec_it->second.find(key);
			if (key_it != sec_it->second.end()) {
				last_value = key_it->second; // Store last value read
				last_name = key_it->first;   // Store last key read
				return key_it->second;       // Return the value associated with the key
			}
		}
		return ""; // Key not found
	}

	// Write a new key-value pair to the specified section
	void write(const std::string& section, const std::string& key, const std::string& value) {
		data[section][key] = value; // Update or insert the key-value pair
	}
	std::vector<std::string> get_keys(const std::string& section) {
		std::vector<std::string> keys;
		auto sec_it = data.find(section);
		if (sec_it != data.end()) {
			for (const auto& pair : sec_it->second) {
				keys.push_back(pair.first);
			}
		}
		return keys;
	}


private:
	std::string filename; // Filename for loading/saving config
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> data; // Storage for sections and their key-value pairs

	// Helper function to trim whitespace from both ends of a string
	std::string trim(const std::string& str) {
		size_t first = str.find_first_not_of(' ');
		size_t last = str.find_last_not_of(' ');
		return (first == std::string::npos) ? "" : str.substr(first, (last - first + 1));
	}

};

