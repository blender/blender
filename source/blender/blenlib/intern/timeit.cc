/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_timeit.hh"

namespace blender {
namespace Timeit {

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

}  // namespace Timeit
}  // namespace blender
