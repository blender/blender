/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DEG_depsgraph.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "WM_api.hh"

/* Keying Set Type Info declarations. */
static ListBase keyingset_type_infos = {nullptr, nullptr};
ListBase builtin_keyingsets = {nullptr, nullptr};

namespace blender::animrig {

void keyingset_info_register(KeyingSetInfo *keyingset_info)
{
  /* Create a new KeyingSet
   * - inherit name and keyframing settings from the typeinfo
   */
  KeyingSet *keyingset = BKE_keyingset_add(&builtin_keyingsets,
                                           keyingset_info->idname,
                                           keyingset_info->name,
                                           1,
                                           keyingset_info->keyingflag);

  /* Link this KeyingSet with its typeinfo. */
  memcpy(&keyingset->typeinfo, keyingset_info->idname, sizeof(keyingset->typeinfo));

  /* Copy description. */
  STRNCPY(keyingset->description, keyingset_info->description);

  /* Add type-info to the list. */
  BLI_addtail(&keyingset_type_infos, keyingset_info);
}

void keyingset_info_unregister(Main *bmain, KeyingSetInfo *keyingset_info)
{
  /* Find relevant builtin KeyingSets which use this, and remove them. */
  /* TODO: this isn't done now, since unregister is really only used at the moment when we
   * reload the scripts, which kind of defeats the purpose of "builtin"? */
  LISTBASE_FOREACH_MUTABLE (KeyingSet *, keyingset, &builtin_keyingsets) {
    /* Remove if matching typeinfo name. */
    if (!STREQ(keyingset->typeinfo, keyingset_info->idname)) {
      continue;
    }
    Scene *scene;
    BKE_keyingset_free_paths(keyingset);
    BLI_remlink(&builtin_keyingsets, keyingset);

    for (scene = static_cast<Scene *>(bmain->scenes.first); scene;
         scene = static_cast<Scene *>(scene->id.next))
    {
      BLI_remlink_safe(&scene->keyingsets, keyingset);
    }

    MEM_freeN(keyingset);
  }

  BLI_freelinkN(&keyingset_type_infos, keyingset_info);
}

void keyingset_infos_exit()
{
  /* Free type infos. */
  LISTBASE_FOREACH_MUTABLE (KeyingSetInfo *, keyingset_info, &keyingset_type_infos) {
    /* Free extra RNA data, and remove from list. */
    if (keyingset_info->rna_ext.free) {
      keyingset_info->rna_ext.free(keyingset_info->rna_ext.data);
    }
    BLI_freelinkN(&keyingset_type_infos, keyingset_info);
  }

  BKE_keyingsets_free(&builtin_keyingsets);
}

bool keyingset_find_id(KeyingSet *keyingset, ID *id)
{
  if (ELEM(nullptr, keyingset, id)) {
    return false;
  }

  return BLI_findptr(&keyingset->paths, id, offsetof(KS_Path, id)) != nullptr;
}

KeyingSetInfo *keyingset_info_find_name(const char name[])
{
  if ((name == nullptr) || (name[0] == 0)) {
    return nullptr;
  }

  /* Search by comparing names. */
  return static_cast<KeyingSetInfo *>(
      BLI_findstring(&keyingset_type_infos, name, offsetof(KeyingSetInfo, idname)));
}

KeyingSet *builtin_keyingset_get_named(const char name[])
{
  if (name[0] == 0) {
    return nullptr;
  }

  /* Loop over KeyingSets checking names. */
  LISTBASE_FOREACH (KeyingSet *, keyingset, &builtin_keyingsets) {
    if (STREQ(name, keyingset->idname)) {
      return keyingset;
    }
  }

/* Complain about missing keying sets on debug builds. */
#ifndef NDEBUG
  printf("%s: '%s' not found\n", __func__, name);
#endif

  return nullptr;
}

KeyingSet *get_keyingset_for_autokeying(const Scene *scene, const char *transformKSName)
{
  /* Get KeyingSet to use
   * - use the active KeyingSet if defined (and user wants to use it for all autokeying),
   *   or otherwise key transforms only
   */
  if (is_keying_flag(scene, AUTOKEY_FLAG_ONLYKEYINGSET) && (scene->active_keyingset)) {
    return scene_get_active_keyingset(scene);
  }

  if (is_keying_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE)) {
    return builtin_keyingset_get_named(ANIM_KS_AVAILABLE_ID);
  }

  return builtin_keyingset_get_named(transformKSName);
}

KeyingSet *scene_get_active_keyingset(const Scene *scene)
{
  /* If no scene, we've got no hope of finding the Keying Set. */
  if (scene == nullptr) {
    return nullptr;
  }

  /* Currently, there are several possibilities here:
   * -   0: no active keying set
   * - > 0: one of the user-defined Keying Sets, but indices start from 0 (hence the -1)
   * - < 0: a builtin keying set
   */
  if (scene->active_keyingset > 0) {
    return static_cast<KeyingSet *>(BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1));
  }
  return static_cast<KeyingSet *>(
      BLI_findlink(&builtin_keyingsets, (-scene->active_keyingset) - 1));
}

void relative_keyingset_add_source(blender::Vector<PointerRNA> &sources,
                                   ID *id,
                                   StructRNA *srna,
                                   void *data)
{
  if (ELEM(nullptr, srna, data, id)) {
    return;
  }
  sources.append(RNA_pointer_create_discrete(id, srna, data));
}

void relative_keyingset_add_source(blender::Vector<PointerRNA> &sources, ID *id)
{
  if (id == nullptr) {
    return;
  }
  sources.append(RNA_id_pointer_create(id));
}

/* Special 'Overrides' Iterator for Relative KeyingSets ------ */

/* Iterator used for overriding the behavior of iterators defined for
 * relative Keying Sets, with the main usage of this being operators
 * requiring Auto Keyframing. Internal Use Only!
 */
static void RKS_ITER_overrides_list(KeyingSetInfo *keyingset_info,
                                    bContext *C,
                                    KeyingSet *keyingset,
                                    blender::Vector<PointerRNA> &sources)
{
  for (PointerRNA ptr : sources) {
    /* Run generate callback on this data. */
    keyingset_info->generate(keyingset_info, C, keyingset, &ptr);
  }
}

ModifyKeyReturn validate_keyingset(bContext *C,
                                   blender::Vector<PointerRNA> *sources,
                                   KeyingSet *keyingset)
{
  if (keyingset == nullptr) {
    return ModifyKeyReturn::SUCCESS;
  }

  /* If relative Keying Sets, poll and build up the paths. */
  if (keyingset->flag & KEYINGSET_ABSOLUTE) {
    return ModifyKeyReturn::SUCCESS;
  }

  KeyingSetInfo *keyingset_info = keyingset_info_find_name(keyingset->typeinfo);

  /* Clear all existing paths
   * NOTE: BKE_keyingset_free_paths() frees all of the paths for the KeyingSet, but not the set
   * itself.
   */
  BKE_keyingset_free_paths(keyingset);

  /* Get the associated 'type info' for this KeyingSet. */
  if (keyingset_info == nullptr) {
    return ModifyKeyReturn::MISSING_TYPEINFO;
  }
  /* TODO: check for missing callbacks! */

  /* Check if it can be used in the current context. */
  if (!keyingset_info->poll(keyingset_info, C)) {
    /* Poll callback tells us that KeyingSet is useless in current context. */
    /* FIXME: the poll callback needs to give us more info why. */
    return ModifyKeyReturn::INVALID_CONTEXT;
  }

  /* If a list of data sources are provided, run a special iterator over them,
   * otherwise, just continue per normal.
   */
  if (sources != nullptr) {
    RKS_ITER_overrides_list(keyingset_info, C, keyingset, *sources);
  }
  else {
    keyingset_info->iter(keyingset_info, C, keyingset);
  }

  /* If we don't have any paths now, then this still qualifies as invalid context. */
  /* FIXME: we need some error conditions (to be retrieved from the iterator why this failed!)
   */
  if (BLI_listbase_is_empty(&keyingset->paths)) {
    return ModifyKeyReturn::INVALID_CONTEXT;
  }

  return ModifyKeyReturn::SUCCESS;
}

/* Determine which keying flags apply based on the override flags. */
static eInsertKeyFlags keyingset_apply_keying_flags(const eInsertKeyFlags base_flags,
                                                    const eInsertKeyFlags overrides,
                                                    const eInsertKeyFlags own_flags)
{
  /* Pass through all flags by default (i.e. even not explicitly listed ones). */
  eInsertKeyFlags result = base_flags;

/* The logic for whether a keying flag applies is as follows:
 * - If the flag in question is set in "overrides", that means that the
 *   status of that flag in "own_flags" is used
 * - If however the flag isn't set, then its value in "base_flags" is used
 *   instead (i.e. no override)
 */
#define APPLY_KEYINGFLAG_OVERRIDE(kflag) \
  if (overrides & kflag) { \
    result &= ~kflag; \
    result |= (own_flags & kflag); \
  }

  /* Apply the flags one by one...
   * (See rna_def_common_keying_flags() for the supported flags)
   */
  APPLY_KEYINGFLAG_OVERRIDE(INSERTKEY_NEEDED)
  APPLY_KEYINGFLAG_OVERRIDE(INSERTKEY_MATRIX)

#undef APPLY_KEYINGFLAG_OVERRIDE

  return result;
}

static int insert_key_to_keying_set_path(bContext *C,
                                         KS_Path *keyingset_path,
                                         KeyingSet *keyingset,
                                         const eInsertKeyFlags insert_key_flags,
                                         const ModifyKeyMode mode,
                                         const float frame)
{
  /* Since keying settings can be defined on the paths too,
   * apply the settings for this path first. */
  const eInsertKeyFlags path_insert_key_flags = keyingset_apply_keying_flags(
      insert_key_flags,
      eInsertKeyFlags(keyingset_path->keyingoverride),
      eInsertKeyFlags(keyingset_path->keyingflag));

  const char *groupname = nullptr;
  /* Get pointer to name of group to add channels to. */
  if (keyingset_path->groupmode == KSP_GROUP_NONE) {
    groupname = nullptr;
  }
  else if (keyingset_path->groupmode == KSP_GROUP_KSNAME) {
    groupname = keyingset->name;
  }
  else {
    groupname = keyingset_path->group;
  }

  /* Init - array_length should be greater than array_index so that
   * normal non-array entries get keyframed correctly.
   */
  int array_index = keyingset_path->array_index;
  int array_length = array_index;

  /* Get length of array if whole array option is enabled. */
  if (keyingset_path->flag & KSP_FLAG_WHOLE_ARRAY) {
    PointerRNA ptr;
    PropertyRNA *prop;

    PointerRNA id_ptr = RNA_id_pointer_create(keyingset_path->id);
    if (RNA_path_resolve_property(&id_ptr, keyingset_path->rna_path, &ptr, &prop)) {
      array_length = RNA_property_array_length(&ptr, prop);
      /* Start from start of array, instead of the previously specified index - #48020 */
      array_index = 0;
    }
  }

  /* We should do at least one step. */
  if (array_length == array_index) {
    array_length++;
  }

  Main *bmain = CTX_data_main(C);
  ReportList *reports = CTX_wm_reports(C);
  Scene *scene = CTX_data_scene(C);
  const eBezTriple_KeyframeType keytype = eBezTriple_KeyframeType(
      scene->toolsettings->keyframe_type);
  /* For each possible index, perform operation
   * - Assume that array-length is greater than index. */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    frame);
  int keyed_channels = 0;

  CombinedKeyingResult combined_result;
  for (; array_index < array_length; array_index++) {
    if (mode == ModifyKeyMode::INSERT) {
      const std::optional<blender::StringRefNull> group = groupname ? std::optional(groupname) :
                                                                      std::nullopt;
      const std::optional<int> index = array_index >= 0 ? std::optional(array_index) :
                                                          std::nullopt;
      PointerRNA id_rna_pointer = RNA_id_pointer_create(keyingset_path->id);
      CombinedKeyingResult result = insert_keyframes(bmain,
                                                     &id_rna_pointer,
                                                     group,
                                                     {{keyingset_path->rna_path, {}, index}},
                                                     std::nullopt,
                                                     anim_eval_context,
                                                     keytype,
                                                     path_insert_key_flags);
      keyed_channels += result.get_count(SingleKeyingResult::SUCCESS);
      combined_result.merge(result);
    }
    else if (mode == ModifyKeyMode::DELETE_KEY) {
      RNAPath rna_path = {keyingset_path->rna_path, std::nullopt, array_index};
      if (array_index < 0) {
        rna_path.index = std::nullopt;
      }
      keyed_channels += delete_keyframe(bmain, reports, keyingset_path->id, rna_path, frame);
    }
  }

  if (combined_result.get_count(SingleKeyingResult::SUCCESS) == 0) {
    combined_result.generate_reports(reports);
  }

  switch (GS(keyingset_path->id->name)) {
    case ID_OB: /* Object (or Object-Related) Keyframes */
    {
      Object *ob = reinterpret_cast<Object *>(keyingset_path->id);

      /* XXX: only object transforms? */
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      break;
    }
    default:
      DEG_id_tag_update(keyingset_path->id, ID_RECALC_ANIMATION_NO_FLUSH);
      break;
  }

  WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);

  return keyed_channels;
}

int apply_keyingset(bContext *C,
                    blender::Vector<PointerRNA> *sources,
                    KeyingSet *keyingset,
                    const ModifyKeyMode mode,
                    const float cfra)
{
  if (keyingset == nullptr) {
    return 0;
  }

  Scene *scene = CTX_data_scene(C);
  const eInsertKeyFlags base_kflags = get_keyframing_flags(scene);
  eInsertKeyFlags kflag = INSERTKEY_NOFLAGS;
  if (mode == ModifyKeyMode::INSERT) {
    /* Use context settings as base. */
    kflag = keyingset_apply_keying_flags(base_kflags,
                                         eInsertKeyFlags(keyingset->keyingoverride),
                                         eInsertKeyFlags(keyingset->keyingflag));
  }
  else if (mode == ModifyKeyMode::DELETE_KEY) {
    kflag = INSERTKEY_NOFLAGS;
  }

  /* If relative Keying Sets, poll and build up the paths. */
  {
    const ModifyKeyReturn error = validate_keyingset(C, sources, keyingset);
    if (error != ModifyKeyReturn::SUCCESS) {
      BLI_assert(int(error) < 0);
      return int(error);
    }
  }

  ReportList *reports = CTX_wm_reports(C);
  int keyed_channels = 0;

  /* Apply the paths as specified in the KeyingSet now. */
  LISTBASE_FOREACH (KS_Path *, keyingset_path, &keyingset->paths) {
    /* Skip path if no ID pointer is specified. */
    if (keyingset_path->id == nullptr) {
      BKE_reportf(reports,
                  RPT_WARNING,
                  "Skipping path in keying set, as it has no ID (KS = '%s', path = '%s[%d]')",
                  keyingset->name,
                  keyingset_path->rna_path,
                  keyingset_path->array_index);
      continue;
    }

    keyed_channels += insert_key_to_keying_set_path(
        C, keyingset_path, keyingset, kflag, mode, cfra);
  }

  /* Return the number of channels successfully affected. */
  BLI_assert(keyed_channels >= 0);
  return keyed_channels;
}

}  // namespace blender::animrig
