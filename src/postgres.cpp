#include "postgres.h"

namespace postgres {
    void Database::Init(std::shared_ptr<ConnectionPool> pool) {
            auto connection_ = pool->GetConnection();
            pqxx::work work{ *connection_ };
            work.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players  (
            id UUID PRIMARY KEY,
            name VARCHAR(100),
            score int,
            play_time_ms int);
            )"_zv);
            work.commit();
        }

     std::vector<PlayerRecord> Database::GetPlayersRecords(std::shared_ptr<ConnectionPool> pool, int start, int maxItems) {
        std::vector< PlayerRecord > result;
        auto connection_ = pool->GetConnection();
        pqxx::work work{ *connection_ };
    
        auto query_text = "SELECT id, name, score, play_time_ms FROM retired_players ORDER BY score desc, play_time_ms, name OFFSET "s;
        query_text += (std::to_string(start));
        query_text += (" LIMIT ");
        query_text += (std::to_string(maxItems));
        query_text += (";");
        pqxx::zview query_zv_(query_text);
    
        for (auto& [id_uuid, name, score, playTime_ms] : work.query<std::string, std::string, int, int>(query_zv_))
        {
            result.push_back({ id_uuid, name, score, playTime_ms });
        }
        return result;
    }

     void Database::SaveRecord(std::shared_ptr<ConnectionPool> pool, PlayerRecord record) {
        auto connection_ = pool->GetConnection();
        pqxx::work work{ *connection_ };
        work.exec_params(R"(INSERT INTO retired_players (id, name, score, play_time_ms)
VALUES($1, $2, $3, $4) 
ON CONFLICT (id) 
DO UPDATE SET name = excluded.name, score = excluded.score, play_time_ms = excluded.play_time_ms;)"_zv, record.id_uuid, record.name, record.score, record.playTime_ms);
        work.commit();
    }
};