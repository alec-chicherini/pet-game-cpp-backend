#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>
#include <atomic>
#include <cassert>
#include <optional>
#include <filesystem>

#include <boost/geometry.hpp>

#include "tagged.h"
#include "collision_detector.h"
#include "ticker.h"
#include "postgres.h"
#include "logger.h"

namespace model {
    class GameSession;
    using GameSessionSharedPtr = std::shared_ptr<GameSession>;
    class Dog;
    using DogSharedPtr = std::shared_ptr<Dog>;
    using Dogs = std::vector<DogSharedPtr>;
    class Loot;
    using LootSharedPtr = std::shared_ptr<Loot>;
    using Loots = std::vector<LootSharedPtr>;
    class Map;
    using MapSharedPtr = std::shared_ptr<Map>;

    using Dimension = int;
    using Coord = Dimension;

    using Double = double;

    namespace bg = boost::geometry;
    namespace fs = std::filesystem;
struct Point {
    Coord x, y;
};

struct MapPoint {
    double x, y;
    MapPoint() = default;
    MapPoint(Double x_, Double y_) :x(x_), y(y_) {};
    MapPoint(Point p) :x(p.x), y(p.y) {};
    bool operator==(const MapPoint& rhs) const { return ((x == rhs.x) && (y == rhs.y)); };
};

struct MapLine {
    MapPoint first, second;
    MapLine() = default;
    MapLine(MapPoint first_, MapPoint second_) :first(first_), second(second_) {};
    MapLine(Double x1, Double y1, Double x2, Double y2): first(MapPoint{ x1,y1 }), second(MapPoint{ x2,y2 }) {};
};

struct MapSpeed {
    Double dx, dy;
    bool operator==(const MapSpeed& rhs) const { return ((dx == rhs.dx) && (dy == rhs.dy)); };
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

enum DIRECTION{
    UP,
    DOWN,
    LEFT,
    RIGHT,
    NONE
};

class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept;
    Road(VerticalTag, Point start, Coord end_y) noexcept;

    bool IsHorizontal() const noexcept;
    bool IsVertical() const noexcept;
    Point GetStart() const noexcept;
    Point GetEnd() const noexcept;

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept;
    const Rectangle& GetBounds() const noexcept;

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;
    Office(Id id, Point position, Offset offset) noexcept;
    const Id& GetId() const noexcept;
    Point GetPosition() const noexcept;
    Offset GetOffset() const noexcept;

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept;

    const Id& GetId() const noexcept;
    const std::string& GetName() const noexcept;
    const Buildings& GetBuildings() const noexcept;
    const Roads& GetRoads() const noexcept;
    const Offices& GetOffices() const noexcept;
    const Double GetDogSpeed() const noexcept;
    const int GetBagCapacity() const noexcept;;
    void AddRoad(const Road& road);
    void AddBuilding(const Building& building);
    void AddOffice(const Office & office);
    void SetDogSpeed(const Double& dog_speed);
    void SetBagCapacity(int bag_capacity);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    Double dog_speed_;
    int bag_capacity_;
};

class Loot {
public:
    Loot() = default;
    Loot(int loot_type, MapPoint position, int value);
    std::uint64_t GetId() const noexcept;
    void SetId(uint64_t id_);
    std::string GetIdStr() const noexcept;
    int GetType() const noexcept;
    MapPoint GetPos() const noexcept;
    int GetValue() const noexcept;
    void SetValue(int value);
    void UpdateLootCounter();;
    void SetType(int type);;
    void SetPos(MapPoint position);;

private:
    int loot_type_;
    MapPoint position_;
    std::uint64_t id;
    int value_;
    static std::atomic<std::uint64_t> loot_counter;
};

class Dog {
public:
    Dog();
    Dog(std::string name_);
    std::string GetName() const noexcept;
    void SetPos(const MapPoint& p);
    void SetSpeed(const MapSpeed& s);
    void SetDirection(const DIRECTION& d);
    void SetDirection(const std::string& d);
    void SetScore(int score);;
    void AddLoot(LootSharedPtr loot);
    void SetName(const std::string& name);
    void SetId(const std::uint64_t& id);
    std::vector<std::uint64_t> GetLootList();
    LootSharedPtr GetLoot(std::uint64_t id);
    void ReleaseLootBag();
    MapPoint GetPosition() const noexcept;
    MapSpeed GetSpeed() const noexcept;
    DIRECTION GetDirection() const noexcept;
    std::string GetDirStr() const ;
    uint64_t GetId() const noexcept;
    int GetScore()const noexcept;
    Loots GetLootInBag();
    void UpdateDogCounter();;
    void SetUUID(const std::string& uuid);;
    std::string GetUUID();
    long long NumberOfDogMoves();;

private:
    static std::atomic<std::uint64_t> dog_counter;
    std::uint64_t id_;
    std::string name_;
    MapPoint pos_;
    MapSpeed speed_;
    DIRECTION dir_;
    Loots bag_{Loots()};
    int score_;
    std::string uuid_;
    long long number_of_dog_moves_{ 0 };
};

class GameSession {
public:
    GameSession(MapSharedPtr map_);
    void AddDog(DogSharedPtr dog);
    void AddLoot(LootSharedPtr loot);
    std::uint64_t GetId() const noexcept;
    void SetId(std::uint64_t id_);
    MapSharedPtr GetMap() const noexcept;
    Dogs GetDogs() const noexcept;
    DogSharedPtr GetDog(std::uint64_t dog_id) const;
    Loots GetLoots() const noexcept;
    LootSharedPtr GetLoot(std::uint64_t id);
    void UpdateGameSessionCounter();
    void RemoveDog(std::uint64_t dog_id);
private:
    Dogs dogs;
    Loots loots;
    MapSharedPtr map;
    static std::atomic<std::uint64_t> session_counter;
    std::uint64_t id;
};

struct LootConfig {
    using Id = util::Tagged<std::string, Map>;
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToLootTypeCount = std::unordered_map<Id, int, MapIdHasher>;
    using MapIdToLootTypeToScores = std::unordered_map<Id, std::unordered_map<int, int>, MapIdHasher>;
    Double period_;
    Double probability_;
    MapIdToLootTypeCount map_id_to_loot_type_count_;
    MapIdToLootTypeToScores map_id_to_loot_type_to_scores_;
};

class Game {
    using TickerSaveState = Ticker<std::function<void(void)>>;
    void UpdatePositionAndBag(GameSessionSharedPtr& session, std::uint64_t timeDelta);
    void UpdateLoot(GameSessionSharedPtr& session, std::chrono::milliseconds timeDelta);
public:
    enum tick_state {
        TICK_TESTING_MODE = -1
    };
    enum save_state {
        STATE_FILE_WAS_NOT_SET = -1,
        STATE_PERIOD_WAS_NOT_SET = -2,

    };
    using Maps = std::vector<MapSharedPtr>;

    void AddMap(MapSharedPtr map);
    const Maps& GetMaps() const noexcept;
    MapSharedPtr FindMap(const Map::Id& id) const noexcept;
    std::optional<GameSessionSharedPtr> AddDogToSession(DogSharedPtr dog, const Map::Id& id);
    void SetDefaultDogSpeed(const Double& default_dog_speed);
    const Double DefaultDogSpeed() const noexcept;
    void SetDefaultBagCapacity(const int& bag_capacity);
    int DefaultBagCapacity();
    void Update(std::uint64_t timeDelta);
    void Update(std::chrono::milliseconds timeDelta);
    void SetTickPeriod(int tick_period_);
    int GetTickPeriod()const noexcept;
    void SetRandomizeSpawnPoint(int randomize_spawn_points_);
    int GetRandomizeSpawnPoint()const noexcept;
    void SetLootConfig(const LootConfig& config);
    const LootConfig& GetLootConfig() const noexcept;
    MapPoint GetRandomMapPointOnRoads(const Map::Id& id);
    void LoadState();
    void SaveState();
    void SetSaveFilePath(const std::string& path);
    void SetSaveStatePeriod(int save_state_period);;
    int GetSaveStatePeriod() noexcept;
    void PrepareSaveStateCycle(Strand & strand);;
    bool GetNeedToSaveState();
    void SetNeedToSaveState(bool need_to_save_state);;
    void SetDogRetirementTime(int dog_retirement_time);;
    const int GetDogRetirementTime() const noexcept;;
    void SetDBConnectionPool(ConnectionPoolPtr pool);
    ConnectionPoolPtr GetDBConnectionPool();
    DogSharedPtr GetDogByID(std::uint64_t dog_id);
            
private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<MapSharedPtr> maps_;
    std::unordered_map<Map::Id, GameSessionSharedPtr, MapIdHasher> sessions;
    MapIdToIndex map_id_to_index_;
    Double default_dog_speed_;
    int default_bag_capacity_;
    int tick_period;
    bool randomize_spawn_points;
    LootConfig loot_config_;
    int save_state_period_;
    bool need_to_save_state_{ false };
    std::shared_ptr<TickerSaveState> ticker_save_state_;
    std::string path_to_state_file_;
    int dog_retirement_time_;
    ConnectionPoolPtr pool_;
    std::map< std::uint64_t, std::uint64_t > not_active_dogs_;
    std::map< std::uint64_t, std::uint64_t > dog_in_game_time_;
};

}  // namespace model
