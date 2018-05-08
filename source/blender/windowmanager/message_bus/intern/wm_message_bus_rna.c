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

/** \file blender/windowmanager/message_bus/intern/wm_message_bus_rna.c
 *  \ingroup wm
 */


#include <stdio.h>

#include "CLG_log.h"
#include "MEM_guardedalloc.h"

#include "DNA_ID.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "WM_types.h"
#include "WM_message.h"
#include "message_bus/intern/wm_message_bus_intern.h"

#include "RNA_access.h"


/* -------------------------------------------------------------------------- */

BLI_INLINE uint void_hash_uint(const void *key)
{
	size_t y = (size_t)key >> (sizeof(void *));
	return (unsigned int)y;
}

static uint wm_msg_rna_gset_hash(const void *key_p)
{
	const wmMsgSubscribeKey_RNA *key = key_p;
	const wmMsgParams_RNA *params = &key->msg.params;
//	printf("%s\n", RNA_struct_identifier(params->ptr.type));
	uint k = void_hash_uint(params->ptr.type);
	k     ^= void_hash_uint(params->ptr.data);
	k     ^= void_hash_uint(params->ptr.id.data);
	k     ^= void_hash_uint(params->prop);
	return k;
}
static bool wm_msg_rna_gset_cmp(const void *key_a_p, const void *key_b_p)
{
	const wmMsgParams_RNA *params_a = &((const wmMsgSubscribeKey_RNA *)key_a_p)->msg.params;
	const wmMsgParams_RNA *params_b = &((const wmMsgSubscribeKey_RNA *)key_b_p)->msg.params;
	return !(
		(params_a->ptr.type ==
		 params_b->ptr.type) &&
		(params_a->ptr.id.data ==
		 params_b->ptr.id.data) &&
		(params_a->ptr.data ==
		 params_b->ptr.data) &&
		(params_a->prop ==
		 params_b->prop)
	);
}
static void wm_msg_rna_gset_key_free(void *key_p)
{
	wmMsgSubscribeKey_RNA *key = key_p;
	wmMsgSubscribeValueLink *msg_lnk_next;
	for (wmMsgSubscribeValueLink *msg_lnk = key->head.values.first; msg_lnk; msg_lnk = msg_lnk_next) {
		msg_lnk_next = msg_lnk->next;
		wm_msg_subscribe_value_free(&key->head, msg_lnk);
	}
	if (key->msg.params.data_path != NULL) {
		MEM_freeN(key->msg.params.data_path);
	}
	MEM_freeN(key);
}

static void wm_msg_rna_repr(FILE *stream, const wmMsgSubscribeKey *msg_key)
{
	const wmMsgSubscribeKey_RNA *m = (wmMsgSubscribeKey_RNA *)msg_key;
	const char *none = "<none>";
	fprintf(stream,
	        "<wmMsg_RNA %p, "
	        "id='%s', "
	        "%s.%s values_len=%d\n",
	        m, m->msg.head.id,
	        m->msg.params.ptr.type ? RNA_struct_identifier(m->msg.params.ptr.type) : none,
	        m->msg.params.prop ? RNA_property_identifier((PropertyRNA *)m->msg.params.prop) : none,
	        BLI_listbase_count(&m->head.values));
}

static void wm_msg_rna_update_by_id(
        struct wmMsgBus *mbus,
        ID *id_src, ID *id_dst)
{
	GSet *gs = mbus->messages_gset[WM_MSG_TYPE_RNA];
	GSetIterator gs_iter;
	BLI_gsetIterator_init(&gs_iter, gs);
	while (BLI_gsetIterator_done(&gs_iter) == false) {
		wmMsgSubscribeKey_RNA *key = BLI_gsetIterator_getKey(&gs_iter);
		BLI_gsetIterator_step(&gs_iter);
		if (key->msg.params.ptr.id.data == id_src) {

			/* GSet always needs updating since the key changes. */
			BLI_gset_remove(gs, key, NULL);

			/* Remove any non-persistent values, so a single persistent
			 * value doesn't modify behavior for the rest. */
			wmMsgSubscribeValueLink *msg_lnk_next;
			for (wmMsgSubscribeValueLink *msg_lnk = key->head.values.first; msg_lnk; msg_lnk = msg_lnk_next) {
				msg_lnk_next = msg_lnk->next;
				if (msg_lnk->params.is_persistent == false) {
					wm_msg_subscribe_value_free(&key->head, msg_lnk);
				}
			}

			bool remove = true;

			if (BLI_listbase_is_empty(&key->head.values)) {
				/* Remove, no reason to keep. */
			}
			else if (key->msg.params.ptr.data == key->msg.params.ptr.id.data) {
				/* Simple, just update the ID. */
				key->msg.params.ptr.data = id_dst;
				key->msg.params.ptr.id.data = id_dst;
				remove = false;
			}
			else {
				/* we need to resolve this from the  */
				PointerRNA idptr;
				RNA_id_pointer_create(id_dst, &idptr);
				PointerRNA ptr;
				PropertyRNA *prop;
				if (!RNA_path_resolve(&idptr, key->msg.params.data_path, &ptr, &prop)) {
					key->msg.params.ptr = ptr;
					key->msg.params.prop = prop;
					remove = false;
				}
			}

			if (remove) {
				/* Failed to persist, remove the key. */
				BLI_remlink(&mbus->messages, key);
				wm_msg_rna_gset_key_free(key);
			}
			else {
				/* note that it's not impossible this key exists, however it is very unlikely
				 * since a subscriber would need to register in the middle of an undo for eg. so assert for now. */
				BLI_assert(!BLI_gset_haskey(gs, key));
				BLI_gset_add(gs, key);
			}
		}
	}
}

static void wm_msg_rna_remove_by_id(struct wmMsgBus *mbus, const ID *id)
{
	GSet *gs = mbus->messages_gset[WM_MSG_TYPE_RNA];
	GSetIterator gs_iter;
	BLI_gsetIterator_init(&gs_iter, gs);
	while (BLI_gsetIterator_done(&gs_iter) == false) {
		wmMsgSubscribeKey_RNA *key = BLI_gsetIterator_getKey(&gs_iter);
		BLI_gsetIterator_step(&gs_iter);
		if (key->msg.params.ptr.id.data == id) {
			BLI_remlink(&mbus->messages, key);
			BLI_gset_remove(gs, key, NULL);
			wm_msg_rna_gset_key_free(key);
		}
	}
}

void WM_msgtypeinfo_init_rna(wmMsgTypeInfo *msgtype_info)
{
	msgtype_info->gset.hash_fn = wm_msg_rna_gset_hash;
	msgtype_info->gset.cmp_fn = wm_msg_rna_gset_cmp;
	msgtype_info->gset.key_free_fn = wm_msg_rna_gset_key_free;

	msgtype_info->repr = wm_msg_rna_repr;
	msgtype_info->update_by_id = wm_msg_rna_update_by_id;
	msgtype_info->remove_by_id = wm_msg_rna_remove_by_id;

	msgtype_info->msg_key_size = sizeof(wmMsgSubscribeKey_RNA);
}

/* -------------------------------------------------------------------------- */


wmMsgSubscribeKey_RNA *WM_msg_lookup_rna(struct wmMsgBus *mbus, const wmMsgParams_RNA *msg_key_params)
{
	wmMsgSubscribeKey_RNA key_test;
	key_test.msg.params = *msg_key_params;
	return BLI_gset_lookup(mbus->messages_gset[WM_MSG_TYPE_RNA], &key_test);
}

void WM_msg_publish_rna_params(struct wmMsgBus *mbus, const wmMsgParams_RNA *msg_key_params)
{
	wmMsgSubscribeKey_RNA *key;

	const char *none = "<none>";
	CLOG_INFO(
	        WM_LOG_MSGBUS_PUB, 2,
	        "rna(id='%s', %s.%s)",
	        msg_key_params->ptr.id.data ? ((ID *)msg_key_params->ptr.id.data)->name : none,
	        msg_key_params->ptr.type ? RNA_struct_identifier(msg_key_params->ptr.type) : none,
	        msg_key_params->prop ? RNA_property_identifier((PropertyRNA *)msg_key_params->prop) : none
	);

	if ((key = WM_msg_lookup_rna(mbus, msg_key_params))) {
		WM_msg_publish_with_key(mbus, &key->head);
	}

	/* Support anonymous subscribers, this may be some extra overhead
	 * but we want to be able to be more ambiguous. */
	if (msg_key_params->ptr.id.data || msg_key_params->ptr.data) {
		wmMsgParams_RNA msg_key_params_anon = *msg_key_params;

		/* We might want to enable this later? */
		if (msg_key_params_anon.prop != NULL) {
			/* All properties for this type. */
			msg_key_params_anon.prop = NULL;
			if ((key = WM_msg_lookup_rna(mbus, &msg_key_params_anon))) {
				WM_msg_publish_with_key(mbus, &key->head);
			}
			msg_key_params_anon.prop = msg_key_params->prop;
		}

		msg_key_params_anon.ptr.id.data = NULL;
		msg_key_params_anon.ptr.data = NULL;
		if ((key = WM_msg_lookup_rna(mbus, &msg_key_params_anon))) {
			WM_msg_publish_with_key(mbus, &key->head);
		}

		/* Support subscribers to a type. */
		if (msg_key_params->prop) {
			msg_key_params_anon.prop = NULL;
			if ((key = WM_msg_lookup_rna(mbus, &msg_key_params_anon))) {
				WM_msg_publish_with_key(mbus, &key->head);
			}
		}
	}
}

void WM_msg_publish_rna(struct wmMsgBus *mbus, PointerRNA *ptr, PropertyRNA *prop)
{
	WM_msg_publish_rna_params(mbus, &(wmMsgParams_RNA){ .ptr = *ptr, .prop = prop, });
}

void WM_msg_subscribe_rna_params(
        struct wmMsgBus *mbus,
        const wmMsgParams_RNA *msg_key_params,
        const wmMsgSubscribeValue *msg_val_params,
        const char *id_repr)
{
	wmMsgSubscribeKey_RNA msg_key_test = {{NULL}};

	/* use when added */
	msg_key_test.msg.head.id = id_repr;
	msg_key_test.msg.head.type = WM_MSG_TYPE_RNA;
	/* for lookup */
	msg_key_test.msg.params = *msg_key_params;

	const char *none = "<none>";
	CLOG_INFO(
	        WM_LOG_MSGBUS_SUB, 3,
	        "rna(id='%s', %s.%s, info='%s')",
	        msg_key_params->ptr.id.data ? ((ID *)msg_key_params->ptr.id.data)->name : none,
	        msg_key_params->ptr.type ? RNA_struct_identifier(msg_key_params->ptr.type) : none,
	        msg_key_params->prop ? RNA_property_identifier((PropertyRNA *)msg_key_params->prop) : none,
	        id_repr
	);

	wmMsgSubscribeKey_RNA *msg_key = (wmMsgSubscribeKey_RNA *)WM_msg_subscribe_with_key(
	        mbus, &msg_key_test.head, msg_val_params);

	if (msg_val_params->is_persistent) {
		if (msg_key->msg.params.data_path == NULL) {
			if (msg_key->msg.params.ptr.data != msg_key->msg.params.ptr.id.data) {
				/* We assume prop type can't change. */
				msg_key->msg.params.data_path = RNA_path_from_ID_to_struct(&msg_key->msg.params.ptr);
			}
		}
	}
}

void WM_msg_subscribe_rna(
        struct wmMsgBus *mbus,
        PointerRNA *ptr, const PropertyRNA *prop,
        const wmMsgSubscribeValue *msg_val_params,
        const char *id_repr)
{
	WM_msg_subscribe_rna_params(
	        mbus,
	        &(const wmMsgParams_RNA){
	            .ptr = *ptr,
	            .prop = prop,
	        },
	        msg_val_params, id_repr);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name ID variants of RNA API
 *
 * \note While we could have a separate type for ID's, use RNA since there is enough overlap.
 * \{ */

void WM_msg_subscribe_ID(
        struct wmMsgBus *mbus, ID *id, const wmMsgSubscribeValue *msg_val_params,
        const char *id_repr)
{
	wmMsgParams_RNA msg_key_params = {{{NULL}}};
	RNA_id_pointer_create(id, &msg_key_params.ptr);
	WM_msg_subscribe_rna_params(mbus, &msg_key_params, msg_val_params, id_repr);
}

void WM_msg_publish_ID(struct wmMsgBus *mbus, ID *id)
{
	wmMsgParams_RNA msg_key_params = {{{NULL}}};
	RNA_id_pointer_create(id, &msg_key_params.ptr);
	WM_msg_publish_rna_params(mbus, &msg_key_params);
}

/** \} */
