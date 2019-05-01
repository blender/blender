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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

/** \file
 * \ingroup edanimation
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"

#include "BKE_animsys.h"
#include "BKE_fcurve.h"
#include "BKE_context.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_keyframing.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "anim_intern.h"

/* ************************************************** */
/* Animation Data Validation */

/* Get (or add relevant data to be able to do so) F-Curve from the driver stack,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 *
 * - add: 0 - don't add anything if not found,
 *        1 - add new Driver FCurve (with keyframes for visual tweaking),
 *        2 - add new Driver FCurve (with generator, for script backwards compatibility)
 *        -1 - add new Driver FCurve without driver stuff (for pasting)
 */
FCurve *verify_driver_fcurve(ID *id, const char rna_path[], const int array_index, short add)
{
  AnimData *adt;
  FCurve *fcu;

  /* sanity checks */
  if (ELEM(NULL, id, rna_path)) {
    return NULL;
  }

  /* init animdata if none available yet */
  adt = BKE_animdata_from_id(id);
  if ((adt == NULL) && (add)) {
    adt = BKE_animdata_add_id(id);
  }
  if (adt == NULL) {
    /* if still none (as not allowed to add, or ID doesn't have animdata for some reason) */
    return NULL;
  }

  /* try to find f-curve matching for this setting
   * - add if not found and allowed to add one
   * TODO: add auto-grouping support? how this works will need to be resolved
   */
  fcu = list_find_fcurve(&adt->drivers, rna_path, array_index);

  if ((fcu == NULL) && (add)) {
    /* use default settings to make a F-Curve */
    fcu = MEM_callocN(sizeof(FCurve), "FCurve");

    fcu->flag = (FCURVE_VISIBLE | FCURVE_SELECTED);
    fcu->auto_smoothing = FCURVE_SMOOTH_CONT_ACCEL;

    /* store path - make copy, and store that */
    fcu->rna_path = BLI_strdup(rna_path);
    fcu->array_index = array_index;

    /* If add is negative, don't init this data yet,
     * since it will be filled in by the pasted driver. */
    if (add > 0) {
      BezTriple *bezt;
      size_t i;

      /* add some new driver data */
      fcu->driver = MEM_callocN(sizeof(ChannelDriver), "ChannelDriver");

      /* F-Modifier or Keyframes? */
      // FIXME: replace these magic numbers with defines
      if (add == 2) {
        /* Python API Backwards compatibility hack:
         * Create FModifier so that old scripts won't break
         * for now before 2.7 series -- (September 4, 2013)
         */
        add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_GENERATOR, fcu);
      }
      else {
        /* add 2 keyframes so that user has something to work with
         * - These are configured to 0,0 and 1,1 to give a 1-1 mapping
         *   which can be easily tweaked from there.
         */
        insert_vert_fcurve(fcu, 0.0f, 0.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_FAST);
        insert_vert_fcurve(fcu, 1.0f, 1.0f, BEZT_KEYTYPE_KEYFRAME, INSERTKEY_FAST);

        /* configure this curve to extrapolate */
        for (i = 0, bezt = fcu->bezt; (i < fcu->totvert) && bezt; i++, bezt++) {
          bezt->h1 = bezt->h2 = HD_VECT;
        }

        fcu->extend = FCURVE_EXTRAPOLATE_LINEAR;
        calchandles_fcurve(fcu);
      }
    }

    /* just add F-Curve to end of driver list */
    BLI_addtail(&adt->drivers, fcu);
  }

  /* return the F-Curve */
  return fcu;
}

/* ************************************************** */
/* Driver Management API */

/* Helper for ANIM_add_driver_with_target - Adds the actual driver */
static int add_driver_with_target(ReportList *UNUSED(reports),
                                  ID *dst_id,
                                  const char dst_path[],
                                  int dst_index,
                                  ID *src_id,
                                  const char src_path[],
                                  int src_index,
                                  PointerRNA *dst_ptr,
                                  PropertyRNA *dst_prop,
                                  PointerRNA *src_ptr,
                                  PropertyRNA *src_prop,
                                  short flag,
                                  int driver_type)
{
  FCurve *fcu;
  short add_mode = (flag & CREATEDRIVER_WITH_FMODIFIER) ? 2 : 1;
  const char *prop_name = RNA_property_identifier(src_prop);

  /* Create F-Curve with Driver */
  fcu = verify_driver_fcurve(dst_id, dst_path, dst_index, add_mode);

  if (fcu && fcu->driver) {
    ChannelDriver *driver = fcu->driver;
    DriverVar *dvar;

    /* Set the type of the driver */
    driver->type = driver_type;

    /* Set driver expression, so that the driver works out of the box
     *
     * The following checks define a bit of "autodetection magic" we use
     * to ensure that the drivers will behave as expected out of the box
     * when faced with properties with different units.
     */
    /* XXX: if we have N-1 mapping, should we include all those in the expression? */
    if ((RNA_property_unit(dst_prop) == PROP_UNIT_ROTATION) &&
        (RNA_property_unit(src_prop) != PROP_UNIT_ROTATION)) {
      /* Rotation Destination:  normal -> radians,  so convert src to radians
       * (However, if both input and output is a rotation, don't apply such corrections)
       */
      BLI_strncpy(driver->expression, "radians(var)", sizeof(driver->expression));
    }
    else if ((RNA_property_unit(src_prop) == PROP_UNIT_ROTATION) &&
             (RNA_property_unit(dst_prop) != PROP_UNIT_ROTATION)) {
      /* Rotation Source:  radians -> normal,  so convert src to degrees
       * (However, if both input and output is a rotation, don't apply such corrections)
       */
      BLI_strncpy(driver->expression, "degrees(var)", sizeof(driver->expression));
    }
    else {
      /* Just a normal property without any unit problems */
      BLI_strncpy(driver->expression, "var", sizeof(driver->expression));
    }

    /* Create a driver variable for the target
     *   - For transform properties, we want to automatically use "transform channel" instead
     *     (The only issue is with quat rotations vs euler channels...)
     *   - To avoid problems with transform properties depending on the final transform that they
     *     control (thus creating pseudo-cycles - see T48734), we don't use transform channels
     *     when both the source and destinations are in same places.
     */
    dvar = driver_add_new_variable(driver);

    if (ELEM(src_ptr->type, &RNA_Object, &RNA_PoseBone) &&
        (STREQ(prop_name, "location") || STREQ(prop_name, "scale") ||
         STRPREFIX(prop_name, "rotation_")) &&
        (src_ptr->data != dst_ptr->data)) {
      /* Transform Channel */
      DriverTarget *dtar;

      driver_change_variable_type(dvar, DVAR_TYPE_TRANSFORM_CHAN);
      dtar = &dvar->targets[0];

      /* Bone or Object target? */
      dtar->id = src_id;
      dtar->idtype = GS(src_id->name);

      if (src_ptr->type == &RNA_PoseBone) {
        RNA_string_get(src_ptr, "name", dtar->pchan_name);
      }

      /* Transform channel depends on type */
      if (STREQ(prop_name, "location")) {
        if (src_index == 2) {
          dtar->transChan = DTAR_TRANSCHAN_LOCZ;
        }
        else if (src_index == 1) {
          dtar->transChan = DTAR_TRANSCHAN_LOCY;
        }
        else {
          dtar->transChan = DTAR_TRANSCHAN_LOCX;
        }
      }
      else if (STREQ(prop_name, "scale")) {
        if (src_index == 2) {
          dtar->transChan = DTAR_TRANSCHAN_SCALEZ;
        }
        else if (src_index == 1) {
          dtar->transChan = DTAR_TRANSCHAN_SCALEY;
        }
        else {
          dtar->transChan = DTAR_TRANSCHAN_SCALEX;
        }
      }
      else {
        /* XXX: With quaternions and axis-angle, this mapping might not be correct...
         *      But since those have 4 elements instead, there's not much we can do
         */
        if (src_index == 2) {
          dtar->transChan = DTAR_TRANSCHAN_ROTZ;
        }
        else if (src_index == 1) {
          dtar->transChan = DTAR_TRANSCHAN_ROTY;
        }
        else {
          dtar->transChan = DTAR_TRANSCHAN_ROTX;
        }
      }
    }
    else {
      /* Single RNA Property */
      DriverTarget *dtar = &dvar->targets[0];

      /* ID is as-is */
      dtar->id = src_id;
      dtar->idtype = GS(src_id->name);

      /* Need to make a copy of the path (or build one with array index built in) */
      if (RNA_property_array_check(src_prop)) {
        dtar->rna_path = BLI_sprintfN("%s[%d]", src_path, src_index);
      }
      else {
        dtar->rna_path = BLI_strdup(src_path);
      }
    }
  }

  /* set the done status */
  return (fcu != NULL);
}

/* Main Driver Management API calls:
 * Add a new driver for the specified property on the given ID block,
 * and make it be driven by the specified target.
 *
 * This is intended to be used in conjunction with a modal "eyedropper"
 * for picking the variable that is going to be used to drive this one.
 *
 * - flag: eCreateDriverFlags
 * - driver_type: eDriver_Types
 * - mapping_type: eCreateDriver_MappingTypes
 */
int ANIM_add_driver_with_target(ReportList *reports,
                                ID *dst_id,
                                const char dst_path[],
                                int dst_index,
                                ID *src_id,
                                const char src_path[],
                                int src_index,
                                short flag,
                                int driver_type,
                                short mapping_type)
{
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;

  PointerRNA id_ptr2, ptr2;
  PropertyRNA *prop2;
  int done_tot = 0;

  /* validate pointers first - exit if failure */
  RNA_id_pointer_create(dst_id, &id_ptr);
  if (RNA_path_resolve_property(&id_ptr, dst_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not add driver, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        dst_id->name,
        dst_path);
    return 0;
  }

  RNA_id_pointer_create(src_id, &id_ptr2);
  if ((RNA_path_resolve_property(&id_ptr2, src_path, &ptr2, &prop2) == false) ||
      (mapping_type == CREATEDRIVER_MAPPING_NONE)) {
    /* No target - So, fall back to default method for adding a "simple" driver normally */
    return ANIM_add_driver(
        reports, dst_id, dst_path, dst_index, flag | CREATEDRIVER_WITH_DEFAULT_DVAR, driver_type);
  }

  /* handle curve-property mappings based on mapping_type */
  switch (mapping_type) {
    case CREATEDRIVER_MAPPING_N_N: /* N-N - Try to match as much as possible,
                                    * then use the first one */
    {
      /* Use the shorter of the two (to avoid out of bounds access) */
      int dst_len = (RNA_property_array_check(prop)) ? RNA_property_array_length(&ptr, prop) : 1;
      int src_len = (RNA_property_array_check(prop)) ? RNA_property_array_length(&ptr2, prop2) : 1;

      int len = MIN2(dst_len, src_len);
      int i;

      for (i = 0; i < len; i++) {
        done_tot += add_driver_with_target(reports,
                                           dst_id,
                                           dst_path,
                                           i,
                                           src_id,
                                           src_path,
                                           i,
                                           &ptr,
                                           prop,
                                           &ptr2,
                                           prop2,
                                           flag,
                                           driver_type);
      }
      break;
    }

    case CREATEDRIVER_MAPPING_1_N: /* 1-N - Specified target index for all */
    default: {
      int len = (RNA_property_array_check(prop)) ? RNA_property_array_length(&ptr, prop) : 1;
      int i;

      for (i = 0; i < len; i++) {
        done_tot += add_driver_with_target(reports,
                                           dst_id,
                                           dst_path,
                                           i,
                                           src_id,
                                           src_path,
                                           src_index,
                                           &ptr,
                                           prop,
                                           &ptr2,
                                           prop2,
                                           flag,
                                           driver_type);
      }
      break;
    }

    case CREATEDRIVER_MAPPING_1_1: /* 1-1 - Use the specified index (unless -1) */
    {
      done_tot = add_driver_with_target(reports,
                                        dst_id,
                                        dst_path,
                                        dst_index,
                                        src_id,
                                        src_path,
                                        src_index,
                                        &ptr,
                                        prop,
                                        &ptr2,
                                        prop2,
                                        flag,
                                        driver_type);
      break;
    }
  }

  /* done */
  return done_tot;
}

/* --------------------------------- */

/* Main Driver Management API calls:
 * Add a new driver for the specified property on the given ID block
 */
int ANIM_add_driver(
    ReportList *reports, ID *id, const char rna_path[], int array_index, short flag, int type)
{
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  FCurve *fcu;
  int array_index_max;
  int done_tot = 0;

  /* validate pointer first - exit if failure */
  RNA_id_pointer_create(id, &id_ptr);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not add driver, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path);
    return 0;
  }

  /* key entire array convenience method */
  if (array_index == -1) {
    array_index_max = RNA_property_array_length(&ptr, prop);
    array_index = 0;
  }
  else {
    array_index_max = array_index;
  }

  /* maximum index should be greater than the start index */
  if (array_index == array_index_max) {
    array_index_max += 1;
  }

  /* will only loop once unless the array index was -1 */
  for (; array_index < array_index_max; array_index++) {
    short add_mode = (flag & CREATEDRIVER_WITH_FMODIFIER) ? 2 : 1;

    /* create F-Curve with Driver */
    fcu = verify_driver_fcurve(id, rna_path, array_index, add_mode);

    if (fcu && fcu->driver) {
      ChannelDriver *driver = fcu->driver;

      /* set the type of the driver */
      driver->type = type;

      /* Creating drivers for buttons will create the driver(s) with type
       * "scripted expression" so that their values won't be lost immediately,
       * so here we copy those values over to the driver's expression
       *
       * If the "default dvar" option (for easier UI setup of drivers) is provided,
       * include "var" in the expressions too, so that the user doesn't have to edit
       * it to get something to happen. It should be fine to just add it to the default
       * value, so that we get both in the expression, even if it's a bit more confusing
       * that way...
       */
      if (type == DRIVER_TYPE_PYTHON) {
        PropertyType proptype = RNA_property_type(prop);
        int array = RNA_property_array_length(&ptr, prop);
        const char *dvar_prefix = (flag & CREATEDRIVER_WITH_DEFAULT_DVAR) ? "var + " : "";
        char *expression = driver->expression;
        int val, maxlen = sizeof(driver->expression);
        float fval;

        if (proptype == PROP_BOOLEAN) {
          if (!array) {
            val = RNA_property_boolean_get(&ptr, prop);
          }
          else {
            val = RNA_property_boolean_get_index(&ptr, prop, array_index);
          }

          BLI_snprintf(expression, maxlen, "%s%s", dvar_prefix, (val) ? "True" : "False");
        }
        else if (proptype == PROP_INT) {
          if (!array) {
            val = RNA_property_int_get(&ptr, prop);
          }
          else {
            val = RNA_property_int_get_index(&ptr, prop, array_index);
          }

          BLI_snprintf(expression, maxlen, "%s%d", dvar_prefix, val);
        }
        else if (proptype == PROP_FLOAT) {
          if (!array) {
            fval = RNA_property_float_get(&ptr, prop);
          }
          else {
            fval = RNA_property_float_get_index(&ptr, prop, array_index);
          }

          BLI_snprintf(expression, maxlen, "%s%.3f", dvar_prefix, fval);
          BLI_str_rstrip_float_zero(expression, '\0');
        }
        else if (flag & CREATEDRIVER_WITH_DEFAULT_DVAR) {
          BLI_strncpy(expression, "var", maxlen);
        }
      }

      /* for easier setup of drivers from UI, a driver variable should be
       * added if flag is set (UI calls only)
       */
      if (flag & CREATEDRIVER_WITH_DEFAULT_DVAR) {
        /* assume that users will mostly want this to be of type "Transform Channel" too,
         * since this allows the easiest setting up of common rig components
         */
        DriverVar *dvar = driver_add_new_variable(driver);
        driver_change_variable_type(dvar, DVAR_TYPE_TRANSFORM_CHAN);
      }
    }

    /* set the done status */
    done_tot += (fcu != NULL);
  }

  /* done */
  return done_tot;
}

/* Main Driver Management API calls:
 * Remove the driver for the specified property on the given ID block (if available)
 */
bool ANIM_remove_driver(ReportList *UNUSED(reports),
                        ID *id,
                        const char rna_path[],
                        int array_index,
                        short UNUSED(flag))
{
  AnimData *adt;
  FCurve *fcu;
  bool success = false;

  /* we don't check the validity of the path here yet, but it should be ok... */
  adt = BKE_animdata_from_id(id);

  if (adt) {
    if (array_index == -1) {
      /* step through all drivers, removing all of those with the same base path */
      FCurve *fcu_iter = adt->drivers.first;

      while ((fcu = iter_step_fcurve(fcu_iter, rna_path)) != NULL) {
        /* store the next fcurve for looping  */
        fcu_iter = fcu->next;

        /* remove F-Curve from driver stack, then free it */
        BLI_remlink(&adt->drivers, fcu);
        free_fcurve(fcu);

        /* done successfully */
        success = true;
      }
    }
    else {
      /* find the matching driver and remove it only
       * Note: here is one of the places where we don't want new F-Curve + Driver added!
       *      so 'add' var must be 0
       */
      fcu = verify_driver_fcurve(id, rna_path, array_index, 0);
      if (fcu) {
        BLI_remlink(&adt->drivers, fcu);
        free_fcurve(fcu);

        success = true;
      }
    }
  }

  return success;
}

/* ************************************************** */
/* Driver Management API - Copy/Paste Drivers */

/* Copy/Paste Buffer for Driver Data... */
static FCurve *channeldriver_copypaste_buf = NULL;

/* This function frees any MEM_calloc'ed copy/paste buffer data */
void ANIM_drivers_copybuf_free(void)
{
  /* free the buffer F-Curve if it exists, as if it were just another F-Curve */
  if (channeldriver_copypaste_buf) {
    free_fcurve(channeldriver_copypaste_buf);
  }
  channeldriver_copypaste_buf = NULL;
}

/* Checks if there is a driver in the copy/paste buffer */
bool ANIM_driver_can_paste(void)
{
  return (channeldriver_copypaste_buf != NULL);
}

/* ------------------- */

/* Main Driver Management API calls:
 *  Make a copy of the driver for the specified property on the given ID block
 */
bool ANIM_copy_driver(
    ReportList *reports, ID *id, const char rna_path[], int array_index, short UNUSED(flag))
{
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  FCurve *fcu;

  /* validate pointer first - exit if failure */
  RNA_id_pointer_create(id, &id_ptr);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Could not find driver to copy, as RNA path is invalid for the given ID (ID = %s, "
                "path = %s)",
                id->name,
                rna_path);
    return 0;
  }

  /* try to get F-Curve with Driver */
  fcu = verify_driver_fcurve(id, rna_path, array_index, 0);

  /* clear copy/paste buffer first (for consistency with other copy/paste buffers) */
  ANIM_drivers_copybuf_free();

  /* copy this to the copy/paste buf if it exists */
  if (fcu && fcu->driver) {
    /* Make copies of some info such as the rna_path, then clear this info from the
     * F-Curve temporarily so that we don't end up wasting memory storing the path
     * which won't get used ever.
     */
    char *tmp_path = fcu->rna_path;
    fcu->rna_path = NULL;

    /* make a copy of the F-Curve with */
    channeldriver_copypaste_buf = copy_fcurve(fcu);

    /* restore the path */
    fcu->rna_path = tmp_path;

    /* copied... */
    return 1;
  }

  /* done */
  return 0;
}

/* Main Driver Management API calls:
 * Add a new driver for the specified property on the given ID block or replace an existing one
 * with the driver + driver-curve data from the buffer
 */
bool ANIM_paste_driver(
    ReportList *reports, ID *id, const char rna_path[], int array_index, short UNUSED(flag))
{
  PointerRNA id_ptr, ptr;
  PropertyRNA *prop;
  FCurve *fcu;

  /* validate pointer first - exit if failure */
  RNA_id_pointer_create(id, &id_ptr);
  if (RNA_path_resolve_property(&id_ptr, rna_path, &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not paste driver, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path);
    return 0;
  }

  /* if the buffer is empty, cannot paste... */
  if (channeldriver_copypaste_buf == NULL) {
    BKE_report(reports, RPT_ERROR, "Paste driver: no driver to paste");
    return 0;
  }

  /* create Driver F-Curve, but without data which will be copied across... */
  fcu = verify_driver_fcurve(id, rna_path, array_index, -1);

  if (fcu) {
    /* copy across the curve data from the buffer curve
     * NOTE: this step needs care to not miss new settings
     */
    /* keyframes/samples */
    fcu->bezt = MEM_dupallocN(channeldriver_copypaste_buf->bezt);
    fcu->fpt = MEM_dupallocN(channeldriver_copypaste_buf->fpt);
    fcu->totvert = channeldriver_copypaste_buf->totvert;

    /* modifiers */
    copy_fmodifiers(&fcu->modifiers, &channeldriver_copypaste_buf->modifiers);

    /* extrapolation mode */
    fcu->extend = channeldriver_copypaste_buf->extend;

    /* the 'juicy' stuff - the driver */
    fcu->driver = fcurve_copy_driver(channeldriver_copypaste_buf->driver);
  }

  /* done */
  return (fcu != NULL);
}

/* ************************************************** */
/* Driver Management API - Copy/Paste Driver Variables */

/* Copy/Paste Buffer for Driver Variables... */
static ListBase driver_vars_copybuf = {NULL, NULL};

/* This function frees any MEM_calloc'ed copy/paste buffer data */
void ANIM_driver_vars_copybuf_free(void)
{
  /* Free the driver variables kept in the buffer */
  if (driver_vars_copybuf.first) {
    DriverVar *dvar, *dvarn;

    /* Free variables (and any data they use) */
    for (dvar = driver_vars_copybuf.first; dvar; dvar = dvarn) {
      dvarn = dvar->next;
      driver_free_variable(&driver_vars_copybuf, dvar);
    }
  }

  BLI_listbase_clear(&driver_vars_copybuf);
}

/* Checks if there are driver variables in the copy/paste buffer */
bool ANIM_driver_vars_can_paste(void)
{
  return (BLI_listbase_is_empty(&driver_vars_copybuf) == false);
}

/* -------------------------------------------------- */

/* Copy the given driver's variables to the buffer */
bool ANIM_driver_vars_copy(ReportList *reports, FCurve *fcu)
{
  /* sanity checks */
  if (ELEM(NULL, fcu, fcu->driver)) {
    BKE_report(reports, RPT_ERROR, "No driver to copy variables from");
    return false;
  }

  if (BLI_listbase_is_empty(&fcu->driver->variables)) {
    BKE_report(reports, RPT_ERROR, "Driver has no variables to copy");
    return false;
  }

  /* clear buffer */
  ANIM_driver_vars_copybuf_free();

  /* copy over the variables */
  driver_variables_copy(&driver_vars_copybuf, &fcu->driver->variables);

  return (BLI_listbase_is_empty(&driver_vars_copybuf) == false);
}

/* Paste the variables in the buffer to the given FCurve */
bool ANIM_driver_vars_paste(ReportList *reports, FCurve *fcu, bool replace)
{
  ChannelDriver *driver = (fcu) ? fcu->driver : NULL;
  ListBase tmp_list = {NULL, NULL};

  /* sanity checks */
  if (BLI_listbase_is_empty(&driver_vars_copybuf)) {
    BKE_report(reports, RPT_ERROR, "No driver variables in clipboard to paste");
    return false;
  }

  if (ELEM(NULL, fcu, fcu->driver)) {
    BKE_report(reports, RPT_ERROR, "Cannot paste driver variables without a driver");
    return false;
  }

  /* 1) Make a new copy of the variables in the buffer - these will get pasted later... */
  driver_variables_copy(&tmp_list, &driver_vars_copybuf);

  /* 2) Prepare destination array */
  if (replace) {
    DriverVar *dvar, *dvarn;

    /* Free all existing vars first - We aren't retaining anything */
    for (dvar = driver->variables.first; dvar; dvar = dvarn) {
      dvarn = dvar->next;
      driver_free_variable_ex(driver, dvar);
    }

    BLI_listbase_clear(&driver->variables);
  }

  /* 3) Add new vars */
  if (driver->variables.last) {
    DriverVar *last = driver->variables.last;
    DriverVar *first = tmp_list.first;

    last->next = first;
    first->prev = last;

    driver->variables.last = tmp_list.last;
  }
  else {
    driver->variables.first = tmp_list.first;
    driver->variables.last = tmp_list.last;
  }

  /* since driver variables are cached, the expression needs re-compiling too */
  BKE_driver_invalidate_expression(driver, false, true);

  return true;
}

/* ************************************************** */
/* UI-Button Interface */

/* Add Driver - Enum Defines ------------------------- */

/* Mapping Types enum for operators */
/* NOTE: Used by ANIM_OT_driver_button_add and UI_OT_eyedropper_driver */
// XXX: These names need reviewing
EnumPropertyItem prop_driver_create_mapping_types[] = {
    {CREATEDRIVER_MAPPING_1_N,
     "SINGLE_MANY",
     0,
     "All from Target",
     "Drive all components of this property using the target picked"},
    {CREATEDRIVER_MAPPING_1_1,
     "DIRECT",
     0,
     "Single from Target",
     "Drive this component of this property using the target picked"},

    {CREATEDRIVER_MAPPING_N_N,
     "MATCH",
     ICON_COLOR,
     "Match Indices",
     "Create drivers for each pair of corresponding elements"},

    {CREATEDRIVER_MAPPING_NONE_ALL,
     "NONE_ALL",
     ICON_HAND,
     "Manually Create Later",
     "Create drivers for all properties without assigning any targets yet"},
    {CREATEDRIVER_MAPPING_NONE,
     "NONE_SINGLE",
     0,
     "Manually Create Later (Single)",
     "Create driver for this property only and without assigning any targets yet"},
    {0, NULL, 0, NULL, NULL},
};

/* Filtering callback for driver mapping types enum */
static const EnumPropertyItem *driver_mapping_type_itemsf(bContext *C,
                                                          PointerRNA *UNUSED(owner_ptr),
                                                          PropertyRNA *UNUSED(owner_prop),
                                                          bool *r_free)
{
  EnumPropertyItem *input = prop_driver_create_mapping_types;
  EnumPropertyItem *item = NULL;

  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  int index;

  int totitem = 0;

  if (!C) { /* needed for docs */
    return prop_driver_create_mapping_types;
  }

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.id.data && ptr.data && prop && RNA_property_animateable(&ptr, prop)) {
    const bool is_array = RNA_property_array_check(prop);

    while (input->identifier) {
      if (ELEM(input->value, CREATEDRIVER_MAPPING_1_1, CREATEDRIVER_MAPPING_NONE) || (is_array)) {
        RNA_enum_item_add(&item, &totitem, input);
      }
      input++;
    }
  }
  else {
    /* We need at least this one! */
    RNA_enum_items_add_value(&item, &totitem, input, CREATEDRIVER_MAPPING_NONE);
  }

  RNA_enum_item_end(&item, &totitem);

  *r_free = true;
  return item;
}

/* Add Driver (With Menu) Button Operator ------------------------ */

static bool add_driver_button_poll(bContext *C)
{
  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  int index;
  bool driven, special;

  /* this operator can only run if there's a property button active, and it can be animated */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (!(ptr.id.data && ptr.data && prop)) {
    return false;
  }
  if (!RNA_property_animateable(&ptr, prop)) {
    return false;
  }

  /* Don't do anything if there is an fcurve for animation without a driver. */
  FCurve *fcu = rna_get_fcurve_context_ui(C, &ptr, prop, index, NULL, NULL, &driven, &special);
  return (fcu == NULL || fcu->driver);
}

/* Wrapper for creating a driver without knowing what the targets will be yet
 * (i.e. "manual/add later"). */
static int add_driver_button_none(bContext *C, wmOperator *op, short mapping_type)
{
  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  int index;
  int success = 0;

  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (mapping_type == CREATEDRIVER_MAPPING_NONE_ALL) {
    index = -1;
  }

  if (ptr.id.data && ptr.data && prop && RNA_property_animateable(&ptr, prop)) {
    char *path = BKE_animdata_driver_path_hack(C, &ptr, prop, NULL);
    short flags = CREATEDRIVER_WITH_DEFAULT_DVAR;

    if (path) {
      success += ANIM_add_driver(op->reports, ptr.id.data, path, index, flags, DRIVER_TYPE_PYTHON);
      MEM_freeN(path);
    }
  }

  if (success) {
    /* send updates */
    UI_context_update_anim_flag(C);
    DEG_relations_tag_update(CTX_data_main(C));
    WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL);  // XXX

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static int add_driver_button_menu_exec(bContext *C, wmOperator *op)
{
  short mapping_type = RNA_enum_get(op->ptr, "mapping_type");
  if (ELEM(mapping_type, CREATEDRIVER_MAPPING_NONE, CREATEDRIVER_MAPPING_NONE_ALL)) {
    /* Just create driver with no targets */
    return add_driver_button_none(C, op, mapping_type);
  }
  else {
    /* Create Driver using Eyedropper */
    wmOperatorType *ot = WM_operatortype_find("UI_OT_eyedropper_driver", true);

    /* XXX: We assume that it's fine to use the same set of properties,
     * since they're actually the same. */
    WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, op->ptr);

    return OPERATOR_FINISHED;
  }
}

/* Show menu or create drivers */
static int add_driver_button_menu_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  PropertyRNA *prop;

  if ((prop = RNA_struct_find_property(op->ptr, "mapping_type")) &&
      RNA_property_is_set(op->ptr, prop)) {
    /* Mapping Type is Set - Directly go into creating drivers */
    return add_driver_button_menu_exec(C, op);
  }
  else {
    /* Show menu */
    // TODO: This should get filtered by the enum filter
    /* important to execute in the region we're currently in */
    return WM_menu_invoke_ex(C, op, WM_OP_INVOKE_DEFAULT);
  }
}

static void UNUSED_FUNCTION(ANIM_OT_driver_button_add_menu)(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Driver Menu";
  ot->idname = "ANIM_OT_driver_button_add_menu";
  ot->description = "Add driver(s) for the property(s) represented by the highlighted button";

  /* callbacks */
  ot->invoke = add_driver_button_menu_invoke;
  ot->exec = add_driver_button_menu_exec;
  ot->poll = add_driver_button_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna,
                          "mapping_type",
                          prop_driver_create_mapping_types,
                          0,
                          "Mapping Type",
                          "Method used to match target and driven properties");
  RNA_def_enum_funcs(ot->prop, driver_mapping_type_itemsf);
}

/* Add Driver Button Operator ------------------------ */

static int add_driver_button_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  int index;

  /* try to find driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.id.data && ptr.data && prop && RNA_property_animateable(&ptr, prop)) {
    /* 1) Create a new "empty" driver for this property */
    char *path = BKE_animdata_driver_path_hack(C, &ptr, prop, NULL);
    short flags = CREATEDRIVER_WITH_DEFAULT_DVAR;
    short success = 0;

    if (path) {
      success += ANIM_add_driver(op->reports, ptr.id.data, path, index, flags, DRIVER_TYPE_PYTHON);
      MEM_freeN(path);
    }

    if (success) {
      /* send updates */
      UI_context_update_anim_flag(C);
      DEG_id_tag_update(ptr.id.data, ID_RECALC_COPY_ON_WRITE);
      DEG_relations_tag_update(CTX_data_main(C));
      WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL);
    }

    /* 2) Show editing panel for setting up this driver */
    /* TODO: Use a different one from the editing popever, so we can have the single/all toggle? */
    UI_popover_panel_invoke(C, "GRAPH_PT_drivers_popover", true, op->reports);
  }

  return OPERATOR_INTERFACE;
}

void ANIM_OT_driver_button_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Driver";
  ot->idname = "ANIM_OT_driver_button_add";
  ot->description = "Add driver for the property under the cursor";

  /* callbacks */
  /* NOTE: No exec, as we need all these to use the current context info */
  ot->invoke = add_driver_button_invoke;
  ot->poll = add_driver_button_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* Remove Driver Button Operator ------------------------ */

static int remove_driver_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  short success = 0;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  /* try to find driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (all) {
    index = -1;
  }

  if (ptr.id.data && ptr.data && prop) {
    char *path = BKE_animdata_driver_path_hack(C, &ptr, prop, NULL);

    if (path) {
      success = ANIM_remove_driver(op->reports, ptr.id.data, path, index, 0);

      MEM_freeN(path);
    }
  }

  if (success) {
    /* send updates */
    UI_context_update_anim_flag(C);
    DEG_relations_tag_update(CTX_data_main(C));
    WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL);  // XXX
  }

  return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_driver_button_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Driver";
  ot->idname = "ANIM_OT_driver_button_remove";
  ot->description =
      "Remove the driver(s) for the property(s) connected represented by the highlighted button";

  /* callbacks */
  ot->exec = remove_driver_button_exec;
  // op->poll = ??? // TODO: need to have some driver to be able to do this...

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_boolean(ot->srna, "all", 1, "All", "Delete drivers for all elements of the array");
}

/* Edit Driver Button Operator ------------------------ */

static int edit_driver_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  int index;

  /* try to find driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.id.data && ptr.data && prop) {
    UI_popover_panel_invoke(C, "GRAPH_PT_drivers_popover", true, op->reports);
  }

  return OPERATOR_INTERFACE;
}

void ANIM_OT_driver_button_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Driver";
  ot->idname = "ANIM_OT_driver_button_edit";
  ot->description =
      "Edit the drivers for the property connected represented by the highlighted button";

  /* callbacks */
  ot->exec = edit_driver_button_exec;
  // op->poll = ??? // TODO: need to have some driver to be able to do this...

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* Copy Driver Button Operator ------------------------ */

static int copy_driver_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  short success = 0;
  int index;

  /* try to create driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.id.data && ptr.data && prop && RNA_property_animateable(&ptr, prop)) {
    char *path = BKE_animdata_driver_path_hack(C, &ptr, prop, NULL);

    if (path) {
      /* only copy the driver for the button that this was involved for */
      success = ANIM_copy_driver(op->reports, ptr.id.data, path, index, 0);

      UI_context_update_anim_flag(C);

      MEM_freeN(path);
    }
  }

  /* since we're just copying, we don't really need to do anything else...*/
  return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_copy_driver_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy Driver";
  ot->idname = "ANIM_OT_copy_driver_button";
  ot->description = "Copy the driver for the highlighted button";

  /* callbacks */
  ot->exec = copy_driver_button_exec;
  // op->poll = ??? // TODO: need to have some driver to be able to do this...

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* Paste Driver Button Operator ------------------------ */

static int paste_driver_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = {{NULL}};
  PropertyRNA *prop = NULL;
  short success = 0;
  int index;

  /* try to create driver using property retrieved from UI */
  UI_context_active_but_prop_get(C, &ptr, &prop, &index);

  if (ptr.id.data && ptr.data && prop && RNA_property_animateable(&ptr, prop)) {
    char *path = BKE_animdata_driver_path_hack(C, &ptr, prop, NULL);

    if (path) {
      /* only copy the driver for the button that this was involved for */
      success = ANIM_paste_driver(op->reports, ptr.id.data, path, index, 0);

      UI_context_update_anim_flag(C);

      DEG_relations_tag_update(CTX_data_main(C));
      DEG_id_tag_update(ptr.id.data, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);  // XXX

      MEM_freeN(path);
    }
  }

  /* since we're just copying, we don't really need to do anything else...*/
  return (success) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_paste_driver_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste Driver";
  ot->idname = "ANIM_OT_paste_driver_button";
  ot->description = "Paste the driver in the copy/paste buffer for the highlighted button";

  /* callbacks */
  ot->exec = paste_driver_button_exec;
  // op->poll = ??? // TODO: need to have some driver to be able to do this...

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ************************************************** */
