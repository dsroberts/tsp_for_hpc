#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include "functions.hpp"
#include "semaphore.hpp"

namespace tsp
{
    Semaphore_File::Semaphore_File()
    {

        asprintf(&fn, "%s" SEMAPHORE_FILE_TEMPLATE ".XXXXXX", get_tmp());
        if ((fd = mkstemp(fn)) == -1)
        {
            die_with_err("Unable to create semaphore file", fd);
        }
        unlink(fn);
    }

    Semaphore_File::~Semaphore_File()
    {
        close(fd);
        free(fn);
    }
}