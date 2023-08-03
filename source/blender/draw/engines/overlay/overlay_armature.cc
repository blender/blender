/* SPDX-FileCopyrightText: 2019 Blender Foundation
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
#include "DNA_view3d_types.h"

#include "DRW_render.h"

#include "BLI_listbase_wrapper.hh"
#include "BLI_math.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_deform.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "ED_armature.h"
#include "ED_view3d.h"

#include "ANIM_bone_collections.h"

#include "UI_resources.h"

#include "draw_common.h"
#include "draw_manager_text.h"

#include "overlay_private.hh"

#include "draw_cache_impl.h"

#define PT_DEFAULT_RAD 0.05f /* radius of the point batch. */

using namespace blender;

enum eArmatureDrawMode {
  ARM_DRAW_MODE_OBJECT,
  ARM_DRAW_MODE_POSE,
  ARM_DRAW_MODE_EDIT,
};

struct ArmatureDrawContext {
  /* Current armature object */
  Object *ob;
  /* bArmature *arm; */ /* TODO */
  eArmatureDrawMode draw_mode;
  eArmature_Drawtype drawtype;

  union {
    struct {
      DRWCallBuffer *outline;
      DRWCallBuffer *solid;
      DRWCallBuffer *wire;
    };
    struct {
      DRWCallBuffer *envelope_outline;
      DRWCallBuffer *envelope_solid;
      DRWCallBuffer *envelope_distance;
    };
    struct {
      DRWCallBuffer *stick;
    };
  };

  DRWCallBuffer *dof_lines;
  DRWCallBuffer *dof_sphere;
  DRWCallBuffer *point_solid;
  DRWCallBuffer *point_outline;
  DRWShadingGroup *custom_solid;
  DRWShadingGroup *custom_outline;
  DRWShadingGroup *custom_wire;
  GHash *custom_shapes_ghash;

  OVERLAY_ExtraCallBuffers *extras;

  /* not a theme, this is an override */
  const float *const_color;
  float const_wire;

  bool do_relations;
  bool transparent;
  bool show_relations;
  bool draw_relation_from_head;

  const ThemeWireColor *bcolor; /* pchan color */
};

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
    return static_cast<eBone_Flag>(is_editbone_ ? eBone_->flag : pchan_->bone->flag);
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
};

/**
 * Bone drawing strategy class.
 *
 * Depending on the armature display mode, a different subclass is used to
 * manage drawing. These subclasses are defined further down in the file. This
 * abstract class needs to be defined before any function that uses it, though.
 */
class ArmatureBoneDrawStrategy {
 public:
  virtual void update_display_matrix(UnifiedBonePtr bone) const = 0;

  /**
   * Culling test.
   * \return true when a part of this bPoseChannel is visible in the viewport.
   */
  virtual bool culling_test(const DRWView *view,
                            const Object *ob,
                            const bPoseChannel *pchan) const = 0;

  virtual void draw_context_setup(ArmatureDrawContext *ctx,
                                  const OVERLAY_ArmatureCallBuffersInner *cb,
                                  const bool is_filled,
                                  const bool do_envelope_dist) const = 0;

  virtual void draw_bone(const ArmatureDrawContext *ctx,
                         const UnifiedBonePtr bone,
                         const eBone_Flag boneflag,
                         const int select_id) const = 0;

  /** Should the relationship line between this bone and its parent be drawn? */
  virtual bool should_draw_relation_to_parent(const UnifiedBonePtr bone,
                                              const eBone_Flag boneflag) const
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
      return (boneflag & BONE_SELECTED) ||
             (pchan->parent->bone && (pchan->parent->bone->flag & BONE_SELECTED));
    }

    return false;
  }
};

bool OVERLAY_armature_is_pose_mode(Object *ob, const DRWContextState *draw_ctx)
{
  Object *active_ob = draw_ctx->obact;

  /* Pose armature is handled by pose mode engine. */
  if (((ob == active_ob) || (ob->mode & OB_MODE_POSE)) &&
      ((draw_ctx->object_mode & OB_MODE_POSE) != 0))
  {
    return true;
  }

  /* Armature parent is also handled by pose mode engine. */
  if ((active_ob != nullptr) && (draw_ctx->object_mode & OB_MODE_ALL_WEIGHT_PAINT)) {
    if (ob == draw_ctx->object_pose) {
      return true;
    }
  }

  return false;
}

void OVERLAY_armature_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool is_select_mode = DRW_state_is_select();
  pd->armature.transparent = (draw_ctx->v3d->shading.type == OB_WIRE) ||
                             XRAY_FLAG_ENABLED(draw_ctx->v3d);
  pd->armature.show_relations = ((draw_ctx->v3d->flag & V3D_HIDE_HELPLINES) == 0) &&
                                !is_select_mode;
  pd->armature.do_pose_xray = (pd->overlay.flag & V3D_OVERLAY_BONE_SELECT) != 0;
  pd->armature.do_pose_fade_geom = pd->armature.do_pose_xray &&
                                   ((draw_ctx->object_mode & OB_MODE_WEIGHT_PAINT) == 0) &&
                                   draw_ctx->object_pose != nullptr;

  const float wire_alpha = pd->overlay.bone_wire_alpha;
  const bool use_wire_alpha = (wire_alpha < 1.0f);

  DRWState state;

  if (pd->armature.do_pose_fade_geom) {
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->armature_bone_select_ps, state | pd->clipping_state);

    float alpha = pd->overlay.xray_alpha_bone;
    GPUShader *sh = OVERLAY_shader_uniform_color();
    DRWShadingGroup *grp;

    pd->armature_bone_select_act_grp = grp = DRW_shgroup_create(sh, psl->armature_bone_select_ps);
    float4 color = {0.0f, 0.0f, 0.0f, alpha};
    DRW_shgroup_uniform_vec4_copy(grp, "ucolor", color);

    pd->armature_bone_select_grp = grp = DRW_shgroup_create(sh, psl->armature_bone_select_ps);
    color = {0.0f, 0.0f, 0.0f, powf(alpha, 4)};
    DRW_shgroup_uniform_vec4_copy(grp, "ucolor", color);
  }

  for (int i = 0; i < 2; i++) {
    GPUShader *sh;
    GPUVertFormat *format;
    DRWShadingGroup *grp = nullptr;

    OVERLAY_InstanceFormats *formats = OVERLAY_shader_instance_formats_get();
    OVERLAY_ArmatureCallBuffers *cb = &pd->armature_call_buffers[i];

    cb->solid.custom_shapes_ghash = BLI_ghash_ptr_new(__func__);
    cb->transp.custom_shapes_ghash = BLI_ghash_ptr_new(__func__);

    DRWPass **p_armature_ps = &psl->armature_ps[i];
    DRWState infront_state = (DRW_state_is_select() && (i == 1)) ? DRW_STATE_IN_FRONT_SELECT :
                                                                   DRWState(0);
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_WRITE_DEPTH;
    DRW_PASS_CREATE(*p_armature_ps, state | pd->clipping_state | infront_state);
    DRWPass *armature_ps = *p_armature_ps;

    DRWPass **p_armature_trans_ps = &psl->armature_transp_ps[i];
    state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ADD;
    DRW_PASS_CREATE(*p_armature_trans_ps, state | pd->clipping_state);
    DRWPass *armature_transp_ps = *p_armature_trans_ps;

#define BUF_INSTANCE DRW_shgroup_call_buffer_instance
#define BUF_LINE(grp, format) DRW_shgroup_call_buffer(grp, format, GPU_PRIM_LINES)
#define BUF_POINT(grp, format) DRW_shgroup_call_buffer(grp, format, GPU_PRIM_POINTS)

    {
      format = formats->instance_bone;

      sh = OVERLAY_shader_armature_sphere(false);
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.point_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_point_get());

      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_state_disable(grp, DRW_STATE_WRITE_DEPTH);
      DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
      DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha * 0.4f);
      cb->transp.point_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_point_get());

      sh = OVERLAY_shader_armature_shape(false);
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.custom_fill = grp;
      cb->solid.box_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_box_get());
      cb->solid.octa_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_octahedral_get());

      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_state_disable(grp, DRW_STATE_WRITE_DEPTH);
      DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
      DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha * 0.6f);
      cb->transp.custom_fill = grp;
      cb->transp.box_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_box_get());
      cb->transp.octa_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_octahedral_get());

      sh = OVERLAY_shader_armature_sphere(true);
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.point_outline = BUF_INSTANCE(grp, format, DRW_cache_bone_point_wire_outline_get());

      if (use_wire_alpha) {
        grp = DRW_shgroup_create(sh, armature_ps);
        DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        cb->transp.point_outline = BUF_INSTANCE(
            grp, format, DRW_cache_bone_point_wire_outline_get());
      }
      else {
        cb->transp.point_outline = cb->solid.point_outline;
      }

      sh = OVERLAY_shader_armature_shape(true);
      cb->solid.custom_outline = grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.box_outline = BUF_INSTANCE(grp, format, DRW_cache_bone_box_wire_get());
      cb->solid.octa_outline = BUF_INSTANCE(grp, format, DRW_cache_bone_octahedral_wire_get());

      if (use_wire_alpha) {
        cb->transp.custom_outline = grp = DRW_shgroup_create(sh, armature_ps);
        DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        cb->transp.box_outline = BUF_INSTANCE(grp, format, DRW_cache_bone_box_wire_get());
        cb->transp.octa_outline = BUF_INSTANCE(grp, format, DRW_cache_bone_octahedral_wire_get());
      }
      else {
        cb->transp.custom_outline = cb->solid.custom_outline;
        cb->transp.box_outline = cb->solid.box_outline;
        cb->transp.octa_outline = cb->solid.octa_outline;
      }

      sh = OVERLAY_shader_armature_shape_wire();
      cb->solid.custom_wire = grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);

      if (use_wire_alpha) {
        cb->transp.custom_wire = grp = DRW_shgroup_create(sh, armature_ps);
        DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
      }
      else {
        cb->transp.custom_wire = cb->solid.custom_wire;
      }
    }
    {
      format = formats->instance_extra;

      sh = OVERLAY_shader_armature_degrees_of_freedom_wire();
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.dof_lines = BUF_INSTANCE(grp, format, DRW_cache_bone_dof_lines_get());

      if (use_wire_alpha) {
        grp = DRW_shgroup_create(sh, armature_ps);
        DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        cb->transp.dof_lines = BUF_INSTANCE(grp, format, DRW_cache_bone_dof_lines_get());
      }
      else {
        cb->transp.dof_lines = cb->solid.dof_lines;
      }

      sh = OVERLAY_shader_armature_degrees_of_freedom_solid();
      grp = DRW_shgroup_create(sh, armature_transp_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.dof_sphere = BUF_INSTANCE(grp, format, DRW_cache_bone_dof_sphere_get());

      if (use_wire_alpha) {
        grp = DRW_shgroup_create(sh, armature_transp_ps);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        cb->transp.dof_sphere = BUF_INSTANCE(grp, format, DRW_cache_bone_dof_sphere_get());
      }
      else {
        cb->transp.dof_sphere = cb->solid.dof_sphere;
      }
    }
    {
      format = formats->instance_bone_stick;

      sh = OVERLAY_shader_armature_stick();
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.stick = BUF_INSTANCE(grp, format, DRW_cache_bone_stick_get());

      if (use_wire_alpha) {
        grp = DRW_shgroup_create(sh, armature_ps);
        DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        cb->transp.stick = BUF_INSTANCE(grp, format, DRW_cache_bone_stick_get());
      }
      else {
        cb->transp.stick = cb->solid.stick;
      }
    }
    {
      format = formats->instance_bone_envelope;

      sh = OVERLAY_shader_armature_envelope(false);
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_state_enable(grp, DRW_STATE_CULL_BACK);
      DRW_shgroup_uniform_bool_copy(grp, "isDistance", false);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.envelope_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_envelope_solid_get());

      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_state_disable(grp, DRW_STATE_WRITE_DEPTH);
      DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA | DRW_STATE_CULL_BACK);
      DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha * 0.6f);
      cb->transp.envelope_fill = BUF_INSTANCE(grp, format, DRW_cache_bone_envelope_solid_get());

      format = formats->instance_bone_envelope_outline;

      sh = OVERLAY_shader_armature_envelope(true);
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.envelope_outline = BUF_INSTANCE(
          grp, format, DRW_cache_bone_envelope_outline_get());

      if (use_wire_alpha) {
        grp = DRW_shgroup_create(sh, armature_ps);
        DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        cb->transp.envelope_outline = BUF_INSTANCE(
            grp, format, DRW_cache_bone_envelope_outline_get());
      }
      else {
        cb->transp.envelope_outline = cb->solid.envelope_outline;
      }

      format = formats->instance_bone_envelope_distance;

      sh = OVERLAY_shader_armature_envelope(false);
      grp = DRW_shgroup_create(sh, armature_transp_ps);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      DRW_shgroup_uniform_bool_copy(grp, "isDistance", true);
      DRW_shgroup_state_enable(grp, DRW_STATE_CULL_FRONT);
      cb->solid.envelope_distance = BUF_INSTANCE(grp, format, DRW_cache_bone_envelope_solid_get());

      if (use_wire_alpha) {
        grp = DRW_shgroup_create(sh, armature_transp_ps);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        DRW_shgroup_uniform_bool_copy(grp, "isDistance", true);
        DRW_shgroup_state_enable(grp, DRW_STATE_CULL_FRONT);
        cb->transp.envelope_distance = BUF_INSTANCE(
            grp, format, DRW_cache_bone_envelope_solid_get());
      }
      else {
        cb->transp.envelope_distance = cb->solid.envelope_distance;
      }
    }
    {
      format = formats->pos_color;

      sh = OVERLAY_shader_armature_wire();
      grp = DRW_shgroup_create(sh, armature_ps);
      DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
      DRW_shgroup_uniform_float_copy(grp, "alpha", 1.0f);
      cb->solid.wire = BUF_LINE(grp, format);

      if (use_wire_alpha) {
        grp = DRW_shgroup_create(sh, armature_ps);
        DRW_shgroup_state_enable(grp, DRW_STATE_BLEND_ALPHA);
        DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
        DRW_shgroup_uniform_float_copy(grp, "alpha", wire_alpha);
        cb->transp.wire = BUF_LINE(grp, format);
      }
      else {
        cb->transp.wire = cb->solid.wire;
      }
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Shader Groups (DRW_shgroup)
 * \{ */

static void bone_instance_data_set_angle_minmax(BoneInstanceData *data,
                                                const float aminx,
                                                const float aminz,
                                                const float amaxx,
                                                const float amaxz)
{
  data->amin_a = aminx;
  data->amin_b = aminz;
  data->amax_a = amaxx;
  data->amax_b = amaxz;
}

/* Encode 2 units float with byte precision into a float. */
static float encode_2f_to_float(float a, float b)
{
  CLAMP(a, 0.0f, 1.0f);
  CLAMP(b, 0.0f, 2.0f); /* Can go up to 2. Needed for wire size. */
  return float(int(a * 255) | (int(b * 255) << 8));
}

void OVERLAY_bone_instance_data_set_color_hint(BoneInstanceData *data, const float hint_color[4])
{
  /* Encoded color into 2 floats to be able to use the obmat to color the custom bones. */
  data->color_hint_a = encode_2f_to_float(hint_color[0], hint_color[1]);
  data->color_hint_b = encode_2f_to_float(hint_color[2], hint_color[3]);
}

void OVERLAY_bone_instance_data_set_color(BoneInstanceData *data, const float bone_color[4])
{
  /* Encoded color into 2 floats to be able to use the obmat to color the custom bones. */
  data->color_a = encode_2f_to_float(bone_color[0], bone_color[1]);
  data->color_b = encode_2f_to_float(bone_color[2], bone_color[3]);
}

/* Octahedral */
static void drw_shgroup_bone_octahedral(const ArmatureDrawContext *ctx,
                                        const float (*bone_mat)[4],
                                        const float bone_color[4],
                                        const float hint_color[4],
                                        const float outline_color[4])
{
  BoneInstanceData inst_data;
  mul_m4_m4m4(inst_data.mat, ctx->ob->object_to_world, bone_mat);
  if (ctx->solid) {
    OVERLAY_bone_instance_data_set_color(&inst_data, bone_color);
    OVERLAY_bone_instance_data_set_color_hint(&inst_data, hint_color);
    DRW_buffer_add_entry_struct(ctx->solid, &inst_data);
  }
  if (outline_color[3] > 0.0f) {
    OVERLAY_bone_instance_data_set_color(&inst_data, outline_color);
    DRW_buffer_add_entry_struct(ctx->outline, &inst_data);
  }
}

/* Box / B-Bone */
static void drw_shgroup_bone_box(const ArmatureDrawContext *ctx,
                                 const float (*bone_mat)[4],
                                 const float bone_color[4],
                                 const float hint_color[4],
                                 const float outline_color[4])
{
  BoneInstanceData inst_data;
  mul_m4_m4m4(inst_data.mat, ctx->ob->object_to_world, bone_mat);
  if (ctx->solid) {
    OVERLAY_bone_instance_data_set_color(&inst_data, bone_color);
    OVERLAY_bone_instance_data_set_color_hint(&inst_data, hint_color);
    DRW_buffer_add_entry_struct(ctx->solid, &inst_data);
  }
  if (outline_color[3] > 0.0f) {
    OVERLAY_bone_instance_data_set_color(&inst_data, outline_color);
    DRW_buffer_add_entry_struct(ctx->outline, &inst_data);
  }
}

/* Wire */
static void drw_shgroup_bone_wire(const ArmatureDrawContext *ctx,
                                  const float (*bone_mat)[4],
                                  const float color[4])
{
  float head[3], tail[3];
  mul_v3_m4v3(head, ctx->ob->object_to_world, bone_mat[3]);
  add_v3_v3v3(tail, bone_mat[3], bone_mat[1]);
  mul_m4_v3(ctx->ob->object_to_world, tail);

  DRW_buffer_add_entry(ctx->wire, head, color);
  DRW_buffer_add_entry(ctx->wire, tail, color);
}

/* Stick */
static void drw_shgroup_bone_stick(const ArmatureDrawContext *ctx,
                                   const float (*bone_mat)[4],
                                   const float col_wire[4],
                                   const float col_bone[4],
                                   const float col_head[4],
                                   const float col_tail[4])
{
  float head[3], tail[3];
  mul_v3_m4v3(head, ctx->ob->object_to_world, bone_mat[3]);
  add_v3_v3v3(tail, bone_mat[3], bone_mat[1]);
  mul_m4_v3(ctx->ob->object_to_world, tail);

  DRW_buffer_add_entry(ctx->stick, head, tail, col_wire, col_bone, col_head, col_tail);
}

/* Envelope */
static void drw_shgroup_bone_envelope_distance(const ArmatureDrawContext *ctx,
                                               const float (*bone_mat)[4],
                                               const float *radius_head,
                                               const float *radius_tail,
                                               const float *distance)
{
  if (ctx->envelope_distance) {
    float head_sph[4] = {0.0f, 0.0f, 0.0f, 1.0f}, tail_sph[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    float xaxis[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    /* Still less operation than m4 multiplication. */
    mul_m4_v4(bone_mat, head_sph);
    mul_m4_v4(bone_mat, tail_sph);
    mul_m4_v4(bone_mat, xaxis);
    mul_m4_v4(ctx->ob->object_to_world, head_sph);
    mul_m4_v4(ctx->ob->object_to_world, tail_sph);
    mul_m4_v4(ctx->ob->object_to_world, xaxis);
    sub_v3_v3(xaxis, head_sph);
    float obscale = mat4_to_scale(ctx->ob->object_to_world);
    head_sph[3] = *radius_head * obscale;
    head_sph[3] += *distance * obscale;
    tail_sph[3] = *radius_tail * obscale;
    tail_sph[3] += *distance * obscale;
    DRW_buffer_add_entry(ctx->envelope_distance, head_sph, tail_sph, xaxis);
  }
}

static void drw_shgroup_bone_envelope(const ArmatureDrawContext *ctx,
                                      const float (*bone_mat)[4],
                                      const float bone_col[4],
                                      const float hint_col[4],
                                      const float outline_col[4],
                                      const float *radius_head,
                                      const float *radius_tail)
{
  float head_sph[4] = {0.0f, 0.0f, 0.0f, 1.0f}, tail_sph[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  float xaxis[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  /* Still less operation than m4 multiplication. */
  mul_m4_v4(bone_mat, head_sph);
  mul_m4_v4(bone_mat, tail_sph);
  mul_m4_v4(bone_mat, xaxis);
  mul_m4_v4(ctx->ob->object_to_world, head_sph);
  mul_m4_v4(ctx->ob->object_to_world, tail_sph);
  mul_m4_v4(ctx->ob->object_to_world, xaxis);
  float obscale = mat4_to_scale(ctx->ob->object_to_world);
  head_sph[3] = *radius_head * obscale;
  tail_sph[3] = *radius_tail * obscale;

  if (head_sph[3] < 0.0f || tail_sph[3] < 0.0f) {
    BoneInstanceData inst_data;
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

    if (ctx->point_solid) {
      OVERLAY_bone_instance_data_set_color(&inst_data, bone_col);
      OVERLAY_bone_instance_data_set_color_hint(&inst_data, hint_col);
      DRW_buffer_add_entry_struct(ctx->point_solid, &inst_data);
    }
    if (outline_col[3] > 0.0f) {
      OVERLAY_bone_instance_data_set_color(&inst_data, outline_col);
      DRW_buffer_add_entry_struct(ctx->point_outline, &inst_data);
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
      if (ctx->envelope_solid) {
        DRW_buffer_add_entry(ctx->envelope_solid, head_sph, tail_sph, bone_col, hint_col, xaxis);
      }
      if (outline_col[3] > 0.0f) {
        DRW_buffer_add_entry(ctx->envelope_outline, head_sph, tail_sph, outline_col, xaxis);
      }
    }
    else {
      /* Distance between endpoints is too small for a capsule. Draw a Sphere instead. */
      float fac = max_ff(fac_head, 1.0f - fac_tail);
      interp_v4_v4v4(tmp_sph, tail_sph, head_sph, clamp_f(fac, 0.0f, 1.0f));

      BoneInstanceData inst_data;
      scale_m4_fl(inst_data.mat, tmp_sph[3] / PT_DEFAULT_RAD);
      copy_v3_v3(inst_data.mat[3], tmp_sph);
      if (ctx->point_solid) {
        OVERLAY_bone_instance_data_set_color(&inst_data, bone_col);
        OVERLAY_bone_instance_data_set_color_hint(&inst_data, hint_col);
        DRW_buffer_add_entry_struct(ctx->point_solid, &inst_data);
      }
      if (outline_col[3] > 0.0f) {
        OVERLAY_bone_instance_data_set_color(&inst_data, outline_col);
        DRW_buffer_add_entry_struct(ctx->point_outline, &inst_data);
      }
    }
  }
}

/* Custom (geometry) */

BLI_INLINE DRWCallBuffer *custom_bone_instance_shgroup(const ArmatureDrawContext *ctx,
                                                       DRWShadingGroup *grp,
                                                       GPUBatch *custom_geom)
{
  DRWCallBuffer *buf = static_cast<DRWCallBuffer *>(
      BLI_ghash_lookup(ctx->custom_shapes_ghash, custom_geom));
  if (buf == nullptr) {
    OVERLAY_InstanceFormats *formats = OVERLAY_shader_instance_formats_get();
    buf = DRW_shgroup_call_buffer_instance(grp, formats->instance_bone, custom_geom);
    BLI_ghash_insert(ctx->custom_shapes_ghash, custom_geom, buf);
  }
  return buf;
}

static void drw_shgroup_bone_custom_solid_mesh(const ArmatureDrawContext *ctx,
                                               Mesh *mesh,
                                               const float (*bone_mat)[4],
                                               const float bone_color[4],
                                               const float hint_color[4],
                                               const float outline_color[4],
                                               Object *custom)
{
  /* TODO(fclem): arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  DRW_mesh_batch_cache_validate(custom, mesh);

  GPUBatch *surf = DRW_mesh_batch_cache_get_surface(mesh);
  GPUBatch *edges = DRW_mesh_batch_cache_get_edge_detection(mesh, nullptr);
  GPUBatch *loose_edges = DRW_mesh_batch_cache_get_loose_edges(mesh);
  BoneInstanceData inst_data;
  DRWCallBuffer *buf;

  if (surf || edges || loose_edges) {
    mul_m4_m4m4(inst_data.mat, ctx->ob->object_to_world, bone_mat);
  }

  if (surf && ctx->custom_solid) {
    buf = custom_bone_instance_shgroup(ctx, ctx->custom_solid, surf);
    OVERLAY_bone_instance_data_set_color_hint(&inst_data, hint_color);
    OVERLAY_bone_instance_data_set_color(&inst_data, bone_color);
    DRW_buffer_add_entry_struct(buf, inst_data.mat);
  }

  if (edges && ctx->custom_outline) {
    buf = custom_bone_instance_shgroup(ctx, ctx->custom_outline, edges);
    OVERLAY_bone_instance_data_set_color(&inst_data, outline_color);
    DRW_buffer_add_entry_struct(buf, inst_data.mat);
  }

  if (loose_edges) {
    buf = custom_bone_instance_shgroup(ctx, ctx->custom_wire, loose_edges);
    OVERLAY_bone_instance_data_set_color_hint(&inst_data, outline_color);
    OVERLAY_bone_instance_data_set_color(&inst_data, outline_color);
    DRW_buffer_add_entry_struct(buf, inst_data.mat);
  }

  /* TODO(fclem): needs to be moved elsewhere. */
  drw_batch_cache_generate_requested_delayed(custom);
}

static void drw_shgroup_bone_custom_mesh_wire(const ArmatureDrawContext *ctx,
                                              Mesh *mesh,
                                              const float (*bone_mat)[4],
                                              const float color[4],
                                              Object *custom)
{
  /* TODO(fclem): arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  DRW_mesh_batch_cache_validate(custom, mesh);

  GPUBatch *geom = DRW_mesh_batch_cache_get_all_edges(mesh);
  if (geom) {
    DRWCallBuffer *buf = custom_bone_instance_shgroup(ctx, ctx->custom_wire, geom);
    BoneInstanceData inst_data;
    mul_m4_m4m4(inst_data.mat, ctx->ob->object_to_world, bone_mat);
    OVERLAY_bone_instance_data_set_color_hint(&inst_data, color);
    OVERLAY_bone_instance_data_set_color(&inst_data, color);
    DRW_buffer_add_entry_struct(buf, inst_data.mat);
  }

  /* TODO(fclem): needs to be moved elsewhere. */
  drw_batch_cache_generate_requested_delayed(custom);
}

static void drw_shgroup_custom_bone_curve(const ArmatureDrawContext *ctx,
                                          Curve *curve,
                                          const float (*bone_mat)[4],
                                          const float outline_color[4],
                                          Object *custom)
{
  /* TODO(fclem): arg... less than ideal but we never iter on this object
   * to assure batch cache is valid. */
  DRW_curve_batch_cache_validate(curve);

  /* This only handles curves without any surface. The other curve types should have been converted
   * to meshes and rendered in the mesh drawing function. */
  GPUBatch *loose_edges = nullptr;
  if (custom->type == OB_FONT) {
    loose_edges = DRW_cache_text_edge_wire_get(custom);
  }
  else {
    loose_edges = DRW_cache_curve_edge_wire_get(custom);
  }

  if (loose_edges) {
    BoneInstanceData inst_data;
    mul_m4_m4m4(inst_data.mat, ctx->ob->object_to_world, bone_mat);

    DRWCallBuffer *buf = custom_bone_instance_shgroup(ctx, ctx->custom_wire, loose_edges);
    OVERLAY_bone_instance_data_set_color_hint(&inst_data, outline_color);
    OVERLAY_bone_instance_data_set_color(&inst_data, outline_color);
    DRW_buffer_add_entry_struct(buf, inst_data.mat);
  }

  /* TODO(fclem): needs to be moved elsewhere. */
  drw_batch_cache_generate_requested_delayed(custom);
}

static void drw_shgroup_bone_custom_solid(const ArmatureDrawContext *ctx,
                                          const float (*bone_mat)[4],
                                          const float bone_color[4],
                                          const float hint_color[4],
                                          const float outline_color[4],
                                          Object *custom)
{
  /* The custom object is not an evaluated object, so its object->data field hasn't been replaced
   * by #data_eval. This is bad since it gives preference to an object's evaluated mesh over any
   * other data type, but supporting all evaluated geometry components would require a much
   * larger refactor of this area. */
  Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf(custom);
  if (mesh != nullptr) {
    drw_shgroup_bone_custom_solid_mesh(
        ctx, mesh, bone_mat, bone_color, hint_color, outline_color, custom);
    return;
  }

  if (ELEM(custom->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    drw_shgroup_custom_bone_curve(
        ctx, static_cast<Curve *>(custom->data), bone_mat, outline_color, custom);
  }
}

static void drw_shgroup_bone_custom_wire(const ArmatureDrawContext *ctx,
                                         const float (*bone_mat)[4],
                                         const float color[4],
                                         Object *custom)
{
  /* See comments in #drw_shgroup_bone_custom_solid. */
  Mesh *mesh = BKE_object_get_evaluated_mesh_no_subsurf(custom);
  if (mesh != nullptr) {
    drw_shgroup_bone_custom_mesh_wire(ctx, mesh, bone_mat, color, custom);
    return;
  }

  if (ELEM(custom->type, OB_CURVES_LEGACY, OB_FONT, OB_SURF)) {
    drw_shgroup_custom_bone_curve(
        ctx, static_cast<Curve *>(custom->data), bone_mat, color, custom);
  }
}

static void drw_shgroup_bone_custom_empty(const ArmatureDrawContext *ctx,
                                          const float (*bone_mat)[4],
                                          const float color[4],
                                          Object *custom)
{
  const float final_color[4] = {color[0], color[1], color[2], 1.0f};
  float mat[4][4];
  mul_m4_m4m4(mat, ctx->ob->object_to_world, bone_mat);

  switch (custom->empty_drawtype) {
    case OB_PLAINAXES:
    case OB_SINGLE_ARROW:
    case OB_CUBE:
    case OB_CIRCLE:
    case OB_EMPTY_SPHERE:
    case OB_EMPTY_CONE:
    case OB_ARROWS:
      OVERLAY_empty_shape(
          ctx->extras, mat, custom->empty_drawsize, custom->empty_drawtype, final_color);
      break;
    case OB_EMPTY_IMAGE:
      break;
  }
}

/* Head and tail sphere */
static void drw_shgroup_bone_point(const ArmatureDrawContext *ctx,
                                   const float (*bone_mat)[4],
                                   const float bone_color[4],
                                   const float hint_color[4],
                                   const float outline_color[4])
{
  BoneInstanceData inst_data;
  mul_m4_m4m4(inst_data.mat, ctx->ob->object_to_world, bone_mat);
  if (ctx->point_solid) {
    OVERLAY_bone_instance_data_set_color(&inst_data, bone_color);
    OVERLAY_bone_instance_data_set_color_hint(&inst_data, hint_color);
    DRW_buffer_add_entry_struct(ctx->point_solid, &inst_data);
  }
  if (outline_color[3] > 0.0f) {
    OVERLAY_bone_instance_data_set_color(&inst_data, outline_color);
    DRW_buffer_add_entry_struct(ctx->point_outline, &inst_data);
  }
}

/* Axes */
static void drw_shgroup_bone_axes(const ArmatureDrawContext *ctx,
                                  const float (*bone_mat)[4],
                                  const float color[4])
{
  float mat[4][4];
  mul_m4_m4m4(mat, ctx->ob->object_to_world, bone_mat);
  /* Move to bone tail. */
  add_v3_v3(mat[3], mat[1]);
  OVERLAY_empty_shape(ctx->extras, mat, 0.25f, OB_ARROWS, color);
}

/* Relationship lines */
static void drw_shgroup_bone_relationship_lines_ex(const ArmatureDrawContext *ctx,
                                                   const float start[3],
                                                   const float end[3],
                                                   const float color[4])
{
  float s[3], e[3];
  mul_v3_m4v3(s, ctx->ob->object_to_world, start);
  mul_v3_m4v3(e, ctx->ob->object_to_world, end);
  /* reverse order to have less stipple overlap */
  OVERLAY_extra_line_dashed(ctx->extras, s, e, color);
}

static void drw_shgroup_bone_relationship_lines(const ArmatureDrawContext *ctx,
                                                const float start[3],
                                                const float end[3])
{
  drw_shgroup_bone_relationship_lines_ex(ctx, start, end, G_draw.block.color_wire);
}

static void drw_shgroup_bone_ik_lines(const ArmatureDrawContext *ctx,
                                      const float start[3],
                                      const float end[3])
{
  drw_shgroup_bone_relationship_lines_ex(ctx, start, end, G_draw.block.color_bone_ik_line);
}

static void drw_shgroup_bone_ik_no_target_lines(const ArmatureDrawContext *ctx,
                                                const float start[3],
                                                const float end[3])
{
  drw_shgroup_bone_relationship_lines_ex(
      ctx, start, end, G_draw.block.color_bone_ik_line_no_target);
}

static void drw_shgroup_bone_ik_spline_lines(const ArmatureDrawContext *ctx,
                                             const float start[3],
                                             const float end[3])
{
  drw_shgroup_bone_relationship_lines_ex(ctx, start, end, G_draw.block.color_bone_ik_line_spline);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing Theme Helpers
 *
 * \note this section is duplicate of code in 'drawarmature.c'.
 *
 * \{ */

/* This function sets the color-set for coloring a certain bone */
static void set_pchan_colorset(ArmatureDrawContext *ctx, Object *ob, bPoseChannel *pchan)
{
  bPose *pose = (ob) ? ob->pose : nullptr;
  bArmature *arm = (ob) ? static_cast<bArmature *>(ob->data) : nullptr;
  bActionGroup *grp = nullptr;
  short color_index = 0;

  /* sanity check */
  if (ELEM(nullptr, ob, arm, pose, pchan)) {
    ctx->bcolor = nullptr;
    return;
  }

  /* only try to set custom color if enabled for armature */
  if (arm->flag & ARM_COL_CUSTOM) {
    /* currently, a bone can only use a custom color set if its group (if it has one),
     * has been set to use one
     */
    if (pchan->agrp_index) {
      grp = (bActionGroup *)BLI_findlink(&pose->agroups, (pchan->agrp_index - 1));
      if (grp) {
        color_index = grp->customCol;
      }
    }
  }

  /* bcolor is a pointer to the color set to use. If nullptr, then the default
   * color set (based on the theme colors for 3d-view) is used.
   */
  if (color_index > 0) {
    bTheme *btheme = UI_GetTheme();
    ctx->bcolor = &btheme->tarm[(color_index - 1)];
  }
  else if (color_index == -1) {
    /* use the group's own custom color set (grp is always != nullptr here) */
    ctx->bcolor = &grp->cs;
  }
  else {
    ctx->bcolor = nullptr;
  }
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
  uint8_t srgb_color[4];
  copy_v3_v3_uchar(srgb_color, color_from_theme);
  if (shade_offset != 0) {
    cp_shade_color3ub(srgb_color, shade_offset);
  }
  rgb_uchar_to_float(r_color, srgb_color);
  /* Meh, hardcoded srgb transform here. */
  srgb_to_linearrgb_v4(r_color, r_color);
};

static void get_pchan_color_wire(const ThemeWireColor *bcolor,
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
      wire_color = is_edit ? G_draw.block.color_bone_active : G_draw.block.color_bone_pose_active;
    }
    else if (draw_active) {
      wire_color = is_edit ? G_draw.block.color_bone_active_unsel :
                             G_draw.block.color_bone_pose_active_unsel;
    }
    else if (draw_selected) {
      wire_color = is_edit ? G_draw.block.color_bone_select : G_draw.block.color_bone_pose;
    }
    else {
      wire_color = is_edit ? G_draw.block.color_wire_edit : G_draw.block.color_wire;
    }
    copy_v4_v4(r_color, wire_color);
  }
}

static void get_pchan_color_solid(const ThemeWireColor *bcolor, float r_color[4])
{

  if (bcolor) {
    use_bone_color(r_color, bcolor->solid, 0);
  }
  else {
    copy_v4_v4(r_color, G_draw.block.color_bone_solid);
  }
}

static void get_pchan_color_constraint(const ThemeWireColor *bcolor,
                                       const UnifiedBonePtr bone,
                                       float r_color[4])
{
  const ePchan_ConstFlag constflag = bone.constflag();
  if (constflag == 0 || (bcolor && (bcolor->flag & TH_WIRECOLOR_CONSTCOLS) == 0)) {
    get_pchan_color_solid(bcolor, r_color);
    return;
  }

  /* The constraint color needs to be blended with the solid color. */
  float solid_color[4];
  get_pchan_color_solid(bcolor, solid_color);

  float4 constraint_color;
  if (constflag & PCHAN_HAS_TARGET) {
    constraint_color = G_draw.block.color_bone_pose_target;
  }
  else if (constflag & PCHAN_HAS_IK) {
    constraint_color = G_draw.block.color_bone_pose_ik;
  }
  else if (constflag & PCHAN_HAS_SPLINEIK) {
    constraint_color = G_draw.block.color_bone_pose_spline_ik;
  }
  else if (constflag & PCHAN_HAS_CONST) {
    constraint_color = G_draw.block.color_bone_pose_constraint;
  }
  interp_v3_v3v3(r_color, solid_color, constraint_color, 0.5f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing Color Helpers
 * \{ */

static void bone_locked_color_shade(float color[4])
{
  float *locked_color = G_draw.block.color_bone_locked;

  interp_v3_v3v3(color, color, locked_color, locked_color[3]);
}

static const float *get_bone_solid_color(const ArmatureDrawContext *ctx, const eBone_Flag boneflag)
{
  if (ctx->const_color) {
    return G_draw.block.color_bone_solid;
  }

  if (ctx->draw_mode == ARM_DRAW_MODE_POSE) {
    static float disp_color[4];
    get_pchan_color_solid(ctx->bcolor, disp_color);

    if (boneflag & BONE_DRAW_LOCKED_WEIGHT) {
      bone_locked_color_shade(disp_color);
    }

    return disp_color;
  }

  return G_draw.block.color_bone_solid;
}

static const float *get_bone_solid_with_consts_color(const ArmatureDrawContext *ctx,
                                                     const UnifiedBonePtr bone,
                                                     const eBone_Flag boneflag)
{
  if (ctx->const_color) {
    return G_draw.block.color_bone_solid;
  }

  const float *col = get_bone_solid_color(ctx, boneflag);

  if (ctx->draw_mode != ARM_DRAW_MODE_POSE || (boneflag & BONE_DRAW_LOCKED_WEIGHT)) {
    return col;
  }

  static float consts_color[4];
  get_pchan_color_constraint(ctx->bcolor, bone, consts_color);
  return consts_color;
}

static float get_bone_wire_thickness(const ArmatureDrawContext *ctx, int boneflag)
{
  if (ctx->const_color) {
    return ctx->const_wire;
  }
  if (boneflag & (BONE_DRAW_ACTIVE | BONE_SELECTED)) {
    return 2.0f;
  }

  return 1.0f;
}

static const float *get_bone_wire_color(const ArmatureDrawContext *ctx, const eBone_Flag boneflag)
{
  static float disp_color[4];

  if (ctx->const_color) {
    copy_v3_v3(disp_color, ctx->const_color);
  }
  else {
    switch (ctx->draw_mode) {
      case ARM_DRAW_MODE_EDIT:
        get_pchan_color_wire(ctx->bcolor, ctx->draw_mode, boneflag, disp_color);
        break;
      case ARM_DRAW_MODE_POSE:
        get_pchan_color_wire(ctx->bcolor, ctx->draw_mode, boneflag, disp_color);

        if (boneflag & BONE_DRAW_LOCKED_WEIGHT) {
          bone_locked_color_shade(disp_color);
        }
        break;
      case ARM_DRAW_MODE_OBJECT:
        copy_v3_v3(disp_color, G_draw.block.color_vertex);
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

static const float *get_bone_hint_color(const ArmatureDrawContext *ctx, const eBone_Flag boneflag)
{
  static float hint_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};

  if (ctx->const_color) {
    bone_hint_color_shade(hint_color, G_draw.block.color_bone_solid);
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
  float(*bone_mat)[4];
  float(*disp_mat)[4] = bone.disp_mat();
  float(*disp_tail_mat)[4] = bone.disp_tail_mat();

  /* TODO: This should be moved to depsgraph or armature refresh
   * and not be tight to the draw pass creation.
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

/* compute connected child pointer for B-Bone drawing */
static void edbo_compute_bbone_child(bArmature *arm)
{
  EditBone *eBone;

  for (eBone = static_cast<EditBone *>(arm->edbo->first); eBone; eBone = eBone->next) {
    eBone->bbone_child = nullptr;
  }

  for (eBone = static_cast<EditBone *>(arm->edbo->first); eBone; eBone = eBone->next) {
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
  float(*bone_mat)[4];
  short bbone_segments;

  /* TODO: This should be moved to depsgraph or armature refresh
   * and not be tight to the draw pass creation.
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
    float(*bbones_mat)[4][4] = eBone->disp_bbone_mat;

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

static void draw_axes(const ArmatureDrawContext *ctx,
                      const UnifiedBonePtr bone,
                      const bArmature *arm)
{
  float final_col[4];
  const float *col = (ctx->const_color)            ? ctx->const_color :
                     (bone.flag() & BONE_SELECTED) ? &G_draw.block.color_text_hi.x :
                                                     &G_draw.block.color_text.x;
  copy_v4_v4(final_col, col);
  /* Mix with axes color. */
  final_col[3] = (ctx->const_color) ? 1.0 : (bone.flag() & BONE_SELECTED) ? 0.1 : 0.65;

  if (bone.is_posebone() && bone.as_posebone()->custom && !(arm->flag & ARM_NO_CUSTOM)) {
    const bPoseChannel *pchan = bone.as_posebone();
    /* Special case: Custom bones can have different scale than the bone.
     * Recompute display matrix without the custom scaling applied. (#65640). */
    float axis_mat[4][4];
    float length = pchan->bone->length;
    copy_m4_m4(axis_mat, pchan->custom_tx ? pchan->custom_tx->pose_mat : pchan->pose_mat);
    const float3 length_vec = {length, length, length};
    rescale_m4(axis_mat, length_vec);
    translate_m4(axis_mat, 0.0, arm->axes_position - 1.0, 0.0);

    drw_shgroup_bone_axes(ctx, axis_mat, final_col);
  }
  else {
    float disp_mat[4][4];
    copy_m4_m4(disp_mat, bone.disp_mat());
    translate_m4(disp_mat, 0.0, arm->axes_position - 1.0, 0.0);
    drw_shgroup_bone_axes(ctx, disp_mat, final_col);
  }
}

static void draw_points(const ArmatureDrawContext *ctx,
                        const UnifiedBonePtr bone,
                        const eBone_Flag boneflag,
                        const int select_id)
{
  float col_solid_root[4], col_solid_tail[4], col_wire_root[4], col_wire_tail[4];
  float col_hint_root[4], col_hint_tail[4];

  copy_v4_v4(col_solid_root, G_draw.block.color_bone_solid);
  copy_v4_v4(col_solid_tail, G_draw.block.color_bone_solid);
  copy_v4_v4(col_wire_root, (ctx->const_color) ? ctx->const_color : &G_draw.block.color_vertex.x);
  copy_v4_v4(col_wire_tail, (ctx->const_color) ? ctx->const_color : &G_draw.block.color_vertex.x);

  const bool is_envelope_draw = (ctx->drawtype == ARM_ENVELOPE);
  const float envelope_ignore = -1.0f;

  col_wire_tail[3] = col_wire_root[3] = get_bone_wire_thickness(ctx, boneflag);

  /* Edit bone points can be selected */
  if (ctx->draw_mode == ARM_DRAW_MODE_EDIT) {
    const EditBone *eBone = bone.as_editbone();
    if (eBone->flag & BONE_ROOTSEL) {
      copy_v3_v3(col_wire_root, G_draw.block.color_vertex_select);
    }
    if (eBone->flag & BONE_TIPSEL) {
      copy_v3_v3(col_wire_tail, G_draw.block.color_vertex_select);
    }
  }
  else if (ctx->draw_mode == ARM_DRAW_MODE_POSE) {
    const float *solid_color = get_bone_solid_color(ctx, boneflag);
    const float *wire_color = get_bone_wire_color(ctx, boneflag);
    copy_v4_v4(col_wire_tail, wire_color);
    copy_v4_v4(col_wire_root, wire_color);
    copy_v4_v4(col_solid_tail, solid_color);
    copy_v4_v4(col_solid_root, solid_color);
  }

  bone_hint_color_shade(col_hint_root, (ctx->const_color) ? col_solid_root : col_wire_root);
  bone_hint_color_shade(col_hint_tail, (ctx->const_color) ? col_solid_tail : col_wire_tail);

  /* Draw root point if we are not connected to our parent */

  if (!(bone.has_parent() && (boneflag & BONE_CONNECTED))) {
    if (select_id != -1) {
      DRW_select_load_id(select_id | BONESEL_ROOT);
    }

    if (is_envelope_draw) {
      drw_shgroup_bone_envelope(ctx,
                                bone.disp_mat(),
                                col_solid_root,
                                col_hint_root,
                                col_wire_root,
                                &bone.rad_head(),
                                &envelope_ignore);
    }
    else {
      drw_shgroup_bone_point(ctx, bone.disp_mat(), col_solid_root, col_hint_root, col_wire_root);
    }
  }

  /*  Draw tip point */
  if (select_id != -1) {
    DRW_select_load_id(select_id | BONESEL_TIP);
  }

  if (is_envelope_draw) {
    drw_shgroup_bone_envelope(ctx,
                              bone.disp_mat(),
                              col_solid_tail,
                              col_hint_tail,
                              col_wire_tail,
                              &envelope_ignore,
                              &bone.rad_tail());
  }
  else {
    drw_shgroup_bone_point(
        ctx, bone.disp_tail_mat(), col_solid_tail, col_hint_tail, col_wire_tail);
  }

  if (select_id != -1) {
    DRW_select_load_id(-1);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Degrees of Freedom
 * \{ */

static void draw_bone_degrees_of_freedom(const ArmatureDrawContext *ctx, const bPoseChannel *pchan)
{
  BoneInstanceData inst_data;
  float tmp[4][4], posetrans[4][4];
  float xminmax[2], zminmax[2];
  float color[4];

  if (ctx->dof_sphere == nullptr) {
    return;
  }

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
  /* ... but own rest-space. */
  mul_m4_m4m3(posetrans, posetrans, pchan->bone->bone_mat);

  float scale = pchan->bone->length * pchan->size[1];
  scale_m4_fl(tmp, scale);
  tmp[1][1] = -tmp[1][1];
  mul_m4_m4m4(posetrans, posetrans, tmp);

  /* into world space. */
  mul_m4_m4m4(inst_data.mat, ctx->ob->object_to_world, posetrans);

  if ((pchan->ikflag & BONE_IK_XLIMIT) && (pchan->ikflag & BONE_IK_ZLIMIT)) {
    bone_instance_data_set_angle_minmax(
        &inst_data, xminmax[0], zminmax[0], xminmax[1], zminmax[1]);

    copy_v4_fl4(color, 0.25f, 0.25f, 0.25f, 0.25f);
    DRW_buffer_add_entry(ctx->dof_sphere, color, &inst_data);

    copy_v4_fl4(color, 0.0f, 0.0f, 0.0f, 1.0f);
    DRW_buffer_add_entry(ctx->dof_lines, color, &inst_data);
  }
  if (pchan->ikflag & BONE_IK_XLIMIT) {
    bone_instance_data_set_angle_minmax(&inst_data, xminmax[0], 0.0f, xminmax[1], 0.0f);
    copy_v4_fl4(color, 1.0f, 0.0f, 0.0f, 1.0f);
    DRW_buffer_add_entry(ctx->dof_lines, color, &inst_data);
  }
  if (pchan->ikflag & BONE_IK_ZLIMIT) {
    bone_instance_data_set_angle_minmax(&inst_data, 0.0f, zminmax[0], 0.0f, zminmax[1]);
    copy_v4_fl4(color, 0.0f, 0.0f, 1.0f, 1.0f);
    DRW_buffer_add_entry(ctx->dof_lines, color, &inst_data);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Relationships
 * \{ */

static void pchan_draw_ik_lines(const ArmatureDrawContext *ctx,
                                const bPoseChannel *pchan,
                                const bool only_temp)
{
  const bConstraint *con;
  const bPoseChannel *parchan;
  const float *line_start = nullptr, *line_end = nullptr;
  const ePchan_ConstFlag constflag = ePchan_ConstFlag(pchan->constflag);

  for (con = static_cast<bConstraint *>(pchan->constraints.first); con; con = con->next) {
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

          if (constflag & PCHAN_HAS_TARGET) {
            drw_shgroup_bone_ik_lines(ctx, line_start, line_end);
          }
          else {
            drw_shgroup_bone_ik_no_target_lines(ctx, line_start, line_end);
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

static void draw_bone_bone_relationship_line(const ArmatureDrawContext *ctx,
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

static void draw_bone_relations(const ArmatureDrawContext *ctx,
                                const ArmatureBoneDrawStrategy &draw_strategy,
                                const UnifiedBonePtr bone,
                                const eBone_Flag boneflag)
{
  if (ctx->draw_mode == ARM_DRAW_MODE_EDIT) {
    const EditBone *ebone = bone.as_editbone();
    if (ebone->parent) {
      if (ctx->do_relations && draw_strategy.should_draw_relation_to_parent(bone, boneflag)) {
        draw_bone_bone_relationship_line(
            ctx, ebone->head, ebone->parent->head, ebone->parent->tail);
      }
    }
  }
  else {
    const bPoseChannel *pchan = bone.as_posebone();
    if (pchan->parent) {
      if (ctx->do_relations && draw_strategy.should_draw_relation_to_parent(bone, boneflag)) {
        draw_bone_bone_relationship_line(
            ctx, pchan->pose_head, pchan->parent->pose_head, pchan->parent->pose_tail);
      }

      /* Draw a line to IK root bone if bone is selected. */
      if (ctx->draw_mode == ARM_DRAW_MODE_POSE) {
        if (pchan->constflag & (PCHAN_HAS_IK | PCHAN_HAS_SPLINEIK)) {
          if (boneflag & BONE_SELECTED) {
            pchan_draw_ik_lines(ctx, bone.as_posebone(), !ctx->do_relations);
          }
        }
      }
    }
  }
}

static void draw_bone_name(const ArmatureDrawContext *ctx,
                           const UnifiedBonePtr bone,
                           const eBone_Flag boneflag)
{
  DRWTextStore *dt = DRW_text_cache_ensure();
  uchar color[4];
  float vec[3];

  const bool is_pose = bone.is_posebone();
  const EditBone *eBone = nullptr;
  const bPoseChannel *pchan = nullptr;
  bone.get(&eBone, &pchan);

  /* TODO: make this look at `boneflag` only. */
  bool highlight = (is_pose && ctx->draw_mode == ARM_DRAW_MODE_POSE &&
                    (boneflag & BONE_SELECTED)) ||
                   (!is_pose && (eBone->flag & BONE_SELECTED));

  /* Color Management: Exception here as texts are drawn in sRGB space directly. */
  UI_GetThemeColor4ubv(highlight ? TH_TEXT_HI : TH_TEXT, color);

  const float *head = is_pose ? pchan->pose_head : eBone->head;
  const float *tail = is_pose ? pchan->pose_tail : eBone->tail;
  mid_v3_v3v3(vec, head, tail);
  mul_m4_v3(ctx->ob->object_to_world, vec);

  DRW_text_cache_add(dt,
                     vec,
                     is_pose ? pchan->name : eBone->name,
                     is_pose ? strlen(pchan->name) : strlen(eBone->name),
                     10,
                     0,
                     DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                     color);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Bone Culling
 *
 * Used for selection since drawing many bones can be slow, see: #91253.
 *
 * Bounding spheres are used with margins added to ensure bones are included.
 * An added margin is needed because #BKE_pchan_minmax only returns the bounds
 * of the bones head & tail which doesn't account for parts of the bone users may select
 * (octahedral spheres or envelope radius for example).
 * \{ */

static void pchan_culling_calc_bsphere(const Object *ob,
                                       const bPoseChannel *pchan,
                                       BoundSphere *r_bsphere)
{
  float min[3], max[3];
  INIT_MINMAX(min, max);
  BKE_pchan_minmax(ob, pchan, true, min, max);
  mid_v3_v3v3(r_bsphere->center, min, max);
  r_bsphere->radius = len_v3v3(min, r_bsphere->center);
}

/**
 * \return true when bounding sphere from `pchan` intersect the view.
 * (same for other "test" functions defined here).
 */
static bool pchan_culling_test_simple(const DRWView *view,
                                      const Object *ob,
                                      const bPoseChannel *pchan)
{
  BoundSphere bsphere;
  pchan_culling_calc_bsphere(ob, pchan, &bsphere);
  return DRW_culling_sphere_test(view, &bsphere);
}

static bool pchan_culling_test_with_radius_scale(const DRWView *view,
                                                 const Object *ob,
                                                 const bPoseChannel *pchan,
                                                 const float scale)
{
  BoundSphere bsphere;
  pchan_culling_calc_bsphere(ob, pchan, &bsphere);
  bsphere.radius *= scale;
  return DRW_culling_sphere_test(view, &bsphere);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bone Drawing Strategies
 *
 * Bone drawing uses a strategy pattern for the different armature drawing modes.
 * \{ */

/**
 * Bone drawing strategy for unknown draw types.
 * This doesn't do anything, except call the default matrix update function.
 */
class ArmatureBoneDrawStrategyEmpty : public ArmatureBoneDrawStrategy {
 public:
  void update_display_matrix(UnifiedBonePtr bone) const override
  {
    draw_bone_update_disp_matrix_default(bone);
  }

  bool culling_test(const DRWView * /*view*/,
                    const Object * /*ob*/,
                    const bPoseChannel * /*pchan*/) const override
  {
    return false;
  }

  void draw_context_setup(ArmatureDrawContext * /*ctx*/,
                          const OVERLAY_ArmatureCallBuffersInner * /*cb*/,
                          const bool /*is_filled*/,
                          const bool /*do_envelope_dist*/) const override
  {
  }

  void draw_bone(const ArmatureDrawContext * /*ctx*/,
                 const UnifiedBonePtr /*bone*/,
                 const eBone_Flag /*boneflag*/,
                 const int /*select_id*/) const override
  {
  }
};

/** Bone drawing strategy for custom bone shapes. */
class ArmatureBoneDrawStrategyCustomShape : public ArmatureBoneDrawStrategy {
 public:
  void update_display_matrix(UnifiedBonePtr bone) const override
  {
    float bone_scale[3];
    float(*bone_mat)[4];
    float(*disp_mat)[4];
    float(*disp_tail_mat)[4];
    float rot_mat[3][3];

    /* Custom bone shapes are only supported in pose mode for now. */
    bPoseChannel *pchan = bone.as_posebone();

    /* TODO: This should be moved to depsgraph or armature refresh
     * and not be tight to the draw pass creation.
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

  bool culling_test(const DRWView *view,
                    const Object *ob,
                    const bPoseChannel *pchan) const override
  {
    /* For more aggressive culling the bounding box of the custom-object could be used. */
    return pchan_culling_test_simple(view, ob, pchan);
  }

  void draw_context_setup(ArmatureDrawContext * /*ctx*/,
                          const OVERLAY_ArmatureCallBuffersInner * /*cb*/,
                          const bool /*is_filled*/,
                          const bool /*do_envelope_dist*/) const override
  {
  }

  void draw_bone(const ArmatureDrawContext *ctx,
                 const UnifiedBonePtr bone,
                 const eBone_Flag boneflag,
                 const int select_id) const override
  {
    const float *col_solid = get_bone_solid_color(ctx, boneflag);
    const float *col_wire = get_bone_wire_color(ctx, boneflag);
    const float *col_hint = get_bone_hint_color(ctx, boneflag);
    const float(*disp_mat)[4] = bone.disp_mat();

    if (select_id != -1) {
      DRW_select_load_id(select_id | BONESEL_BONE);
    }

    /* Custom bone shapes are only supported in pose mode for now. */
    const bPoseChannel *pchan = bone.as_posebone();

    if (pchan->custom->type == OB_EMPTY) {
      Object *ob = pchan->custom;
      if (ob->empty_drawtype != OB_EMPTY_IMAGE) {
        drw_shgroup_bone_custom_empty(ctx, disp_mat, col_wire, pchan->custom);
      }
    }
    if ((boneflag & BONE_DRAWWIRE) == 0 && (boneflag & BONE_DRAW_LOCKED_WEIGHT) == 0) {
      drw_shgroup_bone_custom_solid(ctx, disp_mat, col_solid, col_hint, col_wire, pchan->custom);
    }
    else {
      drw_shgroup_bone_custom_wire(ctx, disp_mat, col_wire, pchan->custom);
    }

    if (select_id != -1) {
      DRW_select_load_id(-1);
    }
  }
};

/** Bone drawing strategy for ARM_OCTA. */
class ArmatureBoneDrawStrategyOcta : public ArmatureBoneDrawStrategy {
 public:
  void update_display_matrix(UnifiedBonePtr bone) const override
  {
    draw_bone_update_disp_matrix_default(bone);
  }

  bool culling_test(const DRWView *view,
                    const Object *ob,
                    const bPoseChannel *pchan) const override
  {
    /* No type assertion as this is a fallback (files from the future will end up here). */
    /* Account for spheres on the end-points. */
    const float scale = 1.2f;
    return pchan_culling_test_with_radius_scale(view, ob, pchan, scale);
  }

  void draw_context_setup(ArmatureDrawContext *ctx,
                          const OVERLAY_ArmatureCallBuffersInner *cb,
                          const bool is_filled,
                          const bool /*do_envelope_dist*/) const override
  {
    ctx->outline = cb->octa_outline;
    ctx->solid = (is_filled) ? cb->octa_fill : nullptr;
  }

  void draw_bone(const ArmatureDrawContext *ctx,
                 const UnifiedBonePtr bone,
                 const eBone_Flag boneflag,
                 const int select_id) const override
  {
    const float *col_solid = get_bone_solid_with_consts_color(ctx, bone, boneflag);
    const float *col_wire = get_bone_wire_color(ctx, boneflag);
    const float *col_hint = get_bone_hint_color(ctx, boneflag);

    if (select_id != -1) {
      DRW_select_load_id(select_id | BONESEL_BONE);
    }

    drw_shgroup_bone_octahedral(ctx, bone.disp_mat(), col_solid, col_hint, col_wire);

    if (select_id != -1) {
      DRW_select_load_id(-1);
    }

    draw_points(ctx, bone, boneflag, select_id);
  }
};

/** Bone drawing strategy for ARM_LINE. */
class ArmatureBoneDrawStrategyLine : public ArmatureBoneDrawStrategy {
 public:
  void update_display_matrix(UnifiedBonePtr bone) const override
  {
    draw_bone_update_disp_matrix_default(bone);
  }

  bool culling_test(const DRWView *view,
                    const Object *ob,
                    const bPoseChannel *pchan) const override
  {
    /* Account for the end-points, as the line end-points size is in pixels, this is a rough
     * value. Since the end-points are small the difference between having any margin or not is
     * unlikely to be noticeable. */
    const float scale = 1.1f;
    return pchan_culling_test_with_radius_scale(view, ob, pchan, scale);
  }

  void draw_context_setup(ArmatureDrawContext *ctx,
                          const OVERLAY_ArmatureCallBuffersInner *cb,
                          const bool /*is_filled*/,
                          const bool /*do_envelope_dist*/) const override
  {
    ctx->stick = cb->stick;
  }

  void draw_bone(const ArmatureDrawContext *ctx,
                 const UnifiedBonePtr bone,
                 const eBone_Flag boneflag,
                 const int select_id) const override
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
      if (bone.is_editbone()) {
        if (bone.flag() & BONE_TIPSEL) {
          col_tail = G_draw.block.color_vertex_select;
        }
        if (boneflag & BONE_SELECTED) {
          col_bone = G_draw.block.color_bone_active;
        }
        col_wire = G_draw.block.color_wire;
      }

      /* Draw root point if we are not connected to our parent. */
      if (!(bone.has_parent() && (boneflag & BONE_CONNECTED))) {

        if (bone.is_editbone()) {
          col_head = (bone.flag() & BONE_ROOTSEL) ? &G_draw.block.color_vertex_select.x : col_bone;
        }
        else {
          col_head = col_bone;
        }
      }
    }

    if (select_id == -1) {
      /* Not in selection mode, draw everything at once. */
      drw_shgroup_bone_stick(ctx, bone.disp_mat(), col_wire, col_bone, col_head, col_tail);
    }
    else {
      /* In selection mode, draw bone, root and tip separately. */
      DRW_select_load_id(select_id | BONESEL_BONE);
      drw_shgroup_bone_stick(ctx, bone.disp_mat(), col_wire, col_bone, no_display, no_display);

      if (col_head[3] > 0.0f) {
        DRW_select_load_id(select_id | BONESEL_ROOT);
        drw_shgroup_bone_stick(ctx, bone.disp_mat(), col_wire, no_display, col_head, no_display);
      }

      DRW_select_load_id(select_id | BONESEL_TIP);
      drw_shgroup_bone_stick(ctx, bone.disp_mat(), col_wire, no_display, no_display, col_tail);

      DRW_select_load_id(-1);
    }
  }
};

/** Bone drawing strategy for ARM_B_BONE. */
class ArmatureBoneDrawStrategyBBone : public ArmatureBoneDrawStrategy {
 public:
  void update_display_matrix(UnifiedBonePtr bone) const override
  {
    draw_bone_update_disp_matrix_bbone(bone);
  }

  bool culling_test(const DRWView *view,
                    const Object *ob,
                    const bPoseChannel *pchan) const override
  {
    const bArmature *arm = static_cast<bArmature *>(ob->data);
    BLI_assert(arm->drawtype == ARM_B_BONE);
    UNUSED_VARS_NDEBUG(arm);
    const float ob_scale = mat4_to_size_max_axis(ob->object_to_world);
    const Mat4 *bbones_mat = (const Mat4 *)pchan->draw_data->bbone_matrix;
    for (int i = pchan->bone->segments; i--; bbones_mat++) {
      BoundSphere bsphere;
      float size[3];
      mat4_to_size(size, bbones_mat->mat);
      mul_v3_m4v3(bsphere.center, ob->object_to_world, bbones_mat->mat[3]);
      bsphere.radius = len_v3(size) * ob_scale;
      if (DRW_culling_sphere_test(view, &bsphere)) {
        return true;
      }
    }
    return false;
  }

  void draw_context_setup(ArmatureDrawContext *ctx,
                          const OVERLAY_ArmatureCallBuffersInner *cb,
                          const bool is_filled,
                          const bool /*do_envelope_dist*/) const override
  {
    ctx->outline = cb->box_outline;
    ctx->solid = (is_filled) ? cb->box_fill : nullptr;
  }

  void draw_bone(const ArmatureDrawContext *ctx,
                 const UnifiedBonePtr bone,
                 const eBone_Flag boneflag,
                 const int select_id) const override
  {
    const float *col_solid = get_bone_solid_with_consts_color(ctx, bone, boneflag);
    const float *col_wire = get_bone_wire_color(ctx, boneflag);
    const float *col_hint = get_bone_hint_color(ctx, boneflag);

    if (select_id != -1) {
      DRW_select_load_id(select_id | BONESEL_BONE);
    }

    if (bone.is_posebone()) {
      const bPoseChannel *pchan = bone.as_posebone();
      Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
      BLI_assert(bbones_mat != nullptr);

      for (int i = pchan->bone->segments; i--; bbones_mat++) {
        drw_shgroup_bone_box(ctx, bbones_mat->mat, col_solid, col_hint, col_wire);
      }
    }
    else {
      const EditBone *eBone = bone.as_editbone();
      for (int i = 0; i < eBone->segments; i++) {
        drw_shgroup_bone_box(ctx, eBone->disp_bbone_mat[i], col_solid, col_hint, col_wire);
      }
    }

    if (select_id != -1) {
      DRW_select_load_id(-1);
    }

    if (bone.is_editbone()) {
      draw_points(ctx, bone, boneflag, select_id);
    }
  }
};

/** Bone drawing strategy for ARM_ENVELOPE. */
class ArmatureBoneDrawStrategyEnvelope : public ArmatureBoneDrawStrategy {
 public:
  void update_display_matrix(UnifiedBonePtr bone) const override
  {
    draw_bone_update_disp_matrix_default(bone);
  }

  bool culling_test(const DRWView *view,
                    const Object *ob,
                    const bPoseChannel *pchan) const override
  {
    const bArmature *arm = static_cast<bArmature *>(ob->data);
    BLI_assert(arm->drawtype == ARM_ENVELOPE);
    UNUSED_VARS_NDEBUG(arm);
    BoundSphere bsphere;
    pchan_culling_calc_bsphere(ob, pchan, &bsphere);
    bsphere.radius += max_ff(pchan->bone->rad_head, pchan->bone->rad_tail) *
                      mat4_to_size_max_axis(ob->object_to_world) *
                      mat4_to_size_max_axis(pchan->disp_mat);
    return DRW_culling_sphere_test(view, &bsphere);
  }

  void draw_context_setup(ArmatureDrawContext *ctx,
                          const OVERLAY_ArmatureCallBuffersInner *cb,
                          const bool is_filled,
                          const bool do_envelope_dist) const override
  {
    ctx->envelope_outline = cb->envelope_outline;
    ctx->envelope_solid = (is_filled) ? cb->envelope_fill : nullptr;
    ctx->envelope_distance = (do_envelope_dist) ? cb->envelope_distance : nullptr;
  }

  void draw_bone(const ArmatureDrawContext *ctx,
                 const UnifiedBonePtr bone,
                 const eBone_Flag boneflag,
                 const int select_id) const override
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

    if (select_id != -1) {
      DRW_select_load_id(select_id | BONESEL_BONE);
    }

    drw_shgroup_bone_envelope(
        ctx, bone.disp_mat(), col_solid, col_hint, col_wire, rad_head, rad_tail);

    if (select_id != -1) {
      DRW_select_load_id(-1);
    }

    draw_points(ctx, bone, boneflag, select_id);
  }
};

/** Bone drawing strategy for ARM_WIRE. */
class ArmatureBoneDrawStrategyWire : public ArmatureBoneDrawStrategy {
 public:
  void update_display_matrix(UnifiedBonePtr bone) const override
  {
    draw_bone_update_disp_matrix_bbone(bone);
  }

  bool culling_test(const DRWView *view,
                    const Object *ob,
                    const bPoseChannel *pchan) const override
  {
    BLI_assert(((const bArmature *)ob->data)->drawtype == ARM_WIRE);
    return pchan_culling_test_simple(view, ob, pchan);
  }

  void draw_context_setup(ArmatureDrawContext *ctx,
                          const OVERLAY_ArmatureCallBuffersInner *cb,
                          const bool /*is_filled*/,
                          const bool /*do_envelope_dist*/) const override
  {
    ctx->wire = cb->wire;
    ctx->const_wire = 1.5f;
  }

  void draw_bone(const ArmatureDrawContext *ctx,
                 const UnifiedBonePtr bone,
                 const eBone_Flag boneflag,
                 const int select_id) const override
  {
    const float *col_wire = get_bone_wire_color(ctx, boneflag);

    if (select_id != -1) {
      DRW_select_load_id(select_id | BONESEL_BONE);
    }

    if (bone.is_posebone()) {
      const bPoseChannel *pchan = bone.as_posebone();
      Mat4 *bbones_mat = (Mat4 *)pchan->draw_data->bbone_matrix;
      BLI_assert(bbones_mat != nullptr);

      for (int i = pchan->bone->segments; i--; bbones_mat++) {
        drw_shgroup_bone_wire(ctx, bbones_mat->mat, col_wire);
      }
    }
    else {
      const EditBone *eBone = bone.as_editbone();
      for (int i = 0; i < eBone->segments; i++) {
        drw_shgroup_bone_wire(ctx, eBone->disp_bbone_mat[i], col_wire);
      }
    }

    if (select_id != -1) {
      DRW_select_load_id(-1);
    }

    if (bone.is_editbone()) {
      draw_points(ctx, bone, boneflag, select_id);
    }
  }
};

namespace {
/**
 * Armature drawing strategies.
 *
 * Declared statically here because they cost almost no memory (no fields in any
 * of the structs, so just the virtual function table), and this makes it very
 * simple to just pass references to them around.
 *
 * See the functions below.
 */
static ArmatureBoneDrawStrategyOcta strat_octa;
static ArmatureBoneDrawStrategyLine strat_line;
static ArmatureBoneDrawStrategyBBone strat_b_bone;
static ArmatureBoneDrawStrategyEnvelope strat_envelope;
static ArmatureBoneDrawStrategyWire strat_wire;
static ArmatureBoneDrawStrategyEmpty strat_empty;
};  // namespace

/**
 * Return the armature bone drawing strategy for the given draw type.
 *
 * Note that this does not consider custom bone shapes, as those can be set per bone.
 * For those occasions just instance a `ArmatureBoneDrawStrategyCustomShape` and use that.
 */
static ArmatureBoneDrawStrategy &strategy_for_armature_drawtype(const eArmature_Drawtype drawtype)
{
  switch (drawtype) {
    case ARM_OCTA:
      return strat_octa;
    case ARM_LINE:
      return strat_line;
    case ARM_B_BONE:
      return strat_b_bone;
    case ARM_ENVELOPE:
      return strat_envelope;
    case ARM_WIRE:
      return strat_wire;
  }
  BLI_assert_unreachable();
  return strat_empty;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Draw Loops
 * \{ */

static void draw_armature_edit(const ArmatureDrawContext *ctx)
{
  Object *ob = ctx->ob;
  EditBone *eBone;
  int index;
  const bool is_select = DRW_state_is_select();
  const bool show_text = DRW_state_show_text();

  const Object *ob_orig = DEG_get_original_object(ob);
  /* FIXME(@ideasman42): We should be able to use the CoW object,
   * however the active bone isn't updated. Long term solution is an 'EditArmature' struct.
   * for now we can draw from the original armature. See: #66773. */
  // bArmature *arm = ob->data;
  bArmature *arm = static_cast<bArmature *>(ob_orig->data);

  edbo_compute_bbone_child(arm);

  /* Determine drawing strategy. */
  const ArmatureBoneDrawStrategy &draw_strat = strategy_for_armature_drawtype(
      eArmature_Drawtype(arm->drawtype));

  for (eBone = static_cast<EditBone *>(arm->edbo->first), index = ob_orig->runtime.select_id;
       eBone;
       eBone = eBone->next, index += 0x10000)
  {
    if (!ANIM_bonecoll_is_visible_editbone(arm, eBone)) {
      continue;
    }
    if (eBone->flag & BONE_HIDDEN_A) {
      continue;
    }

    const int select_id = is_select ? index : uint(-1);

    /* catch exception for bone with hidden parent */
    eBone_Flag boneflag = eBone_Flag(eBone->flag);
    if ((eBone->parent) && !EBONE_VISIBLE(arm, eBone->parent)) {
      boneflag &= ~BONE_CONNECTED;
    }

    /* set temporary flag for drawing bone as active, but only if selected */
    if (eBone == arm->act_edbone) {
      boneflag |= BONE_DRAW_ACTIVE;
    }

    boneflag &= ~BONE_DRAW_LOCKED_WEIGHT;

    UnifiedBonePtr bone = eBone;
    if (!is_select) {
      draw_bone_relations(ctx, draw_strat, bone, boneflag);
    }

    draw_strat.update_display_matrix(bone);
    draw_strat.draw_bone(ctx, bone, boneflag, select_id);

    if (!is_select) {
      if (show_text && (arm->flag & ARM_DRAWNAMES)) {
        draw_bone_name(ctx, bone, boneflag);
      }

      if (arm->flag & ARM_DRAWAXES) {
        draw_axes(ctx, bone, arm);
      }
    }
  }
}

static void draw_armature_pose(ArmatureDrawContext *ctx)
{
  Object *ob = ctx->ob;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  bArmature *arm = static_cast<bArmature *>(ob->data);
  bPoseChannel *pchan;
  int index = -1;
  const bool show_text = DRW_state_show_text();
  bool draw_locked_weights = false;

  /* We can't safely draw non-updated pose, might contain nullptr bone pointers... */
  if (ob->pose->flag & POSE_RECALC) {
    return;
  }

  ctx->draw_mode = ARM_DRAW_MODE_OBJECT; /* Will likely be set to ARM_DRAW_MODE_POSE below. */

  bool is_pose_select = false;
  /* Object can be edited in the scene. */
  if ((ob->base_flag & (BASE_FROM_SET | BASE_FROM_DUPLI)) == 0) {
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
        DRW_state_is_select();

    if (is_pose_select) {
      const Object *ob_orig = DEG_get_original_object(ob);
      index = ob_orig->runtime.select_id;
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

    const Object *obact_orig = DEG_get_original_object(draw_ctx->obact);

    const ListBase *defbase = BKE_object_defgroup_list(obact_orig);
    for (const bDeformGroup *dg : ConstListBaseWrapper<bDeformGroup>(defbase)) {
      if ((dg->flag & DG_LOCK_WEIGHT) == 0) {
        continue;
      }

      pchan = BKE_pose_channel_find_name(ob->pose, dg->name);
      if (!pchan) {
        continue;
      }

      pchan->bone->flag |= BONE_DRAW_LOCKED_WEIGHT;
    }
  }

  const DRWView *view = is_pose_select ? DRW_view_default_get() : nullptr;

  const ArmatureBoneDrawStrategy &draw_strat_normal = strategy_for_armature_drawtype(
      eArmature_Drawtype(arm->drawtype));
  const ArmatureBoneDrawStrategyCustomShape draw_strat_custom;

  for (pchan = static_cast<bPoseChannel *>(ob->pose->chanbase.first); pchan;
       pchan = pchan->next, index += 0x10000)
  {
    Bone *bone = pchan->bone;
    const bool bone_visible = (bone->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG)) == 0;
    if (!bone_visible) {
      continue;
    }
    if ((bone->layer & arm->layer) == 0) {
      continue;
    }

    const bool draw_dofs = !is_pose_select && ctx->show_relations &&
                           (ctx->draw_mode == ARM_DRAW_MODE_POSE) &&
                           (bone->flag & BONE_SELECTED) &&
                           ((ob->base_flag & BASE_FROM_DUPLI) == 0) &&
                           (pchan->ikflag & (BONE_IK_XLIMIT | BONE_IK_ZLIMIT));
    const int select_id = is_pose_select ? index : uint(-1);

    pchan_draw_data_init(pchan);

    if (!ctx->const_color) {
      set_pchan_colorset(ctx, ob, pchan);
    }

    eBone_Flag boneflag = eBone_Flag(bone->flag);
    if (bone->parent && (bone->parent->flag & (BONE_HIDDEN_P | BONE_HIDDEN_PG))) {
      /* Avoid drawing connection line to hidden parent. */
      boneflag &= ~BONE_CONNECTED;
    }
    if (bone == arm->act_bone) {
      /* Draw bone as active, but only if selected. */
      boneflag |= BONE_DRAW_ACTIVE;
    }
    if (!draw_locked_weights) {
      boneflag &= ~BONE_DRAW_LOCKED_WEIGHT;
    }

    const bool use_custom_shape = (pchan->custom) && !(arm->flag & ARM_NO_CUSTOM);
    const ArmatureBoneDrawStrategy &draw_strat = use_custom_shape ? draw_strat_custom :
                                                                    draw_strat_normal;
    UnifiedBonePtr bone_ptr = pchan;

    if (!is_pose_select) {
      draw_bone_relations(ctx, draw_strat, bone_ptr, boneflag);
    }

    draw_strat.update_display_matrix(bone_ptr);
    if (!is_pose_select || draw_strat.culling_test(view, ob, pchan)) {
      draw_strat.draw_bone(ctx, bone_ptr, boneflag, select_id);
    }

    /* Below this point nothing is used for selection queries. */
    if (is_pose_select) {
      continue;
    }

    if (draw_dofs) {
      draw_bone_degrees_of_freedom(ctx, pchan);
    }
    if (show_text && (arm->flag & ARM_DRAWNAMES)) {
      draw_bone_name(ctx, bone_ptr, boneflag);
    }
    if (arm->flag & ARM_DRAWAXES) {
      draw_axes(ctx, bone_ptr, arm);
    }
  }
}

static void armature_context_setup(ArmatureDrawContext *ctx,
                                   OVERLAY_PrivateData *pd,
                                   Object *ob,
                                   const eArmatureDrawMode draw_mode,
                                   const float *const_color)
{
  BLI_assert(BLI_memory_is_zero(ctx, sizeof(*ctx)));
  const bool is_edit_or_pose_mode = draw_mode != ARM_DRAW_MODE_OBJECT;
  const bool is_xray = (ob->dtx & OB_DRAW_IN_FRONT) != 0 ||
                       (pd->armature.do_pose_xray && draw_mode == ARM_DRAW_MODE_POSE);
  const bool draw_as_wire = (ob->dt < OB_SOLID);
  const bool is_filled = (!pd->armature.transparent && !draw_as_wire) || is_edit_or_pose_mode;
  const bool is_transparent = pd->armature.transparent || (draw_as_wire && is_edit_or_pose_mode);
  bArmature *arm = static_cast<bArmature *>(ob->data);
  OVERLAY_ArmatureCallBuffers *cbo = &pd->armature_call_buffers[is_xray];
  const OVERLAY_ArmatureCallBuffersInner *cb = is_transparent ? &cbo->transp : &cbo->solid;

  static const float select_const_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  ctx->ob = ob;
  ctx->draw_mode = draw_mode;
  ctx->extras = &pd->extra_call_buffers[is_xray];
  ctx->dof_lines = cb->dof_lines;
  ctx->dof_sphere = cb->dof_sphere;
  ctx->point_solid = (is_filled) ? cb->point_fill : nullptr;
  ctx->point_outline = cb->point_outline;
  ctx->custom_solid = (is_filled) ? cb->custom_fill : nullptr;
  ctx->custom_outline = cb->custom_outline;
  ctx->custom_wire = cb->custom_wire;
  ctx->custom_shapes_ghash = cb->custom_shapes_ghash;
  ctx->show_relations = pd->armature.show_relations;
  ctx->do_relations = !DRW_state_is_select() && pd->armature.show_relations &&
                      is_edit_or_pose_mode;
  ctx->const_color = DRW_state_is_select() ? select_const_color : const_color;
  ctx->const_wire = ((ob->base_flag & BASE_SELECTED) && (pd->v3d_flag & V3D_SELECT_OUTLINE) ?
                         1.5f :
                         ((!is_filled || is_transparent) ? 1.0f : 0.0f));
  ctx->draw_relation_from_head = (arm->flag & ARM_DRAW_RELATION_FROM_HEAD);

  /* Call the draw strategy after setting the generic context properties, so
   * that they can be overridden. */
  const eArmature_Drawtype drawtype = eArmature_Drawtype(arm->drawtype);
  ctx->drawtype = drawtype;
  const ArmatureBoneDrawStrategy &draw_strat = strategy_for_armature_drawtype(drawtype);
  draw_strat.draw_context_setup(ctx, cb, is_filled, is_edit_or_pose_mode);
}

void OVERLAY_edit_armature_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  ArmatureDrawContext arm_ctx = {nullptr};
  armature_context_setup(&arm_ctx, pd, ob, ARM_DRAW_MODE_EDIT, nullptr);
  draw_armature_edit(&arm_ctx);
}

void OVERLAY_pose_armature_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  ArmatureDrawContext arm_ctx = {nullptr};
  armature_context_setup(&arm_ctx, pd, ob, ARM_DRAW_MODE_POSE, nullptr);
  draw_armature_pose(&arm_ctx);
}

void OVERLAY_armature_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  ArmatureDrawContext arm_ctx = {nullptr};
  float *color;

  if (ob->dt == OB_BOUNDBOX) {
    return;
  }

  DRW_object_wire_theme_get(ob, draw_ctx->view_layer, &color);
  armature_context_setup(&arm_ctx, pd, ob, ARM_DRAW_MODE_OBJECT, color);
  draw_armature_pose(&arm_ctx);
}

static bool POSE_is_driven_by_active_armature(Object *ob)
{
  Object *ob_arm = BKE_modifiers_is_deformed_by_armature(ob);
  if (ob_arm) {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    bool is_active = OVERLAY_armature_is_pose_mode(ob_arm, draw_ctx);
    return is_active;
  }

  Object *ob_mesh_deform = BKE_modifiers_is_deformed_by_meshdeform(ob);
  if (ob_mesh_deform) {
    /* Recursive. */
    return POSE_is_driven_by_active_armature(ob_mesh_deform);
  }

  return false;
}

void OVERLAY_pose_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  GPUBatch *geom = DRW_cache_object_surface_get(ob);
  if (geom) {
    if (POSE_is_driven_by_active_armature(ob)) {
      DRW_shgroup_call(pd->armature_bone_select_act_grp, geom, ob);
    }
    else {
      DRW_shgroup_call(pd->armature_bone_select_grp, geom, ob);
    }
  }
}

void OVERLAY_armature_cache_finish(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  for (int i = 0; i < 2; i++) {
    if (pd->armature_call_buffers[i].solid.custom_shapes_ghash) {
      /* TODO(fclem): Do not free it for each frame but reuse it. Avoiding alloc cost. */
      BLI_ghash_free(pd->armature_call_buffers[i].solid.custom_shapes_ghash, nullptr, nullptr);
      BLI_ghash_free(pd->armature_call_buffers[i].transp.custom_shapes_ghash, nullptr, nullptr);
    }
  }
}

void OVERLAY_armature_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->armature_transp_ps[0]);
  DRW_draw_pass(psl->armature_ps[0]);
}

void OVERLAY_armature_in_front_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  if (psl->armature_bone_select_ps == nullptr || DRW_state_is_select()) {
    DRW_draw_pass(psl->armature_transp_ps[1]);
    DRW_draw_pass(psl->armature_ps[1]);
  }
}

void OVERLAY_pose_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_FramebufferList *fbl = vedata->fbl;

  if (psl->armature_bone_select_ps != nullptr) {
    if (DRW_state_is_fbo()) {
      GPU_framebuffer_bind(fbl->overlay_default_fb);
    }

    DRW_draw_pass(psl->armature_bone_select_ps);

    if (DRW_state_is_fbo()) {
      GPU_framebuffer_bind(fbl->overlay_line_in_front_fb);
      GPU_framebuffer_clear_depth(fbl->overlay_line_in_front_fb, 1.0f);
    }

    DRW_draw_pass(psl->armature_transp_ps[1]);
    DRW_draw_pass(psl->armature_ps[1]);
  }
}

/** \} */
