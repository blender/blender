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
 * The Original Code is Copyright (C) 2014 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey SHarybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_LIBRARY_QUERY_H__
#define __BKE_LIBRARY_QUERY_H__

/** \file BKE_library_query.h
 *  \ingroup bke
 *  \since March 2014
 *  \author sergey
 */

struct ID;
struct Main;

/* Tips for the callback for cases it's gonna to modify the pointer. */
enum {
	IDWALK_CB_NOP = 0,
	IDWALK_CB_NEVER_NULL = (1 << 0),
	IDWALK_CB_NEVER_SELF = (1 << 1),

	/**
	 * Indicates whether this is direct (i.e. by local data) or indirect (i.e. by linked data) usage.
	 * \note Object proxies are half-local, half-linked...
	 */
	IDWALK_CB_INDIRECT_USAGE = (1 << 2),

	/** That ID is used as mere sub-data by its owner
	 * (only case currently: those f***ing nodetrees in materials etc.).
	 * This means callback shall not *do* anything, only use this as informative data if it needs it. */
	IDWALK_CB_PRIVATE = (1 << 3),

	/** That ID is not really used by its owner, it's just an internal hint/helper.
	 * This addresses Their Highest Ugliness the 'from' pointers: Object->from_proxy and Key->from.
	 * How to handle that kind of cases totally depends on what caller code is doing... */
	IDWALK_CB_LOOPBACK = (1 << 4),

	/**
	 * Adjusts #ID.us reference-count.
	 * \note keep in sync with 'newlibadr_us' use in readfile.c
	 */
	IDWALK_CB_USER = (1 << 8),
	/** Ensure #ID.us is at least 1 on use. */
	IDWALK_CB_USER_ONE = (1 << 9),
};

enum {
	IDWALK_RET_NOP            = 0,
	IDWALK_RET_STOP_ITER      = 1 << 0,  /* Completely stop iteration. */
	IDWALK_RET_STOP_RECURSION = 1 << 1,  /* Stop recursion, that is, do not loop over ID used by current one. */
};

/**
 * Call a callback for each ID link which the given ID uses.
 *
 * \return a set of flags to control further iteration (0 to keep going).
 */
typedef int (*LibraryIDLinkCallback) (void *user_data, struct ID *id_self, struct ID **id_pointer, int cb_flag);

/* Flags for the foreach function itself. */
enum {
	IDWALK_NOP      = 0,
	IDWALK_READONLY = (1 << 0),
	IDWALK_RECURSE  = (1 << 1),  /* Also implies IDWALK_READONLY. */

	IDWALK_NO_INDIRECT_PROXY_DATA_USAGE = (1 << 8),  /* Ugly special case :(((( */
};

/* Loop over all of the ID's this datablock links to. */
void BKE_library_foreach_ID_link(
        struct Main *bmain, struct ID *id, LibraryIDLinkCallback callback, void *user_data, int flag);
void BKE_library_update_ID_link_user(struct ID *id_dst, struct ID *id_src, const int cb_flag);

int BKE_library_ID_use_ID(struct ID *id_user, struct ID *id_used);

bool BKE_library_id_can_use_idtype(struct ID *id_owner, const short id_type_used);

bool BKE_library_ID_is_locally_used(struct Main *bmain, void *idv);
bool BKE_library_ID_is_indirectly_used(struct Main *bmain, void *idv);
void BKE_library_ID_test_usages(struct Main *bmain, void *idv, bool *is_used_local, bool *is_used_linked);

void BKE_library_unused_linked_data_set_tag(struct Main *bmain, const bool do_init_tag);
void BKE_library_indirectly_used_data_tag_clear(struct Main *bmain);

#endif  /* __BKE_LIBRARY_QUERY_H__ */
