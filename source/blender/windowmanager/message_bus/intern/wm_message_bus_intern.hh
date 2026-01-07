/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

#include "../wm_message_bus.hh"

struct GSet;

namespace blender {

struct wmMsgBus {
  GSet *messages_gset[WM_MSG_TYPE_NUM];
  /** Messages in order of being added. */
  ListBaseT<wmMsgSubscribeKey> messages;
  /** Avoid checking messages when no tags exist. */
  uint messages_tag_count;
};

/**
 * \note #wmMsgBus.messages_tag_count isn't updated, caller must handle.
 */
void wm_msg_subscribe_value_free(wmMsgSubscribeKey *msg_key, wmMsgSubscribeValueLink *msg_lnk);

struct wmMsgSubscribeKey_Generic {
  wmMsgSubscribeKey head;
  wmMsg msg;
};

BLI_INLINE const wmMsg *wm_msg_subscribe_value_msg_cast(const wmMsgSubscribeKey *key)
{
  return &(reinterpret_cast<wmMsgSubscribeKey_Generic *>(const_cast<wmMsgSubscribeKey *>(key)))
              ->msg;
}
BLI_INLINE wmMsg *wm_msg_subscribe_value_msg_cast_mut(wmMsgSubscribeKey *key)
{
  return &(reinterpret_cast<wmMsgSubscribeKey_Generic *>(key))->msg;
}

}  // namespace blender
