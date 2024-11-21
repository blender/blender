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
#include "RNA_prototypes.hh"

#include "ED_anim_api.hh"

#include "ANIM_action.hh"

#include "fmt/format.h"

#include <cstring>

struct StructRNA;

/* ----------------------- Getter functions ----------------------- */

std::optional<int> getname_anim_fcurve(char *name, ID *id, FCurve *fcu)
{
  /* Could make an argument, it's a documented limit at the moment. */
  constexpr size_t name_maxncpy = 256;

  /* Handle some nullptr cases. */
  if (name == nullptr) {
    /* A 'get name' function should be able to get the name, otherwise it's a bug. */
    BLI_assert_unreachable();
    return {};
  }
  if (fcu == nullptr) {
    BLI_strncpy(name, RPT_("<invalid>"), name_maxncpy);
    return {};
  }
  if (fcu->rna_path == nullptr) {
    BLI_strncpy(name, RPT_("<no path>"), name_maxncpy);
    return {};
  }
  if (id == nullptr) {
    BLI_snprintf(name, name_maxncpy, "%s[%d]", fcu->rna_path, fcu->array_index);
    return {};
  }

  PointerRNA id_ptr = RNA_id_pointer_create(id);

  PointerRNA ptr;
  PropertyRNA *prop;

  if (!RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
    /* Could not resolve the path, so just use the path itself as 'name'. */
    BLI_snprintf(name, name_maxncpy, "\"%s[%d]\"", fcu->rna_path, fcu->array_index);

    /* Tag F-Curve as disabled - as not usable path. */
    fcu->flag |= FCURVE_DISABLED;
    return {};
  }

  const char *structname = nullptr, *propname = nullptr;
  char arrayindbuf[16];
  const char *arrayname = nullptr;
  bool free_structname = false;

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

  char pchanName[name_maxncpy], constName[name_maxncpy];
  if (BLI_str_quoted_substr(fcu->rna_path, "bones[", pchanName, sizeof(pchanName)) &&
      BLI_str_quoted_substr(fcu->rna_path, "constraints[", constName, sizeof(constName)))
  {
    structname = BLI_sprintfN("%s : %s", pchanName, constName);
    free_structname = true;
  }
  else if (ptr.data != ptr.owner_id) {
    PropertyRNA *nameprop = RNA_struct_name_property(ptr.type);
    if (nameprop) {
      structname = RNA_property_string_get_alloc(&ptr, nameprop, nullptr, 0, nullptr);
      free_structname = true;
    }
    else {
      structname = RNA_struct_ui_name(ptr.type);
    }

    /* For the sequencer, a strip's 'Transform' or 'Crop' is a nested (under Sequence)
     * struct, but displaying the struct name alone is no meaningful information
     * (and also cannot be filtered well), same for modifiers.
     * So display strip name alongside as well. */
    if (GS(ptr.owner_id->name) == ID_SCE) {
      char stripname[name_maxncpy];
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
          free_structname = true;
        }
      }
    }
    /* For node sockets, it is useful to include the node name as well (multiple similar nodes
     * are not distinguishable otherwise). Unfortunately, the node label cannot be retrieved
     * from the rna path, for this to work access to the underlying node is needed (but finding
     * the node iterates all nodes & sockets which would result in bad performance in some
     * circumstances). */
    if (RNA_struct_is_a(ptr.type, &RNA_NodeSocket)) {
      char nodename[name_maxncpy];
      if (BLI_str_quoted_substr(fcu->rna_path, "nodes[", nodename, sizeof(nodename))) {
        const char *structname_all = BLI_sprintfN("%s : %s", nodename, structname);
        if (free_structname) {
          MEM_freeN((void *)structname);
        }
        structname = structname_all;
        free_structname = true;
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

  /* Use the property's owner struct icon. */
  return RNA_struct_ui_icon(ptr.type);
}

std::string getname_anim_fcurve_for_slot(Main &bmain,
                                         const blender::animrig::Slot &slot,
                                         FCurve &fcurve)
{
  /* TODO: Refactor to avoid this variable. */
  constexpr size_t name_maxncpy = 256;
  char name_buffer[name_maxncpy];
  name_buffer[0] = '\0';

  /* Check the Slot's users to see if we can find an ID* that can resolve the F-Curve. */
  for (ID *user : slot.users(bmain)) {
    const std::optional<int> icon = getname_anim_fcurve(name_buffer, user, &fcurve);
    if (icon.has_value()) {
      /* Managed to find a name! */
      return name_buffer;
    }
  }

  if (!slot.users(bmain).is_empty()) {
    /* This slot is assigned to at least one ID, and still the property it animates could not be
     * found. There is no use in continuing. */
    fcurve.flag |= FCURVE_DISABLED;
    return fmt::format("\"{}[{}]\"", fcurve.rna_path, fcurve.array_index);
  }

  /* If this part of the code is hit, the slot is not assigned to anything. The remainder of
   * this function is all a best-effort attempt. Because of that, it will not set the
   * FCURVE_DISABLED flag on the F-Curve, as having unassigned animation data is not an error (and
   * that flag indicates an error). */

  /* Fall back to the ID type of the slot for simple properties. */
  if (!slot.has_idtype()) {
    /* The Slot has never been assigned to any ID, so we don't even know what type of ID it is
     * meant for. */
    return fmt::format("\"{}[{}]\"", fcurve.rna_path, fcurve.array_index);
  }

  if (blender::StringRef(fcurve.rna_path).find(".") != blender::StringRef::not_found) {
    /* Not a simple property, so bail out. This needs path resolution, which needs an ID*. */
    return fmt::format("\"{}[{}]\"", fcurve.rna_path, fcurve.array_index);
  }

  /* Find the StructRNA for this Slot's ID type. */
  StructRNA *srna = ID_code_to_RNA_type(slot.idtype);
  if (!srna) {
    return fmt::format("\"{}[{}]\"", fcurve.rna_path, fcurve.array_index);
  }

  /* Find the property. */
  PropertyRNA *prop = RNA_struct_type_find_property(srna, fcurve.rna_path);
  if (!prop) {
    return fmt::format("\"{}[{}]\"", fcurve.rna_path, fcurve.array_index);
  }

  /* Property Name is straightforward */
  const char *propname = RNA_property_ui_name(prop);

  /* Array Index - only if applicable */
  if (!RNA_property_array_check(prop)) {
    return propname;
  }

  std::string arrayname = "";
  char c = RNA_property_array_item_char(prop, fcurve.array_index);
  if (c) {
    arrayname = std::string(1, c);
  }
  else {
    arrayname = fmt::format("[{}]", fcurve.array_index);
  }
  return arrayname + " " + propname;
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
