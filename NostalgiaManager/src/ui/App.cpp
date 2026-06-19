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

// Shade an RGBA colour by a multiplier (clamped), keeping alpha.
ImU32 shade(ImU32 c, float m) {
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    auto cl = [](float x) { return x < 0 ? 0.f : (x > 1 ? 1.f : x); };
    return ImGui::ColorConvertFloat4ToU32(ImVec4(cl(v.x * m), cl(v.y * m), cl(v.z * m), v.w));
}

// A glossy, beveled coloured button matching the start-menu art.
bool tintButton(const char* label, ImU32 base, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, shade(base, 1.25f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, shade(base, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, shade(base, 1.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    bool r = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    return r;
}

// Section header inside a panel: a coloured bar with centred title.
void panelHeader(const char* title, ImU32 col = IM_COL32(120, 70, 40, 255)) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetTextLineHeight() + 10;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col, 4.0f);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(210, 170, 90, 255), 4.0f, 0, 1.5f);
    ImVec2 ts = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + 5), IM_COL32(245, 225, 170, 255), title);
    ImGui::Dummy(ImVec2(w, h + 4));
}

// Squad list panel (Starting XI then Substitutes), as in the match mock-up.
void squadPanel(const char* id, const char* title, const Team* t, const ImVec2& size) {
    ImGui::BeginChild(id, size, true);
    panelHeader(title);
    if (!t) {
        ImGui::TextDisabled("-");
        ImGui::EndChild();
        return;
    }
    std::vector<const Player*> ps;
    for (const auto& p : t->squad) ps.push_back(&p);
    std::sort(ps.begin(), ps.end(),
              [](const Player* a, const Player* b) { return a->shirtNumber < b->shirtNumber; });
    int i = 0;
    ImGui::TextColored(ImVec4(0.86f, 0.78f, 0.55f, 1), "Starting XI");
    for (const Player* p : ps) {
        if (i == 11) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.86f, 0.78f, 0.55f, 1), "Substitutes");
        }
        if (i >= 18) break;
        ImGui::Text("%2d  %-3s %s", p->shirtNumber, RoleName(p->role).c_str(),
                    p->name.c_str());
        ++i;
    }
    ImGui::EndChild();
}
}  // namespace

void ApplyNostalgiaTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.FrameRounding = 5.0f;
    s.GrabRounding = 4.0f;
    s.ChildRounding = 6.0f;
    s.PopupRounding = 5.0f;
    s.FrameBorderSize = 1.0f;
    s.WindowBorderSize = 0.0f;
    s.FramePadding = ImVec2(10, 6);
    s.ItemSpacing = ImVec2(10, 8);
    s.ScrollbarSize = 14.0f;

    ImVec4* c = s.Colors;
    auto col = [](int r, int g, int b, int a = 255) {
        return ImVec4(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    };
    c[ImGuiCol_WindowBg]        = col(24, 30, 22);       // dark pitch green
    c[ImGuiCol_ChildBg]         = col(34, 30, 24, 235);  // leather
    c[ImGuiCol_PopupBg]         = col(28, 26, 22, 245);
    c[ImGuiCol_Border]          = col(120, 92, 48);      // muted gold
    c[ImGuiCol_FrameBg]         = col(48, 42, 32);
    c[ImGuiCol_FrameBgHovered]  = col(64, 56, 40);
    c[ImGuiCol_FrameBgActive]   = col(74, 64, 46);
    c[ImGuiCol_TitleBgActive]   = col(40, 34, 26);
    c[ImGuiCol_Button]          = col(58, 78, 52);
    c[ImGuiCol_ButtonHovered]   = col(78, 104, 66);
    c[ImGuiCol_ButtonActive]    = col(46, 62, 42);
    c[ImGuiCol_Header]          = col(70, 60, 40);
    c[ImGuiCol_HeaderHovered]   = col(96, 80, 50);
    c[ImGuiCol_HeaderActive]    = col(110, 90, 56);
    c[ImGuiCol_TableHeaderBg]   = col(60, 50, 34);
    c[ImGuiCol_TableRowBg]      = col(40, 36, 28);
    c[ImGuiCol_TableRowBgAlt]   = col(46, 42, 32);
    c[ImGuiCol_TableBorderStrong] = col(120, 92, 48);
    c[ImGuiCol_TableBorderLight]  = col(80, 64, 40);
    c[ImGuiCol_Text]            = col(238, 232, 214);
    c[ImGuiCol_TextDisabled]    = col(150, 140, 120);
    c[ImGuiCol_CheckMark]       = col(220, 180, 90);
    c[ImGuiCol_SliderGrab]      = col(200, 160, 80);
    c[ImGuiCol_SliderGrabActive]= col(230, 190, 100);
    c[ImGuiCol_ScrollbarGrab]   = col(90, 76, 50);
    c[ImGuiCol_Separator]       = col(120, 92, 48);
}

bool App::init(const std::string& dataDir) {
    dataDir_ = dataDir;
    ApplyNostalgiaTheme();
    cfg_.loadFile(dataDir_ + "/engine.cfg");
    if (!db_.load(dataDir_)) {
        status_ = "Failed to load database from " + dataDir_;
        return false;
    }
    AppLoadTexture(dataDir_ + "/images/menu_bg.png", &menuBg_);
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

void App::beginFullscreen(const char* id, bool withBackground) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoScrollbar;
    if (!withBackground) flags |= ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(id, nullptr, flags);
    ImGui::PopStyleVar();
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
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 pos = vp->WorkPos;
    const ImVec2 size = vp->WorkSize;

    // Background: the stadium key-art, scaled to "cover" the window.
    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    if (menuBg_.ok && menuBg_.w > 0 && menuBg_.h > 0) {
        float ws = size.x / size.y, is = (float)menuBg_.w / (float)menuBg_.h;
        ImVec2 uv0(0, 0), uv1(1, 1);
        if (ws > is) {  // window wider than image: crop top/bottom
            float v = is / ws;
            uv0.y = (1 - v) * 0.5f;
            uv1.y = 1 - uv0.y;
        } else {  // crop left/right
            float u = ws / is;
            uv0.x = (1 - u) * 0.5f;
            uv1.x = 1 - uv0.x;
        }
        bg->AddImage(menuBg_.id, pos, ImVec2(pos.x + size.x, pos.y + size.y), uv0, uv1);
    } else {
        bg->AddRectFilledMultiColor(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                                    IM_COL32(20, 40, 70, 255), IM_COL32(20, 40, 70, 255),
                                    IM_COL32(10, 20, 14, 255), IM_COL32(10, 20, 14, 255));
        ImVec2 ts = ImGui::CalcTextSize("Nostalgia Manager Simulation");
        bg->AddText(ImVec2(pos.x + (size.x - ts.x) * 0.5f, pos.y + size.y * 0.18f),
                    IM_COL32(245, 215, 120, 255), "Nostalgia Manager Simulation");
    }

    beginFullscreen("##main", false);

    // Centred vertical stack of glossy coloured buttons, as in the mock-up.
    const ImVec2 bsz(430, 58);
    const float gap = 14.0f;
    struct Item { const char* label; ImU32 col; Screen scr; };
    const Item items[] = {
        {"Friendly Match", IM_COL32(86, 150, 38, 255), Screen::Friendly},
        {"Career Game", IM_COL32(196, 150, 40, 255), Screen::Career},
        {"Load Game", IM_COL32(40, 92, 178, 255), Screen::Career},
        {"Edit Database", IM_COL32(180, 92, 30, 255), Screen::Database},
        {"Exit", IM_COL32(188, 42, 38, 255), Screen::Main},
    };
    const int n = (int)(sizeof(items) / sizeof(items[0]));
    float startY = pos.y + size.y * 0.50f;
    float startX = pos.x + (size.x - bsz.x) * 0.5f;

    ImGui::SetWindowFontScale(1.25f);
    for (int i = 0; i < n; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(startX, startY + i * (bsz.y + gap)));
        if (tintButton(items[i].label, items[i].col, bsz)) {
            if (std::strcmp(items[i].label, "Exit") == 0) {
                quit_ = true;
            } else if (std::strcmp(items[i].label, "Load Game") == 0) {
                careerLoad();
            } else {
                screen_ = items[i].scr;
            }
        }
    }
    ImGui::SetWindowFontScale(1.0f);

    // Keep Data Sources / About reachable as small secondary links.
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 16, pos.y + size.y - 40));
    if (ImGui::Button("Data Sources")) screen_ = Screen::Data;
    ImGui::SameLine();
    if (ImGui::Button("About")) screen_ = Screen::About;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 st = ImGui::CalcTextSize(status_.c_str());
    dl->AddText(ImVec2(pos.x + size.x - st.x - 16, pos.y + size.y - 32),
                IM_COL32(230, 225, 205, 230), status_.c_str());

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
    matchHomeTeam_ = home;
    matchAwayTeam_ = away;
    homeScorers_.clear();
    awayScorers_.clear();

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

    // Fill running score across frames and collect scorers per side.
    int hg = 0, ag = 0;
    for (auto& f : frames_) {
        int h, a;
        if (f.text.rfind("GOAL!", 0) == 0 && parseScore(f.text, h, a)) {
            // Extract scorer name: "GOAL! #<n> <Name> scores! (h-a)".
            std::string who;
            size_t s = f.text.find(' ');
            size_t e = f.text.find(" scores!");
            if (s != std::string::npos && e != std::string::npos && e > s)
                who = f.text.substr(s + 1, e - s - 1);
            char line[160];
            std::snprintf(line, sizeof(line), "%s  %d'", who.c_str(), f.minute);
            if (h > hg) homeScorers_.emplace_back(f.minute, line);
            else if (a > ag) awayScorers_.emplace_back(f.minute, line);
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

    // Space bar toggles play/pause, as noted on the mock-up's bottom bar.
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) playing_ = !playing_;

    size_t cur = playIdx_ == 0 ? 0 : playIdx_ - 1;
    const Frame* f = frames_.empty() ? nullptr : &frames_[std::min(cur, frames_.size() - 1)];
    int hg = f ? f->hg : 0, ag = f ? f->ag : 0;
    int minute = f ? f->minute : 0;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags wf = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("##match", nullptr, wf);

    const float fullW = ImGui::GetContentRegionAvail().x;

    // ---- Scoreboard: TEAM  H - A  TEAM, with the clock underneath ----
    ImDrawList* dl = ImGui::GetWindowDrawList();
    char score[64];
    std::snprintf(score, sizeof(score), "%d  -  %d", hg, ag);
    ImGui::SetWindowFontScale(2.0f);
    ImVec2 ssz = ImGui::CalcTextSize(score);
    ImGui::SetWindowFontScale(1.4f);
    ImVec2 hsz = ImGui::CalcTextSize(matchHome_.c_str());
    ImVec2 asz = ImGui::CalcTextSize(matchAway_.c_str());
    float cx = ImGui::GetCursorScreenPos().x + fullW * 0.5f;
    float y0 = ImGui::GetCursorScreenPos().y + 4;
    dl->AddText(ImVec2(cx - fullW * 0.25f - hsz.x * 0.5f, y0 + 6),
                IM_COL32(140, 200, 255, 255), matchHome_.c_str());
    dl->AddText(ImVec2(cx + fullW * 0.25f - asz.x * 0.5f, y0 + 6),
                IM_COL32(255, 200, 140, 255), matchAway_.c_str());
    ImGui::SetWindowFontScale(2.0f);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(cx - ssz.x * 0.5f, y0), IM_COL32(255, 240, 190, 255), score);
    ImGui::SetWindowFontScale(1.0f);
    char clk[16];
    std::snprintf(clk, sizeof(clk), "%2d'", minute);
    ImVec2 csz = ImGui::CalcTextSize(clk);
    dl->AddText(ImVec2(cx - csz.x * 0.5f, y0 + ssz.y + 2), IM_COL32(220, 220, 210, 255), clk);
    ImGui::Dummy(ImVec2(fullW, ssz.y + csz.y + 10));

    // Scorers under each team name (only those that have happened so far).
    ImGui::Columns(2, "scorers", false);
    for (const auto& sc : homeScorers_)
        if (sc.first <= minute) ImGui::TextDisabled("%s", sc.second.c_str());
    ImGui::NextColumn();
    for (const auto& sc : awayScorers_) {
        if (sc.first > minute) continue;
        ImVec2 t = ImGui::CalcTextSize(sc.second.c_str());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - t.x - 16);
        ImGui::TextDisabled("%s", sc.second.c_str());
    }
    ImGui::Columns(1);
    ImGui::Separator();

    // ---- Main row: squad | pitch+stats | squad ----
    const float sideW = 240.0f;
    const float bottomH = 54.0f;
    const float feedH = 150.0f;
    float rowH = ImGui::GetContentRegionAvail().y - bottomH - feedH - 16;
    if (rowH < 200) rowH = 200;
    float centreW = fullW - 2 * (sideW + ImGui::GetStyle().ItemSpacing.x);

    squadPanel("home_sq", matchHome_.c_str(), matchHomeTeam_, ImVec2(sideW, rowH));
    ImGui::SameLine();

    ImGui::BeginGroup();
    float statsH = 132.0f;
    ImGui::BeginChild("pitch", ImVec2(centreW, rowH - statsH - ImGui::GetStyle().ItemSpacing.y),
                      true);
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(190, 255, 190, 255));
    if (f) ImGui::TextUnformatted(f->pitch.c_str());
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::BeginChild("stats", ImVec2(centreW, statsH), true);
    panelHeader("Game Stats");
    if (ImGui::BeginTable("st", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Home", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Stat", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Away", ImGuiTableColumnFlags_WidthStretch);
        auto row = [](const char* name, const char* h, const char* a) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(h);
            ImGui::TableSetColumnIndex(1);
            float w = ImGui::GetContentRegionAvail().x, tw = ImGui::CalcTextSize(name).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - tw) * 0.5f);
            ImGui::TextDisabled("%s", name);
            ImGui::TableSetColumnIndex(2);
            float w2 = ImGui::GetContentRegionAvail().x, tw2 = ImGui::CalcTextSize(a).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w2 - tw2);
            ImGui::TextUnformatted(a);
        };
        char hb[16], ab[16];
        std::snprintf(hb, sizeof(hb), "%d", hg); std::snprintf(ab, sizeof(ab), "%d", ag);
        row("Goals", hb, ab);
        if (matchOver_) {
            std::snprintf(hb, sizeof(hb), "%d", finalHS_);
            std::snprintf(ab, sizeof(ab), "%d", finalAS_);
            row("Attempts", hb, ab);
        } else {
            row("Attempts", "-", "-");
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine();
    squadPanel("away_sq", matchAway_.c_str(), matchAwayTeam_, ImVec2(sideW, rowH));

    // ---- Match events feed ----
    ImGui::BeginChild("feed", ImVec2(fullW, feedH), true);
    panelHeader("Match Events");
    size_t shown = playIdx_;
    size_t startI = shown > 40 ? shown - 40 : 0;
    for (size_t i = startI; i < shown && i < frames_.size(); ++i) {
        const Frame& ev = frames_[i];
        if (ev.key)
            ImGui::TextColored(ImVec4(1, 0.9f, 0.3f, 1), "%2d'  %s", ev.minute, ev.text.c_str());
        else
            ImGui::TextWrapped("%2d'  %s", ev.minute, ev.text.c_str());
    }
    if (playing_) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    // ---- Bottom control bar ----
    if (matchOver_) {
        if (tintButton("Back to Menu", IM_COL32(40, 92, 178, 255), ImVec2(220, bottomH - 10)))
            screen_ = Screen::Main;
        ImGui::SameLine();
        if (tintButton("Watch Again", IM_COL32(86, 150, 38, 255), ImVec2(180, bottomH - 10))) {
            playIdx_ = 0;
            playing_ = true;
            matchOver_ = false;
        }
    } else {
        char bar[64];
        std::snprintf(bar, sizeof(bar), "%s  (space)", playing_ ? "Pause Match" : "Continue");
        if (tintButton(bar, IM_COL32(60, 120, 170, 255),
                       ImVec2(fullW - 460, bottomH - 10)))
            playing_ = !playing_;
        ImGui::SameLine();
        if (ImGui::Button("Skip to End", ImVec2(160, bottomH - 10))) {
            playIdx_ = frames_.size();
            matchOver_ = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260);
        ImGui::SliderFloat("Speed", &speed_, 2.0f, 40.0f, "%.0f ev/s");
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
