#include <cstdlib>
#include <string>
#include <stdexcept>
#include <cstring>

const char *get_tmp()
{
    const char *out = getenv("TMPDIR");
    if (out == nullptr)
    {
        out = "/tmp";
    }
    return out;
}

void die_with_err(std::string msg, int status)
{
    std::string out(msg);
    out.append("\nstat=" + std::to_string(status) + ", errno=" + std::to_string(errno));
    out.append(std::string("\n") + strerror(errno));
    throw std::runtime_error(out);
}