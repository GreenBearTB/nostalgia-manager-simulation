#pragma once
#include <random>

namespace nm {

// Thin wrapper around a Mersenne Twister so the whole match can be seeded
// (reproducible simulations) and so "Chance" is centralised.
class Rng {
public:
    explicit Rng(unsigned seed = std::random_device{}()) : gen_(seed) {}

    void seed(unsigned s) { gen_.seed(s); }

    // Uniform double in [lo, hi].
    double real(double lo, double hi) {
        std::uniform_real_distribution<double> d(lo, hi);
        return d(gen_);
    }

    // Uniform int in [lo, hi] inclusive.
    int range(int lo, int hi) {
        std::uniform_int_distribution<int> d(lo, hi);
        return d(gen_);
    }

    bool chance(double p) { return real(0.0, 1.0) < p; }

    std::mt19937& engine() { return gen_; }

private:
    std::mt19937 gen_;
};

}  // namespace nm
