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
 * The Original Code is Copyright (C) 2014 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>

#include "DNA_anim_types.h"

#include "BLI_ghash.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_node.h"

/* status */
enum {
  IDWALK_STOP = 1 << 0,
};

typedef struct LibraryForeachIDData {
  Main *bmain;
  /**
   * 'Real' ID, the one that might be in `bmain`, only differs from self_id when the later is a
   * private one.
   */
  ID *owner_id;
  /**
   * ID from which the current ID pointer is being processed. It may be an embedded ID like master
   * collection or root node tree.
   */
  ID *self_id;

  int flag;
  int cb_flag;
  int cb_flag_clear;
  LibraryIDLinkCallback callback;
  void *user_data;
  int status;

  /* To handle recursion. */
  GSet *ids_handled; /* All IDs that are either already done, or still in ids_todo stack. */
  BLI_LINKSTACK_DECLARE(ids_todo, ID *);
} LibraryForeachIDData;

bool BKE_lib_query_foreachid_process(LibraryForeachIDData *data, ID **id_pp, int cb_flag)
{
  if (!(data->status & IDWALK_STOP)) {
    const int flag = data->flag;
    ID *old_id = *id_pp;
    const int callback_return = data->callback(&(struct LibraryIDLinkCallbackData){
        .user_data = data->user_data,
        .bmain = data->bmain,
        .id_owner = data->owner_id,
        .id_self = data->self_id,
        .id_pointer = id_pp,
        .cb_flag = ((cb_flag | data->cb_flag) & ~data->cb_flag_clear)});
    if (flag & IDWALK_READONLY) {
      BLI_assert(*(id_pp) == old_id);
    }
    if (old_id && (flag & IDWALK_RECURSE)) {
      if (BLI_gset_add((data)->ids_handled, old_id)) {
        if (!(callback_return & IDWALK_RET_STOP_RECURSION)) {
          BLI_LINKSTACK_PUSH(data->ids_todo, old_id);
        }
      }
    }
    if (callback_return & IDWALK_RET_STOP_ITER) {
      data->status |= IDWALK_STOP;
      return false;
    }
    return true;
  }

  return false;
}

int BKE_lib_query_foreachid_process_flags_get(LibraryForeachIDData *data)
{
  return data->flag;
}

int BKE_lib_query_foreachid_process_callback_flag_override(LibraryForeachIDData *data,
                                                           const int cb_flag,
                                                           const bool do_replace)
{
  const int cb_flag_backup = data->cb_flag;
  if (do_replace) {
    data->cb_flag = cb_flag;
  }
  else {
    data->cb_flag |= cb_flag;
  }
  return cb_flag_backup;
}

static void library_foreach_ID_link(Main *bmain,
                                    ID *id_owner,
                                    ID *id,
                                    LibraryIDLinkCallback callback,
                                    void *user_data,
                                    int flag,
                                    LibraryForeachIDData *inherit_data);

void BKE_lib_query_idpropertiesForeachIDLink_callback(IDProperty *id_prop, void *user_data)
{
  BLI_assert(id_prop->type == IDP_ID);

  LibraryForeachIDData *data = (LibraryForeachIDData *)user_data;
  BKE_LIB_FOREACHID_PROCESS_ID(data, id_prop->data.pointer, IDWALK_CB_USER);
}

bool BKE_library_foreach_ID_embedded(LibraryForeachIDData *data, ID **id_pp)
{
  /* Needed e.g. for callbacks handling relationships. This call shall be absolutely read-only. */
  ID *id = *id_pp;
  const int flag = data->flag;

  if (!BKE_lib_query_foreachid_process(data, id_pp, IDWALK_CB_EMBEDDED)) {
    return false;
  }
  BLI_assert(id == *id_pp);

  if (id == NULL) {
    return true;
  }

  if (flag & IDWALK_IGNORE_EMBEDDED_ID) {
    /* Do Nothing. */
  }
  else if (flag & IDWALK_RECURSE) {
    /* Defer handling into main loop, recursively calling BKE_library_foreach_ID_link in
     * IDWALK_RECURSE case is troublesome, see T49553. */
    /* XXX note that this breaks the 'owner id' thing now, we likely want to handle that
     * differently at some point, but for now it should not be a problem in practice. */
    if (BLI_gset_add(data->ids_handled, id)) {
      BLI_LINKSTACK_PUSH(data->ids_todo, id);
    }
  }
  else {
    library_foreach_ID_link(
        data->bmain, data->owner_id, id, data->callback, data->user_data, data->flag, data);
  }

  return true;
}

static void library_foreach_ID_link(Main *bmain,
                                    ID *id_owner,
                                    ID *id,
                                    LibraryIDLinkCallback callback,
                                    void *user_data,
                                    int flag,
                                    LibraryForeachIDData *inherit_data)
{
  LibraryForeachIDData data = {.bmain = bmain};

  BLI_assert(inherit_data == NULL || data.bmain == inherit_data->bmain);

  if (flag & IDWALK_RECURSE) {
    /* For now, recursion implies read-only, and no internal pointers. */
    flag |= IDWALK_READONLY;
    flag &= ~IDWALK_DO_INTERNAL_RUNTIME_POINTERS;

    data.ids_handled = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
    BLI_LINKSTACK_INIT(data.ids_todo);

    BLI_gset_add(data.ids_handled, id);
  }
  else {
    data.ids_handled = NULL;
  }
  data.flag = flag;
  data.status = 0;
  data.callback = callback;
  data.user_data = user_data;

#define CALLBACK_INVOKE_ID(check_id, cb_flag) \
  BKE_LIB_FOREACHID_PROCESS_ID(&data, check_id, cb_flag)

#define CALLBACK_INVOKE(check_id_super, cb_flag) \
  BKE_LIB_FOREACHID_PROCESS(&data, check_id_super, cb_flag)

  for (; id != NULL; id = (flag & IDWALK_RECURSE) ? BLI_LINKSTACK_POP(data.ids_todo) : NULL) {
    data.self_id = id;
    /* Note that we may call this functions sometime directly on an embedded ID, without any
     * knowledge of the owner ID then.
     * While not great, and that should be probably sanitized at some point, we cal live with it
     * for now. */
    data.owner_id = ((id->flag & LIB_EMBEDDED_DATA) != 0 && id_owner != NULL) ? id_owner :
                                                                                data.self_id;

    /* inherit_data is non-NULL when this function is called for some sub-data ID
     * (like root node-tree of a material).
     * In that case, we do not want to generate those 'generic flags' from our current sub-data ID
     * (the node tree), but re-use those generated for the 'owner' ID (the material). */
    if (inherit_data == NULL) {
      data.cb_flag = ID_IS_LINKED(id) ? IDWALK_CB_INDIRECT_USAGE : 0;
      /* When an ID is not in Main database, it should never refcount IDs it is using.
       * Exceptions: NodeTrees (yeah!) directly used by Materials. */
      data.cb_flag_clear = (id->tag & LIB_TAG_NO_MAIN) ? IDWALK_CB_USER | IDWALK_CB_USER_ONE : 0;
    }
    else {
      data.cb_flag = inherit_data->cb_flag;
      data.cb_flag_clear = inherit_data->cb_flag_clear;
    }

    if (bmain != NULL && bmain->relations != NULL && (flag & IDWALK_READONLY) &&
        (flag & IDWALK_DO_INTERNAL_RUNTIME_POINTERS) == 0 &&
        (((bmain->relations->flag & MAINIDRELATIONS_INCLUDE_UI) == 0) ==
         ((data.flag & IDWALK_INCLUDE_UI) == 0))) {
      /* Note that this is minor optimization, even in worst cases (like id being an object with
       * lots of drivers and constraints and modifiers, or material etc. with huge node tree),
       * but we might as well use it (Main->relations is always assumed valid,
       * it's responsibility of code creating it to free it,
       * especially if/when it starts modifying Main database). */
      MainIDRelationsEntry *entry = BLI_ghash_lookup(bmain->relations->relations_from_pointers,
                                                     id);
      for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != NULL;
           to_id_entry = to_id_entry->next) {
        BKE_lib_query_foreachid_process(
            &data, to_id_entry->id_pointer.to, to_id_entry->usage_flag);
      }
      continue;
    }

    /* Note: ID.lib pointer is purposefully fully ignored here...
     * We may want to add it at some point? */

    if (flag & IDWALK_DO_INTERNAL_RUNTIME_POINTERS) {
      CALLBACK_INVOKE_ID(id->newid, IDWALK_CB_INTERNAL);
      CALLBACK_INVOKE_ID(id->orig_id, IDWALK_CB_INTERNAL);
    }

    if (id->override_library != NULL) {
      CALLBACK_INVOKE_ID(id->override_library->reference,
                         IDWALK_CB_USER | IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE);
      CALLBACK_INVOKE_ID(id->override_library->storage,
                         IDWALK_CB_USER | IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE);
    }

    IDP_foreach_property(id->properties,
                         IDP_TYPE_FILTER_ID,
                         BKE_lib_query_idpropertiesForeachIDLink_callback,
                         &data);

    AnimData *adt = BKE_animdata_from_id(id);
    if (adt) {
      BKE_animdata_foreach_id(adt, &data);
    }

    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
    if (id_type->foreach_id != NULL) {
      id_type->foreach_id(id, &data);

      if (data.status & IDWALK_STOP) {
        break;
      }
    }
  }

  if (data.ids_handled) {
    BLI_gset_free(data.ids_handled, NULL);
    BLI_LINKSTACK_FREE(data.ids_todo);
  }

#undef CALLBACK_INVOKE_ID
#undef CALLBACK_INVOKE
}

/**
 * Loop over all of the ID's this data-block links to.
 */
void BKE_library_foreach_ID_link(
    Main *bmain, ID *id, LibraryIDLinkCallback callback, void *user_data, int flag)
{
  library_foreach_ID_link(bmain, NULL, id, callback, user_data, flag, NULL);
}

/**
 * re-usable function, use when replacing ID's
 */
void BKE_library_update_ID_link_user(ID *id_dst, ID *id_src, const int cb_flag)
{
  if (cb_flag & IDWALK_CB_USER) {
    id_us_min(id_src);
    id_us_plus(id_dst);
  }
  else if (cb_flag & IDWALK_CB_USER_ONE) {
    id_us_ensure_real(id_dst);
  }
}

/**
 * Say whether given \a id_owner may use (in any way) a data-block of \a id_type_used.
 *
 * This is a 'simplified' abstract version of #BKE_library_foreach_ID_link() above,
 * quite useful to reduce useless iterations in some cases.
 */
bool BKE_library_id_can_use_idtype(ID *id_owner, const short id_type_used)
{
  /* any type of ID can be used in custom props. */
  if (id_owner->properties) {
    return true;
  }

  const short id_type_owner = GS(id_owner->name);

  /* IDProps of armature bones and nodes, and bNode->id can use virtually any type of ID. */
  if (ELEM(id_type_owner, ID_NT, ID_AR)) {
    return true;
  }

  if (ntreeFromID(id_owner)) {
    return true;
  }

  if (BKE_animdata_from_id(id_owner)) {
    /* AnimationData can use virtually any kind of data-blocks, through drivers especially. */
    return true;
  }

  switch ((ID_Type)id_type_owner) {
    case ID_LI:
      return ELEM(id_type_used, ID_LI);
    case ID_SCE:
      return (ELEM(id_type_used,
                   ID_OB,
                   ID_WO,
                   ID_SCE,
                   ID_MC,
                   ID_MA,
                   ID_GR,
                   ID_TXT,
                   ID_LS,
                   ID_MSK,
                   ID_SO,
                   ID_GD,
                   ID_BR,
                   ID_PAL,
                   ID_IM,
                   ID_NT));
    case ID_OB:
      /* Could be more specific, but simpler to just always say 'yes' here. */
      return true;
    case ID_ME:
      return ELEM(id_type_used, ID_ME, ID_KE, ID_MA, ID_IM);
    case ID_CU:
      return ELEM(id_type_used, ID_OB, ID_KE, ID_MA, ID_VF);
    case ID_MB:
      return ELEM(id_type_used, ID_MA);
    case ID_MA:
      return (ELEM(id_type_used, ID_TE, ID_GR));
    case ID_TE:
      return (ELEM(id_type_used, ID_IM, ID_OB));
    case ID_LT:
      return ELEM(id_type_used, ID_KE);
    case ID_LA:
      return (ELEM(id_type_used, ID_TE));
    case ID_CA:
      return ELEM(id_type_used, ID_OB);
    case ID_KE:
      /* Warning! key->from, could be more types in future? */
      return ELEM(id_type_used, ID_ME, ID_CU, ID_LT);
    case ID_SCR:
      return ELEM(id_type_used, ID_SCE);
    case ID_WO:
      return (ELEM(id_type_used, ID_TE));
    case ID_SPK:
      return ELEM(id_type_used, ID_SO);
    case ID_GR:
      return ELEM(id_type_used, ID_OB, ID_GR);
    case ID_NT:
      /* Could be more specific, but node.id has no type restriction... */
      return true;
    case ID_BR:
      return ELEM(id_type_used, ID_BR, ID_IM, ID_PC, ID_TE, ID_MA);
    case ID_PA:
      return ELEM(id_type_used, ID_OB, ID_GR, ID_TE);
    case ID_MC:
      return ELEM(id_type_used, ID_GD, ID_IM);
    case ID_MSK:
      /* WARNING! mask->parent.id, not typed. */
      return ELEM(id_type_used, ID_MC);
    case ID_LS:
      return (ELEM(id_type_used, ID_TE, ID_OB));
    case ID_LP:
      return ELEM(id_type_used, ID_IM);
    case ID_GD:
      return ELEM(id_type_used, ID_MA);
    case ID_WS:
      return ELEM(id_type_used, ID_SCR, ID_SCE);
    case ID_HA:
      return ELEM(id_type_used, ID_MA);
    case ID_PT:
      return ELEM(id_type_used, ID_MA);
    case ID_VO:
      return ELEM(id_type_used, ID_MA);
    case ID_SIM:
      return ELEM(id_type_used, ID_OB, ID_IM);
    case ID_WM:
      return ELEM(id_type_used, ID_SCE, ID_WS);
    case ID_IM:
    case ID_VF:
    case ID_TXT:
    case ID_SO:
    case ID_AR:
    case ID_AC:
    case ID_PAL:
    case ID_PC:
    case ID_CF:
      /* Those types never use/reference other IDs... */
      return false;
    case ID_IP:
      /* Deprecated... */
      return false;
  }
  return false;
}

/* ***** ID users iterator. ***** */
typedef struct IDUsersIter {
  ID *id;

  ListBase *lb_array[INDEX_ID_MAX];
  int lb_idx;

  ID *curr_id;
  int count_direct, count_indirect; /* Set by callback. */
} IDUsersIter;

static int foreach_libblock_id_users_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;
  IDUsersIter *iter = cb_data->user_data;

  if (*id_p) {
    /* 'Loopback' ID pointers (the ugly 'from' ones, Object->proxy_from and Key->from).
     * Those are not actually ID usage, we can ignore them here.
     */
    if (cb_flag & IDWALK_CB_LOOPBACK) {
      return IDWALK_RET_NOP;
    }

    if (*id_p == iter->id) {
#if 0
      printf(
          "%s uses %s (refcounted: %d, userone: %d, used_one: %d, used_one_active: %d, "
          "indirect_usage: %d)\n",
          iter->curr_id->name,
          iter->id->name,
          (cb_flag & IDWALK_USER) ? 1 : 0,
          (cb_flag & IDWALK_USER_ONE) ? 1 : 0,
          (iter->id->tag & LIB_TAG_EXTRAUSER) ? 1 : 0,
          (iter->id->tag & LIB_TAG_EXTRAUSER_SET) ? 1 : 0,
          (cb_flag & IDWALK_INDIRECT_USAGE) ? 1 : 0);
#endif
      if (cb_flag & IDWALK_CB_INDIRECT_USAGE) {
        iter->count_indirect++;
      }
      else {
        iter->count_direct++;
      }
    }
  }

  return IDWALK_RET_NOP;
}

/**
 * Return the number of times given \a id_user uses/references \a id_used.
 *
 * \note This only checks for pointer references of an ID, shallow usages
 * (like e.g. by RNA paths, as done for FCurves) are not detected at all.
 *
 * \param id_user: the ID which is supposed to use (reference) \a id_used.
 * \param id_used: the ID which is supposed to be used (referenced) by \a id_user.
 * \return the number of direct usages/references of \a id_used by \a id_user.
 */
int BKE_library_ID_use_ID(ID *id_user, ID *id_used)
{
  IDUsersIter iter;

  /* We do not care about iter.lb_array/lb_idx here... */
  iter.id = id_used;
  iter.curr_id = id_user;
  iter.count_direct = iter.count_indirect = 0;

  BKE_library_foreach_ID_link(
      NULL, iter.curr_id, foreach_libblock_id_users_callback, (void *)&iter, IDWALK_READONLY);

  return iter.count_direct + iter.count_indirect;
}

static bool library_ID_is_used(Main *bmain, void *idv, const bool check_linked)
{
  IDUsersIter iter;
  ListBase *lb_array[INDEX_ID_MAX];
  ID *id = idv;
  int i = set_listbasepointers(bmain, lb_array);
  bool is_defined = false;

  iter.id = id;
  iter.count_direct = iter.count_indirect = 0;
  while (i-- && !is_defined) {
    ID *id_curr = lb_array[i]->first;

    if (!id_curr || !BKE_library_id_can_use_idtype(id_curr, GS(id->name))) {
      continue;
    }

    for (; id_curr && !is_defined; id_curr = id_curr->next) {
      if (id_curr == id) {
        /* We are not interested in self-usages (mostly from drivers or bone constraints...). */
        continue;
      }
      iter.curr_id = id_curr;
      BKE_library_foreach_ID_link(
          bmain, id_curr, foreach_libblock_id_users_callback, &iter, IDWALK_READONLY);

      is_defined = ((check_linked ? iter.count_indirect : iter.count_direct) != 0);
    }
  }

  return is_defined;
}

/**
 * Check whether given ID is used locally (i.e. by another non-linked ID).
 */
bool BKE_library_ID_is_locally_used(Main *bmain, void *idv)
{
  return library_ID_is_used(bmain, idv, false);
}

/**
 * Check whether given ID is used indirectly (i.e. by another linked ID).
 */
bool BKE_library_ID_is_indirectly_used(Main *bmain, void *idv)
{
  return library_ID_is_used(bmain, idv, true);
}

/**
 * Combine #BKE_library_ID_is_locally_used() and #BKE_library_ID_is_indirectly_used()
 * in a single call.
 */
void BKE_library_ID_test_usages(Main *bmain, void *idv, bool *is_used_local, bool *is_used_linked)
{
  IDUsersIter iter;
  ListBase *lb_array[INDEX_ID_MAX];
  ID *id = idv;
  int i = set_listbasepointers(bmain, lb_array);
  bool is_defined = false;

  iter.id = id;
  iter.count_direct = iter.count_indirect = 0;
  while (i-- && !is_defined) {
    ID *id_curr = lb_array[i]->first;

    if (!id_curr || !BKE_library_id_can_use_idtype(id_curr, GS(id->name))) {
      continue;
    }

    for (; id_curr && !is_defined; id_curr = id_curr->next) {
      if (id_curr == id) {
        /* We are not interested in self-usages (mostly from drivers or bone constraints...). */
        continue;
      }
      iter.curr_id = id_curr;
      BKE_library_foreach_ID_link(
          bmain, id_curr, foreach_libblock_id_users_callback, &iter, IDWALK_READONLY);

      is_defined = (iter.count_direct != 0 && iter.count_indirect != 0);
    }
  }

  *is_used_local = (iter.count_direct != 0);
  *is_used_linked = (iter.count_indirect != 0);
}

/* ***** IDs usages.checking/tagging. ***** */
static void lib_query_unused_ids_tag_recurse(Main *bmain,
                                             const int tag,
                                             const bool do_local_ids,
                                             const bool do_linked_ids,
                                             ID *id,
                                             int *r_num_tagged)
{
  /* We should never deal with embedded, not-in-main IDs here. */
  BLI_assert((id->flag & LIB_EMBEDDED_DATA) == 0);

  if ((!do_linked_ids && ID_IS_LINKED(id)) || (!do_local_ids && !ID_IS_LINKED(id))) {
    return;
  }

  MainIDRelationsEntry *id_relations = BLI_ghash_lookup(bmain->relations->relations_from_pointers,
                                                        id);
  if ((id_relations->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) != 0) {
    return;
  }
  id_relations->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  if ((id->tag & tag) != 0) {
    return;
  }

  if ((id->flag & LIB_FAKEUSER) != 0) {
    /* This ID is forcefully kept around, and therefore never unused, no need to check it further.
     */
    return;
  }

  if (ELEM(GS(id->name), ID_WM, ID_WS, ID_SCE, ID_SCR, ID_LI)) {
    /* Some 'root' ID types are never unused (even though they may not have actual users), unless
     * their actual user-count is set to 0. */
    return;
  }

  /* An ID user is 'valid' (i.e. may affect the 'used'/'not used' status of the ID it uses) if it
   * does not match `ignored_usages`, and does match `required_usages`. */
  const int ignored_usages = (IDWALK_CB_LOOPBACK | IDWALK_CB_EMBEDDED);
  const int required_usages = (IDWALK_CB_USER | IDWALK_CB_USER_ONE);

  /* This ID may be tagged as unused if none of its users are 'valid', as defined above.
   *
   * First recursively check all its valid users, if all of them can be tagged as
   * unused, then we can tag this ID as such too. */
  bool has_valid_from_users = false;
  for (MainIDRelationsEntryItem *id_from_item = id_relations->from_ids; id_from_item != NULL;
       id_from_item = id_from_item->next) {
    if ((id_from_item->usage_flag & ignored_usages) != 0 ||
        (id_from_item->usage_flag & required_usages) == 0) {
      continue;
    }

    ID *id_from = id_from_item->id_pointer.from;
    if ((id_from->flag & LIB_EMBEDDED_DATA) != 0) {
      /* Directly 'by-pass' to actual real ID owner. */
      const IDTypeInfo *type_info_from = BKE_idtype_get_info_from_id(id_from);
      BLI_assert(type_info_from->owner_get != NULL);
      id_from = type_info_from->owner_get(bmain, id_from);
    }

    lib_query_unused_ids_tag_recurse(
        bmain, tag, do_local_ids, do_linked_ids, id_from, r_num_tagged);
    if ((id_from->tag & tag) == 0) {
      has_valid_from_users = true;
      break;
    }
  }
  if (!has_valid_from_users) {
    /* This ID has no 'valid' users, tag it as unused. */
    id->tag |= tag;
    if (r_num_tagged != NULL) {
      r_num_tagged[INDEX_ID_NULL]++;
      r_num_tagged[BKE_idtype_idcode_to_index(GS(id->name))]++;
    }
  }
}

/**
 * Tag all unused IDs (a.k.a 'orphaned').
 *
 * By default only tag IDs with `0` user count.
 * If `do_tag_recursive` is set, it will check dependencies to detect all IDs that are not actually
 * used in current file, including 'archipelagos` (i.e. set of IDs referencing each other in
 * loops, but without any 'external' valid usages.
 *
 * Valid usages here are defined as ref-counting usages, which are not towards embedded or
 * loop-back data.
 *
 * \param r_num_tagged If non-NULL, must be a zero-initialized array of #INDEX_ID_MAX integers.
 *                     Number of tagged-as-unused IDs is then set for each type, and as total in
 *                     #INDEX_ID_NULL item.
 */
void BKE_lib_query_unused_ids_tag(Main *bmain,
                                  const int tag,
                                  const bool do_local_ids,
                                  const bool do_linked_ids,
                                  const bool do_tag_recursive,
                                  int *r_num_tagged)
{
  /* First loop, to only check for immediately unused IDs (those with 0 user count).
   * NOTE: It also takes care of clearing given tag for used IDs. */
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if ((!do_linked_ids && ID_IS_LINKED(id)) || (!do_local_ids && !ID_IS_LINKED(id))) {
      id->tag &= ~tag;
    }
    else if (id->us == 0) {
      id->tag |= tag;
      if (r_num_tagged != NULL) {
        r_num_tagged[INDEX_ID_NULL]++;
        r_num_tagged[BKE_idtype_idcode_to_index(GS(id->name))]++;
      }
    }
    else {
      id->tag &= ~tag;
    }
  }
  FOREACH_MAIN_ID_END;

  if (!do_tag_recursive) {
    return;
  }

  BKE_main_relations_create(bmain, 0);
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    lib_query_unused_ids_tag_recurse(bmain, tag, do_local_ids, do_linked_ids, id, r_num_tagged);
  }
  FOREACH_MAIN_ID_END;
  BKE_main_relations_free(bmain);
}

static int foreach_libblock_used_linked_data_tag_clear_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID *self_id = cb_data->id_self;
  ID **id_p = cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;
  bool *is_changed = cb_data->user_data;

  if (*id_p) {
    /* The infamous 'from' pointers (Key.from, Object.proxy_from, ...).
     * those are not actually ID usage, so we ignore them here. */
    if (cb_flag & IDWALK_CB_LOOPBACK) {
      return IDWALK_RET_NOP;
    }

    /* If checked id is used by an assumed used ID,
     * then it is also used and not part of any linked archipelago. */
    if (!(self_id->tag & LIB_TAG_DOIT) && ((*id_p)->tag & LIB_TAG_DOIT)) {
      (*id_p)->tag &= ~LIB_TAG_DOIT;
      *is_changed = true;
    }
  }

  return IDWALK_RET_NOP;
}

/**
 * Detect orphaned linked data blocks (i.e. linked data not used (directly or indirectly)
 * in any way by any local data), including complex cases like 'linked archipelagoes', i.e.
 * linked data-blocks that use each other in loops,
 * which prevents their deletion by 'basic' usage checks.
 *
 * \param do_init_tag: if \a true, all linked data are checked, if \a false,
 * only linked data-blocks already tagged with #LIB_TAG_DOIT are checked.
 */
void BKE_library_unused_linked_data_set_tag(Main *bmain, const bool do_init_tag)
{
  ID *id;

  if (do_init_tag) {
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      if (id->lib && (id->tag & LIB_TAG_INDIRECT) != 0) {
        id->tag |= LIB_TAG_DOIT;
      }
      else {
        id->tag &= ~LIB_TAG_DOIT;
      }
    }
    FOREACH_MAIN_ID_END;
  }

  for (bool do_loop = true; do_loop;) {
    do_loop = false;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      /* We only want to check that ID if it is currently known as used... */
      if ((id->tag & LIB_TAG_DOIT) == 0) {
        BKE_library_foreach_ID_link(
            bmain, id, foreach_libblock_used_linked_data_tag_clear_cb, &do_loop, IDWALK_READONLY);
      }
    }
    FOREACH_MAIN_ID_END;
  }
}

/**
 * Untag linked data blocks used by other untagged linked data-blocks.
 * Used to detect data-blocks that we can forcefully make local
 * (instead of copying them to later get rid of original):
 * All data-blocks we want to make local are tagged by caller,
 * after this function has ran caller knows data-blocks still tagged can directly be made local,
 * since they are only used by other data-blocks that will also be made fully local.
 */
void BKE_library_indirectly_used_data_tag_clear(Main *bmain)
{
  ListBase *lb_array[INDEX_ID_MAX];

  bool do_loop = true;
  while (do_loop) {
    int i = set_listbasepointers(bmain, lb_array);
    do_loop = false;

    while (i--) {
      LISTBASE_FOREACH (ID *, id, lb_array[i]) {
        if (id->lib == NULL || id->tag & LIB_TAG_DOIT) {
          /* Local or non-indirectly-used ID (so far), no need to check it further. */
          continue;
        }
        BKE_library_foreach_ID_link(
            bmain, id, foreach_libblock_used_linked_data_tag_clear_cb, &do_loop, IDWALK_READONLY);
      }
    }
  }
}
