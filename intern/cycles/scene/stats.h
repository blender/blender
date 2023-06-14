/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __RENDER_STATS_H__
#define __RENDER_STATS_H__

#include "scene/scene.h"

#include "util/stats.h"
#include "util/string.h"
#include "util/vector.h"

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

class NamedTimeEntry {
 public:
  NamedTimeEntry();
  NamedTimeEntry(const string &name, double time);

  string name;
  double time;
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

class NamedTimeStats {
 public:
  NamedTimeStats();

  /* Add entry to the statistics. */
  void add_entry(const NamedTimeEntry &entry)
  {
    total_time += entry.time;
    entries.push_back(entry);
  }

  /* Generate full human-readable report. */
  string full_report(int indent_level = 0);

  /* Total time of all entries. */
  double total_time;

  /* NOTE: Is fine to read directly, but for adding use add_entry(), which
   * makes sure all accumulating  values are properly updated.
   */
  vector<NamedTimeEntry> entries;

  void clear()
  {
    total_time = 0.0;
    entries.clear();
  }
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

class UpdateTimeStats {
 public:
  /* Generate full human-readable report. */
  string full_report(int indent_level = 0);

  NamedTimeStats times;
};

class SceneUpdateStats {
 public:
  SceneUpdateStats();

  UpdateTimeStats geometry;
  UpdateTimeStats image;
  UpdateTimeStats light;
  UpdateTimeStats object;
  UpdateTimeStats background;
  UpdateTimeStats bake;
  UpdateTimeStats camera;
  UpdateTimeStats film;
  UpdateTimeStats integrator;
  UpdateTimeStats osl;
  UpdateTimeStats particles;
  UpdateTimeStats scene;
  UpdateTimeStats svm;
  UpdateTimeStats tables;
  UpdateTimeStats procedurals;

  string full_report();

  void clear();
};

CCL_NAMESPACE_END

#endif /* __RENDER_STATS_H__ */
