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

#include "blendef.h"
#include "MEM_guardedalloc.h"

#include "DNA_color_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "BSE_drawview.h"	// for do_viewbuttons.c .... hurms
#include "BSE_node.h"

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

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "mydevice.h"
#include "butspace.h" // own module

/************************ function prototypes ***************************/
void drawbutspace(ScrArea *, void *);


/* Local vars ---------------------------------------------------------- */
short bgpicmode=0, near=1000, far=1000;
MTex emptytex;
MTex mtexcopybuf;

char texstr[20][12]= {"None"  , "Clouds" , "Wood", "Marble", "Magic"  , "Blend",
					 "Stucci", "Noise"  , "Image", "Plugin", "EnvMap" , "Musgrave",
					 "Voronoi", "DistNoise", "", "", "", "", "", ""};
/*  ---------------------------------------------------------------------- */

void test_idbutton_cb(void *namev, void *arg2)
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
	*idpp= NULL;
}

void test_actionpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	id= G.main->action.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			id_us_plus(id);
			*idpp= id;
			return;
		}
		id= id->next;
	}
	*idpp= NULL;
}


void test_obpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if(idpp == (ID **)&(emptytex.object)) {
		error("You must add a texture first");
		*idpp= 0;
		return;
	}
	
	id= G.main->object.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_lib_extern(id);	/* checks lib data, sets correct flag for saving then */
			return;
		}
		id= id->next;
	}
	*idpp= NULL;
}

/* tests for an object of type OB_MESH */
void test_meshobpoin_but(char *name, ID **idpp)
{
	ID *id;

	id = G.main->object.first;
	while(id) {
		Object *ob = (Object *)id;
		if(ob->type == OB_MESH && strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			/* checks lib data, sets correct flag for saving then */
			id_lib_extern(id);
			return;
		}
		id = id->next;
	}
	*idpp = NULL;
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
	*idpp= NULL;
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
	*idpp= NULL;
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
	*idpp= NULL;
}

void test_grouppoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if( *idpp ) (*idpp)->us--;
	
	id= G.main->group.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= NULL;
}

void test_texpoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if( *idpp ) (*idpp)->us--;
	
	id= G.main->tex.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= NULL;
}

void test_imapoin_but(char *name, ID **idpp)
{
	ID *id;
	
	if( *idpp ) (*idpp)->us--;
	
	id= G.main->image.first;
	while(id) {
		if( strcmp(name, id->name+2)==0 ) {
			*idpp= id;
			id_us_plus(id);
			return;
		}
		id= id->next;
	}
	*idpp= NULL;
}

/* ----------- custom button group ---------------------- */

static void curvemap_buttons_zoom_in(void *cumap_v, void *unused)
{
	CurveMapping *cumap = cumap_v;
	float d;
	
	/* we allow 20 times zoom */
	if( (cumap->curr.xmax - cumap->curr.xmin) > 0.04f*(cumap->clipr.xmax - cumap->clipr.xmin) ) {
		d= 0.1154f*(cumap->curr.xmax - cumap->curr.xmin);
		cumap->curr.xmin+= d;
		cumap->curr.xmax-= d;
		d= 0.1154f*(cumap->curr.ymax - cumap->curr.ymin);
		cumap->curr.ymin+= d;
		cumap->curr.ymax-= d;
	}
}

static void curvemap_buttons_zoom_out(void *cumap_v, void *unused)
{
	CurveMapping *cumap = cumap_v;
	float d, d1;
	
	/* we allow 20 times zoom, but dont view outside clip */
	if( (cumap->curr.xmax - cumap->curr.xmin) < 20.0f*(cumap->clipr.xmax - cumap->clipr.xmin) ) {
		d= d1= 0.15f*(cumap->curr.xmax - cumap->curr.xmin);
		
		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.xmin-d < cumap->clipr.xmin)
				d1= cumap->curr.xmin - cumap->clipr.xmin;
		cumap->curr.xmin-= d1;
		
		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.xmax+d > cumap->clipr.xmax)
				d1= -cumap->curr.xmax + cumap->clipr.xmax;
		cumap->curr.xmax+= d1;
		
		d= d1= 0.15f*(cumap->curr.ymax - cumap->curr.ymin);
		
		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.ymin-d < cumap->clipr.ymin)
				d1= cumap->curr.ymin - cumap->clipr.ymin;
		cumap->curr.ymin-= d1;
		
		if(cumap->flag & CUMA_DO_CLIP) 
			if(cumap->curr.ymax+d > cumap->clipr.ymax)
				d1= -cumap->curr.ymax + cumap->clipr.ymax;
		cumap->curr.ymax+= d1;
	}
}

static void curvemap_buttons_setclip(void *cumap_v, void *unused)
{
	CurveMapping *cumap = cumap_v;
	
	curvemapping_changed(cumap, 0);
}	

static void curvemap_buttons_delete(void *cumap_v, void *unused)
{
	CurveMapping *cumap = cumap_v;
	
	curvemap_remove(cumap->cm+cumap->cur, SELECT);
	curvemapping_changed(cumap, 0);
}

/* NOTE: this is a block-menu, needs 0 events, otherwise the menu closes */
static uiBlock *curvemap_clipping_func(void *cumap_v)
{
	CurveMapping *cumap = cumap_v;
	uiBlock *block;
	uiBut *bt;
	
	block= uiNewBlock(&curarea->uiblocks, "curvemap_clipping_func", UI_EMBOSS, UI_HELV, curarea->win);
	
	/* use this for a fake extra empy space around the buttons */
	uiDefBut(block, LABEL, 0, "",			-4, 16, 128, 106, NULL, 0, 0, 0, 0, "");
	
	bt= uiDefButBitI(block, TOG, CUMA_DO_CLIP, 1, "Use Clipping",	 
										0,100,120,18, &cumap->flag, 0.0, 0.0, 10, 0, "");
	uiButSetFunc(bt, curvemap_buttons_setclip, cumap, NULL);

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, 0, "Min X ",	 0,74,120,18, &cumap->clipr.xmin, -100.0, cumap->clipr.xmax, 10, 0, "");
	uiDefButF(block, NUM, 0, "Min Y ",	 0,56,120,18, &cumap->clipr.ymin, -100.0, cumap->clipr.ymax, 10, 0, "");
	uiDefButF(block, NUM, 0, "Max X ",	 0,38,120,18, &cumap->clipr.xmax, cumap->clipr.xmin, 100.0, 10, 0, "");
	uiDefButF(block, NUM, 0, "Max Y ",	 0,20,120,18, &cumap->clipr.ymax, cumap->clipr.ymin, 100.0, 10, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	
	return block;
}


static void curvemap_tools_dofunc(void *cumap_v, int event)
{
	CurveMapping *cumap = cumap_v;
	CurveMap *cuma= cumap->cm+cumap->cur;
	
	switch(event) {
		case 0:
			curvemap_reset(cuma, &cumap->clipr);
			curvemapping_changed(cumap, 0);
			break;
		case 1:
			cumap->curr= cumap->clipr;
			break;
		case 2:	/* set vector */
			curvemap_sethandle(cuma, 1);
			curvemapping_changed(cumap, 0);
			break;
		case 3: /* set auto */
			curvemap_sethandle(cuma, 0);
			curvemapping_changed(cumap, 0);
			break;
		case 4: /* extend horiz */
			cuma->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
			curvemapping_changed(cumap, 0);
			break;
		case 5: /* extend extrapolate */
			cuma->flag |= CUMA_EXTEND_EXTRAPOLATE;
			curvemapping_changed(cumap, 0);
			break;
	}
	addqueue(curarea->win, REDRAW, 1);
}

static uiBlock *curvemap_tools_func(void *cumap_v)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "curvemap_tools_func", UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, curvemap_tools_dofunc, cumap_v);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset View",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Vector Handle",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Auto Handle",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extend Horizontal",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extend Extrapolated",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset Curve",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
	return block;
}

/* still unsure how this call evolves... we use labeltype for defining what curve-channels to show */
void curvemap_buttons(uiBlock *block, CurveMapping *cumap, char labeltype, short event, short redraw, rctf *rect)
{
	uiBut *bt;
	float dx, fy= rect->ymax-18.0f;
	int icon;
	short xco, yco;
	
	yco= (short)(rect->ymax-18.0f);
	
	/* curve choice options + tools/settings, 8 icons + spacer */
	dx= (rect->xmax-rect->xmin)/(9.0f);
	
	uiBlockBeginAlign(block);
	if(labeltype=='v') {	/* vector */
		xco= (short)rect->xmin;
		if(cumap->cm[0].curve)
			uiDefButI(block, ROW, redraw, "X", xco, yco+2, dx, 16, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
		xco= (short)(rect->xmin+1.0f*dx);
		if(cumap->cm[1].curve)
			uiDefButI(block, ROW, redraw, "Y", xco, yco+2, dx, 16, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
		xco= (short)(rect->xmin+2.0f*dx);
		if(cumap->cm[2].curve)
			uiDefButI(block, ROW, redraw, "Z", xco, yco+2, dx, 16, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
	}
	else if(labeltype=='c') { /* color */
		xco= (short)rect->xmin;
		if(cumap->cm[3].curve)
			uiDefButI(block, ROW, redraw, "C", xco, yco+2, dx, 16, &cumap->cur, 0.0, 3.0, 0.0, 0.0, "");
		xco= (short)(rect->xmin+1.0f*dx);
		if(cumap->cm[0].curve)
			uiDefButI(block, ROW, redraw, "R", xco, yco+2, dx, 16, &cumap->cur, 0.0, 0.0, 0.0, 0.0, "");
		xco= (short)(rect->xmin+2.0f*dx);
		if(cumap->cm[1].curve)
			uiDefButI(block, ROW, redraw, "G", xco, yco+2, dx, 16, &cumap->cur, 0.0, 1.0, 0.0, 0.0, "");
		xco= (short)(rect->xmin+3.0f*dx);
		if(cumap->cm[2].curve)
			uiDefButI(block, ROW, redraw, "B", xco, yco+2, dx, 16, &cumap->cur, 0.0, 2.0, 0.0, 0.0, "");
	}
	/* else no channels ! */
	uiBlockEndAlign(block);

	xco= (short)(rect->xmin+4.5f*dx);
	uiBlockSetEmboss(block, UI_EMBOSSN);
	bt= uiDefIconBut(block, BUT, redraw, ICON_ZOOMIN, xco, yco, dx, 14, NULL, 0.0, 0.0, 0.0, 0.0, "Zoom in");
	uiButSetFunc(bt, curvemap_buttons_zoom_in, cumap, NULL);
	
	xco= (short)(rect->xmin+5.25f*dx);
	bt= uiDefIconBut(block, BUT, redraw, ICON_ZOOMOUT, xco, yco, dx, 14, NULL, 0.0, 0.0, 0.0, 0.0, "Zoom out");
	uiButSetFunc(bt, curvemap_buttons_zoom_out, cumap, NULL);
	
	xco= (short)(rect->xmin+6.0f*dx);
	bt= uiDefIconBlockBut(block, curvemap_tools_func, cumap, event, ICON_MODIFIER, xco, yco, dx, 18, "Tools");
	
	xco= (short)(rect->xmin+7.0f*dx);
	if(cumap->flag & CUMA_DO_CLIP) icon= ICON_CLIPUV_HLT; else icon= ICON_CLIPUV_DEHLT;
	bt= uiDefIconBlockBut(block, curvemap_clipping_func, cumap, event, icon, xco, yco, dx, 18, "Clipping Options");
	
	xco= (short)(rect->xmin+8.0f*dx);
	bt= uiDefIconBut(block, BUT, event, ICON_X, xco, yco, dx, 18, NULL, 0.0, 0.0, 0.0, 0.0, "Delete points");
	uiButSetFunc(bt, curvemap_buttons_delete, cumap, NULL);
	
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	uiDefBut(block, BUT_CURVE, event, "", 
			  rect->xmin, rect->ymin, rect->xmax-rect->xmin, fy-rect->ymin, 
			  cumap, 0.0f, 1.0f, 0, 0, "");
	
	
}


/* --------------------------------- */

/* nodes have button callbacks, that can draw in butspace too. need separate handling */
static void do_node_buts(unsigned short event)
{
	Material *ma;

	/* all operations default on active material layer here */
	/* but this also gets called for lamp and world... */
	ma= G.buts->lockpoin;
	if(ma && GS(ma->id.name)==ID_MA)
		ma = editnode_get_active_material(ma);
	else
		ma= NULL;
	
	if(event>=B_NODE_EXEC) {
		if(ma) end_render_material(ma);	/// temporal... 3d preview
		BIF_preview_changed(ID_MA);
		allqueue(REDRAWNODE, 0);
		allqueue(REDRAWBUTSSHADING, 0);
	}		
}

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
	else if(event<=B_VGROUPBUTS) {
		do_vgroupbuts(event);
	}
	else if(event<=B_CURVEBUTS) {
		do_curvebuts(event);
	}
	else if(event<=B_FONTBUTS) {
		do_fontbuts(event);
	}
	else if(event<=B_ARMBUTS) {
		do_armbuts(event);
	}
	else if(event<=B_CAMBUTS) {
		do_cambuts(event);
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
	else if(event<=B_UVAUTOCALCBUTS) {
		do_uvcalculationbuts(event);
	}
	else if(event<=B_EFFECTSBUTS) {
		do_effects_panels(event);
	}
	else if(event<=B_MODIFIER_BUTS) {
		extern void do_modifier_panels(unsigned short event);
		do_modifier_panels(event);
	}
	else if(event<=B_NODE_BUTS) {
		do_node_buts(event);
	}
	else if(event==REDRAWVIEW3D) allqueue(event, 1);	// 1=do header too
	else if(event>REDRAWVIEW3D) allqueue(event, 0);
}

static void butspace_context_switch(SpaceButs *buts, Object *new)
{
	// change type automatically
	if(new) {
		int tab= buts->tab[CONTEXT_SHADING];
		
		if(tab == TAB_SHADING_WORLD) {
			if(new->type==OB_CAMERA);
			else if(new->type==OB_LAMP) {
				buts->tab[CONTEXT_SHADING]= TAB_SHADING_LAMP;
			}
			else buts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
			
		}
		else if(tab == TAB_SHADING_TEX) {
			if(new->type==OB_LAMP) buts->texfrom= 2;
			else if(new->type==OB_CAMERA) buts->texfrom= 1;
			else buts->texfrom= 0;
		}
		else if(tab == TAB_SHADING_RAD) {
		}
		else if(new->type==OB_CAMERA) {
			buts->tab[CONTEXT_SHADING]= TAB_SHADING_WORLD;
		}
		else if(new->type==OB_LAMP) {
			buts->tab[CONTEXT_SHADING]= TAB_SHADING_LAMP;
		}
		else {
			buts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
		}
	}
}

/* new active object */
void redraw_test_buttons(Object *new)
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
			
				if(new && buts->mainb==CONTEXT_SHADING) {
					/* does node previews too... */
					BIF_preview_changed(ID_TE);
				}
			}
			// always do context switch
			if(new) butspace_context_switch(buts, new);

		}
		sa= sa->next;
	}
}


/* callback */
void drawbutspace(ScrArea *sa, void *spacedata)
{
	ID *id, *idfrom;
	SpaceButs *sbuts= sa->spacedata.first;
	View2D *v2d= &sbuts->v2d;
	float col[3];
	int tab, align=0;
	
	/* context */
	buttons_active_id(&id, &idfrom);
	G.buts->lockpoin= id;
	
	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);

	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	uiSetButLock(G.scene->id.lib!=0, ERROR_LIBDATA_MESSAGE);	
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
		tab= sbuts->tab[CONTEXT_OBJECT];
		
		if(tab==TAB_OBJECT_OBJECT)
		   object_panels();
		else if(tab==TAB_OBJECT_PHYSICS)
			physics_panels();
		else if(tab==TAB_OBJECT_PARTICLE)
			particle_panels();
		   
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

	/* when align changes, also do this for new panels */
	/* don't always align, this function is called during AnmatePanels too */
	if(sbuts->align)
		if(sbuts->re_align || sbuts->mainbo!=sbuts->mainb || sbuts->tabo!=sbuts->tab[sbuts->mainb])
			align= 1;

	uiDrawBlocksPanels(sa, align);	
	
	/* since panels give different layouts, we have to make sure v2d.tot matches */
	uiMatchPanel_view2d(sa);

	sbuts->re_align= 0;
	// also for memory for finding which texture you'd like to see
	sbuts->mainbo= sbuts->mainb;
	sbuts->tabo= sbuts->tab[sbuts->mainb];

	myortho2(-0.375, (float)(sa->winx)-0.375, -0.375, (float)(sa->winy)-0.375);
	draw_area_emboss(sa);
	myortho2(v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);

	/* always in end */
	sa->win_swap= WIN_BACK_OK;
}


