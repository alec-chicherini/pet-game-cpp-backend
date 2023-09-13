#pragma once

#include "model.h"
#include "tagged.h"

#include <sstream>
#include <random>
#include <atomic>
#include <optional>
#include <map>

namespace detail {
	struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;
namespace app {
	using namespace std::literals;
	using namespace model;

	class Player;
	using PlayerSharedPtr = std::shared_ptr<Player>;
	using DogSharedPtr = std::shared_ptr<Dog>;

	class RandomToken {
	public:
		RandomToken() = delete;

		static Token get();
	};

	enum class ACTIONS {
		MOVE
	};

	class Player {
	public:
		Player();
		Player(DogSharedPtr dog_,GameSessionSharedPtr session_);
		void SetId(std::uint64_t id_);
		Token GetToken() const ;
		void SetToken(const std::string& token_);
		std::string GetId() const;
		uint64_t GetIdAsUInt64() const;
		std::string GetName() const;
		GameSessionSharedPtr GetSession() const;
		DogSharedPtr GetDog() const;
		void DoAction(ACTIONS action, const std::string& param);
		void UpdatePlayerCounter();;

	private:
		static std::atomic<std::uint64_t> player_counter;
		GameSessionSharedPtr session;
		DogSharedPtr dog;
		Token token{"00000000000000000000000000000000"s};
		uint64_t id;
	};

	class Players {
	public:
		Players() = delete;
		static PlayerSharedPtr AddPlayer(model::Game & game, const std::string& userName, const Map::Id& mapId);
		static std::optional<PlayerSharedPtr> FindPlayerByToken(const Token& token);
		static std::vector<PlayerSharedPtr> FindPlayersInSessionWithToken(const Token& token);
		static std::vector<LootSharedPtr> FindLootsInSessionWithPlayerToken(const Token& token);
		static std::vector<PlayerSharedPtr> players;
		static void RemovePlayerFromGameByDogId(std::uint64_t dogId);
	};
}//namespace app