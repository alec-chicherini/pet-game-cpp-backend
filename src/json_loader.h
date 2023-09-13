#pragma once

#include <filesystem>
#include <optional>

#include "model.h"
#include "logger.h"
#include "application.h"

#include <boost/json.hpp>

using namespace model;

namespace json = boost::json;
namespace sys = boost::system;

using namespace model;
using namespace std::literals;
using namespace logger;

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path);
std::string GetMapsList(const Game & game);
std::optional<std::string> GetMap(const Game& game, const std::string & map_name );
std::string MakeError(const std::string& code, const std::string& message);

int ToInt(const json::value& value);
std::string ToStr(const json::value& value);
double ToDouble(const json::value& value);

void RoadsFromJSONToGame(MapSharedPtr new_map, const json::array & roads);
void BuildingsFromJSONToGame(MapSharedPtr new_map, const json::array& buildings);
void OfficiesFromJSONToGame(MapSharedPtr new_map, const json::array& offices);

json::array RoadsFromGameToJSON(MapSharedPtr map);
json::array BuildingsFromGameToJSON(MapSharedPtr map);
json::array OfficiesFromGameToJSON(MapSharedPtr map);

json::array LootTypesFromExtraDataToJSON(const std::string& map_name);

template<typename... KEYS>
bool isJsonHaveKeys(const::std::string& str, KEYS&&...keys) {
	json::error_code ec;
	json::value jv = json::parse(str,ec);
	if (ec) {
		return false;
	}
	bool contains = true;
	([&] { contains &= jv.as_object().contains(keys); }(), ...);

	return contains;
};

template<GoodJSONType... ARGS>
static std::string ToJsonAsString(ARGS&&... args){
    return json::serialize(LOG::ToJson(args...).as_object());
};

template<GoodJSONType... ARGS>
static json::object ToJsonAsObject(ARGS&&... args) {
    return  LOG::ToJson(args...).as_object();
};

std::string GetValueAsString(const std::string& str, const std::string& key);
std::optional<std::uint64_t> GetValueAsUInt64(const std::string& str, const std::string& key);
bool isJsonValid(const std::string& str);
std::string GetPlayersCase(const std::vector<app::PlayerSharedPtr>& players);
std::string GetStateCase(const std::vector<app::PlayerSharedPtr>& players, const std::vector<app::LootSharedPtr>& loots);
std::string GetRecordsCase(Game& game, int start, int maxItems);

}  // namespace json_loader
