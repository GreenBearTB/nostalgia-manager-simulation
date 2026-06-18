#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "data/Database.h"
#include "engine/MatchEngine.h"
#include "ui/Game.h"

namespace {
// Locate the data directory containing TeamsDB.csv, trying a few common spots
// so the game runs both from Visual Studio and from a terminal.
std::string findDataDir(int argc, char** argv) {
    std::vector<std::string> candidates;
    if (argc > 1) candidates.push_back(argv[1]);
    candidates.push_back("data");
    candidates.push_back("../data");
    candidates.push_back("../../data");
    candidates.push_back("NostalgiaManager/data");
    candidates.push_back("../NostalgiaManager/data");
    for (const auto& c : candidates) {
        std::ifstream f(c + "/TeamsDB.csv");
        if (f.is_open()) return c;
    }
    return "data";
}
}  // namespace

int main(int argc, char** argv) {
    std::string dataDir = findDataDir(argc, argv);

    // Utility: write the built-in match-engine weights to data/engine.cfg so
    // they can be inspected and tuned (spec section 12).
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--dump-config") {
            nm::Config cfg;
            std::string path = dataDir + "/engine.cfg";
            if (cfg.saveFile(path)) {
                std::printf("Wrote %s\n", path.c_str());
                return 0;
            }
            std::printf("Failed to write %s\n", path.c_str());
            return 1;
        }
    }

    // Utility: simulate ONE match and print every narrated event.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--trace") {
            nm::Database db;
            nm::Config cfg;
            cfg.loadFile(dataDir + "/engine.cfg");
            if (!db.load(dataDir) || db.teams.size() < 2) return 1;
            nm::MatchEngine engine(cfg, 7);
            nm::MatchResult r = engine.simulate(db.teams[0], db.teams[1]);
            for (const auto& e : r.events)
                std::printf("%4.0f' %s\n", e.minute, e.text.c_str());
            std::printf("Shots %d-%d  Goals %d-%d\n", r.homeShots, r.awayShots,
                        r.homeGoals, r.awayGoals);
            return 0;
        }
    }

    // Utility: simulate N random matches and print aggregate stats for balancing.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--bench") {
            int n = (i + 1 < argc) ? std::atoi(argv[i + 1]) : 50;
            if (n <= 0) n = 50;
            nm::Database db;
            nm::Config cfg;
            cfg.loadFile(dataDir + "/engine.cfg");
            if (!db.load(dataDir)) {
                std::printf("Could not load data from %s\n", dataDir.c_str());
                return 1;
            }
            double goals = 0, shots = 0;
            int games = 0;
            size_t nt = db.teams.size();
            for (int m = 0; m < n; ++m) {
                nm::Team& home = db.teams[m % nt];
                nm::Team& away = db.teams[(m + 1 + (m / nt)) % nt];
                if (home.id == away.id) continue;
                nm::MatchEngine engine(cfg, 1000u + m);
                nm::MatchResult r = engine.simulate(home, away);
                goals += r.homeGoals + r.awayGoals;
                shots += r.homeShots + r.awayShots;
                ++games;
            }
            std::printf("Benchmarked %d games\n", games);
            std::printf("Avg goals/match: %.2f\n", goals / games);
            std::printf("Avg shots/match: %.2f\n", shots / games);
            return 0;
        }
    }

    nm::Game game(dataDir);
    return game.run();
}
