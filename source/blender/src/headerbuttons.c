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
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_storage_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_oops_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view2d_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_constraint_types.h"

#include "BKE_utildefines.h"

#include "BKE_constraint.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_exotic.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_ika.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_text.h"
#include "BKE_world.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"

#include "BIF_drawimage.h"
#include "BIF_drawoops.h"
#include "BIF_drawscene.h"
#include "BIF_drawtext.h"
#include "BIF_editarmature.h"
#include "BIF_editfont.h"
#include "BIF_editlattice.h"
#include "BIF_editconstraint.h"
#include "BIF_editmesh.h"
#include "BIF_editmesh.h"
#include "BIF_editsima.h"
#include "BIF_editsound.h"
#include "BIF_editsound.h"
#include "BIF_gl.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_mainqueue.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_usiblender.h"
#include "BIF_previewrender.h"
#include "BIF_writeimage.h"

#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_view.h"
#include "BSE_sequence.h"
#include "BSE_editaction.h"
#include "BSE_editaction_types.h"
#include "BSE_editipo.h"
#include "BSE_drawipo.h"

#include "BDR_drawmesh.h"
#include "BDR_vpaint.h"
#include "BDR_editface.h"
#include "BDR_editobject.h"
#include "BDR_editcurve.h"
#include "BDR_editmball.h"

#include "BPY_extern.h" // Blender Python library

#include "interface.h"
#include "mydevice.h"
#include "blendef.h"
#include "render.h"
#include "ipo.h"
#include "nla.h"	/* __NLA : To be removed later */

#include "TPT_DependKludge.h"

/* local (?) functions */
void do_file_buttons(short event);
void do_text_buttons(unsigned short event);
void load_space_sound(char *str);
void load_sound_buttons(char *str);
void load_space_image(char *str);
void image_replace(Image *old, Image *new);
void replace_space_image(char *str);
void do_image_buttons(unsigned short event);
void do_imasel_buttons(short event);
static void check_packAll(void);
static void unique_bone_name(Bone *bone, bArmature *arm);
static int bonename_exists(Bone *orig, char *name, ListBase *list);

static 	void test_idbutton_cb(void *namev, void *arg2_unused)
{
	char *name= namev;
	test_idbutton(name+2);
}

#define SPACEICONMAX	14 /* See release/datafiles/blenderbuttons */

#include "BIF_poseobject.h"

#include "SYS_System.h"

static int std_libbuttons(uiBlock *block, 
						  int xco, int pin, short *pinpoin, 
						  int browse, ID *id, ID *parid, 
						  short *menupoin, int users, 
						  int lib, int del, int autobut, int keepbut);


extern char versionstr[]; /* from blender.c */
/*  extern                void add_text_fs(char *file);  *//* from text.c, BIF_text.h*/

 /* WATCH IT:  always give all headerbuttons for same window the same name
  *			event B_REDR is a standard redraw
  *
  */


/* View3d->modeselect 
 * This is a bit of a dodgy hack to enable a 'mode' menu with icons+labels rather than those buttons.
 * I know the implementation's not good - it's an experiment to see if this approach would work well
 *
 * This can be cleaned when I make some new 'mode' icons.
 */
 
#define V3D_OBJECTMODE_SEL			ICON_ORTHO
#define V3D_EDITMODE_SEL			ICON_EDITMODE_HLT
#define V3D_FACESELECTMODE_SEL		ICON_FACESEL_HLT
#define V3D_VERTEXPAINTMODE_SEL		ICON_VPAINT_HLT
#define V3D_TEXTUREPAINTMODE_SEL	ICON_TPAINT_HLT
#define V3D_WEIGHTPAINTMODE_SEL		ICON_WPAINT_HLT
#define V3D_POSEMODE_SEL			ICON_POSE_HLT

#define XIC 20
#define YIC 20

static int viewmovetemp=0;

/*  extern void info_buttons(); in BSE_headerbuttons.c */

extern char videosc_dir[];	/* exotic.c */

/* *********************************************************************** */

void write_videoscape_fs()
{
	if(G.obedit) {
		error("Can't save Videoscape. Press TAB to leave EditMode");
	}
	else {
		if(videosc_dir[0]==0) strcpy(videosc_dir, G.sce);
		activate_fileselect(FILE_SPECIAL, "SAVE VIDEOSCAPE", videosc_dir, write_videoscape);
	}
}

void write_vrml_fs()
{
	if(G.obedit) {
		error("Can't save VRML. Press TAB to leave EditMode");
	}
	else {
		if(videosc_dir[0]==0) strcpy(videosc_dir, G.sce);
	
		activate_fileselect(FILE_SPECIAL, "SAVE VRML1", videosc_dir, write_vrml);
	}
	
}

void write_dxf_fs()
{
	if(G.obedit) {
		error("Can't save DXF. Press TAB to leave EditMode");
	}
	else {

		if(videosc_dir[0]==0) strcpy(videosc_dir, G.sce);

		activate_fileselect(FILE_SPECIAL, "SAVE DXF", videosc_dir, write_dxf);	
	}
}

/* ********************** GLOBAL ****************************** */

static int std_libbuttons(uiBlock *block, int xco, int pin, short *pinpoin, int browse, ID *id, ID *parid, short *menupoin, int users, int lib, int del, int autobut, int keepbut)
{
	ListBase *lb;
	Object *ob;
	Ipo *ipo;
	uiBut *but;
	int len, idwasnul=0, idtype, oldcol;
	char *str=NULL, str1[10];
	
	oldcol= uiBlockGetCol(block);

	if(id && pin) {
		uiDefIconButS(block, ICONTOG, pin, ICON_PIN_DEHLT,	(short)xco,0,XIC,YIC, pinpoin, 0, 0, 0, 0, "Keeps this view displaying the current data regardless of what object is selected");
		xco+= XIC;
	}
	if(browse) {
		if(id==0) {
			idwasnul= 1;
			/* only the browse button */
			ob= OBACT;
			if(curarea->spacetype==SPACE_IMAGE) {
				id= G.main->image.first;
			}
			else if(curarea->spacetype==SPACE_SOUND) {
				id= G.main->sound.first;
			}
			else if(curarea->spacetype==SPACE_ACTION) {
				id= G.main->action.first;
			}
			else if(curarea->spacetype==SPACE_NLA) {
				id=NULL;
			}
			else if(curarea->spacetype==SPACE_IPO) {
				id= G.main->ipo.first;
				/* test for ipotype */
				while(id) {
					ipo= (Ipo *)id;
					if(G.sipo->blocktype==ipo->blocktype) break;
					id= id->next;
				}
			}
			else if(curarea->spacetype==SPACE_BUTS) {
				if(browse==B_WORLDBROWSE) {
					id= G.main->world.first;
				}
				else if(ob && ob->type && (ob->type<OB_LAMP)) {
					if(G.buts->mainb==BUTS_MAT) id= G.main->mat.first;
					else if(G.buts->mainb==BUTS_TEX) id= G.main->tex.first;
				}
			}
			else if(curarea->spacetype==SPACE_TEXT) {
				id= G.main->text.first;
			}
		}
		if(id) {
			char *extrastr= NULL;
			
			idtype= GS(id->name);
			lb= wich_libbase(G.main, GS(id->name));
			
			if(idwasnul) id= NULL;
			else if(id->us>1) uiBlockSetCol(block, BUTDBLUE);

			if (pin && *pinpoin) {
				uiBlockSetCol(block, BUTDPINK);
			}
			
			if ELEM7( idtype, ID_SCE, ID_SCR, ID_MA, ID_TE, ID_WO, ID_IP, ID_AC) extrastr= "ADD NEW %x 32767";
			else if (idtype==ID_TXT) extrastr= "OPEN NEW %x 32766 |ADD NEW %x 32767";
			else if (idtype==ID_SO) extrastr= "OPEN NEW %x 32766";
			
			uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
			if( idtype==ID_SCE || idtype==ID_SCR ) uiClearButLock();
			
			if(curarea->spacetype==SPACE_BUTS)
				uiSetButLock(idtype!=ID_SCR && G.obedit!=0 && G.buts->mainb==BUTS_EDIT, NULL);
			
			if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");

			if (lb) {
				if( idtype==ID_IP)
					IPOnames_to_pupstring(&str, NULL, extrastr, lb, id, menupoin, G.sipo->blocktype);
				else
					IDnames_to_pupstring(&str, NULL, extrastr, lb, id, menupoin);
			}
			
			uiDefButS(block, MENU, browse, str, (short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses existing choices or adds NEW");
			
			uiClearButLock();
		
			MEM_freeN(str);
			xco+= XIC;
		}
		else if(curarea->spacetype==SPACE_BUTS) {
			if ELEM3(G.buts->mainb, BUTS_MAT, BUTS_TEX, BUTS_WORLD) {
				uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
				if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");
				uiDefButS(block, MENU, browse, "ADD NEW %x 32767",(short) xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
				uiClearButLock();
			} else if (G.buts->mainb == BUTS_SOUND) {
				uiDefButS(block, MENU, browse, "OPEN NEW %x 32766",(short) xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
			}
		}
		else if(curarea->spacetype==SPACE_TEXT) {
			uiDefButS(block, MENU, browse, "OPEN NEW %x 32766 | ADD NEW %x 32767", (short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
		}
		else if(curarea->spacetype==SPACE_SOUND) {
			uiDefButS(block, MENU, browse, "OPEN NEW %x 32766",(short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
		}
		else if(curarea->spacetype==SPACE_NLA) {
		}
		else if(curarea->spacetype==SPACE_ACTION) {
			uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
			if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");

			uiDefButS(block, MENU, browse, "ADD NEW %x 32767", xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
			uiClearButLock();
		}
		else if(curarea->spacetype==SPACE_IPO) {
			uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
			if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");

			uiDefButS(block, MENU, browse, "ADD NEW %x 32767", (short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
			uiClearButLock();
		}
	}


	uiBlockSetCol(block, oldcol);

	if(id) {
	
		/* name */
		if(id->us>1) uiBlockSetCol(block, BUTDBLUE);
		/* Pinned data ? */
		if (pin && *pinpoin) {
			uiBlockSetCol(block, BUTDPINK);
		}
		/* Redalert overrides pin color */
		if(id->us<=0) uiBlockSetCol(block, REDALERT);

		uiSetButLock(id->lib!=0, "Can't edit library data");
		
		str1[0]= id->name[0];
		str1[1]= id->name[1];
		str1[2]= ':';
		str1[3]= 0;
		if(strcmp(str1, "SC:")==0) strcpy(str1, "SCE:");
		else if(strcmp(str1, "SR:")==0) strcpy(str1, "SCR:");
		
		if( GS(id->name)==ID_IP) len= 110;
		else len= 120;
		
		but= uiDefBut(block, TEX, B_IDNAME, str1,(short)xco, 0, (short)len, YIC, id->name+2, 0.0, 19.0, 0, 0, "Displays current Datablock name. Click to change.");
		uiButSetFunc(but, test_idbutton_cb, id->name, NULL);

		uiClearButLock();

		xco+= len;
		
		if(id->lib) {
			
			if(parid && parid->lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Displays name of the current Indirect Library Datablock. Click to change.");
			else uiDefIconBut(block, BUT, lib, ICON_PARLIB,	(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Displays current Library Datablock name. Click to make local.");
			
			xco+= XIC;
		}
		
		
		if(users && id->us>1) {
			uiSetButLock (pin && *pinpoin, "Can't make pinned data single-user");
			
			sprintf(str1, "%d", id->us);
			if(id->us<100) {
				
				uiDefBut(block, BUT, users, str1,	(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Displays number of users of this data. Click to make a single-user copy.");
				xco+= XIC;
			}
			else {
				uiDefBut(block, BUT, users, str1,	(short)xco, 0, XIC+10, YIC, 0, 0, 0, 0, 0, "Displays number of users of this data. Click to make a single-user copy.");
				xco+= XIC+10;
			}
			
			uiClearButLock();
			
		}
		
		if(del) {

			uiSetButLock (pin && *pinpoin, "Can't unlink pinned data");
			if(parid && parid->lib);
			else {
				uiDefIconBut(block, BUT, del, ICON_X,	(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Deletes link to this Datablock");
				xco+= XIC;
			}

			uiClearButLock();
		}

		if(autobut) {
			if(parid && parid->lib);
			else {
				uiDefIconBut(block, BUT, autobut, ICON_AUTO,(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Generates an automatic name");
				xco+= XIC;
			}
			
			
		}
		if(keepbut) {
			uiDefBut(block, BUT, keepbut, "F", (short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Saves this datablock even if it has no users");	
			xco+= XIC;
		}
	}
	else xco+=XIC;
	
	uiBlockSetCol(block, oldcol);

	return xco;
}

void do_update_for_newframe(int mute)
{
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION,0);
	allqueue(REDRAWNLA,0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWINFO, 1);
	allqueue(REDRAWSEQ, 1);
	allqueue(REDRAWSOUND, 1);
	allqueue(REDRAWBUTSHEAD, 1);
	allqueue(REDRAWBUTSMAT, 1);
	allqueue(REDRAWBUTSLAMP, 1);

	/* layers/materials, object ipos are calculted in where_is_object (too) */
	do_all_ipos();
	BPY_do_all_scripts(SCRIPT_FRAMECHANGED);
	do_all_keys();
	do_all_actions();
	do_all_ikas();

	test_all_displists();
	
	if ( (CFRA>1) && (!mute) && (G.scene->audio.flag & AUDIO_SCRUB)) audiostream_scrub( CFRA );
}

void update_for_newframe(void)
{
	do_update_for_newframe(0);
}

void update_for_newframe_muted(void)
{
	do_update_for_newframe(1);
}

static void show_splash(void)
{
	extern char datatoc_splash_jpg[];
	extern int datatoc_splash_jpg_size;
	char *string = NULL;

#ifdef NAN_BUILDINFO
	char buffer[1024];
	extern char * build_date;
	extern char * build_time;
	extern char * build_platform;
	extern char * build_type;

	string = &buffer[0];
	sprintf(string,"Built on %s %s     Version %s %s", build_date, build_time, build_platform, build_type);
#endif

	splash((void *)datatoc_splash_jpg, datatoc_splash_jpg_size, string);
}


/* Functions for user preferences fileselect windows */

void filesel_u_fontdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.fontdir, dir);
	allqueue(REDRAWALL, 0);
}

void filesel_u_textudir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.textudir, dir);
	allqueue(REDRAWALL, 0);
}

void filesel_u_plugtexdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.plugtexdir, dir);
	allqueue(REDRAWALL, 0);
}

void filesel_u_plugseqdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.plugseqdir, dir);
	allqueue(REDRAWALL, 0);
}

void filesel_u_renderdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.renderdir, dir);
	allqueue(REDRAWALL, 0);
}

void filesel_u_pythondir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.pythondir, dir);
	allqueue(REDRAWALL, 0);
}

void filesel_u_sounddir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.sounddir, dir);
	allqueue(REDRAWALL, 0);
}

void filesel_u_tempdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	BLI_split_dirfile(name, dir, file);

	strcpy(U.tempdir, dir);
	allqueue(REDRAWALL, 0);
}

/* END Functions for user preferences fileselect windows */


void do_global_buttons(unsigned short event)
{
	ListBase *lb;
	Object *ob;
	Material *ma;
	MTex *mtex;
	Ipo *ipo;
	Lamp *la;
	World *wrld;
	Sequence *seq;
	bAction *act;
	ID *id, *idtest, *from;
	ScrArea *sa;
	int nr= 1;
	char buf[FILE_MAXDIR+FILE_MAXFILE];

	
	ob= OBACT;
	
	id= 0;	/* id at null for texbrowse */
	

	switch(event) {
	
	case B_NEWFRAME:
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);

		update_for_newframe();
		break;		
	case B_REDR:
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_REDRCURW3D:
		allqueue(REDRAWVIEW3D, 0);
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_EDITBROWSE:
		if(ob==0) return;
		if(ob->id.lib) return;
		id= ob->data;
		if(id==0) return;

		if(G.buts->menunr== -2) {
			activate_databrowse((ID *)G.buts->lockpoin, GS(id->name), 0, B_EDITBROWSE, &G.buts->menunr, do_global_buttons);
			return;
		}
		if(G.buts->menunr < 0) return;
		
		lb= wich_libbase(G.main, GS(id->name));
		idtest= lb->first;
		while(idtest) {
			if(nr==G.buts->menunr) {
				if(idtest!=id) {
					id->us--;
					id_us_plus(idtest);
					
					ob->data= idtest;
					
					test_object_materials(idtest);
					
					if( GS(idtest->name)==ID_CU ) {
						test_curve_type(ob);
						allqueue(REDRAWBUTSEDIT, 0);
						makeDispList(ob);
					}
					else if( ob->type==OB_MESH ) {
						makeDispList(ob);
					}
					
					allqueue(REDRAWBUTSEDIT, 0);
					allqueue(REDRAWVIEW3D, 0);
					allqueue(REDRAWACTION,0);
					allqueue(REDRAWIPO, 0);
					allqueue(REDRAWNLA,0);
				}
				break;
			}
			nr++;
			idtest= idtest->next;
		}

		break;
	case B_MESHBROWSE:
		if(ob==0) return;
		if(ob->id.lib) return;
		
		id= ob->data;
		if(id==0) id= G.main->mesh.first;
		if(id==0) return;
		
		if(G.buts->menunr== -2) {
			activate_databrowse((ID *)G.buts->lockpoin, GS(id->name), 0, B_MESHBROWSE, &G.buts->menunr, do_global_buttons);
			return;
		}
		if(G.buts->menunr < 0) return;
		

		idtest= G.main->mesh.first;
		while(idtest) {
			if(nr==G.buts->menunr) {
					
				set_mesh(ob, (Mesh *)idtest);
				
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWACTION,0);
				allqueue(REDRAWIPO, 0);

				break;
			}
			nr++;
			idtest= idtest->next;
		}

		break;
	case B_MATBROWSE:
		if(G.buts->menunr== -2) {
			activate_databrowse((ID *)G.buts->lockpoin, ID_MA, 0, B_MATBROWSE, &G.buts->menunr, do_global_buttons);
			return;
		}
		
		if(G.buts->menunr < 0) return;
		
		if(G.buts->pin) {
			
		}
		else {
			
			ma= give_current_material(ob, ob->actcol);
			nr= 1;
			
			id= (ID *)ma;
			
			idtest= G.main->mat.first;
			while(idtest) {
				if(nr==G.buts->menunr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) {	/* new mat */
				if(id)  idtest= (ID *)copy_material((Material *)id);
				else {
					idtest= (ID *)add_material("Material");
				}
				idtest->us--;
			}
			if(idtest!=id) {
				assign_material(ob, (Material *)idtest, ob->actcol);
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSMAT, 0);
				allqueue(REDRAWIPO, 0);
				BIF_preview_changed(G.buts);
			}
			
		}
		break;
	case B_MATDELETE:
		if(G.buts->pin) {
			
		}
		else {
			ma= give_current_material(ob, ob->actcol);
			if(ma) {
				assign_material(ob, 0, ob->actcol);
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSMAT, 0);
				allqueue(REDRAWIPO, 0);
				BIF_preview_changed(G.buts);
			}
		}
		break;
	case B_TEXDELETE:
		if(G.buts->pin) {
			
		}
		else {
			if(G.buts->texfrom==0) {	/* from mat */
				ma= give_current_material(ob, ob->actcol);
				if(ma) {
					mtex= ma->mtex[ ma->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						ma->mtex[ ma->texact ]= 0;
						allqueue(REDRAWBUTSTEX, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(G.buts);
					}
				}
			}
			else if(G.buts->texfrom==1) {	/* from world */
				wrld= G.scene->world;
				if(wrld) {
					mtex= wrld->mtex[ wrld->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						wrld->mtex[ wrld->texact ]= 0;
						allqueue(REDRAWBUTSTEX, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(G.buts);
					}
				}
			}
			else {	/* from lamp */
				la= ob->data;
				if(la && ob->type==OB_LAMP) {	/* to be sure */
					mtex= la->mtex[ la->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						la->mtex[ la->texact ]= 0;
						allqueue(REDRAWBUTSTEX, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(G.buts);
					}
				}
			}
		}
		break;
	case B_EXTEXBROWSE:	
	case B_TEXBROWSE:

		if(G.buts->texnr== -2) {
			
			id= G.buts->lockpoin;
			if(event==B_EXTEXBROWSE) {
				id= 0;
				ma= give_current_material(ob, ob->actcol);
				if(ma) {
					mtex= ma->mtex[ ma->texact ];
					if(mtex) id= (ID *)mtex->tex;
				}
			}
			
			activate_databrowse(id, ID_TE, 0, B_TEXBROWSE, &G.buts->texnr, do_global_buttons);
			return;
		}
		if(G.buts->texnr < 0) break;
		
		if(G.buts->pin) {
			
		}
		else {
			id= 0;
			
			ma= give_current_material(ob, ob->actcol);
			if(ma) {
				mtex= ma->mtex[ ma->texact ];
				if(mtex) id= (ID *)mtex->tex;
			}

			idtest= G.main->tex.first;
			while(idtest) {
				if(nr==G.buts->texnr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) {	/* new tex */
				if(id)  idtest= (ID *)copy_texture((Tex *)id);
				else idtest= (ID *)add_texture("Tex");
				idtest->us--;
			}
			if(idtest!=id && ma) {
				
				if( ma->mtex[ma->texact]==0) ma->mtex[ma->texact]= add_mtex();
				
				ma->mtex[ ma->texact ]->tex= (Tex *)idtest;
				id_us_plus(idtest);
				if(id) id->us--;
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSTEX, 0);
				allqueue(REDRAWBUTSMAT, 0);
				allqueue(REDRAWIPO, 0);
				BIF_preview_changed(G.buts);
			}
		}
		break;
	case B_ACTIONDELETE:
		act=ob->action;
		
		if (act)
			act->id.us--;
		ob->action=NULL;
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWIPO, 0);
		break;
	case B_ACTIONBROWSE:
		if (!ob)
			break;
		act=ob->action;
		id= (ID *)act;

		if (G.saction->actnr== -2){
				activate_databrowse((ID *)G.saction->action, ID_AC,  0, B_ACTIONBROWSE, &G.saction->actnr, do_global_buttons);
			return;
		}

		if(G.saction->actnr < 0) break;

		/*	See if we have selected a valid action */
		for (idtest= G.main->action.first; idtest; idtest= idtest->next) {
				if(nr==G.saction->actnr) {
					break;
				}
				nr++;
			
		}

		if(G.saction->pin) {
			G.saction->action= (bAction *)idtest;
			allqueue(REDRAWACTION, 0);
		}
		else {

			/* Store current action */
			if (!idtest){
				if (act)
					idtest= (ID *)copy_action(act);
				else 
					idtest=(ID *)add_empty_action();
				idtest->us--;
			}
			
			
			if(idtest!=id && ob) {
				act= (bAction *)idtest;
				
				ob->action= act;
				ob->activecon=NULL;
				id_us_plus(idtest);
				
				if(id) id->us--;
				
				// Update everything
				do_global_buttons (B_NEWFRAME);
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWNLA, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWHEADERS, 0);	
			}
		}
		
		break;
	case B_IPOBROWSE:

		ipo= get_ipo_to_edit(&from);
		id= (ID *)ipo;
		if(from==0) return;

		if(G.sipo->menunr== -2) {
			activate_databrowse((ID *)G.sipo->ipo, ID_IP, GS(from->name), B_IPOBROWSE, &G.sipo->menunr, do_global_buttons);
			return;
		}

		if(G.sipo->menunr < 0) break;

		idtest= G.main->ipo.first;
		while(idtest) {
			if( ((Ipo *)idtest)->blocktype == G.sipo->blocktype) {
				if(nr==G.sipo->menunr) {
					break;
				}
				nr++;
			}
			idtest= idtest->next;
		}

		if(G.sipo->pin) {
			if(idtest) {
				G.sipo->ipo= (Ipo *)idtest;
				allspace(REMAKEIPO, 0);		// in fact it should only do this one, but there is no function for it
			}
		}
		else {
			// assign the ipo to ...

			if(idtest==0) {
				if(ipo) idtest= (ID *)copy_ipo(ipo);
				else {
					nr= GS(from->name);
					if(nr==ID_OB){
						if (G.sipo->blocktype==IPO_CO)
							idtest= (ID *)add_ipo("CoIpo", IPO_CO);	/* BLEARGH! */
						else
							idtest= (ID *)add_ipo("ObIpo", nr);
					}
					else if(nr==ID_MA) idtest= (ID *)add_ipo("MatIpo", nr);
					else if(nr==ID_SEQ) idtest= (ID *)add_ipo("MatSeq", nr);
					else if(nr==ID_CU) idtest= (ID *)add_ipo("CuIpo", nr);
					else if(nr==ID_KE) idtest= (ID *)add_ipo("KeyIpo", nr);
					else if(nr==ID_WO) idtest= (ID *)add_ipo("WoIpo", nr);
					else if(nr==ID_LA) idtest= (ID *)add_ipo("LaIpo", nr);
					else if(nr==ID_CA) idtest= (ID *)add_ipo("CaIpo", nr);
					else if(nr==ID_SO) idtest= (ID *)add_ipo("SndIpo", nr);
					else if(nr==ID_AC) idtest= (ID *)add_ipo("ActIpo", nr);
					else error("Warn bugs@blender.nl!");
				}
				idtest->us--;
			}
			if(idtest!=id && from) {
				ipo= (Ipo *)idtest;
		
				if (ipo->blocktype==IPO_CO){
					((Object*)from)->activecon->ipo = ipo;
					id_us_plus(idtest);
					allqueue(REDRAWVIEW3D, 0);
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
				}
				else if(ipo->blocktype==ID_OB) {
					( (Object *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWVIEW3D, 0);
				}
				else if(ipo->blocktype==ID_AC) {
					bActionChannel *chan;
					chan = get_hilighted_action_channel ((bAction*)from);
					if (!chan){
						error ("Create an action channel first");
						return;
					}
					chan->ipo=ipo;
					id_us_plus(idtest);
					allqueue(REDRAWNLA, 0);
					allqueue(REDRAWACTION, 0);
				}
				else if(ipo->blocktype==ID_MA) {
					( (Material *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWBUTSMAT, 0);
				}
				else if(ipo->blocktype==ID_SEQ) {
					seq= (Sequence *)from;
					if((seq->type & SEQ_EFFECT)||(seq->type == SEQ_SOUND)) {
						id_us_plus(idtest);
						seq->ipo= ipo;
					}
				}
				else if(ipo->blocktype==ID_CU) {
					( (Curve *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWVIEW3D, 0);
				}
				else if(ipo->blocktype==ID_KE) {
					( (Key *)from)->ipo= ipo;
					
					id_us_plus(idtest);
					allqueue(REDRAWVIEW3D, 0);
					
				}
				else if(ipo->blocktype==ID_WO) {
					( (World *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWBUTSWORLD, 0);
				}
				else if(ipo->blocktype==ID_LA) {
					( (Lamp *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWBUTSLAMP, 0);
				}
				else if(ipo->blocktype==ID_CA) {
					( (Camera *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWBUTSEDIT, 0);
				}
				else if(ipo->blocktype==ID_SO) {
					( (bSound *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWBUTSEDIT, 0);
				}
				else
					printf("error in browse ipo \n");
				
				if(id) id->us--;
				
				scrarea_queue_winredraw(curarea);
				scrarea_queue_headredraw(curarea);
				allqueue(REDRAWIPO, 0);
			}
		}
		break;
	case B_IPODELETE:
		ipo= get_ipo_to_edit(&from);
		if(from==0) return;
		
		ipo->id.us--;
		
		if(ipo->blocktype==ID_OB) ( (Object *)from)->ipo= 0;
		else if(ipo->blocktype==ID_MA) ( (Material *)from)->ipo= 0;
		else if(ipo->blocktype==ID_SEQ) ( (Sequence *)from)->ipo= 0;
		else if(ipo->blocktype==ID_CU) ( (Curve *)from)->ipo= 0;
		else if(ipo->blocktype==ID_KE) ( (Key *)from)->ipo= 0;
		else if(ipo->blocktype==ID_WO) ( (World *)from)->ipo= 0;
		else if(ipo->blocktype==ID_LA) ( (Lamp *)from)->ipo= 0;
		else if(ipo->blocktype==ID_WO) ( (World *)from)->ipo= 0;
		else if(ipo->blocktype==ID_CA) ( (Camera *)from)->ipo= 0;
		else if(ipo->blocktype==ID_SO) ( (bSound *)from)->ipo= 0;
		else if(ipo->blocktype==ID_AC) {
			bAction *act = (bAction*) from;
			bActionChannel *chan = 
				get_hilighted_action_channel((bAction*)from);
			BLI_freelinkN (&act->chanbase, chan);
		}
		else if(ipo->blocktype==IPO_CO) ((Object *)from)->activecon->ipo= 0;

		else error("Warn bugs@blender.nl!");
		
		editipo_changed(G.sipo, 1);	/* doredraw */
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWNLA, 0);
		allqueue (REDRAWACTION, 0);
		
		break;
	case B_WORLDBROWSE:

		if(G.buts->menunr==-2) {
			activate_databrowse((ID *)G.scene->world, ID_WO, 0, B_WORLDBROWSE, &G.buts->menunr, do_global_buttons);
			break;
		}

		if(G.buts->menunr < 0) break;
		/* no lock */
			
		wrld= G.scene->world;
		nr= 1;
		
		id= (ID *)wrld;
		
		idtest= G.main->world.first;
		while(idtest) {
			if(nr==G.buts->menunr) {
				break;
			}
			nr++;
			idtest= idtest->next;
		}
		if(idtest==0) {	/* new world */
			if(id) idtest= (ID *)copy_world((World *)id);
			else idtest= (ID *)add_world("World");
			idtest->us--;
		}
		if(idtest!=id) {
			G.scene->world= (World *)idtest;
			id_us_plus(idtest);
			if(id) id->us--;
			
			allqueue(REDRAWBUTSHEAD, 0);
			allqueue(REDRAWBUTSWORLD, 0);
			allqueue(REDRAWIPO, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	case B_WORLDDELETE:
		if(G.scene->world) {
			G.scene->world->id.us--;
			G.scene->world= 0;
			allqueue(REDRAWBUTSWORLD, 0);
			allqueue(REDRAWIPO, 0);
		}
		
		break;
	case B_WTEXBROWSE:

		if(G.buts->texnr== -2) {
			id= 0;
			wrld= G.scene->world;
			if(wrld) {
				mtex= wrld->mtex[ wrld->texact ];
				if(mtex) id= (ID *)mtex->tex;
			}

			activate_databrowse((ID *)id, ID_TE, 0, B_WTEXBROWSE, &G.buts->texnr, do_global_buttons);
			return;
		}
		if(G.buts->texnr < 0) break;

		if(G.buts->pin) {
			
		}
		else {
			id= 0;
			
			wrld= G.scene->world;
			if(wrld) {
				mtex= wrld->mtex[ wrld->texact ];
				if(mtex) id= (ID *)mtex->tex;
			}

			idtest= G.main->tex.first;
			while(idtest) {
				if(nr==G.buts->texnr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) {	/* new tex */
				if(id)  idtest= (ID *)copy_texture((Tex *)id);
				else idtest= (ID *)add_texture("Tex");
				idtest->us--;
			}
			if(idtest!=id && wrld) {
				
				if( wrld->mtex[wrld->texact]==0) {
					wrld->mtex[wrld->texact]= add_mtex();
					wrld->mtex[wrld->texact]->texco= TEXCO_VIEW;
				}
				wrld->mtex[ wrld->texact ]->tex= (Tex *)idtest;
				id_us_plus(idtest);
				if(id) id->us--;
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSTEX, 0);
				allqueue(REDRAWBUTSWORLD, 0);
				allqueue(REDRAWIPO, 0);
				BIF_preview_changed(G.buts);
			}
		}
		break;
	case B_LAMPBROWSE:
		/* no lock */
		if(ob==0) return;
		if(ob->type!=OB_LAMP) return;

		if(G.buts->menunr== -2) {
			activate_databrowse((ID *)G.buts->lockpoin, ID_LA, 0, B_LAMPBROWSE, &G.buts->menunr, do_global_buttons);
			return;
		}
		if(G.buts->menunr < 0) break;
		
		la= ob->data;
		nr= 1;
		id= (ID *)la;
		
		idtest= G.main->lamp.first;
		while(idtest) {
			if(nr==G.buts->menunr) {
				break;
			}
			nr++;
			idtest= idtest->next;
		}
		if(idtest==0) {	/* no new lamp */
			return;
		}
		if(idtest!=id) {
			ob->data= (Lamp *)idtest;
			id_us_plus(idtest);
			if(id) id->us--;
			
			allqueue(REDRAWBUTSHEAD, 0);
			allqueue(REDRAWBUTSLAMP, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIPO, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	
	case B_LTEXBROWSE:

		if(ob==0) return;
		if(ob->type!=OB_LAMP) return;

		if(G.buts->texnr== -2) {
			id= 0;
			la= ob->data;
			mtex= la->mtex[ la->texact ];
			if(mtex) id= (ID *)mtex->tex;

			activate_databrowse(id, ID_TE, 0, B_LTEXBROWSE, &G.buts->texnr, do_global_buttons);
			return;
		}
		if(G.buts->texnr < 0) break;

		if(G.buts->pin) {
			
		}
		else {
			id= 0;
			
			la= ob->data;
			mtex= la->mtex[ la->texact ];
			if(mtex) id= (ID *)mtex->tex;

			idtest= G.main->tex.first;
			while(idtest) {
				if(nr==G.buts->texnr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) {	/* new tex */
				if(id)  idtest= (ID *)copy_texture((Tex *)id);
				else idtest= (ID *)add_texture("Tex");
				idtest->us--;
			}
			if(idtest!=id && la) {
				
				if( la->mtex[la->texact]==0) {
					la->mtex[la->texact]= add_mtex();
					la->mtex[la->texact]->texco= TEXCO_GLOB;
				}
				la->mtex[ la->texact ]->tex= (Tex *)idtest;
				id_us_plus(idtest);
				if(id) id->us--;
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSTEX, 0);
				allqueue(REDRAWBUTSLAMP, 0);
				allqueue(REDRAWIPO, 0);
				BIF_preview_changed(G.buts);
			}
		}
		break;
	
	case B_IMAGEDELETE:
		G.sima->image= 0;
		image_changed(G.sima, 0);
		allqueue(REDRAWIMAGE, 0);
		break;
	
	case B_AUTOMATNAME:
		automatname(G.buts->lockpoin);
		allqueue(REDRAWBUTSHEAD, 0);
		break;		
	case B_AUTOTEXNAME:
		if(G.buts->mainb==BUTS_TEX) {
			autotexname(G.buts->lockpoin);
			allqueue(REDRAWBUTSHEAD, 0);
			allqueue(REDRAWBUTSTEX, 0);
		}
		else if(G.buts->mainb==BUTS_MAT) {
			ma= G.buts->lockpoin;
			if(ma->mtex[ ma->texact]) autotexname(ma->mtex[ma->texact]->tex);
			allqueue(REDRAWBUTSMAT, 0);
		}
		else if(G.buts->mainb==BUTS_WORLD) {
			wrld= G.buts->lockpoin;
			if(wrld->mtex[ wrld->texact]) autotexname(wrld->mtex[wrld->texact]->tex);
			allqueue(REDRAWBUTSWORLD, 0);
		}
		else if(G.buts->mainb==BUTS_LAMP) {
			la= G.buts->lockpoin;
			if(la->mtex[ la->texact]) autotexname(la->mtex[la->texact]->tex);
			allqueue(REDRAWBUTSLAMP, 0);
		}
		break;

	case B_RESETAUTOSAVE:
		reset_autosave();
		allqueue(REDRAWINFO, 0);
		break;
	case B_SOUNDTOGGLE:
		SYS_WriteCommandLineInt(SYS_GetSystem(), "noaudio", (U.gameflags & USERDEF_DISABLE_SOUND));
		break;
	case B_SHOWSPLASH:
	     	show_splash();
		break;
	case B_MIPMAPCHANGED:
		set_mipmap(!(U.gameflags & USERDEF_DISABLE_SOUND));
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_NEWSPACE:
		newspace(curarea, curarea->butspacetype);
		break;
	case B_LOADTEMP: 	/* is button from space.c */
		BIF_read_autosavefile();
		break;

	case B_USERPREF:
		allqueue(REDRAWINFO, 0);
		break;

	case B_DRAWINFO: 	/* is button from space.c  *info* */
		allqueue(REDRAWVIEW3D, 0);
		break;

	case B_FLIPINFOMENU: 	/* is button from space.c  *info* */
		scrarea_queue_headredraw(curarea);
		break;

#ifdef _WIN32	// FULLSCREEN
	case B_FLIPFULLSCREEN:
		if(U.uiflag & FLIPFULLSCREEN)
			U.uiflag &= ~FLIPFULLSCREEN;
		else
			U.uiflag |= FLIPFULLSCREEN;
		mainwindow_toggle_fullscreen((U.uiflag & FLIPFULLSCREEN));
		break;
#endif

	/* Fileselect windows for user preferences file paths */

	case B_FONTDIRFILESEL: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT FONT PATH", U.fontdir, filesel_u_fontdir);
		break;

	case B_TEXTUDIRFILESEL: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEXTURE PATH", U.textudir, filesel_u_textudir);
		break;
	
	case B_PLUGTEXDIRFILESEL: 	/* is button form space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEX PLUGIN PATH", U.plugtexdir, filesel_u_plugtexdir);
		break;
	
	case B_PLUGSEQDIRFILESEL: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SEQ PLUGIN PATH", U.plugseqdir, filesel_u_plugseqdir);
		break;
	
	case B_RENDERDIRFILESEL: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT RENDER PATH", U.renderdir, filesel_u_renderdir);
		break;
	
	case B_PYTHONDIRFILESEL: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SCRIPT PATH", U.pythondir, filesel_u_pythondir);
		break;

	case B_SOUNDDIRFILESEL: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SOUND PATH", U.sounddir, filesel_u_sounddir);
		break;

	case B_TEMPDIRFILESEL: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEMP FILE PATH", U.tempdir, filesel_u_tempdir);
		break;

	/* END Fileselect windows for user preferences file paths */


#ifdef INTERNATIONAL
	case B_LOADUIFONT: 	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}
		BLI_make_file_string("/", buf, U.fontdir, U.fontname);
		activate_fileselect(FILE_SPECIAL, "LOAD UI FONT", buf, set_interface_font);
		break;

	case B_SETLANGUAGE: 	/* is button from space.c  *info* */
		lang_setlanguage();
		allqueue(REDRAWALL, 0);
		break;

	case B_SETFONTSIZE: 	/* is button from space.c  *info* */
		FTF_SetSize(U.fontsize);
		allqueue(REDRAWALL, 0);
		break;
		
	case B_SETTRANSBUTS: 	/* is button from space.c  *info* */
		allqueue(REDRAWALL, 0);
		break;

	case B_DOLANGUIFONT: 	/* is button from space.c  *info* */
		if(U.transopts & TR_ALL)
			start_interface_font();
		else
			G.ui_international = FALSE;
		allqueue(REDRAWALL, 0);
		break;
#endif
		
	case B_FULL:
		if(curarea->spacetype!=SPACE_INFO) {
			area_fullscreen();
		}
		break;	

	case B_IDNAME:
			/* changing a metaballs name, sadly enough,
			 * can require it to be updated because its
			 * basis might have changed... -zr
			 */
		if (OBACT && OBACT->type==OB_MBALL)
			makeDispList(OBACT);
			
		/* redraw because name has changed: new pup */
		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWBUTSHEAD, 0);
		allqueue(REDRAWINFO, 1);
		allqueue(REDRAWOOPS, 1);
		/* name scene also in set PUPmenu */
		if ELEM(curarea->spacetype, SPACE_BUTS, SPACE_INFO) allqueue(REDRAWBUTSALL, 0);

		allqueue(REDRAWHEADERS, 0);

		break;
	
	case B_KEEPDATA:
		/* keep datablock. similar to pressing FKEY in a fileselect window
		 * maybe we can move that stuff to a seperate function? -- sg
		 */
		if (curarea->spacetype==SPACE_BUTS) {
			id= (ID *)G.buts->lockpoin;
		} else if(curarea->spacetype==SPACE_IPO) {
			id = (ID *)G.sipo->ipo;
		} /* similar for other spacetypes ? */
		if (id) {
			if( id->flag & LIB_FAKEUSER) {
				id->flag -= LIB_FAKEUSER;
				id->us--;
			} else {
				id->flag |= LIB_FAKEUSER;
				id->us++;
			}
		}
		allqueue(REDRAWHEADERS, 0);

		break;

	}
}


void do_global_buttons2(short event)
{
	Base *base;
	Object *ob;
	Material *ma;
	MTex *mtex;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Ipo *ipo;
	Lamp *la;
	Lattice *lt;
	World *wrld;
	ID *idfrom;	
	bAction *act;

	/* general:  Single User is allowed when from==LOCAL 
	 *			 Make Local is allowed when (from==LOCAL && id==LIB)
	 */
	
	ob= OBACT;
	
	switch(event) {
		
	case B_LAMPALONE:
		if(ob && ob->id.lib==0) {
			la= ob->data;
			if(la->id.us>1) {
				if(okee("Single user")) {
					ob->data= copy_lamp(la);
					la->id.us--;
				}
			}
		}
		break;
	case B_LAMPLOCAL:
		if(ob && ob->id.lib==0) {
			la= ob->data;
			if(la->id.lib) {
				if(okee("Make local")) {
					make_local_lamp(la);
				}
			}
		}
		break;
	
	case B_ARMLOCAL:
		if (ob&&ob->id.lib==0){
			bArmature *arm=ob->data;
			if (arm->id.lib){
				if(okee("Make local")) {
					make_local_armature(arm);
				}
			}
		}
		break;
	case B_ARMALONE:
		if(ob && ob->id.lib==0) {
			bArmature *arm=ob->data;
			if(arm->id.us>1) {
				if(okee("Single user")) {
					ob->data= copy_armature(arm);
					arm->id.us--;
				}
			}
		}
		break;
	case B_ACTLOCAL:
		if(ob && ob->id.lib==0) {
			act= ob->action;
			if(act->id.lib) {
				if(okee("Make local")) {
					make_local_action(act);
					allqueue(REDRAWACTION,0);
				}
			}
		}
		break;
	case B_ACTALONE:
		if(ob && ob->id.lib==0) {
			act= ob->action;
		
			if(act->id.us>1) {
				if(okee("Single user")) {
					ob->action=copy_action(act);
					ob->activecon=NULL;
					act->id.us--;
					allqueue(REDRAWACTION, 0);
				}
			}
		}
		break;

	case B_CAMERAALONE:
		if(ob && ob->id.lib==0) {
			Camera *ca= ob->data;
			if(ca->id.us>1) {
				if(okee("Single user")) {
					ob->data= copy_camera(ca);
					ca->id.us--;
				}
			}
		}
		break;
	case B_CAMERALOCAL:
		if(ob && ob->id.lib==0) {
			Camera *ca= ob->data;
			if(ca->id.lib) {
				if(okee("Make local")) {
					make_local_camera(ca);
				}
			}
		}
		break;
	case B_WORLDALONE:
		wrld= G.scene->world;
		if(wrld->id.us>1) {
			if(okee("Single user")) {
				G.scene->world= copy_world(wrld);
				wrld->id.us--;
			}
		}
		break;
	case B_WORLDLOCAL:
		wrld= G.scene->world;
		if(wrld && wrld->id.lib) {
			if(okee("Make local")) {
				make_local_world(wrld);
			}
		}
		break;

	case B_LATTALONE:
		if(ob && ob->id.lib==0) {
			lt= ob->data;
			if(lt->id.us>1) {
				if(okee("Single user")) {
					ob->data= copy_lattice(lt);
					lt->id.us--;
				}
			}
		}
		break;
	case B_LATTLOCAL:
		if(ob && ob->id.lib==0) {
			lt= ob->data;
			if(lt->id.lib) {
				if(okee("Make local")) {
					make_local_lattice(lt);
				}
			}
		}
		break;
	
	case B_MATALONE:
		if(ob==0) return;
		ma= give_current_material(ob, ob->actcol);
		idfrom= material_from(ob, ob->actcol);
		if(idfrom && idfrom->lib==0) {
			if(ma->id.us>1) {
				if(okee("Single user")) {
					ma= copy_material(ma);
					ma->id.us= 0;
					assign_material(ob, ma, ob->actcol);
				}
			}
		}
		break;
	case B_MATLOCAL:
		if(ob==0) return;
		idfrom= material_from(ob, ob->actcol);
		if(idfrom->lib==0) {
			ma= give_current_material(ob, ob->actcol);
			if(ma && ma->id.lib) {
				if(okee("Make local")) {
					make_local_material(ma);
				}
			}
		}
		break;

	case B_MESHLOCAL:
		if(ob && ob->id.lib==0) {
			me= ob->data;
			if(me && me->id.lib) {
				if(okee("Make local")) {
					make_local_mesh(me);
					make_local_key( me->key );
				}
			}
		}
		break;

	case B_MBALLALONE:
		if(ob && ob->id.lib==0) {
			mb= ob->data;
			if(mb->id.us>1) {
				if(okee("Single user")) {
					ob->data= copy_mball(mb);
					mb->id.us--;
					if(ob==G.obedit) allqueue(REDRAWVIEW3D, 0);
				}
			}
		}
		break;
	case B_MBALLLOCAL:
		if(ob && ob->id.lib==0) {
			mb= ob->data;
			if(mb->id.lib) {
				if(okee("Make local")) {
					make_local_mball(mb);
				}
			}
		}
		break;

	case B_CURVEALONE:
		if(ob && ob->id.lib==0) {
			cu= ob->data;
			if(cu->id.us>1) {
				if(okee("Single user")) {
					ob->data= copy_curve(cu);
					cu->id.us--;
					makeDispList(ob);
					if(ob==G.obedit) allqueue(REDRAWVIEW3D, 0);
				}
			}
		}
		break;
	case B_CURVELOCAL:
		if(ob && ob->id.lib==0) {
			cu= ob->data;
			if(cu->id.lib) {
				if(okee("Make local")) {
					make_local_curve(cu);
					make_local_key( cu->key );
					makeDispList(ob);
				}
			}
		}
		break;
		
	case B_TEXALONE:
		if(G.buts->texfrom==0) {	/* from mat */
			if(ob==0) return;
			ma= give_current_material(ob, ob->actcol);
			if(ma && ma->id.lib==0) {
				mtex= ma->mtex[ ma->texact ];
				if(mtex->tex && mtex->tex->id.us>1) {
					if(okee("Single user")) {
						mtex->tex->id.us--;
						mtex->tex= copy_texture(mtex->tex);
					}
				}
			}
		}
		else if(G.buts->texfrom==1) {	/* from world */
			wrld= G.scene->world;
			if(wrld->id.lib==0) {
				mtex= wrld->mtex[ wrld->texact ];
				if(mtex->tex && mtex->tex->id.us>1) {
					if(okee("Single user")) {
						mtex->tex->id.us--;
						mtex->tex= copy_texture(mtex->tex);
					}
				}
			}
		}
		else if(G.buts->texfrom==2) {	/* from lamp */
			if(ob==0 || ob->type!=OB_LAMP) return;
			la= ob->data;
			if(la->id.lib==0) {
				mtex= la->mtex[ la->texact ];
				if(mtex->tex && mtex->tex->id.us>1) {
					if(okee("Single user")) {
						mtex->tex->id.us--;
						mtex->tex= copy_texture(mtex->tex);
					}
				}
			}
		}
		break;
	case B_TEXLOCAL:
		if(G.buts->texfrom==0) {	/* from mat */
			if(ob==0) return;
			ma= give_current_material(ob, ob->actcol);
			if(ma && ma->id.lib==0) {
				mtex= ma->mtex[ ma->texact ];
				if(mtex->tex && mtex->tex->id.lib) {
					if(okee("Make local")) {
						make_local_texture(mtex->tex);
					}
				}
			}
		}
		else if(G.buts->texfrom==1) {	/* from world */
			wrld= G.scene->world;
			if(wrld->id.lib==0) {
				mtex= wrld->mtex[ wrld->texact ];
				if(mtex->tex && mtex->tex->id.lib) {
					if(okee("Make local")) {
						make_local_texture(mtex->tex);
					}
				}
			}
		}
		else if(G.buts->texfrom==2) {	/* from lamp */
			if(ob==0 || ob->type!=OB_LAMP) return;
			la= ob->data;
			if(la->id.lib==0) {
				mtex= la->mtex[ la->texact ];
				if(mtex->tex && mtex->tex->id.lib) {
					if(okee("Make local")) {
						make_local_texture(mtex->tex);
					}
				}
			}
		}
		break;
	
	case B_IPOALONE:
		ipo= get_ipo_to_edit(&idfrom);
		
		if(idfrom && idfrom->lib==0) {
			if(ipo->id.us>1) {
				if(okee("Single user")) {
					if(ipo->blocktype==ID_OB) ((Object *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_MA) ((Material *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_SEQ) ((Sequence *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_CU) ((Curve *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_KE) ((Key *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_LA) ((Lamp *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_WO) ((World *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_CA) ((Camera *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_SO) ((bSound *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==ID_AC) get_hilighted_action_channel((bAction *)idfrom)->ipo= copy_ipo(ipo);
					else if(ipo->blocktype==IPO_CO) ((Object *)idfrom)->activecon->ipo= copy_ipo(ipo);
					else error("Warn ton!");
					
					ipo->id.us--;
					allqueue(REDRAWIPO, 0);
				}
			}
		}
		break;
	case B_IPOLOCAL:
		ipo= get_ipo_to_edit(&idfrom);
		
		if(idfrom && idfrom->lib==0) {
			if(ipo->id.lib) {
				if(okee("Make local")) {
					make_local_ipo(ipo);
					allqueue(REDRAWIPO, 0);
				}
			}
		}
		break;

	case B_OBALONE:
		if(G.scene->id.lib==0) {
			if(ob->id.us>1) {
				if(okee("Single user")) {
					base= FIRSTBASE;
					while(base) {
						if(base->object==ob) {
							base->object= copy_object(ob);
							ob->id.us--;
							allqueue(REDRAWVIEW3D, 0);
							break;
						}
						base= base->next;
					}
				}
			}
		}
		break;
	case B_OBLOCAL:
		if(G.scene->id.lib==0) {
			if(ob->id.lib) {
				if(okee("Make local")) {
					make_local_object(ob);
					allqueue(REDRAWVIEW3D, 0);
				}
			}
		}
		break;
	case B_MESHALONE:
		if(ob && ob->id.lib==0) {
			
			me= ob->data;
			
			if(me && me->id.us>1) {
				if(okee("Single user")) {
					Mesh *men= copy_mesh(me);
					men->id.us= 0;
					
					set_mesh(ob, men);
					
					if(ob==G.obedit) allqueue(REDRAWVIEW3D, 0);
				}
			}
		}
		break;
	}
	
	allqueue(REDRAWBUTSALL, 0);
	allqueue(REDRAWOOPS, 0);
}

/* ********************** EMPTY ****************************** */
/* ********************** INFO ****************************** */

int buttons_do_unpack()
{
	int how;
	char menu[2048];
	char line[128];
	int ret_value = RET_OK, count = 0;

	count = countPackedFiles();

	if (count) {
		if (count == 1) {
			sprintf(menu, "Unpack 1 file%%t");
		} else {
			sprintf(menu, "Unpack %d files%%t", count);
		}
		
		sprintf(line, "|Use files in current directory (create when necessary)%%x%d", PF_USE_LOCAL);
		strcat(menu, line);
	
		sprintf(line, "|Write files to current directory (overwrite existing files)%%x%d", PF_WRITE_LOCAL);
		strcat(menu, line);
	
		sprintf(line, "|%%l|Use files in original location (create when necessary)%%x%d", PF_USE_ORIGINAL);
		strcat(menu, line);
	
		sprintf(line, "|Write files to original location (overwrite existing files)%%x%d", PF_WRITE_ORIGINAL);
		strcat(menu, line);
	
		sprintf(line, "|%%l|Disable AutoPack, keep all packed files %%x%d", PF_KEEP);
		strcat(menu, line);
	
		sprintf(line, "|Ask for each file %%x%d", PF_ASK);
		strcat(menu, line);
		
		how = pupmenu(menu);
		
		if(how != -1) {
			if (how != PF_KEEP) {
				unpackAll(how);
			}
			G.fileflags &= ~G_AUTOPACK;
		} else {
			ret_value = RET_CANCEL;
		}
	} else {
		pupmenu("No packed files. Autopack disabled");
	}
	
	return (ret_value);
}

/* here, because of all creator stuff */

Scene *copy_scene(Scene *sce, int level)
{
	/* level 0: al objects shared
	 * level 1: al object-data shared
	 * level 2: full copy
	 */
	Scene *scen;
	Base *base, *obase;


	/* level 0 */
	scen= copy_libblock(sce);
	duplicatelist(&(scen->base), &(sce->base));
	
	clear_id_newpoins();
	
	id_us_plus((ID *)scen->world);
	id_us_plus((ID *)scen->set);
	
	scen->ed= 0;
	scen->radio= 0;
	
	obase= sce->base.first;
	base= scen->base.first;
	while(base) {
		base->object->id.us++;
		if(obase==sce->basact) scen->basact= base;
		
		obase= obase->next;
		base= base->next;
	}
	
	if(level==0) return scen;
	
	/* level 1 */
	G.scene= scen;
	single_object_users(0);

	/*  camera */
	ID_NEW(G.scene->camera);
		
	/* level 2 */
	if(level>=2) {
		if(scen->world) {
			scen->world->id.us--;
			scen->world= copy_world(scen->world);
		}
		single_obdata_users(0);
		single_mat_users_expand();
		single_tex_users_expand();
	}

	clear_id_newpoins();

	BPY_copy_scriptlink(&sce->scriptlink);



	// make a private copy of the avicodecdata

	if (sce->r.avicodecdata) {

		scen->r.avicodecdata = MEM_dupallocN(sce->r.avicodecdata);
		scen->r.avicodecdata->lpFormat = MEM_dupallocN(scen->r.avicodecdata->lpFormat);
		scen->r.avicodecdata->lpParms = MEM_dupallocN(scen->r.avicodecdata->lpParms);
	}

	// make a private copy of the qtcodecdata

	if (sce->r.qtcodecdata) {

		scen->r.qtcodecdata = MEM_dupallocN(sce->r.qtcodecdata);
		scen->r.qtcodecdata->cdParms = MEM_dupallocN(scen->r.qtcodecdata->cdParms);
	}
	return scen;
}

void do_info_buttons(unsigned short event)
{
	bScreen *sc, *oldscreen;
	Scene *sce, *sce1;
	ScrArea *sa;
	int nr;
	
	switch(event) {
	
	case B_INFOSCR:		/* menu select screen */

		if( G.curscreen->screennr== -2) {
			if(curarea->winy <50) {
				sa= closest_bigger_area();
				areawinset(sa->win);
			}
			activate_databrowse((ID *)G.curscreen, ID_SCR, 0, B_INFOSCR, &G.curscreen->screennr, do_info_buttons);
			return;
		}
		if( G.curscreen->screennr < 0) return;
		
		sc= G.main->screen.first;
		nr= 1;
		while(sc) {
			if(nr==G.curscreen->screennr) {
				if(is_allowed_to_change_screen(sc)) setscreen(sc);
				else error("Unable to perform function in EditMode");
				break;
			}
			nr++;
			sc= sc->id.next;
		}
		/* last item: NEW SCREEN */
		if(sc==0) {
			duplicate_screen();
		}
		break;
	case B_INFODELSCR:
		/* do this event only with buttons, so it can never be called with full-window */

		if(G.curscreen->id.prev) sc= G.curscreen->id.prev;
		else if(G.curscreen->id.next) sc= G.curscreen->id.next;
		else return;
		if(okee("Delete current screen")) {
			/* find new G.curscreen */
			
			oldscreen= G.curscreen;
			setscreen(sc);		/* this test if sc has a full */
			unlink_screen(oldscreen);
			free_libblock(&G.main->screen, oldscreen);
		}
		scrarea_queue_headredraw(curarea);

		break;
	case B_INFOSCE:		/* menu select scene */
		
		if( G.obedit) {
			error("Unable to perform function in EditMode");
			return;
		}
		if( G.curscreen->scenenr== -2) {
			if(curarea->winy <50) {
				sa= closest_bigger_area();
				areawinset(sa->win);
			}
			activate_databrowse((ID *)G.scene, ID_SCE, 0, B_INFOSCE, &G.curscreen->scenenr, do_info_buttons);
			return;
		}
		if( G.curscreen->scenenr < 0) return;

		sce= G.main->scene.first;
		nr= 1;
		while(sce) {
			if(nr==G.curscreen->scenenr) {
				if(sce!=G.scene) set_scene(sce);
				break;
			}
			nr++;
			sce= sce->id.next;
		}
		/* last item: NEW SCENE */
		if(sce==0) {
			nr= pupmenu("Add scene%t|Empty|Link Objects|Link ObData|Full Copy");
			if(nr<= 0) return;
			if(nr==1) {
				sce= add_scene(G.scene->id.name+2);
				sce->r= G.scene->r;
#ifdef _WIN32
				if (sce->r.avicodecdata) {
					sce->r.avicodecdata = MEM_dupallocN(G.scene->r.avicodecdata);
					sce->r.avicodecdata->lpFormat = MEM_dupallocN(G.scene->r.avicodecdata->lpFormat);
					sce->r.avicodecdata->lpParms = MEM_dupallocN(G.scene->r.avicodecdata->lpParms);
				}
#endif
#ifdef WITH_QUICKTIME
				if (sce->r.qtcodecdata) {
					sce->r.qtcodecdata = MEM_dupallocN(G.scene->r.qtcodecdata);
					sce->r.qtcodecdata->cdParms = MEM_dupallocN(G.scene->r.qtcodecdata->cdParms);
				}
#endif
			}
			else sce= copy_scene(G.scene, nr-2);
			
			set_scene(sce);
		}
		BIF_preview_changed(G.buts);

		break;
	case B_INFODELSCE:
		
		if(G.scene->id.prev) sce= G.scene->id.prev;
		else if(G.scene->id.next) sce= G.scene->id.next;
		else return;
		if(okee("Delete current scene")) {
			
			/* check all sets */
			sce1= G.main->scene.first;
			while(sce1) {
				if(sce1->set == G.scene) sce1->set= 0;
				sce1= sce1->id.next;
			}
			
			/* check all sequences */
			clear_scene_in_allseqs(G.scene);
			
			/* al screens */
			sc= G.main->screen.first;
			while(sc) {
				if(sc->scene == G.scene) sc->scene= sce;
				sc= sc->id.next;
			}
			free_libblock(&G.main->scene, G.scene);
			set_scene(sce);
		}
	
		break;
	case B_FILEMENU:
		tbox_setmain(9);
		toolbox();
		break;
	}
}

/* strubi shamelessly abused the status line as a progress bar... 
   feel free to kill him after release */

static int	g_progress_bar = 0;
static char *g_progress_info = 0;
static float g_done;

int start_progress_bar(void)
{
	g_progress_bar = 1;
	return 1;		// we never fail (yet)
}

void end_progress_bar(void)
{
	g_progress_bar = 0;
}

static void update_progress_bar(float done, char *info)
{
	g_done = done;
	g_progress_info = info;
}

/** Progress bar
	'done': a value between 0.0 and 1.0, showing progress
	'info': a info text what is currently being done

	Make sure that the progress bar is always called with:
	done = 0.0 first
		and
	done = 1.0 last -- or alternatively use:

	start_progressbar();
	do_stuff_and_callback_progress_bar();
	end_progressbar();
*/
int progress_bar(float done, char *busy_info)
{
	ScrArea *sa;
	short val; 

	/* User break (ESC) */
	while (qtest()) {
		if (extern_qread(&val) == ESCKEY) 
			return 0;
	}
	if (done == 0.0) {
		start_progress_bar();
	} else if (done > 0.99) {
		end_progress_bar();
	}

	sa= G.curscreen->areabase.first;
	while(sa) {
		if (sa->spacetype == SPACE_INFO) {
			update_progress_bar(done, busy_info);

			curarea = sa;

			scrarea_do_headdraw(curarea);
			areawinset(curarea->win);
			sa->head_swap= WIN_BACK_OK;
			screen_swapbuffers();
		}
		sa = sa->next;
	}
	return 1;
}


static void check_packAll()
{
	// first check for dirty images
	Image *ima;

	ima = G.main->image.first;
	while (ima) {
		if (ima->ibuf && (ima->ibuf->userflags &= IB_BITMAPDIRTY)) {
			break;
		}
		ima= ima->id.next;
   	}
	
	if (ima == 0 || okee("Some images are painted on. These changes will be lost. Continue ?")) {
		packAll();
		G.fileflags |= G_AUTOPACK;
	}
}


int write_runtime(char *str, char *exename)
{
	char *freestr= NULL;
	char *ext = 0;

#ifdef _WIN32
	ext = ".exe";
#endif

#ifdef __APPLE__
	ext = ".app";
#endif
	if (ext && (!BLI_testextensie(str, ext))) {
		freestr= MEM_mallocN(strlen(str) + strlen(ext) + 1, "write_runtime_check");
		strcpy(freestr, str);
		strcat(freestr, ext);
		str= freestr;
	}

	if (!BLI_exists(str) || saveover(str))
		BLO_write_runtime(str, exename);

	if (freestr)
		MEM_freeN(freestr);
	
	return 0;
}

static void write_runtime_check_dynamic(char *str) 
{
	write_runtime(str, "blenderdynplayer.exe");
}

static void write_runtime_check(char *str) 
{
	char player[128];

	strcpy(player, "blenderplayer");

#ifdef _WIN32
	strcat(player, ".exe");
#endif

#ifdef __APPLE__
	strcat(player, ".app");
#endif

	write_runtime(str, player);
}
/* end keyed functions */

static char *windowtype_pup(void)
{
	static char string[1024];

	strcpy(string, "Window type:%t"); //14
	strcat(string, "|3D Viewport %x1"); //30

	strcat(string, "|%l"); // 33

	strcat(string, "|Ipo Curve Editor %x2"); //54
	strcat(string, "|Action Editor %x12"); //73
	strcat(string, "|NLA Editor %x13"); //94

	strcat(string, "|%l"); //97

	strcat(string, "|UV/Image Editor %x6"); //117

	strcat(string, "|Video Sequence Editor %x8"); //143
	strcat(string, "|Audio Timeline %x11"); //163
	strcat(string, "|Text Editor %x9"); //179

	strcat(string, "|%l"); //192


	strcat(string, "|User Preferences %x7"); //213
	strcat(string, "|OOPS Schematic %x3"); //232
	strcat(string, "|Buttons Window %x4"); //251

	strcat(string, "|%l"); //254

	strcat(string, "|Image Browser %x10");
	strcat(string, "|File Browser %x5");

	return (string);
}

/************************** MAIN MENU *****************************/
/************************** FILE *****************************/

void do_info_file_optionsmenu(void *arg, int event)
{
	G.fileflags ^= (1 << event);

	// allqueue(REDRAWINFO, 0);
}

static uiBlock *info_file_optionsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, xco = 20;

	block= uiNewBlock(&curarea->uiblocks, "runtime_options", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_file_optionsmenu, NULL);
	uiBlockSetXOfs(block,-40);  // offset to parent button
	uiBlockSetCol(block, MENUCOL);
	
	/* flags are case-values */
	uiDefBut(block, BUTM, 1, "Compress File",	xco, yco-=20, 100, 19, NULL, 0.0, 0.0, 0, G_FILE_COMPRESS_BIT, "Enables file compression");
/*
	uiDefBut(block, BUTM, 1, "Sign File",	xco, yco-=20, 100, 19, NULL, 0.0, 0.0, 0, G_FILE_SIGN_BIT, "Add signature to file");
	uiDefBut(block, BUTM, 1, "Lock File",	xco, yco-=20, 100, 19, NULL, 0.0, 0.0, 0, G_FILE_LOCK_BIT, "Protect the file from editing by others");
*/
	uiTextBoundsBlock(block, 50);

	/* Toggle buttons */
	
	yco= 0;
	xco -= 20;
	uiBlockSetEmboss(block, UI_EMBOSSW);
	uiBlockSetButmFunc(block, NULL, NULL);
	/* flags are defines */
	uiDefIconButI(block, ICONTOG|BIT|G_FILE_COMPRESS_BIT, 0, ICON_CHECKBOX_DEHLT, xco, yco-=20, 19, 19, &G.fileflags, 0.0, 0.0, 0, 0, "");
/*
	uiDefIconButI(block, ICONTOG|BIT|G_FILE_SIGN_BIT, 0, ICON_CHECKBOX_DEHLT, xco, yco-=20, 19, 19, &G.fileflags, 0.0, 0.0, 0, 0, "");
	uiDefIconButI(block, ICONTOG|BIT|G_FILE_LOCK_BIT, 0, ICON_CHECKBOX_DEHLT, xco, yco-=20, 19, 19, &G.fileflags, 0.0, 0.0, 0, 0, "");
*/
	uiBlockSetDirection(block, UI_RIGHT);
		
	return block;
}

static uiBlock *info_runtime_optionsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, xco = 20;

	block= uiNewBlock(&curarea->uiblocks, "add_surfacemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetXOfs(block, -40);  // offset to parent button
	uiBlockSetCol(block, MENUCOL);
	uiBlockSetEmboss(block, UI_EMBOSSW);

	uiDefBut(block, LABEL, 0, "Size options:",		xco, yco-=20, 114, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, NUM, 0, "X:",		xco+19, yco-=20, 95, 19,    &G.scene->r.xplay, 10.0, 2000.0, 0, 0, "Displays current X screen/window resolution. Click to change.");
	uiDefButS(block, NUM, 0, "Y:",		xco+19, yco-=20, 95, 19, &G.scene->r.yplay, 10.0, 2000.0, 0, 0, "Displays current Y screen/window resolution. Click to change.");

	uiDefBut(block, SEPR, 0, "",		xco, yco-=4, 114, 4, NULL, 0.0, 0.0, 0, 0, "");

	uiDefBut(block, LABEL, 0, "Fullscreen options:",		xco, yco-=20, 114, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, TOG, 0, "Fullscreen", xco + 19, yco-=20, 95, 19, &G.scene->r.fullscreen, 0.0, 0.0, 0, 0, "Starts player in a new fullscreen display");
	uiDefButS(block, NUM, 0, "Freq:",	xco+19, yco-=20, 95, 19, &G.scene->r.freqplay, 10.0, 120.0, 0, 0, "Displays clock frequency of fullscreen display. Click to change.");
	uiDefButS(block, NUM, 0, "Bits:",	xco+19, yco-=20, 95, 19, &G.scene->r.depth, 1.0, 32.0, 0, 0, "Displays bit depth of full screen display. Click to change.");

	uiDefBut(block, SEPR, 0, "",		xco, yco-=4, 114, 4, NULL, 0.0, 0.0, 0, 0, "");

	/* stereo settings */
	/* can't use any definition from the game engine here so hardcode it. Change it here when it changes there!
	 * RAS_IRasterizer has definitions:
	 * RAS_STEREO_NOSTEREO     1
	 * RAS_STEREO_QUADBUFFERED 2
	 * RAS_STEREO_ABOVEBELOW   3
	 * RAS_STEREO_INTERLACED   4   future
	 */
	uiDefBut(block, LABEL, 0, "Stereo options", xco, yco-=20, 114, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, ROW, 0, "no stereo", xco+19, yco-=20, 95, 19, &(G.scene->r.stereomode), 6.0, 1.0, 0, 0, "Disables stereo");
	uiDefButS(block, ROW, 0, "h/w pageflip", xco+19, yco-=20, 95, 19, &(G.scene->r.stereomode), 6.0, 2.0, 0, 0, "Enables hardware pageflip stereo method");
	uiDefButS(block, ROW, 0, "syncdoubling", xco+19, yco-=20, 95, 19, &(G.scene->r.stereomode), 6.0, 3.0, 0, 0, "Enables syncdoubling stereo method");
#if 0
	// future
	uiDefButS(block, ROW, 0, "syncdoubling", xco+19, yco, 95, 19, &(G.scene->r.stereomode), 5.0, 4.0, 0, 0, "Enables interlaced stereo method");
#endif

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
		
	return block;
}


static void do_info_file_importmenu(void *arg, int event)
{
	ScrArea *sa;

	if(curarea->spacetype==SPACE_INFO) {
		sa= closest_bigger_area();
		areawinset(sa->win);
	}

	/* these are no defines, easier this way, the codes are in the function below */
	switch(event) {
	              	
	case 0:
		break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_file_importmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "importmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_file_importmenu, NULL);
	//uiBlockSetXOfs(block, -50);  // offset to parent button
	uiBlockSetCol(block, MENUCOL);
	
	uiDefBut(block, BUTM, 1, "Python scripts go here somehow!",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_info_file_exportmenu(void *arg, int event)
{
	ScrArea *sa;

	if(curarea->spacetype==SPACE_INFO) {
		sa= closest_bigger_area();
		areawinset(sa->win);
	}

	/* these are no defines, easier this way, the codes are in the function below */
	switch(event) {
	              	
	case 0:
		write_vrml_fs();
		break;
	case 1:
		write_dxf_fs();
		break;
	case 2:
		write_videoscape_fs();
		break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_file_exportmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20;

	block= uiNewBlock(&curarea->uiblocks, "exportmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_file_exportmenu, NULL);
	//uiBlockSetXOfs(block, -50);  // offset to parent button
	uiBlockSetCol(block, MENUCOL);
	
	uiDefBut(block, BUTM, 1, "VRML 1.0...|Ctrl F2",		0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "DXF...|Shift F2",		0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, BUTM, 1, "Videoscape...|Alt W",		0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}


static void do_info_filemenu(void *arg, int event)
{
	ScrArea *sa;
	char dir[FILE_MAXDIR];
	
	if(curarea->spacetype==SPACE_INFO) {
		sa= closest_bigger_area();
		areawinset(sa->win);
	}

	/* these are no defines, easier this way, the codes are in the function below */
	switch(event) {
	case 0:
		if (okee("Erase All")) {
			if (!BIF_read_homefile())
				error("No file ~/.B.blend");
		}
		break;
	case 1: /* open */
		activate_fileselect(FILE_BLENDER, "Open", G.sce, BIF_read_file);
		break;
	case 2: /* reopen last */
		{
			char *s= MEM_mallocN(strlen(G.sce) + 11 + 1, "okee_reload");
			strcpy(s, "Open file: ");
			strcat(s, G.sce);
			if (okee(s)) BIF_read_file(G.sce);
			MEM_freeN(s);
		}
		break;
	case 3: /* append */
		activate_fileselect(FILE_LOADLIB, "Load Library", G.lib, 0);
		break;
	case 4: /* save */
		strcpy(dir, G.sce);
		untitled(dir);
		activate_fileselect(FILE_BLENDER, "Save As", dir, BIF_write_file);
		break;
	case 5:
		strcpy(dir, G.sce);
		if (untitled(dir)) {
			activate_fileselect(FILE_BLENDER, "Save As", dir, BIF_write_file);
		} else {
			BIF_write_file(dir);
			free_filesel_spec(dir);
		}
		break;
	case 6: /* save image */
		mainqenter(F3KEY, 1);
		break;
/*
	case 20:
		strcpy(dir, G.sce);
		activate_fileselect(FILE_SPECIAL, "INSTALL LICENSE KEY", dir, loadKeyboard);
		break;
	case 21:
		SHOW_LICENSE_KEY();
		break;
*/
	case 22: /* save runtime */
		activate_fileselect(FILE_SPECIAL, "Save Runtime", "", write_runtime_check);
		break;
	case 23: /* save dynamic runtime */
		activate_fileselect(FILE_SPECIAL, "Save Dynamic Runtime", "", write_runtime_check_dynamic);
		break;
	case 10: /* pack data */
		check_packAll();
		break;
	case 11: /* unpack to current dir */
		unpackAll(PF_WRITE_LOCAL);
		G.fileflags &= ~G_AUTOPACK;
		break;
	case 12: /* unpack data */
		if (buttons_do_unpack() != RET_CANCEL) {
			/* Clear autopack bit only if user selected one of the unpack options */
			G.fileflags &= ~G_AUTOPACK;
		}
		break;
	case 13:
		exit_usiblender();
		break;
	case 31: /* save default settings */
		BIF_write_homefile();
		break;
	}
	allqueue(REDRAWINFO, 0);
}
static uiBlock *info_filemenu(void *arg_unused)
{
	uiBlock *block;
	short yco=0;
	short menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "info_filemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_filemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "New|Ctrl X",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Open...|F1",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reopen Last|Ctrl O",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save As...|F2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");

	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Image...|F3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Runtime...",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 22, "");
#ifdef _WIN32
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Dynamic Runtime...",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 23, "");
#endif
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Save Default Settings|Ctrl U",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 31, "");

	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Append...|Shift F1",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBlockBut(block, info_file_importmenu, NULL, ICON_RIGHTARROW_THIN, "Import", 0, yco-=20, menuwidth, 19, "");
	uiDefIconTextBlockBut(block, info_file_exportmenu, NULL, ICON_RIGHTARROW_THIN, "Export", 0, yco-=20, menuwidth, 19, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Pack Data",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 10, "");
//	uiDefBut(block, BUTM, 1, "Unpack Data to current dir",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 11, "Removes all packed files from the project and saves them to the current directory");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Unpack Data...",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 12, "");

	uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Quit Blender| Q",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");

	uiBlockSetDirection(block, UI_DOWN);
	uiTextBoundsBlock(block, 80);

	return block;
}




/**************************** ADD ******************************/

static void do_info_add_meshmenu(void *arg, int event)
{

	switch(event) {		
		case 0:
			/* Plane */
			add_primitiveMesh(0);
			break;
		case 1:
			/* Cube */
			add_primitiveMesh(1);
			break;
		case 2:
			/* Circle */
			add_primitiveMesh(4);
			break;
		case 3:
			/* UVsphere */
			add_primitiveMesh(11);
			break;
		case 4:
			/* IcoSphere */
			add_primitiveMesh(12);
			break;
		case 5:
			/* Cylinder */
			add_primitiveMesh(5);
			break;
		case 6:
			/* Tube */
			add_primitiveMesh(6);
			break;
		case 7:
			/* Cone */
			add_primitiveMesh(7);
			break;
		case 8:
			/* Grid */
			add_primitiveMesh(10);
			break;
		default:
			break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_add_meshmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "add_meshmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_add_meshmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Plane|",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cube|",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Circle|",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "UVsphere",			0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "IcoSphere|",			0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cylinder|",			0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Tube|",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cone|",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, SEPR, 0, ICON_BLANK1, "",					0, yco-=6,  160, 6,  NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grid|",				0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 8, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
		
	return block;
}

static void do_info_add_curvemenu(void *arg, int event)
{

	switch(event) {		
		case 0:
			/* Bezier Curve */
			add_primitiveCurve(10);
			break;
		case 1:
			/* Bezier Circle */
			add_primitiveCurve(11);
			break;
		case 2:
			/* NURB Curve */
			add_primitiveCurve(40);
			break;
		case 3:
			/* NURB Circle */
			add_primitiveCurve(41);
			break;
		case 4:
			/* Path */
			add_primitiveCurve(46);
			break;
		default:
			break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_add_curvemenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "add_curvemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_add_curvemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bezier Curve|",	0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bezier Circle|",	0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Curve|",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Circle",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Path|",			0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
		
	return block;
}


static void do_info_add_surfacemenu(void *arg, int event)
{

	switch(event) {		
		case 0:
			/* Curve */
			add_primitiveNurb(0);
			break;
		case 1:
			/* Circle */
			add_primitiveNurb(1);
			break;
		case 2:
			/* Surface */
			add_primitiveNurb(2);
			break;
		case 3:
			/* Tube */
			add_primitiveNurb(3);
			break;
		case 4:
			/* Sphere */
			add_primitiveNurb(4);
			break;
		case 5:
			/* Donut */
			add_primitiveNurb(5);
			break;
		default:
			break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_add_surfacemenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "add_surfacemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_add_surfacemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Curve|",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Circle|",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Surface|",	0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Tube",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Sphere|",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "NURBS Donut|",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
		
	return block;
}

static void do_info_add_metamenu(void *arg, int event)
{

	switch(event) {		
		case 0:
			/* Ball */
			add_primitiveMball(1);
			break;
		case 1:
			/* Tube */
			add_primitiveMball(2);
			break;
		case 2:
			/* Plane */
			add_primitiveMball(3);
			break;
		case 3:
			/* Elipsoid */
			add_primitiveMball(4);
			break;
		case 4:
			/* Cube */
			add_primitiveMball(5);
			break;
		default:
			break;
	}
	allqueue(REDRAWINFO, 0);
}


static uiBlock *info_add_metamenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "add_metamenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_add_metamenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1,"Meta Ball|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Meta Tube|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Meta Plane|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Meta Ellipsoid|",	0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Meta Cube|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
		
	return block;
}


static void do_info_addmenu(void *arg, int event)
{

	switch(event) {		
		case 0:
			/* Mesh */
			break;
		case 1:
			/* Curve */
			break;
		case 2:
			/* Surface */
			break;
		case 3:
			/* Metaball */
			break;
		case 4:
			/* Text (argument is discarded) */
			add_primitiveFont(event);
			break;
		case 5:
			/* Empty */
			add_object_draw(OB_EMPTY);
			break;
		case 6:
			/* Camera */
			add_object_draw(OB_CAMERA);
			break;
		case 7:
			/* Lamp */
			add_object_draw(OB_LAMP);
			break;
		case 8:
			/* Armature */
			add_primitiveArmature(OB_ARMATURE);
			break;
		case 9:
			/* Lattice */
			add_object_draw(OB_LATTICE);
			break;
		default:
			break;
	}
	allqueue(REDRAWINFO, 0);
}


static uiBlock *info_addmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;

	block= uiNewBlock(&curarea->uiblocks, "addmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_addmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBlockBut(block, info_add_meshmenu, NULL, ICON_RIGHTARROW_THIN, "Mesh", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, info_add_curvemenu, NULL, ICON_RIGHTARROW_THIN, "Curve", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, info_add_surfacemenu, NULL, ICON_RIGHTARROW_THIN, "Surface", 0, yco-=20, 120, 19, "");
	uiDefIconTextBlockBut(block, info_add_metamenu, NULL, ICON_RIGHTARROW_THIN, "Meta", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Lattice|",			0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Text|",				0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Empty|",				0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
//	uiDefBut(block, BUTM, 1, "Armature|",			0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 8, "Adds an Armature");
//	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Camera|",				0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Lamp|",				0, yco-=20, 120, 19, NULL, 0.0, 0.0, 1, 7, "");

	uiBlockSetDirection(block, UI_DOWN);
	uiTextBoundsBlock(block, 80);
		
	return block;
}

/************************** GAME *****************************/

static void do_info_gamemenu(void *arg, int event)
{
	switch (event) {
	case G_FILE_ENABLE_ALL_FRAMES_BIT:
	case G_FILE_SHOW_FRAMERATE_BIT:
	case G_FILE_SHOW_DEBUG_PROPS_BIT:
	case G_FILE_AUTOPLAY_BIT:
		G.fileflags ^= (1 << event);
		break;
	default:
		; /* ignore the rest */
	}
}

static uiBlock *info_gamemenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;
	short menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "gamemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_gamemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, B_STARTGAME, ICON_BLANK1,	"Start Game|P",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 1, 0, "");


	if(G.fileflags & (1 << G_FILE_ENABLE_ALL_FRAMES_BIT)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Enable All Frames",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_ENABLE_ALL_FRAMES_BIT, "");
	} else {
	       	uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Enable All Frames",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_ENABLE_ALL_FRAMES_BIT, "");
	}

	if(G.fileflags & (1 << G_FILE_SHOW_FRAMERATE_BIT)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Show Framerate and Profile",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_SHOW_FRAMERATE_BIT, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Show Framerate and Profile",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_SHOW_FRAMERATE_BIT, "");
	}

	if(G.fileflags & (1 << G_FILE_SHOW_DEBUG_PROPS_BIT)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Show Debug Properties",		 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_SHOW_DEBUG_PROPS_BIT, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Show Debug Properties",		 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_SHOW_DEBUG_PROPS_BIT, "");
	}
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 1, 0, "");

	if(G.fileflags & (1 << G_FILE_AUTOPLAY_BIT)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Autostart",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_AUTOPLAY_BIT, "");
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Autostart",	 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, G_FILE_AUTOPLAY_BIT, "");
	}

	uiBlockSetDirection(block, UI_DOWN);
	uiTextBoundsBlock(block, 50);
	
	return block;
}
/************************** TIMELINE *****************************/

static void do_info_timelinemenu(void *arg, int event)
{
	/* needed to check for valid selected objects */
	Base *base=NULL;
	Object *ob=NULL;
	//char file[FILE_MAXDIR+FILE_MAXFILE];

	base= BASACT;
	if (base) ob= base->object;

	switch(event) {
	case 1:
		/* Show Keyframes */
		if (!ob) error("Select an object before showing its keyframes");
		else set_ob_ipoflags();
		break;
	case 2:
		/* Show and select Keyframes */
		if (!ob) error("Select an object before showing and selecting its keyframes");
		else select_select_keys();
  		break;
  	case 3:
		/* select next keyframe */
		if (!ob) error("Select an object before selecting its next keyframe");
		else nextkey_obipo(1);
  		break;
  	case 4:
		/* select previous keyframe */
		if (!ob) error("Select an object before selecting its previous keyframe");
		else nextkey_obipo(-1);
		break;
  	case 5:
		/* next keyframe */
		if (!ob) error("Select an object before going to its next keyframe");
		else movekey_obipo(1);
  		break;
  	case 6:
		/* previous keyframe */
		if (!ob) error("Select an object before going to its previous keyframe");
		else movekey_obipo(-1);
		break;
	case 7:
		/* next frame */
		CFRA++;
		update_for_newframe();
		break;
  	case 8:
		/* previous frame */
		CFRA--;
		if(CFRA<1) CFRA=1;
		update_for_newframe();
		break;
	case 9:
		/* forward 10 frames */
		CFRA+= 10;
		update_for_newframe();
		break;
	case 10:
		/* back 10 frames */
		CFRA-= 10;
		if(CFRA<1) CFRA=1;
		update_for_newframe();
		break;
	case 11:
		/* end frame */
		CFRA= EFRA;
		update_for_newframe();
		break;
	case 12:
		/* start frame */
		CFRA= SFRA;
		update_for_newframe();
		break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_timelinemenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;
	short menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "timelinemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_timelinemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Keyframes|K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show and Select Keyframes|Shift K",0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select Next Keyframe|PageUp",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select Previous Keyframe|PageDown",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Next Keyframe|Ctrl PageUp",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Previous Keyframe|Ctrl PageDown",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Next Frame|RightArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Previous Frame|LeftArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Forward 10 Frames|UpArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Back 10 Frames|DownArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "End Frame|Shift RightArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Start Frame|Shift LeftArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");

	uiBlockSetDirection(block, UI_DOWN);
	uiTextBoundsBlock(block, 80);

	return block;
}

/************************** RENDER *****************************/

/* copied from buttons.c. .. probably not such a good idea!? */
static void run_playanim(char *file) {
	extern char bprogname[];	/* usiblender.c */
	char str[FILE_MAXDIR+FILE_MAXFILE];
	int pos[2], size[2];

	calc_renderwin_rectangle(R.winpos, pos, size);

	sprintf(str, "%s -a -p %d %d \"%s\"", bprogname, pos[0], pos[1], file);
	system(str);
}

static void do_info_rendermenu(void *arg, int event)
{
	char file[FILE_MAXDIR+FILE_MAXFILE];

	extern void makeavistring(char *string);
	extern void makeqtstring (char *string);

	switch(event) {
		
	case 0:
		BIF_do_render(0);
		break;
	case 1:
		BIF_do_render(1);
		break;
	case 2:
		if(select_area(SPACE_VIEW3D)) {
			BIF_do_ogl_render(curarea->spacedata.first, 0 );
		}
  		break;
  	case 3:
		if(select_area(SPACE_VIEW3D)) {
			BIF_do_ogl_render(curarea->spacedata.first, 1 );
		}
  		break;
  	case 4:
		BIF_toggle_render_display();
		break;
	case 5:
#ifdef WITH_QUICKTIME
		if(G.scene->r.imtype == R_QUICKTIME)
			makeqtstring(file);
		else
#endif
			makeavistring(file);
		if(BLI_exist(file)) {
			run_playanim(file);
		}
		else {
			makepicstring(file, G.scene->r.sfra);
			if(BLI_exist(file)) {
				run_playanim(file);
			}
			else error("Can't find image: %s", file);
		}
		break;
	case 6:
		/* dodgy hack turning on SHIFT key to do a proper render border select
		strangely, set_render_border(); won't work :( 
		
		This code copied from toolbox.c */

		if(select_area(SPACE_VIEW3D)) {
			mainqenter(LEFTSHIFTKEY, 1);
			mainqenter(BKEY, 1);
			mainqenter(BKEY, 0);
			mainqenter(EXECUTE, 1);
			mainqenter(LEFTSHIFTKEY, 0);
		}

		break;

  	case 7:
		extern_set_butspace(F10KEY);
		break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_rendermenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;
	short menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "rendermenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_rendermenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Render Current Frame|F12",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Render Animation",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "OpenGL Preview Current Frame",0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "OpenGL Preview Animation",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Render Buffer|F11",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Rendered Animation",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Set Render Border|Shift B",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Render Settings|F10",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");

	uiBlockSetDirection(block, UI_DOWN);
	uiTextBoundsBlock(block, 80);

	return block;
}

/************************** HELP *****************************/

static void do_info_help_websitesmenu(void *arg, int event)
{
	/* these are no defines, easier this way, the codes are in the function below */
	switch(event) {
	case 0: /*  */

		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}


static uiBlock *info_help_websitesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "info_help_websitesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_help_websitesmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Blender Website *",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Blender E-shop *",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Development Community *",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "User Community *",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "...? *",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
		
	return block;
}


static void do_info_helpmenu(void *arg, int event)
{
	switch(event) {
		
	case 0:
		break;
	case 1:
		/* dodgy hack turning on CTRL ALT SHIFT key to do a benchmark 
		 *	rather than copying lines and lines of code from toets.c :( 
		 */

		if(select_area(SPACE_VIEW3D)) {
			mainqenter(LEFTSHIFTKEY, 1);
			mainqenter(LEFTCTRLKEY, 1);
			mainqenter(LEFTALTKEY, 1);
			mainqenter(TKEY, 1);
			mainqenter(TKEY, 0);
			mainqenter(EXECUTE, 1);
			mainqenter(LEFTSHIFTKEY, 0);
			mainqenter(LEFTCTRLKEY, 0);
			mainqenter(LEFTALTKEY, 0);
		}
		break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_helpmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0;
	short menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "info_helpmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_helpmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "-- Placeholders only --",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Tutorials *",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "User Manual *",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Python Scripting Reference *",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, info_help_websitesmenu, NULL, ICON_RIGHTARROW_THIN, "Websites", 0, yco-=20, 120, 19, "");

	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Benchmark",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, B_SHOWSPLASH, ICON_BLANK1, "About Blender...",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Release Notes *",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_DOWN);
	uiTextBoundsBlock(block, 80);

	return block;
}


/************************** END MAIN MENU *****************************/


static void info_text(int x, int y)
{
	Object *ob;
	extern float hashvectf[];
	extern int mem_in_use;
	unsigned int swatch_color;
	float fac1, fac2, fac3;
	char infostr[300];
	char *headerstr;
	int hsize;


	if(G.obedit) {
		sprintf(infostr,"Ve:%d-%d Fa:%d-%d  Mem:%.2fM  ",
		G.totvertsel, G.totvert, G.totfacesel, G.totface,
		(mem_in_use>>10)/1024.0);
	}
	else {
		sprintf(infostr,"Ve:%d Fa:%d  Ob:%d-%d La:%d  Mem:%.2fM   ",
			G.totvert, G.totface, G.totobj, G.totobjsel, G.totlamp,  (mem_in_use>>10)/1024.0);
	}
	ob= OBACT;
	if(ob) {
		strcat(infostr, ob->id.name+2);
	}

	if  (g_progress_bar) {
		hsize = 4 + (138.0 * g_done);
		fac1 = 0.5 * g_done; // do some rainbow colours on progress
		fac2 = 1.0;
		fac3 = 0.9;
	} else {
		hsize = 142;
		/* promise! Never change these lines again! (zr & ton did!) */
		fac1= fabs(hashvectf[ 2*G.version+4]);
		fac2= 0.5+0.1*hashvectf[ G.version+3];
		fac3= 0.7;
	}

	if (g_progress_bar && g_progress_info) {
		headerstr= g_progress_info;
	} else {
		headerstr= versionstr; 
	}
	
	swatch_color= hsv_to_cpack(fac1, fac2, fac3);

	cpack( swatch_color );
	glRecti(x-24,  y-4,  x-24+hsize,  y+13);

	glColor3ub(0, 0, 0);

	glRasterPos2i(x, y);

	BIF_DrawString(G.font, headerstr, (U.transopts & TR_MENUS), 0);
		
	glRasterPos2i(x+120,  y);

	BIF_DrawString(G.font, infostr, (U.transopts & TR_MENUS), 0);
}

static int GetButStringLength(char *str) {
	int rt;

	rt= BIF_GetStringWidth(G.font, str, (U.transopts & TR_BUTTONS));

	return rt + 15;
}



void info_buttons(void)
{
	uiBlock *block;
	short xco= 32;
	char naam[20];
	int xmax;

	sprintf(naam, "header %d", curarea->headwin);	
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSN, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREY);
	
	if(U.uiflag & FLIPINFOMENU) {
		uiDefIconButS(block, TOG|BIT|6, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(U.uiflag), 0, 0, 0, 0, "Enables display of pulldown menus");/* dir   */
	} else {
		uiDefIconButS(block, TOG|BIT|6, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(U.uiflag), 0, 0, 0, 0, "Hides pulldown menus");/* dir   */
	}
	xco+=XIC;

	if(U.uiflag & FLIPINFOMENU) {
	} else {
		uiBlockSetEmboss(block, UI_EMBOSSP);
		if(area_is_active_area(curarea)) uiBlockSetCol(block, HEADERCOLSEL);	
		else uiBlockSetCol(block, HEADERCOL);	

		xmax= GetButStringLength("File");
		uiDefBlockBut(block, info_filemenu, NULL, "File",	xco, 0, xmax, 21, "");
		xco+= xmax;

		xmax= GetButStringLength("Add");
		uiDefBlockBut(block, info_addmenu, NULL, "Add",	xco, 0, xmax, 21, "");
		xco+= xmax;

		xmax= GetButStringLength("Timeline");
		uiDefBlockBut(block, info_timelinemenu, NULL, "Timeline",	xco, 0, xmax, 21, "");
		xco+= xmax;

		xmax= GetButStringLength("Game");
		uiDefBlockBut(block, info_gamemenu, NULL, "Game",	xco, 0, xmax, 21, "");
		xco+= xmax;

		xmax= GetButStringLength("Render");
		uiDefBlockBut(block, info_rendermenu, NULL, "Render",	xco, 0, xmax, 21, "");
		xco+= xmax;

		xmax= GetButStringLength("Help");
		uiDefBlockBut(block, info_helpmenu, NULL, "Help",	xco, 0, xmax, 21, "");
		xco+= xmax;

	}

	/* pack icon indicates a packed file */
	uiBlockSetCol(block, BUTGREY);
	
	if (G.fileflags & G_AUTOPACK) {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		uiDefIconBut(block, LABEL, 0, ICON_PACKAGE, xco, 0, XIC, YIC, &G.fileflags, 0.0, 0.0, 0, 0, "Indicates this is a Packed file. See File menu.");
		xco += XIC;
		uiBlockSetEmboss(block, UI_EMBOSSX);
	}

	uiBlockSetEmboss(block, UI_EMBOSSX);
	
	if (curarea->full == 0) {
		curarea->butspacetype= SPACE_INFO;
		uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
		
		/* STD SCREEN BUTTONS */
//		xco+= XIC;
		xco+= 4;
		xco= std_libbuttons(block, xco, 0, NULL, B_INFOSCR, (ID *)G.curscreen, 0, &G.curscreen->screennr, 1, 1, B_INFODELSCR, 0, 0);
	
		/* STD SCENE BUTTONS */
		xco+= 5;
		xco= std_libbuttons(block, xco, 0, NULL, B_INFOSCE, (ID *)G.scene, 0, &G.curscreen->scenenr, 1, 1, B_INFODELSCE, 0, 0);
	}
	else xco= 430;
	
	info_text(xco+24, 6);
	
	uiBlockSetEmboss(block, UI_EMBOSSN);
	uiDefIconBut(block, BUT, B_SHOWSPLASH, ICON_BLENDER, xco+1, 0,XIC,YIC, 0, 0, 0, 0, 0, "Click to display Splash Screen");
	uiBlockSetEmboss(block, UI_EMBOSSX);

/*
	uiBlockSetEmboss(block, UI_EMBOSSN);
	uiDefIconBut(block, LABEL, 0, ICON_PUBLISHER, xco+125, 0,XIC,YIC, 0, 0, 0, 0, 0, "");
	uiBlockSetEmboss(block, UI_EMBOSSX);
*/
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;
	
	if(curarea->headbutlen + 4*XIC < curarea->winx) {
		uiDefIconBut(block, BUT, B_FILEMENU, ICON_HELP,
			(short)(curarea->winx-XIC-2), 0,XIC,YIC,
			0, 0, 0, 0, 0, "Displays Toolbox menu (SPACE)");

#ifdef _WIN32	// FULLSCREEN
	if(U.uiflag & FLIPFULLSCREEN) {
		uiDefIconBut(block, BUT, B_FLIPFULLSCREEN, ICON_WINDOW_WINDOW,
				(short)(curarea->winx-(XIC*2)-2), 0,XIC,YIC,
				0, 0, 0, 0, 0, "Toggles Blender to fullscreen mode");/* dir   */
	} else {
		uiDefIconBut(block, BUT, B_FLIPFULLSCREEN, ICON_WINDOW_FULLSCREEN,
				(short)(curarea->winx-(XIC*2)-2), 0,XIC,YIC,
				0, 0, 0, 0, 0, "Toggles Blender to fullscreen mode");/* dir   */
	}
#endif
	
	}
	
	uiDrawBlock(block);
}

/* ********************** END INFO ****************************** */
/* ********************** SEQUENCE ****************************** */

void do_seq_buttons(short event)
{
	Editing *ed;
	
	ed= G.scene->ed;
	if(ed==0) return;
	
	switch(event) {
	case B_SEQHOME:
		G.v2d->cur= G.v2d->tot;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	case B_SEQCLEAR:
		free_imbuf_seq();
		allqueue(REDRAWSEQ, 1);
		break;
	}
	
}

void seq_buttons()
{
	SpaceSeq *sseq;
	short xco;
	char naam[20];
	uiBlock *block;
	
	sseq= curarea->spacedata.first;
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTPURPLE);

	curarea->butspacetype= SPACE_SEQ;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_SEQHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");	
	xco+= XIC;
	
	/* IMAGE */
	uiDefIconButS(block, TOG, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &sseq->mainb, 0, 0, 0, 0, "Toggles image display");

	/* ZOOM and BORDER */
	xco+= XIC;
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view in and out (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to fit area");

	/* CLEAR MEM */
	xco+= XIC;
	uiDefBut(block, BUT, B_SEQCLEAR, "Clear",	xco+=XIC,0,2*XIC,YIC, 0, 0, 0, 0, 0, "Forces a clear of all buffered images in memory");
	
	uiDrawBlock(block);
}

/* ********************** END SEQ ****************************** */
/* ********************** VIEW3D ****************************** */
void do_layer_buttons(short event)
{
	static int oldlay= 1;
	
	if(G.vd==0) return;
	if(G.vd->localview) return;
	
	if(event==-1 && (G.qual & LR_CTRLKEY)) {
		G.vd->scenelock= !G.vd->scenelock;
		do_view3d_buttons(B_SCENELOCK);
	} else if (event==-1) {
		if(G.vd->lay== (2<<20)-1) {
			if(G.qual & LR_SHIFTKEY) G.vd->lay= oldlay;
		}
		else {
			oldlay= G.vd->lay;
			G.vd->lay= (2<<20)-1;
		}
		
		if(G.vd->scenelock) handle_view3d_lock();
		scrarea_queue_winredraw(curarea);
	}
	else {
		if(G.qual & LR_ALTKEY) {
			if(event<11) event+= 10;
		}
		if(G.qual & LR_SHIFTKEY) {
			if(G.vd->lay & (1<<event)) G.vd->lay -= (1<<event);
			else  G.vd->lay += (1<<event);
		}
		do_view3d_buttons(event+B_LAY);
	}
	/* redraw seems double: but the queue nicely handles that */
	scrarea_queue_headredraw(curarea);
	
	if(curarea->spacetype==SPACE_OOPS) allqueue(REDRAWVIEW3D, 1);	/* 1==also do headwin */
	
}

static void do_view3d_view_cameracontrolsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* Orbit Left */
		persptoetsen(PAD4);
		break;
	case 1: /* Orbit Right */
		persptoetsen(PAD6);
		break;
	case 2: /* Orbit Up */
		persptoetsen(PAD8);
		break;
	case 3: /* Orbit Down */
		persptoetsen(PAD2);
		break;
	case 4: /* Zoom In */
		persptoetsen(PADPLUSKEY);
		break;
	case 5: /* Zoom Out */
		persptoetsen(PADMINUS);
		break;
	case 6: /* Reset Zoom */
		persptoetsen(PADENTER);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_view_cameracontrolsmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_view_cameracontrolsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_view_cameracontrolsmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Left|NumPad 4",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Right|NumPad 6",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Up|NumPad 8",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Orbit Down|NumPad 2",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 140, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Reset Zoom|NumPad Enter",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_viewmenu(void *arg, int event)
{
	extern int play_anim(int mode);

	float *curs;
	
	switch(event) {
	case 0: /* User */
		G.vd->viewbut = 0;
		G.vd->persp = 1;
		break;
	case 1: /* Camera */
		persptoetsen(PAD0);
		break;
	case 2: /* Top */
		persptoetsen(PAD7);
		break;
	case 3: /* Front */
		persptoetsen(PAD1);
		break;
	case 4: /* Side */
		persptoetsen(PAD3);
		break;
	case 5: /* Perspective */
		G.vd->persp=1;
		break;
	case 6: /* Orthographic */
		G.vd->persp=0;
		break;
	case 7: /* Local View */
		G.vd->localview= 1;
		initlocalview();
		break;
	case 8: /* Global View */
		G.vd->localview= 0;
		endlocalview(curarea);
		break;
	case 9: /* Frame All (Home) */
		view3d_home(0);
		break;
	case 10: /* Center at Cursor */
		curs= give_cursor();
		G.vd->ofs[0]= -curs[0];
		G.vd->ofs[1]= -curs[1];
		G.vd->ofs[2]= -curs[2];
		scrarea_queue_winredraw(curarea);
		break;
	case 11: /* Center View to Selected */
		centreview();
		break;
	case 12: /* Align View to Selected */
		mainqenter(PADASTERKEY, 1);
		break;
	case 13: /* Play Back Animation */
		play_anim(0);
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_viewmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_viewmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	/*
	 * Reverse the menu order depending if the header is on top or bottom.
	 * Is more usable/logical this way by using motor memory to remember the
	 * positioning of menu items - remembering a distance that the mouse
	 * pointer has to travel, rather than a specific x,y co-ordinate down the list.
	 */
	if(curarea->headertype==HEADERTOP) { 
		
		if ((G.vd->viewbut == 0) && !(G.vd->persp == 2)) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "User",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "User",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
		if (G.vd->persp == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Camera|NumPad 0",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Camera|NumPad 0",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		if (G.vd->viewbut == 1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Top|NumPad 7",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Top|NumPad 7",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
		if (G.vd->viewbut == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Front|NumPad 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Front|NumPad 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
		if (G.vd->viewbut == 3) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Side|NumPad 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Side|NumPad 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(G.vd->persp==1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Perspective|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Perspective|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
		if(G.vd->persp==0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Orthographic|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Orthographic|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
		
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(G.vd->localview) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Local View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Local View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
		if(!G.vd->localview) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
		
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_view_cameracontrolsmenu, NULL, ICON_RIGHTARROW_THIN, "Viewport Navigation", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame All|Home",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame Cursor|C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame Selected|NumPad .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align View to Selected|NumPad *",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
		
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Animation|Alt A",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
		
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,0, "");
		else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	} else {
	
		if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,0, "");
		else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Back Animation|Alt A",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Align View to Selected|NumPad *",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame Selected|NumPad .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame Cursor|C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Frame All|Home",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
		uiDefIconTextBlockBut(block, view3d_view_cameracontrolsmenu, NULL, ICON_RIGHTARROW_THIN, "Viewport Navigation", 0, yco-=20, 120, 19, "");
	
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(!G.vd->localview) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Global View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
		if(G.vd->localview) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Local View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Local View|NumPad /",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(G.vd->persp==0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Orthographic|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Orthographic|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
		if(G.vd->persp==1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Perspective|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Perspective|NumPad 5",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if (G.vd->viewbut == 3) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Side|NumPad 3",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Side|NumPad 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
		if (G.vd->viewbut == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Front|NumPad 1",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Front|NumPad 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
		if (G.vd->viewbut == 1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Top|NumPad 7",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Top|NumPad 7",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
		if (G.vd->persp == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Camera|NumPad 0",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Camera|NumPad 0",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
		if ((G.vd->viewbut == 0) && !(G.vd->persp == 2)) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "User",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
		else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "User",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");

	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	
	return block;
}

static void do_view3d_select_objectmenu(void *arg, int event)
{
	extern void borderselect(void);
	extern void deselectall(void);
	
	switch(event) {
	
	case 0: /* border select */
		borderselect();
		break;
	case 1: /* Select/Deselect All */
		deselectall();
		break;
	case 2: /* Select Linked */
		selectlinks();
		break;
	case 3: /* Select Grouped */
		group_menu();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_objectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_objectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_objectmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
	
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked...|Shift L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grouped...|Shift G",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grouped...|Shift G",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linked...|Shift L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_meshmenu(void *arg, int event)
{
	extern void borderselect(void);

	switch(event) {
	
		case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_mesh();
			break;
		case 3: /* Inverse */
			selectswap_mesh();
			break;
		case 4: /* select linked vertices */
			G.qual |= LR_CTRLKEY;
			selectconnected_mesh();
			G.qual &= ~LR_CTRLKEY;
			break;
		case 5: /* select random */
			// selectrandom_mesh();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}


static uiBlock *view3d_select_meshmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_meshmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_meshmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Edge Loop|Shift R",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Random Vertices...",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connected Vertices|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connected Vertices|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Random Vertices...",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Edge Loop|Shift R",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_curvemenu(void *arg, int event)
{
	extern void borderselect(void);

	switch(event) {
		case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_nurb();
			break;
		case 3: /* Inverse */
			selectswapNurb();
			break;
		//case 4: /* select connected control points */
			//G.qual |= LR_CTRLKEY;
			//selectconnected_nurb();
			//G.qual &= ~LR_CTRLKEY;
			//break;
		case 5: /* select row (nurb) */
			selectrow_nurb();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}


static uiBlock *view3d_select_curvemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_curvemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_curvemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
		if (OBACT->type == OB_SURF) {
			uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Control Point Row|Shift R",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		}
		/* commented out because it seems to only like the LKEY method - based on mouse pointer position :( */
		//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connected Control Points|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
	} else {
		
		/* commented out because it seems to only like the LKEY method - based on mouse pointer position :( */
		//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Connected Control Points|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		if (OBACT->type == OB_SURF) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Control Point Row|Shift R",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
			
			uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		}
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Inverse",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_metaballmenu(void *arg, int event)
{
	extern void borderselect(void);

	switch(event) {
		case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_mball();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}


static uiBlock *view3d_select_metaballmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_metaballmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_metaballmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_latticemenu(void *arg, int event)
{
	extern void borderselect(void);
	
	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_Latt();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_latticemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_latticemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_latticemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_armaturemenu(void *arg, int event)
{
	extern void borderselect(void);

	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_armature();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_armaturemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_pose_armaturemenu(void *arg, int event)
{
	extern void borderselect(void);
	
	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_posearmature(1);
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_pose_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_pose_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_pose_armaturemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_select_faceselmenu(void *arg, int event)
{
	extern void borderselect(void);
	
	switch(event) {
			case 0: /* border select */
			borderselect();
			break;
		case 2: /* Select/Deselect all */
			deselectall_tface();
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_select_faceselmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_select_faceselmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_select_faceselmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	
	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_object_transformmenu(void *arg, int event)
{
	switch(event) {
	case 0: /*  clear origin */
		clear_object('o');
		break;
	case 1: /* clear size */
		clear_object('s');
		break;
	case 2: /* clear rotation */
		clear_object('r');
		break;
	case 3: /* clear location */
		clear_object('g');
		break;
	case 4: /* apply deformation */
		make_duplilist_real();
		break;
	case 5: /* apply size/rotation */
		apply_object();
		break;	
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_transformmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_transformmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_transformmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Size/Rotation|Ctrl A",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Apply Deformation|Ctrl Shift A",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Location|Alt G",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Rotation|Alt R",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Size|Alt S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Origin|Alt O",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_parentmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* clear parent */
		clear_parent();
		break;
	case 1: /* make parent */
		make_parent();
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_parentmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_parentmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_parentmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Parent...|Ctrl P",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Parent...|Alt P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_object_trackmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* clear track */
		clear_track();
		break;
	case 1: /* make track */
		make_track();
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_object_trackmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_object_trackmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_object_trackmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Track...|Ctrl T",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Track...|Alt T",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_objectmenu(void *arg, int event)
{
	/* needed to check for valid selected objects */
	Base *base=NULL;
	Object *ob=NULL;

	base= BASACT;
	if (base) ob= base->object;
	
	switch(event) {
	 
	case 0: /* transform  properties*/
			blenderqread(NKEY, 1);
		break;
	case 1: /* delete */
		delete_context_selected();
		break;
	case 2: /* duplicate */
		duplicate_context_selected();
		break;
	case 3: /* duplicate linked */
		G.qual |= LR_ALTKEY;
		adduplicate(0);
		G.qual &= ~LR_ALTKEY;
		break;
	case 4: /* make links */
		linkmenu();
		break;
	case 5: /* make single user */
		single_user();
		break;
	case 6: /* copy properties */
		copymenu();
		break;
	case 7: /* boolean operation */
		special_editmenu();
		break;
	case 8: /* join objects */
		if( (ob= OBACT) ) {
			if(ob->type == OB_MESH) join_mesh();
			else if(ob->type == OB_CURVE) join_curve(OB_CURVE);
			else if(ob->type == OB_SURF) join_curve(OB_SURF);
			else if(ob->type == OB_ARMATURE) join_armature();
		}
		break;
	case 9: /* convert object type */
		convertmenu();
		break;
	case 10: /* move to layer */
		movetolayer();
		break;
	case 11: /* insert keyframe */
		common_insertkey();
		break;
	case 12: /* snap */
		snapmenu();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_objectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_objectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_objectmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Move", 0, yco-=20, 120, 19, "");
		//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Rotate", 0, yco-=20, 120, 19, "");
		//uiDefIconTextBlockBut(block, 0, NULL, ICON_RIGHTARROW_THIN, "Scale", 0, yco-=20, 120, 19, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBlockBut(block, view3d_edit_object_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");	
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Linked|Alt D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Links...|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Single User...|U",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Properties...|Ctrl C",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_object_parentmenu, NULL, ICON_RIGHTARROW_THIN, "Parent", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_object_trackmenu, NULL, ICON_RIGHTARROW_THIN, "Track", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Boolean Operation...|W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		}
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Join Objects|Ctrl J",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Convert Object Type...|Alt C",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
				
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Convert Object Type...|Alt C",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Join Objects|Ctrl J",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Boolean Operation...|W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		}
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_object_trackmenu, NULL, ICON_RIGHTARROW_THIN, "Track", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_object_parentmenu, NULL, ICON_RIGHTARROW_THIN, "Parent", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Properties...|Ctrl C",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Single User...|U",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Links...|Ctrl L",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Linked|Alt D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");	
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		uiDefIconTextBlockBut(block, view3d_edit_object_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}


static void do_view3d_edit_propfalloffmenu(void *arg, int event)
{
	extern int prop_mode;
	
	switch(event) {
	case 0: /* proportional edit - sharp*/
		prop_mode = 0;
		break;
	case 1: /* proportional edit - smooth*/
		prop_mode = 1;
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_propfalloffmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;
	extern int prop_mode;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_propfalloffmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_propfalloffmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if (prop_mode==0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Sharp|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Sharp|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	if (prop_mode==1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Smooth|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Smooth|Shift O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_mesh_verticesmenu(void *arg, int event)
{
	extern float doublimit;
	
	switch(event) {
	   
	case 0: /* make vertex parent */
		make_parent();
		break;
	case 1: /* remove doubles */
		notice("Removed: %d", removedoublesflag(1, doublimit));
		break;
	case 2: /* smooth */
		vertexsmooth();
		break;
	case 3: /* separate */
		separate_mesh();
		break;
	case 4: /*split */
		split_mesh();
		break;
	case 5: /*merge */
		mergemenu();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_verticesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_verticesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_verticesmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Merge...|Alt M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Split|Y",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Separate|P",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Smooth",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Remove Doubles",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Vertex Parent|Ctrl P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_mesh_edgesmenu(void *arg, int event)
{
	extern short editbutflag;
	float fac;
	short randfac;

	switch(event) {
	   
	case 0: /* subdivide smooth */
		subdivideflag(1, 0.0, editbutflag | B_SMOOTH);
		break;
	case 1: /*subdivide fractal */
		randfac= 10;
		if(button(&randfac, 1, 100, "Rand fac:")==0) return;
		fac= -( (float)randfac )/100;
		subdivideflag(1, fac, editbutflag);
		break;
	case 2: /* subdivide */
		subdivideflag(1, 0.0, editbutflag);
		break;
	case 3: /* knife subdivide */
		// KnifeSubdivide();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_edgesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_edgesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_edgesmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Knife Subdivide|K",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide Fractal",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide Smooth",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_mesh_facesmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* Fill Faces */
		fill_mesh();
		break;
	case 1: /* Beauty Fill Faces */
		beauty_fill();
		break;
	case 2: /* Quads to Tris */
		convert_to_triface(0);
		allqueue(REDRAWVIEW3D, 0);
		countall();
		makeDispList(G.obedit);
		break;
	case 3: /* Tris to Quads */
		join_triangles();
		break;
	case 4: /* Flip triangle edges */
		edge_flip();
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_facesmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_facesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_facesmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Fill|Shift F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Beauty Fill|Alt F",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Convert Quads to Triangles|Ctrl T",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Convert Triangles to Quads|Alt J",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Flip Triangle Edges|Ctrl F",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_mesh_normalsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* flip */
		flip_editnormals();
		break;
	case 1: /* recalculate inside */
		righthandfaces(2);
		break;
	case 2: /* recalculate outside */
		righthandfaces(1);
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_mesh_normalsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_mesh_normalsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_mesh_normalsmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Recalculate Outside|Ctrl N",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Recalculate Inside|Ctrl Shift N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Flip",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_meshmenu(void *arg, int event)
{
	switch(event) {
	              	
	case 0: /* Undo Editing */
		remake_editMesh();
		break;
	case 1: /* transform properties */
		blenderqread(NKEY, 1);
		break;
	case 2: /* Extrude */
		extrude_mesh();
		break;
	case 3: /* duplicate */
		duplicate_context_selected();
		break;
	case 4: /* Make Edge/Face */
		addedgevlak_mesh();
		break;
	case 5: /* delete */
		delete_context_selected();
		break;
	case 6: /* Shrink/Fatten Along Normals */
		transform('N');
		break;
	case 7: /* Shear */
		transform('S');
		break;
	case 8: /* Warp */
		transform('w');
		break;
	case 9: /* proportional edit (toggle) */
		if(G.f & G_PROPORTIONAL) G.f &= ~G_PROPORTIONAL;
		else G.f |= G_PROPORTIONAL;
		break;
	case 10: /* show hidden vertices */
		reveal_mesh();
		break;
	case 11: /* hide selected vertices */
		hide_mesh(0);
		break;
	case 12: /* hide deselected vertices */
		hide_mesh(1);
		break;
	case 13: /* insert keyframe */
		common_insertkey();
		break;
	case 14: /* snap */
		snapmenu();
		break;
	case 15: /* move to layer */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_meshmenu(void *arg_unused)
{

	uiBlock *block;
	short yco= 0, menuwidth=120;
		
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_meshmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_meshmenu, NULL);
		uiBlockSetCol(block, MENUCOL);
		
	if(curarea->headertype==HEADERTOP) { 
		
		/*
		uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Move", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Rotate", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Scale", 0, yco-=20, 120, 19, "");
		*/
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Edge/Face|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_mesh_verticesmenu, NULL, ICON_RIGHTARROW_THIN, "Vertices", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_edgesmenu, NULL, ICON_RIGHTARROW_THIN, "Edges", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Faces", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_normalsmenu, NULL, ICON_RIGHTARROW_THIN, "Normals", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink/Fatten Along Normals|Alt S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(G.f & G_PROPORTIONAL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		}
		uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Vertices",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Vertices|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Vertices|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Vertices|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Vertices|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Vertices",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
				
		uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
		if(G.f & G_PROPORTIONAL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		}

		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shrink/Fatten Along Normals|Alt S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_mesh_normalsmenu, NULL, ICON_RIGHTARROW_THIN, "Normals", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_facesmenu, NULL, ICON_RIGHTARROW_THIN, "Faces", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_edgesmenu, NULL, ICON_RIGHTARROW_THIN, "Edges", 0, yco-=20, 120, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_mesh_verticesmenu, NULL, ICON_RIGHTARROW_THIN, "Vertices", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Edge/Face|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_curve_controlpointsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* tilt */
		transform('t');
		break;
	case 1: /* clear tilt */
		clear_tilt();
		break;
	case 2: /* Free */
		sethandlesNurb(3);
		makeDispList(G.obedit);
		break;
	case 3: /* vector */
		sethandlesNurb(2);
		makeDispList(G.obedit);
		break;
	case 4: /* smooth */
		sethandlesNurb(1);
		makeDispList(G.obedit);
		break;
	case 5: /* make vertex parent */
		make_parent();
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_curve_controlpointsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_curve_controlpointsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_curve_controlpointsmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if (OBACT->type == OB_CURVE) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Tilt|T",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Tilt|Alt T",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Toggle Free/Aligned|H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Vector|V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Smooth|Shift H",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	}
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Vertex Parent|Ctrl P",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_curve_segmentsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* subdivide */
		subdivideNurb();
		break;
	case 1: /* switch direction */
		switchdirectionNurb2();
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_curve_segmentsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_curve_segmentsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_curve_segmentsmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Subdivide",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Switch Direction",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_edit_curvemenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* Undo Editing */
		remake_editNurb();
		break;
	case 1: /* transformation properties */
		blenderqread(NKEY, 1);
		break;
	case 2: /* insert keyframe */
		common_insertkey();
		break;
	case 4: /* extrude */
		if (OBACT->type == OB_CURVE) {
			addvert_Nurb('e');
		} else if (OBACT->type == OB_SURF) {
			extrude_nurb();
		}
		break;
	case 5: /* duplicate */
		duplicate_context_selected();
		break;
	case 6: /* make segment */
		addsegment_nurb();
		break;
	case 7: /* toggle cyclic */
		makecyclicNurb();
		makeDispList(G.obedit);
		break;
	case 8: /* delete */
		delete_context_selected();
		break;
	case 9: /* proportional edit (toggle) */
		if(G.f & G_PROPORTIONAL) G.f &= ~G_PROPORTIONAL;
		else G.f |= G_PROPORTIONAL;
		break;
	case 10: /* show hidden control points */
		revealNurb();
		break;
	case 11: /* hide selected control points */
		hideNurb(0);
		break;
	case 12: /* hide deselected control points */
		hideNurb(1);
		break;
	case 13: /* Shear */
		transform('S');
		break;
	case 14: /* Warp */
		transform('w');
		break;
	case 15: /* snap */
		snapmenu();
		break;
	case 16: /* move to layer  */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_curvemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_curvemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_curvemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
	
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Segment|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Toggle Cyclic|C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_curve_controlpointsmenu, NULL, ICON_RIGHTARROW_THIN, "Control Points", 0, yco-=20, menuwidth, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_curve_segmentsmenu, NULL, ICON_RIGHTARROW_THIN, "Segments", 0, yco-=20, menuwidth, 19, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(G.f & G_PROPORTIONAL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		}
		uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Control Points|Alt H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Control Points|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
		if (OBACT->type == OB_SURF) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Control Points|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if (OBACT->type == OB_SURF) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Control Points|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Control Points|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Control Points|Alt H",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
		if(G.f & G_PROPORTIONAL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
		}
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_curve_segmentsmenu, NULL, ICON_RIGHTARROW_THIN, "Segments", 0, yco-=20, menuwidth, 19, "");
		uiDefIconTextBlockBut(block, view3d_edit_curve_controlpointsmenu, NULL, ICON_RIGHTARROW_THIN, "Control Points", 0, yco-=20, menuwidth, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Toggle Cyclic|C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Make Segment|F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties...|N",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_metaballmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* duplicate */
		duplicate_context_selected();
		break;
	case 2: /* delete */
		delete_context_selected();
		break;
	case 3: /* Shear */
		transform('S');
		break;
	case 4: /* Warp */
		transform('w');
		break;
	case 5: /* move to layer */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_metaballmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
		
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_metaballmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_metaballmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
	
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
				
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete...|X",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_text_charsmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* copyright */
		do_textedit(0,0,169);
		break;
	case 1: /* registered trademark */
		do_textedit(0,0,174);
		break;
	case 2: /* degree sign */
		do_textedit(0,0,176);
		break;
	case 3: /* Multiplication Sign */
		do_textedit(0,0,215);
		break;
	case 4: /* Circle */
		do_textedit(0,0,138);
		break;
	case 5: /* superscript 1 */
		do_textedit(0,0,185);
		break;
	case 6: /* superscript 2 */
		do_textedit(0,0,178);
		break;
	case 7: /* superscript 3 */
		do_textedit(0,0,179);
		break;
	case 8: /* double >> */
		do_textedit(0,0,187);
		break;
	case 9: /* double << */
		do_textedit(0,0,171);
		break;
	case 10: /* Promillage */
		do_textedit(0,0,139);
		break;
	case 11: /* dutch florin */
		do_textedit(0,0,164);
		break;
	case 12: /* british pound */
		do_textedit(0,0,163);
		break;
	case 13: /* japanese yen*/
		do_textedit(0,0,165);
		break;
	case 14: /* german S */
		do_textedit(0,0,223);
		break;
	case 15: /* spanish question mark */
		do_textedit(0,0,191);
		break;
	case 16: /* spanish exclamation mark */
		do_textedit(0,0,161);
		break;
  	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_text_charsmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_text_charsmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_edit_text_charsmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copyright|Alt C",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Registered Trademark|Alt R",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Degree Sign|Alt G",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Multiplication Sign|Alt x",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Circle|Alt .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Superscript 1|Alt 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Superscript 2|Alt 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Superscript 3|Alt 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Double >>|Alt >",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Double <<|Alt <",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Promillage|Alt %",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 10, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Dutch Florin|Alt F",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "British Pound|Alt L",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 12, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Japanese Yen|Alt Y",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "German S|Alt S",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Spanish Question Mark|Alt ?",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Spanish Exclamation Mark|Alt !",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 16, "");
		
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
		
	return block;
}

static void do_view3d_edit_textmenu(void *arg, int event)
{
	switch(event) {
	              	
	case 0: /* Undo Editing */
		remake_editText();
		break;
	case 1: /* paste from file buffer */
		paste_editText();
		break;
	case 2: /* move to layer */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_textmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_textmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_textmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste From Buffer File|Alt V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_text_charsmenu, NULL, ICON_RIGHTARROW_THIN, "Special Characters", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_text_charsmenu, NULL, ICON_RIGHTARROW_THIN, "Special Characters", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Paste From Buffer File|Alt V",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_latticemenu(void *arg, int event)
{
	switch(event) {
	              	
	case 0: /* Undo Editing */
		remake_editLatt();
		break;
	case 1: /* snap */
		snapmenu();
		break;
	case 2: /* insert keyframe */
		common_insertkey();
		break;
	case 3: /* Shear */
		transform('S');
		break;
	case 4: /* Warp */
		transform('w');
		break;
	case 5: /* proportional edit (toggle) */
		if(G.f & G_PROPORTIONAL) G.f &= ~G_PROPORTIONAL;
		else G.f |= G_PROPORTIONAL;
		break;
	case 6: /* move to layer */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_latticemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
		
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_latticemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_latticemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if(G.f & G_PROPORTIONAL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		}
		uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_edit_propfalloffmenu, NULL, ICON_RIGHTARROW_THIN, "Proportional Falloff", 0, yco-=20, 120, 19, "");
		if(G.f & G_PROPORTIONAL) {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		} else {
			uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Proportional Editing|O",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		}

		uiDefBut(block, SEPR, 0, "",			0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_edit_armaturemenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* Undo Editing */
		remake_editArmature();
		break;
	case 1: /* transformation properties */
		blenderqread(NKEY, 1);
		break;
	case 2: /* snap */
		snapmenu();
		break;
	case 3: /* extrude */
		extrude_armature();
		break;
	case 4: /* duplicate */
		duplicate_context_selected();
		break;
	case 5: /* delete */
		delete_context_selected();
		break;
	case 6: /* Shear */
		transform('S');
		break;
	case 7: /* Warp */
		transform('w');
		break;
	case 8: /* Move to Layer */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_edit_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_edit_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_edit_armaturemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties|N",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 8, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Warp|Ctrl W",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Shear|Ctrl S",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrude|E",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");

		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Snap...|Shift S",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties|N",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Editing|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	
	return block;
}

static void do_view3d_pose_armature_transformmenu(void *arg, int event)
{
	switch(event) {
	case 0: /*  clear origin */
		clear_object('o');
		break;
	case 1: /* clear size */
		clear_object('s');
		break;
	case 2: /* clear rotation */
		clear_object('r');
		break;
	case 3: /* clear location */
		clear_object('g');
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_pose_armature_transformmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "view3d_pose_armature_transformmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_pose_armature_transformmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Location|Alt G",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Rotation|Alt R",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Size|Alt S",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Origin|Alt O",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_pose_armaturemenu(void *arg, int event)
{
	switch(event) {
	
	case 0: /* transform properties */
		blenderqread(NKEY, 1);
		break;
	case 1: /* insert keyframe */
		common_insertkey();
		break;
	case 2: /* Move to Layer */
		movetolayer();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_pose_armaturemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_pose_armaturemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_pose_armaturemenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties|N",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBlockBut(block, view3d_pose_armature_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
	} else {
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move to Layer...|M",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe|I",				0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, view3d_pose_armature_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Transform Properties|N",	0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	
	return block;
}


static void do_view3d_paintmenu(void *arg, int event)
{
	switch(event) {
	case 0: /* undo vertex painting */
		vpaint_undo();
		break;
	case 1: /* undo weight painting */
		wpaint_undo();
		break;
	case 2: /* clear vertex colors */
		clear_vpaint();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *view3d_paintmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_paintmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_paintmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
		
		if (G.f & G_VERTEXPAINT) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Vertex Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		if (G.f & G_WEIGHTPAINT) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Weight Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		if (G.f & G_TEXTUREPAINT) uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		if (G.f & G_VERTEXPAINT) {
			uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
			
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		}
		
	} else {
		
		if (G.f & G_VERTEXPAINT) {
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
			
			uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		}
		
		if (G.f & G_TEXTUREPAINT) uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		if (G.f & G_WEIGHTPAINT) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Weight Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		if (G.f & G_VERTEXPAINT) uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Undo Vertex Painting|U",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}

static void do_view3d_facesel_propertiesmenu(void *arg, int event)
{
	extern TFace *lasttface;
	set_lasttface();
	
	switch(event) {
	case 0: /*  textured */
		lasttface->mode ^= TF_TEX;
		break;
	case 1: /* tiled*/
		lasttface->mode ^= TF_TILES;
		break;
	case 2: /* light */
		lasttface->mode ^= TF_LIGHT;
		break;
	case 3: /* invisible */
		lasttface->mode ^= TF_INVISIBLE;
		break;
	case 4: /* collision */
		lasttface->mode ^= TF_DYNAMIC;
		break;
	case 5: /* shared vertex colors */
		lasttface->mode ^= TF_SHAREDCOL;
		break;
	case 6: /* two sided */
		lasttface->mode ^= TF_TWOSIDE;
		break;
	case 7: /* use object color */
		lasttface->mode ^= TF_OBCOL;
		break;
	case 8: /* halo */
		lasttface->mode ^= TF_BILLBOARD;
		break;
	case 9: /* billboard */
		lasttface->mode ^= TF_BILLBOARD2;
		break;
	case 10: /* shadow */
		lasttface->mode ^= TF_SHADOW;
		break;
	case 11: /* text */
		lasttface->mode ^= TF_BMFONT;
		break;
	case 12: /* opaque blend mode */
		lasttface->transp = TF_SOLID;
		break;
	case 13: /* additive blend mode */
		lasttface->transp |= TF_ADD;
		break;
	case 14: /* alpha blend mode */
		lasttface->transp = TF_ALPHA;
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSGAME, 0);
}

static uiBlock *view3d_facesel_propertiesmenu(void *arg_unused)
{
	extern TFace *lasttface;
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	/* to display ticks/crosses depending on face properties */
	set_lasttface();

	block= uiNewBlock(&curarea->uiblocks, "view3d_facesel_propertiesmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_view3d_facesel_propertiesmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if (lasttface->mode & TF_TEX) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Textured",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Textured",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	
	if (lasttface->mode & TF_TILES) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Tiled",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Tiled",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	
	if (lasttface->mode & TF_LIGHT) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Light",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Light",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	if (lasttface->mode & TF_INVISIBLE) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Invisible",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Invisible",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	if (lasttface->mode & TF_DYNAMIC) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Collision",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Collision",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
	if (lasttface->mode & TF_SHAREDCOL) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Shared Vertex Colors",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Shared Vertex Colors",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	
	if (lasttface->mode & TF_TWOSIDE) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Two Sided",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Two Sided",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	
	if (lasttface->mode & TF_OBCOL) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Use Object Color",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Use Object Color",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	
	if (lasttface->mode & TF_BILLBOARD) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Halo",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Halo",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	
	if (lasttface->mode & TF_BILLBOARD2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Billboard",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Billboard",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
		
	if (lasttface->mode & TF_SHADOW) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Shadow",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Shadow",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	
	if (lasttface->mode & TF_BMFONT) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Text",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Text",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	
	uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if (lasttface->transp == TF_SOLID) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Opaque Blend Mode",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Opaque Blend Mode",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	
	if (lasttface->transp == TF_ADD) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Additive Blend Mode",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Additive Blend Mode",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 13, "");
	
	if (lasttface->transp == TF_ALPHA) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Alpha Blend Mode",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Alpha Blend Mode",						0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 14, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_view3d_faceselmenu(void *arg, int event)
{
	/* code copied from buttons.c :(  
		would be nice if it was split up into functions */
	Mesh *me;
	Object *ob;
	extern TFace *lasttface; /* caches info on tface bookkeeping ?*/
	
	ob= OBACT;
	
	switch(event) {
	case 0: /* copy draw mode */
	case 1: /* copy UVs */
	case 2: /* copy vertex colors */
		me= get_mesh(ob);
		if(me && me->tface) {

			TFace *tface= me->tface;
			int a= me->totface;
			
			set_lasttface();
			if(lasttface) {
			
				while(a--) {
					if(tface!=lasttface && (tface->flag & TF_SELECT)) {
						if(event==0) {
							tface->mode= lasttface->mode;
							tface->transp= lasttface->transp;
						} else if(event==1) {
							memcpy(tface->uv, lasttface->uv, sizeof(tface->uv));
							tface->tpage= lasttface->tpage;
							tface->tile= lasttface->tile;
							
							if(lasttface->mode & TF_TILES) tface->mode |= TF_TILES;
							else tface->mode &= ~TF_TILES;
							
						} else if(event==2) memcpy(tface->col, lasttface->col, sizeof(tface->col));
					}
					tface++;
				}
			}
			do_shared_vertexcol(me);	
		}
		break;
	case 3: /* clear vertex colors */
		clear_vpaint_selectedfaces();
		break;
	// case 3: /* uv calculation */
	//	uv_autocalc_tface();
	//	break;
	case 4: /* show hidden faces */
		reveal_tface();
		break;
	case 5: /* hide selected faces */
		hide_tface();
		break;
	case 6: /* hide deselected faces */
		G.qual |= LR_SHIFTKEY;
		hide_tface();
		G.qual &= ~LR_SHIFTKEY;
		break;
	case 7: /* rotate UVs */
		rotate_uv_tface();
		break;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSGAME, 0);
	allqueue(REDRAWIMAGE, 0);
}

static uiBlock *view3d_faceselmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	set_lasttface();
	
	block= uiNewBlock(&curarea->uiblocks, "view3d_faceselmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_view3d_faceselmenu, NULL);
	uiBlockSetCol(block, MENUCOL);
	
	if(curarea->headertype==HEADERTOP) { 
	
		uiDefIconTextBlockBut(block, view3d_facesel_propertiesmenu, NULL, ICON_RIGHTARROW_THIN, "Active Draw Mode", 0, yco-=20, 120, 19, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Draw Mode",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");

		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy UVs & Textures",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Vertex Colors",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		/* for some reason calling this from the header messes up the 'from window'
		 * UV calculation :(
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Calculate UVs",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		*/
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotate UVs|R",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Faces|Alt H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Faces|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Faces|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		
	} else {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Deselected Faces|Shift H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Hide Selected Faces|H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Show Hidden Faces|Alt H",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotate UVs|R",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");
		/* for some reason calling this from the header messes up the 'from window'
		 * UV calculation :(
			uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Calculate UVs",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		*/
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Vertex Colors|Shift K",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Vertex Colors",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy UVs & Textures",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
		
		uiDefBut(block, SEPR, 0, "",				0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Copy Draw Mode",		0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefIconTextBlockBut(block, view3d_facesel_propertiesmenu, NULL, ICON_RIGHTARROW_THIN, "Active Draw Mode", 0, yco-=20, 120, 19, "");
	
	}

	uiBlockSetDirection(block, UI_TOP);
	uiTextBoundsBlock(block, 50);
	return block;
}


static char *view3d_modeselect_pup(void)
{
	static char string[1024];
	char formatstring[1024];

	strcpy(formatstring, "Mode: %%t");
	
	strcat(formatstring, "|%s %%x%d");	// add space in the menu for Object
	
	/* if active object is an armature */
	if (OBACT && OBACT->type==OB_ARMATURE) {
		strcat(formatstring, "|%s %%x%d");	// add space in the menu for pose
	}
	
	/* if active object is a mesh */
	if (OBACT && OBACT->type == OB_MESH) {
		strcat(formatstring, "|%s %%x%d|%s %%x%d|%s %%x%d");	// add space in the menu for faceselect, vertex paint, texture paint
		
		/* if active mesh has an armature */
		if ((((Mesh*)(OBACT->data))->dvert)) {
			strcat(formatstring, "|%s %%x%d");	// add space in the menu for weight paint
		}
	}
	
	/* if active object is editable */
	if (OBACT && ((OBACT->type == OB_MESH) || (OBACT->type == OB_ARMATURE)
	|| (OBACT->type == OB_CURVE) || (OBACT->type == OB_SURF) || (OBACT->type == OB_FONT)
	|| (OBACT->type == OB_MBALL) || (OBACT->type == OB_LATTICE))) {
		strcat(formatstring, "|%s %%x%d");	// add space in the menu for Edit
	}
	
	/*
	 * fill in the spaces in the menu with appropriate mode choices depending on active object
	 */

	/* if active object is an armature */
	if (OBACT && OBACT->type==OB_ARMATURE) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL,
		"Pose",						V3D_POSEMODE_SEL
		);
	}
	/* if active object is a mesh with armature */
	else if ((OBACT && OBACT->type == OB_MESH) && ((((Mesh*)(OBACT->data))->dvert))) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL,
		"Face Select",			V3D_FACESELECTMODE_SEL,
		"Vertex Paint",			V3D_VERTEXPAINTMODE_SEL,
		"Texture Paint",		V3D_TEXTUREPAINTMODE_SEL,
		"Weight Paint",			V3D_WEIGHTPAINTMODE_SEL	
		);
	}
	/* if active object is a mesh */
	else if (OBACT && OBACT->type == OB_MESH) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL,
		"Face Select",			V3D_FACESELECTMODE_SEL,
		"Vertex Paint",			V3D_VERTEXPAINTMODE_SEL,
		"Texture Paint",		V3D_TEXTUREPAINTMODE_SEL
		);
	} 
	/* if active object is editable */
	else if (OBACT && ((OBACT->type == OB_MESH) || (OBACT->type == OB_ARMATURE)
	|| (OBACT->type == OB_CURVE) || (OBACT->type == OB_SURF) || (OBACT->type == OB_FONT)
	|| (OBACT->type == OB_MBALL) || (OBACT->type == OB_LATTICE))) {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL,
		"Edit",						V3D_EDITMODE_SEL
		);
	}
	/* if active object is not editable */
	else {
		sprintf(string, formatstring,
		"Object",						V3D_OBJECTMODE_SEL
		);
	}
	
	return (string);
}


void do_view3d_buttons(short event)
{
	int bit;

	/* watch it: if curarea->win does not exist, check that when calling direct drawing routines */

	switch(event) {
	case B_HOME:
		view3d_home(0);
		break;
	case B_SCENELOCK:
		if(G.vd->scenelock) {
			G.vd->lay= G.scene->lay;
			/* seek for layact */
			bit= 0;
			while(bit<32) {
				if(G.vd->lay & (1<<bit)) {
					G.vd->layact= 1<<bit;
					break;
				}
				bit++;
			}
			G.vd->camera= G.scene->camera;
			scrarea_queue_winredraw(curarea);
			scrarea_queue_headredraw(curarea);
		}
		break;
	case B_LOCALVIEW:
		if(G.vd->localview) initlocalview();
		else endlocalview(curarea);
		scrarea_queue_headredraw(curarea);
		break;
	case B_EDITMODE:
		if (G.f & G_VERTEXPAINT) {
			/* Switch off vertex paint */
			G.f &= ~G_VERTEXPAINT;
		}
		if (G.f & G_WEIGHTPAINT){
			/* Switch off weight paint */
			G.f &= ~G_WEIGHTPAINT;
		}
#ifdef NAN_TPT
		if (G.f & G_TEXTUREPAINT) {
			/* Switch off texture paint */
			G.f &= ~G_TEXTUREPAINT;
		}
#endif /* NAN_VPT */
		if(G.obedit==0)	enter_editmode();
		else exit_editmode(1);
		scrarea_queue_headredraw(curarea);
		break;
	case B_POSEMODE:
	/*	if (G.obedit){
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_POSEMODE;
			scrarea_queue_headredraw(curarea);
		}
		else{
		*/	
		if (G.obpose==NULL)	enter_posemode();
		else exit_posemode(1);

		allqueue(REDRAWHEADERS, 0);	
		
		break;
	case B_WPAINT:
		if (G.f & G_VERTEXPAINT) {
			/* Switch off vertex paint */
			G.f &= ~G_VERTEXPAINT;
		}
#ifdef NAN_TPT
		if ((!(G.f & G_WEIGHTPAINT)) && (G.f & G_TEXTUREPAINT)) {
			/* Switch off texture paint */
			G.f &= ~G_TEXTUREPAINT;
		}
#endif /* NAN_VPT */
		if(G.obedit) {
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_WEIGHTPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else if(G.obpose) {
			error("Unable to perform function in PoseMode");
			G.vd->flag &= ~V3D_WEIGHTPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else set_wpaint();
		break;
	case B_VPAINT:
		if ((!(G.f & G_VERTEXPAINT)) && (G.f & G_WEIGHTPAINT)) {
			G.f &= ~G_WEIGHTPAINT;
		}
#ifdef NAN_TPT
		if ((!(G.f & G_VERTEXPAINT)) && (G.f & G_TEXTUREPAINT)) {
			/* Switch off texture paint */
			G.f &= ~G_TEXTUREPAINT;
		}
#endif /* NAN_VPT */
		if(G.obedit) {
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_VERTEXPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else if(G.obpose) {
			error("Unable to perform function in PoseMode");
			G.vd->flag &= ~V3D_VERTEXPAINT;
			scrarea_queue_headredraw(curarea);
		}
		else set_vpaint();
		break;
		
#ifdef NAN_TPT
	case B_TEXTUREPAINT:
		if (G.f & G_TEXTUREPAINT) {
			G.f &= ~G_TEXTUREPAINT;
		}
		else {
			if (G.obedit) {
				error("Unable to perform function in EditMode");
				G.vd->flag &= ~V3D_TEXTUREPAINT;
			}
			else {
				if (G.f & G_WEIGHTPAINT){
					/* Switch off weight paint */
					G.f &= ~G_WEIGHTPAINT;
				}
				if (G.f & G_VERTEXPAINT) {
					/* Switch off vertex paint */
					G.f &= ~G_VERTEXPAINT;
				}
				if (G.f & G_FACESELECT) {
					/* Switch off face select */
					G.f &= ~G_FACESELECT;
				}
				G.f |= G_TEXTUREPAINT;
				scrarea_queue_headredraw(curarea);
			}
		}
		break;
#endif /* NAN_TPT */

	case B_FACESEL:
		if(G.obedit) {
			error("Unable to perform function in EditMode");
			G.vd->flag &= ~V3D_FACESELECT;
			scrarea_queue_headredraw(curarea);
		}
		else if(G.obpose) {
			error("Unable to perform function in PoseMode");
			G.vd->flag &= ~V3D_FACESELECT;
			scrarea_queue_headredraw(curarea);
		}
		else set_faceselect();
		break;
		
	case B_VIEWBUT:
	
		if(G.vd->viewbut==1) persptoetsen(PAD7);
		else if(G.vd->viewbut==2) persptoetsen(PAD1);
		else if(G.vd->viewbut==3) persptoetsen(PAD3);
		break;

	case B_PERSP:
	
		if(G.vd->persp==2) persptoetsen(PAD0);
		else {
			G.vd->persp= 1-G.vd->persp;
			persptoetsen(PAD5);
		}
		
		break;
	case B_PROPTOOL:
		allqueue(REDRAWHEADERS, 0);
		break;
	case B_VIEWRENDER:
		if (curarea->spacetype==SPACE_VIEW3D) {
			BIF_do_ogl_render(curarea->spacedata.first, G.qual!=0 );
		}
	 	break;
	case B_STARTGAME:
		if (select_area(SPACE_VIEW3D)) {
	    	start_game();
		}
		break;
	case B_VIEWZOOM:
		viewmovetemp= 0;
		viewmove(2);
		scrarea_queue_headredraw(curarea);
		break;
	case B_VIEWTRANS:
		viewmovetemp= 0;
		viewmove(1);
		scrarea_queue_headredraw(curarea);
		break;
	case B_MODESELECT:
		if (G.vd->modeselect == V3D_OBJECTMODE_SEL) { 
			G.vd->flag &= ~V3D_MODE;
			G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
			G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
			G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
			G.f &= ~G_FACESELECT;		/* Switch off face select */
			if (G.obpose) exit_posemode(1);	/* exit posemode */
			if(G.obedit) exit_editmode(1);	/* exit editmode */
		} else if (G.vd->modeselect == V3D_EDITMODE_SEL) {
			if(!G.obedit) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1);	/* exit posemode */
					
				enter_editmode();
			}
		} else if (G.vd->modeselect == V3D_FACESELECTMODE_SEL) {
			if ((G.obedit) && (G.f & G_FACESELECT)) {
				exit_editmode(1);	/* exit editmode */
			} else if ((G.f & G_FACESELECT) && (G.f & G_VERTEXPAINT)) {
				G.f &= ~G_VERTEXPAINT;	
			} else if ((G.f & G_FACESELECT) && (G.f & G_TEXTUREPAINT)) {
				G.f &= ~G_TEXTUREPAINT;	
			} else {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1);	/* exit posemode */
				if (G.obedit) exit_editmode(1);	/* exit editmode */
				
				set_faceselect();
			}
		} else if (G.vd->modeselect == V3D_VERTEXPAINTMODE_SEL) {
			if (!(G.f & G_VERTEXPAINT)) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1);	/* exit posemode */
				if(G.obedit) exit_editmode(1);	/* exit editmode */
					
				set_vpaint();
			}
		} else if (G.vd->modeselect == V3D_TEXTUREPAINTMODE_SEL) {
			if (!(G.f & G_TEXTUREPAINT)) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_WEIGHTPAINT;		/* Switch off weight paint */
				if (G.obpose) exit_posemode(1);	/* exit posemode */
				if(G.obedit) exit_editmode(1);	/* exit editmode */
					
				G.f |= G_TEXTUREPAINT;		/* Switch on texture paint flag */
			}
		} else if (G.vd->modeselect == V3D_WEIGHTPAINTMODE_SEL) {
			if (!(G.f & G_WEIGHTPAINT) && (OBACT && OBACT->type == OB_MESH) && ((((Mesh*)(OBACT->data))->dvert))) {
				G.vd->flag &= ~V3D_MODE;
				G.f &= ~G_VERTEXPAINT;		/* Switch off vertex paint */
				G.f &= ~G_TEXTUREPAINT;		/* Switch off texture paint */
				if (G.obpose) exit_posemode(1);	/* exit posemode */
				if(G.obedit) exit_editmode(1);	/* exit editmode */
				
				set_wpaint();
			}
		} else if (G.vd->modeselect == V3D_POSEMODE_SEL) {
			if (!G.obpose) {
				G.vd->flag &= ~V3D_MODE;
				if(G.obedit) exit_editmode(1);	/* exit editmode */
					
				enter_posemode();
			}
		}
		allqueue(REDRAWVIEW3D, 0);
		break;
	
	default:

		if(event>=B_LAY && event<B_LAY+31) {
			if(G.vd->lay!=0 && (G.qual & LR_SHIFTKEY)) {
				
				/* but do find active layer */
				
				bit= event-B_LAY;
				if( G.vd->lay & (1<<bit)) G.vd->layact= 1<<bit;
				else {
					if( (G.vd->lay & G.vd->layact) == 0) {
						bit= 0;
						while(bit<32) {
							if(G.vd->lay & (1<<bit)) {
								G.vd->layact= 1<<bit;
								break;
							}
							bit++;
						}
					}
				}
			}
			else {
				bit= event-B_LAY;
				G.vd->lay= 1<<bit;
				G.vd->layact= G.vd->lay;
				scrarea_queue_headredraw(curarea);
			}
			scrarea_queue_winredraw(curarea);
			countall();

			if(G.vd->scenelock) handle_view3d_lock();
			allqueue(REDRAWOOPS, 0);
		}
		break;
	}
}

void do_nla_buttons(unsigned short event)
{
	View2D	*v2d;

	switch(event){
	case B_NLAHOME:
		//	Find X extents
		v2d= &(G.snla->v2d);

		v2d->cur.xmin = G.scene->r.sfra;
		v2d->cur.ymin=-SCROLLB;
		
//		if (!G.saction->action){
			v2d->cur.xmax=G.scene->r.efra;
//		}
//		else
//		{
//			v2d->cur.xmax=calc_action_length(G.saction->action)+1;
//		}
		
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		addqueue (curarea->win, REDRAW, 1);
		break;
	}
}

void nla_buttons(void)
{
	SpaceNla *snla;
	short xco;
	char naam[20];
	uiBlock *block;
	
	snla= curarea->spacedata.first;
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTCHOKE);

	curarea->butspacetype= SPACE_NLA;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_NLAHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");	
	xco+= XIC;
	
	/* IMAGE */
//	uiDefIconButS(block, TOG, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &sseq->mainb, 0, 0, 0, 0, "Toggles image display");

	/* ZOOM en BORDER */
//	xco+= XIC;
//	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zoom view (CTRL+MiddleMouse)");
//	uiDefIconBut(block, BUT, B_NLABORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zoom view to area");

	/* draw LOCK */
	xco+= XIC/2;

	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(snla->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");

	uiDrawBlock(block);}

void action_buttons(void)
{
	uiBlock *block;
	short xco;
	char naam[256];
	Object *ob;
	ID *from;

	if (!G.saction)
		return;

	// copy from drawactionspace....
	if (!G.saction->pin) {
		if (OBACT)
			G.saction->action = OBACT->action;
		else
			G.saction->action=NULL;
	}

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTPINK);

	curarea->butspacetype= SPACE_ACTION;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_ACTHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");

	
	if (!get_action_mesh_key()) {
		/* NAME ETC */
		ob=OBACT;
		from = (ID*) ob;

		xco= std_libbuttons(block, xco+1.5*XIC, B_ACTPIN, &G.saction->pin, 
							B_ACTIONBROWSE, (ID*)G.saction->action, 
							from, &(G.saction->actnr), B_ACTALONE, 
							B_ACTLOCAL, B_ACTIONDELETE, 0, 0);	

#ifdef __NLA_BAKE
		/* Draw action baker */
		uiDefBut(block, BUT, B_ACTBAKE, "Bake", 
				 xco+=XIC, 0, 64, YIC, 0, 0, 0, 0, 0, 
				 "Generate an action with the constraint "
				 "effects converted into ipo keys");
		xco+=64;
#endif
	}
	uiClearButLock();

	/* draw LOCK */
	xco+= XIC/2;
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.saction->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");


	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}

void view3d_buttons(void)
{
	uiBlock *block;
	int a;
	short xco = 0;
	char naam[20];
	short xmax;
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, MIDGREY);	

	curarea->butspacetype= SPACE_VIEW3D;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+18;

	/* pull down menus */
	uiBlockSetEmboss(block, UI_EMBOSSP);
	if(area_is_active_area(curarea)) uiBlockSetCol(block, HEADERCOLSEL);	
	else uiBlockSetCol(block, HEADERCOL);	
	
	/* compensate for local mode when setting up the viewing menu/iconrow values */
	if(G.vd->view==7) G.vd->viewbut= 1;
	else if(G.vd->view==1) G.vd->viewbut= 2;
	else if(G.vd->view==3) G.vd->viewbut= 3;
	else G.vd->viewbut= 0;
	
	xmax= GetButStringLength("View");
	uiDefBlockBut(block, view3d_viewmenu, NULL, "View",	xco, 0, xmax, 20, "");
	xco+= xmax;
	
	xmax= GetButStringLength("Select");
	if (G.obedit) {
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefBlockBut(block, view3d_select_meshmenu, NULL, "Select",	xco, 0, xmax, 20, "");
		} else if (OBACT && (OBACT->type == OB_CURVE || OBACT->type == OB_SURF)) {
			uiDefBlockBut(block, view3d_select_curvemenu, NULL, "Select",	xco, 0, xmax, 20, "");
		} else if (OBACT && OBACT->type == OB_FONT) {
			uiDefBlockBut(block, view3d_select_meshmenu, NULL, "Select",	xco, 0, xmax, 20, "");
		} else if (OBACT && OBACT->type == OB_MBALL) {
			uiDefBlockBut(block, view3d_select_metaballmenu, NULL, "Select",	xco, 0, xmax, 20, "");
		} else if (OBACT && OBACT->type == OB_LATTICE) {
			uiDefBlockBut(block, view3d_select_latticemenu, NULL, "Select",	xco, 0, xmax, 20, "");
		} else if (OBACT && OBACT->type == OB_ARMATURE) {
			uiDefBlockBut(block, view3d_select_armaturemenu, NULL, "Select",	xco, 0, xmax, 20, "");
		}
	} else if (G.f & G_FACESELECT) {
		if (OBACT && OBACT->type == OB_MESH) {
			uiDefBlockBut(block, view3d_select_faceselmenu, NULL, "Select",	xco, 0, xmax, 20, "");
		}
	} else if (G.obpose) {
		if (OBACT && OBACT->type == OB_ARMATURE) {
			uiDefBlockBut(block, view3d_select_pose_armaturemenu, NULL, "Select",	xco, 0, xmax, 20, "");
		}
	} else if ((G.f & G_VERTEXPAINT) || (G.f & G_TEXTUREPAINT) || (G.f & G_WEIGHTPAINT)) {
		uiDefBut(block, LABEL,0,"", xco, 0, xmax, 20, 0, 0, 0, 0, 0, "");
	} else {
		uiDefBlockBut(block, view3d_select_objectmenu, NULL, "Select",	xco, 0, xmax, 20, "");
	}
	xco+= xmax;
	
	if ((G.f & G_VERTEXPAINT) || (G.f & G_TEXTUREPAINT) || (G.f & G_WEIGHTPAINT)) {
			xmax= GetButStringLength("Paint");
			uiDefBlockBut(block, view3d_paintmenu, NULL, "Paint",	xco, 0, xmax, 20, "");
			xco+= xmax;
	} else if (G.obedit) {
		if (OBACT && OBACT->type == OB_MESH) {
			xmax= GetButStringLength("Mesh");
			uiDefBlockBut(block, view3d_edit_meshmenu, NULL, "Mesh",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_CURVE) {
			xmax= GetButStringLength("Curve");
			uiDefBlockBut(block, view3d_edit_curvemenu, NULL, "Curve",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_SURF) {
			xmax= GetButStringLength("Surface");
			uiDefBlockBut(block, view3d_edit_curvemenu, NULL, "Surface",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_FONT) {
			xmax= GetButStringLength("Text");
			uiDefBlockBut(block, view3d_edit_textmenu, NULL, "Text",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_MBALL) {
			xmax= GetButStringLength("Metaball");
			uiDefBlockBut(block, view3d_edit_metaballmenu, NULL, "Metaball",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_LATTICE) {
			xmax= GetButStringLength("Lattice");
			uiDefBlockBut(block, view3d_edit_latticemenu, NULL, "Lattice",	xco, 0, xmax, 20, "");
			xco+= xmax;
		} else if (OBACT && OBACT->type == OB_ARMATURE) {
			xmax= GetButStringLength("Armature");
			uiDefBlockBut(block, view3d_edit_armaturemenu, NULL, "Armature",	xco, 0, xmax, 20, "");
			xco+= xmax;
		}
	} else if (G.f & G_FACESELECT) {
		if (OBACT && OBACT->type == OB_MESH) {
			xmax= GetButStringLength("Face");
			uiDefBlockBut(block, view3d_faceselmenu, NULL, "Face",	xco, 0, xmax, 20, "");
			xco+= xmax;
		}
	} else if (G.obpose) {
		if (OBACT && OBACT->type == OB_ARMATURE) {
			xmax= GetButStringLength("Armature");
			uiDefBlockBut(block, view3d_pose_armaturemenu, NULL, "Armature",	xco, 0, xmax, 20, "");
			xco+= xmax;
		}
	} else {
		xmax= GetButStringLength("Object");
		uiDefBlockBut(block, view3d_edit_objectmenu, NULL, "Object",	xco, 0, xmax, 20, "");
		xco+= xmax;
	}

	/* end pulldowns, other buttons: */
	uiBlockSetCol(block, MIDGREY);
	uiBlockSetEmboss(block, UI_EMBOSSX);
	
	/* mode */
	G.vd->modeselect = V3D_OBJECTMODE_SEL;
	if (G.f & G_WEIGHTPAINT) G.vd->modeselect = V3D_WEIGHTPAINTMODE_SEL;
	else if (G.f & G_VERTEXPAINT) G.vd->modeselect = V3D_VERTEXPAINTMODE_SEL;
	else if (G.f & G_TEXTUREPAINT) G.vd->modeselect = V3D_TEXTUREPAINTMODE_SEL;
	else if(G.f & G_FACESELECT) G.vd->modeselect = V3D_FACESELECTMODE_SEL;
	if (G.obpose) G.vd->modeselect = V3D_POSEMODE_SEL;
	if (G.obedit) G.vd->modeselect = V3D_EDITMODE_SEL;
		
	G.vd->flag &= ~V3D_MODE;
	if(G.obedit) G.vd->flag |= V3D_EDITMODE;
	if(G.f & G_VERTEXPAINT) G.vd->flag |= V3D_VERTEXPAINT;
	if(G.f & G_WEIGHTPAINT) G.vd->flag |= V3D_WEIGHTPAINT;
#ifdef NAN_TPT
	if (G.f & G_TEXTUREPAINT) G.vd->flag |= V3D_TEXTUREPAINT;
#endif /* NAN_TPT */
	if(G.f & G_FACESELECT) G.vd->flag |= V3D_FACESELECT;
	if(G.obpose){
		G.vd->flag |= V3D_POSEMODE;
	}
	
	xco+= 16;

	uiDefIconTextButS(block, MENU, B_MODESELECT, (G.vd->modeselect),view3d_modeselect_pup() ,	
																xco,0,120,20, &(G.vd->modeselect), 0, 0, 0, 0, "Mode:");
	
	xco+= 120;
	xco +=14;
	
	//uiDefIconTextButS(block, MENU, REDRAWVIEW3D, (ICON_BBOX+G.vd->drawtype-1), "Viewport Shading%t|Bounding Box %x1|Wireframe %x2|Solid %x3|Shaded %x4|Textured %x5",	
	//															xco,0,124,20, &(G.vd->drawtype), 0, 0, 0, 0, "Viewport Shading");
	
	uiDefButS(block, MENU, REDRAWVIEW3D, "Viewport Shading%t|Bounding Box %x1|Wireframe %x2|Solid %x3|Shaded %x4|Textured %x5",	
																xco,0,110,20, &(G.vd->drawtype), 0, 0, 0, 0, "Viewport Shading");
	
	xco+=110;
	
	xco+= 14;
	/* LAYERS */
	if(G.vd->localview==0) {
		
		for(a=0; a<10; a++) {
			uiDefButI(block, TOG|BIT|(a+10), B_LAY+10+a, "",(short)(xco+a*(XIC/2)), 0,			XIC/2, (YIC)/2, &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");
			uiDefButI(block, TOG|BIT|a, B_LAY+a, "",	(short)(xco+a*(XIC/2)), (short)(YIC/2),(short)(XIC/2),(short)(YIC/2), &(G.vd->lay), 0, 0, 0, 0, "Toggles Layer visibility");
			if(a==4) xco+= 5;
		}
		xco+= (a-2)*(XIC/2)+5;

		/* LOCK */
		uiDefIconButS(block, ICONTOG, B_SCENELOCK, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.vd->scenelock), 0, 0, 0, 0, "Locks layers and used Camera to Scene");
		xco+= 14;

	}
	else xco+= (10+1)*(XIC/2)+10+4;

	/* VIEWMOVE */

	uiDefIconButI(block, TOG, B_VIEWTRANS, ICON_VIEWMOVE,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Translates view (SHIFT+MiddleMouse)");
	uiDefIconButI(block, TOG, B_VIEWZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (CTRL+MiddleMouse)");

	/* around */
	xco+= XIC/2;
	uiDefIconButS(block, ROW, 1, ICON_ROTATE,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 0.0, 0, 0, "Enables Rotation or Scaling around boundbox center (COMMAKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECENTER,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 3.0, 0, 0, "Enables Rotation or Scaling around median point");
	uiDefIconButS(block, ROW, 1, ICON_CURSOR,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 1.0, 0, 0, "Enables Rotation or Scaling around cursor (DOTKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECOLLECTION,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 2.0, 0, 0, "Enables Rotation or Scaling around individual object centers");

	
	
	
	if(G.vd->bgpic) {
		xco+= XIC/2;
		uiDefIconButS(block, TOG|BIT|1, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "Displays a Background picture");
	}
	if(G.obedit && (OBACT->type == OB_MESH || OBACT->type == OB_CURVE || OBACT->type == OB_SURF || OBACT->type == OB_LATTICE)) {
		extern int prop_mode;
		xco+= XIC/2;
		uiDefIconButI(block, ICONTOG|BIT|14, B_PROPTOOL, ICON_GRID,	xco+=XIC,0,XIC,YIC, &G.f, 0, 0, 0, 0, "Toggles Proportional Vertex Editing (OKEY)");
		if(G.f & G_PROPORTIONAL) {
			uiDefIconButI(block, ROW, 0, ICON_SHARPCURVE,	xco+=XIC,0,XIC,YIC, &prop_mode, 4.0, 0.0, 0, 0, "Enables Sharp falloff (SHIFT+OKEY)");
			uiDefIconButI(block, ROW, 0, ICON_SMOOTHCURVE,	xco+=XIC,0,XIC,YIC, &prop_mode, 4.0, 1.0, 0, 0, "Enables Smooth falloff (SHIFT+OKEY)");
		}
	}
	
	xco+=XIC;

	/* Always do this last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);

}

/* ********************** VIEW3D ****************************** */
/* ********************** IPO ****************************** */

void do_ipo_buttons(short event)
{
	EditIpo *ei;
	View2D *v2d;
	rcti rect;
	float xmin, ymin, dx, dy;
	int a, val, first;
	short mval[2];

	if(curarea->win==0) return;

	switch(event) {
	case B_IPOHOME:
		
		/* boundbox */
			
		v2d= &(G.sipo->v2d);
		first= 1;
		
		ei= G.sipo->editipo;
		if(ei==0) return;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			
				boundbox_ipocurve(ei->icu);
				
				if(first) {
					v2d->tot= ei->icu->totrct;
					first= 0;
				}
				else BLI_union_rctf(&(v2d->tot), &(ei->icu->totrct));
			}
		}

		/* speciale home */
		if(G.qual & LR_SHIFTKEY) {
			v2d->tot.xmin= SFRA;
			v2d->tot.xmax= EFRA;
		}

		/* zoom out a bit */
		dx= 0.10*(v2d->tot.xmax-v2d->tot.xmin);
		dy= 0.10*(v2d->tot.ymax-v2d->tot.ymin);
		
		if(dx<v2d->min[0]) dx= v2d->min[0];
		if(dy<v2d->min[1]) dy= v2d->min[1];
		
		v2d->cur.xmin= v2d->tot.xmin- dx;
		v2d->cur.xmax= v2d->tot.xmax+ dx;
		v2d->cur.ymin= v2d->tot.ymin- dy;
		v2d->cur.ymax= v2d->tot.ymax+ dy;

		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	case B_IPOBORDER:
		val= get_border(&rect, 2);
		if(val) {
			mval[0]= rect.xmin;
			mval[1]= rect.ymin;
			areamouseco_to_ipoco(G.v2d, mval, &xmin, &ymin);
			mval[0]= rect.xmax;
			mval[1]= rect.ymax;
			areamouseco_to_ipoco(G.v2d, mval, &(G.v2d->cur.xmax), &(G.v2d->cur.ymax));
			G.v2d->cur.xmin= xmin;
			G.v2d->cur.ymin= ymin;
			
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			scrarea_queue_winredraw(curarea);
		}
		break;

	case B_IPOPIN:
		allqueue (REDRAWIPO, 0);
		break;

	case B_IPOCOPY:
		copy_editipo();
		break;
	case B_IPOPASTE:
		paste_editipo();
		break;
	case B_IPOCONT:
		set_exprap_ipo(IPO_HORIZ);
		break;
	case B_IPOEXTRAP:
		set_exprap_ipo(IPO_DIR);
		break;
	case B_IPOCYCLIC:
		set_exprap_ipo(IPO_CYCL);
		break;
	case B_IPOCYCLICX:
		set_exprap_ipo(IPO_CYCLX);
		break;
	case B_IPOMAIN:
		make_editipo();
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);

		break;
	case B_IPOSHOWKEY:
		/* reverse value because of winqread */
		G.sipo->showkey= 1-G.sipo->showkey;
		ipo_toggle_showkey();
		scrarea_queue_headredraw(curarea);
		scrarea_queue_winredraw(curarea);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_VIEW2DZOOM:
		viewmovetemp= 0;
		view2dzoom(event);
		scrarea_queue_headredraw(curarea);
		break;
			
	}	
}

void ipo_buttons(void)
{
	Object *ob;
	ID *id, *from;
	uiBlock *block;
	short xco;
	char naam[20];

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTSALMON);

	curarea->butspacetype= SPACE_IPO;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	test_editipo();	/* test if current editipo is OK, make_editipo sets v2d->cur */

		/* FULL WINDOW en HOME */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_IPOHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");
	uiDefIconButS(block, ICONTOG, B_IPOSHOWKEY, ICON_KEY_DEHLT,	xco+=XIC,0,XIC,YIC, &G.sipo->showkey, 0, 0, 0, 0, "Toggles between Curve and Key display (KKEY)");

	/* mainmenu, only when data is there and no pin */
	uiSetButLock(G.sipo->pin, "Can't change because of pinned data");
	
	ob= OBACT;
	xco+= XIC/2;
	uiDefIconButS(block, ROW, B_IPOMAIN, ICON_OBJECT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_OB, 0, 0, "Displays Object Ipos");
	
	if(ob && give_current_material(ob, ob->actcol)) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_MATERIAL,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_MA, 0, 0, "Displays Material Ipos");
		if(G.sipo->blocktype==ID_MA) {
			uiDefButS(block, NUM, B_IPOMAIN, "", 	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active Material texture. Click to change.");
			xco-= 4;
		}
	}
	if(G.scene->world) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_WORLD,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_WO, 0, 0, "Display World Ipos");
		if(G.sipo->blocktype==ID_WO) {
			uiDefButS(block, NUM, B_IPOMAIN, "", 	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active World texture. Click to change.");
			xco-= 4;
		}
	}
	
	if(ob && ob->type==OB_CURVE)
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_ANIM,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_CU, 0, 0, "Display Curve Ipos");
	
	if(ob && ob->type==OB_CAMERA)
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_CAMERA,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_CA, 0, 0, "Display Camera Ipos");
	
	if(ob && ob->type==OB_LAMP) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_LAMP,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_LA, 0, 0, "Display Lamp Ipos");
		if(G.sipo->blocktype==ID_LA) {
			uiDefButS(block, NUM, B_IPOMAIN, "", 	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active Lamp texture. Click to change.");
			xco-= 4;
		}
	}
	
	if(ob) {
		if ELEM4(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_LATTICE)
			uiDefIconButS(block, ROW, B_IPOMAIN, ICON_EDIT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_KE, 0, 0, "Displays VertexKeys Ipos");
		if (ob->action)
			uiDefIconButS(block, ROW, B_IPOMAIN, ICON_ACTION,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_AC, 0, 0, "Displays Action Ipos");
#ifdef __CON_IPO
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_CONSTRAINT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)IPO_CO, 0, 0, "Displays Constraint Ipos");
#endif
	}
	uiDefIconButS(block, ROW, B_IPOMAIN, ICON_SEQUENCE,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_SEQ, 0, 0, "Displays Sequence Ipos");

	if(G.buts && G.buts->mainb == BUTS_SOUND && G.buts->lockpoin) 
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_SOUND,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_SO, 0, 0, "Displays Sound Ipos");

	
	uiClearButLock();

	/* NAME ETC */
	id= (ID *)get_ipo_to_edit(&from);

	xco= std_libbuttons(block, (short)(xco+1.5*XIC), B_IPOPIN, &G.sipo->pin, B_IPOBROWSE, (ID*)G.sipo->ipo, from, &(G.sipo->menunr), B_IPOALONE, B_IPOLOCAL, B_IPODELETE, 0, B_KEEPDATA);

	uiSetButLock(id && id->lib, "Can't edit library data");

	/* COPY PASTE */
	xco-= XIC/2;
	if(curarea->headertype==HEADERTOP) {
		uiDefIconBut(block, BUT, B_IPOCOPY, ICON_COPYUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected curves to the buffer");
		uiSetButLock(id && id->lib, "Can't edit library data");
		uiDefIconBut(block, BUT, B_IPOPASTE, ICON_PASTEUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the curves from the buffer");
	}
	else {
		uiDefIconBut(block, BUT, B_IPOCOPY, ICON_COPYDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected curves to the buffer");
		uiSetButLock(id && id->lib, "Can't edit library data");
		uiDefIconBut(block, BUT, B_IPOPASTE, ICON_PASTEDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the curves from the buffer");
	}
	xco+=XIC/2;
	
	/* EXTRAP */
	uiDefIconBut(block, BUT, B_IPOCONT, ICON_CONSTANT,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Sets the extend mode to constant");
	uiDefIconBut(block, BUT, B_IPOEXTRAP, ICON_LINEAR,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Sets the extend mode to extrapolation");
	uiDefIconBut(block, BUT, B_IPOCYCLIC, ICON_CYCLIC,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0,  "Sets the extend mode to cyclic");
	uiDefIconBut(block, BUT, B_IPOCYCLICX, ICON_CYCLICLINEAR,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0,  "Sets the extend mode to cyclic extrapolation");
	xco+= XIC/2;

	uiClearButLock();
	/* ZOOM en BORDER */
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area");
	
	/* draw LOCK */
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.sipo->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");

	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}

/* ********************** IPO ****************************** */
/* ********************** BUTS ****************************** */

Material matcopybuf;

void clear_matcopybuf(void)
{
	memset(&matcopybuf, 0, sizeof(Material));
}

void free_matcopybuf(void)
{
	extern MTex mtexcopybuf;	/* buttons.c */
	int a;
	
	for(a=0; a<8; a++) {
		if(matcopybuf.mtex[a]) {
			MEM_freeN(matcopybuf.mtex[a]);
			matcopybuf.mtex[a]= 0;
		}
	}
	
	default_mtex(&mtexcopybuf);
}

void do_buts_buttons(short event)
{
	static short matcopied=0;
	MTex *mtex;
	Material *ma;
	ID id;
	int a;
	
	if(curarea->win==0) return;

	switch(event) {
	case B_BUTSHOME:
		uiSetPanel_view2d(curarea);
		G.v2d->cur= G.v2d->tot;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	case B_BUTSPREVIEW:
		BIF_preview_changed(G.buts);
		scrarea_queue_headredraw(curarea);
		scrarea_queue_winredraw(curarea);
		break;
	case B_MATCOPY:
		if(G.buts->lockpoin) {
			
			if(matcopied) free_matcopybuf();
			
			memcpy(&matcopybuf, G.buts->lockpoin, sizeof(Material));
			for(a=0; a<8; a++) {
				mtex= matcopybuf.mtex[a];
				if(mtex) {
					matcopybuf.mtex[a]= MEM_dupallocN(mtex);
				}
			}
			matcopied= 1;
		}
		break;
	case B_MATPASTE:
		if(matcopied && G.buts->lockpoin) {
			ma= G.buts->lockpoin;
			/* free current mat */
			for(a=0; a<8; a++) {
				mtex= ma->mtex[a];
				if(mtex && mtex->tex) mtex->tex->id.us--;
				if(mtex) MEM_freeN(mtex);
			}
			
			id= (ma->id);
			memcpy(G.buts->lockpoin, &matcopybuf, sizeof(Material));
			(ma->id)= id;
			
			for(a=0; a<8; a++) {
				mtex= ma->mtex[a];
				if(mtex) {
					ma->mtex[a]= MEM_dupallocN(mtex);
					if(mtex->tex) id_us_plus((ID *)mtex->tex);
				}
			}
			BIF_preview_changed(G.buts);
			scrarea_queue_winredraw(curarea);
		}
		break;
	case B_MESHTYPE:
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	}
}

void buttons_active_id(ID **id, ID **idfrom)
{
	Object *ob= OBACT;
	Material *ma;
	ID * search;

	*id= NULL;
	*idfrom= (ID *)ob;
	
	if(G.buts->mainb==BUTS_LAMP) {
		if(ob && ob->type==OB_LAMP) {
			*id= ob->data;
		}
	}
	else if(G.buts->mainb==BUTS_MAT) {
		if(ob && (ob->type<OB_LAMP) && ob->type) {
			*id= (ID *)give_current_material(ob, ob->actcol);
			*idfrom= material_from(ob, ob->actcol);
		}
	}
	else if(G.buts->mainb==BUTS_TEX) {
		MTex *mtex;
		
		if(G.buts->mainbo != G.buts->mainb) {
			if(G.buts->mainbo==BUTS_LAMP) G.buts->texfrom= 2;
			else if(G.buts->mainbo==BUTS_WORLD) G.buts->texfrom= 1;
			else if(G.buts->mainbo==BUTS_MAT) G.buts->texfrom= 0;
		}

		if(G.buts->texfrom==0) {
			if(ob && ob->type<OB_LAMP && ob->type) {
				ma= give_current_material(ob, ob->actcol);
				*idfrom= (ID *)ma;
				if(ma) {
					mtex= ma->mtex[ ma->texact ];
					if(mtex) *id= (ID *)mtex->tex;
				}
			}
		}
		else if(G.buts->texfrom==1) {
			World *wrld= G.scene->world;
			*idfrom= (ID *)wrld;
			if(wrld) {
				mtex= wrld->mtex[ wrld->texact];
				if(mtex) *id= (ID *)mtex->tex;
			}
		}
		else if(G.buts->texfrom==2) {
			Lamp *la;
			if(ob && ob->type==OB_LAMP) {
				la= ob->data;
				*idfrom= (ID *)la;
				mtex= la->mtex[ la->texact];
				if(mtex) *id= (ID *)mtex->tex;
			}
		}
	}
	else if ELEM3(G.buts->mainb, BUTS_ANIM, BUTS_GAME, BUTS_CONSTRAINT) {
		if(ob) {
			*idfrom= (ID *)G.scene;
			*id= (ID *)ob;
		}
	}
	else if(G.buts->mainb==BUTS_WORLD) {
		*id= (ID *)G.scene->world;
		*idfrom= (ID *)G.scene;
	}
	else if(G.buts->mainb==BUTS_RENDER) {
		*id= (ID *)G.scene;
		
	}
	else if(G.buts->mainb==BUTS_EDIT) {
		if(ob && ob->data) {
			*id= ob->data;
		}
	}
	else if (G.buts->mainb == BUTS_SOUND) {
		// printf("lockp: %d\n", G.buts->lockpoin);

		if (G.buts->lockpoin) {
			search = G.main->sound.first;
			while (search) {
				if (search == G.buts->lockpoin) {
					break;
				}
				search = search->next;
			}
			if (search == NULL) {
				*id = G.main->sound.first;
			} else {
				*id = search;
			}
		} else {
			*id = G.main->sound.first;
		}
		//  printf("id:    %d\n\n", *id);
	}
}

#if 0
static void validate_bonebutton(void *bonev, void *data2_unused){
	Bone *bone= bonev;
	bArmature *arm;

	arm = get_armature(G.obpose);
	unique_bone_name(bone, arm);
}
#endif

static int bonename_exists(Bone *orig, char *name, ListBase *list)
{
	Bone *curbone;
	
	for (curbone=list->first; curbone; curbone=curbone->next){
		/* Check this bone */
		if (orig!=curbone){
			if (!strcmp(curbone->name, name))
				return 1;
		}
		
		/* Check Children */
		if (bonename_exists(orig, name, &curbone->childbase))
			return 1;
	}
	
	return 0;

}

static void unique_bone_name (Bone *bone, bArmature *arm)
{
	char		tempname[64];
	char		oldname[64];
	int			number;
	char		*dot;

	if (!arm)
		return;

	strcpy(oldname, bone->name);

	/* See if we even need to do this */
	if (!bonename_exists(bone, bone->name, &arm->bonebase))
		return;

	/* Strip off the suffix */
	dot=strchr(bone->name, '.');
	if (dot)
		*dot=0;
	
	for (number = 1; number <=999; number++){
		sprintf (tempname, "%s.%03d", bone->name, number);
		
		if (!bonename_exists(bone, tempname, &arm->bonebase)){
			strcpy (bone->name, tempname);
			return;
		}
	}
}

void buts_buttons(void)
{
	ID *id, *idfrom;
	Object *ob;
	uiBlock *block;
	uiBut *but;
	short xco;
	int alone, local, browse;
	char naam[20];
	short type, t_base= -2;
	void *data=NULL;
	char str[256];

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREY);

	curarea->butspacetype= SPACE_BUTS;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");

	/* HOME */
	uiDefIconBut(block, BUT, B_BUTSHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");
	
	ob= OBACT;

	/* choice menu */

	uiBlockSetCol(block, MIDGREY);
	uiBlockSetEmboss(block, UI_EMBOSST);

	xco+= 2*XIC;
	uiDefIconButS(block, ROW, B_REDR,			ICON_EYE,	xco+=XIC, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_VIEW, 0, 0, "View buttons");
	
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_LAMP,	xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_LAMP, 0, 0, "Lamp buttons (F4)");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_MATERIAL,	xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_MAT, 0, 0, "Material buttons (F5)");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_TEXTURE,	xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_TEX, 0, 0, "Texture buttons (F6)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_ANIM,	xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_ANIM, 0, 0, "Animation buttons (F7)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_GAME,	xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_GAME, 0, 0, "Realtime buttons (F8)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_EDIT,	xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_EDIT, 0, 0, "Edit buttons (F9)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_CONSTRAINT,xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_CONSTRAINT, 0, 0, "Constraint buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_SPEAKER,xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_SOUND, 0, 0, "Sound buttons");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_WORLD,	xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_WORLD, 0, 0, "World buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_PAINT,xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_FPAINT, 0, 0, "Paint buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_RADIO,xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_RADIO, 0, 0, "Radiosity buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_SCRIPT,xco+=30, t_base, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_SCRIPT, 0, 0, "Script buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_SCENE,	xco+=30, t_base, 50, YIC, &(G.buts->mainb), 1.0, (float)BUTS_RENDER, 0, 0, "Display buttons (F10)");
	xco+= 80;

	uiBlockSetCol(block, BUTGREY);
	uiBlockSetEmboss(block, UI_EMBOSSX);
	
	buttons_active_id(&id, &idfrom);
	
	G.buts->lockpoin= id;
	
	if(G.buts->mainb==BUTS_LAMP) {
		if(id) {
			xco= std_libbuttons(block, xco, 0, NULL, B_LAMPBROWSE, id, (ID *)ob, &(G.buts->menunr), B_LAMPALONE, B_LAMPLOCAL, 0, 0, 0);	
		}
	}
	else if(G.buts->mainb==BUTS_MAT) {
		if(ob && (ob->type<OB_LAMP) && ob->type) {
			xco= std_libbuttons(block, xco, 0, NULL, B_MATBROWSE, id, idfrom, &(G.buts->menunr), B_MATALONE, B_MATLOCAL, B_MATDELETE, B_AUTOMATNAME, B_KEEPDATA);
		}
	
		/* COPY PASTE */
		if(curarea->headertype==HEADERTOP) {
			uiDefIconBut(block, BUT, B_MATCOPY, ICON_COPYUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies Material to the buffer");
			uiSetButLock(id && id->lib, "Can't edit library data");
			uiDefIconBut(block, BUT, B_MATPASTE, ICON_PASTEUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes Material from the buffer");
		}
		else {
			uiDefIconBut(block, BUT, B_MATCOPY, ICON_COPYDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies Material to the buffer");
			uiSetButLock(id && id->lib, "Can't edit library data");
			uiDefIconBut(block, BUT, B_MATPASTE, ICON_PASTEDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes Material from the buffer");
		}
		xco+=XIC;

	}
	else if(G.buts->mainb==BUTS_TEX) {
		if(G.buts->texfrom==0) {
			if(idfrom) {
				xco= std_libbuttons(block, xco, 0, NULL, B_TEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
			}
		}
		else if(G.buts->texfrom==1) {
			if(idfrom) {
				xco= std_libbuttons(block, xco, 0, NULL, B_WTEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
			}
		}
		else if(G.buts->texfrom==2) {
			if(idfrom) {
				xco= std_libbuttons(block, xco, 0, NULL, B_LTEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
			}
		}
	}
	else if(G.buts->mainb==BUTS_ANIM) {
		if(id) {
			xco= std_libbuttons(block, xco, 0, NULL, 0, id, idfrom, &(G.buts->menunr), B_OBALONE, B_OBLOCAL, 0, 0, 0);
			
			if(G.scene->group) {
				Group *group= G.scene->group;
				but= uiDefBut(block, TEX, B_IDNAME, "GR:",	xco, 0, 135, YIC, group->id.name+2, 0.0, 19.0, 0, 0, "Displays Active Group name. Click to change.");
				uiButSetFunc(but, test_idbutton_cb, group->id.name, NULL);
				xco+= 135;
			}
		}
	}
	else if(G.buts->mainb==BUTS_GAME) {
		if(id) {
			xco= std_libbuttons(block, xco, 0, NULL, 0, id, idfrom, &(G.buts->menunr), B_OBALONE, B_OBLOCAL, 0, 0, 0);
		}
	}
	else if(G.buts->mainb==BUTS_WORLD) {
		xco= std_libbuttons(block, xco, 0, NULL, B_WORLDBROWSE, id, idfrom, &(G.buts->menunr), B_WORLDALONE, B_WORLDLOCAL, B_WORLDDELETE, 0, B_KEEPDATA);
	}
	else if (G.buts->mainb==BUTS_SOUND) {
		xco= std_libbuttons(block, xco, 0, NULL, B_SOUNDBROWSE2, id, idfrom, &(G.buts->texnr), 1, 0, 0, 0, 0);
	}

	else if(G.buts->mainb==BUTS_RENDER) {
		xco= std_libbuttons(block, xco, 0, NULL, B_INFOSCE, (ID *)G.scene, 0, &(G.curscreen->scenenr), 1, 1, B_INFODELSCE, 0, B_KEEPDATA);
	}
	else if(G.buts->mainb==BUTS_EDIT) {
		if(id) {
			
			alone= 0;
			local= 0;
			browse= B_EDITBROWSE;
			xco+= 10;
			
			if(ob->type==OB_MESH) {
				browse= B_MESHBROWSE;
				alone= B_MESHALONE;
				local= B_MESHLOCAL;
				uiSetButLock(G.obedit!=0, "Unable to perform function in EditMode");
			}
			else if(ob->type==OB_MBALL) {
				alone= B_MBALLALONE;
				local= B_MBALLLOCAL;
			}
			else if ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF) {
				alone= B_CURVEALONE;
				local= B_CURVELOCAL;
			}
			else if(ob->type==OB_CAMERA) {
				alone= B_CAMERAALONE;
				local= B_CAMERALOCAL;
			}
			else if(ob->type==OB_LAMP) {
				alone= B_LAMPALONE;
				local= B_LAMPLOCAL;
			}
			else if (ob->type==OB_ARMATURE){
				alone = B_ARMALONE;
				local = B_ARMLOCAL;
			}
			else if(ob->type==OB_LATTICE) {
				alone= B_LATTALONE;
				local= B_LATTLOCAL;
			}
			
			xco= std_libbuttons(block, xco, 0, NULL, browse, id, idfrom, &(G.buts->menunr), alone, local, 0, 0, B_KEEPDATA);
 				
			xco+= XIC;
		}
		if(ob) {
			but= uiDefBut(block, TEX, B_IDNAME, "OB:",	xco, 0, 135, YIC, ob->id.name+2, 0.0, 19.0, 0, 0, "Displays Active Object name. Click to change.");
			uiButSetFunc(but, test_idbutton_cb, ob->id.name, NULL);
			xco+= 135;
		}
	}
	else if(G.buts->mainb==BUTS_CONSTRAINT){
		if(id) {


			xco= std_libbuttons(block, xco, 0, NULL, 0, id, idfrom, &(G.buts->menunr), B_OBALONE, B_OBLOCAL, 0, 0, 0);

			get_constraint_client(NULL, &type, &data);
			if (data && type==TARGET_BONE){
				sprintf(str, "BO:%s", ((Bone*)data)->name);
#if 0
					/* XXXXX, not idname... but check redrawing */
				but= uiDefBut(block, TEX, 1, "BO:",	xco, 0, 135, YIC, ((Bone*)data)->name, 0.0, 19.0, 0, 0, "Displays Active Bone name. Click to change.");
				uiButSetFunc(but, validate_bonebutton, data, NULL);
#else
				but= uiDefBut(block, LABEL, 1, str,	xco, 0, 135, YIC, ((Bone*)data)->name, 0.0, 19.0, 0, 0, "Displays Active Bone name. Click to change.");
#endif
				xco+= 135;
			}
		}
	}
	else if(G.buts->mainb==BUTS_SCRIPT) {
		if(ob)
			uiDefIconButS(block, ROW, B_REDR, ICON_OBJECT, xco,0,XIC,YIC, &G.buts->scriptblock,  2.0, (float)ID_OB, 0, 0, "Displays Object script links");

		if(ob && give_current_material(ob, ob->actcol))
			uiDefIconButS(block, ROW, B_REDR, ICON_MATERIAL,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_MA, 0, 0, "Displays Material script links ");

		if(G.scene->world) 
			uiDefIconButS(block, ROW, B_REDR, ICON_WORLD,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_WO, 0, 0, "Displays World script links");
	
		if(ob && ob->type==OB_CAMERA)
			uiDefIconButS(block, ROW, B_REDR, ICON_CAMERA,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_CA, 0, 0, "Displays Camera script links");

		if(ob && ob->type==OB_LAMP)
			uiDefIconButS(block, ROW, B_REDR, ICON_LAMP,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_LA, 0, 0, "Displays Lamp script links");

		xco+= 20;
	}
	
	uiDefButS(block, NUM, B_NEWFRAME, "",	(short)(xco+20),0,60,YIC, &(G.scene->r.cfra), 1.0, 18000.0, 0, 0, "Displays Current Frame of animation. Click to change.");
	xco+= 80;

	G.buts->mainbo= G.buts->mainb;

	/* always do as last */
	uiDrawBlock(block);
	curarea->headbutlen= xco;
}

/* ********************** BUTS ****************************** */
/* ******************** FILE ********************** */

void do_file_buttons(short event)
{
	SpaceFile *sfile;
	
	if(curarea->win==0) return;
	sfile= curarea->spacedata.first;
	
	switch(event) {
	case B_SORTFILELIST:
		sort_filelist(sfile);
		scrarea_queue_winredraw(curarea);
		break;
	case B_RELOADDIR:
		freefilelist(sfile);
		scrarea_queue_winredraw(curarea);
		break;
	}
	
}

void file_buttons(void)
{
	SpaceFile *sfile;
	uiBlock *block;
	float df, totlen, sellen;
	short xco;
	int totfile, selfile;
	char naam[256];

	sfile= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREY);

	curarea->butspacetype= SPACE_FILE;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* SORT TYPE */
	xco+=XIC;
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTALPHA,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 0.0, 0, 0, "Sorts files alphabetically");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTTIME,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 1.0, 0, 0, "Sorts files by time");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTSIZE,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 2.0, 0, 0, "Sorts files by size");	

	cpack(0x0);
	glRasterPos2i(xco+=XIC+10,  5);

	BIF_DrawString(uiBlockGetCurFont(block), sfile->title, (U.transopts & TR_BUTTONS), 0);
	xco+= BIF_GetStringWidth(G.font, sfile->title, (U.transopts & TR_BUTTONS));
	
	uiDefIconButS(block, ICONTOG|BIT|0, B_SORTFILELIST, ICON_LONGDISPLAY,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Toggles long info");
	uiDefIconButS(block, TOG|BIT|3, B_RELOADDIR, ICON_GHOST,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Hides dot files");

	xco+=XIC+10;

	if(sfile->type==FILE_LOADLIB) {
		uiDefButS(block, TOGN|BIT|2, B_REDR, "Append",		xco+=XIC,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Copies selected data into current project");
		uiDefButS(block, TOG|BIT|2, B_REDR, "Link",	xco+=100,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Creates a link to selected data from current project");
	}

	if(sfile->type==FILE_UNIX) {
		df= BLI_diskfree(sfile->dir)/(1048576.0);

		filesel_statistics(sfile, &totfile, &selfile, &totlen, &sellen);
		
		sprintf(naam, "Free: %.3f Mb   Files: (%d) %d    (%.3f) %.3f Mb", 
					df, selfile,totfile, sellen, totlen);
		
		cpack(0x0);
		glRasterPos2i(xco,  5);

		BIF_DrawString(uiBlockGetCurFont(block), naam, 0, 0);
	}
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;
	
	uiDrawBlock(block);
}


/* ********************** FILE ****************************** */
/* ******************** OOPS ********************** */

void do_oops_buttons(short event)
{
	float dx, dy;
	
	if(curarea->win==0) return;

	switch(event) {
	case B_OOPSHOME:
		boundbox_oops();
		G.v2d->cur= G.v2d->tot;
		dx= 0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin);
		dy= 0.15*(G.v2d->cur.ymax-G.v2d->cur.ymin);
		G.v2d->cur.xmin-= dx;
		G.v2d->cur.xmax+= dx;
		G.v2d->cur.ymin-= dy;
		G.v2d->cur.ymax+= dy;		
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
		
	case B_NEWOOPS:
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		G.soops->lockpoin= 0;
		break;
	}
	
}

void oops_buttons(void)
{
	SpaceOops *soops;
	Oops *oops;
	uiBlock *block;
	short xco;
	char naam[256];

	soops= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREEN);

	curarea->butspacetype= SPACE_OOPS;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_OOPSHOME, ICON_HOME,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");	
	xco+= XIC;
	
	/* ZOOM and BORDER */
	xco+= XIC;
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	(short)(xco+=XIC),0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area");

	/* VISIBLE */
	xco+= XIC;
	uiDefButS(block, TOG|BIT|10,B_NEWOOPS, "lay",		(short)(xco+=XIC),0,XIC+10,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Objects based on layer");
	uiDefIconButS(block, TOG|BIT|0, B_NEWOOPS, ICON_SCENE_HLT,	(short)(xco+=XIC+10),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Scene data");
	uiDefIconButS(block, TOG|BIT|1, B_NEWOOPS, ICON_OBJECT_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Object data");
	uiDefIconButS(block, TOG|BIT|2, B_NEWOOPS, ICON_MESH_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Mesh data");
	uiDefIconButS(block, TOG|BIT|3, B_NEWOOPS, ICON_CURVE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Curve/Surface/Font data");
	uiDefIconButS(block, TOG|BIT|4, B_NEWOOPS, ICON_MBALL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Metaball data");
	uiDefIconButS(block, TOG|BIT|5, B_NEWOOPS, ICON_LATTICE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lattice data");
	uiDefIconButS(block, TOG|BIT|6, B_NEWOOPS, ICON_LAMP_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Lamp data");
	uiDefIconButS(block, TOG|BIT|7, B_NEWOOPS, ICON_MATERIAL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Material data");
	uiDefIconButS(block, TOG|BIT|8, B_NEWOOPS, ICON_TEXTURE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Texture data");
	uiDefIconButS(block, TOG|BIT|9, B_NEWOOPS, ICON_IPO_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Ipo data");
	uiDefIconButS(block, TOG|BIT|12, B_NEWOOPS, ICON_IMAGE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Image data");
	uiDefIconButS(block, TOG|BIT|11, B_NEWOOPS, ICON_LIBRARY_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Displays Library data");

	/* name */
	if(G.soops->lockpoin) {
		oops= G.soops->lockpoin;
		if(oops->type==ID_LI) strcpy(naam, ((Library *)oops->id)->name);
		else strcpy(naam, oops->id->name);
		
		cpack(0x0);
		glRasterPos2i(xco+=XIC+10,  5);
		BMF_DrawString(uiBlockGetCurFont(block), naam);

	}

	/* always do as last */
	curarea->headbutlen= xco+2*XIC;
	
	uiDrawBlock(block);
}


/* ********************** OOPS ****************************** */
/* ********************** TEXT ****************************** */

void do_text_buttons(unsigned short event)
{
	SpaceText *st= curarea->spacedata.first;
	ID *id, *idtest;
	int nr= 1;
	Text *text;
		
	if (!st) return;
	if (st->spacetype != SPACE_TEXT) return;
	
	switch (event) {
	case B_TEXTBROWSE:
		if (st->menunr==-2) {
			activate_databrowse((ID *)st->text, ID_TXT, 0, B_TEXTBROWSE, &st->menunr, do_text_buttons);
			break;
		}
		if(st->menunr < 0) break;
			
		text= st->text;

		nr= 1;
		id= (ID *)text;
		
		if (st->menunr==32767) {
			st->text= (Text *)add_empty_text();

			st->top= 0;
			
			allqueue(REDRAWTEXT, 0);
			allqueue(REDRAWHEADERS, 0);	
		}
		else if (st->menunr==32766) {
			activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs); 
			return;
		}
		else {		
			idtest= G.main->text.first;
			while(idtest) {
				if(nr==st->menunr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) {	/* new text */
				activate_fileselect(FILE_SPECIAL, "LOAD TEXT FILE", G.sce, add_text_fs); 
				return;
			}
			if(idtest!=id) {
				st->text= (Text *)idtest;
				st->top= 0;
				
				pop_space_text(st);
			
				allqueue(REDRAWTEXT, 0);
				allqueue(REDRAWHEADERS, 0);
			}
		}
		break;
				
	case B_TEXTDELETE:
		
		text= st->text;
		if (!text) return;
		
		BPY_clear_bad_scriptlinks(text);
		free_text_controllers(text);
		
		unlink_text(text);
		free_libblock(&G.main->text, text);
		
		break;
		
/*
	case B_TEXTSTORE:
		st->text->flags ^= TXT_ISEXT;
		
		allqueue(REDRAWHEADERS, 0);
		break;
*/		 
	case B_TEXTLINENUM:
		if(st->showlinenrs)
			st->showlinenrs = 0;
		else
			st->showlinenrs = 1;

		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);
		break;

	case B_TEXTFONT:
		switch(st->font_id) {
		case 0:
			st->lheight= 12; break;
		case 1:
			st->lheight= 15; break;
		}

		allqueue(REDRAWTEXT, 0);
		allqueue(REDRAWHEADERS, 0);

		break;
	}
}

void text_buttons(void)
{
	uiBlock *block;
	SpaceText *st= curarea->spacedata.first;
	short xco;
	char naam[256];
	
	if (!st || st->spacetype != SPACE_TEXT) return;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREY);

	curarea->butspacetype= SPACE_TEXT;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
		
	if(st->showlinenrs)
		uiDefIconBut(block, BUT, B_TEXTLINENUM, ICON_SHORTDISPLAY, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Hides line numbers");
	else
		uiDefIconBut(block, BUT, B_TEXTLINENUM, ICON_LONGDISPLAY, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Displays line numbers");


	/* STD TEXT BUTTONS */
	if (!BPY_spacetext_is_pywin(st)) {
		xco+= 2*XIC;
		xco= std_libbuttons(block, xco, 0, NULL, B_TEXTBROWSE, (ID*)st->text, 0, &(st->menunr), 0, 0, B_TEXTDELETE, 0, 0);

		/*
		if (st->text) {
			if (st->text->flags & TXT_ISDIRTY && (st->text->flags & TXT_ISEXT || !(st->text->flags & TXT_ISMEM)))
				uiDefIconBut(block, BUT,0, ICON_ERROR, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "The text has been changed");
			if (st->text->flags & TXT_ISEXT) 
				uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Stores text in project file");
			else 
				uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Disables storing of text in project file");
			xco+=10;
		}
		*/		

		xco+=XIC;
		if(st->font_id>1) st->font_id= 0;
		uiDefButI(block, MENU, B_TEXTFONT, "Screen 12 %x0|Screen 15%x1", xco,0,100,YIC, &st->font_id, 0, 0, 0, 0, "Displays available fonts");
		xco+=100;
	}
	
	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}



/* ******************** TEXT  ********************** */
/* ******************** SOUND ********************** */

void load_space_sound(char *str)	/* called from fileselect */
{
	bSound *sound;
	
	sound= sound_new_sound(str);
	if (sound) {
		if (G.ssound) {
			G.ssound->sound= sound;
		}
	} else {
		error("Not a valid sample: %s", str);
	}

	allqueue(REDRAWSOUND, 0);
	allqueue(REDRAWBUTSGAME, 0);
}


void load_sound_buttons(char *str)	/* called from fileselect */
{
	bSound *sound;
	
	sound= sound_new_sound(str);
	if (sound) {
		if (curarea && curarea->spacetype==SPACE_BUTS) {
			if (G.buts->mainb == BUTS_SOUND) {
				G.buts->lockpoin = sound;
			}
		}
	} else {
		error("Not a valid sample: %s", str);
	}

	allqueue(REDRAWBUTSSOUND, 0);
}

void do_action_buttons(unsigned short event)
{

	switch(event){
#ifdef __NLA_BAKE
	case B_ACTBAKE:
		bake_action_with_client (G.saction->action, OBACT, 0.01);
		break;
#endif
	case B_ACTCONT:
		set_exprap_action(IPO_HORIZ);
		break;
//	case B_ACTEXTRAP:
//		set_exprap_ipo(IPO_DIR);
//		break;
	case B_ACTCYCLIC:
		set_exprap_action(IPO_CYCL);
		break;
//	case B_ACTCYCLICX:
//		set_exprap_ipo(IPO_CYCLX);
//		break;
	case B_ACTHOME:
		//	Find X extents
		//v2d= &(G.saction->v2d);

		G.v2d->cur.xmin = 0;
		G.v2d->cur.ymin=-SCROLLB;
		
		if (!G.saction->action){	// here the mesh rvk?
			G.v2d->cur.xmax=100;
		}
		else {
			float extra;
			G.v2d->cur.xmin= calc_action_start(G.saction->action);
			G.v2d->cur.xmax= calc_action_end(G.saction->action);
			extra= 0.05*(G.v2d->cur.xmax - G.v2d->cur.xmin);
			G.v2d->cur.xmin-= extra;
			G.v2d->cur.xmax+= extra;
		}

		G.v2d->tot= G.v2d->cur;
		test_view2d(G.v2d, curarea->winx, curarea->winy);


		addqueue (curarea->win, REDRAW, 1);

		break;
	case B_ACTCOPY:
		copy_posebuf();
		allqueue(REDRAWVIEW3D, 1);
		break;
	case B_ACTPASTE:
		paste_posebuf(0);
		allqueue(REDRAWVIEW3D, 1);
		break;
	case B_ACTPASTEFLIP:
		paste_posebuf(1);
		allqueue(REDRAWVIEW3D, 1);
		break;

	case B_ACTPIN:	/* __PINFAKE */
/*		if (G.saction->flag & SACTION_PIN){
			if (G.saction->action)
				G.saction->action->id.us ++;

		}
		else {
			if (G.saction->action)
				G.saction->action->id.us --;
		}
*/		/* end PINFAKE */
		allqueue(REDRAWACTION, 1);
		break;

	}
}

void do_sound_buttons(unsigned short event)
{
	ID *id, *idtest;
	int nr;
	char name[256];
	
	switch(event) {

	case B_SOUNDBROWSE:	
		if(G.ssound->sndnr== -2) {
			activate_databrowse((ID *)G.ssound->sound, ID_SO, 0, B_SOUNDBROWSE, &G.ssound->sndnr, do_sound_buttons);
			return;
		}
		if (G.ssound->sndnr < 0) break;
		if (G.ssound->sndnr == 32766) {
			if (G.ssound && G.ssound->sound) strcpy(name, G.ssound->sound->name);
			else strcpy(name, U.sounddir);
			activate_fileselect(FILE_SPECIAL, "SELECT WAV FILE", name, load_space_sound);
		} else {
			nr= 1;
			id= (ID *)G.ssound->sound;

			idtest= G.main->sound.first;
			while(idtest) {
				if(nr==G.ssound->sndnr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}

			if(idtest==0) {	/* no new */
				return;
			}
		
			if(idtest!=id) {
				G.ssound->sound= (bSound *)idtest;
				if(idtest->us==0) idtest->us= 1;
				allqueue(REDRAWSOUND, 0);
			}
		}

		break;
	case B_SOUNDBROWSE2:	
		id = (ID *)G.buts->lockpoin;
		if(G.buts->texnr == -2) {
			activate_databrowse(id, ID_SO, 0, B_SOUNDBROWSE2, &G.buts->texnr, do_sound_buttons);
			return;
		}
		if (G.buts->texnr < 0) break;
		if (G.buts->texnr == 32766) {
			if (id) strcpy(name, ((bSound *)id)->name);
			else strcpy(name, U.sounddir);
			activate_fileselect(FILE_SPECIAL, "SELECT WAV FILE", name, load_sound_buttons);
		} else {
			nr= 1;

			idtest= G.main->sound.first;
			while (idtest) {
				if(nr == G.buts->texnr) {
					break;
				}
				nr++;
				idtest = idtest->next;
			}

			if (idtest == 0) {	/* geen new */
				return;
			}
		
			if (idtest != id) {
				G.buts->lockpoin = (bSound *)idtest;
				if(idtest->us==0) idtest->us= 1;
				allqueue(REDRAWBUTSSOUND, 0);
				BIF_preview_changed(G.buts);
			}
		}

		break;
	case B_SOUNDHOME:
	
		G.v2d->cur= G.v2d->tot;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		scrarea_queue_winredraw(curarea);
		break;
	}
}

void sound_buttons(void)
{
	uiBlock *block;
	short xco;
	char naam[256];
	char ch[20];
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTYELLOW);

	curarea->butspacetype= SPACE_SOUND;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_SOUNDHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");

	xco= std_libbuttons(block, xco+40, 0, NULL, B_SOUNDBROWSE, (ID *)G.ssound->sound, 0, &(G.ssound->sndnr), 1, 0, 0, 0, 0);	

	if(G.ssound->sound) {
		bSound *sound= G.ssound->sound;

		if (sound->sample && sound->sample->len)
		{
			if (sound->sample->channels == 1)
				strcpy(ch, "Mono");
			else if (sound->sample->channels == 2)
				strcpy(ch, "Stereo");
			else
				strcpy(ch, "Unknown");
			
			sprintf(naam, "Sample: %s, %d bit, %d Hz, %d samples", ch, sound->sample->bits, sound->sample->rate, sound->sample->len);
			cpack(0x0);
			glRasterPos2i(xco+10, 5);
			BMF_DrawString(uiBlockGetCurFont(block), naam);
		}
		else
		{
			sprintf(naam, "No sample info available.");
			cpack(0x0);
			glRasterPos2i(xco+10, 5);
			BMF_DrawString(uiBlockGetCurFont(block), naam);
		}
		
	}

	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}


/* ******************** SOUND  ********************** */
/* ******************** IMAGE ********************** */

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
					allqueue(REDRAWBUTSTEX, 0);
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
			activate_databrowse((ID *)G.sima->image, ID_IM, 0, B_SIMABROWSE, &G.sima->imanr, do_image_buttons);
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
		if(idtest==0) {	/* no new */
			return;
		}
	
		if(idtest!=id) {
			G.sima->image= (Image *)idtest;
			if(idtest->us==0) idtest->us= 1;
			allqueue(REDRAWIMAGE, 0);
		}
		image_changed(G.sima, 0);	/* also when image is the same: assign! 0==no tileflag */
		
		break;
	case B_SIMAGELOAD:
	case B_SIMAGELOAD1:
		
		if(G.sima->image) strcpy(name, G.sima->image->name);
		else strcpy(name, U.textudir);
		
		if(event==B_SIMAGELOAD)
			activate_imageselect(FILE_SPECIAL, "SELECT IMAGE", name, load_space_image);
		else
			activate_fileselect(FILE_SPECIAL, "SELECT IMAGE", name, load_space_image);
		break;
	case B_SIMAGEREPLACE:
	case B_SIMAGEREPLACE1:
		
		if(G.sima->image) strcpy(name, G.sima->image->name);
		else strcpy(name, U.textudir);
		
		if(event==B_SIMAGEREPLACE)
			activate_imageselect(FILE_SPECIAL, "REPLACE IMAGE", name, replace_space_image);
		else
			activate_fileselect(FILE_SPECIAL, "REPLACE IMAGE", name, replace_space_image);
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
			allqueue(REDRAWBUTSTEX, 0);
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

	xco= std_libbuttons(block, xco+40, 0, NULL, B_SIMABROWSE, (ID *)G.sima->image, 0, &(G.sima->imanr), 0, 0, B_IMAGEDELETE, 0, 0);

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


/* ********************** IMAGE ****************************** */
/* ******************** IMASEL ********************** */

void do_imasel_buttons(short event)
{
	SpaceImaSel *simasel;
	char name[256];
	
	simasel= curarea->spacedata.first;
	
	if(curarea->win==0) return;

	switch(event) {
	case B_IMASELHOME:
		break;
		
	case B_IMASELREMOVEBIP:
		
		if(bitset(simasel->fase, IMS_FOUND_BIP)){
		
			strcpy(name, simasel->dir);
			strcat(name, ".Bpib");
		
			remove(name);
		
			simasel->fase &= ~ IMS_FOUND_BIP;
		}
		break;
	}
}

void imasel_buttons(void)
{
	SpaceImaSel *simasel;
	uiBlock *block;
	short xco;
	char naam[256];
	
	simasel= curarea->spacedata.first;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTBLUE);

	curarea->butspacetype= SPACE_IMASEL;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
	
	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "");
	
	xco+=XIC;
	if (simasel->title){
		xco+=25;
		glRasterPos2i(xco,  4);
		BMF_DrawString(G.font, simasel->title);
		xco+=BMF_GetStringWidth(G.fonts, simasel->title);
		xco+=25;
	}
	uiDefIconBut(block, BUT, B_IMASELREMOVEBIP, ICON_BPIBFOLDER_X, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "");/* remove  */
	
	uiDefIconButS(block, TOG|BIT|0, B_REDR, ICON_BPIBFOLDERGREY, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "Toggles display of directory information");/* dir   */
	uiDefIconButS(block, TOG|BIT|1, B_REDR, ICON_INFO, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "Toggles display of selected image information");/* info  */
	uiDefIconButS(block, TOG|BIT|2, B_REDR, ICON_IMAGE_COL, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "");/* image */
	uiDefIconButS(block, TOG|BIT|3, B_REDR, ICON_MAGNIFY, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "Toggles magnified view of thumbnail of images under mouse pointer");/* magnify */
	
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}

/* ********************** IMASEL ****************************** */

/* ******************** GENERAL ********************** */

void do_headerbuttons(short event)
{

	if(event<=50) do_global_buttons2(event);
	else if(event<=100) do_global_buttons(event);
	else if(event<200) do_view3d_buttons(event);
	else if(event<250) do_ipo_buttons(event);
	else if(event<300) do_oops_buttons(event);
	else if(event<350) do_info_buttons(event);
	else if(event<400) do_image_buttons(event);
	else if(event<450) do_buts_buttons(event);
	else if(event<500) do_imasel_buttons(event);
	else if(event<550) do_text_buttons(event);
	else if(event<600) do_file_buttons(event);
	else if(event<650) do_seq_buttons(event);
	else if(event<700) do_sound_buttons(event);
	else if(event<800) do_action_buttons(event);
	else if(event<900) do_nla_buttons(event);
}

