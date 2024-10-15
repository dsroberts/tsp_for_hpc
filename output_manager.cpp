#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "output_manager.hpp"
#include "functions.hpp"

namespace tsp
{
    Output_handler::Output_handler(bool disappear, bool separate_stderr, const char *jobid)
    {
        if (disappear)
        {
            stdout_fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
            stderr_fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
        }
        else
        {
            asprintf(&stdout_fn, "%s" OUTPUT_FILE_TEMPLATE "%s", get_tmp(), jobid);
            stdout_fd = open(stdout_fn, O_WRONLY | O_CREAT, 0600);
            dup2(stdout_fd, 1);
            if (separate_stderr)
            {
                asprintf(&stderr_fn, "%s" ERROR_FILE_TEMPLATE "%s", get_tmp(), jobid);
                dup2(stderr_fd, 2);
                stderr_fd = open(stderr_fn, O_WRONLY | O_CREAT, 0600);
            }
            else
            {
                dup2(stdout_fd, 2);
            }
        }
    }
    Output_handler::~Output_handler()
    {
        if (stdout_fn != nullptr)
        {
            free(stdout_fn);
        }
        if (stderr_fn != nullptr)
        {
            free(stderr_fn);
        }
        if (stdout_fd != -1)
        {
            close(stdout_fd);
        }
        if (stderr_fd != -1)
        {
            close(stderr_fd);
        }
    }
}