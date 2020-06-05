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
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_genfile.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"

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
  }
}
