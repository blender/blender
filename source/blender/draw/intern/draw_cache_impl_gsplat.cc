/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief GSplat API for render engines
 *
 * Draw cache creating buffers and batches for the gsplat implementation. This handles
 * quantized packing of gsplat attribute data. For matching unpacking, refer to the
 * `draw_gsplat_lib.bsl.hh` shader-side.
 *
 * References:
 *
 *  [quat2017]  Mark Reynolds
 *              Quaternion quantization
 *              https://marc-b-reynolds.github.io/quaternions/2017/05/02/QuatQuantPart1.html
 *
 *  [gspt2026]  Joris Rijsdijk et al.
 *              Gaussian Point Splatting
 *              ACM Transactions on Graphics (SIGGRAPH 2026)
 *              https://jorisar.nl/gaussian_point_splatting/
 */

#include <cstring>

#include <fmt/format.h>

#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_color_types.hh"
#include "BLI_listbase.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.hh"

#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_userdef_types.h"

#include "BKE_attribute.hh"
#include "BKE_material.hh"
#include "BKE_pointcloud.hh"

#include "GPU_batch.hh"
#include "GPU_material.hh"
#include "GPU_uniform_buffer.hh"

#include "DRW_render.hh"

#include "draw_attributes.hh"
#include "draw_cache_impl.hh"
#include "draw_cache_inline.hh"
#include "draw_gsplat_private.hh" /* own include */
#include "draw_shader_shared.hh"

namespace blender::draw {

/* Draw module and real-time engines (EEVEE, Overlay, Workbench) use max
 * 3 spherical harmonics degrees for performance and memory reasons. */
constexpr uint32_t radiance_degs_max = 3;

/* Nr. of coefficients matching 3 degree spherical harmonics. */
constexpr uint32_t radiance_coeffs_max = (radiance_degs_max + 1u) * (radiance_degs_max + 1u);

/* Sane defaults for missing but non-zero attribute data. */
constexpr float value_scale_default = 1e-3f;
constexpr float value_opacity_default = 0.5f;

namespace detail {

/**
 * Given an IndexMask, produce a gpu::IndexBuffer holding mask indices.
 */
static void index_mask_to_ibo(const IndexMask &mask, gpu::IndexBuf &ibo)
{
  const int max_index = mask.min_array_size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, mask.size(), max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);
  mask.to_indices<int>(data.cast<int>());
  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, &ibo);
}

/**
 * Expmap packing [quat2017] of quaternion data.
 *
 * \note Keep in sync with `unpack_quaternion_3_to_4` in `draw_gsplat_lib.bsl.hh`.
 */
BLI_INLINE float3 pack_quaternion_4_to_3(const float4 &q)
{
  float w = q.w;
  float a = 1.0f - w * w;
  float b = 1.0f / sqrt(a + 1.0e-8f);
  float k = a * b;
  float s = (2.0f / M_PI) * atan(k / w) * b;
  return s * float3(q);
}

/**
 * Tightly pack Gaussian shape data to a uint4.
 *
 * This is an adapted version of the quantization in [gspt2026], which relies on separately
 * stored boundaries in a UBO. Our variant reduces Gaussian mean to 3x22b, scale to 3x9b, and
 * rotation to 3x9b. We additionally pack Gaussian opacity in 8b. Across 4 uints, data is then
 * distributed as follows:
 *
 * - word 0: [ meanx, 22 ] [ meany, 10 ]
 * - word 1: [ meany, 12 ] [ meanz, 20 ]
 * - word 2: [ meanz,  2 ] [ quatx,  9 ] [ quaty, 9 ] [ quatz, 9 ] [ scalx, 3 ]
 * - word 3: [ scalx,  6 ] [ scaly,  9 ] [ scalz, 9 ] [ opaci, 8 ]
 *
 * \note Keep in sync with `unpack_gaussian_*` in `draw_gsplat_lib.bsl.hh`.
 */
BLI_INLINE uint4 pack_gaussian(const float3 &bounded_mean,
                               const float3 &bounded_log_scale,
                               const math::Quaternion &rotation,
                               float opacity)
{
  /* Quantize mean to `pu`: 3x22b. */
  uint3 pu = static_cast<uint3>(bounded_mean * ((1 << 22) - 1u));
  /* Quantize scaling to `su`: 3x9b. */
  uint3 su = static_cast<uint3>(bounded_log_scale * 511.0f);
  /* Quantize opacity to `au`, 1x8b. */
  uint au = static_cast<uint>(clamp(opacity, 0.0f, 1.0f) * 255.0f);
  /* Quantize rotation to `ru`: 3x9b.
   * Note that we shuffle quaternion `wxyz` to `xyzw` for consistency inside draw module shaders.
   */
  float4 q = math::normalize(float4(rotation.x, rotation.y, rotation.z, rotation.w));
  if (q.w < 0.0f) {
    q = -q;
  }
  float3 vr = math::clamp((pack_quaternion_4_to_3(q) + 1.0f) * 0.5f, 0.0f, 1.0f);
  uint3 ru = static_cast<uint3>(vr * 511.0f);

  /* Now for the fun part: pack quantized data to 4 words. */
  uint4 w;
  w.x = /* 22b */ (pu.x & 0x3FFFFFu) |
        /* 10b */ ((pu.y & 0x0003FFu) << 22);
  w.y = /* 12b */ ((pu.y >> 10) & 0x000FFFu) |
        /* 20b */ ((pu.z & 0x0FFFFFu) << 12);
  w.z = /* 02b */ ((pu.z >> 20) & 0x000003u) |
        /* 09b */ ((ru.x & 0x0001FFu) << 2) |
        /* 09b */ ((ru.y & 0x0001FFu) << 11) |
        /* 09b */ ((ru.z & 0x0001FFu) << 20) |
        /* 03b */ ((su.x & 0x000007u) << 29);
  w.w = /* 06b */ ((su.x >> 3) & 0x00003Fu) |
        /* 09b */ ((su.y & 0x0001FFu) << 6) |
        /* 09b */ ((su.z & 0x0001FFu) << 15) |
        /* 08b */ ((au & 0x0000FFu) << 24);
  return w;
}

struct RadiancePack {
  uint8_t radiance_sh[48];
};
static_assert(sizeof(RadiancePack) == 48ul);

/**
 * Tightly pack radiance and opacity data to 4x uint4.
 *
 * This quantizes 3 channels of spherical harmonics degrees 0-3 using 8b per coefficient, so
 * 3x16 coefficients. Note that the draw module restricts spherical harmonics to 3 degrees;
 * packing a 4th would greatly increase memory and packing complexity for little gain.
 *
 * The coefficients are distributed over 12 words as follows:
 *
 *  [0, 1,  2,  3] : [xyzx][yzxy][zxyz][xyzx]
 *  [4, 5,  6,  7] : [yzxy][zxyz][xyzx][yzxy]
 *  [8, 9, 10, 11] : [zxyz][xyzx][yzxy][zxyz]
 *
 * \note Keep in sync with `eval_radiance` in `draw_gsplat.bsl.hh`.
 */
BLI_INLINE RadiancePack pack_radiance(const Array<float3, radiance_coeffs_max> &radiance_sh)
{
  RadiancePack pack;
  for (uint32_t i = 0u; i < radiance_coeffs_max; ++i) {
    /* Quantize float3 coefficients to 3x8b. */
    uchar3 shu = static_cast<uchar3>(math::clamp(radiance_sh[i] * 255.0f, 0.0f, 255.0f));
    /* Coefficients are stored in packs with a padding byte every 15 bytes.
     * This simplifies unpacking shader-side, until we find more data to pack in. */
    uchar3 *shu_data = reinterpret_cast<uchar3 *>(pack.radiance_sh + (3 * i));
    *shu_data = shu;
  }
  return pack;
}

/**
 * Collect virtual arrays for existing pointcloud attributes that store the various
 * spherical harmonics coefficients, up to the maximum supported number.
 */
static Vector<VArraySpan<float3>> read_radiance_sh_attributes(
    const bke::AttributeAccessor &attributes)
{
  Vector<VArraySpan<float3>> arrays;
  for (uint i = 0; i < (radiance_coeffs_max - 1); ++i) {
    VArraySpan<float3> array_i = *attributes.lookup<float3>(fmt::format("radiance:sh_{}", i));
    if (array_i.is_empty()) {
      /* If a coefficient does not exist, we can stop reading. */
      break;
    }
    arrays.append(std::move(array_i));
  }
  return arrays;
}

}  // namespace detail

/* -------------------------------------------------------------------- */
/** \name GSplatEvalCache management
 * \{ */

void GSplatEvalCache::ensure_builtin_buffers(PointCloud &pointcloud)
{
  /* Accessors over attributes. Arrays are default-initialized to values s.t. missing or invalid
   * scale/rotation/radiance do not cause rendering to fail, instead falling back to defaults.*/
  const bke::AttributeAccessor attributes = pointcloud.attributes();
  const Span<float3> mean = pointcloud.positions();
  const VArray<float3> exp_scale = *attributes.lookup_or_default<float3>(
      "scale", bke::AttrDomain::Point, float3(value_scale_default));
  const VArray<math::Quaternion> quaternion = *attributes.lookup_or_default<math::Quaternion>(
      "rotation", bke::AttrDomain::Point, math::Quaternion(0.0f, 0.0f, 0.0f, 1.0f));
  const VArray<float4> radiance_base = *attributes.lookup_or_default<float4>(
      "radiance:base", bke::AttrDomain::Point, float4(0.0f, 0.0f, 0.0f, value_opacity_default));
  const Vector<VArraySpan<float3>> radiance_sh = detail::read_radiance_sh_attributes(attributes);

  /* An activation function `exp(x)` is typically applied to scaling coefficients before rendering,
   * as the scale data comes directly from the fitting process. We strip this function for packing
   * and re-apply it when unpacking shader-side. Note that 0 is a valid input through attributes,
   * but as ln(0) is undefined we instead pack an epsilon. */
  Array<float3> scale(exp_scale.size());
  threading::parallel_for(exp_scale.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      scale[i] = math::is_any_zero(exp_scale[i]) ? float3(-1e6f) :
                                                   math::log(math::abs(exp_scale[i]));
    }
  });

  /* Obtain data boundaries; we quantize uploaded data such that it lies within [0, 1]. */
  Bounds<float3> splat_mean_bounds = pointcloud.bounds_min_max(false).value_or(Bounds<float3>());
  Bounds<float3> splat_scale_bounds = bounds::min_max(scale.as_span()).value_or(Bounds<float3>());
  auto radiance_rgba_bounds = bounds::min_max(radiance_base).value_or(Bounds<float4>());
  /* Omit opacity, which is packed with Gaussian data and clamped to lie in [0, 1]. */
  Bounds<float3> radiance_base_bounds = Bounds(radiance_rgba_bounds.min.xyz(),
                                               radiance_rgba_bounds.max.xyz());
  Bounds<float3> radiance_sh_bounds = Bounds<float3>(float3(0.0f));
  for (const VArraySpan<float3> &radiance_sh_i : radiance_sh) {
    if (auto bounds = bounds::min_max(radiance_sh_i); bounds.has_value()) {
      /* Accumulate sh degrees 1-3 into bounds. */
      radiance_sh_bounds = bounds::merge(radiance_sh_bounds, *bounds);
    }
  }

  /* Scaling range helpers to quantize values inside the data boundaries to [0, 1].  */
  ScalingRange splat_mean_range = {.range_add = splat_mean_bounds.min,
                                   .range_mul = splat_mean_bounds.size()};
  ScalingRange splat_scale_range = {.range_add = splat_scale_bounds.min,
                                    .range_mul = splat_scale_bounds.size()};
  ScalingRange radiance_base_range = {.range_add = radiance_base_bounds.min,
                                      .range_mul = radiance_base_bounds.size()};
  ScalingRange radiance_sh_range = {.range_add = radiance_sh_bounds.min,
                                    .range_mul = radiance_sh_bounds.size()};

  /* Place range helpers in a UBO for unpacking quantized data shader-side. */
  GSplatInfos ubo_data = {.splat_mean_range = splat_mean_range,
                          .splat_scale_range = splat_scale_range,
                          .radiance_base_range = radiance_base_range,
                          .radiance_sh_range = radiance_sh_range};
  GPU_uniformbuf_update(infos_buf, &ubo_data);

  /* Gaussian shape and radiance data is packed into uint4 and uint4x3 buffers respectively. */
  static constexpr GPUUsageType usage_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  shape_data_buf->init(gpu::GenericVertexFormat<uint4>::format(), usage_flag);
  shape_data_buf->allocate(mean.size());
  radiance_data_buf->init(gpu::GenericVertexFormat<uint4>::format(), usage_flag);
  radiance_data_buf->allocate(mean.size() * 3);

  /* Pack Gaussian shape data to a uint4 span. */
  MutableSpan shape_span = shape_data_buf->data<uint4>();
  threading::parallel_for(shape_span.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      float3 value_mean = splat_mean_range.scale_to_unit(mean[i]);
      float3 value_scale = splat_scale_range.scale_to_unit(scale[i]);
      shape_span[i] = detail::pack_gaussian(
          value_mean, value_scale, quaternion[i], radiance_base[i].w);
    }
  });

  /* Pack SH coefficients to 3x uint4 span. Missing coefficients are default-initialized. */
  MutableSpan radiance_span = radiance_data_buf->data<detail::RadiancePack>();
  const float3 radiance_sh_default = radiance_sh_range.scale_to_unit(float3(0.0f));
  threading::parallel_for(radiance_span.index_range(), 4096, [&](IndexRange range) {
    for (const int i : range) {
      Array<float3, radiance_coeffs_max> value_radiance(radiance_coeffs_max, radiance_sh_default);
      value_radiance[0] = radiance_base_range.scale_to_unit(radiance_base[i].xyz());
      for (uint j = 0; j < radiance_sh.size(); ++j) {
        value_radiance[j + 1] = radiance_sh_range.scale_to_unit(radiance_sh[j][i]);
      }
      radiance_span[i] = detail::pack_radiance(value_radiance);
    }
  });
}

void GSplatEvalCache::ensure_edit_dots_indices(PointCloud &pointcloud)
{
  const bke::AttributeAccessor attributes = pointcloud.attributes();
  const VArray selection = *attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);

  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_bools(selection, memory);
  if (mask.is_empty()) {
    return;
  }
  detail::index_mask_to_ibo(mask, *edit_dots_indices);
}

void GSplatEvalCache::ensure_attribute(PointCloud &pointcloud, int attrib_i)
{
  gpu::VertBuf &attr_buf = *attributes_buf[attrib_i];
  const StringRef attr_name = attr_used[attrib_i];

  /* TODO(@kevindietrich): float4 is used for scalar attributes as the conversion done
   * by OpenGL to float4 for a scalar `s` will produce a `float4(s, 0, 0, 1)`. However,
   * following the Blender convention, it should be `float4(s, s, s, 1)`. This could be
   * resolved using a similar texture state swizzle to map the attribute correctly as
   * for volume attributes, so we can control the conversion ourselves. */
  const bke::AttributeAccessor attributes = pointcloud.attributes();
  bke::AttributeReader<ColorGeometry4f> attribute = attributes.lookup_or_default<ColorGeometry4f>(
      attr_name, bke::AttrDomain::Point, {0.0f, 0.0f, 0.0f, 1.0f});

  static const GPUVertFormat format = [&]() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "attr", gpu::VertAttrType::SFLOAT_32_32_32_32);
    return format;
  }();
  static const GPUUsageType usage_flag = GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;
  GPU_vertbuf_init_with_format_ex(attr_buf, format, usage_flag);
  GPU_vertbuf_data_alloc(attr_buf, pointcloud.totpoint);

  array_utils::copy(attribute.varray, attr_buf.data<ColorGeometry4f>());
}

void GSplatEvalCache::ensure_compute_buffers(PointCloud &pointcloud)
{
  static const GPUVertFormat ellipse_format = gpu::GenericVertexFormat<uint2>::format();
  static const GPUVertFormat radiance_format = gpu::GenericVertexFormat<float2>::format();
  static const GPUUsageType buffer_usage_flag = GPU_USAGE_STATIC | GPU_USAGE_DEVICE_ONLY |
                                                GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY;

  ellipses_comp_buf->init(ellipse_format, buffer_usage_flag);
  ellipses_comp_buf->allocate(pointcloud.totpoint);
  radiance_comp_buf->init(radiance_format, buffer_usage_flag);
  radiance_comp_buf->allocate(pointcloud.totpoint);
}

void GSplatEvalCache::ensure_requested(PointCloud &pointcloud)
{
  if (DRW_batch_requested(dots, GPU_PRIM_POINTS)) {
    DRW_vbo_request(dots, &shape_data_buf);
  }
  if (DRW_batch_requested(edit_dots, GPU_PRIM_POINTS)) {
    DRW_ibo_request(edit_dots, &edit_dots_indices);
    DRW_vbo_request(edit_dots, &shape_data_buf);
  }
  if (DRW_ibo_requested(edit_dots_indices)) {
    ensure_edit_dots_indices(pointcloud);
  }
  if (DRW_vbo_requested(shape_data_buf) || DRW_vbo_requested(radiance_data_buf)) {
    ensure_builtin_buffers(pointcloud);
  }
  if (DRW_vbo_requested(ellipses_comp_buf) || DRW_vbo_requested(radiance_comp_buf)) {
    ensure_compute_buffers(pointcloud);
  }
  for (int request_i : attr_used.index_range()) {
    DRW_vbo_request(nullptr, &attributes_buf[request_i]);
    if (DRW_vbo_requested(attributes_buf[request_i])) {
      ensure_attribute(pointcloud, request_i);
    }
  }
}

void GSplatEvalCache::discard_buffers()
{
  GPU_BATCH_DISCARD_SAFE(surface);
  GPU_BATCH_DISCARD_SAFE(dots);
  GPU_BATCH_DISCARD_SAFE(edit_dots);

  GPU_VERTBUF_DISCARD_SAFE(shape_data_buf);
  GPU_VERTBUF_DISCARD_SAFE(radiance_data_buf);
  GPU_VERTBUF_DISCARD_SAFE(ellipses_comp_buf);
  GPU_VERTBUF_DISCARD_SAFE(radiance_comp_buf);
  GPU_VERTBUF_DISCARD_SAFE(attr_viewer);

  GPU_UBO_FREE_SAFE(infos_buf);
  GPU_INDEXBUF_DISCARD_SAFE(edit_dots_indices);

  if (surface_per_mat) {
    for (int i = 0; i < mat_len; ++i) {
      GPU_BATCH_DISCARD_SAFE(surface_per_mat[i]);
    }
  }
  MEM_SAFE_DELETE(surface_per_mat);

  discard_attributes();
}

void GSplatEvalCache::discard_attributes()
{
  for (const int j : IndexRange(GPU_MAX_ATTR)) {
    GPU_VERTBUF_DISCARD_SAFE(attributes_buf[j]);
  }
  attr_used.clear();
}

gpu::VertBuf **GSplatEvalCache::attributes_get(uint request_i)
{
  return &attributes_buf[request_i];
}

gpu::VertBuf *GSplatEvalCache::shape_data_buf_get()
{
  DRW_vbo_request(nullptr, &shape_data_buf);
  return shape_data_buf;
}

gpu::VertBuf *GSplatEvalCache::radiance_data_buf_get()
{
  DRW_vbo_request(nullptr, &radiance_data_buf);
  return radiance_data_buf;
}

gpu::VertBuf *GSplatEvalCache::ellipses_comp_buf_get()
{
  DRW_vbo_request(nullptr, &ellipses_comp_buf);
  return ellipses_comp_buf;
}

gpu::VertBuf *GSplatEvalCache::radiance_comp_buf_get()
{
  DRW_vbo_request(nullptr, &radiance_comp_buf);
  return radiance_comp_buf;
}

gpu::UniformBuf *GSplatEvalCache::infos_buf_get()
{
  if (infos_buf == nullptr) {
    infos_buf = GPU_uniformbuf_create_ex(sizeof(GSplatInfos), nullptr, "GSplatInfos");
  }
  return infos_buf;
}

gpu::Batch *GSplatEvalCache::dots_get()
{
  return DRW_batch_request(&dots);
}

gpu::Batch *GSplatEvalCache::edit_dots_get()
{
  return DRW_batch_request(&edit_dots);
}

gpu::Batch *GSplatEvalCache::surface_get(const PointCloud &pointcloud)
{
  if (surface == nullptr) {
    surface = GPU_batch_create_procedural(GPU_PRIM_TRI_STRIP,
                                          DRW_GSPLAT_STRIP_TILE_SIZE * pointcloud.totpoint);

    DRW_vbo_request(nullptr, &shape_data_buf);
    DRW_vbo_request(nullptr, &radiance_data_buf);

    if (!infos_buf) {
      infos_buf = GPU_uniformbuf_create_ex(sizeof(GSplatInfos), nullptr, "GSplatInfos");
    }
  }
  return surface;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GSplatBatchCache management
 * \{ */

/**
 * Wrapper struct which is attached to the pointcloud.
 */
struct GSplatBatchCache {
  /* Underlying cache of batches, buffers, etc. */
  GSplatEvalCache eval_cache;
  /* Determines if cache is invalid */
  bool is_dirty;

 public:
  static GSplatBatchCache *get_ptr(PointCloud &pointcloud)
  {
    return pointcloud.gsplat_batch_cache;
  }

  static bool is_valid(PointCloud &pointcloud)
  {
    GSplatBatchCache *ptr = GSplatBatchCache::get_ptr(pointcloud);
    if (!ptr) {
      return false;
    }
    if (ptr->eval_cache.mat_len != BKE_id_material_used_with_fallback_eval(pointcloud.id)) {
      return false;
    }
    return !ptr->is_dirty;
  }

  static void init(PointCloud &pointcloud)
  {
    GSplatBatchCache *ptr = GSplatBatchCache::get_ptr(pointcloud);
    if (!ptr) {
      ptr = MEM_new<GSplatBatchCache>(__func__);
      pointcloud.gsplat_batch_cache = ptr;
    }
    else {
      ptr->eval_cache = {};
      ptr->eval_cache.edit_dots = nullptr;
      ptr->eval_cache.edit_dots_indices = nullptr;
    }

    ptr->eval_cache.mat_len = BKE_id_material_used_with_fallback_eval(pointcloud.id);
    ptr->eval_cache.surface_per_mat = MEM_new_array_zeroed<gpu::Batch *>(ptr->eval_cache.mat_len,
                                                                         __func__);
    ptr->is_dirty = false;
  }

  static void clear(PointCloud &pointcloud)
  {
    GSplatBatchCache *ptr = GSplatBatchCache::get_ptr(pointcloud);
    if (!ptr) {
      return;
    }
    ptr->eval_cache.discard_buffers();
  }
};

GSplatEvalCache &GSplatEvalCache::get(PointCloud &pointcloud)
{
  return GSplatBatchCache::get_ptr(pointcloud)->eval_cache;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Batch cache API
 * \{ */

gpu::VertBuf **DRW_gsplat_evaluated_attribute(PointCloud *pointcloud, const StringRef name)
{
  if (pointcloud->type != PointCloudType::GSplat) {
    return nullptr;
  }

  const bke::AttributeAccessor attributes = pointcloud->attributes();
  GSplatEvalCache &cache = GSplatEvalCache::get(*pointcloud);

  if (!attributes.contains(name)) {
    return nullptr;
  }
  {
    VectorSet<std::string> requests;
    drw_attributes_add_request(&requests, name);
    drw_attributes_merge(&cache.attr_used, &requests);
  }

  for (const int i : IndexRange(cache.attr_used.index_range())) {
    if (cache.attr_used[i] == name) {
      return cache.attributes_get(i);
    }
  }
  return nullptr;
}

gpu::Batch *DRW_gsplat_batch_cache_get_dots(Object *ob)
{
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob);
  return pointcloud.type == PointCloudType::GSplat ? GSplatEvalCache::get(pointcloud).dots_get() :
                                                     nullptr;
}

gpu::Batch *DRW_gsplat_batch_cache_get_edit_dots(Object *ob)
{
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob);
  return pointcloud.type == PointCloudType::GSplat ?
             GSplatEvalCache::get(pointcloud).edit_dots_get() :
             nullptr;
}

void DRW_gsplat_batch_cache_create_requested(Object *ob)
{
  PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(*ob);
  if (pointcloud.type != PointCloudType::GSplat) {
    return;
  }
  GSplatEvalCache::get(pointcloud).ensure_requested(pointcloud);
}

void DRW_gsplat_batch_cache_dirty_tag(PointCloud *pointcloud, int mode)
{
  GSplatBatchCache *ptr = GSplatBatchCache::get_ptr(*pointcloud);
  if (ptr == nullptr) {
    return;
  }

  switch (mode) {
    case BKE_POINTCLOUD_BATCH_DIRTY_ALL:
      ptr->is_dirty = true;
      break;
    default:
      BLI_assert(0);
  }
}

void DRW_gsplat_batch_cache_validate(PointCloud *pointcloud)
{
  if (pointcloud->type != PointCloudType::GSplat) {
    return;
  }

  if (!GSplatBatchCache::is_valid(*pointcloud)) {
    GSplatBatchCache::clear(*pointcloud);
    GSplatBatchCache::init(*pointcloud);
  }
}

void DRW_gsplat_batch_cache_free(PointCloud *pointcloud)
{
  GSplatBatchCache::clear(*pointcloud);
  MEM_delete(pointcloud->gsplat_batch_cache);
  pointcloud->gsplat_batch_cache = nullptr;
}

void DRW_gsplat_batch_cache_free_old(PointCloud *pointcloud, int ctime)
{
  if (!GSplatBatchCache::get_ptr(*pointcloud)) {
    return;
  }
  GSplatEvalCache &cache = GSplatEvalCache::get(*pointcloud);

  bool do_discard = false;

  if (drw_attributes_overlap(&cache.attr_used_over_time, &cache.attr_used)) {
    cache.last_attr_matching_time = ctime;
  }

  if (ctime - cache.last_attr_matching_time > U.vbotimeout) {
    do_discard = true;
  }

  cache.attr_used_over_time.clear();
  if (do_discard) {
    cache.discard_attributes();
  }
}

/** \} */

}  // namespace blender::draw
