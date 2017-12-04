/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/message_bus/intern/wm_message_bus_intern.h
 *  \ingroup wm
 */

#ifndef __WM_MESSAGE_BUS_INTERN_H__
#define __WM_MESSAGE_BUS_INTERN_H__

/* wm_message_bus.h must be included first */

struct wmMsgBus {
	struct GSet *messages_gset[WM_MSG_TYPE_NUM];
	/** Messages in order of being added. */
	ListBase messages;
	/** Avoid checking messages when no tags exist. */
	uint     messages_tag_count;
};

void wm_msg_subscribe_value_free(
        struct wmMsgSubscribeKey *msg_key, struct wmMsgSubscribeValueLink *msg_lnk);

typedef struct wmMsgSubscribeKey_Generic {
	wmMsgSubscribeKey head;
	wmMsg msg;
} wmMsgSubscribeKey_Generic;

BLI_INLINE const wmMsg *wm_msg_subscribe_value_msg_cast(const wmMsgSubscribeKey *key)
{
	return &((wmMsgSubscribeKey_Generic *)key)->msg;
}
BLI_INLINE wmMsg *wm_msg_subscribe_value_msg_cast_mut(wmMsgSubscribeKey *key)
{
	return &((wmMsgSubscribeKey_Generic *)key)->msg;
}

#endif /* __WM_MESSAGE_BUS_INTERN_H__ */
