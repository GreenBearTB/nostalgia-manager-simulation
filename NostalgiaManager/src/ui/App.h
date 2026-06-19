#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "../data/Database.h"
#include "../engine/Config.h"
#include "../engine/MatchEngine.h"

namespace nm {

// Dear ImGui application: the windowed front-end for Nostalgia Manager
// Simulation. It drives the same Config / Database / MatchEngine core used by
// the original console build, but presents everything as clickable screens.
class App {
public:
    bool init(const std::string& dataDir);
    void render();  // call once per frame
    bool wantQuit() const { return quit_; }

private:
    enum class Screen { Main, Friendly, Match, Database, Career, Data, About };

    // One snapshot of the match at the moment an event was narrated.
    struct Frame {
        std::string text;
        bool key = false;
        int minute = 0;
        std::string pitch;
        int hg = 0, ag = 0;
    };

    // Career standings row.
    struct Standing {
        int teamId = 0;
        std::string name;
        int p = 0, w = 0, d = 0, l = 0, gf = 0, ga = 0, pts = 0;
    };

    void beginScreen(const char* title);
    void renderMain();
    void renderFriendly();
    void renderMatch();
    void renderDatabase();
    void renderCareer();
    void renderData();
    void renderAbout();

    void teamPicker(const char* id, int& leagueIdx, int& teamId, char* filter,
                    size_t filterSz);
    void startMatch(Team* home, Team* away);
    Team* teamById(int id);

    // Career helpers
    void careerStart(int teamId);
    void careerAdvance();
    void careerSave();
    void careerLoad();

    Config cfg_;
    Database db_;
    std::string dataDir_;
    std::string status_;
    Screen screen_ = Screen::Main;
    bool quit_ = false;

    std::vector<std::string> leagues_;

    // Friendly selection
    int homeLeague_ = 0, awayLeague_ = 0;
    int homeTeam_ = -1, awayTeam_ = -1;
    char homeFilter_[64] = "";
    char awayFilter_[64] = "";

    // Match playback
    std::vector<Frame> frames_;
    std::string matchHome_, matchAway_;
    int finalHG_ = 0, finalAG_ = 0, finalHS_ = 0, finalAS_ = 0;
    size_t playIdx_ = 0;
    double playAccum_ = 0.0;
    float speed_ = 8.0f;  // events revealed per second
    bool playing_ = true;
    bool matchOver_ = false;

    // Database browse
    char dbSearch_[128] = "";

    // Custom data sources
    char playersPath_[512] = "";
    char clubsPath_[512] = "";

    // Career
    bool careerActive_ = false;
    int careerTeam_ = -1;
    int careerLeague_ = 0;
    char careerFilter_[64] = "";
    std::string careerLeagueName_;
    std::vector<std::pair<int, int>> fixtures_;  // flattened home,away pairs
    std::vector<size_t> roundStart_;             // index into fixtures_ per round
    int careerRound_ = 0;
    std::unordered_map<int, Standing> table_;
    std::vector<std::string> careerLog_;
};

}  // namespace nm
