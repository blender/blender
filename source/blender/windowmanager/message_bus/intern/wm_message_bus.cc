/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include <cstring>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"

#include "WM_types.hh"

#include "message_bus/intern/wm_message_bus_intern.hh"
#include "message_bus/wm_message_bus.hh"

/* -------------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static wmMsgTypeInfo wm_msg_types[WM_MSG_TYPE_NUM] = {{{nullptr}}};

using wmMsgTypeInitFn = void (*)(wmMsgTypeInfo *);

static wmMsgTypeInitFn wm_msg_init_fn[WM_MSG_TYPE_NUM] = {
    WM_msgtypeinfo_init_rna,
    WM_msgtypeinfo_init_static,
};

void WM_msgbus_types_init()
{
  for (uint i = 0; i < WM_MSG_TYPE_NUM; i++) {
    wm_msg_init_fn[i](&wm_msg_types[i]);
  }
}

wmMsgBus *WM_msgbus_create()
{
  wmMsgBus *mbus = static_cast<wmMsgBus *>(MEM_callocN(sizeof(*mbus), __func__));
  const uint gset_reserve = 512;
  for (uint i = 0; i < WM_MSG_TYPE_NUM; i++) {
    wmMsgTypeInfo *info = &wm_msg_types[i];
    mbus->messages_gset[i] = BLI_gset_new_ex(
        info->gset.hash_fn, info->gset.cmp_fn, __func__, gset_reserve);
  }
  return mbus;
}

void WM_msgbus_destroy(wmMsgBus *mbus)
{
  for (uint i = 0; i < WM_MSG_TYPE_NUM; i++) {
    wmMsgTypeInfo *info = &wm_msg_types[i];
    BLI_gset_free(mbus->messages_gset[i], info->gset.key_free_fn);
  }
  MEM_freeN(mbus);
}

void WM_msgbus_clear_by_owner(wmMsgBus *mbus, void *owner)
{
  wmMsgSubscribeKey *msg_key, *msg_key_next;
  for (msg_key = static_cast<wmMsgSubscribeKey *>(mbus->messages.first); msg_key;
       msg_key = msg_key_next)
  {
    msg_key_next = msg_key->next;

    wmMsgSubscribeValueLink *msg_lnk_next;
    for (wmMsgSubscribeValueLink *msg_lnk =
             static_cast<wmMsgSubscribeValueLink *>(msg_key->values.first);
         msg_lnk;
         msg_lnk = msg_lnk_next)
    {
      msg_lnk_next = msg_lnk->next;
      if (msg_lnk->params.owner == owner) {
        if (msg_lnk->params.tag) {
          mbus->messages_tag_count -= 1;
        }
        if (msg_lnk->params.free_data) {
          msg_lnk->params.free_data(msg_key, &msg_lnk->params);
        }
        BLI_remlink(&msg_key->values, msg_lnk);
        MEM_freeN(msg_lnk);
      }
    }

    if (BLI_listbase_is_empty(&msg_key->values)) {
      const wmMsg *msg = wm_msg_subscribe_value_msg_cast(msg_key);
      wmMsgTypeInfo *info = &wm_msg_types[msg->type];
      BLI_remlink(&mbus->messages, msg_key);
      bool ok = BLI_gset_remove(mbus->messages_gset[msg->type], msg_key, info->gset.key_free_fn);
      BLI_assert(ok);
      UNUSED_VARS_NDEBUG(ok);
    }
  }
}

void WM_msg_dump(wmMsgBus *mbus, const char *info_str)
{
  printf(">>>> %s\n", info_str);
  LISTBASE_FOREACH (wmMsgSubscribeKey *, key, &mbus->messages) {
    const wmMsg *msg = wm_msg_subscribe_value_msg_cast(key);
    const wmMsgTypeInfo *info = &wm_msg_types[msg->type];
    info->repr(stdout, key);
  }
  printf("<<<< %s\n", info_str);
}

void WM_msgbus_handle(wmMsgBus *mbus, bContext *C)
{
  if (mbus->messages_tag_count == 0) {
    // printf("msgbus: skipping\n");
    return;
  }

  if (false) {
    WM_msg_dump(mbus, __func__);
  }

  // uint a = 0, b = 0;
  LISTBASE_FOREACH (wmMsgSubscribeKey *, key, &mbus->messages) {
    LISTBASE_FOREACH (wmMsgSubscribeValueLink *, msg_lnk, &key->values) {
      if (msg_lnk->params.tag) {
        msg_lnk->params.notify(C, key, &msg_lnk->params);
        msg_lnk->params.tag = false;
        mbus->messages_tag_count -= 1;
      }
      // b++;
    }
    // a++;
  }
  BLI_assert(mbus->messages_tag_count == 0);
  mbus->messages_tag_count = 0;
  // printf("msgbus: keys=%u values=%u\n", a, b);
}

wmMsgSubscribeKey *WM_msg_subscribe_with_key(wmMsgBus *mbus,
                                             const wmMsgSubscribeKey *msg_key_test,
                                             const wmMsgSubscribeValue *msg_val_params)
{
  const uint type = wm_msg_subscribe_value_msg_cast(msg_key_test)->type;
  const wmMsgTypeInfo *info = &wm_msg_types[type];
  wmMsgSubscribeKey *key;

  BLI_assert(wm_msg_subscribe_value_msg_cast(msg_key_test)->id != nullptr);

  void **r_key;
  if (!BLI_gset_ensure_p_ex(mbus->messages_gset[type], msg_key_test, &r_key)) {
    key = static_cast<wmMsgSubscribeKey *>(*r_key = MEM_mallocN(info->msg_key_size, __func__));
    memcpy(key, msg_key_test, info->msg_key_size);
    BLI_addtail(&mbus->messages, key);
  }
  else {
    key = static_cast<wmMsgSubscribeKey *>(*r_key);
    LISTBASE_FOREACH (wmMsgSubscribeValueLink *, msg_lnk, &key->values) {
      if ((msg_lnk->params.notify == msg_val_params->notify) &&
          (msg_lnk->params.owner == msg_val_params->owner) &&
          (msg_lnk->params.user_data == msg_val_params->user_data))
      {
        return key;
      }
    }
  }

  wmMsgSubscribeValueLink *msg_lnk = static_cast<wmMsgSubscribeValueLink *>(
      MEM_mallocN(sizeof(wmMsgSubscribeValueLink), __func__));
  msg_lnk->params = *msg_val_params;
  BLI_addtail(&key->values, msg_lnk);
  return key;
}

void WM_msg_publish_with_key(wmMsgBus *mbus, wmMsgSubscribeKey *msg_key)
{
  CLOG_INFO(WM_LOG_MSGBUS_SUB,
            2,
            "tagging subscribers: (ptr=%p, len=%d)",
            msg_key,
            BLI_listbase_count(&msg_key->values));

  LISTBASE_FOREACH (wmMsgSubscribeValueLink *, msg_lnk, &msg_key->values) {
    if (false) { /* Make an option? */
      msg_lnk->params.notify(nullptr, msg_key, &msg_lnk->params);
    }
    else {
      if (msg_lnk->params.tag == false) {
        msg_lnk->params.tag = true;
        mbus->messages_tag_count += 1;
      }
    }
  }
}

void WM_msg_id_update(wmMsgBus *mbus, ID *id_src, ID *id_dst)
{
  for (uint i = 0; i < WM_MSG_TYPE_NUM; i++) {
    wmMsgTypeInfo *info = &wm_msg_types[i];
    if (info->update_by_id != nullptr) {
      info->update_by_id(mbus, id_src, id_dst);
    }
  }
}

void WM_msg_id_remove(wmMsgBus *mbus, const ID *id)
{
  for (uint i = 0; i < WM_MSG_TYPE_NUM; i++) {
    wmMsgTypeInfo *info = &wm_msg_types[i];
    if (info->remove_by_id != nullptr) {
      info->remove_by_id(mbus, id);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Internal API
 *
 * \note While we could have a separate type for ID's, use RNA since there is enough overlap.
 * \{ */

void wm_msg_subscribe_value_free(wmMsgSubscribeKey *msg_key, wmMsgSubscribeValueLink *msg_lnk)
{
  if (msg_lnk->params.free_data) {
    msg_lnk->params.free_data(msg_key, &msg_lnk->params);
  }
  BLI_remlink(&msg_key->values, msg_lnk);
  MEM_freeN(msg_lnk);
}

/** \} */
