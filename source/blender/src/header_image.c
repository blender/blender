/**
 * header_image.c oct-2003
 *
 * Functions to draw the "UV/Image Editor" window header
 * and handle user events sent to it.
 * 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BDR_drawmesh.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BLI_blenlib.h"
#include "BIF_drawimage.h"
#include "BIF_editsima.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_writeimage.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "interface.h"
#include "mydevice.h"

#include "TPT_DependKludge.h"

void load_space_image(char *str)	/* called from fileselect */
{
	Image *ima=0;
 
	if(G.obedit) {
		error("Can't perfom this in editmode");
		return;
	}

	ima= add_image(str);
	if(ima) {

		G.sima->image= ima;

		free_image_buffers(ima);	/* force read again */
		ima->ok= 1;
		image_changed(G.sima, 0);

	}

	allqueue(REDRAWIMAGE, 0);
}

void image_replace(Image *old, Image *new)
{
	TFace *tface;
	Mesh *me;
	int a, rep=0;
 
	new->tpageflag= old->tpageflag;
	new->twsta= old->twsta;
	new->twend= old->twend;
	new->xrep= old->xrep;
	new->yrep= old->yrep;
 
	me= G.main->mesh.first;
	while(me) {

		if(me->tface) {
			tface= me->tface;
			a= me->totface;
			while(a--) {
				if(tface->tpage==old) {
					tface->tpage= new;
					rep++;
				}
				tface++;
			}
		}
		me= me->id.next;
 
	}
	if(rep) {
		if(new->id.us==0) new->id.us= 1;
	}
	else error("Nothing replaced");
}

void replace_space_image(char *str)		/* called from fileselect */
{
	Image *ima=0;

	if(G.obedit) {
		error("Can't perfom this in editmode");
		return;
	}

	ima= add_image(str);
	if(ima) {
 
		if(G.sima->image != ima) {
			image_replace(G.sima->image, ima);
		}
 
		G.sima->image= ima;

		free_image_buffers(ima);	/* force read again */
		ima->ok= 1;
		/* replace also assigns: */
		image_changed(G.sima, 0);

	}
	allqueue(REDRAWIMAGE, 0);
}

void save_paint(char *name)
{
	char str[FILE_MAXDIR+FILE_MAXFILE];
	Image *ima = G.sima->image;
	ImBuf *ibuf;

	if (ima  && ima->ibuf) {
		BLI_strncpy(str, name, sizeof(str));

		BLI_convertstringcode(str, G.sce, G.scene->r.cfra);

		if (saveover(str)) {
			ibuf = IMB_dupImBuf(ima->ibuf);

			if (ibuf) {
				if (BIF_write_ibuf(ibuf, str)) {
					BLI_strncpy(ima->name, name, sizeof(ima->name));
					ima->ibuf->userflags &= ~IB_BITMAPDIRTY;
					allqueue(REDRAWHEADERS, 0);
					allqueue(REDRAWBUTSSHADING, 0);
				} else {
					error("Couldn't write image: %s", str);
				}

				IMB_freeImBuf(ibuf);
			}
		}
	}
}

void do_image_buttons(unsigned short event)
{
	Image *ima;
	ID *id, *idtest;
	int nr;
	char name[256], str[256];

	if(curarea->win==0) return;

	switch(event) {
	case B_SIMAGEHOME:
		image_home();
		break;

	case B_SIMABROWSE:	
		if(G.sima->imanr== -2) {
			activate_databrowse((ID *)G.sima->image, ID_IM, 0, B_SIMABROWSE,
											&G.sima->imanr, do_image_buttons);
			return;
		}
		if(G.sima->imanr < 0) break;
	
		nr= 1;
		id= (ID *)G.sima->image;

		idtest= G.main->image.first;
		while(idtest) {
			if(nr==G.sima->imanr) {
				break;
			}
			nr++;
			idtest= idtest->next;
		}
		if(idtest==0) { /* no new */
			return;
		}
	
		if(idtest!=id) {
			G.sima->image= (Image *)idtest;
			if(idtest->us==0) idtest->us= 1;
			allqueue(REDRAWIMAGE, 0);
		}
		/* also when image is the same: assign! 0==no tileflag: */
		image_changed(G.sima, 0);

		break;
	case B_SIMAGELOAD:
	case B_SIMAGELOAD1:
		
		if(G.sima->image) strcpy(name, G.sima->image->name);
		else strcpy(name, U.textudir);
		
		if(event==B_SIMAGELOAD)
			activate_imageselect(FILE_SPECIAL, "SELECT IMAGE", name,
											load_space_image);
		else
			activate_fileselect(FILE_SPECIAL, "SELECT IMAGE", name,
											load_space_image);
		break;
	case B_SIMAGEREPLACE:
	case B_SIMAGEREPLACE1:
		
		if(G.sima->image) strcpy(name, G.sima->image->name);
		else strcpy(name, U.textudir);
		
		if(event==B_SIMAGEREPLACE)
			activate_imageselect(FILE_SPECIAL, "REPLACE IMAGE", name,
											replace_space_image);
		else
			activate_fileselect(FILE_SPECIAL, "REPLACE IMAGE", name,
											replace_space_image);
		break;
	case B_SIMAGEDRAW:
		
		if(G.f & G_FACESELECT) {
			make_repbind(G.sima->image);
			image_changed(G.sima, 1);
		}
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;

	case B_SIMAGEDRAW1:
		image_changed(G.sima, 2);		/* 2: only tileflag */
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
		
	case B_TWINANIM:
		ima = G.sima->image;
		if (ima) {
			if(ima->flag & IMA_TWINANIM) {
				nr= ima->xrep*ima->yrep;
				if(ima->twsta>=nr) ima->twsta= 1;
				if(ima->twend>=nr) ima->twend= nr-1;
				if(ima->twsta>ima->twend) ima->twsta= 1;
				allqueue(REDRAWIMAGE, 0);
			}
		}
		break;

	case B_CLIP_UV:
		tface_do_clip();
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;

	case B_SIMAGEPAINTTOOL:
		// check for packed file here
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_SIMAPACKIMA:
		ima = G.sima->image;
		if (ima) {
			if (ima->packedfile) {
				if (G.fileflags & G_AUTOPACK) {
					if (okee("Disable AutoPack ?")) {
						G.fileflags &= ~G_AUTOPACK;
					}
				}
				
				if ((G.fileflags & G_AUTOPACK) == 0) {
					unpackImage(ima, PF_ASK);
				}
			} else {
				if (ima->ibuf && (ima->ibuf->userflags & IB_BITMAPDIRTY)) {
					error("Can't pack painted image. Save image first.");
				} else {
					ima->packedfile = newPackedFile(ima->name);
				}
			}
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWHEADERS, 0);
		}
		break;
	case B_SIMAGESAVE:
		ima = G.sima->image;
		if (ima) {
			strcpy(name, ima->name);
			if (ima->ibuf) {
				save_image_filesel_str(str);
				activate_fileselect(FILE_SPECIAL, str, name, save_paint);
			}
		}
		break;
	}
}

/* This should not be a stack var! */
static int headerbuttons_packdummy;

void image_buttons(void)
{
	uiBlock *block;
	short xco;
	char naam[256];
	headerbuttons_packdummy = 0;
		
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTBLUE);

	what_image(G.sima);

	curarea->butspacetype= SPACE_IMAGE;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* HOME*/
	uiDefIconBut(block, BUT, B_SIMAGEHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");
	uiDefIconButS(block, TOG|BIT|0, B_BE_SQUARE, ICON_KEEPRECT,	xco+=XIC,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Toggles constraining UV polygons to squares while editing");
	uiDefIconButS(block, ICONTOG|BIT|2, B_CLIP_UV, ICON_CLIPUV_DEHLT,xco+=XIC,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Toggles clipping UV with image size");		

	xco= std_libbuttons(block, xco+40, 0, 0, NULL, B_SIMABROWSE, (ID *)G.sima->image, 0, &(G.sima->imanr), 0, 0, B_IMAGEDELETE, 0, 0);

	if (G.sima->image) {
		if (G.sima->image->packedfile) {
			headerbuttons_packdummy = 1;
		}
		uiDefIconButI(block, TOG|BIT|0, B_SIMAPACKIMA, ICON_PACKAGE,	xco,0,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Toggles packed status of this Image");
		xco += XIC;
	}
	
	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_SIMAGELOAD, "Load",		xco+=XIC,0,2*XIC,YIC, 0, 0, 0, 0, 0, "Loads image - thumbnail view");

	uiBlockSetCol(block, BUTGREY);
	uiDefBut(block, BUT, B_SIMAGELOAD1, "",		(short)(xco+=2*XIC+2),0,10,YIC, 0, 0, 0, 0, 0, "Loads image - file select view");
	xco+=XIC/2;

	if (G.sima->image) {
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_SIMAGEREPLACE, "Replace",xco+=XIC,0,(short)(3*XIC),YIC, 0, 0, 0, 0, 0, "Replaces current image - thumbnail view");
		
		uiBlockSetCol(block, BUTGREY);
		uiDefBut(block, BUT, B_SIMAGEREPLACE1, "",	(short)(xco+=3*XIC+2),0,10,YIC, 0, 0, 0, 0, 0, "Replaces current image - file select view");
		xco+=XIC/2;
	
		uiDefIconButS(block, TOG|BIT|4, 0, ICON_ENVMAP, xco+=XIC,0,XIC,YIC, &G.sima->image->flag, 0, 0, 0, 0, "Uses this image as a reflection map (Ignores UV Coordinates)");
		xco+=XIC/2;

		uiDefIconButS(block, TOG|BIT|0, B_SIMAGEDRAW1, ICON_GRID, xco+=XIC,0,XIC,YIC, &G.sima->image->flag, 0, 0, 0, 0, "");
		uiDefButS(block, NUM, B_SIMAGEDRAW, "",	xco+=XIC,0,XIC,YIC, &G.sima->image->xrep, 1.0, 16.0, 0, 0, "Sets the degree of repetition in the X direction");
		uiDefButS(block, NUM, B_SIMAGEDRAW, "",	xco+=XIC,0,XIC,YIC, &G.sima->image->yrep, 1.0, 16.0, 0, 0, "Sets the degree of repetition in the Y direction");
	
		uiDefButS(block, TOG|BIT|1, B_TWINANIM, "Anim", xco+=XIC,0,(short)(2*XIC),YIC, &G.sima->image->tpageflag, 0, 0, 0, 0, "Toggles use of animated texture");
		uiDefButS(block, NUM, B_TWINANIM, "",	(short)(xco+=2*XIC),0,XIC,YIC, &G.sima->image->twsta, 0.0, 128.0, 0, 0, "Displays the start frame of an animated texture. Click to change.");
		uiDefButS(block, NUM, B_TWINANIM, "",	xco+=XIC,0,XIC,YIC, &G.sima->image->twend, 0.0, 128.0, 0, 0, "Displays the end frame of an animated texture. Click to change.");
//		uiDefButS(block, TOG|BIT|2, 0, "Cycle", xco+=XIC,0,2*XIC,YIC, &G.sima->image->tpageflag, 0, 0, 0, 0, "");
		uiDefButS(block, NUM, 0, "Speed", xco+=(2*XIC),0,4*XIC,YIC, &G.sima->image->animspeed, 1.0, 100.0, 0, 0, "Displays Speed of the animation in frames per second. Click to change.");

#ifdef NAN_TPT
		xco+= 4*XIC;
		uiDefIconButS(block, ICONTOG|BIT|3, B_SIMAGEPAINTTOOL, ICON_TPAINT_DEHLT, xco+=XIC,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Enables TexturePaint Mode");
		if (G.sima->image && G.sima->image->ibuf && (G.sima->image->ibuf->userflags & IB_BITMAPDIRTY)) {
			uiDefBut(block, BUT, B_SIMAGESAVE, "Save",		xco+=XIC,0,2*XIC,YIC, 0, 0, 0, 0, 0, "Saves image");
			xco += XIC;
		}
#endif /* NAN_TPT */
		xco+= XIC;
	}

	/* draw LOCK */
	xco+= XIC/2;
	uiDefIconButS(block, ICONTOG, 0, ICON_UNLOCKED,	(short)(xco+=XIC),0,XIC,YIC, &(G.sima->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");

	
	/* Always do this last */
	curarea->headbutlen= xco+2*XIC;
	
	uiDrawBlock(block);
}
