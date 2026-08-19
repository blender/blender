/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Implementation of Section backed by TOML.
 */

#include "toml.hpp"

#include <atomic>
#include <condition_variable>
#include <optional>
#include <thread>
#include <vector>

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_global.hh"
#include "BKE_recents.hh"

#include "BLI_fileops.hh"
#include "BLI_mutex.hh"
#include "BLI_path_utils.hh"

namespace blender::recents {

constexpr toml::spec version = toml::spec::v(1, 1, 0);

/* TOML storage. Protected by recents_mutex. */
static Mutex recents_mutex;      /* protects TOML values */
static Mutex recents_init_mutex; /* used with cv for init wait */
static std::atomic<bool> recents_ready{false};
static std::condition_variable_any recents_init_cv;
static std::once_flag recents_init_once;

static std::string recents_file_path()
{
  std::optional<std::string> datafiles_path = BKE_appdir_folder_id(BLENDER_USER_CONFIG, "");
  if (!datafiles_path.has_value()) {
    return {};
  }
  datafiles_path->append(SEP_STR);
  datafiles_path->append(BLENDER_RECENTS_FILE);
  return std::move(*datafiles_path);
}

static void recents_print_errors(const std::vector<toml::error_info> &errors)
{
  for (const toml::error_info &error : errors) {
    std::string msg = toml::format_error(error);
    fprintf(stderr, "%s\n", msg.c_str());
  }
}

static const toml::value &defaults_get()
{
  static toml::value value = []() -> toml::value {
    const StringRef default_str = R"_delim_(
title = "Saved UI State Settings"
name = "Blender"

["temp.window.dimensions"]
_default = [100.0, 900.0, 200.0, 800.0]
PREFERENCES = [100.0, 940.0, 350.0, 900.0]
FILE_BROWSER = [100.0, 1160.0, 350.0, 950.0]
IMAGE_EDITOR = [50.0, 1360.0, 50.0, 830.0]
GRAPH_EDITOR = [50.0, 950.0, 200.0, 780.0]
INFO = [100.0, 1000.0, 300.0, 880.0]
OUTLINER = [100.0, 550.0, 350.0, 800.0]

["panel.sortorder"]
_default = -1

["panel.open"]
_default = true

)_delim_";
    toml::result default_result = toml::try_parse_str(default_str, version);
    if (!default_result.is_ok()) {
      recents_print_errors(default_result.unwrap_err());
      return toml::value();
    }
    return default_result.unwrap();
  }();
  return value;
}

static toml::value &current_get()
{
  static toml::value value = defaults_get();
  return value;
}

static void recents_init_impl()
{
  defaults_get();

  /* Load from on-disk file if found. */
  /* In background mode avoid any file I/O or console error output. The
   * in-memory defaults are still parsed above so API calls will work. */
  if (!G.background) {
    const std::string path = recents_file_path();
    if (!path.empty() && BLI_exists(path.c_str())) {
      toml::result file_result = toml::try_parse(path, version);
      if (file_result.is_ok()) {
        std::lock_guard lock(recents_mutex);
        current_get() = file_result.unwrap();
      }
      else {
        recents_print_errors(file_result.unwrap_err());
      }
    }
  }

  /* Mark ready and wake any waiters. */
  recents_ready.store(true, std::memory_order_release);
  recents_init_cv.notify_all();
}

void init()
{
  recents_init_impl();
}

void init_async()
{
  std::call_once(recents_init_once, []() { std::thread([]() { recents_init_impl(); }).detach(); });
}

void ensure_init()
{
  if (recents_ready.load(std::memory_order_acquire)) {
    return;
  }

  /* If async init wasn't started for some reason (like background mode), start
   * init synchronously. If async init is already scheduled then this call_once
   * will do nothing and we will wait below for the background thread to finish. */
  std::call_once(recents_init_once, recents_init_impl);

  if (recents_ready.load(std::memory_order_acquire)) {
    return;
  }

  /* Wait using BLI Mutex + condition_variable_any. */
  std::unique_lock<Mutex> lock(recents_init_mutex);
  recents_init_cv.wait(lock, [] { return recents_ready.load(std::memory_order_acquire); });
}

bool save()
{
  ensure_init();

  std::lock_guard lock(recents_mutex);
  toml::value &current = current_get();
  if (current.is_empty()) {
    return false;
  }

  /* Stamp the Blender version that wrote this file, so a future version can tell
   * how old the on-disk data is and adjust how it's read if the format changes. */
  current["blender_version"] = BLENDER_VERSION;

  /* Don't write files when running in background/headless mode. */
  if (G.background) {
    return false;
  }

  std::string toml_as_string = toml::format(current, version);
  FILE *file_handle = BLI_fopen(recents_file_path().c_str(), "w");
  if (file_handle == nullptr) {
    return false;
  }
  fputs(toml_as_string.c_str(), file_handle);
  fclose(file_handle);
  return true;
}

static const toml::value *recents_find_in(const toml::value &root,
                                          const std::string &section_name,
                                          const std::string &item_key)
{
  if (!root.is_table()) {
    /* Only tables can contain key/value pairs. */
    return nullptr;
  }

  const toml::table &root_table = root.as_table();

  if (section_name.empty()) {
    /* No section name, so look in the root of the document. */
    toml::table::const_iterator root_iter = root_table.find(item_key);
    if (root_iter != root_table.end()) {
      /* std::pair. first is key, second is value. */
      return &root_iter->second;
    }
    return nullptr;
  }

  toml::table::const_iterator section_iter = root_table.find(section_name);
  if (section_iter == root_table.end()) {
    /* The named section does not exist. */
    return nullptr;
  }
  const toml::value &section_value = section_iter->second;
  if (!section_value.is_table()) {
    /* The named section is not a table. */
    return nullptr;
  }

  /* Search the named section table for the item key value. */
  const toml::table &section_table = section_value.as_table();
  toml::table::const_iterator result = section_table.find(item_key);
  if (result != section_table.end()) {
    return &result->second;
  }

  return nullptr;
}

template<typename T> T Section::get(const std::string &item_key) const
{
  ensure_init();

  std::lock_guard lock(recents_mutex);

  const toml::value &current = current_get();

  /* Try user value first. */
  if (const toml::value *v = recents_find_in(current, section_, item_key)) {
    return toml::get_or(*v, T{});
  }

  const toml::value &defaults = defaults_get();

  /* Per-key default value. */
  if (const toml::value *v = recents_find_in(defaults, section_, item_key)) {
    return toml::get_or(*v, T{});
  }

  /* User's section-level "_default" (user override of section default). */
  if (!section_.empty()) {
    if (const toml::value *v = recents_find_in(current, section_, "_default")) {
      return toml::get_or(*v, T{});
    }
  }

  /* Builtin section-level "_default". */
  if (!section_.empty()) {
    if (const toml::value *v = recents_find_in(defaults, section_, "_default")) {
      return toml::get_or(*v, T{});
    }
  }

  /* Final fallback. False, 0, 0,0f, {}, etc. */
  return T{};
}

template<typename T> void Section::set(const std::string &item_key, const T &value)
{
  ensure_init();
  toml::value &current = current_get();

  std::lock_guard lock(recents_mutex);
  if (section_.empty()) {
    current[item_key] = value;
  }
  else {
    current[section_][item_key] = value;
  }
}

void Section::remove(const std::string &item_key)
{
  ensure_init();
  std::lock_guard lock(recents_mutex);
  toml::value &current = current_get();
  if (!current.is_table()) {
    return;
  }
  toml::table &root_table = current.as_table();
  if (section_.empty()) {
    root_table.erase(item_key);
    return;
  }
  toml::table::iterator section_iter = root_table.find(section_);
  if (section_iter == root_table.end()) {
    return;
  }
  toml::value &section_value = section_iter->second;
  if (!section_value.is_table()) {
    return;
  }
  toml::table &section_table = section_value.as_table();
  section_table.erase(item_key);
}

void Section::remove_section()
{
  ensure_init();
  std::lock_guard lock(recents_mutex);
  toml::value &current = current_get();
  if (section_.empty() || !current.is_table()) {
    return;
  }
  toml::table &root_table = current.as_table();
  root_table.erase(section_);
}

template std::string Section::get<std::string>(const std::string &item_key) const;
template void Section::set<std::string>(const std::string &item_key, const std::string &value);

template char Section::get<char>(const std::string &item_key) const;
template void Section::set<char>(const std::string &item_key, const char &value);
template bool Section::get<bool>(const std::string &item_key) const;
template void Section::set<bool>(const std::string &item_key, const bool &value);

template int16_t Section::get<int16_t>(const std::string &item_key) const;
template void Section::set<int16_t>(const std::string &item_key, const int16_t &value);

template uint16_t Section::get<uint16_t>(const std::string &item_key) const;
template void Section::set<uint16_t>(const std::string &item_key, const uint16_t &value);

template int32_t Section::get<int32_t>(const std::string &item_key) const;
template void Section::set<int32_t>(const std::string &item_key, const int32_t &value);

template uint32_t Section::get<uint32_t>(const std::string &item_key) const;
template void Section::set<uint32_t>(const std::string &item_key, const uint32_t &value);

template int64_t Section::get<int64_t>(const std::string &item_key) const;
template void Section::set<int64_t>(const std::string &item_key, const int64_t &value);

template uint64_t Section::get<uint64_t>(const std::string &item_key) const;
template void Section::set<uint64_t>(const std::string &item_key, const uint64_t &value);

template float Section::get<float>(const std::string &item_key) const;
template void Section::set<float>(const std::string &item_key, const float &value);

template double Section::get<double>(const std::string &item_key) const;
template void Section::set<double>(const std::string &item_key, const double &value);

template std::vector<int> Section::get<std::vector<int>>(const std::string &item_key) const;
template void Section::set<std::vector<int>>(const std::string &item_key,
                                             const std::vector<int> &value);

template std::vector<double> Section::get<std::vector<double>>(const std::string &item_key) const;
template void Section::set<std::vector<double>>(const std::string &item_key,
                                                const std::vector<double> &value);

template std::vector<float> Section::get<std::vector<float>>(const std::string &item_key) const;
template void Section::set<std::vector<float>>(const std::string &item_key,
                                               const std::vector<float> &value);

}  // namespace blender::recents
