#include "json_loader.h"
#include "model.h"
#include "logger.h"
#include "extra_data.h"

#include <fstream>
#include <iostream>

#include <boost/algorithm/string.hpp>

namespace json_loader {
    using namespace logger;
    Game LoadGame(const std::filesystem::path& json_path) {
        // Загрузить содержимое файла json_path, например, в виде строки
        // Распарсить строку как JSON, используя boost::json::parse
        // Загрузить модель игры из файла
        Game game;

        auto read_json = [](std::istream& is, sys::error_code& ec)->json::value
        {
            json::stream_parser p;
            std::string line;
            while (std::getline(is, line)) {
                p.write(line, ec);
                if (ec) {
                    return nullptr;
                }
            }
            p.finish(ec);
            if (ec) {
                return nullptr;
            }
            return p.release();
        };

        std::ifstream ifstream(json_path, std::ios::binary);
        json::value jv_config;
        if (!ifstream.is_open()) {
            LOG(LOG::ERROR_) << "failed to open GAME json file " << json_path.string() << LOG::Flush;
        }
        else {
            std::istream& istream = ifstream;
            sys::error_code ec;
            jv_config = read_json(istream, ec);
            if (ec) {
                LOG(LOG::ERROR_) << "Error parsing json file:" << ec.message() << LOG::Flush;
            }
        }

        Double default_dog_speed;
        try {
            default_dog_speed = static_cast<Double>(ToDouble(jv_config.at("defaultDogSpeed")));
        }
        catch (std::exception&) {
            default_dog_speed = 1.0;
        }
        game.SetDefaultDogSpeed(default_dog_speed);

        Double dog_retirement_time;
        try {
            dog_retirement_time = static_cast<Double>(ToDouble(jv_config.at("dogRetirementTime")));
        }
        catch (std::exception&) {
            dog_retirement_time = 10.0;
        }
        game.SetDogRetirementTime(static_cast<int>(dog_retirement_time*1000));

        int default_bag_capacity;
        try {
            default_bag_capacity = static_cast<int>(ToInt(jv_config.at("defaultBagCapacity")));
        }
        catch (std::exception&) {
            default_bag_capacity = 3;
        }
        game.SetDefaultBagCapacity(default_bag_capacity);

        LootConfig loot_config;
        try {
            loot_config.period_ = static_cast<double>(ToDouble(jv_config.at("lootGeneratorConfig").at("period")));
        }
        catch (std::exception&) {
            loot_config.period_ = 5.0;
        }

        try {
            loot_config.probability_ = static_cast<double>(ToDouble(jv_config.at("lootGeneratorConfig").at("probability")));
        }
        catch (std::exception&) {
            loot_config.probability_ = 0.5;
        }

        try {
            for (auto& map_json : jv_config.at("maps").get_array()) {
                Map::Id map_id(ToStr(map_json.at("id")));
                std::string name(ToStr(map_json.at("name")));
                ///WARNING:этот указатель на карту дальше используется во всём приложении.
                MapSharedPtr map_game(new Map{map_id, name });

                RoadsFromJSONToGame(map_game, map_json.at("roads").get_array());
                BuildingsFromJSONToGame(map_game, map_json.at("buildings").get_array());
                OfficiesFromJSONToGame(map_game, map_json.at("offices").get_array());

                Double dog_speed;
                try {
                    dog_speed = static_cast<Double>(ToDouble(map_json.at("dogSpeed")));
                }
                catch (std::exception&) {
                    dog_speed = game.DefaultDogSpeed();
                }
                map_game->SetDogSpeed(dog_speed);

                int bag_capacity;
                try {
                    bag_capacity = ToInt(map_json.at("bagCapacity"));
                }
                catch (std::exception&) {
                    bag_capacity = game.DefaultBagCapacity();
                }
                map_game->SetBagCapacity(bag_capacity);

                game.AddMap(map_game);

                json::array array_of_loots_;
                try {
                    array_of_loots_ = map_json.at("lootTypes").as_array();
                    loot_config.map_id_to_loot_type_count_[map_id] = static_cast<int>(array_of_loots_.size());
                }
                catch (std::exception&) {
                    loot_config.map_id_to_loot_type_count_[map_id] = 0;
                }

                if (array_of_loots_.size() != 0) {
                    std::string extra_data_tag = ExtraData::NameHelper({ *map_id,"lootTypes"s });
                    int loot_type_index = 0;
                    for (auto& loot_type_jv : array_of_loots_) {
                        int value;
                        try {
                            value = ToInt(loot_type_jv.at("value"));
                        }
                        catch (std::exception&) {
                            value = 0;
                        }
                        loot_config.map_id_to_loot_type_to_scores_[map_id][loot_type_index]=value;
                        ++loot_type_index;

                        ExtraData::Store(extra_data_tag, json::serialize(loot_type_jv));
                    }
                }
            }
        }
        catch (std::exception& ec) {
            LOG(LOG::ERROR_) << "Error parsing json file:" << ec.what() << LOG::Flush;
        }

        game.SetLootConfig(loot_config);

        return game;
    }

    std::string GetMapsList(const Game& game)
    {
        json::array maps_json;
        for (auto map : game.GetMaps()) {
            json::object jo;
            jo["id"] = *(map->GetId());
            jo["name"] = map->GetName();
            maps_json.push_back(jo);
        }
        return json::serialize(maps_json);
    }

    std::optional<std::string> GetMap(const Game& game, const std::string& map_name)
    {
        std::optional<std::string> result(std::nullopt);

        for (auto map : game.GetMaps()) {
            if (*(map->GetId()) == map_name) {
                json::object jo_map;
                jo_map["id"] = *(map->GetId());
                jo_map["name"] = map->GetName();

                jo_map["roads"] = RoadsFromGameToJSON(map);
                jo_map["buildings"] = BuildingsFromGameToJSON(map);
                jo_map["offices"] = OfficiesFromGameToJSON(map);
                jo_map["lootTypes"] = LootTypesFromExtraDataToJSON(*(map->GetId()));

                result = json::serialize(jo_map);

                break;
            }
        }

        return result;
    }

    std::string MakeError(const std::string& code, const std::string& message)
    {
        json::object jo;
        jo["code"] = code;
        jo["message"] = message;
        return  json::serialize(jo);
    }

    int ToInt(const json::value& value) {
        bool have_error = false;
        if (value.if_int64() == nullptr) {
            LOG(LOG::ERROR_) << "Error ToInt. value is not an integer" << LOG::Flush;
            have_error = true;
        }

        long long i64 = json::value_to<long long>(value);
        if ((i64 > (long long)std::numeric_limits<int>::max()) ||
            (i64 < (long long)std::numeric_limits<int>::min())) {
            LOG(LOG::ERROR_) << "Error ToInt. conversion from int64 to int32 is wrong." << LOG::Flush;
            have_error = true;
        }

        return have_error ? 0 : json::value_to<int>(value);
    }

    std::string ToStr(const json::value& value) {
        std::string result;
        auto str = value.if_string();
        if (str) {
            result.append(str->c_str());
        }
        else {
            LOG(LOG::ERROR_) << "Error ToStr. value is not a string" << LOG::Flush;
        }
        return result;
    }

    double ToDouble(const json::value& value) {
        double result;
        if (value.is_double()) {
            result = value.as_double();
        }
        else {
            LOG(LOG::ERROR_) << "Error ToDouble. value is not a double" << LOG::Flush;
        }
        return result;
    }

    void RoadsFromJSONToGame(MapSharedPtr new_map, const json::array& roads)
    {
        try {
            for (auto& road : roads) {
                Point start{ ToInt(road.at("x0")), ToInt(road.at("y0")) };
                if (road.as_object().contains("x1")) {
                    Road new_road(Road::HORIZONTAL, start, ToInt(road.at("x1")));
                    new_map->AddRoad(new_road);
                }
                else if (road.as_object().contains("y1")) {
                    Road new_road(Road::VERTICAL, start, ToInt(road.at("y1")));
                    new_map->AddRoad(new_road);
                }
            }
        }
        catch (std::exception& ec) {
            LOG(LOG::ERROR_) << "Error parsing json::array& roads:" << ec.what() << LOG::Flush;
        }
    }

    void BuildingsFromJSONToGame(MapSharedPtr map_game, const json::array& buildings)
    {
        try {
            for (auto& building : buildings) {
                Point position{ ToInt(building.at("x")), ToInt(building.at("y")) };
                Size size{ ToInt(building.at("w")), ToInt(building.at("h")) };
                Building new_building(model::Rectangle{ position, size });
                map_game->AddBuilding(new_building);
            }
        }
        catch (std::exception& ec) {
            LOG(LOG::ERROR_) << "Error parsing json::array& buildings:" << ec.what() << LOG::Flush;
        }
    }

    void OfficiesFromJSONToGame(MapSharedPtr map_game, const json::array& offices)
    {
        try {
            for (auto& office : offices) {
                Office::Id office_id(ToStr(office.at("id")));
                Point position{ ToInt(office.at("x")), ToInt(office.at("y")) };
                Offset offset{ ToInt(office.at("offsetX")), ToInt(office.at("offsetY")) };
                Office new_office(office_id, position, offset);
                map_game->AddOffice(new_office);
            }
        }
        catch (std::exception& ec) {
            LOG(LOG::ERROR_) << "Error parsing json::array& offices:" << ec.what() << LOG::Flush;
        }
    }

    json::array RoadsFromGameToJSON(MapSharedPtr map)
    {
        json::array roads;
        for (auto const& road : map->GetRoads()) {
            json::object jo;
            jo["x0"] = road.GetStart().x;
            jo["y0"] = road.GetStart().y;
            if (road.IsHorizontal()) {
                jo["x1"] = road.GetEnd().x;
            }
            else {
                jo["y1"] = road.GetEnd().y;
            }
            roads.push_back(jo);
        }
        return roads;
    }  // namespace json_loader

    json::array BuildingsFromGameToJSON(MapSharedPtr map)
    {
        json::array buildings;
        for (auto const& building : map->GetBuildings()) {
            json::object jo;
            jo["x"] = building.GetBounds().position.x;
            jo["y"] = building.GetBounds().position.y;
            jo["w"] = building.GetBounds().size.width;
            jo["h"] = building.GetBounds().size.height;
            buildings.push_back(jo);
        }
        return buildings;
    }

    json::array OfficiesFromGameToJSON(MapSharedPtr map)
    {
        json::array offices;
        for (auto const& office : map->GetOffices()) {
            json::object jo;
            jo["id"] = *office.GetId();
            jo["x"] = office.GetPosition().x;
            jo["y"] = office.GetPosition().y;
            jo["offsetX"] = office.GetOffset().dx;
            jo["offsetY"] = office.GetOffset().dy;
            offices.push_back(jo);
        }
        return offices;
    }

    json::array LootTypesFromExtraDataToJSON(const std::string& map_name)
    {
        json::array loot_types;
        std::string extra_data_tag = ExtraData::NameHelper({ map_name, "lootTypes"s });
        for (auto& loot_type : ExtraData::ReadAll(extra_data_tag)) {
            loot_types.push_back(json::parse(loot_type));
        }
        
        return loot_types;
    }
    
    std::string GetValueAsString(const std::string& str, const std::string& key) {
        json::error_code ec;
        json::value jv = json::parse(str, ec);

        if (ec) {
            return "";
        }

        if (jv.as_object().contains(key) == false) {
            return "";
        }
        try {
            auto result = json::serialize(jv.at(key));
            boost::erase_all(result, "\"");
            return result;
        }catch(std::exception&){
            return "";
        }
    }

    std::optional<std::uint64_t> GetValueAsUInt64(const std::string& str, const std::string& key) {
        json::error_code ec;
        json::value jv = json::parse(str, ec);

        if (ec) {
            return std::nullopt;
        }

        if (jv.as_object().contains(key) == false) {
            return std::nullopt;
        }
        try {
            std::uint64_t result = jv.at(key).as_int64();
            return result;
        }
        catch (std::exception&) {
            return std::nullopt;
        }
    }

    bool isJsonValid(const std::string& str) {
        json::error_code ec;
        json::value jv = json::parse(str, ec);
        if (ec) {
            return false;
        }
        return true;
    }
    std::string GetPlayersCase(const std::vector<app::PlayerSharedPtr> & players) {
        json::object arr;

        std::map<uint64_t, std::string> result_map;
        for (auto& p : players) {
            result_map[p.get()->GetIdAsUInt64()] = p.get()->GetName();
        }

        for (const auto& [key, value] : result_map) {
            arr.emplace(std::to_string(key), ToJsonAsObject("name", value));
        }

        std::string result= json::serialize(arr);
        boost::replace_all(result, "\\\\", "\\");
        return result;
    }

    std::string GetStateCase(const std::vector<app::PlayerSharedPtr>& players, const std::vector<app::LootSharedPtr>& loots) {
        auto array_pair = []<typename T>(T left, T right) {
            return json::array{left, right};
        };

        json::object result;
        json::object arr_players;

        for (auto& p : players) {
            json::object player;

            player.emplace("pos", array_pair(p->GetDog()->GetPosition().x, p->GetDog()->GetPosition().y));
            player.emplace("speed", array_pair(p->GetDog()->GetSpeed().dx, p->GetDog()->GetSpeed().dy));
            player.emplace("dir", p->GetDog()->GetDirStr());
            
            json::array bag;
            for (auto& loot_id : p->GetDog()->GetLootList()) {
                json::value loot = {
                    { "id",loot_id },
                    { "type",p->GetDog()->GetLoot(loot_id)->GetType()}
                };
                bag.push_back(loot);
            }
            player.emplace("bag", bag);
            player.emplace("score", p->GetDog()->GetScore());

            arr_players.emplace(p->GetId(), player);
        }
        result.emplace("players", arr_players);

        json::object arr_loots;
        for (auto& l : loots) {
            json::object loot;

            loot.emplace("type", l->GetType());
            loot.emplace("pos", array_pair(l->GetPos().x, l->GetPos().y));

            arr_loots.emplace(l->GetIdStr(), loot);
        }
        result.emplace("lostObjects", arr_loots);


        std::string result_str = json::serialize(result);
        boost::replace_all(result_str, "\\\\", "\\");
        return result_str;
    }
    std::string GetRecordsCase(Game& game, int start, int maxItems)
    {
        auto pool = game.GetDBConnectionPool();
        auto records = postgres::Database::GetPlayersRecords(pool, start, maxItems);

        json::array ja_records;
       
        for (auto& [id, name, score, playTime ] : records) {
            json::value record = {
                { "name",name },
                { "score",score },
                { "playTime",static_cast<double>(playTime)/1000. }
            };
            ja_records.push_back(record);
        }

        std::string result = json::serialize(ja_records);
        //boost::replace_all(result, "\\\\", "\\");
        return result;
    }
}//namespace json_loader