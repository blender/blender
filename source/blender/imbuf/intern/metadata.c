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

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_idprop.h"

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_metadata.h"



void IMB_metadata_free(struct ImBuf *img)
{
	if (!img)
		return;
	if (!img->metadata) {
		return;
	}

	IDP_FreeProperty(img->metadata);
	MEM_freeN(img->metadata);
}

bool IMB_metadata_get_field(struct ImBuf *img, const char *key, char *field, const size_t len)
{
	IDProperty *prop;

	bool retval = false;

	if (!img)
		return false;
	if (!img->metadata)
		return false;

	prop = IDP_GetPropertyFromGroup(img->metadata, key);

	if (prop && prop->type == IDP_STRING) {
		BLI_strncpy(field, IDP_String(prop), len);
		retval = true;
	}
	return retval;
}

void IMB_metadata_copy(struct ImBuf *dimb, struct ImBuf *simb)
{
	BLI_assert(dimb != simb);
	if (simb->metadata) {
		IMB_metadata_free(dimb);
		dimb->metadata = IDP_CopyProperty(simb->metadata);
	}
}

bool IMB_metadata_add_field(struct ImBuf *img, const char *key, const char *value)
{
	IDProperty *prop;

	if (!img)
		return false;

	if (!img->metadata) {
		IDPropertyTemplate val;
		img->metadata = IDP_New(IDP_GROUP, &val, "metadata");
	}

	prop = IDP_NewString(value, key, 512);
	return IDP_AddToGroup(img->metadata, prop);
}

bool IMB_metadata_del_field(struct ImBuf *img, const char *key)
{
	IDProperty *prop;

	if ((!img) || (!img->metadata))
		return false;

	prop = IDP_GetPropertyFromGroup(img->metadata, key);

	if (prop) {
		IDP_FreeFromGroup(img->metadata, prop);
	}
	return false;
}

bool IMB_metadata_change_field(struct ImBuf *img, const char *key, const char *field)
{
	IDProperty *prop;

	if (!img)
		return false;

	prop = (img->metadata) ? IDP_GetPropertyFromGroup(img->metadata, key) : NULL;

	if (!prop) {
		return (IMB_metadata_add_field(img, key, field));
	}
	else if (prop->type == IDP_STRING) {
		IDP_AssignString(prop, field, 1024);
		return true;
	}
	else {
		return false;
	}
}
