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

 *
 *
 * General blender hot keys (toets = dutch), special hotkeys are in space.c
 *
 */

#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "nla.h"	/* Only for the #ifdef flag - To be removed later */

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_anim.h"
#include "BKE_scene.h"
#include "BKE_ipo.h"
#include "BKE_action.h"
#include "BKE_ika.h"
#include "BKE_key.h"

#include "BIF_interface.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_butspace.h"
#include "BIF_renderwin.h"
#include "BIF_toolbox.h"
#include "BIF_toets.h"
#include "BIF_editseq.h"
#include "BIF_editsound.h"
#include "BIF_poseobject.h"
#include "BIF_usiblender.h"

#include "BDR_vpaint.h"
#include "BDR_editobject.h"
#include "BDR_editface.h"

#include "BSE_filesel.h"	/* For activate_fileselect */
#include "BSE_drawview.h"	/* For play_anim */
#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_headerbuttons.h"

#include "blendef.h"
#include "render.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "mydevice.h"

#include "BIF_poseobject.h"

/* only used in toets.c */
/* this function doesn't really belong here */
/* ripped from render module */
void schrijfplaatje(char *name);


void write_imag(char *name)
{
	/* from file select */
	char str[256];

	strcpy(str, name);
	BLI_convertstringcode(str, G.sce, G.scene->r.cfra);

	if(saveover(str)) {
		if(BLI_testextensie(str,".blend")) {
			error("Wrong filename");
			return;
		}
		waitcursor(1); /* from screen.c */
		schrijfplaatje(str);
		strcpy(G.ima, name);
		waitcursor(0);
	}
}


/* From matrix.h: it's really a [4][4]! */
/* originally in initrender... maybe add fileControl thingy? */

/* should be called write_image(char *name) :-) */
void schrijfplaatje(char *name)
{
	struct ImBuf *ibuf=0;
	unsigned int *temprect=0;
	char str[FILE_MAXDIR+FILE_MAXFILE];

	/* has RGBA been set? If so: use alpha channel for color zero */
	IMB_alpha_to_col0(FALSE);

	if(R.r.planes == 32) {
		/* everything with less than 50 % alpha -> col 0 */
		if(R.r.alphamode == R_ALPHAKEY) IMB_alpha_to_col0(2);
		/* only when 0 alpha -> col 0 */
		else IMB_alpha_to_col0(1);
	}

	/* Seems to me this is also superfluous.... */
	if (R.r.imtype==R_FTYPE) {
		strcpy(str, R.r.ftype);
		BLI_convertstringcode(str, G.sce, G.scene->r.cfra);

		ibuf = IMB_loadiffname(str, IB_test);
		if(ibuf) {
			ibuf->x = R.rectx;
			ibuf->y = R.recty;
		}
		else {
			error("Can't find filetype");
			G.afbreek= 1;
			return;
		}
		/* setdither(2); */
	}

	if(ibuf == 0) {
		ibuf= IMB_allocImBuf(R.rectx, R.recty, R.r.planes, 0, 0);
	}

	if(ibuf) {
		ibuf->rect= (unsigned int *) R.rectot;

		if(R.r.planes == 8) IMB_cspace(ibuf, rgb_to_bw);

		if(R.r.imtype== R_IRIS) {
			ibuf->ftype= IMAGIC;
		}
		else if(R.r.imtype==R_IRIZ) {
			ibuf->ftype= IMAGIC;
			if (ibuf->zbuf == 0) {
				if (R.rectz) {
					ibuf->zbuf = (int *)R.rectz;
				}
				else printf("no zbuf\n");
			}
		}
		else if(R.r.imtype==R_PNG) {
			ibuf->ftype= PNG;
		}
		else if((R.r.imtype==R_TARGA) || (R.r.imtype==R_PNG)) {
			ibuf->ftype= TGA;
		}
		else if(R.r.imtype==R_RAWTGA) {
			ibuf->ftype= RAWTGA;
		}
		else if(R.r.imtype==R_HAMX) {
			/* make copy */
			temprect= MEM_dupallocN(R.rectot);
			ibuf->ftype= AN_hamx;
		}
		else if(ELEM5(R.r.imtype, R_MOVIE, R_AVICODEC, R_AVIRAW, R_AVIJPEG, R_JPEG90)) {
			if(R.r.quality < 10) R.r.quality= 90;

			if(R.r.mode & R_FIELDS) ibuf->ftype= JPG_VID|R.r.quality;
			else ibuf->ftype= JPG|R.r.quality;
		}
	
		RE_make_existing_file(name);

		if(IMB_saveiff(ibuf, name, IB_rect | IB_zbuf)==0) {
			perror(name);
			G.afbreek= 1;
		}

		IMB_freeImBuf(ibuf);

		if (R.r.imtype==R_HAMX) {
			MEM_freeN(R.rectot);
			R.rectot= temprect;
		}
	}
	else {
		G.afbreek= 1;
	}
}



/* ------------------------------------------------------------------------- */

static int is_an_active_object(void *ob) {
	Base *base;
	
	for (base= FIRSTBASE; base; base= base->next)
		if (base->object == ob)
			return 1;
	
	return 0;
}

void persptoetsen(unsigned short event)
{
	static Object *oldcamera=0;
	float phi, si, q1[4], vec[3];
	static int perspo=1;
	
	if(event==PADENTER) {
		if (G.qual == LR_SHIFTKEY) {
			view3d_set_1_to_1_viewborder(G.vd);
		} else {
			if (G.vd->persp==2) {
				G.vd->camzoom= 0.0;
			} else {
				G.vd->dist= 10.0;
			}
		}
	}
	else if((G.qual & (LR_SHIFTKEY | LR_CTRLKEY)) && (event != PAD0)) {
		if(event==PAD0) {
			/* G.vd->persp= 3; */
		}
		else if(event==PAD7) {
			G.vd->viewquat[0]= 0.0;
			G.vd->viewquat[1]= -1.0;
			G.vd->viewquat[2]= 0.0;
			G.vd->viewquat[3]= 0.0;
			G.vd->view= 7;
			if(G.vd->persp>=2) G.vd->persp= perspo;
		}
		else if(event==PAD1) {
			G.vd->viewquat[0]= 0.0;
			G.vd->viewquat[1]= 0.0;
			G.vd->viewquat[2]= (float)-cos(M_PI/4.0);
			G.vd->viewquat[3]= (float)-cos(M_PI/4.0);
			G.vd->view=1;
			if(G.vd->persp>=2) G.vd->persp= perspo;
		}
		else if(event==PAD3) {
			G.vd->viewquat[0]= 0.5;
			G.vd->viewquat[1]= -0.5;
			G.vd->viewquat[2]= 0.5;
			G.vd->viewquat[3]= 0.5;
			G.vd->view=3;
			if(G.vd->persp>=2) G.vd->persp= perspo;
		}
		else if(event==PADMINUS) {
			/* this min and max is also in viewmove() */
			if(G.vd->persp==2) {
					G.vd->camzoom-= 10;
					if(G.vd->camzoom<-30) G.vd->camzoom= -30;
				}
			else if(G.vd->dist<10.0*G.vd->far) G.vd->dist*=1.2f;
		}
		else if(event==PADPLUSKEY) {
			if(G.vd->persp==2) {
					G.vd->camzoom+= 10;
					if(G.vd->camzoom>300) G.vd->camzoom= 300;
			}
			else if(G.vd->dist> 0.001*G.vd->grid) G.vd->dist*=.83333f;
		}
		else {

			initgrabz(0.0, 0.0, 0.0);
			
			if(event==PAD6) window_to_3d(vec, -32, 0);
			else if(event==PAD4) window_to_3d(vec, 32, 0);
			else if(event==PAD8) window_to_3d(vec, 0, -25);
			else if(event==PAD2) window_to_3d(vec, 0, 25);
			G.vd->ofs[0]+= vec[0];
			G.vd->ofs[1]+= vec[1];
			G.vd->ofs[2]+= vec[2];
		}
	}
	else {

		if(event==PAD7) {
			G.vd->viewquat[0]= 1.0;
			G.vd->viewquat[1]= 0.0;
			G.vd->viewquat[2]= 0.0;
			G.vd->viewquat[3]= 0.0;
			G.vd->view=7;
			if(G.vd->persp>=2) G.vd->persp= perspo;
		}
		else if(event==PAD1) {
			G.vd->viewquat[0]= (float)cos(M_PI/4.0);
			G.vd->viewquat[1]= (float)-sin(M_PI/4.0);
			G.vd->viewquat[2]= 0.0;
			G.vd->viewquat[3]= 0.0;
			G.vd->view=1;
			if(G.vd->persp>=2) G.vd->persp= perspo;
		}
		else if(event==PAD3) {
			G.vd->viewquat[0]= 0.5;
			G.vd->viewquat[1]= -0.5;
			G.vd->viewquat[2]= -0.5;
			G.vd->viewquat[3]= -0.5;
			G.vd->view=3;
			if(G.vd->persp>=2) G.vd->persp= perspo;
		}
		else if(event==PADMINUS) {
			/* this min and max is also in viewmove() */
			if(G.vd->persp==2) {
				G.vd->camzoom= MAX2(-30, G.vd->camzoom-5);
			}
			else if(G.vd->dist<10.0*G.vd->far) G.vd->dist*=1.2f;
		}
		else if(event==PADPLUSKEY) {
			if(G.vd->persp==2) {
				G.vd->camzoom= MIN2(300, G.vd->camzoom+5);
			}
			else if(G.vd->dist> 0.001*G.vd->grid) G.vd->dist*=.83333f;
		}
		else if(event==PAD5) {
			if(G.vd->persp==1) G.vd->persp=0;
			else G.vd->persp=1;
		}
		else if(event==PAD0) {
			if(G.qual==LR_ALTKEY) {
				if(oldcamera && is_an_active_object(oldcamera)) {
					G.vd->camera= oldcamera;
				}
				
				handle_view3d_lock();
			}
			else if(BASACT) {
				if(G.qual==LR_CTRLKEY) {
					if(G.vd->camera != OBACT) {
						if(G.vd->camera && G.vd->camera->type==OB_CAMERA)
							oldcamera= G.vd->camera;
						
						G.vd->camera= OBACT;
						handle_view3d_lock();
					}
				}
				else if(G.vd->camera==0 && OBACT->type==OB_CAMERA) {
					G.vd->camera= OBACT;
					handle_view3d_lock();
				}
			}
			if(G.vd->camera==0) {
				G.vd->camera= scene_find_camera(G.scene);
				handle_view3d_lock();
			}
			
			if(G.vd->camera) {
				G.vd->persp= 2;
				G.vd->view= 0;
				if(G.qual & LR_SHIFTKEY) {
					void setcameratoview3d(void);	// view.c
					setcameratoview3d();
				}				
			}
		}
		else if(event==PAD9) {
			countall();
			do_all_ipos();
			do_all_keys();
			do_all_actions();
			do_all_ikas();

			reset_slowparents();	/* editobject.c */
		}
		else if(G.vd->persp<2) {
			if(event==PAD4 || event==PAD6) {
				/* z-axis */
				phi= (float)(M_PI/24.0);
				if(event==PAD6) phi= -phi;
				si= (float)sin(phi);
				q1[0]= (float)cos(phi);
				q1[1]= q1[2]= 0.0;
				q1[3]= si;
				QuatMul(G.vd->viewquat, G.vd->viewquat, q1);
				G.vd->view= 0;
			}
			if(event==PAD2 || event==PAD8) {
				
				/* horizontal axis */
				VECCOPY(q1+1, G.vd->viewinv[0]);
				
				Normalise(q1+1);
				phi= (float)(M_PI/24.0);
				if(event==PAD2) phi= -phi;
				si= (float)sin(phi);
				q1[0]= (float)cos(phi);
				q1[1]*= si;
				q1[2]*= si;
				q1[3]*= si;
				QuatMul(G.vd->viewquat, G.vd->viewquat, q1);
				G.vd->view= 0;
			}
		}

		if(G.vd->persp<2) perspo= G.vd->persp;
	}
	scrarea_queue_redraw(curarea);
}

int untitled(char * name)
{
	if (G.save_over == 0 ) {
		char * c= BLI_last_slash(name);
		
		if (c)
			strcpy(&c[1], "untitled.blend");
		else
			strcpy(name, "untitled.blend");
			
		return(TRUE);
	}
	
	return(FALSE);
}

int save_image_filesel_str(char *str)
{
	switch(G.scene->r.imtype) {
	case R_PNG:
		strcpy(str, "SAVE PNG"); return 1;
	case R_TARGA:
		strcpy(str, "SAVE TARGA"); return 1;
	case R_RAWTGA:
		strcpy(str, "SAVE RAW TARGA"); return 1;
	case R_IRIS:
		strcpy(str, "SAVE IRIS"); return 1;
	case R_IRIZ:
		strcpy(str, "SAVE IRIS"); return 1;
	case R_HAMX:
		strcpy(str, "SAVE HAMX"); return 1;
	case R_FTYPE:
		strcpy(str, "SAVE FTYPE"); return 1;
	case R_JPEG90:
		strcpy(str, "SAVE JPEG"); return 1;
	default:
		strcpy(str, "SAVE IMAGE"); return 0;
	}	
}

void BIF_save_rendered_image(void)
{
	if(!R.rectot) {
		error("No image rendered");
	} else {
		char dir[FILE_MAXDIR * 2], str[FILE_MAXFILE * 2];

		if(G.ima[0]==0) {
			strcpy(dir, G.sce);
			BLI_splitdirstring(dir, str);
			strcpy(G.ima, dir);
		}
		
		R.r.imtype= G.scene->r.imtype;
		R.r.quality= G.scene->r.quality;
		R.r.planes= G.scene->r.planes;
	
		if (!save_image_filesel_str(str)) {
			error("Select an image type in DisplayButtons(F10)");
		} else {
			activate_fileselect(FILE_SPECIAL, str, G.ima, write_imag);
		}
	}
}

int blenderqread(unsigned short event, short val)
{
	/* here do the general keys handling (not screen/window/space) */
	/* return 0: do not pass on to the other queues */
	extern int textediting;
	ScrArea *sa;
	Object *ob;
	int textspace=0;
	/* Changed str and dir size to 160, to make sure there is enough
	 * space for filenames. */
	char dir[FILE_MAXDIR * 2], str[FILE_MAXFILE * 2];
	
	if(val==0) return 1;
	if(event==MOUSEY || event==MOUSEX) return 1;
	if (G.flags & G_FLAGS_AUTOPLAY) return 1;

	if (curarea && curarea->spacetype==SPACE_TEXT) textspace= 1;

	switch(event) {

	case F1KEY:
		if(G.qual==0) {
			/* this exception because of the '?' button */
			if(curarea->spacetype==SPACE_INFO) {
				sa= closest_bigger_area();
				areawinset(sa->win);
			}
			
			activate_fileselect(FILE_BLENDER, "LOAD FILE", G.sce, BIF_read_file);
			return 0;
		}
		else if(G.qual==LR_SHIFTKEY) {
			activate_fileselect(FILE_LOADLIB, "LOAD LIBRARY", G.lib, 0);
			return 0;
		}
		break;
	case F2KEY:
		if(G.qual==0) {
			strcpy(dir, G.sce);
			untitled(dir);
			activate_fileselect(FILE_BLENDER, "SAVE FILE", dir, BIF_write_file);
			return 0;
		}
		else if(G.qual==LR_CTRLKEY) {
			write_vrml_fs();
			return 0;
		}
		else if(G.qual==LR_SHIFTKEY) {
			write_dxf_fs();
			return 0;
		}
		break;
	case F3KEY:
		if(G.qual==0) {
			BIF_save_rendered_image();
			return 0;
		}
		else if((G.qual==LR_CTRLKEY)||(G.qual==(LR_CTRLKEY|LR_SHIFTKEY))) {
			BIF_screendump();
		}
		break;
	case F4KEY:
		if(G.qual==LR_SHIFTKEY) {

			memset(str, 0, 16);
			ob= OBACT;
			if(ob) strcpy(str, ob->id.name);

			activate_fileselect(FILE_MAIN, "DATA SELECT", str, 0);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event);
		}
		break;
	case F5KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_VIEW3D);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event);
		}
		break;
	case F6KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_IPO);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event);
		}
		break;
	case F7KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_BUTS);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event);
		}
		break;
	case F8KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_SEQ);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event);
		}
		break;
	case F9KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_OOPS);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event);
		}
		break;
	case F10KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_IMAGE);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event);
		}
		break;
	case F11KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_TEXT);
			return 0;
		}
		else if(G.qual==0) {
			BIF_toggle_render_display();
			return 0;
		}
		break;
	case F12KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_ACTION);
			return 0;
		}
		else if (G.qual==(LR_SHIFTKEY|LR_CTRLKEY)) {
			newspace(curarea, SPACE_NLA);
			return 0;
		}
		else if(G.qual==0) {
			BIF_do_render(0);
		}
		return 0;
		break;
	
	case LEFTARROWKEY:
	case DOWNARROWKEY:
		if(textediting==0 && textspace==0) {

#ifdef _WIN32	// FULLSCREEN
			if(event==DOWNARROWKEY){
				if (G.qual==LR_ALTKEY)
					mainwindow_toggle_fullscreen(0);
				else if(G.qual==0)
					CFRA-= 10;
			}
#else
			if((event==DOWNARROWKEY)&&(G.qual==0))
				CFRA-= 10;
#endif
			else if((event==LEFTARROWKEY)&&(G.qual==0))
				CFRA--;
			
			if(G.qual==LR_SHIFTKEY)
				CFRA= SFRA;
			if(CFRA<1) CFRA=1;
	
			update_for_newframe();
			return 0;
		}
		break;

	case RIGHTARROWKEY:
	case UPARROWKEY:
		if(textediting==0 && textspace==0) {

#ifdef _WIN32	// FULLSCREEN
			if(event==UPARROWKEY){ 
				if(G.qual==LR_ALTKEY)
					mainwindow_toggle_fullscreen(1);
				else if(G.qual==0)
					CFRA+= 10;
			}
#else
			if((event==UPARROWKEY)&&(G.qual==0))
				CFRA+= 10;
#endif
			else if((event==RIGHTARROWKEY)&&(G.qual==0))
				CFRA++;

			if(G.qual==LR_SHIFTKEY)
				CFRA= EFRA;
			
			update_for_newframe();
		}
		break;

	case ESCKEY:
		sound_stop_all_sounds();
		break;
	case TABKEY:
		if(G.qual==0) {
			if(textspace==0) {
				if(curarea->spacetype==SPACE_IPO)
					set_editflag_editipo();
				else if(curarea->spacetype==SPACE_SEQ)
					enter_meta();
				else if(G.vd) {
					/* also when Alt-E */
					if(G.obedit==0)
						enter_editmode();
					else
						exit_editmode(1);
				}
				return 0;
			}
		}
		else if(G.qual==LR_CTRLKEY){
			if(G.obpose)
				exit_posemode(1);
			else
				enter_posemode();
			allqueue(REDRAWHEADERS, 0);	
			
		}
		else if(G.qual==LR_SHIFTKEY) {
			if(G.obedit)
				exit_editmode(1);
			if(G.f & G_FACESELECT)
				set_faceselect();
			if(G.f & G_VERTEXPAINT)
				set_vpaint();
			if(G.f & G_WEIGHTPAINT)
				set_wpaint();
			if(G.obpose)
				exit_posemode(1);
		}
		break;

	case BACKSPACEKEY:
		break;

	case AKEY:
		if(textediting==0 && textspace==0) {
			if(G.qual==(LR_SHIFTKEY|LR_ALTKEY)){
				play_anim(1);
				return 0;
			}
			else if(G.qual==LR_ALTKEY) {
				play_anim(0);
				return 0;
			}
		}
		break;
	case EKEY:
		if(G.qual==LR_ALTKEY) {
			if(G.vd && textspace==0) {
				if(G.obedit==0)
					enter_editmode();
				else
					exit_editmode(1);
				return 0;
			}			
		}
		break;
	case IKEY:
		if(textediting==0 && textspace==0 && curarea->spacetype!=SPACE_FILE && curarea->spacetype!=SPACE_IMASEL) {
			if(G.qual==0) {
				common_insertkey();
				return 0;
			}
		}
		break;
	case JKEY:
		if(textediting==0 && textspace==0) {
			if(R.rectot && G.qual==0) {
				BIF_swap_render_rects();
				return 0;
			}
		}
		break;

	case NKEY:
		if(textediting==0 && textspace==0 ) {
			if(G.qual & LR_CTRLKEY);
			else if(G.qual==0 || (G.qual & LR_SHIFTKEY)) {
				if(curarea->spacetype==SPACE_VIEW3D);		// is new panel, in view3d queue
				else if(curarea->spacetype==SPACE_IPO);			// is new panel, in ipo queue
				else {
					clever_numbuts();
					return 0;
				}
			}
		}
		break;
		
	case OKEY:
		if(textediting==0) {
			if(G.qual==LR_CTRLKEY) {
				/* There seem to be crashes here sometimes.... String
				 * bound overwrites? I changed dir and str sizes,
				 * let's see if this reoccurs. */
				sprintf(str, "Open file: %s", G.sce);
			
				if(okee(str)) {
					strcpy(dir, G.sce);
					BIF_read_file(dir);
				}
				return 0;
			}
		}
		break;
		
	case SKEY:
		if(G.obpose==0 && G.obedit==0) {
			if(G.qual==LR_CTRLKEY) {
				strcpy(dir, G.sce);
				if (untitled(dir)) {
					activate_fileselect(FILE_BLENDER, "SAVE FILE", dir, BIF_write_file);
				} else {
					BIF_write_file(dir);
					free_filesel_spec(dir);
				}
				return 0;
			}
		}
		break;
	
	case TKEY:
		if (G.qual==(LR_SHIFTKEY|LR_ALTKEY|LR_CTRLKEY)) {
			int a;
			double delta, stime;

			waitcursor(1);
			
			stime= PIL_check_seconds_timer();
			for(a=0; a<100000; a++) {
				scrarea_do_windraw(curarea);

				delta= PIL_check_seconds_timer()-stime;
				if (delta>5.0) break;
			}
			
			waitcursor(0);
			notice("FPS: %f (%d iterations)", a/delta, a);
			return 0;
		}
		else if(G.qual==(LR_ALTKEY|LR_CTRLKEY)) {
			int a;
			int event= pupmenu("10 Timer%t|draw|draw+swap");
			if(event>0) {
				double stime= PIL_check_seconds_timer();
				char tmpstr[128];
				int time;

				printf("start timer\n");
				waitcursor(1);
								
				for(a=0; a<10; a++) {
					scrarea_do_windraw(curarea);
					if(event==2) screen_swapbuffers();
				}
			
				time= (PIL_check_seconds_timer()-stime)*1000;
				
				if(event==1) sprintf(tmpstr, "draw %%t|%d", time);
				if(event==2) sprintf(tmpstr, "d+sw %%t|%d", time);
			
				waitcursor(0);
				pupmenu(tmpstr);

			}
			return 0;
		}
		break;
				
	case UKEY:
		if(textediting==0) {
			if(G.qual==LR_CTRLKEY) {
				if(okee("SAVE USER DEFAULTS")) {
					BIF_write_homefile();
				}
				return 0;
			}
		}
		break;
		
	case WKEY:
		if(textediting==0) {
			if(G.qual==LR_CTRLKEY) {
				strcpy(dir, G.sce);
				if (untitled(dir)) {
					activate_fileselect(FILE_BLENDER, "SAVE FILE", dir, BIF_write_file);
				} else {
					BIF_write_file(dir);
					free_filesel_spec(dir);
				}
				return 0;
			}
			else if(G.qual==LR_ALTKEY) {
				write_videoscape_fs();
			}
		}
		break;
		
	case XKEY:
		if(textspace==0) {
			if(G.qual==LR_CTRLKEY) {
				if(okee("ERASE ALL")) {
					if( BIF_read_homefile()==0) error("No file ~/.B.blend");
				}
				return 0;
			}
		}
		break;
	}
	
	return 1;
}

/* eof */
