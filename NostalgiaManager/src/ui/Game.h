#pragma once
#include <map>
#include <string>
#include <vector>

#include "../data/Database.h"
#include "../engine/Config.h"
#include "../engine/MatchEngine.h"

namespace nm {

// One scheduled league match.
struct Fixture {
    int homeId = 0;
    int awayId = 0;
    int round = 0;
    bool played = false;
    int homeGoals = 0;
    int awayGoals = 0;
};

// A row in the league table.
struct Standing {
    int teamId = 0;
    int p = 0, w = 0, d = 0, l = 0, gf = 0, ga = 0;
    int pts() const { return w * 3 + d; }
    int gd() const { return gf - ga; }
};

// Persistent state of a career (saved/loaded to .nms files).
struct Career {
    int controlledTeamId = 0;
    std::string league;
    int matchday = 0;
    std::vector<Fixture> fixtures;
    std::map<int, Standing> table;

    int totalRounds() const {
        int m = 0;
        for (const auto& f : fixtures) m = m > f.round ? m : f.round;
        return m;
    }
};

// Top-level application controlling the menu flow from Flow.drawio.
class Game {
public:
    Game(const std::string& dataDir);

    int run();

private:
    std::string dataDir_;
    Database db_;
    Config cfg_;
    bool dataLoaded_ = false;

    // Menu screens.
    void mainMenu();
    void friendlyMatch();
    void careerGame();
    void careerLoop(Career& car);
    void playMatchday(Career& car);
    void loadGameMenu();
    void editDatabase();

    // Shared helpers.
    Team* selectTeam(const std::string& prompt);
    void tactics(Team& team);
    MatchResult playMatch(Team& home, Team& away, bool watch);
    void showResult(const MatchResult& res);
    void showSquad(const Team& team);

    unsigned nextSeed();
};

}  // namespace nm
