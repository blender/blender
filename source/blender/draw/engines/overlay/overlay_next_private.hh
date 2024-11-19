/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_movieclip.h"

#include "BLI_function_ref.hh"

#include "GPU_matrix.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"
#include "UI_resources.hh"
#include "draw_manager.hh"
#include "draw_pass.hh"
#include "gpu_shader_create_info.hh"

#include "../select/select_instance.hh"
#include "overlay_shader_shared.h"

#include "draw_common.hh"

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
  const SpaceLink *space_data;
  const ARegion *region;
  const RegionView3D *rv3d;
  const Base *active_base;
  DRWTextStore *dt;
  View3DOverlay overlay;
  float pixelsize;
  eSpace_Type space_type;
  eContextObjectMode ctx_mode;
  eObjectMode object_mode;
  const Object *object_active;
  bool clear_in_front;
  bool use_in_front;
  bool is_wireframe_mode;
  bool hide_overlays;
  bool xray_enabled;
  bool xray_enabled_and_not_wire;
  /* Brings the active pose armature in front of all objects. */
  bool do_pose_xray;
  /* Add a veil on top of all surfaces to make the active pose armature pop out. */
  bool do_pose_fade_geom;
  float xray_opacity;
  short v3d_flag;     /* TODO: move to #View3DOverlay. */
  short v3d_gridflag; /* TODO: move to #View3DOverlay. */
  int cfra;
  float3 camera_position;
  float3 camera_forward;
  int clipping_plane_count;

  /* Active Image properties. Only valid image space only. */
  int2 image_size;
  float2 image_uv_aspect;
  float2 image_aspect;

  float view_dist_get(const float4x4 &winmat) const
  {
    float view_dist = rv3d->dist;
    /* Special exception for orthographic camera:
     * `view_dist` isn't used as the depth range isn't the same. */
    if (rv3d->persp == RV3D_CAMOB && rv3d->is_persp == false) {
      view_dist = 1.0f / max_ff(fabsf(winmat[0][0]), fabsf(winmat[1][1]));
    }
    return view_dist;
  }
};

static inline float4x4 winmat_polygon_offset(float4x4 winmat, float view_dist, float offset)
{
  winmat[3][2] -= GPU_polygon_offset_calc(winmat.ptr(), view_dist, offset);
  return winmat;
}

/**
 * Contains all overlay generic geometry batches.
 */
class ShapeCache {
 private:
  struct BatchDeleter {
    void operator()(gpu::Batch *shader)
    {
      GPU_BATCH_DISCARD_SAFE(shader);
    }
  };
  using BatchPtr = std::unique_ptr<gpu::Batch, BatchDeleter>;

 public:
  BatchPtr bone_box;
  BatchPtr bone_box_wire;
  BatchPtr bone_envelope;
  BatchPtr bone_envelope_wire;
  BatchPtr bone_octahedron;
  BatchPtr bone_octahedron_wire;
  BatchPtr bone_sphere;
  BatchPtr bone_sphere_wire;
  BatchPtr bone_stick;

  BatchPtr bone_degrees_of_freedom;
  BatchPtr bone_degrees_of_freedom_wire;

  BatchPtr grid;

  BatchPtr quad_wire;
  BatchPtr quad_solid;
  BatchPtr plain_axes;
  BatchPtr single_arrow;
  BatchPtr cube;
  BatchPtr circle;
  BatchPtr empty_sphere;
  BatchPtr empty_cone;
  BatchPtr cylinder;
  BatchPtr capsule_body;
  BatchPtr capsule_cap;
  BatchPtr arrows;
  BatchPtr metaball_wire_circle;

  BatchPtr speaker;

  BatchPtr camera_distances;
  BatchPtr camera_frame;
  BatchPtr camera_tria_wire;
  BatchPtr camera_tria;

  BatchPtr camera_volume;
  BatchPtr camera_volume_wire;

  BatchPtr sphere_low_detail;

  BatchPtr ground_line;

  BatchPtr light_icon_outer_lines;
  BatchPtr light_icon_inner_lines;
  BatchPtr light_icon_sun_rays;
  BatchPtr light_point_lines;
  BatchPtr light_sun_lines;
  BatchPtr light_spot_lines;
  BatchPtr light_area_disk_lines;
  BatchPtr light_area_square_lines;
  BatchPtr light_spot_volume;

  BatchPtr field_force;
  BatchPtr field_wind;
  BatchPtr field_vortex;
  BatchPtr field_curve;
  BatchPtr field_sphere_limit;
  BatchPtr field_tube_limit;
  BatchPtr field_cone_limit;

  BatchPtr lightprobe_cube;
  BatchPtr lightprobe_planar;
  BatchPtr lightprobe_grid;

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
  static ShaderModule *g_shader_modules[2 /* Selection Instance. */][2 /* Clipping Enabled. */];

  const SelectionType selection_type_;
  /** TODO: Support clipping. This global state should be set by the overlay::Instance and switch
   * to the shader variations that use clipping. */
  const bool clipping_enabled_;

 public:
  /** Shaders */
  ShaderPtr attribute_viewer_mesh;
  ShaderPtr attribute_viewer_pointcloud;
  ShaderPtr attribute_viewer_curve;
  ShaderPtr attribute_viewer_curves;

  ShaderPtr anti_aliasing = shader("overlay_antialiasing");
  ShaderPtr armature_degrees_of_freedom;
  ShaderPtr background_fill = shader("overlay_background");
  ShaderPtr background_clip_bound = shader("overlay_clipbound");
  ShaderPtr curve_edit_points;
  ShaderPtr curve_edit_line;
  ShaderPtr curve_edit_handles = shader("overlay_edit_curves_handle_next");
  ShaderPtr extra_point;
  ShaderPtr facing;
  ShaderPtr grid = shader("overlay_grid");
  ShaderPtr grid_background;
  ShaderPtr grid_grease_pencil = shader("overlay_gpencil_canvas");
  ShaderPtr grid_image;
  ShaderPtr legacy_curve_edit_wires;
  ShaderPtr legacy_curve_edit_normals = shader("overlay_edit_curve_normals");
  ShaderPtr legacy_curve_edit_handles = shader("overlay_edit_curve_handle_next");
  ShaderPtr legacy_curve_edit_points;
  ShaderPtr motion_path_line = shader("overlay_motion_path_line_next");
  ShaderPtr motion_path_vert = shader("overlay_motion_path_point");
  ShaderPtr mesh_analysis;
  ShaderPtr mesh_edit_depth;
  ShaderPtr mesh_edit_edge = shader("overlay_edit_mesh_edge_next");
  ShaderPtr mesh_edit_face = shader("overlay_edit_mesh_face_next");
  ShaderPtr mesh_edit_vert = shader("overlay_edit_mesh_vert_next");
  ShaderPtr mesh_edit_facedot = shader("overlay_edit_mesh_facedot_next");
  ShaderPtr mesh_edit_skin_root;
  ShaderPtr mesh_face_normal, mesh_face_normal_subdiv;
  ShaderPtr mesh_loop_normal, mesh_loop_normal_subdiv;
  ShaderPtr mesh_vert_normal;
  ShaderPtr outline_prepass_mesh;
  ShaderPtr outline_prepass_wire = shader("overlay_outline_prepass_wire_next");
  ShaderPtr outline_prepass_curves;
  ShaderPtr outline_prepass_pointcloud;
  ShaderPtr outline_prepass_gpencil;
  ShaderPtr outline_detect = shader("overlay_outline_detect");
  ShaderPtr particle_edit_vert;
  ShaderPtr particle_edit_edge;
  ShaderPtr paint_region_edge;
  ShaderPtr paint_region_face;
  ShaderPtr paint_region_vert;
  ShaderPtr paint_texture;
  ShaderPtr paint_weight;
  ShaderPtr paint_weight_fake_shading; /* TODO(fclem): Specialization constant. */
  ShaderPtr sculpt_mesh;
  ShaderPtr sculpt_curves;
  ShaderPtr sculpt_curves_cage;
  ShaderPtr uniform_color;
  ShaderPtr uniform_color_batch;
  ShaderPtr uv_analysis_stretch_angle;
  ShaderPtr uv_analysis_stretch_area;
  ShaderPtr uv_brush_stencil;
  ShaderPtr uv_edit_edge = shader("overlay_edit_uv_edges_next");
  ShaderPtr uv_edit_face;
  ShaderPtr uv_edit_facedot;
  ShaderPtr uv_edit_vert;
  ShaderPtr uv_image_borders;
  ShaderPtr uv_paint_mask;
  ShaderPtr uv_wireframe = shader("overlay_wireframe_uv");
  ShaderPtr xray_fade;

  /** Selectable Shaders */
  ShaderPtr armature_envelope_fill;
  ShaderPtr armature_envelope_outline;
  ShaderPtr armature_shape_outline;
  ShaderPtr armature_shape_fill;
  ShaderPtr armature_shape_wire;
  ShaderPtr armature_sphere_outline;
  ShaderPtr armature_sphere_fill;
  ShaderPtr armature_stick;
  ShaderPtr armature_wire;
  ShaderPtr depth_curves = selectable_shader("overlay_depth_curves");
  ShaderPtr depth_grease_pencil = selectable_shader("overlay_depth_gpencil");
  ShaderPtr depth_mesh = selectable_shader("overlay_depth_mesh");
  ShaderPtr depth_mesh_conservative = selectable_shader("overlay_depth_mesh_conservative");
  ShaderPtr depth_point_cloud = selectable_shader("overlay_depth_pointcloud");
  ShaderPtr extra_grid;
  ShaderPtr extra_shape;
  ShaderPtr extra_wire_object;
  ShaderPtr extra_wire;
  ShaderPtr extra_loose_points;
  ShaderPtr extra_ground_line;
  ShaderPtr fluid_grid_lines_flags;
  ShaderPtr fluid_grid_lines_flat;
  ShaderPtr fluid_grid_lines_range;
  ShaderPtr fluid_velocity_streamline;
  ShaderPtr fluid_velocity_mac;
  ShaderPtr fluid_velocity_needle;
  ShaderPtr image_plane;
  ShaderPtr image_plane_depth_bias;
  ShaderPtr lattice_points;
  ShaderPtr lattice_wire;
  ShaderPtr particle_dot;
  ShaderPtr particle_shape;
  ShaderPtr particle_hair;
  ShaderPtr wireframe_mesh;
  ShaderPtr wireframe_curve;
  ShaderPtr wireframe_points; /* Draw objects without edges for the wireframe overlay. */

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
  ShaderPtr shader(const char *create_info_name,
                   FunctionRef<void(gpu::shader::ShaderCreateInfo &info)> patch);
  ShaderPtr selectable_shader(const char *create_info_name);
  ShaderPtr selectable_shader(const char *create_info_name,
                              FunctionRef<void(gpu::shader::ShaderCreateInfo &info)> patch);
};

struct Resources : public select::SelectMap {
  ShaderModule &shaders;

  /* Overlay Color. */
  Framebuffer overlay_color_only_fb = {"overlay_color_only_fb"};
  /* Overlay Color, Line Data. */
  Framebuffer overlay_line_only_fb = {"overlay_line_only_fb"};
  /* Depth, Overlay Color. */
  Framebuffer overlay_fb = {"overlay_fb"};
  /* Depth, Overlay Color, Line Data. */
  Framebuffer overlay_line_fb = {"overlay_line_fb"};
  /* Depth In-Front, Overlay Color. */
  Framebuffer overlay_in_front_fb = {"overlay_in_front_fb"};
  /* Depth In-Front, Overlay Color, Line Data. */
  Framebuffer overlay_line_in_front_fb = {"overlay_line_in_front_fb"};

  /* Output Color. */
  Framebuffer overlay_output_fb = {"overlay_output_fb"};

  /* Render Frame-buffers. Only used for multiplicative blending on top of the render. */
  /* TODO(fclem): Remove the usage of these somehow. This is against design. */
  GPUFrameBuffer *render_fb = nullptr;
  GPUFrameBuffer *render_in_front_fb = nullptr;

  /* Target containing line direction and data for line expansion and anti-aliasing. */
  TextureFromPool line_tx = {"line_tx"};
  /* Target containing overlay color before anti-aliasing. */
  TextureFromPool overlay_tx = {"overlay_tx"};
  /* Target containing depth of overlays when xray is enabled. */
  TextureFromPool xray_depth_tx = {"xray_depth_tx"};

  /* Texture that are usually allocated inside. These are fallback when they aren't.
   * They are then wrapped inside the #TextureRefs below. */
  TextureFromPool depth_in_front_alloc_tx = {"overlay_depth_in_front_tx"};
  TextureFromPool color_overlay_alloc_tx = {"overlay_color_overlay_alloc_tx"};
  TextureFromPool color_render_alloc_tx = {"overlay_color_render_alloc_tx"};

  /* 1px texture containing only maximum depth. To be used for fulfilling bindings when depth
   * texture is not available or not needed. */
  Texture dummy_depth_tx = {"dummy_depth_tx"};

  /** TODO(fclem): Copy of G_data.block that should become theme colors only and managed by the
   * engine. */
  GlobalsUboStorage theme_settings;
  /* References, not owned. */
  GPUUniformBuf *globals_buf;
  TextureRef weight_ramp_tx;
  /* Wrappers around #DefaultTextureList members. */
  TextureRef depth_in_front_tx;
  TextureRef color_overlay_tx;
  TextureRef color_render_tx;
  /**
   * Scene depth buffer that can also be used as render target for overlays.
   *
   * Can only be bound as a texture if either:
   * - the current frame-buffer has no depth buffer attached.
   * - `state.xray_enabled` is true.
   */
  TextureRef depth_tx;
  /**
   * Depth target.
   * Can either be default depth buffer texture from #DefaultTextureList
   * or `xray_depth_tx` if X-ray is enabled.
   */
  TextureRef depth_target_tx;
  TextureRef depth_target_in_front_tx;

  Vector<MovieClip *> bg_movie_clips;

  Resources(const SelectionType selection_type_, ShaderModule &shader_module)
      : select::SelectMap(selection_type_), shaders(shader_module){};

  ~Resources()
  {
    free_movieclips_textures();
  }

  void begin_sync()
  {
    SelectMap::begin_sync();
    free_movieclips_textures();
  }

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

  float4 background_blend_color(ThemeColorID theme_id) const
  {
    float4 color;
    UI_GetThemeColorBlendShade4fv(theme_id, TH_BACK, 0.5, 0, color);
    return color;
  }

  float4 object_background_blend_color(const ObjectRef &ob_ref, const State &state) const
  {
    ThemeColorID theme_id = object_wire_theme_id(ob_ref, state);
    return background_blend_color(theme_id);
  }

  float4 background_color_get(const State &state)
  {
    if (state.v3d->shading.background_type == V3D_SHADING_BACKGROUND_WORLD) {
      if (state.scene->world) {
        return float4(float3(&state.scene->world->horr));
      }
    }
    else if (state.v3d->shading.background_type == V3D_SHADING_BACKGROUND_VIEWPORT) {
      return state.v3d->shading.background_color;
    }
    float4 color;
    UI_GetThemeColor3fv(TH_BACK, color);
    return color;
  }

  void free_movieclips_textures()
  {
    /* Free Movie clip textures after rendering */
    for (MovieClip *clip : bg_movie_clips) {
      BKE_movieclip_free_gputexture(clip);
    }
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

  void end_sync(PassSimple::Sub &pass, gpu::Batch *shape)
  {
    if (data_buf.is_empty()) {
      return;
    }
    this->select_bind(pass);
    data_buf.push_update();
    pass.bind_ssbo("data_buf", &data_buf);
    pass.draw(shape, data_buf.size());
  }

  void end_sync(PassSimple::Sub &pass,
                gpu::Batch *shape,
                GPUPrimType primitive_type,
                uint primitive_len)
  {
    if (data_buf.is_empty()) {
      return;
    }
    this->select_bind(pass);
    data_buf.push_update();
    pass.bind_ssbo("data_buf", &data_buf);
    pass.draw_expand(
        shape, primitive_type, primitive_len, data_buf.size(), ResourceHandle(0), uint(0));
  }
};

struct VertexPrimitiveBuf {
 protected:
  select::SelectBuf select_buf;
  StorageVectorBuffer<VertexData> data_buf;
  int color_id = 0;

  VertexPrimitiveBuf(const SelectionType selection_type, const char *name = nullptr)
      : select_buf(selection_type), data_buf(name){};

  void append(const float3 &position, const float4 &color)
  {
    data_buf.append({float4(position), color});
  }

  void end_sync(PassSimple::Sub &pass, GPUPrimType primitive)
  {
    if (data_buf.is_empty()) {
      return;
    }
    select_buf.select_bind(pass);
    data_buf.push_update();
    pass.bind_ssbo("data_buf", &data_buf);
    pass.push_constant("colorid", color_id);
    pass.draw_procedural(primitive, 1, data_buf.size());
  }

 public:
  void clear()
  {
    select_buf.select_clear();
    data_buf.clear();
    color_id = 0;
  }
};

struct PointPrimitiveBuf : public VertexPrimitiveBuf {

 public:
  PointPrimitiveBuf(const SelectionType selection_type, const char *name = nullptr)
      : VertexPrimitiveBuf(selection_type, name)
  {
  }

  void append(const float3 &position,
              const float4 &color,
              select::ID select_id = select::SelectMap::select_invalid_id())
  {
    select_buf.select_append(select_id);
    VertexPrimitiveBuf::append(position, color);
  }

  void append(const float3 &position, const int color_id, select::ID select_id)
  {
    this->color_id = color_id;
    append(position, float4(), select_id);
  }

  void end_sync(PassSimple::Sub &pass)
  {
    VertexPrimitiveBuf::end_sync(pass, GPU_PRIM_POINTS);
  }
};

struct LinePrimitiveBuf : public VertexPrimitiveBuf {

 public:
  LinePrimitiveBuf(const SelectionType selection_type, const char *name = nullptr)
      : VertexPrimitiveBuf(selection_type, name)
  {
  }

  void append(const float3 &start,
              const float3 &end,
              const float4 &color,
              select::ID select_id = select::SelectMap::select_invalid_id())
  {
    select_buf.select_append(select_id);
    VertexPrimitiveBuf::append(start, color);
    VertexPrimitiveBuf::append(end, color);
  }

  void append(const float3 &start,
              const float3 &end,
              const int color_id,
              select::ID select_id = select::SelectMap::select_invalid_id())
  {
    this->color_id = color_id;
    append(start, end, float4(), select_id);
  }

  void end_sync(PassSimple::Sub &pass)
  {
    VertexPrimitiveBuf::end_sync(pass, GPU_PRIM_LINES);
  }
};

/* Consider instance any object form a set or a dupli system.
 * This hides some overlay to avoid making the viewport unreadable. */
static inline bool is_from_dupli_or_set(const Object *ob)
{
  return ob->base_flag & (BASE_FROM_SET | BASE_FROM_DUPLI);
}

/* Consider instance any object form a set or a dupli system.
 * This hides some overlay to avoid making the viewport unreadable. */
static inline bool is_from_dupli_or_set(const ObjectRef &ob_ref)
{
  return is_from_dupli_or_set(ob_ref.object);
}

}  // namespace blender::draw::overlay
