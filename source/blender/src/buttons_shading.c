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

	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, B_TEXPRV, "Depth:",		10, 90, 150, 19, &tex->noisedepth, 0.0, 10.0, 0, 0, "Sets the depth of the pattern");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	10, 70, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Sets the strength of the pattern");
}

static void texture_panel_blend(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_blend", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Blend", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_TEXPRV, "Lin",		10, 180, 75, 19, &tex->stype, 2.0, 0.0, 0, 0, "Creates a linear progresion"); 
	uiDefButS(block, ROW, B_TEXPRV, "Quad",		85, 180, 75, 19, &tex->stype, 2.0, 1.0, 0, 0, "Creates a quadratic progression"); 
	uiDefButS(block, ROW, B_TEXPRV, "Ease",		160, 180, 75, 19, &tex->stype, 2.0, 2.0, 0, 0, "Creates a progression easing from one step to the next"); 
	uiDefButS(block, TOG|BIT|1, B_TEXPRV, "Flip XY",	235, 180, 75, 19, &tex->flag, 0, 0, 0, 0, "Flips the direction of the progression 90 degrees");

	uiDefButS(block, ROW, B_TEXPRV, "Diag",		10, 160, 100, 19, &tex->stype, 2.0, 3.0, 0, 0, "Use a diagonal progression");
	uiDefButS(block, ROW, B_TEXPRV, "Sphere",	110, 160, 100, 19, &tex->stype, 2.0, 4.0, 0, 0, "Use progression with the shape of a sphere");
	uiDefButS(block, ROW, B_TEXPRV, "Halo",		210, 160, 100, 19, &tex->stype, 2.0, 5.0, 0, 0, "Use a quadratic progression with the shape of a sphere");
	
}

/* newnoise: noisebasis menu string */
static char* noisebasis_menu()
{
	static char nbmenu[256];
	sprintf(nbmenu, "Noise Basis %%t|Blender Original %%x%d|Original Perlin %%x%d|Improved Perlin %%x%d|Voronoi F1 %%x%d|Voronoi F2 %%x%d|Voronoi F3 %%x%d|Voronoi F4 %%x%d|Voronoi F2-F1 %%x%d|Voronoi Crackle %%x%d|CellNoise %%x%d", TEX_BLENDER, TEX_STDPERLIN, TEX_NEWPERLIN, TEX_VORONOI_F1, TEX_VORONOI_F2, TEX_VORONOI_F3, TEX_VORONOI_F4, TEX_VORONOI_F2F1, TEX_VORONOI_CRACKLE, TEX_CELLNOISE);
	return nbmenu;
}

static void texture_panel_wood(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_wood", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Wood", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_TEXPRV, "Bands",		10, 180, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Uses standard wood texture in bands"); 
	uiDefButS(block, ROW, B_TEXPRV, "Rings",		85, 180, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Uses wood texture in rings"); 
	uiDefButS(block, ROW, B_TEXPRV, "BandNoise",	160, 180, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, "Adds noise to standard wood"); 
	uiDefButS(block, ROW, B_TEXPRV, "RingNoise",	235, 180, 75, 18, &tex->stype, 2.0, 3.0, 0, 0, "Adds noise to rings"); 

	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	10, 160, 150, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Generates soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	160, 160, 150, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Generates hard noise");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 130, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	160, 130, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Sets the turbulence of the bandnoise and ringnoise types");
	uiBlockEndAlign(block);

	/* newnoise: noisebasis menu */
	uiDefBut(block, LABEL, 0, "Noise Basis",		10, 30, 150, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(),	10, 10, 150, 19, &tex->noisebasis, 0,0,0,0, "Sets the noise basis used for turbulence");

}

static void texture_panel_stucci(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_stucci", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Stucci", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_TEXPRV, "Plastic",		10, 180, 75, 19, &tex->stype, 2.0, 0.0, 0, 0, "Uses standard stucci");
	uiDefButS(block, ROW, B_TEXPRV, "Wall In",		85, 180, 75, 19, &tex->stype, 2.0, 1.0, 0, 0, "Creates Dimples"); 
	uiDefButS(block, ROW, B_TEXPRV, "Wall Out",		160, 180, 75, 19, &tex->stype, 2.0, 2.0, 0, 0, "Creates Ridges"); 
	
	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	10, 160, 112, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Generates soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	122, 160, 113, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Generates hard noise");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	10, 90, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Sets the depth of the stucci");
	uiBlockEndAlign(block);

	/* newnoise: noisebasis menu */
	uiDefBut(block, LABEL, 0, "Noise Basis",		10, 30, 150, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(),	10, 10, 150, 19, &tex->noisebasis, 0,0,0,0, "Sets the noise basis used for turbulence");

}

static void texture_panel_marble(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_marble", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Marble", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_TEXPRV, "Soft",			10, 180, 75, 18, &tex->stype, 2.0, 0.0, 0, 0, "Uses soft marble"); 
	uiDefButS(block, ROW, B_TEXPRV, "Sharp",		85, 180, 75, 18, &tex->stype, 2.0, 1.0, 0, 0, "Uses more clearly defined marble"); 
	uiDefButS(block, ROW, B_TEXPRV, "Sharper",		160, 180, 75, 18, &tex->stype, 2.0, 2.0, 0, 0, "Uses very clearly defined marble"); 

	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	10, 160, 112, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Generates soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	122, 160, 113, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Generates hard noise");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 110, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");
	uiDefButS(block, NUM, B_TEXPRV, "NoiseDepth:",	10, 90, 150, 19, &tex->noisedepth, 0.0, 6.0, 0, 0, "Sets the depth of the marble calculation");
	uiDefButF(block, NUM, B_TEXPRV, "Turbulence:",	10, 70, 150, 19, &tex->turbul, 0.0, 200.0, 10, 0, "Sets the turbulence of the sine bands");
	uiBlockEndAlign(block);
	
	/* newnoise: noisebasis menu */
	uiDefBut(block, LABEL, 0, "Noise Basis",		10, 30, 150, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(),	10, 10, 150, 19, &tex->noisebasis, 0,0,0,0, "Sets the noise basis used for turbulence");

}

static void texture_panel_clouds(Tex *tex)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_clouds", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Clouds", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_TEXPRV, "Default",		10, 180, 70, 18, &tex->stype, 2.0, 0.0, 0, 0, "Uses standard noise"); 
	uiDefButS(block, ROW, B_TEXPRV, "Color",		80, 180, 70, 18, &tex->stype, 2.0, 1.0, 0, 0, "Lets Noise return RGB value"); 
	uiDefButS(block, ROW, B_TEXPRV, "Soft noise",	155, 180, 75, 19, &tex->noisetype, 12.0, 0.0, 0, 0, "Generates soft noise");
	uiDefButS(block, ROW, B_TEXPRV, "Hard noise",	230, 180, 80, 19, &tex->noisetype, 12.0, 1.0, 0, 0, "Generates hard noise");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize :",	10, 130, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");
	uiDefButS(block, NUM, B_TEXPRV, "NoiseDepth:",	160, 130, 150, 19, &tex->noisedepth, 0.0, 6.0, 0, 0, "Sets the depth of the cloud calculation");
	uiBlockEndAlign(block);
	
	/* newnoise: noisebasis menu */
	uiDefBut(block, LABEL, 0, "Noise Basis",		10, 30, 150, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(),	10, 10, 150, 19, &tex->noisebasis, 0,0,0,0, "Sets the noise basis used for turbulence");

}

/*****************************************/
/* newnoise: panel(s) for musgrave types */
/*****************************************/

static void texture_panel_musgrave(Tex *tex)
{
	uiBlock *block;
	char *str;
	
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_musgrave", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Musgrave", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	str= "Ridged Multifractal %x0|Hybrid Multifractal %x1|Multifractal %x2|Hetero Terrain %x3|fBm %x4";
	uiDefButS(block, MENU, B_TEXPRV, str, 10, 160, 150, 19, &tex->stype, 0.0, 0.0, 0, 0, "Sets Musgrave type");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_TEXPRV, "H: ", 10, 130, 150, 19, &tex->mg_H, 0.0001, 2.0, 10, 0, "Sets the highest fractal dimension");
	uiDefButF(block, NUMSLI, B_TEXPRV, "Lacu: ", 160, 130, 150, 19, &tex->mg_lacunarity, 0.0, 6.0, 10, 0, "Sets the gap between succesive frequencies");
	uiDefButF(block, NUMSLI, B_TEXPRV, "Octs: ", 10, 110, 150, 19, &tex->mg_octaves, 0.0, 8.0, 10, 0, "Sets the number of frequencies used");
	if ((tex->stype==TEX_RIDGEDMF) || (tex->stype==TEX_HYBRIDMF) || (tex->stype==TEX_HTERRAIN)) {
		uiDefButF(block, NUMSLI, B_TEXPRV, "Ofst: ", 160, 110, 150, 19, &tex->mg_offset, 0.0, 6.0, 10, 0, "Sets the fractal offset");
		if ((tex->stype==TEX_RIDGEDMF) || (tex->stype==TEX_HYBRIDMF))
			uiDefButF(block, NUMSLI, B_TEXPRV, "Gain: ", 10, 90, 150, 19, &tex->mg_gain, 0.0, 6.0, 10, 0, "Sets the gain multiplier");
	}

	uiBlockBeginAlign(block);
	/* noise output scale */
	uiDefButF(block, NUM, B_TEXPRV, "iScale: ", 10, 60, 150, 19, &tex->ns_outscale, 0.0, 10.0, 10, 0, "Scales intensity output");	
	/* frequency scale */
	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize: ",	160, 60, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");
	uiBlockEndAlign(block);

	/* noisebasis menu */
	uiDefBut(block, LABEL, 0, "Noise Basis", 10, 30, 150, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(), 10, 10, 150, 19, &tex->noisebasis, 0,0,0,0, "Sets the noise basis used for turbulence");

}


static void texture_panel_distnoise(Tex *tex)
{
	uiBlock *block;
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_distnoise", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Distorted Noise", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	uiBlockBeginAlign(block);
	/* distortion amount */
	uiDefButF(block, NUM, B_TEXPRV, "DistAmnt: ", 10, 130, 150, 19, &tex->dist_amount, 0.0, 10.0, 10, 0, "Sets amount of distortion");	
	/* frequency scale */
	uiDefButF(block, NUM, B_TEXPRV, "NoiseSize: ",	160, 130, 150, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Distortion Noise", 10, 100, 150, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, LABEL, 0, "Noise Basis", 	160, 100, 150, 19, 0, 0.0, 0.0, 0, 0, "");

	uiBlockBeginAlign(block);
	/* noisebasis used for the distortion */	
	uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(), 10, 80, 150, 19, &tex->noisebasis, 0,0,0,0, "Sets the noise basis which does the distortion");
	/* noisebasis to distort */
	uiDefButS(block, MENU, B_TEXPRV, noisebasis_menu(), 160, 80, 150, 19, &tex->noisebasis2, 0,0,0,0, "Sets the noise basis to distort");

}


static void texture_panel_voronoi(Tex *tex)
{
	char dm_menu[256];
	uiBlock *block;
	block= uiNewBlock(&curarea->uiblocks, "texture_panel_voronoi", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Voronoi", "Texture", 640, 0, 318, 204)==0) return;
	uiSetButLock(tex->id.lib!=0, "Can't edit library data");

	/* color types */
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_TEXPRV, "Int", 10, 180, 50, 18, &tex->vn_coltype, 1.0, 0.0, 0, 0, "Only calculate intensity"); 
	uiDefButS(block, ROW, B_TEXPRV, "Col1", 60, 180, 50, 18, &tex->vn_coltype, 1.0, 1.0, 0, 0, "Color cells by position"); 
	uiDefButS(block, ROW, B_TEXPRV, "Col2", 110, 180, 50, 18, &tex->vn_coltype, 1.0, 2.0, 0, 0, "Same as Col1 + outline based on F2-F1"); 
	uiDefButS(block, ROW, B_TEXPRV, "Col3", 160, 180, 50, 18, &tex->vn_coltype, 1.0, 3.0, 0, 0, "Same as Col2 * intensity"); 
	uiBlockEndAlign(block);

	/* distance metric */
	sprintf(dm_menu, "Distance Metric %%t|Actual Distance %%x%d|Distance Squared %%x%d|Manhattan %%x%d|Chebychev %%x%d|Minkovsky 1/2 %%x%d|Minkovsky 4 %%x%d|Minkovsky %%x%d", TEX_DISTANCE, TEX_DISTANCE_SQUARED, TEX_MANHATTAN, TEX_CHEBYCHEV, TEX_MINKOVSKY_HALF, TEX_MINKOVSKY_FOUR, TEX_MINKOVSKY);
	uiDefBut(block, LABEL, 0, "Distance Metric", 10, 160, 200, 19, 0, 0, 0, 0, 0, "");
	uiDefButS(block, MENU, B_TEXPRV, dm_menu, 10, 140, 200, 19, &tex->vn_distm, 0,0,0,0, "Sets the distance metric to be used");

	if (tex->vn_distm==TEX_MINKOVSKY)
		uiDefButF(block, NUMSLI, B_TEXPRV, "Exp: ", 10, 120, 200, 19, &tex->vn_mexp, 0.01, 10.0, 10, 0, "Sets minkovsky exponent");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_TEXPRV, "iScale: ", 	10, 95, 100, 19, &tex->ns_outscale, 0.01, 10.0, 10, 0, "Scales intensity output");
	uiDefButF(block, NUM, B_TEXPRV, "Size: ",	110, 95, 100, 19, &tex->noisesize, 0.0001, 2.0, 10, 0, "Sets scaling for noise input");

	/* weights */
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_TEXPRV, "W1: ", 10, 70, 200, 19, &tex->vn_w1, -2.0, 2.0, 10, 0, "Sets feature weight 1");
	uiDefButF(block, NUMSLI, B_TEXPRV, "W2: ", 10, 50, 200, 19, &tex->vn_w2, -2.0, 2.0, 10, 0, "Sets feature weight 2");
	uiDefButF(block, NUMSLI, B_TEXPRV, "W3: ", 10, 30, 200, 19, &tex->vn_w3, -2.0, 2.0, 10, 0, "Sets feature weight 3");
	uiDefButF(block, NUMSLI, B_TEXPRV, "W4: ", 10, 10, 200, 19, &tex->vn_w4, -2.0, 2.0, 10, 0, "Sets feature weight 4");
}


/***************************************/

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
		
		uiDefButS(block, ROW, B_REDR, 	"Static", 	10, 180, 100, 19, &env->stype, 2.0, 0.0, 0, 0, "Calculates environment map only once");
		uiDefButS(block, ROW, B_REDR, 	"Anim", 	110, 180, 100, 19, &env->stype, 2.0, 1.0, 0, 0, "Calculates environment map at each rendering");
		uiDefButS(block, ROW, B_ENV_FREE, "Load", 	210, 180, 100, 19, &env->stype, 2.0, 2.0, 0, 0, "Loads saved environment map from disk");

		if(env->stype==ENV_LOAD) {
			/* file input */
			id= (ID *)tex->ima;
			IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(G.buts->menunr));
			if(strp[0]) {
				uiBlockBeginAlign(block);
				uiDefButS(block, MENU, B_TEXIMABROWSE, strp, 10,145,23,20, &(G.buts->menunr), 0, 0, 0, 0, "Selects an existing environment map or creates new");
				
				if(tex->ima) {
					uiDefBut(block, TEX, B_NAMEIMA, "",			35,145,255,20, tex->ima->name, 0.0, 79.0, 0, 0, "Displays environment map name: click to change");
					sprintf(str, "%d", tex->ima->id.us);
					uiDefBut(block, BUT, 0, str,				290,145,20,20, 0, 0, 0, 0, 0, "Displays number of users of environment map: click to make single user");
					uiBlockEndAlign(block);
					
					uiDefBut(block, BUT, B_RELOADIMA, "Reload",	230,125,80,20, 0, 0, 0, 0, 0, "Reloads saved environment map");
				
					if (tex->ima->packedfile) packdummy = 1;
					else packdummy = 0;
					uiDefIconButI(block, TOG|BIT|0, B_PACKIMA, ICON_PACKAGE, 205,125,24,20, &packdummy, 0, 0, 0, 0, "Toggles Packed status of this environment map");
				}
				else uiBlockEndAlign(block);
			}
			MEM_freeN(strp);
		
			uiDefBut(block, BUT, B_LOADTEXIMA, "Load Image", 10,125,150,20, 0, 0, 0, 0, 0, "Loads saved environment map - file select");
		}
		else {
			uiBlockBeginAlign(block);
			uiDefBut(block, BUT, B_ENV_FREE, "Free Data", 	10,135,100,20, 0, 0, 0, 0, 0, "Releases all images associated with this environment map");
			uiDefBut(block, BUT, B_ENV_SAVE, "Save EnvMap", 110,135,100,20, 0, 0, 0, 0, 0, "Saves current environment map");
			uiDefBut(block, BUT, B_ENV_FREE_ALL, "Free all EnvMaps", 210,135,100,20, 0, 0, 0, 0, 0, "Frees all rendered environment maps for all materials");
			uiBlockEndAlign(block);
		}

		uiDefIDPoinBut(block, test_obpoin_but, B_ENV_OB, "Ob:",	10,90,150,20, &(env->object), "Displays object to use as viewpoint for environment map: click to change");
		if(env->stype!=ENV_LOAD) 
			uiDefButS(block, NUM, B_ENV_FREE, 	"CubeRes", 		160,90,150,20, &env->cuberes, 50, 2048.0, 0, 0, "Sets the pixel resolution of the rendered environment map");

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_TEXPRV, "Filter :",				10,65,150,20, &tex->filtersize, 0.1, 25.0, 0, 0, "Adjusts sharpness or blurriness of the reflection"),
		uiDefButS(block, NUM, B_ENV_FREE, "Depth:",				160,65,150,20, &env->depth, 0, 5.0, 0, 0, "Sets the number of times a map will be rendered recursively mirror effects"),
		uiDefButF(block, NUM, REDRAWVIEW3D, 	"ClipSta", 		10,40,150,20, &env->clipsta, 0.01, 50.0, 100, 0, "Sets start value for clipping: objects nearer than this are not visible to map");
		uiDefButF(block, NUM, 0, 	"ClipEnd", 					160,40,150,20, &env->clipend, 0.1, 5000.0, 1000, 0, "Sets end value for clipping beyond which objects are not visible to map");
		uiBlockEndAlign(block);
		
		uiDefBut(block, LABEL, 0, "Don't render layer:",		10,10,140,22, 0, 0.0, 0.0, 0, 0, "");	
		xco= 160;
		yco= 10;
		dx= 28;
		dy= 26;
		
		uiBlockBeginAlign(block);
		for(a=0; a<5; a++) 
			uiDefButI(block, TOG|BIT|a, 0, "",	(xco+a*(dx/2)), (yco+dy/2), (dx/2), (1+dy/2), &env->notlay, 0, 0, 0, 0, "Toggles layer visibility to environment map");
		for(a=0; a<5; a++) 
			uiDefButI(block, TOG|BIT|(a+10), 0, "",(xco+a*(dx/2)), yco, (dx/2), (dy/2), &env->notlay, 0, 0, 0, 0, "Toggles layer visibility to environment map");

		uiBlockBeginAlign(block);
		xco+= 5;
		for(a=5; a<10; a++) 
			uiDefButI(block, TOG|BIT|a, 0, "",	(xco+a*(dx/2)), (yco+dy/2), (dx/2), (1+dy/2), &env->notlay, 0, 0, 0, 0, "Toggles layer visibility to environment map");
		for(a=5; a<10; a++) 
			uiDefButI(block, TOG|BIT|(a+10), 0, "",(xco+a*(dx/2)), yco, (dx/2), (dy/2), &env->notlay, 0, 0, 0, 0, "Toggles layer visibility to environment map");

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
		uiDefBut(block, BUT, B_TEXSETFRAMES, "<",      802, 110, 20, 18, 0, 0, 0, 0, 0, "Copies number of frames in movie file to Frames: button");
		sprintf(str, "%d frs  ", IMB_anim_get_duration(tex->ima->anim));
		uiDefBut(block, LABEL, 0, str,      834, 110, 90, 18, 0, 0, 0, 0, 0, "Number of frames in movie file");
		sprintf(str, "%d cur  ", tex->ima->lastframe);
		uiDefBut(block, LABEL, 0, str,      834, 90, 90, 18, 0, 0, 0, 0, 0, "");
	}
	else uiDefBut(block, LABEL, 0, "<",      802, 110, 20, 18, 0, 0, 0, 0, 0, "");
			
	uiDefButS(block, NUM, B_TEXPRV, "Frames :",	642,110,150,19, &tex->frames, 0.0, 18000.0, 0, 0, "Sets the number of frames of a movie to use and activates animation options");
	uiDefButS(block, NUM, B_TEXPRV, "Offset :",	642,90,150,19, &tex->offset, -9000.0, 9000.0, 0, 0, "Offsets the number of the first movie frame to use in the animation");
	uiDefButS(block, NUM, B_TEXPRV, "Fie/Ima:",	642,60,98,19, &tex->fie_ima, 1.0, 200.0, 0, 0, "Sets the number of fields per rendered frame");
	uiDefButS(block, NUM, B_TEXPRV, "StartFr:",	642,30,150,19, &tex->sfra, 1.0, 9000.0, 0, 0, "Sets the starting frame of the movie to use in animation");
	uiDefButS(block, NUM, B_TEXPRV, "Len:",		642,10,150,19, &tex->len, 0.0, 9000.0, 0, 0, "Sets the number of movie frames to use in animation: 0=all");
	
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
	uiDefButS(block, TOG|BIT|6, B_TEXPRV, "Cyclic",		743,60,48,19, &tex->imaflag, 0, 0, 0, 0, "Toggles looping of animated frames");
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
	uiDefButS(block, TOG|BIT|0, 0, "InterPol",			10, 180, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Interpolates pixels of the Image to fit texture mapping");
	uiDefButS(block, TOG|BIT|1, B_TEXPRV, "UseAlpha",	85, 180, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Click to use Image's alpha channel");
	uiDefButS(block, TOG|BIT|5, B_TEXPRV, "CalcAlpha",	160, 180, 75, 18, &tex->imaflag, 0, 0, 0, 0, "Click to calculate an alpha channel based on Image RGB values");
	uiDefButS(block, TOG|BIT|2, B_TEXPRV, "NegAlpha",	235, 180, 75, 18, &tex->flag, 0, 0, 0, 0, "Click to invert the alpha values");
	
	uiDefButS(block, TOG|BIT|2, B_IMAPTEST, "MipMap",	10, 160, 60, 18, &tex->imaflag, 0, 0, 0, 0, "Generates a series of pictures to use for mipmapping");
	uiDefButS(block, TOG|BIT|3, B_IMAPTEST, "Fields",	70, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Click to enable use of fields in Image");
	uiDefButS(block, TOG|BIT|4, B_TEXPRV, "Rot90",		120, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Rotates image 90 degrees for rendering");
	uiDefButS(block, TOG|BIT|7, B_RELOADIMA, "Movie",	170, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Click to enable movie frames as Images");
	uiDefButS(block, TOG|BIT|8, 0, "Anti",				220, 160, 40, 18, &tex->imaflag, 0, 0, 0, 0, "Toggles Image anti-aliasing");
	uiDefButS(block, TOG|BIT|10, 0, "StField",			260, 160, 50, 18, &tex->imaflag, 0, 0, 0, 0, "Standard Field Toggle");
	uiBlockEndAlign(block);
	
	/* file input */
	id= (ID *)tex->ima;
	IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->image), id, &(G.buts->menunr));
	if(strp[0]) {
		uiBlockBeginAlign(block);
		uiDefButS(block, MENU, B_TEXIMABROWSE, strp, 10,135,23,20, &(G.buts->menunr), 0, 0, 0, 0, "Selects an existing texture or creates new");
		
		if(tex->ima) {
			uiDefBut(block, TEX, B_NAMEIMA, "",			35,135,255,20, tex->ima->name, 0.0, 79.0, 0, 0, "Displays name of the texture block: click to change");
			sprintf(str, "%d", tex->ima->id.us);
			uiDefBut(block, BUT, 0, str,				290,135,20,20, 0, 0, 0, 0, 0, "Displays number of users of texture: click to make single user");
			uiBlockEndAlign(block);
			
			uiDefBut(block, BUT, B_RELOADIMA, "Reload",	230,115,80,19, 0, 0, 0, 0, 0, "Reloads Image");
		
			if (tex->ima->packedfile) packdummy = 1;
			else packdummy = 0;
			
			uiDefIconButI(block, TOG|BIT|0, B_PACKIMA, ICON_PACKAGE, 205,115,24,19, &packdummy, 0, 0, 0, 0, "Toggles Packed status of this Image");
		}
		else uiBlockEndAlign(block);
	}
	MEM_freeN(strp);

	uiDefBut(block, BUT, B_LOADTEXIMA, "Load Image", 10,115,150,19, 0, 0, 0, 0, 0, "Click to load an Image");

	/* crop extend clip */
	
	uiDefButF(block, NUM, B_TEXPRV, "Filter :",	10,92,150,19, &tex->filtersize, 0.1, 25.0, 0, 0, "Sets the filter size used by mipmap and interpol");
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, 0, "Extend",			10,70,75,19, &tex->extend, 4.0, 1.0, 0, 0, "Extends the colour of the edge pixels");
	uiDefButS(block, ROW, 0, "Clip",			85,70,75,19, &tex->extend, 4.0, 2.0, 0, 0, "Sets alpha 0.0 outside Image edges");
	uiDefButS(block, ROW, 0, "ClipCube",		160,70,75,19, &tex->extend, 4.0, 4.0, 0, 0, "Sets alpha to 0.0 outside cubeshaped area around Image");
	uiDefButS(block, ROW, 0, "Repeat",			235,70,75,19, &tex->extend, 4.0, 3.0, 0, 0, "Causes Image to repeat horizontally and vertically");
	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, B_TEXPRV, "Xrepeat:",	10,50,150,19, &tex->xrepeat, 1.0, 512.0, 0, 0, "Sets a repetition multiplier in the X direction");
	uiDefButS(block, NUM, B_TEXPRV, "Yrepeat:",	160,50,150,19, &tex->yrepeat, 1.0, 512.0, 0, 0, "Sets a repetition multiplier in the Y direction");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_REDR, "MinX ",		10,28,150,19, &tex->cropxmin, -10.0, 10.0, 10, 0, "Sets minimum X value to crop Image");
	uiDefButF(block, NUM, B_REDR, "MinY ",		10,8,150,19, &tex->cropymin, -10.0, 10.0, 10, 0, "Sets minimum Y value to crop Image");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_REDR, "MaxX ",		160,28,150,19, &tex->cropxmax, -10.0, 10.0, 10, 0, "Sets maximum X value to crop Image");
	uiDefButF(block, NUM, B_REDR, "MaxY ",		160,8,150,19, &tex->cropymax, -10.0, 10.0, 10, 0, "Sets maximum Y value to crop Image");
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
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|0, B_COLORBAND, "Colorband",10,180,100,20, &tex->flag, 0, 0, 0, 0, "Toggles colorband operations");

	if(tex->flag & TEX_COLORBAND) {
		uiDefBut(block, BUT, B_ADDCOLORBAND, "Add",		110,180,50,20, 0, 0, 0, 0, 0, "Adds a new colour position to the colorband");
		uiDefButS(block, NUM, B_REDR,		"Cur:",		160,180,100,20, &tex->coba->cur, 0.0, (float)(tex->coba->tot-1), 0, 0, "Displays the active colour from the colorband");
		uiDefBut(block, BUT, B_DELCOLORBAND, "Del",		260,180,50,20, 0, 0, 0, 0, 0, "Deletes the active position");
		uiDefBut(block, LABEL, B_DOCOLORBAND, "", 		10,150,300,20, 0, 0, 0, 0, 0, "Colorband"); /* only for event! */
		
		uiBlockSetDrawExtraFunc(block, drawcolorband_cb);
		cbd= tex->coba->data + tex->coba->cur;
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_CALCCBAND, "Pos",	10,120,80,20, &cbd->pos, 0.0, 1.0, 10, 0, "Sets the position of the active colour");
		uiDefButS(block, ROW, B_TEXREDR_PRV, "E",		90,120,20,20, &tex->coba->ipotype, 5.0, 1.0, 0, 0, "More complicated Interpolation");
		uiDefButS(block, ROW, B_TEXREDR_PRV, "L",		110,120,20,20, &tex->coba->ipotype, 5.0, 0.0, 0, 0, "Sets interpolation type to Linear");
		uiDefButS(block, ROW, B_TEXREDR_PRV, "S",		130,120,20,20, &tex->coba->ipotype, 5.0, 2.0, 0, 0, "Sets interpolation type to Spline");
		uiDefButF(block, COL, B_BANDCOL, "",		150,120,30,20, &(cbd->r), 0, 0, 0, 0, "");
		uiDefButF(block, NUMSLI, B_TEXREDR_PRV, "A ",	180,120,130,20, &cbd->a, 0.0, 1.0, 0, 0, "Sets the alpha value for this position");
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_TEXREDR_PRV, "R ",	10,100,100,20, &cbd->r, 0.0, 1.0, B_BANDCOL, 0, "Sets the red value for the active colour");
		uiDefButF(block, NUMSLI, B_TEXREDR_PRV, "G ",	110,100,100,20, &cbd->g, 0.0, 1.0, B_BANDCOL, 0, "Sets the green value for the active colour");
		uiDefButF(block, NUMSLI, B_TEXREDR_PRV, "B ",	210,100,100,20, &cbd->b, 0.0, 1.0, B_BANDCOL, 0, "Sets the blue value for the active colour");
	}
	
	/* RGB-BRICON */
	if((tex->flag & TEX_COLORBAND)==0) {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_TEXPRV, "R ",		60,80,200,20, &tex->rfac, 0.0, 2.0, 0, 0, "Changes the red value of the texture");
		uiDefButF(block, NUMSLI, B_TEXPRV, "G ",		60,60,200,20, &tex->gfac, 0.0, 2.0, 0, 0, "Changes the green value of the texture");
		uiDefButF(block, NUMSLI, B_TEXPRV, "B ",		60,40,200,20, &tex->bfac, 0.0, 2.0, 0, 0, "Changes the blue value of the texture");
	}

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_TEXPRV, "Bright",		10,10,150,20, &tex->bright, 0.0, 2.0, 0, 0, "Changes the brightness of the colour or intensity of a texture");
	uiDefButF(block, NUMSLI, B_TEXPRV, "Contr",			160,10,150,20, &tex->contrast, 0.01, 2.0, 0, 0, "Changes the contrast of the colour or intensity of a texture");
}


static void texture_panel_texture(MTex *mtex, Material *ma, World *wrld, Lamp *la)
{
	MTex *mt=NULL;
	uiBlock *block;
	ID *id, *idfrom;
	int a, yco, loos;
	char str[32];
	

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
			uiDefButC(block, ROW, B_TEXCHANNEL, str,	10,yco,140,19, &(ma->texact), 0.0, (float)a, 0, 0, "Click to select texture channel");
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
		char textypes[512];
		Tex *tex= mtex->tex;

		uiSetButLock(tex->id.lib!=0, "Can't edit library data");
		
		/* newnoise: all texture types as menu, not enough room for more buttons.
		 * Can widen panel, but looks ugly when other panels overlap it */
		
		sprintf(textypes, "Texture Type %%t|None %%x%d|Image %%x%d|EnvMap %%x%d|Clouds %%x%d|Marble %%x%d|Stucci %%x%d|Wood %%x%d|Magic %%x%d|Blend %%x%d|Noise %%x%d|Plugin %%x%d|Musgrave %%x%d|Voronoi %%x%d|DistortedNoise %%x%d", 0, TEX_IMAGE, TEX_ENVMAP, TEX_CLOUDS, TEX_MARBLE, TEX_STUCCI, TEX_WOOD, TEX_MAGIC, TEX_BLEND, TEX_NOISE, TEX_PLUGIN, TEX_MUSGRAVE, TEX_VORONOI, TEX_DISTNOISE);
		uiDefBut(block, LABEL, 0, "Texture Type",		160, 150, 140, 20, 0, 0.0, 0.0, 0, 0, "");
		uiDefButS(block, MENU, B_TEXTYPE, textypes,	160, 130, 140, 20, &tex->type, 0,0,0,0, "Select texture type");

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
	uiDefButC(block, ROW, B_TEXREDR_PRV, "Mat",		200,175,80,25, &G.buts->texfrom, 3.0, 0.0, 0, 0, "Displays the textures of the active material");
	uiDefButC(block, ROW, B_TEXREDR_PRV, "World",	200,150,80,25, &G.buts->texfrom, 3.0, 1.0, 0, 0, "Displays the textures of the world block");
	uiDefButC(block, ROW, B_TEXREDR_PRV, "Lamp",	200,125,80,25, &G.buts->texfrom, 3.0, 2.0, 0, 0, "Displays the textures of the selected lamp");
	uiBlockEndAlign(block);
	uiDefBut(block, BUT, B_DEFTEXVAR, "Default Vars",200,10,80,20, 0, 0, 0, 0, 0, "Sets all values to defaults");

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
	uiDefBut(block,  BUT, B_RAD_GO, "GO",					0, 0, 10, 15, NULL, 0, 0, 0, 0, "Starts the radiosity simulation");

	uiBlockSetCol(block, TH_AUTO);
	uiDefButS(block,  NUM, 0, "SubSh Patch:", 				1, 0, 10, 10, &rad->subshootp, 0.0, 10.0, 0, 0, "Sets the number of times the environment is tested to detect pathes");
	uiDefButS(block,  NUM, 0, "SubSh Element:", 			1, 0, 10, 10, &rad->subshoote, 0.0, 10.0, 0, 0, "Sets the number of times the environment is tested to detect elements");

	if(flag != RAD_PHASE_PATCHES) uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_SHOOTE, "Subdiv Shoot Element", 2, 0, 10, 10, NULL, 0, 0, 0, 0, "For pre-subdivision, Detects high energy changes and subdivide Elements");
	uiDefBut(block,  BUT, B_RAD_SHOOTP, "Subdiv Shoot Patch",	2, 0, 10, 10, NULL, 0, 0, 0, 0, "For pre-subdivision, Detects high energy changes and subdivide Patches");

	uiBlockSetCol(block, TH_AUTO);
	uiDefButI(block,  NUM, 0, "MaxEl:",						3, 0, 10, 10, &rad->maxnode, 1.0, 250000.0, 0, 0, "Sets the maximum allowed number of elements");
	uiDefButS(block,  NUM, 0, "Max Subdiv Shoot:", 			3, 0, 10, 10, &rad->maxsublamp, 1.0, 250.0, 0, 0, "Sets the maximum number of initial shoot patches that are evaluated");

	if(flag & RAD_PHASE_FACES);
	else uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_FACEFILT, "FaceFilter",		4, 0, 10, 10, NULL, 0, 0, 0, 0, "Forces an extra smoothing");
	uiDefBut(block,  BUT, B_RAD_NODEFILT, "Element Filter",	4, 0, 10, 10, NULL, 0, 0, 0, 0, "Filters elements to remove aliasing artefacts");

	uiDefBut(block,  BUT, B_RAD_NODELIM, "RemoveDoubles",	5, 0, 30, 10, NULL, 0.0, 50.0, 0, 0, "Joins elements which differ less than 'Lim'");
	uiBlockSetCol(block, TH_AUTO);
	uiDefButS(block,  NUM, 0, "Lim:",						5, 0, 10, 10, &rad->nodelim, 0.0, 50.0, 0, 0, "Sets the range for removing doubles");


}

static void radio_panel_tool(Radio *rad, int flag)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "radio_panel_tool", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Radio Tool", "Radio", 320, 0, 318, 204)==0) return;
	uiAutoBlock(block, 10, 10, 300, 200, UI_BLOCK_ROWS);

	if(flag & RAD_PHASE_PATCHES) uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefBut(block,  BUT, B_RAD_COLLECT, "Collect Meshes",	0, 0, 10, 15, NULL, 0, 0, 0, 0, "Converts selected visible meshes to patches");

	if(flag & RAD_PHASE_PATCHES)uiBlockSetCol(block, TH_AUTO);
	else uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_FREE, "Free Radio Data",	0, 0, 10, 15, NULL, 0, 0, 0, 0, "Releases all memory used by Radiosity");	

	if(flag & RAD_PHASE_FACES) uiBlockSetCol(block, TH_AUTO);
	else uiBlockSetCol(block, TH_BUT_NEUTRAL);
	uiDefBut(block,  BUT, B_RAD_REPLACE, "Replace Meshes",	1, 0, 10, 12, NULL, 0, 0, 0, 0, "Converts meshes to Mesh objects with vertex colours, changing input-meshes");
	uiDefBut(block,  BUT, B_RAD_ADDMESH, "Add new Meshes",	1, 0, 10, 12, NULL, 0, 0, 0, 0, "Converts meshes to Mesh objects with vertex colours, unchanging input-meshes");

	uiBlockSetCol(block, TH_AUTO);
	uiDefButS(block,  ROW, B_RAD_DRAW, "Wire",			2, 0, 10, 10, &rad->drawtype, 0.0, 0.0, 0, 0, "Enables wireframe drawmode");
	uiDefButS(block,  ROW, B_RAD_DRAW, "Solid",			2, 0, 10, 10, &rad->drawtype, 0.0, 1.0, 0, 0, "Enables solid drawmode");
	uiDefButS(block,  ROW, B_RAD_DRAW, "Gour",			2, 0, 10, 10, &rad->drawtype, 0.0, 2.0, 0, 0, "Enables Gourad drawmode");
	uiDefButS(block,  TOG|BIT|0, B_RAD_DRAW, "ShowLim", 2, 0, 10, 10, &rad->flag, 0, 0, 0, 0, "Draws patch and element limits");
	uiDefButS(block,  TOG|BIT|1, B_RAD_DRAW, "Z",		2, 0, 3, 10, &rad->flag, 0, 0, 0, 0, "Draws limits differently");

	uiDefButS(block,  NUM, B_RAD_LIMITS, "ElMax:", 		3, 0, 10, 10, &rad->elma, 1.0, 500.0, 0, 0, "Sets maximum size of an element");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "ElMin:", 		3, 0, 10, 10, &rad->elmi, 1.0, 100.0, 0, 0, "Sets minimum size of an element");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "PaMax:", 		3, 0, 10, 10, &rad->pama, 10.0, 1000.0, 0, 0, "Sets maximum size of a patch");
	uiDefButS(block,  NUM, B_RAD_LIMITS, "PaMin:", 		3, 0, 10, 10, &rad->pami, 10.0, 1000.0, 0, 0, "Sets minimum size of a patch");

	uiDefBut(block,  BUT, B_RAD_INIT, "Limit Subdivide", 5, 0, 10, 10, NULL, 0, 0, 0, 0, "Subdivides patches");
}


static void radio_panel_render(Radio *rad)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "radio_panel_render", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Radio Render", "Radio", 0, 0, 318, 204)==0) return;
	uiAutoBlock(block, 210, 30, 230, 150, UI_BLOCK_ROWS);

	uiDefButS(block,  NUMSLI, B_RAD_LIMITS, "Hemires:", 0, 0, 10, 10, &rad->hemires, 100.0, 1000.0, 100, 0, "Sets the size of a hemicube");
	uiDefButS(block,  NUM, 0, "Max Iterations:", 		2, 0, 10, 15, &rad->maxiter, 0.0, 10000.0, 0, 0, "Limits the maximum number of radiosity rounds");
	uiDefButF(block,  NUM, B_RAD_FAC, "Mult:",			3, 0, 10, 15, &rad->radfac, 0.001, 250.0, 100, 0, "Mulitplies the energy values");
	uiDefButF(block,  NUM, B_RAD_FAC, "Gamma:",			3, 0, 10, 15, &rad->gamma, 0.2, 10.0, 10, 0, "Changes the contrast of the energy values");
	uiDefButF(block,  NUMSLI, 0, "Convergence:", 		5, 0, 10, 10, &rad->convergence, 0.0, 1.0, 10, 0, "Sets the lower threshold of unshot energy");
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
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	920,130,52,19, &(mtex->texflag), 0, 0, 0, 0, "Sets the texture mapping to stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		974,130,38,19, &(mtex->texflag), 0, 0, 0, 0, "Inverts the values of the texture to reverse its effect");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "No RGB",		1014,130,69,19, &(mtex->texflag), 0, 0, 0, 0, "Converts texture RGB values to intensity (gray) values");
	uiBlockEndAlign(block);
	
	uiDefButF(block, COL, B_MTEXCOL, "",				920,105,163,19, &(mtex->r), 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "R ",			920,80,163,19, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of red the intensity texture blends");
	uiDefButF(block, NUMSLI, B_MATPRV, "G ",			920,60,163,19, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of green the intensity texture blends");
	uiDefButF(block, NUMSLI, B_MATPRV, "B ",			920,40,163,19, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of blue the intensity texture blends");
	uiBlockEndAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		920,10,163,19, &(mtex->def_var), 0.0, 1.0, 0, 0, "Sets the amount the texture blends with the basic value");
	
	/* MAP TO */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Blend",		920,180,86,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the colour progression of the background");
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Hori",		1006,180,87,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the colour of the horizon");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "ZenUp",		1093,180,86,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the colour of the zenith above");
	uiDefButS(block, TOG|BIT|3, B_MATPRV, "ZenDo",		1179,180,86,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the colour of the zenith below");

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Mix",			1087,130,48,19, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "Sets texture to blend the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,130,44,19, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "Sets texture to multiply the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,130,41,19, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "Sets texture to add the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,130,40,19, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "Sets texture to subtract the values or colour");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "Col  ",		1087,50,179,19, &(mtex->colfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects colour values");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor  ",		1087,30,179,19, &(mtex->norfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects normal values");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var  ",		1087,10,179,19, &(mtex->varfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects other values");
	
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
		uiDefButS(block, ROW, REDRAWBUTSSHADING, str,10, 160-20*a, 80, 20, &(wrld->texact), 3.0, (float)a, 0, 0, "Texture channel");
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
	uiDefButS(block, MENU, B_WTEXBROWSE, strp, 100,140,20,19, &(G.buts->texnr), 0, 0, 0, 0, "Selects an existing texture or creates new");
	MEM_freeN(strp);
	
	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	100,160,200,19, id->name+2, 0.0, 18.0, 0, 0, "Displays name of the texture block: click to change");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,			196,140,21,19, 0, 0, 0, 0, 0, "Displays number of users of texture: click to make single user");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 279,140,21,19, 0, 0, 0, 0, 0, "Auto-assigns name to texture");
		if(id->lib) {
			if(wrld->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	219,140,21,19, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	219,140,21,19, 0, 0, 0, 0, 0, "");	
		}
		uiBlockSetCol(block, TH_AUTO);
		uiDefBut(block, BUT, B_TEXCLEARWORLD, "Clear", 122, 140, 72, 19, 0, 0, 0, 0, 0, "Erases link to texture");
	}
	else 
		uiDefButS(block, TOG, B_WTEXBROWSE, "Add New" ,100, 160, 200, 19, &(G.buts->texnr), -1.0, 32767.0, 0, 0, "Adds a new texture datablock");

	uiBlockSetCol(block, TH_AUTO);
	

	/* TEXCO */
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "View",			100,110,60,20, &(mtex->texco), 4.0, (float)TEXCO_VIEW, 0, 0, "Uses global coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "AngMap",		160,110,70,20, &(mtex->texco), 4.0, (float)TEXCO_ANGMAP, 0, 0, "Uses angular coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Object",		230,110,70,20, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "Uses linked object's coordinates for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "", 100,90,200,20, &(mtex->object), "");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "dX",		100,50,100,19, mtex->ofs, -20.0, 20.0, 10, 0, "Fine tunes texture mapping X coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dY",		100,30,100,19, mtex->ofs+1, -20.0, 20.0, 10, 0, "Fine tunes texture mapping Y coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dZ",		100,10,100,19, mtex->ofs+2, -20.0, 20.0, 10, 0, "Fine tunes texture mapping Z coordinate");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	200,50,100,19, mtex->size, -10.0, 10.0, 10, 0, "Sets scaling for the texture's X size");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	200,30,100,19, mtex->size+1, -10.0, 10.0, 10, 0, "Sets scaling for the texture's Y size");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	200,10,100,19, mtex->size+2, -10.0, 10.0, 10, 0, "Sets scaling for the texture's Z size");
	
}

static void world_panel_mistaph(World *wrld)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "world_panel_mistaph", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Mist / Stars / Physics", "World", 640, 0, 318, 204)==0) return;

	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");

#if GAMEBLENDER == 1
	uiDefButI(block, MENU, 1, 
			  "Physics %t|None %x0|Sumo %x2",	
			  10,180,140,19, &wrld->physicsEngine, 0, 0, 0, 0, 
			  "Physics Engine");
	
	/* Gravitation for the game worlds */
	uiDefButF(block, NUMSLI,0, "Grav ", 150,180,150,19,	&(wrld->gravity), 0.0, 25.0, 0, 0,  "Sets the gravitation constant of the game world");
#endif

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|0,REDRAWVIEW3D,"Mist",	10,120,140,19, &wrld->mode, 0, 0, 0, 0, "Toggles mist simulation");
	uiBlockSetCol(block, TH_AUTO);

	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_DIFF, "Qua", 10, 90, 40, 19, &wrld->mistype, 1.0, 0.0, 0, 0, "Mist uses quadratic progression");
	uiDefButS(block, ROW, B_DIFF, "Lin", 50, 90, 50, 19, &wrld->mistype, 1.0, 1.0, 0, 0, "Mist uses linear progression");
	uiDefButS(block, ROW, B_DIFF, "Sqr", 100, 90, 50, 19, &wrld->mistype, 1.0, 2.0, 0, 0, "Mist uses inverse quadratic progression");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,REDRAWVIEW3D, "Sta:",10,70,140,19, &wrld->miststa, 0.0, 1000.0, 10, 0, "Specifies the starting distance of the mist");
	uiDefButF(block, NUM,REDRAWVIEW3D, "Di:",10,50,140,19, &wrld->mistdist, 0.0,1000.0, 10, 00, "Specifies the depth of the mist");
	uiDefButF(block, NUM,B_DIFF,"Hi:",		10,30,140,19, &wrld->misthi,0.0,100.0, 10, 0, "Specifies the factor for a less dense mist with increasing height");
	uiDefButF(block, NUMSLI, 0, "Misi",		10,10,140,19,	&(wrld->misi), 0., 1.0, 0, 0, "Sets the mist intensity");
	uiBlockEndAlign(block);

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|1,B_DIFF,	"Stars",160,120,140,19, &wrld->mode, 0, 0, 0, 0, "Toggles starfield generation");
	uiBlockSetCol(block, TH_AUTO);
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM,B_DIFF,"StarDist:",	160,70,140,19, &(wrld->stardist), 2.0, 1000.0, 100, 0, "Specifies the average distance between any two stars");
	uiDefButF(block, NUM,B_DIFF,"MinDist:",		160,50,140,19, &(wrld->starmindist), 0.0, 1000.0, 100, 0, "Specifies the minimum distance to the camera for stars");
	uiDefButF(block, NUMSLI,B_DIFF,"Size:",		160,30,140,19, &(wrld->starsize), 0.0, 10.0, 10, 0, "Specifies the average screen dimension of stars");
	uiDefButF(block, NUMSLI,B_DIFF,"Colnoise:",	160,10,140,19, &(wrld->starcolnoise), 0.0, 1.0, 100, 0, "Randomizes starcolour");
	uiBlockEndAlign(block);

}

static void world_panel_amb_occ(World *wrld)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "world_panel_amb_oc", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Mist / Stars / Physics", "World");
	if(uiNewPanel(curarea, block, "Amb Occ", "World", 320, 0, 318, 204)==0) return;

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|4,B_REDR,	"Ambient Occlusion",10,150,300,19, &wrld->mode, 0, 0, 0, 0, "Toggles starfield generation");
	uiBlockSetCol(block, TH_AUTO);

	if(wrld->mode & WO_AMB_OCC) {

		/* aolight: samples */
		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, 0, "Samples:", 10, 120, 150, 19, &wrld->aosamp, 1.0, 16.0, 100, 0, "Sets the number of samples used for AO  (actual number: squared)");
		/* enable/disable total random sampling */
		uiDefButS(block, TOG|BIT|1, 0, "Random Sampling", 160, 120, 150, 19, &wrld->aomode, 0, 0, 0, 0, "When enabled, total random sampling will be used for an even noisier effect");
		uiBlockEndAlign(block);

		uiDefButF(block, NUM, 0, "Dist:", 10, 95, 150, 19, &wrld->aodist, 0.001, 5000.0, 100, 0, "Sets length of AO rays, defines how far away other faces give occlusion effect");

		uiBlockBeginAlign(block);
		uiDefButS(block, TOG|BIT|0, B_REDR, "Use Distances", 10, 70, 150, 19, &wrld->aomode, 0, 0, 0, 0, "When enabled, distances to objects will be used to attenuate shadows");
		/* distance attenuation factor */
		if (wrld->aomode & WO_AODIST)
			uiDefButF(block, NUM, 0, "DistF:", 160, 70, 150, 19, &wrld->aodistfac, 0.00001, 10.0, 100, 0, "Distance factor, the higher, the 'shorter' the shadows");

		/* result mix modes */
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_REDR, "Add", 10, 45, 100, 20, &wrld->aomix, 1.0, (float)WO_AOADD, 0, 0, "adds light/shadows");
		uiDefButS(block, ROW, B_REDR, "Sub", 110, 45, 100, 20, &wrld->aomix, 1.0, (float)WO_AOSUB, 0, 0, "subtracts light/shadows (needs at least one normal light to make anything visible)");
		uiDefButS(block, ROW, B_REDR, "Both", 210, 45, 100, 20, &wrld->aomix, 1.0, (float)WO_AOADDSUB, 0, 0, "both lightens & darkens");

		/* color treatment */
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, B_REDR, "Plain", 10, 25, 100, 20, &wrld->aocolor, 2.0, (float)WO_AOPLAIN, 0, 0, "Plain diffuse energy (white)");
		uiDefButS(block, ROW, B_REDR, "Sky Color", 110, 25, 100, 20, &wrld->aocolor, 2.0, (float)WO_AOSKYCOL, 0, 0, "Use horizon and zenith color for diffuse energy");
		uiDefButS(block, ROW, B_REDR, "Sky Texture", 210, 25, 100, 20, &wrld->aocolor, 2.0, (float)WO_AOSKYTEX, 0, 0, "Does full Sky texture render for diffuse energy");
		uiBlockEndAlign(block);
		
		uiDefButF(block, NUMSLI, 0, "Energy:", 10, 0, 300, 19, &wrld->aoenergy, 0.01, 3.0, 100, 0, "Sets global energy scale for AO");
	}

}

static void world_panel_world(World *wrld)
{
	uiBlock *block;
	ID *id, *idfrom;
	
	block= uiNewBlock(&curarea->uiblocks, "world_panel_world", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "World", "World", 320, 0, 318, 204)==0) return;

	/* first do the browse but */
	buttons_active_id(&id, &idfrom);

	uiBlockSetCol(block, TH_BUT_SETTING2);
	std_libbuttons(block, 10, 180, 0, NULL, B_WORLDBROWSE, id, idfrom, &(G.buts->menunr), B_WORLDALONE, B_WORLDLOCAL, B_WORLDDELETE, 0, B_KEEPDATA);

	if(wrld==NULL) return;
	
	uiSetButLock(wrld->id.lib!=0, "Can't edit library data");
	uiBlockSetCol(block, TH_AUTO);

	uiDefButF(block, COL, B_COLHOR, "",			10,150,145,19, &wrld->horr, 0, 0, 0, 0, "");
	uiDefButF(block, COL, B_COLZEN, "",			160,150,145,19, &wrld->zenr, 0, 0, 0, 0, "");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"HoR ",	10,130,145,19,	&(wrld->horr), 0.0, 1.0, B_COLHOR,0, "Sets the amount of red colour at the horizon");
	uiDefButF(block, NUMSLI,B_MATPRV,"HoG ",	10,110,145,19,	&(wrld->horg), 0.0, 1.0, B_COLHOR,0, "Sets the amount of green colour at the horizon");
	uiDefButF(block, NUMSLI,B_MATPRV,"HoB ",	10,90,145,19,	&(wrld->horb), 0.0, 1.0, B_COLHOR,0, "Sets the amount of blue colour at the horizon");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeR ",	160,130,145,19,	&(wrld->zenr), 0.0, 1.0, B_COLZEN,0, "Sets the amount of red colour at the zenith");
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeG ",	160,110,145,19,	&(wrld->zeng), 0.0, 1.0, B_COLZEN,0, "Sets the amount of green colour at the zenith");
	uiDefButF(block, NUMSLI,B_MATPRV,"ZeB ",	160,90,145,19,	&(wrld->zenb), 0.0, 1.0, B_COLZEN,0, "Sets the amount of blue colour at the zenith");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbR ",	10,50,145,19,	&(wrld->ambr), 0.0, 1.0 ,0,0, "Sets the amount of red ambient colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbG ",	10,30,145,19,	&(wrld->ambg), 0.0, 1.0 ,0,0, "Sets the amount of green ambient colour");
	uiDefButF(block, NUMSLI,B_MATPRV,"AmbB ",	10,10,145,19,	&(wrld->ambb), 0.0, 1.0 ,0,0, "Sets the amount of blue ambient colour");

	uiBlockBeginAlign(block);
	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButF(block, NUMSLI,0, "Exp ",			160,30,145,19,	&(wrld->exp), 0.0, 1.0, 0, 2, "Sets amount of exponential color correction for light");
	uiDefButF(block, NUMSLI,0, "Range ",		160,10,145,19,	&(wrld->range), 0.2, 5.0, 0, 2, "Sets the color amount that will be mapped on color 1.0");


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
	uiDefButS(block, TOG|BIT|1,B_MATPRV,"Real",	200,175,80,25, &wrld->skytype, 0, 0, 0, 0, "Renders background with a real horizon");
	uiDefButS(block, TOG|BIT|0,B_MATPRV,"Blend",200,150,80,25, &wrld->skytype, 0, 0, 0, 0, "Renders background with natural progression from horizon to zenith");
	uiDefButS(block, TOG|BIT|2,B_MATPRV,"Paper",200,125,80,25, &wrld->skytype, 0, 0, 0, 0, "Flattens blend or texture coordinates");
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
		allqueue(REDRAWBUTSSHADING, 0);
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
		la= G.buts->lockpoin;
		la->bufsize = la->bufsize&=(~15); 
		allqueue(REDRAWBUTSSHADING, 0); 
		allqueue(REDRAWOOPS, 0); 
		break; 
	case B_SHADBUF:
		la= G.buts->lockpoin; 
		la->mode &= ~LA_SHAD_RAY;
		allqueue(REDRAWBUTSSHADING, 0); 
		allqueue(REDRAWVIEW3D, 0); 		
		break;
	case B_SHADRAY:
		la= G.buts->lockpoin; 
		la->mode &= ~LA_SHAD;
		allqueue(REDRAWBUTSSHADING, 0);
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
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	920,130,52,19, &(mtex->texflag), 0, 0, 0, 0, "Sets the texture mapping to stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		974,130,38,19, &(mtex->texflag), 0, 0, 0, 0, "Inverts the values of the texture to reverse its effect");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "RGBtoInt",	1014,130,69,19, &(mtex->texflag), 0, 0, 0, 0, "Converts texture RGB values to intensity (gray) values");
	uiBlockEndAlign(block);
	
	uiDefButF(block, COL, B_MTEXCOL, "",				920,105,163,19, &(mtex->r), 0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "R ",			920,80,163,19, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of red the intensity texture blends");
	uiDefButF(block, NUMSLI, B_MATPRV, "G ",			920,60,163,19, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of green the intensity texture blends");
	uiDefButF(block, NUMSLI, B_MATPRV, "B ",			920,40,163,19, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of blue the intensity texture blends");
	uiBlockEndAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		920,10,163,19, &(mtex->def_var), 0.0, 1.0, 0, 0, "Sets the amount the texture blends with the basic value");
	
	/* MAP TO */
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Col",		920,180,81,19, &(mtex->mapto), 0, 0, 0, 0, "Lets the texture affect the basic colour of the lamp");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Mix",			1087,130,48,19, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "Sets texture to blend the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,130,44,19, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "Sets texture to multiply the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,130,41,19, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "Sets texture to add the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,130,40,19, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "Sets texture to subtract the values or colour");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,50,179,19, &(mtex->colfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects colour values");
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,30,179,19, &(mtex->norfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects normal values");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,10,179,19, &(mtex->varfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects other values");
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
		uiDefButS(block, ROW, B_REDR, str,	10, 160-20*a, 80, 20, &(la->texact), 3.0, (float)a, 0, 0, "");
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
	uiDefButS(block, MENU, B_LTEXBROWSE, strp, 100,140,20,19, &(G.buts->texnr), 0, 0, 0, 0, "Selects an existing texture or creates new");	
	MEM_freeN(strp);
	
	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	100,160,200,19, id->name+2, 0.0, 18.0, 0, 0, "Displays name of the texture block: click to change");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,			196,140,21,19, 0, 0, 0, 0, 0, "Displays number of users of texture: click to make single user");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 241,140,21,19, 0, 0, 0, 0, 0, "Auto-assigns name to texture");
		if(id->lib) {
			if(la->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	219,140,21,19, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	219,140,21,19, 0, 0, 0, 0, 0, "");	
		}
		uiBlockSetCol(block, TH_AUTO);
		uiDefBut(block, BUT, B_TEXCLEARLAMP, "Clear", 122, 140, 72, 19, 0, 0, 0, 0, 0, "Erases link to texture");
	}
	else 
		uiDefButS(block, TOG, B_LTEXBROWSE, "Add New" ,100, 160, 200, 19, &(G.buts->texnr), -1.0, 32767.0, 0, 0, "Adds a new texture datablock");

	/* TEXCO */
	uiBlockSetCol(block, TH_AUTO);
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Glob",			100,110,60,20, &(mtex->texco), 4.0, (float)TEXCO_GLOB, 0, 0, "Uses global coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "View",			160,110,70,20, &(mtex->texco), 4.0, (float)TEXCO_VIEW, 0, 0, "Uses view coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Object",		230,110,70,20, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "Uses linked object's coordinates for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "", 100,90,200,20, &(mtex->object), "");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "dX",		100,50,100,18, mtex->ofs, -20.0, 20.0, 10, 0, "Fine tunes texture mapping X coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dY",		100,30,100,18, mtex->ofs+1, -20.0, 20.0, 10, 0, "Fine tunes texture mapping Y coordinate");
	uiDefButF(block, NUM, B_MATPRV, "dZ",		100,10,100,18, mtex->ofs+2, -20.0, 20.0, 10, 0, "Fine tunes texture mapping Z coordinate");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	200,50,100,18, mtex->size, -10.0, 10.0, 10, 0, "Sets scaling for the texture's X size");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	200,30,100,18, mtex->size+1, -10.0, 10.0, 10, 0, "Sets scaling for the texture's Y size");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	200,10,100,18, mtex->size+2, -10.0, 10.0, 10, 0, "Sets scaling for the texture's Z size");
	uiBlockEndAlign(block);
}

static void lamp_panel_spot(Object *ob, Lamp *la)
{
	uiBlock *block;
	float grid=0.0;
	
	block= uiNewBlock(&curarea->uiblocks, "lamp_panel_spot", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Shadow and Spot", "Lamp", 640, 0, 318, 204)==0) return;

	// hemis and ray shadow dont work at all...
	if(la->type==LA_HEMI) return;

	if(G.vd) grid= G.vd->grid; 
	if(grid<1.0) grid= 1.0;

	uiSetButLock(la->id.lib!=0, "Can't edit library data");

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|13, B_SHADRAY,"Ray Shadow",10,180,80,19,&la->mode, 0, 0, 0, 0, "Use ray tracing for shadow");
	if(la->type==LA_SPOT) 
		uiDefButS(block, TOG|BIT|0, B_SHADBUF, "Buf.Shadow",10,160,80,19,&la->mode, 0, 0, 0, 0, "Lets spotlight produce shadows using shadow buffer");
	uiBlockEndAlign(block);
	
	uiDefButS(block, TOG|BIT|5, 0,"OnlyShadow",			10,110,80,19,&la->mode, 0, 0, 0, 0, "Causes light to cast shadows only without illuminating objects");

	if(la->type==LA_SPOT) {
		uiDefButS(block, TOG|BIT|7, B_LAMPREDRAW,"Square",	10,70,80,19,&la->mode, 0, 0, 0, 0, "Sets square spotbundles");
		uiDefButS(block, TOG|BIT|1, 0,"Halo",				10,50,80,19,&la->mode, 0, 0, 0, 0, "Renders spotlight with a volumetric halo"); 

		uiBlockSetCol(block, TH_AUTO);
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI,B_LAMPREDRAW,"SpotSi ",	100,180,200,19,&la->spotsize, 1.0, 180.0, 0, 0, "Sets the angle of the spotlight beam in degrees");
		uiDefButF(block, NUMSLI,B_MATPRV,"SpotBl ",		100,160,200,19,&la->spotblend, 0.0, 1.0, 0, 0, "Sets the softness of the spotlight edge");
		uiBlockEndAlign(block);
	
		uiDefButF(block, NUMSLI,0,"HaloInt ",			100,135,200,19,&la->haint, 0.0, 5.0, 0, 0, "Sets the intensity of the spotlight halo");
		
		if(la->mode & LA_SHAD) {
			uiDefButS(block, NUM,B_SBUFF,"ShadowBufferSize:", 100,110,200,19,	&la->bufsize,512,5120, 0, 0, "Sets the size of the shadow buffer to nearest multiple of 16");
		
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM,REDRAWVIEW3D,"ClipSta:",	100,70,100,19,	&la->clipsta, 0.1*grid,1000.0*grid, 10, 0, "Sets the shadow map clip start: objects closer will not generate shadows");
			uiDefButF(block, NUM,REDRAWVIEW3D,"ClipEnd:",	200,70,100,19,&la->clipend, 1.0, 5000.0*grid, 100, 0, "Sets the shadow map clip end beyond which objects will not generate shadows");
			uiBlockEndAlign(block);
			
			uiDefButS(block, NUM,0,"Samples:",		100,30,100,19,	&la->samp,1.0,16.0, 0, 0, "Sets the number of shadow map samples");
			uiDefButS(block, NUM,0,"Halo step:",	200,30,100,19,	&la->shadhalostep, 0.0, 12.0, 0, 0, "Sets the volumetric halo sampling frequency");
			uiDefButF(block, NUM,0,"Bias:",			100,10,100,19,	&la->bias, 0.01, 5.0, 1, 0, "Sets the shadow map sampling bias");
			uiDefButF(block, NUM,0,"Soft:",			200,10,100,19,	&la->soft,1.0,100.0, 100, 0, "Sets the size of the shadow sample area");
		}
	}
	else if(la->type==LA_AREA && (la->mode & LA_SHAD_RAY)) {
		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_AUTO);
		if(la->area_shape==LA_AREA_SQUARE) 
			uiDefButS(block, NUM,0,"Samples:",	100,180,200,19,	&la->ray_samp, 1.0, 16.0, 100, 0, "Sets the amount of samples taken extra (samp x samp)");
		if(la->area_shape==LA_AREA_CUBE) 
			uiDefButS(block, NUM,0,"Samples:",	100,160,200,19,	&la->ray_samp, 1.0, 16.0, 100, 0, "Sets the amount of samples taken extra (samp x samp x samp)");

		if ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_BOX) {
			uiDefButS(block, NUM,0,"SamplesX:",	100,180,200,19,	&la->ray_samp, 1.0, 16.0, 100, 0, "Sets the amount of X samples taken extra");
			uiDefButS(block, NUM,0,"SamplesY:",	100,160,200,19,	&la->ray_sampy, 1.0, 16.0, 100, 0, "Sets the amount of Y samples taken extra");
			if(la->area_shape==LA_AREA_BOX)
				uiDefButS(block, NUM,0,"SamplesZ:",	100,140,200,19,	&la->ray_sampz, 1.0, 8.0, 100, 0, "Sets the amount of Z samples taken extra");
		}
		
		uiBlockBeginAlign(block);
		uiDefButS(block, TOG|BIT|1, 0,"Umbra",			100,110,200,19,&la->ray_samp_type, 0, 0, 0, 0, "Emphasis parts that are fully shadowed");
		uiDefButS(block, TOG|BIT|2, 0,"Dither",			100,90,100,19,&la->ray_samp_type, 0, 0, 0, 0, "Use 2x2 dithering for sampling");
		uiDefButS(block, TOG|BIT|3, 0,"Noise",			200,90,100,19,&la->ray_samp_type, 0, 0, 0, 0, "Use noise for sampling");
	}
	else uiDefBut(block, LABEL,0," ",	100,180,200,19,NULL, 0, 0, 0, 0, "");

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
	uiDefButF(block, NUM,B_LAMPREDRAW,"Dist:", xco,180,300-xco,20,&la->dist, 0.01, 5000.0*grid, 100, 0, "Sets the distance value at which light intensity is half");

	uiBlockBeginAlign(block);
	if(la->type==LA_AREA) {
		//uiDefButS(block, MENU, B_LAMPREDRAW, "Shape %t|Square %x0|Rect %x1|Cube %x2|Box %x3",
		uiDefButS(block, MENU, B_LAMPREDRAW, "Shape %t|Square %x0|Rect %x1",
				10, 150, 100, 19, &la->area_shape, 0,0,0,0, "Sets area light shape");	
		if ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_BOX){
			uiDefButF(block, NUM,B_LAMPREDRAW,"SizeX ",	10,130,100,19, &la->area_size, 0.01, 100.0, 10, 0, "Area light size X, doesn't affect energy amount");
			uiDefButF(block, NUM,B_LAMPREDRAW,"SizeY ",	10,110,100,19, &la->area_sizey, 0.01, 100.0, 10, 0, "Area light size Y, doesn't affect energy amount");
		}
		if(la->area_shape==LA_AREA_BOX)
			uiDefButF(block, NUM,B_LAMPREDRAW,"SizeZ ",	10,90,100,19, &la->area_sizez, 0.01, 100.0, 10, 0, "Area light size Z, doesn't affect energy amount");
		if ELEM(la->area_shape, LA_AREA_SQUARE, LA_AREA_CUBE) 
			uiDefButF(block, NUM,B_LAMPREDRAW,"Size ",	10,130,100,19, &la->area_size, 0.01, 100.0, 10, 0, "Area light size, doesn't affect energy amount");
	}
	else if ELEM(la->type, LA_LOCAL, LA_SPOT) {
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButS(block, TOG|BIT|3, B_MATPRV,"Quad",		10,150,100,19,&la->mode, 0, 0, 0, 0, "Uses inverse quadratic proportion for light attenuation");
		uiDefButS(block, TOG|BIT|6, REDRAWVIEW3D,"Sphere",	10,130,100,19,&la->mode, 0, 0, 0, 0, "Sets light intensity to zero for objects beyond the distance value");
	}

	uiBlockBeginAlign(block);
	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButS(block, TOG|BIT|2, 0,"Layer",				10,70,100,19,&la->mode, 0, 0, 0, 0, "Illuminates objects in the same layer as the lamp only");
	uiDefButS(block, TOG|BIT|4, B_MATPRV,"Negative",	10,50,100,19,&la->mode, 0, 0, 0, 0, "Sets lamp to cast negative light");
	uiDefButS(block, TOG|BIT|11, 0,"No Diffuse",		10,30,100,19,&la->mode, 0, 0, 0, 0, "Disables diffuse shading of material illuminated by this lamp");
	uiDefButS(block, TOG|BIT|12, 0,"No Specular",		10,10,100,19,&la->mode, 0, 0, 0, 0, "Disables specular shading of material illuminated by this lamp");
	uiBlockEndAlign(block);

	uiBlockSetCol(block, TH_AUTO);
	uiDefButF(block, NUMSLI,B_MATPRV,"Energy ",	120,150,180,20, &(la->energy), 0.0, 10.0, 0, 0, "Sets the intensity of the light");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI,B_MATPRV,"R ",		120,120,180,20,&la->r, 0.0, 1.0, B_COLLAMP, 0, "Sets the red component of the light");
	uiDefButF(block, NUMSLI,B_MATPRV,"G ",		120,100,180,20,&la->g, 0.0, 1.0, B_COLLAMP, 0, "Sets the green component of the light");
	uiDefButF(block, NUMSLI,B_MATPRV,"B ",		120,80,180,20,&la->b, 0.0, 1.0, B_COLLAMP, 0, "Sets the blue component of the light");
	uiBlockEndAlign(block);
	
	uiDefButF(block, COL, B_COLLAMP, "",		120,52,180,24, &la->r, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	if ELEM(la->type, LA_LOCAL, LA_SPOT) {
		uiDefButF(block, NUMSLI,B_MATPRV,"Quad1 ",	120,30,180,19,&la->att1, 0.0, 1.0, 0, 0, "Set the linear distance attenuatation for a quad lamp");
		uiDefButF(block, NUMSLI,B_MATPRV,"Quad2 ",  120,10,180,19,&la->att2, 0.0, 1.0, 0, 0, "Set the qudratic distance attenuatation for a quad lamp");
	}
	else if(la->type==LA_AREA) {
		if(la->k==0.0) la->k= 1.0;
		uiDefButF(block, NUMSLI,0,"Gamma ",	120,10,180,19,&la->k, 0.001, 2.0, 100, 0, "Set the light gamma correction value");
	}
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
	uiDefButS(block, ROW,B_LAMPREDRAW,"Lamp",	200,175,80,25,&la->type,1.0,(float)LA_LOCAL, 0, 0, "Creates an omnidirectional point light source");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Area",	200,150,80,25,&la->type,1.0,(float)LA_AREA, 0, 0, "Creates a directional area light source");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Spot",	200,125,80,25,&la->type,1.0,(float)LA_SPOT, 0, 0, "Creates a directional cone light source");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Sun",	200,100,80,25,&la->type,1.0,(float)LA_SUN, 0, 0, "Creates a constant direction parallel ray light source");
	uiDefButS(block, ROW,B_LAMPREDRAW,"Hemi",	200,75,80,25,&la->type,1.0,(float)LA_HEMI, 0, 0, "Creates a 180 degree constant light source");
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
	case B_MATHALO:
		/* when halo is disabled, clear star flag, this is the same as MA_FACETEXTURE <blush> */
		ma= G.buts->lockpoin;
		if((ma->mode & MA_HALO)==0) ma->mode &= ~MA_STAR;
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
		break;
	case B_MATZTRANSP:
		ma= G.buts->lockpoin;
		if(ma) {
			ma->mode &= ~MA_RAYTRANSP;
			allqueue(REDRAWBUTSSHADING, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	case B_MATRAYTRANSP:
		ma= G.buts->lockpoin;
		if(ma) {
			ma->mode &= ~MA_ZTRA;
			allqueue(REDRAWBUTSSHADING, 0);
			BIF_preview_changed(G.buts);
		}
		break;
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
	uiDefButS(block, TOG|BIT|1, B_MATPRV, "Stencil",	900,120,54,19, &(mtex->texflag), 0, 0, 0, 0, "Sets the texture mapping to stencil mode");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Neg",		956,120,39,19, &(mtex->texflag), 0, 0, 0, 0, "Inverts the values of the texture to reverse its effect");
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "No RGB",		997,120,71,19, &(mtex->texflag), 0, 0, 0, 0, "Converts texture RGB values to intensity (gray) values");
	uiBlockEndAlign(block);

	uiDefButF(block, COL, B_MTEXCOL, "",				900,100,168,18, &(mtex->r), 0, 0, 0, 0, "Browses existing datablocks");
	
	uiBlockBeginAlign(block);
	if(ma->colormodel==MA_HSV) {
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButF(block, HSVSLI, B_MATPRV, "H ",			900,80,168,19, &(mtex->r), 0.0, 0.9999, B_MTEXCOL, 0, "");
		uiDefButF(block, HSVSLI, B_MATPRV, "S ",			900,60,168,19, &(mtex->r), 0.0001, 1.0, B_MTEXCOL, 0, "");
		uiDefButF(block, HSVSLI, B_MATPRV, "V ",			900,40,168,19, &(mtex->r), 0.0001, 1.0, B_MTEXCOL, 0, "");
		uiBlockSetCol(block, TH_AUTO);
	}
	else {
		uiDefButF(block, NUMSLI, B_MATPRV, "R ",			900,80,168,19, &(mtex->r), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of red the intensity texture blends");
		uiDefButF(block, NUMSLI, B_MATPRV, "G ",			900,60,168,19, &(mtex->g), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of green the intensity texture blends");
		uiDefButF(block, NUMSLI, B_MATPRV, "B ",			900,40,168,19, &(mtex->b), 0.0, 1.0, B_MTEXCOL, 0, "Sets the amount of blue the intensity texture blends");
	}
	uiBlockEndAlign(block);
	
	uiDefButF(block, NUMSLI, B_MATPRV, "DVar ",		900,10,168,19, &(mtex->def_var), 0.0, 1.0, 0, 0, "Sets the amount the texture blends with the basic value");
	
	/* MAP TO */
	uiBlockBeginAlign(block);
	uiDefButS(block, TOG|BIT|0, B_MATPRV, "Col",	900,180,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect basic colour of the material");
	uiDefButS(block, TOG3|BIT|1, B_MATPRV, "Nor",	960,180,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the rendered normal");
	uiDefButS(block, TOG|BIT|2, B_MATPRV, "Csp",	1020,180,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the specularity colour");
	uiDefButS(block, TOG|BIT|3, B_MATPRV, "Cmir",	1080,180,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affext the mirror colour");
	uiDefButS(block, TOG3|BIT|4, B_MATPRV, "Ref",	1140,180,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the value of the materials reflectivity");
	uiDefButS(block, TOG3|BIT|5, B_MATPRV, "Spec",	1200,180,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the value of specularity");
	
	uiDefButS(block, TOG3|BIT|8, B_MATPRV, "Hard",	900,160,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the hardness value");
	uiDefButS(block, TOG3|BIT|9, B_MATPRV, "RayMir",960,160,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the ray-mirror value");
	uiDefButS(block, TOG3|BIT|7, B_MATPRV, "Alpha",	1020,160,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the alpha value");
	uiDefButS(block, TOG3|BIT|6, B_MATPRV, "Emit",	1080,160,60,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the emit value");
	uiDefButS(block, TOG3|BIT|10, B_MATPRV, "Translu",1140,160,65,19, &(mtex->mapto), 0, 0, 0, 0, "Causes the texture to affect the translucency value");
	uiDefButS(block, TOG3|BIT|12, B_MATPRV, "Disp",	1205,160,55,19, &(mtex->mapto), 0, 0, 0, 0, "Let the texture displace the surface");
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, B_MATPRV, "Mix",			1087,120,48,18, &(mtex->blendtype), 9.0, (float)MTEX_BLEND, 0, 0, "Sets texture to blend the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Mul",			1136,120,44,18, &(mtex->blendtype), 9.0, (float)MTEX_MUL, 0, 0, "Sets texture to multiply the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Add",			1182,120,41,18, &(mtex->blendtype), 9.0, (float)MTEX_ADD, 0, 0, "Sets texture to add the values or colour");
	uiDefButS(block, ROW, B_MATPRV, "Sub",			1226,120,40,18, &(mtex->blendtype), 9.0, (float)MTEX_SUB, 0, 0, "Sets texture to subtract the values or colour");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "Col ",		1087,70,179,18, &(mtex->colfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects colour values");
	/* newnoise: increased range to 25, the constant offset for bumpmapping quite often needs a higher nor setting */
	uiDefButF(block, NUMSLI, B_MATPRV, "Nor ",		1087,50,179,18, &(mtex->norfac), 0.0, 25.0, 0, 0, "Sets the amount the texture affects normal values");
	uiDefButF(block, NUMSLI, B_MATPRV, "Var ",		1087,30,179,18, &(mtex->varfac), 0.0, 1.0, 0, 0, "Sets the amount the texture affects other values");
	uiDefButF(block, NUMSLI, B_MATPRV, "Disp ",		1087,10,179,19, &(mtex->dispfac), 0.0, 1.0, 0, 0, "Sets the amount the texture displaces the surface");
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
	uiDefButS(block, ROW, B_MATPRV, "UV",			630,166,40,18, &(mtex->texco), 4.0, (float)TEXCO_UV, 0, 0, "Uses UV coordinates for texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Object",		670,166,75,18, &(mtex->texco), 4.0, (float)TEXCO_OBJECT, 0, 0, "Uses linked object's coordinates for texture coordinates");
	uiDefIDPoinBut(block, test_obpoin_but, B_MATPRV, "",745,166,163,18, &(mtex->object), "");
	
	uiDefButS(block, ROW, B_MATPRV, "Glob",			630,146,45,18, &(mtex->texco), 4.0, (float)TEXCO_GLOB, 0, 0, "Uses global coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Orco",			675,146,50,18, &(mtex->texco), 4.0, (float)TEXCO_ORCO, 0, 0, "Uses the original coordinates of the mesh");
	uiDefButS(block, ROW, B_MATPRV, "Stick",		725,146,50,18, &(mtex->texco), 4.0, (float)TEXCO_STICKY, 0, 0, "Uses mesh's sticky coordinates for the texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Win",			775,146,45,18, &(mtex->texco), 4.0, (float)TEXCO_WINDOW, 0, 0, "Uses screen coordinates as texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Nor",			820,146,44,18, &(mtex->texco), 4.0, (float)TEXCO_NORM, 0, 0, "Uses normal vector as texture coordinates");
	uiDefButS(block, ROW, B_MATPRV, "Refl",			864,146,44,18, &(mtex->texco), 4.0, (float)TEXCO_REFL, 0, 0, "Uses reflection vector as texture coordinates");

	/* COORDS */
	uiBlockBeginAlign(block);
	uiDefButC(block, ROW, B_MATPRV, "Flat",			630,114,48,18, &(mtex->mapping), 5.0, (float)MTEX_FLAT, 0, 0, "Maps X and Y coordinates directly");
	uiDefButC(block, ROW, B_MATPRV, "Cube",			681,114,50,18, &(mtex->mapping), 5.0, (float)MTEX_CUBE, 0, 0, "Maps using the normal vector");
	uiDefButC(block, ROW, B_MATPRV, "Tube",			630,94,48,18, &(mtex->mapping), 5.0, (float)MTEX_TUBE, 0, 0, "Maps with Z as central axis (tube-like)");
	uiDefButC(block, ROW, B_MATPRV, "Sphe",			681,94,50,18, &(mtex->mapping), 5.0, (float)MTEX_SPHERE, 0, 0, "Maps with Z as central axis (sphere-like)");

	uiBlockBeginAlign(block);
	for(b=0; b<3; b++) {
		char *cp;
		if(b==0) cp= &(mtex->projx);
		else if(b==1) cp= &(mtex->projy);
		else cp= &(mtex->projz);
		
		uiDefButC(block, ROW, B_MATPRV, "",			630, 50-20*b, 24, 18, cp, 6.0+b, 0.0, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, "X",		656, 50-20*b, 24, 18, cp, 6.0+b, 1.0, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, "Y",		682, 50-20*b, 24, 18, cp, 6.0+b, 2.0, 0, 0, "");
		uiDefButC(block, ROW, B_MATPRV, "Z",		708, 50-20*b, 24, 18, cp, 6.0+b, 3.0, 0, 0, "");
	}
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "ofsX",		778,114,130,18, mtex->ofs, -10.0, 10.0, 10, 0, "Fine tunes texture mapping X coordinate");
	uiDefButF(block, NUM, B_MATPRV, "ofsY",		778,94,130,18, mtex->ofs+1, -10.0, 10.0, 10, 0, "Fine tunes texture mapping Y coordinate");
	uiDefButF(block, NUM, B_MATPRV, "ofsZ",		778,74,130,18, mtex->ofs+2, -10.0, 10.0, 10, 0, "Fine tunes texture mapping Z coordinate");
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, B_MATPRV, "sizeX",	778,50,130,18, mtex->size, -100.0, 100.0, 10, 0, "Sets scaling for the texture's X size");
	uiDefButF(block, NUM, B_MATPRV, "sizeY",	778,30,130,18, mtex->size+1, -100.0, 100.0, 10, 0, "Sets scaling for the texture's Y size");
	uiDefButF(block, NUM, B_MATPRV, "sizeZ",	778,10,130,18, mtex->size+2, -100.0, 100.0, 10, 0, "Sets scaling for the texture's Z size");
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
		uiDefButC(block, ROW, B_MATPRV_DRAW, str,	10, 180-20*a, 70, 20, &(ma->texact), 3.0, (float)a, 0, 0, "");
	}
	uiBlockEndAlign(block);
	
	/* SEPTEX */
	uiBlockSetCol(block, TH_AUTO);
	
	for(a= 0; a<8; a++) {
		mtex= ma->mtex[a];
		if(mtex && mtex->tex) {
			if(ma->septex & (1<<a)) 
				uiDefButC(block, TOG|BIT|a, B_MATPRV_DRAW, " ",	-20, 180-20*a, 28, 20, &ma->septex, 0.0, 0.0, 0, 0, "Click to disable or enable this texture channel");
			else uiDefIconButC(block, TOG|BIT|a, B_MATPRV_DRAW, ICON_CHECKBOX_HLT,	-20, 180-20*a, 28, 20, &ma->septex, 0.0, 0.0, 0, 0, "Click to disable or enable this texture channel");
		}
	}
	
	uiDefIconBut(block, BUT, B_MTEXCOPY, ICON_COPYUP,	100,180,23,21, 0, 0, 0, 0, 0, "Copies the mapping settings to the buffer");
	uiDefIconBut(block, BUT, B_MTEXPASTE, ICON_PASTEUP,	125,180,23,21, 0, 0, 0, 0, 0, "Pastes the mapping settings from the buffer");

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
	uiDefButS(block, MENU, B_EXTEXBROWSE, strp, 100,130,20,20, &(G.buts->texnr), 0, 0, 0, 0, "Selects an existing texture or creates new");
	MEM_freeN(strp);

	if(id) {
		uiDefBut(block, TEX, B_IDNAME, "TE:",	100,150,163,20, id->name+2, 0.0, 18.0, 0, 0, "Displays name of the texture block: click to change");
		sprintf(str, "%d", id->us);
		uiDefBut(block, BUT, 0, str,				196,130,21,20, 0, 0, 0, 0, 0, "Displays number of users of texture");
		uiDefIconBut(block, BUT, B_AUTOTEXNAME, ICON_AUTO, 241,130,21,20, 0, 0, 0, 0, 0, "Auto-assigns name to texture");
		if(id->lib) {
			if(ma->id.lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,	219,130,21,20, 0, 0, 0, 0, 0, "");
			else uiDefIconBut(block, BUT, 0, ICON_PARLIB,	219,130,21,20, 0, 0, 0, 0, 0, "");		
		}
		uiBlockSetCol(block, TH_AUTO);
		uiDefBut(block, BUT, B_TEXCLEAR, "Clear", 122, 130, 72, 20, 0, 0, 0, 0, 0, "Erases link to texture");
	}
	else 
		uiDefButS(block, TOG, B_EXTEXBROWSE, "Add New" ,100, 150, 163, 20, &(G.buts->texnr), -1.0, 32767.0, 0, 0, "Adds a new texture datablock");
	
	// force no centering
	uiDefBut(block, LABEL, 0, " ", 250, 10, 25, 20, 0, 0, 0, 0, 0, "");
	
	uiBlockSetCol(block, TH_AUTO);
}

static void material_panel_tramir(Material *ma)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_tramir", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Shaders", "Material");
	if(uiNewPanel(curarea, block, "Mirror Transp", "Material", 640, 0, 318, 204)==0) return;

	uiDefButI(block, TOG|BIT|18, B_MATPRV,"Ray Mirror",210,180,100,20, &(ma->mode), 0, 0, 0, 0, "Enables raytracing for mirror reflection rendering");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "RayMir ",	10,160,200,20, &(ma->ray_mirror), 0.0, 1.0, 100, 2, "Sets the amount mirror reflection for raytrace");
	uiDefButS(block, NUM, B_MATPRV, "Depth:",		210,160,100,20, &(ma->ray_depth), 0.0, 10.0, 100, 0, "Amount of inter-reflections calculated maximal ");

	uiDefButF(block, NUMSLI, B_MATPRV, "Fresnel ",	10,140,160,20, &(ma->fresnel_mir), 0.0, 5.0, 10, 2, "Power of Fresnel for mirror reflection");
	uiDefButF(block, NUMSLI, B_MATPRV, "Fac ",		170,140,140,20, &(ma->fresnel_mir_i), 1.0, 5.0, 10, 2, "Blending factor for Fresnel");

	uiBlockBeginAlign(block);
	uiDefButI(block, TOG|BIT|6, B_MATZTRANSP,"ZTransp",	110,110,100,20, &(ma->mode), 0, 0, 0, 0, "Enables Z-Buffering of transparent faces");
	uiDefButI(block, TOG|BIT|17, B_MATRAYTRANSP,"Ray Transp",210,110,100,20, &(ma->mode), 0, 0, 0, 0, "Enables raytracing for transparency rendering");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "IOR ",	10,90,200,20, &(ma->ang), 1.0, 3.0, 100, 2, "Sets the angular index of refraction for raytrace");
	uiDefButS(block, NUM, B_MATPRV, "Depth:",	210,90,100,20, &(ma->ray_depth_tra), 0.0, 10.0, 100, 0, "Amount of refractions calculated maximal ");

	uiDefButF(block, NUMSLI, B_MATPRV, "Fresnel ",	10,70,160,20, &(ma->fresnel_tra), 0.0, 5.0, 10, 2, "Power of Fresnel for transparency");
	uiDefButF(block, NUMSLI, B_MATPRV, "Fac ",		170,70,140,20, &(ma->fresnel_tra_i), 1.0, 5.0, 10, 2, "Blending factor for Fresnel");

	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_MATPRV, "SpecTra ",	10,40,150,20, &(ma->spectra), 0.0, 1.0, 0, 0, "Makes specular areas opaque on transparent materials");
	uiDefButF(block, NUMSLI, B_MATPRV, "Add ",		160,40,150,20, &(ma->add), 0.0, 1.0, 0, 0, "Sets a glow factor for transparant materials");

	uiBlockBeginAlign(block);
	uiDefButI(block, TOG|BIT|10, 0,	"OnlyShadow",	10,10,100,20, &(ma->mode), 0, 0, 0, 0, "Renders shadows falling on material only");
	uiDefButI(block, TOG|BIT|14, 0,	"No Mist",		110,10,100,20, &(ma->mode), 0, 0, 0, 0, "Sets the material to ignore mist values");
	uiDefButI(block, TOG|BIT|9, 0,	"Env",			210,10,100,20, &(ma->mode), 0, 0, 0, 0, "Causes faces to render with alpha zero: allows sky/backdrop to show through");
	uiBlockEndAlign(block);


}

static void material_panel_shading(Material *ma)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_shading", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Shaders", "Material", 640, 0, 318, 204)==0) return;

	uiBlockSetCol(block, TH_BUT_SETTING1);
	uiDefButI(block, TOG|BIT|5, B_MATHALO, "Halo",	245,180,65,18, &(ma->mode), 0, 0, 0, 0, "Renders material as a halo");
	uiBlockSetCol(block, TH_AUTO);

	if(ma->mode & MA_HALO) {
		uiDefButF(block, NUM, B_MATPRV, "HaloSize: ",		10,155,190,18, &(ma->hasize), 0.0, 100.0, 10, 0, "Sets the dimension of the halo");
		uiDefButS(block, NUMSLI, B_MATPRV, "Hard ",			10,135,190,18, &(ma->har), 1.0, 127.0, 0, 0, "Sets the hardness of the halo");
		uiDefButF(block, NUMSLI, B_MATPRV, "Add  ",			10,115,190,18, &(ma->add), 0.0, 1.0, 0, 0, "Sets the strength of the add effect");
		
		uiDefButS(block, NUM, B_MATPRV, "Rings: ",			10,90,90,18, &(ma->ringc), 0.0, 24.0, 0, 0, "Sets the number of rings rendered over the halo");
		uiDefButS(block, NUM, B_MATPRV, "Lines: ",			100,90,100,18, &(ma->linec), 0.0, 250.0, 0, 0, "Sets the number of star shaped lines rendered over the halo");
		uiDefButS(block, NUM, B_MATPRV, "Star: ",			10,70,90,18, &(ma->starc), 3.0, 50.0, 0, 0, "Sets the number of points on the star shaped halo");
		uiDefButC(block, NUM, B_MATPRV, "Seed: ",			100,70,100,18, &(ma->seed1), 0.0, 255.0, 0, 0, "Randomizes ring dimension and line location");
		if(ma->mode & MA_HALO_FLARE) {
			uiDefButF(block, NUM, B_MATPRV, "FlareSize: ",		10,50,95,18, &(ma->flaresize), 0.1, 25.0, 10, 0, "Sets the factor by which the flare is larger than the halo");
			uiDefButF(block, NUM, B_MATPRV, "Sub Size: ",		100,50,100,18, &(ma->subsize), 0.1, 25.0, 10, 0, "Sets the dimension of the subflares, dots and circles");
			uiDefButF(block, NUMSLI, B_MATPRV, "Boost: ",		10,30,190,18, &(ma->flareboost), 0.1, 10.0, 10, 0, "Gives the flare extra strength");
			uiDefButC(block, NUM, B_MATPRV, "Fl.seed: ",		10,10,90,18, &(ma->seed2), 0.0, 255.0, 0, 0, "Specifies an offset in the flare seed table");
			uiDefButS(block, NUM, B_MATPRV, "Flares: ",			100,10,100,18, &(ma->flarec), 1.0, 32.0, 0, 0, "Sets the number of subflares");
		}
		uiBlockSetCol(block, TH_BUT_SETTING1);
		
		uiBlockBeginAlign(block);
		uiDefButI(block, TOG|BIT|15, B_MATPRV_DRAW, "Flare",245,142,65,28, &(ma->mode), 0, 0, 0, 0, "Renders halo as a lensflare");
		uiDefButI(block, TOG|BIT|8, B_MATPRV, "Rings",		245,123,65, 18, &(ma->mode), 0, 0, 0, 0, "Renders rings over halo");
		uiDefButI(block, TOG|BIT|9, B_MATPRV, "Lines",		245,104,65, 18, &(ma->mode), 0, 0, 0, 0, "Renders star shaped lines over halo");
		uiDefButI(block, TOG|BIT|11, B_MATPRV, "Star",		245,85,65, 18, &(ma->mode), 0, 0, 0, 0, "Renders halo as a star");
		uiDefButI(block, TOG|BIT|12, B_MATPRV, "HaloTex",	245,66,65, 18, &(ma->mode), 0, 0, 0, 0, "Gives halo a texture");
		uiDefButI(block, TOG|BIT|13, B_MATPRV, "HaloPuno",	245,47,65, 18, &(ma->mode), 0, 0, 0, 0, "Uses the vertex normal to specify the dimension of the halo");
		uiDefButI(block, TOG|BIT|10, B_MATPRV, "X Alpha",	245,28,65, 18, &(ma->mode), 0, 0, 0, 0, "Uses extreme alpha");
		uiDefButI(block, TOG|BIT|14, B_MATPRV, "Shaded",	245,9,65, 18, &(ma->mode), 0, 0, 0, 0, "Lets halo receive light and shadows");
		uiBlockEndAlign(block);
	}
	else {
		char *str1= "Diffuse Shader%t|Lambert %x0|Oren-Nayar %x1|Toon %x2";
		char *str2= "Specular Shader%t|CookTorr %x0|Phong %x1|Blinn %x2|Toon %x3";
		
		/* diff shader buttons */
		uiDefButS(block, MENU, B_MATPRV_DRAW, str1,		9, 180,78,19, &(ma->diff_shader), 0.0, 0.0, 0, 0, "Creates a diffuse shader");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_MATPRV, "Ref   ",	90,180,150,19, &(ma->ref), 0.0, 1.0, 0, 0, "Sets the amount of reflection");
		if(ma->diff_shader==MA_DIFF_ORENNAYAR)
			uiDefButF(block, NUMSLI, B_MATPRV, "Rough:",90,160, 150,19, &(ma->roughness), 0.0, 3.14, 0, 0, "Sets Oren Nayar Roughness");
		else if(ma->diff_shader==MA_DIFF_TOON) {
			uiDefButF(block, NUMSLI, B_MATPRV, "Size:",	90, 160,150,19, &(ma->param[0]), 0.0, 3.14, 0, 0, "Sets size of diffuse toon area");
			uiDefButF(block, NUMSLI, B_MATPRV, "Smooth:",90,140,150,19, &(ma->param[1]), 0.0, 1.0, 0, 0, "Sets smoothness of diffuse toon area");
		}
		uiBlockEndAlign(block);
		
		/* spec shader buttons */
		uiDefButS(block, MENU, B_MATPRV_DRAW, str2,		9,120,77,19, &(ma->spec_shader), 0.0, 0.0, 0, 0, "Creates a specular shader");
		
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_MATPRV, "Spec ",		90,120,150,19, &(ma->spec), 0.0, 2.0, 0, 0, "Sets the degree of specularity");
		if ELEM3(ma->spec_shader, MA_SPEC_COOKTORR, MA_SPEC_PHONG, MA_SPEC_BLINN) {
			uiDefButS(block, NUMSLI, B_MATPRV, "Hard:",	90, 100, 150,19, &(ma->har), 1.0, 511, 0, 0, "Sets the hardness of the specularity");
		}
		if(ma->spec_shader==MA_SPEC_BLINN)
			uiDefButF(block, NUMSLI, B_MATPRV, "Refr:",	90, 80,150,19, &(ma->refrac), 1.0, 10.0, 0, 0, "Sets the material's Index of Refraction");
		if(ma->spec_shader==MA_SPEC_TOON) {
			uiDefButF(block, NUMSLI, B_MATPRV, "Size:",	90, 100,150,19, &(ma->param[2]), 0.0, 1.53, 0, 0, "Sets the size of specular toon area");
			uiDefButF(block, NUMSLI, B_MATPRV, "Smooth:",90, 80,150,19, &(ma->param[3]), 0.0, 1.0, 0, 0, "Sets the smoothness of specular toon area");
		}
		
		/* default shading variables */
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, 0, "Translucency ",	9,30,301,19, &(ma->translucency), 0.0, 1.0, 100, 2, "Amount of diffuse shading of the back side");
		uiDefButF(block, NUMSLI, B_MATPRV, "Amb ",		9,10,150,19, &(ma->amb), 0.0, 1.0, 0, 0, "Sets the amount of global ambient color the material receives");
		uiDefButF(block, NUMSLI, B_MATPRV, "Emit ",		160,10,150,19, &(ma->emit), 0.0, 1.0, 0, 0, "Sets the amount of light the material emits");
		uiBlockEndAlign(block);

		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButI(block, TOG|BIT|0, 0,	"Traceable",		245,140,65,19, &(ma->mode), 0, 0, 0, 0, "Makes material cast shadows in spotlights");

		uiBlockBeginAlign(block);
		uiDefButI(block, TOG|BIT|1, 0,	"Shadow",			245,110,65,19, &(ma->mode), 0, 0, 0, 0, "Makes material receive shadows from spotlights");
		uiDefButI(block, TOG|BIT|19, 0, "TraShadow",		245,90,65,19, &(ma->mode), 0, 0, 0, 0, "Recieves transparent shadows based at material color and alpha");
		uiBlockEndAlign(block);

		uiDefButI(block, TOG|BIT|16, 0,	"Radio",			245,60,65,19, &(ma->mode), 0, 0, 0, 0, "Enables material for radiosty rendering");

	}

}


static void material_panel_material(Object *ob, Material *ma)
{
	uiBlock *block;
	ID *id, *idn, *idfrom;
	uiBut *but;
	float *colpoin = NULL, min;
	int rgbsel = 0;
	char str[30];
	
	block= uiNewBlock(&curarea->uiblocks, "material_panel_material", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Material", "Material", 320, 0, 318, 204)==0) return;

	/* first do the browse but */
	buttons_active_id(&id, &idfrom);

	uiBlockSetCol(block, TH_BUT_SETTING2);
	std_libbuttons(block, 8, 200, 0, NULL, B_MATBROWSE, id, idfrom, &(G.buts->menunr), B_MATALONE, B_MATLOCAL, B_MATDELETE, B_AUTOMATNAME, B_KEEPDATA);
	
	uiDefIconBut(block, BUT, B_MATCOPY, ICON_COPYUP,	263,200,XIC,YIC, 0, 0, 0, 0, 0, "Copies Material to the buffer");
	uiSetButLock(id && id->lib, "Can't edit library data");
	uiDefIconBut(block, BUT, B_MATPASTE, ICON_PASTEUP,	284,200,XIC,YIC, 0, 0, 0, 0, 0, "Pastes Material from the buffer");
	
	if(ob->actcol==0) ob->actcol= 1;	/* because of TOG|BIT button */
	
	uiBlockBeginAlign(block);

	/* id is the block from which the material is used */
	if( BTST(ob->colbits, ob->actcol-1) ) id= (ID *)ob;
	else id= ob->data;

	/* indicate which one is linking a material */
	if(id) {
		strncpy(str, id->name, 2);
		str[2]= ':'; str[3]= 0;
		but= uiDefBut(block, TEX, B_IDNAME, str,		8,174,115,20, id->name+2, 0.0, 18.0, 0, 0, "Shows the block the material is linked to");
		uiButSetFunc(but, test_idbutton_cb, id->name, NULL);
	}
	uiBlockSetCol(block, TH_BUT_ACTION);
	uiDefButS(block, TOG|BIT|(ob->actcol-1), B_MATFROM, "OB",	125,174,32,20, &ob->colbits, 0, 0, 0, 0, "Links material to object");
	idn= ob->data;
	strncpy(str, idn->name, 2);
	str[2]= 0;
	uiBlockSetCol(block, TH_BUT_SETTING);
	uiDefButS(block, TOGN|BIT|(ob->actcol-1), B_MATFROM, str,	158,174,32,20, &ob->colbits, 0, 0, 0, 0, "Shows the block the material is linked to");
	uiBlockSetCol(block, TH_AUTO);
	
	sprintf(str, "%d Mat", ob->totcol);
	if(ob->totcol) min= 1.0; else min= 0.0;
	uiDefButC(block, NUM, B_ACTCOL, str,			191,174,114,20, &(ob->actcol), min, (float)ob->totcol, 0, 0, "Shows the number of materials on object and the active material");
	uiBlockEndAlign(block);
	
	if(ob->totcol==0) return;
	uiSetButLock(id->lib!=0, "Can't edit library data");

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
			uiDefButI(block, TOG|BIT|4, B_REDR,	"VCol Light",	8,146,75,20, &(ma->mode), 0, 0, 0, 0, "Adds vertex colours as extra light");
			uiDefButI(block, TOG|BIT|7, B_REDR, "VCol Paint",	85,146,72,20, &(ma->mode), 0, 0, 0, 0, "Replaces material's colours with vertex colours");
			uiDefButI(block, TOG|BIT|11, B_REDR, "TexFace",		160,146,62,20, &(ma->mode), 0, 0, 0, 0, "Sets UV-Editor assigned texture as color and texture info for faces");
			uiDefButI(block, TOG|BIT|2, B_MATPRV, "Shadeless",	223,146,80,20, &(ma->mode), 0, 0, 0, 0, "Makes material insensitive to light or shadow");
			uiBlockSetCol(block, TH_AUTO);
			uiDefButF(block, NUM, 0, "Zoffs:",					8,127,120,19, &(ma->zoffs), 0.0, 10.0, 0, 0, "Gives faces an artificial offset in the Z buffer");
			uiBlockSetCol(block, TH_BUT_SETTING1);
			uiDefButI(block, TOG|BIT|3, 0,	"Wire",				128,127,96,19, &(ma->mode), 0, 0, 0, 0, "Renders only the edges of faces as a wireframe");
			uiDefButI(block, TOG|BIT|8, 0,	"ZInvert",			224,127,79,19, &(ma->mode), 0, 0, 0, 0, "Renders material's faces with inverted Z Buffer");

		}
		uiBlockSetCol(block, TH_AUTO);
		uiBlockBeginAlign(block);
		uiDefButF(block, COL, B_MATCOL, "",		8,97,72,20, &(ma->r), 0, 0, 0, 0, "");
		uiDefButF(block, COL, B_SPECCOL, "",	8,77,72,20, &(ma->specr), 0, 0, 0, 0, "");
		uiDefButF(block, COL, B_MIRCOL, "",		8,57,72,20, &(ma->mirr), 0, 0, 0, 0, "");
	
		uiBlockBeginAlign(block);
		if(ma->mode & MA_HALO) {
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Halo",		83,97,40,20, &(ma->rgbsel), 2.0, 0.0, 0, 0, "Sets the colour of the halo with the RGB sliders");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Line",		83,77,40,20, &(ma->rgbsel), 2.0, 1.0, 0, 0, "Sets the colour of the lines with the RGB sliders");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Ring",		83,57,40,20, &(ma->rgbsel), 2.0, 2.0, 0, 0, "Sets the colour of the rings with the RGB sliders");
		}
		else {
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Col",			83,97,40,20, &(ma->rgbsel), 2.0, 0.0, 0, 0, "Sets the basic colour of the material");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Spe",			83,77,40,20, &(ma->rgbsel), 2.0, 1.0, 0, 0, "Sets the specular colour of the material");
			uiDefButC(block, ROW, REDRAWBUTSSHADING, "Mir",			83,57,40,20, &(ma->rgbsel), 2.0, 2.0, 0, 0, "Sets the mirror colour of the material");
		}
		
		if(ma->rgbsel==0) {colpoin= &(ma->r); rgbsel= B_MATCOL;}
		else if(ma->rgbsel==1) {colpoin= &(ma->specr); rgbsel= B_SPECCOL;}
		else if(ma->rgbsel==2) {colpoin= &(ma->mirr); rgbsel= B_MIRCOL;}
		
		if(ma->rgbsel==0 && (ma->mode & (MA_VERTEXCOLP|MA_FACETEXTURE) && !(ma->mode & MA_HALO)));
		else if(ma->colormodel==MA_HSV) {
			uiBlockSetCol(block, TH_BUT_SETTING1);
			uiBlockBeginAlign(block);
			uiDefButF(block, HSVSLI, B_MATPRV, "H ",		128,97,175,19, colpoin, 0.0, 0.9999, rgbsel, 0, "");
			uiDefButF(block, HSVSLI, B_MATPRV, "S ",		128,77,175,19, colpoin, 0.0001, 1.0, rgbsel, 0, "");
			uiDefButF(block, HSVSLI, B_MATPRV, "V ",		128,57,175,19, colpoin, 0.0001, 1.0, rgbsel, 0, "");
			uiBlockSetCol(block, TH_AUTO);
		}
		else {
			uiBlockBeginAlign(block);
			uiDefButF(block, NUMSLI, B_MATPRV, "R ",		128,97,175,19, colpoin, 0.0, 1.0, rgbsel, 0, "");
			uiDefButF(block, NUMSLI, B_MATPRV, "G ",		128,77,175,19, colpoin+1, 0.0, 1.0, rgbsel, 0, "");
			uiDefButF(block, NUMSLI, B_MATPRV, "B ",		128,57,175,19, colpoin+2, 0.0, 1.0, rgbsel, 0, "");
		}
		uiBlockEndAlign(block);
		uiDefButF(block, NUMSLI, B_MATPRV, "A ",			128,30,175,19, &ma->alpha, 0.0, 1.0, 0, 0, "Alpha");
		
	}
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW, REDRAWBUTSSHADING, "RGB",			8,30,38,19, &(ma->colormodel), 1.0, (float)MA_RGB, 0, 0, "Creates colour using red, green and blue");
	uiDefButS(block, ROW, REDRAWBUTSSHADING, "HSV",			46,30,38,19, &(ma->colormodel), 1.0, (float)MA_HSV, 0, 0, "Creates colour using hue, saturation and value");
	uiDefButS(block, TOG|BIT|0, REDRAWBUTSSHADING, "DYN",	84,30,39,19, &(ma->dynamode), 0.0, 0.0, 0, 0, "Adjusts parameters for dynamics options");

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
			material_panel_tramir(ma);
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
		world_panel_amb_occ(wrld);
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
				// no panel! (e: not really true, is affected by noisedepth param)
				break;
			/* newnoise: musgrave panels */
			case TEX_MUSGRAVE:
				texture_panel_musgrave(mtex->tex);
				break;
			case TEX_DISTNOISE:
				texture_panel_distnoise(mtex->tex);
				break;
			/* newnoise: voronoi */
			case TEX_VORONOI:
				texture_panel_voronoi(mtex->tex);
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


