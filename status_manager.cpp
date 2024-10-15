#include <random>

#include "status_manager.hpp"

namespace tsp
{
    Status_Manager::Status_Manager(Run_cmd cmd, uint32_t slots)
    {
        gen_jobid();
        std::string stat_fn(get_tmp());
        stat_fn.append(STATUS_FILE_TEMPLATE);
        stat_fn.append(jobid);
        stat_file.open(stat_fn);
        stat_file << "Command: " << cmd.print() << std::endl;
        stat_file << "Slots required: " << std::to_string(slots) << std::endl;
        qtime = std::chrono::system_clock::now();
        stat_file << "Enqueue time: " << time_writer(qtime) << std::endl;
    }
    Status_Manager::~Status_Manager()
    {
        stat_file.close();
    }

    void Status_Manager::job_start()
    {
        stime = std::chrono::system_clock::now();
        stat_file << "Start time: " << time_writer(stime) << std::endl;
    }

    void Status_Manager::job_end(int exit_stat)
    {
        etime = std::chrono::system_clock::now();
        auto dur = etime - stime;
        stat_file << "End Time: " << time_writer(etime) << std::endl;
        using namespace std::literals;
        auto delta_t = (float)(dur / 1us) / 1000000;
        stat_file << "Time run: " << delta_t << std::endl;
        stat_file << "Exit status: died with exit code " << std::to_string(exit_stat) << std::endl;
        stat_file.close();
    }

    std::string Status_Manager::time_writer(std::chrono::time_point<std::chrono::system_clock> in)
    {
        const std::time_t t_c = std::chrono::system_clock::to_time_t(in);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&t_c), "%c");
        return ss.str();
    }

    void Status_Manager::gen_jobid()
    {
        // https://stackoverflow.com/questions/24365331/how-can-i-generate-uuid-in-c-without-using-boost-library
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);

        std::stringstream ss;
        int i;
        ss << std::hex;
        for (i = 0; i < 8; i++)
        {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 4; i++)
        {
            ss << dis(gen);
        }
        ss << "-4";
        for (i = 0; i < 3; i++)
        {
            ss << dis(gen);
        }
        ss << "-";
        ss << dis2(gen);
        for (i = 0; i < 3; i++)
        {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 12; i++)
        {
            ss << dis(gen);
        }
        jobid = ss.str();
    }
}