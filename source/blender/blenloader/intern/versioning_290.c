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

#include "BLI_alloca.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_screen_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_workspace_types.h"

#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_gpencil.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_node.h"

#include "MEM_guardedalloc.h"

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

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 1)) {
    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      if (BKE_collection_cycles_fix(bmain, collection)) {
        printf(
            "WARNING: Cycle detected in collection '%s', fixed as best as possible.\n"
            "You may have to reconstruct your View Layers...\n",
            collection->id.name);
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 8)) {
    /**
     * Make sure Emission Alpha fcurve and drivers is properly mapped after the Emission Strength
     * got introduced.
     * */

    /**
     * Effectively we are replacing the (animation of) node socket input 18 with 19.
     * Emission Strength is the new socket input 18, pushing Emission Alpha to input 19.
     *
     * To play safe we move all the inputs beyond 18 to their rightful new place.
     * In case users are doing unexpected things with not-really supported keyframeable channels.
     *
     * The for loop for the input ids is at the top level otherwise we lose the animation
     * keyframe data.
     * */
    for (int input_id = 21; input_id >= 18; input_id--) {
      FOREACH_NODETREE_BEGIN (bmain, ntree, id) {
        if (ntree->type == NTREE_SHADER) {
          LISTBASE_FOREACH (bNode *, node, &ntree->nodes) {
            if (node->type != SH_NODE_BSDF_PRINCIPLED) {
              continue;
            }

            const size_t node_name_length = strlen(node->name);
            const size_t node_name_escaped_max_length = (node_name_length * 2);
            char *node_name_escaped = MEM_mallocN(node_name_escaped_max_length + 1,
                                                  "escaped name");
            BLI_strescape(node_name_escaped, node->name, node_name_escaped_max_length);
            char *rna_path_prefix = BLI_sprintfN("nodes[\"%s\"].inputs", node_name_escaped);

            BKE_animdata_fix_paths_rename_all_ex(
                bmain, id, rna_path_prefix, NULL, NULL, input_id, input_id + 1, false);
            MEM_freeN(rna_path_prefix);
            MEM_freeN(node_name_escaped);
          }
        }
      }
      FOREACH_NODETREE_END;
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - #blo_do_versions_290 in this file.
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {

    /* Keep this block, even when empty. */
  }
}

static void panels_remove_x_closed_flag_recursive(Panel *panel)
{
  const bool was_closed_x = panel->flag & PNL_UNUSED_1;
  const bool was_closed_y = panel->flag & PNL_CLOSED; /* That value was the Y closed flag. */

  SET_FLAG_FROM_TEST(panel->flag, was_closed_x || was_closed_y, PNL_CLOSED);

  /* Clear the old PNL_CLOSEDX flag. */
  panel->flag &= ~PNL_UNUSED_1;

  LISTBASE_FOREACH (Panel *, child_panel, &panel->children) {
    panels_remove_x_closed_flag_recursive(child_panel);
  }
}

static void do_versions_point_attributes(CustomData *pdata)
{
  /* Change to generic named float/float3 attributes. */
  const int CD_LOCATION = 43;
  const int CD_RADIUS = 44;

  for (int i = 0; i < pdata->totlayer; i++) {
    CustomDataLayer *layer = &pdata->layers[i];
    if (layer->type == CD_LOCATION) {
      STRNCPY(layer->name, "Position");
      layer->type = CD_PROP_FLOAT3;
    }
    else if (layer->type == CD_RADIUS) {
      STRNCPY(layer->name, "Radius");
      layer->type = CD_PROP_FLOAT;
    }
  }
}

/* Move FCurve handles towards the control point in such a way that the curve itself doesn't
 * change. Since 2.91 FCurves are computed slightly differently, which requires this update to keep
 * the same animation result. Previous versions scaled down overlapping handles during evaluation.
 * This function applies the old correction to the actual animation data instead. */
static void do_versions_291_fcurve_handles_limit(FCurve *fcu)
{
  uint i = 1;
  for (BezTriple *bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    /* Only adjust bezier keyframes. */
    if (bezt->ipo != BEZT_IPO_BEZ) {
      continue;
    }

    BezTriple *nextbezt = bezt + 1;
    const float v1[2] = {bezt->vec[1][0], bezt->vec[1][1]};
    const float v2[2] = {bezt->vec[2][0], bezt->vec[2][1]};
    const float v3[2] = {nextbezt->vec[0][0], nextbezt->vec[0][1]};
    const float v4[2] = {nextbezt->vec[1][0], nextbezt->vec[1][1]};

    /* If the handles have no length, no need to do any corrections. */
    if (v1[0] == v2[0] && v3[0] == v4[0]) {
      continue;
    }

    /* Calculate handle deltas. */
    float delta1[2], delta2[2];
    sub_v2_v2v2(delta1, v1, v2);
    sub_v2_v2v2(delta2, v4, v3);

    const float len1 = fabsf(delta1[0]); /* Length of handle of first key. */
    const float len2 = fabsf(delta2[0]); /* Length of handle of second key. */

    /* Overlapping handles used to be internally scaled down in previous versions.
     * We bake the handles onto these previously virtual values. */
    const float time_delta = v4[0] - v1[0];
    const float total_len = len1 + len2;
    if (total_len <= time_delta) {
      continue;
    }

    const float factor = time_delta / total_len;
    /* Current keyframe's right handle: */
    madd_v2_v2v2fl(bezt->vec[2], v1, delta1, -factor); /* vec[2] = v1 - factor * delta1 */
    /* Next keyframe's left handle: */
    madd_v2_v2v2fl(nextbezt->vec[0], v4, delta2, -factor); /* vec[0] = v4 - factor * delta2 */
  }
}

void blo_do_versions_290(FileData *fd, Library *UNUSED(lib), Main *bmain)
{
  UNUSED_VARS(fd);

  if (MAIN_VERSION_ATLEAST(bmain, 290, 2) && MAIN_VERSION_OLDER(bmain, 291, 1)) {
    /* In this range, the extrude manifold could generate meshes with degenerated face. */
    LISTBASE_FOREACH (Mesh *, me, &bmain->meshes) {
      for (MPoly *mp = me->mpoly, *mp_end = mp + me->totpoly; mp < mp_end; mp++) {
        if (mp->totloop == 2) {
          bool changed;
          BKE_mesh_validate_arrays(me,
                                   me->mvert,
                                   me->totvert,
                                   me->medge,
                                   me->totedge,
                                   me->mface,
                                   me->totface,
                                   me->mloop,
                                   me->totloop,
                                   me->mpoly,
                                   me->totpoly,
                                   me->dvert,
                                   false,
                                   true,
                                   &changed);
          break;
        }
      }
    }
  }

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

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 1)) {

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

    /* Initialize additional velocity parameter for #CacheFile's. */
    if (!DNA_struct_elem_find(
            fd->filesdna, "MeshSeqCacheModifierData", "float", "velocity_scale")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_MeshSequenceCache) {
            MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)md;
            mcmd->velocity_scale = 1.0f;
            mcmd->vertex_velocities = NULL;
            mcmd->num_vertices = 0;
          }
        }
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "CacheFile", "char", "velocity_unit")) {
      for (CacheFile *cache_file = bmain->cachefiles.first; cache_file != NULL;
           cache_file = cache_file->id.next) {
        BLI_strncpy(cache_file->velocity_name, ".velocities", sizeof(cache_file->velocity_name));
        cache_file->velocity_unit = CACHEFILE_VELOCITY_UNIT_SECOND;
      }
    }

    if (!DNA_struct_elem_find(fd->filesdna, "OceanModifierData", "int", "viewport_resolution")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Ocean) {
            OceanModifierData *omd = (OceanModifierData *)md;
            omd->viewport_resolution = omd->resolution;
          }
        }
      }
    }

    /* Remove panel X axis collapsing, a remnant of horizontal panel alignment. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          LISTBASE_FOREACH (Panel *, panel, &region->panels) {
            panels_remove_x_closed_flag_recursive(panel);
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 2)) {
    for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
      RigidBodyWorld *rbw = scene->rigidbody_world;

      if (rbw == NULL) {
        continue;
      }

      /* The substep method changed from "per second" to "per frame".
       * To get the new value simply divide the old bullet sim fps with the scene fps.
       */
      rbw->substeps_per_frame /= FPS;

      if (rbw->substeps_per_frame <= 0) {
        rbw->substeps_per_frame = 1;
      }
    }

    /* Set the minimum sequence interpolate for grease pencil. */
    if (!DNA_struct_elem_find(fd->filesdna, "GP_Interpolate_Settings", "int", "step")) {
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        ToolSettings *ts = scene->toolsettings;
        ts->gp_interpolate.step = 1;
      }
    }

    /* Hair and PointCloud attributes. */
    for (Hair *hair = bmain->hairs.first; hair != NULL; hair = hair->id.next) {
      do_versions_point_attributes(&hair->pdata);
    }
    for (PointCloud *pointcloud = bmain->pointclouds.first; pointcloud != NULL;
         pointcloud = pointcloud->id.next) {
      do_versions_point_attributes(&pointcloud->pdata);
    }

    /* Show outliner mode column by default. */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
          if (space->spacetype == SPACE_OUTLINER) {
            SpaceOutliner *space_outliner = (SpaceOutliner *)space;

            space_outliner->flag |= SO_MODE_COLUMN;
          }
        }
      }
    }

    /* Solver and Collections for Boolean. */
    if (!DNA_struct_elem_find(fd->filesdna, "BooleanModifierData", "char", "solver")) {
      for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
        LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
          if (md->type == eModifierType_Boolean) {
            BooleanModifierData *bmd = (BooleanModifierData *)md;
            bmd->solver = eBooleanModifierSolver_Fast;
            bmd->flag = eBooleanModifierFlag_Object;
          }
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 4) && MAIN_VERSION_ATLEAST(bmain, 291, 1)) {
    /* Due to a48d78ce07f4f, CustomData.totlayer and CustomData.maxlayer has been written
     * incorrectly. Fortunately, the size of the layers array has been written to the .blend file
     * as well, so we can reconstruct totlayer and maxlayer from that. */
    LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
      mesh->vdata.totlayer = mesh->vdata.maxlayer = MEM_allocN_len(mesh->vdata.layers) /
                                                    sizeof(CustomDataLayer);
      mesh->edata.totlayer = mesh->edata.maxlayer = MEM_allocN_len(mesh->edata.layers) /
                                                    sizeof(CustomDataLayer);
      /* We can be sure that mesh->fdata is empty for files written by 2.90. */
      mesh->ldata.totlayer = mesh->ldata.maxlayer = MEM_allocN_len(mesh->ldata.layers) /
                                                    sizeof(CustomDataLayer);
      mesh->pdata.totlayer = mesh->pdata.maxlayer = MEM_allocN_len(mesh->pdata.layers) /
                                                    sizeof(CustomDataLayer);
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 5)) {
    /* Fix fcurves to allow for new bezier handles behaviour (T75881 and D8752). */
    for (bAction *act = bmain->actions.first; act; act = act->id.next) {
      for (FCurve *fcu = act->curves.first; fcu; fcu = fcu->next) {
        /* Only need to fix Bezier curves with at least 2 keyframes. */
        if (fcu->totvert < 2 || fcu->bezt == NULL) {
          continue;
        }
        do_versions_291_fcurve_handles_limit(fcu);
      }
    }

    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      collection->color_tag = COLLECTION_COLOR_NONE;
    }
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      /* Old files do not have a master collection, but it will be created by
       * `BKE_collection_master_add()`. */
      if (scene->master_collection) {
        scene->master_collection->color_tag = COLLECTION_COLOR_NONE;
      }
    }

    /* Add custom profile and bevel mode to curve bevels. */
    if (!DNA_struct_elem_find(fd->filesdna, "Curve", "char", "bevel_mode")) {
      LISTBASE_FOREACH (Curve *, curve, &bmain->curves) {
        if (curve->bevobj != NULL) {
          curve->bevel_mode = CU_BEV_MODE_OBJECT;
        }
        else {
          curve->bevel_mode = CU_BEV_MODE_ROUND;
        }
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 6)) {
    /* Darken Inactive Overlay. */
    if (!DNA_struct_elem_find(fd->filesdna, "View3DOverlay", "float", "fade_alpha")) {
      for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
            if (sl->spacetype == SPACE_VIEW3D) {
              View3D *v3d = (View3D *)sl;
              v3d->overlay.fade_alpha = 0.40f;
              v3d->overlay.flag |= V3D_OVERLAY_FADE_INACTIVE;
            }
          }
        }
      }
    }

    /* Unify symmetry as a mesh property. */
    if (!DNA_struct_elem_find(fd->filesdna, "Mesh", "char", "symmetry")) {
      LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
        /* The previous flags used to store mesh symmetry in edit-mode match the new ones that are
         * used in #Mesh.symmetry. */
        mesh->symmetry = mesh->editflag & (ME_SYMMETRY_X | ME_SYMMETRY_Y | ME_SYMMETRY_Z);
      }
    }

    /* Alembic importer: allow vertex interpolation by default. */
    for (Object *object = bmain->objects.first; object != NULL; object = object->id.next) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type != eModifierType_MeshSequenceCache) {
          continue;
        }

        MeshSeqCacheModifierData *data = (MeshSeqCacheModifierData *)md;
        data->read_flag |= MOD_MESHSEQ_INTERPOLATE_VERTICES;
      }
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 7)) {
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      scene->r.simplify_volumes = 1.0f;
    }
  }

  if (!MAIN_VERSION_ATLEAST(bmain, 291, 8)) {
    if (!DNA_struct_elem_find(fd->filesdna, "WorkSpaceDataRelation", "int", "parentid")) {
      LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
        LISTBASE_FOREACH_MUTABLE (
            WorkSpaceDataRelation *, relation, &workspace->hook_layout_relations) {
          relation->parent = blo_read_get_new_globaldata_address(fd, relation->parent);
          BLI_assert(relation->parentid == 0);
          if (relation->parent != NULL) {
            LISTBASE_FOREACH (wmWindowManager *, wm, &bmain->wm) {
              wmWindow *win = BLI_findptr(
                  &wm->windows, relation->parent, offsetof(wmWindow, workspace_hook));
              if (win != NULL) {
                relation->parentid = win->winid;
                break;
              }
            }
            if (relation->parentid == 0) {
              BLI_assert(
                  !"Found a valid parent for workspace data relation, but no valid parent id.");
            }
          }
          if (relation->parentid == 0) {
            BLI_freelinkN(&workspace->hook_layout_relations, relation);
          }
        }
      }
    }

    /* UV/Image show overlay option. */
    if (!DNA_struct_find(fd->filesdna, "SpaceImageOverlay")) {
      LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          LISTBASE_FOREACH (SpaceLink *, space, &area->spacedata) {
            if (space->spacetype == SPACE_IMAGE) {
              SpaceImage *sima = (SpaceImage *)space;
              sima->overlay.flag = SI_OVERLAY_SHOW_OVERLAYS;
            }
          }
        }
      }
    }

    /* Ensure that particle systems generated by fluid modifier have correct phystype. */
    LISTBASE_FOREACH (ParticleSettings *, part, &bmain->particles) {
      if (ELEM(
              part->type, PART_FLUID_FLIP, PART_FLUID_SPRAY, PART_FLUID_BUBBLE, PART_FLUID_FOAM)) {
        part->phystype = PART_PHYS_NO;
      }
    }
  }

  /**
   * Versioning code until next subversion bump goes here.
   *
   * \note Be sure to check when bumping the version:
   * - "versioning_userdef.c", #blo_do_versions_userdef
   * - "versioning_userdef.c", #do_versions_theme
   *
   * \note Keep this message at the bottom of the function.
   */
  {
    /* Keep this block, even when empty. */
  }
}
