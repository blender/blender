/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "GPU_shader.hh"
#include "GPU_vertex_buffer.hh"

#include "draw_pass.hh"

namespace blender {

namespace gpu {
class Batch;
class VertBuf;
class UniformBuf;
}  // namespace gpu

struct Object;
struct PointCloud;
struct GPUMaterial;

namespace draw {

struct GSplatModule;
struct GSplatBatchCache;

/* Computue passes are categorized. Some renderers only require projection of gaussians,
 * while others also require some or all of the spherical harmonics to be evaluated. */
enum class GSplatEvalShader {
  /* Ellipse computation only. */
  Ellipses,
  /* Degree 0-3 spherical harmonics. */
  Radiance,
  /* Ellipses, and degree 0-3 spherical harmonics. */
  EllipsesRadiance,
};

/**
 * Cache object that stores batches and buffers for drawing a gsplat, and is
 * responsible for filling these from the object's attributes.
 *
 * Pointers to batches/buffers are provided through `_get()`, and are not
 * present until `ensure_requested()` is called.
 */
struct GSplatEvalCache {
  friend GSplatModule;
  friend GSplatBatchCache;

  /* --- Batches. --- */

  /* Triangle primitive types. */
  gpu::Batch *surface;
  gpu::Batch **surface_per_mat;
  /* Dot primitive types. */
  gpu::Batch *dots;
  gpu::Batch *edit_dots;

  /* --- Builtin attribute buffers. --- */

  /* Packed mean, quaternion, scaling per unit: 4x32ui. */
  gpu::VertBuf *shape_data_buf;
  /* Packed opacity and spherical harmonics per unit: 16x32ui. */
  gpu::VertBuf *radiance_data_buf;
  /* Miscellaneous data, mostly for shader-side unpacking. */
  gpu::UniformBuf *infos_buf;

  /* --- Intermediate compute buffers. --- */

  /* Compute intermediate output: per-splat ellipse data. */
  gpu::VertBuf *ellipses_comp_buf;
  /* Compute intermediate output: per-splat radiance value. */
  gpu::VertBuf *radiance_comp_buf;

  /* --- Other attribute buffers. --- */

  /* Active attribute in 3D view. */
  gpu::VertBuf *attr_viewer;
  /* Requested attributes. */
  gpu::VertBuf *attributes_buf[GPU_MAX_ATTR];
  /* Index buffer for selection dots. */
  gpu::IndexBuf *edit_dots_indices = nullptr;

  /* --- Internal batch/buffer builders. --- */

  void ensure_builtin_buffers(PointCloud &pointcloud);
  void ensure_compute_buffers(PointCloud &pointcloud);
  void ensure_edit_dots_indices(PointCloud &pointcloud);
  void ensure_attribute(PointCloud &pointcloud, int attrib_i);

 public:
  /* --- Generic attribute infos. --- */

  /** Attributes currently being drawn or about to be drawn. */
  VectorSet<std::string> attr_used;

  /**
   * Attributes that were used at some point. This is used for garbage collection, to remove
   * attributes that are not used in shaders anymore due to user edits.
   */
  VectorSet<std::string> attr_used_over_time;

  /**
   * The last time in seconds that the `attr_used` and `attr_used_over_time` were exactly the same.
   * If the delta between this time and the current scene time is greater than the timeout set in
   * user preferences (`U.vbotimeout`) then garbage collection is performed.
   */
  int last_attr_matching_time;

  /* Nr. of batches in surface_per_mat. */
  int mat_len;

  /* --- API --- */

  /* Get the eval cache for a object. */
  static GSplatEvalCache &get(PointCloud &pointcloud);

  /* Request pointers to buffers that may not be filled yet, see `ensure_requested()`. */
  gpu::VertBuf *shape_data_buf_get();
  gpu::VertBuf *radiance_data_buf_get();
  gpu::VertBuf *ellipses_comp_buf_get();
  gpu::VertBuf *radiance_comp_buf_get();
  gpu::UniformBuf *infos_buf_get();
  gpu::VertBuf **attributes_get(uint request_i);

  /* Request pointers to batches that may not be filled yet, see `ensure_requested()`. */
  gpu::Batch *dots_get();
  gpu::Batch *edit_dots_get();
  gpu::Batch *surface_get(const PointCloud &pointcloud);

  /* Fill and build all requestedf buffers/batches. */
  void ensure_requested(PointCloud &pointcloud);

  /* Safely discard buffers, batches, etc. */
  void discard_buffers();
  void discard_attributes();
};

/* Stored in DRWData. */
struct GSplatModule {
  gpu::VertBuf *dummy_vbo = create_dummy_vbo();
  gpu::UniformBuf *dummy_ubo = create_dummy_ubo();

  ~GSplatModule()
  {
    GPU_VERTBUF_DISCARD_SAFE(dummy_vbo);
    GPU_UBO_FREE_SAFE(dummy_ubo);
  }

  void begin_sync();
  void sync_object(const ObjectRef &ob_ref, const ResourceHandleRange &res_handle);
  void update(draw::Manager &manager, draw::View &view, GSplatEvalShader type);

 private:
  Set<ObjectKey> compute_objects_;

  /* Passes matching `GSplatEvalShader` selection. */
  PassSimple compute_dummy_ps_ = {"GSplatPassEmpty"};
  PassSimple compute_ellipses_ps_ = {"GSplatPassEllipses"};
  PassSimple compute_radiance_ps_ = {"GSplatPassRadiance"};
  PassSimple compute_ellipses_radiance_ps_ = {"GSplatPassEllipsesRadiance"};

  PassSimple &get_ps(GSplatEvalShader type)
  {
    switch (type) {
      case GSplatEvalShader::Ellipses:
        return compute_ellipses_ps_;
      case GSplatEvalShader::Radiance:
        return compute_radiance_ps_;
      case GSplatEvalShader::EllipsesRadiance:
        return compute_ellipses_radiance_ps_;
      default:
        return compute_dummy_ps_;
    }
  }

  gpu::VertBuf *create_dummy_vbo()
  {
    GPUVertFormat format = {0};
    uint dummy_id = GPU_vertformat_attr_add(
        &format, "dummy", gpu::VertAttrType::SFLOAT_32_32_32_32);

    gpu::VertBuf *vbo = GPU_vertbuf_create_with_format_ex(
        format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

    const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_vertbuf_data_alloc(*vbo, 1);
    GPU_vertbuf_attr_fill(vbo, dummy_id, vert);
    return vbo;
  }

  gpu::UniformBuf *create_dummy_ubo()
  {
    const uint4 data = uint4(0u);
    gpu::UniformBuf *ubo = GPU_uniformbuf_create_ex(sizeof(uint4), &data, "DummyUBO");
    return ubo;
  }
};

}  // namespace draw
}  // namespace blender
