#include "response_utils.h"

void http_handler::DumpRequest(const StringRequest& req) {
    BOOST_LOG(my_logger::get()) << req.method_string() << ' ' << req.target() << std::endl;
    // Выводим заголовки запроса
    for (const auto& header : req) {
        BOOST_LOG(my_logger::get()) << "  "sv << header.name_string() << ": "sv << header.value() << std::endl;
    }
}

std::string http_handler::FromUrlEncoding(const std::string& str) noexcept {
    std::string result;
    for (int i = 0; i < str.size(); i++) {
        if (str[i] == '+') {
            result.append(1, ' ');
            continue;
        }
        if (str[i] != '%') {
            result.append(1, str[i]);
            continue;
        }
        if (i >(str.size() - 3)) {
            result.append(1, str[i]);
            continue;
        }
        if (std::isxdigit(str[i + 1]) == false || (std::isxdigit(str[i + 2]) == false)) {
            result.append(1, str[i]);
            continue;
        }
        auto str_ascii_code = std::make_unique<const char*>(new char[2] {str[i + 1], str[i + 2]});
        long long_ascii_code = strtol(*str_ascii_code.get(), NULL, 16);

        if (long_ascii_code < 0 || long_ascii_code > 254) {
            result.append(1, str[i]);
            continue;
        }

        result += static_cast<char>(long_ascii_code);
        i += 2;
    }
    return result;
}

namespace http_handler {

	fs::path FileManager::root_path = {};
	bool FileManager::is_initialized = false;

	bool FileManager::IsFileInRootDir(const std::string& file)
	{
		if (CheckInitialized() == false) {
			return false;
		}

		if (file.size() == 0) {
			return false;
		}

		fs::path file_path = CanonicalFullPath(file);

		for (auto b = root_path.begin(), p = file_path.begin(); b != root_path.end(); ++b, ++p) {
			if (p == file_path.end() || *p != *b) {
				return false;
			}
		}
		return true;
	}

	bool FileManager::IsFileReadyToWork(const std::string& file)
	{
		if (CheckInitialized() == false) {
			return false;
		}

		fs::path file_path = CanonicalFullPath(file);

		if (fs::exists(file_path) == false) {
			LOG(LOG::ERROR_) << "Can`t open file  " << file_path.string() << ". File not exists." << LOG::Flush;

			return false;
		}

		if (fs::is_regular_file(file_path) == false) {
			LOG(LOG::ERROR_) << "Can`t open file  " << file_path.string() << ". It is not a regular file." << LOG::Flush;
			return false;
		}

		auto need_perm = fs::perms::owner_read | fs::perms::group_read;
		auto file_perm = fs::status(file_path).permissions();

		if ((file_perm & need_perm) == fs::perms::none) {
			LOG(LOG::ERROR_) << "Can`t open file  " << file_path.string() << ".Do not have enough read permissions." << LOG::Flush;
			return false;
		}

		return true;
	}

	void FileManager::SetRootPath(const std::string& p)
	{
		is_initialized = true;
		root_path = fs::weakly_canonical(fs::path(p));
	}

	std::string FileManager::GetAbsoluteFilePath(const std::string& file)
	{
		fs::path file_path = CanonicalFullPath(file);
		return file_path.string();
	}

	std::string FileManager::GetExtension(const std::string& file)
	{
		fs::path file_path(file);
		return file_path.extension().string();
	}

	fs::path FileManager::CanonicalFullPath(const std::string& file)
	{
		if (CheckInitialized() == false) {
			return fs::path();
		}

		fs::path file_path(root_path);
		file_path += fs::path(file);
		file_path = fs::weakly_canonical(file_path);
		return file_path;
	}

	bool FileManager::CheckInitialized()
	{
		if (is_initialized == false) {
			LOG(LOG::ERROR_) << " Root path not initialized. It must be set before using FileManager::" << LOG::Flush;

		}
		return is_initialized;
	}

} // namespace http_handler