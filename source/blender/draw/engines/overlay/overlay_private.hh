/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_context.hh"
#include "BKE_movieclip.h"
#include "BKE_object.hh"

#include "BLI_function_ref.hh"

#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "GPU_matrix.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"
#include "UI_resources.hh"
#include "draw_manager.hh"
#include "draw_pass.hh"
#include "draw_view_data.hh"
#include "gpu_shader_create_info.hh"

#include "../select/select_instance.hh"
#include "overlay_shader_shared.hh"

#include "draw_common.hh"

template<> struct blender::gpu::AttrType<VertexClass> {
  static constexpr VertAttrType type = VertAttrType::SINT_32;
};
template<> struct blender::gpu::AttrType<StickBoneFlag> {
  static constexpr VertAttrType type = VertAttrType::SINT_32;
};

namespace blender::draw::overlay {

struct BoneInstanceData {
  /* Keep sync with bone instance vertex format (OVERLAY_InstanceFormats) */
  union {
    float4x4 mat44;
    float mat[4][4];
    struct {
      float _pad0[3], color_hint_a;
      float _pad1[3], color_hint_b;
      float _pad2[3], color_a;
      float _pad3[3], color_b;
    };
    struct {
      float _pad00[3], amin_a;
      float _pad01[3], amin_b;
      float _pad02[3], amax_a;
      float _pad03[3], amax_b;
    };
  };

  BoneInstanceData() = default;

  /**
   * Constructor used by meta-ball overlays and expected to be used for drawing
   * meta-ball edit circles with armature wire shader that produces wide-lines.
   */
  BoneInstanceData(const float4x4 &ob_mat,
                   const float3 &pos,
                   const float radius,
                   const float color[4])

  {
    mat44[0] = ob_mat[0] * radius;
    mat44[1] = ob_mat[1] * radius;
    mat44[2] = ob_mat[2] * radius;
    mat44[3] = float4(blender::math::transform_point(ob_mat, pos), 0.0f);
    set_color(color);
  }

  BoneInstanceData(const float4x4 &bone_mat, const float4 &bone_color, const float4 &hint_color)
      : mat44(bone_mat)
  {
    set_color(bone_color);
    set_hint_color(hint_color);
  };

  BoneInstanceData(const float4x4 &bone_mat, const float4 &bone_color) : mat44(bone_mat)
  {
    set_color(bone_color);
  };

  void set_color(const float4 &bone_color)
  {
    /* Encoded color into 2 floats to be able to use the matrix to color the custom bones. */
    color_a = encode_2f_to_float(bone_color[0], bone_color[1]);
    color_b = encode_2f_to_float(bone_color[2], bone_color[3]);
  }

  void set_hint_color(const float4 &hint_color)
  {
    /* Encoded color into 2 floats to be able to use the matrix to color the custom bones. */
    color_hint_a = encode_2f_to_float(hint_color[0], hint_color[1]);
    color_hint_b = encode_2f_to_float(hint_color[2], hint_color[3]);
  }

 private:
  /* Encode 2 units float with byte precision into a float. */
  float encode_2f_to_float(float a, float b) const
  {
    /* NOTE: `b` can go up to 2. Needed to encode wire size. */
    return float(int(clamp_f(a, 0.0f, 1.0f) * 255) | (int(clamp_f(b, 0.0f, 2.0f) * 255) << 8));
  }
};

using SelectionType = select::SelectionType;

using blender::draw::Framebuffer;
using blender::draw::StorageVectorBuffer;
using blender::draw::Texture;
using blender::draw::TextureFromPool;
using blender::draw::TextureRef;

struct State {
  Depsgraph *depsgraph = nullptr;
  const ViewLayer *view_layer = nullptr;
  const Scene *scene = nullptr;
  const View3D *v3d = nullptr;
  const SpaceLink *space_data = nullptr;
  const ARegion *region = nullptr;
  const RegionView3D *rv3d = nullptr;
  DRWTextStore *dt = nullptr;
  View3DOverlay overlay = {};
  eSpace_Type space_type = SPACE_EMPTY;
  eContextObjectMode ctx_mode = CTX_MODE_EDIT_MESH;
  eObjectMode object_mode = OB_MODE_OBJECT;
  const Object *object_active = nullptr;
  bool clear_in_front = false;
  bool use_in_front = false;
  bool is_wireframe_mode = false;
  /** Whether we are rendering for an image (viewport render). */
  bool is_viewport_image_render = false;
  /** Whether we are rendering for an image. */
  bool is_image_render = false;
  /** True if rendering only to query the depth. Can be for auto-depth rotation. */
  bool is_depth_only_drawing = false;
  /** Skip drawing particle systems. Prevents self-occlusion issues in Particle Edit mode. */
  bool skip_particles = false;
  /** When drag-dropping material onto objects to assignment. */
  bool is_material_select = false;
  /** Whether we should render the background or leave it transparent. */
  bool draw_background = false;
  /** True if the render engine outputs satisfactory depth information to the depth buffer. */
  bool is_render_depth_available = false;
  /** Whether we should render a vignette over the scene. */
  bool vignette_enabled = false;
  /** Should text draw in this mode? */
  bool show_text = false;
  bool hide_overlays = false;
  bool xray_enabled = false;
  bool xray_enabled_and_not_wire = false;
  /** Can be true even if X-ray Alpha is 1.0. */
  bool xray_flag_enabled = false;
  /** Brings the active pose armature in front of all objects. */
  bool do_pose_xray = false;
  /** Add a veil on top of all surfaces to make the active pose armature pop out. */
  bool do_pose_fade_geom = false;
  float xray_opacity = 0.0f;
  short v3d_flag = 0;     /* TODO: move to #View3DOverlay. */
  short v3d_gridflag = 0; /* TODO: move to #View3DOverlay. */
  int cfra = 0;
  float3 camera_position = float3(0.0f);
  float3 camera_forward = float3(0.0f);
  int clipping_plane_count = 0;

  /** Active Image properties. Only valid image space only. */
  bool is_image_valid = false;
  int2 image_size = int2(0);
  float2 image_uv_aspect = float2(0.0f);
  float2 image_aspect = float2(0.0f);

  View::OffsetData offset_data_get() const
  {
    if (rv3d == nullptr) {
      return View::OffsetData();
    }
    return View::OffsetData(*rv3d);
  }

  /* Factor to use for wireframe offset.
   * Result of GPU_polygon_offset_calc for the current view.
   * Only valid at draw time, so use push constant reference instead of copy. */
  float ndc_offset_factor = 0.0f;

  /** Convenience functions. */

  /** Scene geometry is solid. Occlude overlays behind scene geometry. */
  bool is_solid() const
  {
    return xray_opacity == 1.0f;
  }
  /** Scene geometry is semi-transparent. Fade overlays behind scene geometry (see #XrayFade). */
  bool is_xray() const
  {
    return (xray_opacity < 1.0f) && (xray_opacity > 0.0f);
  }
  /** Scene geometry is fully transparent. Scene geometry does not occlude overlays. */
  bool is_wire() const
  {
    return xray_opacity == 0.0f;
  }

  bool is_space_v3d() const
  {
    return this->space_type == SPACE_VIEW3D;
  }
  bool is_space_image() const
  {
    return this->space_type == SPACE_IMAGE;
  }
  bool is_space_node() const
  {
    return this->space_type == SPACE_NODE;
  }

  bool show_extras() const
  {
    return (this->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_XTRAS) == 0;
  }
  bool show_face_orientation() const
  {
    return (this->overlay.flag & V3D_OVERLAY_FACE_ORIENTATION);
  }
  bool show_bone_selection() const
  {
    return (this->overlay.flag & V3D_OVERLAY_BONE_SELECT);
  }
  bool show_wireframes() const
  {
    return (this->overlay.flag & V3D_OVERLAY_WIREFRAMES);
  }
  bool show_motion_paths() const
  {
    return (this->overlay.flag & V3D_OVERLAY_HIDE_MOTION_PATHS) == 0;
  }
  bool show_bones() const
  {
    return (this->overlay.flag & V3D_OVERLAY_HIDE_BONES) == 0;
  }
  bool show_object_origins() const
  {
    return (this->overlay.flag & V3D_OVERLAY_HIDE_OBJECT_ORIGINS) == 0;
  }
  bool show_fade_inactive() const
  {
    return (this->overlay.flag & V3D_OVERLAY_FADE_INACTIVE);
  }
  bool show_attribute_viewer() const
  {
    return (this->overlay.flag & V3D_OVERLAY_VIEWER_ATTRIBUTE);
  }
  bool show_attribute_viewer_text() const
  {
    return (this->overlay.flag & V3D_OVERLAY_VIEWER_ATTRIBUTE_TEXT);
  }
  bool show_sculpt_mask() const
  {
    return (this->overlay.flag & V3D_OVERLAY_SCULPT_SHOW_MASK);
  }
  bool show_sculpt_face_sets() const
  {
    return (this->overlay.flag & V3D_OVERLAY_SCULPT_SHOW_FACE_SETS);
  }
  bool show_sculpt_curves_cage() const
  {
    return (this->overlay.flag & V3D_OVERLAY_SCULPT_CURVES_CAGE);
  }
  bool show_light_colors() const
  {
    return (this->overlay.flag & V3D_OVERLAY_SHOW_LIGHT_COLORS);
  }
};

/* Matches Vertex Format. */
struct Vertex {
  float3 pos;
  VertexClass vclass;

  GPU_VERTEX_FORMAT_FUNC(Vertex, pos, vclass);
};

struct VertexBone {
  float3 pos;
  StickBoneFlag vclass;

  GPU_VERTEX_FORMAT_FUNC(VertexBone, pos, vclass);
};

struct VertexWithColor {
  float3 pos;
  float3 color;

  GPU_VERTEX_FORMAT_FUNC(VertexWithColor, pos, color);
};

struct VertShaded {
  float3 pos;
  VertexClass vclass;
  float3 nor;

  GPU_VERTEX_FORMAT_FUNC(VertShaded, pos, vclass, nor);
};

/* TODO(fclem): Might be good to remove for simplicity. */
struct VertexTriple {
  float2 pos0;
  float2 pos1;
  float2 pos2;

  GPU_VERTEX_FORMAT_FUNC(VertexTriple, pos0, pos1, pos2);
};

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
  BatchPtr cube_solid;

  BatchPtr cursor_circle;
  BatchPtr cursor_lines;

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

  /* Batch drawing a quad with coordinate [0..1] at 0.75 depth. */
  BatchPtr image_quad;

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

 private:
  /* Caller gets ownership of the #gpu::VertBuf. */
  template<typename T> gpu::VertBuf *vbo_from_vector(const Vector<T> &vector)
  {
    gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(T::format());
    GPU_vertbuf_data_alloc(*vbo, vector.size());
    vbo->data<T>().copy_from(vector);
    return vbo;
  }
};

using StaticShader = gpu::StaticShader;

/**
 * Shader module. Shared between instances.
 */
class ShaderModule {
 private:
  /* Allow StaticShaderCache access to the constructor. */
  friend gpu::StaticShaderCache<ShaderModule>;

  using StaticCache =
      gpu::StaticShaderCache<ShaderModule>[2 /* Selection Instance. */][2 /* Clipping Enabled. */];

  static StaticCache &get_static_cache()
  {
    /** Shared shader module across all engine instances. */
    static StaticCache static_cache;
    return static_cache;
  }

  const SelectionType selection_type_;
  /** TODO: Support clipping. This global state should be set by the overlay::Instance and switch
   * to the shader variations that use clipping. */
  const bool clipping_enabled_;

 public:
  /** Shaders */
  StaticShader anti_aliasing = {"overlay_antialiasing"};
  StaticShader armature_degrees_of_freedom = shader_clippable("overlay_armature_dof");
  StaticShader attribute_viewer_mesh = shader_clippable("overlay_viewer_attribute_mesh");
  StaticShader attribute_viewer_pointcloud = shader_clippable(
      "overlay_viewer_attribute_pointcloud");
  StaticShader attribute_viewer_curve = shader_clippable("overlay_viewer_attribute_curve");
  StaticShader attribute_viewer_curves = shader_clippable("overlay_viewer_attribute_curves");
  StaticShader background_fill = {"overlay_background"};
  StaticShader background_clip_bound = {"overlay_clipbound"};
  StaticShader curve_edit_points = shader_clippable("overlay_edit_curves_point");
  StaticShader curve_edit_line = shader_clippable("overlay_edit_particle_strand");
  StaticShader curve_edit_handles = shader_clippable("overlay_edit_curves_handle");
  StaticShader facing = shader_clippable("overlay_facing");
  StaticShader grid = {"overlay_grid_next"};
  StaticShader grid_background = {"overlay_grid_background"};
  StaticShader grid_grease_pencil = shader_clippable("overlay_gpencil_canvas");
  StaticShader grid_image = {"overlay_grid_image"};
  StaticShader lattice_points = shader_clippable("overlay_edit_lattice_point");
  StaticShader lattice_wire = shader_clippable("overlay_edit_lattice_wire");
  StaticShader legacy_curve_edit_handles = shader_clippable("overlay_edit_curve_handle");
  StaticShader legacy_curve_edit_normals = shader_clippable("overlay_edit_curve_normals");
  StaticShader legacy_curve_edit_points = shader_clippable("overlay_edit_curve_point");
  StaticShader legacy_curve_edit_wires = shader_clippable("overlay_edit_curve_wire");
  StaticShader light_spot_cone = shader_clippable("overlay_extra_spot_cone");
  StaticShader mesh_analysis = shader_clippable("overlay_edit_mesh_analysis");
  StaticShader mesh_edit_depth = shader_clippable("overlay_edit_mesh_depth");
  StaticShader mesh_edit_edge = shader_clippable("overlay_edit_mesh_edge");
  StaticShader mesh_edit_face = shader_clippable("overlay_edit_mesh_face");
  StaticShader mesh_edit_facedot = shader_clippable("overlay_edit_mesh_facedot");
  StaticShader mesh_edit_vert = shader_clippable("overlay_edit_mesh_vert");
  StaticShader mesh_edit_skin_root = shader_clippable("overlay_edit_mesh_skin_root");
  StaticShader mesh_face_normal = shader_clippable("overlay_mesh_face_normal");
  StaticShader mesh_face_normal_subdiv = shader_clippable("overlay_mesh_face_normal_subdiv");
  StaticShader mesh_loop_normal = shader_clippable("overlay_mesh_loop_normal");
  StaticShader mesh_loop_normal_subdiv = shader_clippable("overlay_mesh_loop_normal_subdiv");
  StaticShader mesh_vert_normal = shader_clippable("overlay_mesh_vert_normal");
  StaticShader mesh_vert_normal_subdiv = shader_clippable("overlay_mesh_vert_normal_subdiv");
  StaticShader motion_path_line = shader_clippable("overlay_motion_path_line");
  StaticShader motion_path_vert = shader_clippable("overlay_motion_path_point");
  StaticShader outline_detect = {"overlay_outline_detect"};
  StaticShader outline_prepass_curves = shader_clippable("overlay_outline_prepass_curves");
  StaticShader outline_prepass_gpencil = shader_clippable("overlay_outline_prepass_gpencil");
  StaticShader outline_prepass_mesh = shader_clippable("overlay_outline_prepass_mesh");
  StaticShader outline_prepass_pointcloud = shader_clippable("overlay_outline_prepass_pointcloud");
  StaticShader outline_prepass_wire = shader_clippable("overlay_outline_prepass_wire");
  StaticShader paint_region_edge = shader_clippable("overlay_paint_wire");
  StaticShader paint_region_face = shader_clippable("overlay_paint_face");
  StaticShader paint_region_vert = shader_clippable("overlay_paint_point");
  StaticShader paint_texture = shader_clippable("overlay_paint_texture");
  StaticShader paint_weight = shader_clippable("overlay_paint_weight");
  /* TODO(fclem): Specialization constant. */
  StaticShader paint_weight_fake_shading = shader_clippable("overlay_paint_weight_fake_shading");
  StaticShader particle_edit_vert = shader_clippable("overlay_edit_particle_point");
  StaticShader particle_edit_edge = shader_clippable("overlay_edit_particle_strand");
  StaticShader pointcloud_points = shader_clippable("overlay_edit_pointcloud");
  StaticShader sculpt_curves = shader_clippable("overlay_sculpt_curves_selection");
  StaticShader sculpt_curves_cage = shader_clippable("overlay_sculpt_curves_cage");
  StaticShader sculpt_mesh = shader_clippable("overlay_sculpt_mask");
  StaticShader uniform_color = shader_clippable("overlay_uniform_color");
  StaticShader uv_analysis_stretch_angle = {"overlay_edit_uv_stretching_angle"};
  StaticShader uv_analysis_stretch_area = {"overlay_edit_uv_stretching_area"};
  StaticShader uv_brush_stencil = {"overlay_edit_uv_stencil_image"};
  StaticShader uv_edit_edge = {"overlay_edit_uv_edges"};
  StaticShader uv_edit_face = {"overlay_edit_uv_faces"};
  StaticShader uv_edit_facedot = {"overlay_edit_uv_face_dots"};
  StaticShader uv_edit_vert = {"overlay_edit_uv_verts"};
  StaticShader uv_image_borders = {"overlay_edit_uv_tiled_image_borders"};
  StaticShader uv_paint_mask = {"overlay_edit_uv_mask_image"};
  StaticShader uv_wireframe = {"overlay_wireframe_uv"};
  StaticShader xray_fade = {"overlay_xray_fade"};

  /** Selectable Shaders */
  StaticShader armature_envelope_fill = shader_selectable("overlay_armature_envelope_solid");
  StaticShader armature_envelope_outline = shader_selectable("overlay_armature_envelope_outline");
  StaticShader armature_shape_outline = shader_selectable("overlay_armature_shape_outline");
  StaticShader armature_shape_fill = shader_selectable("overlay_armature_shape_solid");
  StaticShader armature_shape_wire = shader_selectable("overlay_armature_shape_wire");
  StaticShader armature_shape_wire_strip = shader_selectable("overlay_armature_shape_wire_strip");
  StaticShader armature_sphere_outline = shader_selectable("overlay_armature_sphere_outline");
  StaticShader armature_sphere_fill = shader_selectable("overlay_armature_sphere_solid");
  StaticShader armature_stick = shader_selectable("overlay_armature_stick");
  StaticShader armature_wire = shader_selectable("overlay_armature_wire");
  StaticShader depth_curves = shader_selectable("overlay_depth_curves");
  StaticShader depth_grease_pencil = shader_selectable("overlay_depth_gpencil");
  StaticShader depth_mesh = shader_selectable("overlay_depth_mesh");
  StaticShader depth_mesh_conservative = shader_selectable("overlay_depth_mesh_conservative");
  StaticShader depth_pointcloud = shader_selectable("overlay_depth_pointcloud");
  StaticShader extra_shape = shader_selectable("overlay_extra");
  StaticShader extra_point = shader_selectable("overlay_extra_point");
  StaticShader extra_wire = shader_selectable("overlay_extra_wire");
  StaticShader extra_wire_object = shader_selectable("overlay_extra_wire_object");
  StaticShader extra_loose_points = shader_selectable("overlay_extra_loose_point");
  StaticShader extra_grid = shader_selectable("overlay_extra_grid");
  StaticShader extra_ground_line = shader_selectable("overlay_extra_groundline");
  StaticShader image_plane = shader_selectable("overlay_image");
  StaticShader image_plane_depth_bias = shader_selectable("overlay_image_depth_bias");
  StaticShader particle_dot = shader_selectable("overlay_particle_dot");
  StaticShader particle_shape = shader_selectable("overlay_particle_shape");
  StaticShader particle_hair = shader_selectable("overlay_particle_hair");
  StaticShader wireframe_mesh = shader_selectable("overlay_wireframe");
  /* Draw objects without edges for the wireframe overlay. */
  StaticShader wireframe_points = shader_selectable("overlay_wireframe_points");
  StaticShader wireframe_curve = shader_selectable("overlay_wireframe_curve");

  StaticShader fluid_grid_lines_flags = shader_selectable_no_clip(
      "overlay_volume_gridlines_flags");
  StaticShader fluid_grid_lines_flat = shader_selectable_no_clip("overlay_volume_gridlines_flat");
  StaticShader fluid_grid_lines_range = shader_selectable_no_clip(
      "overlay_volume_gridlines_range");
  StaticShader fluid_velocity_streamline = shader_selectable_no_clip(
      "overlay_volume_velocity_streamline");
  StaticShader fluid_velocity_mac = shader_selectable_no_clip("overlay_volume_velocity_mac");
  StaticShader fluid_velocity_needle = shader_selectable_no_clip("overlay_volume_velocity_needle");

  /** Module */
  /** Only to be used by Instance constructor. */
  static ShaderModule &module_get(SelectionType selection_type, bool clipping_enabled);
  static void module_free();

 private:
  ShaderModule(const SelectionType selection_type, const bool clipping_enabled)
      : selection_type_(selection_type), clipping_enabled_(clipping_enabled) {};

  StaticShader shader_clippable(const char *create_info_name);
  StaticShader shader_selectable(const char *create_info_name);
  StaticShader shader_selectable_no_clip(const char *create_info_name);
};

struct GreasePencilDepthPlane {
  /* Plane data to reference as push constant.
   * Will be computed just before drawing. */
  float4 plane;
  /* Center and size of the bounding box of the Grease Pencil object. */
  Bounds<float3> bounds;
  /* Grease-pencil object resource handle. */
  ResourceHandleRange handle;
};

struct Resources : public select::SelectMap {
  ShaderModule *shaders = nullptr;

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
  Framebuffer overlay_output_color_only_fb = {"overlay_output_color_only_fb"};
  /* Depth, Output Color. */
  Framebuffer overlay_output_fb = {"overlay_output_fb"};

  /* Render Frame-buffers. Only used for multiplicative blending on top of the render. */
  /* TODO(fclem): Remove the usage of these somehow. This is against design. */
  gpu::FrameBuffer *render_fb = nullptr;
  gpu::FrameBuffer *render_in_front_fb = nullptr;

  /* Target containing line direction and data for line expansion and anti-aliasing. */
  TextureFromPool line_tx = {"line_tx"};
  /* Target containing overlay color before anti-aliasing. */
  TextureFromPool overlay_tx = {"overlay_tx"};
  /* Target containing depth of overlays when xray is enabled. */
  TextureFromPool xray_depth_tx = {"xray_depth_tx"};
  TextureFromPool xray_depth_in_front_tx = {"xray_depth_in_front_tx"};

  /* Texture that are usually allocated inside. These are fallback when they aren't.
   * They are then wrapped inside the #TextureRefs below. */
  TextureFromPool depth_in_front_alloc_tx = {"overlay_depth_in_front_tx"};
  TextureFromPool color_overlay_alloc_tx = {"overlay_color_overlay_alloc_tx"};
  TextureFromPool color_render_alloc_tx = {"overlay_color_render_alloc_tx"};

  /* 1px texture containing only maximum depth. To be used for fulfilling bindings when depth
   * texture is not available or not needed. */
  Texture dummy_depth_tx = {"dummy_depth_tx"};

  /* Global vector for all grease pencil depth planes.
   * Managed by the grease pencil overlay module.
   * This is to avoid passing the grease pencil overlay class to other overlay and
   * keep draw_grease_pencil as a static function.
   * Memory is reference, so we have to use a container with fixed memory. */
  detail::SubPassVector<GreasePencilDepthPlane, 16> depth_planes;
  int64_t depth_planes_count = 0;

  draw::UniformBuffer<UniformData> globals_buf;
  UniformData &theme = globals_buf;
  draw::UniformArrayBuffer<float4, 6> clip_planes_buf;
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

  /** Copy of the settings the current texture was generated with. Used to detect updates. */
  bool weight_ramp_custom = false;
  ColorBand weight_ramp_copy = {};
  /** Baked color ramp texture from theme and user settings. Maps weight [0..1] to color. */
  Texture weight_ramp_tx = {"weight_ramp"};

  Vector<MovieClip *> bg_movie_clips;

  const ShapeCache &shapes;

  Resources(const SelectionType selection_type_, const ShapeCache &shapes_)
      : select::SelectMap(selection_type_), shapes(shapes_) {};

  ~Resources()
  {
    free_movieclips_textures();
  }

  void update_theme_settings(const DRWContext *ctx, const State &state);
  void update_clip_planes(const State &state);

  void init(bool clipping_enabled)
  {
    shaders = &overlay::ShaderModule::module_get(selection_type, clipping_enabled);
    shaders->anti_aliasing.ensure_compile_async();
    shaders->armature_degrees_of_freedom.ensure_compile_async();
    shaders->armature_envelope_fill.ensure_compile_async();
    shaders->armature_envelope_outline.ensure_compile_async();
    shaders->armature_shape_fill.ensure_compile_async();
    shaders->armature_shape_outline.ensure_compile_async();
    shaders->armature_shape_wire_strip.ensure_compile_async();
    shaders->armature_shape_wire.ensure_compile_async();
    shaders->armature_sphere_fill.ensure_compile_async();
    shaders->armature_sphere_outline.ensure_compile_async();
    shaders->armature_stick.ensure_compile_async();
    shaders->armature_wire.ensure_compile_async();
    shaders->attribute_viewer_curve.ensure_compile_async();
    shaders->attribute_viewer_curves.ensure_compile_async();
    shaders->attribute_viewer_mesh.ensure_compile_async();
    shaders->attribute_viewer_pointcloud.ensure_compile_async();
    shaders->background_fill.ensure_compile_async();
    shaders->curve_edit_handles.ensure_compile_async();
    shaders->curve_edit_line.ensure_compile_async();
    shaders->curve_edit_points.ensure_compile_async();
    shaders->depth_curves.ensure_compile_async();
    shaders->depth_grease_pencil.ensure_compile_async();
    shaders->depth_mesh.ensure_compile_async();
    shaders->depth_pointcloud.ensure_compile_async();
    shaders->extra_grid.ensure_compile_async();
    shaders->extra_ground_line.ensure_compile_async();
    shaders->extra_loose_points.ensure_compile_async();
    shaders->extra_point.ensure_compile_async();
    shaders->extra_shape.ensure_compile_async();
    shaders->extra_wire_object.ensure_compile_async();
    shaders->extra_wire.ensure_compile_async();
    shaders->fluid_grid_lines_flags.ensure_compile_async();
    shaders->fluid_grid_lines_flat.ensure_compile_async();
    shaders->fluid_grid_lines_range.ensure_compile_async();
    shaders->fluid_velocity_mac.ensure_compile_async();
    shaders->fluid_velocity_needle.ensure_compile_async();
    shaders->fluid_velocity_streamline.ensure_compile_async();
    shaders->grid.ensure_compile_async();
    shaders->image_plane_depth_bias.ensure_compile_async();
    shaders->lattice_points.ensure_compile_async();
    shaders->lattice_wire.ensure_compile_async();
    shaders->legacy_curve_edit_handles.ensure_compile_async();
    shaders->legacy_curve_edit_points.ensure_compile_async();
    shaders->legacy_curve_edit_wires.ensure_compile_async();
    shaders->light_spot_cone.ensure_compile_async();
    shaders->mesh_analysis.ensure_compile_async();
    shaders->mesh_edit_depth.ensure_compile_async();
    shaders->mesh_edit_edge.ensure_compile_async();
    shaders->mesh_edit_face.ensure_compile_async();
    shaders->mesh_edit_facedot.ensure_compile_async();
    shaders->mesh_edit_skin_root.ensure_compile_async();
    shaders->mesh_edit_vert.ensure_compile_async();
    shaders->motion_path_line.ensure_compile_async();
    shaders->motion_path_vert.ensure_compile_async();
    shaders->outline_detect.ensure_compile_async();
    shaders->outline_prepass_curves.ensure_compile_async();
    shaders->outline_prepass_gpencil.ensure_compile_async();
    shaders->outline_prepass_mesh.ensure_compile_async();
    shaders->outline_prepass_pointcloud.ensure_compile_async();
    shaders->outline_prepass_wire.ensure_compile_async();
    shaders->paint_weight_fake_shading.ensure_compile_async();
    shaders->particle_dot.ensure_compile_async();
    shaders->particle_edit_edge.ensure_compile_async();
    shaders->particle_edit_vert.ensure_compile_async();
    shaders->particle_hair.ensure_compile_async();
    shaders->particle_shape.ensure_compile_async();
    shaders->pointcloud_points.ensure_compile_async();
    shaders->uniform_color.ensure_compile_async();
    shaders->wireframe_curve.ensure_compile_async();
    shaders->wireframe_mesh.ensure_compile_async();
    shaders->wireframe_points.ensure_compile_async();
  }

  void begin_sync(int clipping_plane_count)
  {
    SelectMap::begin_sync(clipping_plane_count);
    free_movieclips_textures();
  }

  void acquire(const DRWContext *draw_ctx, const State &state)
  {
    DefaultTextureList &viewport_textures = *draw_ctx->viewport_texture_list_get();
    DefaultFramebufferList &viewport_framebuffers = *draw_ctx->viewport_framebuffer_list_get();
    this->depth_tx.wrap(viewport_textures.depth);
    this->depth_in_front_tx.wrap(viewport_textures.depth_in_front);
    this->color_overlay_tx.wrap(viewport_textures.color_overlay);
    this->color_render_tx.wrap(viewport_textures.color);

    this->render_fb = viewport_framebuffers.default_fb;
    this->render_in_front_fb = viewport_framebuffers.in_front_fb;

    int2 render_size = int2(this->depth_tx.size());

    if (state.xray_enabled) {
      /* For X-ray we render the scene to a separate depth buffer. */
      this->xray_depth_tx.acquire(render_size, gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8);
      this->depth_target_tx.wrap(this->xray_depth_tx);
      /* TODO(fclem): Remove mandatory allocation. */
      this->xray_depth_in_front_tx.acquire(render_size,
                                           gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8);
      this->depth_target_in_front_tx.wrap(this->xray_depth_in_front_tx);
    }
    else {
      /* TODO(fclem): Remove mandatory allocation. */
      if (!this->depth_in_front_tx.is_valid()) {
        this->depth_in_front_alloc_tx.acquire(render_size,
                                              gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8);
        this->depth_in_front_tx.wrap(this->depth_in_front_alloc_tx);
      }
      this->depth_target_tx.wrap(this->depth_tx);
      this->depth_target_in_front_tx.wrap(this->depth_in_front_tx);
    }

    /* TODO: Better semantics using a switch? */
    if (!this->color_overlay_tx.is_valid()) {
      /* Likely to be the selection case. Allocate dummy texture and bind only depth buffer. */
      this->color_overlay_alloc_tx.acquire(int2(1, 1), gpu::TextureFormat::SRGBA_8_8_8_8);
      this->color_render_alloc_tx.acquire(int2(1, 1), gpu::TextureFormat::SRGBA_8_8_8_8);

      this->color_overlay_tx.wrap(this->color_overlay_alloc_tx);
      this->color_render_tx.wrap(this->color_render_alloc_tx);

      this->line_tx.acquire(int2(1, 1), gpu::TextureFormat::UNORM_8_8_8_8);
      this->overlay_tx.acquire(int2(1, 1), gpu::TextureFormat::SRGBA_8_8_8_8);

      this->overlay_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_tx));
      this->overlay_line_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_tx));
      this->overlay_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_tx));
      this->overlay_line_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_tx));
    }
    else {
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE |
                               GPU_TEXTURE_USAGE_ATTACHMENT;
      this->line_tx.acquire(render_size, gpu::TextureFormat::UNORM_8_8_8_8, usage);
      this->overlay_tx.acquire(render_size, gpu::TextureFormat::SRGBA_8_8_8_8, usage);

      this->overlay_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_tx),
                              GPU_ATTACHMENT_TEXTURE(this->overlay_tx));
      this->overlay_line_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_tx),
                                   GPU_ATTACHMENT_TEXTURE(this->overlay_tx),
                                   GPU_ATTACHMENT_TEXTURE(this->line_tx));
      this->overlay_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_in_front_tx),
                                       GPU_ATTACHMENT_TEXTURE(this->overlay_tx));
      this->overlay_line_in_front_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_target_in_front_tx),
                                            GPU_ATTACHMENT_TEXTURE(this->overlay_tx),
                                            GPU_ATTACHMENT_TEXTURE(this->line_tx));
    }

    this->overlay_line_only_fb.ensure(GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(this->overlay_tx),
                                      GPU_ATTACHMENT_TEXTURE(this->line_tx));
    this->overlay_color_only_fb.ensure(GPU_ATTACHMENT_NONE,
                                       GPU_ATTACHMENT_TEXTURE(this->overlay_tx));

    this->overlay_output_color_only_fb.ensure(GPU_ATTACHMENT_NONE,
                                              GPU_ATTACHMENT_TEXTURE(this->color_overlay_tx));
    this->overlay_output_fb.ensure(GPU_ATTACHMENT_TEXTURE(this->depth_tx),
                                   GPU_ATTACHMENT_TEXTURE(this->color_overlay_tx));
  }

  void release()
  {
    this->line_tx.release();
    this->overlay_tx.release();
    this->xray_depth_tx.release();
    this->xray_depth_in_front_tx.release();
    this->depth_in_front_alloc_tx.release();
    this->color_overlay_alloc_tx.release();
    this->color_render_alloc_tx.release();
    free_movieclips_textures();
  }

  ThemeColorID object_wire_theme_id(const ObjectRef &ob_ref, const State &state) const
  {
    const bool is_edit = (state.object_mode & OB_MODE_EDIT) &&
                         (ob_ref.object->mode & OB_MODE_EDIT);
    const bool active = ob_ref.is_active(state.object_active);
    const bool is_selected = ((ob_ref.object->base_flag & BASE_SELECTED) != 0);

    /* Object in edit mode. */
    if (is_edit) {
      return TH_WIRE_EDIT;
    }
    /* Transformed object during operators. */
    if (((G.moving & G_TRANSFORM_OBJ) != 0) && is_selected) {
      return TH_TRANSFORM;
    }
    /* Sets the 'theme_id' or fall back to wire */
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
      return theme.colors.wire;
    }
    switch (theme_id) {
      case TH_WIRE_EDIT:
        return theme.colors.wire_edit;
      case TH_ACTIVE:
        return theme.colors.active_object;
      case TH_SELECT:
        return theme.colors.object_select;
      case TH_TRANSFORM:
        return theme.colors.transform;
      case TH_SPEAKER:
        return theme.colors.speaker;
      case TH_CAMERA:
        return theme.colors.camera;
      case TH_EMPTY:
        return theme.colors.empty;
      case TH_LIGHT:
        return theme.colors.light;
      default:
        return theme.colors.wire;
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
        return float4(float3(&state.scene->world->horr), 0.0f);
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
    bg_movie_clips.clear();
  }

  static float vertex_size_get()
  {
    /* M_SQRT2 to be at least the same size of the old square */
    return max_ff(1.0f, UI_GetThemeValuef(TH_VERTEX_SIZE) * float(M_SQRT2) / 2.0f);
  }

  /** Convenience functions. */

  /* Returns true if drawing for any selection mode. */
  bool is_selection() const
  {
    return this->selection_type != SelectionType::DISABLED;
  }
};

/* List of flat objects draw-calls.
 * In order to not loose selection display of flat objects view from the side,
 * we store them in a list and add them to the pass just in time if their flat side is
 * perpendicular to the view. */
/* Reference to a flat object.
 * Allow deferred rendering condition of flat object for special purpose. */
struct FlatObjectRef {
  gpu::Batch *geom;
  ResourceHandleRange handle;
  int flattened_axis_id;

  /* Returns flat axis index if only one axis is flat. Returns -1 otherwise. */
  static int flat_axis_index_get(const Object *ob)
  {
    BLI_assert(ELEM(ob->type,
                    OB_MESH,
                    OB_CURVES_LEGACY,
                    OB_SURF,
                    OB_FONT,
                    OB_CURVES,
                    OB_POINTCLOUD,
                    OB_VOLUME));

    float dim[3];
    BKE_object_dimensions_get(ob, dim);
    if (dim[0] == 0.0f) {
      return 0;
    }
    if (dim[1] == 0.0f) {
      return 1;
    }
    if (dim[2] == 0.0f) {
      return 2;
    }
    return -1;
  }

  using Callback = FunctionRef<void(gpu::Batch *geom, ResourceIndex handle)>;

  /* Execute callback for every handles that is orthogonal to the view.
   * Note: Only works in orthogonal view. */
  void if_flat_axis_orthogonal_to_view(Manager &manager, const View &view, Callback callback) const
  {
    for (ResourceIndex resource_index : handle.index_range()) {
      const float4x4 &object_to_world =
          manager.matrix_buf.current().get_or_resize(resource_index.resource_index()).model;

      float3 view_forward = view.forward();
      float3 axis_not_flat_a = (flattened_axis_id == 0) ? object_to_world.y_axis() :
                                                          object_to_world.x_axis();
      float3 axis_not_flat_b = (flattened_axis_id == 1) ? object_to_world.z_axis() :
                                                          object_to_world.y_axis();
      float3 axis_flat = math::cross(axis_not_flat_a, axis_not_flat_b);

      if (math::abs(math::dot(view_forward, axis_flat)) < 1e-3f) {
        callback(geom, resource_index);
      }
    }
  }
};

/**
 * Buffer containing instances of a certain shape.
 */
template<typename InstanceDataT> struct ShapeInstanceBuf : private select::SelectBuf {

  StorageVectorBuffer<InstanceDataT> data_buf;

  ShapeInstanceBuf(const SelectionType selection_type, const char *name = nullptr)
      : select::SelectBuf(selection_type), data_buf(name) {};

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
    pass.draw_expand(shape, primitive_type, primitive_len, data_buf.size());
  }
};

struct VertexPrimitiveBuf {
 protected:
  select::SelectBuf select_buf;
  StorageVectorBuffer<VertexData> data_buf;
  int color_id = 0;

  VertexPrimitiveBuf(const SelectionType selection_type, const char *name = nullptr)
      : select_buf(selection_type), data_buf(name) {};

  void append(const float3 &position, const float4 &color)
  {
    data_buf.append({float4(position, 0.0f), color});
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
