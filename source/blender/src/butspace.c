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

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "BSE_drawview.h"	// for do_viewbuttons.c .... hurms

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_butspace.h"
#include "BSE_headerbuttons.h"
#include "BIF_previewrender.h"
#include "BIF_mywindow.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"

#include "mydevice.h"
#include "butspace.h" // own module


/* Local vars ---------------------------------------------------------- */
short bgpicmode=0, near=1000, far=1000;
MTex emptytex;
MTex mtexcopybuf;

char texstr[15][8]= {"None"  , "Clouds" , "Wood", "Marble", "Magic"  , "Blend",
					 "Stucci", "Noise"  , "Image", "Plugin", "EnvMap" , "",
					 ""      , ""       , ""};
/*  ---------------------------------------------------------------------- */

void test_idbutton_cb(void *namev, void *arg2_unused)
{
	char *name= namev;
	test_idbutton(name+2);
}


void test_scriptpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	id= G.main->text.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_actionpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	id= G.main->action.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}


void test_obpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if(idpp == (ID **)&(emptytex.object)) {
		error("You must add a Texture first!");
		*idpp= 0;
		return;
	}
	
	id= G.main->object.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_obcurpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if(idpp == (ID **)&(emptytex.object)) {
		error("You must add a Texture first!");
		*idpp= 0;
		return;
	}
	
	id= G.main->object.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			if (((Object *)id)->type != OB_CURVE) {
				error ("Bevel object must be a Curve.");
				break;
			} 
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_meshpoin_but(char *name, ID **idpp)
{
	ID *id;

	if( *idpp ) (*idpp)->us--;
	
	id= G.main->mesh.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_matpoin_but(char *name, ID **idpp)
{
	ID *id;

	if( *idpp ) (*idpp)->us--;
	
	id= G.main->mat.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}

void test_scenepoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if( *idpp ) (*idpp)->us--;
	
	id= G.main->scene.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= 0;
}


/* --------------------------------- */




void do_butspace(unsigned short event)
{
	SpaceButs *buts;

	/* redraw windows of the same type? */
	buts= curarea->spacedata.first;
	if(buts->mainb==CONTEXT_SCENE) allqueue(REDRAWBUTSSCENE, curarea->win);
	if(buts->mainb==CONTEXT_OBJECT) allqueue(REDRAWBUTSOBJECT, curarea->win);
	if(buts->mainb==CONTEXT_SHADING) allqueue(REDRAWBUTSSHADING, curarea->win);
	if(buts->mainb==CONTEXT_EDITING) allqueue(REDRAWBUTSEDIT, curarea->win);
	if(buts->mainb==CONTEXT_SCRIPT) allqueue(REDRAWBUTSSCRIPT, curarea->win);
	if(buts->mainb==CONTEXT_LOGIC) allqueue(REDRAWBUTSLOGIC, curarea->win);
	
	if (event <=50){
		do_global_buttons2(event);
	}
	else if(event<=100) {
		do_global_buttons(event);
	}
	else if(event < 1000) {
		do_headerbuttons(event);
	}
	else if(event<=B_VIEWBUTS) {
		do_viewbuts(event);
	}
	else if(event<=B_LAMPBUTS) {
		do_lampbuts(event);
	}
	else if(event<=B_MATBUTS) {
		do_matbuts(event);
	}
	else if(event<=B_TEXBUTS) {
		do_texbuts(event);
	}
	else if(event<=B_ANIMBUTS) {
		do_object_panels(event);
	}
	else if(event<=B_WORLDBUTS) {
		do_worldbuts(event);
	}
	else if(event<=B_RENDERBUTS) {
		do_render_panels(event);	// buttons_scene.c
	}
	else if(event<=B_COMMONEDITBUTS) {
		do_common_editbuts(event);
	}
	else if(event<=B_MESHBUTS) {
		do_meshbuts(event);
	}
	else if(event<=B_CURVEBUTS) {
		do_curvebuts(event);
	}
	else if(event<=B_FONTBUTS) {
		do_fontbuts(event);
	}
	else if(event<=B_CAMBUTS) {
		;
	}
	else if(event<=B_MBALLBUTS) {
		do_mballbuts(event);
	}
	else if(event<=B_LATTBUTS) {
		do_latticebuts(event);
	}
	else if(event<=B_GAMEBUTS) {
		do_logic_buts(event);	// buttons_logic.c
	}
	else if(event<=B_FPAINTBUTS) {
		do_fpaintbuts(event);
	}
	else if(event<=B_RADIOBUTS) {
		do_radiobuts(event);
	}
	else if(event<=B_SCRIPTBUTS) {
		do_scriptbuts(event);
	}
	else if(event<=B_SOUNDBUTS) {
		do_soundbuts(event);
	}
	else if(event<=B_CONSTRAINTBUTS) {
		do_constraintbuts(event);
	}
	else if(event==REDRAWVIEW3D) allqueue(event, 1);	// 1=do header too
	else if(event>REDRAWVIEW3D) allqueue(event, 0);
}

/* new active object */
void redraw_test_buttons(Base *new)
{
	ScrArea *sa;
	SpaceButs *buts;
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==SPACE_BUTS) {
			buts= sa->spacedata.first;
			
			if(ELEM5(buts->mainb, CONTEXT_OBJECT, CONTEXT_EDITING, CONTEXT_SHADING, CONTEXT_LOGIC, CONTEXT_SCRIPT)) {
				addqueue(sa->win, REDRAW, 1);
				buts->re_align= 1;
			}
			
			if(buts->mainb==CONTEXT_SHADING) {
				buts->re_align= 1;
				
				// change type automatically
				if(new) {
					int tab= buts->tab[CONTEXT_SHADING];
					
					if(tab == TAB_SHADING_WORLD) {
						if(new->object->type==OB_LAMP) {
							buts->tab[CONTEXT_SHADING]= TAB_SHADING_LAMP;
						}
						else buts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
						
					}
					else if(tab == TAB_SHADING_TEX) {
					}
					else if(tab == TAB_SHADING_RAD) {
					}
					else if(new->object->type==OB_CAMERA) {
						buts->tab[CONTEXT_SHADING]= TAB_SHADING_WORLD;
					}
					else if(new->object->type==OB_LAMP) {
						buts->tab[CONTEXT_SHADING]= TAB_SHADING_LAMP;
					}
					else {
						buts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
					}
					BIF_preview_changed(buts);
				}
			}
		}
		sa= sa->next;
	}
}


/* callback */
void drawbutspace(ScrArea *sa, void *spacedata)
{
	SpaceButs *sbuts= sa->spacedata.first;
	View2D *v2d= &sbuts->v2d;
	float col[3];
	int tab, align=0;
	
	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);

	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");	
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
 
	/* select the context to be drawn, per contex/tab the actual context is tested */
	switch(sbuts->mainb) {
	case CONTEXT_SCENE:
		tab= sbuts->tab[CONTEXT_SCENE];

		if(tab== TAB_SCENE_RENDER) 
			render_panels();
		else if(tab == TAB_SCENE_ANIM) 
			anim_panels();
		else if(tab == TAB_SCENE_SOUND) 
			sound_panels();

		break;
	case CONTEXT_OBJECT:
		/* no tabs */
		object_panels();
		break;
		
		break;
	case CONTEXT_SHADING:
		tab= sbuts->tab[CONTEXT_SHADING];
		
		if(tab==TAB_SHADING_MAT)
			material_panels();
		else if(tab==TAB_SHADING_LAMP)
			lamp_panels();
		else if(tab==TAB_SHADING_WORLD)
			world_panels();
		else if(tab==TAB_SHADING_RAD)
			radio_panels();
		else if(tab==TAB_SHADING_TEX)
			texture_panels();
			
		break;
	case CONTEXT_EDITING:
		/* no tabs */
		editing_panels();

		break;
	case CONTEXT_SCRIPT:
		script_panels();
		
		break;
	case CONTEXT_LOGIC:
		/* no tabs */
		logic_buts();
		break;
	}

	uiClearButLock();

	/* since panels give different layouts, we have to make sure v2d.tot matches */
	uiMatchPanel_view2d(sa);

	/* when align changes, also do this for new panels */
	/* don't always align, this function is called during AnmatePanels too */
	if(sbuts->align)
		if(sbuts->re_align || sbuts->mainbo!=sbuts->mainb || sbuts->tabo!=sbuts->tab[sbuts->mainb])
			align= 1;

	uiDrawBlocksPanels(sa, align);	
	
	sbuts->re_align= 0;
	// also for memory for finding which texture you'd like to see
	sbuts->mainbo= sbuts->mainb;
	sbuts->tabo= sbuts->tab[sbuts->mainb];

	myortho2(-0.5, (float)(sa->winx)-0.5, -0.5, (float)(sa->winy)-0.5);
	draw_area_emboss(sa);
	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);

	/* always in end */
	sa->win_swap= WIN_BACK_OK;
}


