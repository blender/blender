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
 */

/** \file
 * \ingroup blenloader
 */
/* allow readfile to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_constraint_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"

#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_gpencil.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLO_readfile.h"
#include "readfile.h"

/* Make preferences read-only, use versioning_userdef.c. */
#define U (*((const UserDef *)&U))

void do_versions_after_linking_290(Main *bmain, ReportList *UNUSED(reports))
{
  if (!MAIN_VERSION_ATLEAST(bmain, 290, 1)) {
    /* Patch old grease pencil modifiers material filter. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {
        switch (md->type) {
          case eGpencilModifierType_Array: {
            ArrayGpencilModifierData *gpmd = (ArrayGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Color: {
            ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Hook: {
            HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Lattice: {
            LatticeGpencilModifierData *gpmd = (LatticeGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Mirror: {
            MirrorGpencilModifierData *gpmd = (MirrorGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Multiply: {
            MultiplyGpencilModifierData *gpmd = (MultiplyGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Noise: {
            NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Offset: {
            OffsetGpencilModifierData *gpmd = (OffsetGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Opacity: {
            OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Simplify: {
            SimplifyGpencilModifierData *gpmd = (SimplifyGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Smooth: {
            SmoothGpencilModifierData *gpmd = (SmoothGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Subdiv: {
            SubdivGpencilModifierData *gpmd = (SubdivGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Texture: {
            TextureGpencilModifierData *gpmd = (TextureGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          case eGpencilModifierType_Thick: {
            ThickGpencilModifierData *gpmd = (ThickGpencilModifierData *)md;
            if (gpmd->materialname[0] != '\0') {
              gpmd->material = BLI_findstring(
                  &bmain->materials, gpmd->materialname, offsetof(ID, name) + 2);
              gpmd->materialname[0] = '\0';
            }
            break;
          }
          default:
            break;
        }
      }
    }

    /* Patch first frame for old files. */
    Scene *scene = bmain->scenes.first;
    if (scene != NULL) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->type != OB_GPENCIL) {
          continue;
        }
        bGPdata *gpd = ob->data;
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          bGPDframe *gpf = gpl->frames.first;
          if (gpf && gpf->framenum > scene->r.sfra) {
            bGPDframe *gpf_dup = BKE_gpencil_frame_duplicate(gpf);
            gpf_dup->framenum = scene->r.sfra;
            BLI_addhead(&gpl->frames, gpf_dup);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 290, 8)) {
    /* Paint Brush. This ensure that the brush paints by default. Used during the development and
     * patch review of the initial Sculpt Vertex Colors implementation (D5975) */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->ob_mode & OB_MODE_SCULPT && brush->sculpt_tool == SCULPT_TOOL_PAINT) {
        brush->tip_roundness = 1.0f;
        brush->flow = 1.0f;
        brush->density = 1.0f;
        brush->tip_scale_x = 1.0f;
      }
    }

    /* Pose Brush with support for loose parts. */
    LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
      if (brush->sculpt_tool == SCULPT_TOOL_POSE && brush->disconnected_distance_max == 0.0f) {
        brush->flag2 |= BRUSH_USE_CONNECTED_ONLY;
        brush->disconnected_distance_max = 0.1f;
      }
    }

    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      if (BKE_collection_cycles_fix(bmain, collection)) {
        printf(
            "WARNING: Cycle detected in collection '%s', fixed as best as possible.\n"
            "You may have to reconstruct your View Layers...\n",
            collection->id.name);
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #blo_do_versions_290 in this file.
   * - "versioning_userdef.c", #BLO_version_defaults_userpref_blend
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}

void blo_do_versions_290(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  UNUSED_VARS(fd);

  /** Repair files from duplicate brushes added to blend files, see: T76738. */
  if (!MAIN_VERSION_ATLEAST(bmain, 290, 2)) {
    {
      short id_codes[] = {ID_BR, ID_PAL};
      for (int i = 0; i < ARRAY_SIZE(id_codes); i++) {
        ListBase *lb = which_libbase(bmain, id_codes[i]);
        BKE_main_id_repair_duplicate_names_listbase(lb);
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SpaceImage", "float", "uv_opacity")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_IMAGE) {
              SpaceImage *sima = (SpaceImage *)sl;
              sima->uv_opacity = 1.0f;
            }
          }
        }
      }
    }

    /* Init Grease Pencil new random curves. */
    if (!DNA_struct_elem_find(fd->filesdna, "BrushGpencilSettings", "float", "random_hue")) {
      LISTBASE_FOREACH (Brush *, brush, &bmain->brushes) {
        if ((brush->gpencil_settings) && (brush->gpencil_settings->curve_rand_pressure == NULL)) {
          brush->gpencil_settings->curve_rand_pressure = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_strength = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_uv = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_hue = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_saturation = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
          brush->gpencil_settings->curve_rand_value = BKE_curvemapping_add(
              1, 0.0f, 0.0f, 1.0f, 1.0f);
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 290, 4)) {
    /* Clear old deprecated bit-flag from edit weights modifiers, we now use it for something else.
     */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type == eModifierType_WeightVGEdit) {
          ((WeightVGEditModifierData *)md)->edit_flags &= ~MOD_WVG_EDIT_WEIGHTS_NORMALIZE;
        }
      }
    }

    /* Initialize parameters of the new Nishita sky model. */
    if (!DNA_struct_elem_find(fd->filesdna, "NodeTexSky", "float", "sun_size")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (node->type == SH_NODE_TEX_SKY && node->storage) {
              NodeTexSky *tex = (NodeTexSky *)node->storage;
              tex->sun_disc = true;
              tex->sun_size = DEG2RADF(0.545);
              tex->sun_elevation = M_PI_2;
              tex->sun_rotation = 0.0f;
              tex->altitude = 0.0f;
              tex->air_density = 1.0f;
              tex->dust_density = 1.0f;
              tex->ozone_density = 1.0f;
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 290, 6)) {
    /* Transition to saving expansion for all of a modifier's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "ModifierData", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->mode & eModifierMode_Expanded_DEPRECATED) {
            md->ui_expand_flag = 1;
          }
          else {
            md->ui_expand_flag = 0;
          }
        }
      }
    }

    /* EEVEE Motion blur new parameters. */
    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "float", "motion_blur_depth_scale")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.motion_blur_depth_scale = 100.0f;
        scene->eevee.motion_blur_max = 32;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "SceneEEVEE", "int", "motion_blur_steps")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        scene->eevee.motion_blur_steps = 1;
      }
    }

    /* Transition to saving expansion for all of a constraint's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "bConstraint", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (bConstraint *, con, &object->constraints) {
          if (con->flag & CONSTRAINT_EXPAND_DEPRECATED) {
            con->ui_expand_flag = 1;
          }
          else {
            con->ui_expand_flag = 0;
          }
        }
      }
    }

    /* Transition to saving expansion for all of grease pencil modifier's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "GpencilModifierData", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (GpencilModifierData *, md, &object->greasepencil_modifiers) {
          if (md->mode & eGpencilModifierMode_Expanded_DEPRECATED) {
            md->ui_expand_flag = 1;
          }
          else {
            md->ui_expand_flag = 0;
          }
        }
      }
    }

    /* Transition to saving expansion for all of an effect's sub-panels. */
    if (!DNA_struct_elem_find(fd->filesdna, "ShaderFxData", "short", "ui_expand_flag")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ShaderFxData *, fx, &object->shader_fx) {
          if (fx->mode & eShaderFxMode_Expanded_DEPRECATED) {
            fx->ui_expand_flag = 1;
          }
          else {
            fx->ui_expand_flag = 0;
          }
        }
      }
    }

    /* Refactor bevel profile type to use an enum. */
    if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "short", "profile_type")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            bool use_custom_profile = bmd->flags & MOD_BEVEL_CUSTOM_PROFILE_DEPRECATED;
            bmd->profile_type = use_custom_profile ? MOD_BEVEL_PROFILE_CUSTOM :
                                                     MOD_BEVEL_PROFILE_SUPERELLIPSE;
          }
        }
      }
    }

    /* Change ocean modifier values from [0, 10] to [0, 1] ranges. */
    for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Ocean) {
          OceanModifierData *omd = (OceanModifierData *)md;
          omd->wave_alignment *= 0.1f;
          omd->sharpen_peak_jonswap *= 0.1f;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 290, 8)) {
    /* Set the cloth wind factor to 1 for old forces. */
    if (!DNA_struct_elem_find(fd->filesdna, "PartDeflect", "float", "f_wind_factor")) {
      LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
        if (ob->pd) {
          ob->pd->f_wind_factor = 1.0f;
        }
      }
      LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
        if (part->pd) {
          part->pd->f_wind_factor = 1.0f;
        }
        if (part->pd2) {
          part->pd2->f_wind_factor = 1.0f;
        }
      }
    }

    for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
      /* Don't rotate light with the viewer by default, make it fixed. Shading settings can't be
       * edited and this flag should always be set. So we can always execute this. */
      wm->xr.session_settings.shading.flag |= V3D_SHADING_WORLD_ORIENTATION;
    }

    /* Initialize additional parameter of the Nishita sky model and change altitude unit. */
    if (!DNA_struct_elem_find(fd->filesdna, "NodeTexSky", "float", "sun_intensity")) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (node->type == SH_NODE_TEX_SKY && node->storage) {
              NodeTexSky *tex = (NodeTexSky *)node->storage;
              tex->sun_intensity = 1.0f;
              tex->altitude *= 0.001f;
            }
          }
        }
      }
      FOREACH_NODETREE_END;
    }

    /* Refactor bevel affect type to use an enum. */
    if (!DNA_struct_elem_find(fd->filesdna, "BevelModifierData", "char", "affect_type")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Bevel) {
            BevelModifierData *bmd = (BevelModifierData *)md;
            const bool use_vertex_bevel = bmd->flags & MOD_BEVEL_VERT_DEPRECATED;
            bmd->affect_type = use_vertex_bevel ? MOD_BEVEL_AFFECT_VERTICES :
                                                  MOD_BEVEL_AFFECT_EDGES;
          }
        }
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - "versioning_userdef.c", #BLO_version_defaults_userpref_blend
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}
