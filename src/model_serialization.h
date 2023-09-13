#pragma once
#include <boost/serialization/vector.hpp>

#include "model.h"
#include "application.h"

namespace geom {

template <typename Archive>
void serialize(Archive& ar, Point2D& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, Vec2D& vec, [[maybe_unused]] const unsigned version) {
    ar& vec.x;
    ar& vec.y;
}

}  // namespace geom


namespace model {

template <typename Archive>
void serialize(Archive& ar, model::MapPoint& point, [[maybe_unused]] const unsigned version) {
    ar& point.x;
    ar& point.y;
}

template <typename Archive>
void serialize(Archive& ar, MapSpeed& point, [[maybe_unused]] const unsigned version) {
    ar& point.dx;
    ar& point.dy;
}

template <typename Archive>
void serialize(Archive& ar, model::DIRECTION& dir, [[maybe_unused]] const unsigned version) {
    ar& dir;
}

}  // namespace model

namespace serialization {

    using DogSharedPtr = std::shared_ptr<model::Dog>;
    using GameSessionSharedPtr = std::shared_ptr<model::GameSession>;
    using PlayerSharedPtr = std::shared_ptr<app::Player>;
    using LootSharedPtr = std::shared_ptr<model::Loot>;

    // LootRepr (LootsRepresentation) - сериализованное представление класса Loot
    class LootRepr {

    public:
        LootRepr() = default;

        explicit LootRepr(LootSharedPtr loot)
            : id_(loot->GetId())
            , loot_type_(loot->GetType())
            , position_(loot->GetPos())
            , value_(loot->GetValue())
        {
        }

        [[nodiscard]] LootSharedPtr Restore() const {
            LootSharedPtr result=std::make_shared<model::Loot>();

            result->SetId(id_);
            result->SetType(loot_type_);
            result->SetPos(position_);
            result->SetValue(value_);
            return result;
        }

        template <typename Archive>
        void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
            ar& id_;
            ar& loot_type_;
            ar& position_;
            ar& value_;
        }

    private:
        std::uint64_t id_;
        int loot_type_;
        model::MapPoint position_;
        int value_;
    };

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(model::Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , pos_(dog.GetPosition())
        , speed_(dog.GetSpeed())
        , dir_(dog.GetDirection())
        , score_(dog.GetScore())
        {
        for (auto l : dog.GetLootInBag()) {
            id_loots_in_bag_.push_back(l->GetId());
        }
    }

    [[nodiscard]] model::Dog Restore(std::vector< LootSharedPtr>& loots_restored) const {
        model::Dog dog{name_};
        dog.SetId(id_);
        dog.SetPos(pos_);
        dog.SetSpeed(speed_);
        dog.SetDirection(dir_);
        dog.SetScore(score_);

        int count_loots_was_found = 0;

        for (auto& l_restored : loots_restored)
            for (auto& l_id : id_loots_in_bag_) {
                if (l_restored->GetId() == l_id) {
                    count_loots_was_found += 1;
                    dog.AddLoot(l_restored);
                }
            }

        if (count_loots_was_found != id_loots_in_bag_.size()) {
            throw std::logic_error("ERROR: not all loots was found");
        }

        dog.UpdateDogCounter();
        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& pos_;
        ar& speed_;
        ar& dir_;
        ar& id_loots_in_bag_;
        ar& score_;
    }

private:
    std::uint64_t id_;
    std::string name_;
    model::MapPoint pos_;
    model::MapSpeed speed_;
    model::DIRECTION dir_;
    std::vector<std::uint64_t> id_loots_in_bag_;
    int score_;
};

// PlayerRepr (PlayerRepresentation) - сериализованное представление класса Player
class PlayerRepr {

public:
    PlayerRepr() = default;

    explicit PlayerRepr(PlayerSharedPtr player)
        : id_(player->GetIdAsUInt64())
        , id_dog_(player->GetDog()->GetId())
        , id_session_(player->GetSession()->GetId())
        , token_(*(player->GetToken()))
    {
    }

    [[nodiscard]] PlayerSharedPtr Restore(std::vector<DogSharedPtr>& dogs, std::vector<GameSessionSharedPtr>& sessions) const {
        DogSharedPtr dog_;
        GameSessionSharedPtr session_;
        bool dog_was_found = false;
        bool session_was_found = false;

        for (auto& d : dogs) {
            if (d->GetId() == id_dog_) {
                dog_ = d;
                dog_was_found = true;
                break;
            }
        }

        for (auto& s : sessions) {
            if (s->GetId() == id_session_) {
                session_ = s;
                session_was_found = true;
                break;
            }
        }

        if (dog_was_found == false) {
            throw std::logic_error("ERROR: dog not found.");
        }
        if (session_was_found == false) {
            throw std::logic_error("ERROR: session not found.");
        }

        PlayerSharedPtr player = std::make_shared<app::Player>(dog_, session_);
        player->SetId(id_);
        player->SetToken(token_);
        player->UpdatePlayerCounter();
        return player;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& id_dog_;
        ar& id_session_;
        ar& token_;
    }

private:
    std::uint64_t id_;
    std::uint64_t id_dog_;
    std::uint64_t id_session_;
    std::string token_;
};

// GameSessionRepr (GameSessionRepresentation) - сериализованное представление класса GameSession
class GameSessionRepr {
public:
    GameSessionRepr() = default;

    explicit GameSessionRepr(GameSessionSharedPtr gameSession)
        : id_(gameSession->GetId()),
          id_map_(*(gameSession->GetMap()->GetId()))
    {
        for (auto& l : gameSession->GetLoots()) {
            id_loots_.push_back(l->GetId());
        }

        for (auto& d : gameSession->GetDogs()) {
            id_dogs_.push_back(d->GetId());
        }
    }

    [[nodiscard]] GameSessionSharedPtr Restore(const model::Game& game, std::vector<DogSharedPtr>& dogs_restored, std::vector< LootSharedPtr>& loots_restored) const {
        auto map = game.FindMap(model::Map::Id(id_map_));
        if (map == nullptr) {
            throw std::logic_error("ERROR: map not found.");
        }
        GameSessionSharedPtr session_=std::make_shared<model::GameSession>(map);

        session_->SetId(id_);
        session_->UpdateGameSessionCounter();

        int count_loots_was_found = 0;

        for (auto& l_restored : loots_restored)
            for (auto& l_id : id_loots_) {
                if (l_restored->GetId() == l_id) {
                    count_loots_was_found += 1;
                    session_->AddLoot(l_restored);
                }
            }

        if (count_loots_was_found != id_loots_.size()) {
            throw std::logic_error("ERROR: not all loots was found");
        }

        int count_dogs_was_found = 0;

        for (auto& d_restored : dogs_restored)
            for (auto& d_id : id_dogs_) {
                if (d_restored->GetId() == d_id) {
                    count_dogs_was_found += 1;
                    session_->AddDog(d_restored);
                }
            }

        if (count_dogs_was_found != id_dogs_.size()) {
            throw std::logic_error("ERROR: not all dogs was found");
        }

        return session_;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& id_map_;
        ar& id_loots_;
        ar& id_dogs_;
    }

private:
    std::uint64_t id_;
    std::string id_map_;
    std::vector<std::uint64_t> id_loots_;
    std::vector<std::uint64_t> id_dogs_;
};

}  // namespace serialization
