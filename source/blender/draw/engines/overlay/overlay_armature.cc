/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DRW_render.hh"

#include "BLI_listbase_wrapper.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_deform.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_armature.hh"
#include "ED_view3d.hh"

#include "ANIM_armature.hh"
#include "ANIM_bonecolor.hh"

#include "UI_resources.hh"

#include "draw_cache.hh"
#include "draw_context_private.hh"
#include "draw_manager_text.hh"

#include "overlay_armature.hh"

#include "draw_cache_impl.hh"

#define PT_DEFAULT_RAD 0.05f /* radius of the point batch. */

using namespace blender::draw::overlay;

/**
 * Container for either an #EditBone or a #bPoseChannel.
 */
class UnifiedBonePtr {
 private:
  union {
    EditBone *eBone_;
    bPoseChannel *pchan_;
  };
  bool is_editbone_; /* Discriminator for the above union. */

 public:
  UnifiedBonePtr(const UnifiedBonePtr &ptr) = default;
  ~UnifiedBonePtr() {}

  UnifiedBonePtr(EditBone *eBone) : eBone_(eBone), is_editbone_(true) {}
  UnifiedBonePtr(bPoseChannel *pchan) : pchan_(pchan), is_editbone_(false) {}

  const char *name() const
  {
    return is_editbone_ ? eBone_->name : pchan_->name;
  }

  const EditBone *as_editbone() const
  {
    BLI_assert_msg(is_editbone_,
                   "conversion to EditBone* only possible when "
                   "UnifiedBonePtr contains an edit bone");
    return eBone_;
  }
  EditBone *as_editbone()
  {
    BLI_assert_msg(is_editbone_,
                   "conversion to EditBone* only possible when "
                   "UnifiedBonePtr contains an edit bone");
    return eBone_;
  }

  const bPoseChannel *as_posebone() const
  {
    BLI_assert_msg(!is_editbone_,
                   "conversion to bPoseChannel* only possible when "
                   "UnifiedBonePtr contains a pose channel");
    return pchan_;
  }
  bPoseChannel *as_posebone()
  {
    BLI_assert_msg(!is_editbone_,
                   "conversion to bPoseChannel* only possible when "
                   "UnifiedBonePtr contains a pose channel");
    return pchan_;
  }

  bool is_editbone() const
  {
    return is_editbone_;
  };
  bool is_posebone() const
  {
    return !is_editbone();
  };

  void get(const EditBone **eBone, const bPoseChannel **pchan) const
  {
    *eBone = eBone_;
    *pchan = pchan_;
  }
  void get(EditBone **eBone, bPoseChannel **pchan)
  {
    *eBone = eBone_;
    *pchan = pchan_;
  }

  eBone_Flag flag() const
  {
    if (is_editbone_) {
      return static_cast<eBone_Flag>(eBone_->flag);
    }
    /* Making sure the select flag is set correctly since it moved to the pose channel. */
    eBone_Flag flag = static_cast<eBone_Flag>(pchan_->bone->flag);
    if (pchan_->flag & POSE_SELECTED) {
      flag |= BONE_SELECTED;
    }
    else {
      flag &= ~BONE_SELECTED;
    }
    return flag;
  }

  /** Return the pose bone's constraint flags, or 0 if not a pose bone. */
  ePchan_ConstFlag constflag() const
  {
    return ePchan_ConstFlag(is_editbone_ ? 0 : pchan_->constflag);
  }

  bool has_parent() const
  {
    return is_editbone_ ? eBone_->parent != nullptr : pchan_->bone->parent != nullptr;
  }

  using f44 = float[4][4];
  const f44 &disp_mat() const
  {
    return is_editbone_ ? eBone_->disp_mat : pchan_->disp_mat;
  }
  f44 &disp_mat()
  {
    return is_editbone_ ? eBone_->disp_mat : pchan_->disp_mat;
  }

  const f44 &disp_tail_mat() const
  {
    return is_editbone_ ? eBone_->disp_tail_mat : pchan_->disp_tail_mat;
  }

  f44 &disp_tail_mat()
  {
    return is_editbone_ ? eBone_->disp_tail_mat : pchan_->disp_tail_mat;
  }

  /* For some, to me unknown, reason, the drawing code passes these around as pointers. This is the
   * reason that these are returned as references. I'll leave refactoring that for another time. */
  const float &rad_head() const
  {
    return is_editbone_ ? eBone_->rad_head : pchan_->bone->rad_head;
  }

  const float &rad_tail() const
  {
    return is_editbone_ ? eBone_->rad_tail : pchan_->bone->rad_tail;
  }

  const blender::animrig::BoneColor &effective_bonecolor() const
  {
    if (is_editbone_) {
      return eBone_->color.wrap();
    }

    if (pchan_->color.palette_index == 0) {
      /* If the pchan has the 'default' color, treat it as a signal to use the underlying bone
       * color. */
      return pchan_->bone->color.wrap();
    }
    return pchan_->color.wrap();
  }
};

/* -------------------------------------------------------------------- */
/** \name Shading Groups
 * \{ */

/* Stick */
static void drw_shgroup_bone_stick(const Armatures::DrawContext *ctx,
                                   const float (*bone_mat)[4],
                                   const float col_wire[4],
                                   const float col_bone[4],
                                   const float col_head[4],
                                   const float col_tail[4],
                                   const int select_id)
{
  float4x4 bmat = float4x4(bone_mat);
  float3 head = math::transform_point(ctx->ob->object_to_world(), bmat.location());
  float3 tail = math::transform_point(ctx->ob->object_to_world(), bmat.location() + bmat.y_axis());

  auto sel_id = ctx->res->select_id(*ctx->ob_ref, select_id);

  ctx->bone_buf->stick_buf.append({head,
                                   tail,
                                   *(float4 *)col_wire,
                                   *(float4 *)col_bone,
                                   *(float4 *)col_head,
                                   *(float4 *)col_tail},
                                  sel_id);
}

/* Envelope */
static void drw_shgroup_bone_envelope_distance(const Armatures::DrawContext *ctx,
                                               const float (*bone_mat)[4],
                                               const float *radius_head,
                                               const float *radius_tail,
                                               const float *distance)
{
  if (ctx->draw_envelope_distance) {
    float head_sph[4] = {0.0f, 0.0f, 0.0f, 1.0f}, tail_sph[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float xaxis[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    /* Still less operation than m4 multiplication. */
    mul_m4_v4(bone_mat, head_sph);
    mul_m4_v4(bone_mat, tail_sph);
    mul_m4_v4(bone_mat, xaxis);
    mul_m4_v4(ctx->ob->object_to_world().ptr(), head_sph);
    mul_m4_v4(ctx->ob->object_to_world().ptr(), tail_sph);
    mul_m4_v4(ctx->ob->object_to_world().ptr(), xaxis);
    sub_v3_v3(xaxis, head_sph);
    float obscale = mat4_to_scale(ctx->ob->object_to_world().ptr());
    head_sph[3] = *radius_head * obscale;
    head_sph[3] += *distance * obscale;
    tail_sph[3] = *radius_tail * obscale;
    tail_sph[3] += *distance * obscale;
    /* TODO(fclem): Cleanup these casts when Overlay Next is shipped. */
    ctx->bone_buf->envelope_distance_buf.append(
        {*(float4 *)head_sph, *(float4 *)tail_sph, *(float3 *)xaxis},
        draw::select::SelectMap::select_invalid_id());
  }
}

static void drw_shgroup_bone_envelope(const Armatures::DrawContext *ctx,
                                      const float (*bone_mat)[4],
                                      const float bone_col[4],
                                      const float hint_col[4],
                                      const float outline_col[4],
                                      const float *radius_head,
                                      const float *radius_tail,
                                      const int select_id)
{
  float head_sph[4] = {0.0f, 0.0f, 0.0f, 1.0f}, tail_sph[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  float xaxis[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  /* Still less operation than m4 multiplication. */
  mul_m4_v4(bone_mat, head_sph);
  mul_m4_v4(bone_mat, tail_sph);
  mul_m4_v4(bone_mat, xaxis);
  mul_m4_v4(ctx->ob->object_to_world().ptr(), head_sph);
  mul_m4_v4(ctx->ob->object_to_world().ptr(), tail_sph);
  mul_m4_v4(ctx->ob->object_to_world().ptr(), xaxis);
  float obscale = mat4_to_scale(ctx->ob->object_to_world().ptr());
  head_sph[3] = *radius_head * obscale;
  tail_sph[3] = *radius_tail * obscale;

  auto sel_id = (ctx->bone_buf) ? ctx->res->select_id(*ctx->ob_ref, select_id) :
                                  draw::select::SelectMap::select_invalid_id();

  if (head_sph[3] < 0.0f || tail_sph[3] < 0.0f) {
    draw::overlay::BoneInstanceData inst_data;
    if (head_sph[3] < 0.0f) {
      /* Draw Tail only */
      scale_m4_fl(inst_data.mat, tail_sph[3] / PT_DEFAULT_RAD);
      copy_v3_v3(inst_data.mat[3], tail_sph);
    }
    else {
      /* Draw Head only */
      scale_m4_fl(inst_data.mat, head_sph[3] / PT_DEFAULT_RAD);
      copy_v3_v3(inst_data.mat[3], head_sph);
    }

    if (ctx->is_filled) {
      ctx->bone_buf->sphere_fill_buf.append({inst_data.mat44, bone_col, hint_col}, sel_id);
    }
    if (outline_col[3] > 0.0f) {
      ctx->bone_buf->sphere_outline_buf.append({inst_data.mat44, outline_col}, sel_id);
    }
  }
  else {
    /* Draw Body */
    float tmp_sph[4];
    float len = len_v3v3(tail_sph, head_sph);
    float fac_head = (len - head_sph[3]) / len;
    float fac_tail = (len - tail_sph[3]) / len;
    /* Small epsilon to avoid problem with float precision in shader. */
    if (len > (tail_sph[3] + head_sph[3]) + 1e-8f) {
      copy_v4_v4(tmp_sph, head_sph);
      interp_v4_v4v4(head_sph, tail_sph, head_sph, fac_head);
      interp_v4_v4v4(tail_sph, tmp_sph, tail_sph, fac_tail);

      if (ctx->is_filled) {
        /* TODO(fclem): Cleanup these casts when Overlay Next is shipped. */
        ctx->bone_buf->envelope_fill_buf.append({*(float4 *)head_sph,
                                                 *(float4 *)tail_sph,
                                                 *(float3 *)bone_col,
                                                 *(float3 *)hint_col,
                                                 *(float3 *)xaxis},
                                                sel_id);
      }
      if (outline_col[3] > 0.0f) {
        ctx->bone_buf->envelope_outline_buf.append(
            {*(float4 *)head_sph, *(float4 *)tail_sph, *(float4 *)outline_col, *(float3 *)xaxis},
            sel_id);
      }
    }
    else {
      /* Distance between endpoints is too small for a capsule. Draw a Sphere instead. */
      float fac = max_ff(fac_head, 1.0f - fac_tail);
      interp_v4_v4v4(tmp_sph, tail_sph, head_sph, clamp_f(fac, 0.0f, 1.0f));

      draw::overlay::BoneInstanceData inst_data;
      scale_m4_fl(inst_data.mat, tmp_sph[3] / PT_DEFAULT_RAD);
      copy_v3_v3(inst_data.mat[3], tmp_sph);

      if (ctx->is_filled) {
        ctx->bone_buf->sphere_fill_buf.append({inst_data.mat44, bone_col, hint_col}, sel_id);
      }
      if (outline_col[3] > 0.0f) {
        ctx->bone_buf->sphere_outline_buf.append({inst_data.mat44, outline_col}, sel_id);
      }
    }
  }
}

/* Custom (geometry) */

static void drw_shgroup_bone_custom_solid_mesh(const Armatures::DrawContext *ctx,
                                               Mesh &mesh,
                                               const float (*bone_mat)[4],
                                               const float bone_color[4],
                                               const float hint_color[4],
                                               const float outline_color[4],
                                               const float wire_width,
                                               const draw::select::ID select_id,
                                               Object &custom)
{
  using namespace blender::draw;
  /* TODO(fclem): arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  DRW_mesh_batch_cache_validate(mesh);

  blender::gpu::Batch *surf = DRW_mesh_batch_cache_get_surface(mesh);
  blender::gpu::Batch *edges = DRW_mesh_batch_cache_get_edge_detection(mesh, nullptr);
  blender::gpu::Batch *loose_edges = DRW_mesh_batch_cache_get_loose_edges(mesh);
  draw::overlay::BoneInstanceData inst_data;

  if (surf || edges || loose_edges) {
    inst_data.mat44 = ctx->ob->object_to_world() * float4x4(bone_mat);
  }

  if (surf) {
    inst_data.set_hint_color(hint_color);
    inst_data.set_color(bone_color);
    if (ctx->is_filled) {
      ctx->bone_buf->custom_shape_fill_get_buffer(surf).append(inst_data, select_id);
    }
  }

  if (edges) {
    inst_data.set_color(outline_color);
    ctx->bone_buf->custom_shape_outline_get_buffer(edges).append(inst_data, select_id);
  }

  if (loose_edges) {
    inst_data.set_hint_color(outline_color);
    inst_data.set_color(float4(UNPACK3(outline_color), wire_width / WIRE_WIDTH_COMPRESSION));
    ctx->bone_buf->custom_shape_wire_get_buffer(loose_edges).append(inst_data, select_id);
  }

  /* TODO(fclem): needs to be moved elsewhere. */
  drw_batch_cache_generate_requested_delayed(&custom);
}

static void drw_shgroup_bone_custom_mesh_wire(const Armatures::DrawContext *ctx,
                                              Mesh &mesh,
                                              const float (*bone_mat)[4],
                                              const float color[4],
                                              const float wire_width,
                                              const draw::select::ID select_id,
                                              Object &custom)
{
  using namespace blender::draw;
  /* TODO(fclem): arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  DRW_mesh_batch_cache_validate(mesh);

  blender::gpu::Batch *geom = DRW_mesh_batch_cache_get_all_edges(mesh);
  if (geom) {
    draw::overlay::BoneInstanceData inst_data;
    inst_data.mat44 = ctx->ob->object_to_world() * float4x4(bone_mat);
    inst_data.set_hint_color(color);
    inst_data.set_color(float4(UNPACK3(color), wire_width / WIRE_WIDTH_COMPRESSION));

    ctx->bone_buf->custom_shape_wire_get_buffer(geom).append(inst_data, select_id);
  }

  /* TODO(fclem): needs to be moved elsewhere. */
  drw_batch_cache_generate_requested_delayed(&custom);
}

static void drw_shgroup_custom_bone_curve(const Armatures::DrawContext *ctx,
                                          Curve *curve,
                                          const float (*bone_mat)[4],
                                          const float outline_color[4],
                                          const float wire_width,
                                          const draw::select::ID select_id,
                                          Object *custom)
{
  using namespace blender::draw;
  /* TODO(fclem): arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  DRW_curve_batch_cache_validate(curve);

  /* This only handles curves without any surface. The other curve types should have been converted
   * to meshes and rendered in the mesh drawing function. */
  blender::gpu::Batch *loose_edges = nullptr;
  if (custom->type == OB_FONT) {
    loose_edges = DRW_cache_text_edge_wire_get(custom);
  }
  else {
    loose_edges = DRW_cache_curve_edge_wire_get(custom);
  }

  if (loose_edges) {
    draw::overlay::BoneInstanceData inst_data;
    inst_data.mat44 = ctx->ob->object_to_world() * float4x4(bone_mat);
    inst_data.set_hint_color(outline_color);
    inst_data.set_color(float4(UNPACK3(outline_color), wire_width / WIRE_WIDTH_COMPRESSION));

    ctx->bone_buf->custom_shape_wire_get_buffer(loose_edges).append(inst_data, select_id);
  }

  /* TODO(fclem): needs to be moved elsewhere. */
  drw_batch_cache_generate_requested_delayed(custom);
}

static void drw_shgroup_bone_custom_solid(const Armatures::DrawContext *ctx,
                                          const float (*bone_mat)[4],
                                          const float bone_color[4],
                                          const float hint_color[4],
                                          const float outline_color[4],
                                          const float wire_width,
                                          const draw::select::ID select_id,
                                          Object *custom)
{
  /* The custom object is not an evaluated object, so its object->data field hasn't been replaced
   * by #data_eval. This is bad since it gives preference to an object's evaluated mesh over any
   * other data type, but supporting all evaluated geometry components would require a much
   * larger refactor of this area. */
  Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf_unchecked(custom);
  if (mesh != nullptr) {
    drw_shgroup_bone_custom_solid_mesh(ctx,
                                       DRW_mesh_get_for_drawing(*mesh),
                                       bone_mat,
                                       bone_color,
                                       hint_color,
                                       outline_color,
                                       wire_width,
                                       select_id,
                                       *custom);
    return;
  }

  if (ELEM(custom->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    drw_shgroup_custom_bone_curve(ctx,
                                  &DRW_object_get_data_for_drawing<Curve>(*custom),
                                  bone_mat,
                                  outline_color,
                                  wire_width,
                                  select_id,
                                  custom);
  }
}

static void drw_shgroup_bone_custom_wire(const Armatures::DrawContext *ctx,
                                         const float (*bone_mat)[4],
                                         const float color[4],
                                         const float wire_width,
                                         const draw::select::ID select_id,
                                         Object *custom)
{
  /* See comments in #drw_shgroup_bone_custom_solid. */
  Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf_unchecked(custom);
  if (mesh != nullptr) {
    drw_shgroup_bone_custom_mesh_wire(
        ctx, DRW_mesh_get_for_drawing(*mesh), bone_mat, color, wire_width, select_id, *custom);
    return;
  }

  if (ELEM(custom->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    drw_shgroup_custom_bone_curve(ctx,
                                  &DRW_object_get_data_for_drawing<Curve>(*custom),
                                  bone_mat,
                                  color,
                                  wire_width,
                                  select_id,
                                  custom);
  }
}

static void drw_shgroup_bone_custom_empty(const Armatures::DrawContext *ctx,
                                          const float (*bone_mat)[4],
                                          const float color[4],
                                          const float wire_width,
                                          const draw::select::ID select_id,
                                          Object *custom)
{
  using namespace blender::draw;

  gpu::Batch *geom = nullptr;
  switch (custom->empty_drawtype) {
    case OB_PLAINAXES:
      geom = ctx->res->shapes.plain_axes.get();
      break;
    case OB_SINGLE_ARROW:
      geom = ctx->res->shapes.single_arrow.get();
      break;
    case OB_CUBE:
      geom = ctx->res->shapes.cube.get();
      break;
    case OB_CIRCLE:
      geom = ctx->res->shapes.circle.get();
      break;
    case OB_EMPTY_SPHERE:
      geom = ctx->res->shapes.empty_sphere.get();
      break;
    case OB_EMPTY_CONE:
      geom = ctx->res->shapes.empty_cone.get();
      break;
    case OB_ARROWS:
      geom = ctx->res->shapes.arrows.get();
      break;
    case OB_EMPTY_IMAGE:
      /* Not supported. */
      return;
  }
  BLI_assert(geom);

  const float4 final_color(UNPACK3(color), 1.0f);

  draw::overlay::BoneInstanceData inst_data;
  inst_data.mat44 = ctx->ob->object_to_world() * float4x4(bone_mat) *
                    math::from_scale<float4x4>(float3(custom->empty_drawsize));
  inst_data.set_hint_color(final_color);
  inst_data.set_color(float4(UNPACK3(final_color), wire_width / WIRE_WIDTH_COMPRESSION));

  ctx->bone_buf->custom_shape_wire_get_buffer(geom).append(inst_data, select_id);
}

/* Head and tail sphere */
static void drw_shgroup_bone_sphere(const Armatures::DrawContext *ctx,
                                    const float (*bone_mat)[4],
                                    const float bone_color[4],
                                    const float hint_color[4],
                                    const float outline_color[4],
                                    const int select_id)
{
  auto sel_id = (ctx->bone_buf) ? ctx->res->select_id(*ctx->ob_ref, select_id) :
                                  draw::select::SelectMap::select_invalid_id();
  float4x4 mat = ctx->ob->object_to_world() * float4x4(bone_mat);

  if (ctx->is_filled) {
    ctx->bone_buf->sphere_fill_buf.append({mat, bone_color, hint_color}, sel_id);
  }
  if (outline_color[3] > 0.0f) {
    ctx->bone_buf->sphere_outline_buf.append({mat, outline_color}, sel_id);
  }
}

/* Axes */
static void drw_shgroup_bone_axes(const Armatures::DrawContext *ctx,
                                  const float (*bone_mat)[4],
                                  const float color[4])
{
  float4x4 mat = ctx->ob->object_to_world() * float4x4(bone_mat);
  /* Move to bone tail. */
  mat[3] += mat[1];
  ExtraInstanceData data(mat, color, 0.25f);
  /* NOTE: Axes are not drawn in bone selection (pose or edit mode).
   * They are only drawn and selectable in object mode. So only load the object select ID. */
  ctx->bone_buf->arrows_buf.append(data, ctx->res->select_id(*ctx->ob_ref));
}

/* Relationship lines */
static void drw_shgroup_bone_relationship_lines_ex(const Armatures::DrawContext *ctx,
                                                   const float start[3],
                                                   const float end[3],
                                                   const float color[4])
{
  float3 start_pt = math::transform_point(ctx->ob->object_to_world(), float3(start));
  float3 end_pt = math::transform_point(ctx->ob->object_to_world(), float3(end));

  /* Reverse order to have less stipple overlap. */
  ctx->bone_buf->relations_buf.append(end_pt, start_pt, float4(color));
}

static void drw_shgroup_bone_relationship_lines(const Armatures::DrawContext *ctx,
                                                const float start[3],
                                                const float end[3])
{
  const UniformData &theme = ctx->res->theme;
  drw_shgroup_bone_relationship_lines_ex(ctx, start, end, theme.colors.wire);
}

static void drw_shgroup_bone_ik_lines(const Armatures::DrawContext *ctx,
                                      const float start[3],
                                      const float end[3])
{
  const UniformData &theme = ctx->res->theme;
  drw_shgroup_bone_relationship_lines_ex(ctx, start, end, theme.colors.bone_ik_line);
}

static void drw_shgroup_bone_ik_no_target_lines(const Armatures::DrawContext *ctx,
                                                const float start[3],
                                                const float end[3])
{
  const UniformData &theme = ctx->res->theme;
  drw_shgroup_bone_relationship_lines_ex(ctx, start, end, theme.colors.bone_ik_line_no_target);
}

static void drw_shgroup_bone_ik_spline_lines(const Armatures::DrawContext *ctx,
                                             const float start[3],
                                             const float end[3])
{
  const UniformData &theme = ctx->res->theme;
  drw_shgroup_bone_relationship_lines_ex(ctx, start, end, theme.colors.bone_ik_line_spline);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing Theme Helpers
 *
 * \note this section is duplicate of code in 'drawarmature.c'.
 *
 * \{ */

/* This function sets the color-set for coloring a certain bone */
static void set_ctx_bcolor(Armatures::DrawContext *ctx, const UnifiedBonePtr bone)
{
  bArmature &arm = DRW_object_get_data_for_drawing<bArmature>(*ctx->ob);

  if ((arm.flag & ARM_COL_CUSTOM) == 0) {
    /* Only set a custom color if that's enabled on this armature. */
    ctx->bcolor = nullptr;
    return;
  }

  const blender::animrig::BoneColor &bone_color = bone.effective_bonecolor();
  ctx->bcolor = bone_color.effective_color();
}

/* This function is for brightening/darkening a given color (like UI_GetThemeColorShade3ubv()) */
static void cp_shade_color3ub(uchar cp[3], const int offset)
{
  int r, g, b;

  r = offset + int(cp[0]);
  CLAMP(r, 0, 255);
  g = offset + int(cp[1]);
  CLAMP(g, 0, 255);
  b = offset + int(cp[2]);
  CLAMP(b, 0, 255);

  cp[0] = r;
  cp[1] = g;
  cp[2] = b;
}

/**
 * Utility function to use a shaded version of one of the colors in 'bcolor'.
 *
 * The r_color parameter is put first for consistency with copy_v4_v4(dest, src).
 */
static void use_bone_color(float *r_color, const uint8_t *color_from_theme, const int shade_offset)
{
  uint8_t srgb_color[4] = {255, 255, 255, 255};
  /* Only copy RGB, not alpha.  The "alpha" channel in the bone theme colors is
   * essentially just padding, and should be ignored. */
  copy_v3_v3_uchar(srgb_color, color_from_theme);
  if (shade_offset != 0) {
    cp_shade_color3ub(srgb_color, shade_offset);
  }
  rgba_uchar_to_float(r_color, srgb_color);
  /* Meh, hardcoded srgb transform here. */
  srgb_to_linearrgb_v4(r_color, r_color);
};

static void get_pchan_color_wire(const UniformData &theme,
                                 const ThemeWireColor *bcolor,
                                 const eArmatureDrawMode draw_mode,
                                 const eBone_Flag boneflag,
                                 float r_color[4])
{
  const bool draw_active = boneflag & BONE_DRAW_ACTIVE;
  const bool draw_selected = boneflag & BONE_SELECTED;
  const bool is_edit = draw_mode == ARM_DRAW_MODE_EDIT;
  float4 wire_color;

  if (bcolor) {
    if (draw_active && draw_selected) {
      use_bone_color(r_color, bcolor->active, 0);
    }
    else if (draw_active) {
      use_bone_color(r_color, bcolor->active, -80);
    }
    else if (draw_selected) {
      use_bone_color(r_color, bcolor->select, 0);
    }
    else {
      use_bone_color(r_color, bcolor->solid, -50);
    }
  }
  else {
    if (draw_active && draw_selected) {
      wire_color = is_edit ? theme.colors.bone_active : theme.colors.bone_pose_active;
    }
    else if (draw_active) {
      wire_color = is_edit ? theme.colors.bone_active_unsel : theme.colors.bone_pose_active_unsel;
    }
    else if (draw_selected) {
      wire_color = is_edit ? theme.colors.bone_select : theme.colors.bone_pose;
    }
    else {
      wire_color = is_edit ? theme.colors.wire_edit : theme.colors.wire;
    }
    copy_v4_v4(r_color, wire_color);
  }
}

static void get_pchan_color_solid(const UniformData &theme,
                                  const ThemeWireColor *bcolor,
                                  float r_color[4])
{

  if (bcolor) {
    use_bone_color(r_color, bcolor->solid, 0);
  }
  else {
    copy_v4_v4(r_color, theme.colors.bone_solid);
  }
}

static void get_pchan_color_constraint(const UniformData &theme,
                                       const ThemeWireColor *bcolor,
                                       const UnifiedBonePtr bone,
                                       float r_color[4])
{
  const ePchan_ConstFlag constflag = bone.constflag();
  /* Not all flags should result in a different bone color. */
  const ePchan_ConstFlag flags_to_color = PCHAN_HAS_NO_TARGET | PCHAN_HAS_IK | PCHAN_HAS_SPLINEIK |
                                          PCHAN_HAS_CONST;
  if ((constflag & flags_to_color) == 0 ||
      (bcolor && (bcolor->flag & TH_WIRECOLOR_CONSTCOLS) == 0))
  {
    get_pchan_color_solid(theme, bcolor, r_color);
    return;
  }

  /* The constraint color needs to be blended with the solid color. */
  float solid_color[4];
  get_pchan_color_solid(theme, bcolor, solid_color);

  float4 constraint_color;
  if (constflag & PCHAN_HAS_NO_TARGET) {
    constraint_color = theme.colors.bone_pose_no_target;
  }
  else if (constflag & PCHAN_HAS_IK) {
    constraint_color = theme.colors.bone_pose_ik;
  }
  else if (constflag & PCHAN_HAS_SPLINEIK) {
    constraint_color = theme.colors.bone_pose_spline_ik;
  }
  else if (constflag & PCHAN_HAS_CONST) {
    constraint_color = theme.colors.bone_pose_constraint;
  }
  interp_v4_v4v4(r_color, solid_color, constraint_color, 0.5f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing Color Helpers
 * \{ */

static void bone_locked_color_shade(const UniformData &theme, float color[4])
{
  const float *locked_color = theme.colors.bone_locked;

  interp_v3_v3v3(color, color, locked_color, locked_color[3]);
}

static const float *get_bone_solid_color(const Armatures::DrawContext *ctx,
                                         const eBone_Flag boneflag)
{
  const UniformData &theme = ctx->res->theme;
  if (ctx->const_color) {
    return theme.colors.bone_solid;
  }

  static float disp_color[4];
  get_pchan_color_solid(theme, ctx->bcolor, disp_color);

  if (ctx->draw_mode == ARM_DRAW_MODE_POSE && (boneflag & BONE_DRAW_LOCKED_WEIGHT)) {
    bone_locked_color_shade(theme, disp_color);
  }

  return disp_color;
}

static const float *get_bone_solid_with_consts_color(const Armatures::DrawContext *ctx,
                                                     const UnifiedBonePtr bone,
                                                     const eBone_Flag boneflag)
{
  const UniformData &theme = ctx->res->theme;
  if (ctx->const_color) {
    return theme.colors.bone_solid;
  }

  const float *col = get_bone_solid_color(ctx, boneflag);

  if (ctx->draw_mode != ARM_DRAW_MODE_POSE || (boneflag & BONE_DRAW_LOCKED_WEIGHT)) {
    return col;
  }

  static float consts_color[4];
  get_pchan_color_constraint(theme, ctx->bcolor, bone, consts_color);
  return consts_color;
}

static float get_bone_wire_thickness(const Armatures::DrawContext *ctx, int boneflag)
{
  if (ctx->const_color) {
    return ctx->const_wire;
  }
  if (boneflag & (BONE_DRAW_ACTIVE | BONE_SELECTED)) {
    return 2.0f;
  }

  return 1.0f;
}

static const float *get_bone_wire_color(const Armatures::DrawContext *ctx,
                                        const eBone_Flag boneflag)
{
  static float disp_color[4];

  if (ctx->const_color) {
    copy_v3_v3(disp_color, ctx->const_color);
  }
  else {
    const UniformData &theme = ctx->res->theme;
    switch (ctx->draw_mode) {
      case ARM_DRAW_MODE_EDIT:
        get_pchan_color_wire(theme, ctx->bcolor, ctx->draw_mode, boneflag, disp_color);
        break;
      case ARM_DRAW_MODE_POSE:
        get_pchan_color_wire(theme, ctx->bcolor, ctx->draw_mode, boneflag, disp_color);

        if (boneflag & BONE_DRAW_LOCKED_WEIGHT) {
          bone_locked_color_shade(theme, disp_color);
        }
        break;
      case ARM_DRAW_MODE_OBJECT:
        copy_v3_v3(disp_color, theme.colors.vert);
        break;
    }
  }

  disp_color[3] = get_bone_wire_thickness(ctx, boneflag);

  return disp_color;
}

static void bone_hint_color_shade(float hint_color[4], const float color[4])
{
  /* Increase contrast. */
  mul_v3_v3v3(hint_color, color, color);
  /* Decrease value to add mode shading to the shape. */
  mul_v3_fl(hint_color, 0.1f);
  hint_color[3] = 1.0f;
}

static const float *get_bone_hint_color(const Armatures::DrawContext *ctx,
                                        const eBone_Flag boneflag)
{
  static float hint_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  if (ctx->const_color) {
    bone_hint_color_shade(hint_color, ctx->res->theme.colors.bone_solid);
  }
  else {
    const float *wire_color = get_bone_wire_color(ctx, boneflag);
    bone_hint_color_shade(hint_color, wire_color);
  }

  return hint_color;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Helper Utils
 * \{ */

static void pchan_draw_data_init(bPoseChannel *pchan)
{
  if (pchan->draw_data != nullptr) {
    if (pchan->draw_data->bbone_matrix_len != pchan->bone->segments) {
      MEM_SAFE_FREE(pchan->draw_data);
    }
  }

  if (pchan->draw_data == nullptr) {
    pchan->draw_data = static_cast<bPoseChannelDrawData *>(
        MEM_mallocN(sizeof(*pchan->draw_data) + sizeof(Mat4) * pchan->bone->segments, __func__));
    pchan->draw_data->bbone_matrix_len = pchan->bone->segments;
  }
}

static void draw_bone_update_disp_matrix_default(UnifiedBonePtr bone)
{
  float ebmat[4][4];
  float bone_scale[3];
  float (*bone_mat)[4];
  float (*disp_mat)[4] = bone.disp_mat();
  float (*disp_tail_mat)[4] = bone.disp_tail_mat();

  /* TODO: This should be moved to depsgraph or armature refresh
   * and not be tied to the draw pass creation.
   * This would refresh armature without invalidating the draw cache */
  if (bone.is_posebone()) {
    bPoseChannel *pchan = bone.as_posebone();
    bone_mat = pchan->pose_mat;
    copy_v3_fl(bone_scale, pchan->bone->length);
  }
  else {
    EditBone *eBone = bone.as_editbone();
    eBone->length = len_v3v3(eBone->tail, eBone->head);
    ED_armature_ebone_to_mat4(eBone, ebmat);

    copy_v3_fl(bone_scale, eBone->length);
    bone_mat = ebmat;
  }

  copy_m4_m4(disp_mat, bone_mat);
  rescale_m4(disp_mat, bone_scale);
  copy_m4_m4(disp_tail_mat, disp_mat);
  translate_m4(disp_tail_mat, 0.0f, 1.0f, 0.0f);
}

static void draw_bone_update_disp_matrix_custom_shape(UnifiedBonePtr bone)
{
  float bone_scale[3];
  float (*bone_mat)[4];
  float (*disp_mat)[4];
  float (*disp_tail_mat)[4];
  float rot_mat[3][3];

  /* Custom bone shapes are only supported in pose mode for now. */
  bPoseChannel *pchan = bone.as_posebone();

  /* TODO: This should be moved to depsgraph or armature refresh
   * and not be tied to the draw pass creation.
   * This would refresh armature without invalidating the draw cache. */
  mul_v3_v3fl(bone_scale, pchan->custom_scale_xyz, PCHAN_CUSTOM_BONE_LENGTH(pchan));
  bone_mat = pchan->custom_tx ? pchan->custom_tx->pose_mat : pchan->pose_mat;
  disp_mat = bone.disp_mat();
  disp_tail_mat = pchan->disp_tail_mat;

  eulO_to_mat3(rot_mat, pchan->custom_rotation_euler, ROT_MODE_XYZ);

  copy_m4_m4(disp_mat, bone_mat);
  translate_m4(disp_mat,
               pchan->custom_translation[0],
               pchan->custom_translation[1],
               pchan->custom_translation[2]);
  mul_m4_m4m3(disp_mat, disp_mat, rot_mat);
  rescale_m4(disp_mat, bone_scale);
  copy_m4_m4(disp_tail_mat, disp_mat);
  translate_m4(disp_tail_mat, 0.0f, 1.0f, 0.0f);
}

/* compute connected child pointer for B-Bone drawing */
static void edbo_compute_bbone_child(bArmature *arm)
{
  LISTBASE_FOREACH (EditBone *, eBone, arm->edbo) {
    eBone->bbone_child = nullptr;
  }

  LISTBASE_FOREACH (EditBone *, eBone, arm->edbo) {
    if (eBone->parent && (eBone->flag & BONE_CONNECTED)) {
      eBone->parent->bbone_child = eBone;
    }
  }
}

/* A version of BKE_pchan_bbone_spline_setup() for previewing editmode curve settings. */
static void ebone_spline_preview(EditBone *ebone, const float result_array[MAX_BBONE_SUBDIV][4][4])
{
  BBoneSplineParameters param;
  EditBone *prev, *next;
  float imat[4][4], bonemat[4][4];
  float tmp[3];

  memset(&param, 0, sizeof(param));

  param.segments = ebone->segments;
  param.length = ebone->length;

  /* Get "next" and "prev" bones - these are used for handle calculations. */
  if (ebone->bbone_prev_type == BBONE_HANDLE_AUTO) {
    /* Use connected parent. */
    if (ebone->flag & BONE_CONNECTED) {
      prev = ebone->parent;
    }
    else {
      prev = nullptr;
    }
  }
  else {
    prev = ebone->bbone_prev;
  }

  if (ebone->bbone_next_type == BBONE_HANDLE_AUTO) {
    /* Use connected child. */
    next = ebone->bbone_child;
  }
  else {
    next = ebone->bbone_next;
  }

  /* compute handles from connected bones */
  if (prev || next) {
    ED_armature_ebone_to_mat4(ebone, imat);
    invert_m4(imat);

    if (prev) {
      param.use_prev = true;

      if (ebone->bbone_prev_type == BBONE_HANDLE_RELATIVE) {
        zero_v3(param.prev_h);
      }
      else if (ebone->bbone_prev_type == BBONE_HANDLE_TANGENT) {
        sub_v3_v3v3(tmp, prev->tail, prev->head);
        sub_v3_v3v3(tmp, ebone->head, tmp);
        mul_v3_m4v3(param.prev_h, imat, tmp);
      }
      else {
        param.prev_bbone = (prev->segments > 1);

        mul_v3_m4v3(param.prev_h, imat, prev->head);
      }

      if (!param.prev_bbone) {
        ED_armature_ebone_to_mat4(prev, bonemat);
        mul_m4_m4m4(param.prev_mat, imat, bonemat);
      }
    }

    if (next) {
      param.use_next = true;

      if (ebone->bbone_next_type == BBONE_HANDLE_RELATIVE) {
        copy_v3_fl3(param.next_h, 0.0f, param.length, 0.0);
      }
      else if (ebone->bbone_next_type == BBONE_HANDLE_TANGENT) {
        sub_v3_v3v3(tmp, next->tail, next->head);
        add_v3_v3v3(tmp, ebone->tail, tmp);
        mul_v3_m4v3(param.next_h, imat, tmp);
      }
      else {
        param.next_bbone = (next->segments > 1);

        mul_v3_m4v3(param.next_h, imat, next->tail);
      }

      ED_armature_ebone_to_mat4(next, bonemat);
      mul_m4_m4m4(param.next_mat, imat, bonemat);
    }
  }

  param.ease1 = ebone->ease1;
  param.ease2 = ebone->ease2;
  param.roll1 = ebone->roll1;
  param.roll2 = ebone->roll2;

  if (prev && (ebone->bbone_flag & BBONE_ADD_PARENT_END_ROLL)) {
    param.roll1 += prev->roll2;
  }

  copy_v3_v3(param.scale_in, ebone->scale_in);
  copy_v3_v3(param.scale_out, ebone->scale_out);

  param.curve_in_x = ebone->curve_in_x;
  param.curve_in_z = ebone->curve_in_z;

  param.curve_out_x = ebone->curve_out_x;
  param.curve_out_z = ebone->curve_out_z;

  if (ebone->bbone_flag & BBONE_SCALE_EASING) {
    param.ease1 *= param.scale_in[1];
    param.curve_in_x *= param.scale_in[1];
    param.curve_in_z *= param.scale_in[1];

    param.ease2 *= param.scale_out[1];
    param.curve_out_x *= param.scale_out[1];
    param.curve_out_z *= param.scale_out[1];
  }

  ebone->segments = BKE_pchan_bbone_spline_compute(&param, false, (Mat4 *)result_array);
}

/* This function is used for both B-Bone and Wire matrix updates. */
static void draw_bone_update_disp_matrix_bbone(UnifiedBonePtr bone)
{
  float s[4][4], ebmat[4][4];
  float length, xwidth, zwidth;
  float (*bone_mat)[4];
  short bbone_segments;

  /* TODO: This should be moved to depsgraph or armature refresh
   * and not be tied to the draw pass creation.
   * This would refresh armature without invalidating the draw cache. */
  if (bone.is_posebone()) {
    bPoseChannel *pchan = bone.as_posebone();
    length = pchan->bone->length;
    xwidth = pchan->bone->xwidth;
    zwidth = pchan->bone->zwidth;
    bone_mat = pchan->pose_mat;
    bbone_segments = pchan->bone->segments;
  }
  else {
    EditBone *eBone = bone.as_editbone();
    eBone->length = len_v3v3(eBone->tail, eBone->head);
    ED_armature_ebone_to_mat4(eBone, ebmat);

    length = eBone->length;
    xwidth = eBone->xwidth;
    zwidth = eBone->zwidth;
    bone_mat = ebmat;
    bbone_segments = eBone->segments;
  }

  const float3 size_vec = {xwidth, length / bbone_segments, zwidth};
  size_to_mat4(s, size_vec);

  /* Compute BBones segment matrices... */
  /* Note that we need this even for one-segment bones, because box drawing need specific weirdo
   * matrix for the box, that we cannot use to draw end points & co. */
  if (bone.is_posebone()) {
    bPoseChannel *pchan = bone.as_posebone();
    Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
    if (bbone_segments > 1) {
      BKE_pchan_bbone_spline_setup(pchan, false, false, bbones_mat);

      for (int i = bbone_segments; i--; bbones_mat++) {
        mul_m4_m4m4(bbones_mat->mat, bbones_mat->mat, s);
        mul_m4_m4m4(bbones_mat->mat, bone_mat, bbones_mat->mat);
      }
    }
    else {
      mul_m4_m4m4(bbones_mat->mat, bone_mat, s);
    }
  }
  else {
    EditBone *eBone = bone.as_editbone();
    float (*bbones_mat)[4][4] = eBone->disp_bbone_mat;

    if (bbone_segments > 1) {
      ebone_spline_preview(eBone, bbones_mat);

      for (int i = bbone_segments; i--; bbones_mat++) {
        mul_m4_m4m4(*bbones_mat, *bbones_mat, s);
        mul_m4_m4m4(*bbones_mat, bone_mat, *bbones_mat);
      }
    }
    else {
      mul_m4_m4m4(*bbones_mat, bone_mat, s);
    }
  }

  /* Grrr... We need default display matrix to draw end points, axes, etc. :( */
  draw_bone_update_disp_matrix_default(bone);
}

static void draw_axes(const Armatures::DrawContext *ctx,
                      const UnifiedBonePtr bone,
                      const bArmature &arm)
{
  float final_col[4];
  const float *col = (ctx->const_color)            ? ctx->const_color :
                     (bone.flag() & BONE_SELECTED) ? &ctx->res->theme.colors.text_hi.x :
                                                     &ctx->res->theme.colors.text.x;
  copy_v4_v4(final_col, col);
  /* Mix with axes color. */
  final_col[3] = (ctx->const_color) ? 1.0 : (bone.flag() & BONE_SELECTED) ? 0.1 : 0.65;

  if (bone.is_posebone() && bone.as_posebone()->custom && !(arm.flag & ARM_NO_CUSTOM)) {
    const bPoseChannel *pchan = bone.as_posebone();
    /* Special case: Custom bones can have different scale than the bone.
     * Recompute display matrix without the custom scaling applied. (#65640). */
    float axis_mat[4][4];
    float length = pchan->bone->length;
    copy_m4_m4(axis_mat, pchan->custom_tx ? pchan->custom_tx->pose_mat : pchan->pose_mat);
    const float3 length_vec = {length, length, length};
    rescale_m4(axis_mat, length_vec);
    translate_m4(axis_mat, 0.0, arm.axes_position - 1.0, 0.0);

    drw_shgroup_bone_axes(ctx, axis_mat, final_col);
  }
  else {
    float disp_mat[4][4];
    copy_m4_m4(disp_mat, bone.disp_mat());
    translate_m4(disp_mat, 0.0, arm.axes_position - 1.0, 0.0);
    drw_shgroup_bone_axes(ctx, disp_mat, final_col);
  }
}

static void draw_points(const Armatures::DrawContext *ctx,
                        const UnifiedBonePtr bone,
                        const eBone_Flag boneflag,
                        const float col_solid[4],
                        const int select_id)
{
  float col_wire_root[4], col_wire_tail[4];
  float col_hint_root[4], col_hint_tail[4];

  const UniformData &theme = ctx->res->theme;

  copy_v4_v4(col_wire_root, (ctx->const_color) ? ctx->const_color : &theme.colors.vert.x);
  copy_v4_v4(col_wire_tail, (ctx->const_color) ? ctx->const_color : &theme.colors.vert.x);

  const bool is_envelope_draw = (ctx->drawtype == ARM_DRAW_TYPE_ENVELOPE);
  const float envelope_ignore = -1.0f;

  col_wire_tail[3] = col_wire_root[3] = get_bone_wire_thickness(ctx, boneflag);

  /* Edit bone points can be selected */
  if (ctx->draw_mode == ARM_DRAW_MODE_EDIT) {
    const EditBone *eBone = bone.as_editbone();
    if (eBone->flag & BONE_ROOTSEL) {
      copy_v3_v3(col_wire_root, theme.colors.vert_select);
    }
    if (eBone->flag & BONE_TIPSEL) {
      copy_v3_v3(col_wire_tail, theme.colors.vert_select);
    }
  }
  else if (ctx->draw_mode == ARM_DRAW_MODE_POSE) {
    const float *wire_color = get_bone_wire_color(ctx, boneflag);
    copy_v4_v4(col_wire_tail, wire_color);
    copy_v4_v4(col_wire_root, wire_color);
  }

  const float *hint_color_shade_root = (ctx->const_color) ?
                                           (const float *)theme.colors.bone_solid :
                                           col_wire_root;
  const float *hint_color_shade_tail = (ctx->const_color) ?
                                           (const float *)theme.colors.bone_solid :
                                           col_wire_tail;
  bone_hint_color_shade(col_hint_root, hint_color_shade_root);
  bone_hint_color_shade(col_hint_tail, hint_color_shade_tail);

  /* Draw root point if we are not connected to our parent */

  if (!(bone.has_parent() && (boneflag & BONE_CONNECTED))) {
    if (is_envelope_draw) {
      drw_shgroup_bone_envelope(ctx,
                                bone.disp_mat(),
                                col_solid,
                                col_hint_root,
                                col_wire_root,
                                &bone.rad_head(),
                                &envelope_ignore,
                                select_id | BONESEL_ROOT);
    }
    else {
      drw_shgroup_bone_sphere(
          ctx, bone.disp_mat(), col_solid, col_hint_root, col_wire_root, select_id | BONESEL_ROOT);
    }
  }

  /* Draw tip point. */
  if (is_envelope_draw) {
    drw_shgroup_bone_envelope(ctx,
                              bone.disp_mat(),
                              col_solid,
                              col_hint_tail,
                              col_wire_tail,
                              &envelope_ignore,
                              &bone.rad_tail(),
                              select_id | BONESEL_TIP);
  }
  else {
    drw_shgroup_bone_sphere(ctx,
                            bone.disp_tail_mat(),
                            col_solid,
                            col_hint_tail,
                            col_wire_tail,
                            select_id | BONESEL_TIP);
  }
}

static void bone_draw_custom_shape(const Armatures::DrawContext *ctx,
                                   const UnifiedBonePtr bone,
                                   const eBone_Flag boneflag,
                                   const int select_id)
{
  const float *col_solid = get_bone_solid_color(ctx, boneflag);
  const float *col_wire = get_bone_wire_color(ctx, boneflag);
  const float *col_hint = get_bone_hint_color(ctx, boneflag);
  const float (*disp_mat)[4] = bone.disp_mat();

  auto sel_id = ctx->res->select_id(*ctx->ob_ref, select_id | BONESEL_BONE);

  /* Custom bone shapes are only supported in pose mode for now. */
  const bPoseChannel *pchan = bone.as_posebone();
  Object *custom_shape_ob = pchan->custom;

  if (custom_shape_ob->type == OB_EMPTY) {
    if (custom_shape_ob->empty_drawtype != OB_EMPTY_IMAGE) {
      drw_shgroup_bone_custom_empty(
          ctx, disp_mat, col_wire, pchan->custom_shape_wire_width, sel_id, pchan->custom);
    }
  }
  else if (boneflag & (BONE_DRAWWIRE | BONE_DRAW_LOCKED_WEIGHT)) {
    drw_shgroup_bone_custom_wire(
        ctx, disp_mat, col_wire, pchan->custom_shape_wire_width, sel_id, pchan->custom);
  }
  else {
    drw_shgroup_bone_custom_solid(ctx,
                                  disp_mat,
                                  col_solid,
                                  col_hint,
                                  col_wire,
                                  pchan->custom_shape_wire_width,
                                  sel_id,
                                  pchan->custom);
  }
}

static void bone_draw_octa(const Armatures::DrawContext *ctx,
                           const UnifiedBonePtr bone,
                           const eBone_Flag boneflag,
                           const int select_id)
{
  const float *col_solid = get_bone_solid_with_consts_color(ctx, bone, boneflag);
  const float *col_wire = get_bone_wire_color(ctx, boneflag);
  const float *col_hint = get_bone_hint_color(ctx, boneflag);

  auto sel_id = ctx->res->select_id(*ctx->ob_ref, select_id | BONESEL_BONE);
  float4x4 bone_mat = ctx->ob->object_to_world() * float4x4(bone.disp_mat());

  if (ctx->is_filled) {
    ctx->bone_buf->octahedral_fill_buf.append({bone_mat, col_solid, col_hint}, sel_id);
  }
  if (col_wire[3] > 0.0f) {
    ctx->bone_buf->octahedral_outline_buf.append({bone_mat, col_wire}, sel_id);
  }

  draw_points(ctx, bone, boneflag, col_solid, select_id);
}

static void bone_draw_line(const Armatures::DrawContext *ctx,
                           const UnifiedBonePtr bone,
                           const eBone_Flag boneflag,
                           const int select_id)
{
  const float *col_bone = get_bone_solid_with_consts_color(ctx, bone, boneflag);
  const float *col_wire = get_bone_wire_color(ctx, boneflag);
  const float no_display[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float *col_head = no_display;
  const float *col_tail = col_bone;

  if (ctx->const_color != nullptr) {
    col_wire = no_display; /* actually shrink the display. */
    col_bone = col_head = col_tail = ctx->const_color;
  }
  else {
    const UniformData &theme = ctx->res->theme;

    if (bone.is_editbone() && bone.flag() & BONE_TIPSEL) {
      col_tail = &theme.colors.vert_select.x;
    }

    /* Draw root point if we are not connected to our parent. */
    if (!(bone.has_parent() && (boneflag & BONE_CONNECTED))) {

      if (bone.is_editbone()) {
        col_head = (bone.flag() & BONE_ROOTSEL) ? &theme.colors.vert_select.x : col_bone;
      }
      else {
        col_head = col_bone;
      }
    }
  }

  if (select_id == -1) {
    /* Not in bone selection mode (can still be object select mode), draw everything at once.
     */
    drw_shgroup_bone_stick(
        ctx, bone.disp_mat(), col_wire, col_bone, col_head, col_tail, select_id);
  }
  else {
    /* In selection mode, draw bone, root and tip separately. */
    drw_shgroup_bone_stick(ctx,
                           bone.disp_mat(),
                           col_wire,
                           col_bone,
                           no_display,
                           no_display,
                           select_id | BONESEL_BONE);

    if (col_head[3] > 0.0f) {
      drw_shgroup_bone_stick(ctx,
                             bone.disp_mat(),
                             col_wire,
                             no_display,
                             col_head,
                             no_display,
                             select_id | BONESEL_ROOT);
    }

    drw_shgroup_bone_stick(
        ctx, bone.disp_mat(), col_wire, no_display, no_display, col_tail, select_id | BONESEL_TIP);
  }
}

static void bone_draw_b_bone(const Armatures::DrawContext *ctx,
                             const UnifiedBonePtr bone,
                             const eBone_Flag boneflag,
                             const int select_id)
{
  const float *col_solid = get_bone_solid_with_consts_color(ctx, bone, boneflag);
  const float *col_wire = get_bone_wire_color(ctx, boneflag);
  const float *col_hint = get_bone_hint_color(ctx, boneflag);

  /* NOTE: Cannot reinterpret as float4x4 because of alignment requirement of float4x4.
   * This would require a deeper refactor. */
  Span<Mat4> bbone_matrices;
  if (bone.is_posebone()) {
    bbone_matrices = {(Mat4 *)bone.as_posebone()->draw_data->bbone_matrix,
                      bone.as_posebone()->bone->segments};
  }
  else {
    bbone_matrices = {(Mat4 *)bone.as_editbone()->disp_bbone_mat, bone.as_editbone()->segments};
  }

  auto sel_id = ctx->res->select_id(*ctx->ob_ref, select_id | BONESEL_BONE);

  for (const Mat4 &in_bone_mat : bbone_matrices) {
    float4x4 bone_mat = ctx->ob->object_to_world() * float4x4(in_bone_mat.mat);

    if (ctx->is_filled) {
      ctx->bone_buf->bbones_fill_buf.append({bone_mat, col_solid, col_hint}, sel_id);
    }
    if (col_wire[3] > 0.0f) {
      ctx->bone_buf->bbones_outline_buf.append({bone_mat, col_wire}, sel_id);
    }
  }

  if (ctx->draw_mode == ARM_DRAW_MODE_EDIT) {
    draw_points(ctx, bone, boneflag, col_solid, select_id);
  }
}

static void bone_draw_envelope(const Armatures::DrawContext *ctx,
                               const UnifiedBonePtr bone,
                               const eBone_Flag boneflag,
                               const int select_id)
{
  const float *col_solid = get_bone_solid_with_consts_color(ctx, bone, boneflag);
  const float *col_wire = get_bone_wire_color(ctx, boneflag);
  const float *col_hint = get_bone_hint_color(ctx, boneflag);

  const float *rad_head, *rad_tail, *distance;
  if (bone.is_editbone()) {
    const EditBone *eBone = bone.as_editbone();
    rad_tail = &eBone->rad_tail;
    distance = &eBone->dist;
    rad_head = (eBone->parent && (boneflag & BONE_CONNECTED)) ? &eBone->parent->rad_tail :
                                                                &eBone->rad_head;
  }
  else {
    const bPoseChannel *pchan = bone.as_posebone();
    rad_tail = &pchan->bone->rad_tail;
    distance = &pchan->bone->dist;
    rad_head = (pchan->parent && (boneflag & BONE_CONNECTED)) ? &pchan->parent->bone->rad_tail :
                                                                &pchan->bone->rad_head;
  }

  if ((select_id == -1) && (boneflag & BONE_NO_DEFORM) == 0 &&
      ((boneflag & BONE_SELECTED) ||
       (bone.is_editbone() && (boneflag & (BONE_ROOTSEL | BONE_TIPSEL)))))
  {
    drw_shgroup_bone_envelope_distance(ctx, bone.disp_mat(), rad_head, rad_tail, distance);
  }

  drw_shgroup_bone_envelope(ctx,
                            bone.disp_mat(),
                            col_solid,
                            col_hint,
                            col_wire,
                            rad_head,
                            rad_tail,
                            select_id | BONESEL_BONE);

  draw_points(ctx, bone, boneflag, col_solid, select_id);
}

static void bone_draw_wire(const Armatures::DrawContext *ctx,
                           const UnifiedBonePtr bone,
                           const eBone_Flag boneflag,
                           const int select_id)
{
  using namespace blender::math;

  const float *col_wire = get_bone_wire_color(ctx, boneflag);

  auto sel_id = (ctx->bone_buf) ? ctx->res->select_id(*ctx->ob_ref, select_id | BONESEL_BONE) :
                                  draw::select::SelectMap::select_invalid_id();

  /* NOTE: Cannot reinterpret as float4x4 because of alignment requirement of float4x4.
   * This would require a deeper refactor. */
  Span<Mat4> bbone_matrices;
  if (bone.is_posebone()) {
    bbone_matrices = {(Mat4 *)bone.as_posebone()->draw_data->bbone_matrix,
                      bone.as_posebone()->bone->segments};
  }
  else {
    bbone_matrices = {(Mat4 *)bone.as_editbone()->disp_bbone_mat, bone.as_editbone()->segments};
  }

  for (const Mat4 &in_bone_mat : bbone_matrices) {
    float4x4 bmat = float4x4(in_bone_mat.mat);
    float3 head = transform_point(ctx->ob->object_to_world(), bmat.location());
    float3 tail = transform_point(ctx->ob->object_to_world(), bmat.location() + bmat.y_axis());

    ctx->bone_buf->wire_buf.append(head, tail, float4(col_wire), sel_id);
  }

  if (bone.is_editbone()) {
    const float *col_solid = get_bone_solid_with_consts_color(ctx, bone, boneflag);
    draw_points(ctx, bone, boneflag, col_solid, select_id);
  }
}

static void bone_draw(const eArmature_Drawtype drawtype,
                      const bool use_custom_shape,
                      const Armatures::DrawContext *ctx,
                      const UnifiedBonePtr bone,
                      const eBone_Flag boneflag,
                      const int select_id)
{
  if (use_custom_shape) {
    bone_draw_custom_shape(ctx, bone, boneflag, select_id);
    return;
  }

  switch (drawtype) {
    case ARM_DRAW_TYPE_OCTA:
      bone_draw_octa(ctx, bone, boneflag, select_id);
      break;
    case ARM_DRAW_TYPE_STICK:
      bone_draw_line(ctx, bone, boneflag, select_id);
      break;
    case ARM_DRAW_TYPE_B_BONE:
      bone_draw_b_bone(ctx, bone, boneflag, select_id);
      break;
    case ARM_DRAW_TYPE_ENVELOPE:
      bone_draw_envelope(ctx, bone, boneflag, select_id);
      break;
    case ARM_DRAW_TYPE_WIRE:
      bone_draw_wire(ctx, bone, boneflag, select_id);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Degrees of Freedom
 * \{ */

static void draw_bone_degrees_of_freedom(const Armatures::DrawContext *ctx,
                                         const bPoseChannel *pchan)
{
  draw::overlay::BoneInstanceData inst_data;
  float tmp[4][4], posetrans[4][4];
  float xminmax[2], zminmax[2];

  /* *0.5f here comes from M_PI/360.0f when rotations were still in degrees */
  xminmax[0] = sinf(pchan->limitmin[0] * 0.5f);
  xminmax[1] = sinf(pchan->limitmax[0] * 0.5f);
  zminmax[0] = sinf(pchan->limitmin[2] * 0.5f);
  zminmax[1] = sinf(pchan->limitmax[2] * 0.5f);

  unit_m4(posetrans);
  translate_m4(posetrans, pchan->pose_mat[3][0], pchan->pose_mat[3][1], pchan->pose_mat[3][2]);
  /* In parent-bone pose space... */
  if (pchan->parent) {
    copy_m4_m4(tmp, pchan->parent->pose_mat);
    zero_v3(tmp[3]);
    mul_m4_m4m4(posetrans, posetrans, tmp);
  }
  /* ... but its own rest-space. */
  mul_m4_m4m3(posetrans, posetrans, pchan->bone->bone_mat);

  float scale = pchan->bone->length * pchan->scale[1];
  scale_m4_fl(tmp, scale);
  tmp[1][1] = -tmp[1][1];
  mul_m4_m4m4(posetrans, posetrans, tmp);

  /* into world space. */
  inst_data.mat44 = ctx->ob->object_to_world() * float4x4(posetrans);

  /* Not selectable. */
  auto sel_id = draw::select::SelectMap::select_invalid_id();

  if ((pchan->ikflag & BONE_IK_XLIMIT) && (pchan->ikflag & BONE_IK_ZLIMIT)) {
    ExtraInstanceData data(
        inst_data.mat44, float4(0.25f), xminmax[0], zminmax[0], xminmax[1], zminmax[1]);

    ctx->bone_buf->degrees_of_freedom_fill_buf.append(data, sel_id);
    ctx->bone_buf->degrees_of_freedom_wire_buf.append(data.with_color({0.0f, 0.0f, 0.0f, 1.0f}),
                                                      sel_id);
  }
  if (pchan->ikflag & BONE_IK_XLIMIT) {
    ExtraInstanceData data(
        inst_data.mat44, float4(1.0f, 0.0f, 0.0f, 1.0f), xminmax[0], 0.0f, xminmax[1], 0.0f);
    ctx->bone_buf->degrees_of_freedom_wire_buf.append(data, sel_id);
  }
  if (pchan->ikflag & BONE_IK_ZLIMIT) {
    ExtraInstanceData data(
        inst_data.mat44, float4(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, zminmax[0], 0.0f, zminmax[1]);
    ctx->bone_buf->degrees_of_freedom_wire_buf.append(data, sel_id);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Relationships
 * \{ */

/** Should the relationship line between this bone and its parent be drawn? */
static bool should_draw_relation_to_parent(const UnifiedBonePtr bone, const eBone_Flag boneflag)
{
  const bool has_parent = bone.has_parent();

  if (bone.is_editbone() && has_parent) {
    /* Always draw for unconnected bones, regardless of selection,
     * since riggers will want to know about the links between bones
     */
    return (boneflag & BONE_CONNECTED) == 0;
  }

  if (bone.is_posebone() && has_parent) {
    /* Only draw between unconnected bones. */
    if (boneflag & BONE_CONNECTED) {
      return false;
    }

    /* Only draw if bone or its parent is selected - reduces viewport
     * complexity with complex rigs */
    const bPoseChannel *pchan = bone.as_posebone();
    return (pchan->flag & POSE_SELECTED) ||
           (pchan->parent && (pchan->parent->flag & POSE_SELECTED));
  }

  return false;
}

static void pchan_draw_ik_lines(const Armatures::DrawContext *ctx,
                                const bPoseChannel *pchan,
                                const bool only_temp)
{
  const bPoseChannel *parchan;
  const float *line_start = nullptr, *line_end = nullptr;
  const ePchan_ConstFlag constflag = ePchan_ConstFlag(pchan->constflag);

  LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
    if (con->enforce == 0.0f) {
      continue;
    }

    switch (con->type) {
      case CONSTRAINT_TYPE_KINEMATIC: {
        bKinematicConstraint *data = (bKinematicConstraint *)con->data;
        int segcount = 0;

        /* if only_temp, only draw if it is a temporary ik-chain */
        if (only_temp && !(data->flag & CONSTRAINT_IK_TEMP)) {
          continue;
        }

        /* exclude tip from chain? */
        parchan = ((data->flag & CONSTRAINT_IK_TIP) == 0) ? pchan->parent : pchan;
        line_start = parchan->pose_tail;

        /* Find the chain's root */
        while (parchan->parent) {
          segcount++;
          if (segcount == data->rootbone || segcount > 255) {
            break; /* 255 is weak */
          }
          parchan = parchan->parent;
        }

        if (parchan) {
          line_end = parchan->pose_head;

          if (constflag & PCHAN_HAS_NO_TARGET) {
            drw_shgroup_bone_ik_no_target_lines(ctx, line_start, line_end);
          }
          else {
            drw_shgroup_bone_ik_lines(ctx, line_start, line_end);
          }
        }
        break;
      }
      case CONSTRAINT_TYPE_SPLINEIK: {
        bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
        int segcount = 0;

        /* don't draw if only_temp, as Spline IK chains cannot be temporary */
        if (only_temp) {
          continue;
        }

        parchan = pchan;
        line_start = parchan->pose_tail;

        /* Find the chain's root */
        while (parchan->parent) {
          segcount++;
          /* FIXME: revise the breaking conditions */
          if (segcount == data->chainlen || segcount > 255) {
            break; /* 255 is weak */
          }
          parchan = parchan->parent;
        }
        /* Only draw line in case our chain is more than one bone long! */
        if (parchan != pchan) { /* XXX revise the breaking conditions to only stop at the tail? */
          line_end = parchan->pose_head;
          drw_shgroup_bone_ik_spline_lines(ctx, line_start, line_end);
        }
        break;
      }
    }
  }
}

static void draw_bone_bone_relationship_line(const Armatures::DrawContext *ctx,
                                             const float bone_head[3],
                                             const float parent_head[3],
                                             const float parent_tail[3])
{
  if (ctx->draw_relation_from_head) {
    drw_shgroup_bone_relationship_lines(ctx, bone_head, parent_head);
  }
  else {
    drw_shgroup_bone_relationship_lines(ctx, bone_head, parent_tail);
  }
}

static void draw_bone_relations(const Armatures::DrawContext *ctx,
                                const UnifiedBonePtr bone,
                                const eBone_Flag boneflag)
{
  if (ctx->draw_mode == ARM_DRAW_MODE_EDIT) {
    const EditBone *ebone = bone.as_editbone();
    if (ebone->parent) {
      if (ctx->do_relations && should_draw_relation_to_parent(bone, boneflag)) {
        draw_bone_bone_relationship_line(
            ctx, ebone->head, ebone->parent->head, ebone->parent->tail);
      }
    }
  }
  else {
    const bPoseChannel *pchan = bone.as_posebone();
    if (pchan->parent) {
      if (ctx->do_relations && should_draw_relation_to_parent(bone, boneflag)) {
        draw_bone_bone_relationship_line(
            ctx, pchan->pose_head, pchan->parent->pose_head, pchan->parent->pose_tail);
      }

      /* Draw a line to IK root bone if bone is selected. */
      if (ctx->draw_mode == ARM_DRAW_MODE_POSE) {
        if (pchan->constflag & (PCHAN_HAS_IK | PCHAN_HAS_SPLINEIK)) {
          if (pchan->flag & POSE_SELECTED) {
            pchan_draw_ik_lines(ctx, pchan, !ctx->do_relations);
          }
        }
      }
    }
  }
}

static void draw_bone_name(const Armatures::DrawContext *ctx, const UnifiedBonePtr bone)
{
  uchar color[4];
  float vec[3];

  const bool is_pose = bone.is_posebone();
  const EditBone *eBone = nullptr;
  const bPoseChannel *pchan = nullptr;
  bone.get(&eBone, &pchan);

  /* TODO: make this look at `boneflag` only. */
  bool highlight = (is_pose && ctx->draw_mode == ARM_DRAW_MODE_POSE &&
                    (pchan->flag & POSE_SELECTED)) ||
                   (!is_pose && (eBone->flag & BONE_SELECTED));

  /* Color Management: Exception here as texts are drawn in sRGB space directly. */
  UI_GetThemeColor4ubv(highlight ? TH_TEXT_HI : TH_TEXT, color);

  const float *head = is_pose ? pchan->pose_head : eBone->head;
  const float *tail = is_pose ? pchan->pose_tail : eBone->tail;
  mid_v3_v3v3(vec, head, tail);
  mul_m4_v3(ctx->ob->object_to_world().ptr(), vec);

  DRW_text_cache_add(ctx->dt,
                     vec,
                     is_pose ? pchan->name : eBone->name,
                     is_pose ? strlen(pchan->name) : strlen(eBone->name),
                     10,
                     0,
                     DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                     color,
                     true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Draw Loops
 * \{ */

static void bone_draw_update_display_matrix(const eArmature_Drawtype drawtype,
                                            const bool use_custom_shape,
                                            UnifiedBonePtr bone)
{
  if (use_custom_shape) {
    draw_bone_update_disp_matrix_custom_shape(bone);
  }
  else if (ELEM(drawtype, ARM_DRAW_TYPE_B_BONE, ARM_DRAW_TYPE_WIRE)) {
    draw_bone_update_disp_matrix_bbone(bone);
  }
  else {
    draw_bone_update_disp_matrix_default(bone);
  }
}

void Armatures::draw_armature_edit(Armatures::DrawContext *ctx)
{
  Object *ob = ctx->ob;
  EditBone *eBone;
  int index;
  const bool is_select = ctx->res->is_selection();
  const bool show_text = ctx->show_text;

  const Object *ob_orig = DEG_get_original(ob);
  /* FIXME(@ideasman42): We should be able to use the evaluated object,
   * however the active bone isn't updated. Long term solution is an 'EditArmature' struct.
   * for now we can draw from the original armature. See: #66773. */
  // bArmature *arm = ob->data;
  bArmature &arm = DRW_object_get_data_for_drawing<bArmature>(*ob_orig);

  edbo_compute_bbone_child(&arm);

  const eArmature_Drawtype arm_drawtype = eArmature_Drawtype(arm.drawtype);

  for (eBone = static_cast<EditBone *>(arm.edbo->first),
      /* Note: Selection Next handles the object id merging later. */
       index = ctx->bone_buf ? 0x0 : ob_orig->runtime->select_id;
       eBone;
       eBone = eBone->next, index += 0x10000)
  {
    if (!blender::animrig::bone_is_visible(&arm, eBone)) {
      continue;
    }

    const int select_id = is_select ? index : uint(-1);

    /* catch exception for bone with hidden parent */
    eBone_Flag boneflag = eBone_Flag(eBone->flag);
    if ((eBone->parent) && !blender::animrig::bone_is_visible(&arm, eBone->parent)) {
      boneflag &= ~BONE_CONNECTED;
    }

    /* set temporary flag for drawing bone as active, but only if selected */
    if (eBone == arm.act_edbone) {
      boneflag |= BONE_DRAW_ACTIVE;
    }

    boneflag &= ~BONE_DRAW_LOCKED_WEIGHT;

    UnifiedBonePtr bone = eBone;
    if (!ctx->const_color) {
      set_ctx_bcolor(ctx, bone);
    }

    if (!is_select) {
      draw_bone_relations(ctx, bone, boneflag);
    }

    const eArmature_Drawtype drawtype = eBone->drawtype == ARM_DRAW_TYPE_ARMATURE_DEFINED ?
                                            arm_drawtype :
                                            eArmature_Drawtype(eBone->drawtype);
    bone_draw_update_display_matrix(drawtype, false, bone);
    bone_draw(drawtype, false, ctx, bone, boneflag, select_id);

    if (!is_select) {
      if (show_text && (arm.flag & ARM_DRAWNAMES)) {
        draw_bone_name(ctx, bone);
      }

      if (arm.flag & ARM_DRAWAXES) {
        draw_axes(ctx, bone, arm);
      }
    }
  }
}

void Armatures::draw_armature_pose(Armatures::DrawContext *ctx)
{
  Object *ob = ctx->ob;
  const DRWContext *draw_ctx = DRW_context_get();
  const Scene *scene = draw_ctx->scene;
  bArmature &arm = DRW_object_get_data_for_drawing<bArmature>(*ob);
  int index = -1;
  const bool show_text = ctx->show_text;
  bool draw_locked_weights = false;

  /* We can't safely draw non-updated pose, might contain nullptr bone pointers... */
  if (ob->pose->flag & POSE_RECALC) {
    return;
  }

  ctx->draw_mode = ARM_DRAW_MODE_OBJECT; /* Will likely be set to ARM_DRAW_MODE_POSE below. */

  bool is_pose_select = false;
  /* Object can be edited in the scene. */
  if (!is_from_dupli_or_set(ob)) {
    if ((draw_ctx->object_mode & OB_MODE_POSE) || (ob == draw_ctx->object_pose)) {
      ctx->draw_mode = ARM_DRAW_MODE_POSE;
    }
    is_pose_select =
        /* If we're in pose-mode or object-mode with the ability to enter pose mode. */
        (
            /* Draw as if in pose mode (when selection is possible). */
            (ctx->draw_mode == ARM_DRAW_MODE_POSE) ||
            /* When we're in object mode, which may select bones. */
            ((ob->mode & OB_MODE_POSE) &&
             (
                 /* Switch from object mode when object lock is disabled. */
                 ((draw_ctx->object_mode == OB_MODE_OBJECT) &&
                  (scene->toolsettings->object_flag & SCE_OBJECT_MODE_LOCK) == 0) ||
                 /* Allow selection when in weight-paint mode
                  * (selection code ensures this won't become active). */
                 ((draw_ctx->object_mode & OB_MODE_ALL_WEIGHT_PAINT) &&
                  (draw_ctx->object_pose != nullptr))))) &&
        ctx->res->is_selection();

    if (is_pose_select) {
      const Object *ob_orig = DEG_get_original(ob);
      /* Note: Selection Next handles the object id merging later. */
      index = ctx->bone_buf ? 0x0 : ob_orig->runtime->select_id;
    }
  }

  /* In weight paint mode retrieve the vertex group lock status. */
  if ((draw_ctx->object_mode & OB_MODE_ALL_WEIGHT_PAINT) && (draw_ctx->object_pose == ob) &&
      (draw_ctx->obact != nullptr))
  {
    draw_locked_weights = true;

    for (bPoseChannel *pchan : ListBaseWrapper<bPoseChannel>(ob->pose->chanbase)) {
      pchan->bone->flag &= ~BONE_DRAW_LOCKED_WEIGHT;
    }

    const Object *obact_orig = DEG_get_original(draw_ctx->obact);

    const ListBase *defbase = BKE_object_defgroup_list(obact_orig);
    for (const bDeformGroup *dg : ConstListBaseWrapper<bDeformGroup>(defbase)) {
      if ((dg->flag & DG_LOCK_WEIGHT) == 0) {
        continue;
      }

      bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, dg->name);
      if (!pchan) {
        continue;
      }

      pchan->bone->flag |= BONE_DRAW_LOCKED_WEIGHT;
    }
  }

  const eArmature_Drawtype arm_drawtype = eArmature_Drawtype(arm.drawtype);

  for (bPoseChannel *pchan = static_cast<bPoseChannel *>(ob->pose->chanbase.first); pchan;
       pchan = pchan->next, index += 0x10000)
  {
    if (!blender::animrig::bone_is_visible(&arm, pchan)) {
      continue;
    }

    Bone *bone = pchan->bone;
    const bool draw_dofs = !is_pose_select && ctx->show_relations &&
                           (ctx->draw_mode == ARM_DRAW_MODE_POSE) &&
                           (pchan->flag & POSE_SELECTED) &&
                           ((ob->base_flag & BASE_FROM_DUPLI) == 0) &&
                           (pchan->ikflag & (BONE_IK_XLIMIT | BONE_IK_ZLIMIT));
    const int select_id = is_pose_select ? index : uint(-1);

    pchan_draw_data_init(pchan);

    UnifiedBonePtr bone_ptr = pchan;
    if (!ctx->const_color) {
      set_ctx_bcolor(ctx, bone_ptr);
    }

    eBone_Flag boneflag = bone_ptr.flag();
    if (pchan->parent && !blender::animrig::bone_is_visible(&arm, pchan->parent)) {
      /* Avoid drawing connection line to hidden parent. */
      boneflag &= ~BONE_CONNECTED;
    }
    if (bone == arm.act_bone) {
      /* Draw bone as active, but only if selected. */
      boneflag |= BONE_DRAW_ACTIVE;
    }
    if (!draw_locked_weights) {
      boneflag &= ~BONE_DRAW_LOCKED_WEIGHT;
    }

    const bool use_custom_shape = (pchan->custom) && !(arm.flag & ARM_NO_CUSTOM);
    if (!is_pose_select) {
      draw_bone_relations(ctx, bone_ptr, boneflag);
    }

    const eArmature_Drawtype drawtype = bone->drawtype == ARM_DRAW_TYPE_ARMATURE_DEFINED ?
                                            arm_drawtype :
                                            eArmature_Drawtype(bone->drawtype);
    bone_draw_update_display_matrix(drawtype, use_custom_shape, bone_ptr);
    bone_draw(drawtype, use_custom_shape, ctx, bone_ptr, boneflag, select_id);

    /* Below this point nothing is used for selection queries. */
    if (is_pose_select) {
      continue;
    }

    if (draw_dofs) {
      draw_bone_degrees_of_freedom(ctx, pchan);
    }
    if (show_text && (arm.flag & ARM_DRAWNAMES)) {
      draw_bone_name(ctx, bone_ptr);
    }
    if (arm.flag & ARM_DRAWAXES) {
      draw_axes(ctx, bone_ptr, arm);
    }
  }
}

/** \} */
