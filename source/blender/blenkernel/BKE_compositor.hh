/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <string>

#include "BLI_compute_context.hh"
#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender {

struct Scene;
struct View3D;
struct RegionView3D;
struct BlendWriter;
struct BlendDataReader;
struct Main;
struct ViewLayer;
struct LibraryForeachIDData;
struct ImBuf;
struct bContext;
struct SceneCompositorEffect;
struct DepsNodeHandle;
struct bNodeTree;
struct PointerRNA;

namespace bke::compositor {

/* --------------------------------------------------------------------
 * Cache.
 */

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

/* --------------------------------------------------------------------
 * Scene Compositor Effects.
 */

enum class ExecutionMode : uint8_t {
  /* The compositor is executing for a final render. */
  Render,
  /* The compositor is executing for a preview, like the interactive compositor or the viewport
   * compositor. */
  Preview,
};

/* Returns true if the given scene has any enabled effect for the given execution mode. */
bool is_enabled(const Scene &scene, ExecutionMode mode);

/* Gets the compositor effect with the given name in the given scene. */
SceneCompositorEffect *get_effect(const Scene &scene, StringRef name);

/* Gets the active compositor effect in the given scene. */
SceneCompositorEffect *get_active_effect(const Scene &scene);

/* Returns true if the given effect is enabled for the given execution mode. */
bool is_effect_enabled(const SceneCompositorEffect &effect, ExecutionMode mode);

/* Sets the given compositor effect in the given scene to be the active one. */
void set_active_effect(const Scene &scene, SceneCompositorEffect &effect);

/* Rename the given compositor effect in the given scene to the given name. Animation data paths
 * may be updated if update_animation_data is true. */
void rename_effect(Scene &scene,
                   SceneCompositorEffect &effect,
                   StringRef new_name,
                   bool update_animation_data = true);

/* Adds a new compositor effect of the given name to the given scene. */
SceneCompositorEffect &new_effect(Scene &scene, StringRef name);

/* Copy the given compositor effect in the given scene. */
SceneCompositorEffect &duplicate_effect(Scene &scene, SceneCompositorEffect &source_effect);

/* Frees the given compositor effect from the given scene. Does not decrement user counts. */
void free_effect(Scene &scene, SceneCompositorEffect &effect);

/* Removes the given compositor effect from the given scene. */
void remove_effect(Scene &scene, SceneCompositorEffect &effect);

/* Copy the effects from the given source scene to the given destination scene using the given ID
 * copying flags. */
void copy_effects(Scene &target_scene, const Scene &source_scene, const int flags);

/* Frees all compositor effects in the given scene. Does not decrement user counts. */
void free_effects(Scene &scene);

/* Removes all compositor effects in the given scene. */
void clear_effects(Scene &scene);

/* Walk over each ID in the effects of the given scene executing the callback defined by the given
 * data. */
void for_each_id_in_effects(const Scene &scene, LibraryForeachIDData &data);

/* Write the effects of the given scene to the given blend file writer. */
void write_effects(const Scene &scene, BlendWriter &writer);

/* Read the effects of the given scene from the given blend file reader. */
void read_effects(Scene &scene, BlendDataReader &reader);

/* Gets the effect that the given property belongs to. */
const SceneCompositorEffect *get_effect_from_property(const PointerRNA &property_ptr);

/* Update the system properties of the effect. Should be call whenever the node group of the
 * effect changes or the interface of the assigned node group changes. */
void update_effect_node_group_interface(Main &main, Scene &scene, SceneCompositorEffect &effect);

/* --------------------------------------------------------------------
 * Query.
 */

/* Get the set of all passes used by the compositor for the given view layer and execution mode,
 * identified by their pass names. This might be a superset of the passes actually supported by the
 * render engine, in which case, the compositor will return an invalid output and issue a
 * warning. */
Set<std::string> get_used_passes(const Scene &scene,
                                 const ViewLayer *view_layer,
                                 ExecutionMode mode);

/* Checks if the viewport compositor is currently enabled in the given 3D viewport. */
bool is_viewport_compositor_enabled(const View3D &view_3d, const RegionView3D &region_view_3d);

/* Checks if the viewport compositor is currently being used in any 3D viewport. */
bool is_viewport_compositor_used(const bContext &context);

/* --------------------------------------------------------------------
 * Depsgraph.
 */

/* Add the depsgraph relations needed by the given scene compositor effect in the given scene. A
 * handle for the compositor output depsgraph node is given to be the target of the relation. */
void add_depsgraph_relations(Scene &scene,
                             const SceneCompositorEffect &effect,
                             DepsNodeHandle *compositor_output_depsgraph_node);

/* --------------------------------------------------------------------
 * Compute Contexts.
 */

/* Computes the hash of the compositor active compute context. The active compute context is the
 * context that the user last interacted with, see root_node_group.active_viewer_key for more
 * information. */
ComputeContextHash compute_active_compute_context_hash(const Scene &scene);
ComputeContextHash compute_active_compute_context_hash(const Scene &scene,
                                                       const bNodeTree &root_node_group);

}  // namespace bke::compositor
}  // namespace blender
