/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode undo system.
 *
 * Usage Guide
 * ===========
 *
 * The sculpt undo system is a delta-based system. Each undo step stores the difference with the
 * prior one.
 *
 * To use the sculpt undo system, you must call #push_begin inside an operator exec or invoke
 * callback (#geometry_begin may be called if you wish to save a non-delta copy of the entire
 * mesh). This will initialize the sculpt undo stack and set up an undo step.
 *
 * At the end of the operator you should call #push_end.
 *
 * #push_begin and #geometry_begin both take a #wmOperatorType as an argument. There are _ex
 * versions that allow a custom name; try to avoid using them. These can break the redo panel since
 * it requires the undo push to have the same name as the calling operator.
 */
#include "sculpt_undo.hh"

#include <mutex>
#include <zstd.h>

#include "CLG_log.h"

#include "BLI_array.hh"
#include "BLI_bit_group_vector.hh"
#include "BLI_compression.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_memory_counter.hh"
#include "BLI_string_utf8.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_ccg.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_scene.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_undo_system.hh"

/* TODO(sergey): Ideally should be no direct call to such low level things. */
#include "BKE_subdiv_eval.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_geometry.hh"
#include "ED_object.hh"
#include "ED_sculpt.hh"
#include "ED_undo.hh"

#include "bmesh.hh"
#include "mesh_brush_common.hh"
#include "paint_hide.hh"
#include "sculpt_color.hh"
#include "sculpt_dyntopo.hh"
#include "sculpt_face_set.hh"
#include "sculpt_intern.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

static CLG_LogRef LOG = {"undo.sculpt"};

namespace blender::ed::sculpt_paint::undo {

/* Implementation of undo system for objects in sculpt mode.
 *
 * Each undo step in sculpt mode consists of list of nodes, each node contains a flat array of data
 * related to the step type.
 *
 * Node type used for undo depends on specific operation and active sculpt mode ("regular" or
 * dynamic topology).
 *
 * Regular sculpt brushes will use Position, HideVert, HideFace, Mask, Face Set * nodes. These
 * nodes are created for every BVH node which is affected by the brush. The undo push for the node
 * happens BEFORE modifications. This makes the operation undo to work in the following way: for
 * every node in the undo step swap happens between node in the undo stack and the corresponding
 * value in the BVH. This is how redo is possible after undo.
 *
 * The COORDS, HIDDEN or MASK type of nodes contains arrays of the corresponding values.
 *
 * Operations like Symmetrize are using GEOMETRY type of nodes which pushes the entire state of the
 * mesh to the undo stack. This node contains all CustomData layers.
 *
 * The tricky aspect of this undo node type is that it stores mesh before and after modification.
 * This allows the undo system to both undo and redo the symmetrize operation within the
 * pre-modified-push of other node type behavior, but it uses more memory that it seems it should
 * be.
 *
 * The dynamic topology undo nodes are handled somewhat separately from all other ones and the idea
 * there is to store log of operations: which vertices and faces have been added or removed.
 *
 * Begin of dynamic topology sculpting mode have own node type. It contains an entire copy of mesh
 * since just enabling the dynamic topology mode already does modifications on it.
 *
 * End of dynamic topology and symmetrize in this mode are handled in a special manner as well. */

#define NO_ACTIVE_LAYER bke::AttrDomain::Auto

struct Node {
  Array<float3, 0> position;
  Array<float3, 0> orig_position;
  Array<float3, 0> normal;
  Array<float4, 0> col;
  Array<float, 0> mask;

  Array<float4, 0> loop_col;

  /* Mesh. */

  Array<int, 0> vert_indices;
  int unique_verts_num;

  /**
   * \todo Storing corners rather than faces is unnecessary.
   */
  Vector<int, 0> corner_indices;

  BitVector<0> vert_hidden;
  BitVector<0> face_hidden;

  /* Multires. */

  /** Indices of grids in the pbvh::Tree node. */
  Array<int, 0> grids;
  BitGroupVector<0> grid_hidden;

  /* Sculpt Face Sets */
  Array<int, 0> face_sets;

  Vector<int> face_indices;
};

struct SculptAttrRef {
  bke::AttrDomain domain;
  eCustomDataType type;
  char name[MAX_CUSTOMDATA_LAYER_NAME];
  bool was_set;
};

/* Storage of geometry for the undo node.
 * Is used as a storage for either original or modified geometry. */
struct NodeGeometry {
  /* Is used for sanity check, helping with ensuring that two and only two
   * geometry pushes happened in the undo stack. */
  bool is_initialized;

  CustomData vert_data;
  CustomData edge_data;
  CustomData corner_data;
  CustomData face_data;
  int *face_offset_indices;
  const ImplicitSharingInfo *face_offsets_sharing_info;
  int verts_num;
  int edges_num;
  int corners_num;
  int faces_num;
};

struct Node;
struct PositionUndoStorage;

struct StepData {
 private:
  bool applied_ = true;

 public:
  /**
   * The type of data stored in this undo step. For historical reasons this is often set when the
   * first undo node is pushed.
   */
  Type type = Type::None;

  /** Name of the object associated with this undo data (`object.id.name`). */
  std::string object_name;

  /** Name of the object's active shape key when the undo step was created. */
  std::string active_shape_key_name;

  /* TODO: Combine the three structs into a variant, since they specify data that is only valid
   * within a single mode. */
  struct {
    /* The number of vertices in the entire mesh. */
    int verts_num;
    /* The number of face corners in the entire mesh. */
    int corners_num;
  } mesh;

  struct {
    /** The number of grids in the entire mesh. A copy of #SubdivCCG::grids_num. */
    int grids_num;
    /** A copy of #SubdivCCG::grid_size. */
    int grid_size;
  } grids;

  struct {
    /**
     * The current log entry for the given BMLog step. Represents the most recent step at the time
     * that this entry is added.
     *
     * There are two usages of this pointer:
     * - If undoing or redoing a enter / exit from Dyntopo, this entry is used to rebuild the
     *   BMLog from all of the relevant entries
     * - When an undo step is no longer valid, this is used to free the data that it holds and
     *   remove it from the underlying list.
     */
    BMLogEntry *bm_entry;

    /* Geometry at the bmesh enter moment. */
    NodeGeometry geometry_enter;
  } bmesh;

  float3 pivot_pos;
  float4 pivot_rot;

  /* Geometry modification operations. */
  /* Original geometry is stored before the modification and is restored from when undoing. */
  NodeGeometry geometry_original;
  /* Modified geometry is stored after the modification and is restored from when redoing. */
  NodeGeometry geometry_modified;

  Mutex nodes_mutex;

  /**
   * #undo::Node is stored per #pbvh::Node to reduce data storage needed for changes only impacting
   * small portions of the mesh. During undo step creation and brush evaluation we often need to
   * look up the undo state for a specific node. That lookup must be protected by a lock since
   * nodes are pushed from multiple threads. This map speeds up undo node access to reduce the
   * amount of time we wait for the lock.
   *
   * This is only accessible when building the undo step, in between #push_begin and #push_end.
   */
  Map<const bke::pbvh::Node *, std::unique_ptr<Node>> undo_nodes_by_pbvh_node;

  /** Storage of per-node undo data after creation of the undo step is finished. */
  Vector<std::unique_ptr<Node>> nodes;
  std::unique_ptr<PositionUndoStorage> position_step_storage;
  size_t undo_size;

  /** Whether processing code needs to handle the current data as an undo step. */
  bool needs_undo() const
  {
    return applied_;
  }

  void tag_needs_undo()
  {
    applied_ = true;
  }

  void tag_needs_redo()
  {
    applied_ = false;
  }
};

namespace compression {

/**
 * Compress a span, using a prefiltering step that can improve compression speed and ratios for
 * certain float data types.
 */
template<typename T>
void filter_compress(const Span<T> src,
                     Vector<std::byte> &filter_buffer,
                     Vector<std::byte> &compress_buffer)
{
  filter_buffer.resize(src.size_in_bytes());
  filter_transpose_delta(reinterpret_cast<const uint8_t *>(src.data()),
                         reinterpret_cast<uint8_t *>(filter_buffer.data()),
                         src.size(),
                         sizeof(T));

  /* Level 3 gives a good balance of compression performance and ratio, and is also used elsewhere
   * across Blender for calls to #ZSTD_compress. */
  constexpr int zstd_level = 3;
  compress_buffer.resize(ZSTD_compressBound(src.size_in_bytes()));
  const size_t dst_size = ZSTD_compress(compress_buffer.data(),
                                        compress_buffer.size(),
                                        filter_buffer.data(),
                                        filter_buffer.size(),
                                        zstd_level);
  if (ZSTD_isError(dst_size)) {
    compress_buffer.clear();
    return;
  }

  compress_buffer.resize(dst_size);
}

template<typename T>
void filter_decompress(const Span<std::byte> src, Vector<std::byte> &buffer, Vector<T> &dst)
{
  const unsigned long long dst_size_in_bytes = ZSTD_getFrameContentSize(src.data(), src.size());
  if (ELEM(dst_size_in_bytes, ZSTD_CONTENTSIZE_ERROR, ZSTD_CONTENTSIZE_UNKNOWN)) {
    dst.clear();
    return;
  }

  buffer.resize(dst_size_in_bytes);
  const size_t result = ZSTD_decompress(buffer.data(), buffer.size(), src.data(), src.size());
  if (ZSTD_isError(result)) {
    dst.clear();
    return;
  }

  dst.resize(buffer.size() / sizeof(T));
  unfilter_transpose_delta(reinterpret_cast<const uint8_t *>(buffer.data()),
                           reinterpret_cast<uint8_t *>(dst.data()),
                           dst.size(),
                           sizeof(T));
}

template void filter_compress<float3>(Span<float3>, Vector<std::byte> &, Vector<std::byte> &);
template void filter_compress<int>(Span<int>, Vector<std::byte> &, Vector<std::byte> &);

template void filter_decompress<float3>(Span<std::byte>, Vector<std::byte> &, Vector<float3> &);
template void filter_decompress<int>(Span<std::byte>, Vector<std::byte> &, Vector<int> &);

}  // namespace compression

struct PositionUndoStorage : NonMovable {
  Vector<std::unique_ptr<Node>> nodes_to_compress;
  bool multires_undo;

  Array<Array<std::byte>> compressed_indices;

  /* As undo and redo happen, the data in these arrays is swapped (an undo step becomes a redo
   * step, and vice versa). */
  Array<Array<std::byte>> compressed_positions;

  Array<int> unique_verts_nums;

  TaskPool *compression_task_pool;
  std::atomic<bool> compression_ready = false;
  std::atomic<bool> compression_started = false;
  StepData *owner_step_data = nullptr;

  explicit PositionUndoStorage(StepData &step_data)
      : nodes_to_compress(std::move(step_data.nodes)), owner_step_data(&step_data)
  {
    this->multires_undo = step_data.grids.grids_num != 0;
    if (!multires_undo) {
      this->unique_verts_nums.reinitialize(this->nodes_to_compress.size());
      for (const int i : this->nodes_to_compress.index_range()) {
        this->unique_verts_nums[i] = this->nodes_to_compress[i]->unique_verts_num;
      }
    }

    this->compression_task_pool = BLI_task_pool_create_background(this, TASK_PRIORITY_LOW);
    this->compression_started = true;

    BLI_task_pool_push(this->compression_task_pool, compress_fn, this, false, nullptr);
  }

  ~PositionUndoStorage()
  {
    if (compression_started.load() && compression_task_pool) {
      BLI_task_pool_work_and_wait(compression_task_pool);
      BLI_task_pool_free(compression_task_pool);
    }
  }

  void ensure_compression_complete()
  {
    if (!compression_ready.load(std::memory_order_acquire)) {
      BLI_task_pool_work_and_wait(compression_task_pool);
    }
  }

  static void compress_fn(TaskPool * /*pool*/, void *task_data)
  {
#ifdef DEBUG_TIME
    SCOPED_TIMER_AVERAGED(__func__);
#endif
    auto *data = static_cast<PositionUndoStorage *>(task_data);
    MutableSpan<std::unique_ptr<Node>> nodes = data->nodes_to_compress;
    const int nodes_num = nodes.size();

    Array<Array<std::byte>> compressed_indices(nodes.size(), NoInitialization());
    Array<Array<std::byte>> compressed_data(nodes.size(), NoInitialization());
    struct CompressLocalData {
      Vector<std::byte> filtered;
      Vector<std::byte> compressed;
    };
    threading::isolate_task([&]() {
      threading::EnumerableThreadSpecific<CompressLocalData> all_tls;
      threading::parallel_for(IndexRange(nodes_num), 1, [&](const IndexRange range) {
        CompressLocalData &local_data = all_tls.local();
        for (const int i : range) {
          const Span<int> indices = data->multires_undo ? nodes[i]->grids : nodes[i]->vert_indices;
          const Span<float3> positions = !nodes[i]->orig_position.is_empty() ?
                                             nodes[i]->orig_position :
                                             nodes[i]->position;
          compression::filter_compress(indices, local_data.filtered, local_data.compressed);
          new (&compressed_indices[i]) Array<std::byte>(local_data.compressed.as_span());
          compression::filter_compress(positions, local_data.filtered, local_data.compressed);
          new (&compressed_data[i]) Array<std::byte>(local_data.compressed.as_span());
          nodes[i].reset();
        }
      });
    });
    data->nodes_to_compress.clear_and_shrink();

    size_t memory_size = 0;
    for (const int i : IndexRange(nodes_num)) {
      memory_size += compressed_indices[i].as_span().size_in_bytes();
      memory_size += compressed_data[i].as_span().size_in_bytes();
    }

    data->compressed_indices = std::move(compressed_indices);
    data->compressed_positions = std::move(compressed_data);
    data->owner_step_data->undo_size += memory_size;

    data->compression_ready.store(true, std::memory_order_release);
  }
};

struct SculptUndoStep {
  UndoStep step;
  /* NOTE: will split out into list for multi-object-sculpt-mode. */
  StepData data;

  /* Active color attribute at the start of this undo step. */
  SculptAttrRef active_color_start;

  /* Active color attribute at the end of this undo step. */
  SculptAttrRef active_color_end;
};

size_t step_memory_size_get(UndoStep *step)
{
  if (step->type != BKE_UNDOSYS_TYPE_SCULPT) {
    return 0;
  }

  SculptUndoStep *sculpt_step = reinterpret_cast<SculptUndoStep *>(step);

  if (sculpt_step->data.position_step_storage) {
    sculpt_step->data.position_step_storage->ensure_compression_complete();
  }

  return sculpt_step->data.undo_size;
}

static SculptUndoStep *get_active_step()
{
  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us = BKE_undosys_stack_init_or_active_with_type(ustack, BKE_UNDOSYS_TYPE_SCULPT);
  return reinterpret_cast<SculptUndoStep *>(us);
}

static StepData *get_step_data()
{
  if (SculptUndoStep *us = get_active_step()) {
    return &us->data;
  }
  return nullptr;
}

static bool use_multires_undo(const StepData &step_data, const SculptSession &ss)
{
  return step_data.grids.grids_num != 0 && ss.subdiv_ccg != nullptr;
}

static bool topology_matches(const StepData &step_data, const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  if (use_multires_undo(step_data, ss)) {
    const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
    return subdiv_ccg.grids_num == step_data.grids.grids_num &&
           subdiv_ccg.grid_size == step_data.grids.grid_size;
  }
  const Mesh &mesh = *static_cast<Mesh *>(object.data);
  return mesh.verts_num == step_data.mesh.verts_num;
}

static bool indices_contain_true(const Span<bool> data, const Span<int> indices)
{
  return std::any_of(indices.begin(), indices.end(), [&](const int i) { return data[i]; });
}

static bool restore_active_shape_key(bContext &C,
                                     Depsgraph &depsgraph,
                                     const StepData &step_data,
                                     Object &object)
{
  const SculptSession &ss = *object.sculpt;
  if (ss.shapekey_active && ss.shapekey_active->name != step_data.active_shape_key_name) {
    /* Shape key has been changed before calling undo operator. */

    Key *key = BKE_key_from_object(&object);
    const KeyBlock *kb = key ?
                             BKE_keyblock_find_name(key, step_data.active_shape_key_name.c_str()) :
                             nullptr;

    if (kb) {
      object.shapenr = BLI_findindex(&key->block, kb) + 1;

      BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
      WM_event_add_notifier(&C, NC_OBJECT | ND_DATA, &object);
    }
    else {
      /* Key has been removed -- skip this undo node. */
      return false;
    }
  }
  return true;
}

template<typename T>
static void swap_indexed_data(MutableSpan<T> full, const Span<int> indices, MutableSpan<T> indexed)
{
  BLI_assert(full.size() == indices.size());
  for (const int i : indices.index_range()) {
    std::swap(full[i], indexed[indices[i]]);
  }
}

static void restore_position_mesh(Object &object,
                                  PositionUndoStorage &undo_data,
                                  const MutableSpan<bool> modified_verts)
{
#ifdef DEBUG_TIME
  SCOPED_TIMER_AVERAGED(__func__);
#endif
  SculptSession &ss = *object.sculpt;
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  std::optional<ShapeKeyData> shape_key_data = ShapeKeyData::from_object(object);

  undo_data.ensure_compression_complete();

  const int nodes_num = undo_data.unique_verts_nums.size();

  struct LocalData {
    Vector<std::byte> compress_buffer;
    Vector<std::byte> filter_buffer;
    Vector<int> indices;
    Vector<float3> positions;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  threading::parallel_for(IndexRange(nodes_num), 1, [&](const IndexRange range) {
    LocalData &tls = all_tls.local();
    for (const int i : range) {
      compression::filter_decompress<int>(
          undo_data.compressed_indices[i], tls.compress_buffer, tls.indices);
      const int unique_verts_num = undo_data.unique_verts_nums[i];
      const Span<int> verts = tls.indices.as_span().take_front(unique_verts_num);

      compression::filter_decompress<float3>(
          undo_data.compressed_positions[i], tls.compress_buffer, tls.positions);
      MutableSpan undo_positions = tls.positions.as_mutable_span();

      if (!ss.deform_modifiers_active) {
        /* When original positions aren't written separately in the undo step, there are no
         * deform modifiers. Therefore the original and evaluated deform positions will be the
         * same, and modifying the positions from the original mesh is enough. */
        swap_indexed_data(undo_positions.take_front(unique_verts_num), verts, positions);
      }
      else {
        /* When original positions are stored in the undo step, undo/redo will cause a reevaluation
         * of the object. The evaluation will recompute the evaluated positions, so dealing with
         * them here is unnecessary. */
        if (shape_key_data) {
          MutableSpan<float3> active_data = shape_key_data->active_key_data;

          if (!shape_key_data->dependent_keys.is_empty()) {
            Array<float3, 1024> translations(verts.size());
            translations_from_new_positions(
                undo_positions.take_front(unique_verts_num), verts, active_data, translations);
            for (MutableSpan<float3> data : shape_key_data->dependent_keys) {
              apply_translations(translations, verts, data);
            }
          }

          if (shape_key_data->basis_key_active) {
            /* The basis key positions and the mesh positions are always kept in sync. */
            scatter_data_mesh(undo_positions.as_span(), verts, positions);
          }
          swap_indexed_data(undo_positions.take_front(unique_verts_num), verts, active_data);
        }
        else {
          /* There is a deform modifier, but no shape keys. */
          swap_indexed_data(undo_positions.take_front(unique_verts_num), verts, positions);
        }
      }

      modified_verts.fill_indices(verts, true);

      compression::filter_compress<float3>(undo_positions, tls.filter_buffer, tls.compress_buffer);
      undo_data.compressed_positions[i] = tls.compress_buffer.as_span();
    }
  });
}

static void restore_position_grids(const MutableSpan<float3> positions,
                                   const CCGKey &key,
                                   PositionUndoStorage &undo_data,
                                   const MutableSpan<bool> modified_grids)
{
  const int nodes_num = undo_data.compressed_indices.size();

  struct LocalData {
    Vector<std::byte> compress_buffer;
    Vector<std::byte> filter_buffer;
    Vector<int> indices;
    Vector<float3> positions;
  };
  threading::EnumerableThreadSpecific<LocalData> all_tls;
  threading::parallel_for(IndexRange(nodes_num), 1, [&](const IndexRange range) {
    LocalData &tls = all_tls.local();
    for (const int i : range) {
      compression::filter_decompress<int>(
          undo_data.compressed_indices[i], tls.compress_buffer, tls.indices);
      const Span<int> grids = tls.indices.as_span();

      compression::filter_decompress<float3>(
          undo_data.compressed_positions[i], tls.compress_buffer, tls.positions);
      MutableSpan node_positions = tls.positions.as_mutable_span();

      for (const int i : grids.index_range()) {
        MutableSpan data = positions.slice(bke::ccg::grid_range(key, grids[i]));
        MutableSpan undo_data = node_positions.slice(bke::ccg::grid_range(key, i));
        for (const int offset : data.index_range()) {
          std::swap(data[offset], undo_data[offset]);
        }
      }

      modified_grids.fill_indices(grids, true);

      compression::filter_compress<float3>(node_positions, tls.filter_buffer, tls.compress_buffer);
      undo_data.compressed_positions[i] = tls.compress_buffer.as_span();
    }
  });
}

static void restore_vert_visibility_mesh(Object &object,
                                         Node &unode,
                                         const MutableSpan<bool> modified_verts)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_vert", bke::AttrDomain::Point);
  for (const int i : unode.vert_indices.index_range().take_front(unode.unique_verts_num)) {
    const int vert = unode.vert_indices[i];
    if (unode.vert_hidden[i].test() != hide_vert.span[vert]) {
      unode.vert_hidden[i].set(!unode.vert_hidden[i].test());
      hide_vert.span[vert] = !hide_vert.span[vert];
      modified_verts[vert] = true;
    }
  }
  hide_vert.finish();
}

static void restore_vert_visibility_grids(SubdivCCG &subdiv_ccg,
                                          Node &unode,
                                          const MutableSpan<bool> modified_grids)
{
  if (unode.grid_hidden.is_empty()) {
    BKE_subdiv_ccg_grid_hidden_free(subdiv_ccg);
    return;
  }

  BitGroupVector<> &grid_hidden = BKE_subdiv_ccg_grid_hidden_ensure(subdiv_ccg);
  const Span<int> grids = unode.grids;
  for (const int i : grids.index_range()) {
    /* Swap the two bit spans. */
    MutableBoundedBitSpan a = unode.grid_hidden[i];
    MutableBoundedBitSpan b = grid_hidden[grids[i]];
    for (const int j : a.index_range()) {
      const bool value_a = a[j];
      const bool value_b = b[j];
      a[j].set(value_b);
      b[j].set(value_a);
    }
  }

  modified_grids.fill_indices(grids, true);
}

static void restore_hidden_face(Object &object,
                                Node &unode,
                                const MutableSpan<bool> modified_faces)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter hide_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_poly", bke::AttrDomain::Face);

  const Span<int> face_indices = unode.face_indices;

  for (const int i : face_indices.index_range()) {
    const int face = face_indices[i];
    if (unode.face_hidden[i].test() != hide_poly.span[face]) {
      unode.face_hidden[i].set(!unode.face_hidden[i].test());
      hide_poly.span[face] = !hide_poly.span[face];
      modified_faces[face] = true;
    }
  }
  hide_poly.finish();
}

static void restore_color(Object &object,
                          StepData &step_data,
                          const MutableSpan<bool> modified_verts)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::GSpanAttributeWriter color_attribute = color::active_color_attribute_for_write(mesh);

  for (std::unique_ptr<Node> &unode : step_data.nodes) {
    if (color_attribute.domain == bke::AttrDomain::Point && !unode->col.is_empty()) {
      color::swap_gathered_colors(
          unode->vert_indices.as_span().take_front(unode->unique_verts_num),
          color_attribute.span,
          unode->col);
    }
    else if (color_attribute.domain == bke::AttrDomain::Corner && !unode->loop_col.is_empty()) {
      color::swap_gathered_colors(unode->corner_indices, color_attribute.span, unode->loop_col);
    }

    modified_verts.fill_indices(unode->vert_indices.as_span(), true);
  }

  color_attribute.finish();
}

static void restore_mask_mesh(Object &object, Node &unode, const MutableSpan<bool> modified_verts)
{
  Mesh *mesh = BKE_object_get_original_mesh(&object);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
      ".sculpt_mask", bke::AttrDomain::Point);

  const Span<int> index = unode.vert_indices.as_span().take_front(unode.unique_verts_num);

  for (const int i : index.index_range()) {
    const int vert = index[i];
    if (mask.span[vert] != unode.mask[i]) {
      std::swap(mask.span[vert], unode.mask[i]);
      modified_verts[vert] = true;
    }
  }

  mask.finish();
}

static void restore_mask_grids(Object &object, Node &unode, const MutableSpan<bool> modified_grids)
{
  SculptSession &ss = *object.sculpt;
  SubdivCCG *subdiv_ccg = ss.subdiv_ccg;
  MutableSpan<float> masks = subdiv_ccg->masks;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(*subdiv_ccg);

  const Span<int> grids = unode.grids;
  MutableSpan<float> undo_mask = unode.mask;

  for (const int i : grids.index_range()) {
    MutableSpan data = masks.slice(bke::ccg::grid_range(key, grids[i]));
    MutableSpan undo_data = undo_mask.slice(bke::ccg::grid_range(key, i));
    for (const int offset : data.index_range()) {
      std::swap(data[offset], undo_data[offset]);
    }
  }

  modified_grids.fill_indices(unode.grids.as_span(), true);
}

static bool restore_face_sets(Object &object,
                              Node &unode,
                              const MutableSpan<bool> modified_face_set_faces)
{
  const Span<int> face_indices = unode.face_indices;

  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(
      *static_cast<Mesh *>(object.data));
  bool modified = false;
  for (const int i : face_indices.index_range()) {
    const int face = face_indices[i];
    if (unode.face_sets[i] == face_sets.span[face]) {
      continue;
    }
    std::swap(unode.face_sets[i], face_sets.span[face]);
    modified_face_set_faces[face] = true;
    modified = true;
  }
  face_sets.finish();
  return modified;
}

static void bmesh_restore_generic(StepData &step_data, Object &object)
{
  SculptSession &ss = *object.sculpt;
  if (step_data.needs_undo()) {
    BM_log_undo(ss.bm, ss.bm_log);
    step_data.tag_needs_redo();
  }
  else {
    BM_log_redo(ss.bm, ss.bm_log);
    step_data.tag_needs_undo();
  }

  if (step_data.type == Type::Mask) {
    bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
    IndexMaskMemory memory;
    const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);
    pbvh.tag_masks_changed(node_mask);
    bke::pbvh::update_mask_bmesh(*ss.bm, node_mask, pbvh);
  }
  else {
    BKE_sculptsession_free_pbvh(object);
    DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);
    BM_mesh_normals_update(ss.bm);
  }
}

/* Create empty sculpt BMesh and enable logging. */
static void bmesh_enable(Object &object, const StepData &step_data)
{
  SculptSession &ss = *object.sculpt;
  Mesh *mesh = static_cast<Mesh *>(object.data);

  BKE_sculptsession_free_pbvh(object);
  DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);

  /* Create empty BMesh and enable logging. */
  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = false;

  ss.bm = BM_mesh_create(&bm_mesh_allocsize_default, &bmesh_create_params);
  BM_data_layer_add_named(ss.bm, &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  mesh->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Restore the BMLog using saved entries. */
  ss.bm_log = BM_log_from_existing_entries_create(ss.bm, step_data.bmesh.bm_entry);
}

static void bmesh_handle_dyntopo_begin(bContext *C, StepData &step_data, Object &object)
{
  if (step_data.needs_undo()) {
    dyntopo::disable(C, &step_data);
    step_data.tag_needs_redo();
  }
  else /* needs_redo */ {
    SculptSession &ss = *object.sculpt;
    bmesh_enable(object, step_data);

    /* Restore the mesh from the first log entry. */
    BM_log_redo(ss.bm, ss.bm_log);

    step_data.tag_needs_undo();
  }
}

static void bmesh_handle_dyntopo_end(bContext *C, StepData &step_data, Object &object)
{
  if (step_data.needs_undo()) {
    SculptSession &ss = *object.sculpt;
    bmesh_enable(object, step_data);

    /* Restore the mesh from the last log entry. */
    BM_log_undo(ss.bm, ss.bm_log);

    step_data.tag_needs_redo();
  }
  else /* needs_redo */ {
    /* Disable dynamic topology sculpting. */
    dyntopo::disable(C, nullptr);
    step_data.tag_needs_undo();
  }
}

static void store_geometry_data(NodeGeometry *geometry, const Object &object)
{
  const Mesh *mesh = static_cast<const Mesh *>(object.data);

  BLI_assert(!geometry->is_initialized);
  geometry->is_initialized = true;

  CustomData_init_from(
      &mesh->vert_data, &geometry->vert_data, CD_MASK_MESH.vmask, mesh->verts_num);
  CustomData_init_from(
      &mesh->edge_data, &geometry->edge_data, CD_MASK_MESH.emask, mesh->edges_num);
  CustomData_init_from(
      &mesh->corner_data, &geometry->corner_data, CD_MASK_MESH.lmask, mesh->corners_num);
  CustomData_init_from(
      &mesh->face_data, &geometry->face_data, CD_MASK_MESH.pmask, mesh->faces_num);
  implicit_sharing::copy_shared_pointer(mesh->face_offset_indices,
                                        mesh->runtime->face_offsets_sharing_info,
                                        &geometry->face_offset_indices,
                                        &geometry->face_offsets_sharing_info);

  geometry->verts_num = mesh->verts_num;
  geometry->edges_num = mesh->edges_num;
  geometry->corners_num = mesh->corners_num;
  geometry->faces_num = mesh->faces_num;
}

static void restore_geometry_data(const NodeGeometry *geometry, Mesh *mesh)
{
  BLI_assert(geometry->is_initialized);

  BKE_mesh_clear_geometry(mesh);

  mesh->verts_num = geometry->verts_num;
  mesh->edges_num = geometry->edges_num;
  mesh->corners_num = geometry->corners_num;
  mesh->faces_num = geometry->faces_num;
  mesh->totface_legacy = 0;

  CustomData_init_from(
      &geometry->vert_data, &mesh->vert_data, CD_MASK_MESH.vmask, geometry->verts_num);
  CustomData_init_from(
      &geometry->edge_data, &mesh->edge_data, CD_MASK_MESH.emask, geometry->edges_num);
  CustomData_init_from(
      &geometry->corner_data, &mesh->corner_data, CD_MASK_MESH.lmask, geometry->corners_num);
  CustomData_init_from(
      &geometry->face_data, &mesh->face_data, CD_MASK_MESH.pmask, geometry->faces_num);
  implicit_sharing::copy_shared_pointer(geometry->face_offset_indices,
                                        geometry->face_offsets_sharing_info,
                                        &mesh->face_offset_indices,
                                        &mesh->runtime->face_offsets_sharing_info);
}

static void geometry_free_data(NodeGeometry *geometry)
{
  CustomData_free(&geometry->vert_data);
  CustomData_free(&geometry->edge_data);
  CustomData_free(&geometry->corner_data);
  CustomData_free(&geometry->face_data);
  implicit_sharing::free_shared_data(&geometry->face_offset_indices,
                                     &geometry->face_offsets_sharing_info);
}

static void restore_geometry(StepData &step_data, Object &object)
{
  BKE_sculptsession_free_pbvh(object);
  DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);

  Mesh *mesh = static_cast<Mesh *>(object.data);

  if (step_data.needs_undo()) {
    restore_geometry_data(&step_data.geometry_original, mesh);
    step_data.tag_needs_redo();
  }
  else {
    restore_geometry_data(&step_data.geometry_modified, mesh);
    step_data.tag_needs_undo();
  }
}

/* Handle all dynamic-topology updates
 *
 * Returns true if this was a dynamic-topology undo step, otherwise
 * returns false to indicate the non-dyntopo code should run. */
static int bmesh_restore(bContext *C, Depsgraph &depsgraph, StepData &step_data, Object &object)
{
  SculptSession &ss = *object.sculpt;
  switch (step_data.type) {
    case Type::DyntopoBegin:
      BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
      bmesh_handle_dyntopo_begin(C, step_data, object);
      return true;

    case Type::DyntopoEnd:
      BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
      bmesh_handle_dyntopo_end(C, step_data, object);
      return true;
    default:
      if (ss.bm_log) {
        BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
        bmesh_restore_generic(step_data, object);
        return true;
      }
      break;
  }

  return false;
}

void restore_from_bmesh_enter_geometry(const StepData &step_data, Mesh &mesh)
{
  restore_geometry_data(&step_data.bmesh.geometry_enter, &mesh);
}

bool has_bmesh_log_entry()
{
  return get_step_data()->bmesh.bm_entry;
}

/* Geometry updates (such as Apply Base, for example) will re-evaluate the object and refine its
 * Subdiv descriptor. Upon undo it is required that mesh, grids, and subdiv all stay consistent
 * with each other. This means that when geometry coordinate changes the undo should refine the
 * subdiv to the new coarse mesh coordinates. Tricky part is: this needs to happen without using
 * dependency graph tag: tagging object for geometry update will either loose sculpted data from
 * the sculpt grids, or will wrongly "commit" them to the CD_MDISPS.
 *
 * So what we do instead is do minimum object evaluation to get base mesh coordinates for the
 * multires modifier input. While this is expensive, it is less expensive than dependency graph
 * evaluation and is only happening when geometry coordinates changes on undo.
 *
 * Note that the dependency graph is ensured to be evaluated prior to the undo step is decoded,
 * so if the object's modifier stack references other object it is all fine. */
static void refine_subdiv(Depsgraph *depsgraph,
                          const SculptSession &ss,
                          Object &object,
                          bke::subdiv::Subdiv *subdiv)
{
  Array<float3> deformed_verts = BKE_multires_create_deformed_base_mesh_vert_coords(
      depsgraph, &object, ss.multires.modifier);

  bke::subdiv::eval_refine_from_mesh(
      subdiv, static_cast<const Mesh *>(object.data), deformed_verts);
}

static void restore_list(bContext *C, Depsgraph *depsgraph, StepData &step_data)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object &object = *BKE_view_layer_active_object_get(view_layer);
  if (step_data.object_name != object.id.name) {
    return;
  }
  SculptSession &ss = *object.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);

  /* Restore pivot. */
  ss.pivot_pos = step_data.pivot_pos;
  ss.pivot_rot = step_data.pivot_rot;

  if (bmesh_restore(C, *depsgraph, step_data, object)) {
    return;
  }

  /* Switching to sculpt mode does not push a particular type.
   * See #124484. */
  /* TODO: Add explicit type for switching into Sculpt Mode. */
  if (step_data.type == Type::None && step_data.nodes.is_empty()) {
    return;
  }

  /* Adding multires via the `subdivision_set` operator results in the subsequent undo step
   * not correctly performing a global undo step; we exit early here to avoid crashing.
   * See: #131478 */
  const bool multires_undo_step = use_multires_undo(step_data, ss);
  if ((multires_undo_step && pbvh.type() != bke::pbvh::Type::Grids) ||
      (!multires_undo_step && pbvh.type() != bke::pbvh::Type::Mesh))
  {
    CLOG_WARN(&LOG,
              "Undo step type and sculpt geometry type do not match: skipping undo state restore");
    return;
  }

  const bool tag_update = ID_REAL_USERS(object.data) > 1 ||
                          !BKE_sculptsession_use_pbvh_draw(&object, rv3d) || ss.shapekey_active ||
                          ss.deform_modifiers_active;

  switch (step_data.type) {
    case Type::None: {
      BLI_assert_unreachable();
      break;
    }
    case Type::Position: {
      IndexMaskMemory memory;
      const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      if (use_multires_undo(step_data, ss)) {
        MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

        Array<bool> modified_grids(subdiv_ccg.grids_num, false);
        restore_position_grids(
            subdiv_ccg.positions, key, *step_data.position_step_storage, modified_grids);

        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_grids, nodes[i].grids());
            });
        pbvh.tag_positions_changed(changed_nodes);
        multires_mark_as_modified(depsgraph, &object, MULTIRES_COORDS_MODIFIED);
      }
      else {
        MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        if (!restore_active_shape_key(*C, *depsgraph, step_data, object)) {
          return;
        }
        const Mesh &mesh = *static_cast<const Mesh *>(object.data);
        Array<bool> modified_verts(mesh.verts_num, false);
        restore_position_mesh(object, *step_data.position_step_storage, modified_verts);

        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_verts, nodes[i].all_verts());
            });
        pbvh.tag_positions_changed(changed_nodes);
      }

      if (tag_update) {
        Mesh &mesh = *static_cast<Mesh *>(object.data);
        mesh.tag_positions_changed();
        BKE_sculptsession_free_deformMats(&ss);
      }
      else {
        Mesh &mesh = *static_cast<Mesh *>(object.data);
        /* The BVH normals recalculation that will happen later (caused by
         * `pbvh.tag_positions_changed`) won't recalculate the face corner normals.
         * We need to manually clear that cache. */
        mesh.runtime->corner_normals_cache.tag_dirty();
      }
      pbvh.update_bounds(*depsgraph, object);
      bke::pbvh::store_bounds_orig(pbvh);
      break;
    }
    case Type::HideVert: {
      IndexMaskMemory memory;
      const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      if (use_multires_undo(step_data, ss)) {
        MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        Array<bool> modified_grids(subdiv_ccg.grids_num, false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_vert_visibility_grids(subdiv_ccg, *unode, modified_grids);
        }
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_grids, nodes[i].grids());
            });
        pbvh.tag_visibility_changed(changed_nodes);
      }
      else {
        MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        const Mesh &mesh = *static_cast<const Mesh *>(object.data);
        Array<bool> modified_verts(mesh.verts_num, false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_vert_visibility_mesh(object, *unode, modified_verts);
        }
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_verts, nodes[i].all_verts());
            });
        pbvh.tag_visibility_changed(changed_nodes);
      }

      BKE_pbvh_sync_visibility_from_verts(object);
      pbvh.update_visibility(object);
      if (BKE_sculpt_multires_active(scene, &object)) {
        multires_mark_as_modified(depsgraph, &object, MULTIRES_HIDDEN_MODIFIED);
      }
      break;
    }
    case Type::HideFace: {
      IndexMaskMemory memory;
      const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      Array<bool> modified_faces(mesh.faces_num, false);
      for (std::unique_ptr<Node> &unode : step_data.nodes) {
        restore_hidden_face(object, *unode, modified_faces);
      }

      if (use_multires_undo(step_data, ss)) {
        MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              Vector<int> faces_vector;
              const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
                  subdiv_ccg, nodes[i], faces_vector);
              return indices_contain_true(modified_faces, faces);
            });
        pbvh.tag_visibility_changed(changed_nodes);
      }
      else {
        MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_faces, nodes[i].faces());
            });
        pbvh.tag_visibility_changed(changed_nodes);
      }

      hide::sync_all_from_faces(object);
      pbvh.update_visibility(object);
      break;
    }
    case Type::Mask: {
      IndexMaskMemory memory;
      const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      if (use_multires_undo(step_data, ss)) {
        MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        Array<bool> modified_grids(ss.subdiv_ccg->grids_num, false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_mask_grids(object, *unode, modified_grids);
        }
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_grids, nodes[i].grids());
            });
        bke::pbvh::update_mask_grids(*ss.subdiv_ccg, changed_nodes, pbvh);
        pbvh.tag_masks_changed(changed_nodes);
      }
      else {
        MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        const Mesh &mesh = *static_cast<const Mesh *>(object.data);
        Array<bool> modified_verts(mesh.verts_num, false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_mask_mesh(object, *unode, modified_verts);
        }
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_verts, nodes[i].all_verts());
            });
        bke::pbvh::update_mask_mesh(mesh, changed_nodes, pbvh);
        pbvh.tag_masks_changed(changed_nodes);
      }
      break;
    }
    case Type::FaceSet: {
      IndexMaskMemory memory;
      const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      Array<bool> modified_faces(mesh.faces_num, false);
      for (std::unique_ptr<Node> &unode : step_data.nodes) {
        restore_face_sets(object, *unode, modified_faces);
      }
      if (use_multires_undo(step_data, ss)) {
        MutableSpan<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
        const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              Vector<int> faces_vector;
              const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
                  subdiv_ccg, nodes[i], faces_vector);
              return indices_contain_true(modified_faces, faces);
            });
        pbvh.tag_face_sets_changed(changed_nodes);
      }
      else {
        MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
        const IndexMask changed_nodes = IndexMask::from_predicate(
            node_mask, GrainSize(1), memory, [&](const int i) {
              return indices_contain_true(modified_faces, nodes[i].faces());
            });
        pbvh.tag_face_sets_changed(changed_nodes);
      }
      break;
    }
    case Type::Color: {
      IndexMaskMemory memory;
      const IndexMask node_mask = bke::pbvh::all_leaf_nodes(pbvh, memory);

      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      Array<bool> modified_verts(mesh.verts_num, false);
      restore_color(object, step_data, modified_verts);
      const IndexMask changed_nodes = IndexMask::from_predicate(
          node_mask, GrainSize(1), memory, [&](const int i) {
            return indices_contain_true(modified_verts, nodes[i].all_verts());
          });
      pbvh.tag_attribute_changed(changed_nodes, mesh.active_color_attribute);
      break;
    }
    case Type::Geometry: {
      BLI_assert(!ss.bm);

      restore_geometry(step_data, object);
      BKE_sculptsession_free_deformMats(&ss);
      if (SubdivCCG *subdiv_ccg = ss.subdiv_ccg) {
        refine_subdiv(depsgraph, ss, object, subdiv_ccg->subdiv);
      }
      break;
    }
    case Type::DyntopoBegin:
    case Type::DyntopoEnd:
      /* Handled elsewhere. */
      BLI_assert_unreachable();
      break;
  }

  DEG_id_tag_update(&object.id, ID_RECALC_SHADING);
  if (tag_update) {
    DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);
  }
}

static void free_step_data(StepData &step_data)
{
  geometry_free_data(&step_data.geometry_original);
  geometry_free_data(&step_data.geometry_modified);
  geometry_free_data(&step_data.bmesh.geometry_enter);
  if (step_data.bmesh.bm_entry) {
    BM_log_entry_drop(step_data.bmesh.bm_entry);
  }
  step_data.~StepData();
}

/**
 * Retrieve the undo data of a given type for the active undo step. For example, this is used to
 * access "original" data from before the current stroke.
 *
 * This is only possible when building an undo step, in between #push_begin and #push_end.
 */
static const Node *get_node(const bke::pbvh::Node *node, const Type type)
{
  StepData *step_data = get_step_data();
  if (!step_data) {
    return nullptr;
  }
  if (step_data->type != type) {
    return nullptr;
  }
  /* This access does not need to be locked because this function is not expected to be called
   * while the per-node undo data is being pushed. In other words, this must not be called
   * concurrently with #push_node. */
  std::unique_ptr<Node> *node_ptr = step_data->undo_nodes_by_pbvh_node.lookup_ptr(node);
  if (!node_ptr) {
    return nullptr;
  }
  return node_ptr->get();
}

static void store_vert_visibility_grids(const SubdivCCG &subdiv_ccg,
                                        const bke::pbvh::GridsNode &node,
                                        Node &unode)
{
  const BitGroupVector<> &grid_hidden = subdiv_ccg.grid_hidden;
  if (grid_hidden.is_empty()) {
    return;
  }

  const Span<int> grid_indices = node.grids();
  unode.grid_hidden = BitGroupVector<0>(grid_indices.size(), grid_hidden.group_size());
  for (const int i : grid_indices.index_range()) {
    unode.grid_hidden[i].copy_from(grid_hidden[grid_indices[i]]);
  }
}

static void store_positions_mesh(const Depsgraph &depsgraph, const Object &object, Node &unode)
{
  const SculptSession &ss = *object.sculpt;
  gather_data_mesh(bke::pbvh::vert_positions_eval(depsgraph, object),
                   unode.vert_indices.as_span(),
                   unode.position.as_mutable_span());
  gather_data_mesh(bke::pbvh::vert_normals_eval(depsgraph, object),
                   unode.vert_indices.as_span(),
                   unode.normal.as_mutable_span());

  if (ss.deform_modifiers_active) {
    const Mesh &mesh = *static_cast<const Mesh *>(object.data);
    const Span<float3> orig_positions = ss.shapekey_active ? Span(static_cast<const float3 *>(
                                                                      ss.shapekey_active->data),
                                                                  mesh.verts_num) :
                                                             mesh.vert_positions();

    gather_data_mesh(
        orig_positions, unode.vert_indices.as_span(), unode.orig_position.as_mutable_span());
  }
}

static void store_positions_grids(const SubdivCCG &subdiv_ccg, Node &unode)
{
  gather_data_grids(
      subdiv_ccg, subdiv_ccg.positions.as_span(), unode.grids, unode.position.as_mutable_span());
  gather_data_grids(
      subdiv_ccg, subdiv_ccg.normals.as_span(), unode.grids, unode.normal.as_mutable_span());
}

static void store_vert_visibility_mesh(const Mesh &mesh, const bke::pbvh::Node &node, Node &unode)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                              bke::AttrDomain::Point);
  if (hide_vert.is_empty()) {
    return;
  }

  const Span<int> verts = static_cast<const bke::pbvh::MeshNode &>(node).all_verts();
  for (const int i : verts.index_range()) {
    unode.vert_hidden[i].set(hide_vert[verts[i]]);
  }
}

static void store_face_visibility(const Mesh &mesh, Node &unode)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  if (hide_poly.is_empty()) {
    unode.face_hidden.fill(false);
    return;
  }
  const Span<int> faces = unode.face_indices;
  for (const int i : faces.index_range()) {
    unode.face_hidden[i].set(hide_poly[faces[i]]);
  }
}

static void store_mask_mesh(const Mesh &mesh, Node &unode)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point);
  if (mask.is_empty()) {
    unode.mask.fill(0.0f);
  }
  else {
    gather_data_mesh(mask, unode.vert_indices.as_span(), unode.mask.as_mutable_span());
  }
}

static void store_mask_grids(const SubdivCCG &subdiv_ccg, Node &unode)
{
  if (!subdiv_ccg.masks.is_empty()) {
    gather_data_grids(
        subdiv_ccg, subdiv_ccg.masks.as_span(), unode.grids, unode.mask.as_mutable_span());
  }
  else {
    unode.mask.fill(0.0f);
  }
}

static void store_color(const Mesh &mesh, const bke::pbvh::MeshNode &node, Node &unode)
{
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::GAttributeReader color_attribute = color::active_color_attribute(mesh);
  const GVArraySpan colors(*color_attribute);

  /* NOTE: even with loop colors we still store (derived)
   * vertex colors for original data lookup. */
  const Span<int> verts = node.verts();
  unode.col.reinitialize(verts.size());
  color::gather_colors_vert(
      faces, corner_verts, vert_to_face_map, colors, color_attribute.domain, verts, unode.col);

  if (color_attribute.domain == bke::AttrDomain::Corner) {
    for (const int face : node.faces()) {
      for (const int corner : faces[face]) {
        unode.corner_indices.append(corner);
      }
    }
    unode.loop_col.reinitialize(unode.corner_indices.size());
    color::gather_colors(colors, unode.corner_indices, unode.loop_col);
  }
}

static NodeGeometry *geometry_get(StepData &step_data)
{
  if (!step_data.geometry_original.is_initialized) {
    return &step_data.geometry_original;
  }

  BLI_assert(!step_data.geometry_modified.is_initialized);

  return &step_data.geometry_modified;
}

static void geometry_push(const Object &object)
{
  StepData *step_data = get_step_data();

  step_data->type = Type::Geometry;

  NodeGeometry *geometry = geometry_get(*step_data);
  store_geometry_data(geometry, object);
}

static void store_face_sets(const Mesh &mesh, Node &unode)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan face_sets = *attributes.lookup<int>(".sculpt_face_set", bke::AttrDomain::Face);
  if (face_sets.is_empty()) {
    unode.face_sets.fill(1);
  }
  else {
    gather_data_mesh(face_sets, unode.face_indices.as_span(), unode.face_sets.as_mutable_span());
  }
}

static void fill_node_data_mesh(const Depsgraph &depsgraph,
                                const Object &object,
                                const bke::pbvh::MeshNode &node,
                                const Type type,
                                Node &unode)
{
  const SculptSession &ss = *object.sculpt;
  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  unode.vert_indices = node.all_verts();
  unode.unique_verts_num = node.verts().size();

  const int verts_num = unode.vert_indices.size();

  if (ELEM(type, Type::FaceSet, Type::HideFace)) {
    unode.face_indices = node.faces();
  }

  switch (type) {
    case Type::None:
      BLI_assert_unreachable();
      break;
    case Type::Position: {
      unode.position.reinitialize(verts_num);
      /* Needed for original data lookup. */
      unode.normal.reinitialize(verts_num);
      if (ss.deform_modifiers_active) {
        unode.orig_position.reinitialize(verts_num);
      }
      store_positions_mesh(depsgraph, object, unode);
      break;
    }
    case Type::HideVert: {
      unode.vert_hidden.resize(unode.vert_indices.size());
      store_vert_visibility_mesh(mesh, node, unode);
      break;
    }
    case Type::HideFace: {
      unode.face_hidden.resize(unode.face_indices.size());
      store_face_visibility(mesh, unode);
      break;
    }
    case Type::Mask: {
      unode.mask.reinitialize(verts_num);
      store_mask_mesh(mesh, unode);
      break;
    }
    case Type::Color: {
      store_color(mesh, node, unode);
      break;
    }
    case Type::DyntopoBegin:
    case Type::DyntopoEnd:
      /* Dyntopo should be handled elsewhere. */
      BLI_assert_unreachable();
      break;
    case Type::Geometry:
      /* See #geometry_push. */
      BLI_assert_unreachable();
      break;
    case Type::FaceSet: {
      unode.face_sets.reinitialize(unode.face_indices.size());
      store_face_sets(mesh, unode);
      break;
    }
  }
}

static void fill_node_data_grids(const Object &object,
                                 const bke::pbvh::GridsNode &node,
                                 const Type type,
                                 Node &unode)
{
  const SculptSession &ss = *object.sculpt;
  const Mesh &base_mesh = *static_cast<const Mesh *>(object.data);
  const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;

  unode.grids = node.grids();

  const int grid_area = subdiv_ccg.grid_size * subdiv_ccg.grid_size;
  const int verts_num = unode.grids.size() * grid_area;

  if (ELEM(type, Type::FaceSet, Type::HideFace)) {
    bke::pbvh::node_face_indices_calc_grids(subdiv_ccg, node, unode.face_indices);
  }

  switch (type) {
    case Type::None:
      BLI_assert_unreachable();
      break;
    case Type::Position: {
      unode.position.reinitialize(verts_num);
      /* Needed for original data lookup. */
      unode.normal.reinitialize(verts_num);
      store_positions_grids(subdiv_ccg, unode);
      break;
    }
    case Type::HideVert: {
      store_vert_visibility_grids(subdiv_ccg, node, unode);
      break;
    }
    case Type::HideFace: {
      unode.face_hidden.resize(unode.face_indices.size());
      store_face_visibility(base_mesh, unode);
      break;
    }
    case Type::Mask: {
      unode.mask.reinitialize(verts_num);
      store_mask_grids(subdiv_ccg, unode);
      break;
    }
    case Type::Color: {
      BLI_assert_unreachable();
      break;
    }
    case Type::DyntopoBegin:
    case Type::DyntopoEnd:
      /* Dyntopo should be handled elsewhere. */
      BLI_assert_unreachable();
      break;
    case Type::Geometry:
      /* See #geometry_push. */
      BLI_assert_unreachable();
      break;
    case Type::FaceSet: {
      unode.face_sets.reinitialize(unode.face_indices.size());
      store_face_sets(base_mesh, unode);
      break;
    }
  }
}

/**
 * Dynamic topology stores only one undo node per stroke, regardless of the number of
 * bke::pbvh::Tree nodes modified.
 */
BLI_NOINLINE static void bmesh_push(const Object &object,
                                    const bke::pbvh::BMeshNode *node,
                                    Type type)
{
  StepData *step_data = get_step_data();
  const SculptSession &ss = *object.sculpt;

  std::scoped_lock lock(step_data->nodes_mutex);

  if (step_data->nodes.is_empty()) {
    /* We currently need to append data here so that the overall undo system knows to indicate that
     * data should be flushed to the memfile */
    /* TODO: Once we store entering Sculpt Mode as a specific type of action, we can remove this
     * call. */
    step_data->nodes.append(std::make_unique<Node>());

    step_data->type = type;

    if (type == Type::DyntopoEnd) {
      step_data->bmesh.bm_entry = BM_log_entry_add(ss.bm_log);
      BM_log_before_all_removed(ss.bm, ss.bm_log);
    }
    else if (type == Type::DyntopoBegin) {
      /* Store a copy of the mesh's current vertices, loops, and
       * faces. A full copy like this is needed because entering
       * dynamic-topology immediately does topological edits
       * (converting faces to triangles) that the BMLog can't
       * fully restore from. */
      NodeGeometry *geometry = &step_data->bmesh.geometry_enter;
      store_geometry_data(geometry, object);

      step_data->bmesh.bm_entry = BM_log_entry_add(ss.bm_log);
      BM_log_all_added(ss.bm, ss.bm_log);
    }
    else {
      step_data->bmesh.bm_entry = BM_log_entry_add(ss.bm_log);
    }
  }

  if (node) {
    const int cd_vert_mask_offset = CustomData_get_offset_named(
        &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

    /* The vertices and node aren't changed, though pointers to them are stored in the log. */
    bke::pbvh::BMeshNode *node_mut = const_cast<bke::pbvh::BMeshNode *>(node);

    switch (type) {
      case Type::None:
        BLI_assert_unreachable();
        break;
      case Type::Position:
      case Type::Mask:
        /* Before any vertex values get modified, ensure their
         * original positions are logged. */
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }
        for (BMVert *vert : BKE_pbvh_bmesh_node_other_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }
        break;

      case Type::HideFace:
      case Type::HideVert: {
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }
        for (BMVert *vert : BKE_pbvh_bmesh_node_other_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }

        for (BMFace *f : BKE_pbvh_bmesh_node_faces(node_mut)) {
          BM_log_face_modified(ss.bm_log, f);
        }
        break;
      }

      case Type::DyntopoBegin:
      case Type::DyntopoEnd:
      case Type::Geometry:
      case Type::FaceSet:
      case Type::Color:
        break;
    }
  }
}

/**
 * Add an undo node for the bke::pbvh::Tree node to the step's storage. If the node was
 * newly created and needs to be filled with data, set \a r_new to true.
 */
static Node *ensure_node(StepData &step_data, const bke::pbvh::Node &node, bool &r_new)
{
  std::scoped_lock lock(step_data.nodes_mutex);
  r_new = false;
  std::unique_ptr<Node> &unode = step_data.undo_nodes_by_pbvh_node.lookup_or_add_cb(&node, [&]() {
    std::unique_ptr<Node> new_unode = std::make_unique<Node>();
    r_new = true;
    return new_unode;
  });
  return unode.get();
}

void push_node(const Depsgraph &depsgraph,
               const Object &object,
               const bke::pbvh::Node *node,
               const Type type)
{
  SculptSession &ss = *object.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (ss.bm || ELEM(type, Type::DyntopoBegin, Type::DyntopoEnd)) {
    bmesh_push(object, static_cast<const bke::pbvh::BMeshNode *>(node), type);
    return;
  }

  StepData *step_data = get_step_data();
  BLI_assert(ELEM(step_data->type, Type::None, type));
  step_data->type = type;

  bool newly_added;
  Node *unode = ensure_node(*step_data, *node, newly_added);
  if (!newly_added) {
    /* The node was already filled with data for this undo step. */
    return;
  }

  ss.needs_flush_to_id = true;

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh:
      fill_node_data_mesh(
          depsgraph, object, static_cast<const bke::pbvh::MeshNode &>(*node), type, *unode);
      break;
    case bke::pbvh::Type::Grids:
      fill_node_data_grids(object, static_cast<const bke::pbvh::GridsNode &>(*node), type, *unode);
      break;
    case bke::pbvh::Type::BMesh:
      BLI_assert_unreachable();
      break;
  }
}

void push_nodes(const Depsgraph &depsgraph,
                Object &object,
                const IndexMask &node_mask,
                const Type type)
{
  SculptSession &ss = *object.sculpt;

  ss.needs_flush_to_id = true;

  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(object);
  if (ss.bm || ELEM(type, Type::DyntopoBegin, Type::DyntopoEnd)) {
    const Span<bke::pbvh::BMeshNode> nodes = pbvh.nodes<bke::pbvh::BMeshNode>();
    node_mask.foreach_index([&](const int i) { bmesh_push(object, &nodes[i], type); });
    return;
  }

  StepData *step_data = get_step_data();
  BLI_assert(ELEM(step_data->type, Type::None, type));
  step_data->type = type;

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Span<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();
      Vector<std::pair<const bke::pbvh::MeshNode *, Node *>, 32> nodes_to_fill;
      node_mask.foreach_index([&](const int i) {
        bool newly_added;
        Node *unode = ensure_node(*step_data, nodes[i], newly_added);
        if (newly_added) {
          nodes_to_fill.append({&nodes[i], unode});
        }
      });
      threading::parallel_for(nodes_to_fill.index_range(), 1, [&](const IndexRange range) {
        for (const auto &[node, unode] : nodes_to_fill.as_span().slice(range)) {
          fill_node_data_mesh(depsgraph, object, *node, type, *unode);
        }
      });
      break;
    }
    case bke::pbvh::Type::Grids: {
      const Span<bke::pbvh::GridsNode> nodes = pbvh.nodes<bke::pbvh::GridsNode>();
      Vector<std::pair<const bke::pbvh::GridsNode *, Node *>, 32> nodes_to_fill;
      node_mask.foreach_index([&](const int i) {
        bool newly_added;
        Node *unode = ensure_node(*step_data, nodes[i], newly_added);
        if (newly_added) {
          nodes_to_fill.append({&nodes[i], unode});
        }
      });
      threading::parallel_for(nodes_to_fill.index_range(), 1, [&](const IndexRange range) {
        for (const auto &[node, unode] : nodes_to_fill.as_span().slice(range)) {
          fill_node_data_grids(object, *node, type, *unode);
        }
      });
      break;
    }
    case bke::pbvh::Type::BMesh: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void save_active_attribute(Object &object, SculptAttrRef *attr)
{
  Mesh *mesh = BKE_object_get_original_mesh(&object);
  attr->was_set = true;
  attr->domain = NO_ACTIVE_LAYER;
  attr->name[0] = 0;
  if (!mesh) {
    return;
  }
  const char *name = mesh->active_color_attribute;
  const bke::AttributeAccessor attributes = mesh->attributes();
  const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(name);
  if (!bke::mesh::is_color_attribute(meta_data)) {
    return;
  }
  attr->domain = meta_data->domain;
  STRNCPY_UTF8(attr->name, name);
  attr->type = *bke::attr_type_to_custom_data_type(meta_data->data_type);
}

/**
 * Does not save topology counts, as that data is unneeded for full geometry pushes and
 * requires the PBVH to exist.
 */
static void save_common_data(Object &ob, SculptUndoStep *us)
{
  us->data.object_name = ob.id.name;

  if (!us->active_color_start.was_set) {
    save_active_attribute(ob, &us->active_color_start);
  }

  /* Set end attribute in case push_end is not called,
   * so we don't end up with corrupted state.
   */
  if (!us->active_color_end.was_set) {
    save_active_attribute(ob, &us->active_color_end);
    us->active_color_end.was_set = false;
  }

  const SculptSession &ss = *ob.sculpt;

  us->data.pivot_pos = ss.pivot_pos;
  us->data.pivot_rot = ss.pivot_rot;

  if (const KeyBlock *key = BKE_keyblock_from_object(&ob)) {
    us->data.active_shape_key_name = key->name;
  }
}

void push_begin_ex(const Scene & /*scene*/, Object &ob, const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();

  /* If possible, we need to tag the object and its geometry data as 'changed in the future' in
   * the previous undo step if it's a memfile one. */
  ED_undosys_stack_memfile_id_changed_tag(ustack, &ob.id);
  ED_undosys_stack_memfile_id_changed_tag(ustack, static_cast<ID *>(ob.data));

  /* Special case, we never read from this. */
  bContext *C = nullptr;

  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(
      BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_SCULPT));

  const SculptSession &ss = *ob.sculpt;
  const bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);

  save_common_data(ob, us);

  switch (pbvh.type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      us->data.mesh.verts_num = mesh.verts_num;
      us->data.mesh.corners_num = mesh.corners_num;
      break;
    }
    case bke::pbvh::Type::Grids: {
      us->data.grids.grids_num = ss.subdiv_ccg->grids_num;
      us->data.grids.grid_size = ss.subdiv_ccg->grid_size;
      break;
    }
    case bke::pbvh::Type::BMesh: {
      break;
    }
  }
}

void push_begin(const Scene &scene, Object &ob, const wmOperator *op)
{
  push_begin_ex(scene, ob, op->type->name);
}

void push_enter_sculpt_mode(const Scene & /*scene*/, Object &ob, const wmOperator *op)
{
  UndoStack *ustack = ED_undo_stack_get();

  /* If possible, we need to tag the object and its geometry data as 'changed in the future' in
   * the previous undo step if it's a memfile one. */
  ED_undosys_stack_memfile_id_changed_tag(ustack, &ob.id);
  ED_undosys_stack_memfile_id_changed_tag(ustack, static_cast<ID *>(ob.data));

  /* Special case, we never read from this. */
  bContext *C = nullptr;

  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(
      BKE_undosys_step_push_init_with_type(ustack, C, op->type->name, BKE_UNDOSYS_TYPE_SCULPT));
  save_common_data(ob, us);
}

static size_t node_size_in_bytes(const Node &node)
{
  size_t size = sizeof(Node);
  size += node.position.as_span().size_in_bytes();
  size += node.orig_position.as_span().size_in_bytes();
  size += node.normal.as_span().size_in_bytes();
  size += node.col.as_span().size_in_bytes();
  size += node.mask.as_span().size_in_bytes();
  size += node.loop_col.as_span().size_in_bytes();
  size += node.vert_indices.as_span().size_in_bytes();
  size += node.corner_indices.as_span().size_in_bytes();
  size += node.vert_hidden.size() / 8;
  size += node.face_hidden.size() / 8;
  size += node.grids.as_span().size_in_bytes();
  size += node.grid_hidden.all_bits().size() / 8;
  size += node.face_sets.as_span().size_in_bytes();
  size += node.face_indices.as_span().size_in_bytes();
  return size;
}

void push_end_ex(Object &ob, const bool use_nested_undo)
{
  StepData *step_data = get_step_data();

  /* Move undo node storage from map to vector. */
  step_data->nodes.reserve(step_data->undo_nodes_by_pbvh_node.size());
  for (std::unique_ptr<Node> &node : step_data->undo_nodes_by_pbvh_node.values()) {
    step_data->nodes.append(std::move(node));
  }
  step_data->undo_nodes_by_pbvh_node.clear();

  /* We don't need normals in the undo stack. */
  for (std::unique_ptr<Node> &unode : step_data->nodes) {
    unode->normal = {};
  }
  /* TODO: When #Node.orig_positions is stored, #Node.positions is unnecessary, don't keep it in
   * the stored undo step. In the future the stored undo step should use a different format with
   * just one positions array that has a different semantic meaning depending on whether there are
   * deform modifiers. */

  if (step_data->type == Type::Position) {
    step_data->position_step_storage = std::make_unique<PositionUndoStorage>(*step_data);
  }
  else {
    step_data->undo_size = threading::parallel_reduce(
        step_data->nodes.index_range(),
        16,
        0,
        [&](const IndexRange range, size_t size) {
          for (const int i : range) {
            size += node_size_in_bytes(*step_data->nodes[i]);
          }
          return size;
        },
        std::plus<size_t>());
  }

  /* We could remove this and enforce all callers run in an operator using 'OPTYPE_UNDO'. */
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  if (wm->op_undo_depth == 0 || use_nested_undo) {
    UndoStack *ustack = ED_undo_stack_get();
    BKE_undosys_step_push(ustack, nullptr, nullptr);
    if (wm->op_undo_depth == 0) {
      BKE_undosys_stack_limit_steps_and_memory_defaults(ustack);
    }
    WM_file_tag_modified();
  }

  UndoStack *ustack = ED_undo_stack_get();
  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(
      BKE_undosys_stack_init_or_active_with_type(ustack, BKE_UNDOSYS_TYPE_SCULPT));

  save_active_attribute(ob, &us->active_color_end);
}

void push_end(Object &ob)
{
  push_end_ex(ob, false);
}

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

static void set_active_layer(bContext *C, const SculptAttrRef *attr_ref)
{
  if (attr_ref->domain == bke::AttrDomain::Auto) {
    return;
  }

  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  SculptAttrRef existing;
  save_active_attribute(*ob, &existing);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  /* Temporary fix for #97408. This is a fundamental
   * bug in the undo stack; the operator code needs to push
   * an extra undo step before running an operator if a
   * non-memfile undo system is active.
   *
   * For now, detect if the layer does exist but with a different
   * domain and just unconvert it.
   */
  if (const bke::GAttributeReader attr = attributes.lookup(attr_ref->name)) {
    if (attr.domain != attr_ref->domain ||
        bke::cpp_type_to_custom_data_type(attr.varray.type()) != attr_ref->type)
    {
      AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
      if (ed::geometry::convert_attribute(owner,
                                          mesh->attributes_for_write(),
                                          attr_ref->name,
                                          attr_ref->domain,
                                          *bke::custom_data_type_to_attr_type(attr_ref->type),
                                          nullptr))
      {
      }
    }
  }

  if (!attributes.contains(attr_ref->name)) {
    /* Memfile undo killed the layer; re-create it. */
    mesh->attributes_for_write().add(attr_ref->name,
                                     attr_ref->domain,
                                     *bke::custom_data_type_to_attr_type(attr_ref->type),
                                     bke::AttributeInitDefaultValue());
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (attributes.contains(attr_ref->name)) {
    BKE_id_attributes_active_color_set(&mesh->id, attr_ref->name);
  }
}

static void step_encode_init(bContext * /*C*/, UndoStep *us_p)
{
  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(us_p);
  new (&us->data) StepData();
}

static bool step_encode(bContext * /*C*/, Main *bmain, UndoStep *us_p)
{
  /* Dummy, encoding is done along the way by adding tiles
   * to the current 'SculptUndoStep' added by encode_init. */
  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(us_p);
  us->step.data_size = us->data.undo_size;

  if (us->data.type == Type::DyntopoEnd) {
    us->step.use_memfile_step = true;
  }
  us->step.is_applied = true;

  /* We do not flush data when entering sculpt mode - this is currently indicated by Type::None */
  if (us->data.type != Type::None) {
    bmain->is_memfile_undo_flush_needed = true;
  }

  return true;
}

static void step_decode_undo_impl(bContext *C, Depsgraph *depsgraph, SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);

  restore_list(C, depsgraph, us->data);
  us->step.is_applied = false;
}

static void step_decode_redo_impl(bContext *C, Depsgraph *depsgraph, SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);

  restore_list(C, depsgraph, us->data);
  us->step.is_applied = true;
}

static void step_decode_undo(bContext *C,
                             Depsgraph *depsgraph,
                             SculptUndoStep *us,
                             const bool is_final)
{
  /* Walk forward over any applied steps of same type,
   * then walk back in the next loop, un-applying them. */
  SculptUndoStep *us_iter = us;
  while (us_iter->step.next && (us_iter->step.next->type == us_iter->step.type)) {
    if (us_iter->step.next->is_applied == false) {
      break;
    }
    us_iter = reinterpret_cast<SculptUndoStep *>(us_iter->step.next);
  }

  while ((us_iter != us) || (!is_final && us_iter == us)) {
    BLI_assert(us_iter->step.type == us->step.type); /* Previous loop ensures this. */

    set_active_layer(C, &us_iter->active_color_start);
    step_decode_undo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      if (us_iter->step.prev && us_iter->step.prev->type == BKE_UNDOSYS_TYPE_SCULPT) {
        set_active_layer(
            C, &reinterpret_cast<SculptUndoStep *>(us_iter->step.prev)->active_color_end);
      }
      break;
    }

    us_iter = reinterpret_cast<SculptUndoStep *>(us_iter->step.prev);
  }
}

static void step_decode_redo(bContext *C, Depsgraph *depsgraph, SculptUndoStep *us)
{
  SculptUndoStep *us_iter = us;
  while (us_iter->step.prev && (us_iter->step.prev->type == us_iter->step.type)) {
    if (us_iter->step.prev->is_applied == true) {
      break;
    }
    us_iter = reinterpret_cast<SculptUndoStep *>(us_iter->step.prev);
  }
  while (us_iter && (us_iter->step.is_applied == false)) {
    set_active_layer(C, &us_iter->active_color_end);
    step_decode_redo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      set_active_layer(C, &us_iter->active_color_start);
      break;
    }
    us_iter = reinterpret_cast<SculptUndoStep *>(us_iter->step.next);
  }
}

static void step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir dir, const bool is_final)
{
  /* NOTE: behavior for undo/redo closely matches image undo. */
  BLI_assert(dir != STEP_INVALID);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* Ensure sculpt mode. */
  {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *ob = BKE_view_layer_active_object_get(view_layer);
    if (ob && (ob->type == OB_MESH)) {
      if (ob->mode & (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT)) {
        /* Pass. */
      }
      else {
        object::mode_generic_exit(bmain, depsgraph, scene, ob);

        /* Sculpt needs evaluated state.
         * NOTE: needs to be done here, as #object::mode_generic_exit will usually invalidate
         * (some) evaluated data. */
        BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

        Mesh *mesh = static_cast<Mesh *>(ob->data);
        /* Don't add sculpt topology undo steps when reading back undo state.
         * The undo steps must enter/exit for us. */
        mesh->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
        object_sculpt_mode_enter(*bmain, *depsgraph, *scene, *ob, true, nullptr);
      }

      if (ob->sculpt) {
        ob->sculpt->needs_flush_to_id = true;
      }
      bmain->is_memfile_undo_flush_needed = true;
    }
    else {
      BLI_assert(0);
      return;
    }
  }

  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(us_p);
  if (dir == STEP_UNDO) {
    step_decode_undo(C, depsgraph, us, is_final);
  }
  else if (dir == STEP_REDO) {
    step_decode_redo(C, depsgraph, us);
  }
}

static void step_free(UndoStep *us_p)
{
  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(us_p);
  free_step_data(us->data);
}

void geometry_begin(const Scene &scene, Object &ob, const wmOperator *op)
{
  geometry_begin_ex(scene, ob, op->type->name);
}

void geometry_begin_ex(const Scene & /*scene*/, Object &ob, const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();

  /* If possible, we need to tag the object and its geometry data as 'changed in the future' in
   * the previous undo step if it's a memfile one. */
  ED_undosys_stack_memfile_id_changed_tag(ustack, &ob.id);
  ED_undosys_stack_memfile_id_changed_tag(ustack, static_cast<ID *>(ob.data));

  /* Special case, we never read from this. */
  bContext *C = nullptr;

  SculptUndoStep *us = reinterpret_cast<SculptUndoStep *>(
      BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_SCULPT));
  save_common_data(ob, us);
  geometry_push(ob);
}

static size_t calculate_node_geometry_allocated_size(const NodeGeometry &node_geometry)
{
  BLI_assert(node_geometry.is_initialized);

  MemoryCount memory;
  MemoryCounter memory_counter(memory);

  memory_counter.add_shared(node_geometry.face_offsets_sharing_info,
                            sizeof(int) * (node_geometry.faces_num + 1));

  CustomData_count_memory(node_geometry.corner_data, node_geometry.corners_num, memory_counter);
  CustomData_count_memory(node_geometry.face_data, node_geometry.faces_num, memory_counter);
  CustomData_count_memory(node_geometry.vert_data, node_geometry.verts_num, memory_counter);
  CustomData_count_memory(node_geometry.edge_data, node_geometry.edges_num, memory_counter);

  return memory.total_bytes;
}

static size_t estimate_geometry_step_size(const StepData &step_data)
{
  size_t step_size = 0;

  /* TODO: This calculation is not entirely accurate, as the current amount of memory consumed by
   * Sculpt Undo is not updated when elements are evicted. Further changes to the overall undo
   * system would be needed to measure this accurately. */
  step_size += calculate_node_geometry_allocated_size(step_data.geometry_original);
  step_size += calculate_node_geometry_allocated_size(step_data.geometry_modified);

  return step_size;
}

void geometry_end(Object &ob)
{
  geometry_push(ob);

  StepData *step_data = get_step_data();
  step_data->undo_size = estimate_geometry_step_size(*step_data);

  /* We could remove this and enforce all callers run in an operator using 'OPTYPE_UNDO'. */
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  if (wm->op_undo_depth == 0) {
    UndoStack *ustack = ED_undo_stack_get();
    BKE_undosys_step_push(ustack, nullptr, nullptr);
    if (wm->op_undo_depth == 0) {
      BKE_undosys_stack_limit_steps_and_memory_defaults(ustack);
    }
    WM_file_tag_modified();
  }
}

void register_type(UndoType *ut)
{
  ut->name = "Sculpt";
  ut->poll = nullptr; /* No poll from context for now. */
  ut->step_encode_init = step_encode_init;
  ut->step_encode = step_encode;
  ut->step_decode = step_decode;
  ut->step_free = step_free;

  ut->flags = UNDOTYPE_FLAG_DECODE_ACTIVE_STEP;

  ut->step_size = sizeof(SculptUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Multires + Base Mesh Undo
 *
 * Example of a relevant operator is Apply Base.
 *
 * Usage:
 *
 *   static int operator_exec((bContext *C, wmOperator *op) {
 *
 *      ed::sculpt_paint::undo::push_multires_mesh_begin(C, op->type->name);
 *      // Modify base mesh.
 *      ed::sculpt_paint::undo::push_multires_mesh_end(C, op->type->name);
 *
 *      return OPERATOR_FINISHED;
 *   }
 *
 * If object is not in Sculpt mode or there is no active multires object, ED_undo_push is used
 * instead.
 * \{ */

static bool use_multires_mesh(bContext *C)
{
  if (BKE_paintmode_get_active_from_context(C) != PaintMode::Sculpt) {
    return false;
  }

  const Object *object = CTX_data_active_object(C);
  const SculptSession *sculpt_session = object->sculpt;

  return sculpt_session->multires.active;
}

void push_multires_mesh_begin(bContext *C, const char *str)
{
  if (!use_multires_mesh(C)) {
    return;
  }

  const Scene &scene = *CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);

  multires_flush_sculpt_updates(object);

  push_begin_ex(scene, *object, str);

  geometry_push(*object);
}

void push_multires_mesh_end(bContext *C, const char *str)
{
  if (!use_multires_mesh(C)) {
    ED_undo_push(C, str);
    return;
  }

  Object *object = CTX_data_active_object(C);

  geometry_push(*object);

  push_end(*object);
}

/** \} */

}  // namespace blender::ed::sculpt_paint::undo

namespace blender::ed::sculpt_paint {

std::optional<OrigPositionData> orig_position_data_lookup_mesh_all_verts(
    const Object & /*object*/, const bke::pbvh::MeshNode &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Position);
  if (!unode) {
    return std::nullopt;
  }
  return OrigPositionData{unode->position.as_span(), unode->normal.as_span()};
}

std::optional<OrigPositionData> orig_position_data_lookup_mesh(const Object &object,
                                                               const bke::pbvh::MeshNode &node)
{
  const std::optional<OrigPositionData> result = orig_position_data_lookup_mesh_all_verts(object,
                                                                                          node);
  if (!result) {
    return std::nullopt;
  }
  return OrigPositionData{result->positions.take_front(node.verts().size()),
                          result->normals.take_front(node.verts().size())};
}

std::optional<OrigPositionData> orig_position_data_lookup_grids(const Object & /*object*/,
                                                                const bke::pbvh::GridsNode &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Position);
  if (!unode) {
    return std::nullopt;
  }
  return OrigPositionData{unode->position.as_span(), unode->normal.as_span()};
}

void orig_position_data_gather_bmesh(const BMLog &bm_log,
                                     const Set<BMVert *, 0> &verts,
                                     const MutableSpan<float3> positions,
                                     const MutableSpan<float3> normals)
{
  int i = 0;
  for (const BMVert *vert : verts) {
    const float *co;
    const float *no;
    BM_log_original_vert_data(&const_cast<BMLog &>(bm_log), const_cast<BMVert *>(vert), &co, &no);
    if (!positions.is_empty()) {
      positions[i] = co;
    }
    if (!normals.is_empty()) {
      normals[i] = no;
    }
    i++;
  }
}

std::optional<Span<float4>> orig_color_data_lookup_mesh(const Object & /*object*/,
                                                        const bke::pbvh::MeshNode &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Color);
  if (!unode) {
    return std::nullopt;
  }
  return unode->col.as_span();
}

std::optional<Span<int>> orig_face_set_data_lookup_mesh(const Object & /*object*/,
                                                        const bke::pbvh::MeshNode &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::FaceSet);
  if (!unode) {
    return std::nullopt;
  }
  return unode->face_sets.as_span();
}

std::optional<Span<int>> orig_face_set_data_lookup_grids(const Object & /*object*/,
                                                         const bke::pbvh::GridsNode &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::FaceSet);
  if (!unode) {
    return std::nullopt;
  }
  return unode->face_sets.as_span();
}

std::optional<Span<float>> orig_mask_data_lookup_mesh(const Object & /*object*/,
                                                      const bke::pbvh::MeshNode &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Mask);
  if (!unode) {
    return std::nullopt;
  }
  return unode->mask.as_span();
}

std::optional<Span<float>> orig_mask_data_lookup_grids(const Object & /*object*/,
                                                       const bke::pbvh::GridsNode &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Mask);
  if (!unode) {
    return std::nullopt;
  }
  return unode->mask.as_span();
}

}  // namespace blender::ed::sculpt_paint
