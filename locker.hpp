#pragma once

#include <string>
#include <array>
#include <unistd.h>
#include <signal.h>

#include "functions.hpp"

namespace tsp
{
    class Locker
    {
    public:
        Locker();
        ~Locker();
        void lock();
        void unlock();

    private:
        const std::string lock_file_path{get_tmp() / ".affinity_lock_file.lock"};
        std::array<sighandler_t, NSIG> prev_sigs;
    };
}