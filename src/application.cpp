#include "application.h"

namespace app {
	std::atomic<std::uint64_t> Player::player_counter{0};
	std::vector<PlayerSharedPtr> Players::players{};
	Token RandomToken::get() {

		std::random_device random_device_;
		std::mt19937_64 generator1_(random_device_());
		std::uniform_int_distribution<> dist(0, 15);
		std::stringstream ss;

		for (int i = 0; i < 32; i++) {
			ss << std::hex << dist(generator1_);
		}

		return Token(ss.str());
	}
	Player::Player() {
		id = 0;
	}
	Player::Player(DogSharedPtr dog_, GameSessionSharedPtr session_) :dog(dog_), session(session_) {
		token = RandomToken::get();
		id = player_counter++;
	}
	void Player::SetId(std::uint64_t id_) {
		id = id_;
	}
	Token Player::GetToken() const {
		return token;
	}
	void Player::SetToken(const std::string& token_) {
		token = Token(token_);
	}
	std::string Player::GetId() const {
		return std::to_string(id);
	}
	uint64_t Player::GetIdAsUInt64() const {
		return id;
	}
	std::string Player::GetName() const {
		return dog->GetName();
	}
	GameSessionSharedPtr Player::GetSession() const {
		return session;
	}
	DogSharedPtr Player::GetDog() const {
		return dog;
	}

	void Player::DoAction(ACTIONS action, const std::string& param) {
		if (action == ACTIONS::MOVE) {
			const auto s = session->GetMap()->GetDogSpeed();
			dog->SetDirection(param);

			switch (dog->GetDirection()) {
			case UP:   dog->SetSpeed({ 0,-s }); break;
			case DOWN: dog->SetSpeed({ 0, s }); break;
			case LEFT: dog->SetSpeed({ -s,0 }); break;
			case RIGHT:dog->SetSpeed({ s ,0 }); break;
			case NONE: dog->SetSpeed({ 0 ,0 }); break;
			}
		}
	}
	void Player::UpdatePlayerCounter() {
		if (id > player_counter) {
			player_counter = id;
		}
	}
	PlayerSharedPtr Players::AddPlayer(model::Game& game, const std::string& userName, const Map::Id& mapId) {
		auto newDog = DogSharedPtr(new Dog(userName));
		auto session = game.AddDogToSession(newDog, mapId);
		if (session == std::nullopt) {
			players.push_back(PlayerSharedPtr(new Player()));
		}
		else {
			auto p = PlayerSharedPtr(new Player(newDog, session.value()));
			players.push_back(p);
		}
		return players.back();
	}
	std::optional<PlayerSharedPtr> Players::FindPlayerByToken(const Token& token) {
		for (auto& p : players) {
			if (p->GetToken() == token) {
				return p;
			}
		}

		return std::nullopt;
	}
	std::vector<PlayerSharedPtr> Players::FindPlayersInSessionWithToken(const Token& token) {
		std::optional<PlayerSharedPtr> player = FindPlayerByToken(token);

		std::vector<PlayerSharedPtr> result;
		if (player == std::nullopt) {
			return result;
		}
		for (auto& p : players) {
			if ((p.get()->GetSession()).get()->GetId() == ((player.value())->GetSession()).get()->GetId()) {
				result.push_back(p);
			}
		}
		return result;
	}

	 std::vector<LootSharedPtr> Players::FindLootsInSessionWithPlayerToken(const Token& token) {
		 std::optional<PlayerSharedPtr> player = FindPlayerByToken(token);

		 std::vector<LootSharedPtr> result;
		 if (player == std::nullopt) {
			 return {};
		 }
		 result = player.value()->GetSession()->GetLoots();
		 return result;
	 }

	 void Players::RemovePlayerFromGameByDogId(std::uint64_t dog_id)
	 {
		 for (std::vector<PlayerSharedPtr>::iterator it = players.begin(); it != players.end();)
		 {
			 if (it->get()->GetDog()->GetId() == dog_id) {
				 it->get()->GetSession()->RemoveDog(dog_id);
				 players.erase(it);
				 break;
			 }
		 }
		 
		 return;
	 }
}//namespace app