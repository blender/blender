/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 */

#pragma once

#include "DRW_gpu_wrapper.hh"

#include "GPU_select.h"

#include "../intern/gpu_select_private.h"

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "gpu_shader_create_info.hh"

#include "select_defines.h"
#include "select_shader_shared.hh"

namespace blender::draw::select {

enum class SelectionType { DISABLED = 0, ENABLED = 1 };

class ID {
 private:
  uint32_t value;

  /* Add type safety to selection ID. Only the select types should provide them. */
  ID(uint32_t value) : value(value){};

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

  SelectBuf(const SelectionType selection_type) : selection_type(selection_type){};

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

  void select_bind(PassSimple &pass)
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

  /** Mapping between internal IDs and `object->runtime.select_id`. */
  Vector<uint> select_id_map;
#ifdef DEBUG
  /** Debug map containing a copy of the object name. */
  Vector<std::string> map_names;
#endif
  /** Stores the result of the whole selection drawing. Content depends on selection mode. */
  StorageArrayBuffer<uint> select_output_buf = {"select_output_buf"};
  /** Dummy buffer. Might be better to remove, but simplify the shader create info patching. */
  StorageArrayBuffer<uint, 4, true> dummy_select_buf = {"dummy_select_buf"};
  /** Uniform buffer to bind to all passes to pass information about the selection state. */
  UniformBuffer<SelectInfoData> info_buf;
  /** Will remove the depth test state from any pass drawing objects with select id. */
  bool disable_depth_test;

  SelectMap(const SelectionType selection_type) : selection_type(selection_type){};

  /* TODO(fclem): The sub_object_id id should eventually become some enum or take a sub-object
   * reference directly. This would isolate the selection logic to this class. */
  [[nodiscard]] const ID select_id(const ObjectRef &ob_ref, uint sub_object_id = 0)
  {
    if (selection_type == SelectionType::DISABLED) {
      return {0};
    }

    uint object_id = ob_ref.object->runtime.select_id;
    uint id = select_id_map.append_and_get_index(object_id | sub_object_id);
#ifdef DEBUG
    map_names.append(ob_ref.object->id.name);
#endif
    return {id};
  }

  /* Load an invalid index that will not write to the output (not selectable). */
  [[nodiscard]] const ID select_invalid_id()
  {
    return {uint32_t(-1)};
  }

  void begin_sync()
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    switch (gpu_select_next_get_mode()) {
      case GPU_SELECT_ALL:
        info_buf.mode = SelectType::SELECT_ALL;
        disable_depth_test = true;
        break;
      /* Not sure if these 2 NEAREST are mapped to the right algorithm. */
      case GPU_SELECT_NEAREST_FIRST_PASS:
      case GPU_SELECT_NEAREST_SECOND_PASS:
      case GPU_SELECT_PICK_ALL:
        info_buf.mode = SelectType::SELECT_PICK_ALL;
        info_buf.cursor = int2(gpu_select_next_get_pick_area_center());
        disable_depth_test = true;
        break;
      case GPU_SELECT_PICK_NEAREST:
        info_buf.mode = SelectType::SELECT_PICK_NEAREST;
        info_buf.cursor = int2(gpu_select_next_get_pick_area_center());
        disable_depth_test = true;
        break;
    }
    info_buf.push_update();

    select_id_map.clear();
#ifdef DEBUG
    map_names.clear();
#endif
  }

  /** IMPORTANT: Changes the draw state. Need to be called after the pass own state_set. */
  void select_bind(PassSimple &pass)
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    if (disable_depth_test) {
      /* TODO: clipping state. */
      pass.state_set(DRW_STATE_WRITE_COLOR);
    }
    pass.bind_ubo(SELECT_DATA, &info_buf);
    pass.bind_ssbo(SELECT_ID_OUT, &select_output_buf);
  }

  /** IMPORTANT: Changes the draw state. Need to be called after the pass own state_set. */
  void select_bind(PassMain &pass)
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    pass.use_custom_ids = true;
    if (disable_depth_test) {
      /* TODO: clipping state. */
      pass.state_set(DRW_STATE_WRITE_COLOR);
    }
    pass.bind_ubo(SELECT_DATA, &info_buf);
    /* IMPORTANT: This binds a dummy buffer `in_select_buf` but it is not supposed to be used. */
    pass.bind_ssbo(SELECT_ID_IN, &dummy_select_buf);
    pass.bind_ssbo(SELECT_ID_OUT, &select_output_buf);
  }

  void end_sync()
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    select_output_buf.resize(ceil_to_multiple_u(select_id_map.size(), 4));
    select_output_buf.push_update();
    if (info_buf.mode == SelectType::SELECT_ALL) {
      /* This mode uses atomicOr and store result as a bitmap. Clear to 0 (no selection). */
      GPU_storagebuf_clear(select_output_buf, 0);
    }
    else {
      /* Other modes use atomicMin. Clear to UINT_MAX. */
      GPU_storagebuf_clear(select_output_buf, 0xFFFFFFFFu);
    }
  }

  void read_result()
  {
    if (selection_type == SelectionType::DISABLED) {
      return;
    }

    GPU_memory_barrier(GPU_BARRIER_BUFFER_UPDATE);
    select_output_buf.read();

    Vector<GPUSelectResult> result;

    /* Convert raw data from GPU to #GPUSelectResult. */
    switch (info_buf.mode) {
      case SelectType::SELECT_ALL:
        for (auto i : IndexRange(select_id_map.size())) {
          if (((select_output_buf[i / 32] >> (i % 32)) & 1) != 0) {
            result.append({select_id_map[i], 0xFFFFu});
          }
        }
        break;

      case SelectType::SELECT_PICK_ALL:
      case SelectType::SELECT_PICK_NEAREST:
        for (auto i : IndexRange(select_id_map.size())) {
          if (select_output_buf[i] != 0xFFFFFFFFu) {
            /* NOTE: For `SELECT_PICK_NEAREST`, `select_output_buf` also contains the screen
             * distance to cursor in the lowest bits. */
            result.append({select_id_map[i], select_output_buf[i]});
          }
        }
        break;
    }

    gpu_select_next_set_result(result.data(), result.size());
  }
};

}  // namespace blender::draw::select
