/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.h"
#include "UI_resources.h"
#include "draw_manager.hh"
#include "draw_pass.hh"
#include "gpu_shader_create_info.hh"

#include "../select/select_instance.hh"
#include "overlay_shader_shared.h"

/* Needed for BoneInstanceData. */
#include "overlay_private.hh"

namespace blender::draw::overlay {

using SelectionType = select::SelectionType;

using blender::draw::Framebuffer;
using blender::draw::StorageVectorBuffer;
using blender::draw::Texture;
using blender::draw::TextureFromPool;
using blender::draw::TextureRef;

struct State {
  Depsgraph *depsgraph;
  const ViewLayer *view_layer;
  const Scene *scene;
  const View3D *v3d;
  const RegionView3D *rv3d;
  const Base *active_base;
  View3DOverlay overlay;
  float pixelsize;
  enum eSpace_Type space_type;
  enum eContextObjectMode ctx_mode;
  enum eObjectMode object_mode;
  bool clear_in_front;
  bool use_in_front;
  bool is_wireframe_mode;
  bool hide_overlays;
  bool xray_enabled;
  bool xray_enabled_and_not_wire;
  float xray_opacity;
  short v3d_flag;     /* TODO: move to #View3DOverlay. */
  short v3d_gridflag; /* TODO: move to #View3DOverlay. */
  int cfra;
  DRWState clipping_state;
};

/**
 * Contains all overlay generic geometry batches.
 */
class ShapeCache {
 private:
  struct BatchDeleter {
    void operator()(GPUBatch *shader)
    {
      GPU_BATCH_DISCARD_SAFE(shader);
    }
  };
  using BatchPtr = std::unique_ptr<GPUBatch, BatchDeleter>;

 public:
  BatchPtr quad_wire;
  BatchPtr plain_axes;
  BatchPtr single_arrow;
  BatchPtr cube;
  BatchPtr circle;
  BatchPtr empty_sphere;
  BatchPtr empty_cone;
  BatchPtr arrows;
  BatchPtr metaball_wire_circle;

  ShapeCache();
};

/**
 * Shader module. Shared between instances.
 */
class ShaderModule {
 private:
  struct ShaderDeleter {
    void operator()(GPUShader *shader)
    {
      DRW_SHADER_FREE_SAFE(shader);
    }
  };
  using ShaderPtr = std::unique_ptr<GPUShader, ShaderDeleter>;

  /** Shared shader module across all engine instances. */
  static ShaderModule *g_shader_modules[2 /*Selection Instance*/][2 /*Clipping Enabled*/];

  const SelectionType selection_type_;
  /** TODO: Support clipping. This global state should be set by the overlay::Instance and switch
   * to the shader variations that use clipping. */
  const bool clipping_enabled_;

 public:
  /** Shaders */
  ShaderPtr grid = shader("overlay_grid");
  ShaderPtr background_fill = shader("overlay_background");
  ShaderPtr background_clip_bound = shader("overlay_clipbound");

  /** Selectable Shaders */
  ShaderPtr armature_sphere_outline;
  ShaderPtr depth_mesh;
  ShaderPtr extra_shape;

  ShaderModule(const SelectionType selection_type, const bool clipping_enabled);

  /** Module */
  /** Only to be used by Instance constructor. */
  static ShaderModule &module_get(SelectionType selection_type, bool clipping_enabled);
  static void module_free();

 private:
  ShaderPtr shader(const char *create_info_name)
  {
    return ShaderPtr(GPU_shader_create_from_info_name(create_info_name));
  }
  ShaderPtr selectable_shader(const char *create_info_name);
  ShaderPtr selectable_shader(const char *create_info_name,
                              std::function<void(gpu::shader::ShaderCreateInfo &info)> patch);
};

struct Resources : public select::SelectMap {
  ShaderModule &shaders;

  Framebuffer overlay_fb = {"overlay_fb"};
  Framebuffer overlay_in_front_fb = {"overlay_in_front_fb"};
  Framebuffer overlay_color_only_fb = {"overlay_color_only_fb"};
  Framebuffer overlay_line_fb = {"overlay_line_fb"};
  Framebuffer overlay_line_in_front_fb = {"overlay_line_in_front_fb"};

  TextureFromPool line_tx = {"line_tx"};
  TextureFromPool depth_in_front_alloc_tx = {"overlay_depth_in_front_tx"};
  TextureFromPool color_overlay_alloc_tx = {"overlay_color_overlay_alloc_tx"};
  TextureFromPool color_render_alloc_tx = {"overlay_color_render_alloc_tx"};

  /** TODO(fclem): Copy of G_data.block that should become theme colors only and managed by the
   * engine. */
  GlobalsUboStorage theme_settings;
  /* References, not owned. */
  GPUUniformBuf *globals_buf;
  TextureRef depth_tx;
  TextureRef depth_in_front_tx;
  TextureRef color_overlay_tx;
  TextureRef color_render_tx;

  Resources(const SelectionType selection_type_, ShaderModule &shader_module)
      : select::SelectMap(selection_type_), shaders(shader_module){};

  ThemeColorID object_wire_theme_id(const ObjectRef &ob_ref, const State &state) const
  {
    const bool is_edit = (state.object_mode & OB_MODE_EDIT) &&
                         (ob_ref.object->mode & OB_MODE_EDIT);
    const bool active = (state.active_base != nullptr) &&
                        ((ob_ref.dupli_parent != nullptr) ?
                             (state.active_base->object == ob_ref.dupli_parent) :
                             (state.active_base->object == ob_ref.object));
    const bool is_selected = ((ob_ref.object->base_flag & BASE_SELECTED) != 0);

    /* Object in edit mode. */
    if (is_edit) {
      return TH_WIRE_EDIT;
    }
    /* Transformed object during operators. */
    if (((G.moving & G_TRANSFORM_OBJ) != 0) && is_selected) {
      return TH_TRANSFORM;
    }
    /* Sets the 'theme_id' or fallback to wire */
    if ((ob_ref.object->base_flag & BASE_SELECTED) != 0) {
      return (active) ? TH_ACTIVE : TH_SELECT;
    }

    switch (ob_ref.object->type) {
      case OB_LAMP:
        return TH_LIGHT;
      case OB_SPEAKER:
        return TH_SPEAKER;
      case OB_CAMERA:
        return TH_CAMERA;
      case OB_LIGHTPROBE:
        /* TODO: add light-probe color. Use empty color for now. */
      case OB_EMPTY:
        return TH_EMPTY;
      default:
        return (is_edit) ? TH_WIRE_EDIT : TH_WIRE;
    }
  }

  const float4 &object_wire_color(const ObjectRef &ob_ref, ThemeColorID theme_id) const
  {
    if (UNLIKELY(ob_ref.object->base_flag & BASE_FROM_SET)) {
      return theme_settings.color_wire;
    }
    switch (theme_id) {
      case TH_WIRE_EDIT:
        return theme_settings.color_wire_edit;
      case TH_ACTIVE:
        return theme_settings.color_active;
      case TH_SELECT:
        return theme_settings.color_select;
      case TH_TRANSFORM:
        return theme_settings.color_transform;
      case TH_SPEAKER:
        return theme_settings.color_speaker;
      case TH_CAMERA:
        return theme_settings.color_camera;
      case TH_EMPTY:
        return theme_settings.color_empty;
      case TH_LIGHT:
        return theme_settings.color_light;
      default:
        return theme_settings.color_wire;
    }
  }

  const float4 &object_wire_color(const ObjectRef &ob_ref, const State &state) const
  {
    ThemeColorID theme_id = object_wire_theme_id(ob_ref, state);
    return object_wire_color(ob_ref, theme_id);
  }
};

/**
 * Buffer containing instances of a certain shape.
 */
template<typename InstanceDataT> struct ShapeInstanceBuf : private select::SelectBuf {

  StorageVectorBuffer<InstanceDataT> data_buf;

  ShapeInstanceBuf(const SelectionType selection_type, const char *name = nullptr)
      : select::SelectBuf(selection_type), data_buf(name){};

  void clear()
  {
    this->select_clear();
    data_buf.clear();
  }

  void append(const InstanceDataT &data, select::ID select_id)
  {
    this->select_append(select_id);
    data_buf.append(data);
  }

  void end_sync(PassSimple &pass, GPUBatch *shape)
  {
    if (data_buf.size() == 0) {
      return;
    }
    this->select_bind(pass);
    data_buf.push_update();
    pass.bind_ssbo("data_buf", &data_buf);
    pass.draw(shape, data_buf.size());
  }
};

}  // namespace blender::draw::overlay
