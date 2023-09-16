#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sstream>

#include "../src/model.h"
#include "../src/model_serialization.h"

using namespace model;
using namespace std::literals;
namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace

SCENARIO_METHOD(Fixture, "Point serialization") {
    GIVEN("A point") {
        const geom::Point2D p{10, 20};
        WHEN("point is serialized") {
            output_archive << p;

            THEN("it is equal to point after serialization") {
                InputArchive input_archive{strm};
                geom::Point2D restored_point;
                input_archive >> restored_point;
                CHECK(p == restored_point);
            }
        }
    }
}

SCENARIO_METHOD(Fixture, "Dog Serialization") {
    GIVEN("a dog") {
         auto dog = [] {
            Dog dog{"Pluto"s};
            dog.SetId(42);
            dog.SetPos({ 42.2, 12.5 });
            dog.SetSpeed({ 2.3, -1.2 });
            dog.SetScore(42);
            dog.SetDirection(model::DIRECTION::UP);
            return dog;
        }();

        WHEN("dog is serialized") {
            {
                serialization::DogRepr repr(dog);
                output_archive << repr;
            }

            THEN("it can be deserialized") {
                InputArchive input_archive{strm};
                serialization::DogRepr repr;
                input_archive >> repr;
                const auto restored = repr.Restore();

                CHECK(dog.GetName() == restored.GetName());
                CHECK(dog.GetId() == restored.GetId());
                CHECK(dog.GetPosition() == restored.GetPosition());
                CHECK(dog.GetSpeed() == restored.GetSpeed());
                CHECK(dog.GetScore() == restored.GetScore());
                CHECK(dog.GetDirection() == restored.GetDirection());
            }
        }
    }
}
