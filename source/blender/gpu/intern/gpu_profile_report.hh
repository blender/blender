/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_string_ref.hh"

#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace blender::gpu {

class ProfileReport {
 private:
  std::fstream _report;
  Mutex _mutex;
  Map<uint64_t, int> _thread_ids;

  ProfileReport()
  {
    _report.open("profile.json", std::ios::out);
    _report << R"([{"name":"process_name","ph":"M","pid":1,"args":{"name":"GPU"}})"
               ",\n";
    _report << R"({"name":"process_name","ph":"M","pid":2,"args":{"name":"CPU"}})";
  }

  ~ProfileReport()
  {
    _report << "\n]\n";
    _report.close();
  }

 public:
  static ProfileReport &get()
  {
    static ProfileReport singleton;
    return singleton;
  }

  void add_group(StringRefNull name,
                 uint64_t gpu_start,
                 uint64_t gpu_end,
                 uint64_t cpu_start,
                 uint64_t cpu_end)
  {
    std::scoped_lock lock(_mutex);

    size_t thread_hash = std::hash<std::thread::id>()(std::this_thread::get_id());
    int thread_id = _thread_ids.lookup_or_add(thread_hash, _thread_ids.size());

    _report << fmt::format(
        ",\n"
        R"({{"name":"{}","ph":"X","ts":{},"dur":{},"pid":1,"tid":{}}})",
        name.c_str(),
        gpu_start / uint64_t(1000),
        (gpu_end - gpu_start) / uint64_t(1000),
        thread_id);

    _report << fmt::format(
        ",\n"
        R"({{"name":"{}","ph":"X","ts":{},"dur":{},"pid":2,"tid":{}}})",
        name.c_str(),
        cpu_start / uint64_t(1000),
        (cpu_end - cpu_start) / uint64_t(1000),
        thread_id);
  }

  void add_group_cpu(StringRefNull name, uint64_t cpu_start, uint64_t cpu_end)
  {
    std::scoped_lock lock(_mutex);

    size_t thread_hash = std::hash<std::thread::id>()(std::this_thread::get_id());
    int thread_id = _thread_ids.lookup_or_add(thread_hash, _thread_ids.size());

    _report << fmt::format(
        ",\n"
        R"({{"name":"{}","ph":"X","ts":{},"dur":{},"pid":2,"tid":{}}})",
        name.c_str(),
        cpu_start / uint64_t(1000),
        (cpu_end - cpu_start) / uint64_t(1000),
        thread_id);
  }
};

}  // namespace blender::gpu
