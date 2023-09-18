/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_mem
 */

#include <cstdio> /* Needed for `printf` on WIN32/APPLE. */
#include <cstdlib>

#include "MEM_guardedalloc.h"
#include "mallocn_intern.h"

bool leak_detector_has_run = false;
char free_after_leak_detection_message[] =
    "Freeing memory after the leak detector has run. This can happen when using "
    "static variables in C++ that are defined outside of functions. To fix this "
    "error, use the 'construct on first use' idiom.";

namespace {

bool fail_on_memleak = false;
bool ignore_memleak = false;

class MemLeakPrinter {
 public:
  ~MemLeakPrinter()
  {
    if (ignore_memleak) {
      return;
    }
    leak_detector_has_run = true;
    const uint leaked_blocks = MEM_get_memory_blocks_in_use();
    if (leaked_blocks == 0) {
      return;
    }
    const size_t mem_in_use = MEM_get_memory_in_use();
    printf("Error: Not freed memory blocks: %u, total unfreed memory %f MB\n",
           leaked_blocks,
           double(mem_in_use) / 1024 / 1024);
    MEM_printmemlist();

    /* In guarded implementation, the fact that all allocated memory blocks are stored in the
     * static `membase` listbase is enough for LSAN to not detect them as leaks. Clearing it solves
     * that issue. */
    mem_clearmemlist();

    if (fail_on_memleak) {
      /* There are many other ways to change the exit code to failure here:
       * - Make the destructor `noexcept(false)` and throw an exception.
       * - Call exit(EXIT_FAILURE).
       * - Call terminate().
       */
      abort();
    }
  }
};
}  // namespace

void MEM_init_memleak_detection()
{
  /* Calling this ensures that the memory usage counters outlive the memory leak detection. */
  memory_usage_init();

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

void MEM_use_memleak_detection(bool enabled)
{
  ignore_memleak = !enabled;
}

void MEM_enable_fail_on_memleak()
{
  fail_on_memleak = true;
}
