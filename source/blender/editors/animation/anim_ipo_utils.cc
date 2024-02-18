/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

/* This file contains code for presenting F-Curves and other animation data
 * in the UI (especially for use in the Animation Editors).
 *
 * -- Joshua Leung, Dec 2008
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.h"

#include "ED_anim_api.hh"

#include <cstring>

/* ----------------------- Getter functions ----------------------- */

int getname_anim_fcurve(char *name, ID *id, FCurve *fcu)
{
  /* Could make an argument, it's a documented limit at the moment. */
  const size_t name_maxncpy = 256;

  int icon = 0;

  /* sanity checks */
  if (name == nullptr) {
    return icon;
  }

  if (ELEM(nullptr, id, fcu, fcu->rna_path)) {
    if (fcu == nullptr) {
      BLI_strncpy(name, RPT_("<invalid>"), name_maxncpy);
    }
    else if (fcu->rna_path == nullptr) {
      BLI_strncpy(name, RPT_("<no path>"), name_maxncpy);
    }
    else { /* id == nullptr */
      BLI_snprintf(name, name_maxncpy, "%s[%d]", fcu->rna_path, fcu->array_index);
    }
  }
  else {
    PointerRNA ptr;
    PropertyRNA *prop;

    /* get RNA pointer, and resolve the path */
    PointerRNA id_ptr = RNA_id_pointer_create(id);

    /* try to resolve the path */
    if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
      const char *structname = nullptr, *propname = nullptr;
      char arrayindbuf[16];
      const char *arrayname = nullptr;
      short free_structname = 0;

      /* For now, name will consist of 3 parts: struct-name, property name, array index
       * There are several options possible:
       * 1) <struct-name>.<property-name>.<array-index>
       *     i.e. Bone1.Location.X, or Object.Location.X
       * 2) <array-index> <property-name> (<struct name>)
       *     i.e. X Location (Bone1), or X Location (Object)
       *
       * Currently, option 2 is in use, to try and make it easier to quickly identify F-Curves
       * (it does have problems with looking rather odd though).
       * Option 1 is better in terms of revealing a consistent sense of hierarchy though,
       * which isn't so clear with option 2.
       */

      /* For struct-name:
       * - As base, we use a custom name from the structs if one is available
       * - However, if we're showing sub-data of bones
       *   (probably there will be other exceptions later).
       *   need to include that info too since it gets confusing otherwise.
       * - If a pointer just refers to the ID-block, then don't repeat this info
       *   since this just introduces clutter.
       */

      char pchanName[256], constName[256];
      if (BLI_str_quoted_substr(fcu->rna_path, "bones[", pchanName, sizeof(pchanName)) &&
          BLI_str_quoted_substr(fcu->rna_path, "constraints[", constName, sizeof(constName)))
      {

        /* assemble the string to display in the UI... */
        structname = BLI_sprintfN("%s : %s", pchanName, constName);
        free_structname = 1;
      }
      else if (ptr.data != ptr.owner_id) {
        PropertyRNA *nameprop = RNA_struct_name_property(ptr.type);
        if (nameprop) {
          /* this gets a string which will need to be freed */
          structname = RNA_property_string_get_alloc(&ptr, nameprop, nullptr, 0, nullptr);
          free_structname = 1;
        }
        else {
          structname = RNA_struct_ui_name(ptr.type);
        }

        /* For the sequencer, a strip's 'Transform' or 'Crop' is a nested (under Sequence)
         * struct, but displaying the struct name alone is no meaningful information
         * (and also cannot be filtered well), same for modifiers.
         * So display strip name alongside as well. */
        if (GS(ptr.owner_id->name) == ID_SCE) {
          char stripname[256];
          if (BLI_str_quoted_substr(
                  fcu->rna_path, "sequence_editor.sequences_all[", stripname, sizeof(stripname)))
          {
            if (strstr(fcu->rna_path, ".transform.") || strstr(fcu->rna_path, ".crop.") ||
                strstr(fcu->rna_path, ".modifiers["))
            {
              const char *structname_all = BLI_sprintfN("%s : %s", stripname, structname);
              if (free_structname) {
                MEM_freeN((void *)structname);
              }
              structname = structname_all;
              free_structname = 1;
            }
          }
        }
        /* For node sockets, it is useful to include the node name as well (multiple similar nodes
         * are not distinguishable otherwise). Unfortunately, the node label cannot be retrieved
         * from the rna path, for this to work access to the underlying node is needed (but finding
         * the node iterates all nodes & sockets which would result in bad performance in some
         * circumstances). */
        if (RNA_struct_is_a(ptr.type, &RNA_NodeSocket)) {
          char nodename[256];
          if (BLI_str_quoted_substr(fcu->rna_path, "nodes[", nodename, sizeof(nodename))) {
            const char *structname_all = BLI_sprintfN("%s : %s", nodename, structname);
            if (free_structname) {
              MEM_freeN((void *)structname);
            }
            structname = structname_all;
            free_structname = 1;
          }
        }
      }

      /* Property Name is straightforward */
      propname = RNA_property_ui_name(prop);

      /* Array Index - only if applicable */
      if (RNA_property_array_check(prop)) {
        char c = RNA_property_array_item_char(prop, fcu->array_index);

        /* we need to write the index to a temp buffer (in py syntax) */
        if (c) {
          SNPRINTF(arrayindbuf, "%c ", c);
        }
        else {
          SNPRINTF(arrayindbuf, "[%d]", fcu->array_index);
        }

        arrayname = &arrayindbuf[0];
      }
      else {
        /* no array index */
        arrayname = "";
      }

      /* putting this all together into the buffer */
      /* XXX we need to check for invalid names...
       * XXX the name length limit needs to be passed in or as some define */
      if (structname) {
        BLI_snprintf(name, name_maxncpy, "%s%s (%s)", arrayname, propname, structname);
      }
      else {
        BLI_snprintf(name, name_maxncpy, "%s%s", arrayname, propname);
      }

      /* free temp name if nameprop is set */
      if (free_structname) {
        MEM_freeN((void *)structname);
      }

      /* Icon for this property's owner:
       * use the struct's icon if it is set
       */
      icon = RNA_struct_ui_icon(ptr.type);

      /* valid path - remove the invalid tag since we now know how to use it saving
       * users manual effort to re-enable using "Revive Disabled FCurves" #29629. */
      fcu->flag &= ~FCURVE_DISABLED;
    }
    else {
      /* invalid path */
      BLI_snprintf(name, name_maxncpy, "\"%s[%d]\"", fcu->rna_path, fcu->array_index);

      /* icon for this should be the icon for the base ID */
      /* TODO: or should we just use the error icon? */
      icon = RNA_struct_ui_icon(id_ptr.type);

      /* tag F-Curve as disabled - as not usable path */
      fcu->flag |= FCURVE_DISABLED;
    }
  }

  /* return the icon that the active data had */
  return icon;
}

/* ------------------------------- Color Codes for F-Curve Channels ---------------------------- */

/* step between the major distinguishable color bands of the primary colors */
#define HSV_BANDWIDTH 0.3f

/* used to determine the color of F-Curves with FCURVE_COLOR_AUTO_RAINBOW set */
// void fcurve_rainbow(uint cur, uint tot, float *out)
void getcolor_fcurve_rainbow(int cur, int tot, float out[3])
{
  float hsv[3], fac;
  int grouping;

  /* we try to divide the color into groupings of n colors,
   * where n is:
   * 3 - for 'odd' numbers of curves - there should be a majority of triplets of curves
   * 4 - for 'even' numbers of curves - there should be a majority of quartets of curves
   * so the base color is simply one of the three primary colors
   */
  grouping = (4 - (tot % 2));
  hsv[0] = HSV_BANDWIDTH * float(cur % grouping);

  /* 'Value' (i.e. darkness) needs to vary so that larger sets of three will be
   * 'darker' (i.e. smaller value), so that they don't look that similar to previous ones.
   * However, only a range of 0.3 to 1.0 is really usable to avoid clashing
   * with some other stuff
   */
  fac = (float(cur) / float(tot)) * 0.7f;

  /* the base color can get offset a bit so that the colors aren't so identical */
  hsv[0] += fac * HSV_BANDWIDTH;
  if (hsv[0] > 1.0f) {
    hsv[0] = fmod(hsv[0], 1.0f);
  }

  /* saturation adjustments for more visible range */
  hsv[1] = ((hsv[0] > 0.5f) && (hsv[0] < 0.8f)) ? 0.5f : 0.6f;

  /* value is fixed at 1.0f, otherwise we cannot clearly see the curves... */
  hsv[2] = 1.0f;

  /* finally, convert this to RGB colors */
  hsv_to_rgb_v(hsv, out);
}
