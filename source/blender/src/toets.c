/**
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****

 *
 *
 * General blender hot keys (toets = dutch), special hotkeys are in space.c
 *
 */

#include <string.h>
#include <math.h>

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "nla.h"	/* Only for the #ifdef flag - To be removed later */

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_blender.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "BIF_butspace.h"
#include "BIF_editseq.h"
#include "BIF_editsound.h"
#include "BIF_editmesh.h"
#include "BIF_imasel.h"
#include "BIF_editparticle.h"
#include "BIF_interface.h"
#include "BIF_poseobject.h"
#include "BIF_previewrender.h"
#include "BIF_renderwin.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_usiblender.h"
#include "BIF_writeimage.h"

#include "BDR_sculptmode.h"
#include "BDR_vpaint.h"
#include "BDR_editobject.h"
#include "BDR_editface.h"

#include "BSE_filesel.h"	/* For activate_fileselect */
#include "BSE_drawview.h"	/* For play_anim */
#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_seqaudio.h"

#include "blendef.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "mydevice.h"

#include "transform.h"

#define VIEW_ZOOM_OUT_FACTOR (1.15f)
#define VIEW_ZOOM_IN_FACTOR (1.0f/VIEW_ZOOM_OUT_FACTOR)

/* ------------------------------------------------------------------------- */

static int is_an_active_object(void *ob) {
	Base *base;
	
	for (base= FIRSTBASE; base; base= base->next)
		if (base->object == ob)
			return 1;
	
	return 0;
}

/* run when pressing 1,3 or 7 */
static void axis_set_view(float q1, float q2, float q3, float q4, short view, int perspo)
{
	float new_quat[4];
	new_quat[0]= q1; new_quat[1]= q2;
	new_quat[2]= q3; new_quat[3]= q4;
	G.vd->view=0;
	
	if (G.vd->persp==V3D_CAMOB && G.vd->camera) {
		/* Is this switching from a camera view ? */
		float orig_ofs[3];
		float orig_lens= G.vd->lens;
		VECCOPY(orig_ofs, G.vd->ofs);
		view_settings_from_ob(G.vd->camera, G.vd->ofs, G.vd->viewquat, &G.vd->dist, &G.vd->lens);
		
		if (U.uiflag & USER_AUTOPERSP) G.vd->persp= V3D_ORTHO;
		else if(G.vd->persp==V3D_CAMOB) G.vd->persp= perspo;
		
		smooth_view(G.vd, orig_ofs, new_quat, NULL, &orig_lens);
	} else {
		
		if (U.uiflag & USER_AUTOPERSP) G.vd->persp= V3D_ORTHO;
		else if(G.vd->persp==V3D_CAMOB) G.vd->persp= perspo;
		
		smooth_view(G.vd, NULL, new_quat, NULL, NULL);
	}
	G.vd->view= view;
}

void persptoetsen(unsigned short event)
{
	static Object *oldcamera=0;
	float phi, si, q1[4], vec[3];
	static int perspo=V3D_PERSP;
	int preview3d_event= 1;
	short mouseloc[2];
	
	float new_dist, orig_ofs[3];
	
	/* Use this to test if we started out with a camera */
	Object *act_cam_orig=NULL;
	if (G.vd->persp == V3D_CAMOB)
		act_cam_orig = G.vd->camera;
	
	if(event==PADENTER) {
		if (G.qual == LR_SHIFTKEY) {
			view3d_set_1_to_1_viewborder(G.vd);
		} else {
			if (G.vd->persp==V3D_CAMOB) {
				G.vd->camzoom= 0;
			} else {
				new_dist = 10.0;
				smooth_view(G.vd, NULL, NULL, &new_dist, NULL);
			}
		}
	}
	else if((G.qual & (LR_SHIFTKEY | LR_CTRLKEY)) && (event != PAD0)) {
		
		/* Indicate that this view is inverted,
		 * but only if it actually _was_ inverted (jobbe) */
		if (event==PAD7 || event == PAD1 || event == PAD3)
			G.vd->flag2 |= V3D_OPP_DIRECTION_NAME;
		
		if(event==PAD0) {
			/* G.vd->persp= 3; */
		}
		else if(event==PAD7) {
			axis_set_view(0.0, -1.0, 0.0, 0.0, 7, perspo);
		}
		else if(event==PAD1) {
			axis_set_view(0.0, 0.0, (float)-cos(M_PI/4.0), (float)-cos(M_PI/4.0), 1, perspo);
		}
		else if(event==PAD3) {
			axis_set_view(0.5, -0.5, 0.5, 0.5, 3, perspo);
		}
		else if(event==PADMINUS) {
			/* this min and max is also in viewmove() */
			if(G.vd->persp==V3D_CAMOB) {
				G.vd->camzoom-= 10;
				if(G.vd->camzoom<-30) G.vd->camzoom= -30;
			}
			else if(G.vd->dist<10.0*G.vd->far) G.vd->dist*=1.2f;
		}
		else if(event==PADPLUSKEY) {
			if(G.vd->persp==V3D_CAMOB) {
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
		/* Indicate that this view is not inverted.
		 * Don't do this for PADMINUS/PADPLUSKEY/PAD5, though. (jobbe)*/
		if (! ELEM3(event, PADMINUS, PADPLUSKEY, PAD5) )
			G.vd->flag2 &= ~V3D_OPP_DIRECTION_NAME;
		

		if(event==PAD7) {
			axis_set_view(1.0, 0.0, 0.0, 0.0, 7, perspo);
		}
		else if(event==PAD1) {
			axis_set_view((float)cos(M_PI/4.0), (float)-sin(M_PI/4.0), 0.0, 0.0, 1, perspo);
		}
		else if(event==PAD3) {
			axis_set_view(0.5, -0.5, -0.5, -0.5, 3, perspo);
		}
		else if(event==PADMINUS) {
			/* this min and max is also in viewmove() */
			if(G.vd->persp==V3D_CAMOB) {
				G.vd->camzoom= MAX2(-30, G.vd->camzoom-5);
			}
			else if(G.vd->dist<10.0*G.vd->far) {
				getmouseco_areawin(mouseloc);
				view_zoom_mouseloc(VIEW_ZOOM_OUT_FACTOR, mouseloc);
			}
			if(G.vd->persp!=V3D_PERSP) preview3d_event= 0;
		}
		else if(event==PADPLUSKEY) {
			if(G.vd->persp==V3D_CAMOB) {
				G.vd->camzoom= MIN2(300, G.vd->camzoom+5);
			}
			else if(G.vd->dist> 0.001*G.vd->grid) {
				getmouseco_areawin(mouseloc);
				view_zoom_mouseloc(VIEW_ZOOM_IN_FACTOR, mouseloc);
			}
			if(G.vd->persp!=V3D_PERSP) preview3d_event= 0;
		}
		else if(event==PAD5) {
			if (U.smooth_viewtx) {
				if(G.vd->persp==V3D_PERSP) { G.vd->persp=V3D_ORTHO;
				} else if (act_cam_orig) {
					/* were from a camera view */
					float orig_dist= G.vd->dist;
					float orig_lens= G.vd->lens;
					VECCOPY(orig_ofs, G.vd->ofs);

					G.vd->persp=V3D_PERSP;
					G.vd->dist= 0.0;

					view_settings_from_ob(act_cam_orig, G.vd->ofs, NULL, NULL, &G.vd->lens);
					
					smooth_view(G.vd, orig_ofs, NULL, &orig_dist, &orig_lens);
					
				} else {
					G.vd->persp=V3D_PERSP;
				}
			} else {
				if(G.vd->persp==V3D_PERSP) G.vd->persp=V3D_ORTHO;
				else G.vd->persp=V3D_PERSP;
			}
		}
		else if(event==PAD0) {
			if(G.qual==LR_ALTKEY) {
				if(oldcamera && is_an_active_object(oldcamera)) {
					G.vd->camera= oldcamera;
				}
				handle_view3d_lock();
			}
			else if(BASACT) {
				/* check both G.vd as G.scene cameras */
				if(G.qual==LR_CTRLKEY) {
					if(G.vd->camera != OBACT || G.scene->camera != OBACT) {
						if(G.vd->camera && G.vd->camera->type==OB_CAMERA)
							oldcamera= G.vd->camera;
						
						G.vd->camera= OBACT;
						handle_view3d_lock();
					}
				}
				else if((G.vd->camera==NULL || G.scene->camera==NULL) && OBACT->type==OB_CAMERA) {
					G.vd->camera= OBACT;
					handle_view3d_lock();
				}
			}
			if(G.vd->camera==0) {
				G.vd->camera= scene_find_camera(G.scene);
				handle_view3d_lock();
			}
			
			if(G.vd->camera && (G.vd->camera != act_cam_orig)) {
				G.vd->persp= V3D_CAMOB;
				G.vd->view= 0;
				
				if(((G.qual & LR_CTRLKEY) && (G.qual & LR_ALTKEY)) || (G.qual & LR_SHIFTKEY)) {
					void setcameratoview3d(void);	// view.c
					setcameratoview3d();
					autokeyframe_ob_cb_func(G.scene->camera, TFM_TRANSLATION|TFM_ROTATION);
					DAG_object_flush_update(G.scene, G.scene->camera, OB_RECALC_OB);
					BIF_undo_push("View to Camera position");
					allqueue(REDRAWVIEW3D, 0);
				
				} else if (U.smooth_viewtx) {
					/* move 3d view to camera view */
					float orig_lens = G.vd->lens;
					VECCOPY(orig_ofs, G.vd->ofs);
					
					if (act_cam_orig)
						view_settings_from_ob(act_cam_orig, G.vd->ofs, G.vd->viewquat, &G.vd->dist, &G.vd->lens);
					
					smooth_view_to_camera(G.vd);
					VECCOPY(G.vd->ofs, orig_ofs);
					G.vd->lens = orig_lens;
				}
				
			
			}
		}
		else if(event==PAD9) {
			countall();
			update_for_newframe();
			
			reset_slowparents();	/* editobject.c */
		}
		else if(G.vd->persp != V3D_CAMOB) {
			if(event==PAD4 || event==PAD6) {
				/* z-axis */
				phi= (float)(M_PI/360.0)*U.pad_rot_angle;
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
				
				Normalize(q1+1);
				phi= (float)(M_PI/360.0)*U.pad_rot_angle;
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

		if(G.vd->persp != V3D_CAMOB) perspo= G.vd->persp;
	}

	if(G.vd->depths) G.vd->depths->damaged= 1;
	retopo_queue_updates(G.vd);
	
	if(preview3d_event) 
		BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);
	else
		BIF_view3d_previewrender_signal(curarea, PR_PROJECTED);

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

char *recent_filelist(void)
{
	struct RecentFile *recent;
	int event, i, ofs;
	char pup[2048], *p;

	p= pup + sprintf(pup, "Open Recent%%t");
	
	if (G.sce[0]) {
		p+= sprintf(p, "|%s %%x%d", G.sce, 1);
		ofs = 1;
	} else ofs = 0;

	for (recent = G.recent_files.first, i=0; (i<U.recent_files) && (recent); recent = recent->next, i++) {
		if (strcmp(recent->filename, G.sce)) {
			p+= sprintf(p, "|%s %%x%d", recent->filename, i+ofs+1);
		}
	}
	event= pupmenu(pup);
	if(event>0) {
		if (ofs && (event==1))
			return(G.sce);
		else
			recent = BLI_findlink(&(G.recent_files), event-1-ofs);
			if(recent) return(recent->filename);
	}
	
	return(NULL);
}

int blenderqread(unsigned short event, short val)
{
	/* here do the general keys handling (not screen/window/space) */
	/* return 0: do not pass on to the other queues */
	extern int textediting;
	extern void playback_anim();
	ScrArea *sa;
	Object *ob;
	int textspace=0;
	/* Changed str and dir size to 160, to make sure there is enough
	 * space for filenames. */
	char dir[FILE_MAXDIR * 2], str[FILE_MAXFILE * 2];
	char *recentfile;
	
	if(val==0) return 1;
	if(event==MOUSEY || event==MOUSEX) return 1;
	if (G.flags & G_FILE_AUTOPLAY) return 1;

	if (curarea && curarea->spacetype==SPACE_TEXT) textspace= 1;
	else if (curarea && curarea->spacetype==SPACE_SCRIPT) textspace= 1;

	switch(event) {

	case F1KEY:
		if(G.qual==0) {
			/* this exception because of the '?' button */
			if(curarea->spacetype==SPACE_INFO) {
				sa= closest_bigger_area();
				areawinset(sa->win);
			}
			
			activate_fileselect(FILE_BLENDER, "Open File", G.sce, BIF_read_file);
			return 0;
		}
		else if(G.qual==LR_SHIFTKEY) {
			activate_fileselect(FILE_LOADLIB, "Load Library", G.lib, 0);
			return 0;
		}
		else if(G.qual==LR_CTRLKEY) {
			activate_imageselect(FILE_LOADLIB, "Load Library", G.lib, 0);
			return 0;
		}
		break;
	case F2KEY:
		if(G.qual==0) {
			strcpy(dir, G.sce);
			untitled(dir);
			activate_fileselect(FILE_BLENDER, "Save File", dir, BIF_write_file);
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
			BIF_save_rendered_image_fs();
			return 0;
		}
		else if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_NODE);
			return 0;
		}
		else if(G.qual & LR_CTRLKEY) {
			BIF_screendump(0);
		}
		break;
	case F4KEY:
		if(G.qual==LR_SHIFTKEY) {

			memset(str, 0, 16);
			ob= OBACT;
			if(ob) strcpy(str, ob->id.name);

			activate_fileselect(FILE_MAIN, "Data Select", str, NULL);
			return 0;
		}
		else if(G.qual==LR_CTRLKEY) {

			memset(str, 0, 16);
			ob= OBACT;
			if(ob) strcpy(str, ob->id.name);

			activate_imageselect(FILE_MAIN, "Data Select", str, 0);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event, 1);
		}
		break;
	case F5KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_VIEW3D);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event, 1);
		}
		break;
	case F6KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_IPO);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event, 1);
		}
		break;
	case F7KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_BUTS);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event, 1);
		}
		break;
	case F8KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_SEQ);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event, 1);
		}
		break;
	case F9KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_OOPS);
			return 0;
		}
		else if(G.qual==(LR_SHIFTKEY|LR_ALTKEY)) {
			newspace(curarea, SPACE_OOPS+256);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event, 1);
		}
		break;
	case F10KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_IMAGE);
			return 0;
		}
		else if(G.qual==0) {
			extern_set_butspace(event, 1);
		}
		break;
	case F11KEY:
		if(G.qual==LR_SHIFTKEY) {
			newspace(curarea, SPACE_TEXT);
			return 0;
		}
		else if (G.qual==LR_CTRLKEY) {
			playback_anim();
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
		else if (G.qual==LR_CTRLKEY) {
			BIF_do_render(1);
		}
		else {
			/* ctrl/alt + f12 should render too, for some macs have f12 assigned to cd eject */
			BIF_do_render(0);
		}
		return 0;
		break;
	
	case WHEELUPMOUSE:
		if(G.qual==LR_ALTKEY || G.qual==LR_COMMANDKEY) {
			if(CFRA>1) {
				CFRA--;
				update_for_newframe();
			}
			return 0;
		}
		break;
	case WHEELDOWNMOUSE:
		if(G.qual==LR_ALTKEY || G.qual==LR_COMMANDKEY) {
			CFRA++;
			update_for_newframe();
			return 0;
		}
		break;
		
	case LEFTARROWKEY:
	case DOWNARROWKEY:
		if(textediting==0 && textspace==0) {

#if 0
//#ifdef _WIN32	// FULLSCREEN
			if(event==DOWNARROWKEY){
				if (G.qual==LR_ALTKEY)
					mainwindow_toggle_fullscreen(0);
				else if(G.qual==0)
					CFRA-= G.scene->jumpframe;
			}
#else
			if((event==DOWNARROWKEY)&&(G.qual==0))
				CFRA-= G.scene->jumpframe;
#endif
			else if((event==LEFTARROWKEY)&&(G.qual==0))
				CFRA--;
			
			if(G.qual==LR_SHIFTKEY)
				CFRA= PSFRA;
			if(CFRA<1) CFRA=1;
	
			update_for_newframe();
			return 0;
		}
		break;

	case RIGHTARROWKEY:
	case UPARROWKEY:
		if(textediting==0 && textspace==0) {

#if 0
//#ifdef _WIN32	// FULLSCREEN
			if(event==UPARROWKEY){ 
				if(G.qual==LR_ALTKEY)
					mainwindow_toggle_fullscreen(1);
				else if(G.qual==0)
					CFRA+= G.scene->jumpframe;
			}
#else
			if((event==UPARROWKEY)&&(G.qual==0))
				CFRA+= G.scene->jumpframe;
#endif
			else if((event==RIGHTARROWKEY)&&(G.qual==0))
				CFRA++;

			if(G.qual==LR_SHIFTKEY)
				CFRA= PEFRA;
			
			update_for_newframe();
		}
		break;

	case ESCKEY:
		sound_stop_all_sounds();	// whats this?
		
		/* stop playback on ESC always */
		rem_screenhandler(G.curscreen, SCREEN_HANDLER_ANIM);
		audiostream_stop();
		BKE_ptcache_set_continue_physics(0);
		allqueue(REDRAWALL, 0);
		
		break;
	case TABKEY:
		if(G.qual==0) {
			if(textspace==0) {
				if(curarea->spacetype==SPACE_IPO)
					set_editflag_editipo();
				else if(curarea->spacetype==SPACE_SEQ)
					enter_meta();
				else if(curarea->spacetype==SPACE_NODE)
					return 1;
				else if(G.vd) {
					/* also when Alt-E */
					if(G.obedit==NULL) {
						enter_editmode(EM_WAITCURSOR);
						if(G.obedit) BIF_undo_push("Original");	// here, because all over code enter_editmode is abused
					}
					else
						exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); // freedata, and undo
				}
				return 0;
			}
		}
		else if(G.qual==LR_CTRLKEY){
			Object *ob= OBACT;
			if(ob) {
				if(ob->type==OB_ARMATURE) {
					if(ob->flag & OB_POSEMODE) exit_posemode();
					else enter_posemode();
				}
				else if(ob->type==OB_MESH) {
					if(ob==G.obedit) EM_selectmode_menu();
					else if(G.f & G_PARTICLEEDIT)
						PE_selectbrush_menu();
					else if(G.f & G_SCULPTMODE)
						sculptmode_selectbrush_menu();
					else set_wpaint();
				}
			}
		}
		else if(G.qual&LR_CTRLKEY && G.qual&LR_SHIFTKEY){
			if(!(G.f & G_PARTICLEEDIT))
				exit_paint_modes();
			PE_set_particle_edit();
		}
		break;

	case BACKSPACEKEY:
		break;
	case SPACEKEY:
		if (curarea && curarea->spacetype==SPACE_SEQ) {
			SpaceSeq *sseq= curarea->spacedata.first;
			if (G.qual==0 && sseq->mainb) {
				play_anim(1);
				return 0;
			}
		}
		break;
	case AKEY:
		if(textediting==0 && textspace==0) {
			if ((G.qual==LR_ALTKEY) && (curarea && curarea->spacetype==SPACE_VIEW3D)) {
				play_anim(0);
				return 0;
			}
			else if ((G.qual==LR_ALTKEY) || (G.qual==(LR_ALTKEY|LR_SHIFTKEY))){
				play_anim(1);
				return 0;
			}
		}
		break;
	case EKEY:
		if(G.qual==LR_ALTKEY) {
			if(G.vd && textspace==0) {
				if(G.obedit==0) {
					enter_editmode(EM_WAITCURSOR);
					BIF_undo_push("Original");
				}
				else
					exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); // freedata, and undo
				return 0;
			}			
		}
		break;
	case IKEY:
		if(textediting==0 && textspace==0 && !ELEM3(curarea->spacetype, SPACE_FILE, SPACE_IMASEL, SPACE_NODE)) {
			ob= OBACT;

			if(G.f & G_SCULPTMODE) return 1;
			else if(G.qual==0) {
				common_insertkey();
				return 0;
			}
		}
		break;
	case JKEY:
		if(textediting==0 && textspace==0) {
			if (G.qual==0) {
				BIF_swap_render_rects();
				return 0;
			}
		}
		break;

	case NKEY:
		if(textediting==0 && textspace==0) {
			if(G.qual & LR_CTRLKEY);
			else if(G.qual==0 || (G.qual & LR_SHIFTKEY)) {
				if(curarea->spacetype==SPACE_VIEW3D);		// is new panel, in view3d queue
				else if(curarea->spacetype==SPACE_IPO);			// is new panel, in ipo queue
				else if(curarea->spacetype==SPACE_IMAGE);			// is new panel, in ipo queue
				else if(curarea->spacetype==SPACE_ACTION);			// is own queue
				else if(curarea->spacetype==SPACE_NLA);			// is new panel
				else if(curarea->spacetype==SPACE_SEQ);			// is new panel
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
				recentfile = recent_filelist();
				if(recentfile) {
					BIF_read_file(recentfile);
				}
				return 0;
			}
		}
		break;
		
	case SKEY:
		if(G.obedit==NULL) {
			if(G.qual==LR_CTRLKEY) {
				strcpy(dir, G.sce);
				if (untitled(dir)) {
					activate_fileselect(FILE_BLENDER, "Save File", dir, BIF_write_file);
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
			Object *ob = OBACT;
			int event = pupmenu(ob?"Time%t|draw|recalc ob|recalc data":"Time%t|draw");
			int a;
			double delta, stime;

			if (event < 0) return 0; /* cancelled by user */

			waitcursor(1);
			
			stime= PIL_check_seconds_timer();
			for(a=0; a<100000; a++) {
				if (event==1) {
					scrarea_do_windraw(curarea);
				} else if (event==2) {
					ob->recalc |= OB_RECALC_OB;
					object_handle_update(ob);
				} else if (event==3) {
					ob->recalc |= OB_RECALC_DATA;
					object_handle_update(ob);
				}

				delta= PIL_check_seconds_timer()-stime;
				if (delta>5.0) break;
			}
			
			waitcursor(0);
			notice("%8.6f s/op - %6.2f ops/s - %d iterations", delta/a, a/delta, a);
			return 0;
		}
		else if(G.qual==(LR_ALTKEY|LR_CTRLKEY)) {
			int a;
			int event= pupmenu("10 Timer%t|draw|draw+swap|undo");
			if(event>0) {
				double stime= PIL_check_seconds_timer();
				char tmpstr[128];
				int time;

				waitcursor(1);
				
				for(a=0; a<10; a++) {
					if (event==1) {
						scrarea_do_windraw(curarea);
					} else if (event==2) {
						scrarea_do_windraw(curarea);
						screen_swapbuffers();
					}
					else if(event==3) {
						BIF_undo();
						BIF_redo();
					}
				}
			
				time= (int) ((PIL_check_seconds_timer()-stime)*1000);
				
				if(event==1) sprintf(tmpstr, "draw %%t|%d ms", time);
				if(event==2) sprintf(tmpstr, "d+sw %%t|%d ms", time);
				if(event==3) sprintf(tmpstr, "undo %%t|%d ms", time);
			
				waitcursor(0);
				pupmenu(tmpstr);

			}
			return 0;
		}
		break;
				
	case UKEY:
		if(textediting==0) {
			if(G.qual==LR_CTRLKEY) {
				if(okee("Save user defaults")) {
					BIF_write_homefile();
				}
				return 0;
			}
			else if(G.qual==LR_ALTKEY) {
				if(curarea->spacetype!=SPACE_TEXT) {
					BIF_undo_menu();
					return 0;
				}
			}
		}
		break;
		
	case WKEY:
		if(textediting==0) {
			if(G.qual==LR_CTRLKEY) {
				strcpy(dir, G.sce);
				if (untitled(dir)) {
					activate_fileselect(FILE_BLENDER, "Save File", dir, BIF_write_file);
				} else {
					BIF_write_file(dir);
					free_filesel_spec(dir);
				}
				return 0;
			}
			/* Python specials? ;)
			else if(G.qual==LR_ALTKEY) {
				write_videoscape_fs();
				return 0;
			}*/ 
		}
		break;
		
	case XKEY:
		if(textspace==0 && textediting==0) {
			if(G.qual==LR_CTRLKEY) {
				if(okee("Erase all")) {
					if( BIF_read_homefile(0)==0) error("No file ~/.B.blend");
					
					/* Reset lights
					 * This isn't done when reading userdef, do it now
					 *  */
					default_gl_light(); 
				}
				return 0;
			}
		}
		break;
	case YKEY:	// redo alternative
		if(textspace==0) {
			if(G.qual==LR_CTRLKEY) {
				BIF_redo(); 
				return 0;
			}
		}
		break;
	case ZKEY:	// undo
		if(textspace==0) {
			if(G.qual & (LR_CTRLKEY|LR_COMMANDKEY)) { // all combos with ctrl/commandkey are accepted
				if ELEM(G.qual, LR_CTRLKEY, LR_COMMANDKEY) BIF_undo();
				else BIF_redo(); // all combos with ctrl is redo
				return 0;
			}
		}
		break; 
	}
	
	return 1;
}

/* eof */
