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

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BLI_blenlib.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_spacetypes.h"

/***/

struct _SpaceType {
	char			name[32];
	
	SpacePrefetchDrawFP winprefetchdraw;
	SpaceDrawFP		windraw;
	SpaceChangeFP	winchange;
	SpaceHandleFP	winhandle;
};

/* local prototypes ----------------------- */
void scrarea_do_windraw(ScrArea *area);
void scrarea_do_winchange(ScrArea *area);
void scrarea_do_winhandle(ScrArea *area, BWinEvent *evt);

SpaceType *spacetype_new(char *name)
{
	SpaceType *st= calloc(1, sizeof(*st));
	BLI_strncpy(st->name, name, sizeof(st->name));

	return st;
}

void spacetype_set_winfuncs(SpaceType *st, SpacePrefetchDrawFP prefetchdraw, SpaceDrawFP draw, SpaceChangeFP change, SpaceHandleFP handle) 
{
	st->winprefetchdraw = prefetchdraw;
	st->windraw= draw;
	st->winchange= change;
	st->winhandle= handle;
}

	/* gets SpaceType struct, but also has patch to initialize unknown spacetypes */

static SpaceType *spacetype_from_area(ScrArea *area)
{
	switch (area->spacetype) {
	case SPACE_ACTION:	return spaceaction_get_type();
	case SPACE_BUTS:	return spacebuts_get_type();
	case SPACE_FILE:	return spacefile_get_type();
	case SPACE_IMAGE:	return spaceimage_get_type();
	case SPACE_IMASEL:	return spaceimasel_get_type();
	case SPACE_INFO:	return spaceinfo_get_type();
	case SPACE_IPO:		return spaceipo_get_type();
	case SPACE_NLA:		return spacenla_get_type();
	case SPACE_OOPS:	return spaceoops_get_type();
	case SPACE_SEQ:		return spaceseq_get_type();
	case SPACE_SOUND:	return spacesound_get_type();
	case SPACE_TEXT:	return spacetext_get_type();
	case SPACE_SCRIPT:	return spacescript_get_type();
	case SPACE_VIEW3D:	return spaceview3d_get_type();
	case SPACE_TIME:	return spacetime_get_type();
	case SPACE_NODE:	return spacenode_get_type();
	default:
		newspace(area, SPACE_VIEW3D);
		return spaceview3d_get_type();
		return NULL;
	}
}

void scrarea_do_winprefetchdraw(ScrArea *area)
{
	SpaceType *st= spacetype_from_area(area);
	
	areawinset(area->win);

	if(area->win && st->winprefetchdraw) {
		st->winprefetchdraw(area, area->spacedata.first);
	}
}

void scrarea_do_windraw(ScrArea *area)
{
	SpaceType *st= spacetype_from_area(area);
	
	areawinset(area->win);

	if(area->win && st->windraw) {
		st->windraw(area, area->spacedata.first);
	}
	else {
		glClearColor(0.4375, 0.4375, 0.4375, 0.0); 
		glClear(GL_COLOR_BUFFER_BIT);
	}
	
	area->win_swap= WIN_BACK_OK;
}
void scrarea_do_winchange(ScrArea *area)
{
	SpaceType *st= spacetype_from_area(area);

	areawinset(area->win);

	if (st->winchange) {
		st->winchange(area, area->spacedata.first);
	} else {
		if (!BLI_rcti_is_empty(&area->winrct)) {
			bwin_ortho2(area->win, -0.375, area->winrct.xmax-area->winrct.xmin-0.375, -0.375, area->winrct.ymax-area->winrct.ymin-0.375);
			glLoadIdentity();
		}
	}
}
void scrarea_do_winhandle(ScrArea *area, BWinEvent *evt)
{
	SpaceType *st= spacetype_from_area(area);

	areawinset(area->win);
	
	if (st->winhandle) {
		st->winhandle(area, area->spacedata.first, evt);
	}
}
