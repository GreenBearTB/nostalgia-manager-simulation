#include "App.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>

#include "imgui.h"

namespace nm {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool contains(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return true;
    return lower(hay).find(lower(needle)) != std::string::npos;
}

// Pull the running score "(h-a)" out of a goal event string, if present.
bool parseScore(const std::string& text, int& h, int& a) {
    size_t open = text.rfind('(');
    size_t dash = text.find('-', open == std::string::npos ? 0 : open);
    size_t close = text.find(')', dash == std::string::npos ? 0 : dash);
    if (open == std::string::npos || dash == std::string::npos ||
        close == std::string::npos || dash < open || close < dash)
        return false;
    try {
        h = std::stoi(text.substr(open + 1, dash - open - 1));
        a = std::stoi(text.substr(dash + 1, close - dash - 1));
        return true;
    } catch (...) {
        return false;
    }
}

double playerOverall(const Player& p) { return PlayerAbility(p); }
}  // namespace

bool App::init(const std::string& dataDir) {
    dataDir_ = dataDir;
    cfg_.loadFile(dataDir_ + "/engine.cfg");
    if (!db_.load(dataDir_)) {
        status_ = "Failed to load database from " + dataDir_;
        return false;
    }
    leagues_ = db_.leagues();
    status_ = "Loaded " + std::to_string(db_.teams.size()) + " teams across " +
              std::to_string(leagues_.size()) + " leagues.";
    return true;
}

Team* App::teamById(int id) { return db_.findTeam(id); }

void App::beginScreen(const char* title) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##screen", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 230, 150, 255));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted(title);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
}

void App::render() {
    switch (screen_) {
        case Screen::Main: renderMain(); break;
        case Screen::Friendly: renderFriendly(); break;
        case Screen::Match: renderMatch(); break;
        case Screen::Database: renderDatabase(); break;
        case Screen::Career: renderCareer(); break;
        case Screen::Data: renderData(); break;
        case Screen::About: renderAbout(); break;
    }
}

void App::renderMain() {
    beginScreen("Nostalgia Manager Simulation");
    ImGui::TextDisabled("%s", status_.c_str());
    ImGui::Spacing();
    ImGui::Spacing();

    ImVec2 sz(280, 44);
    if (ImGui::Button("Friendly Match", sz)) screen_ = Screen::Friendly;
    ImGui::Spacing();
    if (ImGui::Button("Career", sz)) screen_ = Screen::Career;
    ImGui::Spacing();
    if (ImGui::Button("Database", sz)) screen_ = Screen::Database;
    ImGui::Spacing();
    if (ImGui::Button("Data Sources", sz)) {
        std::strncpy(playersPath_, "", sizeof(playersPath_));
        screen_ = Screen::Data;
    }
    ImGui::Spacing();
    if (ImGui::Button("About", sz)) screen_ = Screen::About;
    ImGui::Spacing();
    ImGui::Spacing();
    if (ImGui::Button("Quit", sz)) quit_ = true;

    ImGui::End();
}

void App::teamPicker(const char* id, int& leagueIdx, int& teamId, char* filter,
                     size_t filterSz) {
    ImGui::PushID(id);
    if (leagues_.empty()) {
        ImGui::TextDisabled("No leagues loaded.");
        ImGui::PopID();
        return;
    }
    if (leagueIdx < 0 || leagueIdx >= static_cast<int>(leagues_.size())) leagueIdx = 0;
    if (ImGui::BeginCombo("League", leagues_[leagueIdx].c_str())) {
        for (int i = 0; i < static_cast<int>(leagues_.size()); ++i) {
            bool sel = (i == leagueIdx);
            if (ImGui::Selectable(leagues_[i].c_str(), sel)) {
                leagueIdx = i;
                teamId = -1;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::InputTextWithHint("Filter", "type to filter teams", filter, filterSz);

    std::vector<Team*> teams = db_.teamsInLeague(leagues_[leagueIdx]);
    std::sort(teams.begin(), teams.end(),
              [](Team* a, Team* b) { return a->name < b->name; });
    ImGui::BeginChild("teamlist", ImVec2(360, 240), true);
    for (Team* t : teams) {
        if (!contains(t->name, filter)) continue;
        char label[160];
        std::snprintf(label, sizeof(label), "%s  (%d players)", t->name.c_str(),
                      static_cast<int>(t->squad.size()));
        if (ImGui::Selectable(label, teamId == t->id)) teamId = t->id;
    }
    ImGui::EndChild();
    ImGui::PopID();
}

void App::renderFriendly() {
    beginScreen("Friendly Match");
    if (ImGui::Button("< Back")) screen_ = Screen::Main;
    ImGui::Spacing();

    ImGui::Columns(2, "sel", false);
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1, 1), "Home");
    teamPicker("home", homeLeague_, homeTeam_, homeFilter_, sizeof(homeFilter_));
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(1, 0.7f, 0.5f, 1), "Away");
    teamPicker("away", awayLeague_, awayTeam_, awayFilter_, sizeof(awayFilter_));
    ImGui::Columns(1);

    ImGui::Spacing();
    Team* home = teamById(homeTeam_);
    Team* away = teamById(awayTeam_);
    ImGui::Text("Selected: %s  vs  %s",
                home ? home->name.c_str() : "(none)",
                away ? away->name.c_str() : "(none)");

    bool ok = home && away && home->id != away->id && home->squad.size() >= 7 &&
              away->squad.size() >= 7;
    if (!ok) ImGui::BeginDisabled();
    if (ImGui::Button("Kick Off", ImVec2(200, 40))) startMatch(home, away);
    if (!ok) ImGui::EndDisabled();
    if (home && away && home->id == away->id)
        ImGui::TextDisabled("Pick two different teams.");
    else if ((home && home->squad.size() < 7) || (away && away->squad.size() < 7))
        ImGui::TextDisabled("Both teams need at least 7 players.");

    ImGui::End();
}

void App::startMatch(Team* home, Team* away) {
    frames_.clear();
    playIdx_ = 0;
    playAccum_ = 0.0;
    playing_ = true;
    matchOver_ = false;
    matchHome_ = home->name;
    matchAway_ = away->name;

    MatchEngine engine(cfg_, static_cast<unsigned>(std::time(nullptr)));
    MatchEngine* ep = &engine;
    auto hook = [this, ep](const MatchEvent& e) {
        Frame f;
        f.text = e.text;
        f.key = e.key;
        f.minute = static_cast<int>(e.minute);  // already absolute 0-90
        f.pitch = ep->renderPitch();
        frames_.push_back(std::move(f));
    };
    MatchResult r = engine.simulate(*home, *away, false, hook);
    finalHG_ = r.homeGoals;
    finalAG_ = r.awayGoals;
    finalHS_ = r.homeShots;
    finalAS_ = r.awayShots;

    // Fill running score across frames from the goal annotations.
    int hg = 0, ag = 0;
    for (auto& f : frames_) {
        int h, a;
        if (f.text.rfind("GOAL!", 0) == 0 && parseScore(f.text, h, a)) {
            hg = h;
            ag = a;
        }
        f.hg = hg;
        f.ag = ag;
    }
    screen_ = Screen::Match;
}

void App::renderMatch() {
    // Advance playback timeline.
    if (playing_ && playIdx_ < frames_.size()) {
        playAccum_ += ImGui::GetIO().DeltaTime * speed_;
        while (playAccum_ >= 1.0 && playIdx_ < frames_.size()) {
            playAccum_ -= 1.0;
            ++playIdx_;
        }
    }
    if (playIdx_ >= frames_.size()) matchOver_ = true;

    size_t cur = playIdx_ == 0 ? 0 : playIdx_ - 1;
    const Frame* f = frames_.empty() ? nullptr : &frames_[std::min(cur, frames_.size() - 1)];

    beginScreen("Match Day");

    int hg = f ? f->hg : 0, ag = f ? f->ag : 0;
    int minute = f ? f->minute : 0;
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("%s  %d - %d  %s", matchHome_.c_str(), hg, ag, matchAway_.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    ImGui::TextDisabled("   %2d'", minute);
    ImGui::Separator();

    ImGui::BeginChild("pitch", ImVec2(560, 360), true);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 255, 200, 255));
    if (f) ImGui::TextUnformatted(f->pitch.c_str());
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("feed", ImVec2(0, 360), true);
    // Show the most recent events up to the current index.
    size_t shown = playIdx_;
    size_t start = shown > 16 ? shown - 16 : 0;
    for (size_t i = start; i < shown && i < frames_.size(); ++i) {
        const Frame& ev = frames_[i];
        if (ev.key)
            ImGui::TextColored(ImVec4(1, 0.9f, 0.3f, 1), "%2d'  %s", ev.minute,
                               ev.text.c_str());
        else
            ImGui::TextWrapped("%2d'  %s", ev.minute, ev.text.c_str());
    }
    if (playing_) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::Spacing();
    if (ImGui::Button(playing_ ? "Pause" : "Play")) playing_ = !playing_;
    ImGui::SameLine();
    if (ImGui::Button("Skip to End")) {
        playIdx_ = frames_.size();
        matchOver_ = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Speed", &speed_, 2.0f, 40.0f, "%.0f ev/s");

    if (matchOver_) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 1, 0.5f, 1), "Full Time:  %s %d - %d %s",
                           matchHome_.c_str(), finalHG_, finalAG_, matchAway_.c_str());
        ImGui::Text("Shots: %d - %d", finalHS_, finalAS_);
        ImGui::Spacing();
        if (ImGui::Button("Back to Menu", ImVec2(200, 36))) screen_ = Screen::Main;
    }

    ImGui::End();
}

void App::renderDatabase() {
    beginScreen("Database");
    if (ImGui::Button("< Back")) screen_ = Screen::Main;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(360);
    ImGui::InputTextWithHint("##search", "search players by name", dbSearch_,
                             sizeof(dbSearch_));
    ImGui::Spacing();

    std::vector<std::pair<const Team*, const Player*>> out;
    db_.searchPlayers(dbSearch_, out);
    int total = static_cast<int>(out.size());
    if (total > 300) out.resize(300);
    ImGui::TextDisabled("%d players matched%s", total,
                        total > 300 ? " (showing first 300)" : "");

    ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                         ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("players", 8, tf, ImVec2(0, 460))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Club");
        ImGui::TableSetupColumn("Pos");
        ImGui::TableSetupColumn("OVR");
        ImGui::TableSetupColumn("Pac");
        ImGui::TableSetupColumn("Sho");
        ImGui::TableSetupColumn("Pas");
        ImGui::TableSetupColumn("Tck");
        ImGui::TableHeadersRow();
        for (auto& pr : out) {
            const Team* t = pr.first;
            const Player* p = pr.second;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(p->name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(t ? t->name.c_str() : "-");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(RoleName(p->role).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.0f", playerOverall(*p));
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", p->attr.get("Pace"));
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%d", p->attr.get("Shooting"));
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%d", p->attr.get("Passing"));
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%d", p->attr.get("Tackling"));
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// --------------------------------------------------------------------------
// Career
// --------------------------------------------------------------------------
void App::careerStart(int teamId) {
    Team* t = teamById(teamId);
    if (!t) return;
    careerTeam_ = teamId;
    careerLeagueName_ = t->league;
    careerActive_ = true;
    careerRound_ = 0;
    table_.clear();
    careerLog_.clear();
    fixtures_.clear();
    roundStart_.clear();

    std::vector<Team*> teams = db_.teamsInLeague(careerLeagueName_);
    std::vector<int> ids;
    for (Team* tm : teams)
        if (tm->squad.size() >= 7) ids.push_back(tm->id);
    for (int id : ids) {
        Standing s;
        s.teamId = id;
        s.name = teamById(id)->name;
        table_[id] = s;
    }
    if (ids.size() < 2) {
        careerActive_ = false;
        status_ = "Not enough playable teams in " + careerLeagueName_;
        return;
    }
    // Round-robin (circle method). Add a bye if odd.
    bool bye = ids.size() % 2 != 0;
    if (bye) ids.push_back(-1);
    int n = static_cast<int>(ids.size());
    int rounds = n - 1;
    std::vector<int> arr = ids;
    for (int r = 0; r < rounds; ++r) {
        roundStart_.push_back(fixtures_.size());
        for (int i = 0; i < n / 2; ++i) {
            int a = arr[i];
            int b = arr[n - 1 - i];
            if (a != -1 && b != -1) {
                if (r % 2 == 0)
                    fixtures_.emplace_back(a, b);
                else
                    fixtures_.emplace_back(b, a);
            }
        }
        // rotate (keep first fixed)
        int last = arr[n - 1];
        for (int i = n - 1; i > 1; --i) arr[i] = arr[i - 1];
        arr[1] = last;
    }
    roundStart_.push_back(fixtures_.size());
    screen_ = Screen::Career;
}

void App::careerAdvance() {
    if (!careerActive_) return;
    if (careerRound_ + 1 >= static_cast<int>(roundStart_.size())) return;
    size_t from = roundStart_[careerRound_];
    size_t to = roundStart_[careerRound_ + 1];
    for (size_t i = from; i < to; ++i) {
        Team* h = teamById(fixtures_[i].first);
        Team* a = teamById(fixtures_[i].second);
        if (!h || !a) continue;
        MatchEngine engine(cfg_, 5000u + static_cast<unsigned>(i) + careerRound_ * 131u);
        MatchResult r = engine.simulate(*h, *a);
        Standing& sh = table_[h->id];
        Standing& sa = table_[a->id];
        sh.p++; sa.p++;
        sh.gf += r.homeGoals; sh.ga += r.awayGoals;
        sa.gf += r.awayGoals; sa.ga += r.homeGoals;
        if (r.homeGoals > r.awayGoals) { sh.w++; sh.pts += 3; sa.l++; }
        else if (r.homeGoals < r.awayGoals) { sa.w++; sa.pts += 3; sh.l++; }
        else { sh.d++; sa.d++; sh.pts++; sa.pts++; }
        if (h->id == careerTeam_ || a->id == careerTeam_) {
            char line[160];
            std::snprintf(line, sizeof(line), "R%d: %s %d-%d %s", careerRound_ + 1,
                          h->name.c_str(), r.homeGoals, r.awayGoals, a->name.c_str());
            careerLog_.push_back(line);
        }
    }
    careerRound_++;
}

void App::careerSave() {
    std::ofstream o(dataDir_ + "/career.sav");
    if (!o.is_open()) { status_ = "Could not write save."; return; }
    o << careerTeam_ << "\n" << careerLeagueName_ << "\n" << careerRound_ << "\n";
    for (auto& kv : table_) {
        const Standing& s = kv.second;
        o << s.teamId << " " << s.p << " " << s.w << " " << s.d << " " << s.l
          << " " << s.gf << " " << s.ga << " " << s.pts << "\n";
    }
    status_ = "Career saved.";
}

void App::careerLoad() {
    std::ifstream in(dataDir_ + "/career.sav");
    if (!in.is_open()) { status_ = "No save found."; return; }
    int teamId, round;
    in >> teamId;
    in.ignore();
    std::string league;
    std::getline(in, league);
    in >> round;
    careerStart(teamId);
    if (!careerActive_) return;
    careerRound_ = round;
    int id;
    while (in >> id) {
        Standing& s = table_[id];
        s.teamId = id;
        in >> s.p >> s.w >> s.d >> s.l >> s.gf >> s.ga >> s.pts;
        Team* t = teamById(id);
        if (t) s.name = t->name;
    }
    status_ = "Career loaded.";
    screen_ = Screen::Career;
}

void App::renderCareer() {
    beginScreen("Career");
    if (ImGui::Button("< Back")) screen_ = Screen::Main;
    ImGui::SameLine();
    if (ImGui::Button("Load Save")) careerLoad();

    if (!careerActive_) {
        ImGui::Spacing();
        ImGui::TextWrapped("Pick a club to manage. You'll play a full round-robin "
                           "season in its league.");
        teamPicker("career", careerLeague_, careerTeam_, careerFilter_,
                   sizeof(careerFilter_));
        Team* t = teamById(careerTeam_);
        bool ok = t && t->squad.size() >= 7;
        if (!ok) ImGui::BeginDisabled();
        if (ImGui::Button("Start Career", ImVec2(200, 40))) careerStart(careerTeam_);
        if (!ok) ImGui::EndDisabled();
        ImGui::End();
        return;
    }

    Team* myteam = teamById(careerTeam_);
    int totalRounds = static_cast<int>(roundStart_.size()) - 1;
    ImGui::Text("Managing: %s   (%s)", myteam ? myteam->name.c_str() : "?",
                careerLeagueName_.c_str());
    ImGui::Text("Round %d / %d", careerRound_, totalRounds);
    ImGui::SameLine();
    bool done = careerRound_ >= totalRounds;
    if (done) ImGui::BeginDisabled();
    if (ImGui::Button("Advance Round")) careerAdvance();
    if (done) ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Save")) careerSave();
    if (done) ImGui::SameLine(), ImGui::TextColored(ImVec4(1, 0.9f, 0.3f, 1), "Season complete!");

    ImGui::Spacing();
    // Standings sorted by points then goal difference.
    std::vector<Standing> rows;
    rows.reserve(table_.size());
    for (auto& kv : table_) rows.push_back(kv.second);
    std::sort(rows.begin(), rows.end(), [](const Standing& a, const Standing& b) {
        if (a.pts != b.pts) return a.pts > b.pts;
        int ga = a.gf - a.ga, gb = b.gf - b.ga;
        if (ga != gb) return ga > gb;
        return a.gf > b.gf;
    });

    ImGui::Columns(2, "careercols", false);
    ImGui::SetColumnWidth(0, 560);
    ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                         ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("table", 9, tf, ImVec2(0, 460))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("Club", ImGuiTableColumnFlags_WidthFixed, 220);
        const char* nums[] = {"P", "W", "D", "L", "GF", "GA", "Pts"};
        for (const char* c : nums)
            ImGui::TableSetupColumn(c, ImGuiTableColumnFlags_WidthFixed, 34);
        ImGui::TableHeadersRow();
        int pos = 1;
        for (const Standing& s : rows) {
            ImGui::TableNextRow();
            bool mine = s.teamId == careerTeam_;
            if (mine)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       IM_COL32(40, 80, 40, 255));
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", pos++);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(s.name.c_str());
            int vals[] = {s.p, s.w, s.d, s.l, s.gf, s.ga, s.pts};
            for (int i = 0; i < 7; ++i) {
                ImGui::TableSetColumnIndex(2 + i);
                ImGui::Text("%d", vals[i]);
            }
        }
        ImGui::EndTable();
    }
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1, 1), "Your results");
    ImGui::BeginChild("clog", ImVec2(0, 460), true);
    for (auto it = careerLog_.rbegin(); it != careerLog_.rend(); ++it)
        ImGui::TextWrapped("%s", it->c_str());
    ImGui::EndChild();
    ImGui::Columns(1);

    ImGui::End();
}

void App::renderData() {
    beginScreen("Data Sources");
    if (ImGui::Button("< Back")) screen_ = Screen::Main;
    ImGui::Spacing();
    ImGui::TextWrapped("Load your own Championship Manager / FM style exports. "
                       "Enter full paths to the players and clubs CSV files, then "
                       "press Load. Leave clubs blank to keep the current clubs.");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(700);
    ImGui::InputTextWithHint("Players CSV", "e.g. D:\\DEV\\Docs\\PlayersDB.csv",
                             playersPath_, sizeof(playersPath_));
    ImGui::SetNextItemWidth(700);
    ImGui::InputTextWithHint("Clubs CSV", "e.g. D:\\DEV\\Docs\\ClubsDB.csv",
                             clubsPath_, sizeof(clubsPath_));
    ImGui::Spacing();
    if (ImGui::Button("Load", ImVec2(160, 36))) {
        Database fresh;
        bool ok = true;
        if (clubsPath_[0]) ok = fresh.loadTeams(clubsPath_);
        if (ok && playersPath_[0]) ok = fresh.loadPlayers(playersPath_);
        if (ok && !fresh.teams.empty()) {
            db_.teams = std::move(fresh.teams);
            leagues_ = db_.leagues();
            homeTeam_ = awayTeam_ = careerTeam_ = -1;
            careerActive_ = false;
            status_ = "Loaded " + std::to_string(db_.teams.size()) + " teams from custom files.";
        } else {
            status_ = "Could not load the given files.";
        }
    }
    ImGui::Spacing();
    ImGui::TextDisabled("%s", status_.c_str());
    ImGui::End();
}

void App::renderAbout() {
    beginScreen("About");
    if (ImGui::Button("< Back")) screen_ = Screen::Main;
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Nostalgia Manager Simulation\n\n"
        "A retro football management game built on the GBNFM match engine: a "
        "two-phase (Desire then Execution) action model on a 9x13 pitch matrix, "
        "with zone-restricted actions, defensive pressure and configurable "
        "thresholds (data/engine.cfg).\n\n"
        "This window is a Dear ImGui front-end over the same engine, database "
        "loader (Championship Manager / FM CSV exports) and career logic.");
    ImGui::End();
}

}  // namespace nm
