/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BKE_object_types.hh"

#include "DRW_gpu_wrapper.hh"

#include "GPU_select.hh"

#include "../intern/gpu_select_private.hh"

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "gpu_shader_create_info.hh"

#include "select_defines.hh"
#include "select_shader_shared.hh"

namespace blender::draw::select {

// #define DEBUG_PRINT

enum class SelectionType { DISABLED = 0, ENABLED = 1 };

class ID {
 private:
  uint32_t value;

  /* Add type safety to selection ID. Only the select types should provide them. */
  ID(uint32_t value) : value(value) {};

  friend struct SelectBuf;
  friend struct SelectMap;

 public:
  uint32_t get() const
  {
    return value;
  }
};

/**
 * Add a dedicated selection id buffer to a pass.
 * To be used when not using a #PassMain which can pass the select ID via CustomID.
 */
struct SelectBuf {
  const SelectionType selection_type;

  StorageVectorBuffer<uint32_t> select_buf = {"select_buf"};

  SelectBuf(const SelectionType selection_type) : selection_type(selection_type) {};

  void select_clear()
  {
    if (selection_type != SelectionType::DISABLED) {
      select_buf.clear();
    }
  }

  void select_append(ID select_id)
  {
    if (selection_type != SelectionType::DISABLED) {
      select_buf.append(select_id.get());
    }
  }

  void select_bind(PassSimple::Sub &pass)
  {
    if (selection_type != SelectionType::DISABLED) {
      select_buf.push_update();
      pass.bind_ssbo(SELECT_ID_IN, &select_buf);
    }
  }
};

/**
 * Generate selection IDs from objects and keep record of the mapping between them.
 * The id's are contiguous so that we can create a destination buffer.
 */
struct SelectMap {
  const SelectionType selection_type;

  /** Mapping between internal IDs and `object->runtime->select_id`. */
  Vector<uint> select_id_map;
  /** Track objects with OB_DRAW_IN_FRONT. */
  Vector<bool> in_front_map;
#ifndef NDEBUG
  /** Debug map containing a copy of the object name. */
  Vector<std::string> map_names;
#endif
  /** Stores the result of the whole selection drawing. Content depends on selection mode. */
  StorageArrayBuffer<uint> select_output_buf = {"select_output_buf"};
  /** Dummy buffer. Might be better to remove, but simplify the shader create info patching. */
  StorageArrayBuffer<uint, 4, true> dummy_select_buf = {"dummy_select_buf"};
  /** Uniform buffer to bind to all passes to pass information about the selection state. */
  UniformBuffer<SelectInfoData> info_buf;
  /** If clipping is enabled, this is the number of clip planes to enable. */
  int clipping_plane_count = 0;

  SelectMap(const SelectionType selection_type) : selection_type(selection_type) {};

  /* TODO(fclem): The sub_object_id id should eventually become some enum or take a sub-object
   * reference directly. This would isolate the selection logic to this class. */
  [[nodiscard]] const ID select_id(const ObjectRef &ob_ref, uint sub_object_id = 0)
  {
    if (selection_type == SelectionType::DISABLED) {
      return {0};
    }

    if (sub_object_id == uint(-1)) {
      /* WORKAROUND: Armature code set the sub_object_id to -1 when individual bones are not
       * selectable (i.e. in object mode). */
      sub_object_id = 0;
    }

    uint object_id = ob_ref.object->runtime->select_id;
    uint id = select_id_map.append_and_get_index(object_id | sub_object_id);
    in_front_map.append(ob_ref.object->dtx & OB_DRAW_IN_FRONT);

#ifdef DEBUG_PRINT
    /* Print mapping from object name, select id and the mapping to internal select id.
     * If something is wrong at this stage, it indicates an error in the caller code. */
    printf("%s : %u | %u = %u -> %u\n",
           ob_ref.object->id.name,
           object_id,
           sub_object_id,
           object_id | sub_object_id,
           id);
#endif

#ifndef NDEBUG
    map_names.append(ob_ref.object->id.name);
#endif
    return {id};
  }

  /* TODO: refactor this method to select::ID::invalid(). */
  /* Load an invalid index that will not write to the output (not selectable). */
  [[nodiscard]] static const ID select_invalid_id()
  {
    return {uint32_t(-1)};
  }

  void begin_sync(int clipping_plane_count)
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    this->clipping_plane_count = clipping_plane_count;

    select_id_map.clear();
    in_front_map.clear();
#ifndef NDEBUG
    map_names.clear();
#endif
  }

  /** IMPORTANT: Changes the draw state. Need to be called after the pass's own state_set. */
  void select_bind(PassSimple &pass)
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    pass.state_set(DRW_STATE_WRITE_COLOR, clipping_plane_count);
    pass.bind_ubo(SELECT_DATA, &info_buf);
    pass.bind_ssbo(SELECT_ID_OUT, &select_output_buf);
  }

  /** IMPORTANT: Changes the draw state. Need to be called after the pass's own state_set. */
  void select_bind(PassMain &pass)
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    pass.use_custom_ids = true;
    pass.state_set(DRW_STATE_WRITE_COLOR, clipping_plane_count);
    pass.bind_ubo(SELECT_DATA, &info_buf);
    /* IMPORTANT: This binds a dummy buffer `in_select_buf` but it is not supposed to be used. */
    pass.bind_ssbo(SELECT_ID_IN, &dummy_select_buf);
    pass.bind_ssbo(SELECT_ID_OUT, &select_output_buf);
  }

  /* TODO: Deduplicate. */
  /** IMPORTANT: Changes the draw state. Need to be called after the pass's own state_set. */
  void select_bind(PassMain &pass, PassMain::Sub &sub)
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    pass.use_custom_ids = true;
    sub.state_set(DRW_STATE_WRITE_COLOR, clipping_plane_count);
    sub.bind_ubo(SELECT_DATA, &info_buf);
    /* IMPORTANT: This binds a dummy buffer `in_select_buf` but it is not supposed to be used. */
    sub.bind_ssbo(SELECT_ID_IN, &dummy_select_buf);
    sub.bind_ssbo(SELECT_ID_OUT, &select_output_buf);
  }

  void end_sync()
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    BLI_assert(select_id_map.size() == in_front_map.size());

    select_output_buf.resize(max_uu(ceil_to_multiple_u(select_id_map.size(), 4), 4));
    select_output_buf.push_update();
  }

  void pre_draw()
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    switch (gpu_select_next_get_mode()) {
      /* Should not be used anymore for viewport selection. */
      case GPU_SELECT_NEAREST_FIRST_PASS:
      case GPU_SELECT_NEAREST_SECOND_PASS:
      case GPU_SELECT_INVALID:
        BLI_assert_unreachable();
        break;
      case GPU_SELECT_ALL:
        info_buf.mode = SelectType::SELECT_ALL;
        info_buf.cursor = int2(0);
        /* This mode uses atomicOr and store result as a bitmap. Clear to 0 (no selection). */
        GPU_storagebuf_clear(select_output_buf, 0);
        break;
      case GPU_SELECT_PICK_ALL:
        info_buf.mode = SelectType::SELECT_PICK_ALL;
        info_buf.cursor = int2(gpu_select_next_get_pick_area_center());
        /* Mode uses atomicMin. Clear to UINT_MAX. */
        GPU_storagebuf_clear(select_output_buf, 0xFFFFFFFFu);
        break;
      case GPU_SELECT_PICK_NEAREST:
        info_buf.mode = SelectType::SELECT_PICK_NEAREST;
        info_buf.cursor = int2(gpu_select_next_get_pick_area_center());
        /* Mode uses atomicMin. Clear to UINT_MAX. */
        GPU_storagebuf_clear(select_output_buf, 0xFFFFFFFFu);
        break;
    }
    info_buf.push_update();
  }

  void read_result()
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
    /* This flush call should not be required. Still, on non-unified memory architecture
     * Apple devices this is needed for the result to be host visible.
     * This is likely to be a bug in the GPU backend.
     * So it should eventually be transformed into a backend
     * workaround instead of being fixed in user code. */
    select_output_buf.async_flush_to_host();
    select_output_buf.read();

    Vector<GPUSelectResult> hit_results;

    /* Convert raw data from GPU to #GPUSelectResult. */
    switch (info_buf.mode) {
      case SelectType::SELECT_ALL:
        for (auto i : IndexRange(select_id_map.size())) {
          if (((select_output_buf[i / 32] >> (i % 32)) & 1) != 0) {
            GPUSelectResult hit_result{};
            hit_result.id = select_id_map[i];
            hit_result.depth = 0xFFFF;
            hit_results.append(hit_result);
          }
        }
        break;

      case SelectType::SELECT_PICK_ALL:
        for (auto i : IndexRange(select_id_map.size())) {
          if (select_output_buf[i] != 0xFFFFFFFFu) {
            GPUSelectResult hit_result{};
            hit_result.id = select_id_map[i];
            hit_result.depth = select_output_buf[i];
            if (in_front_map[i]) {
              /* Divide "In Front" objects depth so they go first. */
              /* TODO(Miguel Pozo): This reproduces the previous engine behavior, but it breaks
               * with code using depth for position reconstruction. Should we improve this? */
              float offset_depth = *reinterpret_cast<float *>(&hit_result.depth) / 100.0f;
              hit_result.depth = *reinterpret_cast<uint32_t *>(&offset_depth);
            }
            hit_results.append(hit_result);
          }
        }
        break;

      case SelectType::SELECT_PICK_NEAREST:
        for (auto i : IndexRange(select_id_map.size())) {
          if (select_output_buf[i] != 0xFFFFFFFFu) {
            /* NOTE: For `SELECT_PICK_NEAREST`, `select_output_buf` also contains the screen
             * distance to cursor in the lowest bits. */
            GPUSelectResult hit_result{};
            hit_result.id = select_id_map[i];
            hit_result.depth = select_output_buf[i];
            if (in_front_map[i]) {
              /* Divide "In Front" objects depth so they go first. */
              const uint32_t depth_mask = 0x00FFFFFFu << 8u;
              uint32_t offset_depth = ((hit_result.depth & depth_mask) >> 8u) / 100;
              hit_result.depth &= ~depth_mask;
              hit_result.depth |= offset_depth << 8u;
            }
            if (hit_results.is_empty() || hit_result.depth < hit_results[0].depth) {
              hit_results = {hit_result};
            }
          }
        }
        break;
    }
#ifdef DEBUG_PRINT
    for (auto &hit : hit_results) {
      /* Print hit results right out of the GPU selection buffer.
       * If something is wrong at this stage, it indicates an error in the selection shaders. */
      printf(" hit: %u: depth %u\n", hit.id, hit.depth);
    }
#endif

    gpu_select_next_set_result(hit_results.data(), hit_results.size());
  }
};

}  // namespace blender::draw::select
