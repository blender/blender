/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_timeit.hh"

namespace blender::timeit {

void print_duration(Nanoseconds duration)
{
  if (duration < std::chrono::microseconds(100)) {
    std::cout << duration.count() << " ns";
  }
  else if (duration < std::chrono::seconds(5)) {
    std::cout << duration.count() / 1.0e6 << " ms";
  }
  else {
    std::cout << duration.count() / 1.0e9 << " s";
  }
}

}  // namespace blender::timeit
