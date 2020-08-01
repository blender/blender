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

/** \file
 * \ingroup MEM
 */

#include "MEM_guardedalloc.h"
#include "mallocn_intern.h"

bool leak_detector_has_run = false;
char free_after_leak_detection_message[] =
    "Freeing memory after the leak detector has run. This can happen when using "
    "static variables in C++ that are defined outside of functions. To fix this "
    "error, use the 'construct on first use' idiom.";

namespace {
class MemLeakPrinter {
 public:
  ~MemLeakPrinter()
  {
    leak_detector_has_run = true;
    const uint leaked_blocks = MEM_get_memory_blocks_in_use();
    if (leaked_blocks == 0) {
      return;
    }
    const size_t mem_in_use = MEM_get_memory_in_use();
    printf("Error: Not freed memory blocks: %u, total unfreed memory %f MB\n",
           leaked_blocks,
           (double)mem_in_use / 1024 / 1024);
    MEM_printmemlist();
  }
};
}  // namespace

void MEM_init_memleak_detection(void)
{
  /**
   * This variable is constructed when this function is first called. This should happen as soon as
   * possible when the program starts.
   *
   * It is destructed when the program exits. During destruction, it will print information about
   * leaked memory blocks. Static variables are destructed in reversed order of their
   * construction. Therefore, all static variables that own memory have to be constructed after
   * this function has been called.
   */
  static MemLeakPrinter printer;
}
