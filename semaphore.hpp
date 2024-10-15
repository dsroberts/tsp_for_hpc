#pragma once

#define SEMAPHORE_FILE_TEMPLATE "/tsp_hpc_is_waiting"

namespace tsp
{
    class Semaphore_File
    {
    public:
        Semaphore_File();
        ~Semaphore_File();

    private:
        char *fn;
        int fd;
    };
}