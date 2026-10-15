/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * bke::pbvh::Tree drawing.
 * Embeds GPU meshes inside of bke::pbvh::Tree nodes, used by mesh sculpt mode.
 */

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_math_geom_c.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.hh"
#include "BLI_vector.hh"

#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_eval.hh"

#include "DEG_depsgraph_query.hh"

#include "GPU_batch.hh"
#include "GPU_vertex_buffer.hh"

#include "DRW_engine.hh"
#include "DRW_pbvh.hh"
#include "DRW_render.hh"

#include "PRF_profile.hh"

#include "draw_pbvh_allocator.hh"
#include "draw_sculpt.hh"

#include "attribute_convert.hh"
#include "bmesh.hh"

namespace blender {

template<> struct DefaultHash<draw::pbvh::AttributeRequest> {
  uint64_t operator()(const draw::pbvh::AttributeRequest &value) const
  {
    using namespace draw::pbvh;
    if (const CustomRequest *request_type = std::get_if<CustomRequest>(&value)) {
      return get_default_hash(*request_type);
    }
    const GenericRequest &attr = std::get<GenericRequest>(value);
    return get_default_hash(attr);
  }
};

namespace draw::pbvh {

uint64_t ViewportRequest::hash() const
{
  return get_default_hash(attributes, use_coarse_grids);
}

static bool use_coarse_buffers(const bke::pbvh::Tree &pbvh, const ViewportRequest &request)
{
  return request.use_coarse_grids && pbvh.type() == bke::pbvh::Type::Grids;
}

/**
 * Per-node location of its data inside a single shared GPU buffer.
 *
 * Pairs one #sizes and #offsets entry per node with the #acquired bit that gates whether that
 * range has been assigned a valid offset yet, and owns the #RangeAllocator that hands out the
 * space inside the shared buffer.
 */
struct Buffer {
  /** \brief Assigns the sub-ranges of the shared buffer to nodes. */
  RangeAllocator space;
  /** \brief The sub-range of the shared buffer that each node occupies. */
  Vector<uint32_t> offsets;
  Vector<int32_t> sizes;
  /** \brief Whether each node currently has space acquired in the shared buffer. */
  BitVector<> acquired;
  /**
   * \brief Nodes whose size might have changed and has to be recalculated. For correctness, just
   * recounting the number of elements in each node every redraw would be correct. This bit per
   * node allows avoiding that recounting for performance reasons.
   */
  BitVector<> dirty;
  /**
   * \brief The size of #space when the GPU buffer was last (re)allocated.
   *
   * When the assigned space grows past this, the underlying buffer is reallocated and batches
   * need rebuilding.
   */
  uint64_t space_limit = 0;

  /**
   * \brief Set the size of the range of node #node_index to #new_len.
   *
   * \return whether the range was changed (and possibly released).
   */
  bool update_range(const int64_t node_index, const int64_t new_len)
  {
    if (this->sizes[node_index] == new_len) {
      return false;
    }
    const bool was_acquired = this->acquired[node_index];
    if (was_acquired) {
      this->space.release(this->offsets[node_index], this->sizes[node_index]);
      this->acquired[node_index].reset();
    }
    this->sizes[node_index] = new_len;
    return was_acquired;
  }

  /**
   * \brief Acquire space for node #node_index using the previously set length.
   */
  void acquire(const int64_t node_index)
  {
    BLI_assert(!this->acquired[node_index]);
    this->offsets[node_index] = this->space.acquire(this->sizes[node_index]);
    this->acquired[node_index].set();
  }

  /** \brief Grow the per-node arrays to cover #nodes_num nodes. */
  void ensure_nodes_num(const int64_t nodes_num)
  {
    if (this->offsets.size() < nodes_num) {
      this->offsets.resize(nodes_num);
      this->sizes.resize(nodes_num);
      this->acquired.resize(nodes_num);
    }
    if (this->dirty.size() < nodes_num) {
      this->dirty.resize(nodes_num, true);
    }
  }

  /**
   * \brief Recalculate the ranges of the nodes marked dirty, and assign space to nodes that
   * don't have any yet.
   *
   * \param calc_sizes: Fills in the number of elements each node of the given mask needs.
   * \param r_changed: Receives the nodes whose range moved or resized. GPU data stored at their
   * old location is now stale.
   * \return True when the assigned space grew past the size of the current GPU buffer.
   */
  bool ensure_ranges(const int64_t nodes_num,
                     const IndexMask &node_mask,
                     const FunctionRef<void(const IndexMask &, MutableSpan<int>)> calc_sizes,
                     BitVector<> &r_changed)
  {
    this->ensure_nodes_num(nodes_num);
    r_changed.resize(std::max<int64_t>(r_changed.size(), nodes_num), false);

    IndexMaskMemory memory;
    const IndexMask dirty_mask = IndexMask::from_bits(node_mask, this->dirty, memory);
    if (!dirty_mask.is_empty()) {
      Array<int> new_sizes(dirty_mask.size());
      calc_sizes(dirty_mask, new_sizes);
      dirty_mask.foreach_index([&](const int node_index, const int pos) {
        if (this->update_range(node_index, new_sizes[pos])) {
          r_changed[node_index].set();
        }
        this->dirty[node_index].reset();
      });
      /* Assign space to nodes that lost it above, and to nodes that never had any. */
      dirty_mask.foreach_index([&](const int node_index) {
        if (!this->acquired[node_index] && this->sizes[node_index] > 0) {
          this->acquire(node_index);
        }
      });
    }

    const uint64_t size = uint64_t(this->space.size());
    if (size <= this->space_limit) {
      return false;
    }
    this->space_limit = size;
    return true;
  }

  /** \brief Mark the sizes of #node_mask as needing recalculation. */
  void tag_dirty(const IndexMask &node_mask)
  {
    /* Nodes past the end have never been visited, so they are dirty by definition. */
    this->dirty.resize(std::max(this->dirty.size(), node_mask.min_array_size()), true);
    node_mask.set_bits(this->dirty);
  }

  IndexRange index_range(int node_index) const
  {
    return IndexRange(this->offsets[node_index], this->sizes[node_index]);
  }

  /**
   * \brief The sub-range of a scratch buffer that node #node_index occupies.
   */
  IndexRange scratch_range(int node_index, int64_t scratch_offset) const
  {
    return this->acquired[node_index] ? IndexRange(scratch_offset, this->sizes[node_index]) :
                                        IndexRange(scratch_offset, 0);
  }
};

/**
 * \brief Scratch buffer offsets for modified nodes.
 *
 * \note `#offsets[n]` refers to the n-th staged node (scratch index), not to the PBVH node index.
 */
struct ScratchOffsets {
  Array<int64_t> offsets;
  int64_t total = 0;
};

/**
 * Because many sculpt mode operations skip tagging dependency graph for reevaluation for
 * performance reasons, the relevant data must be retrieved directly from the original mesh rather
 * than the evaluated copy.
 */
struct OrigMeshData {
  StringRef active_color;
  StringRef default_color;
  VectorSet<StringRef> uv_map_names;
  StringRef active_uv_map;
  StringRef default_uv_map;
  int face_set_default;
  int face_set_seed;
  bke::AttributeAccessor attributes;
  OrigMeshData(const Mesh &mesh)
      : active_color(mesh.active_color_attribute),
        default_color(mesh.default_color_attribute),
        active_uv_map(mesh.active_uv_map_name()),
        default_uv_map(mesh.default_uv_map_name()),
        face_set_default(mesh.face_sets_color_default),
        face_set_seed(mesh.face_sets_color_seed),
        attributes(mesh.attributes())
  {
    uv_map_names.add_multiple(mesh.uv_map_names().as_span().cast<StringRef>());
  }
};

/**
 * Stores the data necessary to draw the PBVH geometry. A separate `*Impl` class is used to hide
 * implementation details from the public header.
 */
class DrawCacheImpl : public DrawCache {
 public:
  struct AttributeData {
    /**
     * \brief A vertex buffer containing data for all BVH nodes.
     */
    gpu::VertBufPtr vbo;

    /**
     * A separate "dirty" bit per node. We track the dirty value separately from deleting the VBO
     * for a node in order to avoid recreating batches with new VBOs. It's also a necessary
     * addition to the flags stored in the PBVH which are cleared after it's used for drawing
     * (those aren't sufficient when multiple viewports are drawing with the same PBVH but request
     * different sets of attributes).
     */
    BitVector<> dirty_nodes;
    /**
     * Mark attribute values dirty for specific nodes. The next time the attribute is requested,
     * the values will be extracted again.
     */
    void tag_dirty(const IndexMask &node_mask);
    void ensure_nodes_capacity(int64_t nodes_num);
    void ensure_buffer_capacity(uint64_t needed_vertex_len);
  };

 private:
  /**
   * \brief Used to determine whether to use indexed VBO layouts for multires grids.
   *
   * Bit for each node. `false` means that the node contains smooth shaded vertex data (elements
   * are shared between faces). `true` means that the node is "flat shaded" (elements are stored
   * per face corner).
   */
  BitVector<> use_flat_layout_;
  /**
   * \brief The material index for each node.
   *
   * Is used during drawing to place the node in the correct material bucket.
   */
  Array<int> material_indices_;

  /**
   * \brief Shared index buffer with space bookkeeping and the per-node subranges.
   */
  struct IBOFamily {
    /** \brief Per-node location of its data inside the shared index buffer. */
    Buffer buffer;
    /** \brief Index buffer containing all indices. */
    gpu::IndexBufPtr ibo;
    /** \brief Per node a sub-range of ibo. */
    Vector<gpu::IndexBufPtr> ibos;

    /**
     * \brief Recalculate this family's node ranges and free the sub-buffers that moved.
     *
     * \return True when the shared index buffer has to be reallocated.
     */
    bool ensure_ranges(int nodes_num,
                       const IndexMask &node_mask,
                       FunctionRef<void(const IndexMask &, MutableSpan<int>)> calc_sizes);

    /**
     * \brief Ensure the shared index buffer is large enough to hold the assigned space.
     *
     * When this requires growing (or creating) the buffer, existing data is copied over and
     * existing sub-ranges are recreated.
     */
    void ensure_capacity();

    /**
     * \brief Upload the staged indices of each node and recreate its sub-range of #ibo.
     */
    void commit_subranges(const IndexMask &node_mask,
                          const ScratchOffsets &scratch_offsets,
                          Span<uint32_t> scratch_buffer);
  };
  IBOFamily ibo_lines_;
  IBOFamily ibo_lines_coarse_;
  IBOFamily ibo_tris_;
  IBOFamily ibo_tris_coarse_;

  /** \brief Per-node location of its data inside the shared vertex buffer. */
  Buffer vbo_;

  /**
   * GPU data and per-node dirty status for all requested attributes.
   * \note Currently we do not remove "stale" attributes that haven't been requested in a while.
   */
  Map<AttributeRequest, AttributeData> attribute_vbos_;

  /**
   * \brief Cached batches for a given setup.
   *
   * Batches hold raw pointers to the shared attribute VBOs and index subranges. Those pointers
   * become stale when the underlying buffers are reallocated.
   */
  struct BatchCache {
    Vector<Batch *> batches;
    /**
     * \brief Nodes whose batch references a buffer that has since been reallocated.
     *
     * Tracked per node rather than for the whole cache, because a viewport only rebuilds the
     * batches of the nodes it draws. If all nodes are rebuilt, this can become a single boolean.
     */
    BitVector<> dirty;
  };

  /** Batches for drawing wireframe geometry. */
  BatchCache lines_batches_;
  /** Batches for drawing coarse "fast navigate" wireframe geometry. */
  BatchCache lines_batches_coarse_;
  /**
   * Batches for drawing triangles, stored separately for each combination of attributes and
   * coarse-ness. Different viewports might request different sets of attributes, and we don't want
   * to recreate the batches on every redraw.
   */
  Map<ViewportRequest, BatchCache> tris_batches_;

  /**
   * Which nodes have GPU data that no longer matches the geometry, either because their visible
   * faces changed or because their range inside a shared buffer moved.
   *
   * \note Theoretically the dirty tag is redundant with checking for a different number of visible
   * triangles in the PBVH node on every redraw. We could do that too, but it's simpler overall to
   * just tag the node whenever there is such a topology change, and for now there is no real
   * downside.
   */
  BitVector<> dirty_topology_;

 public:
  ~DrawCacheImpl() override;

  void tag_positions_changed(const IndexMask &node_mask) override;
  void tag_visibility_changed(const IndexMask &node_mask) override;
  void tag_topology_changed(const IndexMask &node_mask) override;
  void tag_face_sets_changed(const IndexMask &node_mask) override;
  void tag_masks_changed(const IndexMask &node_mask) override;
  void tag_attribute_changed(const IndexMask &node_mask, StringRef attribute_name) override;

  Span<Batch *> ensure_tris_batches(const Object &object,
                                    const ViewportRequest &request,
                                    const IndexMask &nodes_to_update) override;

  Span<Batch *> ensure_lines_batches(const Object &object,
                                     const ViewportRequest &request,
                                     const IndexMask &nodes_to_update) override;

  Span<int> ensure_material_indices(const Object &object) override;

 private:
  /**
   * \brief Assign the ranges inside the shared vertex buffer, which every index buffer family
   * refers to.
   *
   * \return True when the shared vertex buffer has to be reallocated.
   */
  bool ensure_vert_ranges(const Object &object,
                          const OrigMeshData &orig_mesh_data,
                          const IndexMask &node_mask);

  /** Mark every cached batch as needing to be reconfigured, after a shared buffer was moved. */
  void tag_batches_dirty();

  /**
   * Free the GPU data of every node tagged in #dirty_topology_. The next time the data is
   * requested it will be rebuilt.
   */
  void free_nodes_with_changed_topology(const bke::pbvh::Tree &pbvh);

  BitSpan ensure_use_flat_layout(const Object &object, const OrigMeshData &orig_mesh_data);

  AttributeData &ensure_attribute_data(const Object &object,
                                       const OrigMeshData &orig_mesh_data,
                                       const AttributeRequest &attr,
                                       const IndexMask &node_mask);

  Span<gpu::IndexBufPtr> ensure_tri_indices(const Object &object,
                                            const OrigMeshData &orig_mesh_data,
                                            const IndexMask &node_mask,
                                            bool coarse);

  Span<gpu::IndexBufPtr> ensure_lines_indices(const Object &object,
                                              const OrigMeshData &orig_mesh_data,
                                              const IndexMask &node_mask,
                                              bool coarse);
};

void DrawCacheImpl::AttributeData::tag_dirty(const IndexMask &node_mask)
{
  this->dirty_nodes.resize(std::max(this->dirty_nodes.size(), node_mask.min_array_size()), false);
  node_mask.set_bits(this->dirty_nodes);
}

void DrawCacheImpl::tag_positions_changed(const IndexMask &node_mask)
{
  if (DrawCacheImpl::AttributeData *data = attribute_vbos_.lookup_ptr(CustomRequest::Position)) {
    data->tag_dirty(node_mask);
  }
  if (DrawCacheImpl::AttributeData *data = attribute_vbos_.lookup_ptr(CustomRequest::Normal)) {
    data->tag_dirty(node_mask);
  }
}

void DrawCacheImpl::tag_visibility_changed(const IndexMask &node_mask)
{
  /* The index buffers only reference visible faces, so their contents are stale even when the
   * number of visible faces happens to stay the same. */
  dirty_topology_.resize(std::max(dirty_topology_.size(), node_mask.min_array_size()), false);
  node_mask.set_bits(dirty_topology_);

  vbo_.tag_dirty(node_mask);
  ibo_lines_.buffer.tag_dirty(node_mask);
  ibo_lines_coarse_.buffer.tag_dirty(node_mask);
  ibo_tris_.buffer.tag_dirty(node_mask);
  ibo_tris_coarse_.buffer.tag_dirty(node_mask);
}

void DrawCacheImpl::tag_topology_changed(const IndexMask &node_mask)
{
  /* Currently the only times where topology changes are for BMesh dynamic topology, where tagging
   * a visibility update deletes all the GPU data anyway. */
  this->tag_visibility_changed(node_mask);
}

void DrawCacheImpl::tag_face_sets_changed(const IndexMask &node_mask)
{
  if (DrawCacheImpl::AttributeData *data = attribute_vbos_.lookup_ptr(CustomRequest::FaceSet)) {
    data->tag_dirty(node_mask);
  }
}

void DrawCacheImpl::tag_masks_changed(const IndexMask &node_mask)
{
  if (DrawCacheImpl::AttributeData *data = attribute_vbos_.lookup_ptr(CustomRequest::Mask)) {
    data->tag_dirty(node_mask);
  }
}

void DrawCacheImpl::tag_attribute_changed(const IndexMask &node_mask, StringRef attribute_name)
{
  for (const auto &[data_request, data] : attribute_vbos_.items()) {
    if (const GenericRequest *request = std::get_if<GenericRequest>(&data_request)) {
      if (*request == attribute_name) {
        data.tag_dirty(node_mask);
      }
    }
  }
}

DrawCache &ensure_draw_data(std::unique_ptr<bke::pbvh::DrawCache> &ptr)
{
  if (!ptr) {
    ptr = std::make_unique<DrawCacheImpl>();
  }
  return dynamic_cast<DrawCache &>(*ptr);
}

BLI_NOINLINE static void free_ibos(const MutableSpan<gpu::IndexBufPtr> ibos,
                                   const IndexMask &node_mask)
{
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_intersection(node_mask, ibos.index_range(), memory);
  mask.foreach_index([&](const int node_index) { ibos[node_index].reset(); });
}

/**
 * Upload a node's index data to the shared IBO. Zero-length ranges (a leaf with no visible
 * geometry) are skipped.
 */
static void update_ibos_subrange(gpu::IndexBuf &ibo_shared,
                                 const IndexRange dst_range,
                                 const uint32_t *host_data)
{
  if (dst_range.is_empty()) {
    return;
  }
  GPU_indexbuf_update_sub(&ibo_shared,
                          uint(dst_range.start()) * sizeof(uint32_t),
                          uint(dst_range.size()) * sizeof(uint32_t),
                          host_data);
}

static ScratchOffsets build_scratch_offsets(const Buffer &buffer, const IndexMask &node_mask)
{
  ScratchOffsets result;
  if (node_mask.is_empty()) {
    return result;
  }
  result.offsets = Array<int64_t>(node_mask.size());
  int64_t offset = 0;
  node_mask.foreach_index([&](const int node_index, const int scratch_index) {
    result.offsets[scratch_index] = offset;
    if (buffer.acquired[node_index]) {
      offset += buffer.sizes[node_index];
    }
  });
  result.total = offset;
  return result;
}

/**
 * \brief Staging offsets for an initial vertex buffer build.
 *
 * Nodes are staged at their absolute VBO offsets so that the full-sized scratch buffer can be
 * promoted to the final vertex buffer.
 */
static ScratchOffsets build_vbo_initial_offsets(const Buffer &buffer, const IndexMask &node_mask)
{
  ScratchOffsets result;
  if (node_mask.is_empty()) {
    return result;
  }
  result.offsets = Array<int64_t>(node_mask.size());
  node_mask.foreach_index([&](const int node_index, const int scratch_index) {
    result.offsets[scratch_index] = buffer.offsets[node_index];
  });
  result.total = buffer.space.size();
  return result;
}

static gpu::VertBufPtr create_vbo_scratch_buffer(const GPUVertFormat &format, const int64_t total)
{
  gpu::VertBufPtr scratch = gpu::VertBufPtr(
      GPU_vertbuf_create_with_format_ex(format, GPU_USAGE_STATIC));
  scratch->allocate(uint32_t(total));
  return scratch;
}

/**
 * Copy a node's slice from an uploaded scratch buffer into the final vertex buffer.
 *
 * \param dst The final vertex buffer (GPU_USAGE_DEVICE_ONLY) to copy into.
 * \param dst_range The destination vertex range of this node inside #dst.
 * \param src_vertex Vertex offset of this node inside the scratch buffer.
 * \param scratch The uploaded scratch buffer to read from.
 */
static void update_vbo_subrange(gpu::VertBuf &dst,
                                const IndexRange dst_range,
                                const int64_t src_vertex,
                                gpu::VertBuf &scratch)
{
  if (dst_range.is_empty()) {
    return;
  }
  dst.copy_sub(scratch, uint(src_vertex), uint(dst_range.start()), uint(dst_range.size()));
}

/**
 * \brief Upload staged vertex data and commit it to the final vertex buffer.
 *
 * \note Will promote the staging buffer to the final vertex buffer for initial
 * uploads.
 */
static void finalize_vbo_update(DrawCacheImpl::AttributeData &attr_vbo,
                                const Buffer &vbo,
                                const int vert_len,
                                const IndexMask &node_mask,
                                const ScratchOffsets &scratch_offsets,
                                gpu::VertBufPtr &scratch_vbo)
{
  scratch_vbo->upload();
  if (attr_vbo.vbo == nullptr) {
    attr_vbo.vbo = std::move(scratch_vbo);
    return;
  }
  attr_vbo.ensure_buffer_capacity(vert_len);
  node_mask.foreach_index([&](const int node_index, const int scratch_index) {
    update_vbo_subrange(*attr_vbo.vbo,
                        vbo.index_range(node_index),
                        scratch_offsets.offsets[scratch_index],
                        *scratch_vbo);
  });
}

BLI_NOINLINE static void free_batches(const MutableSpan<Batch *> batches)
{
  for (Batch *&batch : batches) {
    MEM_SAFE_DELETE(batch);
  }
}

static const GPUVertFormat &position_format()
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  return format;
}

static const GPUVertFormat &normal_format()
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "nor", gpu::VertAttrType::SNORM_16_16_16_16);
  return format;
}

static const GPUVertFormat &mask_format()
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute("msk",
                                                                    gpu::VertAttrType::SFLOAT_32);
  return format;
}

static const GPUVertFormat &face_set_format()
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "fset", gpu::VertAttrType::UNORM_8_8_8_8);
  return format;
}

static GPUVertFormat attribute_format(const OrigMeshData &orig_mesh_data,
                                      const StringRef name,
                                      const bke::AttrType data_type)
{
  GPUVertFormat format = init_format_for_attribute(data_type, "data");

  bool is_render, is_active;
  const char *prefix = "a";

  if (CD_TYPE_AS_MASK(*bke::attr_type_to_custom_data_type(data_type))) {
    prefix = "c";
    is_active = orig_mesh_data.active_color == name;
    is_render = orig_mesh_data.default_color == name;
  }
  if (data_type == bke::AttrType::Float2) {
    prefix = "u";
    is_active = orig_mesh_data.active_uv_map == name;
    is_render = orig_mesh_data.default_uv_map == name;
  }

  DRW_cdlayer_attr_aliases_add(&format, prefix, data_type, name, is_render, is_active);
  return format;
}

inline short4 normal_float_to_short(const float3 &value)
{
  short3 result;
  normal_float_to_short_v3(result, value);
  return short4(result.x, result.y, result.z, 0);
}

template<typename T>
void extract_data_vert_mesh(const OffsetIndices<int> faces,
                            const Span<int> corner_verts,
                            const Span<T> attribute,
                            const Span<int> face_indices,
                            gpu::VertBuf &vbo,
                            const IndexRange scratch_range)
{
  PRF_scope(ProfileCategory::Draw);
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = vbo.data<VBOType>().data() + scratch_range.start();
  VBOType *data_start = data;
  for (const int face : face_indices) {
    for (const int vert : corner_verts.slice(faces[face])) {
      *data = Converter::convert(attribute[vert]);
      data++;
    }
  }
  UNUSED_VARS_NDEBUG(data_start);
  BLI_assert(data == data_start + scratch_range.size());
}

template<typename T>
void extract_data_face_mesh(const OffsetIndices<int> faces,
                            const Span<T> attribute,
                            const Span<int> face_indices,
                            gpu::VertBuf &vbo,
                            const IndexRange scratch_range)
{
  PRF_scope(ProfileCategory::Draw);
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;

  VBOType *data = vbo.data<VBOType>().data() + scratch_range.start();
  VBOType *data_start = data;
  for (const int face : face_indices) {
    const int face_size = faces[face].size();
    std::fill_n(data, face_size, Converter::convert(attribute[face]));
    data += face_size;
  }
  UNUSED_VARS_NDEBUG(data_start);
  BLI_assert(data == data_start + scratch_range.size());
}

template<typename T>
void extract_data_corner_mesh(const OffsetIndices<int> faces,
                              const Span<T> attribute,
                              const Span<int> face_indices,
                              gpu::VertBuf &vbo,
                              const IndexRange scratch_range)
{
  PRF_scope(ProfileCategory::Draw);
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;

  VBOType *data = vbo.data<VBOType>().data() + scratch_range.start();
  VBOType *data_start = data;
  for (const int face : face_indices) {
    for (const int corner : faces[face]) {
      *data = Converter::convert(attribute[corner]);
      data++;
    }
  }
  UNUSED_VARS_NDEBUG(data_start);
  BLI_assert(data == data_start + scratch_range.size());
}

template<typename T> const T &bmesh_cd_vert_get(const BMVert &vert, const int offset)
{
  return *static_cast<const T *>(POINTER_OFFSET(vert.head.data, offset));
}

template<typename T> const T &bmesh_cd_loop_get(const BMLoop &loop, const int offset)
{
  return *static_cast<const T *>(POINTER_OFFSET(loop.head.data, offset));
}

template<typename T> const T &bmesh_cd_face_get(const BMFace &face, const int offset)
{
  return *static_cast<const T *>(POINTER_OFFSET(face.head.data, offset));
}

template<typename T>
void extract_data_vert_bmesh(const Set<BMFace *, 0> &faces,
                             const int cd_offset,
                             gpu::VertBuf &vbo,
                             const IndexRange scratch_range)
{
  PRF_scope(ProfileCategory::Draw);
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = vbo.data<VBOType>().data() + scratch_range.start();
  VBOType *data_start = data;

  for (const BMFace *face : faces) {
    if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
      continue;
    }
    const BMLoop *l = face->l_first;
    *data = Converter::convert(bmesh_cd_vert_get<T>(*l->prev->v, cd_offset));
    data++;
    *data = Converter::convert(bmesh_cd_vert_get<T>(*l->v, cd_offset));
    data++;
    *data = Converter::convert(bmesh_cd_vert_get<T>(*l->next->v, cd_offset));
    data++;
  }
  UNUSED_VARS_NDEBUG(data_start);
  BLI_assert(data == data_start + scratch_range.size());
}

template<typename T>
void extract_data_face_bmesh(const Set<BMFace *, 0> &faces,
                             const int cd_offset,
                             gpu::VertBuf &vbo,
                             const IndexRange scratch_range)
{
  PRF_scope(ProfileCategory::Draw);
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = vbo.data<VBOType>().data() + scratch_range.start();
  VBOType *data_start = data;

  for (const BMFace *face : faces) {
    if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
      continue;
    }
    std::fill_n(data, 3, Converter::convert(bmesh_cd_face_get<T>(*face, cd_offset)));
    data += 3;
  }
  UNUSED_VARS_NDEBUG(data_start);
  BLI_assert(data == data_start + scratch_range.size());
}

template<typename T>
void extract_data_corner_bmesh(const Set<BMFace *, 0> &faces,
                               const int cd_offset,
                               gpu::VertBuf &vbo,
                               const IndexRange scratch_range)
{
  PRF_scope(ProfileCategory::Draw);
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = vbo.data<VBOType>().data() + scratch_range.start();
  VBOType *data_start = data;

  for (const BMFace *face : faces) {
    if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
      continue;
    }
    const BMLoop *l = face->l_first;
    *data = Converter::convert(bmesh_cd_loop_get<T>(*l->prev, cd_offset));
    data++;
    *data = Converter::convert(bmesh_cd_loop_get<T>(*l, cd_offset));
    data++;
    *data = Converter::convert(bmesh_cd_loop_get<T>(*l->next, cd_offset));
    data++;
  }
  UNUSED_VARS_NDEBUG(data_start);
  BLI_assert(data == data_start + scratch_range.size());
}

static int count_visible_tris_bmesh(const Set<BMFace *, 0> &faces)
{
  return std::count_if(faces.begin(), faces.end(), [&](const BMFace *face) {
    return !BM_elem_flag_test_bool(face, BM_ELEM_HIDDEN);
  });
}

DrawCacheImpl::~DrawCacheImpl()
{
  free_batches(lines_batches_.batches);
  free_batches(lines_batches_coarse_.batches);
  for (BatchCache &batch_cache : tris_batches_.values()) {
    free_batches(batch_cache.batches);
  }
}

void DrawCacheImpl::free_nodes_with_changed_topology(const bke::pbvh::Tree &pbvh)
{
  PRF_scope(ProfileCategory::Draw);
  /* NOTE: Theoretically we shouldn't need to free batches with a changed triangle count, but
   * currently it's the simplest way to reallocate all the GPU data while keeping everything in a
   * consistent state. */
  IndexMaskMemory memory;
  const IndexMask nodes_to_free = IndexMask::from_bits(dirty_topology_, memory);
  if (nodes_to_free.is_empty()) {
    return;
  }

  dirty_topology_.clear_and_shrink();

  free_ibos(ibo_lines_.ibos, nodes_to_free);
  free_ibos(ibo_lines_coarse_.ibos, nodes_to_free);
  free_ibos(ibo_tris_.ibos, nodes_to_free);
  free_ibos(ibo_tris_coarse_.ibos, nodes_to_free);

  if (pbvh.type() == bke::pbvh::Type::BMesh) {
    /* For BMesh, VBOs are only filled with data for visible triangles, and topology can also
     * completely change due to dynamic topology, so VBOs must be rebuilt from scratch. For other
     * types, actual topology doesn't change, and visibility changes are accounted for by the index
     * buffers. */
    for (AttributeData &data : attribute_vbos_.values()) {
      data.tag_dirty(nodes_to_free);
    }
  }
}

/** \brief The number of face corners of a node's visible faces. */
static int count_visible_corners_mesh(const OffsetIndices<int> faces,
                                      const Span<bool> hide_poly,
                                      const bke::pbvh::MeshNode &node)
{
  if (hide_poly.is_empty()) {
    return node.corners_num();
  }
  int count = 0;
  for (const int face : node.faces()) {
    if (!hide_poly[face]) {
      count += faces[face].size();
    }
  }
  return count;
}

/** \brief The number of triangles of a node's visible faces. */
static int count_visible_tris_mesh(const OffsetIndices<int> faces,
                                   const Span<bool> hide_poly,
                                   const bke::pbvh::MeshNode &node)
{
  if (hide_poly.is_empty()) {
    return poly_to_tri_count(node.faces().size(), node.corners_num());
  }
  int count = 0;
  for (const int face : node.faces()) {
    if (!hide_poly[face]) {
      count += bke::mesh::face_triangles_num(faces[face].size());
    }
  }
  return count;
}

/**
 * \brief The size of the grid "quad" area that a node displays, in quads per axis.
 *
 * The coarse "fast navigate" level only draws the corner quad of every grid.
 */
static int grid_quads_per_axis(const CCGKey &key, const bool coarse)
{
  const int skip = coarse && key.level > 0 ? 1 << (key.level - 1) : 1;
  return (key.grid_size - 1) / skip;
}

/**
 * \brief Calculate the number of vertices each node stores in the shared vertex buffer.
 */
static void calc_vert_counts(const Object &object,
                             const BitSpan use_flat_layout,
                             const IndexMask &node_mask,
                             MutableSpan<int> counts)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      node_mask.foreach_index(
          [&](const int i, const int pos) { counts[pos] = nodes[i].corners_num(); });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
      /* Flat shaded nodes store separate vertices for each of the four quad corners. */
      const int verts_per_grid_flat = square_i(subdiv_ccg.grid_size - 1) * 4;
      const int verts_per_grid = square_i(subdiv_ccg.grid_size);
      node_mask.foreach_index([&](const int i, const int pos) {
        counts[pos] = nodes[i].grids().size() *
                      (use_flat_layout[i] ? verts_per_grid_flat : verts_per_grid);
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      /* BMesh vertex buffers only contain the visible triangles, stored per face corner. */
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(
          [&](const int i, const int pos) {
            counts[pos] = count_visible_tris_bmesh(nodes[i].bm_faces_) * 3;
          },
          exec_mode::grain_size(8));
      break;
    }
  }
}

/**
 * \brief Calculate the number of indices each node needs in the shared wireframe index buffer.
 *
 * Hidden faces aren't indexed, so the counts change whenever visibility does.
 */
static void calc_lines_index_counts(const Object &object,
                                    const OrigMeshData &orig_mesh_data,
                                    const bool coarse,
                                    const IndexMask &node_mask,
                                    MutableSpan<int> counts)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
      const OffsetIndices<int> faces = mesh.faces();
      const VArraySpan hide_poly = *orig_mesh_data.attributes.lookup<bool>(".hide_poly",
                                                                           bke::AttrDomain::Face);
      node_mask.foreach_index(
          [&](const int i, const int pos) {
            /* One line per face corner, two indices per line. */
            counts[pos] = count_visible_corners_mesh(faces, hide_poly, nodes[i]) * 2;
          },
          exec_mode::grain_size(8));
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      /* Hidden grid elements are counted too, they are collapsed to a degenerate line instead. */
      const int quads_per_axis = grid_quads_per_axis(key, coarse);
      const int indices_per_grid = 4 * quads_per_axis * (quads_per_axis + 1);
      node_mask.foreach_index([&](const int i, const int pos) {
        counts[pos] = nodes[i].grids().size() * indices_per_grid;
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(
          [&](const int i, const int pos) {
            /* Three lines per triangle, two indices per line. */
            counts[pos] = count_visible_tris_bmesh(nodes[i].bm_faces_) * 6;
          },
          exec_mode::grain_size(8));
      break;
    }
  }
}

/**
 * \brief Calculate the number of indices each node needs in the triangle index buffer.
 *
 * Hidden faces aren't indexed, so the counts change whenever visibility does.
 */
static void calc_tris_index_counts(const Object &object,
                                   const OrigMeshData &orig_mesh_data,
                                   const bool coarse,
                                   const IndexMask &node_mask,
                                   MutableSpan<int> counts)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
      const OffsetIndices<int> faces = mesh.faces();
      const VArraySpan hide_poly = *orig_mesh_data.attributes.lookup<bool>(".hide_poly",
                                                                           bke::AttrDomain::Face);
      node_mask.foreach_index(
          [&](const int i, const int pos) {
            counts[pos] = count_visible_tris_mesh(faces, hide_poly, nodes[i]) * 3;
          },
          exec_mode::grain_size(8));
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      const int display_gridsize = coarse ? 2 : key.grid_size;
      node_mask.foreach_index(
          [&](const int i, const int pos) {
            /* Two triangles per visible quad, three indices per triangle. */
            counts[pos] = bke::pbvh::count_grid_quads(subdiv_ccg.grid_hidden,
                                                      nodes[i].grids(),
                                                      key.grid_size,
                                                      display_gridsize) *
                          6;
          },
          exec_mode::grain_size(8));
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      node_mask.foreach_index(
          [&](const int i, const int pos) {
            counts[pos] = count_visible_tris_bmesh(nodes[i].bm_faces_) * 3;
          },
          exec_mode::grain_size(8));
      break;
    }
  }
}

void DrawCacheImpl::AttributeData::ensure_nodes_capacity(const int64_t nodes_num)
{
  this->dirty_nodes.resize(nodes_num, true);
}
void DrawCacheImpl::AttributeData::ensure_buffer_capacity(const uint64_t needed_vertex_len)
{
  BLI_assert_msg(vbo != nullptr, "Can only grow an existing VBO");
  if (vbo->vertex_len < needed_vertex_len) {
    gpu::VertBufPtr new_vbo = gpu::VertBufPtr(
        GPU_vertbuf_create_with_format_ex(vbo->format, GPU_USAGE_DEVICE_ONLY));
    new_vbo->allocate(uint32_t(needed_vertex_len));
    new_vbo->upload();
    /* Preserve the data of the existing vertices by copying the used vertex data to the new
     * buffer. */
    new_vbo->copy_sub(*vbo, 0, 0, vbo->vertex_len);
    vbo.swap(new_vbo);
  }
}

static void update_positions_mesh(const Object &object,
                                  const Buffer &vbo,
                                  const int vert_len,
                                  const IndexMask &node_mask,
                                  const ScratchOffsets &scratch_offsets,
                                  DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval_from_eval(object);
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(position_format(),
                                                          scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        const IndexRange scratch_range = vbo.scratch_range(node_index,
                                                           scratch_offsets.offsets[scratch_index]);
        extract_data_vert_mesh<float3>(faces,
                                       corner_verts,
                                       vert_positions,
                                       nodes[node_index].faces(),
                                       *scratch_vbo,
                                       scratch_range);
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

static void update_normals_mesh(const Object &object,
                                const Buffer &vbo,
                                const int vert_len,
                                const IndexMask &node_mask,
                                const ScratchOffsets &scratch_offsets,
                                DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval_from_eval(object);
  const Span<float3> face_normals = bke::pbvh::face_normals_eval_from_eval(object);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(normal_format(), scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        short4 *data = scratch_vbo->data<short4>().data() + scratch_offsets.offsets[scratch_index];
        short4 *data_start = data;

        for (const int face : nodes[node_index].faces()) {
          if (!sharp_faces.is_empty() && sharp_faces[face]) {
            const int face_size = faces[face].size();
            std::fill_n(data, face_size, normal_float_to_short(face_normals[face]));
            data += face_size;
          }
          else {
            for (const int vert : corner_verts.slice(faces[face])) {
              *data = normal_float_to_short(vert_normals[vert]);
              data++;
            }
          }
        }
        UNUSED_VARS_NDEBUG(data_start);
        BLI_assert(data == data_start + vbo.sizes[node_index]);
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_masks_mesh(const Object &object,
                                           const OrigMeshData &orig_mesh_data,
                                           const Buffer &vbo,
                                           const int vert_len,
                                           const IndexMask &node_mask,
                                           const ScratchOffsets &scratch_offsets,
                                           DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const VArraySpan mask = *orig_mesh_data.attributes.lookup<float>(".sculpt_mask",
                                                                   bke::AttrDomain::Point);
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(mask_format(), scratch_offsets.total);
  if (!mask.is_empty()) {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          float *data = scratch_vbo->data<float>().data() + scratch_offsets.offsets[scratch_index];
          float *data_start = data;
          for (const int face : nodes[node_index].faces()) {
            for (const int vert : corner_verts.slice(faces[face])) {
              *data = mask[vert];
              data++;
            }
          }
          UNUSED_VARS_NDEBUG(data_start);
          BLI_assert(data == data_start + vbo.sizes[node_index]);
        },
        exec_mode::grain_size(1));
  }
  else {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          scratch_vbo->data<float>()
              .slice(scratch_offsets.offsets[scratch_index],
                     vbo.acquired[node_index] ? vbo.sizes[node_index] : 0)
              .fill(0.0f);
        },
        exec_mode::grain_size(64));
  }
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_face_sets_mesh(const Object &object,
                                               const OrigMeshData &orig_mesh_data,
                                               const Buffer &vbo,
                                               const int vert_len,
                                               const IndexMask &node_mask,
                                               const ScratchOffsets &scratch_offsets,
                                               DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
  const OffsetIndices<int> faces = mesh.faces();
  const int color_default = orig_mesh_data.face_set_default;
  const int color_seed = orig_mesh_data.face_set_seed;
  const VArraySpan face_sets = *orig_mesh_data.attributes.lookup<int>(".sculpt_face_set",
                                                                      bke::AttrDomain::Face);
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(face_set_format(),
                                                          scratch_offsets.total);
  if (!face_sets.is_empty()) {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          uchar4 *data = scratch_vbo->data<uchar4>().data() +
                         scratch_offsets.offsets[scratch_index];
          uchar4 *data_start = data;
          for (const int face : nodes[node_index].faces()) {
            const int id = face_sets[face];

            uchar4 fset_color(UCHAR_MAX);
            if (id != color_default) {
              BKE_paint_face_set_overlay_color_get(id, color_seed, fset_color);
            }
            else {
              /* Skip for the default color face set to render it white. */
              fset_color[0] = fset_color[1] = fset_color[2] = UCHAR_MAX;
            }

            const int face_size = faces[face].size();
            std::fill_n(data, face_size, fset_color);
            data += face_size;
          }
          UNUSED_VARS_NDEBUG(data_start);
          BLI_assert(data == data_start + vbo.sizes[node_index]);
        },
        exec_mode::grain_size(1));
  }
  else {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          scratch_vbo->data<uchar4>()
              .slice(scratch_offsets.offsets[scratch_index],
                     vbo.acquired[node_index] ? vbo.sizes[node_index] : 0)
              .fill(uchar4(255));
        },
        exec_mode::grain_size(64));
  }
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_generic_attribute_mesh(const Object &object,
                                                       const OrigMeshData &orig_mesh_data,
                                                       const Buffer &vbo,
                                                       const int vert_len,
                                                       const StringRef name,
                                                       const IndexMask &node_mask,
                                                       const ScratchOffsets &scratch_offsets,
                                                       DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
  const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
  const bke::GAttributeReader attr = attributes.lookup(name);
  if (!attr || attr.domain == bke::AttrDomain::Edge) {
    return;
  }
  const bke::AttrType data_type = bke::cpp_type_to_attribute_type(attr.varray.type());
  const GPUVertFormat format = attribute_format(orig_mesh_data, name, data_type);
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(format, scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        const IndexRange scratch_range = vbo.scratch_range(node_index,
                                                           scratch_offsets.offsets[scratch_index]);
        bke::attribute_math::to_static_type(attr.varray.type(), [&]<typename T>() {
          if constexpr (!std::is_void_v<typename AttributeConverter<T>::VBOType>) {
            const VArraySpan<T> src = attr.varray.typed<T>();
            switch (attr.domain) {
              case bke::AttrDomain::Point:
                extract_data_vert_mesh<T>(faces,
                                          corner_verts,
                                          src,
                                          nodes[node_index].faces(),
                                          *scratch_vbo,
                                          scratch_range);
                break;
              case bke::AttrDomain::Face:
                extract_data_face_mesh<T>(
                    faces, src, nodes[node_index].faces(), *scratch_vbo, scratch_range);
                break;
              case bke::AttrDomain::Corner:
                extract_data_corner_mesh<T>(
                    faces, src, nodes[node_index].faces(), *scratch_vbo, scratch_range);
                break;
              default:
                BLI_assert_unreachable();
            }
          }
        });
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_positions_grids(const Object &object,
                                                const Buffer &vbo,
                                                const int vert_len,
                                                const BitSpan use_flat_layout,
                                                const IndexMask &node_mask,
                                                const ScratchOffsets &scratch_offsets,
                                                DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  const Span<float3> positions = subdiv_ccg.positions;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(position_format(),
                                                          scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        float3 *data = scratch_vbo->data<float3>().data() + scratch_offsets.offsets[scratch_index];
        float3 *data_start = data;
        if (use_flat_layout[node_index]) {
          const int grid_size_1 = key.grid_size - 1;
          for (const int grid : nodes[node_index].grids()) {
            const Span<float3> grid_positions = positions.slice(bke::ccg::grid_range(key, grid));
            for (int y = 0; y < grid_size_1; y++) {
              for (int x = 0; x < grid_size_1; x++) {
                *data = grid_positions[CCG_grid_xy_to_index(key.grid_size, x, y)];
                data++;
                *data = grid_positions[CCG_grid_xy_to_index(key.grid_size, x + 1, y)];
                data++;
                *data = grid_positions[CCG_grid_xy_to_index(key.grid_size, x + 1, y + 1)];
                data++;
                *data = grid_positions[CCG_grid_xy_to_index(key.grid_size, x, y + 1)];
                data++;
              }
            }
          }
        }
        else {
          for (const int grid : nodes[node_index].grids()) {
            const Span<float3> grid_positions = positions.slice(bke::ccg::grid_range(key, grid));
            std::copy_n(grid_positions.data(), grid_positions.size(), data);
            data += grid_positions.size();
          }
        }
        UNUSED_VARS_NDEBUG(data_start);
        BLI_assert(data == data_start + vbo.sizes[node_index]);
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_normals_grids(const Object &object,
                                              const OrigMeshData &orig_mesh_data,
                                              const Buffer &vbo,
                                              const int vert_len,
                                              const BitSpan use_flat_layout,
                                              const IndexMask &node_mask,
                                              const ScratchOffsets &scratch_offsets,
                                              DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  const Span<float3> positions = subdiv_ccg.positions;
  const Span<float3> normals = subdiv_ccg.normals;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
  const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(normal_format(), scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        short4 *data = scratch_vbo->data<short4>().data() + scratch_offsets.offsets[scratch_index];
        short4 *data_start = data;

        if (use_flat_layout[node_index]) {
          const int grid_size_1 = key.grid_size - 1;
          for (const int grid : nodes[node_index].grids()) {
            const Span<float3> grid_positions = positions.slice(bke::ccg::grid_range(key, grid));
            const Span<float3> grid_normals = normals.slice(bke::ccg::grid_range(key, grid));
            if (!sharp_faces.is_empty() && sharp_faces[grid_to_face_map[grid]]) {
              for (int y = 0; y < grid_size_1; y++) {
                for (int x = 0; x < grid_size_1; x++) {
                  float3 no;
                  normal_quad_v3(no,
                                 grid_positions[CCG_grid_xy_to_index(key.grid_size, x, y + 1)],
                                 grid_positions[CCG_grid_xy_to_index(key.grid_size, x + 1, y + 1)],
                                 grid_positions[CCG_grid_xy_to_index(key.grid_size, x + 1, y)],
                                 grid_positions[CCG_grid_xy_to_index(key.grid_size, x, y)]);
                  std::fill_n(data, 4, normal_float_to_short(no));
                  data += 4;
                }
              }
            }
            else {
              for (int y = 0; y < grid_size_1; y++) {
                for (int x = 0; x < grid_size_1; x++) {
                  std::fill_n(data,
                              4,
                              normal_float_to_short(
                                  grid_normals[CCG_grid_xy_to_index(key.grid_size, x, y)]));
                  data += 4;
                }
              }
            }
          }
        }
        else {
          /* The non-flat VBO layout does not support sharp faces. */
          for (const int grid : nodes[node_index].grids()) {
            for (const float3 &normal : normals.slice(bke::ccg::grid_range(key, grid))) {
              *data = normal_float_to_short(normal);
              data++;
            }
          }
        }
        UNUSED_VARS_NDEBUG(data_start);
        BLI_assert(data == data_start + vbo.sizes[node_index]);
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_masks_grids(const Object &object,
                                            const Buffer &vbo,
                                            const int vert_len,
                                            const BitSpan use_flat_layout,
                                            const IndexMask &node_mask,
                                            const ScratchOffsets &scratch_offsets,
                                            DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const Span<float> masks = subdiv_ccg.masks;
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(mask_format(), scratch_offsets.total);
  if (!masks.is_empty()) {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          float *data = scratch_vbo->data<float>().data() + scratch_offsets.offsets[scratch_index];
          float *data_start = data;
          if (use_flat_layout[node_index]) {
            const int grid_size_1 = key.grid_size - 1;
            for (const int grid : nodes[node_index].grids()) {
              const Span<float> grid_masks = masks.slice(bke::ccg::grid_range(key, grid));
              for (int y = 0; y < grid_size_1; y++) {
                for (int x = 0; x < grid_size_1; x++) {
                  *data = grid_masks[CCG_grid_xy_to_index(key.grid_size, x, y)];
                  data++;
                  *data = grid_masks[CCG_grid_xy_to_index(key.grid_size, x + 1, y)];
                  data++;
                  *data = grid_masks[CCG_grid_xy_to_index(key.grid_size, x + 1, y + 1)];
                  data++;
                  *data = grid_masks[CCG_grid_xy_to_index(key.grid_size, x, y + 1)];
                  data++;
                }
              }
            }
          }
          else {
            for (const int grid : nodes[node_index].grids()) {
              const Span<float> grid_masks = masks.slice(bke::ccg::grid_range(key, grid));
              std::copy_n(grid_masks.data(), grid_masks.size(), data);
              data += grid_masks.size();
            }
          }
          UNUSED_VARS_NDEBUG(data_start);
          BLI_assert(data == data_start + vbo.sizes[node_index]);
        },
        exec_mode::grain_size(1));
  }
  else {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          scratch_vbo->data<float>()
              .slice(scratch_offsets.offsets[scratch_index],
                     vbo.acquired[node_index] ? vbo.sizes[node_index] : 0)
              .fill(0.0f);
        },
        exec_mode::grain_size(64));
  }
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_face_sets_grids(const Object &object,
                                                const OrigMeshData &orig_mesh_data,
                                                const Buffer &vbo,
                                                const int vert_len,
                                                const BitSpan use_flat_layout,
                                                const IndexMask &node_mask,
                                                const ScratchOffsets &scratch_offsets,
                                                DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int color_default = orig_mesh_data.face_set_default;
  const int color_seed = orig_mesh_data.face_set_seed;
  const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;
  const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(face_set_format(),
                                                          scratch_offsets.total);
  if (const VArray<int> face_sets = *attributes.lookup<int>(".sculpt_face_set",
                                                            bke::AttrDomain::Face))
  {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          const Span<int> grids = nodes[node_index].grids();
          const int verts_per_grid = use_flat_layout[node_index] ?
                                         square_i(key.grid_size - 1) * 4 :
                                         square_i(key.grid_size);
          uchar4 *data = scratch_vbo->data<uchar4>().data() +
                         scratch_offsets.offsets[scratch_index];
          uchar4 *data_start = data;
          for (const int j : grids.index_range()) {
            uchar4 color{UCHAR_MAX};
            const int fset = face_sets[grid_to_face_map[grids[j]]];
            if (fset != color_default) {
              BKE_paint_face_set_overlay_color_get(fset, color_seed, color);
            }

            std::fill_n(data, verts_per_grid, color);
            data += verts_per_grid;
          }
          UNUSED_VARS_NDEBUG(data_start);
          BLI_assert(data == data_start + vbo.sizes[node_index]);
        },
        exec_mode::grain_size(1));
  }
  else {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          scratch_vbo->data<uchar4>()
              .slice(scratch_offsets.offsets[scratch_index],
                     vbo.acquired[node_index] ? vbo.sizes[node_index] : 0)
              .fill(uchar4{UCHAR_MAX});
        },
        exec_mode::grain_size(1));
  }
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

static void calc_quad_uv(const SubdivCCG &subdiv_ccg,
                         const int uv_map_index,
                         const float grid_size_1_inv,
                         const int ptex_face_index,
                         const int grid_start,
                         const int corner,
                         const int x,
                         const int y,
                         const int offset,
                         MutableSpan<float2> result)
{
  const float grid_u = x * grid_size_1_inv;
  const float grid_v = y * grid_size_1_inv;
  float u;
  float v;
  bke::subdiv::rotate_grid_to_quad(corner, grid_u, grid_v, &u, &v);
  bke::subdiv::eval_face_varying(
      subdiv_ccg.subdiv, uv_map_index, ptex_face_index, u, v, result[grid_start + offset]);
}

static void calc_non_quad_uv(const SubdivCCG &subdiv_ccg,
                             const int uv_map_index,
                             const float grid_size_1_inv,
                             const int ptex_face_index,
                             const int grid_start,
                             const int x,
                             const int y,
                             const int offset,
                             MutableSpan<float2> result)
{
  const float u = 1.0f - (y * grid_size_1_inv);
  const float v = 1.0f - (x * grid_size_1_inv);
  bke::subdiv::eval_face_varying(
      subdiv_ccg.subdiv, uv_map_index, ptex_face_index, u, v, result[grid_start + offset]);
}

static void calc_node_uvs_for_flat_layout(const SubdivCCG &subdiv_ccg,
                                          const int uv_map_index,
                                          Span<int> grids,
                                          MutableSpan<float2> result)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grid_size_1 = key.grid_size - 1;
  const int grid_area_1 = grid_size_1 * grid_size_1;
  const int verts_per_grid = grid_area_1 * 4;

  const Span<int> face_ptex_offset = bke::subdiv::face_ptex_offset_get(subdiv_ccg.subdiv);
  const float grid_size_1_inv = 1.0f / (key.grid_size - 1);

  threading::parallel_for(grids.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int grid = grids[i];
      const int face_index = subdiv_ccg.grid_to_face_map[grid];
      const IndexRange face = subdiv_ccg.faces[face_index];
      const int corner = grid - face.start();
      if (face.size() == 4) {
        const int ptex_face_index = face_ptex_offset[face_index];
        for (int y = 0; y < grid_size_1; y++) {
          for (int x = 0; x < grid_size_1; x++) {
            const int start = verts_per_grid * i + CCG_grid_xy_to_index(grid_size_1, x, y) * 4;
            calc_quad_uv(subdiv_ccg,
                         uv_map_index,
                         grid_size_1_inv,
                         ptex_face_index,
                         start,
                         corner,
                         x,
                         y,
                         0,
                         result);
            calc_quad_uv(subdiv_ccg,
                         uv_map_index,
                         grid_size_1_inv,
                         ptex_face_index,
                         start,
                         corner,
                         x + 1,
                         y,
                         1,
                         result);
            calc_quad_uv(subdiv_ccg,
                         uv_map_index,
                         grid_size_1_inv,
                         ptex_face_index,
                         start,
                         corner,
                         x + 1,
                         y + 1,
                         2,
                         result);
            calc_quad_uv(subdiv_ccg,
                         uv_map_index,
                         grid_size_1_inv,
                         ptex_face_index,
                         start,
                         corner,
                         x,
                         y + 1,
                         3,
                         result);
          }
        }
      }
      else {
        const int ptex_face_index = face_ptex_offset[face_index] + corner;
        for (int y = 0; y < grid_size_1; y++) {
          for (int x = 0; x < grid_size_1; x++) {
            const int start = verts_per_grid * i + CCG_grid_xy_to_index(grid_size_1, x, y) * 4;
            calc_non_quad_uv(subdiv_ccg,
                             uv_map_index,
                             grid_size_1_inv,
                             ptex_face_index,
                             start,
                             x,
                             y,
                             0,
                             result);
            calc_non_quad_uv(subdiv_ccg,
                             uv_map_index,
                             grid_size_1_inv,
                             ptex_face_index,
                             start,
                             x + 1,
                             y,
                             1,
                             result);
            calc_non_quad_uv(subdiv_ccg,
                             uv_map_index,
                             grid_size_1_inv,
                             ptex_face_index,
                             start,
                             x + 1,
                             y + 1,
                             2,
                             result);
            calc_non_quad_uv(subdiv_ccg,
                             uv_map_index,
                             grid_size_1_inv,
                             ptex_face_index,
                             start,
                             x,
                             y + 1,
                             3,
                             result);
          }
        }
      }
    }
  });
}

static void calc_node_uvs_for_standard_layout(const SubdivCCG &subdiv_ccg,
                                              const int uv_map_index,
                                              Span<int> grids,
                                              MutableSpan<float2> result)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grid_size = key.grid_size;
  const int grid_area = key.grid_area;

  const Span<int> face_ptex_offset = bke::subdiv::face_ptex_offset_get(subdiv_ccg.subdiv);
  const float grid_size_1_inv = 1.0f / (grid_size - 1);
  threading::parallel_for(grids.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int grid = grids[i];
      const int face_index = subdiv_ccg.grid_to_face_map[grid];
      const IndexRange face = subdiv_ccg.faces[face_index];
      const int corner = grid - face.start();
      if (face.size() == 4) {
        const int ptex_face_index = face_ptex_offset[face_index];
        for (int y = 0; y < grid_size; y++) {
          const float grid_v = y * grid_size_1_inv;
          for (int x = 0; x < grid_size; x++) {
            const float grid_u = x * grid_size_1_inv;
            float u;
            float v;
            bke::subdiv::rotate_grid_to_quad(corner, grid_u, grid_v, &u, &v);
            const int element = i * grid_area + CCG_grid_xy_to_index(grid_size, x, y);
            bke::subdiv::eval_face_varying(
                subdiv_ccg.subdiv, uv_map_index, ptex_face_index, u, v, result[element]);
          }
        }
      }
      else {
        const int ptex_face_index = face_ptex_offset[face_index] + corner;
        for (int y = 0; y < grid_size; y++) {
          const float u = 1.0f - (y * grid_size_1_inv);
          for (int x = 0; x < grid_size; x++) {
            const float v = 1.0f - (x * grid_size_1_inv);
            const int element = i * grid_area + CCG_grid_xy_to_index(grid_size, x, y);
            bke::subdiv::eval_face_varying(
                subdiv_ccg.subdiv, uv_map_index, ptex_face_index, u, v, result[element]);
          }
        }
      }
    }
  });
}

static void calc_node_uvs(const SubdivCCG &subdiv_ccg,
                          const int uv_map_index,
                          const bke::pbvh::GridsNode &node,
                          const bool use_flat_layout,
                          MutableSpan<float2> result)
{
  const Span<int> grids = node.grids();
  if (use_flat_layout) {
    calc_node_uvs_for_flat_layout(subdiv_ccg, uv_map_index, grids, result);
  }
  if (!use_flat_layout) {
    calc_node_uvs_for_standard_layout(subdiv_ccg, uv_map_index, grids, result);
  }
}

BLI_NOINLINE static void fill_uvs_grids(const Object &object,
                                        const OrigMeshData &orig_mesh_data,
                                        const Buffer &vbo,
                                        const BitSpan use_flat_layout,
                                        const IndexMask &node_mask,
                                        const StringRef name,
                                        const ScratchOffsets &scratch_offsets,
                                        const gpu::VertBufPtr &scratch_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        float2 *data = scratch_vbo->data<float2>().data() + scratch_offsets.offsets[scratch_index];

        const int uv_channel = orig_mesh_data.uv_map_names.index_of_try(name);
        if (uv_channel == -1) [[unlikely]] {
          BLI_assert_msg(uv_channel != -1,
                         "Unable to find appropriate FVar channel for given UV map");
          const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
          std::fill_n(data, dst_len, float2(0));
        }
        else {
          calc_node_uvs(subdiv_ccg,
                        uv_channel,
                        nodes[node_index],
                        use_flat_layout[node_index],
                        {data, vbo.acquired[node_index] ? vbo.sizes[node_index] : 0});
        }
      },
      exec_mode::grain_size(1));
}

static float4 quad_weights_from_grid(const int corner,
                                     const int x,
                                     const int y,
                                     const float grid_size_1_inv)
{
  const float grid_u = x * grid_size_1_inv;
  const float grid_v = y * grid_size_1_inv;
  float u;
  float v;
  bke::subdiv::rotate_grid_to_quad(corner, grid_u, grid_v, &u, &v);
  return bke::subdiv::quad_weights_from_uv(u, v);
}

static float4 quad_weights_from_irregular_corner(const int x,
                                                 const int y,
                                                 const float grid_size_1_inv)
{
  const float u = 1.0f - (y * grid_size_1_inv);
  const float v = 1.0f - (x * grid_size_1_inv);

  return bke::subdiv::quad_weights_from_uv(u, v);
}

template<typename T>
static void interpolate_vert_data_for_flat_layout(
    const SubdivCCG &subdiv_ccg,
    const Span<int> corner_verts,
    const Span<int> grids,
    const Span<T> src,
    MutableSpan<typename AttributeConverter<T>::VBOType> vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grid_size_1 = key.grid_size - 1;
  const int grid_area_1 = grid_size_1 * grid_size_1;
  const int verts_per_grid = grid_area_1 * 4;
  const float grid_size_1_inv = 1.0f / (key.grid_size - 1);
  MutableSpan<VBOType> dst = vbo;

  threading::parallel_for(grids.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int grid = grids[i];
      const int face_index = subdiv_ccg.grid_to_face_map[grid];
      const IndexRange face = subdiv_ccg.faces[face_index];
      const int corner = grid - face.start();
      if (face.size() == 4) {
        Span<int> face_verts = corner_verts.slice(face);
        for (int y = 0; y < grid_size_1; y++) {
          for (int x = 0; x < grid_size_1; x++) {
            const int start = verts_per_grid * i + CCG_grid_xy_to_index(grid_size_1, x, y) * 4;
            dst[start] = Converter::convert(
                bke::attribute_math::mix4(quad_weights_from_grid(corner, x, y, grid_size_1_inv),
                                          src[face_verts[0]],
                                          src[face_verts[1]],
                                          src[face_verts[2]],
                                          src[face_verts[3]]));
            dst[start + 1] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_grid(corner, x + 1, y, grid_size_1_inv),
                src[face_verts[0]],
                src[face_verts[1]],
                src[face_verts[2]],
                src[face_verts[3]]));
            dst[start + 2] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_grid(corner, x + 1, y + 1, grid_size_1_inv),
                src[face_verts[0]],
                src[face_verts[1]],
                src[face_verts[2]],
                src[face_verts[3]]));
            dst[start + 3] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_grid(corner, x, y + 1, grid_size_1_inv),
                src[face_verts[0]],
                src[face_verts[1]],
                src[face_verts[2]],
                src[face_verts[3]]));
          }
        }
      }
      else {
        const std::array<T, 4> values =
            bke::subdiv::interpolate_vert_data_for_subdivided_irregular_face<T>(
                corner_verts, face, grid, src);
        for (int y = 0; y < grid_size_1; y++) {
          for (int x = 0; x < grid_size_1; x++) {
            const int start = verts_per_grid * i + CCG_grid_xy_to_index(grid_size_1, x, y) * 4;
            dst[start] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x, y, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
            dst[start + 1] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x + 1, y, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
            dst[start + 2] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x + 1, y + 1, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
            dst[start + 3] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x, y + 1, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
          }
        }
      }
    }
  });
}

template<typename T>
static void interpolate_vert_data_for_standard_layout(
    const SubdivCCG &subdiv_ccg,
    const Span<int> corner_verts,
    const Span<int> grids,
    const Span<T> src,
    MutableSpan<typename AttributeConverter<T>::VBOType> vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grid_size = key.grid_size;
  const int grid_area = key.grid_area;

  const float grid_size_1_inv = 1.0f / (grid_size - 1);
  MutableSpan<VBOType> dst = vbo;
  threading::parallel_for(grids.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int grid = grids[i];
      const int face_index = subdiv_ccg.grid_to_face_map[grid];
      const IndexRange face = subdiv_ccg.faces[face_index];
      const int corner = grid - face.start();
      if (face.size() == 4) {
        const Span<int> face_verts = corner_verts.slice(face);
        for (int y = 0; y < grid_size; y++) {
          for (int x = 0; x < grid_size; x++) {
            const int element = i * grid_area + CCG_grid_xy_to_index(grid_size, x, y);
            dst[element] = Converter::convert(
                bke::attribute_math::mix4(quad_weights_from_grid(corner, x, y, grid_size_1_inv),
                                          src[face_verts[0]],
                                          src[face_verts[1]],
                                          src[face_verts[2]],
                                          src[face_verts[3]]));
          }
        }
      }
      else {
        const std::array<T, 4> values =
            bke::subdiv::interpolate_vert_data_for_subdivided_irregular_face<T>(
                corner_verts, face, grid, src);
        for (int y = 0; y < grid_size; y++) {
          for (int x = 0; x < grid_size; x++) {
            const int element = i * grid_area + CCG_grid_xy_to_index(grid_size, x, y);
            dst[element] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x, y, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
          }
        }
      }
    }
  });
}

BLI_NOINLINE static void calc_generic_vert_attribute_grids(const Object &object,
                                                           const OrigMeshData &orig_mesh_data,
                                                           const Buffer &vbo,
                                                           const BitSpan use_flat_layout,
                                                           const IndexMask &node_mask,
                                                           const StringRef name,
                                                           const ScratchOffsets &scratch_offsets,
                                                           const gpu::VertBufPtr &scratch_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
  const GVArraySpan attr = *attributes.lookup(name);

  const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
  const Span<int> corner_verts = mesh.corner_verts();

  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        bke::attribute_math::to_static_type(attr.type(), [&]<typename T>() {
          if constexpr (!std::is_void_v<typename AttributeConverter<T>::VBOType>) {
            const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
            MutableSpan<typename AttributeConverter<T>::VBOType> dst =
                scratch_vbo->data<typename AttributeConverter<T>::VBOType>().slice(
                    scratch_offsets.offsets[scratch_index], dst_len);
            if (use_flat_layout[node_index]) {
              interpolate_vert_data_for_flat_layout<T>(
                  subdiv_ccg, corner_verts, nodes[node_index].grids(), attr.typed<T>(), dst);
            }
            else {
              interpolate_vert_data_for_standard_layout<T>(
                  subdiv_ccg, corner_verts, nodes[node_index].grids(), attr.typed<T>(), dst);
            }
          }
        });
      },
      exec_mode::grain_size(1));
}

template<typename T>
static void interpolate_corner_data_for_flat_layout(
    const SubdivCCG &subdiv_ccg,
    const Span<int> grids,
    const Span<T> src,
    MutableSpan<typename AttributeConverter<T>::VBOType> vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grid_size_1 = key.grid_size - 1;
  const int grid_area_1 = grid_size_1 * grid_size_1;
  const int verts_per_grid = grid_area_1 * 4;
  const float grid_size_1_inv = 1.0f / (key.grid_size - 1);
  MutableSpan<VBOType> dst = vbo;

  threading::parallel_for(grids.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int grid = grids[i];
      const int face_index = subdiv_ccg.grid_to_face_map[grid];
      const IndexRange face = subdiv_ccg.faces[face_index];
      const int corner = grid - face.start();
      if (face.size() == 4) {
        for (int y = 0; y < grid_size_1; y++) {
          for (int x = 0; x < grid_size_1; x++) {
            const int start = verts_per_grid * i + CCG_grid_xy_to_index(grid_size_1, x, y) * 4;
            dst[start] = Converter::convert(
                bke::attribute_math::mix4(quad_weights_from_grid(corner, x, y, grid_size_1_inv),
                                          src[face[0]],
                                          src[face[1]],
                                          src[face[2]],
                                          src[face[3]]));
            dst[start + 1] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_grid(corner, x + 1, y, grid_size_1_inv),
                src[face[0]],
                src[face[1]],
                src[face[2]],
                src[face[3]]));
            dst[start + 2] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_grid(corner, x + 1, y + 1, grid_size_1_inv),
                src[face[0]],
                src[face[1]],
                src[face[2]],
                src[face[3]]));
            dst[start + 3] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_grid(corner, x, y + 1, grid_size_1_inv),
                src[face[0]],
                src[face[1]],
                src[face[2]],
                src[face[3]]));
          }
        }
      }
      else {
        const std::array<T, 4> values =
            bke::subdiv::interpolate_corner_data_for_subdivided_irregular_face<T>(face, grid, src);
        for (int y = 0; y < grid_size_1; y++) {
          for (int x = 0; x < grid_size_1; x++) {
            const int start = verts_per_grid * i + CCG_grid_xy_to_index(grid_size_1, x, y) * 4;
            dst[start] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x, y, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
            dst[start + 1] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x + 1, y, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
            dst[start + 2] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x + 1, y + 1, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
            dst[start + 3] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x, y + 1, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
          }
        }
      }
    }
  });
}

template<typename T>
static void interpolate_corner_data_for_standard_layout(
    const SubdivCCG &subdiv_ccg,
    const Span<int> grids,
    const Span<T> src,
    MutableSpan<typename AttributeConverter<T>::VBOType> vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int grid_size = key.grid_size;
  const int grid_area = key.grid_area;

  const float grid_size_1_inv = 1.0f / (grid_size - 1);
  MutableSpan<VBOType> dst = vbo;
  threading::parallel_for(grids.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int grid = grids[i];
      const int face_index = subdiv_ccg.grid_to_face_map[grid];
      const IndexRange face = subdiv_ccg.faces[face_index];
      const int corner = grid - face.start();
      if (face.size() == 4) {
        for (int y = 0; y < grid_size; y++) {
          for (int x = 0; x < grid_size; x++) {
            const int element = i * grid_area + CCG_grid_xy_to_index(grid_size, x, y);
            dst[element] = Converter::convert(
                bke::attribute_math::mix4(quad_weights_from_grid(corner, x, y, grid_size_1_inv),
                                          src[face[0]],
                                          src[face[1]],
                                          src[face[2]],
                                          src[face[3]]));
          }
        }
      }
      else {
        const std::array<T, 4> values =
            bke::subdiv::interpolate_corner_data_for_subdivided_irregular_face<T>(face, grid, src);
        for (int y = 0; y < grid_size; y++) {
          for (int x = 0; x < grid_size; x++) {
            const int element = i * grid_area + CCG_grid_xy_to_index(grid_size, x, y);
            dst[element] = Converter::convert(bke::attribute_math::mix4(
                quad_weights_from_irregular_corner(x, y, grid_size_1_inv),
                values[0],
                values[1],
                values[2],
                values[3]));
          }
        }
      }
    }
  });
}

BLI_NOINLINE static void calc_generic_corner_attribute_grids(const Object &object,
                                                             const OrigMeshData &orig_mesh_data,
                                                             const Buffer &vbo,
                                                             const BitSpan use_flat_layout,
                                                             const IndexMask &node_mask,
                                                             const StringRef name,
                                                             const ScratchOffsets &scratch_offsets,
                                                             const gpu::VertBufPtr &scratch_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
  const GVArraySpan attr = *attributes.lookup(name);

  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        bke::attribute_math::to_static_type(attr.type(), [&]<typename T>() {
          if constexpr (!std::is_void_v<typename AttributeConverter<T>::VBOType>) {
            const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
            MutableSpan<typename AttributeConverter<T>::VBOType> dst =
                scratch_vbo->data<typename AttributeConverter<T>::VBOType>().slice(
                    scratch_offsets.offsets[scratch_index], dst_len);
            if (use_flat_layout[node_index]) {
              interpolate_corner_data_for_flat_layout<T>(
                  subdiv_ccg, nodes[node_index].grids(), attr.typed<T>(), dst);
            }
            else {
              interpolate_corner_data_for_standard_layout<T>(
                  subdiv_ccg, nodes[node_index].grids(), attr.typed<T>(), dst);
            }
          }
        });
      },
      exec_mode::grain_size(1));
}

template<typename T>
static void fill_face_data_grids(const SubdivCCG &subdiv_ccg,
                                 const bool use_flat_layout,
                                 const Span<int> grids,
                                 const Span<T> src,
                                 MutableSpan<typename AttributeConverter<T>::VBOType> vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
  const int verts_per_grid = use_flat_layout ? (key.grid_size - 1) * (key.grid_size - 1) * 4 :
                                               key.grid_area;
  VBOType *dst = vbo.data();
  for (const int i : grids.index_range()) {
    const int face_index = subdiv_ccg.grid_to_face_map[grids[i]];
    std::fill_n(dst, verts_per_grid, Converter::convert(src[face_index]));
    dst += verts_per_grid;
  }
}

BLI_NOINLINE static void fill_generic_face_attribute_grids(const Object &object,
                                                           const OrigMeshData &orig_mesh_data,
                                                           const Buffer &vbo,
                                                           const BitSpan use_flat_layout,
                                                           const IndexMask &node_mask,
                                                           const StringRef name,
                                                           const ScratchOffsets &scratch_offsets,
                                                           const gpu::VertBufPtr &scratch_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
  const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
  const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
  const GVArraySpan attr = *attributes.lookup(name);

  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        bke::attribute_math::to_static_type(attr.type(), [&]<typename T>() {
          if constexpr (!std::is_void_v<typename AttributeConverter<T>::VBOType>) {
            const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
            MutableSpan<typename AttributeConverter<T>::VBOType> dst =
                scratch_vbo->data<typename AttributeConverter<T>::VBOType>().slice(
                    scratch_offsets.offsets[scratch_index], dst_len);
            fill_face_data_grids<T>(subdiv_ccg,
                                    use_flat_layout[node_index],
                                    nodes[node_index].grids(),
                                    attr.typed<T>(),
                                    dst);
          }
        });
      },
      exec_mode::grain_size(1));
}

BLI_NOINLINE static void update_generic_attribute_grids(const Object &object,
                                                        const OrigMeshData &orig_mesh_data,
                                                        const Buffer &vbo,
                                                        const int vert_len,
                                                        const StringRef name,
                                                        const BitSpan use_flat_layout,
                                                        const IndexMask &node_mask,
                                                        const ScratchOffsets &scratch_offsets,
                                                        DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
  const bke::GAttributeReader attr = attributes.lookup(name);
  if (attr.domain == bke::AttrDomain::Edge) {
    return;
  }

  const bke::AttrType data_type = (!attr) ? bke::AttrType::Float3 :
                                            bke::cpp_type_to_attribute_type(attr.varray.type());
  const StringRef format_name = (!attr) ? "Dummy" : name;
  const GPUVertFormat format = attribute_format(orig_mesh_data, format_name, data_type);
  const bool is_uvs = attr && attr.domain == bke::AttrDomain::Corner &&
                      data_type == bke::AttrType::Float2;
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(format, scratch_offsets.total);

  if (!attr) {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
          scratch_vbo->data<float3>()
              .slice(scratch_offsets.offsets[scratch_index], dst_len)
              .fill(float3(0.0f));
        },
        exec_mode::grain_size(1));
  }
  else if (is_uvs) {
    fill_uvs_grids(object,
                   orig_mesh_data,
                   vbo,
                   use_flat_layout,
                   node_mask,
                   name,
                   scratch_offsets,
                   scratch_vbo);
  }
  else {
    switch (attr.domain) {
      case bke::AttrDomain::Point:
        calc_generic_vert_attribute_grids(object,
                                          orig_mesh_data,
                                          vbo,
                                          use_flat_layout,
                                          node_mask,
                                          name,
                                          scratch_offsets,
                                          scratch_vbo);
        break;
      case bke::AttrDomain::Corner:
        calc_generic_corner_attribute_grids(object,
                                            orig_mesh_data,
                                            vbo,
                                            use_flat_layout,
                                            node_mask,
                                            name,
                                            scratch_offsets,
                                            scratch_vbo);
        break;
      case bke::AttrDomain::Face:
        fill_generic_face_attribute_grids(object,
                                          orig_mesh_data,
                                          vbo,
                                          use_flat_layout,
                                          node_mask,
                                          name,
                                          scratch_offsets,
                                          scratch_vbo);
        break;
      default:
        BLI_assert_unreachable();
    }
  }
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_positions_bmesh(const Object &object,
                                                const Buffer &vbo,
                                                const int vert_len,
                                                const IndexMask &node_mask,
                                                const ScratchOffsets &scratch_offsets,
                                                DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(position_format(),
                                                          scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        float3 *data = scratch_vbo->data<float3>().data() + scratch_offsets.offsets[scratch_index];
        float3 *data_start = data;
        for (const BMFace *face :
             BKE_pbvh_bmesh_node_faces(&const_cast<bke::pbvh::BMeshNode &>(nodes[node_index])))
        {
          if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
            continue;
          }
          const BMLoop *l = face->l_first;
          *data = l->prev->v->co;
          data++;
          *data = l->v->co;
          data++;
          *data = l->next->v->co;
          data++;
        }
        UNUSED_VARS_NDEBUG(data_start);
        BLI_assert(data == data_start + vbo.sizes[node_index]);
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_normals_bmesh(const Object &object,
                                              const Buffer &vbo,
                                              const int vert_len,
                                              const IndexMask &node_mask,
                                              const ScratchOffsets &scratch_offsets,
                                              DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(normal_format(), scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        short4 *data = scratch_vbo->data<short4>().data() + scratch_offsets.offsets[scratch_index];
        short4 *data_start = data;
        for (const BMFace *face :
             BKE_pbvh_bmesh_node_faces(&const_cast<bke::pbvh::BMeshNode &>(nodes[node_index])))
        {
          if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
            continue;
          }
          if (BM_elem_flag_test(face, BM_ELEM_SMOOTH)) {
            const BMLoop *l = face->l_first;
            *data = normal_float_to_short(l->prev->v->no);
            data++;
            *data = normal_float_to_short(l->v->no);
            data++;
            *data = normal_float_to_short(l->next->v->no);
            data++;
          }
          else {
            std::fill_n(data, 3, normal_float_to_short(face->no));
            data += 3;
          }
        }
        UNUSED_VARS_NDEBUG(data_start);
        BLI_assert(data == data_start + vbo.sizes[node_index]);
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_masks_bmesh(const Object &object,
                                            const Buffer &vbo,
                                            const int vert_len,
                                            const IndexMask &node_mask,
                                            const ScratchOffsets &scratch_offsets,
                                            DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  const BMesh &bm = *object.runtime->sculpt_session->bm;
  const int cd_offset = CustomData_get_offset_named(&bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(mask_format(), scratch_offsets.total);
  if (cd_offset != -1) {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          float *data = scratch_vbo->data<float>().data() + scratch_offsets.offsets[scratch_index];
          float *data_start = data;
          for (const BMFace *face :
               BKE_pbvh_bmesh_node_faces(&const_cast<bke::pbvh::BMeshNode &>(nodes[node_index])))
          {
            if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
              continue;
            }
            const BMLoop *l = face->l_first;
            *data = bmesh_cd_vert_get<float>(*l->prev->v, cd_offset);
            data++;
            *data = bmesh_cd_vert_get<float>(*l->v, cd_offset);
            data++;
            *data = bmesh_cd_vert_get<float>(*l->next->v, cd_offset);
            data++;
          }
          UNUSED_VARS_NDEBUG(data_start);
          BLI_assert(data == data_start + vbo.sizes[node_index]);
        },
        exec_mode::grain_size(1));
  }
  else {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
          scratch_vbo->data<float>()
              .slice(scratch_offsets.offsets[scratch_index], dst_len)
              .fill(0.0f);
        },
        exec_mode::grain_size(64));
  }
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_face_sets_bmesh(const Object &object,
                                                const OrigMeshData &orig_mesh_data,
                                                const Buffer &vbo,
                                                const int vert_len,
                                                const IndexMask &node_mask,
                                                const ScratchOffsets &scratch_offsets,
                                                DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  const BMesh &bm = *object.runtime->sculpt_session->bm;
  const int color_default = orig_mesh_data.face_set_default;
  const int color_seed = orig_mesh_data.face_set_seed;
  const int offset = CustomData_get_offset_named(&bm.pdata, CD_PROP_INT32, ".sculpt_face_set");
  if (node_mask.is_empty()) {
    return;
  }
  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(face_set_format(),
                                                          scratch_offsets.total);
  if (offset != -1) {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          uchar4 *data = scratch_vbo->data<uchar4>().data() +
                         scratch_offsets.offsets[scratch_index];
          uchar4 *data_start = data;
          for (const BMFace *face :
               BKE_pbvh_bmesh_node_faces(&const_cast<bke::pbvh::BMeshNode &>(nodes[node_index])))
          {
            if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
              continue;
            }
            uchar4 color{UCHAR_MAX};
            const int fset = bmesh_cd_face_get<int>(*face, offset);
            if (fset != color_default) {
              BKE_paint_face_set_overlay_color_get(fset, color_seed, color);
            }
            std::fill_n(data, 3, color);
            data += 3;
          }
          UNUSED_VARS_NDEBUG(data_start);
          BLI_assert(data == data_start + vbo.sizes[node_index]);
        },
        exec_mode::grain_size(1));
  }
  else {
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
          scratch_vbo->data<uchar4>()
              .slice(scratch_offsets.offsets[scratch_index], dst_len)
              .fill(uchar4(255));
        },
        exec_mode::grain_size(64));
  }
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

BLI_NOINLINE static void update_generic_attribute_bmesh(const Object &object,
                                                        const OrigMeshData &orig_mesh_data,
                                                        const Buffer &vbo,
                                                        const int vert_len,
                                                        const StringRef name,
                                                        const IndexMask &node_mask,
                                                        const ScratchOffsets &scratch_offsets,
                                                        DrawCacheImpl::AttributeData &attr_vbo)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
  const BMesh &bm = *object.runtime->sculpt_session->bm;
  const BMDataLayerLookup attr = BM_data_layer_lookup(bm, name);
  if (attr.domain == bke::AttrDomain::Edge) {
    return;
  }

  if (node_mask.is_empty()) {
    return;
  }

  const bke::AttrType attr_type = attr ? attr.type : bke::AttrType::Float3;
  const GPUVertFormat format = attribute_format(orig_mesh_data, name, attr_type);
  if (!attr) {
    gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(format, scratch_offsets.total);
    node_mask.foreach_index(
        [&](const int node_index, const int scratch_index) {
          const int64_t dst_len = vbo.acquired[node_index] ? vbo.sizes[node_index] : 0;
          scratch_vbo->data<float3>()
              .slice(scratch_offsets.offsets[scratch_index], dst_len)
              .fill(float3(0.0f));
        },
        exec_mode::grain_size(1));
    finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
    return;
  }

  gpu::VertBufPtr scratch_vbo = create_vbo_scratch_buffer(format, scratch_offsets.total);
  node_mask.foreach_index(
      [&](const int node_index, const int scratch_index) {
        const IndexRange scratch_range = vbo.scratch_range(node_index,
                                                           scratch_offsets.offsets[scratch_index]);
        bke::attribute_math::to_static_type(attr.type, [&]<typename T>() {
          const auto &faces = BKE_pbvh_bmesh_node_faces(
              &const_cast<bke::pbvh::BMeshNode &>(nodes[node_index]));
          if constexpr (!std::is_void_v<typename AttributeConverter<T>::VBOType>) {
            switch (attr.domain) {
              case bke::AttrDomain::Point:
                extract_data_vert_bmesh<T>(faces, attr.offset, *scratch_vbo, scratch_range);
                break;
              case bke::AttrDomain::Face:
                extract_data_face_bmesh<T>(faces, attr.offset, *scratch_vbo, scratch_range);
                break;
              case bke::AttrDomain::Corner:
                extract_data_corner_bmesh<T>(faces, attr.offset, *scratch_vbo, scratch_range);
                break;
              default:
                BLI_assert_unreachable();
            }
          }
        });
      },
      exec_mode::grain_size(1));
  finalize_vbo_update(attr_vbo, vbo, vert_len, node_mask, scratch_offsets, scratch_vbo);
}

static void create_lines_index_faces(const OffsetIndices<int> faces,
                                     const Span<bool> hide_poly,
                                     const Span<int> face_indices,
                                     const int64_t vbo_offset,
                                     const int64_t ibo_len,
                                     uint32_t *host_data)
{
  MutableSpan<uint2> data = MutableSpan<uint32_t>(host_data, ibo_len).cast<uint2>();

  int node_corner_offset = 0;
  int line_index = 0;
  for (const int face_index : face_indices) {
    const int face_size = faces[face_index].size();
    if (!hide_poly.is_empty() && hide_poly[face_index]) {
      node_corner_offset += face_size;
      continue;
    }
    for (const int i : IndexRange(face_size)) {
      const int next = (i == face_size - 1) ? 0 : i + 1;
      data[line_index] = uint2(i, next) + (node_corner_offset + int(vbo_offset));
      line_index++;
    }

    node_corner_offset += face_size;
  }
}

/* NOTE: This is a temporary solution. The index buffers are only needed to offset the vertices
 * into the shared (per-object) vertex buffers. Once multi draw indirect is used (each draw can
 * specify its own base vertex) these index buffers can be removed. */
static void create_tri_index_bmesh(const Set<BMFace *, 0> &faces,
                                   const int64_t vbo_offset,
                                   const int64_t ibo_len,
                                   uint32_t *host_data)
{
  MutableSpan<uint3> data = MutableSpan<uint32_t>(host_data, ibo_len).cast<uint3>();

  int tri_index = 0;
  int vert_index = vbo_offset;

  for (const BMFace *face : faces) {
    if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
      continue;
    }

    data[tri_index] = uint3(vert_index, vert_index + 1, vert_index + 2);
    tri_index++;

    vert_index += 3;
  }
}

/* NOTE: Temporary solution, see the note on #create_tri_index_bmesh. */
static void create_lines_index_bmesh(const Set<BMFace *, 0> &faces,
                                     const int64_t vbo_offset,
                                     const int64_t ibo_len,
                                     uint32_t *host_data)
{
  MutableSpan<uint2> data = MutableSpan<uint32_t>(host_data, ibo_len).cast<uint2>();

  int line_index = 0;
  int vert_index = vbo_offset;

  for (const BMFace *face : faces) {
    if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
      continue;
    }

    data[line_index] = uint2(vert_index, vert_index + 1);
    line_index++;
    data[line_index] = uint2(vert_index + 1, vert_index + 2);
    line_index++;
    data[line_index] = uint2(vert_index + 2, vert_index);
    line_index++;

    vert_index += 3;
  }
}

static int create_tri_index_grids(const Span<int> grid_indices,
                                  const BitGroupVector<> &grid_hidden,
                                  const int gridsize,
                                  const int skip,
                                  const int totgrid,
                                  const int64_t vbo_offset,
                                  MutableSpan<uint3> data)
{
  int tri_index = 0;
  int offset = vbo_offset;
  const int grid_vert_len = gridsize * gridsize;
  for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
    uint v0, v1, v2, v3;

    const BoundedBitSpan gh = grid_hidden.is_empty() ? BoundedBitSpan() :
                                                       grid_hidden[grid_indices[i]];

    for (int y = 0; y < gridsize - skip; y += skip) {
      for (int x = 0; x < gridsize - skip; x += skip) {
        /* Skip hidden grid face */
        if (!gh.is_empty() && BKE_paint_is_grid_face_hidden(gh, gridsize, x, y)) {
          continue;
        }
        /* Indices in a Clockwise QUAD disposition. */
        v0 = offset + CCG_grid_xy_to_index(gridsize, x, y);
        v1 = offset + CCG_grid_xy_to_index(gridsize, x + skip, y);
        v2 = offset + CCG_grid_xy_to_index(gridsize, x + skip, y + skip);
        v3 = offset + CCG_grid_xy_to_index(gridsize, x, y + skip);

        data[tri_index] = uint3(v0, v2, v1);
        tri_index++;
        data[tri_index] = uint3(v0, v3, v2);
        tri_index++;
      }
    }
  }

  return tri_index;
}

static int create_tri_index_grids_flat_layout(const Span<int> grid_indices,
                                              const BitGroupVector<> &grid_hidden,
                                              const int gridsize,
                                              const int skip,
                                              const int totgrid,
                                              const int64_t vbo_offset,
                                              MutableSpan<uint3> data)
{
  int tri_index = 0;
  int offset = vbo_offset;
  const int grid_vert_len = square_uint(gridsize - 1) * 4;
  for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
    const BoundedBitSpan gh = grid_hidden.is_empty() ? BoundedBitSpan() :
                                                       grid_hidden[grid_indices[i]];

    uint v0, v1, v2, v3;
    for (int y = 0; y < gridsize - skip; y += skip) {
      for (int x = 0; x < gridsize - skip; x += skip) {
        /* Skip hidden grid face */
        if (!gh.is_empty() && BKE_paint_is_grid_face_hidden(gh, gridsize, x, y)) {
          continue;
        }

        v0 = (y * (gridsize - 1) + x) * 4;

        if (skip > 1) {
          v1 = (y * (gridsize - 1) + x + skip - 1) * 4;
          v2 = ((y + skip - 1) * (gridsize - 1) + x + skip - 1) * 4;
          v3 = ((y + skip - 1) * (gridsize - 1) + x) * 4;
        }
        else {
          v1 = v2 = v3 = v0;
        }

        /* VBO data are in a Clockwise QUAD disposition.  Note
         * that vertices might be in different quads if we're
         * building a coarse index buffer.
         */
        v0 += offset;
        v1 += offset + 1;
        v2 += offset + 2;
        v3 += offset + 3;

        data[tri_index] = uint3(v0, v2, v1);
        tri_index++;
        data[tri_index] = uint3(v0, v3, v2);
        tri_index++;
      }
    }
  }
  return tri_index;
}

static void create_lines_index_grids(const Span<int> grid_indices,
                                     int display_gridsize,
                                     const BitGroupVector<> &grid_hidden,
                                     const int gridsize,
                                     const int skip,
                                     const int totgrid,
                                     const int64_t vbo_offset,
                                     MutableSpan<uint2> data)
{
  int line_index = 0;
  int offset = vbo_offset;
  const int grid_vert_len = gridsize * gridsize;
  for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
    uint v0, v1, v2, v3;
    bool grid_visible = false;

    const BoundedBitSpan gh = grid_hidden.is_empty() ? BoundedBitSpan() :
                                                       grid_hidden[grid_indices[i]];

    for (int y = 0; y < gridsize - skip; y += skip) {
      for (int x = 0; x < gridsize - skip; x += skip) {
        /* Skip hidden grid face */
        if (!gh.is_empty() && BKE_paint_is_grid_face_hidden(gh, gridsize, x, y)) {
          continue;
        }
        /* Indices in a Clockwise QUAD disposition. */
        v0 = offset + CCG_grid_xy_to_index(gridsize, x, y);
        v1 = offset + CCG_grid_xy_to_index(gridsize, x + skip, y);
        v2 = offset + CCG_grid_xy_to_index(gridsize, x + skip, y + skip);
        v3 = offset + CCG_grid_xy_to_index(gridsize, x, y + skip);

        data[line_index] = uint2(v0, v1);
        line_index++;
        data[line_index] = uint2(v0, v3);
        line_index++;

        if (y / skip + 2 == display_gridsize) {
          data[line_index] = uint2(v2, v3);
          line_index++;
        }
        grid_visible = true;
      }

      if (grid_visible) {
        data[line_index] = uint2(v1, v2);
        line_index++;
      }
    }
  }
}

static void create_lines_index_grids_flat_layout(const Span<int> grid_indices,
                                                 int display_gridsize,
                                                 const BitGroupVector<> &grid_hidden,
                                                 const int gridsize,
                                                 const int skip,
                                                 const int totgrid,
                                                 const int64_t vbo_offset,
                                                 MutableSpan<uint2> data)
{
  int line_index = 0;
  int offset = vbo_offset;
  const int grid_vert_len = square_uint(gridsize - 1) * 4;
  for (int i = 0; i < totgrid; i++, offset += grid_vert_len) {
    bool grid_visible = false;
    const BoundedBitSpan gh = grid_hidden.is_empty() ? BoundedBitSpan() :
                                                       grid_hidden[grid_indices[i]];

    uint v0, v1, v2, v3;
    for (int y = 0; y < gridsize - skip; y += skip) {
      for (int x = 0; x < gridsize - skip; x += skip) {
        /* Skip hidden grid face */
        if (!gh.is_empty() && BKE_paint_is_grid_face_hidden(gh, gridsize, x, y)) {
          continue;
        }

        v0 = (y * (gridsize - 1) + x) * 4;

        if (skip > 1) {
          v1 = (y * (gridsize - 1) + x + skip - 1) * 4;
          v2 = ((y + skip - 1) * (gridsize - 1) + x + skip - 1) * 4;
          v3 = ((y + skip - 1) * (gridsize - 1) + x) * 4;
        }
        else {
          v1 = v2 = v3 = v0;
        }

        /* VBO data are in a Clockwise QUAD disposition.  Note
         * that vertices might be in different quads if we're
         * building a coarse index buffer.
         */
        v0 += offset;
        v1 += offset + 1;
        v2 += offset + 2;
        v3 += offset + 3;

        data[line_index] = uint2(v0, v1);
        line_index++;
        data[line_index] = uint2(v0, v3);
        line_index++;

        if (y / skip + 2 == display_gridsize) {
          data[line_index] = uint2(v2, v3);
          line_index++;
        }
        grid_visible = true;
      }

      if (grid_visible) {
        data[line_index] = uint2(v1, v2);
        line_index++;
      }
    }
  }
}

static Array<int> calc_material_indices(const Object &object, const OrigMeshData &orig_mesh_data)
{
  const SculptSession &ss = *object.runtime->sculpt_session;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
      const bke::AttributeAccessor attributes = mesh.attributes();
      const VArray material_indices = *attributes.lookup<int>("material_index",
                                                              bke::AttrDomain::Face);
      if (!material_indices) {
        return {};
      }
      Array<int> node_materials(nodes.size());
      threading::parallel_for(nodes.index_range(), 64, [&](const IndexRange range) {
        for (const int i : range) {
          const Span<int> face_indices = nodes[i].faces();
          if (face_indices.is_empty()) {
            continue;
          }
          node_materials[i] = material_indices[face_indices.first()];
        }
      });
      return node_materials;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      /* Use original mesh data because evaluated mesh is empty. */
      const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
      const VArray material_indices = *attributes.lookup<int>("material_index",
                                                              bke::AttrDomain::Face);
      if (!material_indices) {
        return {};
      }
      Array<int> node_materials(nodes.size());
      const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
      const Span<int> grid_faces = subdiv_ccg.grid_to_face_map;
      threading::parallel_for(nodes.index_range(), 64, [&](const IndexRange range) {
        for (const int i : range) {
          const Span<int> grids = nodes[i].grids();
          if (grids.is_empty()) {
            continue;
          }
          node_materials[i] = material_indices[grid_faces[grids.first()]];
        }
      });
      return node_materials;
    }
    case bke::pbvh::Type::BMesh:
      return {};
  }
  BLI_assert_unreachable();
  return {};
}

static BitVector<> calc_use_flat_layout(const Object &object, const OrigMeshData &orig_mesh_data)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      /* NOTE: Theoretically it would be possible to used vertex indexed buffers if there are no
       * face corner attributes, sharp faces, or face sets. */
      return {};
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
      const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
      if (sharp_faces.is_empty()) {
        return BitVector<>(nodes.size(), false);
      }

      const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
      const Span<int> grid_to_face_map = subdiv_ccg.grid_to_face_map;

      /* Use boolean array instead of #BitVector for parallelized writing. */
      Array<bool> use_flat_layout(nodes.size());
      threading::parallel_for(nodes.index_range(), 4, [&](const IndexRange range) {
        for (const int i : range) {
          const Span<int> grids = nodes[i].grids();
          if (grids.is_empty()) {
            continue;
          }
          use_flat_layout[i] = std::any_of(grids.begin(), grids.end(), [&](const int grid) {
            return sharp_faces[grid_to_face_map[grid]];
          });
        }
      });
      return BitVector<>(use_flat_layout);
    }
    case bke::pbvh::Type::BMesh:
      return {};
  }
  BLI_assert_unreachable();
  return {};
}

static void create_tri_index_mesh(const OffsetIndices<int> faces,
                                  const Span<int3> corner_tris,
                                  const Span<bool> hide_poly,
                                  const bke::pbvh::MeshNode &node,
                                  const int64_t vbo_offset,
                                  const int64_t ibo_len,
                                  uint32_t *host_data)
{
  const Span<int> face_indices = node.faces();

  MutableSpan<uint3> data = MutableSpan<uint32_t>(host_data, ibo_len).cast<uint3>();

  int tri_index = 0;
  int node_corner_offset = 0;
  for (const int face_index : face_indices) {
    const IndexRange face = faces[face_index];
    if (!hide_poly.is_empty() && hide_poly[face_index]) {
      node_corner_offset += face.size();
      continue;
    }
    for (const int3 &tri : corner_tris.slice(bke::mesh::face_triangles_range(faces, face_index))) {
      for (int i : IndexRange(3)) {
        const int corner = tri[i];
        const int index_in_face = corner - face.first();
        data[tri_index][i] = node_corner_offset + index_in_face + int(vbo_offset);
      }
      tri_index++;
    }
    node_corner_offset += face.size();
  }
}

static void create_tri_index_grids(const CCGKey &key,
                                   const BitGroupVector<> &grid_hidden,
                                   const bool do_coarse,
                                   const Span<int> grid_indices,
                                   const bool use_flat_layout,
                                   const int64_t vbo_offset,
                                   const int64_t ibo_len,
                                   uint32_t *host_data)
{
  int gridsize = key.grid_size;
  int display_gridsize = gridsize;
  int totgrid = grid_indices.size();
  int skip = 1;

  const int display_level = do_coarse ? 0 : key.level;

  if (display_level < key.level) {
    display_gridsize = (1 << display_level) + 1;
    skip = 1 << (key.level - display_level - 1);
  }

  uint visible_quad_len = bke::pbvh::count_grid_quads(
      grid_hidden, grid_indices, key.grid_size, display_gridsize);
  UNUSED_VARS_NDEBUG(visible_quad_len);
  BLI_assert(uint(ibo_len) == 2 * visible_quad_len * 3);

  MutableSpan<uint3> data = MutableSpan<uint32_t>(host_data, ibo_len).cast<uint3>();
  data.fill(uint3(0));

  int tri_count;
  if (use_flat_layout) {
    tri_count = create_tri_index_grids_flat_layout(
        grid_indices, grid_hidden, gridsize, skip, totgrid, vbo_offset, data);
  }
  else {
    tri_count = create_tri_index_grids(
        grid_indices, grid_hidden, gridsize, skip, totgrid, vbo_offset, data);
  }
  BLI_assert(tri_count * 3 <= uint(ibo_len));
  UNUSED_VARS(tri_count);
}

static void create_lines_index_grids(const CCGKey &key,
                                     const BitGroupVector<> &grid_hidden,
                                     const bool do_coarse,
                                     const Span<int> grid_indices,
                                     const bool use_flat_layout,
                                     const int64_t vbo_offset,
                                     const int64_t ibo_len,
                                     uint32_t *host_data)
{
  int gridsize = key.grid_size;
  int display_gridsize = gridsize;
  int totgrid = grid_indices.size();
  int skip = 1;

  const int display_level = do_coarse ? 0 : key.level;

  if (display_level < key.level) {
    display_gridsize = (1 << display_level) + 1;
    skip = 1 << (key.level - display_level - 1);
  }

  const int quads_per_axis = (gridsize - 1) / skip;
  UNUSED_VARS_NDEBUG(quads_per_axis);
  BLI_assert(uint(ibo_len) == 4 * totgrid * quads_per_axis * (quads_per_axis + 1));
  MutableSpan<uint2> data = MutableSpan<uint32_t>(host_data, ibo_len).cast<uint2>();
  /* The buffer might contain hidden elements which are not initialized but still accounted. We
   * don't count them to skip from allocation, so must fill that gaps by 0 to hide redundant edges.
   */
  data.fill(uint2(0));

  if (use_flat_layout) {
    create_lines_index_grids_flat_layout(
        grid_indices, display_gridsize, grid_hidden, gridsize, skip, totgrid, vbo_offset, data);
  }
  else {
    create_lines_index_grids(
        grid_indices, display_gridsize, grid_hidden, gridsize, skip, totgrid, vbo_offset, data);
  }
}

Span<gpu::IndexBufPtr> DrawCacheImpl::ensure_lines_indices(const Object &object,
                                                           const OrigMeshData &orig_mesh_data,
                                                           const IndexMask &node_mask,
                                                           const bool coarse)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IBOFamily &ibo_family = coarse ? ibo_lines_coarse_ : ibo_lines_;
  ibo_family.ensure_capacity();
  const Buffer &ibo_buffer = ibo_family.buffer;
  Vector<gpu::IndexBufPtr> &ibos = ibo_family.ibos;
  ibos.resize(pbvh.nodes_num());

  /* Whenever a node's visible triangle count has changed the index buffers are freed, so we only
   * recalculate null IBOs here. A new mask is recalculated for more even task distribution between
   * threads. */
  IndexMaskMemory memory;
  const IndexMask nodes_to_calculate = IndexMask::from_predicate(
      node_mask, memory, [&](const int node_index) {
        return !ibos[node_index] && ibo_buffer.acquired[node_index];
      });

  /* Build scratch offsets for the dirty node family, and the scratch buffer to stage into. */
  const ScratchOffsets scratch_offsets = build_scratch_offsets(ibo_buffer, nodes_to_calculate);
  Array<uint32_t> scratch_buffer(uint32_t(scratch_offsets.total));

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
      const OffsetIndices<int> faces = mesh.faces();
      const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
      const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      nodes_to_calculate.foreach_index(
          [&](const int node_index, const int scratch_index) {
            create_lines_index_faces(faces,
                                     hide_poly,
                                     nodes[node_index].faces(),
                                     vbo_.offsets[node_index],
                                     ibo_buffer.sizes[node_index],
                                     scratch_buffer.data() +
                                         scratch_offsets.offsets[scratch_index]);
          },
          exec_mode::grain_size(1));
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      nodes_to_calculate.foreach_index(
          [&](const int node_index, const int scratch_index) {
            create_lines_index_grids(key,
                                     subdiv_ccg.grid_hidden,
                                     coarse,
                                     nodes[node_index].grids(),
                                     use_flat_layout_[node_index],
                                     vbo_.offsets[node_index],
                                     ibo_buffer.sizes[node_index],
                                     scratch_buffer.data() +
                                         scratch_offsets.offsets[scratch_index]);
          },
          exec_mode::grain_size(1));
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      nodes_to_calculate.foreach_index(
          [&](const int node_index, const int scratch_index) {
            const Set<BMFace *, 0> &faces = BKE_pbvh_bmesh_node_faces(
                &const_cast<bke::pbvh::BMeshNode &>(nodes[node_index]));
            create_lines_index_bmesh(faces,
                                     vbo_.offsets[node_index],
                                     ibo_buffer.sizes[node_index],
                                     scratch_buffer.data() +
                                         scratch_offsets.offsets[scratch_index]);
          },
          exec_mode::grain_size(1));
      break;
    }
  }

  ibo_family.commit_subranges(nodes_to_calculate, scratch_offsets, scratch_buffer);
  return ibos;
}

BitSpan DrawCacheImpl::ensure_use_flat_layout(const Object &object,
                                              const OrigMeshData &orig_mesh_data)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (use_flat_layout_.size() != pbvh.nodes_num()) {
    use_flat_layout_ = calc_use_flat_layout(object, orig_mesh_data);
  }
  return use_flat_layout_;
}

DrawCacheImpl::AttributeData &DrawCacheImpl::ensure_attribute_data(
    const Object &object,
    const OrigMeshData &orig_mesh_data,
    const AttributeRequest &attr,
    const IndexMask &node_mask)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  AttributeData &attr_vbo = attribute_vbos_.lookup_or_add_default(attr);
  attr_vbo.ensure_nodes_capacity(node_mask.min_array_size());

  /* Compute mask that needs to be recomputed. */
  IndexMaskMemory memory;
  const IndexMask dirty_mask = IndexMask::from_bits(
      node_mask.slice_content(attr_vbo.dirty_nodes.index_range()), attr_vbo.dirty_nodes, memory);

  const ScratchOffsets scratch_offsets = (attr_vbo.vbo == nullptr) ?
                                             build_vbo_initial_offsets(vbo_, dirty_mask) :
                                             build_scratch_offsets(vbo_, dirty_mask);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      if (const CustomRequest *request_type = std::get_if<CustomRequest>(&attr)) {
        switch (*request_type) {
          case CustomRequest::Position:
            update_positions_mesh(
                object, vbo_, vbo_.space.size(), dirty_mask, scratch_offsets, attr_vbo);
            break;
          case CustomRequest::Normal:
            update_normals_mesh(
                object, vbo_, vbo_.space.size(), dirty_mask, scratch_offsets, attr_vbo);
            break;
          case CustomRequest::Mask:
            update_masks_mesh(object,
                              orig_mesh_data,
                              vbo_,
                              vbo_.space.size(),
                              dirty_mask,
                              scratch_offsets,
                              attr_vbo);
            break;
          case CustomRequest::FaceSet:
            update_face_sets_mesh(object,
                                  orig_mesh_data,
                                  vbo_,
                                  vbo_.space.size(),
                                  dirty_mask,
                                  scratch_offsets,
                                  attr_vbo);
            break;
        }
      }
      else {
        update_generic_attribute_mesh(object,
                                      orig_mesh_data,
                                      vbo_,
                                      vbo_.space.size(),
                                      std::get<GenericRequest>(attr),
                                      dirty_mask,
                                      scratch_offsets,
                                      attr_vbo);
      }
      break;
    }
    case bke::pbvh::Type::Grids: {
      if (const CustomRequest *request_type = std::get_if<CustomRequest>(&attr)) {
        switch (*request_type) {
          case CustomRequest::Position:
            update_positions_grids(object,
                                   vbo_,
                                   vbo_.space.size(),
                                   use_flat_layout_,
                                   dirty_mask,
                                   scratch_offsets,
                                   attr_vbo);
            break;
          case CustomRequest::Normal:
            update_normals_grids(object,
                                 orig_mesh_data,
                                 vbo_,
                                 vbo_.space.size(),
                                 use_flat_layout_,
                                 dirty_mask,
                                 scratch_offsets,
                                 attr_vbo);
            break;
          case CustomRequest::Mask:
            update_masks_grids(object,
                               vbo_,
                               vbo_.space.size(),
                               use_flat_layout_,
                               dirty_mask,
                               scratch_offsets,
                               attr_vbo);
            break;
          case CustomRequest::FaceSet:
            update_face_sets_grids(object,
                                   orig_mesh_data,
                                   vbo_,
                                   vbo_.space.size(),
                                   use_flat_layout_,
                                   dirty_mask,
                                   scratch_offsets,
                                   attr_vbo);
            break;
        }
      }
      else {
        update_generic_attribute_grids(object,
                                       orig_mesh_data,
                                       vbo_,
                                       vbo_.space.size(),
                                       std::get<GenericRequest>(attr),
                                       use_flat_layout_,
                                       dirty_mask,
                                       scratch_offsets,
                                       attr_vbo);
      }
      break;
    }
    case bke::pbvh::Type::BMesh: {
      if (const CustomRequest *request_type = std::get_if<CustomRequest>(&attr)) {
        switch (*request_type) {
          case CustomRequest::Position:
            update_positions_bmesh(
                object, vbo_, vbo_.space.size(), dirty_mask, scratch_offsets, attr_vbo);
            break;
          case CustomRequest::Normal:
            update_normals_bmesh(
                object, vbo_, vbo_.space.size(), dirty_mask, scratch_offsets, attr_vbo);
            break;
          case CustomRequest::Mask:
            update_masks_bmesh(
                object, vbo_, vbo_.space.size(), dirty_mask, scratch_offsets, attr_vbo);
            break;
          case CustomRequest::FaceSet:
            update_face_sets_bmesh(object,
                                   orig_mesh_data,
                                   vbo_,
                                   vbo_.space.size(),
                                   dirty_mask,
                                   scratch_offsets,
                                   attr_vbo);
            break;
        }
      }
      else {
        update_generic_attribute_bmesh(object,
                                       orig_mesh_data,
                                       vbo_,
                                       vbo_.space.size(),
                                       std::get<GenericRequest>(attr),
                                       dirty_mask,
                                       scratch_offsets,
                                       attr_vbo);
      }
      break;
    }
  }

  /* TODO: It would be good to deallocate the bit vector if all of the bits have been reset to
   * avoid unnecessary processing in subsequent redraws. */
  dirty_mask.foreach_index_optimized<int>(
      [&](const int node_index) { attr_vbo.dirty_nodes[node_index].reset(); });

  return attr_vbo;
}

Span<gpu::IndexBufPtr> DrawCacheImpl::ensure_tri_indices(const Object &object,
                                                         const OrigMeshData &orig_mesh_data,
                                                         const IndexMask &node_mask,
                                                         const bool coarse)
{
  PRF_scope(ProfileCategory::Draw);
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  IBOFamily &ibo_family = coarse ? ibo_tris_coarse_ : ibo_tris_;
  ibo_family.ensure_capacity();
  const Buffer &ibo_buffer = ibo_family.buffer;
  Vector<gpu::IndexBufPtr> &ibos = ibo_family.ibos;
  ibos.resize(pbvh.nodes_num());

  /* Whenever a node's visible triangle count has changed the index buffers are freed, so we only
   * recalculate null IBOs here. A new mask is recalculated for more even task distribution between
   * threads. */
  IndexMaskMemory memory;
  const IndexMask nodes_to_calculate = IndexMask::from_predicate(
      node_mask, memory, [&](const int node_index) {
        return !ibos[node_index] && ibo_buffer.acquired[node_index];
      });

  /* Build scratch offsets for the dirty node family, and the scratch buffer to stage into. */
  const ScratchOffsets scratch_offsets = build_scratch_offsets(ibo_buffer, nodes_to_calculate);
  Array<uint32_t> scratch_buffer(uint32_t(scratch_offsets.total));

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
      const OffsetIndices<int> faces = mesh.faces();
      const Span<int3> corner_tris = mesh.corner_tris();
      const bke::AttributeAccessor attributes = orig_mesh_data.attributes;
      const VArraySpan hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
      nodes_to_calculate.foreach_index(
          [&](const int node_index, const int scratch_index) {
            create_tri_index_mesh(faces,
                                  corner_tris,
                                  hide_poly,
                                  nodes[node_index],
                                  vbo_.offsets[node_index],
                                  ibo_buffer.sizes[node_index],
                                  scratch_buffer.data() + scratch_offsets.offsets[scratch_index]);
          },
          exec_mode::grain_size(1));
      break;
    }
    case bke::pbvh::Type::Grids: {
      /* Unlike the other geometry types, multires grids use indexed vertex buffers because when
       * there are no flat faces, vertices can be shared between neighboring quads. This results in
       * a 4x decrease in the amount of data uploaded. Theoretically it also means freeing VBOs
       * because of visibility changes is unnecessary.
       *
       * TODO: With the "flat layout" and no hidden faces, the index buffers are unnecessary, we
       * should avoid creating them in that case. */
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      const SubdivCCG &subdiv_ccg = *object.runtime->sculpt_session->subdiv_ccg;
      const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
      nodes_to_calculate.foreach_index(
          [&](const int node_index, const int scratch_index) {
            create_tri_index_grids(key,
                                   subdiv_ccg.grid_hidden,
                                   coarse,
                                   nodes[node_index].grids(),
                                   use_flat_layout_[node_index],
                                   vbo_.offsets[node_index],
                                   ibo_buffer.sizes[node_index],
                                   scratch_buffer.data() + scratch_offsets.offsets[scratch_index]);
          },
          exec_mode::grain_size(1));
      break;
    }
    case bke::pbvh::Type::BMesh: {
      const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
      nodes_to_calculate.foreach_index(
          [&](const int node_index, const int scratch_index) {
            const Set<BMFace *, 0> &faces = BKE_pbvh_bmesh_node_faces(
                &const_cast<bke::pbvh::BMeshNode &>(nodes[node_index]));
            create_tri_index_bmesh(faces,
                                   vbo_.offsets[node_index],
                                   ibo_buffer.sizes[node_index],
                                   scratch_buffer.data() + scratch_offsets.offsets[scratch_index]);
          },
          exec_mode::grain_size(1));
      break;
    }
  }

  ibo_family.commit_subranges(nodes_to_calculate, scratch_offsets, scratch_buffer);
  return ibos;
}

void DrawCacheImpl::IBOFamily::commit_subranges(const IndexMask &node_mask,
                                                const ScratchOffsets &scratch_offsets,
                                                const Span<uint32_t> scratch_buffer)
{
  node_mask.foreach_index([&](const int node_index, const int scratch_index) {
    const IndexRange dst_range = buffer.index_range(node_index);
    update_ibos_subrange(
        *ibo, dst_range, scratch_buffer.data() + scratch_offsets.offsets[scratch_index]);
    ibos[node_index] = gpu::IndexBufPtr(
        GPU_indexbuf_create_subrange(ibo.get(), uint(dst_range.start()), uint(dst_range.size())));
  });
}

void DrawCacheImpl::IBOFamily::ensure_capacity()
{
  const uint32_t needed_len = uint32_t(buffer.space.size());
  if (ibo && ibo->index_len_get() >= needed_len) {
    return;
  }

  /* Create new buffer. */
  gpu::IndexBufPtr new_ibo = gpu::IndexBufPtr(
      GPU_indexbuf_build_on_device(std::max(needed_len, 1u)));
  GPU_indexbuf_use(new_ibo.get());
  /* Copy over data from previous buffer. */
  if (ibo) {
    new_ibo->copy_sub(*ibo, 0, 0, ibo->index_len_get());
    /* Rebuild all subranges. */
    for (gpu::IndexBufPtr &ibo_sub : ibos) {
      if (ibo_sub) {
        gpu::IndexBufPtr new_sub = gpu::IndexBufPtr(GPU_indexbuf_create_subrange(
            new_ibo.get(), ibo_sub->index_start_get(), ibo_sub->index_len_get()));
        ibo_sub.swap(new_sub);
      }
    }
  }
  ibo.swap(new_ibo);
}

Span<Batch *> DrawCacheImpl::ensure_tris_batches(const Object &object,
                                                 const ViewportRequest &request,
                                                 const IndexMask &nodes_to_update)
{
  PRF_scope(ProfileCategory::Draw);
  const Object &object_orig = *DEG_get_original(&object);
  const OrigMeshData orig_mesh_data{*id_cast<const Mesh *>(object_orig.data)};
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const bool coarse = use_coarse_buffers(pbvh, request);

  IBOFamily &ibo_family = coarse ? ibo_tris_coarse_ : ibo_tris_;
  bool grown = this->ensure_vert_ranges(object, orig_mesh_data, nodes_to_update);
  grown |= ibo_family.ensure_ranges(
      pbvh.nodes_num(), nodes_to_update, [&](const IndexMask &mask, MutableSpan<int> sizes) {
        calc_tris_index_counts(object, orig_mesh_data, coarse, mask, sizes);
      });
  if (grown) {
    this->tag_batches_dirty();
  }

  this->free_nodes_with_changed_topology(pbvh);

  const Span<gpu::IndexBufPtr> ibos = this->ensure_tri_indices(
      object, orig_mesh_data, nodes_to_update, coarse);

  for (const AttributeRequest &attr : request.attributes) {
    this->ensure_attribute_data(object, orig_mesh_data, attr, nodes_to_update);
  }

  /* Collect VBO spans in a different loop because #ensure_attribute_data invalidates the allocated
   * arrays when its map is changed. */
  Vector<gpu::VertBuf *> attr_vbos;
  for (const AttributeRequest &attr : request.attributes) {
    if (const AttributeData *attr_data = attribute_vbos_.lookup_ptr(attr)) {
      attr_vbos.append(attr_data->vbo.get());
    }
  }

  /* Except for the first iteration of the draw loop, we only need to rebuild batches for nodes
   * with changed topology (visible triangle count). Rebuilds reconfigure the batch in place (the
   * passes that draw them cache the raw batch pointers until submit, so freeing them mid-frame
   * would leave those passes with dangling pointers). */
  BatchCache &batch_cache = tris_batches_.lookup_or_add_default(request);
  Vector<Batch *> &batches = batch_cache.batches;
  batches.resize(pbvh.nodes_num(), nullptr);
  batch_cache.dirty.resize(pbvh.nodes_num(), true);
  nodes_to_update.foreach_index(
      [&](const int node_index) {
        const bool has_ibo = !ibos.is_empty() && ibos[node_index] != nullptr;
        if (!batches[node_index] || batch_cache.dirty[node_index] ||
            batches[node_index]->gpu_batch()->elem_() !=
                (has_ibo ? ibos[node_index].get() : nullptr))
        {
          if (!has_ibo) {
            /* No geometry for this node. */
            return;
          }
          if (!batches[node_index]) {
            batches[node_index] = MEM_new<Batch>(__func__, GPU_batch_calloc());
          }
          Batch::clear(batches[node_index]);
          GPU_batch_init(
              batches[node_index]->gpu_batch(), GPU_PRIM_TRIS, nullptr, ibos[node_index].get());
          for (gpu::VertBuf *vbo : attr_vbos) {
            GPU_batch_vertbuf_add(batches[node_index]->gpu_batch(), vbo, false);
          }
        }
      },
      exec_mode::grain_size(64));
  /* Clear the tags in a separate serial loop, since neighboring bits share the same word. */
  nodes_to_update.foreach_index([&](const int node_index) {
    if (!ibos.is_empty() && ibos[node_index]) {
      batch_cache.dirty[node_index].reset();
    }
  });

  return batches;
}

Span<Batch *> DrawCacheImpl::ensure_lines_batches(const Object &object,
                                                  const ViewportRequest &request,
                                                  const IndexMask &nodes_to_update)
{
  PRF_scope(ProfileCategory::Draw);
  const Object &object_orig = *DEG_get_original(&object);
  const OrigMeshData orig_mesh_data(*id_cast<const Mesh *>(object_orig.data));
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const bool coarse = use_coarse_buffers(pbvh, request);

  IBOFamily &ibo_family = coarse ? ibo_lines_coarse_ : ibo_lines_;
  bool grown = this->ensure_vert_ranges(object, orig_mesh_data, nodes_to_update);
  grown |= ibo_family.ensure_ranges(
      pbvh.nodes_num(), nodes_to_update, [&](const IndexMask &mask, MutableSpan<int> sizes) {
        calc_lines_index_counts(object, orig_mesh_data, coarse, mask, sizes);
      });
  if (grown) {
    this->tag_batches_dirty();
  }

  this->free_nodes_with_changed_topology(pbvh);

  const AttributeData &position = this->ensure_attribute_data(
      object, orig_mesh_data, CustomRequest::Position, nodes_to_update);
  const Span<gpu::IndexBufPtr> lines = this->ensure_lines_indices(
      object, orig_mesh_data, nodes_to_update, coarse);

  /* See the note on #ensure_tris_batches for why the batches are reconfigured in place. */
  BatchCache &batch_cache = coarse ? lines_batches_coarse_ : lines_batches_;
  Vector<Batch *> &batches = batch_cache.batches;
  batches.resize(pbvh.nodes_num(), nullptr);
  batch_cache.dirty.resize(pbvh.nodes_num(), true);
  nodes_to_update.foreach_index(
      [&](const int node_index) {
        if (lines[node_index] == nullptr) {
          return;
        }
        if (!batches[node_index] || batch_cache.dirty[node_index] ||
            batches[node_index]->gpu_batch()->elem_() != lines[node_index].get())
        {
          if (!batches[node_index]) {
            batches[node_index] = MEM_new<Batch>(__func__, GPU_batch_calloc());
          }
          Batch::clear(batches[node_index]);
          GPU_batch_init(
              batches[node_index]->gpu_batch(), GPU_PRIM_LINES, nullptr, lines[node_index].get());
          GPU_batch_vertbuf_add(batches[node_index]->gpu_batch(), position.vbo.get(), false);
        }
      },
      exec_mode::grain_size(64));
  /* Clear the tags in a separate serial loop, since neighboring bits share the same word. */
  nodes_to_update.foreach_index([&](const int node_index) {
    if (lines[node_index]) {
      batch_cache.dirty[node_index].reset();
    }
  });

  return batches;
}

Span<int> DrawCacheImpl::ensure_material_indices(const Object &object)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const Object &object_orig = *DEG_get_original(&object);
  const OrigMeshData orig_mesh_data(*id_cast<const Mesh *>(object_orig.data));
  if (orig_mesh_data.attributes.contains("material_index") &&
      material_indices_.size() != pbvh.nodes_num())
  {
    material_indices_ = calc_material_indices(object, orig_mesh_data);
  }
  return material_indices_;
}

bool DrawCacheImpl::IBOFamily::ensure_ranges(
    const int nodes_num,
    const IndexMask &node_mask,
    const FunctionRef<void(const IndexMask &, MutableSpan<int>)> calc_sizes)
{
  PRF_scope(ProfileCategory::Draw);
  BitVector<> moved;
  const bool grown = buffer.ensure_ranges(nodes_num, node_mask, calc_sizes, moved);
  /* A moved sub-range only invalidates this family's own index buffers. The other families are
   * deliberately left alone: another draw engine may have retrieved their batches earlier in this
   * same frame, and freeing an index buffer out from under one of those leaves the pass that
   * cached the batch pointer referencing freed memory. */
  IndexMaskMemory memory;
  free_ibos(ibos, IndexMask::from_bits(moved, memory));
  return grown;
}

bool DrawCacheImpl::ensure_vert_ranges(const Object &object,
                                       const OrigMeshData &orig_mesh_data,
                                       const IndexMask &node_mask)
{
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  const BitSpan use_flat_layout = this->ensure_use_flat_layout(object, orig_mesh_data);
  /* Every index buffer stores offsets into the shared vertex buffer, and for BMesh the attribute
   * values are stored per visible triangle, so a moved vertex range invalidates all of them. That
   * is recorded globally in #dirty_topology_. This only affects the first request in the drawing
   * of a frame; the nodes a later request could still move are the ones it does not share with the
   * earlier request, which have not been given to a draw engine. */
  return vbo_.ensure_ranges(
      pbvh.nodes_num(),
      node_mask,
      [&](const IndexMask &mask, MutableSpan<int> sizes) {
        calc_vert_counts(object, use_flat_layout, mask, sizes);
      },
      dirty_topology_);
}

void DrawCacheImpl::tag_batches_dirty()
{
  /* Batches reference the shared buffers with raw pointers, so a reallocation invalidates all of
   * them, including the batches of viewports that aren't drawing right now. */
  for (BatchCache &batch_cache : tris_batches_.values()) {
    batch_cache.dirty.fill(true);
  }
  lines_batches_.dirty.fill(true);
  lines_batches_coarse_.dirty.fill(true);
}

}  // namespace draw::pbvh
}  // namespace blender
