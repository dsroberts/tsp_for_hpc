#include "jitter.hpp"

namespace tsp
{
    Jitter::Jitter(int limit)
    {
        std::random_device dev;
        rng = std::mt19937(dev());
        dist = std::uniform_int_distribution<int>(-abs(limit), abs(limit));
    }
    int Jitter::get()
    {
        return dist(rng);
    }
}