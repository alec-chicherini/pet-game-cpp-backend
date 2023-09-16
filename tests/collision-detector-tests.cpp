#define _USE_MATH_DEFINES
#include <type_traits>
#include <concepts>
#include <iostream>
#include <typeinfo>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/collision_detector.h"

template<typename T>
concept ItemOrGatherer = std::is_same<collision_detector::Item, T>::value || std::is_same<collision_detector::Gatherer, T>::value;

class TestFixture {
public:
    class ConcreteProvider:public collision_detector::ItemGathererProvider {
    public:
        template<ItemOrGatherer... ARGS>
        ConcreteProvider(ARGS... args) {
            ([&]{
                if constexpr (std::is_same<collision_detector::Item, decltype(args)>::value) {
                    AddItem(args);
                }else if constexpr (std::is_same<collision_detector::Gatherer, decltype(args)>::value) {
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
};

SCENARIO("Test of ConcreteProvider and FindGatherEvents") {
    GIVEN(" A ConcreteProvider") {
        TestFixture::ConcreteProvider provider{
        collision_detector::Gatherer{ geom::Point2D(1.,1.),geom::Point2D(5.,1.), 0.5, 0},
        collision_detector::Item{ geom::Point2D(3.,1.), 0.5 ,0},  //+ have collision
        collision_detector::Item{ geom::Point2D(6.,1.), 0.5, 1},  //- not
        collision_detector::Item{ geom::Point2D(5.,1.), 0.5, 2}   //+
        };
        provider.AddItem({ geom::Point2D(5.,2.0000000001), 0.5 ,3 }); //-
        provider.AddItem({ geom::Point2D(5.,1.9999999999), 0.5 ,4 }); //+

        THEN("Items and gatherers count") {
            REQUIRE(provider.ItemsCount() == size_t(5));
            REQUIRE(provider.GatherersCount() == size_t(1));
        }

        WHEN("FindGatherEvents called") {
            auto result = FindGatherEvents(provider);

            THEN( "GatheringEvent count") {
                REQUIRE(result.size() == size_t(3));
            }

            THEN("GatheringEvent have right indexces") {
                CHECK(result[0].gatherer_id == size_t(0));
                CHECK(result[1].gatherer_id == size_t(0));
                CHECK(result[2].gatherer_id == size_t(0));

                CHECK(result[0].item_id == size_t(0));
                CHECK(result[1].item_id == size_t(2));
                CHECK(result[2].item_id == size_t(4));
            }

            THEN("GatheringEvent have right distance") {
                using Catch::Matchers::WithinAbs;
                CHECK_THAT(result[0].sq_distance, WithinAbs(0., 1e-10));
                CHECK_THAT(result[1].sq_distance, WithinAbs(0., 1e-10));
                CHECK_THAT(result[2].sq_distance, WithinAbs(0.9999999999, 1e-10));
            }
        }
    }
}
