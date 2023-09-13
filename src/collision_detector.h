#pragma once

#include "geom.h"

#include <algorithm>
#include <vector>

#include <type_traits>
#include <concepts>
#include <typeinfo>

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    // квадрат расстояния до точки
    double sq_distance;

    // доля пройденного отрезка
    double proj_ratio;
};

// Движемся из точки a в точку b и пытаемся подобрать точку c.
// Эта функция реализована в уроке.
CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c);

struct Item {
    geom::Point2D position;
    double width;
    std::uint64_t item_id_external;
};

struct Gatherer {
    geom::Point2D start_pos;
    geom::Point2D end_pos;
    double width;
    std::uint64_t gatherer_id_external;
};

class ItemGathererProvider {
protected:
    ~ItemGathererProvider() = default;

public:
    virtual size_t ItemsCount() const = 0;
    virtual Item GetItem(size_t idx) const = 0;
    virtual size_t GatherersCount() const = 0;
    virtual Gatherer GetGatherer(size_t idx) const = 0;
};

struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};

template<typename T>
concept ItemOrGatherer = std::is_same<collision_detector::Item, T>::value || std::is_same<collision_detector::Gatherer, T>::value;

struct ConcreteProvider :collision_detector::ItemGathererProvider {
    template<ItemOrGatherer... ARGS>
    ConcreteProvider(ARGS... args) {
        ([&] {
            if constexpr (std::is_same<collision_detector::Item, decltype(args)>::value) {
                AddItem(args);
            }
            else if constexpr (std::is_same<collision_detector::Gatherer, decltype(args)>::value) {
                AddGatherer(args);
            }
            }(), ...);
    };
    virtual size_t ItemsCount() const override final
    {
        return items_.size();
    }
    virtual collision_detector::Item GetItem(size_t idx) const override final
    {
        collision_detector::Item result;
        try {
            result = items_.at(idx);
        }
        catch (std::exception&) {
            result = collision_detector::Item();
        }
        return result;
    }
    virtual size_t GatherersCount() const override final
    {
        return gatherers_.size();
    }
    virtual collision_detector::Gatherer GetGatherer(size_t idx) const override final
    {
        collision_detector::Gatherer result;
        try {
            result = gatherers_.at(idx);
        }
        catch (std::exception&) {
            result = collision_detector::Gatherer();
        }
        return result;
    }
    size_t AddItem(collision_detector::Item item) {
        items_.push_back(item);
        return items_.size() - 1;
    }
    size_t AddGatherer(collision_detector::Gatherer gatherer) {
        gatherers_.push_back(gatherer);
        return gatherers_.size() - 1;
    }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
};

// Эту функцию вам нужно будет реализовать в соответствующем задании.
// При проверке ваших тестов она не нужна - функция будет линковаться снаружи.
std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector