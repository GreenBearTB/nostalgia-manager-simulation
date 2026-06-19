#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"

#include "../data/Database.h"
#include "../engine/Config.h"
#include "../engine/MatchEngine.h"

namespace nm {

// A GPU texture handle. Created by the active rendering backend (OpenGL3 or
// Direct3D 11) via AppLoadTexture, which each platform entry point defines.
struct AppTexture {
    ImTextureID id = (ImTextureID)0;
    int w = 0;
    int h = 0;
    bool ok = false;
};

// Loads a PNG/JPG file into a GPU texture. Implemented per backend in the
// platform entry point (main_glfw.cpp / main_win32.cpp).
bool AppLoadTexture(const std::string& path, AppTexture* out);

// Applies the shared "nostalgia" visual style (colors, rounding, spacing).
void ApplyNostalgiaTheme();

// Dear ImGui application: the windowed front-end for Nostalgia Manager
// Simulation. It drives the same Config / Database / MatchEngine core used by
// the original console build, but presents everything as clickable screens.
class App {
public:
    bool init(const std::string& dataDir);
    void render();  // call once per frame
    bool wantQuit() const { return quit_; }

private:
    enum class Screen { Main, Friendly, Tactics, Match, Database, Career, Data, About };

    // One snapshot of the match at the moment an event was narrated.
    struct Frame {
        std::string text;
        bool key = false;
        int minute = 0;
        std::string pitch;
        int hg = 0, ag = 0;
        int ballCol = 7, ballRow = 5;  // 1..13 (length), 1..9 (width)
        int carrier = 0;               // 0 home, 1 away
        MatchStats stats;              // cumulative stats at this frame
    };

    // Career standings row.
    struct Standing {
        int teamId = 0;
        std::string name;
        int p = 0, w = 0, d = 0, l = 0, gf = 0, ga = 0, pts = 0;
    };

    void beginScreen(const char* title);
    void beginFullscreen(const char* id, bool withBackground);
    void renderMain();
    void renderFriendly();
    void renderTactics();
    void renderMatch();
    void drawPitch(ImVec2 pos, ImVec2 size, const Frame* f);
    void renderDatabase();
    void renderCareer();
    void renderData();
    void renderAbout();

    void teamPicker(const char* id, int& leagueIdx, int& teamId, char* filter,
                    size_t filterSz);
    void startMatch(Team* home, Team* away);
    void openTactics(Team* team, Screen returnTo);
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

    AppTexture menuBg_;
    std::vector<std::string> leagues_;

    // Tactics screen
    Team* tacticsTeam_ = nullptr;
    Screen tacticsReturn_ = Screen::Friendly;
    int tacticsXiSel_ = -1;   // selected starter (player id) for a swap
    int tacticsSubSel_ = -1;  // selected substitute (player id) for a swap
    int matchSubsUsed_ = 0;   // substitutions made in the current match (max 3)
    static constexpr int kMaxMatchSubs = 3;

    // Friendly selection
    int homeLeague_ = 0, awayLeague_ = 0;
    int homeTeam_ = -1, awayTeam_ = -1;
    char homeFilter_[64] = "";
    char awayFilter_[64] = "";

    // Match playback
    std::vector<Frame> frames_;
    std::string matchHome_, matchAway_;
    int finalHG_ = 0, finalAG_ = 0, finalHS_ = 0, finalAS_ = 0;
    std::vector<std::pair<int, std::string>> homeScorers_, awayScorers_;
    Team* matchHomeTeam_ = nullptr;
    Team* matchAwayTeam_ = nullptr;
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
