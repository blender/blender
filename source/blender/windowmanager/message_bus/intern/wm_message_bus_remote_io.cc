/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "WM_types.hh"
#include "message_bus/intern/wm_message_bus_intern.hh"

namespace blender {

/* -------------------------------------------------------------------------- */

static uint wm_msg_remote_io_gset_hash(const void *key_p)
{
  const wmMsgSubscribeKey_RemoteIO *key = static_cast<const wmMsgSubscribeKey_RemoteIO *>(key_p);
  const wmMsgParams_RemoteIO *params = &key->msg.params;
  uint k = BLI_hash_string(params->remote_url);
  return k;
}
static bool wm_msg_remote_io_gset_cmp(const void *key_a_p, const void *key_b_p)
{
  const wmMsgParams_RemoteIO *params_a =
      &(static_cast<const wmMsgSubscribeKey_RemoteIO *>(key_a_p))->msg.params;
  const wmMsgParams_RemoteIO *params_b =
      &(static_cast<const wmMsgSubscribeKey_RemoteIO *>(key_b_p))->msg.params;
  return !STREQ(params_a->remote_url, params_b->remote_url);
}

static void *wm_msg_remote_io_gset_key_duplicate(const void *key_p)
{
  const wmMsgSubscribeKey_RemoteIO *key_src = static_cast<const wmMsgSubscribeKey_RemoteIO *>(
      key_p);
  wmMsgSubscribeKey_RemoteIO *key = MEM_new<wmMsgSubscribeKey_RemoteIO>(__func__, *key_src);
  key->msg.params.remote_url = BLI_strdup(key_src->msg.params.remote_url);
  return key;
}
static void wm_msg_remote_io_gset_key_free(void *key_p)
{
  wmMsgSubscribeKey_RemoteIO *key = static_cast<wmMsgSubscribeKey_RemoteIO *>(key_p);
  MEM_delete(key->msg.params.remote_url);
  wmMsgSubscribeValueLink *msg_lnk_next;
  for (wmMsgSubscribeValueLink *msg_lnk =
           static_cast<wmMsgSubscribeValueLink *>(key->head.values.first);
       msg_lnk;
       msg_lnk = msg_lnk_next)
  {
    msg_lnk_next = msg_lnk->next;
    BLI_remlink(&key->head.values, msg_lnk);
    MEM_delete(msg_lnk);
  }
  MEM_delete(key);
}

static void wm_msg_remote_io_repr(FILE *stream, const wmMsgSubscribeKey *msg_key)
{
  const wmMsgSubscribeKey_RemoteIO *m = reinterpret_cast<wmMsgSubscribeKey_RemoteIO *>(
      const_cast<wmMsgSubscribeKey *>(msg_key));
  fprintf(stream,
          "<wmMsg_RemoteIO %p, "
          "id='%s', "
          "values_len=%d\n",
          m,
          m->msg.head.id,
          BLI_listbase_count(&m->head.values));
}

void WM_msgtypeinfo_init_remote_io(wmMsgTypeInfo *msgtype_info)
{
  msgtype_info->gset.hash_fn = wm_msg_remote_io_gset_hash;
  msgtype_info->gset.cmp_fn = wm_msg_remote_io_gset_cmp;
  msgtype_info->gset.key_duplicate_fn = wm_msg_remote_io_gset_key_duplicate;
  msgtype_info->gset.key_free_fn = wm_msg_remote_io_gset_key_free;
  msgtype_info->repr = wm_msg_remote_io_repr;
}

/* -------------------------------------------------------------------------- */

wmMsgSubscribeKey_RemoteIO *WM_msg_lookup_remote_io(wmMsgBus *mbus,
                                                    const wmMsgParams_RemoteIO *msg_key_params)
{
  wmMsgSubscribeKey_RemoteIO key_test;
  key_test.msg.params = *msg_key_params;
  return static_cast<wmMsgSubscribeKey_RemoteIO *>(
      BLI_gset_lookup(mbus->messages_gset[WM_MSG_TYPE_REMOTE_IO], &key_test));
}

void WM_msg_publish_remote_io_params(wmMsgBus *mbus, const wmMsgParams_RemoteIO *msg_key_params)
{
  CLOG_DEBUG(WM_LOG_MSGBUS_PUB, "remote_io(remote_url=%s)", msg_key_params->remote_url);

  wmMsgSubscribeKey_RemoteIO *key = WM_msg_lookup_remote_io(mbus, msg_key_params);
  if (key) {
    WM_msg_publish_with_key(mbus, &key->head);
  }
}

void WM_msg_publish_remote_io(wmMsgBus *mbus, const StringRef remote_url)
{
  wmMsgParams_RemoteIO params{};
  params.remote_url = BLI_strdupn(remote_url.data(), remote_url.size());
  WM_msg_publish_remote_io_params(mbus, &params);

  /* Value was copied into the publish key. */
  MEM_delete(params.remote_url);
}

void WM_msg_subscribe_remote_io_params(wmMsgBus *mbus,
                                       const wmMsgParams_RemoteIO *msg_key_params,
                                       const wmMsgSubscribeValue *msg_val_params,
                                       const char *id_repr)
{
  wmMsgSubscribeKey_RemoteIO msg_key_test{};

  /* Use when added. */
  msg_key_test.msg.head.id = id_repr;
  msg_key_test.msg.head.type = WM_MSG_TYPE_REMOTE_IO;
  /* For lookup. */
  msg_key_test.msg.params = *msg_key_params;

  WM_msg_subscribe_with_key(mbus, &msg_key_test.head, msg_val_params);
}

void WM_msg_subscribe_remote_io(wmMsgBus *mbus,
                                const StringRef remote_url,
                                const wmMsgSubscribeValue *msg_val_params,
                                const char *id_repr)
{
  wmMsgParams_RemoteIO params{};
  params.remote_url = BLI_strdupn(remote_url.data(), remote_url.size());
  WM_msg_subscribe_remote_io_params(mbus, &params, msg_val_params, id_repr);

  /* Value was copied into the subscribe key. */
  MEM_delete(params.remote_url);
}

}  // namespace blender
