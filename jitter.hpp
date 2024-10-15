#pragma once
#include <random>

#define JITTER_MS 250

namespace tsp
{
    class Jitter
    {
    public:
        Jitter(int limit);
        int get();
        ~Jitter() {};

    private:
        std::mt19937 rng;
        std::uniform_int_distribution<int> dist;
    };
}