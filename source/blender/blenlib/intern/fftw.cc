/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if defined(WITH_FFTW3_THREADS_F_SUPPORT)
#  include <fftw3.h>
#endif

#include "BLI_fftw.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"

namespace blender::fftw {

/* Identifies if the given number is a 7-smooth number. */
static bool is_humble_number(int n)
{
  if (n <= 1) {
    return true;
  }
  if (n % 2 == 0) {
    return is_humble_number(n / 2);
  }
  if (n % 3 == 0) {
    return is_humble_number(n / 3);
  }
  if (n % 5 == 0) {
    return is_humble_number(n / 5);
  }
  if (n % 7 == 0) {
    return is_humble_number(n / 7);
  }
  return false;
}

/* Finds the even humble number larger than or equal the given number. */
static int find_next_even_humble_number(int n)
{
  if (n % 2 == 1) {
    n++;
  }

  while (true) {
    if (is_humble_number(n)) {
      return n;
    }
    n += 2;
  }

  return n;
}

int optimal_size_for_real_transform(int size)
{
  /* FFTW is best at handling sizes of the form 2^a * 3^b * 5^c * 7^d * 11^e * 13^f, where e + f is
   * either 0 or 1, and the other exponents are arbitrary. And it is beneficial for the size to be
   * even for real transforms. To simplify computation, we ignore the 11 and 13 factors and find
   * the even humble number that is more then or equal the given size. See Section 4.3.3 Real-data
   * DFTs in the FFTW manual for more information. */
  return find_next_even_humble_number(size);
}

int2 optimal_size_for_real_transform(int2 size)
{
  return int2(optimal_size_for_real_transform(size.x), optimal_size_for_real_transform(size.y));
}

/* See Section 5.2 Usage of Multi-threaded FFTW in the FFTW manual for more information. */
[[maybe_unused]] static void tbb_parallel_loop_for_fftw(void *(*work)(char *),
                                                        char *job_data,
                                                        size_t element_size,
                                                        int number_of_jobs,
                                                        void * /* data */)
{
  threading::parallel_for(IndexRange(number_of_jobs), 1, [&](const IndexRange sub_range) {
    for (const int64_t i : sub_range) {
      work(job_data + element_size * i);
    }
  });
}

void initialize_float()
{
#if defined(WITH_FFTW3_THREADS_F_SUPPORT)
  fftwf_init_threads();
  fftwf_make_planner_thread_safe();
  fftwf_plan_with_nthreads(BLI_system_thread_count());
  fftwf_threads_set_callback(tbb_parallel_loop_for_fftw, nullptr);
#endif
}

}  // namespace blender::fftw
