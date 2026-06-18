#pragma once
#include <iostream>
#include <limits>
#include <string>

namespace nm {

// Small helpers for the text UI.
class Console {
public:
    static void line(const std::string& s = "") { std::cout << s << "\n"; }

    static void rule(char c = '=', int n = 60) {
        std::cout << std::string(n, c) << "\n";
    }

    static void header(const std::string& title) {
        rule();
        std::cout << "  " << title << "\n";
        rule();
    }

    static std::string readLine(const std::string& prompt) {
        std::cout << prompt;
        std::cout.flush();
        std::string s;
        std::getline(std::cin, s);
        return s;
    }

    // Reads an integer in [lo, hi]; re-prompts on bad input. Returns lo-1 on EOF.
    static int readInt(const std::string& prompt, int lo, int hi) {
        while (true) {
            std::cout << prompt;
            std::cout.flush();
            std::string s;
            if (!std::getline(std::cin, s)) return lo - 1;
            try {
                int v = std::stoi(s);
                if (v >= lo && v <= hi) return v;
            } catch (...) {
            }
            std::cout << "  Please enter a number between " << lo << " and " << hi
                      << ".\n";
        }
    }

    static void pause(const std::string& msg = "Press Enter to continue...") {
        std::cout << msg;
        std::cout.flush();
        std::string s;
        std::getline(std::cin, s);
    }
};

}  // namespace nm
