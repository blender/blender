/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <string>

#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

namespace blender {

struct Scene;
struct ViewLayer;
struct ImBuf;
struct bContext;
struct DepsNodeHandle;
struct bNodeTree;

namespace bke::compositor {

struct Cache {
  struct FrameKey {
    int frame_number = 0;
    int view_identifier = 0;

    uint64_t hash() const
    {
      return get_default_hash(frame_number, view_identifier);
    }

    friend bool operator==(const FrameKey &a, const FrameKey &b) = default;
  };

 private:
  /* A cache of final interactive compositor results across frames. */
  Map<FrameKey, ImBuf *> frames_;
  /* A mutex for accessing frames_. The frames cache is intrinsically thread-safe since all access
   * happen in the same interactive compositor job, except for drawing cache overlays since it can
   * happen simultaneously while the job is running, that's why the mutex is needed. */
  Mutex frames_mutex_;

 public:
  /* Clear all caches. */
  ~Cache();

  /* Get the frame cache corresponding to the given frame number and view. */
  const ImBuf *get_frame(int frame_number, int view_identifier);

  /* Add a new frame cache entry. If the new entry would surpass the memory cache limit, frames
   * will be evicted to make room. */
  void add_frame(int frame_number, int view_identifier, ImBuf *image_buffer);

  /* Clears the frames cache. */
  void clear_frames();

  /* Computes a list of every contiguous segment of cached frames. Can be used to draw which frame
   * ranges are cached. */
  Vector<IndexRange> compute_frame_ranges();

 private:
  /* Delete one entry from the frames cache given the current frame number. If a cached frame exist
   * before the current frame, the furthest one will be removed, otherwise, the furthest cached
   * frame after the current frame will be removed. */
  void evict_frame(int current_frame_number);

  /* Computes the total size of the cache in bytes. */
  int64_t size();
};

/* Get the set of all passes used by the compositor for the given view layer, identified by their
 * pass names. This might be a superset of the passes actually supported by the render engine, in
 * which case, the compositor will return an invalid output and issue a warning. */
Set<std::string> get_used_passes(const Scene &scene, const ViewLayer *view_layer);

/* Checks if the viewport compositor is currently being used. This is similar to
 * DRWContext::is_viewport_compositor_enabled but checks all 3D views. */
bool is_viewport_compositor_used(const bContext &context);

/* Note: Links to the File Output node do not guarantee it will write a result to disk, e.g. if
 * Menu Switch nodes exists but it's a good estimation without evaluating the node tree. */
bool node_tree_has_linked_file_output(const bNodeTree *node_tree);

/* Add the depsgraph relations needed by the compositor node tree of the given scene. A handle for
 * the compositor output depsgraph node is given to be the target of the relation. */
void add_depsgraph_relations(Scene &scene, DepsNodeHandle *compositor_output_depsgraph_node);

}  // namespace bke::compositor
}  // namespace blender
