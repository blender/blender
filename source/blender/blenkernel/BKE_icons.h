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
* The Original Code is Copyright (C) 2006-2007 Blender Foundation.
* All rights reserved.
* 
* The Original Code is: all of this file.
* 
* Contributor(s): none yet.
* 
* ***** END GPL LICENSE BLOCK *****
*
*/

#ifndef BKE_ICONS_H
#define BKE_ICONS_H

/*
 Resizable Icons for Blender
*/

typedef void (*DrawInfoFreeFP) (void *drawinfo);

struct Icon
{
	void *drawinfo;
	void *obj;
	short type;
	DrawInfoFreeFP drawinfo_free;
};

typedef struct Icon Icon;

struct PreviewImage;

void BKE_icons_init(int first_dyn_id);

/* return icon id for library object or create new icon if not found */
int	BKE_icon_getid(struct ID* id);

/* retrieve icon for id */
struct Icon* BKE_icon_get(int icon_id);

/* set icon for id if not already defined */
/* used for inserting the internal icons */
void BKE_icon_set(int icon_id, struct Icon* icon);

/* remove icon and free date if library object becomes invalid */
void BKE_icon_delete(struct ID* id);

/* report changes - icon needs to be recalculated */
void BKE_icon_changed(int icon_id);

/* free all icons */
void BKE_icons_free();

/* free the preview image */
void BKE_previewimg_free(struct PreviewImage **prv);

/* free the preview image belonging to the id */
void BKE_previewimg_free_id(ID *id);

/* create a new preview image */
struct PreviewImage* BKE_previewimg_create() ;

/* create a copy of the preview image */
struct PreviewImage* BKE_previewimg_copy(struct PreviewImage *prv);

/* retrieve existing or create new preview image */
PreviewImage* BKE_previewimg_get(ID *id);

#endif /*  BKE_ICONS_H */
