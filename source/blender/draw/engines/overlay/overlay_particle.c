/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "DEG_depsgraph_query.h"

#include "DNA_particle_types.h"

#include "BKE_pointcache.h"

#include "ED_particle.h"

#include "overlay_private.h"

/* -------------------------------------------------------------------- */
/** \name Edit Particles
 * \{ */

void OVERLAY_edit_particle_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ParticleEditSettings *pset = PE_settings(draw_ctx->scene);
  GPUShader *sh;
  DRWShadingGroup *grp;

  pd->edit_particle.use_weight = (pset->brushtype == PE_BRUSH_WEIGHT);
  pd->edit_particle.select_mode = pset->selectmode;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
  DRW_PASS_CREATE(psl->edit_particle_ps, state | pd->clipping_state);

  sh = OVERLAY_shader_edit_particle_strand();
  pd->edit_particle_strand_grp = grp = DRW_shgroup_create(sh, psl->edit_particle_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_bool_copy(grp, "useWeight", pd->edit_particle.use_weight);
  DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.weight_ramp);

  sh = OVERLAY_shader_edit_particle_point();
  pd->edit_particle_point_grp = grp = DRW_shgroup_create(sh, psl->edit_particle_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
}

void OVERLAY_edit_particle_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene_orig = (Scene *)DEG_get_original_id(&draw_ctx->scene->id);

  /* Usually the edit structure is created by Particle Edit Mode Toggle
   * operator, but sometimes it's invoked after tagging hair as outdated
   * (for example, when toggling edit mode). That makes it impossible to
   * create edit structure for until after next dependency graph evaluation.
   *
   * Ideally, the edit structure will be created here already via some
   * dependency graph callback or so, but currently trying to make it nicer
   * only causes bad level calls and breaks design from the past.
   */
  Object *ob_orig = DEG_get_original_object(ob);
  PTCacheEdit *edit = PE_create_current(draw_ctx->depsgraph, scene_orig, ob_orig);
  if (edit == NULL) {
    /* Happens when trying to edit particles in EMITTER mode without
     * having them cached.
     */
    return;
  }
  /* NOTE: We need to pass evaluated particle system, which we need
   * to find first.
   */
  ParticleSystem *psys = ob->particlesystem.first;
  LISTBASE_FOREACH (ParticleSystem *, psys_orig, &ob_orig->particlesystem) {
    if (PE_get_current_from_psys(psys_orig) == edit) {
      break;
    }
    psys = psys->next;
  }
  if (psys == NULL) {
    printf("Error getting evaluated particle system for edit.\n");
    return;
  }

  struct GPUBatch *geom;
  {
    geom = DRW_cache_particles_get_edit_strands(ob, psys, edit, pd->edit_particle.use_weight);
    DRW_shgroup_call(pd->edit_particle_strand_grp, geom, NULL);
  }

  if (pd->edit_particle.select_mode == SCE_SELECT_POINT) {
    geom = DRW_cache_particles_get_edit_inner_points(ob, psys, edit);
    DRW_shgroup_call(pd->edit_particle_point_grp, geom, NULL);
  }

  if (ELEM(pd->edit_particle.select_mode, SCE_SELECT_POINT, SCE_SELECT_END)) {
    geom = DRW_cache_particles_get_edit_tip_points(ob, psys, edit);
    DRW_shgroup_call(pd->edit_particle_point_grp, geom, NULL);
  }
}

void OVERLAY_edit_particle_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_FramebufferList *fbl = vedata->fbl;

  if (DRW_state_is_fbo()) {
    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }

  DRW_draw_pass(psl->edit_particle_ps);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particles
 * \{ */

void OVERLAY_particle_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ParticleEditSettings *pset = PE_settings(draw_ctx->scene);
  GPUShader *sh;
  DRWShadingGroup *grp;

  pd->edit_particle.use_weight = (pset->brushtype == PE_BRUSH_WEIGHT);
  pd->edit_particle.select_mode = pset->selectmode;

  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
  DRW_PASS_CREATE(psl->particle_ps, state | pd->clipping_state);

  sh = OVERLAY_shader_particle_dot();
  pd->particle_dots_grp = grp = DRW_shgroup_create(sh, psl->particle_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.ramp);

  sh = OVERLAY_shader_particle_shape();
  pd->particle_shapes_grp = grp = DRW_shgroup_create(sh, psl->particle_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
  DRW_shgroup_uniform_texture(grp, "weightTex", G_draw.ramp);
}

void OVERLAY_particle_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
    if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
      continue;
    }

    ParticleSettings *part = psys->part;
    int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

    if (part->type == PART_HAIR) {
      /* Hairs should have been rendered by the render engine.*/
      continue;
    }

    if (!ELEM(draw_as, PART_DRAW_NOT, PART_DRAW_OB, PART_DRAW_GR)) {
      struct GPUBatch *geom = DRW_cache_particles_get_dots(ob, psys);
      struct GPUBatch *shape = NULL;
      DRWShadingGroup *grp;

      /* TODO(fclem) Here would be a good place for preemptive culling. */

      /* fclem: Is color even usefull in our modern context? */
      Material *ma = BKE_object_material_get(ob, part->omat);
      float color[4] = {0.6f, 0.6f, 0.6f, part->draw_size};
      if (ma != NULL) {
        copy_v3_v3(color, &ma->r);
      }

      switch (draw_as) {
        default:
        case PART_DRAW_DOT:
          grp = DRW_shgroup_create_sub(pd->particle_dots_grp);
          DRW_shgroup_uniform_vec4_copy(grp, "color", color);
          DRW_shgroup_call(grp, geom, NULL);
          break;
        case PART_DRAW_AXIS:
        case PART_DRAW_CIRC:
        case PART_DRAW_CROSS:
          grp = DRW_shgroup_create_sub(pd->particle_shapes_grp);
          DRW_shgroup_uniform_vec4_copy(grp, "color", color);
          shape = DRW_cache_particles_get_prim(draw_as);
          DRW_shgroup_call_instances_with_attrs(grp, NULL, shape, geom);
          break;
      }
    }
  }
}

void OVERLAY_particle_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->particle_ps);
}

/** \} */
