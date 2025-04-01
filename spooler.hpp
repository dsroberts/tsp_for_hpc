#pragma once

#include "generic_config.hpp"

namespace tsp {

class Spooler_config : public Generic_config {
public:
  Spooler_config();
};

int do_spooler(Spooler_config conf, int argc, int optind, char *argv[]);
} // namespace tsp