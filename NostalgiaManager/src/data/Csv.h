#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace nm {

// Minimal CSV reader. Handles simple comma separated values and optional
// double-quoted fields. Sufficient for the TeamsDB / PlayersDB data files.
class Csv {
public:
    static std::vector<std::string> splitLine(const std::string& line) {
        std::vector<std::string> out;
        std::string field;
        bool inQuotes = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (inQuotes) {
                if (c == '"') {
                    if (i + 1 < line.size() && line[i + 1] == '"') {
                        field += '"';
                        ++i;
                    } else {
                        inQuotes = false;
                    }
                } else {
                    field += c;
                }
            } else {
                if (c == '"') {
                    inQuotes = true;
                } else if (c == ',') {
                    out.push_back(trim(field));
                    field.clear();
                } else {
                    field += c;
                }
            }
        }
        out.push_back(trim(field));
        return out;
    }

    static bool read(const std::string& path, std::vector<std::vector<std::string>>& rows) {
        std::ifstream in(path);
        if (!in.is_open()) return false;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            rows.push_back(splitLine(line));
        }
        return true;
    }

    static std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t");
        return s.substr(a, b - a + 1);
    }
};

}  // namespace nm
