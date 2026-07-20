#pragma once

#include <vector>

namespace omniface::tracking {

int Lapjv(const float* cost, int rows, int cols, bool extend_cost, float cost_limit,
          std::vector<int>& x, std::vector<int>& y);

}
