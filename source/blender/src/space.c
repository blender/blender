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
 * - here initialize and free and handling SPACE data
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

#ifdef INTERNATIONAL
#include "BIF_language.h"
#endif

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

#include "BIF_butspace.h"
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
#include "BDR_drawobject.h"

#include "BLO_readfile.h" /* for BLO_blendhandle_close */

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

/* *************************************** */

/* don't know yet how the handlers will evolve, for simplicity
   i choose for an array with eventcodes, this saves in a file!
   */
void add_blockhandler(ScrArea *sa, short eventcode, short val)
{
	SpaceLink *sl= sa->spacedata.first;
	short a;
	
	// find empty spot
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		if( sl->blockhandler[a]==eventcode ) {
			sl->blockhandler[a+1]= val;
			break;
		}
		else if( sl->blockhandler[a]==0) {
			sl->blockhandler[a]= eventcode;
			sl->blockhandler[a+1]= val;
			break;
		}
	}
	if(a==SPACE_MAXHANDLER) printf("error; max (4) blockhandlers reached!\n");
}

void rem_blockhandler(ScrArea *sa, short eventcode)
{
	SpaceLink *sl= sa->spacedata.first;
	short a;
	
	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		if( sl->blockhandler[a]==eventcode) {
			sl->blockhandler[a]= 0;
			break;
		}
	}
}




/* ************* SPACE: VIEW3D  ************* */

/*  extern void drawview3dspace(ScrArea *sa, void *spacedata); BSE_drawview.h */


void copy_view3d_lock(short val)
{
	bScreen *sc;
	int bit;
	
	/* from G.scene copy to the other views */
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
								if(sa->win) scrarea_queue_redraw(sa);
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

			/* copy to scene */
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

#if GAMEBLENDER == 1
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
		BLI_linklist_prepend(&storelist, (void*) (long) sc->r.cfra);
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
#endif

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
#else
	notice("Game engine is disabled in this release!");
#endif
}

static void changeview3dspace(ScrArea *sa, void *spacedata)
{
	setwinmatrixview3d(0);	/* 0= no pick rect */
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

		if ((G.obedit) && (G.obedit->type == OB_MESH)) {
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

void select_children(Object *ob, int recursive)
{
	Base *base;

	for (base= FIRSTBASE; base; base= base->next)
		if (ob == base->object->parent) {
			base->flag |= SELECT;
			base->object->flag |= SELECT;
			if (recursive) select_children(base->object, 1);
		}
}

void select_parent(void)	/* Makes parent active and de-selected OBACT */
{
	Base *base, *startbase, *basact=NULL, *oldbasact;

	if (!(OBACT->parent)) return;
	BASACT->flag &= (~SELECT);
	BASACT->object->flag &= (~SELECT);
	startbase=  FIRSTBASE;
	if(BASACT && BASACT->next) startbase= BASACT->next;
	base = startbase;
	while(base) {
		if(base->object==BASACT->object->parent) { basact=base; break; }
		base=base->next;
		if(base==0) base= FIRSTBASE;
		if(base==startbase) break;
	}
	oldbasact = BASACT;
	BASACT = basact;
	basact->flag |= SELECT;		
	if(oldbasact) if(oldbasact != basact) draw_object_ext(oldbasact);
	basact->object->flag= basact->flag;
	draw_object_ext(basact);
	set_active_base(basact);
}

void group_menu(void)
{
	Base *base;
	short nr;
	char *str;

	/* make menu string */
	
	str= MEM_mallocN(160, "groupmenu");
	strcpy(str, "Group selection%t|Children%x1|"
	            "Immediate children%x2|Parent%x3|"
	            "Objects on shared layers%x4");

	/* here we go */
	
	nr= pupmenu(str);
	MEM_freeN(str);

	if(nr==4) {
		base= FIRSTBASE;
		while(base) {
			if (base->lay & OBACT->lay) {
				base->flag |= SELECT;
				base->object->flag |= SELECT;
			}
			base= base->next;
		}		
	}
	else if(nr==2) select_children(OBACT, 0);
	else if(nr==1) select_children(OBACT, 1);
	else if(nr==3) select_parent();
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
}

void winqreadview3dspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	char ascii= evt->ascii;
	View3D *v3d= curarea->spacedata.first;
	Object *ob;
	float *curs;
	int doredraw= 0, pupval;
	
	if(curarea->win==0) return;	/* when it comes from sa->headqread() */
	
	
	if(val) {

		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;
		if(event==MOUSEY) return;
		
		if(event==UI_BUT_EVENT) do_butspace(val); // temporal, view3d deserves own queue?

		
		/* TEXTEDITING?? */
		if((G.obedit) && G.obedit->type==OB_FONT) {
			switch(event) {
			
			case LEFTMOUSE:
				mouse_cursor();
				break;
			case MIDDLEMOUSE:
				/* use '&' here, because of alt+leftmouse which emulates middlemouse */
				if(U.flag & VIEWMOVE) {
					if((G.qual==LR_SHIFTKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_SHIFTKEY))))
						viewmove(0);
					else if((G.qual==LR_CTRLKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_CTRLKEY))))
						viewmove(2);
					else if((G.qual==0) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==LR_ALTKEY)))
						viewmove(1);
				}
				else {
					if((G.qual==LR_SHIFTKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_SHIFTKEY))))
						viewmove(1);
					else if((G.qual==LR_CTRLKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_CTRLKEY))))
						viewmove(2);
					else
						viewmove(0);
				}
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

			case UKEY:
				if(G.qual==LR_ALTKEY) {
					remake_editText();
					doredraw= 1;
				} 
				else {
					do_textedit(event, val, ascii);
				}
				break;
			case VKEY:
				if(G.qual==LR_ALTKEY) {
					paste_editText();
					doredraw= 1;
				} 
				else {
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
				if ((G.obedit) || !(G.f&(G_VERTEXPAINT|G_WEIGHTPAINT|G_TEXTUREPAINT))) {
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
				/* use '&' here, because of alt+leftmouse which emulates middlemouse */
				if(U.flag & VIEWMOVE) {
					if((G.qual==LR_SHIFTKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_SHIFTKEY))))
						viewmove(0);
					else if((G.qual==LR_CTRLKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_CTRLKEY))))
						viewmove(2);
					else if((G.qual==0) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==LR_ALTKEY)))
						viewmove(1);
				}
				else {
					if((G.qual==LR_SHIFTKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_SHIFTKEY))))
						viewmove(1);
					else if((G.qual==LR_CTRLKEY) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==(LR_ALTKEY|LR_CTRLKEY))))
						viewmove(2);
					else if((G.qual==0) || ((U.flag & TWOBUTTONMOUSE) && (G.qual==LR_ALTKEY)))
						viewmove(0);
				}
				break;
			case RIGHTMOUSE:
				if((G.obedit) && (G.qual & LR_CTRLKEY)==0) {
					if(G.obedit->type==OB_MESH)
						mouse_mesh();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF)
						mouse_nurb();
					else if(G.obedit->type==OB_MBALL)
						mouse_mball();
					else if(G.obedit->type==OB_LATTICE)
						mouse_lattice();
					else if(G.obedit->type==OB_ARMATURE)
						mouse_armature();
				}
				else if((G.obedit) && (G.qual==(LR_CTRLKEY|LR_ALTKEY)))
					mouse_mesh();	// edge select
				else if(G.obpose) { 
					if (G.obpose->type==OB_ARMATURE)
						mousepose_armature();
				}
				else if(G.qual==LR_CTRLKEY)
					mouse_select();
				else if(G.f & G_FACESELECT)
					face_select();
				else if( G.f & (G_VERTEXPAINT|G_TEXTUREPAINT))
					sample_vpaint();
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
				ob= OBACT;
				if(G.qual==LR_CTRLKEY) {
					if(G.obedit) {
							flip_subdivison(G.obedit, 1);
					}
					else if(ob->type == OB_MESH) {
						flip_subdivison(ob, 1);
					}
				}
				else
					do_layer_buttons(0); break;
			case TWOKEY:
				ob= OBACT;
				if(G.qual==LR_CTRLKEY) {
					if(G.obedit) {
							flip_subdivison(G.obedit, 2);
					}
					else if(ob->type == OB_MESH) {
						flip_subdivison(ob, 2);
					}
				}
				else
					do_layer_buttons(1); 
				break;
			case THREEKEY:
				ob= OBACT;
				if(G.qual==LR_CTRLKEY) {
					if(G.obedit) {
							flip_subdivison(G.obedit, 3);
					}
					else if(ob->type == OB_MESH) {
						flip_subdivison(ob, 3);
					}
				}
				else
					do_layer_buttons(2); break;
			case FOURKEY:
				ob= OBACT;
				if(G.qual & LR_CTRLKEY) {
					if(G.obedit) {
							flip_subdivison(G.obedit, 4);
					}
					else if(ob->type == OB_MESH) {
						flip_subdivison(ob, 4);
					}
				}
				else
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
				if(G.qual & LR_CTRLKEY) apply_object();	// also with shift!
				else if((G.qual==LR_SHIFTKEY)) {
					toolbox_n_add();
				}
				else {
					if(G.obedit) {
						if(G.obedit->type==OB_MESH)
							deselectall_mesh();
						else if ELEM(G.obedit->type, OB_CURVE, OB_SURF)
							deselectall_nurb();
						else if(G.obedit->type==OB_MBALL)
							deselectall_mball();
						else if(G.obedit->type==OB_LATTICE)
							deselectall_Latt();
						else if(G.obedit->type==OB_ARMATURE)
							deselectall_armature();
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
						else {
							/* by design, the center of the active object 
							 * (which need not necessarily by selected) will
							 * still be drawn as if it were selected.
							 */
							deselectall();
						}
					}
				}
				break;
			case BKEY:
				if((G.qual==LR_SHIFTKEY))
					set_render_border();
				else if((G.qual==0))
					borderselect();
				break;
			case CKEY:
				if(G.qual==LR_CTRLKEY) {
					copymenu();
				}
				else if(G.qual==LR_ALTKEY) {
					convertmenu();	/* editobject.c */
				}
				else if((G.qual==LR_SHIFTKEY)) {
					view3d_home(1);
					curs= give_cursor();
					curs[0]=curs[1]=curs[2]= 0.0;
					allqueue(REDRAWVIEW3D, 0);
				}
				else if((G.obedit) && ELEM(G.obedit->type, OB_CURVE, OB_SURF) ) {
					makecyclicNurb();
					makeDispList(G.obedit);
					allqueue(REDRAWVIEW3D, 0);
				}
				else if((G.qual==0)){
					curs= give_cursor();
					G.vd->ofs[0]= -curs[0];
					G.vd->ofs[1]= -curs[1];
					G.vd->ofs[2]= -curs[2];
					scrarea_queue_winredraw(curarea);
				}
			
				break;
			case DKEY:
				if((G.qual==LR_SHIFTKEY)) {
					duplicate_context_selected();
				}
				else if(G.qual==LR_ALTKEY) {
					if(G.obpose)
						error ("Duplicate not possible in posemode.");
					else if((G.obedit==0))
						adduplicate(0);
				}
				else if(G.qual==LR_CTRLKEY) {
					imagestodisplist();
				}
				else if((G.qual==0)){
					pupval= pupmenu("Draw mode%t|BoundBox %x1|Wire %x2|OpenGL Solid %x3|Shaded Solid %x4|Textured Solid %x5");
					if(pupval>0) {
						G.vd->drawtype= pupval;
						doredraw= 1;
					
					}
				}
				
				break;
			case EKEY:
				if (G.qual==0){
					if(G.obedit) {
						if(G.obedit->type==OB_MESH)
							extrude_mesh();
						else if(G.obedit->type==OB_CURVE)
							addvert_Nurb('e');
						else if(G.obedit->type==OB_SURF)
							extrude_nurb();
						else if(G.obedit->type==OB_ARMATURE)
							extrude_armature();
					}
					else {
						ob= OBACT;
						if(ob && ob->type==OB_IKA) if(okee("extrude IKA"))
							extrude_ika(ob, 1);
					}
				}
				break;
			case FKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) {
						if((G.qual==LR_SHIFTKEY))
							fill_mesh();
						else if(G.qual==LR_ALTKEY)
							beauty_fill();
						else if(G.qual==LR_CTRLKEY)
							edge_flip();
						else if (G.qual==0)
							addedgevlak_mesh();
					}
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) addsegment_nurb();
				}
				else if(G.qual==LR_CTRLKEY)
					sort_faces();
				else if((G.qual==LR_SHIFTKEY))
					fly();
				else {
						set_faceselect();
					}
				
				break;
			case GKEY:
				/* RMGRP if(G.qual & LR_CTRLKEY) add_selected_to_group();
				else if(G.qual & LR_ALTKEY) rem_selected_from_group(); */
				
				if((G.qual==LR_SHIFTKEY))
					group_menu();
				else if(G.qual==LR_ALTKEY)
					clear_object('g');
				else if((G.qual==0))
					transform('g');
				break;
			case HKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH) {
						if(G.qual==LR_ALTKEY)
							reveal_mesh();
						else if((G.qual==LR_SHIFTKEY))
							hide_mesh(1);
						else if((G.qual==0)) 
							hide_mesh(0);
					}
					else if(G.obedit->type== OB_SURF) {
						if(G.qual==LR_ALTKEY)
							revealNurb();
						else if((G.qual==LR_SHIFTKEY))
							hideNurb(1);
						else if((G.qual==0))
							hideNurb(0);
					}
					else if(G.obedit->type==OB_CURVE) {
						if(G.qual==LR_CTRLKEY)
							autocalchandlesNurb_all(1);	/* flag=1, selected */
						else if((G.qual==LR_SHIFTKEY))
							sethandlesNurb(1);
						else if((G.qual==0))
							sethandlesNurb(3);
						
						makeDispList(G.obedit);
						
						allqueue(REDRAWVIEW3D, 0);
					}
				}
				else if(G.f & G_FACESELECT)
					hide_tface();
				
				break;
			case IKEY:
				break;
				
			case JKEY:
				if(G.qual==LR_CTRLKEY) {
					if( (ob= OBACT) ) {
						if(ob->type == OB_MESH)
							join_mesh();
						else if(ob->type == OB_CURVE)
							join_curve(OB_CURVE);
						else if(ob->type == OB_SURF)
							join_curve(OB_SURF);
						else if(ob->type == OB_ARMATURE)
							join_armature ();
					}
					else if ((G.obedit) && ELEM(G.obedit->type, OB_CURVE, OB_SURF))
						addsegment_nurb();
				}
				else if(G.obedit) {
					if(G.obedit->type==OB_MESH) {
						join_triangles();
					}
				}

				break;
			case KKEY:
				if(G.obedit) {
					if (G.obedit->type==OB_MESH) {
						if (G.qual==LR_SHIFTKEY)
							KnifeSubdivide(KNIFE_PROMPT);
						else if (G.qual==0)
							LoopMenu();
					}
					else if(G.obedit->type==OB_SURF)
						printknots();
				}
				else {
					if((G.qual==LR_SHIFTKEY)) {
						if(G.f & G_FACESELECT)
							clear_vpaint_selectedfaces();
						else if(G.f & G_VERTEXPAINT)
							clear_vpaint();
						else
							select_select_keys();
					}
					else if(G.qual==LR_CTRLKEY)
						make_skeleton();
/* 					else if(G.qual & LR_ALTKEY) delete_skeleton(); */
					else if (G.qual==0)
						set_ob_ipoflags();
				}
				
				break;
			case LKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH)
						selectconnected_mesh(G.qual);
					if(G.obedit->type==OB_ARMATURE)
						selectconnected_armature();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF)
						selectconnected_nurb();
				}
				else if(G.obpose) {
					if(G.obpose->type==OB_ARMATURE)
						selectconnected_posearmature();
				}
				else {
					if((G.qual==LR_SHIFTKEY))
						selectlinks();
					else if(G.qual==LR_CTRLKEY)
						linkmenu();
					else if(G.f & G_FACESELECT)
						select_linked_tfaces();
					else if((G.qual==0))
						make_local();
				}
				break;
 			case MKEY:
				if(G.obedit){
					if(G.qual==LR_ALTKEY) {
						if(G.obedit->type==OB_MESH)
							undo_push_mesh("Merge");
							mergemenu();
					}
					else if((G.qual==0))
						if(G.obedit->type==OB_MESH)
							undo_push_mesh("Mirror");
						mirrormenu();
				}
				else if((G.qual==0)){
				     movetolayer();
				}
 				break;
			case NKEY:
				if((G.qual==0)) {
					add_blockhandler(curarea, VIEW3D_HANDLER_OBJECT, UI_PNL_TO_MOUSE);
					allqueue(REDRAWVIEW3D, 0);
				}
				else if(G.obedit) {
					switch (G.obedit->type){
					case OB_ARMATURE:
						if(G.qual==LR_CTRLKEY){
							if (okee("Recalc bone roll angles")) {
								auto_align_armature();
								allqueue(REDRAWVIEW3D, 0);
							}
						}
						break;
					case OB_MESH: 
						if(G.qual==(LR_SHIFTKEY|LR_CTRLKEY)) {
							if(okee("Recalc normals inside")) {
								undo_push_mesh("Recalc normals inside");
								righthandfaces(2);
								allqueue(REDRAWVIEW3D, 0);
							}
						}
						else if(G.qual==LR_CTRLKEY){
							if(okee("Recalc normals outside")) {
								undo_push_mesh("Recalc normals outside");
								righthandfaces(1);
								allqueue(REDRAWVIEW3D, 0);
							}
						}
						break;
					}
				}
				
				break;
			case OKEY:
				ob= OBACT;
				if(G.obedit) {
					extern int prop_mode;

					if (G.qual==LR_SHIFTKEY) {
						prop_mode= !prop_mode;
						allqueue(REDRAWHEADERS, 0);
					}
					else if((G.qual==0)) {
						G.f ^= G_PROPORTIONAL;
						allqueue(REDRAWHEADERS, 0);
					}
				}
				else if((G.qual==LR_SHIFTKEY)) {
					if(ob && ob->type == OB_MESH) {
						flip_subdivison(ob, 0);
					}
				}
				else if(G.qual==LR_ALTKEY) clear_object('o');
				break;

			case PKEY:
				
				if(G.obedit) {
					if(G.qual==LR_CTRLKEY)
						make_parent();
					else if((G.qual==0) && G.obedit->type==OB_MESH)
						separatemenu();
					else if ((G.qual==0) && ELEM(G.obedit->type, OB_CURVE, OB_SURF))
						separate_nurb();
				}
				else if(G.qual==LR_CTRLKEY)
					make_parent();
				else if(G.qual==LR_ALTKEY)
					clear_parent();
				else if((G.qual==0)) {
                	start_game();
				}
				break;				
			case RKEY:
				if((G.obedit==0) && (G.f & G_FACESELECT) && (G.qual==0))
					rotate_uv_tface();
				else if(G.qual==LR_ALTKEY)
					clear_object('r');
				else if (G.obedit) {
					if((G.qual==LR_SHIFTKEY)) {
						if ELEM(G.obedit->type,  OB_CURVE, OB_SURF)					
							selectrow_nurb();
						else if (G.obedit->type==OB_MESH)
							loop('s');
					}
					else if(G.qual==LR_CTRLKEY) {
						if (G.obedit->type==OB_MESH)
							loop('c');
					}
					else if((G.qual==0))
						transform('r');
				}
				else if((G.qual==0))
					transform('r');
				break;
			case SKEY:
				if(G.obedit) {
					if(G.qual==LR_ALTKEY)
						transform('N'); /* scale along normal */
					else if(G.qual==LR_CTRLKEY)
						transform('S');
					else if(G.qual==LR_SHIFTKEY)
						snapmenu();
					else if((G.qual==0))
						transform('s');
				}
				else if(G.qual==LR_ALTKEY) {
					clear_object('s');
				}
				else if((G.qual==LR_SHIFTKEY))
					snapmenu();
				else if((G.qual==0))
					transform('s');
				break;
			case TKEY:
				if(G.obedit){
					if((G.qual==LR_CTRLKEY) && G.obedit->type==OB_MESH) {
						convert_to_triface(0);
						allqueue(REDRAWVIEW3D, 0);
						countall();
						makeDispList(G.obedit);
					}
					else if((G.qual==LR_ALTKEY) && G.obedit->type==OB_CURVE)
						clear_tilt();
					else if((G.qual==0))
						transform('t');
				}
				else if(G.qual==LR_CTRLKEY) {
					make_track();
				}
				else if(G.qual==LR_ALTKEY) {
					clear_track();
				}
				else if((G.qual==0)){
					texspace_edit();
				}
				
				break;
			case UKEY:
				if(G.obedit) {
					if(G.obedit->type==OB_MESH){
						if (G.qual==LR_ALTKEY)
							undo_menu_mesh();
						else if (G.qual==LR_SHIFTKEY)
							undo_redo_mesh();
						else if((G.qual==0))
							undo_pop_mesh(1);
					}
					else if(G.obedit->type==OB_ARMATURE)
						remake_editArmature();
					else if ELEM(G.obedit->type, OB_CURVE, OB_SURF)
						remake_editNurb();
					else if(G.obedit->type==OB_LATTICE)
						remake_editLatt();
				}
				else if((G.qual==0)){
					if (G.f & G_FACESELECT)
						uv_autocalc_tface();
					else if(G.f & G_WEIGHTPAINT)
						wpaint_undo();
					else if(G.f & G_VERTEXPAINT)
						vpaint_undo();
					else
						single_user();
				}
				
				break;
			case VKEY:
				ob= OBACT;
				if((G.qual==LR_SHIFTKEY)) {
					if ((G.obedit) && G.obedit->type==OB_MESH) {
						align_view_to_selected(v3d);
					}
					else if (G.f & G_FACESELECT) {
						align_view_to_selected(v3d);
					}
				}
				else if(G.qual==LR_ALTKEY)
					image_aspect();
				else if (G.qual==0){
					if(G.obedit) {
						if(G.obedit->type==OB_CURVE) {
							sethandlesNurb(2);
							makeDispList(G.obedit);
							allqueue(REDRAWVIEW3D, 0);
						}
					}
					else if(ob && ob->type == OB_MESH) 
						set_vpaint();
				}
				break;
			case WKEY:
				if((G.qual==LR_SHIFTKEY)) {
					transform('w');
				}
				else if(G.qual==LR_ALTKEY) {
					/* if(G.obedit && G.obedit->type==OB_MESH) write_videoscape(); */
				}
				else if(G.qual==LR_CTRLKEY) {
					if(G.obedit) {
						if ELEM(G.obedit->type,  OB_CURVE, OB_SURF) {
							switchdirectionNurb2();
						}
					}
				}
				else if((G.qual==0))
					special_editmenu();
				
				break;
			case XKEY:
			case DELKEY:
				if(G.qual==0)
					delete_context_selected();
				break;
			case YKEY:
				if((G.qual==0) && (G.obedit)) {
					if(G.obedit->type==OB_MESH) split_mesh();
				}
				break;
			case ZKEY:
				toggle_shading();
				
				scrarea_queue_headredraw(curarea);
				scrarea_queue_winredraw(curarea);
				break;
				
			
			case HOMEKEY:
				if(G.qual==0)
					view3d_home(0);
				break;
			case COMMAKEY:
				if(G.qual==0) {
					G.vd->around= V3D_CENTRE;
					scrarea_queue_headredraw(curarea);
				}
				break;
				
			case PERIODKEY:
				if(G.qual==0) {
					G.vd->around= V3D_CURSOR;
					scrarea_queue_headredraw(curarea);
				}
				break;
			
			case PADSLASHKEY:
				if(G.qual==0) {
					if(G.vd->localview) {
						G.vd->localview= 0;
						endlocalview(curarea);
					}
					else {
						G.vd->localview= 1;
						initlocalview();
					}
					scrarea_queue_headredraw(curarea);
				}
				break;
			case PADASTERKEY:	/* '*' */
				if(G.qual==0) {
					ob= OBACT;
					if(ob) {
						obmat_to_viewmat(ob);
						if(G.vd->persp==2) G.vd->persp= 1;
						scrarea_queue_winredraw(curarea);
					}
				}
				break;
			case PADPERIOD:	/* '.' */
				if(G.qual==0)
					centreview();
				break;
			
			case PAGEUPKEY:
				if(G.qual==LR_CTRLKEY)
					movekey_obipo(1);
				else if((G.qual==0))
					nextkey_obipo(1);	/* in editipo.c */
				break;

			case PAGEDOWNKEY:
				if(G.qual==LR_CTRLKEY)
					movekey_obipo(-1);
				else if((G.qual==0))
					nextkey_obipo(-1);
				break;
				
			case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
			case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
			case PADMINUS: case PADPLUSKEY: case PADENTER:
				persptoetsen(event);
				doredraw= 1;
				break;
			
			case ESCKEY:
				if(G.qual==0) {
					if (G.vd->flag & V3D_DISPIMAGE) {
						G.vd->flag &= ~V3D_DISPIMAGE;
						doredraw= 1;
					}
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
	BLI_addhead(&sa->spacedata, vd);	/* addhead! not addtail */

	vd->spacetype= SPACE_VIEW3D;
	vd->viewquat[0]= 1.0;
	vd->viewquat[1]= vd->viewquat[2]= vd->viewquat[3]= 0.0;
	vd->persp= 1;
	vd->drawtype= OB_WIRE;
	vd->view= 7;
	vd->dist= 10.0;
	vd->lens= 35.0f;
	vd->near= 0.01f;
	vd->far= 500.0f;
	vd->grid= 1.0f;
	vd->gridlines= 16;
	vd->lay= vd->layact= 1;
	if(G.scene) {
		vd->lay= vd->layact= G.scene->lay;
		vd->camera= G.scene->camera;
	}
	vd->scenelock= 1;
}


/* ******************** SPACE: IPO ********************** */

static void changeview2dspace(ScrArea *sa, void *spacedata)
{
	if(G.v2d==0) return;

	test_view2d(G.v2d, curarea->winx, curarea->winy);
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
}

void winqreadipospace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	extern void do_ipobuts(unsigned short event); 	// drawipo.c
	unsigned short event= evt->event;
	short val= evt->val;
	SpaceIpo *sipo= curarea->spacedata.first;
	View2D *v2d= &sipo->v2d;
	float dx, dy;
	int cfra, doredraw= 0;
	short mval[2];
	

	if(sa->win==0) return;

	if(val) {
		if( uiDoBlocks(&sa->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case UI_BUT_EVENT:
			/* note: bad bad code, will be cleaned! is because event queues are all shattered */
			if(val>0 && val < 32) do_ipowin_buts(val-1);
			else do_ipobuts(val);
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
			view2d_zoom(v2d, 0.1154, sa->winx, sa->winy);
			doredraw= 1;
			break;
		case PADMINUS:
			view2d_zoom(v2d, -0.15, sa->winx, sa->winy);
			doredraw= 1;
			break;
		case PAGEUPKEY:
			if(G.qual==LR_CTRLKEY)
				movekey_ipo(1);
			else if((G.qual==0))
				nextkey_ipo(1);
			break;
		case PAGEDOWNKEY:
			if(G.qual==LR_CTRLKEY)
				movekey_ipo(-1);
			else if((G.qual==0))
				nextkey_ipo(-1);
			break;
		case HOMEKEY:
			if((G.qual==0))
				do_ipo_buttons(B_IPOHOME);
			break;
			
		case AKEY:
			if((G.qual==0)) {
				if(in_ipo_buttons()) {
					swap_visible_editipo();
				}
				else swap_selectall_editipo();
				allspace (REMAKEIPO, 0);
				allqueue (REDRAWNLA, 0);
				allqueue (REDRAWACTION, 0);
			}
			break;
		case BKEY:
			if((G.qual==0))
				borderselect_ipo();
			break;
		case CKEY:
			if((G.qual==0))
				move_to_frame();
			break;
		case DKEY:
			if((G.qual==LR_SHIFTKEY))
				add_duplicate_editipo();
			break;
		case GKEY:
			if((G.qual==0))
				transform_ipo('g');
			break;
		case HKEY:
			if((G.qual==LR_SHIFTKEY))
				sethandles_ipo(HD_AUTO);
			else if((G.qual==0))
				sethandles_ipo(HD_ALIGN);
			break;
		case JKEY:
			if((G.qual==0))
				join_ipo();
			break;
		case KKEY:
			if((G.qual==0)) {
				ipo_toggle_showkey();
				scrarea_queue_headredraw(curarea);
				allqueue(REDRAWVIEW3D, 0);
				doredraw= 1;
			}
			break;
		case NKEY:
			add_blockhandler(sa, IPO_HANDLER_PROPERTIES, UI_PNL_TO_MOUSE);
			doredraw= 1;
			break;
		case RKEY:
			if((G.qual==0))
				ipo_record();
			break;
		case SKEY:
			if((G.qual==LR_SHIFTKEY))
				ipo_snapmenu();
			else if((G.qual==0))
				transform_ipo('s');
			break;
		case TKEY:
			if((G.qual==0))
				set_ipotype();
			break;
		case VKEY:
			if((G.qual==0))
				sethandles_ipo(HD_VECT);
			break;
		case XKEY:
		case DELKEY:
			if((G.qual==LR_SHIFTKEY))
				delete_key();
			else if((G.qual==0))
				del_ipo();
			break;
		}
	}

	if(doredraw) scrarea_queue_winredraw(sa);
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

	sipo->v2d.min[0]= 0.01f;
	sipo->v2d.min[1]= 0.01f;

	sipo->v2d.max[0]= 15000.0f;
	sipo->v2d.max[1]= 10000.0f;
	
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

#define B_ADD_THEME 	3301
#define B_DEL_THEME 	3302
#define B_NAME_THEME 	3303
#define B_THEMECOL 		3304
#define B_UPDATE_THEME 	3305
#define B_CHANGE_THEME 	3306
#define B_THEME_COPY 	3307
#define B_THEME_PASTE 	3308

#define B_RECALCLIGHT 	3310

// needed for event; choose new 'curmain' resets it...
static short th_curcol= TH_BACK;
static char *th_curcol_ptr= NULL;
static char th_curcol_arr[4]={0, 0, 0, 255};

void info_user_themebuts(uiBlock *block, short y1, short y2, short y3)
{
	bTheme *btheme, *bt;
	int spacetype= 0;
	static short cur=1, curmain=2;
	short a, tot=0, isbuiltin= 0;
	char string[20*32], *strp, *col;
	
	y3= y2+23;	// exception!
	
	/* count total, max 16! */
	for(bt= U.themes.first; bt; bt= bt->next) tot++;
	
	/* if cur not is 1; move that to front of list */
	if(cur!=1) {
		a= 1;
		for(bt= U.themes.first; bt; bt= bt->next, a++) {
			if(a==cur) {
				BLI_remlink(&U.themes, bt);
				BLI_addhead(&U.themes, bt);
				cur= 1;
				break;
			}
		}
	}
	
	/* the current theme */
	btheme= U.themes.first;
	if(strcmp(btheme->name, "Default")==0) isbuiltin= 1;

	/* construct popup script */
	string[0]= 0;
	for(bt= U.themes.first; bt; bt= bt->next) {
		strcat(string, bt->name);
		if(btheme->next) strcat(string, "   |");
	}
	uiDefButS(block, MENU, B_UPDATE_THEME, string, 			45,y3,200,20, &cur, 0, 0, 0, 0, "Current theme");
	
	/* add / delete / name */

	if(tot<16)
		uiDefBut(block, BUT, B_ADD_THEME, "Add", 	45,y2,200,20, NULL, 0, 0, 0, 0, "Makes new copy of this theme");
	if(tot>1 && isbuiltin==0)
		uiDefBut(block, BUT, B_DEL_THEME, "Delete", 45,y1,200,20, NULL, 0, 0, 0, 0, "Delete theme");

	if(isbuiltin) return;
	
	/* name */
	uiDefBut(block, TEX, B_NAME_THEME, "", 			255,y3,200,20, btheme->name, 1.0, 30.0, 0, 0, "Rename theme");

	/* main choices pup */
	uiDefButS(block, MENU, B_CHANGE_THEME, "UI and Buttons %x1|3D View %x2|Ipo Curve Editor %x3|Action Editor %x4|"
		"NLA Editor %x5|UV/Image Editor %x6|Sequence Editor %x7|Sound Editor %x8|Text Editor %x9|User Preferences %x10|"
		"OOPS Schematic %x11|Buttons Window %x12|File Window %x13|Image Browser %x14",
													255,y2,200,20, &curmain, 0, 0, 0, 0, "Specify theme for...");
	if(curmain==1) spacetype= 0;
	else if(curmain==2) spacetype= SPACE_VIEW3D;
	else if(curmain==3) spacetype= SPACE_IPO;
	else if(curmain==4) spacetype= SPACE_ACTION;
	else if(curmain==5) spacetype= SPACE_NLA;
	else if(curmain==6) spacetype= SPACE_IMAGE;
	else if(curmain==7) spacetype= SPACE_SEQ;
	else if(curmain==8) spacetype= SPACE_SOUND;
	else if(curmain==9) spacetype= SPACE_TEXT;
	else if(curmain==10) spacetype= SPACE_INFO;
	else if(curmain==11) spacetype= SPACE_OOPS;
	else if(curmain==12) spacetype= SPACE_BUTS;
	else if(curmain==13) spacetype= SPACE_FILE;
	else if(curmain==14) spacetype= SPACE_IMASEL;
	else return; // only needed while coding... when adding themes for more windows
	
	/* color choices pup */
	if(curmain==1) {
		strp= BIF_ThemeColorsPup(0);
		if(th_curcol==TH_BACK) th_curcol= TH_BUT_NEUTRAL;  // switching main choices...
	}
	else strp= BIF_ThemeColorsPup(spacetype);
	
	uiDefButS(block, MENU, B_REDR, strp, 			255,y1,200,20, &th_curcol, 0, 0, 0, 0, "Current color");
	MEM_freeN(strp);
	
	th_curcol_ptr= col= BIF_ThemeGetColorPtr(btheme, spacetype, th_curcol);
	if(col==NULL) return;
	
	/* first handle exceptions, special single values, row selection, etc */
	if(th_curcol==TH_VERTEX_SIZE) {
		uiDefButC(block, NUMSLI, B_UPDATE_THEME,"Vertex size ",	465,y3,200,20,  col, 1.0, 10.0, 0, 0, "");
	}
	else if(th_curcol==TH_BUT_DRAWTYPE) {
		uiDefButC(block, ROW, B_UPDATE_THEME, "Minimal",	465,y3,200,20,  col, 2.0, 0.0, 0, 0, "");
		uiDefButC(block, ROW, B_UPDATE_THEME, "Default",	465,y2,200,20,  col, 2.0, 1.0, 0, 0, "");
	
	}
	else {
		uiDefButC(block, NUMSLI, B_UPDATE_THEME,"R ",	465,y3,200,20,  col, 0.0, 255.0, B_THEMECOL, 0, "");
		uiDefButC(block, NUMSLI, B_UPDATE_THEME,"G ",	465,y2,200,20,  col+1, 0.0, 255.0, B_THEMECOL, 0, "");
		uiDefButC(block, NUMSLI, B_UPDATE_THEME,"B ",	465,y1,200,20,  col+2, 0.0, 255.0, B_THEMECOL, 0, "");

		uiDefButC(block, COL, B_THEMECOL, "",		675,y1,50,y3-y1+20, col, 0, 0, 0, 0, "");
		if ELEM3(th_curcol, TH_PANEL, TH_FACE, TH_FACE_SELECT) {
			uiDefButC(block, NUMSLI, B_UPDATE_THEME,"A ",	465,y3+25,200,20,  col+3, 0.0, 255.0, B_THEMECOL, 0, "");
		}
		
		/* copy paste */
		uiDefBut(block, BUT, B_THEME_COPY, "Copy Color", 	755,y2,120,20, NULL, 0, 0, 0, 0, "Stores current color in buffer");
		uiDefBut(block, BUT, B_THEME_PASTE, "Paste Color", 	755,y1,120,20, NULL, 0, 0, 0, 0, "Pastes buffer color");

		uiDefButC(block, COL, 0, "",				885,y1,50,y2-y1+20, th_curcol_arr, 0, 0, 0, 0, "");
		
	}
}


void drawinfospace(ScrArea *sa, void *spacedata)
{
	uiBlock *block;
	static short cur_light=0, cur_light_var=0;
	float fac, col[3];
	short xpos, ypos, ypostab,  buth, rspace, dx, y1, y2, y3, y4, y2label, y3label, y4label;
	short smallprefbut, medprefbut, largeprefbut, smfileselbut;
	short edgespace, midspace;
	char naam[32];

	if(curarea->win==0) return;

	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	fac= ((float)curarea->winx)/1280.0f;
	myortho2(0.0, 1280.0, 0.0, curarea->winy/fac);
	
	sprintf(naam, "infowin %d", curarea->win);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->win);


	dx= (1280-90)/7;	/* spacing for use in equally dividing 'tab' row */

	xpos = 45;		/* left padding */
	ypos = 50;		/* bottom padding for buttons */
	ypostab = 10;		/* bottom padding for 'tab' row */


	buth = 20;		/* standard button height */

	smallprefbut = 94;	/* standard size for small preferences button */
	medprefbut = 193;	/* standard size for medium preferences button */
	largeprefbut = 292;	/* standard size for large preferences button */
	smfileselbut = buth;	/* standard size for fileselect button (square) */
	
	edgespace = 3;		/* space from edge of end 'tab' to edge of end button */
	midspace = 5;		/* horizontal space between buttons */

	rspace = 3;		/* default space between rows */

	y1 = ypos;		/* bottom padding of 1st (bottom) button row */
	y2 = ypos+buth+rspace;	/* bottom padding of 2nd button row */
	y3 = ypos+2*(buth+rspace)+(3*rspace);	/* bottom padding of 3rd button row */
	y4 = ypos+3*(buth+rspace)+(3*rspace);	/* bottom padding of 4th button row */

	y2label = y2-2;		/* adjustments to offset the labels down to align better */
	y3label = y3-(3*rspace)-2;	/* again for 3rd row */
	y4label = y4-2;		/* again for 4th row */


	/* set the colour to blue and draw the main 'tab' controls */

	uiBlockSetCol(block, TH_BUT_SETTING1);

	uiDefButI(block, ROW,B_USERPREF,"View & Controls",
		xpos,ypostab,(short)dx,buth,
		&U.userpref,1.0,0.0, 0, 0,"");
		
	uiDefButI(block, ROW,B_USERPREF,"Edit Methods",
		(short)(xpos+dx),ypostab,(short)dx,buth,
		&U.userpref,1.0,1.0, 0, 0,"");

	uiDefButI(block, ROW,B_USERPREF,"Language & Font",
		(short)(xpos+2*dx),ypostab,(short)dx,buth,
		&U.userpref,1.0,2.0, 0, 0,"");

	uiDefButI(block, ROW,B_USERPREF,"Themes",
		(short)(xpos+3*dx),ypostab,(short)dx,buth,
		&U.userpref,1.0,6.0, 0, 0,"");

	uiDefButI(block, ROW,B_USERPREF,"Auto Save",
		(short)(xpos+4*dx),ypostab,(short)dx,buth,
		&U.userpref,1.0,3.0, 0, 0,"");

	uiDefButI(block, ROW,B_USERPREF,"System & OpenGL",
		(short)(xpos+5*dx),ypostab,(short)dx,buth,
		&U.userpref,1.0,4.0, 0, 0,"");
		
	uiDefButI(block, ROW,B_USERPREF,"File Paths",
		(short)(xpos+6*dx),ypostab,(short)dx,buth,
		&U.userpref,1.0,5.0, 0, 0,"");

	uiBlockSetCol(block, TH_AUTO);

	/* end 'tab' controls */

        /* line 2: left x co-ord, top y co-ord, width, height */

	if(U.userpref == 6) {
		info_user_themebuts(block, y1, y2, y3);
	}
	else if (U.userpref == 0) { /* view & controls */

		uiDefBut(block, LABEL,0,"Display:",
			xpos,y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|11, 0, "ToolTips",
			(xpos+edgespace),y2,smallprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Displays tooltips (help tags) over buttons");

		uiDefButS(block, TOG|BIT|4, B_DRAWINFO, "Object Info",
			(xpos+edgespace+midspace+smallprefbut),y2,smallprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0,
			"Displays current object name and frame number in the 3D viewport");

		uiDefButS(block, TOG|BIT|4, 0, "Global Scene",
			(xpos+edgespace),y1,medprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Forces the current Scene to be displayed in all Screens");


		uiDefBut(block, LABEL,0,"Snap to grid:",
			(xpos+edgespace+medprefbut),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|1, 0, "Grab",
			(xpos+edgespace+medprefbut+midspace),y2,smallprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Move objects to grid units");

		uiDefButS(block, TOG|BIT|3, 0, "Size",
			(xpos+edgespace+medprefbut+midspace),y1,smallprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Scale objects to grid units");

		uiDefButS(block, TOG|BIT|2, 0, "Rotate",
			(xpos+edgespace+medprefbut+(2*midspace)+smallprefbut),y2,smallprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Rotate objects to grid units");



		uiDefBut(block, LABEL,0,"Menu Buttons:",
			(xpos+edgespace+medprefbut+(3*midspace)+(2*smallprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|9, 0, "Auto Open",
			(xpos+edgespace+medprefbut+(3*midspace)+(2*smallprefbut)),y2,smallprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0,
			"Automatic opening of menu buttons");

		uiDefButS(block, NUM, 0, "ThresA:",
			(xpos+edgespace+medprefbut+(3*midspace)+(2*smallprefbut)),y1,smallprefbut,buth,
			&(U.menuthreshold1), 1, 40, 0, 0,
			"Time in 1/10 seconds for auto open");

		uiDefButS(block, NUM, 0, "ThresB:",
			(xpos+edgespace+medprefbut+(4*midspace)+(3*smallprefbut)),y1,smallprefbut,buth,
			&(U.menuthreshold2), 1, 40, 0, 0,
			"Time in 1/10 seconds for auto open sublevels");


		uiDefBut(block, LABEL,0,"Toolbox Thresh.:",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, NUM, 0, "LMB:",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)),y2,smallprefbut,buth,
			&(U.tb_leftmouse), 2, 40, 0, 0,
			"Time in 1/10 seconds leftmouse hold to open toolbox");

		uiDefButS(block, NUM, 0, "RMB:",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)),y1,smallprefbut,buth,
			&(U.tb_rightmouse), 2, 40, 0, 0,
			"Time in 1/10 seconds for rightmouse to open toolbox");

			
		uiDefBut(block, LABEL,0,"View rotation:",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)+smallprefbut+2),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|5, B_DRAWINFO, "Trackball",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)+smallprefbut+2),y2,(smallprefbut+2),buth,
			&(U.flag), 0, 0, 0, 0,
			"Use trackball style rotation with middle mouse button");

		uiDefButS(block, TOGN|BIT|5, B_DRAWINFO, "Turntable",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)+smallprefbut+2),y1,(smallprefbut+2),buth,
			&(U.flag), 0, 0, 0, 0,
			"Use turntable style rotation with middle mouse button");



		uiDefButS(block, TOGN|BIT|10, B_DRAWINFO, "Rotate View",
			(xpos+edgespace+(4*midspace)+(4*medprefbut)),y2,(smallprefbut+2),buth,
			&(U.flag), 0, 0, 0, 0, "Default action for the middle mouse button");

		uiDefButS(block, TOG|BIT|10, B_DRAWINFO, "Pan View",
			(xpos+edgespace+(4*midspace)+(4*medprefbut)+smallprefbut+2),y2,(smallprefbut+2),buth,
			&(U.flag), 0, 0, 0, 0, "Default action for the middle mouse button");

		uiDefBut(block, LABEL,0,"Middle mouse button:",
			(xpos+edgespace+(3*midspace)+(4*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");
		uiDefButS(block, TOG|BIT|12, 0, "Emulate 3 Buttons",
			(xpos+edgespace+(4*midspace)+(4*medprefbut)),y1,medprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Emulates a middle mouse button with ALT+LeftMouse");




		uiDefBut(block, LABEL,0,"Mousewheel:",
			(xpos+edgespace+(4*midspace)+(5*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");
		uiDefButS(block, TOG|BIT|2, 0, "Invert Wheel Zoom",
			(xpos+edgespace+(5*midspace)+(5*medprefbut)),y1,medprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0,
			"Swaps mouse wheel zoom direction");


		uiDefButI(block, NUM, 0, "Scroll Lines:",
			(xpos+edgespace+(5*midspace)+(5*medprefbut)),y2,medprefbut,buth,
			&U.wheellinescroll, 0.0, 32.0, 0, 0,
			"The number of lines scrolled at a time with the mouse wheel");


	} else if (U.userpref == 1) { /* edit methods */


		uiDefBut(block, LABEL,0,"Material linked to:",
			xpos,y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOGN|BIT|8, B_DRAWINFO, "ObData",
			(xpos+edgespace),y2,(smallprefbut),buth,
			&(U.flag), 0, 0, 0, 0, "Link new objects' material to the obData block");

		uiDefButS(block, TOG|BIT|8, B_DRAWINFO, "Object",
			(xpos+edgespace+midspace+smallprefbut),y2,(smallprefbut),buth,
			&(U.flag), 0, 0, 0, 0, "Link new objects' material to the object block");



		uiDefBut(block, LABEL,0,"Mesh Undo",
			(xpos+edgespace+medprefbut),y3label, medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, NUMSLI, B_DRAWINFO, "Steps:",
			(xpos+edgespace+medprefbut+midspace),y2,(medprefbut),buth,
				&(U.undosteps), 1, 64, 0, 0, "Number of undo steps avail. in Editmode.  Smaller conserves memory.");


		uiDefBut(block, LABEL,0,"Auto keyframe on:",
			(xpos+edgespace+(2*medprefbut)+midspace),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|0, 0, "Action",
			(xpos+edgespace+(2*medprefbut)+(2*midspace)),y2,smallprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0, "Automatic keyframe insertion in action ipo curve");

		uiDefButS(block, TOG|BIT|1, 0, "Object",
			(xpos+edgespace+(2*medprefbut)+(3*midspace)+smallprefbut),y2,smallprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0, "Automatic keyframe insertion in object ipo curve");



                uiDefBut(block, LABEL,0,"Duplicate with object:",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)+smallprefbut),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|0, 0, "Mesh",
			(xpos+edgespace+(4*midspace)+(3*medprefbut)+smallprefbut),y2,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes mesh data to be duplicated with Shift+D");
		uiDefButS(block, TOG|BIT|9, 0, "Armature",
			(xpos+edgespace+(4*midspace)+(3*medprefbut)+smallprefbut),y1,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes armature data to be duplicated with Shift+D");

		uiDefButS(block, TOG|BIT|2, 0, "Surface",
			(xpos+edgespace+(5*midspace)+(3*medprefbut)+(2*smallprefbut)),y2,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes surface data to be duplicated with Shift+D");
		uiDefButS(block, TOG|BIT|5, 0, "Lamp",
			(xpos+edgespace+(5*midspace)+(3*medprefbut)+(2*smallprefbut)),y1,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes lamp data to be duplicated with Shift+D");

                uiDefButS(block, TOG|BIT|1, 0, "Curve",
			(xpos+edgespace+(6*midspace)+(3*medprefbut)+(3*smallprefbut)),y2,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes curve data to be duplicated with Shift+D");
		uiDefButS(block, TOG|BIT|7, 0, "Material",
			(xpos+edgespace+(6*midspace)+(3*medprefbut)+(3*smallprefbut)),y1,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes material data to be duplicated with Shift+D");

		uiDefButS(block, TOG|BIT|3, 0, "Text",
			(xpos+edgespace+(7*midspace)+(3*medprefbut)+(4*smallprefbut)),y2,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes text data to be duplicated with Shift+D");
		uiDefButS(block, TOG|BIT|8, 0, "Texture",
			(xpos+edgespace+(7*midspace)+(3*medprefbut)+(4*smallprefbut)),y1,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes texture data to be duplicated with Shift+D");

		uiDefButS(block, TOG|BIT|4, 0, "Metaball",
			(xpos+edgespace+(8*midspace)+(3*medprefbut)+(5*smallprefbut)),y2,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes metaball data to be duplicated with Shift+D");
		uiDefButS(block, TOG|BIT|6, 0, "Ipo",
			(xpos+edgespace+(8*midspace)+(3*medprefbut)+(5*smallprefbut)),y1,smallprefbut,buth,
			&(U.dupflag), 0, 0, 0, 0, "Causes ipo data to be duplicated with Shift+D");
	
	} else if(U.userpref == 2) { /* language & colors */

#ifdef INTERNATIONAL
		char curfont[64];

		sprintf(curfont, "Interface Font: ");
		strcat(curfont,U.fontname);

		uiDefButS(block, TOG|BIT|5, B_DOLANGUIFONT, "International Fonts",
			xpos,y2,medprefbut,buth,
			&(U.transopts), 0, 0, 0, 0, "Activate international interface");

		if(U.transopts & TR_ALL) {
			uiDefBut(block, LABEL,0,curfont,
				(xpos+edgespace+medprefbut+midspace),y2,medprefbut,buth,
				0, 0, 0, 0, 0, "");

			uiDefBut(block, BUT, B_LOADUIFONT, "Select Font",
				xpos,y1,medprefbut,buth,
				0, 0, 0, 0, 0, "Select a new font for the interface");


			uiDefButI(block, MENU|INT, B_SETFONTSIZE, fontsize_pup(),
				(xpos+edgespace+medprefbut+midspace),y1,medprefbut,buth,
				&U.fontsize, 0, 0, 0, 0, "Current interface font size (points)");

/*
			uiDefButS(block, MENU|SHO, B_SETENCODING, encoding_pup(),
				(xpos+edgespace+medprefbut+midspace),y1,medprefbut,buth,
				&U.encoding, 0, 0, 0, 0, "Current interface font encoding");


			uiDefBut(block, LABEL,0,"Translate:",
				(xpos+edgespace+(2.1*medprefbut)+(2*midspace)),y3label,medprefbut,buth,
				0, 0, 0, 0, 0, "");
*/

			uiDefButS(block, TOG|BIT|0, B_SETTRANSBUTS, "Tooltips",
				(xpos+edgespace+(2.2*medprefbut)+(3*midspace)),y1,smallprefbut,buth,
				&(U.transopts), 0, 0, 0, 0, "Translate tooltips");

			uiDefButS(block, TOG|BIT|1, B_SETTRANSBUTS, "Buttons",
				(xpos+edgespace+(2.2*medprefbut)+(4*midspace)+smallprefbut),y1,smallprefbut,buth,
				&(U.transopts), 0, 0, 0, 0, "Translate button labels");

			uiDefButS(block, TOG|BIT|2, B_SETTRANSBUTS, "Toolbox",
				(xpos+edgespace+(2.2*medprefbut)+(5*midspace)+(2*smallprefbut)),y1,smallprefbut,buth,
				&(U.transopts), 0, 0, 0, 0, "Translate toolbox menu");

			uiDefButS(block, MENU|SHO, B_SETLANGUAGE, language_pup(),
				(xpos+edgespace+(2.2*medprefbut)+(3*midspace)),y2,medprefbut+(0.5*medprefbut)+3,buth,
				&U.language, 0, 0, 0, 0, "Select interface language");
				
			/* uiDefButS(block, TOG|BIT|3, B_SETTRANSBUTS, "FTF All windows",
				(xpos+edgespace+(4*medprefbut)+(4*midspace)),y1,medprefbut,buth,
				&(U.transopts), 0, 0, 0, 0,
				"Use FTF drawing for fileselect and textwindow "
				"(under construction)");
			*/
		}

/* end of INTERNATIONAL */
#endif

	} else if(U.userpref == 3) { /* auto save */


		uiDefButS(block, TOG|BIT|0, B_RESETAUTOSAVE, "Auto Save Temp Files",
			(xpos+edgespace),y2,medprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Enables automatic saving of temporary files");

		if(U.flag & AUTOSAVE) {

			uiDefBut(block, BUT, B_LOADTEMP, "Open Recent",
				(xpos+edgespace),y1,medprefbut,buth,
				0, 0, 0, 0, 0,"Opens the most recently saved temporary file");

			uiDefButI(block, NUM, B_RESETAUTOSAVE, "Minutes:",
				(xpos+edgespace+medprefbut+midspace),y2,medprefbut,buth,
				&(U.savetime), 1.0, 60.0, 0, 0,
				"The time (in minutes) to wait between automatic temporary saves");

			uiDefButS(block, NUM, 0, "Versions:",
				(xpos+edgespace+medprefbut+midspace),y1,medprefbut,buth,
				&U.versions, 0.0, 32.0, 0, 0,
				"The number of old versions to maintain when saving");
		}

	} else if (U.userpref == 4) { /* system & opengl */
		uiDefBut(block, LABEL,0,"Solid OpenGL light:",
			xpos+edgespace, y3label, medprefbut, buth,
			0, 0, 0, 0, 0, "");
		
		uiDefButS(block, MENU, B_REDR, "Light1 %x0|Light2 %x1|Light3 %x2",
			xpos+edgespace, y2, 2*medprefbut/6, buth, &cur_light, 0.0, 0.0, 0, 0, "");
		uiBlockSetCol(block, TH_BUT_SETTING1);
		uiDefButI(block, TOG|BIT|0, B_RECALCLIGHT, "On",
			xpos+edgespace+2*medprefbut/6, y2, medprefbut/6, buth, 
			&U.light[cur_light].flag, 0.0, 0.0, 0, 0, "");
			
		uiBlockSetCol(block, TH_AUTO);
		uiDefButS(block, ROW, B_REDR, "Vec",
			xpos+edgespace+3*medprefbut/6, y2, medprefbut/6, buth, 
			&cur_light_var, 123.0, 0.0, 0, 0, "");
		uiDefButS(block, ROW, B_REDR, "Col",
			xpos+edgespace+4*medprefbut/6, y2, medprefbut/6, buth, 
			&cur_light_var, 123.0, 1.0, 0, 0, "");
		uiDefButS(block, ROW, B_REDR, "Spec",
			xpos+edgespace+5*medprefbut/6, y2, medprefbut/6, buth, 
			&cur_light_var, 123.0, 2.0, 0, 0, "");

		if(cur_light_var==1) {
			uiDefButF(block, NUM, B_RECALCLIGHT, "R ",
				xpos+edgespace, y1, medprefbut/3, buth, 
				U.light[cur_light].col, 0.0, 1.0, 100, 2, "");
			uiDefButF(block, NUM, B_RECALCLIGHT, "G ",
				xpos+edgespace+medprefbut/3, y1, medprefbut/3, buth, 
				U.light[cur_light].col+1, 0.0, 1.0, 100, 2, "");
			uiDefButF(block, NUM, B_RECALCLIGHT, "B ",
				xpos+edgespace+2*medprefbut/3, y1, medprefbut/3, buth, 
				U.light[cur_light].col+2, 0.0, 1.0, 100, 2, "");
		}
		else if(cur_light_var==2) {
			uiDefButF(block, NUM, B_RECALCLIGHT, "sR ",
				xpos+edgespace, y1, medprefbut/3, buth, 
				U.light[cur_light].spec, 0.0, 1.0, 100, 2, "");
			uiDefButF(block, NUM, B_RECALCLIGHT, "sG ",
				xpos+edgespace+medprefbut/3, y1, medprefbut/3, buth, 
				U.light[cur_light].spec+1, 0.0, 1.0, 100, 2, "");
			uiDefButF(block, NUM, B_RECALCLIGHT, "sB ",
				xpos+edgespace+2*medprefbut/3, y1, medprefbut/3, buth, 
				U.light[cur_light].spec+2, 0.0, 1.0, 100, 2, "");
		}
		else if(cur_light_var==0) {
			uiDefButF(block, NUM, B_RECALCLIGHT, "X ",
				xpos+edgespace, y1, medprefbut/3, buth, 
				U.light[cur_light].vec, -1.0, 1.0, 100, 2, "");
			uiDefButF(block, NUM, B_RECALCLIGHT, "Y ",
				xpos+edgespace+medprefbut/3, y1, medprefbut/3, buth, 
				U.light[cur_light].vec+1, -1.0, 1.0, 100, 2, "");
			uiDefButF(block, NUM, B_RECALCLIGHT, "Z ",
				xpos+edgespace+2*medprefbut/3, y1, medprefbut/3, buth, 
				U.light[cur_light].vec+2, -1.0, 1.0, 100, 2, "");
		}

/*
		uiDefButS(block, TOG|BIT|5, 0, "Log Events to Console",
			(xpos+edgespace),y2,largeprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0, "Display a list of input events in the console");

		uiDefButS(block, MENU|SHO, B_CONSOLEOUT, consolemethod_pup(),
			(xpos+edgespace), y1, largeprefbut,buth,
			&U.console_out, 0, 0, 0, 0, "Select console output method");

		uiDefButS(block, NUM, B_CONSOLENUMLINES, "Lines:",
			(xpos+edgespace+largeprefbut+midspace),y1,smallprefbut,buth,
			&U.console_buffer, 1.0, 4000.0, 0, 0, "Maximum number of internal console lines");
*/

#ifdef _WIN32
		uiDefBut(block, LABEL,0,"Win Codecs:",
			(xpos+edgespace+(1*midspace)+(1*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|8, 0, "Enable all codecs",
			(xpos+edgespace+(1*medprefbut)+(1*midspace)),y2,medprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0, "Allows all codecs for rendering (not guaranteed)");
#endif

		uiDefBut(block, LABEL,0,"Keyboard:",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButS(block, TOG|BIT|9, B_U_CAPSLOCK, "Disable Caps Lock",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)),y1,medprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Disables the Caps Lock key when entering text");

		uiDefButS(block, TOG|BIT|13, 0, "Emulate Numpad",
			(xpos+edgespace+(3*midspace)+(3*medprefbut)),y2,medprefbut,buth,
			&(U.flag), 0, 0, 0, 0,
			"Causes the 1 to 0 keys to act as the numpad (useful for laptops)");


		uiDefBut(block, LABEL,0,"System:",
			(xpos+edgespace+(4*midspace)+(4*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButI(block, TOG|BIT|USERDEF_DISABLE_SOUND_BIT, B_SOUNDTOGGLE, "Disable Sound",
			(xpos+edgespace+(4*medprefbut)+(4*midspace)),y2,medprefbut,buth,
			&(U.gameflags), 0, 0, 0, 0, "Disables sounds from being played");

		uiDefButS(block, TOG|BIT|3, 0, "Filter File Extensions",
			(xpos+edgespace+(4*medprefbut)+(4*midspace)),y1,medprefbut,buth,
			&(U.uiflag), 0, 0, 0, 0, "Display only files with extensions in the image select window");


		uiDefBut(block, LABEL,0,"OpenGL:",
			(xpos+edgespace+(5*midspace)+(5*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButI(block, TOGN|BIT|USERDEF_DISABLE_MIPMAP_BIT, B_MIPMAPCHANGED, "Mipmaps",
			(xpos+edgespace+(5*medprefbut)+(5*midspace)),y2,medprefbut,buth,
			&(U.gameflags), 0, 0, 0, 0, "Toggles between mipmap textures on (beautiful) and off (fast)");

		uiDefButI(block, TOG|BIT|USERDEF_VERTEX_ARRAYS_BIT, 0, "Vertex Arrays",
			(xpos+edgespace+(5*medprefbut)+(5*midspace)),y1,medprefbut,buth,
			&(U.gameflags), 0, 0, 0, 0, "Toggles between vertex arrays on (less reliable) and off (more reliable)");

		uiDefBut(block, LABEL,0,"Audio:",
			(xpos+edgespace+(2*midspace)+(2*medprefbut)),y3label,medprefbut,buth,
			0, 0, 0, 0, 0, "");

		uiDefButI(block, ROW, 0, "Mixing buffer 256", (xpos+edgespace+(2*midspace)+(2*medprefbut)),y2,medprefbut,buth, &U.mixbufsize, 2.0, 256.0, 0, 0, "Set audio buffer size to 256 samples");
		uiDefButI(block, ROW, 0, "512",	(xpos+edgespace+(2*midspace)+(2*medprefbut)),y1,61,buth, &U.mixbufsize, 2.0, 512.0, 0, 0, "Set audio buffer size to 512 samples");	
		uiDefButI(block, ROW, 0, "1024", (xpos+edgespace+(2*midspace)+(2*medprefbut))+61+midspace,y1,61,buth, &U.mixbufsize, 2.0, 1024.0, 0, 0, "Set audio buffer size to 1024 samples");		
		uiDefButI(block, ROW, 0, "2048", (xpos+edgespace+(2*midspace)+(2*medprefbut))+2*(61+midspace),y1,61,buth, &U.mixbufsize, 2.0, 2048.0, 0, 0, "Set audio buffer size to 2048 samples");			

	} else if(U.userpref == 5) { /* file paths */


		uiDefBut(block, TEX, 0, "Fonts: ",
			(xpos+edgespace),y2,(largeprefbut-smfileselbut),buth,
			U.fontdir, 1.0, 63.0, 0, 0,
			"The default directory to search for loading fonts");
		uiDefIconBut(block, BUT, B_FONTDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+largeprefbut-smfileselbut),y2,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default font directory");

		uiDefBut(block, TEX, 0, "Textures: ",
			(xpos+edgespace+largeprefbut+midspace),y2,(largeprefbut-smfileselbut),buth,
			U.textudir, 1.0, 63.0, 0, 0, "The default directory to search for textures");
		uiDefIconBut(block, BUT, B_TEXTUDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+(2*largeprefbut)+midspace-smfileselbut),y2,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default texture location");


		uiDefBut(block, TEX, 0, "Tex Plugins: ",
			(xpos+edgespace+(2*largeprefbut)+(2*midspace)),y2,(largeprefbut-smfileselbut),buth,
			U.plugtexdir, 1.0, 63.0, 0, 0, "The default directory to search for texture plugins");
		uiDefIconBut(block, BUT, B_PLUGTEXDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+(3*largeprefbut)+(2*midspace)-smfileselbut),y2,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default texture plugin location");

		uiDefBut(block, TEX, 0, "Seq Plugins: ",
			(xpos+edgespace+(3*largeprefbut)+(3*midspace)),y2,(largeprefbut-smfileselbut),buth,
			U.plugseqdir, 1.0, 63.0, 0, 0, "The default directory to search for sequence plugins");
		uiDefIconBut(block, BUT, B_PLUGSEQDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+(4*largeprefbut)+(3*midspace)-smfileselbut),y2,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default sequence plugin location");


		uiDefBut(block, TEX, 0, "Render: ",
			(xpos+edgespace),y1,(largeprefbut-smfileselbut),buth,
			U.renderdir, 1.0, 63.0, 0, 0, "The default directory for rendering output");
		uiDefIconBut(block, BUT, B_RENDERDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+largeprefbut-smfileselbut),y1,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default render output location");

		uiDefBut(block, TEX, 0, "Python: ",
			(xpos+edgespace+largeprefbut+midspace),y1,(largeprefbut-smfileselbut),buth,
			U.pythondir, 1.0, 63.0, 0, 0, "The default directory to search for Python scripts");
		uiDefIconBut(block, BUT, B_PYTHONDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+(2*largeprefbut)+midspace-smfileselbut),y1,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default Python script location");


		uiDefBut(block, TEX, 0, "Sounds: ",
			(xpos+edgespace+(2*largeprefbut)+(2*midspace)),y1,(largeprefbut-smfileselbut),buth,
			U.sounddir, 1.0, 63.0, 0, 0, "The default directory to search for sounds");
		uiDefIconBut(block, BUT, B_SOUNDDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+(3*largeprefbut)+(2*midspace)-smfileselbut),y1,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default sound location");

		uiDefBut(block, TEX, 0, "Temp: ",
			 (xpos+edgespace+(3*largeprefbut)+(3*midspace)),y1,(largeprefbut-smfileselbut),buth,
			 U.tempdir, 1.0, 63.0, 0, 0, "The directory for storing temporary save files");
		uiDefIconBut(block, BUT, B_TEMPDIRFILESEL, ICON_FILESEL,
			(xpos+edgespace+(4*largeprefbut)+(3*midspace)-smfileselbut),y1,smfileselbut,buth,
			0, 0, 0, 0, 0, "Select the default temporary save file location");

	}

	uiDrawBlock(block);
	
	myortho2(-0.5, (float)(sa->winx)-0.5, -0.5, (float)(sa->winy)-0.5);
	draw_area_emboss(sa);
	myortho2(0.0, 1280.0, 0.0, curarea->winy/fac);
	sa->win_swap= WIN_BACK_OK;
	
}


void winqreadinfospace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	
	if(val) {
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case UI_BUT_EVENT:
			if(val==B_ADD_THEME) {
				bTheme *btheme, *new;
				
				btheme= U.themes.first;
				new= MEM_callocN(sizeof(bTheme), "theme");
				memcpy(new, btheme, sizeof(bTheme));
				BLI_addhead(&U.themes, new);
				strcpy(new->name, "New User Theme");
				addqueue(sa->win, REDRAW, 1);
			}
			else if(val==B_DEL_THEME) {
				bTheme *btheme= U.themes.first;
				BLI_remlink(&U.themes, btheme);
				MEM_freeN(btheme);
				BIF_SetTheme(curarea); // prevent usage of old theme in calls	
				addqueue(sa->win, REDRAW, 1);
			}
			else if(val==B_NAME_THEME) {
				bTheme *btheme= U.themes.first;
				if(strcmp(btheme->name, "Default")==0) {
					strcpy(btheme->name, "New User Theme");
					addqueue(sa->win, REDRAW, 1);
				}
			}
			else if(val==B_UPDATE_THEME) {
				allqueue(REDRAWALL, 0);
			}
			else if(val==B_CHANGE_THEME) {
				th_curcol= TH_BACK;	// backdrop color is always there...
				addqueue(sa->win, REDRAW, 1);
			}
			else if(val==B_THEME_COPY) {
				if(th_curcol_ptr) {
					th_curcol_arr[0]= th_curcol_ptr[0];
					th_curcol_arr[1]= th_curcol_ptr[1];
					th_curcol_arr[2]= th_curcol_ptr[2];
					th_curcol_arr[3]= th_curcol_ptr[3];
					addqueue(sa->win, REDRAW, 1);
				}
			}
			else if(val==B_THEME_PASTE) {
				if(th_curcol_ptr) {
					th_curcol_ptr[0]= th_curcol_arr[0];
					th_curcol_ptr[1]= th_curcol_arr[1];
					th_curcol_ptr[2]= th_curcol_arr[2];
					th_curcol_ptr[3]= th_curcol_arr[3];
					allqueue(REDRAWALL, 0);
				}
			}
			else if(val==B_RECALCLIGHT) {
				if(U.light[0].flag==0 && U.light[1].flag==0 && U.light[2].flag==0)
					U.light[0].flag= 1;
				
				default_gl_light();
				addqueue(sa->win, REDRAW, 1);
				allqueue(REDRAWVIEW3D, 0);
			}
			else do_global_buttons(val);
			
			break;	
		}
	}
}

void init_infospace(ScrArea *sa)
{
	SpaceInfo *sinfo;
	
	sinfo= MEM_callocN(sizeof(SpaceInfo), "initinfo");
	BLI_addhead(&sa->spacedata, sinfo);

	sinfo->spacetype=SPACE_INFO;
}

/* ******************** SPACE: BUTS ********************** */

extern void drawbutspace(ScrArea *sa, void *spacedata);	/* buttons.c */

static void changebutspace(ScrArea *sa, void *spacedata)
{
	if(G.v2d==0) return;
	
	test_view2d(G.v2d, curarea->winx, curarea->winy);
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
}

void winqreadbutspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	SpaceButs *sbuts= curarea->spacedata.first;
	ScrArea *sa2, *sa3d;
	int nr, doredraw= 0;

	if(val) {
		
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;

		switch(event) {
		case UI_BUT_EVENT:
			do_butspace(val);
			break;
			
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		case PAGEUPKEY:
			event= WHEELUPMOUSE;
			view2dmove(event);	/* in drawipo.c */
			break;
		case PAGEDOWNKEY:
			event= WHEELDOWNMOUSE;
			view2dmove(event);	/* in drawipo.c */
			break;
			
		case RIGHTMOUSE:
			nr= pupmenu("Align buttons%t|Free %x0|Horizontal%x1|Vertical%x2");
			if (nr>=0) {
				sbuts->align= nr;
				if(nr) {
					uiAlignPanelStep(sa, 1.0);
					do_buts_buttons(B_BUTSHOME);
				}
			}

			break;
		case PADPLUSKEY:
			view2d_zoom(&sbuts->v2d, 0.06f, curarea->winx, curarea->winy);
			scrarea_queue_winredraw(curarea);
			break;
		case PADMINUS:
			view2d_zoom(&sbuts->v2d, -0.075f, curarea->winx, curarea->winy);
			scrarea_queue_winredraw(curarea);
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
			sa2= G.curscreen->areabase.first;
			while(sa2) {
				if(sa2->spacetype==SPACE_VIEW3D) {
					if(sa3d) return;
					sa3d= sa2;
				}
				sa2= sa2->next;
			}
			if(sa3d) {
				sa= curarea;
				areawinset(sa3d->win);
				
				if(event==PKEY) start_game();
				else if(event==ZKEY) toggle_shading();
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
	/* buts space goes from (0,0) to (1280, 228) */

	buts->v2d.tot.xmin= 0.0f;
	buts->v2d.tot.ymin= 0.0f;
	buts->v2d.tot.xmax= 1279.0f;
	buts->v2d.tot.ymax= 228.0f;
	
	buts->v2d.min[0]= 256.0f;
	buts->v2d.min[1]= 42.0f;

	buts->v2d.max[0]= 2048.0f;
	buts->v2d.max[1]= 450.0f;
	
	buts->v2d.minzoom= 0.5f;
	buts->v2d.maxzoom= 1.21f;
	
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

	/* set_rects only does defaults, so after reading a file the cur has not changed */
	set_rects_butspace(buts);
	buts->v2d.cur= buts->v2d.tot;
}

void extern_set_butspace(int fkey)
{
	ScrArea *sa;
	SpaceButs *sbuts;
	
	/* when a f-key pressed: closest button window is initialized */
	if(curarea->spacetype==SPACE_BUTS) sa= curarea;
	else {
		/* find area */
		sa= G.curscreen->areabase.first;
		while(sa) {
			if(sa->spacetype==SPACE_BUTS) break;
			sa= sa->next;
		}
	}
	
	if(sa==0) return;
	
	if(sa!=curarea) areawinset(sa->win);
	
	sbuts= sa->spacedata.first;
	
	if(fkey==F4KEY) {
		sbuts->mainb= CONTEXT_LOGIC;
	}
	else if(fkey==F5KEY) {
		sbuts->mainb= CONTEXT_SHADING;
		if(OBACT) {
			if(OBACT->type==OB_CAMERA) 
				sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_WORLD;
			else if(OBACT->type==OB_LAMP) 
				sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_LAMP;
			else  
				sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
		}
		else sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_MAT;
	}
	else if(fkey==F6KEY) {
		sbuts->mainb= CONTEXT_SHADING;
		sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_TEX;
	}
	else if(fkey==F7KEY) sbuts->mainb= CONTEXT_OBJECT;
	else if(fkey==F8KEY) {
		sbuts->mainb= CONTEXT_SHADING;
		sbuts->tab[CONTEXT_SHADING]= TAB_SHADING_WORLD;
	}
	else if(fkey==F9KEY) sbuts->mainb= CONTEXT_EDITING;
	else if(fkey==F10KEY) sbuts->mainb= CONTEXT_SCENE;

	scrarea_queue_headredraw(sa);
	scrarea_queue_winredraw(sa);
	BIF_preview_changed(sbuts);
}

/* ******************** SPACE: SEQUENCE ********************** */

/*  extern void drawseqspace(ScrArea *sa, void *spacedata); BIF_drawseq.h */

void winqreadseqspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
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
						update_for_newframe();	/* for audio scrubbing */						
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
				if((G.qual==0)) {
					dx= 0.1154*(v2d->cur.xmax-v2d->cur.xmin);
					v2d->cur.xmin+= dx;
					v2d->cur.xmax-= dx;
					test_view2d(G.v2d, curarea->winx, curarea->winy);
				}
				else if((G.qual==LR_SHIFTKEY)) {
					insert_gap(25, CFRA);
					allqueue(REDRAWSEQ, 0);
				}
				else if(G.qual==LR_ALTKEY) {
					insert_gap(250, CFRA);
					allqueue(REDRAWSEQ, 0);
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
				if((G.qual==LR_SHIFTKEY))
					no_gaps();
				else if((G.qual==0)) {
					dx= 0.15*(v2d->cur.xmax-v2d->cur.xmin);
					v2d->cur.xmin-= dx;
					v2d->cur.xmax+= dx;
					test_view2d(G.v2d, curarea->winx, curarea->winy);
				}
			}
			doredraw= 1;
			break;
		case HOMEKEY:
			if((G.qual==0))
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
			if((G.qual==LR_SHIFTKEY)) {
				add_sequence(-1);
			}
			else if((G.qual==0))
				swap_select_seq();
			break;
		case BKEY:
			if(sseq->mainb) break;
			if((G.qual==0))
				borderselect_seq();
			break;
		case CKEY:
			if((G.qual==0)) {
				if(last_seq && (last_seq->flag & (SEQ_LEFTSEL+SEQ_RIGHTSEL))) {
					if(last_seq->flag & SEQ_LEFTSEL) CFRA= last_seq->startdisp;
					else CFRA= last_seq->enddisp-1;
					
					dx= CFRA-(v2d->cur.xmax+v2d->cur.xmin)/2;
					v2d->cur.xmax+= dx;
					v2d->cur.xmin+= dx;
					update_for_newframe();
				}
				else
					change_sequence();
			}
			break;
		case DKEY:
			if(sseq->mainb) break;
			if((G.qual==LR_SHIFTKEY)) add_duplicate_seq();
			break;
		case EKEY:
			break;
		case FKEY:
			if((G.qual==0))
				set_filter_seq();
			break;
		case GKEY:
			if(sseq->mainb) break;
			if((G.qual==0))
				transform_seq('g');
			break;
		case MKEY:
			if(G.qual==LR_ALTKEY)
				un_meta();
			else if((G.qual==0)){
				if ((last_seq) && (last_seq->type == SEQ_SOUND)) 
				{
					last_seq->flag ^= SEQ_MUTE;
					doredraw = 1;
				}
				else
					make_meta();
			}
			break;
		case SKEY:
			if((G.qual==LR_SHIFTKEY))
				seq_snapmenu();
			break;
		case TKEY:
			if((G.qual==0))
				touch_seq_files();
			break;
		case XKEY:
		case DELKEY:
			if(G.qual==0) {
				if(sseq->mainb) break;
				if((G.qual==0))
					del_seq();
			}
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
	
	/* seq space goes from (0,8) to (250, 0) */

	sseq->v2d.tot.xmin= 0.0;
	sseq->v2d.tot.ymin= 0.0;
	sseq->v2d.tot.xmax= 250.0;
	sseq->v2d.tot.ymax= 8.0;
	
	sseq->v2d.cur= sseq->v2d.tot;

	sseq->v2d.min[0]= 10.0;
	sseq->v2d.min[1]= 4.0;

	sseq->v2d.max[0]= 32000.0;
	sseq->v2d.max[1]= MAXSEQ;
	
	sseq->v2d.minzoom= 0.1f;
	sseq->v2d.maxzoom= 10.0;
	
	sseq->v2d.scroll= L_SCROLL+B_SCROLL;
	sseq->v2d.keepaspect= 0;
	sseq->v2d.keepzoom= 0;
	sseq->v2d.keeptot= 0;
}

/* ******************** SPACE: ACTION ********************** */
extern void drawactionspace(ScrArea *sa, void *spacedata);
extern void winqreadactionspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

static void changeactionspace(ScrArea *sa, void *spacedata)
{
	if(G.v2d==0) return;

	/* this sets the sub-areas correct, for scrollbars */
	test_view2d(G.v2d, curarea->winx, curarea->winy);
	
	/* action space uses weird matrices... local calculated in a function */
	// myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
}


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

	saction->v2d.max[0]= 32000.0;
	saction->v2d.max[1]= 1000.0;
	
	saction->v2d.minzoom= 0.01;
	saction->v2d.maxzoom= 50;

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

extern void drawsoundspace(ScrArea *sa, void *spacedata);
extern void winqreadsoundspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

void init_soundspace(ScrArea *sa)
{
	SpaceSound *ssound;
	
	ssound= MEM_callocN(sizeof(SpaceSound), "initsoundspace");
	BLI_addhead(&sa->spacedata, ssound);

	ssound->spacetype= SPACE_SOUND;
	
	/* sound space goes from (0,8) to (250, 0) */

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
	
	ssound->v2d.minzoom= 0.1f;
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

/*  extern void drawimagespace(ScrArea *sa, void *spacedata); BIF_drawimage.h */

void winqreadimagespace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
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
				/* skipx is not set most of the times. Make a guess. */
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
				if((G.qual==0))
					select_swap_tface_uv();
				break;
			case BKEY:
				if((G.qual==0))
					borderselect_sima();
				break;
			case GKEY:
				if((G.qual==0))
					transform_tface_uv('g');
				break;
			case NKEY:
				if(G.qual==LR_CTRLKEY)
					replace_names_but();
				break;
			case RKEY:
				if((G.qual==0))
					transform_tface_uv('r');
				break;
			case SKEY:
				if((G.qual==0))
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
			if((G.qual==0))
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

extern void drawimaselspace(ScrArea *sa, void *spacedata);
extern void winqreadimaselspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);


/* everything to imasel.c */


/* ******************** SPACE: OOPS ********************** */

extern void drawoopsspace(ScrArea *sa, void *spacedata);

void winqreadoopsspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
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
		if((G.qual==0))
			do_oops_buttons(B_OOPSHOME);
		break;
		
	case AKEY:
		if((G.qual==0)) {
			swap_select_all_oops();
			scrarea_queue_winredraw(curarea);
		}
		break;
	case BKEY:
		if((G.qual==0))
			borderselect_oops();
		break;
	case GKEY:
		if((G.qual==0))
			transform_oops('g');
		break;
	case LKEY:
		if((G.qual==LR_SHIFTKEY))
			select_backlinked_oops();
		else if((G.qual==0))
			select_linked_oops();
		break;
	case SKEY:
		
		if(G.qual==LR_ALTKEY)
			shrink_oops();
		else if((G.qual==LR_SHIFTKEY))
			shuffle_oops();
		else if((G.qual==0))
			transform_oops('s');
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
	
	v2d->minzoom= 0.01f;
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

extern void drawtextspace(ScrArea *sa, void *spacedata);
extern void winqreadtextspace(struct ScrArea *sa, void *spacedata, struct BWinEvent *evt);

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

		
	/* exception: filespace */
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
	/* exception: imasel space */
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
	
	/* lb1 is kopie from lb2, from lb2 we free the file list */
	
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

	/* again: from old View3D restore localview (because full) */
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

/* is called everywhere in blender */
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
					buts= sa->spacedata.first;
					buts->re_align= 1;
					scrarea_queue_winredraw(sa);
					scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWBUTSHEAD:
				if(sa->spacetype==SPACE_BUTS) {
					scrarea_queue_headredraw(sa);
				}
				break;
			case REDRAWBUTSSCENE:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==CONTEXT_SCENE) {
						buts->re_align= 1;
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSOBJECT:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==CONTEXT_OBJECT) {
						buts->re_align= 1;
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSSHADING:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==CONTEXT_SHADING) {
						buts->re_align= 1;
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSEDIT:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==CONTEXT_EDITING) {
						buts->re_align= 1;
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSSCRIPT:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==CONTEXT_SCRIPT) {
						buts->re_align= 1;
						scrarea_queue_winredraw(sa);
						scrarea_queue_headredraw(sa);
					}
				}
				break;
			case REDRAWBUTSLOGIC:
				if(sa->spacetype==SPACE_BUTS) {
					buts= sa->spacedata.first;
					if(buts->mainb==CONTEXT_LOGIC) {
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
	/* draws alle areas that something identical to curarea */
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
	/* draws all areas that show something like curarea AND areas of 'type' */
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
	/* redraws all */
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
		spacetype_set_winfuncs(st, drawactionspace, changeactionspace, winqreadactionspace);
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
		spacetype_set_winfuncs(st, drawimaselspace, NULL, winqreadimaselspace);
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
		spacetype_set_winfuncs(st, drawipospace, changeview2dspace, winqreadipospace);
	}

	return st;
}
SpaceType *spacenla_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Nla");
		spacetype_set_winfuncs(st, drawnlaspace, changeview2dspace, winqreadnlaspace);
	}

	return st;
}
SpaceType *spaceoops_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Oops");
		spacetype_set_winfuncs(st, drawoopsspace, changeview2dspace, winqreadoopsspace);
	}

	return st;
}
SpaceType *spaceseq_get_type(void)
{
	static SpaceType *st= NULL;
	
	if (!st) {
		st= spacetype_new("Sequence");
		spacetype_set_winfuncs(st, drawseqspace, changeview2dspace, winqreadseqspace);
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
		spacetype_set_winfuncs(st, drawview3dspace, changeview3dspace, winqreadview3dspace);
	}

	return st;
}
