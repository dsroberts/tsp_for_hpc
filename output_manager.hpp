#pragma once

#define OUTPUT_FILE_TEMPLATE "/tsp.o"
#define ERROR_FILE_TEMPLATE "/tsp.e"

namespace tsp
{
    class Output_handler
    {
    public:
        int stdout_fd;
        int stderr_fd;
        Output_handler(bool disappear, bool separate_stderr, const char *jobid);
        ~Output_handler();

    private:
        char *stdout_fn = nullptr;
        char *stderr_fn = nullptr;
    };
}