#include "Database.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <unordered_map>

#include "Csv.h"

namespace nm {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Normalise a header cell to a comparison key: lowercase, keep only [a-z0-9].
// "Off The Ball" -> "offtheball", "Strenght" -> "strenght".
std::string normKey(const std::string& s) {
    std::string out;
    for (unsigned char c : s)
        if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
    return out;
}

// Fold accented Latin characters to plain ASCII so names from Championship
// Manager / FM exports render in the bitmap GUI font (which only has ASCII).
// Handles both raw Latin-1 bytes and UTF-8 two-byte Latin-1 supplement.
std::string asciiFold(const std::string& s) {
    auto mapLatin1 = [](unsigned cp) -> const char* {
        switch (cp) {
            case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: return "A";
            case 0xC6: return "AE";
            case 0xC7: return "C";
            case 0xC8: case 0xC9: case 0xCA: case 0xCB: return "E";
            case 0xCC: case 0xCD: case 0xCE: case 0xCF: return "I";
            case 0xD0: return "D"; case 0xD1: return "N";
            case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: return "O";
            case 0xD9: case 0xDA: case 0xDB: case 0xDC: return "U";
            case 0xDD: return "Y"; case 0xDE: return "Th"; case 0xDF: return "ss";
            case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: return "a";
            case 0xE6: return "ae";
            case 0xE7: return "c";
            case 0xE8: case 0xE9: case 0xEA: case 0xEB: return "e";
            case 0xEC: case 0xED: case 0xEE: case 0xEF: return "i";
            case 0xF0: return "d"; case 0xF1: return "n";
            case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: return "o";
            case 0xF9: case 0xFA: case 0xFB: case 0xFC: return "u";
            case 0xFD: case 0xFF: return "y"; case 0xFE: return "th";
            default: return nullptr;
        }
    };
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char b = static_cast<unsigned char>(s[i]);
        if (b < 0x80) {
            out += static_cast<char>(b);
        } else if (b == 0xC3 && i + 1 < s.size()) {
            unsigned char n = static_cast<unsigned char>(s[i + 1]);
            unsigned cp = 0xC0 + (n & 0x3F);
            const char* r = mapLatin1(cp);
            if (r) out += r;
            ++i;
        } else if (b == 0xC2 && i + 1 < s.size()) {
            ++i;  // drop C2-prefixed punctuation (©, etc.)
        } else {
            const char* r = mapLatin1(b);
            if (r) out += r;  // raw Latin-1 byte
        }
    }
    return out;
}

Mentality mentalityFromString(const std::string& s) {
    std::string v = lower(s);
    if (v.rfind("def", 0) == 0) return Mentality::Defensive;
    if (v.rfind("att", 0) == 0) return Mentality::Attack;
    return Mentality::Standard;
}

int toInt(const std::string& s) {
    try {
        return s.empty() ? 0 : std::stoi(s);
    } catch (...) {
        return 0;
    }
}

int clampStat(int v) { return std::max(1, std::min(20, v)); }

constexpr size_t kNoTeam = static_cast<size_t>(-1);

// Some databases use placeholder "clubs" for unattached players. These should
// not become real teams.
bool isNonClub(const std::string& name) {
    std::string v = lower(name);
    static const char* markers[] = {"free transfer", "free agent", "retired",
                                    "non league", "non-league", "minor team",
                                    "unknown", "none", "n/a"};
    for (const char* m : markers)
        if (v == m) return true;
    return false;
}

// Maps normalised header names -> the first column index that carries them.
// First occurrence wins, which keeps the detailed CM position block (the first
// "Goalkeeper" column) rather than the later summary block.
struct Header {
    std::unordered_map<std::string, int> idx;
    void build(const std::vector<std::string>& row) {
        for (size_t i = 0; i < row.size(); ++i) {
            std::string k = normKey(row[i]);
            if (!k.empty() && idx.find(k) == idx.end()) idx[k] = static_cast<int>(i);
        }
    }
    bool has(const std::string& k) const { return idx.count(k) > 0; }
    // Returns the column index for the first key that exists, or -1.
    int col(std::initializer_list<const char*> keys) const {
        for (const char* k : keys) {
            auto it = idx.find(k);
            if (it != idx.end()) return it->second;
        }
        return -1;
    }
    std::string get(const std::vector<std::string>& r,
                    std::initializer_list<const char*> keys) const {
        int c = col(keys);
        if (c < 0 || c >= static_cast<int>(r.size())) return "";
        return r[c];
    }
    int getInt(const std::vector<std::string>& r,
               std::initializer_list<const char*> keys) const {
        return toInt(get(r, keys));
    }
};

// Engine attribute -> candidate header keys in a CM/FM style export.
const std::vector<std::pair<const char*, std::vector<const char*>>>& attrMap() {
    static const std::vector<std::pair<const char*, std::vector<const char*>>> m = {
        {"Passing", {"passing"}},
        {"Shooting", {"shooting", "finishing"}},
        {"Technique", {"technique"}},
        {"Dribbling", {"dribbling"}},
        {"Heading", {"heading"}},
        {"Pace", {"pace", "acceleration"}},
        {"Stamina", {"stamina"}},
        {"Strength", {"strength", "strenght"}},
        {"Jumping", {"jumping"}},
        {"Positioning", {"positioning"}},
        {"OffTheBall", {"offtheball"}},
        {"Marking", {"marking"}},
        {"Tackling", {"tackling"}},
        {"Creativity", {"creativity", "vision"}},
        {"Determination", {"determination"}},
        {"Influence", {"influence", "leadership"}},
        {"Aggression", {"aggression"}},
        {"Flair", {"flair"}},
    };
    return m;
}
}  // namespace

// ---------------------------------------------------------------------------
// Teams / clubs
// ---------------------------------------------------------------------------
bool Database::loadTeams(const std::string& path) {
    std::vector<std::vector<std::string>> rows;
    if (!Csv::read(path, rows) || rows.size() < 2) return false;

    Header h;
    h.build(rows[0]);

    bool hasId = h.has("id");
    for (size_t i = 1; i < rows.size(); ++i) {
        const auto& r = rows[i];
        std::string name = asciiFold(h.get(r, {"name", "club", "clubname", "teamname", "team"}));
        if (name.empty() || isNonClub(name)) continue;
        Team t;
        t.id = hasId ? h.getInt(r, {"id"}) : nextTeamId_++;
        if (t.id >= nextTeamId_) nextTeamId_ = t.id + 1;
        t.name = name;
        t.league = asciiFold(h.get(r, {"league", "division", "div", "competition", "nation", "country"}));
        if (t.league.empty()) t.league = "League";
        std::string f = h.get(r, {"formation", "formationa", "formationb", "shape"});
        if (!f.empty()) t.formation = f;
        std::string m = h.get(r, {"mentality", "mentalitiy"});
        if (!m.empty()) t.mentality = mentalityFromString(m);
        teams.push_back(std::move(t));
    }
    return !teams.empty();
}

// ---------------------------------------------------------------------------
// Players
// ---------------------------------------------------------------------------
bool Database::loadPlayers(const std::string& path) {
    std::vector<std::vector<std::string>> rows;
    if (!Csv::read(path, rows) || rows.size() < 2) return false;

    Header h;
    h.build(rows[0]);

    // Legacy bundled format has an explicit teamId + role + number column.
    bool legacy = h.has("teamid") && h.has("role");

    // For CM/FM exports: resolve each engine skill to a column once and detect
    // the value scale (some databases store skills as 0-100 rather than 1-20).
    std::vector<std::pair<std::string, int>> skillCols;
    double skillScale = 1.0;
    if (!legacy) {
        for (const auto& kv : attrMap()) {
            int c = -1;
            for (const char* k : kv.second)
                if (h.has(k)) { c = h.idx.at(k); break; }
            skillCols.emplace_back(kv.first, c);
        }
        int maxSkill = 0;
        for (size_t i = 1; i < rows.size(); ++i)
            for (const auto& sc : skillCols)
                if (sc.second >= 0 && sc.second < static_cast<int>(rows[i].size()))
                    maxSkill = std::max(maxSkill, toInt(rows[i][sc.second]));
        if (maxSkill > 20) skillScale = 20.0 / maxSkill;
    }

    std::unordered_map<std::string, size_t> nameToIdx;
    for (size_t i = 0; i < teams.size(); ++i) nameToIdx[lower(teams[i].name)] = i;

    // When a clubs file has already been loaded, only attach players to those
    // known clubs; otherwise synthesise teams from the players' Club column.
    const bool haveClubs = !teams.empty();
    auto ensureIdx = [&](const std::string& clubName) -> size_t {
        std::string key = lower(clubName);
        auto it = nameToIdx.find(key);
        if (it != nameToIdx.end()) return it->second;
        if (haveClubs) return kNoTeam;
        Team t;
        t.id = nextTeamId_++;
        t.name = clubName.empty() ? ("Club " + std::to_string(t.id)) : clubName;
        t.league = "League";
        size_t idx = teams.size();
        teams.push_back(std::move(t));
        nameToIdx[key] = idx;
        return idx;
    };

    int autoId = 1;
    for (size_t i = 1; i < rows.size(); ++i) {
        const auto& r = rows[i];
        Player p;

        if (legacy) {
            p.id = h.getInt(r, {"id"});
            p.name = asciiFold(h.get(r, {"name"}));
            p.role = RoleFromString(h.get(r, {"role"}));
            p.shirtNumber = h.getInt(r, {"number"});
            for (const auto& an : AttributeNames()) {
                std::string v = h.get(r, {normKey(an).c_str()});
                p.attr.set(an, v.empty() ? 10 : clampStat(toInt(v)));
            }
            int teamId = h.getInt(r, {"teamid"});
            Team* t = findTeam(teamId);
            if (!t) continue;
            t->squad.push_back(std::move(p));
            continue;
        }

        // --- Championship Manager / FM style export ---
        std::string name = h.get(r, {"fullname", "name", "playername"});
        if (name.empty()) {
            std::string fn = h.get(r, {"firstname", "firstnam"});
            std::string sn = h.get(r, {"secondname", "surname", "lastname"});
            name = Csv::trim(fn + " " + sn);
        }
        if (name.empty()) continue;
        p.id = autoId++;
        p.name = asciiFold(name);

        // Overall ability (0-200) drives the GK rating and acts as a baseline
        // for skills the export left blank (these databases are often sparse).
        int ability = h.getInt(r, {"ability", "currentability", "ca"});
        int abil20 = static_cast<int>(std::lround(ability / 10.0));
        int baseline = ability > 0 ? clampStat(abil20) : 8;

        // Skills, rescaled to the engine's 1-20 range if needed.
        for (const auto& sc : skillCols) {
            int raw = (sc.second >= 0 && sc.second < static_cast<int>(r.size()))
                          ? toInt(r[sc.second])
                          : 0;
            int v = raw > 0 ? static_cast<int>(std::lround(raw * skillScale)) : baseline;
            p.attr.set(sc.first, clampStat(v));
        }

        // Role from positional ratings (0-2). Try the detailed CM block first,
        // then fall back to the broader role columns.
        auto pos = [&](std::initializer_list<const char*> keys) { return h.getInt(r, keys); };
        int rGK = pos({"goalkeeper", "gk"});
        int rD = std::max({pos({"defendercentral"}), pos({"defenderright"}),
                           pos({"defenderleft"}), pos({"wingbackright"}),
                           pos({"wingbackleft"}), pos({"sweeper"}), pos({"defender"})});
        int rDM = std::max(pos({"defensivemidfielder"}),
                           pos({"defensivemidfield", "anchor"}));
        int rM = std::max({pos({"midfieldercentral"}), pos({"midfielderright"}),
                           pos({"midfielderleft"}), pos({"midfieldcenter", "midfield"})});
        int rAM = std::max({pos({"attackingmidfieldercentral"}),
                            pos({"attackingmidfielderright"}),
                            pos({"attackingmidfielderleft"}), pos({"support"})});
        int rF = std::max({pos({"centerforward", "centreforward"}),
                           pos({"rightforward"}), pos({"leftforward"}),
                           pos({"attack"})});

        struct RoleScore { Role role; int score; };
        RoleScore arr[] = {{Role::GK, rGK}, {Role::D, rD}, {Role::DM, rDM},
                           {Role::M, rM}, {Role::AM, rAM}, {Role::F, rF}};
        Role best = Role::M;
        int bestScore = -1;
        for (const auto& rs : arr)
            if (rs.score > bestScore) { bestScore = rs.score; best = rs.role; }
        p.role = best;

        // Goalkeeping: no dedicated GK skill in the export, so derive it from a
        // keeper's overall Ability (0-200 -> ~1-20); outfielders get a low value.
        if (p.role == Role::GK)
            p.attr.set("Goalkeeping", clampStat(ability > 0 ? abil20 : 12));
        else
            p.attr.set("Goalkeeping", clampStat(std::max(1, abil20 / 4)));

        std::string clubName = asciiFold(h.get(r, {"club", "team", "clubname"}));
        // Skip free agents / unattached players so they don't form junk teams.
        if (clubName.empty() || isNonClub(clubName)) continue;
        size_t idx = ensureIdx(clubName);
        if (idx == kNoTeam) continue;  // unknown club and clubs file present
        teams[idx].squad.push_back(std::move(p));
    }

    // Assign shirt numbers per club (1..N) where missing, and pick a starting XI.
    for (auto& t : teams) {
        bool anyNumbers = false;
        for (const auto& pl : t.squad)
            if (pl.shirtNumber > 0) { anyNumbers = true; break; }
        if (!anyNumbers) {
            int n = 1;
            for (auto& pl : t.squad) pl.shirtNumber = n++;
        }
        if (t.startingXI.empty()) t.autoSelectXI();
    }
    return true;
}

// ---------------------------------------------------------------------------
// Top-level load (honours data/datasources.cfg)
// ---------------------------------------------------------------------------
bool Database::load(const std::string& dataDir) {
    teams.clear();
    nextTeamId_ = 1;

    std::string teamsPath = dataDir + "/TeamsDB.csv";
    std::string playersPath = dataDir + "/PlayersDB.csv";

    // Optional override file lets the user point at their own databases
    // (e.g. players = D:\DEV\Docs\Players db1 csv.csv).
    std::ifstream cfg(dataDir + "/datasources.cfg");
    if (cfg.is_open()) {
        std::string line;
        while (std::getline(cfg, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t hash = line.find('#');
            if (hash != std::string::npos) line = line.substr(0, hash);
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = lower(Csv::trim(line.substr(0, eq)));
            std::string val = Csv::trim(line.substr(eq + 1));
            if (val.empty()) continue;
            auto resolve = [&](const std::string& v) -> std::string {
                bool absolute = v.size() > 1 &&
                                (v[0] == '/' || v[0] == '\\' || v[1] == ':');
                return absolute ? v : (dataDir + "/" + v);
            };
            if (key == "players" || key == "playersdb") playersPath = resolve(val);
            else if (key == "clubs" || key == "teams" || key == "teamsdb")
                teamsPath = resolve(val);
        }
    }

    // Clubs are optional: if absent or unreadable, they are created on demand
    // from the players' Club column.
    loadTeams(teamsPath);
    if (!loadPlayers(playersPath)) return false;
    return !teams.empty();
}

// ---------------------------------------------------------------------------
// Lookups
// ---------------------------------------------------------------------------
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
