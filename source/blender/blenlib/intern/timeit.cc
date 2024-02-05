/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"
#include "BLI_timeit.hh"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include <fmt/format.h>

namespace blender::timeit {

static void format_duration(Nanoseconds duration, fmt::memory_buffer &buf)
{
  using namespace std::chrono;
  if (duration < microseconds(100)) {
    fmt::format_to(fmt::appender(buf), FMT_STRING("{} ns"), duration.count());
  }
  else if (duration < seconds(5)) {
    fmt::format_to(fmt::appender(buf), FMT_STRING("{:.2f} ms"), duration.count() / 1.0e6);
  }
  else if (duration > seconds(90)) {
    /* Long durations: print seconds, and also H:m:s */
    const auto dur_hours = duration_cast<hours>(duration);
    const auto dur_mins = duration_cast<minutes>(duration - dur_hours);
    const auto dur_sec = duration_cast<seconds>(duration - dur_hours - dur_mins);
    fmt::format_to(fmt::appender(buf),
                   FMT_STRING("{:.1f} s ({}H:{}m:{}s)"),
                   duration.count() / 1.0e9,
                   dur_hours.count(),
                   dur_mins.count(),
                   dur_sec.count());
  }
  else {
    fmt::format_to(fmt::appender(buf), FMT_STRING("{:.1f} s"), duration.count() / 1.0e9);
  }
}

void print_duration(Nanoseconds duration)
{
  fmt::memory_buffer buf;
  format_duration(duration, buf);
  std::cout << StringRef(buf.data(), buf.size());
}

ScopedTimer::~ScopedTimer()
{
  const TimePoint end = Clock::now();
  const Nanoseconds duration = end - start_;

  fmt::memory_buffer buf;
  fmt::format_to(fmt::appender(buf), FMT_STRING("Timer '{}' took "), name_);
  format_duration(duration, buf);
  buf.append(StringRef("\n"));
  std::cout << StringRef(buf.data(), buf.size());
}

ScopedTimerAveraged::~ScopedTimerAveraged()
{
  const TimePoint end = Clock::now();
  const Nanoseconds duration = end - start_;

  total_count_++;
  total_time_ += duration;
  min_time_ = std::min(duration, min_time_);

  fmt::memory_buffer buf;
  fmt::format_to(fmt::appender(buf), FMT_STRING("Timer '{}': (Average: "), name_);
  format_duration(total_time_ / total_count_, buf);
  buf.append(StringRef(", Min: "));
  format_duration(min_time_, buf);
  buf.append(StringRef(", Last: "));
  format_duration(duration, buf);
  buf.append(StringRef(")\n"));
  std::cout << StringRef(buf.data(), buf.size());
}

}  // namespace blender::timeit
