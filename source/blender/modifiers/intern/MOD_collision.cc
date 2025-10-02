/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_kdopbvh.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_collision.h"
#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_prototypes.hh"

#include "MOD_ui_common.hh"

#include "DEG_depsgraph_query.hh"

static void init_data(ModifierData *md)
{
  CollisionModifierData *collmd = (CollisionModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(collmd, modifier));

  MEMCPY_STRUCT_AFTER(collmd, DNA_struct_default_get(CollisionModifierData), modifier);
}

static void free_data(ModifierData *md)
{
  CollisionModifierData *collmd = (CollisionModifierData *)md;

  if (collmd) { /* Seriously? */
    if (collmd->bvhtree) {
      BLI_bvhtree_free(collmd->bvhtree);
      collmd->bvhtree = nullptr;
    }

    MEM_SAFE_FREE(collmd->x);
    MEM_SAFE_FREE(collmd->xnew);
    MEM_SAFE_FREE(collmd->current_x);
    MEM_SAFE_FREE(collmd->current_xnew);
    MEM_SAFE_FREE(collmd->current_v);

    MEM_SAFE_FREE(collmd->vert_tris);

    collmd->time_x = collmd->time_xnew = -1000;
    collmd->mvert_num = 0;
    collmd->tri_num = 0;
    collmd->is_static = false;
  }
}

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static void deform_verts(ModifierData *md,
                         const ModifierEvalContext *ctx,
                         Mesh *mesh,
                         blender::MutableSpan<blender::float3> positions)
{
  CollisionModifierData *collmd = (CollisionModifierData *)md;
  Object *ob = ctx->object;

  /* If collision is disabled, free the stale data and exit. */
  if (!ob->pd || !ob->pd->deflect) {
    if (!ob->pd) {
      printf("CollisionModifier: collision settings are missing!\n");
    }

    free_data(md);
    return;
  }

  if (mesh) {
    float current_time = 0;
    int mvert_num = 0;

    mesh->vert_positions_for_write().copy_from(positions);
    mesh->tag_positions_changed();

    current_time = DEG_get_ctime(ctx->depsgraph);

    if (G.debug & G_DEBUG_SIMDATA) {
      printf("current_time %f, collmd->time_xnew %f\n", current_time, collmd->time_xnew);
    }

    mvert_num = mesh->verts_num;

    if (current_time < collmd->time_xnew) {
      free_data((ModifierData *)collmd);
    }
    else if (current_time == collmd->time_xnew) {
      if (mvert_num != collmd->mvert_num) {
        free_data((ModifierData *)collmd);
      }
    }

    /* check if mesh has changed */
    if (collmd->x && (mvert_num != collmd->mvert_num)) {
      free_data((ModifierData *)collmd);
    }

    if (collmd->time_xnew == -1000) { /* first time */

      mvert_num = mesh->verts_num;
      collmd->x = MEM_malloc_arrayN<float[3]>(size_t(mvert_num), __func__);
      blender::MutableSpan(reinterpret_cast<blender::float3 *>(collmd->x), mvert_num)
          .copy_from(mesh->vert_positions());

      for (uint i = 0; i < mvert_num; i++) {
        /* we save global positions */
        mul_m4_v3(ob->object_to_world().ptr(), collmd->x[i]);
      }

      collmd->xnew = static_cast<float (*)[3]>(MEM_dupallocN(collmd->x)); /* Frame end position. */
      collmd->current_x = static_cast<float (*)[3]>(MEM_dupallocN(collmd->x)); /* Inter-frame. */
      collmd->current_xnew = static_cast<float (*)[3]>(
          MEM_dupallocN(collmd->x));                                           /* Inter-frame. */
      collmd->current_v = static_cast<float (*)[3]>(MEM_dupallocN(collmd->x)); /* Inter-frame. */

      collmd->mvert_num = mvert_num;

      {
        const blender::Span<blender::int3> corner_tris = mesh->corner_tris();
        collmd->tri_num = corner_tris.size();
        int (*vert_tris)[3] = MEM_malloc_arrayN<int[3]>(collmd->tri_num, __func__);
        blender::bke::mesh::vert_tris_from_corner_tris(
            mesh->corner_verts(),
            corner_tris,
            {reinterpret_cast<blender::int3 *>(vert_tris), collmd->tri_num});
        collmd->vert_tris = vert_tris;
      }

      /* create bounding box hierarchy */
      collmd->bvhtree = bvhtree_build_from_mvert(
          collmd->x,
          reinterpret_cast<blender::int3 *>(collmd->vert_tris),
          collmd->tri_num,
          ob->pd->pdef_sboft);

      collmd->time_x = collmd->time_xnew = current_time;
      collmd->is_static = true;
    }
    else if (mvert_num == collmd->mvert_num) {
      /* put positions to old positions */
      float (*temp)[3] = collmd->x;
      collmd->x = collmd->xnew;
      collmd->xnew = temp;
      collmd->time_x = collmd->time_xnew;

      memcpy(collmd->xnew, mesh->vert_positions().data(), mvert_num * sizeof(float[3]));

      bool is_static = true;

      for (uint i = 0; i < mvert_num; i++) {
        /* we save global positions */
        mul_m4_v3(ob->object_to_world().ptr(), collmd->xnew[i]);

        /* detect motion */
        is_static = is_static && equals_v3v3(collmd->x[i], collmd->xnew[i]);
      }

      memcpy(collmd->current_xnew, collmd->x, mvert_num * sizeof(float[3]));
      memcpy(collmd->current_x, collmd->x, mvert_num * sizeof(float[3]));

      /* check if GUI setting has changed for bvh */
      if (collmd->bvhtree) {
        if (ob->pd->pdef_sboft != BLI_bvhtree_get_epsilon(collmd->bvhtree)) {
          BLI_bvhtree_free(collmd->bvhtree);
          collmd->bvhtree = bvhtree_build_from_mvert(
              collmd->current_x,
              reinterpret_cast<const blender::int3 *>(collmd->vert_tris),
              collmd->tri_num,
              ob->pd->pdef_sboft);
        }
      }

      /* Happens on file load (ONLY when I un-comment changes in `readfile.cc`). */
      if (!collmd->bvhtree) {
        collmd->bvhtree = bvhtree_build_from_mvert(
            collmd->current_x,
            reinterpret_cast<const blender::int3 *>(collmd->vert_tris),
            collmd->tri_num,
            ob->pd->pdef_sboft);
      }
      else if (!collmd->is_static || !is_static) {
        /* recalc static bounding boxes */
        bvhtree_update_from_mvert(collmd->bvhtree,
                                  collmd->current_x,
                                  collmd->current_xnew,
                                  reinterpret_cast<const blender::int3 *>(collmd->vert_tris),
                                  collmd->tri_num,
                                  true);
      }

      collmd->is_static = is_static;
      collmd->time_xnew = current_time;
    }
    else if (mvert_num != collmd->mvert_num) {
      free_data((ModifierData *)collmd);
    }
  }
}

static void update_depsgraph(ModifierData * /*md*/, const ModifierUpdateDepsgraphContext *ctx)
{
  DEG_add_depends_on_transform_relation(ctx->node, "Collision Modifier");
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->label(RPT_("Settings are inside the Physics tab"), ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Collision, panel_draw);
}

static void blend_read(BlendDataReader * /*reader*/, ModifierData *md)
{
  CollisionModifierData *collmd = (CollisionModifierData *)md;
#if 0
  /* TODO: #CollisionModifier should use point-cache
   * + have proper reset events before enabling this. */
  collmd->x = newdataadr(fd, collmd->x);
  collmd->xnew = newdataadr(fd, collmd->xnew);
  collmd->mfaces = newdataadr(fd, collmd->mfaces);

  collmd->current_x = MEM_calloc_arrayN<float[3]>(collmd->mvert_num, "current_x");
  collmd->current_xnew = MEM_calloc_arrayN<float[3]>(collmd->mvert_num, "current_xnew");
  collmd->current_v = MEM_calloc_arrayN<float[3]>(collmd->mvert_num, "current_v");
#endif

  collmd->x = nullptr;
  collmd->xnew = nullptr;
  collmd->current_x = nullptr;
  collmd->current_xnew = nullptr;
  collmd->current_v = nullptr;
  collmd->time_x = collmd->time_xnew = -1000;
  collmd->mvert_num = 0;
  collmd->tri_num = 0;
  collmd->is_static = false;
  collmd->bvhtree = nullptr;
  collmd->vert_tris = nullptr;
}

ModifierTypeInfo modifierType_Collision = {
    /*idname*/ "Collision",
    /*name*/ N_("Collision"),
    /*struct_name*/ "CollisionModifierData",
    /*struct_size*/ sizeof(CollisionModifierData),
    /*srna*/ &RNA_CollisionModifier,
    /*type*/ ModifierTypeType::OnlyDeform,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_Single,
    /*icon*/ ICON_MOD_PHYSICS,

    /*copy_data*/ nullptr,

    /*deform_verts*/ deform_verts,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ blend_read,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
