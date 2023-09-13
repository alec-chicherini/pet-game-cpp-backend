#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>
#include <sstream>
#include <vector>

using Storage = std::unordered_map < std::string, std::vector<std::string> >;
class ExtraData {
	static Storage storage_;
	ExtraData() = delete;
public:
	static void Store(const std::string& data_tag, const std::string& value);;
	static void Store(const std::string& data_tag, std::initializer_list<std::string> values);;
	static std::string& ReadAt(const std::string& data_tag, int index_at_array=0);;
	static std::vector<std::string>& ReadAll(const std::string& data_tag);;
	static std::string NameHelper(std::initializer_list<std::string> fields);
};