#pragma once

#include "generic_config.hpp"

namespace tsp {

class Memprof_config : public Generic_config {
public:
  Memprof_config();
};

int do_memprof(Memprof_config conf);
} // namespace tsp