#include "extra_data.h"

Storage ExtraData::storage_{};

void ExtraData::Store(const std::string& data_tag, const std::string& value) {
	if (storage_.find(data_tag) != storage_.end()) {
		storage_[data_tag].push_back(value);
	}
	else {
		storage_[data_tag] = std::vector<std::string>({ value });
	}
}

void ExtraData::Store(const std::string& data_tag, std::initializer_list<std::string> values) {
	if (storage_.find(data_tag) != storage_.end()) {
		storage_[data_tag].insert(storage_[data_tag].end(), values);
	}
	else {
		storage_[data_tag] = std::vector<std::string>({ values });
	}
}

std::string& ExtraData::ReadAt(const std::string& data_tag, int index_at_array) {
	if (storage_.find(data_tag) == storage_.end()) {
		std::stringstream ss;
		ss << "Field: " << data_tag << " not found.";
		std::string error_msg(ss.str());
		throw std::logic_error(error_msg);
	}
	if (storage_[data_tag].size() < (index_at_array + 1)) {
		std::stringstream ss;
		ss << "Wrong index at vector: " << index_at_array << ". Vector size:" << storage_[data_tag].size();
		std::string error_msg(ss.str());
		throw std::logic_error(error_msg);
	}
	return storage_[data_tag][index_at_array];
}

std::vector<std::string>& ExtraData::ReadAll(const std::string& data_tag) {
	if (storage_.find(data_tag) == storage_.end()) {
		std::stringstream ss;
		ss << "Field: " << data_tag << " not found.";
		std::string error_msg(ss.str());
		throw std::logic_error(error_msg);
	}

	return storage_[data_tag];
}

std::string ExtraData::NameHelper(std::initializer_list<std::string> fields) {
	if (fields.size() == 0) {
		using namespace std::literals;
		return ""s;
	}
	std::string result(*fields.begin());
	if (fields.size() == 1) {
		return result;
	}
	result += "_";
	for (int i = 1; i < fields.size(); i++) {
		result += *(fields.begin() + i);
		if (i != fields.size() - 1) {
			result += "_";
		}
	}
	return result;
}
