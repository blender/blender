/**
 * $Id: 
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

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"

#include "BLI_blenlib.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"

#include "BIF_butspace.h"

/* here the calls for building the button main/tabs tree */


static void context_scene_buttons(ScrArea *sa, SpaceButs *sbuts)
{

	/* select tabs */
	if(sbuts->tab[CONTEXT_SCENE] == TAB_SCENE_RENDER) 
		render_panels();
	
}

static void context_object_buttons(ScrArea *sa, SpaceButs *sbuts)
{

	/* select tabs */
	
}

static void context_types_buttons(ScrArea *sa, SpaceButs *sbuts)
{

	/* select tabs */
	
}

static void context_shading_buttons(ScrArea *sa, SpaceButs *sbuts)
{

	/* select tabs */
	
}

static void context_editing_buttons(ScrArea *sa, SpaceButs *sbuts)
{

	/* select tabs */
	
}

static void context_logic_buttons(ScrArea *sa, SpaceButs *sbuts)
{

	/* select tabs */
	
}

static void context_script_buttons(ScrArea *sa, SpaceButs *sbuts)
{

	/* select tabs */
	
}

/* callback */
void drawbutspace(ScrArea *sa, void *spacedata)
{
	SpaceButs *sbuts= sa->spacedata.first;
	View2D *v2d= &sbuts->v2d;

	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);

	glClearColor(0.73, 0.73, 0.73, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");	
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
 
	/* select the context to be drawn, per contex/tab the actual context is tested */
	switch(sbuts->mainb) {
	case CONTEXT_SCENE:
		context_scene_buttons(sa, sbuts);
		break;
	case CONTEXT_OBJECT:
		context_object_buttons(sa, sbuts);
		break;
	case CONTEXT_TYPES:
		context_types_buttons(sa, sbuts);
		break;
	case CONTEXT_SHADING:
		context_shading_buttons(sa, sbuts);
		break;
	case CONTEXT_EDITING:
		context_editing_buttons(sa, sbuts);
		break;
	case CONTEXT_SCRIPT:
		context_script_buttons(sa, sbuts);
		break;
	case CONTEXT_LOGIC:
		context_logic_buttons(sa, sbuts);
		break;
	}

	uiClearButLock();

	myortho2(-0.5, (float)(sa->winx)-.05, -0.5, (float)(sa->winy)-0.5);
	draw_area_emboss(sa);
	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);

	/* always in end */
	sa->win_swap= WIN_BACK_OK;
}

