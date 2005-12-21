/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"

#include "BLI_ghash.h"

#include "BKE_icons.h"

#define GS(a)	(*((short *)(a)))

/* GLOBALS */

static GHash* gIcons = 0;

static int gNextIconId = 1;

static int gFirstIconId = 1;


static void icon_free(void *val)
{
	Icon* icon = val;

	if (icon)
	{
		if (icon->drawinfo_free) {		
			icon->drawinfo_free(icon->drawinfo);
		}
		else if (icon->drawinfo) {
			MEM_freeN(icon->drawinfo);
		}
		MEM_freeN(icon);
	}
}

/* create an id for a new icon and make sure that ids from deleted icons get reused
   after the integer number range is used up */
static int get_next_free_id()
{
	int startId = gFirstIconId;

	/* if we haven't used up the int number range, we just return the next int */
	if (gNextIconId>=gFirstIconId)
		return gNextIconId++;
	
	/* now we try to find the smallest icon id not stored in the gIcons hash */
	while (BLI_ghash_lookup(gIcons, (void *)startId) && startId>=gFirstIconId) 
		startId++;

	/* if we found a suitable one that isnt used yet, return it */
	if (startId>=gFirstIconId)
		return startId;

	/* fail */
	return 0;
}

void BKE_icons_init(int first_dyn_id)
{
	gNextIconId = first_dyn_id;
	gFirstIconId = first_dyn_id;

	if (!gIcons)
		gIcons = BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
}

void BKE_icons_free()
{
	BLI_ghash_free(gIcons, 0, icon_free);
	gIcons = 0;
}


void BKE_icon_changed(int id)
{
	Icon* icon = 0;
	
	if (!id) return;

	icon = BLI_ghash_lookup(gIcons, (void *)id);
	
	if (icon)
	{
		icon->changed = 1;
	}	
}

int BKE_icon_getid(struct ID* id)
{
	Icon* new_icon = 0;

	if (!id)
		return 0;

	if (id->icon_id)
		return id->icon_id;

	id->icon_id = get_next_free_id();

	if (!id->icon_id){
		printf("BKE_icon_getid: Internal error - not enough IDs\n");
		return 0;
	}

	new_icon = MEM_callocN(sizeof(Icon), "texicon");

	new_icon->obj = id;
	new_icon->type = GS(id->name);
	
	/* next two lines make sure image gets created */
	new_icon->drawinfo = 0;
	new_icon->drawinfo_free = 0;
	new_icon->changed = 1; 

	BLI_ghash_insert(gIcons, (void *)id->icon_id, new_icon);
	
	return id->icon_id;
}

Icon* BKE_icon_get(int icon_id)
{
	Icon* icon = 0;

	icon = BLI_ghash_lookup(gIcons, (void*)icon_id);
	
	if (!icon) {
		printf("BKE_icon_get: Internal error, no icon for icon ID: %d\n", icon_id);
		return 0;
	}

	return icon;
}

void BKE_icon_set(int icon_id, struct Icon* icon)
{
	Icon* old_icon = 0;

	old_icon = BLI_ghash_lookup(gIcons, (void*)icon_id);

	if (old_icon)
	{
		printf("BKE_icon_set: Internal error, icon already set: %d\n", icon_id);
		return;
	}

	BLI_ghash_insert(gIcons, (void *)icon_id, icon);
}

void BKE_icon_delete(struct ID* id)
{
	Icon* new_icon = 0;

	if (!id->icon_id) return; /* no icon defined for library object */

	BLI_ghash_remove(gIcons, (void*)id->icon_id, 0, icon_free);
	id->icon_id = 0;
}
