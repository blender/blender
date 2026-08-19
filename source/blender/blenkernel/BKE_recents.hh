/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Recents API for persisted UI state.
 *
 * This namespace provides a small, convenient API to read and write
 * small pieces of persisted UI state (window geometry, panel flags,
 * last-used filters, etc.) organized in named "sections".
 *
 * Data is stored as TOML and split into:
 * - `recents_current` (user-modifiable values, saved to disk)
 * - `recents_default` (builtin defaults embedded in the binary)
 *
 * Lookup order (when reading a key)
 * 1. User value in `recents_current`
 * 2. Per-key default in `recents_default`
 * 3. Section-level `_default` in `recents_current` (user override)
 * 4. Section-level `_default` in `recents_default` (builtin section default)
 * 5. Final fallback: value-initialized `T{}`
 *
 * Versioning
 *   `save()` saves a root-level `blender_version` key (see #BLENDER_VERSION) with the version
 *   that wrote the file. Code that needs to handle older data specifically can read it via
 *   `recents::Section("").get<int32_t>("blender_version")`.
 *
 * Threading / initialization
 *   Call `recents::init()` or `recents::init_async()` early in startup to populate runtime data
 *   from the built-in defaults and the on-disk file. The module will lazily ensure initialization
 *   when needed, so explicit init is optional but recommended to avoid synchronous waits.
 *
 * Typical usage
 *   // Read
 *   auto bounds =
 *       recents::Section("temp.window.dimensions").get<std::vector<float>>("PREFERENCES");
 *
 *   // Write
 *   recents::Section("temp.window.dimensions").set("PREFERENCES", bounds);
 *
 *   // Explicit get/set
 *   auto s = recents::Section("panels");
 *   bool is_open = s.get<bool>("open");
 *   s.set("width", 12);
 */

#pragma once

#include <string>

namespace blender::recents {

class Section {
  std::string section_;

 public:
  explicit Section(std::string section) : section_(std::move(section)) {}

  template<typename T> T get(const std::string &item_key) const;
  template<typename T> void set(const std::string &item_key, const T &value);

  void remove(const std::string &item_key);
  void remove_section();
};

void init();
void init_async();
void ensure_init();
bool save();

}  // namespace blender::recents
