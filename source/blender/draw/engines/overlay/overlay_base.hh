/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_private.hh"

namespace blender::draw::overlay {

/**
 * Base overlay class used for documentation.
 *
 * This is not actually used as all methods should always be called from the derived class.
 * There is still some external conditional logic and draw ordering that needs to be adjusted on a
 * per overlay basis inside the `overlay::Instance`.
 */
struct Overlay {
  /**
   * IMPORTANT: Overlays are used for every area using GPUViewport (i.e. View3D, UV Editor,
   * Compositor ...). They are also used for depth picking and selection. This means each overlays
   * must decide when they are active. The begin_sync method must initialize the `enabled_`
   * member depending on the context state, and every method should implement an early out cases.
   */
  bool enabled_ = false;

  /**
   * Synchronization creates and fill render passes based on context state and scene state.
   *
   * It runs for every scene update, so keep computation overhead low.
   * If it is triggered, everything in the scene is considered updated.
   * Note that this only concerns the render passes, the mesh batch caches are updated
   * on a per object-data basis.
   *
   * IMPORTANT: Synchronization must be view agnostic. That is, not rely on view position,
   * projection matrix or frame-buffer size to do conditional pass creation. This is because, by
   * design, syncing can happen once and rendered multiple time (multi view rendering, stereo
   * rendering, orbiting view ...). Conditional pass creation, must be done in the drawing
   * callbacks, but they should remain the exception. Also there will be no access to object data
   * at this point.
   */

  /**
   * Creates passes used for object sync and enabling / disabling internal overlay types
   * (e.g. vertices, edges, faces in edit mode).
   * Runs once at the start of the sync cycle.
   * Should also contain passes setup for overlays that are not per object overlays (e.g. Grid).
   *
   * This method must be implemented.
   */
  virtual void begin_sync(Resources & /*res*/, const State & /*state*/) = 0;

  /**
   * Fills passes or buffers for each object.
   * Runs for each individual object state.
   * IMPORTANT: Can run only once for instances using the same state (#ObjectRef might contains
   * instancing data).
   */
  virtual void object_sync(Manager & /*manager*/,
                           const ObjectRef & /*ob_ref*/,
                           Resources & /*res*/,
                           const State & /*state*/) {};

  /**
   * Fills passes or buffers for each object in edit mode.
   * Runs for each individual object state for a specific mode.
   * IMPORTANT: Can run only once for instances using the same state (#ObjectRef might contains
   * instancing data).
   */
  virtual void edit_object_sync(Manager & /*manager*/,
                                const ObjectRef & /*ob_ref*/,
                                Resources & /*res*/,
                                const State & /*state*/) {};

  /**
   * Finalize passes or buffers used for object sync.
   * Runs once at the start of the sync cycle.
   */
  virtual void end_sync(Resources & /*res*/, const State & /*state*/) {};

  /**
   * Warms #PassMain and #PassSortable to avoid overhead of pipeline switching.
   * Should only contains calls to `generate_commands`.
   * NOTE: `view` is guaranteed to be the same view that will be passed to the draw functions.
   */
  virtual void pre_draw(Manager & /*manager*/, View & /*view*/) {};

  /**
   * Drawing can be split into multiple passes. Each callback draws onto a specific frame-buffer.
   * The order between each draw function is guaranteed. But it is not guaranteed that no other
   * overlay will render in between. The overlay can render to a temporary frame-buffer before
   * resolving to the given frame-buffer.
   */

  virtual void draw_on_render(gpu::FrameBuffer * /*fb*/, Manager & /*manager*/, View & /*view*/) {
  };
  virtual void draw(Framebuffer & /*fb*/, Manager & /*manager*/, View & /*view*/) {};
  virtual void draw_line(Framebuffer & /*fb*/, Manager & /*manager*/, View & /*view*/) {};
  virtual void draw_line_only(Framebuffer & /*fb*/, Manager & /*manager*/, View & /*view*/) {};
  virtual void draw_color_only(Framebuffer & /*fb*/, Manager & /*manager*/, View & /*view*/) {};
  virtual void draw_output(Framebuffer & /*fb*/, Manager & /*manager*/, View & /*view*/) {};
};

}  // namespace blender::draw::overlay
