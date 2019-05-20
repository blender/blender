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
 * \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#include "ED_keyframing.h"

/* exported for use in API */
const EnumPropertyItem rna_enum_keyingset_path_grouping_items[] = {
    {KSP_GROUP_NAMED, "NAMED", 0, "Named Group", ""},
    {KSP_GROUP_NONE, "NONE", 0, "None", ""},
    {KSP_GROUP_KSNAME, "KEYINGSET", 0, "Keying Set Name", ""},
    {0, NULL, 0, NULL, NULL},
};

/* It would be cool to get rid of this 'INSERTKEY_' prefix in 'py strings' values,
 * but it would break existing
 * exported keyingset... :/
 */
const EnumPropertyItem rna_enum_keying_flag_items[] = {
    {INSERTKEY_NEEDED,
     "INSERTKEY_NEEDED",
     0,
     "Only Needed",
     "Only insert keyframes where they're needed in the relevant F-Curves"},
    {INSERTKEY_MATRIX,
     "INSERTKEY_VISUAL",
     0,
     "Visual Keying",
     "Insert keyframes based on 'visual transforms'"},
    {INSERTKEY_XYZ2RGB,
     "INSERTKEY_XYZ_TO_RGB",
     0,
     "XYZ=RGB Colors",
     "Color for newly added transformation F-Curves (Location, Rotation, Scale) "
     "and also Color is based on the transform axis"},
    {0, NULL, 0, NULL, NULL},
};

/* Contains additional flags suitable for use in Python API functions. */
const EnumPropertyItem rna_enum_keying_flag_items_api[] = {
    {INSERTKEY_NEEDED,
     "INSERTKEY_NEEDED",
     0,
     "Only Needed",
     "Only insert keyframes where they're needed in the relevant F-Curves"},
    {INSERTKEY_MATRIX,
     "INSERTKEY_VISUAL",
     0,
     "Visual Keying",
     "Insert keyframes based on 'visual transforms'"},
    {INSERTKEY_XYZ2RGB,
     "INSERTKEY_XYZ_TO_RGB",
     0,
     "XYZ=RGB Colors",
     "Color for newly added transformation F-Curves (Location, Rotation, Scale) "
     "and also Color is based on the transform axis"},
    {INSERTKEY_REPLACE,
     "INSERTKEY_REPLACE",
     0,
     "Replace Existing",
     "Only replace existing keyframes"},
    {INSERTKEY_AVAILABLE,
     "INSERTKEY_AVAILABLE",
     0,
     "Only Available",
     "Don't create F-Curves when they don't already exist"},
    {INSERTKEY_CYCLE_AWARE,
     "INSERTKEY_CYCLE_AWARE",
     0,
     "Cycle Aware Keying",
     "When inserting into a curve with cyclic extrapolation, remap the keyframe inside "
     "the cycle time range, and if changing an end key, also update the other one"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_math_base.h"

#  include "BKE_animsys.h"
#  include "BKE_fcurve.h"
#  include "BKE_nla.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "DNA_object_types.h"

#  include "ED_anim_api.h"

#  include "WM_api.h"

static void rna_AnimData_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  ID *id = ptr->id.data;

  ANIM_id_update(bmain, id);
}

static void rna_AnimData_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);

  rna_AnimData_update(bmain, scene, ptr);
}

static int rna_AnimData_action_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  AnimData *adt = (AnimData *)ptr->data;

  /* active action is only editable when it is not a tweaking strip */
  if ((adt->flag & ADT_NLA_EDIT_ON) || (adt->actstrip) || (adt->tmpact))
    return 0;
  else
    return PROP_EDITABLE;
}

static void rna_AnimData_action_set(PointerRNA *ptr,
                                    PointerRNA value,
                                    struct ReportList *UNUSED(reports))
{
  ID *ownerId = (ID *)ptr->id.data;

  /* set action */
  BKE_animdata_set_action(NULL, ownerId, value.data);
}

static void rna_AnimData_tweakmode_set(PointerRNA *ptr, const bool value)
{
  AnimData *adt = (AnimData *)ptr->data;

  /* NOTE: technically we should also set/unset SCE_NLA_EDIT_ON flag on the
   * scene which is used to make polling tests faster, but this flag is weak
   * and can easily break e.g. by changing layer visibility. This needs to be
   * dealt with at some point. */

  if (value) {
    BKE_nla_tweakmode_enter(adt);
  }
  else {
    BKE_nla_tweakmode_exit(adt);
  }
}

/* ****************************** */

/* wrapper for poll callback */
static bool RKS_POLL_rna_internal(KeyingSetInfo *ksi, bContext *C)
{
  extern FunctionRNA rna_KeyingSetInfo_poll_func;

  PointerRNA ptr;
  ParameterList list;
  FunctionRNA *func;
  void *ret;
  int ok;

  RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
  func = &rna_KeyingSetInfo_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  {
    /* hook up arguments */
    RNA_parameter_set_lookup(&list, "ksi", &ksi);
    RNA_parameter_set_lookup(&list, "context", &C);

    /* execute the function */
    ksi->ext.call(C, &ptr, func, &list);

    /* read the result */
    RNA_parameter_get_lookup(&list, "ok", &ret);
    ok = *(bool *)ret;
  }
  RNA_parameter_list_free(&list);

  return ok;
}

/* wrapper for iterator callback */
static void RKS_ITER_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks)
{
  extern FunctionRNA rna_KeyingSetInfo_iterator_func;

  PointerRNA ptr;
  ParameterList list;
  FunctionRNA *func;

  RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
  func = &rna_KeyingSetInfo_iterator_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  {
    /* hook up arguments */
    RNA_parameter_set_lookup(&list, "ksi", &ksi);
    RNA_parameter_set_lookup(&list, "context", &C);
    RNA_parameter_set_lookup(&list, "ks", &ks);

    /* execute the function */
    ksi->ext.call(C, &ptr, func, &list);
  }
  RNA_parameter_list_free(&list);
}

/* wrapper for generator callback */
static void RKS_GEN_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks, PointerRNA *data)
{
  extern FunctionRNA rna_KeyingSetInfo_generate_func;

  PointerRNA ptr;
  ParameterList list;
  FunctionRNA *func;

  RNA_pointer_create(NULL, ksi->ext.srna, ksi, &ptr);
  func = &rna_KeyingSetInfo_generate_func; /* RNA_struct_find_generate(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  {
    /* hook up arguments */
    RNA_parameter_set_lookup(&list, "ksi", &ksi);
    RNA_parameter_set_lookup(&list, "context", &C);
    RNA_parameter_set_lookup(&list, "ks", &ks);
    RNA_parameter_set_lookup(&list, "data", data);

    /* execute the function */
    ksi->ext.call(C, &ptr, func, &list);
  }
  RNA_parameter_list_free(&list);
}

/* ------ */

/* XXX: the exact purpose of this is not too clear...
 * maybe we want to revise this at some point? */
static StructRNA *rna_KeyingSetInfo_refine(PointerRNA *ptr)
{
  KeyingSetInfo *ksi = (KeyingSetInfo *)ptr->data;
  return (ksi->ext.srna) ? ksi->ext.srna : &RNA_KeyingSetInfo;
}

static void rna_KeyingSetInfo_unregister(Main *bmain, StructRNA *type)
{
  KeyingSetInfo *ksi = RNA_struct_blender_type_get(type);

  if (ksi == NULL)
    return;

  /* free RNA data referencing this */
  RNA_struct_free_extension(type, &ksi->ext);
  RNA_struct_free(&BLENDER_RNA, type);

  WM_main_add_notifier(NC_WINDOW, NULL);

  /* unlink Blender-side data */
  ANIM_keyingset_info_unregister(bmain, ksi);
}

static StructRNA *rna_KeyingSetInfo_register(Main *bmain,
                                             ReportList *reports,
                                             void *data,
                                             const char *identifier,
                                             StructValidateFunc validate,
                                             StructCallbackFunc call,
                                             StructFreeFunc free)
{
  KeyingSetInfo dummyksi = {NULL};
  KeyingSetInfo *ksi;
  PointerRNA dummyptr = {{NULL}};
  int have_function[3];

  /* setup dummy type info to store static properties in */
  /* TODO: perhaps we want to get users to register
   * as if they're using 'KeyingSet' directly instead? */
  RNA_pointer_create(NULL, &RNA_KeyingSetInfo, &dummyksi, &dummyptr);

  /* validate the python class */
  if (validate(&dummyptr, data, have_function) != 0)
    return NULL;

  if (strlen(identifier) >= sizeof(dummyksi.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering keying set info class: '%s' is too long, maximum length is %d",
                identifier,
                (int)sizeof(dummyksi.idname));
    return NULL;
  }

  /* check if we have registered this info before, and remove it */
  ksi = ANIM_keyingset_info_find_name(dummyksi.idname);
  if (ksi && ksi->ext.srna) {
    rna_KeyingSetInfo_unregister(bmain, ksi->ext.srna);
  }

  /* create a new KeyingSetInfo type */
  ksi = MEM_callocN(sizeof(KeyingSetInfo), "python keying set info");
  memcpy(ksi, &dummyksi, sizeof(KeyingSetInfo));

  /* set RNA-extensions info */
  ksi->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, ksi->idname, &RNA_KeyingSetInfo);
  ksi->ext.data = data;
  ksi->ext.call = call;
  ksi->ext.free = free;
  RNA_struct_blender_type_set(ksi->ext.srna, ksi);

  /* set callbacks */
  /* NOTE: we really should have all of these...  */
  ksi->poll = (have_function[0]) ? RKS_POLL_rna_internal : NULL;
  ksi->iter = (have_function[1]) ? RKS_ITER_rna_internal : NULL;
  ksi->generate = (have_function[2]) ? RKS_GEN_rna_internal : NULL;

  /* add and register with other info as needed */
  ANIM_keyingset_info_register(ksi);

  WM_main_add_notifier(NC_WINDOW, NULL);

  /* return the struct-rna added */
  return ksi->ext.srna;
}

/* ****************************** */

static StructRNA *rna_ksPath_id_typef(PointerRNA *ptr)
{
  KS_Path *ksp = (KS_Path *)ptr->data;
  return ID_code_to_RNA_type(ksp->idtype);
}

static int rna_ksPath_id_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  KS_Path *ksp = (KS_Path *)ptr->data;
  return (ksp->idtype) ? PROP_EDITABLE : 0;
}

static void rna_ksPath_id_type_set(PointerRNA *ptr, int value)
{
  KS_Path *data = (KS_Path *)(ptr->data);

  /* set the driver type, then clear the id-block if the type is invalid */
  data->idtype = value;
  if ((data->id) && (GS(data->id->name) != data->idtype))
    data->id = NULL;
}

static void rna_ksPath_RnaPath_get(PointerRNA *ptr, char *value)
{
  KS_Path *ksp = (KS_Path *)ptr->data;

  if (ksp->rna_path)
    strcpy(value, ksp->rna_path);
  else
    value[0] = '\0';
}

static int rna_ksPath_RnaPath_length(PointerRNA *ptr)
{
  KS_Path *ksp = (KS_Path *)ptr->data;

  if (ksp->rna_path)
    return strlen(ksp->rna_path);
  else
    return 0;
}

static void rna_ksPath_RnaPath_set(PointerRNA *ptr, const char *value)
{
  KS_Path *ksp = (KS_Path *)ptr->data;

  if (ksp->rna_path)
    MEM_freeN(ksp->rna_path);

  if (value[0])
    ksp->rna_path = BLI_strdup(value);
  else
    ksp->rna_path = NULL;
}

/* ****************************** */

static void rna_KeyingSet_name_set(PointerRNA *ptr, const char *value)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;

  /* update names of corresponding groups if name changes */
  if (!STREQ(ks->name, value)) {
    KS_Path *ksp;

    for (ksp = ks->paths.first; ksp; ksp = ksp->next) {
      if ((ksp->groupmode == KSP_GROUP_KSNAME) && (ksp->id)) {
        AnimData *adt = BKE_animdata_from_id(ksp->id);

        /* TODO: NLA strips? */
        if (adt && adt->action) {
          bActionGroup *agrp;

          /* lazy check - should really find the F-Curve for the affected path and check its group
           * but this way should be faster and work well for most cases, as long as there are no
           * conflicts
           */
          for (agrp = adt->action->groups.first; agrp; agrp = agrp->next) {
            if (STREQ(ks->name, agrp->name)) {
              /* there should only be one of these in the action, so can stop... */
              BLI_strncpy(agrp->name, value, sizeof(agrp->name));
              break;
            }
          }
        }
      }
    }
  }

  /* finally, update name to new value */
  BLI_strncpy(ks->name, value, sizeof(ks->name));
}

static int rna_KeyingSet_active_ksPath_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  KeyingSet *ks = (KeyingSet *)ptr->data;

  /* only editable if there are some paths to change to */
  return (BLI_listbase_is_empty(&ks->paths) == false) ? PROP_EDITABLE : 0;
}

static PointerRNA rna_KeyingSet_active_ksPath_get(PointerRNA *ptr)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  return rna_pointer_inherit_refine(
      ptr, &RNA_KeyingSetPath, BLI_findlink(&ks->paths, ks->active_path - 1));
}

static void rna_KeyingSet_active_ksPath_set(PointerRNA *ptr,
                                            PointerRNA value,
                                            struct ReportList *UNUSED(reports))
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  KS_Path *ksp = (KS_Path *)value.data;
  ks->active_path = BLI_findindex(&ks->paths, ksp) + 1;
}

static int rna_KeyingSet_active_ksPath_index_get(PointerRNA *ptr)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  return MAX2(ks->active_path - 1, 0);
}

static void rna_KeyingSet_active_ksPath_index_set(PointerRNA *ptr, int value)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  ks->active_path = value + 1;
}

static void rna_KeyingSet_active_ksPath_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  KeyingSet *ks = (KeyingSet *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&ks->paths) - 1);
}

static PointerRNA rna_KeyingSet_typeinfo_get(PointerRNA *ptr)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  KeyingSetInfo *ksi = NULL;

  /* keying set info is only for builtin Keying Sets */
  if ((ks->flag & KEYINGSET_ABSOLUTE) == 0)
    ksi = ANIM_keyingset_info_find_name(ks->typeinfo);
  return rna_pointer_inherit_refine(ptr, &RNA_KeyingSetInfo, ksi);
}

static KS_Path *rna_KeyingSet_paths_add(KeyingSet *keyingset,
                                        ReportList *reports,
                                        ID *id,
                                        const char rna_path[],
                                        int index,
                                        int group_method,
                                        const char group_name[])
{
  KS_Path *ksp = NULL;
  short flag = 0;

  /* Special case when index = -1, we key the whole array
   * (as with other places where index is used). */
  if (index == -1) {
    flag |= KSP_FLAG_WHOLE_ARRAY;
    index = 0;
  }

  /* if data is valid, call the API function for this */
  if (keyingset) {
    ksp = BKE_keyingset_add_path(keyingset, id, group_name, rna_path, index, flag, group_method);
    keyingset->active_path = BLI_listbase_count(&keyingset->paths);
  }
  else {
    BKE_report(reports, RPT_ERROR, "Keying set path could not be added");
  }

  /* return added path */
  return ksp;
}

static void rna_KeyingSet_paths_remove(KeyingSet *keyingset,
                                       ReportList *reports,
                                       PointerRNA *ksp_ptr)
{
  KS_Path *ksp = ksp_ptr->data;

  /* if data is valid, call the API function for this */
  if ((keyingset && ksp) == false) {
    BKE_report(reports, RPT_ERROR, "Keying set path could not be removed");
    return;
  }

  /* remove the active path from the KeyingSet */
  BKE_keyingset_free_path(keyingset, ksp);
  RNA_POINTER_INVALIDATE(ksp_ptr);

  /* the active path number will most likely have changed */
  /* TODO: we should get more fancy and actually check if it was removed,
   * but this will do for now */
  keyingset->active_path = 0;
}

static void rna_KeyingSet_paths_clear(KeyingSet *keyingset, ReportList *reports)
{
  /* if data is valid, call the API function for this */
  if (keyingset) {
    KS_Path *ksp, *kspn;

    /* free each path as we go to avoid looping twice */
    for (ksp = keyingset->paths.first; ksp; ksp = kspn) {
      kspn = ksp->next;
      BKE_keyingset_free_path(keyingset, ksp);
    }

    /* reset the active path, since there aren't any left */
    keyingset->active_path = 0;
  }
  else {
    BKE_report(reports, RPT_ERROR, "Keying set paths could not be removed");
  }
}

/* needs wrapper function to push notifier */
static NlaTrack *rna_NlaTrack_new(ID *id, AnimData *adt, Main *bmain, bContext *C, NlaTrack *track)
{
  NlaTrack *new_track = BKE_nlatrack_add(adt, track);

  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION | ID_RECALC_COPY_ON_WRITE);

  return new_track;
}

static void rna_NlaTrack_remove(
    ID *id, AnimData *adt, Main *bmain, bContext *C, ReportList *reports, PointerRNA *track_ptr)
{
  NlaTrack *track = track_ptr->data;

  if (BLI_findindex(&adt->nla_tracks, track) == -1) {
    BKE_reportf(reports, RPT_ERROR, "NlaTrack '%s' cannot be removed", track->name);
    return;
  }

  BKE_nlatrack_free(&adt->nla_tracks, track, true);
  RNA_POINTER_INVALIDATE(track_ptr);

  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, NULL);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION | ID_RECALC_COPY_ON_WRITE);
}

static PointerRNA rna_NlaTrack_active_get(PointerRNA *ptr)
{
  AnimData *adt = (AnimData *)ptr->data;
  NlaTrack *track = BKE_nlatrack_find_active(&adt->nla_tracks);
  return rna_pointer_inherit_refine(ptr, &RNA_NlaTrack, track);
}

static void rna_NlaTrack_active_set(PointerRNA *ptr,
                                    PointerRNA value,
                                    struct ReportList *UNUSED(reports))
{
  AnimData *adt = (AnimData *)ptr->data;
  NlaTrack *track = (NlaTrack *)value.data;
  BKE_nlatrack_set_active(&adt->nla_tracks, track);
}

static FCurve *rna_Driver_from_existing(AnimData *adt, bContext *C, FCurve *src_driver)
{
  /* verify that we've got a driver to duplicate */
  if (ELEM(NULL, src_driver, src_driver->driver)) {
    BKE_report(CTX_wm_reports(C), RPT_ERROR, "No valid driver data to create copy of");
    return NULL;
  }
  else {
    /* just make a copy of the existing one and add to self */
    FCurve *new_fcu = copy_fcurve(src_driver);

    /* XXX: if we impose any ordering on these someday, this will be problematic */
    BLI_addtail(&adt->drivers, new_fcu);
    return new_fcu;
  }
}

static FCurve *rna_Driver_new(
    ID *id, AnimData *adt, Main *bmain, ReportList *reports, const char *rna_path, int array_index)
{
  if (rna_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return NULL;
  }

  if (list_find_fcurve(&adt->drivers, rna_path, array_index)) {
    BKE_reportf(reports, RPT_ERROR, "Driver '%s[%d]' already exists", rna_path, array_index);
    return NULL;
  }

  short add_mode = 1;
  FCurve *fcu = verify_driver_fcurve(id, rna_path, array_index, add_mode);
  BLI_assert(fcu != NULL);

  DEG_relations_tag_update(bmain);

  return fcu;
}

static void rna_Driver_remove(AnimData *adt, Main *bmain, ReportList *reports, FCurve *fcu)
{
  if (!BLI_remlink_safe(&adt->drivers, fcu)) {
    BKE_report(reports, RPT_ERROR, "Driver not found in this animation data");
    return;
  }
  free_fcurve(fcu);
  DEG_relations_tag_update(bmain);
}

static FCurve *rna_Driver_find(AnimData *adt,
                               ReportList *reports,
                               const char *data_path,
                               int index)
{
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return NULL;
  }

  /* Returns NULL if not found. */
  return list_find_fcurve(&adt->drivers, data_path, index);
}

bool rna_AnimaData_override_apply(Main *UNUSED(bmain),
                                  PointerRNA *ptr_dst,
                                  PointerRNA *ptr_src,
                                  PointerRNA *ptr_storage,
                                  PropertyRNA *prop_dst,
                                  PropertyRNA *prop_src,
                                  PropertyRNA *UNUSED(prop_storage),
                                  const int len_dst,
                                  const int len_src,
                                  const int len_storage,
                                  PointerRNA *UNUSED(ptr_item_dst),
                                  PointerRNA *UNUSED(ptr_item_src),
                                  PointerRNA *UNUSED(ptr_item_storage),
                                  IDOverrideStaticPropertyOperation *opop)
{
  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert(opop->operation == IDOVERRIDESTATIC_OP_REPLACE &&
             "Unsupported RNA override operation on animdata pointer");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* AnimData is a special case, since you cannot edit/replace it, it's either existent or not. */
  AnimData *adt_dst = RNA_property_pointer_get(ptr_dst, prop_dst).data;
  AnimData *adt_src = RNA_property_pointer_get(ptr_src, prop_src).data;

  if (adt_dst == NULL && adt_src != NULL) {
    /* Copy anim data from reference into final local ID. */
    BKE_animdata_copy_id(NULL, ptr_dst->id.data, ptr_src->id.data, 0);
    return true;
  }
  else if (adt_dst != NULL && adt_src == NULL) {
    /* Override has cleared/removed anim data from its reference. */
    BKE_animdata_free(ptr_dst->id.data, true);
    return true;
  }

  return false;
}

#else

/* helper function for Keying Set -> keying settings */
static void rna_def_common_keying_flags(StructRNA *srna, short reg)
{
  PropertyRNA *prop;

  /* override scene/userpref defaults? */
  prop = RNA_def_property(srna, "use_insertkey_override_needed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "keyingoverride", INSERTKEY_NEEDED);
  RNA_def_property_ui_text(prop,
                           "Override Insert Keyframes Default- Only Needed",
                           "Override default setting to only insert keyframes where they're "
                           "needed in the relevant F-Curves");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  prop = RNA_def_property(srna, "use_insertkey_override_visual", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "keyingoverride", INSERTKEY_MATRIX);
  RNA_def_property_ui_text(
      prop,
      "Override Insert Keyframes Default - Visual",
      "Override default setting to insert keyframes based on 'visual transforms'");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  prop = RNA_def_property(srna, "use_insertkey_override_xyz_to_rgb", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "keyingoverride", INSERTKEY_XYZ2RGB);
  RNA_def_property_ui_text(
      prop,
      "Override F-Curve Colors - XYZ to RGB",
      "Override default setting to set color for newly added transformation F-Curves "
      "(Location, Rotation, Scale) to be based on the transform axis");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  /* value to override defaults with */
  prop = RNA_def_property(srna, "use_insertkey_needed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_NEEDED);
  RNA_def_property_ui_text(prop,
                           "Insert Keyframes - Only Needed",
                           "Only insert keyframes where they're needed in the relevant F-Curves");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  prop = RNA_def_property(srna, "use_insertkey_visual", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_MATRIX);
  RNA_def_property_ui_text(
      prop, "Insert Keyframes - Visual", "Insert keyframes based on 'visual transforms'");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  prop = RNA_def_property(srna, "use_insertkey_xyz_to_rgb", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "keyingflag", INSERTKEY_XYZ2RGB);
  RNA_def_property_ui_text(prop,
                           "F-Curve Colors - XYZ to RGB",
                           "Color for newly added transformation F-Curves (Location, Rotation, "
                           "Scale) is based on the transform axis");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }
}

/* --- */

/* To avoid repeating it twice! */
#  define KEYINGSET_IDNAME_DOC \
    "If this is set, the Keying Set gets a custom ID, otherwise it takes " \
    "the name of the class used to define the Keying Set (for example, " \
    "if the class name is \"BUILTIN_KSI_location\", and bl_idname is not " \
    "set by the script, then bl_idname = \"BUILTIN_KSI_location\")"

static void rna_def_keyingset_info(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "KeyingSetInfo", NULL);
  RNA_def_struct_sdna(srna, "KeyingSetInfo");
  RNA_def_struct_ui_text(
      srna, "Keying Set Info", "Callback function defines for builtin Keying Sets");
  RNA_def_struct_refine_func(srna, "rna_KeyingSetInfo_refine");
  RNA_def_struct_register_funcs(
      srna, "rna_KeyingSetInfo_register", "rna_KeyingSetInfo_unregister", NULL);

  /* Properties --------------------- */

  RNA_define_verify_sdna(0); /* not in sdna */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", KEYINGSET_IDNAME_DOC);

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_ui_text(prop, "UI Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_flag(prop, PROP_REGISTER);

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "description");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Description", "A short description of the keying set");

  /* Regarding why we don't use rna_def_common_keying_flags() here:
   * - Using it would keep this case in sync with the other places
   *   where these options are exposed (which are optimized for being
   *   used in the UI).
   * - Unlike all the other places, this case is used for defining
   *   new "built in" Keying Sets via the Python API. In that case,
   *   it makes more sense to expose these in a way more similar to
   *   other places featuring bl_idname/label/description (i.e. operators)
   */
  prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "keyingflag");
  RNA_def_property_enum_items(prop, rna_enum_keying_flag_items);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_ui_text(prop, "Options", "Keying Set options to use when inserting keyframes");

  RNA_define_verify_sdna(1);

  /* Function Callbacks ------------- */
  /* poll */
  func = RNA_def_function(srna, "poll", NULL);
  RNA_def_function_ui_description(func, "Test if Keying Set can be used or not");
  RNA_def_function_flag(func, FUNC_REGISTER);
  RNA_def_function_return(func, RNA_def_boolean(func, "ok", 1, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  /* iterator */
  func = RNA_def_function(srna, "iterator", NULL);
  RNA_def_function_ui_description(
      func, "Call generate() on the structs which have properties to be keyframed");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "ks", "KeyingSet", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  /* generate */
  func = RNA_def_function(srna, "generate", NULL);
  RNA_def_function_ui_description(
      func, "Add Paths to the Keying Set to keyframe the properties of the given data");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "ks", "KeyingSet", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "data", "AnyType", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
}

static void rna_def_keyingset_path(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "KeyingSetPath", NULL);
  RNA_def_struct_sdna(srna, "KS_Path");
  RNA_def_struct_ui_text(srna, "Keying Set Path", "Path to a setting for use in a Keying Set");

  /* ID */
  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_ksPath_id_editable");
  RNA_def_property_pointer_funcs(prop, NULL, NULL, "rna_ksPath_id_typef", NULL);
  RNA_def_property_ui_text(prop,
                           "ID-Block",
                           "ID-Block that keyframes for Keying Set should be added to "
                           "(for Absolute Keying Sets only)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */

  prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "idtype");
  RNA_def_property_enum_items(prop, rna_enum_id_type_items);
  RNA_def_property_enum_default(prop, ID_OB);
  RNA_def_property_enum_funcs(prop, NULL, "rna_ksPath_id_type_set", NULL);
  RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */

  /* Group */
  prop = RNA_def_property(srna, "group", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Group Name", "Name of Action Group to assign setting(s) for this path to");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */

  /* Grouping */
  prop = RNA_def_property(srna, "group_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "groupmode");
  RNA_def_property_enum_items(prop, rna_enum_keyingset_path_grouping_items);
  RNA_def_property_ui_text(
      prop, "Grouping Method", "Method used to define which Group-name to use");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */

  /* Path + Array Index */
  prop = RNA_def_property(srna, "data_path", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_ksPath_RnaPath_get", "rna_ksPath_RnaPath_length", "rna_ksPath_RnaPath_set");
  RNA_def_property_ui_text(prop, "Data Path", "Path to property setting");
  RNA_def_struct_name_property(srna, prop); /* XXX this is the best indicator for now... */
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL);

  /* called 'index' when given as function arg */
  prop = RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "RNA Array Index", "Index to the specific setting if applicable");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */

  /* Flags */
  prop = RNA_def_property(srna, "use_entire_array", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", KSP_FLAG_WHOLE_ARRAY);
  RNA_def_property_ui_text(
      prop,
      "Entire Array",
      "When an 'array/vector' type is chosen (Location, Rotation, Color, etc.), "
      "entire array is to be used");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, NULL); /* XXX: maybe a bit too noisy */

  /* Keyframing Settings */
  rna_def_common_keying_flags(srna, 0);
}

/* keyingset.paths */
static void rna_def_keyingset_paths(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "KeyingSetPaths");
  srna = RNA_def_struct(brna, "KeyingSetPaths", NULL);
  RNA_def_struct_sdna(srna, "KeyingSet");
  RNA_def_struct_ui_text(srna, "Keying set paths", "Collection of keying set paths");

  /* Add Path */
  func = RNA_def_function(srna, "add", "rna_KeyingSet_paths_add");
  RNA_def_function_ui_description(func, "Add a new path for the Keying Set");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  /* return arg */
  parm = RNA_def_pointer(
      func, "ksp", "KeyingSetPath", "New Path", "Path created and added to the Keying Set");
  RNA_def_function_return(func, parm);
  /* ID-block for target */
  parm = RNA_def_pointer(
      func, "target_id", "ID", "Target ID", "ID data-block for the destination");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* rna-path */
  /* XXX hopefully this is long enough */
  parm = RNA_def_string(
      func, "data_path", NULL, 256, "Data-Path", "RNA-Path to destination property");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* index (defaults to -1 for entire array) */
  RNA_def_int(func,
              "index",
              -1,
              -1,
              INT_MAX,
              "Index",
              "The index of the destination property (i.e. axis of Location/Rotation/etc.), "
              "or -1 for the entire array",
              0,
              INT_MAX);
  /* grouping */
  RNA_def_enum(func,
               "group_method",
               rna_enum_keyingset_path_grouping_items,
               KSP_GROUP_KSNAME,
               "Grouping Method",
               "Method used to define which Group-name to use");
  RNA_def_string(
      func,
      "group_name",
      NULL,
      64,
      "Group Name",
      "Name of Action Group to assign destination to (only if grouping mode is to use this name)");

  /* Remove Path */
  func = RNA_def_function(srna, "remove", "rna_KeyingSet_paths_remove");
  RNA_def_function_ui_description(func, "Remove the given path from the Keying Set");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  /* path to remove */
  parm = RNA_def_pointer(func, "path", "KeyingSetPath", "Path", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  /* Remove All Paths */
  func = RNA_def_function(srna, "clear", "rna_KeyingSet_paths_clear");
  RNA_def_function_ui_description(func, "Remove all the paths from the Keying Set");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyingSetPath");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_KeyingSet_active_ksPath_editable");
  RNA_def_property_pointer_funcs(
      prop, "rna_KeyingSet_active_ksPath_get", "rna_KeyingSet_active_ksPath_set", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "active_path");
  RNA_def_property_int_funcs(prop,
                             "rna_KeyingSet_active_ksPath_index_get",
                             "rna_KeyingSet_active_ksPath_index_set",
                             "rna_KeyingSet_active_ksPath_index_range");
  RNA_def_property_ui_text(prop, "Active Path Index", "Current Keying Set index");
}

static void rna_def_keyingset(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "KeyingSet", NULL);
  RNA_def_struct_ui_text(srna, "Keying Set", "Settings that should be keyframed together");

  /* Id/Label */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", KEYINGSET_IDNAME_DOC);
  /* NOTE: disabled, as ID name shouldn't be editable */
#  if 0
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_RENAME, NULL);
#  endif

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_KeyingSet_name_set");
  RNA_def_property_ui_text(prop, "UI Name", "");
  RNA_def_struct_ui_icon(srna, ICON_KEYINGSET);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_RENAME, NULL);

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "description");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_ui_text(prop, "Description", "A short description of the keying set");

  /* KeyingSetInfo (Type Info) for Builtin Sets only  */
  prop = RNA_def_property(srna, "type_info", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyingSetInfo");
  RNA_def_property_pointer_funcs(prop, "rna_KeyingSet_typeinfo_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Type Info", "Callback function defines for built-in Keying Sets");

  /* Paths */
  prop = RNA_def_property(srna, "paths", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "paths", NULL);
  RNA_def_property_struct_type(prop, "KeyingSetPath");
  RNA_def_property_ui_text(
      prop, "Paths", "Keying Set Paths to define settings that get keyframed together");
  rna_def_keyingset_paths(brna, prop);

  /* Flags */
  prop = RNA_def_property(srna, "is_path_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYINGSET_ABSOLUTE);
  RNA_def_property_ui_text(prop,
                           "Absolute",
                           "Keying Set defines specific paths/settings to be keyframed "
                           "(i.e. is not reliant on context info)");

  /* Keyframing Flags */
  rna_def_common_keying_flags(srna, 0);

  /* Keying Set API */
  RNA_api_keyingset(srna);
}

#  undef KEYINGSET_IDNAME_DOC
/* --- */

static void rna_api_animdata_nla_tracks(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "NlaTracks");
  srna = RNA_def_struct(brna, "NlaTracks", NULL);
  RNA_def_struct_sdna(srna, "AnimData");
  RNA_def_struct_ui_text(srna, "NLA Tracks", "Collection of NLA Tracks");

  func = RNA_def_function(srna, "new", "rna_NlaTrack_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new NLA Track");
  RNA_def_pointer(func, "prev", "NlaTrack", "", "NLA Track to add the new one after");
  /* return type */
  parm = RNA_def_pointer(func, "track", "NlaTrack", "", "New NLA Track");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NlaTrack_remove");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_MAIN | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove a NLA Track");
  parm = RNA_def_pointer(func, "track", "NlaTrack", "", "NLA Track to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NlaTrack");
  RNA_def_property_pointer_funcs(
      prop, "rna_NlaTrack_active_get", "rna_NlaTrack_active_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Constraint", "Active Object constraint");
  /* XXX: should (but doesn't) update the active track in the NLA window */
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_SELECTED, NULL);
}

static void rna_api_animdata_drivers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  /* PropertyRNA *prop; */

  RNA_def_property_srna(cprop, "AnimDataDrivers");
  srna = RNA_def_struct(brna, "AnimDataDrivers", NULL);
  RNA_def_struct_sdna(srna, "AnimData");
  RNA_def_struct_ui_text(srna, "Drivers", "Collection of Driver F-Curves");

  /* Match: ActionFCurves.new/remove */

  /* AnimData.drivers.new(...) */
  func = RNA_def_function(srna, "new", "rna_Driver_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_MAIN);
  parm = RNA_def_string(func, "data_path", NULL, 0, "Data Path", "F-Curve data path to use");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  /* return type */
  parm = RNA_def_pointer(func, "driver", "FCurve", "", "Newly Driver F-Curve");
  RNA_def_function_return(func, parm);

  /* AnimData.drivers.remove(...) */
  func = RNA_def_function(srna, "remove", "rna_Driver_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "driver", "FCurve", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* AnimData.drivers.from_existing(...) */
  func = RNA_def_function(srna, "from_existing", "rna_Driver_from_existing");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new driver given an existing one");
  RNA_def_pointer(func,
                  "src_driver",
                  "FCurve",
                  "",
                  "Existing Driver F-Curve to use as template for a new one");
  /* return type */
  parm = RNA_def_pointer(func, "driver", "FCurve", "", "New Driver F-Curve");
  RNA_def_function_return(func, parm);

  /* AnimData.drivers.find(...) */
  func = RNA_def_function(srna, "find", "rna_Driver_find");
  RNA_def_function_ui_description(
      func,
      "Find a driver F-Curve. Note that this function performs a linear scan "
      "of all driver F-Curves.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", NULL, 0, "Data Path", "F-Curve data path");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  /* return type */
  parm = RNA_def_pointer(
      func, "fcurve", "FCurve", "", "The found F-Curve, or None if it doesn't exist");
  RNA_def_function_return(func, parm);
}

void rna_def_animdata_common(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "animation_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "adt");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_override_funcs(prop, NULL, NULL, "rna_AnimaData_override_apply");
  RNA_def_property_ui_text(prop, "Animation Data", "Animation data for this data-block");
}

static void rna_def_animdata(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AnimData", NULL);
  RNA_def_struct_ui_text(srna, "Animation Data", "Animation data for data-block");
  RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);

  /* NLA */
  prop = RNA_def_property(srna, "nla_tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "nla_tracks", NULL);
  RNA_def_property_struct_type(prop, "NlaTrack");
  RNA_def_property_ui_text(prop, "NLA Tracks", "NLA Tracks (i.e. Animation Layers)");

  rna_api_animdata_nla_tracks(brna, prop);

  /* Active Action */
  prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
  /* this flag as well as the dynamic test must be defined for this to be editable... */
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_AnimData_action_set", NULL, "rna_Action_id_poll");
  RNA_def_property_editable_func(prop, "rna_AnimData_action_editable");
  RNA_def_property_ui_text(prop, "Action", "Active Action for this data-block");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_AnimData_dependency_update");

  /* Active Action Settings */
  prop = RNA_def_property(srna, "action_extrapolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "act_extendmode");
  RNA_def_property_enum_items(prop, rna_enum_nla_mode_extend_items);
  RNA_def_property_ui_text(
      prop,
      "Action Extrapolation",
      "Action to take for gaps past the Active Action's range (when evaluating with NLA)");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update");

  prop = RNA_def_property(srna, "action_blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "act_blendmode");
  RNA_def_property_enum_items(prop, rna_enum_nla_mode_blend_items);
  RNA_def_property_ui_text(
      prop,
      "Action Blending",
      "Method used for combining Active Action's result with result of NLA stack");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update"); /* this will do? */

  prop = RNA_def_property(srna, "action_influence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "act_influence");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Action Influence",
                           "Amount the Active Action contributes to the result of the NLA stack");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update"); /* this will do? */

  /* Drivers */
  prop = RNA_def_property(srna, "drivers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "drivers", NULL);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_STATIC);
  RNA_def_property_ui_text(prop, "Drivers", "The Drivers/Expressions for this data-block");

  rna_api_animdata_drivers(brna, prop);

  /* General Settings */
  prop = RNA_def_property(srna, "use_nla", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", ADT_NLA_EVAL_OFF);
  RNA_def_property_ui_text(
      prop, "NLA Evaluation Enabled", "NLA stack is evaluated when evaluating this block");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update"); /* this will do? */

  prop = RNA_def_property(srna, "use_tweak_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", ADT_NLA_EDIT_ON);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_AnimData_tweakmode_set");
  RNA_def_property_ui_text(
      prop, "Use NLA Tweak Mode", "Whether to enable or disable tweak mode in NLA");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update");

  /* Animation Data API */
  RNA_api_animdata(srna);
}

/* --- */

void RNA_def_animation(BlenderRNA *brna)
{
  rna_def_animdata(brna);

  rna_def_keyingset(brna);
  rna_def_keyingset_path(brna);
  rna_def_keyingset_info(brna);
}

#endif
