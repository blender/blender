/*
 * Copyright 2011-2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __RENDER_STATS_H__
#define __RENDER_STATS_H__

#include "render/scene.h"

#include "util/util_stats.h"
#include "util/util_string.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

/* Named statistics entry, which corresponds to a size. There is no real
 * semantic around the units of size, it just should be the same for all
 * entries.
 *
 * This is a generic entry for all size-related statistics, which helps
 * avoiding duplicating code for things like sorting.
 */
class NamedSizeEntry {
 public:
  NamedSizeEntry();
  NamedSizeEntry(const string &name, size_t size);

  string name;
  size_t size;
};

/* Container of named size entries. Used, for example, to store per-mesh memory
 * usage statistics. But also keeps track of overall memory usage of the
 * container.
 */
class NamedSizeStats {
 public:
  NamedSizeStats();

  /* Add entry to the statistics. */
  void add_entry(const NamedSizeEntry &entry);

  /* Generate full human-readable report. */
  string full_report(int indent_level = 0);

  /* Total size of all entries. */
  size_t total_size;

  /* NOTE: Is fine to read directly, but for adding use add_entry(), which
   * makes sure all accumulating  values are properly updated.
   */
  vector<NamedSizeEntry> entries;
};

class NamedNestedSampleStats {
 public:
  NamedNestedSampleStats();
  NamedNestedSampleStats(const string &name, uint64_t samples);

  NamedNestedSampleStats &add_entry(const string &name, uint64_t samples);

  /* Updates sum_samples recursively. */
  void update_sum();

  string full_report(int indent_level = 0, uint64_t total_samples = 0);

  string name;

  /* self_samples contains only the samples that this specific event got,
   * while sum_samples also includes the samples of all sub-entries. */
  uint64_t self_samples, sum_samples;

  vector<NamedNestedSampleStats> entries;
};

/* Named entry containing both a time-sample count for objects of a type and a
 * total count of processed items.
 * This allows to estimate the time spent per item. */
class NamedSampleCountPair {
 public:
  NamedSampleCountPair(const ustring &name, uint64_t samples, uint64_t hits);

  ustring name;
  uint64_t samples;
  uint64_t hits;
};

/* Contains statistics about pairs of samples and counts as described above. */
class NamedSampleCountStats {
 public:
  NamedSampleCountStats();

  string full_report(int indent_level = 0);
  void add(const ustring &name, uint64_t samples, uint64_t hits);

  typedef unordered_map<ustring, NamedSampleCountPair, ustringHash> entry_map;
  entry_map entries;
};

/* Statistics about mesh in the render database. */
class MeshStats {
 public:
  MeshStats();

  /* Generate full human-readable report. */
  string full_report(int indent_level = 0);

  /* Input geometry statistics, this is what is coming as an input to render
   * from. say, Blender. This does not include runtime or engine specific
   * memory like BVH.
   */
  NamedSizeStats geometry;
};

/* Statistics about images held in memory. */
class ImageStats {
 public:
  ImageStats();

  /* Generate full human-readable report. */
  string full_report(int indent_level = 0);

  NamedSizeStats textures;
};

/* Render process statistics. */
class RenderStats {
 public:
  RenderStats();

  /* Return full report as string. */
  string full_report();

  /* Collect kernel sampling information from Stats. */
  void collect_profiling(Scene *scene, Profiler &prof);

  bool has_profiling;

  MeshStats mesh;
  ImageStats image;
  NamedNestedSampleStats kernel;
  NamedSampleCountStats shaders;
  NamedSampleCountStats objects;
};

CCL_NAMESPACE_END

#endif /* __RENDER_STATS_H__ */
