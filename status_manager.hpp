#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <sys/types.h>

#include "functions.hpp"
#include "run_cmd.hpp"

#define STATUS_FILE_TEMPLATE "/tsp.s"

namespace tsp
{
    class Status_Manager
    {
    public:
        const std::string jobid;
        std::ofstream stat_file;
        Status_Manager(Run_cmd cmd, uint32_t slots);
        ~Status_Manager();
        void job_start();
        void job_end(int exit_stat);

    private:
        const std::chrono::time_point<std::chrono::system_clock> qtime;
        std::chrono::time_point<std::chrono::system_clock> stime;
        std::chrono::time_point<std::chrono::system_clock> etime;
        std::string time_writer(std::chrono::time_point<std::chrono::system_clock> in);
        std::string gen_jobid();
    };
}