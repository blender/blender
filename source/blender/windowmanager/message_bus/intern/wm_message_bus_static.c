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

/** \file blender/windowmanager/message_bus/intern/wm_message_bus_static.c
 *  \ingroup wm
 */

#include <stdio.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "WM_types.h"
#include "WM_message.h"
#include "message_bus/intern/wm_message_bus_intern.h"


/* -------------------------------------------------------------------------- */

static uint wm_msg_static_gset_hash(const void *key_p)
{
	const wmMsgSubscribeKey_Static *key = key_p;
	const wmMsgParams_Static *params = &key->msg.params;
	uint k = params->event;
	return k;
}
static bool wm_msg_static_gset_cmp(const void *key_a_p, const void *key_b_p)
{
	const wmMsgParams_Static *params_a = &((const wmMsgSubscribeKey_Static *)key_a_p)->msg.params;
	const wmMsgParams_Static *params_b = &((const wmMsgSubscribeKey_Static *)key_b_p)->msg.params;
	return !(
		(params_a->event ==
		 params_b->event)
	);
}
static void wm_msg_static_gset_key_free(void *key_p)
{
	wmMsgSubscribeKey *key = key_p;
	wmMsgSubscribeValueLink *msg_lnk_next;
	for (wmMsgSubscribeValueLink *msg_lnk = key->values.first; msg_lnk; msg_lnk = msg_lnk_next) {
		msg_lnk_next = msg_lnk->next;
		BLI_remlink(&key->values, msg_lnk);
		MEM_freeN(msg_lnk);
	}
	MEM_freeN(key);
}

static void wm_msg_static_repr(FILE *stream, const wmMsgSubscribeKey *msg_key)
{
	const wmMsgSubscribeKey_Static *m = (wmMsgSubscribeKey_Static *)msg_key;
	fprintf(stream,
	        "<wmMsg_Static %p, "
	        "id='%s', "
	        "values_len=%d\n",
	        m, m->msg.head.id,
	        BLI_listbase_count(&m->head.values));
}


void WM_msgtypeinfo_init_static(wmMsgTypeInfo *msgtype_info)
{
	msgtype_info->gset.hash_fn = wm_msg_static_gset_hash;
	msgtype_info->gset.cmp_fn = wm_msg_static_gset_cmp;
	msgtype_info->gset.key_free_fn = wm_msg_static_gset_key_free;
	msgtype_info->repr = wm_msg_static_repr;

	msgtype_info->msg_key_size = sizeof(wmMsgSubscribeKey_Static);
}

/* -------------------------------------------------------------------------- */


wmMsgSubscribeKey_Static *WM_msg_lookup_static(struct wmMsgBus *mbus, const wmMsgParams_Static *msg_key_params)
{
	wmMsgSubscribeKey_Static key_test;
	key_test.msg.params = *msg_key_params;
	return BLI_gset_lookup(mbus->messages_gset[WM_MSG_TYPE_STATIC], &key_test);
}

void WM_msg_publish_static_params(struct wmMsgBus *mbus, const wmMsgParams_Static *msg_key_params)
{
	CLOG_INFO(WM_LOG_MSGBUS_PUB, 2, "static(event=%d)", msg_key_params->event);

	wmMsgSubscribeKey_Static *key = WM_msg_lookup_static(mbus, msg_key_params);
	if (key) {
		WM_msg_publish_with_key(mbus, &key->head);
	}
}

void WM_msg_publish_static(struct wmMsgBus *mbus, int event)
{
	WM_msg_publish_static_params(mbus, &(wmMsgParams_Static){ .event = event, });
}

void WM_msg_subscribe_static_params(
        struct wmMsgBus *mbus,
        const wmMsgParams_Static *msg_key_params,
        const wmMsgSubscribeValue *msg_val_params,
        const char *id_repr)
{
	wmMsgSubscribeKey_Static msg_key_test = {{NULL}};

	/* use when added */
	msg_key_test.msg.head.id = id_repr;
	msg_key_test.msg.head.type = WM_MSG_TYPE_STATIC;
	/* for lookup */
	msg_key_test.msg.params = *msg_key_params;

	WM_msg_subscribe_with_key(mbus, &msg_key_test.head, msg_val_params);
}

void WM_msg_subscribe_static(
        struct wmMsgBus *mbus,
        int event,
        const wmMsgSubscribeValue *msg_val_params,
        const char *id_repr)
{
	WM_msg_subscribe_static_params(mbus, &(const wmMsgParams_Static){ .event = event, }, msg_val_params, id_repr);
}
