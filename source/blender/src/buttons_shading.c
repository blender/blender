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

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_curve_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_world_types.h"
#include "DNA_view3d_types.h"
#include "DNA_image_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_userdef_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_utildefines.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_displist.h"
#include "DNA_radio_types.h"
#include "BKE_packedFile.h"
#include "BKE_plugin_types.h"
#include "BKE_image.h"

#include "BLI_blenlib.h"
#include "BMF_Api.h"

#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

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
#include "BIF_toolbox.h"
#include "BIF_space.h"
#include "BIF_previewrender.h"
#include "BIF_butspace.h"
#include "BIF_writeimage.h"
#include "BIF_toets.h"

#include "mydevice.h"
#include "blendef.h"
#include "radio.h"
#include "render.h"

/* -----includes for this file specific----- */

#include "butspace.h" // own module

static MTex mtexcopybuf;
static MTex emptytex;
static int packdummy = 0;


/* *************************** TEXTURE ******************************** */

Tex *cur_imatex=0;
int prv_win= 0;

void load_tex_image(char *str)	/* called from fileselect */
{
	Image *ima=0;
	Tex *tex;
	
	tex= cur_imatex;
	if(tex->type==TEX_IMAGE || tex->type==TEX_ENVMAP) {

		ima= add_image(str);
		if(ima) {
			if(tex->ima) {
				tex->ima->id.us--;
			}
			tex->ima= ima;

			free_image_buffers(ima);	/* force reading again */
			ima->ok= 1;
		}

		allqueue(REDRAWBUTSSHADING, 0);

		BIF_all_preview_changed();
	}
}

void load_plugin_tex(char *str)	/* called from fileselect */
{
	Tex *tex;
	
	tex= cur_imatex;
	if(tex->type!=TEX_PLUGIN) return;
	
	if(tex->plugin) free_plugin_tex(tex->plugin);
	
	tex->stype= 0;
	tex->plugin= add_plugin_tex(str);

	allqueue(REDRAWBUTSSHADING, 0);
	BIF_all_preview_changed();
}

int vergcband(const void *a1, const void *a2)
{
	const CBData *x1=a1, *x2=a2;

	if( x1->pos > x2->pos ) return 1;
	else if( x1->pos < x2->pos) return -1;
	return 0;
}



void save_env(char *name)
{
	Tex *tex;
	char str[FILE_MAXFILE];
	
	strcpy(str, name);
	BLI_convertstringcode(str, G.sce, G.scene->r.cfra);
	tex= G.buts->lockpoin;
	
	if(tex && GS(tex->id.name)==ID_TE) {
		if(tex->env && tex->env->ok && saveover(str)) {
			waitcursor(1);
			BIF_save_envmap(tex->env, str);
			strcpy(G.ima, name);
			waitcursor(0);
		}
	}
	
}

void drawcolorband(ColorBand *coba, float x1, float y1, float sizex, float sizey)
{
	CBData *cbd;
	float v3[2], v1[2], v2[2];
	int a;
	
	if(coba==0) return;
	
	glShadeModel(GL_SMOOTH);
	cbd= coba->data;
	
	v1[0]= v2[0]= x1;
	v1[1]= y1;
	v2[1]= y1+sizey;
	
	glBegin(GL_QUAD_STRIP);
	
	glColor3fv( &cbd->r );
	glVertex2fv(v1); glVertex2fv(v2);
	
	for(a=0; a<coba->tot; a++, cbd++) {
		
		v1[0]=v2[0]= x1+ cbd->pos*sizex;

		glColor3fv( &cbd->r );
		glVertex2fv(v1); glVertex2fv(v2);
	}
	
	v1[0]=v2[0]= x1+ sizex;
	glVertex2fv(v1); glVertex2fv(v2);
	
	glEnd();
	glShadeModel(GL_FLAT);
	

	/* outline */
	v1[0]= x1; v1[1]= y1;

	cpack(0x0);
	glBegin(GL_LINE_LOOP);
		glVertex2fv(v1);
		v1[0]+= sizex;
		glVertex2fv(v1);
		v1[1]+= sizey;
		glVertex2fv(v1);
		v1[0]-= sizex;
		glVertex2fv(v1);
	glEnd();


	/* help lines */
	
	v1[0]= v2[0]=v3[0]= x1;
	v1[1]= y1;
	v2[1]= y1+0.5*sizey;
	v3[1]= y1+sizey;
	
	cbd= coba->data;
	glBegin(GL_LINES);
	for(a=0; a<coba->tot; a++, cbd++) {
		v1[0]=v2[0]=v3[0]= x1+ cbd->pos*sizex;
		
		glColor3ub(0, 0, 0);
		glVertex2fv(v1);
		glVertex2fv(v2);

		if(a==coba->cur) {
			glVertex2f(v1[0]-1, v1[1]);
			glVertex2f(v2[0]-1, v2[1]);
			glVertex2f(v1[0]+1, v1[1]);
			glVertex2f(v2[0]+1, v2[1]);
		}
			
		glColor3ub(255, 255, 255);
		glVertex2fv(v2);
		glVertex2fv(v3);
		
		if(a==coba->cur) {
			glVertex2f(v2[0]-1, v2[1]);
			glVertex2f(v3[0]-1, v3[1]);
			glVertex2f(v2[0]+1, v2[1]);
			glVertex2f(v3[0]+1, v3[1]);
		}
	}
	glEnd();
	

	glFlush();
}



void do_texbuts(unsigned short event)
{
	Tex *tex;
	ImBuf *ibuf;
	ScrArea *sa;
	ID *id;
	CBData *cbd;
	uiBlock *block;
	float dx;
	int a, nr;
	short mvalo[2], mval[2];
	char *name, str[80];
	
	tex= G.buts->lockpoin;
	
	switch(event) {
	case B_TEXCHANNEL:
		scrarea_queue_headredraw(curarea);
		BIF_all_preview_changed();
		allqueue(REDRAWBUTSSHADING, 0);
		break;
	case B_TEXTYPE:
		if(tex==0) return;
		tex->stype= 0;
		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		break;
	case B_DEFTEXVAR:
		if(tex==0) return;
		default_tex(tex);
		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		break;
	case B_LOADTEXIMA:
		if(tex==0) return;
		/* globals: temporal store them: we make another area a fileselect */
		cur_imatex= tex;
		prv_win= curarea->win;
		
		sa= closest_bigger_area();
		areawinset(sa->win);
		if(tex->ima) name= tex->ima->name;
#ifdef _WIN32
		else {
			if (strcmp (U.textudir, "/") == 0)
				name= G.sce;
			else
				name= U.textudir;
		}
#else
		else name = U.textudir;
#endif
		
		if(G.qual==LR_CTRLKEY)
			activate_imageselect(FILE_SPECIAL, "SELECT IMAGE", name, load_tex_image);
		else 
			activate_fileselect(FILE_SPECIAL, "SELECT IMAGE", name, load_tex_image);
		
		break;
	case B_NAMEIMA:
		if(tex==0) return;
		if(tex->ima) {
			cur_imatex= tex;
			prv_win= curarea->win;
			
			/* name in tex->ima has been changed by button! */
			strcpy(str, tex->ima->name);
			if(tex->ima->ibuf) strcpy(tex->ima->name, tex->ima->ibuf->name);

			load_tex_image(str);
		}
		break;
	case B_TEXPRV:
		BIF_all_preview_changed();
		break;
	case B_TEXREDR_PRV:
		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		break;
	case B_TEXIMABROWSE:
		if(tex) {
			id= (ID*) tex->ima;
			
			if(G.buts->menunr== -2) {
				activate_databrowse(id, ID_IM, 0, B_TEXIMABROWSE, &G.buts->menunr, do_texbuts);
			} else if (G.buts->menunr>0) {
				Image *newima= (Image*) BLI_findlink(&G.main->image, G.buts->menunr-1);
				
				if (newima && newima!=(Image*) id) {
					tex->ima= newima;
					id_us_plus((ID*) newima);
					if(id) id->us--;
				
					allqueue(REDRAWBUTSSHADING, 0);
					BIF_all_preview_changed();
				}
			}
		}
		break;
	case B_IMAPTEST:
		if(tex) {
			if( (tex->imaflag & (TEX_FIELDS+TEX_MIPMAP))== TEX_FIELDS+TEX_MIPMAP ) {
				error("Cannot combine fields and mipmap");
				tex->imaflag -= TEX_MIPMAP;
				allqueue(REDRAWBUTSSHADING, 0);
			}
			
			if(tex->ima && tex->ima->ibuf) {
				ibuf= tex->ima->ibuf;
				nr= 0;
				if( !(tex->imaflag & TEX_FIELDS) && (ibuf->flags & IB_fields) ) nr= 1;
				if( (tex->imaflag & TEX_FIELDS) && !(ibuf->flags & IB_fields) ) nr= 1;
				if(nr) {
					IMB_freeImBuf(ibuf);
					tex->ima->ibuf= 0;
					tex->ima->ok= 1;
					BIF_all_preview_changed();
				}
			}
		}
		break;
	case B_RELOADIMA:
		if(tex && tex->ima) {
			// check if there is a newer packedfile

			if (tex->ima->packedfile) {
				PackedFile *pf;
				pf = newPackedFile(tex->ima->name);
				if (pf) {
					freePackedFile(tex->ima->packedfile);
					tex->ima->packedfile = pf;
				} else {
					error("Image not available. Keeping packed image.");
				}
			}

			IMB_freeImBuf(tex->ima->ibuf);
			tex->ima->ibuf= 0;
			tex->ima->ok= 1;
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIMAGE, 0);
			BIF_all_preview_changed();
		}
		allqueue(REDRAWBUTSSHADING, 0);	// redraw buttons
		
		break;

	case B_TEXSETFRAMES:
		if(tex->ima->anim) tex->frames = IMB_anim_get_duration(tex->ima->anim);
		allqueue(REDRAWBUTSSHADING, 0);
		break;

	case B_PACKIMA:
		if(tex && tex->ima) {
			if (tex->ima->packedfile) {
				if (G.fileflags & G_AUTOPACK) {
					if (okee("Disable AutoPack ?")) {
						G.fileflags &= ~G_AUTOPACK;
					}
				}
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackImage(tex->ima, PF_ASK);
				}
			} else {
				if (tex->ima->ibuf && (tex->ima->ibuf->userflags & IB_BITMAPDIRTY)) {
					error("Can't pack painted image. Save image from Image window first.");
				} else {
					tex->ima->packedfile = newPackedFile(tex->ima->name);
				}
			}
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWHEADERS, 0);
		}
		break;
	case B_LOADPLUGIN:
		if(tex==0) return;

		/* globals: store temporal: we make another area a fileselect */
		cur_imatex= tex;
		prv_win= curarea->win;
			
		sa= closest_bigger_area();
		areawinset(sa->win);
		if(tex->plugin) strcpy(str, tex->plugin->name);
		else {
			strcpy(str, U.plugtexdir);
		}
		activate_fileselect(FILE_SPECIAL, "SELECT PLUGIN", str, load_plugin_tex);
		
		break;

	case B_NAMEPLUGIN:
		if(tex==0 || tex->plugin==0) return;
		strcpy(str, tex->plugin->name);
		free_plugin_tex(tex->plugin);
		tex->stype= 0;
		tex->plugin= add_plugin_tex(str);
		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		break;
	
	case B_COLORBAND:
		if(tex==0) return;
		if(tex->coba==0) tex->coba= add_colorband();
		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		break;
	
	case B_ADDCOLORBAND:
		if(tex==0 || tex->coba==0) return;
		
		if(tex->coba->tot < MAXCOLORBAND-1) tex->coba->tot++;
		tex->coba->cur= tex->coba->tot-1;
		
		do_texbuts(B_CALCCBAND);
		
		break;

	case B_DELCOLORBAND:
		if(tex==0 || tex->coba==0 || tex->coba->tot<2) return;
		
		for(a=tex->coba->cur; a<tex->coba->tot; a++) {
			tex->coba->data[a]= tex->coba->data[a+1];
		}
		if(tex->coba->cur) tex->coba->cur--;
		tex->coba->tot--;

		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		break;

	case B_CALCCBAND:
	case B_CALCCBAND2:
		if(tex==0 || tex->coba==0 || tex->coba->tot<2) return;
		
		for(a=0; a<tex->coba->tot; a++) tex->coba->data[a].cur= a;
		qsort(tex->coba->data, tex->coba->tot, sizeof(CBData), vergcband);
		for(a=0; a<tex->coba->tot; a++) {
			if(tex->coba->data[a].cur==tex->coba->cur) {
				if(tex->coba->cur!=a) addqueue(curarea->win, REDRAW, 0);	/* button cur */
				tex->coba->cur= a;
				break;
			}
		}
		if(event==B_CALCCBAND2) return;
		
		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		
		break;
		
	case B_DOCOLORBAND:
		if(tex==0 || tex->coba==0) return;
		
		block= uiFindOpenPanelBlockName(&curarea->uiblocks, "Colors");
		if(block) {
			cbd= tex->coba->data + tex->coba->cur;
			uiGetMouse(mywinget(), mvalo);
	
			while(get_mbut() & L_MOUSE) {
				uiGetMouse(mywinget(), mval);
				if(mval[0]!=mvalo[0]) {
					dx= mval[0]-mvalo[0];
					dx/= 345.0;
					cbd->pos+= dx;
					CLAMP(cbd->pos, 0.0, 1.0);
	
					glDrawBuffer(GL_FRONT);
					uiPanelPush(block);
					drawcolorband(tex->coba, 10,150,300,20);
					uiPanelPop(block);
					glDrawBuffer(GL_BACK);
					
					do_texbuts(B_CALCCBAND2);
					cbd= tex->coba->data + tex->coba->cur;	/* because qsort */
					
					mvalo[0]= mval[0];
				}
				BIF_wait_for_statechange();
			}
			allqueue(REDRAWBUTSSHADING, 0);
			BIF_all_preview_changed();
		}
		break;
		
	case B_ENV_DELETE:
		if(tex->env) {
			RE_free_envmap(tex->env);
			tex->env= 0;
			allqueue(REDRAWBUTSSHADING, 0);
			BIF_all_preview_changed();
		}
		break;
	case B_ENV_FREE:
		if(tex->env) {
			RE_free_envmapdata(tex->env);
			allqueue(REDRAWBUTSSHADING, 0);
			BIF_all_preview_changed();
		}
		break;
	case B_ENV_FREE_ALL:
		tex= G.main->tex.first;
		while(tex) {
			if(tex->id.us && tex->type==TEX_ENVMAP) {
				if(tex->env) {
					if(tex->env->stype!=ENV_LOAD) RE_free_envmapdata(tex->env);
				}
			}
			tex= tex->id.next;
		}
		allqueue(REDRAWBUTSSHADING, 0);
		BIF_all_preview_changed();
		break;
	case B_ENV_SAVE:
		if(tex->env && tex->env->ok) {
			sa= closest_bigger_area();
			areawinset(sa->win);
			save_image_filesel_str(str);
			activate_fileselect(FILE_SPECIAL, str, G.ima, save_env);
		}
		break;	
	case B_ENV_OB:
		if(tex->env && tex->env->object) {
			BIF_all_preview_changed();
			if ELEM(tex->env->object->type, OB_CAMERA, OB_LAMP) {
				error("Camera or Lamp not allowed");
				tex->env->object= 0;
			}
		}
		break;
		
	default:
		if(event>=B_PLUGBUT && event<=B_PLUGBUT+23) {
			PluginTex *pit= tex->plugin;
			if(pit && pit->callback) {
				pit->callback(event - B_PLUGBUT);
				BIF_all_preview_changed();
				allqueue(REDRAWBUTSSHADING, 0);
			}
		}
	}
}

static void texture_panel_plugin(Tex *tex)
{
	uiBlock *block;
	VarStruct *varstr;
	PluginTex *pit;
	short xco, yco, a;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_plugin", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Plugin", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	if(tex->plugin && tex->plugin->doit) {
		
		pit= tex->plugin;
		
		for(a=0; a<pit->stypes; a++) {
			uiDefButS(block, ROW, B_TEXREDR_PRV, pit->stnames+16*a, (76*a), 152, 75, 20, &tex->stype, 2.0, (float)a, 0, 0, "");
		}
		
		varstr= pit->varstr;
		if(varstr) {
			for(a=0; a<pit->vars; a++, varstr++) {
				xco= 140*(a/6)+1;
				yco= 125 - 20*(a % 6)+1;
				uiDefBut(block, varstr->type, B_PLUGBUT+a, varstr->name, xco,yco,137,19, &(pit->data[a]), varstr->min, varstr->max, 100, 0, varstr->tip);
			}
		}
		uiDefBut(block, TEX, B_NAMEPLUGIN, "",		0,180,318,24, pit->name, 0.0, 159.0, 0, 0, "");
	}

	uiDefBut(block, BUT, B_LOADPLUGIN, "Load Plugin", 0,204,137,24, 0, 0, 0, 0, 0, "");
			
}


static void texture_panel_magic(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_magic", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Magic", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiDefButF(block, NUM, B_TEXPRV, "Size :",		10, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the pattern");
	uiDefButS(block, NUM, B_TEXPRV, "Depth:",		10, 90, 150, 19, &tex->noisedepth, 0.0, 10.0, 0, 0, "Set the depth of the pattern");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	10, 70, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the strength of the pattern");
}

static void texture_panel_blend(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_blend", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Blend", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiDefButS(block, ROW, B_TEXPRV, "Lin",		10, 180, 75, 19, &tex->stype, 2.0, 0.0, 0, 0, "Use a linear progresion"); 
	uiDefButS(block, ROW, B_TEXPRV, "Quad",		85, 180, 75, 19, &tex->stype, 2.0, 1.0, 0, 0, "Use a quadratic progression"); 
	uiDefButS(block, ROW, B_TEXPRV, "Ease",		160, 180, 75, 19, &tex->stype, 2.0, 2.0, 0, 0, ""); 
	uiDefButS(block, TOG|BIT|1, B_TEXPRV, "Flip XY",	235, 180, 75, 19, &tex->flag, 0, 0, 0, 0, "Flip the direction of the progression a quarter turn");

	uiDefButS(block, ROW, B_TEXPRV, "Diag",		10, 160, 75, 19, &tex->stype, 2.0, 3.0, 0, 0, "Use a diagonal progression");
	uiDefButS(block, ROW, B_TEXPRV, "Sphere",	85, 160, 75, 19, &tex->stype, 2.0, 4.0, 0, 0, "Use progression with the shape of a sphere");
	uiDefButS(block, ROW, B_TEXPRV, "Halo",		160, 160, 75, 19, &tex->stype, 2.0, 5.0, 0, 0, "Use a quadratic progression with the shape of a sphere");
	
}



static void texture_panel_wood(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_wood", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Wood", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiDefButS(block, ROW, B_TEXPRV, "Bands",		10, 180, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use standard wood texture"); 
	uiDefButS(block, ROW, B_TEXPRV, "Rings",		85, 180, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Use wood rings"); 
	uiDefButS(block, ROW, B_TEXPRV, "BandNoise",	160, 180, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, "Add noise to standard wood"); 
	uiDefButS(block, ROW, B_TEXPRV, "RingNoise",	235, 180, 75, 18, &tex->stype, 2.0, 3.0, 0, 0, "Add noise to rings"); 

	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	10, 160, 75, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	85, 160, 75, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");

	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 130, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	160, 130, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the turbulence of the bandnoise and ringnoise types");


}

static void texture_panel_stucci(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_stucci", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Stucci", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiDefButS(block, ROW, B_TEXPRV, "Plastic",		10, 180, 100, 19, &tex->stype, 2.0, 0.0, 0, 0, "Use standard stucci");
	uiDefButS(block, ROW, B_TEXPRV, "Wall In",		110, 180, 100, 19, &tex->stype, 2.0, 1.0, 0, 0, "Set start value"); 
	uiDefButS(block, ROW, B_TEXPRV, "Wall Out",		210, 180, 100, 19, &tex->stype, 2.0, 2.0, 0, 0, "Set end value"); 
	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	10, 160, 100, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	110, 160, 100, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");

	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	10, 90, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the depth of the stucci");
}

static void texture_panel_marble(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_marble", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Marble", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiDefButS(block, ROW, B_TEXPRV, "Soft",			10, 180, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use soft marble"); 
	uiDefButS(block, ROW, B_TEXPRV, "Sharp",		85, 180, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Use more clearly defined marble"); 
	uiDefButS(block, ROW, B_TEXPRV, "Sharper",		160, 180, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, "Use very clear defined marble"); 

	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	10, 160, 100, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	110, 160, 100, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");

	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
	uiDefButS(block, NUM, B_TEXPRV, "NoiseDepth:",	10, 90, 150, 19, &tex->noisedepth, 0.0, 6.0, 0, 0, "Set the depth of the marble calculation");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	10, 70, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Set the turbulence of the sine bands");


}

static void texture_panel_clouds(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_clouds", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Clouds", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiDefButS(block, ROW, B_TEXPRV, "Default",		10, 180, 70, 18, &tex->stype, 2.0, 0.0, 0, 0, "Use standard noise"); 
	uiDefButS(block, ROW, B_TEXPRV, "Color",		80, 180, 70, 18, &tex->stype, 2.0, 1.0, 0, 0, "Let Noise give RGB value"); 
	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	155, 180, 75, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Use soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	230, 180, 80, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Use hard noise");

	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 130, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Set the dimension of the noise table");
	uiDefButS(block, NUM, B_TEXPRV, "NoiseDepth:",	160, 130, 150, 19, &tex->noisedepth, 0.0, 6.0, 0, 0, "Set the depth of the cloud calculation");

}


static void texture_panel_envmap(Tex *tex)
{
	uiBlock *block;
	EnvMap *env;
	ID *id;
	short a, xco, yco, dx, dy;
	char *strp, str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_envmap", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Envmap", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	if(tex->env==0) {
		tex->env= RE_add_envmap();
		tex->env->object= OBACT;
	}
	if(tex->env) {
		env= tex->env;
		
		uiDefButS(block, ROW, B_REDR, 	"Static", 	10, 180, 100, 19, &env->stype, 2.0, 0.0, 0, 0, "Calculate map only once");
		uiDefButS(block, ROW, B_REDR, 	"Anim", 	110, 180, 100, 19, &env->stype, 2.0, 1.0, 0, 0, "Calculate map each rendering");
		uiDefButS(block, ROW, B_ENV_FREE, "Load", 	210, 180, 100, 19, &env->stype, 2.0, 2.0, 0, 0, "Load map from disk");
		
		if(env->stype==ENV_LOAD) {
			/* file input */
			id= (ID *)tex->ima;
			IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(G.buts->menunr));
			if(strp[0])
				uiDefButS(block, MENU, B_TEXIMABROWSE, strp, 10,135,23,20, &(G.buts->menunr), 0, 0, 0, 0, "Browse");
			MEM_freeN(strp);
		
			uiDefBut(block, BUT, B_LOADTEXIMA, "Load Image", 10,115,150,20, 0, 0, 0, 0, 0, "Load image - file select");
		
			if(tex->ima) {
				uiDefBut(block, TEX, B_NAMEIMA, "",			35,135,255,20, tex->ima->name, 0.0, 79.0, 0, 0, "Texture name");
				sprintf(str, "%d", tex->ima->id.us);
				uiDefBut(block, BUT, 0, str,				290,135,20,20, 0, 0, 0, 0, 0, "Number of users");
				uiDefBut(block, BUT, B_RELOADIMA, "Reload",	230,115,80,20, 0, 0, 0, 0, 0, "Reload");
			
				if (tex->ima->packedfile) packdummy = 1;
				else packdummy = 0;
				
				uiDefIconButI(block, TOG|BIT|0, B_PACKIMA, ICON_PACKAGE, 205,115,24,20, &packdummy, 0, 0, 0, 0, "Pack/Unpack this Image");
			}
		}
		else {
			uiDefBut(block, BUT, B_ENV_FREE, "Free Data", 	10,135,100,20, 0, 0, 0, 0, 0, "Release all images associated with environment map");
			uiDefBut(block, BUT, B_ENV_SAVE, "Save EnvMap", 110,135,100,20, 0, 0, 0, 0, 0, "Save environment map");
			uiDefBut(block, BUT, B_ENV_FREE_ALL, "Free all EnvMaps", 210,135,100,20, 0, 0, 0, 0, 0, "Frees all rendered environment maps");
		}

		uiDefIDPoinBut(block, test_obpoin_but, B_ENV_OB, "Ob:",	10,90,150,20, &(env->object), "Object name");
		if(env->stype!=ENV_LOAD) 
			uiDefButS(block, NUM, B_ENV_FREE, 	"CubeRes", 		160,90,150,20, &env->cuberes, 50, 2048.0, 0, 0, "Set the resolution in pixels");

		uiDefButF(block, NUM, B_TEXPRV, "Filter :",				10,65,150,20, &tex->filtersize, 0.1, 25.0, 0, 0, "Adjust sharpness or blurriness of the reflection"),
		uiDefButS(block, NUM, B_ENV_FREE, "Depth:",				160,65,150,20, &env->depth, 0, 5.0, 0, 0, "Number of times a map gets rendered again, for recursive mirror effect"),

		uiDefButF(block, NUM, REDRAWVIEW3D, 	"ClipSta", 		10,40,150,20, &env->clipsta, 0.01, 50.0, 100, 0, "Set start value for clipping");
		uiDefButF(block, NUM, 0, 	"ClipEnd", 					160,40,150,20, &env->clipend, 0.1, 5000.0, 1000, 0, "Set end value for clipping");

		uiDefBut(block, LABEL, 0, "Don't render layer:",		10,10,140,22, 0, 0.0, 0.0, 0, 0, "");	
		xco= 160;
		yco= 10;
		dx= 28;
		dy= 26;
		for(a=0; a<10; a++) {
			uiDefButI(block, TOG|BIT|(a+10), 0, "",(xco+a*(dx/2)), yco, (dx/2), (dy/2), &env->notlay, 0, 0, 0, 0, "Render this layer");
			uiDefButI(block, TOG|BIT|a, 0, "",	(xco+a*(dx/2)), (yco+dy/2), (dx/2), (1+dy/2), &env->notlay, 0, 0, 0, 0, "Render this layer");
			if(a==4) xco+= 5;
		}

	}
}


static void texture_panel_image1(Tex *tex)
{
	uiBlock *block;
	char str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel1", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim and Movie", "Texture", 960, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	/* print amount of frames anim */
	if(tex->ima && tex->ima->anim) {
		uiDefBut(block, BUT, B_TEXSETFRAMES, "<",      802, 110, 20, 18, 0, 0, 0, 0, 0, "Paste number of frames in Frames: button");
		sprintf(str, "%d frs  ", IMB_anim_get_duration(tex->ima->anim));
		uiDefBut(block, LABEL, 0, str,      834, 110, 90, 18, 0, 0, 0, 0, 0, "");
		sprintf(str, "%d cur  ", tex->ima->lastframe);
		uiDefBut(block, LABEL, 0, str,      834, 90, 90, 18, 0, 0, 0, 0, 0, "");
	}
	else uiDefBut(block, LABEL, 0, "<",      802, 110, 20, 18, 0, 0, 0, 0, 0, "");
			
	uiDefButS(block, NUM, B_TEXPRV, "Frames :",	642,110,150,19, &tex->frames, 0.0, 18000.0, 0, 0, "Activate animation option");
	uiDefButS(block, NUM, B_TEXPRV, "Offset :",	642,90,150,19, &tex->offset, -9000.0, 9000.0, 0, 0, "Set the number of the first picture of the animation");
	uiDefButS(block, NUM, B_TEXPRV, "Fie/Ima:",	642,60,98,19, &tex->fie_ima, 1.0, 200.0, 0, 0, "Set the number of fields per rendered frame");
	uiDefButS(block, NUM, B_TEXPRV, "StartFr:",	642,30,150,19, &tex->sfra, 1.0, 9000.0, 0, 0, "Set the start frame of the animation");
	uiDefButS(block, NUM, B_TEXPRV, "Len:",		642,10,150,19, &tex->len, 0.0, 9000.0, 0, 0, "Set the length of the animation");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, B_TEXPRV, "Fra:",		802,70,73,19, &(tex->fradur[0][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
	uiDefButS(block, NUM, B_TEXPRV, "Fra:",		802,50,73,19, &(tex->fradur[1][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
	uiDefButS(block, NUM, B_TEXPRV, "Fra:",		802,30,73,19, &(tex->fradur[2][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
	uiDefButS(block, NUM, B_TEXPRV, "Fra:",		802,10,73,19, &(tex->fradur[3][0]), 0.0, 18000.0, 0, 0, "Montage mode: frame start");
	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, B_TEXPRV, "",			879,70,37,19, &(tex->fradur[0][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
	uiDefButS(block, NUM, B_TEXPRV, "",			879,50,37,19, &(tex->fradur[1][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
	uiDefButS(block, NUM, B_TEXPRV, "",			879,30,37,19, &(tex->fradur[2][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
	uiDefButS(block, NUM, B_TEXPRV, "",			879,10,37,19, &(tex->fradur[3][1]), 0.0, 250.0, 0, 0, "Montage mode: amount of displayed frames");
	uiBlockEndAlign(block);
	uiDefButS(block, TOG|BIT|6, 0, "Cyclic",		743,60,48,19, &tex->imaflag, 0, 0, 0, 0, "Repeat animation image");
}


static void texture_panel_image(Tex *tex)
{
	uiBlock *block;
	ID *id;
	char *strp, str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_image", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Image", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	/* types */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|0, 0, "InterPol",			10, 180, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Interpolate pixels of the image");
	uiDefButS(block, TOG|BIT|1, B_TEXPRV, "UseAlpha",	85, 180, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Use the alpha layer");
	uiDefButS(block, TOG|BIT|5, B_TEXPRV, "CalcAlpha",	160, 180, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Calculate an alpha based on the RGB");
	uiDefButS(block, TOG|BIT|2, B_TEXPRV, "NegAlpha",	235, 180, 75, 18, &tex->flag, 0, 0, 0, 0, "Reverse the alpha value");
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|2, B_IMAPTEST, "MipMap",	10, 160, 60, 18, &tex->imaflag, 0, 0, 0, 0, "Generate a series of pictures used for mipmapping");
	uiDefButS(block, TOG|BIT|3, B_IMAPTEST, "Fields",	70, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Work with field images");
	uiDefButS(block, TOG|BIT|4, B_TEXPRV, "Rot90",		120, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Rotate image 90 degrees when rendered");
	uiDefButS(block, TOG|BIT|7, B_RELOADIMA, "Movie",	170, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Use a movie for an image");
	uiDefButS(block, TOG|BIT|8, 0, "Anti",				220, 160, 40, 18, &tex->imaflag, 0, 0, 0, 0, "Use anti-aliasing");
	uiDefButS(block, TOG|BIT|10, 0, "StField",			260, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "");
	uiBlockEndAlign(block);
	
	/* file input */
	id= (ID *)tex->ima;
	IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(G.buts->menunr));
	if(strp[0])
		uiDefButS(block, MENU, B_TEXIMABROWSE, strp, 10,135,23,20, &(G.buts->menunr), 0, 0, 0, 0, "Browse");
	MEM_freeN(strp);

	uiDefBut(block, BUT, B_LOADTEXIMA, "Load Image", 10,115,150,20, 0, 0, 0, 0, 0, "Load image - file view");

	if(tex->ima) {
		uiDefBut(block, TEX, B_NAMEIMA, "",			35,135,255,20, tex->ima->name, 0.0, 79.0, 0, 0, "Texture name");
		sprintf(str, "%d", tex->ima->id.us);
		uiDefBut(block, BUT, 0, str,				290,135,20,20, 0, 0, 0, 0, 0, "Number of users");
		uiDefBut(block, BUT, B_RELOADIMA, "Reload",	230,115,80,20, 0, 0, 0, 0, 0, "Reload");
	
		if (tex->ima->packedfile) packdummy = 1;
		else packdummy = 0;
		
		uiDefIconButI(block, TOG|BIT|0, B_PACKIMA, ICON_PACKAGE, 205,115,24,20, &packdummy, 0, 0, 0, 0, "Pack/Unpack this Image");
	}

	/* crop extend clip */
	
	uiDefButF(block, NUM, B_TEXPRV, "Filter :",	10,92,150,19, &tex->filtersize, 0.1, 25.0, 0, 0, "Set the filter size used by mipmap and interpol");
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, 0, "Extend",			10,70,75,19, &tex->extend, 4.0, 1.0, 0, 0, "Extend the colour of the edge");
	uiDefButS(block, ROW, 0, "Clip",			85,70,75,19, &tex->extend, 4.0, 2.0, 0, 0, "Return alpha 0.0 outside image");
	uiDefButS(block, ROW, 0, "ClipCube",		160,70,75,19, &tex->extend, 4.0, 4.0, 0, 0, "Return alpha 0.0 outside cubeshaped area around image");
	uiDefButS(block, ROW, 0, "Repeat",			235,70,75,19, &tex->extend, 4.0, 3.0, 0, 0, "Repeat image horizontally and vertically");
	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, B_TEXPRV, "Xrepeat:",	10,50,150,19, &tex->xrepeat, 1.0, 512.0, 0, 0, "Set the degree of repetition in the X direction");
	uiDefButS(block, NUM, B_TEXPRV, "Yrepeat:",	160,50,150,19, &tex->yrepeat, 1.0, 512.0, 0, 0, "Set the degree of repetition in the Y direction");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_REDR, "MinX ",		10,28,150,19, &tex->cropxmin, -10.0, 10.0, 10, 0, "Set minimum X value for cropping");
	uiDefButF(block, NUM, B_REDR, "MinY ",		10,8,150,19, &tex->cropymin, -10.0, 10.0, 10, 0, "Set minimum Y value for cropping");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_REDR, "MaxX ",		160,28,150,19, &tex->cropxmax, -10.0, 10.0, 10, 0, "Set maximum X value for cropping");
	uiDefButF(block, NUM, B_REDR, "MaxY ",		160,8,150,19, &tex->cropymax, -10.0, 10.0, 10, 0, "Set maximum Y value for cropping");
	uiBlockEndAlign(block);

}

static void drawcolorband_cb(void)
{
	ID *id, *idfrom;
	
	buttons_active_id(&id, &idfrom);
	if( GS(id->name)==ID_TE) {
		Tex *tex= (Tex *)id;
		drawcolorband(tex->coba, 10,150,300,20);
	}
}

static void texture_panel_colors(Tex *tex)
{
	uiBlock *block;
	CBData *cbd;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_colors", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Texture", "Texture");
	if(uiNewPanel(curarea, block, "Colors", "Texture", 1280, 0, 318, 204)==0) return;
		
	
	/* COLORBAND */
	uiDefButS(block, TOG|BIT|0, B_COLORBAND, "Colorband",10,180,100,20, &tex->flag, 0, 0, 0, 0, "Use colorband");

	if(tex->flag & TEX_COLORBAND) {
		uiDefBut(block, BUT, B_ADDCOLORBAND, "Add",		110,180,50,20, 0, 0, 0, 0, 0, "Add new colour to the colorband");
		uiDefButS(block, NUM, B_REDR,		"Cur:",		160,180,100,20, &tex->coba->cur, 0.0, (float)(tex->coba->tot-1), 0, 0, "The active colour from the colorband");
		uiDefBut(block, BUT, B_DELCOLORBAND, "Del",		260,180,50,20, 0, 0, 0, 0, 0, "Delete the active colour");
		uiDefBut(block, LABEL, B_DOCOLORBAND, "", 		10,150,300,20, 0, 0, 0, 0, 0, "Colorband"); /* only for event! */
		
		uiBlockSetDrawExtraFunc(block, drawcolorband_cb);
		cbd= tex->coba->data + tex->coba->cur;
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_CALCCBAND, "Pos",	10,120,80,20, &cbd->pos, 0.0, 1.0, 10, 0, "Set the position of the active colour");
		uiDefButS(block, ROW, B_TEXPRV, "E",		90,120,20,20, &tex->coba->ipotype, 5.0, 1.0, 0, 0, "Interpolation type Ease");
		uiDefButS(block, ROW, B_TEXPRV, "L",		110,120,20,20, &tex->coba->ipotype, 5.0, 0.0, 0, 0, "Interpolation type Linear");
		uiDefButS(block, ROW, B_TEXPRV, "S",		130,120,20,20, &tex->coba->ipotype, 5.0, 2.0, 0, 0, "Interpolation type Spline");
		uiDefButF(block, COL, B_BANDCOL, "",		150,120,30,20, &(cbd->r), 0, 0, 0, 0, "");
		uiDefButF(block, NUMSLI, B_TEXPRV, "A ",	180,120,130,20, &cbd->a, 0.0, 1.0, 0, 0, "Set the alpha value");
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_TEXPRV, "R ",	10,100,100,20, &cbd->r, 0.0, 1.0, B_BANDCOL, 0, "Set the red value");
		uiDefButF(block, NUMSLI, B_TEXPRV, "G ",	110,100,100,20, &cbd->g, 0.0, 1.0, B_BANDCOL, 0, "Set the green value");
		uiDefButF(block, NUMSLI, B_TEXPRV, "B ",	210,100,100,20, &cbd->b, 0.0, 1.0, B_BANDCOL, 0, "Set the blue value");
		uiBlockEndAlign(block);
	}

	/* RGB-BRICON */
	if((tex->flag & TEX_COLORBAND)==0) {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_TEXPRV, "R ",		60,80,200,20, &tex->rfac, 0.0, 2.0, 0, 0, "Set the red value");
		uiDefButF(block, NUMSLI, B_TEXPRV, "G ",		60,60,200,20, &tex->gfac, 0.0, 2.0, 0, 0, "Set the green value");
		uiDefButF(block, NUMSLI, B_TEXPRV, "B ",		60,40,200,20, &tex->bfac, 0.0, 2.0, 0, 0, "Set the blue value");
		uiBlockEndAlign(block);
	}
	uiDefButF(block, NUMSLI, B_TEXPRV, "Bright",		10,10,150,20, &tex->bright, 0.0, 2.0, 0, 0, "Set the brightness of the colour or intensity of a texture");
	uiDefButF(block, NUMSLI, B_TEXPRV, "Contr",			160,10,150,20, &tex->contrast, 0.01, 2.0, 0, 0, "Set the contrast of the colour or intensity of a texture");

}


static void texture_panel_texture(MTex *mtex, Material *ma, World *wrld, Lamp *la)
{
	extern char texstr[15][8]; // butspace.c
	MTex *mt=NULL;
	uiBlock *block;
	ID *id, *idfrom;
	int a, yco, loos;
	char str[32], *strp;
	

	block= uiNewBlock(&curarea->uiblocks, "texture_panel_texture", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Texture", "Texture", 320, 0, 318, 204)==0) return;

	/* first do the browse but */
	buttons_active_id(&id, &idfrom);

	uiBlockSetCol(block, TH_BUT_SETTING2);
	if(ma) {
		std_libbuttons(block, 10, 180, 0, NULL, B_TEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
	}
	else if(wrld) {
		std_libbuttons(block, 10, 180, 0, NULL, B_WTEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
	}
	else if(la) {
		std_libbuttons(block, 10, 180, 0, NULL, B_LTEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
	}
	uiBlockSetCol(block, TH_BUT_NEUTRAL);

	/* From button: removed */

	/* CHANNELS */
	uiBlockBeginAlign(block);
	yco= 150;
	for(a= 0; a<8; a++) {
		
		if(ma) mt= ma->mtex[a];
		else if(wrld && a<6)  mt= wrld->mtex[a];
		else if(la && a<6)  mt= la->mtex[a];
		
		if(mt && mt->tex) splitIDname(mt->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[14]= 0;

		if(ma) {
			uiDefButC(block, ROW, B_TEXCHANNEL, str,	10,yco,140,19, &(ma->texact), 0.0, (float)a, 0, 0, "Linked channel");
			yco-= 20;
		}
		else if(wrld && a<6) {
			uiDefButS(block, ROW, B_TEXCHANNEL, str,	10,yco,140,19, &(wrld->texact), 0.0, (float)a, 0, 0, "");
			yco-= 20;
		}
		else if(la && a<6) {
			uiDefButS(block, ROW, B_TEXCHANNEL, str,	10,yco,140,19, &(la->texact), 0.0, (float)a, 0, 0, "");
			yco-= 20;
		}
	}
	uiBlockEndAlign(block);
	
	uiBlockSetCol(block, TH_AUTO);

	/* TYPES */
	if(mtex && mtex->tex) {
		Tex *tex= mtex->tex;
		int xco;

		uiSetButLock(tex->id.lib!=0, "Can't edit library data");
		xco= 275;
		uiDefButS(block, ROW, B_TEXTYPE, texstr[0],			160, 150, 70, 20, &tex->type, 1.0, 0.0, 0, 0, "Default");

		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_IMAGE],	160, 110, 70, 20, &tex->type, 1.0, (float)TEX_IMAGE, 0, 0, "Use image texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_ENVMAP],240, 110, 70, 20, &tex->type, 1.0, (float)TEX_ENVMAP, 0, 0, "Use environment maps");

		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_CLOUDS],160, 90, 70, 20, &tex->type, 1.0, (float)TEX_CLOUDS, 0, 0, "Use clouds texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_MARBLE],240, 90, 70, 20, &tex->type, 1.0, (float)TEX_MARBLE, 0, 0, "Use marble texture");

		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_STUCCI],160, 70, 70, 20, &tex->type, 1.0, (float)TEX_STUCCI, 0, 0, "Use strucci texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_WOOD],	240, 70, 70, 20, &tex->type, 1.0, (float)TEX_WOOD, 0, 0, "Use wood texture");

		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_MAGIC],	160, 50, 70, 20, &tex->type, 1.0, (float)TEX_MAGIC, 0, 0, "Use magic texture");
		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_BLEND],	240, 50, 70, 20, &tex->type, 1.0, (float)TEX_BLEND, 0, 0, "Use blend texture");

		uiDefButS(block, ROW, B_TEXTYPE, texstr[TEX_NOISE],	160, 30, 70, 20, &tex->type, 1.0, (float)TEX_NOISE, 0, 0, "Use noise texture");
		if(tex->plugin && tex->plugin->doit) strp= tex->plugin->pname; else strp= texstr[TEX_PLUGIN];
		uiDefButS(block, ROW, B_TEXTYPE, strp,				240, 30, 70, 20, &tex->type, 1.0, (float)TEX_PLUGIN, 0, 0, "Use plugin");
	}
	else {
		// label to avoid centering
		uiDefBut(block, LABEL, 0, " ",	240, 10, 70, 20, 0, 0, 0, 0, 0, "");
	}
}

static void texture_panel_preview(int preview)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_preview", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Preview", "Texture", 0, 0, 318, 204)==0) return;
	
	if(preview) uiBlockSetDrawExtraFunc(block, BIF_previewdraw);

	// label to force a boundbox for buttons not to be centered
	uiDefBut(block, LABEL, 0, " ",	20,20,10,10, 0, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButC(block, ROW, B_TEXREDR_PRV, "Mat",		200,175,80,25, &G.buts->texfrom, 3.0, 0.0, 0, 0, "Display the texture of the active material");
	uiDefButC(block, ROW, B_TEXREDR_PRV, "World",	200,150,80,25, &G.buts->texfrom, 3.0, 1.0, 0, 0, "Display the texture of the world block");
	uiDefButC(block, ROW, B_TEXREDR_PRV, "Lamp",	200,125,80,25, &G.buts->texfrom, 3.0, 2.0, 0, 0, "Display the texture of the lamp");
	uiBlockEndAlign(block);
	uiDefBut(block, BUT, B_DEFTEXVAR, "Default Vars",200,10,80,20, 0, 0, 0, 0, 0, "Return to standard values");

}



/* *************************** RADIO ******************************** */

void do_radiobuts(unsigned short event)
{
	Radio *rad;
	int phase;
	
	phase= rad_phase();
	rad= G.scene->radio;
	
	switch(event) {
	case B_RAD_ADD:
		add_radio();
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_DELETE:
		delete_radio();
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_FREE:
		freeAllRad();
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_COLLECT:
		rad_collect_meshes();
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_INIT:
		if(phase==RAD_PHASE_PATCHES) {
			rad_limit_subdivide();
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_SHOOTP:
		if(phase==RAD_PHASE_PATCHES) {
			waitcursor(1);
			rad_subdivshootpatch();
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWVIEW3D, 0);
			waitcursor(0);
		}
		break;
	case B_RAD_SHOOTE:
		if(phase==RAD_PHASE_PATCHES) {
			waitcursor(1);
			rad_subdivshootelem();
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWVIEW3D, 0);
			waitcursor(0);
		}
		break;
	case B_RAD_GO:
		if(phase==RAD_PHASE_PATCHES) {
			waitcursor(1);
			rad_go();
			waitcursor(0);
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_LIMITS:
		rad_setlimits();
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSSHADING, 0);
		break;
	case B_RAD_FAC:
		set_radglobal();
		if(phase & RAD_PHASE_FACES) make_face_tab();
		else make_node_display();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_NODELIM:
		if(phase & RAD_PHASE_FACES) {
			set_radglobal();
			removeEqualNodes(rad->nodelim);
			make_face_tab();
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSSHADING, 0);
		}
		break;
	case B_RAD_NODEFILT:
		if(phase & RAD_PHASE_FACES) {
			set_radglobal();
			filterNodes();
			make_face_tab();
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_FACEFILT:
		if(phase & RAD_PHASE_FACES) {
			filterFaces();
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_RAD_DRAW:
		set_radglobal();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_ADDMESH:
		if(phase & RAD_PHASE_FACES) rad_addmesh();
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RAD_REPLACE:
		if(phase & RAD_PHASE_FACES) rad_replacemesh();
		allqueue(REDRAWVIEW3D, 0);
		break;
	}

}


#if 0
	char str[128];
		
		rad_status_str(str);
		cpack(0);
		glRasterPos2i(210, 189);
		BMF_DrawString(uiBlockGetCurFont(block), str);

#endif

static void radio_panel_calculation(Radio *rad, int flag)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "radio_panel_calculation", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Calculation", "Radio", 640, 0, 318, 204)==0) return;
	uiAutoBlock(block, 10, 10, 300, 200, UI_BLOCK_ROWS);

	if(flag != RAD_PHASE_PATCHES) uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_GO, "GO",					0, 0, 10, 15, NULL, 0, 0, 0, 0, "Start the radiosity simulation");

	uiBlockSetCol(block, TH_AUTO);
	uiDefButS(block,  NUM, 0, "SubSh Patch:", 				1, 0, 10, 10, &rad->subshootp, 0.0, 10.0, 0, 0, "Set the number of times the environment is tested to detect pathes");
	uiDefButS(block,  NUM, 0, "SubSh Element:", 			1, 0, 10, 10, &rad->subshoote, 0.0, 10.0, 0, 0, "Set the number of times the environment is tested to detect elements");

	if(flag != RAD_PHASE_PATCHES) uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_SHOOTE, "Subdiv Shoot Element", 2, 0, 10, 10, NULL, 0, 0, 0, 0, "For pre-subdivision, detect high energy changes and subdivide Elements");
	uiDefBut(block,  BUT, B_RAD_SHOOTP, "Subdiv Shoot Patch",	2, 0, 10, 10, NULL, 0, 0, 0, 0, "For pre-subdivision, Detect high energy changes and subdivide Patches");

	uiBlockSetCol(block, TH_AUTO);
	uiDefButI(block,  NUM, 0, "MaxEl:",						3, 0, 10, 10, &rad->maxnode, 1.0, 250000.0, 0, 0, "Set the maximum allowed number of elements");
	uiDefButS(block,  NUM, 0, "Max Subdiv Shoot:", 			3, 0, 10, 10, &rad->maxsublamp, 1.0, 250.0, 0, 0, "Set the maximum number of initial shoot patches that are evaluated");

	if(flag & RAD_PHASE_FACES);
	else uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_FACEFILT, "FaceFilter",		4, 0, 10, 10, NULL, 0, 0, 0, 0, "Force an extra smoothing");
	uiDefBut(block,  BUT, B_RAD_NODEFILT, "Element Filter",	4, 0, 10, 10, NULL, 0, 0, 0, 0, "Filter elements to remove aliasing artefacts");

	uiDefBut(block,  BUT, B_RAD_NODELIM, "RemoveDoubles",	5, 0, 30, 10, NULL, 0.0, 50.0, 0, 0, "Join elements which differ less than 'Lim'");
	uiBlockSetCol(block, TH_AUTO);
	uiDefButS(block,  NUM, 0, "Lim:",						5, 0, 10, 10, &rad->nodelim, 0.0, 50.0, 0, 0, "Set the range for removing doubles");


}

static void radio_panel_tool(Radio *rad, int flag)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "radio_panel_tool", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Radio Tool", "Radio", 320, 0, 318, 204)==0) return;
	uiAutoBlock(block, 10, 10, 300, 200, UI_BLOCK_ROWS);

	if(flag & RAD_PHASE_PATCHES) uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefBut(block,  BUT, B_RAD_COLLECT, "Collect Meshes",	0, 0, 10, 15, NULL, 0, 0, 0, 0, "Convert selected and visible meshes to patches");

	if(flag & RAD_PHASE_PATCHES)uiBlockSetCol(block, TH_AUTO);
	else uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_FREE, "Free Radio Data",	0, 0, 10, 15, NULL, 0, 0, 0, 0, "Release all memory used by Radiosity");	

	if(flag & RAD_PHASE_FACES) uiBlockSetCol(block, TH_AUTO);
	else uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_REPLACE, "Replace Meshes",	1, 0, 10, 12, NULL, 0, 0, 0, 0, "Convert meshes to Mesh objects with vertex colours, changing input-meshes");
	uiDefBut(block,  BUT, B_RAD_ADDMESH, "Add new Meshes",	1, 0, 10, 12, NULL, 0, 0, 0, 0, "Convert meshes to Mesh objects with vertex colours, unchanging input-meshes");

	uiBlockSetCol(block, TH_AUTO);
	uiDefButS(block,  ROW, B_RAD_DRAW, "Wire",			2, 0, 10, 10, &rad->drawtype, 0.0, 0.0, 0, 0, "Enable wireframe drawmode");
	uiDefButS(block,  ROW, B_RAD_DRAW, "Solid",			2, 0, 10, 10, &rad->drawtype, 0.0, 1.0, 0, 0, "Enable solid drawmode");
	uiDefButS(block,  ROW, B_RAD_DRAW, "Gour",			2, 0, 10, 10, &rad->drawtype, 0.0, 2.0, 0, 0, "Enable Gourad drawmode");
	uiDefButS(block,  TOG|BIT|0, B_RAD_DRAW, "ShowLim", 2, 0, 10, 10, &rad->flag, 0, 0, 0, 0, "Visualize patch and element limits");
	uiDefButS(block,  TOG|BIT|1, B_RAD_DRAW, "Z",		2, 0, 3, 10, &rad->flag, 0, 0, 0, 0, "Draw limits different");

	uiDefButS(block,  NUM, B_RAD_LIMITS, "ElMax:", 		3, 0, 10, 10, &rad->elma, 1.0, 500.0, 0, 0, "Set maximum size of an element");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "ElMin:", 		3, 0, 10, 10, &rad->elmi, 1.0, 100.0, 0, 0, "Set minimum size of an element");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "PaMax:", 		3, 0, 10, 10, &rad->pama, 10.0, 1000.0, 0, 0, "Set maximum size of a patch");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "PaMin:", 		3, 0, 10, 10, &rad->pami, 10.0, 1000.0, 0, 0, "Set minimum size of a patch");

	uiDefBut(block,  BUT, B_RAD_INIT, "Limit Subdivide", 5, 0, 10, 10, NULL, 0, 0, 0, 0, "Subdivide patches");
}


static void radio_panel_render(Radio *rad)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "radio_panel_render", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Radio Render", "Radio", 0, 0, 318, 204)==0) return;
	uiAutoBlock(block, 210, 30, 230, 150, UI_BLOCK_ROWS);

	uiDefButS(block,  NUMSLI, B_RAD_LIMITS, "Hemires:", 0, 0, 10, 10, &rad->hemires, 100.0, 1000.0, 100, 0, "Set the size of a hemicube");
	uiDefButS(block,  NUM, 0, "Max Iterations:", 		2, 0, 10, 15, &rad->maxiter, 0.0, 10000.0, 0, 0, "Maximum number of radiosity rounds");
	uiDefButF(block,  NUM, B_RAD_FAC, "Mult:",			3, 0, 10, 15, &rad->radfac, 0.001, 250.0, 100, 0, "Mulitply the energy values");
	uiDefButF(block,  NUM, B_RAD_FAC, "Gamma:",			3, 0, 10, 15, &rad->gamma, 0.2, 10.0, 10, 0, "Change the contrast of the energy values");
	uiDefButF(block,  NUMSLI, 0, "Convergence:", 		5, 0, 10, 10, &rad->convergence, 0.0, 1.0, 10, 0, "Set the lower threshold of unshot energy");
}


/* ***************************** WORLD ************************** */

void do_worldbuts(unsigned short event)
{
	World *wrld;
	MTex *mtex;
	
	switch(event) {
	case B_TEXCLEARWORLD:
		wrld= G.buts->lockpoin;
		mtex= wrld->mtex[ wrld->texact ];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
			wrld->mtex[ wrld->texact ]= 0;
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	}
}

static void world_panel_mapto(World *wrld)
{
	uiBlock *block;
	MTex *mtex;
	
	block= uiNewBlock(&curarea->uiblocks, "world_panel_mapto", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Texture and Input", "World");
	if(uiNewPanel(curarea, block, "Map To", "World", 1280, 0, 318, 204)==0) return;

	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");

	mtex= wrld->mtex[ wrld->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
		mtex->texco= TEXCO_VIEW;
	}

	/* TEXTURE OUTPUT */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	920,114,52,18, &(mtex->texflag), 0, 0, 0, 0, "Use stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		974,114,38,18, &(mtex->texflag), 0, 0, 0, 0, "Inverse texture operation");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "RGBtoInt",	1014,114,69,18, &(mtex->texflag), 0, 0, 0, 0, "Use RGB values for intensity texure");
	uiBlockEndAlign(block);
	
	uiDefButF(block, COL, B_MTEXCOL, "",				920,100,163,12, &(mtex->r), 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "R ",			920,80,163,18, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "The amount of red that blends with the intensity colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "G ",			920,60,163,18, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "The amount of green that blends with the intensity colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "B ",			920,40,163,18, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "The amount of blue that blends with the intensity colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		920,10,163,18, &(mtex->def_var), 0.0, 1.0, 0, 0, "The value that an intensity texture blends with the current value");
	uiBlockEndAlign(block);
	
	/* MAP TO */
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Blend",		1087,166,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour progression in the sky");
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Hori",		1172,166,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour of the horizon");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "ZenUp",		1087,147,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour of the zenith above");
	uiDefButS(block, TOG|BIT|3, B_MATPRV, "ZenDo",		1172,147,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture work on the colour of the zenith below");
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Blend",			1087,114,48,18, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "The texture blends the values");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,114,44,18, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "The texture multiplies the values");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,114,41,18, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "The texture adds the values");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,114,40,18, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "The texture subtracts the values");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,50,179,18, &(mtex->colfac), 0.0, 1.0, 0, 0, "Specify the extent to which the texture works on colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,30,179,18, &(mtex->norfac), 0.0, 1.0, 0, 0, "Specify the extent to which the texture works on the normal");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,10,179,18, &(mtex->varfac), 0.0, 1.0, 0, 0, "Specify the extent to which the texture works on a value");

}

static void world_panel_texture(World *wrld)
{
	uiBlock *block;
	MTex *mtex;
	ID *id;
	int a, loos;
	char str[64], *strp;
	
	block= uiNewBlock(&curarea->uiblocks, "world_panel_texture", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Texture and Input", "World", 960, 0, 318, 204)==0) return;

	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");

	/* TEX CHANNELS */
	uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiBlockBeginAlign(block);
	for(a= 0; a<6; a++) {
		mtex= wrld->mtex[a];
		if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[10]= 0;
		uiDefButS(block, ROW, REDRAWBUTSSHADING, str,10, 140-20*a, 80, 20, &(wrld->texact), 3.0, (float)a, 0, 0, "Texture channel");
	}
	uiBlockEndAlign(block);

	mtex= wrld->mtex[ wrld->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
		mtex->texco= TEXCO_VIEW;
	}
	
	/* TEXTUREBLOCK SELECT */
	uiBlockSetCol(block, TH_BUT_SETTING2);
	id= (ID *)mtex->tex;
	IDnames_to_pupstring(&strp, NULL, "ADD NEW %x 32767", &(G.main->tex), id, &(G.buts->texnr));
	uiDefButS(block, MENU, B_WTEXBROWSE, strp, 100,140,20,19, &(G.buts->texnr), 0, 0, 0, 0, "Browse");
	MEM_freeN(strp);
	
	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	100,160,163,19, id->name+2, 0.0, 18.0, 0, 0, "Specify the texture name");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,				196,140,21,19, 0, 0, 0, 0, 0, "Number of users");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 241,140,21,19, 0, 0, 0, 0, 0, "Auto assign name to texture");
		if(id->lib) {
			if(wrld->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	1019,146,21,19, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	219,140,21,19, 0, 0, 0, 0, 0, "");	
		}
		uiBlockSetCol(block, TH_AUTO);
		uiDefBut(block, BUT, B_TEXCLEARWORLD, "Clear", 122, 140, 72, 19, 0, 0, 0, 0, 0, "Erase link to texture");
	}
	else 
		uiDefButS(block, TOG, B_WTEXBROWSE, "Add New" ,100, 160, 163, 19, &(G.buts->texnr), -1.0, 32767.0, 0, 0, "Add new data block");

	uiBlockSetCol(block, TH_AUTO);
	

	/* TEXCO */
	uiDefButS(block, ROW, B_MATPRV, "View",			100,110,50,19, &(mtex->texco), 4.0, (float)TEXCO_VIEW, 0, 0, "Pass camera view vector on to the texture");
	uiDefButS(block, ROW, B_MATPRV, "Object",		150,110,50,19, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "The name of the object used as a source for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "", 100,90,100,19, &(mtex->object), "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "dX",		100,50,100,18, mtex->ofs, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dY",		100,30,100,18, mtex->ofs+1, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dZ",		100,10,100,18, mtex->ofs+2, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	200,50,100,18, mtex->size, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	200,30,100,18, mtex->size+1, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	200,10,100,18, mtex->size+2, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");	
	uiBlockEndAlign(block);
}

static void world_panel_mistaph(World *wrld)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "world_panel_mistaph", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mist Stars Physics", "World", 640, 0, 318, 204)==0) return;

	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");

	uiDefBut(block, MENU|SHO, 1, "Physics %t|None %x1|Sumo %x2|ODE %x3 |Dynamo %x4|",	
			10,180,140,19, &wrld->pad1, 0, 0, 0, 0, "Physics Engine");
	
	/* Gravitation for the game worlds */
	uiDefButF(block, NUMSLI,0, "Grav ", 150,180,150,19,	&(wrld->gravity), 0.0, 25.0, 0, 0,  "Gravitation constant of the game world.");


	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|0,REDRAWVIEW3D,"Mist",	10,120,140,19, &wrld->mode, 0, 0, 0, 0, "Enable mist");
	uiBlockSetCol(block, TH_AUTO);

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_DIFF, "Qua", 10, 90, 40, 19, &wrld->mistype, 1.0, 0.0, 0, 0, "Use quadratic progression");
	uiDefButS(block, ROW, B_DIFF, "Lin", 50, 90, 50, 19, &wrld->mistype, 1.0, 1.0, 0, 0, "Use linear progression");
	uiDefButS(block, ROW, B_DIFF, "Sqr", 100, 90, 50, 19, &wrld->mistype, 1.0, 2.0, 0, 0, "Use inverse quadratic progression");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,REDRAWVIEW3D, "Sta:",10,70,140,19, &wrld->miststa, 0.0, 1000.0, 10, 0, "Specify the starting distance of the mist");
	uiDefButF(block, NUM,REDRAWVIEW3D, "Di:",10,50,140,19, &wrld->mistdist, 0.0,1000.0, 10, 00, "Specify the depth of the mist");
	uiDefButF(block, NUM,B_DIFF,"Hi:",		10,30,140,19, &wrld->misthi,0.0,100.0, 10, 0, "Specify the factor for a less dense mist with increasing height");
	uiDefButF(block, NUMSLI, 0, "Misi",		10,10,140,19,	&(wrld->misi), 0., 1.0, 0, 0, "Set the mist intensity");
	uiBlockEndAlign(block);

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|1,B_DIFF,	"Stars",160,120,140,19, &wrld->mode, 0, 0, 0, 0, "Enable stars");
	uiBlockSetCol(block, TH_AUTO);
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,B_DIFF,"StarDist:",	160,70,140,19, &(wrld->stardist), 2.0, 1000.0, 100, 0, "Specify the average distance between two stars");
	uiDefButF(block, NUM,B_DIFF,"MinDist:",		160,50,140,19, &(wrld->starmindist), 0.0, 1000.0, 100, 0, "Specify the minimum distance to the camera");
	uiDefButF(block, NUMSLI,B_DIFF,"Size:",		160,30,140,19, &(wrld->starsize), 0.0, 10.0, 10, 0, "Specify the average screen dimension");
	uiDefButF(block, NUMSLI,B_DIFF,"Colnoise:",	160,10,140,19, &(wrld->starcolnoise), 0.0, 1.0, 100, 0, "Randomize starcolour");
	uiBlockEndAlign(block);

}

static void world_panel_world(World *wrld)
{
	uiBlock *block;
	ID *id, *idfrom;
	short xco;
	
	block= uiNewBlock(&curarea->uiblocks, "world_panel_world", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "World", "World", 320, 0, 318, 204)==0) return;

	/* first do the browse but */
	buttons_active_id(&id, &idfrom);

	uiBlockSetCol(block, TH_BUT_SETTING2);
	xco= std_libbuttons(block, 10, 180, 0, NULL, B_WORLDBROWSE, id, idfrom, &(G.buts->menunr), B_WORLDALONE, B_WORLDLOCAL, B_WORLDDELETE, 0, B_KEEPDATA);

	if(wrld==NULL) return;
	
	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");
	uiBlockSetCol(block, TH_AUTO);

	uiDefButF(block, COL, B_COLHOR, "",			10,150,145,19, &wrld->horr, 0, 0, 0, 0, "");
	uiDefButF(block, COL, B_COLZEN, "",			160,150,145,19, &wrld->zenr, 0, 0, 0, 0, "");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"HoR ",	10,130,145,19,	&(wrld->horr), 0.0, 1.0, B_COLHOR,0, "The amount of red of the horizon colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"HoG ",	10,110,145,19,	&(wrld->horg), 0.0, 1.0, B_COLHOR,0, "The amount of green of the horizon colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"HoB ",	10,90,145,19,	&(wrld->horb), 0.0, 1.0, B_COLHOR,0, "The amount of blue of the horizon colour");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeR ",	160,130,145,19,	&(wrld->zenr), 0.0, 1.0, B_COLZEN,0, "The amount of red of the zenith colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeG ",	160,110,145,19,	&(wrld->zeng), 0.0, 1.0, B_COLZEN,0, "The amount of green of the zenith colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeB ",	160,90,145,19,	&(wrld->zenb), 0.0, 1.0, B_COLZEN,0, "The amount of blue of the zenith colour");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbR ",	10,50,145,19,	&(wrld->ambr), 0.0, 1.0 ,0,0, "The amount of red of the ambient colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbG ",	10,30,145,19,	&(wrld->ambg), 0.0, 1.0 ,0,0, "The amount of red of the ambient colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbB ",	10,10,145,19,	&(wrld->ambb), 0.0, 1.0 ,0,0, "The amount of red of the ambient colour");
	uiBlockEndAlign(block);

	uiDefButF(block, NUMSLI,0, "Expos ",		160,10,145,19,	&(wrld->exposure), 0.2, 5.0, 0, 0, "Set the lighting time, exposure");


}

static void world_panel_preview(World *wrld)
{
	uiBlock *block;
	
	/* name "Preview" is abused to detect previewrender offset panel */
	block= uiNewBlock(&curarea->uiblocks, "world_panel_preview", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Preview", "World", 0, 0, 318, 204)==0) return;
	
	if(wrld==NULL) return;

	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");

	uiBlockSetDrawExtraFunc(block, BIF_previewdraw);

	// label to force a boundbox for buttons not to be centered
	uiDefBut(block, LABEL, 0, " ",	20,20,10,10, 0, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|1,B_MATPRV,"Real",	200,175,80,25, &wrld->skytype, 0, 0, 0, 0, "Render background with real horizon");
	uiDefButS(block, TOG|BIT|0,B_MATPRV,"Blend",200,150,80,25, &wrld->skytype, 0, 0, 0, 0, "Render background with natural progression");
	uiDefButS(block, TOG|BIT|2,B_MATPRV,"Paper",200,125,80,25, &wrld->skytype, 0, 0, 0, 0, "Flatten blend or texture coordinates");
	uiBlockEndAlign(block);
}



/* ************************ LAMP *************************** */

void do_lampbuts(unsigned short event)
{
	Lamp *la;
	MTex *mtex;
		
	switch(event) {
	case B_LAMPREDRAW:
		BIF_preview_changed(G.buts);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_TEXCLEARLAMP:
		la= G.buts->lockpoin;
		mtex= la->mtex[ la->texact ];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
			la->mtex[ la->texact ]= 0;
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(G.buts);
		}
		break;
      case B_SBUFF: 
		{ 
			la= G.buts->lockpoin; 
			la->bufsize = la->bufsize&=(~15); 
			allqueue(REDRAWBUTSSHADING, 0); 
			allqueue(REDRAWOOPS, 0); 
			/*la->bufsize = la->bufsize % 64;*/ 
		} 
		break; 
	}
	
	if(event) freefastshade();
}


static void lamp_panel_mapto(Object *ob, Lamp *la)
{
	uiBlock *block;
	MTex *mtex;
	
	block= uiNewBlock(&curarea->uiblocks, "lamp_panel_mapto", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Texture and Input", "Lamp");
	if(uiNewPanel(curarea, block, "Map To", "Lamp", 1280, 0, 318, 204)==0) return;

	uiSetButLock(la->id.lib!=0, "Can't edit library data");

	mtex= la->mtex[ la->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
		mtex->texco= TEXCO_VIEW;
	}

	/* TEXTURE OUTPUT */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	920,114,52,18, &(mtex->texflag), 0, 0, 0, 0, "Set the mapping to stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		974,114,38,18, &(mtex->texflag), 0, 0, 0, 0, "Apply the inverse of the texture");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "RGBtoInt",	1014,114,69,18, &(mtex->texflag), 0, 0, 0, 0, "Use an RGB texture as an intensity texture");
	uiBlockEndAlign(block);
	
	uiDefButF(block, COL, B_MTEXCOL, "",				920,100,163,12, &(mtex->r), 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "R ",			920,80,163,18, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "Set the red component of the intensity texture to blend with");
	uiDefButF(block, NUMSLI, B_MATPRV, "G ",			920,60,163,18, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "Set the green component of the intensity texture to blend with");
	uiDefButF(block, NUMSLI, B_MATPRV, "B ",			920,40,163,18, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "Set the blue component of the intensity texture to blend with");
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		920,10,163,18, &(mtex->def_var), 0.0, 1.0, 0, 0, "Set the value the texture blends with");
	uiBlockEndAlign(block);
	
	/* MAP TO */
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Col",		1107,166,81,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the colour of the lamp");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Blend",			1087,114,48,18, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "Mix the values");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,114,44,18, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "Multiply the values");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,114,41,18, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "Add the values");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,114,40,18, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "Subtract the values");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,50,179,18, &(mtex->colfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects the colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,30,179,18, &(mtex->norfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects the normal");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,10,179,18, &(mtex->varfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects the value");
	uiBlockEndAlign(block);
}


static void lamp_panel_texture(Object *ob, Lamp *la)
{
	uiBlock *block;
	MTex *mtex;
	ID *id;
	int a, loos;
	char *strp, str[64];
	
	block= uiNewBlock(&curarea->uiblocks, "lamp_panel_texture", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Texture and Input", "Lamp", 960, 0, 318, 204)==0) return;

	uiSetButLock(la->id.lib!=0, "Can't edit library data");

	/* TEX CHANNELS */
	uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiBlockBeginAlign(block);
	for(a= 0; a<6; a++) {
		mtex= la->mtex[a];
		if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[10]= 0;
		uiDefButS(block, ROW, B_REDR, str,	10, 140-20*a, 80, 20, &(la->texact), 3.0, (float)a, 0, 0, "");
	}
	uiBlockEndAlign(block);
	
	mtex= la->mtex[ la->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
		mtex->texco= TEXCO_VIEW;
	}

	/* TEXTUREBLOK SELECT */
	uiBlockSetCol(block, TH_BUT_SETTING2);
	id= (ID *)mtex->tex;
	IDnames_to_pupstring(&strp, NULL, "ADD NEW %x 32767", &(G.main->tex), id, &(G.buts->texnr));
	
	/* doesnt work, because lockpoin points to lamp, not to texture */
	uiDefButS(block, MENU, B_LTEXBROWSE, strp, 100,140,20,19, &(G.buts->texnr), 0, 0, 0, 0, "Select an existing texture, or create new");	
	MEM_freeN(strp);
	
	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	100,160,163,19, id->name+2, 0.0, 18.0, 0, 0, "Name of the texture block");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,			196,140,21,19, 0, 0, 0, 0, 0, "Select an existing texture, or create new");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 241,140,21,19, 0, 0, 0, 0, 0, "Auto assign a name to the texture");
		if(id->lib) {
			if(la->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	219,140,21,19, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	219,140,21,19, 0, 0, 0, 0, 0, "");	
		}
		uiBlockSetCol(block, TH_AUTO);
		uiDefBut(block, BUT, B_TEXCLEARLAMP, "Clear", 122, 140, 72, 19, 0, 0, 0, 0, 0, "Erase link to texture");
	}
	else 
		uiDefButS(block, TOG, B_LTEXBROWSE, "Add New" ,100, 160, 163, 19, &(G.buts->texnr), -1.0, 32767.0, 0, 0, "Add new data block");

	/* TEXCO */
	uiBlockSetCol(block, TH_AUTO);
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Glob",			100,110,60,20, &(mtex->texco), 4.0, (float)TEXCO_GLOB, 0, 0, "Generate texture coordinates from global coordinates");
	uiDefButS(block, ROW, B_MATPRV, "View",			160,110,70,20, &(mtex->texco), 4.0, (float)TEXCO_VIEW, 0, 0, "Generate texture coordinates from view coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Object",		230,110,70,20, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "Use linked object's coordinates for texture coordinates");
	uiBlockEndAlign(block);
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "", 100,90,200,20, &(mtex->object), "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "dX",		100,50,100,18, mtex->ofs, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dY",		100,30,100,18, mtex->ofs+1, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dZ",		100,10,100,18, mtex->ofs+2, -20.0, 20.0, 10, 0, "Set the extra translation of the texture coordinate");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	200,50,100,18, mtex->size, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	200,30,100,18, mtex->size+1, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	200,10,100,18, mtex->size+2, -10.0, 10.0, 10, 0, "Set the extra scaling of the texture coordinate");
	uiBlockEndAlign(block);
}

static void lamp_panel_spot(Object *ob, Lamp *la)
{
	uiBlock *block;
	float grid=0.0;
	
	block= uiNewBlock(&curarea->uiblocks, "lamp_panel_spot", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Spot", "Lamp", 640, 0, 318, 204)==0) return;

	if(G.vd) grid= G.vd->grid; 
	if(grid<1.0) grid= 1.0;

	uiSetButLock(la->id.lib!=0, "Can't edit library data");

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|0, REDRAWVIEW3D, "Shadows",10,150,80,19,&la->mode, 0, 0, 0, 0, "Let lamp produce shadows");
	uiDefButS(block, TOG|BIT|5, 0,"OnlyShadow",			10,130,80,19,&la->mode, 0, 0, 0, 0, "Render shadow only");
	uiDefButS(block, TOG|BIT|7, B_LAMPREDRAW,"Square",	10,90,80,19,&la->mode, 0, 0, 0, 0, "Use square spotbundles");
 	uiDefButS(block, TOG|BIT|1, 0,"Halo",				10,50,80,19,&la->mode, 0, 0, 0, 0, "Render spotlights with a volumetric halo"); 

	uiBlockSetCol(block, TH_AUTO);
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_LAMPREDRAW,"SpotSi ",	100,180,200,19,&la->spotsize, 1.0, 180.0, 0, 0, "Set the angle of the spot beam in degrees");
	uiDefButF(block, NUMSLI,B_MATPRV,"SpotBl ",		100,160,200,19,&la->spotblend, 0.0, 1.0, 0, 0, "Set the softness of the spot edge");
	uiBlockEndAlign(block);

	uiDefButF(block, NUMSLI,0,"HaloInt ",			100,135,200,19,&la->haint, 0.0, 5.0, 0, 0, "Set the intensity of the spot halo");

	uiDefButS(block, NUM,B_SBUFF,"ShadowBufferSize:", 100,110,200,19,	&la->bufsize,512,5120, 0, 0, "Set the size of the shadow buffer");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,REDRAWVIEW3D,"ClipSta:",	100,70,100,19,	&la->clipsta, 0.1*grid,1000.0*grid, 10, 0, "Set the shadow map clip start");
	uiDefButF(block, NUM,REDRAWVIEW3D,"ClipEnd:",	200,70,100,19,&la->clipend, 1.0, 5000.0*grid, 100, 0, "Set the shadow map clip end");
	uiBlockEndAlign(block);
	
	uiDefButS(block, NUM,0,"Samples:",		100,30,100,19,	&la->samp,1.0,16.0, 0, 0, "Number of shadow map samples");
	uiDefButS(block, NUM,0,"Halo step:",	200,30,100,19,	&la->shadhalostep, 0.0, 12.0, 0, 0, "Volumetric halo sampling frequency");
	uiDefButF(block, NUM,0,"Bias:",			100,10,100,19,	&la->bias, 0.01, 5.0, 1, 0, "Shadow map sampling bias");
	uiDefButF(block, NUM,0,"Soft:",			200,10,100,19,	&la->soft,1.0,100.0, 100, 0, "Set the size of the shadow sample area");
	
	
}


static void lamp_panel_lamp(Object *ob, Lamp *la)
{
	uiBlock *block;
	ID *id, *idfrom;
	float grid= 0.0;
	short xco;
	
	block= uiNewBlock(&curarea->uiblocks, "lamp_panel_lamp", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Lamp", "Lamp", 320, 0, 318, 204)==0) return;

	if(G.vd) grid= G.vd->grid; 
	if(grid<1.0) grid= 1.0;

	uiSetButLock(la->id.lib!=0, "Can't edit library data");

	/* first do the browse but */
	buttons_active_id(&id, &idfrom);

	uiBlockSetCol(block, TH_BUT_SETTING2);
	xco= std_libbuttons(block, 8, 180, 0, NULL, B_LAMPBROWSE, id, (ID *)ob, &(G.buts->menunr), B_LAMPALONE, B_LAMPLOCAL, 0, 0, 0);	

	uiBlockSetCol(block, TH_AUTO);
	uiDefButF(block, NUM,B_LAMPREDRAW,"Dist:", xco+10,180,100,20,&la->dist, 0.01, 5000.0, 100, 0, "Set the distance value");

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|3, B_MATPRV,"Quad",		10,150,100,19,&la->mode, 0, 0, 0, 0, "Use inverse quadratic proportion");
	uiDefButS(block, TOG|BIT|6, REDRAWVIEW3D,"Sphere",	10,130,100,19,&la->mode, 0, 0, 0, 0, "Lamp only shines inside a sphere");
	uiDefButS(block, TOG|BIT|2, 0,"Layer",				10,90,100,19,&la->mode, 0, 0, 0, 0, "Illuminate objects in the same layer only");
	uiDefButS(block, TOG|BIT|4, B_MATPRV,"Negative",	10,70,100,19,&la->mode, 0, 0, 0, 0, "Cast negative light");
	uiDefButS(block, TOG|BIT|11, 0,"No Diffuse",		10,30,100,19,&la->mode, 0, 0, 0, 0, "No diffuse shading of material");
	uiDefButS(block, TOG|BIT|12, 0,"No Specular",		10,10,100,19,&la->mode, 0, 0, 0, 0, "No specular shading of material");


	uiBlockSetCol(block, TH_AUTO);
	uiDefButF(block, NUMSLI,B_MATPRV,"Energy ",	120,150,180,20, &(la->energy), 0.0, 10.0, 0, 0, "Set the intensity of the light");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"R ",		120,120,180,20,&la->r, 0.0, 1.0, B_COLLAMP, 0, "Set the red component of the light");
	uiDefButF(block, NUMSLI,B_MATPRV,"G ",		120,100,180,20,&la->g, 0.0, 1.0, B_COLLAMP, 0, "Set the green component of the light");
	uiDefButF(block, NUMSLI,B_MATPRV,"B ",		120,80,180,20,&la->b, 0.0, 1.0, B_COLLAMP, 0, "Set the blue component of the light");
	uiBlockEndAlign(block);
	
	uiDefButF(block, COL, B_COLLAMP, "",		120,52,180,24, &la->r, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"Quad1 ",	120,30,180,19,&la->att1, 0.0, 1.0, 0, 0, "Set the light intensity value 1 for a quad lamp");
	uiDefButF(block, NUMSLI,B_MATPRV,"Quad2 ",  120,10,180,19,&la->att2, 0.0, 1.0, 0, 0, "Set the light intensity value 2 for a quad lamp");
	uiBlockEndAlign(block);
}


static void lamp_panel_preview(Object *ob, Lamp *la)
{
	uiBlock *block;
	
	/* name "Preview" is abused to detect previewrender offset panel */
	block= uiNewBlock(&curarea->uiblocks, "lamp_panel_preview", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Preview", "Lamp", 0, 0, 318, 204)==0) return;
	
	uiSetButLock(la->id.lib!=0, "Can't edit library data");

	uiBlockSetDrawExtraFunc(block, BIF_previewdraw);

	// label to force a boundbox for buttons not to be centered
	uiDefBut(block, LABEL, 0, " ",	20,20,10,10, 0, 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_LAMPREDRAW,"Lamp",	200,175,80,25,&la->type,1.0,(float)LA_LOCAL, 0, 0, "Use a point light source");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Spot",	200,150,80,25,&la->type,1.0,(float)LA_SPOT, 0, 0, "Restrict lamp to conical space");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Sun",	200,125,80,25,&la->type,1.0,(float)LA_SUN, 0, 0, "Light shines from constant direction");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Hemi",	200,100,80,25,&la->type,1.0,(float)LA_HEMI, 0, 0, "Light shines as half a sphere");
	uiBlockEndAlign(block);
}


/* ****************** MATERIAL ***************** */

void do_matbuts(unsigned short event)
{
	static short mtexcopied=0;
	Material *ma;
	MTex *mtex;

	switch(event) {		
	case B_ACTCOL:
		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWIPO, 0);
		BIF_preview_changed(G.buts);
		break;
	case B_MATFROM:
		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWBUTSSHADING, 0);
		// BIF_previewdraw();  push/pop!
		break;
	case B_MATPRV:
		/* this event also used by lamp, tex and sky */
		BIF_preview_changed(G.buts);
		break;
	case B_MATPRV_DRAW:
		BIF_preview_changed(G.buts);
		allqueue(REDRAWBUTSSHADING, 0);
		break;
	case B_TEXCLEAR:
		ma= G.buts->lockpoin;
		mtex= ma->mtex[(int) ma->texact ];
		if(mtex) {
			if(mtex->tex) mtex->tex->id.us--;
			MEM_freeN(mtex);
			ma->mtex[ (int) ma->texact ]= 0;
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	case B_MTEXCOPY:
		ma= G.buts->lockpoin;
		if(ma && ma->mtex[(int)ma->texact] ) {
			mtex= ma->mtex[(int)ma->texact];
			if(mtex->tex==0) {
				error("No texture available");
			}
			else {
				memcpy(&mtexcopybuf, ma->mtex[(int)ma->texact], sizeof(MTex));
				mtexcopied= 1;
			}
		}
		break;
	case B_MTEXPASTE:
		ma= G.buts->lockpoin;
		if(ma && mtexcopied && mtexcopybuf.tex) {
			if(ma->mtex[(int)ma->texact]==0 ) ma->mtex[(int)ma->texact]= MEM_mallocN(sizeof(MTex), "mtex"); 
			memcpy(ma->mtex[(int)ma->texact], &mtexcopybuf, sizeof(MTex));
			
			id_us_plus((ID *)mtexcopybuf.tex);
			BIF_preview_changed(G.buts);
			scrarea_queue_winredraw(curarea);
		}
		break;
	case B_MATLAY:
		ma= G.buts->lockpoin;
		if(ma && ma->lay==0) {
			ma->lay= 1;
			scrarea_queue_winredraw(curarea);
		}
	}
}

static void material_panel_map_to(Material *ma)
{
	uiBlock *block;
	MTex *mtex;
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_map_to", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Texture", "Material");
	if(uiNewPanel(curarea, block, "Map To", "Material", 1600, 0, 318, 204)==0) return;

	mtex= ma->mtex[ ma->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
	}

	/* TEXTURE OUTPUT */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	900,116,54,18, &(mtex->texflag), 0, 0, 0, 0, "Set the mapping to stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		956,116,39,18, &(mtex->texflag), 0, 0, 0, 0, "Reverse the effect of the texture");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "No RGB",		997,116,71,18, &(mtex->texflag), 0, 0, 0, 0, "Use an RGB texture as an intensity texture");
	uiBlockEndAlign(block);
	
	uiDefButF(block, COL, B_MTEXCOL, "",				900,100,168,12, &(mtex->r), 0, 0, 0, 0, "Browse datablocks");
	
	uiBlockBeginAlign(block);
	if(ma->colormodel==MA_HSV) {
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButF(block, HSVSLI, B_MATPRV, "H ",			900,80,168,18, &(mtex->r), 0.0, 0.9999, B_MTEXCOL, 0, "");
		uiDefButF(block, HSVSLI, B_MATPRV, "S ",			900,60,168,18, &(mtex->r), 0.0001, 1.0, B_MTEXCOL, 0, "");
		uiDefButF(block, HSVSLI, B_MATPRV, "V ",			900,40,168,18, &(mtex->r), 0.0001, 1.0, B_MTEXCOL, 0, "");
		uiBlockSetCol(block, TH_AUTO);
	}
	else {
		uiDefButF(block, NUMSLI, B_MATPRV, "R ",			900,80,168,18, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "Set the amount of red the intensity texture blends with");
		uiDefButF(block, NUMSLI, B_MATPRV, "G ",			900,60,168,18, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "Set the amount of green the intensity texture blends with");
		uiDefButF(block, NUMSLI, B_MATPRV, "B ",			900,40,168,18, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "Set the amount of blue the intensity texture blends with");
	}
	uiBlockEndAlign(block);
	
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		900,10,168,18, &(mtex->def_var), 0.0, 1.0, 0, 0, "Set the value the texture blends with the current value");
	
	/* MAP TO */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Col",	900,186,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect basic colour of the material");
	uiDefButS(block, TOG3|BIT|1, B_MATPRV, "Nor",	952,186,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the rendered normal");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Csp",	1004,186,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the specularity colour");
	uiDefButS(block, TOG|BIT|3, B_MATPRV, "Cmir",	1056,186,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affext the mirror colour");
	uiDefButS(block, TOG3|BIT|4, B_MATPRV, "Ref",	1108,186,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the value of the materials reflectivity");
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG3|BIT|5, B_MATPRV, "Spec",	900,166,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the value of specularity");
	uiDefButS(block, TOG3|BIT|8, B_MATPRV, "Hard",	952,166,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the hardness value");
	uiDefButS(block, TOG3|BIT|7, B_MATPRV, "Alpha",	1004,166,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the alpha value");
	uiDefButS(block, TOG3|BIT|6, B_MATPRV, "Emit",	1056,166,50,18, &(mtex->mapto), 0, 0, 0, 0, "Let the texture affect the emit value");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Mix",			1087,94,48,18, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "The texture blends the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,94,44,18, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "The texture multiplies the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,94,41,18, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "The texture adds the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,94,40,18, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "The texture subtracts the values or colour");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,50,179,18, &(mtex->colfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects colour");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,30,179,18, &(mtex->norfac), 0.0, 5.0, 0, 0, "Set the amount the texture affects the normal");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,10,179,18, &(mtex->varfac), 0.0, 1.0, 0, 0, "Set the amount the texture affects a value");
	uiBlockEndAlign(block);
}


static void material_panel_map_input(Material *ma)
{
	uiBlock *block;
	MTex *mtex;
	int b;
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_map_input", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Texture", "Material");
	if(uiNewPanel(curarea, block, "Map Input", "Material", 1280, 0, 318, 204)==0) return;

	mtex= ma->mtex[ ma->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
	}
	
	/* TEXCO */
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "UV",			630,166,40,18, &(mtex->texco), 4.0, (float)TEXCO_UV, 0, 0, "Use UV coordinates for texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Object",		670,166,75,18, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "Use linked object's coordinates for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "",745,166,163,18, &(mtex->object), "");
	
	uiDefButS(block, ROW, B_MATPRV, "Glob",			630,146,45,18, &(mtex->texco), 4.0, (float)TEXCO_GLOB, 0, 0, "Use global coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Orco",			675,146,50,18, &(mtex->texco), 4.0, (float)TEXCO_ORCO, 0, 0, "Use the original coordinates of the mesh");
	uiDefButS(block, ROW, B_MATPRV, "Stick",		725,146,50,18, &(mtex->texco), 4.0, (float)TEXCO_STICKY, 0, 0, "Use mesh sticky coordaintes for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Win",			775,146,45,18, &(mtex->texco), 4.0, (float)TEXCO_WINDOW, 0, 0, "Use screen coordinates as texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Nor",			820,146,44,18, &(mtex->texco), 4.0, (float)TEXCO_NORM, 0, 0, "Use normal vector as texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Refl",			864,146,44,18, &(mtex->texco), 4.0, (float)TEXCO_REFL, 0, 0, "Use reflection vector as texture coordinates");

	/* COORDS */
	uiBlockBeginAlign(block);
	uiDefButC(block, ROW, B_MATPRV, "Flat",			666,114,48,18, &(mtex->mapping), 5.0, (float)MTEX_FLAT, 0, 0, "Map X and Y coordinates directly");
	uiDefButC(block, ROW, B_MATPRV, "Cube",			717,114,50,18, &(mtex->mapping), 5.0, (float)MTEX_CUBE, 0, 0, "Map using the normal vector");
	uiDefButC(block, ROW, B_MATPRV, "Tube",			666,94,48,18, &(mtex->mapping), 5.0, (float)MTEX_TUBE, 0, 0, "Map with Z as central axis (tube-like)");
	uiDefButC(block, ROW, B_MATPRV, "Sphe",			716,94,50,18, &(mtex->mapping), 5.0, (float)MTEX_SPHERE, 0, 0, "Map with Z as central axis (sphere-like)");

	uiBlockBeginAlign(block);
	for(b=0; b<3; b++) {
		char *cp;
		if(b==0) cp= &(mtex->projx);
		else if(b==1) cp= &(mtex->projy);
		else cp= &(mtex->projz);
		
		uiDefButC(block, ROW, B_MATPRV, "",			665, 50-20*b, 24, 18, cp, 6.0+b, 0.0, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, "X",		691, 50-20*b, 24, 18, cp, 6.0+b, 1.0, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, "Y",		717, 50-20*b, 24, 18, cp, 6.0+b, 2.0, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, "Z",		743, 50-20*b, 24, 18, cp, 6.0+b, 3.0, 0, 0, "");
	}
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "ofsX",		778,114,130,18, mtex->ofs, -10.0, 10.0, 10, 0, "Fine tune X coordinate");
	uiDefButF(block, NUM, B_MATPRV, "ofsY",		778,94,130,18, mtex->ofs+1, -10.0, 10.0, 10, 0, "Fine tune Y coordinate");
	uiDefButF(block, NUM, B_MATPRV, "ofsZ",		778,74,130,18, mtex->ofs+2, -10.0, 10.0, 10, 0, "Fine tune Z coordinate");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	778,50,130,18, mtex->size, -100.0, 100.0, 10, 0, "Set an extra scaling for the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	778,30,130,18, mtex->size+1, -100.0, 100.0, 10, 0, "Set an extra scaling for the texture coordinate");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	778,10,130,18, mtex->size+2, -100.0, 100.0, 10, 0, "Set an extra scaling for the texture coordinate");
	uiBlockEndAlign(block);

}


static void material_panel_texture(Material *ma)
{
	uiBlock *block;
	MTex *mtex;
	ID *id;
	int loos;
	int a;
	char str[64], *strp;
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_texture", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Texture", "Material", 960, 0, 318, 204)==0) return;

	/* TEX CHANNELS */
	uiBlockSetCol(block, TH_BUT_NEUTRAL);
	
	uiBlockBeginAlign(block);
	for(a= 0; a<8; a++) {
		mtex= ma->mtex[a];
		if(mtex && mtex->tex) splitIDname(mtex->tex->id.name+2, str, &loos);
		else strcpy(str, "");
		str[10]= 0;
		uiDefButC(block, ROW, B_MATPRV_DRAW, str,	10, 180-22*a, 70, 20, &(ma->texact), 3.0, (float)a, 0, 0, "");
	}
	uiBlockEndAlign(block);
	
	/* SEPTEX */
	uiBlockSetCol(block, TH_AUTO);
	
	for(a= 0; a<8; a++) {
		mtex= ma->mtex[a];
		if(mtex && mtex->tex) {
			if(ma->septex & (1<<a)) 
				uiDefButC(block, TOG|BIT|a, B_MATPRV_DRAW, " ",	-20, 180-22*a, 28, 20, &ma->septex, 0.0, 0.0, 0, 0, "Disable or enable this channel");
			else uiDefIconButC(block, TOG|BIT|a, B_MATPRV_DRAW, ICON_CHECKBOX_HLT,	-20, 180-22*a, 28, 20, &ma->septex, 0.0, 0.0, 0, 0, "Disable or enable this channel");
		}
	}
	
	uiDefIconBut(block, BUT, B_MTEXCOPY, ICON_COPYUP,	100,180,23,21, 0, 0, 0, 0, 0, "Copy the mapping settings to the buffer");
	uiDefIconBut(block, BUT, B_MTEXPASTE, ICON_PASTEUP,	125,180,23,21, 0, 0, 0, 0, 0, "Paste the mapping settings from the buffer");

	uiBlockSetCol(block, TH_AUTO);
	
	mtex= ma->mtex[ ma->texact ];
	if(mtex==0) {
		mtex= &emptytex;
		default_mtex(mtex);
	}

	/* TEXTUREBLOK SELECT */
	uiBlockSetCol(block, TH_BUT_SETTING2);
	if(G.main->tex.first==0)
		id= NULL;
	else
		id= (ID*) mtex->tex;
	IDnames_to_pupstring(&strp, NULL, "ADD NEW %x32767", &(G.main->tex), id, &(G.buts->texnr));
	uiDefButS(block, MENU, B_EXTEXBROWSE, strp, 100,130,20,20, &(G.buts->texnr), 0, 0, 0, 0, "The name of the texture");
	MEM_freeN(strp);

	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	100,150,163,20, id->name+2, 0.0, 18.0, 0, 0, "The name of the texture block");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,				196,130,21,20, 0, 0, 0, 0, 0, "");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 241,130,21,20, 0, 0, 0, 0, 0, "Auto-assign name to texture");
		if(id->lib) {
			if(ma->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	219,130,21,20, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	219,130,21,20, 0, 0, 0, 0, 0, "");		
		}
		uiBlockSetCol(block, TH_AUTO);
		uiDefBut(block, BUT, B_TEXCLEAR, "Clear", 122, 130, 72, 20, 0, 0, 0, 0, 0, "Erase link to datablock");
	}
	else 
		uiDefButS(block, TOG, B_EXTEXBROWSE, "Add New" ,100, 150, 163, 20, &(G.buts->texnr), -1.0, 32767.0, 0, 0, "Add new data block");
	
	// force no centering
	uiDefBut(block, LABEL, 0, " ", 250, 10, 25, 20, 0, 0, 0, 0, 0, "");
	
	uiBlockSetCol(block, TH_AUTO);
}

static void material_panel_shading(Material *ma)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_shading", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Shaders", "Material", 640, 0, 318, 204)==0) return;

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButI(block, TOG|BIT|5, B_MATPRV_DRAW, "Halo",	245,180,65,18, &(ma->mode), 0, 0, 0, 0, "Render as a halo");
	uiBlockSetCol(block, TH_AUTO);

	if(ma->mode & MA_HALO) {
		uiDefButF(block, NUM, B_MATPRV, "HaloSize: ",		10,155,190,18, &(ma->hasize), 0.0, 100.0, 10, 0, "Set the dimension of the halo");
		uiDefButS(block, NUMSLI, B_MATPRV, "Hard ",			10,135,190,18, &(ma->har), 1.0, 127.0, 0, 0, "Set the hardness of the halo");
		uiDefButF(block, NUMSLI, B_MATPRV, "Add  ",			10,115,190,18, &(ma->add), 0.0, 1.0, 0, 0, "Strength of the add effect");
		
		uiDefButS(block, NUM, B_MATPRV, "Rings: ",			10,90,90,18, &(ma->ringc), 0.0, 24.0, 0, 0, "Set the number of rings rendered over the basic halo");
		uiDefButS(block, NUM, B_MATPRV, "Lines: ",			100,90,100,18, &(ma->linec), 0.0, 250.0, 0, 0, "Set the number of star shaped lines rendered over the halo");
		uiDefButS(block, NUM, B_MATPRV, "Star: ",			10,70,90,18, &(ma->starc), 3.0, 50.0, 0, 0, "Set the number of points on the star shaped halo");
		uiDefButC(block, NUM, B_MATPRV, "Seed: ",			100,70,100,18, &(ma->seed1), 0.0, 255.0, 0, 0, "Use random values for ring dimension and line location");
		if(ma->mode & MA_HALO_FLARE) {
			uiDefButF(block, NUM, B_MATPRV, "FlareSize: ",		10,50,95,18, &(ma->flaresize), 0.1, 25.0, 10, 0, "Set the factor the flare is larger than the halo");
			uiDefButF(block, NUM, B_MATPRV, "Sub Size: ",		100,50,100,18, &(ma->subsize), 0.1, 25.0, 10, 0, "Set the dimension of the subflares, dots and circles");
			uiDefButF(block, NUMSLI, B_MATPRV, "Boost: ",		10,30,190,18, &(ma->flareboost), 0.1, 10.0, 10, 0, "Give the flare extra strength");
			uiDefButC(block, NUM, B_MATPRV, "Fl.seed: ",		10,10,90,18, &(ma->seed2), 0.0, 255.0, 0, 0, "Specify an offset in the seed table");
			uiDefButS(block, NUM, B_MATPRV, "Flares: ",			100,10,100,18, &(ma->flarec), 1.0, 32.0, 0, 0, "Set the nuber of subflares");
		}
		uiBlockSetCol(block, TH_BUT_SETTING1);
		
		uiBlockBeginAlign(block);
		uiDefButI(block, TOG|BIT|15, B_MATPRV_DRAW, "Flare",		245,142,65,28, &(ma->mode), 0, 0, 0, 0, "Render halo as a lensflare");
		uiDefButI(block, TOG|BIT|8, B_MATPRV, "Rings",		245,123,65, 18, &(ma->mode), 0, 0, 0, 0, "Render rings over basic halo");
		uiDefButI(block, TOG|BIT|9, B_MATPRV, "Lines",		245,104,65, 18, &(ma->mode), 0, 0, 0, 0, "Render star shaped lines over the basic halo");
		uiDefButI(block, TOG|BIT|11, B_MATPRV, "Star",		245,85,65, 18, &(ma->mode), 0, 0, 0, 0, "Render halo as a star");
		uiDefButI(block, TOG|BIT|12, B_MATPRV, "HaloTex",	245,66,65, 18, &(ma->mode), 0, 0, 0, 0, "Give halo a texture");
		uiDefButI(block, TOG|BIT|13, B_MATPRV, "HaloPuno",	245,47,65, 18, &(ma->mode), 0, 0, 0, 0, "Use the vertex normal to specify the dimension of the halo");
		uiDefButI(block, TOG|BIT|10, B_MATPRV, "X Alpha",	245,28,65, 18, &(ma->mode), 0, 0, 0, 0, "Use extreme alpha");
		uiDefButI(block, TOG|BIT|14, B_MATPRV, "Shaded",	245,9,65, 18, &(ma->mode), 0, 0, 0, 0, "Let halo receive light");
		uiBlockEndAlign(block);
	}
	else {
		char *str1= "Diffuse Shader%t|Lambert %x0|Oren-Nayar %x1|Toon %x2";
		char *str2= "Specular Shader%t|CookTorr %x0|Phong %x1|Blinn %x2|Toon %x3";
		
		/* diff shader buttons */
		uiDefButS(block, MENU, B_MATPRV_DRAW, str1,		9, 155,78,19, &(ma->diff_shader), 0.0, 0.0, 0, 0, "Set a diffuse shader");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_MATPRV, "Ref   ",	90,155,150,19, &(ma->ref), 0.0, 1.0, 0, 0, "Set the amount of reflection");
		if(ma->diff_shader==MA_DIFF_ORENNAYAR)
			uiDefButF(block, NUMSLI, B_MATPRV, "Rough:",90,135, 150,19, &(ma->roughness), 0.0, 3.14, 0, 0, "Oren Nayar Roughness");
		else if(ma->diff_shader==MA_DIFF_TOON) {
			uiDefButF(block, NUMSLI, B_MATPRV, "Size:",	90, 135,150,19, &(ma->param[0]), 0.0, 3.14, 0, 0, "Size of diffuse toon area");
			uiDefButF(block, NUMSLI, B_MATPRV, "Smooth:",90,115,150,19, &(ma->param[1]), 0.0, 1.0, 0, 0, "Smoothness of diffuse toon area");
		}
		uiBlockEndAlign(block);
		
		/* spec shader buttons */
		uiDefButS(block, MENU, B_MATPRV_DRAW, str2,		9,95,77,19, &(ma->spec_shader), 0.0, 0.0, 0, 0, "Set a specular shader");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_MATPRV, "Spec ",		90,95,150,19, &(ma->spec), 0.0, 2.0, 0, 0, "Set the degree of specularity");
		if ELEM3(ma->spec_shader, MA_SPEC_COOKTORR, MA_SPEC_PHONG, MA_SPEC_BLINN) {
			uiDefButS(block, NUMSLI, B_MATPRV, "Hard:",	90, 75, 150,19, &(ma->har), 1.0, 255, 0, 0, "Set the hardness of the specularity");
		}
		if(ma->spec_shader==MA_SPEC_BLINN)
			uiDefButF(block, NUMSLI, B_MATPRV, "Refr:",	90, 55,150,19, &(ma->refrac), 1.0, 10.0, 0, 0, "Refraction index");
		if(ma->spec_shader==MA_SPEC_TOON) {
			uiDefButF(block, NUMSLI, B_MATPRV, "Size:",	90, 75,150,19, &(ma->param[2]), 0.0, 1.53, 0, 0, "Size of specular toon area");
			uiDefButF(block, NUMSLI, B_MATPRV, "Smooth:",90, 55,150,19, &(ma->param[3]), 0.0, 1.0, 0, 0, "Smoothness of specular toon area");
		}
		uiBlockEndAlign(block);
		
		/* default shading variables */
		uiDefButF(block, NUMSLI, B_MATPRV, "Amb ",		9,30,117,19, &(ma->amb), 0.0, 1.0, 0, 0, "Set the amount of global ambient color");
		uiDefButF(block, NUMSLI, B_MATPRV, "Emit ",		133,30,110,19, &(ma->emit), 0.0, 1.0, 0, 0, "Set the amount of emitting light");
		uiDefButF(block, NUMSLI, B_MATPRV, "Add ",		9,10,117,19, &(ma->add), 0.0, 1.0, 0, 0, "Glow factor for transparant");
		uiDefButF(block, NUM, 0, "Zoffs:",				133,10,110,19, &(ma->zoffs), 0.0, 10.0, 0, 0, "Give face an artificial offset");
	
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiBlockBeginAlign(block);
		uiDefButI(block, TOG|BIT|0, 0,	"Traceable",		245,161,65,18, &(ma->mode), 0, 0, 0, 0, "Make material visible for shadow lamps");
		uiDefButI(block, TOG|BIT|1, 0,	"Shadow",			245,142,65,18, &(ma->mode), 0, 0, 0, 0, "Enable material for shadows");
		uiDefButI(block, TOG|BIT|16, 0,	"Radio",			245,123,65,18, &(ma->mode), 0, 0, 0, 0, "Enable radiosty render");
		uiDefButI(block, TOG|BIT|3, 0,	"Wire",				245,104,65,18, &(ma->mode), 0, 0, 0, 0, "Render only the edges of faces");
		uiDefButI(block, TOG|BIT|6, 0,	"ZTransp",			245,85, 65,18, &(ma->mode), 0, 0, 0, 0, "Z-Buffer transparent faces");
		uiDefButI(block, TOG|BIT|9, 0,	"Env",				245,66, 65,18, &(ma->mode), 0, 0, 0, 0, "Do not render material");
		uiDefButI(block, TOG|BIT|10, 0,	"OnlyShadow",		245,47, 65,18, &(ma->mode), 0, 0, 0, 0, "Let alpha be determined on the degree of shadow");
		uiDefButI(block, TOG|BIT|14, 0,	"No Mist",			245,28, 65,18, &(ma->mode), 0, 0, 0, 0, "Set the material insensitive to mist");
		uiDefButI(block, TOG|BIT|8, 0,	"ZInvert",			245,9, 65,18, &(ma->mode), 0, 0, 0, 0, "Render with inverted Z Buffer");
		uiBlockEndAlign(block);
	}

}


static void material_panel_material(Object *ob, Material *ma)
{
	uiBlock *block;
	ID *id, *idn, *idfrom;
	uiBut *but;
	float *colpoin = NULL, min;
	int rgbsel = 0, xco= 0;
	char str[30];
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_material", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Material", "Material", 320, 0, 318, 204)==0) return;

	/* first do the browse but */
	buttons_active_id(&id, &idfrom);

	uiBlockSetCol(block, TH_BUT_SETTING2);
	xco= std_libbuttons(block, 8, 200, 0, NULL, B_MATBROWSE, id, idfrom, &(G.buts->menunr), B_MATALONE, B_MATLOCAL, B_MATDELETE, B_AUTOMATNAME, B_KEEPDATA);
	
	uiDefIconBut(block, BUT, B_MATCOPY, ICON_COPYUP,	263,200,XIC,YIC, 0, 0, 0, 0, 0, "Copies Material to the buffer");
	uiSetButLock(id && id->lib, "Can't edit library data");
	uiDefIconBut(block, BUT, B_MATPASTE, ICON_PASTEUP,	284,200,XIC,YIC, 0, 0, 0, 0, 0, "Pastes Material from the buffer");
	
	if(ob->actcol==0) ob->actcol= 1;	/* because of TOG|BIT button */
	
	/* indicate which one is linking a material */
	if( id == NULL ) return;

	uiBlockBeginAlign(block);

	uiSetButLock(id->lib!=0, "Can't edit library data");
	
	strncpy(str, id->name, 2);
	str[2]= ':'; str[3]= 0;
	but= uiDefBut(block, TEX, B_IDNAME, str,		8,174,115,20, id->name+2, 0.0, 18.0, 0, 0, "Show the block the material is linked to");
	uiButSetFunc(but, test_idbutton_cb, id->name, NULL);

	uiBlockSetCol(block, TH_BUT_ACTION);
	uiDefButS(block, TOG|BIT|(ob->actcol-1), B_MATFROM, "OB",	125,174,32,20, &ob->colbits, 0, 0, 0, 0, "Link material to object");
	idn= ob->data;
	strncpy(str, idn->name, 2);
	str[2]= 0;
	uiBlockSetCol(block, TH_BUT_SETTING);
	uiDefButS(block, TOGN|BIT|(ob->actcol-1), B_MATFROM, str,	158,174,32,20, &ob->colbits, 0, 0, 0, 0, "Show the block the material is linked to");
	uiBlockSetCol(block, TH_AUTO);
	
	/* id is the block from which the material is used */
	if( BTST(ob->colbits, ob->actcol-1) ) id= (ID *)ob;
	else id= ob->data;

	sprintf(str, "%d Mat", ob->totcol);
	if(ob->totcol) min= 1.0; else min= 0.0;
	uiDefButC(block, NUM, B_ACTCOL, str,			191,174,114,20, &(ob->actcol), min, (float)ob->totcol, 0, 0, "Number of materials on object / Active material");
	uiBlockEndAlign(block);
	
	if(ob->totcol==0) return;

	ma= give_current_material(ob, ob->actcol);	
	if(ma==0) return;	
	
	if(ma->dynamode & MA_DRAW_DYNABUTS) {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, 0, "Restitut ",		128,120,175,20, &ma->reflect, 0.0, 1.0, 0, 0, "Elasticity of collisions");
		uiDefButF(block, NUMSLI, 0, "Friction ",  		128,98 ,175,20, &ma->friction, 0.0, 100.0, 0, 0,   "Coulomb friction coefficient");
		uiDefButF(block, NUMSLI, 0, "Fh Force ",		128,76 ,175,20, &ma->fh, 0.0, 1.0, 0, 0, "Upward spring force within the Fh area");
		uiBlockEndAlign(block);
		uiDefButF(block, NUM, 0,	 "Fh Damp ",		8,120,100,20, &ma->xyfrict, 0.0, 1.0, 10, 0, "Damping of the Fh spring force");
		uiDefButF(block, NUM, 0, "Fh Dist ",			8,98 ,100,20, &ma->fhdist, 0.0, 20.0, 10, 0, "Height of the Fh area");
		uiDefButS(block, TOG|BIT|1, 0, "Fh Norm",		8,76 ,100,20, &ma->dynamode, 0.0, 0.0, 0, 0, "Add a horizontal spring force on slopes");
	}
	else {
		if(!(ma->mode & MA_HALO)) {
			uiBlockBeginAlign(block);
			uiBlockSetCol(block, TH_BUT_SETTING1);
			uiDefButI(block, TOG|BIT|4, B_REDR,	"VCol Light",	8,146,75,20, &(ma->mode), 0, 0, 0, 0, "Add vertex colours as extra light");
			uiDefButI(block, TOG|BIT|7, B_REDR, "VCol Paint",	85,146,72,20, &(ma->mode), 0, 0, 0, 0, "Replace basic colours with vertex colours");
			uiDefButI(block, TOG|BIT|11, B_REDR, "TexFace",		160,146,62,20, &(ma->mode), 0, 0, 0, 0, "UV-Editor assigned texture gives color and texture info for the faces");
			uiDefButI(block, TOG|BIT|2, B_MATPRV, "Shadeless",	223,146,80,20, &(ma->mode), 0, 0, 0, 0, "Make material insensitive to light or shadow");
			uiBlockEndAlign(block);
		}
		uiBlockSetCol(block, TH_AUTO);
		uiDefButF(block, COL, B_MATCOL, "",		8,115,72,24, &(ma->r), 0, 0, 0, 0, "");
		uiDefButF(block, COL, B_SPECCOL, "",	8,88,72,24, &(ma->specr), 0, 0, 0, 0, "");
		uiDefButF(block, COL, B_MIRCOL, "",		8,61,72,24, &(ma->mirr), 0, 0, 0, 0, "");
	
		uiBlockBeginAlign(block);
		if(ma->mode & MA_HALO) {
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Halo",		83,115,40,25, &(ma->rgbsel), 2.0, 0.0, 0, 0, "Mix the colour of the halo with the RGB sliders");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Line",		83,88,40,25, &(ma->rgbsel), 2.0, 1.0, 0, 0, "Mix the colour of the lines with the RGB sliders");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Ring",		83,61,40,25, &(ma->rgbsel), 2.0, 2.0, 0, 0, "Mix the colour of the rings with the RGB sliders");
		}
		else {
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Col",			83,115,40,25, &(ma->rgbsel), 2.0, 0.0, 0, 0, "Set the basic colour of the material");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Spe",			83,88,40,25, &(ma->rgbsel), 2.0, 1.0, 0, 0, "Set the colour of the specularity");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Mir",			83,61,40,25, &(ma->rgbsel), 2.0, 2.0, 0, 0, "Use mirror colour");
		}
		
		if(ma->rgbsel==0) {colpoin= &(ma->r); rgbsel= B_MATCOL;}
		else if(ma->rgbsel==1) {colpoin= &(ma->specr); rgbsel= B_SPECCOL;}
		else if(ma->rgbsel==2) {colpoin= &(ma->mirr); rgbsel= B_MIRCOL;}
		
		if(ma->rgbsel==0 && (ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE) && !(ma->mode & MA_HALO)));
		else if(ma->colormodel==MA_HSV) {
			uiBlockSetCol(block, TH_BUT_SETTING1);
			uiBlockBeginAlign(block);
			uiDefButF(block, HSVSLI, B_MATPRV, "H ",		128,120,175,19, colpoin, 0.0, 0.9999, rgbsel, 0, "");
			uiDefButF(block, HSVSLI, B_MATPRV, "S ",		128,100,175,19, colpoin, 0.0001, 1.0, rgbsel, 0, "");
			uiDefButF(block, HSVSLI, B_MATPRV, "V ",		128,80,175,19, colpoin, 0.0001, 1.0, rgbsel, 0, "");
			uiBlockSetCol(block, TH_AUTO);
		}
		else {
			uiBlockBeginAlign(block);
			uiDefButF(block, NUMSLI, B_MATPRV, "R ",		128,120,175,19, colpoin, 0.0, 1.0, rgbsel, 0, "");
			uiDefButF(block, NUMSLI, B_MATPRV, "G ",		128,100,175,19, colpoin+1, 0.0, 1.0, rgbsel, 0, "");
			uiDefButF(block, NUMSLI, B_MATPRV, "B ",		128,80,175,19, colpoin+2, 0.0, 1.0, rgbsel, 0, "");
		}
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_MATPRV, "Alpha ",		128,52,175,19, &(ma->alpha), 0.0, 1.0, 0, 0, "Set the amount of coverage, to make materials transparent");
		uiDefButF(block, NUMSLI, B_MATPRV, "SpecTra ",		128,32,175,19, &(ma->spectra), 0.0, 1.0, 0, 0, "Make specular areas opaque");
		
	}
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, REDRAWBUTSSHADING, "RGB",			8,32,35,20, &(ma->colormodel), 1.0, (float)MA_RGB, 0, 0, "Create colour by red, green and blue");
	uiDefButS(block, ROW, REDRAWBUTSSHADING, "HSV",			43,32,35,20, &(ma->colormodel), 1.0, (float)MA_HSV, 0, 0, "Mix colour with hue, saturation and value");
	uiDefButS(block, TOG|BIT|0, REDRAWBUTSSHADING, "DYN",	78,32,45,20, &(ma->dynamode), 0.0, 0.0, 0, 0, "Adjust parameters for dynamics options");
	uiBlockEndAlign(block);
}

static void material_panel_preview(Material *ma)
{
	uiBlock *block;
	
	/* name "Preview" is abused to detect previewrender offset panel */
	block= uiNewBlock(&curarea->uiblocks, "material_panel_preview", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Preview", "Material", 0, 0, 318, 204)==0) return;
	
	if(ma) {
		uiBlockSetDrawExtraFunc(block, BIF_previewdraw);
	
		// label to force a boundbox for buttons not to be centered
		uiDefBut(block, LABEL, 0, " ",	20,20,10,10, 0, 0, 0, 0, 0, "");
		uiBlockSetCol(block, TH_BUT_NEUTRAL);
		uiDefIconButC(block, ROW, B_MATPRV, ICON_MATPLANE,		210,180,25,22, &(ma->pr_type), 10, 0, 0, 0, "");
		uiDefIconButC(block, ROW, B_MATPRV, ICON_MATSPHERE,		210,150,25,22, &(ma->pr_type), 10, 1, 0, 0, "");
		uiDefIconButC(block, ROW, B_MATPRV, ICON_MATCUBE,		210,120,25,22, &(ma->pr_type), 10, 2, 0, 0, "");
		uiDefIconButS(block, ICONTOG|BIT|0, B_MATPRV, ICON_TRANSP_HLT,	210,80,25,22, &(ma->pr_back), 0, 0, 0, 0, "");
		uiDefIconBut(block, BUT, B_MATPRV, ICON_EYE,			210,10, 25,22, 0, 0, 0, 0, 0, "");
	}
}

void material_panels()
{
	Material *ma;
	MTex *mtex;
	Object *ob= OBACT;
	
	if(ob==0) return;
	
	// type numbers are ordered
	if((ob->type<OB_LAMP) && ob->type) {
		ma= give_current_material(ob, ob->actcol);

		// always draw first 2 panels
		material_panel_preview(ma);
		material_panel_material(ob, ma);
		
		if(ma) {
			material_panel_shading(ma);
			material_panel_texture(ma);
			
			mtex= ma->mtex[ ma->texact ];
			if(mtex && mtex->tex) {
				material_panel_map_input(ma);
				material_panel_map_to(ma);
			}
		}
	}
}

void lamp_panels()
{
	Object *ob= OBACT;
	
	if(ob==NULL || ob->type!= OB_LAMP) return;

	lamp_panel_preview(ob, ob->data);
	lamp_panel_lamp(ob, ob->data);
	lamp_panel_spot(ob, ob->data);
	lamp_panel_texture(ob, ob->data);
	lamp_panel_mapto(ob, ob->data);

}

void world_panels()
{
	World *wrld;

	wrld= G.scene->world;

	world_panel_preview(wrld);
	world_panel_world(wrld);

	if(wrld) {
		world_panel_mistaph(wrld);
		world_panel_texture(wrld);
		world_panel_mapto(wrld);
	}
}

void texture_panels()
{
	Material *ma=NULL;
	Lamp *la=NULL;
	World *wrld=NULL;
	Object *ob= OBACT;
	MTex *mtex= NULL;
	
	if(G.buts->texfrom==0) {
		if(ob) {
			ma= give_current_material(ob, ob->actcol);
			if(ma) mtex= ma->mtex[ ma->texact ];
		}
	}
	else if(G.buts->texfrom==1) {
		wrld= G.scene->world;
		if(wrld) mtex= wrld->mtex[ wrld->texact ];
	}
	else if(G.buts->texfrom==2) {
		if(ob && ob->type==OB_LAMP) {
			la= ob->data;
			mtex= la->mtex[ la->texact ];
		}
	}
	
	texture_panel_preview(ma || wrld || la);	// for 'from' buttons
	
	if(ma || wrld || la) {
	
		texture_panel_texture(mtex, ma, wrld, la);
		
		if(mtex && mtex->tex) {
			texture_panel_colors(mtex->tex);
			
			switch(mtex->tex->type) {
			case TEX_IMAGE:
				texture_panel_image(mtex->tex);
				texture_panel_image1(mtex->tex);
				break;
			case TEX_ENVMAP:
				texture_panel_envmap(mtex->tex);
				break;
			case TEX_CLOUDS:
				texture_panel_clouds(mtex->tex);
				break;
			case TEX_MARBLE:
				texture_panel_marble(mtex->tex);
				break;
			case TEX_STUCCI:
				texture_panel_stucci(mtex->tex);
				break;
			case TEX_WOOD:
				texture_panel_wood(mtex->tex);
				break;
			case TEX_BLEND:
				texture_panel_blend(mtex->tex);
				break;
			case TEX_MAGIC:
				texture_panel_magic(mtex->tex);
				break;
			case TEX_PLUGIN:
				texture_panel_plugin(mtex->tex);
				break;
			case TEX_NOISE:
				// no panel!
				break;
			}
		}
	}
}

#if 0
/* old popup.. too hackish, should be fixed once (ton) */ 
void clever_numbuts_buts()
{
	Material *ma;
	Lamp *la;
	World *wo;
	static char	hexrgb[8]; /* Uh... */
	static char	hexspec[8]; /* Uh... */
	static char	hexmir[8]; /* Uh... */
	static char hexho[8];
	static char hexze[8];
	int		rgb[3];
	
	switch (G.buts->mainb){
	case BUTS_FPAINT:

		sprintf(hexrgb, "%02X%02X%02X", (int)(Gvp.r*255), (int)(Gvp.g*255), (int)(Gvp.b*255));

		add_numbut(0, TEX, "RGB:", 0, 6, hexrgb, "HTML Hex value for the RGB color");
		do_clever_numbuts("Vertex Paint RGB Hex Value", 1, REDRAW); 
		
		/* Assign the new hex value */
		sscanf(hexrgb, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
		Gvp.r= (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
		Gvp.g = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
		Gvp.b = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;

		break;
	case BUTS_LAMP:
		la= G.buts->lockpoin;
		if (la){
			sprintf(hexrgb, "%02X%02X%02X", (int)(la->r*255), (int)(la->g*255), (int)(la->b*255));
			add_numbut(0, TEX, "RGB:", 0, 6, hexrgb, "HTML Hex value for the lamp color");
			do_clever_numbuts("Lamp RGB Hex Values", 1, REDRAW); 
			sscanf(hexrgb, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			la->r = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			la->g = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			la->b = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			BIF_preview_changed(G.buts);
		}
		break;
	case BUTS_WORLD:
		wo= G.buts->lockpoin;
		if (wo){
			sprintf(hexho, "%02X%02X%02X", (int)(wo->horr*255), (int)(wo->horg*255), (int)(wo->horb*255));
			sprintf(hexze, "%02X%02X%02X", (int)(wo->zenr*255), (int)(wo->zeng*255), (int)(wo->zenb*255));
			add_numbut(0, TEX, "Zen:", 0, 6, hexze, "HTML Hex value for the Zenith color");
			add_numbut(1, TEX, "Hor:", 0, 6, hexho, "HTML Hex value for the Horizon color");
			do_clever_numbuts("World RGB Hex Values", 2, REDRAW); 

			sscanf(hexho, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			wo->horr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			wo->horg = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			wo->horb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			sscanf(hexze, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			wo->zenr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			wo->zeng = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			wo->zenb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			BIF_preview_changed(G.buts);

		}
		break;
	case BUTS_MAT:

		ma= G.buts->lockpoin;

		/* Build a hex value */
		if (ma){
			sprintf(hexrgb, "%02X%02X%02X", (int)(ma->r*255), (int)(ma->g*255), (int)(ma->b*255));
			sprintf(hexspec, "%02X%02X%02X", (int)(ma->specr*255), (int)(ma->specg*255), (int)(ma->specb*255));
			sprintf(hexmir, "%02X%02X%02X", (int)(ma->mirr*255), (int)(ma->mirg*255), (int)(ma->mirb*255));

			add_numbut(0, TEX, "Col:", 0, 6, hexrgb, "HTML Hex value for the RGB color");
			add_numbut(1, TEX, "Spec:", 0, 6, hexspec, "HTML Hex value for the Spec color");
			add_numbut(2, TEX, "Mir:", 0, 6, hexmir, "HTML Hex value for the Mir color");
			do_clever_numbuts("Material RGB Hex Values", 3, REDRAW); 
			
			/* Assign the new hex value */
			sscanf(hexrgb, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			ma->r = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			ma->g = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			ma->b = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			sscanf(hexspec, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			ma->specr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			ma->specg = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			ma->specb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			sscanf(hexmir, "%02X%02X%02X", &rgb[0], &rgb[1], &rgb[2]);
			ma->mirr = (rgb[0]/255.0 >= 0.0 && rgb[0]/255.0 <= 1.0 ? rgb[0]/255.0 : 0.0) ;
			ma->mirg = (rgb[1]/255.0 >= 0.0 && rgb[1]/255.0 <= 1.0 ? rgb[1]/255.0 : 0.0) ;
			ma->mirb = (rgb[2]/255.0 >= 0.0 && rgb[2]/255.0 <= 1.0 ? rgb[2]/255.0 : 0.0) ;
			
			BIF_preview_changed(G.buts);
		}
		break;
	}
}

#endif

void radio_panels()
{
	Radio *rad;
	int flag;
	
	rad= G.scene->radio;
	if(rad==0) {
		add_radio();
		rad= G.scene->radio;
	}

	radio_panel_render(rad);
	
	flag= rad_phase();
	
	radio_panel_tool(rad, flag);
	if(flag) radio_panel_calculation(rad, flag);

	
}


