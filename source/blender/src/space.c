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
 * - hier het initialiseren en vrijgeven van SPACE data
 */

#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_linklist.h"

#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view2d_types.h"
#include "DNA_view3d_types.h"

#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "BIF_buttons.h"
#include "BIF_drawimage.h"
#include "BIF_drawseq.h"
#include "BIF_drawtext.h"
#include "BIF_editarmature.h"
#include "BIF_editfont.h"
#include "BIF_editika.h"
#include "BIF_editkey.h"
#include "BIF_editlattice.h"
#include "BIF_editmesh.h"
#include "BIF_editoops.h"
#include "BIF_editseq.h"
#include "BIF_editsima.h"
#include "BIF_editsound.h"
#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_oops.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_spacetypes.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_usiblender.h"
#include "BIF_previewrender.h"

#include "BSE_edit.h"
#include "BSE_view.h"
#include "BSE_editipo.h"
#include "BSE_drawipo.h"
#include "BSE_drawview.h"
#include "BSE_drawnla.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_editnla_types.h"

#include "BDR_vpaint.h"
#include "BDR_editmball.h"
#include "BDR_editobject.h"
#include "BDR_editcurve.h"
#include "BDR_editface.h"
#include "BDR_drawmesh.h"

#include "BLO_readfile.h" /* for BLO_blendhandle_close */

#include "interface.h"
#include "mydevice.h"
#include "blendef.h"
#include "datatoc.h"

#include "BPY_extern.h" // Blender Python library

#include "TPT_DependKludge.h"
#ifdef NAN_TPT
#include "BSE_trans_types.h"
#include "IMG_Api.h"
#endif /* NAN_TPT */

#include "SYS_System.h" /* for the user def menu ... should move elsewhere. */

extern void StartKetsjiShell(ScrArea *area, char* startscenename, struct Main* maggie, int always_use_expand_framing);

/**
 * When the mipmap setting changes, we want to redraw the view right
 * away to reflect this setting.
 */
void space_mipmap_button_function(int event);


unsigned short convert_for_nonumpad(unsigned short event);
void free_soundspace(SpaceSound *ssound);

/* ************* SPACE: VIEW3D  ************* */

/*  extern void drawview3d(); BSE_drawview.h */


void copy_view3d_lock(short val)
{
	bScreen *sc;
	int bit;
	
	/* van G.scene naar andere views kopieeren */
	sc= G.main->screen.first;
	
	while(sc) {
		if(sc->scene==G.scene) {
			ScrArea *sa= sc->areabase.first;
			while(sa) {
				SpaceLink *sl= sa->spacedata.first;
				while(sl) {
					if(sl->spacetype==SPACE_OOPS && val==REDRAW) {
						if(sa->win) scrarea_queue_winredraw(sa);
					}
					else if(sl->spacetype==SPACE_VIEW3D) {
						View3D *vd= (View3D*) sl;
						if(vd->scenelock && vd->localview==0) {
							vd->lay= G.scene->lay;
							vd->camera= G.scene->camera;
							
							if(vd->camera==0 && vd->persp>1) vd->persp= 1;
							
							if( (vd->lay & vd->layact) == 0) {
								bit= 0;
								while(bit<32) {
									if(vd->lay & (1<<bit)) {
										vd->layact= 1<<bit;
										break;
									}
									bit++;
								}
							}
							
							if(val==REDRAW && vd==sa->spacedata.first) {
								scrarea_queue_redraw(sa);
							}
						}
					}
					sl= sl->next;
				}
				sa= sa->next;
			}
		}
		sc= sc->id.next;
	}
}

void handle_view3d_lock()
{
	if (G.vd != NULL) {
		if(G.vd->localview==0 && G.vd->scenelock && curarea->spacetype==SPACE_VIEW3D) {

			/* naar scene kopieeren */
			G.scene->lay= G.vd->lay;
			G.scene->camera= G.vd->camera;
	
			copy_view3d_lock(REDRAW);
		}
	}
}

void space_set_commmandline_options(void) {
	SYS_SystemHandle syshandle;
	int a;
		
	if ( (syshandle = SYS_GetSystem()) ) {
		/* User defined settings */
		a= (U.gameflags & USERDEF_VERTEX_ARRAYS);
		SYS_WriteCommandLineInt(syshandle, "vertexarrays", a);

		a= (U.gameflags & USERDEF_DISABLE_SOUND);
		SYS_WriteCommandLineInt(syshandle, "noaudio", a);

		a= (U.gameflags & USERDEF_DISABLE_MIPMAP);
		set_mipmap(!a);
		SYS_WriteCommandLineInt(syshandle, "nomipmap", a);

		/* File specific settings: */
		/* Only test the first one. These two are switched
		 * simultaneously. */
		a= (G.fileflags & G_FILE_SHOW_FRAMERATE);
		SYS_WriteCommandLineInt(syshandle, "show_framerate", a);
		SYS_WriteCommandLineInt(syshandle, "show_profile", a);

		/* When in wireframe mode, always draw debug props. */
		if (G.vd) {
			a = ( (G.fileflags & G_FILE_SHOW_DEBUG_PROPS) 
				  || (G.vd->drawtype == OB_WIRE)          
				  || (G.vd->drawtype == OB_SOLID)         );
			SYS_WriteCommandLineInt(syshandle, "show_properties", a);
		}

		a= (G.fileflags & G_FILE_ENABLE_ALL_FRAMES);
		SYS_WriteCommandLineInt(syshandle, "fixedtime", a);
	}
}

	/**
	 * These two routines imported from the gameengine, 
	 * I suspect a lot of the resetting stuff is cruft
	 * and can be removed, but it should be checked.
	 */
static void SaveState(void)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	init_realtime_GL();
	init_gl_stuff();

	if(G.scene->camera==0 || G.scene->camera->type!=OB_CAMERA)
		error("no (correct) camera");

	waitcursor(1);
}

static void RestoreState(void)
{
	curarea->win_swap = 0;
	curarea->head_swap=0;
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSALL, 0);
	reset_slowparents();
	waitcursor(0);
	G.qual= 0;
	glPopAttrib();
}

static LinkNode *save_and_reset_all_scene_cfra(void)
{
	LinkNode *storelist= NULL;
	Scene *sc;
	
	for (sc= G.main->scene.first; sc; sc= sc->id.next) {
		BLI_linklist_prepend(&storelist, (void*) sc->r.cfra);
		sc->r.cfra= 1;

		set_scene_bg(sc);
	}
	
	BLI_linklist_reverse(&storelist);
	
	return storelist;
}

static void restore_all_scene_cfra(LinkNode *storelist) {
	LinkNode *sc_store= storelist;
	Scene *sc;
	
	for (sc= G.main->scene.first; sc; sc= sc->id.next) {
		int stored_cfra= (int) sc_store->link;
		
		sc->r.cfra= stored_cfra;
		set_scene_bg(sc);
		
		sc_store= sc_store->next;
	}
	
	BLI_linklist_free(storelist, NULL);
}

void start_game(void)
{
#if GAMEBLENDER == 1
	Scene *sc, *startscene = G.scene;
	LinkNode *scene_cfra_store;

		/* XXX, silly code -  the game engine can
		 * access any scene through logic, so we try 
		 * to make sure each scene has a valid camera, 
		 * just in case the game engine tries to use it.
		 * 
		 * Better would be to make a better routine
		 * in the game engine for finding the camera.
		 *  - zr
		 */
	for (sc= G.main->scene.first; sc; sc= sc->id.next) {
		if (!sc->camera) {
			Base *base;
	
			for (base= sc->base.first; base; base= base->next)
				if (base->object->type==OB_CAMERA)
					break;
			
			sc->camera= base?base->object:NULL;
		}
	}

	/* these two lines make sure front and backbuffer are equal. for swapbuffers */
	markdirty_all();
	screen_swapbuffers();

	/* can start from header */
	mywinset(curarea->win);
    
	scene_cfra_store= save_and_reset_all_scene_cfra();
	
	BPY_end_python();

	sound_stop_all_sounds();

	/* Before jumping into Ketsji, we configure some settings. */
	space_set_commmandline_options();

	SaveState();
	StartKetsjiShell(curarea, startscene->id.name+2, G.main, 1);
	RestoreState();

	BPY_start_python();

	restore_all_scene_cfra(scene_cfra_store);
	set_scene_bg(startscene);
	
	if (G.flags & G_FLAGS_AUTOPLAY)
		exit_usiblender();

		/* groups could have changed ipo */
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
#endif
}

void changeview3d()
{
	
	setwinmatrixview3d(0);	/* 0= geen pick rect */
	
}

	/* Callable from editmode and faceselect mode from the
	 * moment, would be nice (and is easy) to generalize
	 * to any mode.
	 */
static void align_view_to_selected(View3D *v3d)
{
	int nr= pupmenu("Align view%t|To selection (top)%x2|To selection (front)%x1|To selection (side)%x0");

	if (nr!=-1) {
		int axis= nr;

		if (G.obedit && (G.obedit->type == OB_MESH)) {
			editmesh_align_view_to_selected(v3d, axis);
			addqueue(v3d->area->win, REDRAW, 1);
		} else if (G.f & G_FACESELECT) {
			Object *obact= OBACT;
			if (obact && obact->type==OB_MESH) {
				Mesh *me= obact->data;

				if (me->tface) {
					faceselect_align_view_to_selected(v3d, me, axis);
					addqueue(v3d->area->win, REDRAW, 1);
				}
			}
		}
	}
}

void winqread3d(unsigned short event, short val, char ascii)
{
	View3D *v3d= curarea->spacedata.first;
	Object *ob;
	float *curs;
	int doredraw= 0, pupval;
	
	if(curarea->win==0) return;	/* hier komtie vanuit sa->headqread() */
	if(event==MOUSEY) return;
	
	if(val) {

		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		/* TEXTEDITING?? */
		if(G.obedit && G.obedit->type==OB_FONT) {
			switch(event) {
			
			case LEFTMOUSE:
				mouse_cursor();
				break;
			case MIDDLEMOUSE:
				if(U.flag & VIEWMOVE) {
					if(G.qual & LR_SHIFTKEY) viewmove(0);
					else if(G.qual & LR_CTRLKEY) viewmove(2);
					else viewmove(1);
				}
				else {
					if(G.qual & LR_SHIFTKEY) viewmove(1);
					else if(G.qual & LR_CTRLKEY) viewmove(2);
					else viewmove(0);
				}
			case UKEY:
				if(G.qual & LR_ALTKEY) {
					remake_editText();
					doredraw= 1;
				} else {
					do_textedit(event, val, ascii);
				}
				break;
			case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
			case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
			case PADENTER:
				persptoetsen(event);
				doredraw= 1;
				break;
				
			default:
				do_textedit(event, val, ascii);
				break;
			}
		}
		else {
			switch(event) {
			
			case BACKBUFDRAW:
				backdrawview3d(1);
				break;
				
			case LEFTMOUSE:
				if (G.obedit || !(G.f&(G_VERTEXPAINT|G_WEIGHTPAINT|G_TEXTUREPAINT))) {
					mouse_cursor();
				}
				else if (G.f & G_VERTEXPAINT) {
					vertex_paint();
				}
				else if (G.f & G_WEIGHTPAINT){
					weight_paint();
				}
				else if (G.f & G_TEXTUREPAINT) {
					face_draw();
				}
				break;
			case MIDDLEMOUSE:
				if(U.flag & VIEWMOVE) {
					if(G.qual & LR_SHIFTKEY) viewmove(0);
					else if(G.qual & LR_CTRLKEY) viewmove(2);
					else viewmove(1);
				}
				else {
					if(G.qual & LR_SHIFTKEY) viewmove(1);
					else if(G.qual & LR_CTRLKEY) viewmove(2);
					else viewmove(0);
				}
				break;
			case RIGHTMOUSE:
				if(G.obedit && (G.qual & LR_CTRLKEY)==0) {
					if(G.obedit->type==OB_MESH) mouse_mesh();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) mouse_nurb();
					else if(G.obedit->type==OB_MBALL) mouse_mball();
					else if(G.obedit->type==OB_LATTICE) mouse_lattice();
					else if(G.obedit->type==OB_ARMATURE) mouse_armature();
				}
				else if(G.obpose) { 
					if (G.obpose->type==OB_ARMATURE) mousepose_armature();
				}
				else if( G.qual & LR_CTRLKEY ) mouse_select();
				else if(G.f & G_FACESELECT) face_select();
				else if( G.f & (G_VERTEXPAINT|G_TEXTUREPAINT)) sample_vpaint();
				else
					mouse_select();
				break;

			case WHEELUPMOUSE:
				/* Regular:   Zoom in */
				/* Shift:     Scroll up */
				/* Ctrl:      Scroll right */
				/* Alt-Shift: Rotate up */
				/* Alt-Ctrl:  Rotate right */

				if( G.qual & LR_SHIFTKEY ) {
					if( G.qual & LR_ALTKEY ) { 
						G.qual &= ~LR_SHIFTKEY;
						persptoetsen(PAD2);
						G.qual |= LR_SHIFTKEY;
					} else {
						persptoetsen(PAD2);
					}
				} else if( G.qual & LR_CTRLKEY ) {
					if( G.qual & LR_ALTKEY ) { 
						G.qual &= ~LR_CTRLKEY;
						persptoetsen(PAD4);
						G.qual |= LR_CTRLKEY;
					} else {
						persptoetsen(PAD4);
					}
				} else if(U.uiflag & WHEELZOOMDIR) 
					persptoetsen(PADMINUS);
				else
					persptoetsen(PADPLUSKEY);

				doredraw= 1;
				break;
			case WHEELDOWNMOUSE:
				/* Regular:   Zoom out */
				/* Shift:     Scroll down */
				/* Ctrl:      Scroll left */
				/* Alt-Shift: Rotate down */
				/* Alt-Ctrl:  Rotate left */

				if( G.qual & LR_SHIFTKEY ) {
					if( G.qual & LR_ALTKEY ) { 
						G.qual &= ~LR_SHIFTKEY;
						persptoetsen(PAD8);
						G.qual |= LR_SHIFTKEY;
					} else {
						persptoetsen(PAD8);
					}
				} else if( G.qual & LR_CTRLKEY ) {
					if( G.qual & LR_ALTKEY ) { 
						G.qual &= ~LR_CTRLKEY;
						persptoetsen(PAD6);
						G.qual |= LR_CTRLKEY;
					} else {
						persptoetsen(PAD6);
					}
				} else if(U.uiflag & WHEELZOOMDIR) 
					persptoetsen(PADPLUSKEY);
				else
					persptoetsen(PADMINUS);
				
				doredraw= 1;
				break;
			
			case ONEKEY:
				do_layer_buttons(0); break;
			case TWOKEY:
				do_layer_buttons(1); break;
			case THREEKEY:
				do_layer_buttons(2); break;
			case FOURKEY:
				do_layer_buttons(3); break;
			case FIVEKEY:
				do_layer_buttons(4); break;
			case SIXKEY:
				do_layer_buttons(5); break;
			case SEVENKEY:
				do_layer_buttons(6); break;
			case EIGHTKEY:
				do_layer_buttons(7); break;
			case NINEKEY:
				do_layer_buttons(8); break;
			case ZEROKEY:
				do_layer_buttons(9); break;
			case MINUSKEY:
				do_layer_buttons(10); break;
			case EQUALKEY:
				do_layer_buttons(11); break;
			case ACCENTGRAVEKEY:
				do_layer_buttons(-1); break;
				
			case AKEY:
				if(G.qual & LR_CTRLKEY) apply_object();
				else if(G.qual & LR_SHIFTKEY) {
					tbox_setmain(0);
					toolbox();
				}
				else {
					if(G.obedit) {
						if(G.obedit->type==OB_MESH) deselectall_mesh();
						else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) deselectall_nurb();
						else if(G.obedit->type==OB_MBALL) deselectall_mball();
						else if(G.obedit->type==OB_LATTICE) deselectall_Latt();
						else if(G.obedit->type==OB_ARMATURE) deselectall_armature();
					}
					else if (G.obpose){
						switch (G.obpose->type){
						case OB_ARMATURE:
							deselectall_posearmature(1);
							break;
						}
					}
					else {
						if(G.f & G_FACESELECT) deselectall_tface();
						else deselectall();
					}
				}
				break;
			case BKEY:
				if(G.qual & LR_SHIFTKEY) set_render_border();
				else borderselect();
				break;
			case CKEY:
				if(G.qual & LR_CTRLKEY) {
					copymenu();
				}
				else if(G.qual & LR_ALTKEY) {
					convertmenu();	/* editobject.c */
				}
				else if(G.qual & LR_SHIFTKEY) {
					view3d_home(1);
					curs= give_cursor();
					curs[0]=curs[1]=curs[2]= 0.0;
					scrarea_queue_winredraw(curarea);
				}
				else if(G.obedit!=0 && ELEM(G.obedit->type, OB_CURVE, OB_SURF) ) {
					makecyclicNurb();
					makeDispList(G.obedit);
					allqueue(REDRAWVIEW3D, 0);
				}
				else {
					curs= give_cursor();
					G.vd->ofs[0]= -curs[0];
					G.vd->ofs[1]= -curs[1];
					G.vd->ofs[2]= -curs[2];
					scrarea_queue_winredraw(curarea);
				}
			
				break;
			case DKEY:
				if(G.qual & LR_SHIFTKEY) {
					if(G.obedit) {
						if(G.obedit->type==OB_MESH) adduplicate_mesh();
						else if(G.obedit->type==OB_ARMATURE) adduplicate_armature();
						else if(G.obedit->type==OB_MBALL) adduplicate_mball();
						else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) adduplicate_nurb();
					}
					else if(G.obpose){
						error ("Duplicate not possible in posemode.");
					}
					else adduplicate(0);
				}
				else if(G.qual & LR_ALTKEY) {
					if(G.obpose) error ("Duplicate not possible in posemode.");
					else
					if(G.obedit==0) adduplicate(0);
				}
				else if(G.qual & LR_CTRLKEY) {
					imagestodisplist();
				}
				else {
					pupval= pupmenu("Draw mode%t|BoundBox %x1|Wire %x2|OpenGL Solid %x3|Shaded Solid %x4");
					if(pupval>0) {
						G.vd->drawtype= pupval;
						doredraw= 1;
					
					}
				}
				
				break;
			case EKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) extrude_mesh();
					else if(G.obedit->type==OB_CURVE) addvert_Nurb('e');
					else if(G.obedit->type==OB_SURF) extrude_nurb();
					else if(G.obedit->type==OB_ARMATURE) extrude_armature();
				}
				else {
					ob= OBACT;
					if(ob && ob->type==OB_IKA) if(okee("extrude IKA")) extrude_ika(ob, 1);
				}
				break;
			case FKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) {
						if(G.qual & LR_SHIFTKEY) fill_mesh();
						else if(G.qual & LR_ALTKEY) beauty_fill();
						else if(G.qual & LR_CTRLKEY) edge_flip();
						else addedgevlak_mesh();
					}
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) addsegment_nurb();
				}
				else if(G.qual & LR_CTRLKEY) sort_faces();
				else if(G.qual & LR_SHIFTKEY) fly();
				else {
						set_faceselect();
					}
				
				break;
			case GKEY:
				/* RMGRP if(G.qual & LR_CTRLKEY) add_selected_to_group();
				else if(G.qual & LR_ALTKEY) rem_selected_from_group();
				else if(G.qual & LR_SHIFTKEY) group_menu();
				else */ 
				if(G.qual & LR_ALTKEY) clear_object('g');
				else
					transform('g');
				break;
			case HKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) {
						if(G.qual & LR_ALTKEY) reveal_mesh();
						else hide_mesh(G.qual & LR_SHIFTKEY);
					}
					else if(G.obedit->type== OB_SURF) {
						if(G.qual & LR_ALTKEY) revealNurb();
						else hideNurb(G.qual & LR_SHIFTKEY);
					}
					else if(G.obedit->type==OB_CURVE) {
					
						if(G.qual & LR_CTRLKEY) autocalchandlesNurb_all(1);	/* flag=1, selected */
						else if(G.qual & LR_SHIFTKEY) sethandlesNurb(1);
						else sethandlesNurb(3);
						
						makeDispList(G.obedit);
						
						allqueue(REDRAWVIEW3D, 0);
					}
				}
				else if(G.f & G_FACESELECT) hide_tface();
				
				break;
			case IKEY:
				break;
				
			case JKEY:
				if(G.qual & LR_CTRLKEY) {
					if( (ob= OBACT) ) {
						if(ob->type == OB_MESH) join_mesh();
						else if(ob->type == OB_CURVE) join_curve(OB_CURVE);
						else if(ob->type == OB_SURF) join_curve(OB_SURF);
						else if(ob->type == OB_ARMATURE) join_armature ();
					}
					else if (G.obedit && ELEM(G.obedit->type, OB_CURVE, OB_SURF)) addsegment_nurb();
				} else if(G.obedit) {
					if(G.obedit->type==OB_MESH) {
						join_triangles();
					}
				}

				break;
			case KKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_SURF) printknots();
				}
				else {
					if(G.qual & LR_SHIFTKEY) {
						if(G.f & G_FACESELECT) clear_vpaint_selectedfaces();
						else if(G.f & G_VERTEXPAINT) clear_vpaint();
						else select_select_keys();
					}
					else if(G.qual & LR_CTRLKEY) make_skeleton();
/* 					else if(G.qual & LR_ALTKEY) delete_skeleton(); */
					else set_ob_ipoflags();
				}
				
				break;
			case LKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) selectconnected_mesh();
					if(G.obedit->type==OB_ARMATURE) selectconnected_armature();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) selectconnected_nurb();
				}
				else if(G.obpose) {
					if(G.obpose->type==OB_ARMATURE) selectconnected_posearmature();
				}
				else {
				
					if(G.qual & LR_SHIFTKEY) selectlinks();
					else if(G.qual & LR_CTRLKEY) linkmenu();
					else if(G.f & G_FACESELECT) select_linked_tfaces();
					else make_local();
				}
				break;
			case MKEY:
				movetolayer();
				break;
			case NKEY:
				if(G.obedit) {
					switch (G.obedit->type){
					case OB_ARMATURE:
						if (okee("Recalc bone roll angles")) auto_align_armature();
						break;
					case OB_MESH: 
						if(G.qual & LR_SHIFTKEY) {
							if(okee("Recalc normals inside")) righthandfaces(2);
						}
						else {
							if(okee("Recalc normals outside")) righthandfaces(1);
						}
						break;
					}
					allqueue(REDRAWVIEW3D, 0);
				}
				break;
			case OKEY:
				if(G.qual & LR_ALTKEY) clear_object('o');
				else if(G.obedit) {
					extern int prop_mode;

					if (G.qual & LR_SHIFTKEY) prop_mode= !prop_mode;
					else G.f ^= G_PROPORTIONAL;

					allqueue(REDRAWHEADERS, 0);
				}
				break;
			case PKEY:
				
				if(G.obedit) {
					if(G.qual) {
						if(G.qual & LR_CTRLKEY) make_parent();
					}
					else if(G.obedit->type==OB_MESH) separate_mesh();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) separate_nurb();
				}
				else if(G.qual & LR_CTRLKEY) make_parent();
				else if(G.qual & LR_ALTKEY) clear_parent();
				else {
                	start_game();
				}
				break;
			case RKEY:
				if(G.obedit==0 && (G.f & G_FACESELECT)) rotate_uv_tface();
				else if(G.qual & LR_ALTKEY) clear_object('r');
				else if(G.qual & LR_SHIFTKEY) selectrow_nurb();
				else transform('r');
				break;
			case SKEY:
				if(G.qual & LR_ALTKEY) {
					if(G.obedit) transform('N');	/* scale by vertex normal */
					else clear_object('s');
				}
				else if(G.qual & LR_SHIFTKEY) snapmenu();
				else if(G.qual & LR_CTRLKEY) {
					if(G.obedit) transform('S');
				}
				else transform('s');
				break;
			case TKEY:
				if(G.qual & LR_CTRLKEY) {
					if(G.obedit) {
						if(G.obedit->type==OB_MESH) {
							convert_to_triface(0);
							allqueue(REDRAWVIEW3D, 0);
							countall();
							makeDispList(G.obedit);
						}
					}
					else make_track();
				}
				else if(G.qual & LR_ALTKEY) {
					if(G.obedit && G.obedit->type==OB_CURVE) clear_tilt();
					else clear_track();
				}
				else {
					if(G.obedit) transform('t');
					else texspace_edit();
				}
				
				break;
			case UKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) remake_editMesh();
					else if(G.obedit->type==OB_ARMATURE) remake_editArmature();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) remake_editNurb();
					else if(G.obedit->type==OB_LATTICE) remake_editLatt();
				}
				else if(G.f & G_FACESELECT) uv_autocalc_tface();
				else if(G.f & G_WEIGHTPAINT) wpaint_undo();
				else if(G.f & G_VERTEXPAINT) vpaint_undo();
				else single_user();
				
				break;
			case VKEY:
				if(G.qual==LR_SHIFTKEY) {
					if (G.obedit && G.obedit->type==OB_MESH) {
						align_view_to_selected(v3d);
					} else if (G.f & G_FACESELECT) {
						align_view_to_selected(v3d);
					}
				} else {
					if(G.obedit) {
						if(G.obedit->type==OB_CURVE) {
							sethandlesNurb(2);
							makeDispList(G.obedit);
							allqueue(REDRAWVIEW3D, 0);
						}
					}
					else if(G.qual & LR_ALTKEY) image_aspect();
					else set_vpaint();
				}
				break;
			case WKEY:
				if(G.qual & LR_SHIFTKEY) {
					transform('w');
				}
				else if(G.qual & LR_ALTKEY) {
					/* if(G.obedit && G.obedit->type==OB_MESH) write_videoscape(); */
				}
				else if(G.qual & LR_CTRLKEY) {
					if(G.obedit) {
						if ELEM(G.obedit->type,  OB_CURVE, OB_SURF) {
							switchdirectionNurb2();
						}
					}
				}
				else special_editmenu();
				
				break;
			case XKEY:
			case DELKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) delete_mesh();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) delNurb();
					else if(G.obedit->type==OB_MBALL) delete_mball();
					else if (G.obedit->type==OB_ARMATURE) delete_armature();
				}
				else delete_obj(0);
				break;
			case YKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) split_mesh();
				}
				break;
			case ZKEY:
				if(G.qual & LR_CTRLKEY) {
					reshadeall_displist();
					G.vd->drawtype= OB_SHADED;
				}
				else if(G.qual & LR_SHIFTKEY) {
					if(G.vd->drawtype== OB_SHADED) G.vd->drawtype= OB_WIRE;
					else G.vd->drawtype= OB_SHADED;
				}
				else if(G.qual & LR_ALTKEY) {
					if(G.vd->drawtype== OB_TEXTURE) G.vd->drawtype= OB_SOLID;
					else G.vd->drawtype= OB_TEXTURE;
				}
				else {
					if(G.vd->drawtype==OB_SOLID || G.vd->drawtype==OB_SHADED) G.vd->drawtype= OB_WIRE;
					else G.vd->drawtype= OB_SOLID;
				}
				
				
				scrarea_queue_headredraw(curarea);
				scrarea_queue_winredraw(curarea);
				break;
				
			
			case HOMEKEY:
				view3d_home(0);
				break;
			case COMMAKEY:
				G.vd->around= V3D_CENTRE;
				scrarea_queue_headredraw(curarea);
				break;
				
			case PERIODKEY:
				G.vd->around= V3D_CURSOR;
				scrarea_queue_headredraw(curarea);
				break;
			
			case PADSLASHKEY:
				if(G.vd->localview) {
					G.vd->localview= 0;
					endlocalview(curarea);
				}
				else {
					G.vd->localview= 1;
					initlocalview();
				}
				scrarea_queue_headredraw(curarea);
				break;
			case PADASTERKEY:	/* '*' */
				ob= OBACT;
				if(ob) {
					obmat_to_viewmat(ob);
					if(G.vd->persp==2) G.vd->persp= 1;
					scrarea_queue_winredraw(curarea);
				}
				break;
			case PADPERIOD:	/* '.' */
				centreview();
				break;
			
			case PAGEUPKEY:
				if(G.qual & LR_CTRLKEY) movekey_obipo(1);
				else nextkey_obipo(1);	/* in editipo.c */
				break;

			case PAGEDOWNKEY:
				if(G.qual & LR_CTRLKEY) movekey_obipo(-1);
				else nextkey_obipo(-1);
				break;
				
			case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
			case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
			case PADMINUS: case PADPLUSKEY: case PADENTER:
				persptoetsen(event);
				doredraw= 1;
				break;
			
			case ESCKEY:
				if (G.vd->flag & V3D_DISPIMAGE) {
					G.vd->flag &= ~V3D_DISPIMAGE;
					doredraw= 1;
				}
				break;
			}
		}
	}
	
	if(doredraw) {
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
	}
}

void initview3d(ScrArea *sa)
{
	View3D *vd;
	
	vd= MEM_callocN(sizeof(View3D), "initview3d");
	BLI_addhead(&sa->spacedata, vd);	/* addhead! niet addtail */

	vd->spacetype= SPACE_VIEW3D;
	vd->viewquat[0]= 1.0;
	vd->viewquat[1]= vd->viewquat[2]= vd->viewquat[3]= 0.0;
	vd->persp= 1;
	vd->drawtype= OB_WIRE;
	vd->view= 7;
	vd->dist= 10.0;
	vd->lens= 35.0;
	vd->near= 0.01;
	vd->far= 500.0;
	vd->grid= 1.0;
	vd->gridlines= 16;
	vd->lay= vd->layact= 1;
	if(G.scene) {
		vd->lay= vd->layact= G.scene->lay;
		vd->camera= G.scene->camera;
	}
	vd->scenelock= 1;
}


/* ******************** SPACE: IPO ********************** */

void changeview2d()
{
	if(G.v2d==0) return;

	test_view2d(G.v2d, curarea->winx, curarea->winy);
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
}

void winqreadipo(unsigned short event, short val, char ascii)
{
	SpaceIpo *sipo= curarea->spacedata.first;
	View2D *v2d= &sipo->v2d;
	float dx, dy;
	int cfra, doredraw= 0;
	short mval[2];
	
	if(curarea->win==0) return;

	if(val) {
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case UI_BUT_EVENT:
			if(val>0) do_ipowin_buts(val-1);
			break;
		case LEFTMOUSE:
			if( in_ipo_buttons() ) {
				do_ipo_selectbuttons();
				doredraw= 1;
			}			
			else if(G.qual & LR_CTRLKEY) add_vert_ipo();
			else {
				do {
					getmouseco_areawin(mval);
					areamouseco_to_ipoco(v2d, mval, &dx, &dy);
					
					cfra= (int)dx;
					if(cfra< 1) cfra= 1;
					
					if( cfra!=CFRA ) {
						CFRA= cfra;
						update_for_newframe();
						force_draw_plus(SPACE_VIEW3D);
						force_draw_plus(SPACE_ACTION);
						force_draw_plus(SPACE_BUTS);	/* To make constraint sliders redraw */
					}
				
				} while(get_mbut()&L_MOUSE);
			}
			
			break;
		case MIDDLEMOUSE:
			if(in_ipo_buttons()) {
				scroll_ipobuts();
			}
			else view2dmove(event);	/* in drawipo.c */
			break;

		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;

		case RIGHTMOUSE:
			mouse_select_ipo();
			allqueue (REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			break;
		case PADPLUSKEY:

			dx= 0.1154*(v2d->cur.xmax-v2d->cur.xmin);
			dy= 0.1154*(v2d->cur.ymax-v2d->cur.ymin);
			if(val==SPACE_BUTS) {
				dx/=2.0; dy/= 2.0;
			}
			v2d->cur.xmin+= dx;
			v2d->cur.xmax-= dx;
			v2d->cur.ymin+= dy;
			v2d->cur.ymax-= dy;
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			doredraw= 1;
			break;
		case PADMINUS:
			dx= 0.15*(v2d->cur.xmax-v2d->cur.xmin);
			dy= 0.15*(v2d->cur.ymax-v2d->cur.ymin);
			if(val==SPACE_BUTS) {
				dx/=2.0; dy/= 2.0;
			}
			v2d->cur.xmin-= dx;
			v2d->cur.xmax+= dx;
			v2d->cur.ymin-= dy;
			v2d->cur.ymax+= dy;
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			doredraw= 1;
			break;
		case PAGEUPKEY:
			if(G.qual & LR_CTRLKEY) movekey_ipo(1);
			else nextkey_ipo(1);
			break;
		case PAGEDOWNKEY:
			if(G.qual & LR_CTRLKEY) movekey_ipo(-1);
			else nextkey_ipo(-1);
			break;
		case HOMEKEY:
			do_ipo_buttons(B_IPOHOME);
			break;
			
		case AKEY:
			if(in_ipo_buttons()) {
				swap_visible_editipo();
			}
			else swap_selectall_editipo();
			allspace (REMAKEIPO, 0);
			allqueue (REDRAWNLA, 0);
			allqueue (REDRAWACTION, 0);
			break;
		case BKEY:
			borderselect_ipo();
			break;
		case CKEY:
			move_to_frame();
			break;
		case DKEY:
			if(G.qual & LR_SHIFTKEY) add_duplicate_editipo();
			break;
		case GKEY:
			transform_ipo('g');
			break;
		case HKEY:
			if(G.qual & LR_SHIFTKEY) sethandles_ipo(HD_AUTO);
			else sethandles_ipo(HD_ALIGN);
			break;
		case JKEY:
			join_ipo();
			break;
		case KKEY:
			if(G.sipo->showkey) {
				G.sipo->showkey= 0;
				swap_selectall_editipo();	/* sel all */
			}
			else G.sipo->showkey= 1;
			free_ipokey(&G.sipo->ipokey);
			if(G.sipo->ipo) G.sipo->ipo->showkey= G.sipo->showkey;

			scrarea_queue_headredraw(curarea);
			allqueue(REDRAWVIEW3D, 0);
			doredraw= 1;
			break;
		case RKEY:
			ipo_record();
			break;
		case SKEY:
			if(G.qual & LR_SHIFTKEY) ipo_snapmenu();
			else transform_ipo('s');
			break;
		case TKEY:
			set_ipotype();
			break;
		case VKEY:
			sethandles_ipo(HD_VECT);
			break;
		case XKEY:
		case DELKEY:
			if(G.qual & LR_SHIFTKEY) delete_key();
			else del_ipo();
			break;
		}
	}

	if(doredraw) scrarea_queue_winredraw(curarea);
}

void initipo(ScrArea *sa)
{
	SpaceIpo *sipo;
	
	sipo= MEM_callocN(sizeof(SpaceIpo), "initipo");
	BLI_addhead(&sa->spacedata, sipo);

	sipo->spacetype= SPACE_IPO;
	/* sipo space loopt van (0,-?) tot (??,?) */
	sipo->v2d.tot.xmin= 0.0;
	sipo->v2d.tot.ymin= -10.0;
	sipo->v2d.tot.xmax= G.scene->r.efra;
	sipo->v2d.tot.ymax= 10.0;

	sipo->v2d.cur= sipo->v2d.tot;

	sipo->v2d.min[0]= 0.01;
	sipo->v2d.min[1]= 0.01;

	sipo->v2d.max[0]= 15000.0;
	sipo->v2d.max[1]= 10000.0;
	
	sipo->v2d.scroll= L_SCROLL+B_SCROLL;
	sipo->v2d.keeptot= 0;

	sipo->blocktype= ID_OB;
}

/* ******************** SPACE: INFO ********************** */

void space_mipmap_button_function(int event) {
	set_mipmap(!(U.gameflags & USERDEF_DISABLE_MIPMAP));

	allqueue(REDRAWVIEW3D, 0);
}

void space_sound_button_function(int event)
{
	int a;
	SYS_SystemHandle syshandle;

	if ((syshandle = SYS_GetSystem()))
	{
		a = (U.gameflags & USERDEF_DISABLE_SOUND);
		SYS_WriteCommandLineInt(syshandle, "noaudio", a);
	}
}

void drawinfospace(void)
{
	uiBlock *block;
	float fac;
	int dx;
	char naam[32];

	if(curarea->win==0) return;

	glClearColor(0.5, 0.5, 0.5, 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);

	fac= ((float)curarea->winx)/1280.0;
	myortho2(0.0, 1280.0, 0.0, curarea->winy/fac);
	
	sprintf(naam, "infowin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->win);
	
	uiBlockSetCol(block, BUTBLUE);
	uiDefButS(block, TOG|BIT|0, B_RESETAUTOSAVE, "Auto Temp Save", 45,32,126,20, &(U.flag), 0, 0, 0, 0, "Enables/Disables the automatic temp. file saving");
	uiBlockSetCol(block, BUTGREY);
	uiDefBut(block, TEX, 0, "Dir:",	45,10,127,20, U.tempdir, 1.0, 63.0, 0, 0, "The directory for temp. files");
	uiDefButI(block, NUM, B_RESETAUTOSAVE, "Time:", 174,32,91,20, &(U.savetime), 1.0, 60.0, 0, 0, "The time in minutes to wait between temp. saves");
	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_LOADTEMP, "Load Temp", 174,10,90,20, 0, 0, 0, 0, 0, "Loads the most recently saved temp file");

	uiBlockSetCol(block, BUTGREY);
	uiDefButS(block, NUM, 0, "Versions:", 281,10,86,42, &U.versions, 0.0, 32.0, 0, 0, "The number of old versions to maintain when saving");

	uiBlockSetCol(block, BUTYELLOW);
	uiDefButI(block, TOG|BIT|USERDEF_VERTEX_ARRAYS_BIT, 0, "Vertex arrays",
			 389,54,86,20, &(U.gameflags), 0, 0, 0, 0,
			 "Toggle between vertex arrays on (less reliable) and off (more reliable)");
	uiDefButI(block, TOG|BIT|USERDEF_DISABLE_SOUND_BIT, B_SOUNDTOGGLE, "No sound",
			 478,54,86,20, &(U.gameflags), 0, 0, 0, 0,
			 "Toggle between sound on and sound off");
	
	uiDefButI(block, TOG|BIT|USERDEF_DISABLE_MIPMAP_BIT, B_MIPMAPCHANGED, "No Mipmaps",
				   569,54,78,20, &(U.gameflags), 0, 0, 0, 0,
				   "Toggle between Mipmap textures on (beautiful) and off (fast)");
	
	uiBlockSetCol(block, BUTGREEN);
	uiDefButS(block, TOG|BIT|4, 0, "Scene Global",
			 389,32,86,20, &(U.flag), 0, 0, 0, 0,
			 "Forces the current Scene to be displayed in all Screens");
	uiDefButS(block, TOG|BIT|5, 0, "TrackBall",
			 389,10,86,20, &(U.flag), 0, 0, 0, 0,
			 "Switches between trackball and turntable view rotation methods (MiddleMouse)");
	uiDefButS(block, TOG|BIT|12, 0, "2-Mouse",
			 478,10,86,20, &(U.flag), 0, 0, 0, 0,
			 "Maps ALT+LeftMouse to MiddleMouse button");
	uiDefButS(block, TOG|BIT|8, 0, "Mat on Obj",
			 569,9,78,20, &(U.flag), 0, 0, 0, 0,
			 "Sets whether Material data is linked to Obj or ObjData");
	uiDefButS(block, TOG|BIT|9, B_U_CAPSLOCK, "NoCapsLock",
			 478,32,86,20, &(U.flag), 0, 0, 0, 0,
			 "Deactives the CapsLock button (only applies to text input)");
	uiDefButS(block, TOG|BIT|10, 0, "Viewmove",
			 569,32,78,20, &(U.flag), 0, 0, 0, 0,
			 "Sets the default action for the middle mouse button");

	uiDefButS(block, TOG|BIT|13, 0, "noNumpad",
			 653,10,76,20, &(U.flag), 0, 0, 0, 0, 
			 "For laptops: keys 1 to 0 become numpad keys");
	uiDefButS(block, TOG|BIT|11, 0, "ToolTips",
			 653,32,76,20, &(U.flag), 0, 0, 0, 0,
			 "Enables/Disables tooltips");
	
//	uiDefButS(block, ICONTOG|BIT|14, 0, ICON(),	733,10,50,42, &(U.flag), 0, 0, 0, 0, "Automatic keyframe insertion");
	uiDefButS(block, TOG|BIT|0, 0, "KeyAC",	733,32,50,20, &(U.uiflag), 0, 0, 0, 0, "Automatic keyframe insertion for actions");
	uiDefButS(block, TOG|BIT|1, 0, "KeyOB",	733,10,50,20, &(U.uiflag), 0, 0, 0, 0, "Automatic keyframe insertion for objects");

	uiDefButS(block, TOG|BIT|1, 0, "Grab Grid",	788,32,106,20, &(U.flag), 0, 0, 0, 0, "Changes default step mode for grabbing");
	uiDefButS(block, TOG|BIT|2, 0, "Rot",		842,10,52,20, &(U.flag), 0, 0, 0, 0, "Changes default step mode for rotation");
	uiDefButS(block, TOG|BIT|3, 0, "Size",		788,10,52,20, &(U.flag), 0, 0, 0, 0, "Changes default step mode for scaling");

	uiDefButS(block, TOG|BIT|0, 0, "Dupli Mesh",	902,32,90,20, &(U.dupflag), 0, 0, 0, 0, "Causes Mesh data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|9, 0, "Armature",	902,10,90,20, &(U.dupflag), 0, 0, 0, 0, "Causes Armature data to be duplicated with Shift+D");

	uiDefButS(block, TOG|BIT|1, 0, "Curve",		995,32,50,20, &(U.dupflag), 0, 0, 0, 0, "Causes Curve data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|2, 0, "Surf",		995,10,50,20, &(U.dupflag), 0, 0, 0, 0, "Causes Surface data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|3, 0, "Text",		1048,32,50,20, &(U.dupflag), 0, 0, 0, 0, "Causes Text data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|4, 0, "MBall",		1048,10,50,20, &(U.dupflag), 0, 0, 0, 0, "Causes Metaball data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|5, 0, "Lamp",		1101,32,50,20, &(U.dupflag), 0, 0, 0, 0, "Causes Lamp data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|6, 0, "Ipo",			1101,10,50,20, &(U.dupflag), 0, 0, 0, 0, "Causes Ipo data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|7, 0, "Material",	1153,32,70,20, &(U.dupflag), 0, 0, 0, 0, "Causes Material data to be duplicated with Shift+D");
	uiDefButS(block, TOG|BIT|8, 0, "Texture",		1153,10,70,20, &(U.dupflag), 0, 0, 0, 0, "Causes Texture data to be duplicated with Shift+D");


	uiBlockSetCol(block, BUTGREY);

	uiDefButI(block, NUM, 0, "WLines",
			1153,54,70,20, &U.wheellinescroll,
			0.0, 32.0, 0, 0,
			"Mousewheel: The number of lines that get scrolled");
	uiDefButS(block, TOG|BIT|2, 0, "WZoom",
			1081,54,70,20, &(U.uiflag), 0, 0, 0, 0,
			"Mousewheel: Swaps mousewheel zoom direction");

	dx= (1280-90)/6;


#define _XPOS_ 45
#define _YPOS_ 80
#define _BUTH_ 20
#define _RULESPACE_ 2
	uiDefBut(block, TEX, 0, "Python:",
			 _XPOS_,_YPOS_-_BUTH_-_RULESPACE_,(short)dx,_BUTH_, U.pythondir, 1.0, 63.0, 0, 0,
			 "The default directory for Python scripts");
	uiDefBut(block, TEX, 0, "Fonts:",
			 _XPOS_,_YPOS_,(short)dx,_BUTH_, U.fontdir, 1.0, 63.0, 0, 0,
			 "The default directory to search when loading fonts");
	uiDefBut(block, TEX, 0, "Render:",
			 (short)(_XPOS_+dx),_YPOS_,(short)dx,_BUTH_, U.renderdir, 1.0, 63.0, 0, 0,
			 "The default directory to choose for rendering");
	uiDefBut(block, TEX, 0, "Textures:",
			 (short)(_XPOS_+2*dx),_YPOS_,(short)dx,_BUTH_, U.textudir, 1.0, 63.0, 0, 0,
			 "The default directory to search when loading textures");
	uiDefBut(block, TEX, 0, "TexPlugin:",
			 (short)(_XPOS_+3*dx),_YPOS_,(short)dx,_BUTH_, U.plugtexdir, 1.0, 63.0, 0, 0,
			 "The default directory to search when loading texture plugins");
	uiDefBut(block, TEX, 0, "SeqPlugin:",
			 (short)(_XPOS_+4*dx),_YPOS_,(short)dx,_BUTH_, U.plugseqdir, 1.0, 63.0, 0, 0,
			 "The default directory to search when loading sequence plugins");
	uiDefBut(block, TEX, 0, "Sounds:",
			 (short)(_XPOS_+5*dx),_YPOS_,(short)dx,_BUTH_, U.sounddir, 1.0, 63.0, 0, 0,
			 "The default directory to search when loading sounds");
#undef _XPOS_
#undef _YPOS_
#undef _BUTH_
#undef _RULESPACE_
	uiDrawBlock(block);
}

void winqreadinfospace(unsigned short event, short val, char ascii)
{
	if(val) {
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case UI_BUT_EVENT:
			do_global_buttons(val);
			
			break;	
		}
	}
}

void init_infospace(ScrArea *sa)
{
	SpaceInfo *sinfo;
	
	sinfo= MEM_callocN(sizeof(SpaceInfo), "initinfo");
	BLI_addhead(&sa->spacedata, sinfo);
}

/* ******************** SPACE: BUTS ********************** */

extern void drawbutspace(void);	/* buttons.c */

void changebutspace(void)
{
	if(G.v2d==0) return;
	
	test_view2d(G.v2d, curarea->winx, curarea->winy);
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
}

void winqreadbutspace(unsigned short event, short val, char ascii)
{
	SpaceButs *sbuts= curarea->spacedata.first;
	ScrArea *sa, *sa3d;
	int doredraw= 0;

	if(val) {
		
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case UI_BUT_EVENT:
			do_blenderbuttons(val);
			break;
			
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;

		case PADPLUSKEY:
		case PADMINUS:
			val= SPACE_BUTS;
			winqreadipo(event, val, 0);
			break;
		case RENDERPREVIEW:
			BIF_previewrender(sbuts);
			break;
		
		case HOMEKEY:
			do_buts_buttons(B_BUTSHOME);
			break;


		/* if only 1 view, also de persp, excluding arrowkeys */
		case PAD0: case PAD1: case PAD3:
		case PAD5: case PAD7: case PAD9:
		case PADENTER: case ZKEY: case PKEY:
			sa3d= 0;
			sa= G.curscreen->areabase.first;
			while(sa) {
				if(sa->spacetype==SPACE_VIEW3D) {
					if(sa3d) return;
					sa3d= sa;
				}
				sa= sa->next;
			}
			if(sa3d) {
				sa= curarea;
				areawinset(sa3d->win);
				
				if(event==PKEY) start_game();
				else if(event==ZKEY) winqread3d(event, val, 0);
				else persptoetsen(event);
				
				scrarea_queue_winredraw(sa3d);
				scrarea_queue_headredraw(sa3d);
				areawinset(sa->win);
			}
		}
	}

	if(doredraw) scrarea_queue_winredraw(curarea);
}

void set_rects_butspace(SpaceButs *buts)
{
	/* buts space loopt van (0,0) tot (1280, 228) */

	buts->v2d.tot.xmin= 0.0;
	buts->v2d.tot.ymin= 0.0;
	buts->v2d.tot.xmax= 1279.0;
	buts->v2d.tot.ymax= 228.0;
	
	buts->v2d.min[0]= 256.0;
	buts->v2d.min[1]= 42.0;

	buts->v2d.max[0]= 1600.0;
	buts->v2d.max[1]= 450.0;
	
	buts->v2d.minzoom= 0.5;
	buts->v2d.maxzoom= 1.41;
	
	buts->v2d.scroll= 0;
	buts->v2d.keepaspect= 1;
	buts->v2d.keepzoom= 1;
	buts->v2d.keeptot= 1;
	
}

void test_butspace(void)
{
	ScrArea *area= curarea;
	int blocksmin= uiBlocksGetYMin(&area->uiblocks)-10.0;
	
	G.buts->v2d.tot.ymin= MIN2(0.0, blocksmin-10.0);
}

void init_butspace(ScrArea *sa)
{
	SpaceButs *buts;
	
	buts= MEM_callocN(sizeof(SpaceButs), "initbuts");
	BLI_addhead(&sa->spacedata, buts);

	buts->spacetype= SPACE_BUTS;
	buts->scaflag= BUTS_SENS_LINK|BUTS_SENS_ACT|BUTS_CONT_ACT|BUTS_ACT_ACT|BUTS_ACT_LINK;

	/* set_rects doet alleen defaults, zodat na het filezen de cur niet verandert */
	set_rects_butspace(buts);
	buts->v2d.cur= buts->v2d.tot;
}

void extern_set_butspace(int fkey)
{
	ScrArea *sa;
	SpaceButs *sbuts;
	
	/* als een ftoets ingedrukt: de dichtsbijzijnde buttonwindow wordt gezet */
	if(curarea->spacetype==SPACE_BUTS) sa= curarea;
	else {
		/* area vinden */
		sa= G.curscreen->areabase.first;
		while(sa) {
			if(sa->spacetype==SPACE_BUTS) break;
			sa= sa->next;
		}
	}
	
	if(sa==0) return;
	
	if(sa!=curarea) areawinset(sa->win);
	
	sbuts= sa->spacedata.first;
	
	if(fkey==F4KEY) sbuts->mainb= BUTS_LAMP;
	else if(fkey==F5KEY) sbuts->mainb= BUTS_MAT;
	else if(fkey==F6KEY) sbuts->mainb= BUTS_TEX;
	else if(fkey==F7KEY) sbuts->mainb= BUTS_ANIM;
	else if(fkey==F8KEY) sbuts->mainb= BUTS_GAME;
	else if(fkey==F9KEY) sbuts->mainb= BUTS_EDIT;
	else if(fkey==F10KEY) sbuts->mainb= BUTS_RENDER;

	scrarea_queue_headredraw(sa);
	scrarea_queue_winredraw(sa);
	BIF_preview_changed(sbuts);
}

/* ******************** SPACE: SEQUENCE ********************** */

/*  extern void drawseqspace(); BIF_drawseq.h */

void winqreadsequence(unsigned short event, short val, char ascii)
{
	SpaceSeq *sseq= curarea->spacedata.first;
	View2D *v2d= &sseq->v2d;
	extern Sequence *last_seq;
	float dx, dy;
	int doredraw= 0, cfra, first;
	short mval[2];
	
	if(curarea->win==0) return;

	if(val) {
		
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case LEFTMOUSE:
			if(sseq->mainb || view2dmove(event)==0) {
				
				first= 1;		
				set_special_seq_update(1);

				do {
					getmouseco_areawin(mval);
					areamouseco_to_ipoco(v2d, mval, &dx, &dy);
					
					cfra= (int)dx;
					if(cfra< 1) cfra= 1;
					/* else if(cfra> EFRA) cfra= EFRA; */
					
					if( cfra!=CFRA || first ) {
						first= 0;
				
						CFRA= cfra;
						force_draw();
					}
				
				} while(get_mbut()&L_MOUSE);
				
				set_special_seq_update(0);
				
				update_for_newframe();
			}
			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			if(sseq->mainb) break;
			view2dmove(event);	/* in drawipo.c */
			break;
		case RIGHTMOUSE:
			if(sseq->mainb) break;
			mouse_select_seq();
			break;
		case PADPLUSKEY:
			if(sseq->mainb) {
				sseq->zoom++;
				if(sseq->zoom>8) sseq->zoom= 8;
			}
			else {
				if(G.qual) {
					if(G.qual & LR_SHIFTKEY) insert_gap(25, CFRA);
					else if(G.qual & LR_ALTKEY) insert_gap(250, CFRA);
					allqueue(REDRAWSEQ, 0);
				}
				else {
					dx= 0.1154*(v2d->cur.xmax-v2d->cur.xmin);
					v2d->cur.xmin+= dx;
					v2d->cur.xmax-= dx;
					test_view2d(G.v2d, curarea->winx, curarea->winy);
				}
			}
			doredraw= 1;
			break;
		case PADMINUS:
			if(sseq->mainb) {
				sseq->zoom--;
				if(sseq->zoom<1) sseq->zoom= 1;
			}
			else {
				if(G.qual) {
					if(G.qual & LR_SHIFTKEY) no_gaps();
				}
				else {
					dx= 0.15*(v2d->cur.xmax-v2d->cur.xmin);
					v2d->cur.xmin-= dx;
					v2d->cur.xmax+= dx;
					test_view2d(G.v2d, curarea->winx, curarea->winy);
				}
			}
			doredraw= 1;
			break;
		case HOMEKEY:
			do_seq_buttons(B_SEQHOME);
			break;
		case PADPERIOD:	
			if(last_seq) {
				CFRA= last_seq->startdisp;
				v2d->cur.xmin= last_seq->startdisp- (last_seq->len/20);
				v2d->cur.xmax= last_seq->enddisp+ (last_seq->len/20);
				update_for_newframe();
			}
			break;
			
		case AKEY:
			if(sseq->mainb) break;
			if(G.qual & LR_SHIFTKEY) {
				add_sequence(0);
			}
			else swap_select_seq();
			break;
		case BKEY:
			if(sseq->mainb) break;
			borderselect_seq();
			break;
		case CKEY:
			if(last_seq && (last_seq->flag & (SEQ_LEFTSEL+SEQ_RIGHTSEL))) {
				if(last_seq->flag & SEQ_LEFTSEL) CFRA= last_seq->startdisp;
				else CFRA= last_seq->enddisp-1;
				
				dx= CFRA-(v2d->cur.xmax+v2d->cur.xmin)/2;
				v2d->cur.xmax+= dx;
				v2d->cur.xmin+= dx;
				update_for_newframe();
			}
			else change_sequence();
			break;
		case DKEY:
			if(sseq->mainb) break;
			if(G.qual & LR_SHIFTKEY) add_duplicate_seq();
			break;
		case EKEY:
			break;
		case FKEY:
			set_filter_seq();
			break;
		case GKEY:
			if(sseq->mainb) break;
			transform_seq('g');
			break;
		case MKEY:
			if(G.qual & LR_ALTKEY) un_meta();
			else make_meta();
			break;
		case SKEY:
			if(G.qual & LR_SHIFTKEY) seq_snapmenu();
			break;
		case TKEY:
			touch_seq_files();
			break;
		case XKEY:
		case DELKEY:
			if(sseq->mainb) break;
			del_seq();
			break;
		}
	}

	if(doredraw) scrarea_queue_winredraw(curarea);
}


void init_seqspace(ScrArea *sa)
{
	SpaceSeq *sseq;
	
	sseq= MEM_callocN(sizeof(SpaceSeq), "initseqspace");
	BLI_addhead(&sa->spacedata, sseq);

	sseq->spacetype= SPACE_SEQ;
	sseq->zoom= 1;
	
	/* seq space loopt van (0,8) tot (250, 0) */

	sseq->v2d.tot.xmin= 0.0;
	sseq->v2d.tot.ymin= 0.0;
	sseq->v2d.tot.xmax= 250.0;
	sseq->v2d.tot.ymax= 8.0;
	
	sseq->v2d.cur= sseq->v2d.tot;

	sseq->v2d.min[0]= 10.0;
	sseq->v2d.min[1]= 4.0;

	sseq->v2d.max[0]= 32000.0;
	sseq->v2d.max[1]= MAXSEQ;
	
	sseq->v2d.minzoom= 0.1;
	sseq->v2d.maxzoom= 10.0;
	
	sseq->v2d.scroll= L_SCROLL+B_SCROLL;
	sseq->v2d.keepaspect= 0;
	sseq->v2d.keepzoom= 0;
	sseq->v2d.keeptot= 0;
}

/* ******************** SPACE: ACTION ********************** */
extern void drawactionspace(void);
extern void winqreadactionspace(unsigned short, short, char ascii);

void init_actionspace(ScrArea *sa)
{
	SpaceAction *saction;
	
	saction= MEM_callocN(sizeof(SpaceAction), "initactionspace");
	BLI_addhead(&sa->spacedata, saction);

	saction->spacetype= SPACE_ACTION;

	saction->v2d.tot.xmin= 1.0;
	saction->v2d.tot.ymin=	0.0;
	saction->v2d.tot.xmax= 1000.0;
	saction->v2d.tot.ymax= 1000.0;
	
	saction->v2d.cur.xmin= -5.0;
	saction->v2d.cur.ymin= 0.0;
	saction->v2d.cur.xmax= 65.0;
	saction->v2d.cur.ymax= 1000.0;


	saction->v2d.min[0]= 0.0;
	saction->v2d.min[1]= 0.0;

	saction->v2d.max[0]= 1000.0;
	saction->v2d.max[1]= 1000.0;
	
	saction->v2d.minzoom= 0.1;
	saction->v2d.maxzoom= 10;

	saction->v2d.scroll= R_SCROLL+B_SCROLL;
	saction->v2d.keepaspect= 0;
	saction->v2d.keepzoom= V2D_LOCKZOOM_Y;
	saction->v2d.keeptot= 0;
	
}

void free_actionspace(SpaceAction *saction)
{
	/* don't free saction itself */
	
	/* __PINFAKE */
/*	if (saction->flag & SACTION_PIN)
		if (saction->action)
			saction->action->id.us --;

*/	/* end PINFAKE */
}


/* ******************** SPACE: FILE ********************** */

void init_filespace(ScrArea *sa)
{
	SpaceFile *sfile;
	
	sfile= MEM_callocN(sizeof(SpaceFile), "initfilespace");
	BLI_addhead(&sa->spacedata, sfile);

	sfile->dir[0]= '/';
	sfile->type= FILE_UNIX;

	sfile->spacetype= SPACE_FILE;
}

void init_textspace(ScrArea *sa)
{
	SpaceText *st;
	
	st= MEM_callocN(sizeof(SpaceText), "inittextspace");
	BLI_addhead(&sa->spacedata, st);

	st->spacetype= SPACE_TEXT;	
	
	st->text= NULL;
	st->flags= 0;
	
	st->font_id= 5;
	st->lheight= 12;
	st->showlinenrs= 0;
	
	st->top= 0;
}

void init_imaselspace(ScrArea *sa)
{
	SpaceImaSel *simasel;
	
	simasel= MEM_callocN(sizeof(SpaceImaSel), "initimaselspace");
	BLI_addhead(&sa->spacedata, simasel);

	simasel->spacetype= SPACE_IMASEL;
	
	simasel->mode = 7;
	strcpy (simasel->dir,  U.textudir);	/* TON */
	strcpy (simasel->file, "");
	strcpy(simasel->fole, simasel->file);
	strcpy(simasel->dor,  simasel->dir);

	simasel->first_sel_ima	=  0;
	simasel->hilite_ima	    =  0;
	simasel->firstdir		=  0;
	simasel->firstfile		=  0;
	simasel->cmap           =  0;
	simasel->returnfunc     =  0;
	
	simasel->title[0]       =  0;
	
	clear_ima_dir(simasel);
	
	// simasel->cmap= IMB_loadiffmem((int*)datatoc_cmap_tga, IB_rect|IB_cmap);
	simasel->cmap= IMB_ibImageFromMemory((int *)datatoc_cmap_tga, datatoc_cmap_tga_size, IB_rect|IB_cmap);
	if (!simasel->cmap) {
		error("in console");
		printf("Image select cmap file not found \n");
	}
}

/* ******************** SPACE: SOUND ********************** */

extern void drawsoundspace(void);
extern void winqreadsoundspace(unsigned short, short, char ascii);

void init_soundspace(ScrArea *sa)
{
	SpaceSound *ssound;
	
	ssound= MEM_callocN(sizeof(SpaceSound), "initsoundspace");
	BLI_addhead(&sa->spacedata, ssound);

	ssound->spacetype= SPACE_SOUND;
	
	/* sound space loopt van (0,8) tot (250, 0) */

	ssound->v2d.tot.xmin= -4.0;
	ssound->v2d.tot.ymin= -4.0;
	ssound->v2d.tot.xmax= 250.0;
	ssound->v2d.tot.ymax= 255.0;
	
	ssound->v2d.cur.xmin= -4.0;
	ssound->v2d.cur.ymin= -4.0;
	ssound->v2d.cur.xmax= 50.0;
	ssound->v2d.cur.ymax= 255.0;

	ssound->v2d.min[0]= 1.0;
	ssound->v2d.min[1]= 259.0;

	ssound->v2d.max[0]= 32000.0;
	ssound->v2d.max[1]= 259;
	
	ssound->v2d.minzoom= 0.1;
	ssound->v2d.maxzoom= 10.0;
	
	ssound->v2d.scroll= B_SCROLL;
	ssound->v2d.keepaspect= 0;
	ssound->v2d.keepzoom= 0;
	ssound->v2d.keeptot= 0;
	
}

void free_soundspace(SpaceSound *ssound)
{
	/* don't free ssound itself */
	
	
}

/* ******************** SPACE: IMAGE ********************** */

/*  extern void drawimagespace(); BIF_drawimage.h */

void winqreadimagespace(unsigned short event, short val, char ascii)
{
	SpaceImage *sima= curarea->spacedata.first;
	View2D *v2d= &sima->v2d;
#ifdef NAN_TPT
	IMG_BrushPtr brush;
	IMG_CanvasPtr canvas;
	int rowBytes;
	short xy_prev[2], xy_curr[2];
	float uv_prev[2], uv_curr[2];
	extern VPaint Gvp;
#endif /* NAN_TPT */	
	if(val==0) return;

	if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;
	
	if (sima->flag & SI_DRAWTOOL) {
#ifdef NAN_TPT
		/* Draw tool is active */
		switch(event) {
			case LEFTMOUSE:
				/* Paranoia checks */
				if (!sima) break;
				if (!sima->image) break;
				if (!sima->image->ibuf) break;
				if (sima->image->packedfile) {
					error("Painting in packed images not supported");
					break;
				}
			
				brush = IMG_BrushCreate(Gvp.size, Gvp.size, Gvp.r, Gvp.g, Gvp.b, Gvp.a);
				/* MAART: skipx is not set most of the times. Make a guess. */
				rowBytes = sima->image->ibuf->skipx ? sima->image->ibuf->skipx : sima->image->ibuf->x * 4;
				canvas = IMG_CanvasCreateFromPtr(sima->image->ibuf->rect, sima->image->ibuf->x, sima->image->ibuf->y, rowBytes);

				getmouseco_areawin(xy_prev);
				while (get_mbut() & L_MOUSE) {
					getmouseco_areawin(xy_curr);
					/* Check if mouse position changed */
					if ((xy_prev[0] != xy_curr[0]) || (xy_prev[1] != xy_curr[1])) {
						/* Convert mouse coordinates to u,v and draw */
						areamouseco_to_ipoco(v2d, xy_prev, &uv_prev[0], &uv_prev[1]);
						areamouseco_to_ipoco(v2d, xy_curr, &uv_curr[0], &uv_curr[1]);
						IMG_CanvasDrawLineUV(canvas, brush, uv_prev[0], uv_prev[1], uv_curr[0], uv_curr[1]);
						if (G.sima->lock) {
							/* Make OpenGL aware of a changed texture */
							free_realtime_image(sima->image);
							/* Redraw this view and the 3D view */
							force_draw_plus(SPACE_VIEW3D);
						}
						else {
							/* Redraw only this view */
							force_draw();
						}
						xy_prev[0] = xy_curr[0];
						xy_prev[1] = xy_curr[1];
					}
				}
				/* Set the dirty bit in the image so that it is clear that it has been modified. */
				sima->image->ibuf->userflags |= IB_BITMAPDIRTY;
				if (!G.sima->lock) {
					/* Make OpenGL aware of a changed texture */
					free_realtime_image(sima->image);
					/* Redraw this view and the 3D view */
					force_draw_plus(SPACE_VIEW3D);
				}
				IMG_BrushDispose(brush);
				IMG_CanvasDispose(canvas);
				allqueue(REDRAWHEADERS, 0);
				break;
		}
#endif /* NAN_TPT */
	}
	else {
		/* Draw tool is inactive */
		switch(event) {
			case LEFTMOUSE:
				if(G.qual & LR_SHIFTKEY) mouseco_to_curtile();
				else gesture();
				break;
			case MIDDLEMOUSE:
				image_viewmove();
				break;
			case RIGHTMOUSE:
				mouse_select_sima();
				break;
			case AKEY:
				select_swap_tface_uv();
				break;
			case BKEY:
				borderselect_sima();
				break;
			case GKEY:
				transform_tface_uv('g');
				break;
			case NKEY:
				if(G.qual & LR_CTRLKEY) replace_names_but();
				break;
			case RKEY:
				transform_tface_uv('r');
				break;
			case SKEY:
				transform_tface_uv('s');
				break;
		}
	}

	/* Events handled always (whether the draw tool is active or not) */
	switch (event) {
		case MIDDLEMOUSE:
			image_viewmove();
			break;
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
		case PADPLUSKEY:
		case PADMINUS:
			image_viewzoom(event);
			scrarea_queue_winredraw(curarea);
			break;
		case HOMEKEY:
			image_home();
			break;
	}
}


void init_imagespace(ScrArea *sa)
{
	SpaceImage *sima;
	
	sima= MEM_callocN(sizeof(SpaceImage), "initimaspace");
	BLI_addhead(&sa->spacedata, sima);

	sima->spacetype= SPACE_IMAGE;
	sima->zoom= 1;
}


/* ******************** SPACE: IMASEL ********************** */

extern void drawimasel(void);
extern void winqreadimasel(unsigned short, short, char ascii);


/* alles naar imasel.c */


/* ******************** SPACE: OOPS ********************** */

extern void drawoopsspace(void);

void winqreadoopsspace(unsigned short event, short val, char ascii)
{
	SpaceOops *soops= curarea->spacedata.first;
	View2D *v2d= &soops->v2d;
	float dx, dy;

	if(val==0) return;

	if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

	switch(event) {
	case LEFTMOUSE:
		gesture();
		break;
	case MIDDLEMOUSE:
	case WHEELUPMOUSE:
	case WHEELDOWNMOUSE:
		view2dmove(event);	/* in drawipo.c */
		break;
	case RIGHTMOUSE:
		mouse_select_oops();
		break;
	case PADPLUSKEY:
	
		dx= 0.1154*(v2d->cur.xmax-v2d->cur.xmin);
		dy= 0.1154*(v2d->cur.ymax-v2d->cur.ymin);
		v2d->cur.xmin+= dx;
		v2d->cur.xmax-= dx;
		v2d->cur.ymin+= dy;
		v2d->cur.ymax-= dy;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	
	case PADMINUS:

		dx= 0.15*(v2d->cur.xmax-v2d->cur.xmin);
		dy= 0.15*(v2d->cur.ymax-v2d->cur.ymin);
		v2d->cur.xmin-= dx;
		v2d->cur.xmax+= dx;
		v2d->cur.ymin-= dy;
		v2d->cur.ymax+= dy;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
		
	case HOMEKEY:	
		do_oops_buttons(B_OOPSHOME);
		break;
		
	case AKEY:
		swap_select_all_oops();
		scrarea_queue_winredraw(curarea);
		break;
	case BKEY:
		borderselect_oops();
		break;
	case GKEY:
		transform_oops('g');
		break;
	case LKEY:
		if(G.qual & LR_SHIFTKEY) select_backlinked_oops();
		else select_linked_oops();
		break;
	case SKEY:
		
		if(G.qual & LR_ALTKEY) shrink_oops();
		else if(G.qual & LR_SHIFTKEY) shuffle_oops();
		else transform_oops('s');
		break;

	case ONEKEY:
		do_layer_buttons(0); break;
	case TWOKEY:
		do_layer_buttons(1); break;
	case THREEKEY:
		do_layer_buttons(2); break;
	case FOURKEY:
		do_layer_buttons(3); break;
	case FIVEKEY:
		do_layer_buttons(4); break;
	case SIXKEY:
		do_layer_buttons(5); break;
	case SEVENKEY:
		do_layer_buttons(6); break;
	case EIGHTKEY:
		do_layer_buttons(7); break;
	case NINEKEY:
		do_layer_buttons(8); break;
	case ZEROKEY:
		do_layer_buttons(9); break;
	case MINUSKEY:
		do_layer_buttons(10); break;
	case EQUALKEY:
		do_layer_buttons(11); break;
	case ACCENTGRAVEKEY:
		do_layer_buttons(-1); break;
	
	}
}

void init_v2d_oops(View2D *v2d)
{
	v2d->tot.xmin= -28.0;
	v2d->tot.xmax= 28.0;
	v2d->tot.ymin= -28.0;
	v2d->tot.ymax= 28.0;
	
	v2d->cur= v2d->tot;

	v2d->min[0]= 10.0;
	v2d->min[1]= 4.0;

	v2d->max[0]= 320.0;
	v2d->max[1]= 320.0;
	
	v2d->minzoom= 0.01;
	v2d->maxzoom= 2.0;
	
	/* v2d->scroll= L_SCROLL+B_SCROLL; */
	v2d->scroll= 0;
	v2d->keepaspect= 1;
	v2d->keepzoom= 0;
	v2d->keeptot= 0;
	
}

void init_oopsspace(ScrArea *sa)
{
	SpaceOops *soops;
	
	soops= MEM_callocN(sizeof(SpaceOops), "initoopsspace");
	BLI_addhead(&sa->spacedata, soops);

	soops->visiflag= OOPS_OB+OOPS_MA+OOPS_ME+OOPS_TE+OOPS_CU+OOPS_IP;
	
	soops->spacetype= SPACE_OOPS;
	init_v2d_oops(&soops->v2d);
}

/* ******************** SPACE: PAINT ********************** */


/* ******************** SPACE: Text ********************** */

extern void drawtextspace(void);
extern void winqreadtextspace(unsigned short, short, char ascii);

/* ******************** SPACE: ALGEMEEN ********************** */

void newspace(ScrArea *sa, int type)
{
	if(type>=0) {
		if(sa->spacetype != type) {
			SpaceLink *sl;
			
			sa->spacetype= type;
			sa->headbutofs= 0;
			
			uiFreeBlocks(&sa->uiblocks);
			wich_cursor(sa);
			
			if (sa->headwin) addqueue(sa->headwin, CHANGED, 1);
			scrarea_queue_headredraw(sa);

			addqueue(sa->win, CHANGED, 1);
			scrarea_queue_winredraw(sa);

			areawinset(sa->win);

			bwin_clear_viewmat(sa->win);
			
			for (sl= sa->spacedata.first; sl; sl= sl->next)
				if(sl->spacetype==type)
					break;

			if (sl) {			
				BLI_remlink(&sa->spacedata, sl);
				BLI_addhead(&sa->spacedata, sl);
			} else {
				if(type==SPACE_VIEW3D)
					initview3d(sa);
				else if(type==SPACE_IPO)
					initipo(sa);
				else if(type==SPACE_INFO)
					init_infospace(sa);
				else if(type==SPACE_BUTS)
					init_butspace(sa);
				else if(type==SPACE_FILE)
					init_filespace(sa);
				else if(type==SPACE_SEQ)
					init_seqspace(sa);
				else if(type==SPACE_IMAGE)
					init_imagespace(sa);
				else if(type==SPACE_IMASEL)
					init_imaselspace(sa);
				else if(type==SPACE_OOPS)
					init_oopsspace(sa);
				else if(type==SPACE_ACTION)
					init_actionspace(sa);
				else if(type==SPACE_TEXT)
					init_textspace(sa);
				else if(type==SPACE_SOUND)
					init_soundspace(sa);
				else if(type==SPACE_NLA)
					init_nlaspace(sa);

				sl= sa->spacedata.first;
				sl->area= sa;
			}
		}
	}

		
	/* uitzondering: filespace */
	if(curarea->spacetype==SPACE_FILE) {
		SpaceFile *sfile= curarea->spacedata.first;
		
		if(sfile->type==FILE_MAIN) {
			freefilelist(sfile);
		} else {
			sfile->type= FILE_UNIX;
		}
		
		sfile->returnfunc= 0;
		sfile->title[0]= 0;
		if(sfile->filelist) test_flags_file(sfile);
	}
	/* uitzondering: imasel space */
	else if(curarea->spacetype==SPACE_IMASEL) {
		SpaceImaSel *simasel= curarea->spacedata.first;
		simasel->returnfunc= 0;
		simasel->title[0]= 0;
	}
}

void freespacelist(ListBase *lb)
{
	SpaceLink *sl;

	for (sl= lb->first; sl; sl= sl->next) {
		if(sl->spacetype==SPACE_FILE) {
			SpaceFile *sfile= (SpaceFile*) sl;
			if(sfile->libfiledata)	
				BLO_blendhandle_close(sfile->libfiledata);
		}
		else if(sl->spacetype==SPACE_BUTS) {
			SpaceButs *buts= (SpaceButs*) sl;
			if(buts->rect) MEM_freeN(buts->rect);
			if(G.buts==buts) G.buts= 0;
		}
		else if(sl->spacetype==SPACE_IPO) {
			SpaceIpo *si= (SpaceIpo*) sl;
			if(si->editipo) MEM_freeN(si->editipo);
			free_ipokey(&si->ipokey);
			if(G.sipo==si) G.sipo= 0;
		}
		else if(sl->spacetype==SPACE_VIEW3D) {
			View3D *vd= (View3D*) sl;
			if(vd->bgpic) {
				if(vd->bgpic->rect) MEM_freeN(vd->bgpic->rect);
				if(vd->bgpic->ima) vd->bgpic->ima->id.us--;
				MEM_freeN(vd->bgpic);
			}
			if(vd->localvd) MEM_freeN(vd->localvd);
			if(G.vd==vd) G.vd= 0;
		}
		else if(sl->spacetype==SPACE_OOPS) {
			free_oopspace((SpaceOops *)sl);
		}
		else if(sl->spacetype==SPACE_IMASEL) {
			free_imasel((SpaceImaSel *)sl);
		}
		else if(sl->spacetype==SPACE_ACTION) {
			free_actionspace((SpaceAction*)sl);
		}
		else if(sl->spacetype==SPACE_NLA){
/*			free_nlaspace((SpaceNla*)sl);	*/
		}
		else if(sl->spacetype==SPACE_TEXT) {
			free_textspace((SpaceText *)sl);
		}
		else if(sl->spacetype==SPACE_SOUND) {
			free_soundspace((SpaceSound *)sl);
		}
	}

	BLI_freelistN(lb);
}

void duplicatespacelist(ScrArea *newarea, ListBase *lb1, ListBase *lb2)
{
	SpaceLink *sl;

	duplicatelist(lb1, lb2);
	
	/* lb1 is kopie van lb2, van lb2 geven we de filelist vrij */
	
	sl= lb2->first;
	while(sl) {
		if(sl->spacetype==SPACE_FILE) {
			SpaceFile *sfile= (SpaceFile*) sl;
			sfile->libfiledata= 0;
			sfile->filelist= 0;
		}
		else if(sl->spacetype==SPACE_OOPS) {
			SpaceOops *so= (SpaceOops *)sl;
			so->oops.first= so->oops.last= 0;
		}
		else if(sl->spacetype==SPACE_IMASEL) {
			check_imasel_copy((SpaceImaSel *) sl);
		}
		else if(sl->spacetype==SPACE_TEXT) {
		}
		/* __PINFAKE */
/*		else if(sfile->spacetype==SPACE_ACTION) {
			SpaceAction *sa= (SpaceAction *)sfile;
			if (sa->flag & SACTION_PIN)
				if (sa->action)
					sa->action->id.us++;

		}
*/		/* end PINFAKE */

		sl= sl->next;
	}
	
	sl= lb1->first;
	while(sl) {
		sl->area= newarea;

		if(sl->spacetype==SPACE_BUTS) {
			SpaceButs *buts= (SpaceButs *)sl;
			buts->rect= 0;
		}
		else if(sl->spacetype==SPACE_IPO) {
			SpaceIpo *si= (SpaceIpo *)sl;
			si->editipo= 0;
			si->ipokey.first= si->ipokey.last= 0;
		}
		else if(sl->spacetype==SPACE_VIEW3D) {
			View3D *vd= (View3D *)sl;
			if(vd->bgpic) {
				vd->bgpic= MEM_dupallocN(vd->bgpic);
				vd->bgpic->rect= 0;
				if(vd->bgpic->ima) vd->bgpic->ima->id.us++;
			}
		}
		sl= sl->next;
	}

	/* nog een keer: van oude View3D de localview restoren (ivm full) */
	sl= lb2->first;
	while(sl) {
		if(sl->spacetype==SPACE_VIEW3D) {
			View3D *v3d= (View3D*) sl;
			if(v3d->localvd) {
				restore_localviewdata(v3d);
				v3d->localvd= 0;
				v3d->localview= 0;
				v3d->lay &= 0xFFFFFF;
			}
		}
		sl= sl->next;
	}
}

/* wordt overal aangeroepen */
void allqueue(unsigned short event, short val)
{
	ScrArea *sa;
	View3D *v3d;
	SpaceButs *buts;
	SpaceFile *sfile;

	sa= G.curscreen->areabase.first;
	while(sa) {
		if(event==REDRAWALL) {
			scrarea_queue_winredraw(sa);
			scrarea_queue_headredraw(sa);
		}
		else if(sa->win != val) {
			switch(event) {
				
			case REDRAWHEADERS:
				scrarea_queue_headredraw(sa);
				break;
			case REDRAWVIEW3D:
				if(sa->spacetype==SPACE_VIEW3D) {
					scrarea_queue_winredraw(sa);
					if(val) scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWVIEW3D_Z:
				if(sa->spacetype==SPACE_VIEW3D) {
					v3d= sa->spacedata.first;
					if(v3d->drawtype==OB_SOLID) {
						scrarea_queue_winredraw(sa);
						if(val) scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWVIEWCAM:
				if(sa->spacetype==SPACE_VIEW3D) {
					v3d= sa->spacedata.first;
					if(v3d->persp>1) scrarea_queue_winredraw(sa);
				}
				break;
			case REDRAWINFO:
				if(sa->spacetype==SPACE_INFO) {
					scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWIMAGE:
				if(sa->spacetype==SPACE_IMAGE) {
					scrarea_queue_winredraw(sa);
					scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWIPO:
				if(sa->spacetype==SPACE_IPO) {
					SpaceIpo *si;
					scrarea_queue_winredraw(sa);
					scrarea_queue_headredraw(sa);
					if(val) {
						si= sa->spacedata.first;
						if (!G.sipo->pin)							
							si->blocktype= val;
					}
				}
				else if(sa->spacetype==SPACE_OOPS) {
					scrarea_queue_winredraw(sa);
				}
				
				break;
				
			case REDRAWBUTSALL:
				if(sa->spacetype==SPACE_BUTS) {
					scrarea_queue_winredraw(sa);
					scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWBUTSHEAD:
				if(sa->spacetype==SPACE_BUTS) {
					scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWBUTSVIEW:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_VIEW) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSLAMP:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_LAMP) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSMAT:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_MAT) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSTEX:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_TEX) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSANIM:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_ANIM) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSWORLD:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_WORLD) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSRENDER:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_RENDER) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSEDIT:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_EDIT) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSGAME:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if ELEM(buts->mainb, BUTS_GAME, BUTS_FPAINT) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSRADIO:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_RADIO) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSSCRIPT:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_SCRIPT) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSSOUND:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_SOUND) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSCONSTRAINT:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==BUTS_CONSTRAINT) {
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;				
			case REDRAWDATASELECT:
				if(sa->spacetype==SPACE_FILE) {
					sfile= sa->spacedata.first;
					if(sfile->type==FILE_MAIN) {
						freefilelist(sfile);
						scrarea_queue_winredraw(sa);
					}
				}
				else if(sa->spacetype==SPACE_OOPS) {
					scrarea_queue_winredraw(sa);
				}
				break;
			case REDRAWSEQ:
				if(sa->spacetype==SPACE_SEQ) {
					addqueue(sa->win, CHANGED, 1);
					scrarea_queue_winredraw(sa);
					scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWOOPS:
				if(sa->spacetype==SPACE_OOPS) {
					scrarea_queue_winredraw(sa);
				}
				break;
			case REDRAWNLA:
				if(sa->spacetype==SPACE_NLA) {
					scrarea_queue_headredraw(sa);
					scrarea_queue_winredraw(sa);
				}
			case REDRAWACTION:
				if(sa->spacetype==SPACE_ACTION) {
					scrarea_queue_headredraw(sa);
					scrarea_queue_winredraw(sa);
				}
				break;
			case REDRAWTEXT:
				if(sa->spacetype==SPACE_TEXT) {
					scrarea_queue_winredraw(sa);
				}
				break;
			case REDRAWSOUND:
				if(sa->spacetype==SPACE_SOUND) {
					scrarea_queue_headredraw(sa);
					scrarea_queue_winredraw(sa);
				}
				break;
			}
		}
		sa= sa->next;
	}
}

void allspace(unsigned short event, short val)
{
	bScreen *sc;

	sc= G.main->screen.first;
	while(sc) {
		ScrArea *sa= sc->areabase.first;
		while(sa) {
			SpaceLink *sl= sa->spacedata.first;
			while(sl) {
				switch(event) {
				case REMAKEALLIPO:
					{
						Ipo *ipo;
						IpoCurve *icu;
						
						/* Go to each ipo */
						for (ipo=G.main->ipo.first; ipo; ipo=ipo->id.next){
							for (icu = ipo->curve.first; icu; icu=icu->next){
								sort_time_ipocurve(icu);
								testhandles_ipocurve(icu);
							}
						}
					}
					break;
				case REMAKEIPO:
					if(sl->spacetype==SPACE_IPO) {
						SpaceIpo *si= (SpaceIpo *)sl;
						{
							if(si->editipo) MEM_freeN(si->editipo);
							si->editipo= 0;
							free_ipokey(&si->ipokey);
						}
					}
					break;
										
				case OOPS_TEST:
					if(sl->spacetype==SPACE_OOPS) {
						SpaceOops *so= (SpaceOops *)sl;
						so->flag |= SO_TESTBLOCKS;
					}
					break;
				}

				sl= sl->next;
			}
			sa= sa->next;
		}
		sc= sc->id.next;
	}
}


void force_draw()
{
	/* alle area's die (ongeveer) zelfde laten zien als curarea */
	ScrArea *tempsa, *sa;

	scrarea_do_windraw(curarea);
	
	tempsa= curarea;
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa!=tempsa && sa->spacetype==tempsa->spacetype) {
			if(sa->spacetype==SPACE_VIEW3D) {
				if( ((View3D *)sa->spacedata.first)->lay & ((View3D *)tempsa->spacedata.first)->lay) {
					areawinset(sa->win);
					scrarea_do_windraw(sa);
				}
			}
			else if(sa->spacetype==SPACE_IPO) {
				areawinset(sa->win);
				scrarea_do_windraw(sa);
			}
			else if(sa->spacetype==SPACE_SEQ) {
				areawinset(sa->win);
				scrarea_do_windraw(sa);
			}
			else if(sa->spacetype==SPACE_ACTION) {
				areawinset(sa->win);
				scrarea_do_windraw(sa);
			}
		}
		sa= sa->next;
	}
	if(curarea!=tempsa) areawinset(tempsa->win);
	
	screen_swapbuffers();

}

void force_draw_plus(int type)
{
	/* alle area's die (ongeveer) zelfde laten zien als curarea EN areas van 'type' */
	ScrArea *tempsa, *sa;

	scrarea_do_windraw(curarea); 

	tempsa= curarea;
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa!=tempsa && (sa->spacetype==tempsa->spacetype || sa->spacetype==type)) {
			if(ELEM5(sa->spacetype, SPACE_VIEW3D, SPACE_IPO, SPACE_SEQ, SPACE_BUTS, SPACE_ACTION)) {
				areawinset(sa->win);
				scrarea_do_windraw(sa);
			}
		}
		sa= sa->next;
	}
	if(curarea!=tempsa) areawinset(tempsa->win);

	screen_swapbuffers();
}

void force_draw_all(void)
{
	/* alle area's die (ongeveer) zelfde laten zien als curarea EN areas van 'type' */
	ScrArea *tempsa, *sa;

	drawscreen();

	tempsa= curarea;
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->headwin) {
			scrarea_do_headdraw(sa);
			scrarea_do_headchange(sa);
		}
		if(sa->win) {
			scrarea_do_windraw(sa);
		}
		sa= sa->next;
	}
	if(curarea!=tempsa) areawinset(tempsa->win);

	screen_swapbuffers();
}

/***/

SpaceType *spaceaction_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Action");
		spacetype_set_winfuncs(st, drawactionspace, changeview2d, winqreadactionspace);
	}

	return st;
}
SpaceType *spacebuts_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Buts");
		spacetype_set_winfuncs(st, drawbutspace, changebutspace, winqreadbutspace);
	}

	return st;
}
SpaceType *spacefile_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("File");
		spacetype_set_winfuncs(st, drawfilespace, NULL, winqreadfilespace);
	}

	return st;
}
SpaceType *spaceimage_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Image");
		spacetype_set_winfuncs(st, drawimagespace, NULL, winqreadimagespace);
	}

	return st;
}
SpaceType *spaceimasel_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Imasel");
		spacetype_set_winfuncs(st, drawimasel, NULL, winqreadimasel);
	}

	return st;
}
SpaceType *spaceinfo_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Info");
		spacetype_set_winfuncs(st, drawinfospace, NULL, winqreadinfospace);
	}

	return st;
}
SpaceType *spaceipo_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Ipo");
		spacetype_set_winfuncs(st, drawipo, changeview2d, winqreadipo);
	}

	return st;
}
SpaceType *spacenla_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Nla");
		spacetype_set_winfuncs(st, drawnlaspace, changeview2d, winqreadnlaspace);
	}

	return st;
}
SpaceType *spaceoops_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Oops");
		spacetype_set_winfuncs(st, drawoopsspace, changeview2d, winqreadoopsspace);
	}

	return st;
}
SpaceType *spaceseq_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Sequence");
		spacetype_set_winfuncs(st, drawseqspace, changeview2d, winqreadsequence);
	}

	return st;
}
SpaceType *spacesound_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Sound");
		spacetype_set_winfuncs(st, drawsoundspace, NULL, winqreadsoundspace);
	}

	return st;
}
SpaceType *spacetext_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Text");
		spacetype_set_winfuncs(st, drawtextspace, NULL, winqreadtextspace);
	}

	return st;
}
SpaceType *spaceview3d_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("View3D");
		spacetype_set_winfuncs(st, drawview3d, changeview3d, winqread3d);
	}

	return st;
}
