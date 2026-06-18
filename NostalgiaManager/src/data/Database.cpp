#include "Database.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

#include "Csv.h"

namespace nm {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

Mentality mentalityFromString(const std::string& s) {
    std::string v = lower(s);
    if (v == "defensive") return Mentality::Defensive;
    if (v == "attack") return Mentality::Attack;
    return Mentality::Standard;
}
}  // namespace

bool Database::loadTeams(const std::string& path) {
    std::vector<std::vector<std::string>> rows;
    if (!Csv::read(path, rows) || rows.empty()) return false;

    // Header: id,name,league,formation,mentality
    std::unordered_map<std::string, int> col;
    for (size_t i = 0; i < rows[0].size(); ++i) col[lower(rows[0][i])] = static_cast<int>(i);

    auto get = [&](const std::vector<std::string>& r, const std::string& key) -> std::string {
        auto it = col.find(key);
        if (it == col.end() || it->second >= static_cast<int>(r.size())) return "";
        return r[it->second];
    };

    for (size_t i = 1; i < rows.size(); ++i) {
        const auto& r = rows[i];
        Team t;
        t.id = std::stoi(get(r, "id"));
        t.name = get(r, "name");
        t.league = get(r, "league");
        std::string f = get(r, "formation");
        if (!f.empty()) t.formation = f;
        t.mentality = mentalityFromString(get(r, "mentality"));
        teams.push_back(std::move(t));
    }
    return true;
}

bool Database::loadPlayers(const std::string& path) {
    std::vector<std::vector<std::string>> rows;
    if (!Csv::read(path, rows) || rows.empty()) return false;

    std::unordered_map<std::string, int> col;
    for (size_t i = 0; i < rows[0].size(); ++i) col[lower(rows[0][i])] = static_cast<int>(i);

    auto get = [&](const std::vector<std::string>& r, const std::string& key) -> std::string {
        auto it = col.find(key);
        if (it == col.end() || it->second >= static_cast<int>(r.size())) return "";
        return r[it->second];
    };
    auto geti = [&](const std::vector<std::string>& r, const std::string& key) -> int {
        std::string v = get(r, key);
        return v.empty() ? 0 : std::stoi(v);
    };

    std::unordered_map<int, Team*> byId;
    for (auto& t : teams) byId[t.id] = &t;

    for (size_t i = 1; i < rows.size(); ++i) {
        const auto& r = rows[i];
        Player p;
        p.id = geti(r, "id");
        p.name = get(r, "name");
        p.teamId = geti(r, "teamid");
        p.role = RoleFromString(get(r, "role"));
        p.shirtNumber = geti(r, "number");
        for (const auto& an : AttributeNames()) {
            std::string v = get(r, lower(an));
            if (!v.empty()) p.attr.set(an, std::stoi(v));
        }
        auto it = byId.find(p.teamId);
        if (it != byId.end()) it->second->squad.push_back(std::move(p));
    }

    for (auto& t : teams)
        if (t.startingXI.empty()) t.autoSelectXI();
    return true;
}

bool Database::load(const std::string& dataDir) {
    teams.clear();
    if (!loadTeams(dataDir + "/TeamsDB.csv")) return false;
    if (!loadPlayers(dataDir + "/PlayersDB.csv")) return false;
    return true;
}

Team* Database::findTeam(int id) {
    for (auto& t : teams)
        if (t.id == id) return &t;
    return nullptr;
}

Team* Database::findTeamByName(const std::string& name) {
    for (auto& t : teams)
        if (lower(t.name) == lower(name)) return &t;
    return nullptr;
}

std::vector<std::string> Database::leagues() const {
    std::vector<std::string> out;
    for (const auto& t : teams)
        if (std::find(out.begin(), out.end(), t.league) == out.end()) out.push_back(t.league);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<Team*> Database::teamsInLeague(const std::string& league) {
    std::vector<Team*> out;
    for (auto& t : teams)
        if (t.league == league) out.push_back(&t);
    return out;
}

void Database::searchPlayers(const std::string& query,
                             std::vector<std::pair<const Team*, const Player*>>& out) const {
    std::string q = lower(query);
    for (const auto& t : teams)
        for (const auto& p : t.squad)
            if (q.empty() || lower(p.name).find(q) != std::string::npos)
                out.emplace_back(&t, &p);
}

void Database::searchTeams(const std::string& query, std::vector<const Team*>& out) const {
    std::string q = lower(query);
    for (const auto& t : teams)
        if (q.empty() || lower(t.name).find(q) != std::string::npos) out.push_back(&t);
}

}  // namespace nm
