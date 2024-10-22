#include <cinttypes>
#include <filesystem>
#include <string>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "functions.hpp"
#include "jitter.hpp"
#include "semaphore.hpp"
#include "run_cmd.hpp"
#include "status_manager.hpp"
#include "proc_manager.hpp"
#include "output_manager.hpp"

#define BASE_WAIT_PERIOD 2

int main(int argc, char *argv[])
{
    tsp::Status_Manager stat;
    int c;
    uint32_t nslots = 1;
    bool disappear_output = false;
    bool do_fork = true;
    bool separate_stderr = false;

    // Parse args: only take the ones we use for now
    while ((c = getopt(argc, argv, "+nfN:E")) != -1)
    {
        switch (c)
        {
        case 'n':
            // Pipe stdout and stderr of forked process to /dev/null
            disappear_output = true;
            break;
        case 'f':
            do_fork = false;
            break;
        case 'N':
            nslots = std::stoul(optarg);
            break;
        case 'E':
            separate_stderr = true;
            break;
        }
    }

    tsp::Run_cmd cmd(argv, optind, argc);
    stat.add_cmd(cmd,nslots);

    if (do_fork)
    {
        pid_t main_fork_pid;
        main_fork_pid = fork();
        if (main_fork_pid == -1)
        {
            die_with_err("Unable to fork when forking requested", main_fork_pid);
        }
        if (main_fork_pid != 0)
        {
            // We're done here
            return 0;
        }
    }

    tsp::Jitter jitter(JITTER_MS);
    std::chrono::duration sleep(std::chrono::milliseconds(JITTER_MS + jitter.get()));
    std::this_thread::sleep_for(sleep);

    tsp::Tsp_Proc me(nslots);

    cpu_set_t mask;
    CPU_ZERO(&mask);

    while (!me.allowed_to_run())
    {
        std::this_thread::sleep_for(std::chrono::seconds(BASE_WAIT_PERIOD) + std::chrono::milliseconds(jitter.get()));
        me.refresh_allowed_cores();
    }
    stat.job_start();
    //delete sf;
    for (uint32_t i = 0; i < nslots; i++)
    {
        CPU_SET(me.allowed_cores[i], &mask);
    }
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
    {
        die_with_err("Unable to set CPU affinity", -1);
    }
    if (cmd.is_openmpi)
    {
        cmd.add_rankfile(me.allowed_cores, nslots);
    }

    pid_t fork_pid;
    int fork_stat;
    // exec & monitor here.
    if (0 == (fork_pid = fork()))
    {

        // We are now init, so fork again, and wait in a loop until it returns ECHILD
        int child_stat;
        int ret;
        pid_t waited_on_pid;
        if (0 == (waited_on_pid = fork()))
        {
            tsp::Output_handler *handler = new tsp::Output_handler(disappear_output, separate_stderr, stat.jobid.c_str());
            if (cmd.is_openmpi)
            {
                setenv("OMPI_MCA_rmaps_base_mapping_policy", "", 1);
                setenv("OMPI_MCA_rmaps_rank_file_physical", "true", 1);
            }
            ret = execvp(cmd.proc_to_run[0], &cmd.proc_to_run[0]);
            delete handler;

            if (ret != 0)
            {
                die_with_err("Error: could not exec " + std::string(cmd.proc_to_run[0]), ret);
            }
        }
        if (waited_on_pid == -1)
        {
            die_with_err("Error: could not fork init subprocess", waited_on_pid);
        }
        for (;;)
        {
            pid_t ret_pid = waitpid(-1, &child_stat, 0);
            if (ret_pid < 0)
            {
                if (errno == ECHILD)
                {
                    break;
                }
            }
        }
        return WEXITSTATUS(child_stat);
    }
    if (fork_pid == -1)
    {
        die_with_err("Error: could not fork into new pid namespace", fork_pid);
    }

    pid_t ret_pid = waitpid(fork_pid, &fork_stat, 0);
    if (ret_pid == -1)
    {
        die_with_err("Error: failed to wait for forked process", ret_pid);
    }

    // Exit with status of forked process.
    stat.job_end(WEXITSTATUS(fork_stat));
    return WEXITSTATUS(fork_stat);
}