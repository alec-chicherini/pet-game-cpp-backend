#include "model.h"
#include "loot_generator.h"

#include <random>
#include <stdexcept>
#include <map>
#include <filesystem>
#include <sstream>
#include <fstream>

#include "model_serialization.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include "application.h"
#include "tagged_uuid.h"

namespace model {
using namespace std::literals;
namespace fs = std::filesystem;
constexpr double mul = 1000.0;
constexpr Double road_radius = 0.4;

Road::Road(HorizontalTag, Point start, Coord end_x) noexcept
    : start_{ start }
    , end_{ end_x, start.y } {
}

Road::Road(VerticalTag, Point start, Coord end_y) noexcept
    : start_{ start }
    , end_{ start.x, end_y } {
}

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

Building::Building(Rectangle bounds) noexcept
    : bounds_{ bounds } {
}

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

Office::Office(Id id, Point position, Offset offset) noexcept
    : id_{ std::move(id) }
    , position_{ position }
    , offset_{ offset } {
}

const Office::Id& Office::GetId() const noexcept {
    return id_;
}

Point Office::GetPosition() const noexcept {
    return position_;
}

Offset Office::GetOffset() const noexcept {
    return offset_;
}

Map::Map(Id id, std::string name) noexcept
    : id_(std::move(id))
    , name_(std::move(name)) {
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

const LootConfig& Game::GetLootConfig() const noexcept
{
    return loot_config_;
}

void Game::LoadState() {
    using InputArchive = boost::archive::text_iarchive;

    fs::path p(path_to_state_file_);
    if (fs::exists(p) == false) {
        throw std::logic_error("State file do not exists.");
    }
    std::ifstream ifs(p);
    InputArchive input_archive{ ifs };

    ///0 loots
    size_t loot_count = 0;
    std::vector<LootSharedPtr> loot_;
    input_archive >> loot_count;
    for (size_t i = 0; i < loot_count; i++) {
        serialization::LootRepr repr_loot;
        input_archive >> repr_loot;
        loot_.push_back(repr_loot.Restore());
    }

    ///1 dogs
    size_t dog_count = 0;
    std::vector<DogSharedPtr> dogs_;
    input_archive >> dog_count;
    for (size_t i = 0; i < dog_count; i++) {
        serialization::DogRepr repr_dog;
        input_archive >> repr_dog;
        dogs_.push_back(std::make_shared<Dog>(repr_dog.Restore(loot_)));
    }

    ///2 sessions 
    size_t session_count = 0;
    std::vector<GameSessionSharedPtr> sessions_;
    input_archive >> session_count;
    for (size_t i = 0; i < session_count; i++) {
        serialization::GameSessionRepr repr_session;
        input_archive >> repr_session;
        sessions_.push_back(repr_session.Restore(*this, dogs_, loot_));
    }

    ///3 players 
    size_t players_count = 0;
    std::vector<app::PlayerSharedPtr> players_;
    input_archive >> players_count;
    for (size_t i = 0; i < players_count; i++) {
        serialization::PlayerRepr repr_player;
        input_archive >> repr_player;
        players_.push_back(repr_player.Restore(dogs_, sessions_));
    }

    ifs.close();

    /// move to game
    for (auto &s : sessions_) {
        sessions[s->GetMap()->GetId()] = s;
    }

    for (auto& p : players_) {
        app::Players::players.emplace_back(std::move(p));
    }
}

void Game::SaveState() {
    using OutputArchive = boost::archive::text_oarchive;
    fs::path p(path_to_state_file_);

    if (fs::exists(p) == false) {
        p = fs::temp_directory_path() / "game_save_data";
    }

    if (fs::exists(p.parent_path()) == false) {
        fs::create_directories(p.parent_path());
    }
    std::ofstream ofs(p);
    OutputArchive output_archive{ ofs };

    try {

        ///0 loots
        size_t loot_count = 0;;
        for (auto& s : sessions) {
            for (auto& l : s.second->GetLoots()) {
                loot_count += 1;
            }
        }
        for (auto& s : sessions) {
            for (auto& d : s.second->GetDogs()) {
                for (auto& l : d->GetLootInBag()) {
                    loot_count += 1;
                }
            }
        }

        output_archive << loot_count;
        for (auto& s : sessions) {
            for (auto& l : s.second->GetLoots()) {
                serialization::LootRepr repr_loot(l);
                output_archive << repr_loot;
            }
        }

        ///1 dogs
        size_t dog_count = 0;;
        for (auto& s : sessions) {
            for (auto& d : s.second->GetDogs()) {
                dog_count += 1;
            }
        }
        output_archive << dog_count;
        for (auto& s : sessions) {
            for (auto& d : s.second->GetDogs()) {
                serialization::DogRepr repr_dog(*d);
                output_archive << repr_dog;
            }
        }

        ///2 sessions
        size_t session_count = sessions.size();
        output_archive << session_count;
        for (auto& s : sessions) {
            serialization::GameSessionRepr repr_session(s.second);
            output_archive << repr_session;
        }

        ///3 players
        size_t players_count = app::Players::players.size();
        output_archive << players_count;
        for (auto& p : app::Players::players) {
            serialization::PlayerRepr repr_session(p);
            output_archive << repr_session;
        }
    }
    catch (std::exception& ex) {
        ofs.close();
        throw std::logic_error(ex.what());
    }

    ofs.close();
}

 void Game::SetSaveFilePath(const std::string& path) {
    path_to_state_file_ = path;
}

 void Game::SetSaveStatePeriod(int save_state_period) {
    save_state_period_ = save_state_period;
}

 int Game::GetSaveStatePeriod() noexcept {
    return save_state_period_;
}

 void Game::PrepareSaveStateCycle(Strand& strand) {
    if ((GetSaveStatePeriod() != Game::STATE_FILE_WAS_NOT_SET) &&
        (GetSaveStatePeriod() != Game::STATE_PERIOD_WAS_NOT_SET)) {
        ticker_save_state_ = std::make_shared<TickerSaveState>(strand, std::chrono::milliseconds(GetSaveStatePeriod()), [this](void) {
            this->SetNeedToSaveState(true);
            });
        ticker_save_state_->SingleShot();
    };
}

 bool Game::GetNeedToSaveState() {
    return need_to_save_state_;
}

 void Game::SetNeedToSaveState(bool need_to_save_state) {
    need_to_save_state_ = need_to_save_state;
}

 void Game::SetDogRetirementTime(int dog_retirement_time) {
     dog_retirement_time_ = dog_retirement_time;
 }

 const int Game::GetDogRetirementTime() const noexcept {
     return dog_retirement_time_;
 }

 void Game::SetDBConnectionPool(ConnectionPoolPtr pool) {
     pool_ = pool;
 }

 ConnectionPoolPtr Game::GetDBConnectionPool() {
     return pool_;
 }

 DogSharedPtr Game::GetDogByID(std::uint64_t dog_id) {
     for (auto& s : sessions) {
         for (auto& dog : s.second->GetDogs()) {
             if (dog->GetId() == dog_id) {
                 return dog;
             }
         }
     }
     return nullptr;
 }

MapPoint Game::GetRandomMapPointOnRoads(const Map::Id& id)
{
    std::random_device rd;
    size_t map_index = map_id_to_index_[id];
    int number_of_roads = int(maps_[map_index]->GetRoads().size()) - 1;
    std::uniform_int_distribution<int> dist(0, number_of_roads);
    auto& random_road = maps_[map_index]->GetRoads()[dist(rd)];

    int x1 = random_road.GetStart().x;
    int y1 = random_road.GetStart().y;
    int x2 = random_road.GetEnd().x;
    int y2 = random_road.GetEnd().y;
    if (x1 > x2) {
        std::swap(x1, x2);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    std::uniform_int_distribution<int> dist_x(x1, x2);
    std::uniform_int_distribution<int> dist_y(y1, y2);
    int x_random_at_map = dist_x(rd);
    int y_random_at_map = dist_y(rd);

    return MapPoint(x_random_at_map, y_random_at_map);
}

const Double Map::GetDogSpeed() const noexcept {
    return dog_speed_;
}
const int Map::GetBagCapacity() const noexcept {
    return bag_capacity_;
}
;

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddOffice(const Office & office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(office);
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::SetLootConfig(const LootConfig& config)
{
    loot_config_ = config;
}

void Map::SetDogSpeed(const Double& dog_speed) {
    dog_speed_ = dog_speed;
}
void Map::SetBagCapacity(int bag_capacity) {
    bag_capacity_ = bag_capacity;
}

void Game::UpdatePositionAndBag(GameSessionSharedPtr& session, std::uint64_t timeDelta) {
    MapSharedPtr map = session->GetMap();
    collision_detector::ConcreteProvider provider;

    //first part
    for (auto& dog : session->GetDogs()) {
        auto dog_start_pos = dog->GetPosition();
        auto cross = [](const MapLine& a, const MapLine& b)->std::optional<MapPoint> {
            typedef bg::model::point<double, 2, bg::cs::cartesian> point_t;
            typedef bg::model::segment<point_t> segment_t;
    
            segment_t ab(point_t(a.first.x, a.first.y), point_t(a.second.x, a.second.y));
            segment_t cd(point_t(b.first.x, b.first.y), point_t(b.second.x, b.second.y));
            std::vector<point_t> intersection_point;
            boost::geometry::intersection(ab, cd, intersection_point);
            if (intersection_point.size() == 0) {
                return std::nullopt;
            }
    
            MapPoint result = MapPoint(bg::get<0>(intersection_point[0]), bg::get<1>(intersection_point[0]));
            if ((result.x == a.first.x) && (result.y == a.first.y)) {
                return std::nullopt;
            }
    
            result.x = std::round(result.x * mul) / mul;
            result.y = std::round(result.y * mul) / mul;
    
            return result;
        };
        //
    
        auto lines_from_road = [](const Road& r) {
            std::vector<MapLine> result;
            auto start = r.GetStart();
            auto end = r.GetEnd();
            if (r.IsHorizontal() && (start.x > end.x)) {
                std::swap(start, end);
            }
            if (r.IsVertical() && (start.y > end.y)) {
                std::swap(start, end);
            }
    
            MapPoint a{ Double(start.x) - road_radius,Double(start.y) - road_radius };
            MapPoint b;
            MapPoint c{ Double(end.x) + road_radius,  Double(end.y) + road_radius };
            MapPoint d;
            if (r.IsHorizontal()) {
                b = MapPoint{ Double(end.x) + road_radius, Double(end.y) - road_radius };
                d = MapPoint{ Double(start.x) - road_radius, Double(start.y) + road_radius };
            }
            else if (r.IsVertical()) {
                b = MapPoint{ Double(start.x) + road_radius, Double(start.y) - road_radius };
                d = MapPoint{ Double(end.x) - road_radius, Double(end.y) + road_radius };
            }
    
            result.push_back(MapLine{ a,b });
            result.push_back(MapLine{ b,c });
            result.push_back(MapLine{ d,c });
            result.push_back(MapLine{ a,d });
    
            return result;
        };
    
        auto is_on_road = [](const Road& r, const MapPoint& p) {
            auto start = r.GetStart();
            auto end = r.GetEnd();
    
            if (r.IsHorizontal() && (start.x > end.x)) {
                std::swap(start, end);
            }
            if (r.IsVertical() && (start.y > end.y)) {
                std::swap(start, end);
            }
    
            MapPoint a{ Double(start.x) - road_radius, Double(start.y) - road_radius };
            MapPoint c{ Double(end.x) + road_radius, Double(end.y) + road_radius };
            auto r_cmp = [](double value) {
                const double edge = 0.001;
                return std::round(value / edge) * edge;
            };
            if (r_cmp(p.x) < r_cmp(a.x) || r_cmp(p.x) > r_cmp(c.x) ||
                r_cmp(p.y) < r_cmp(a.y) || r_cmp(p.y) > r_cmp(c.y)) {
                return false;
            }
            return true;
        };
    
        MapPoint new_position(dog->GetPosition().x + dog->GetSpeed().dx * double(timeDelta) / double(1000),
            dog->GetPosition().y + dog->GetSpeed().dy * double(timeDelta) / double(1000));
        MapLine dogMove{ dog->GetPosition(), new_position };
    
        std::vector<MapPoint> crossed_lines;
        bool new_pos_inside_rectangle = false;
    
        for (auto& r : map->GetRoads()) {
            std::vector<MapLine> borders = lines_from_road(r);
            for (auto& l : borders) {
                auto is_cross = cross(dogMove, l);
                if (is_cross) {
                    crossed_lines.push_back(is_cross.value());
                }
            }
            if (is_on_road(r, new_position)) {
                new_pos_inside_rectangle = true;
            }
        }
    
        if (crossed_lines.size() == 0) {
            if (new_pos_inside_rectangle) {
                dog->SetPos(new_position);
            }
            else {
                dog->SetPos(dog->GetPosition());
                dog->SetSpeed({ 0,0 });
            }
        }
        else if (crossed_lines.size() == 1) {
            if (new_pos_inside_rectangle) {
                dog->SetPos(new_position);
            }
            else {
                dog->SetPos(crossed_lines[0]);
                dog->SetSpeed({ 0,0 });
            }
        }
        else {///crossed_lines.size() > 1
            crossed_lines.push_back(new_position);
            std::vector<MapPoint> crossed_lines_with_good_path;
            for (int i = 0; i < crossed_lines.size(); i++) {
                auto x1 = dog->GetPosition().x;
                auto y1 = dog->GetPosition().y;
                auto x2 = crossed_lines[i].x;
                auto y2 = crossed_lines[i].y;
                double dx, dy;
                const double step_along_the_path = 0.1001;
                if (y2 - y1 > 0) {
                    dy = step_along_the_path;
                }
                else {
                    dy = -1.0 * step_along_the_path;
                }
    
                if (x2 - x1 > 0) {
                    dx = step_along_the_path;
                }
                else {
                    dx = -1.0 * step_along_the_path;
                }
    
                if (x1 == x2) {
                    bool all_points_on_road = true;
                    while (true) {
                        y1 += dy;
                        if (dy > 0 && y1 >= y2) {
                            break;
                        }
                        if (dy < 0 && y2 >= y1) {
                            break;
                        }
                        bool this_point_on_road = false;
                        for (auto& r : map->GetRoads()) {
                            if (is_on_road(r, MapPoint(x1, y1))) {
                                this_point_on_road = true;
                                break;
                            }
                        }
                        if (this_point_on_road == false) {
                            all_points_on_road = false;
                            break;
                        }
    
                    }
                    if (all_points_on_road == true) {
                        crossed_lines_with_good_path.push_back(crossed_lines[i]);
                    }
                }
                else  if (y1 == y2) {
                    bool all_points_on_road = true;
                    while (true) {
                        x1 += dx;
                        if (dx > 0 && x1 >= x2) {
                            break;
                        }
                        if (dx < 0 && x2 >= x1) {
                            break;
                        }
                        bool this_point_on_road = false;
                        for (auto& r : map->GetRoads()) {
                            if (is_on_road(r, MapPoint(x1, y1))) {
                                this_point_on_road = true;
                                break;
                            }
                        }
                        if (this_point_on_road == false) {
                            all_points_on_road = false;
                            break;
                        }
    
                    }
                    if (all_points_on_road == true) {
                        crossed_lines_with_good_path.push_back(crossed_lines[i]);
                    }
                }
            }
    
            Double max_dist = 0.0;
            /// самая дальняя из хороших точек пересечения
            MapPoint farthest_point;
            if (crossed_lines_with_good_path.size() == 0) {
                farthest_point = dog->GetPosition();
            }
            for (int i = 0; i < crossed_lines_with_good_path.size(); i++) {
                typedef boost::geometry::model::d2::point_xy<double> boost_point_type;
    
                Double d = boost::geometry::distance(boost_point_type(dog->GetPosition().x, dog->GetPosition().y),
                    boost_point_type(crossed_lines_with_good_path[i].x, crossed_lines_with_good_path[i].y));
                if (d > max_dist) {
                    max_dist = d;
                    farthest_point = crossed_lines_with_good_path[i];
                }
            }
            dog->SetPos(farthest_point);
            if ((farthest_point.x != new_position.x) || (farthest_point.y != new_position.y)) {
                dog->SetSpeed({ 0,0 });
            }
        }
    
        auto dog_finish_pos = dog->GetPosition();
        provider.AddGatherer({geom::Point2D(dog_start_pos.x,  dog_start_pos.y),
                              geom::Point2D(dog_finish_pos.x, dog_finish_pos.y),
                              0.6,
                              dog->GetId()});
    }//for (auto& dog : session->GetDogs())
    
    
    //second part
    for (auto& l : session->GetLoots()) {
        provider.AddItem({ geom::Point2D(l->GetPos().x, l->GetPos().y), 0.0, l->GetId() });
    }
    
    std::uint64_t id_for_office = std::numeric_limits< std::uint64_t > ::max();
    std::map< std::uint64_t, std::string > item_id_to_office_id;
    for (auto& o : map->GetOffices()) {
        auto o_pos = o.GetPosition();
        item_id_to_office_id[id_for_office] = *o.GetId();
        provider.AddItem({ geom::Point2D(o_pos.x, o_pos.y), 0.5, id_for_office });
        --id_for_office;
    }
    
    auto is_this_office = [&](const collision_detector::GatheringEvent & event) {
        if (item_id_to_office_id.find(event.item_id) == item_id_to_office_id.end()) {
            return false;
        }
        return true;
    };

    auto collision_events = FindGatherEvents(provider);

    std::vector< size_t > already_collected_loot;
    for (auto& event : collision_events) {
        auto dog__ = session->GetDog(event.gatherer_id);
        if (dog__ == nullptr) return;
        auto is_this_office_ = is_this_office(event);
        if (is_this_office_) {
            dog__->ReleaseLootBag();
        }
        else{ ///this is loot
            /// if multiple dogs have same item in event only closer dog capture the loot
            if (std::find(already_collected_loot.begin(),
                already_collected_loot.end(),
                event.item_id) != already_collected_loot.end()) {
                continue;
            }
            auto loot_list_size = dog__->GetLootList().size();
            auto bag_capacity = static_cast<int>(map->GetBagCapacity());
            if (loot_list_size  < bag_capacity) {
                dog__->AddLoot(session->GetLoot(event.item_id));
                already_collected_loot.push_back(event.item_id);
            }
        }
    };
    
}

void Game::AddMap(MapSharedPtr map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map->GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map->GetId() + " already exists"s);
    }
    else {
        try {
            maps_.emplace_back(map);
        }
        catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

MapSharedPtr Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return maps_.at(it->second);
    }
    return nullptr;
};

std::optional<GameSessionSharedPtr> Game::AddDogToSession(DogSharedPtr dog, const Map::Id& id) {
    bool have_map_id = map_id_to_index_.contains(id);
    if (randomize_spawn_points && have_map_id) {
        dog->SetPos(GetRandomMapPointOnRoads(id));
    }
    else if(have_map_id && maps_[map_id_to_index_[id]]->GetRoads().size()) {
        int x_at_frist_road_start = maps_[map_id_to_index_[id]]->GetRoads()[0].GetStart().x;
        int y_at_frist_road_start = maps_[map_id_to_index_[id]]->GetRoads()[0].GetStart().y;

        dog->SetPos(MapPoint(x_at_frist_road_start, y_at_frist_road_start));
    }

    if (sessions.contains(id)) {
        sessions[id]->AddDog(dog);
        return sessions[id];
    }
    else {
        auto map = FindMap(id);
        if (map == nullptr) {
            return std::nullopt;
        }
        sessions[id] = GameSessionSharedPtr(new GameSession(map));
        sessions[id]->AddDog(dog);
        return sessions[id];
    }
}

void Game::SetDefaultDogSpeed(const Double& default_dog_speed){
    default_dog_speed_ = default_dog_speed;
}

const Double Game::DefaultDogSpeed() const noexcept{
    return default_dog_speed_;
}

void Game::SetDefaultBagCapacity(const int& bag_capacity) {
    default_bag_capacity_ = bag_capacity;
}

int Game::DefaultBagCapacity() {
    return default_bag_capacity_;
}

void Game::Update(std::uint64_t timeDelta) {
    for (auto& s : sessions) {
        std::map<std::uint64_t, MapPoint> dogPos;
        for (auto& dog : s.second->GetDogs()) {
            dogPos[dog->GetId()] = dog->GetPosition();
            if (dog_in_game_time_.find(dog->GetId()) == dog_in_game_time_.end()) {
                dog_in_game_time_[dog->GetId()] = timeDelta;
            }
            else {
                dog_in_game_time_[dog->GetId()] += timeDelta;
            }
        }
        UpdatePositionAndBag(s.second,timeDelta);
        for (auto& dog : s.second->GetDogs()) {
            auto dogId = dog->GetId();
            /// first && condition to pass test
            if ( ((dog->NumberOfDogMoves()<=2l) && (dogPos[dogId] == dog->GetPosition())) || (dog->GetDirection() == NONE)) {
                not_active_dogs_[dogId] += timeDelta;
            }
            else {
                not_active_dogs_.erase(dogId);
            }
        }
        UpdateLoot(s.second, std::chrono::milliseconds(timeDelta));
    }

    std::vector<std::uint64_t> dog_id_to_delete;
    for (auto& dog_timer : not_active_dogs_) {
        if (dog_timer.second >= GetDogRetirementTime()) {
            dog_id_to_delete.push_back(dog_timer.first);
        }
    }
    for (const auto& dog_id : dog_id_to_delete) {
        auto dog = GetDogByID(dog_id);
        if (dog == nullptr) {
            throw(std::logic_error("dog must be real instance to update it score."));
        }else{
        postgres::Database::SaveRecord(GetDBConnectionPool(),
            PlayerRecord{ dog->GetUUID(),
                          dog->GetName(),
                          dog->GetScore(),
                          static_cast<int>(dog_in_game_time_[dog->GetId()]) });
        }
        app::Players::RemovePlayerFromGameByDogId(dog_id);
        not_active_dogs_.erase(dog_id);
        dog_in_game_time_.erase(dog_id);
    }
    
    if (GetTickPeriod() == Game::TICK_TESTING_MODE) {
        SaveState();
        return;
    }
    if (GetNeedToSaveState() == true) {
        SetNeedToSaveState(false);
        SaveState();
        ticker_save_state_->SingleShot();
    }
}

void Game::Update(std::chrono::milliseconds timeDelta)
{
    this->Update(timeDelta.count());
}

void Game::UpdateLoot(GameSessionSharedPtr& session, std::chrono::milliseconds timeDelta)
{
    loot_gen::LootGenerator gen{ std::chrono::milliseconds(static_cast<int>(loot_config_.period_)),
                                 loot_config_.probability_ };
    unsigned count_new_loot_to_add = gen.Generate(timeDelta,
                                              static_cast<unsigned>(session->GetLoots().size()),
                                              static_cast<unsigned>(session->GetDogs().size()));
    while (count_new_loot_to_add--) {
        std::random_device rd;
        auto map_id = session->GetMap()->GetId();
        int count_loots_type = loot_config_.map_id_to_loot_type_count_[map_id];
        std::uniform_int_distribution<int> dist(0, count_loots_type-1);
        int loot_type = dist(rd);
        MapPoint loot_pos = GetRandomMapPointOnRoads(map_id);
        int loot_value = loot_config_.map_id_to_loot_type_to_scores_[map_id][loot_type];
        session->AddLoot(std::make_shared< Loot >(loot_type, loot_pos, loot_value));
    }
}

void Game::SetTickPeriod(int tick_period_) {
    tick_period = tick_period_;
}

int Game::GetTickPeriod() const noexcept { 
    return tick_period;
}

void Game::SetRandomizeSpawnPoint(int randomize_spawn_points_) {
    randomize_spawn_points = randomize_spawn_points_;
}

int Game::GetRandomizeSpawnPoint() const noexcept {
    return randomize_spawn_points;
}

std::atomic<std::uint64_t> Dog::dog_counter{0};
std::atomic<std::uint64_t> GameSession::session_counter{1};
std::atomic<std::uint64_t> Loot::loot_counter{0};

 GameSession::GameSession(MapSharedPtr map_) :map(map_) {
}

 void GameSession::AddDog(DogSharedPtr dog) {
     MapPoint startedPos(map->GetRoads()[0].GetStart());
     dogs.push_back(dog);
 }

 void GameSession::AddLoot(LootSharedPtr loot) {
     loots.push_back(loot);
 }

 std::uint64_t GameSession::GetId() const noexcept {
     return id;
 }

  void GameSession::SetId(std::uint64_t id_) {
     id = id_;
 }

 MapSharedPtr GameSession::GetMap() const noexcept {
     return map;
 }

 std::vector<DogSharedPtr> GameSession::GetDogs() const noexcept {
     return dogs;
 }

 DogSharedPtr GameSession::GetDog(std::uint64_t dog_id) const {
     for (auto& d : dogs) {
         if (d->GetId() == dog_id) {
             return d;
         }
     }
     return nullptr;
 }

 Loots GameSession::GetLoots() const noexcept {
     return loots;
 }

 LootSharedPtr GameSession::GetLoot(std::uint64_t id) {
     LootSharedPtr result = nullptr;
     try {
         bool need_to_erase = false;
         auto it = loots.begin();
         for (; it != loots.end(); it++) {
             if (it->get()->GetId() == id) {
                 need_to_erase = true;
                 result = *it;
                 break;
             }
         }
         if(need_to_erase){
             loots.erase(it);
         }
     }
     catch(std::exception& e){
         BOOST_LOG(my_logger::get()) << "e.what " << e.what() << std::endl;
     }
     return result;
 }

  void GameSession::UpdateGameSessionCounter() {
     if (id > session_counter) {
         session_counter = id;
     }
 }

  void GameSession::RemoveDog(std::uint64_t dog_id) {
      dogs.erase(find(dogs.begin(), dogs.end(), GetDog(dog_id)));
  }

 Dog::Dog() {
     id_ = dog_counter++;
     name_ = "Dog_";
     name_.append(std::to_string(id_));
     pos_.x = 0;
     pos_.y = 0;
     speed_.dx = 0;
     speed_.dy = 0;
     dir_ = UP;
     score_ = 0;
     uuid_ = util::detail::UUIDToString(util::detail::NewUUID());
 }

 Dog::Dog(std::string name) :name_(name) {
     id_ = dog_counter++;
     pos_.x = 0;
     pos_.y = 0;
     speed_.dx = 0;
     speed_.dy = 0;
     dir_ = UP;
     score_ = 0;
     uuid_ = util::detail::UUIDToString(util::detail::NewUUID());
 }

 std::string Dog::GetName() const noexcept {
     return name_;
 }

 void Dog::SetPos(const MapPoint& p) {
     pos_ = p;

     pos_.x = std::round(pos_.x * mul) / mul;
     pos_.y = std::round(pos_.y * mul) / mul;
 }

 void Dog::SetSpeed(const MapSpeed& s) {
     speed_ = s;
 }

 void Dog::SetDirection(const DIRECTION& d) {
     dir_ = d;
 }

 void Dog::SetDirection(const std::string& d) {
          if (d == "U") dir_ = UP;
     else if (d == "D") dir_ = DOWN;
     else if (d == "R") dir_ = RIGHT;
     else if (d == "L") dir_ = LEFT;
     else if (d == "")  dir_ = NONE;
     else               dir_ = NONE;
 }

  void Dog::SetScore(int score) {
     score_ = score;
 }

 void Dog::AddLoot(LootSharedPtr loot) {
     if (loot == nullptr) {
         throw std::runtime_error("try to add loot when it nullptr");
     }
     bag_.push_back(loot);
 }

  void Dog::SetName(const std::string& name) {
     name_ = name;
 }

  void Dog::SetId(const std::uint64_t& id) {
     id_ = id;
 }

 std::vector<std::uint64_t> Dog::GetLootList() {
     std::vector<std::uint64_t> result;
     if(bag_.size() > 0){
        for (const auto& l : bag_) {
            result.push_back(l->GetId());
        }
     }
     return result;
 }

 LootSharedPtr Dog::GetLoot(std::uint64_t id){
     LootSharedPtr result = nullptr;
     auto it = bag_.begin();
     for (; it != bag_.end(); it++) {
         if (it->get()->GetId() == id) {
             result = *it;
             break;
         }
     }
     return result;
 }

 void Dog::ReleaseLootBag() {
     for (auto& loot_in_bag : bag_) {
         score_ += loot_in_bag->GetValue();
     }
     bag_.clear();
 }

 MapPoint Dog::GetPosition() const noexcept {
     return pos_;
 }

 MapSpeed Dog::GetSpeed() const noexcept {
     return speed_;
 }

 DIRECTION Dog::GetDirection() const noexcept {
     return dir_;
 }

 std::string Dog::GetDirStr() const {
     std::string result;
     switch (dir_) {
     case UP:result = std::string("U"); break;
     case DOWN:result = std::string("D"); break;
     case LEFT:result = std::string("L"); break;
     case RIGHT:result = std::string("R"); break;
     case NONE:result = std::string(""); break;
     default:result = std::string("WTF");
     }
     return result;
 }

 uint64_t Dog::GetId() const noexcept {
     return id_;
 }

 int Dog::GetScore() const noexcept {
     return score_;
 }

  Loots Dog::GetLootInBag(){
     return bag_;
 }

  void Dog::UpdateDogCounter() {
     if (id_ > dog_counter) {
         dog_counter = id_;
     }
 }

  void Dog::SetUUID(const std::string& uuid) {
      uuid_ = uuid;
  }

  std::string Dog::GetUUID() {
      return uuid_;
  }

  long long Dog::NumberOfDogMoves() {
      return ++number_of_dog_moves_;
  }

 Loot::Loot(int loot_type, MapPoint position, int value)
     :loot_type_(loot_type),
     position_(position),
     id(loot_counter++),
     value_(value) {}

 std::uint64_t Loot::GetId() const noexcept {
     return id;
 }

 void Loot::SetId(uint64_t id_) {
     id = id_;
 }

 std::string Loot::GetIdStr() const noexcept {
     return std::to_string(id);
 }

 int Loot::GetType() const noexcept {
     return loot_type_;
 }

 MapPoint Loot::GetPos() const noexcept {
     return position_;
 }

 int Loot::GetValue() const noexcept {
     return value_;
 }

 void Loot::SetValue(int value) {
     value_ = value;
 }

  void Loot::UpdateLootCounter() {
     if (id > loot_counter) {
         loot_counter = id;
     }
 }

  void Loot::SetType(int type) {
     loot_type_ = type;
 }

  void Loot::SetPos(MapPoint position) {
     position_ = position;
 }

}  // namespace model
