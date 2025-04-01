#pragma once

#include "generic_config.hpp"

namespace tsp {

class Timeout_config : public Generic_config {
public:
  Timeout_config();
};

int do_timeout(Timeout_config conf);
} // namespace tsp