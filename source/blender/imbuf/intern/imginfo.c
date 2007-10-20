/**
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <stdlib.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_imginfo.h"



void IMB_imginfo_free(struct ImBuf* img)
{
	ImgInfo *info;

	if (!img)
		return;
	if (!img->img_info) {
		return;
	}
	info = img->img_info;
	while (info) {
		ImgInfo* next = info->next;
		MEM_freeN(info->key);
		MEM_freeN(info->value);
		MEM_freeN(info);
		info = next;
	}
}

int IMB_imginfo_get_field(struct ImBuf* img, const char* key, char* field, int len)
{
	ImgInfo *info;
	int retval = 0;

	if (!img)
		return 0;
	if (!img->img_info) {
		return 0;
	}
	info = img->img_info;
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

int IMB_imginfo_add_field(struct ImBuf* img, const char* key, const char* field)
{
	ImgInfo *info;
	ImgInfo *last;

	if (!img)
		return 0;

	if (!img->img_info) {
		img->img_info = MEM_callocN(sizeof(ImgInfo), "ImgInfo");
		info = img->img_info;
	} else {
		info = img->img_info;
		last = info;
		while (info) {
			last = info;
			info = info->next;
		}
		info = MEM_callocN(sizeof(ImgInfo), "ImgInfo");
		last->next = info;
	}
	info->key = BLI_strdup(key);
	info->value = BLI_strdup(field);
	return 1;
}

int IMB_imginfo_del_field(struct ImBuf *img, const char *key)
{
	ImgInfo *p, *p1;

	if ((!img) || (!img->img_info))
		return (0);

	p = img->img_info;
	p1 = NULL;
	while (p) {
		if (!strcmp (key, p->key)) {
			if (p1)
				p1->next = p->next;
			else
				img->img_info = p->next;

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

int IMB_imginfo_change_field(struct ImBuf *img, const char *key, const char *field)
{
	ImgInfo *p;

	if (!img)
		return (0);

	if (!img->img_info)
		return (IMB_imginfo_add_field (img, key, field));

	p = img->img_info;
	while (p) {
		if (!strcmp (key, p->key)) {
			MEM_freeN (p->value);
			p->value = BLI_strdup (field);
			return (1);
		}
		p = p->next;
	}

	return (IMB_imginfo_add_field (img, key, field));
}
