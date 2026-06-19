#include "Game.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include "../core/Formation.h"
#include "Console.h"

namespace nm {

namespace {
// Circle-method round robin for an even/odd list of team ids.
std::vector<Fixture> roundRobin(std::vector<int> ids) {
    std::vector<Fixture> out;
    if (ids.size() < 2) return out;
    bool bye = ids.size() % 2 != 0;
    if (bye) ids.push_back(-1);
    int n = static_cast<int>(ids.size());
    int rounds = n - 1;
    for (int r = 0; r < rounds; ++r) {
        for (int i = 0; i < n / 2; ++i) {
            int a = ids[i];
            int b = ids[n - 1 - i];
            if (a != -1 && b != -1) {
                Fixture f;
                // alternate home/away for fairness
                if ((r + i) % 2 == 0) { f.homeId = a; f.awayId = b; }
                else { f.homeId = b; f.awayId = a; }
                f.round = r + 1;
                out.push_back(f);
            }
        }
        // rotate (keep first fixed)
        std::rotate(ids.begin() + 1, ids.begin() + n - 1, ids.end());
    }
    return out;
}
}  // namespace

Game::Game(const std::string& dataDir) : dataDir_(dataDir) {
    dataLoaded_ = db_.load(dataDir_);
    cfg_.loadFile(dataDir_ + "/engine.cfg");  // optional overrides
}

unsigned Game::nextSeed() {
    return static_cast<unsigned>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

int Game::run() {
    Console::header("NOSTALGIA MANAGER SIMULATION");
    if (!dataLoaded_) {
        Console::line("ERROR: could not load data files from: " + dataDir_);
        Console::line("Expected TeamsDB.csv and PlayersDB.csv.");
        return 1;
    }
    Console::line("Loaded " + std::to_string(db_.teams.size()) + " teams across " +
                  std::to_string(db_.leagues().size()) + " leagues.");
    Console::pause();
    mainMenu();
    return 0;
}

void Game::mainMenu() {
    while (true) {
        Console::header("MAIN MENU");
        Console::line("  1. Friendly match");
        Console::line("  2. Career game");
        Console::line("  3. Load game");
        Console::line("  4. Edit database");
        Console::line("  5. Exit");
        int c = Console::readInt("Select> ", 1, 5);
        switch (c) {
            case 1: friendlyMatch(); break;
            case 2: careerGame(); break;
            case 3: loadGameMenu(); break;
            case 4: editDatabase(); break;
            case 5:
            default:
                Console::line("Goodbye!");
                return;
        }
    }
}

Team* Game::selectTeam(const std::string& prompt) {
    auto leagues = db_.leagues();
    Console::header(prompt);
    for (size_t i = 0; i < leagues.size(); ++i)
        Console::line("  " + std::to_string(i + 1) + ". " + leagues[i]);
    int li = Console::readInt("Choose league> ", 1, static_cast<int>(leagues.size()));
    if (li < 1) return nullptr;
    auto teams = db_.teamsInLeague(leagues[li - 1]);
    std::sort(teams.begin(), teams.end(),
              [](Team* a, Team* b) { return a->name < b->name; });
    Console::line("");
    for (size_t i = 0; i < teams.size(); ++i)
        Console::line("  " + std::to_string(i + 1) + ". " + teams[i]->name +
                      "  (avg " + std::to_string(static_cast<int>(teams[i]->averageAbility())) +
                      ")");
    int ti = Console::readInt("Choose team> ", 1, static_cast<int>(teams.size()));
    if (ti < 1) return nullptr;
    return teams[ti - 1];
}

void Game::showSquad(const Team& team) {
    Console::header(team.name + " squad");
    Console::line("Formation: " + team.formation +
                  "   Mentality: " + MentalityName(team.mentality));
    std::cout << std::left << std::setw(4) << "No" << std::setw(22) << "Name"
              << std::setw(5) << "Pos" << "Ability\n";
    std::vector<const Player*> xi;
    for (int pid : team.startingXI)
        for (const auto& p : team.squad)
            if (p.id == pid) xi.push_back(&p);
    for (const Player* p : xi)
        std::cout << std::left << std::setw(4) << p->shirtNumber << std::setw(22)
                  << p->name << std::setw(5) << PosName(p->primaryPos)
                  << static_cast<int>(PlayerAbility(*p)) << "\n";
}

void Game::tactics(Team& team) {
    while (true) {
        Console::header("TACTICS - " + team.name);
        Console::line("Mentality: " + MentalityName(team.mentality));
        Console::line("Formation: " + team.formation);
        Console::line("");
        Console::line("  1. View starting XI");
        Console::line("  2. Set mentality (Defensive/Standard/Attack)");
        Console::line("  3. Make a substitution");
        Console::line("  4. Back");
        int c = Console::readInt("Select> ", 1, 4);
        if (c == 4 || c < 1) return;
        if (c == 1) {
            showSquad(team);
            Console::pause();
        } else if (c == 2) {
            int m = Console::readInt(
                "1=Defensive 2=Standard 3=Attack> ", 1, 3);
            team.mentality = (m == 1) ? Mentality::Defensive
                            : (m == 3) ? Mentality::Attack
                                       : Mentality::Standard;
        } else if (c == 3) {
            showSquad(team);
            int off = Console::readInt("Shirt number to take OFF> ", 1, 99);
            int on = Console::readInt("Shirt number to bring ON> ", 1, 99);
            int offId = -1, onId = -1;
            for (const auto& p : team.squad) {
                if (p.shirtNumber == off) offId = p.id;
                if (p.shirtNumber == on) onId = p.id;
            }
            bool onIsBench = onId != -1 &&
                std::find(team.startingXI.begin(), team.startingXI.end(), onId) ==
                    team.startingXI.end();
            auto it = std::find(team.startingXI.begin(), team.startingXI.end(), offId);
            if (offId != -1 && onIsBench && it != team.startingXI.end()) {
                *it = onId;
                team.subsUsed.push_back(onId);
                Console::line("Substitution made.");
            } else {
                Console::line("Invalid substitution.");
            }
            Console::pause();
        }
    }
}

MatchResult Game::playMatch(Team& home, Team& away, bool watch) {
    MatchEngine engine(cfg_, nextSeed());
    Console::header(home.name + "  vs  " + away.name);
    if (watch) Console::line("Watching live - key moments below:\n");

    MatchEngine::EventHook hook = nullptr;
    if (watch) {
        MatchEngine* ep = &engine;
        hook = [ep](const MatchEvent& e) {
            if (!e.key) return;
            std::cout << "  " << std::fixed << std::setprecision(0) << e.minute
                      << "'  " << e.text << "\n";
            bool isGoal = e.text.rfind("GOAL", 0) == 0;
            if (isGoal) {
                std::cout << ep->renderPitch() << "\n";
            }
        };
    }

    MatchResult res = engine.simulate(home, away, /*verbose=*/false, hook);
    return res;
}

void Game::showResult(const MatchResult& res) {
    Console::header("FULL TIME");
    Console::line("  " + res.homeName + "  " + std::to_string(res.homeGoals) +
                  " - " + std::to_string(res.awayGoals) + "  " + res.awayName);
    Console::line("  Shots: " + std::to_string(res.homeShots) + " - " +
                  std::to_string(res.awayShots));
    Console::line("");
    int g = Console::readInt(
        "1=Show full commentary  2=Continue> ", 1, 2);
    if (g == 1) {
        for (const auto& e : res.events)
            std::cout << "  " << std::fixed << std::setprecision(0) << e.minute
                      << "'  " << e.text << "\n";
        Console::pause();
    }
}

void Game::friendlyMatch() {
    Team* home = selectTeam("FRIENDLY - select your team");
    if (!home) return;
    Team* away = selectTeam("FRIENDLY - select opponent");
    if (!away) return;
    if (away == home) {
        Console::line("Pick two different teams.");
        Console::pause();
        return;
    }
    tactics(*home);
    int w = Console::readInt("1=Watch live  2=Quick result> ", 1, 2);
    MatchResult res = playMatch(*home, *away, w == 1);
    showResult(res);
}

// ---------------------------------------------------------------------------
// Career game
// ---------------------------------------------------------------------------
namespace {
void applyResult(Career& car, int homeId, int awayId, int hg, int ag) {
    Standing& h = car.table[homeId];
    Standing& a = car.table[awayId];
    h.teamId = homeId;
    a.teamId = awayId;
    ++h.p; ++a.p;
    h.gf += hg; h.ga += ag;
    a.gf += ag; a.ga += hg;
    if (hg > ag) { ++h.w; ++a.l; }
    else if (hg < ag) { ++a.w; ++h.l; }
    else { ++h.d; ++a.d; }
}

void printTable(const Career& car, Database& db) {
    std::vector<Standing> rows;
    for (const auto& kv : car.table) rows.push_back(kv.second);
    std::sort(rows.begin(), rows.end(), [](const Standing& a, const Standing& b) {
        if (a.pts() != b.pts()) return a.pts() > b.pts();
        if (a.gd() != b.gd()) return a.gd() > b.gd();
        return a.gf > b.gf;
    });
    Console::header("LEAGUE TABLE - " + car.league);
    std::cout << std::left << std::setw(4) << "#" << std::setw(22) << "Team"
              << std::setw(4) << "P" << std::setw(4) << "W" << std::setw(4) << "D"
              << std::setw(4) << "L" << std::setw(6) << "GD" << "Pts\n";
    int pos = 1;
    for (const auto& s : rows) {
        Team* t = db.findTeam(s.teamId);
        std::cout << std::left << std::setw(4) << pos++ << std::setw(22)
                  << (t ? t->name : "?") << std::setw(4) << s.p << std::setw(4) << s.w
                  << std::setw(4) << s.d << std::setw(4) << s.l << std::setw(6)
                  << s.gd() << s.pts() << "\n";
    }
}

bool saveCareer(const Career& car, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << "NMS_SAVE 1\n";
    out << car.controlledTeamId << "\n";
    out << car.league << "\n";
    out << car.matchday << "\n";
    out << car.fixtures.size() << "\n";
    for (const auto& f : car.fixtures)
        out << f.homeId << " " << f.awayId << " " << f.round << " " << f.played
            << " " << f.homeGoals << " " << f.awayGoals << "\n";
    out << car.table.size() << "\n";
    for (const auto& kv : car.table) {
        const Standing& s = kv.second;
        out << s.teamId << " " << s.p << " " << s.w << " " << s.d << " " << s.l
            << " " << s.gf << " " << s.ga << "\n";
    }
    return true;
}

bool loadCareer(Career& car, const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    std::string magic;
    int ver = 0;
    in >> magic >> ver;
    if (magic != "NMS_SAVE") return false;
    in >> car.controlledTeamId;
    in.ignore();
    std::getline(in, car.league);
    in >> car.matchday;
    size_t nf = 0;
    in >> nf;
    car.fixtures.clear();
    for (size_t i = 0; i < nf; ++i) {
        Fixture f;
        int played = 0;
        in >> f.homeId >> f.awayId >> f.round >> played >> f.homeGoals >> f.awayGoals;
        f.played = played != 0;
        car.fixtures.push_back(f);
    }
    size_t nt = 0;
    in >> nt;
    car.table.clear();
    for (size_t i = 0; i < nt; ++i) {
        Standing s;
        in >> s.teamId >> s.p >> s.w >> s.d >> s.l >> s.gf >> s.ga;
        car.table[s.teamId] = s;
    }
    return true;
}
}  // namespace

void Game::careerGame() {
    Team* mine = selectTeam("CAREER - choose the team you will manage");
    if (!mine) return;
    Career car;
    car.controlledTeamId = mine->id;
    car.league = mine->league;
    std::vector<int> ids;
    for (Team* t : db_.teamsInLeague(car.league)) {
        ids.push_back(t->id);
        car.table[t->id].teamId = t->id;
    }
    car.fixtures = roundRobin(ids);

    Console::line("Career started with " + mine->name + " in " + car.league + ".");
    Console::line("Season fixtures: " + std::to_string(car.fixtures.size()) +
                  " across " + std::to_string(car.totalRounds()) + " match days.");
    Console::pause();

    // Hand off to the shared career main loop.
    careerLoop(car);
}

// Implemented below the class to keep careerGame tidy.
void Game::careerLoop(Career& car) {
    while (true) {
        Console::header("CAREER MAIN - " +
                        (db_.findTeam(car.controlledTeamId)
                             ? db_.findTeam(car.controlledTeamId)->name
                             : "?"));
        int round = car.matchday + 1;
        bool seasonOver = car.matchday >= car.totalRounds();
        Console::line(seasonOver ? "Season complete."
                                 : "Next match day: " + std::to_string(round) +
                                       " / " + std::to_string(car.totalRounds()));
        Console::line("");
        Console::line("  1. View league table");
        Console::line("  2. Tactics");
        Console::line(seasonOver ? "  3. (season over)" : "  3. Play next match day");
        Console::line("  4. Transfers");
        Console::line("  5. Save game");
        Console::line("  6. Exit to main menu");
        int c = Console::readInt("Select> ", 1, 6);
        if (c == 6 || c < 1) return;
        if (c == 1) {
            printTable(car, db_);
            Console::pause();
        } else if (c == 2) {
            if (Team* t = db_.findTeam(car.controlledTeamId)) tactics(*t);
        } else if (c == 3 && !seasonOver) {
            playMatchday(car);
        } else if (c == 4) {
            Console::header("TRANSFERS");
            Console::line("Transfer market is a work-in-progress in this build.");
            Console::line("(Career structure, fixtures, table and saves are live.)");
            Console::pause();
        } else if (c == 5) {
            std::string name = Console::readLine("Save name (e.g. mycareer)> ");
            if (name.empty()) name = "career";
            std::string path = dataDir_ + "/../saves/" + name + ".nms";
            if (saveCareer(car, path))
                Console::line("Saved to " + path);
            else
                Console::line("Could not write save (does the saves/ folder exist?).");
            Console::pause();
        }
    }
}

void Game::playMatchday(Career& car) {
    int round = car.matchday + 1;
    Team* mine = db_.findTeam(car.controlledTeamId);
    for (auto& f : car.fixtures) {
        if (f.round != round || f.played) continue;
        if (f.homeId == car.controlledTeamId || f.awayId == car.controlledTeamId) {
            Team* home = db_.findTeam(f.homeId);
            Team* away = db_.findTeam(f.awayId);
            if (!home || !away) continue;
            if (mine) tactics(*mine);
            int w = Console::readInt("1=Watch live  2=Quick result> ", 1, 2);
            MatchResult res = playMatch(*home, *away, w == 1);
            f.homeGoals = res.homeGoals;
            f.awayGoals = res.awayGoals;
            f.played = true;
            applyResult(car, f.homeId, f.awayId, f.homeGoals, f.awayGoals);
            showResult(res);
        } else {
            Team* home = db_.findTeam(f.homeId);
            Team* away = db_.findTeam(f.awayId);
            if (!home || !away) continue;
            MatchEngine engine(cfg_, nextSeed());
            MatchResult res = engine.simulate(*home, *away);
            f.homeGoals = res.homeGoals;
            f.awayGoals = res.awayGoals;
            f.played = true;
            applyResult(car, f.homeId, f.awayId, f.homeGoals, f.awayGoals);
        }
    }
    ++car.matchday;
    Console::header("MATCH DAY " + std::to_string(round) + " RESULTS");
    for (const auto& f : car.fixtures) {
        if (f.round != round) continue;
        Team* h = db_.findTeam(f.homeId);
        Team* a = db_.findTeam(f.awayId);
        std::cout << "  " << std::left << std::setw(20) << (h ? h->name : "?")
                  << " " << f.homeGoals << " - " << f.awayGoals << "  "
                  << (a ? a->name : "?") << "\n";
    }
    Console::pause();
    printTable(car, db_);
    Console::pause();
}

void Game::loadGameMenu() {
    Console::header("LOAD GAME");
    std::string name = Console::readLine("Save name to load (e.g. mycareer)> ");
    if (name.empty()) return;
    std::string path = dataDir_ + "/../saves/" + name + ".nms";
    Career car;
    if (!loadCareer(car, path)) {
        Console::line("Could not load save: " + path);
        Console::pause();
        return;
    }
    Console::line("Loaded career for " +
                  (db_.findTeam(car.controlledTeamId)
                       ? db_.findTeam(car.controlledTeamId)->name
                       : "?"));
    Console::pause();
    careerLoop(car);
}

void Game::editDatabase() {
    while (true) {
        Console::header("EDIT DATABASE");
        Console::line("  1. Search players");
        Console::line("  2. Search teams");
        Console::line("  3. Back");
        int c = Console::readInt("Select> ", 1, 3);
        if (c == 3 || c < 1) return;
        if (c == 1) {
            std::string q = Console::readLine("Player name contains> ");
            std::vector<std::pair<const Team*, const Player*>> hits;
            db_.searchPlayers(q, hits);
            Console::line("Found " + std::to_string(hits.size()) + " players:");
            int shown = 0;
            for (const auto& h : hits) {
                if (shown++ >= 40) { Console::line("  ...(truncated)"); break; }
                std::cout << "  " << std::left << std::setw(22) << h.second->name
                          << std::setw(5) << PosName(h.second->primaryPos) << "ability "
                          << static_cast<int>(PlayerAbility(*h.second)) << "  ("
                          << h.first->name << ")\n";
            }
            Console::pause();
        } else if (c == 2) {
            std::string q = Console::readLine("Team name contains> ");
            std::vector<const Team*> hits;
            db_.searchTeams(q, hits);
            for (const Team* t : hits)
                std::cout << "  " << std::left << std::setw(24) << t->name
                          << t->league << "  (" << t->squad.size() << " players, avg "
                          << static_cast<int>(t->averageAbility()) << ")\n";
            Console::pause();
        }
    }
}

}  // namespace nm
