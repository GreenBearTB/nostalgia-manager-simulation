#pragma once
#include <string>
#include <vector>

#include "../core/Team.h"

namespace nm {

// In-memory store of all teams (and their players / leagues) loaded from the
// CSV data files. The loader is header-driven and supports both the bundled
// sample format and Championship Manager / FM style exports (see Database.cpp).
class Database {
public:
    std::vector<Team> teams;

    // Loads using paths from data/datasources.cfg if present, else the bundled
    // TeamsDB.csv / PlayersDB.csv inside dataDir.
    bool load(const std::string& dataDir);

    bool loadTeams(const std::string& path);
    bool loadPlayers(const std::string& path);

    Team* findTeam(int id);
    Team* findTeamByName(const std::string& name);
    std::vector<std::string> leagues() const;
    std::vector<Team*> teamsInLeague(const std::string& league);

    // Used by the "Edit database" screen.
    void searchPlayers(const std::string& query,
                       std::vector<std::pair<const Team*, const Player*>>& out) const;
    void searchTeams(const std::string& query, std::vector<const Team*>& out) const;

private:
    int nextTeamId_ = 1;
};

}  // namespace nm
