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
 * The Original Code is Copyright (C) 2005 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Austin Benesh. Ton Roosendaal.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/metadata.c
 *  \ingroup imbuf
 */


#include <stdlib.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_metadata.h"



void IMB_metadata_free(struct ImBuf *img)
{
	ImMetaData *info;

	if (!img)
		return;
	if (!img->metadata) {
		return;
	}
	info = img->metadata;
	while (info) {
		ImMetaData *next = info->next;
		MEM_freeN(info->key);
		MEM_freeN(info->value);
		MEM_freeN(info);
		info = next;
	}
}

int IMB_metadata_get_field(struct ImBuf *img, const char *key, char *field, int len)
{
	ImMetaData *info;
	int retval = 0;

	if (!img)
		return 0;
	if (!img->metadata) {
		return 0;
	}
	info = img->metadata;
	while (info) {
		if (strcmp(key, info->key) == 0) {
			BLI_strncpy(field, info->value, len);
			retval = 1;
			break;
		}
		info = info->next;
	}
	return retval;
}

int IMB_metadata_add_field(struct ImBuf *img, const char *key, const char *value)
{
	ImMetaData *info;
	ImMetaData *last;

	if (!img)
		return 0;

	if (!img->metadata) {
		img->metadata = MEM_callocN(sizeof(ImMetaData), "ImMetaData");
		info = img->metadata;
	}
	else {
		info = img->metadata;
		last = info;
		while (info) {
			last = info;
			info = info->next;
		}
		info = MEM_callocN(sizeof(ImMetaData), "ImMetaData");
		last->next = info;
	}
	info->key = BLI_strdup(key);
	info->value = BLI_strdup(value);
	return 1;
}

int IMB_metadata_del_field(struct ImBuf *img, const char *key)
{
	ImMetaData *p, *p1;

	if ((!img) || (!img->metadata))
		return (0);

	p = img->metadata;
	p1 = NULL;
	while (p) {
		if (!strcmp(key, p->key)) {
			if (p1)
				p1->next = p->next;
			else
				img->metadata = p->next;

			MEM_freeN(p->key);
			MEM_freeN(p->value);
			MEM_freeN(p);
			return (1);
		}
		p1 = p;
		p = p->next;
	}
	return (0);
}

int IMB_metadata_change_field(struct ImBuf *img, const char *key, const char *field)
{
	ImMetaData *p;

	if (!img)
		return (0);

	if (!img->metadata)
		return (IMB_metadata_add_field(img, key, field));

	p = img->metadata;
	while (p) {
		if (!strcmp(key, p->key)) {
			MEM_freeN(p->value);
			p->value = BLI_strdup(field);
			return (1);
		}
		p = p->next;
	}

	return (IMB_metadata_add_field(img, key, field));
}

