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
static void validate_bonebutton(void *data1, void *data2);
static int bonename_exists(Bone *orig, char *name, ListBase *list);

static 	void test_idbutton_cb(void *namev, void *arg2_unused)
{
	char *name= namev;
	test_idbutton(name+2);
}

#define SPACEICONMAX	13 /* See release/datafiles/blenderbuttons */

#include "BIF_poseobject.h"

#include "SYS_System.h"

static int std_libbuttons(uiBlock *block, 
						  int xco, int pin, short *pinpoin, 
						  int browse, ID *id, ID *parid, 
						  short *menupoin, int users, 
						  int lib, int del, int autobut, int keepbut);


extern char versionstr[]; /* from blender.c */
/*  extern                void add_text_fs(char *file);  *//* from text.c, BIF_text.h*/

 /* LET OP:  alle headerbuttons voor zelfde window steeds zelfde naam
  *			event B_REDR is standaard redraw
  *
  */


/*
 * The next define turns the newest menu structure on.
 * There are some loose ends here at the moment so leave this undefined for now.
 */
/* #define EXPERIMENTAL_MENUS */


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
		uiDefIconButS(block, ICONTOG, pin, ICON_PIN_DEHLT,	(short)xco,0,XIC,YIC, pinpoin, 0, 0, 0, 0, "Pin this data block; no update according Object selection");
		xco+= XIC;
	}
	if(browse) {
		if(id==0) {
			idwasnul= 1;
			/* alleen de browse button */
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
				/* testen op ipotype */
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
			
			uiDefButS(block, MENU, browse, str, (short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browse Datablock or Add NEW");
			
			uiClearButLock();
		
			MEM_freeN(str);
			xco+= XIC;
		}
		else if(curarea->spacetype==SPACE_BUTS) {
			if ELEM3(G.buts->mainb, BUTS_MAT, BUTS_TEX, BUTS_WORLD) {
				uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
				if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");
				uiDefButS(block, MENU, browse, "ADD NEW %x 32767",(short) xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browse Datablock");
				uiClearButLock();
			} else if (G.buts->mainb == BUTS_SOUND) {
				uiDefButS(block, MENU, browse, "OPEN NEW %x 32766",(short) xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browse Datablock");
			}
		}
		else if(curarea->spacetype==SPACE_TEXT) {
			uiDefButS(block, MENU, browse, "OPEN NEW %x 32766 | ADD NEW %x 32767", (short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browse Datablock");
		}
		else if(curarea->spacetype==SPACE_SOUND) {
			uiDefButS(block, MENU, browse, "OPEN NEW %x 32766",(short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browse Datablock");
		}
		else if(curarea->spacetype==SPACE_NLA) {
		}
		else if(curarea->spacetype==SPACE_ACTION) {
			uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
			if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");

			uiDefButS(block, MENU, browse, "ADD NEW %x 32767", xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browse Datablock");
			uiClearButLock();
		}
		else if(curarea->spacetype==SPACE_IPO) {
			uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
			if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");

			uiDefButS(block, MENU, browse, "ADD NEW %x 32767", (short)xco,0,XIC,YIC, menupoin, 0, 0, 0, 0, "Browse Datablock");
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
		
		but= uiDefBut(block, TEX, B_IDNAME, str1,(short)xco, 0, (short)len, YIC, id->name+2, 0.0, 19.0, 0, 0, "Datablock name");
		uiButSetFunc(but, test_idbutton_cb, id->name, NULL);

		uiClearButLock();

		xco+= len;
		
		if(id->lib) {
			
			if(parid && parid->lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Indirect Library Datablock");
			else uiDefIconBut(block, BUT, lib, ICON_PARLIB,	(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Library DataBlock, press to make local");
			
			xco+= XIC;
		}
		
		
		if(users && id->us>1) {
			uiSetButLock (pin && *pinpoin, "Can't make pinned data single-user");
			
			sprintf(str1, "%d", id->us);
			if(id->us<100) {
				
				uiDefBut(block, BUT, users, str1,	(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Number of users,  press to make single-user");
				xco+= XIC;
			}
			else {
				uiDefBut(block, BUT, users, str1,	(short)xco, 0, XIC+10, YIC, 0, 0, 0, 0, 0, "Number of users,  press to make single-user");
				xco+= XIC+10;
			}
			
			uiClearButLock();
			
		}
		
		if(del) {

			uiSetButLock (pin && *pinpoin, "Can't unlink pinned data");
			if(parid && parid->lib);
			else {
				uiDefIconBut(block, BUT, del, ICON_X,	(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Delete link to this Datablock");
				xco+= XIC;
			}

			uiClearButLock();
		}

		if(autobut) {
			if(parid && parid->lib);
			else {
				uiDefIconBut(block, BUT, autobut, ICON_AUTO,(short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Automatic name");
				xco+= XIC;
			}
			
			
		}
		if(keepbut) {
			uiDefBut(block, BUT, keepbut, "F", (short)xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Keep Datablock");	
			xco+= XIC;
		}
	}
	else xco+=XIC;
	
	uiBlockSetCol(block, oldcol);

	return xco;
}

void update_for_newframe(void)
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
	
	id= 0;	/* id op nul voor texbrowse */
	

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
				if(la && ob->type==OB_LAMP) {	/* voor zekerheid */
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
					if(seq->type & SEQ_EFFECT) {
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
		/* geen lock */
			
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
		/* geen lock */
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
		if(idtest==0) {	/* geen new lamp */
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
	case B_LOADTEMP: 	/* is button uit space.c */
		BIF_read_autosavefile();
		break;

	case B_USERPREF:
		allqueue(REDRAWINFO, 0);
//		BIF_printf("userpref %d\n", U.userpref);
		break;
	case B_DRAWINFO: 	/* is button uit space.c  *info* */
		allqueue(REDRAWVIEW3D, 0);
		break;

	/* Fileselect windows for user preferences file paths */

	case B_FONTDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT FONT PATH", U.fontdir, filesel_u_fontdir);
		break;

	case B_TEXTUDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEXTURE PATH", U.textudir, filesel_u_textudir);
		break;
	
	case B_PLUGTEXDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEX PLUGIN PATH", U.plugtexdir, filesel_u_plugtexdir);
		break;
	
	case B_PLUGSEQDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SEQ PLUGIN PATH", U.plugseqdir, filesel_u_plugseqdir);
		break;
	
	case B_RENDERDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT RENDER PATH", U.renderdir, filesel_u_renderdir);
		break;
	
	case B_PYTHONDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SCRIPT PATH", U.pythondir, filesel_u_pythondir);
		break;

	case B_SOUNDDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SOUND PATH", U.sounddir, filesel_u_sounddir);
		break;

	case B_TEMPDIRFILESEL: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEMP FILE PATH", U.tempdir, filesel_u_tempdir);
		break;

	/* END Fileselect windows for user preferences file paths */


#ifdef INTERNATIONAL
	case B_LOADUIFONT: 	/* is button uit space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}
		BLI_make_file_string("/", buf, U.fontdir, U.fontname);
		activate_fileselect(FILE_SPECIAL, "LOAD UI FONT", buf, set_interface_font);
		break;

	case B_SETLANGUAGE: 	/* is button uit space.c  *info* */
		lang_setlanguage();
		allqueue(REDRAWALL, 0);
		break;

	case B_SETFONTSIZE: 	/* is button uit space.c  *info* */
		FTF_SetSize(U.fontsize);
		allqueue(REDRAWALL, 0);
		break;
		
	case B_SETTRANSBUTS: 	/* is button uit space.c  *info* */
		allqueue(REDRAWALL, 0);
		break;

	case B_SETENCODING: 	/* is button uit space.c  *info* */
		lang_setencoding();
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
			
		/* redraw omdat naam veranderd is: nieuwe pup */
		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWBUTSHEAD, 0);
		allqueue(REDRAWINFO, 1);
		allqueue(REDRAWOOPS, 1);
		/* naam scene ook in set PUPmenu */
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

	/* algemeen: Single User mag als from==LOCAL 
	 *			 Make Local mag als (from==LOCAL && id==LIB)
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
		if (ob)
			act= ob->action;
		
		if(ob && ob->id.lib==0) {
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
	/* level 0: alle objects shared
	 * level 1: alle objectdata shared
	 * level 2: volledige kopie
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
		/* laatste item: NEW SCREEN */
		if(sc==0) {
			duplicate_screen();
		}
		break;
	case B_INFODELSCR:
		/* dit event alleen met buttons doen, zodoende nooit vanuit full aanroepbaar */

		if(G.curscreen->id.prev) sc= G.curscreen->id.prev;
		else if(G.curscreen->id.next) sc= G.curscreen->id.next;
		else return;
		if(okee("Delete current screen")) {
			/* vind nieuwe G.curscreen */
			
			oldscreen= G.curscreen;
			setscreen(sc);		/* deze test of sc een full heeft */
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
		/* laatste item: NEW SCENE */
		if(sce==0) {
			nr= pupmenu("Add scene%t|Empty|Link Objects|Link ObData|Full Copy");
			if(nr<= 0) return;
			if(nr==1) {
				sce= add_scene(G.scene->id.name+2);
				sce->r= G.scene->r;
			}
			else sce= copy_scene(G.scene, nr-2);
			
			set_scene(sce);
		}

		break;
	case B_INFODELSCE:
		
		if(G.scene->id.prev) sce= G.scene->id.prev;
		else if(G.scene->id.next) sce= G.scene->id.next;
		else return;
		if(okee("Delete current scene")) {
			
			/* alle sets aflopen */
			sce1= G.main->scene.first;
			while(sce1) {
				if(sce1->set == G.scene) sce1->set= 0;
				sce1= sce1->id.next;
			}
			
			/* alle sequences aflopen */
			clear_scene_in_allseqs(G.scene);
			
			/* alle schermen */
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
		if (okee("ERASE ALL")) {
			if (!BIF_read_homefile())
				error("No file ~/.B.blend");
		}
		break;
	case 1:
		activate_fileselect(FILE_BLENDER, "LOAD FILE", G.sce, BIF_read_file);
		break;
	case 2:
		{
			char *s= MEM_mallocN(strlen(G.sce) + 11 + 1, "okee_reload");
			strcpy(s, "Open file: ");
			strcat(s, G.sce);
			if (okee(s))
				BIF_read_file(G.sce);
			MEM_freeN(s);
		}
		break;
	case 3:
		activate_fileselect(FILE_LOADLIB, "LOAD LIBRARY", G.lib, 0);
		break;
	case 4:
		strcpy(dir, G.sce);
		untitled(dir);
		activate_fileselect(FILE_BLENDER, "SAVE FILE", dir, BIF_write_file);
		break;
	case 5:
		strcpy(dir, G.sce);
		if (untitled(dir)) {
			activate_fileselect(FILE_BLENDER, "SAVE FILE", dir, BIF_write_file);
		} else {
			BIF_write_file(dir);
			free_filesel_spec(dir);
		}
		break;
	case 6:
		mainqenter(F3KEY, 1);
		break;
	case 7:
		write_vrml_fs();
		break;
	case 8:
		write_dxf_fs();
		break;
	case 9:
		write_videoscape_fs();
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
	case 22:
		activate_fileselect(FILE_SPECIAL, "WRITE RUNTIME", "", write_runtime_check);
		break;
	case 23:
		activate_fileselect(FILE_SPECIAL, "WRITE DYNAMIC RUNTIME", "", write_runtime_check_dynamic);
		break;
	case 30:
		// import menu, no handling
		break;

#ifdef EXPERIMENTAL_MENUS
	case 10:
		check_packAll();
		break;
	case 11:
		unpackAll(PF_WRITE_LOCAL);
		G.fileflags &= ~G_AUTOPACK;
		break;
	case 12:
		if (buttons_do_unpack() != RET_CANCEL) {
			/* Clear autopack bit only if user selected one of the unpack options */
			G.fileflags &= ~G_AUTOPACK;
		}
		break;
	case 13:
#else /* EXPERIMENTAL_MENUS */
	case 10:
#endif /* EXPERIMENTAL_MENUS */
		exit_usiblender();
		break;		
	}
	allqueue(REDRAWINFO, 0);
}

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

	/* flags are case-values */
	uiDefBut(block, BUTM, 1, "Compress File",	xco, yco-=20, 100, 19, NULL, 0.0, 0.0, 0, G_FILE_COMPRESS_BIT, "Use file compression");
/*
	uiDefBut(block, BUTM, 1, "Sign File",	xco, yco-=20, 100, 19, NULL, 0.0, 0.0, 0, G_FILE_SIGN_BIT, "Add signature to file");
	uiDefBut(block, BUTM, 1, "Lock File",	xco, yco-=20, 100, 19, NULL, 0.0, 0.0, 0, G_FILE_LOCK_BIT, "Protect the file from editing by others");
*/
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

	uiBlockSetEmboss(block, UI_EMBOSSW);

	uiDefBut(block, LABEL, 0, "Size options:",		xco, yco-=20, 114, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, NUM, 0, "X:",		xco+19, yco-=20, 95, 19,    &G.scene->r.xplay, 10.0, 2000.0, 0, 0, "X screen/window resolution");
	uiDefButS(block, NUM, 0, "Y:",		xco+19, yco-=20, 95, 19, &G.scene->r.yplay, 10.0, 2000.0, 0, 0, "Y screen/window resolution");

	uiDefBut(block, SEPR, 0, "",		xco, yco-=4, 114, 4, NULL, 0.0, 0.0, 0, 0, "");

	uiDefBut(block, LABEL, 0, "Fullscreen options:",		xco, yco-=20, 114, 19, 0, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, TOG, 0, "Fullscreen", xco + 19, yco-=20, 95, 19, &G.scene->r.fullscreen, 0.0, 0.0, 0, 0, "Starts player in a new fullscreen display");
	uiDefButS(block, NUM, 0, "Freq:",	xco+19, yco-=20, 95, 19, &G.scene->r.freqplay, 10.0, 120.0, 0, 0, "Clock frequency of fullscreen display");
	uiDefButS(block, NUM, 0, "Bits:",	xco+19, yco-=20, 95, 19, &G.scene->r.depth, 1.0, 32.0, 0, 0, "Bit depth of full screen disply");

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
	uiDefButS(block, ROW, 0, "h/w pageflip", xco+19, yco-=20, 95, 19, &(G.scene->r.stereomode), 6.0, 2.0, 0, 0, "Enables h/w pageflip stereo method");
	uiDefButS(block, ROW, 0, "syncdoubling", xco+19, yco-=20, 95, 19, &(G.scene->r.stereomode), 6.0, 3.0, 0, 0, "Enables syncdoubling stereo method");
#if 0
	// future
	uiDefButS(block, ROW, 0, "syncdoubling", xco+19, yco, 95, 19, &(G.scene->r.stereomode), 5.0, 4.0, 0, 0, "Enables interlaced stereo method");
#endif

	uiBlockSetDirection(block, UI_RIGHT);
		
	return block;
}

static uiBlock *info_file_importmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, xco = 20;

	block= uiNewBlock(&curarea->uiblocks, "importmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetXOfs(block, -40);  // offset to parent button

	uiBlockSetEmboss(block, UI_EMBOSSW);

	/* flags are defines */
	uiDefBut(block, LABEL, 0, "VRML 2.0 options", xco, yco, 125, 19,   NULL, 0.0, 0.0, 0, 0, "");
	uiDefButS(block, TOG|BIT|0, 0, "SepLayers", xco, yco-=20, 75, 19,		  &U.vrmlflag, 0.0, 0.0, 0, 0, "Separate Empties, Lamps, etc. into Layers");
	uiDefButS(block, TOG|BIT|1, 0, "Scale 1/100", xco, yco-=20, 75, 19,   &U.vrmlflag, 0.0, 0.0, 0, 0, "Scale scene by 1/100 (3DS VRML)");
	uiDefButS(block, TOG|BIT|2, 0, "Two Sided", xco, yco-=20, 75, 19,   &U.vrmlflag, 0.0, 0.0, 0, 0, "Import two sided faces");

	uiBlockSetDirection(block, UI_RIGHT);
		
	return block;
}

static uiBlock *info_filemenu(void *arg_unused)
{
	uiBlock *block;
	short xco=0;

	block= uiNewBlock(&curarea->uiblocks, "filemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_filemenu, NULL);
	
	uiDefBut(block, BUTM, 1, "New|Ctrl X",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "Start a new project (and delete the current)");
	uiDefBut(block, BUTM, 1, "Open|F1",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "Open a new file");
	uiDefBut(block, BUTM, 1, "Reopen Last|Ctrl O",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "Revert to the last version saved to file");
	uiDefBut(block, BUTM, 1, "Append|Shift F1",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "Append contents of a file to the current project");
	uiDefBlockBut(block, info_file_importmenu, NULL, "Import Settings|>>", 0, xco-=20, 160, 19, "");

	uiDefBut(block, SEPR, 0, "",						0, xco-=6, 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Save As|F2",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "Save to a new file");
	uiDefBut(block, BUTM, 1, "Save|Ctrl W",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 5, "Save to the current file");

	uiDefBlockBut(block, info_file_optionsmenu, NULL, "File options|>>", 0, xco-=20, 160, 19, "Click to open the File Options menu");

	uiDefBut(block, SEPR, 0, "",						0, xco-=6, 160, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefBut(block, BUTM, 1, "Save Runtime",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 22, "Create a runtime executable with the current project");
#ifdef _WIN32
	uiDefBut(block, BUTM, 1, "Save dynamic Runtime",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 23, "Create a dynamic runtime executable with the current project (requieres extenal python20.dll)");
#endif
	uiDefBlockBut(block, info_runtime_optionsmenu, NULL, "Runtime options|>>", 0, xco-=20, 160, 19, "Click to open the File Options menu");

	uiDefBut(block, SEPR, 0, "",						0, xco-=6, 160, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Save Image|F3",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 6, "Save the image in the render buffer to a file");
	uiDefBut(block, BUTM, 1, "Save VRML 1.0|Ctrl F2",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 7, "Save the current scene to a file in VRML 1.0 format");
	uiDefBut(block, BUTM, 1, "Save DXF|Shift F2",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 8, "Save the current scene to a file in DXF format");
	uiDefBut(block, BUTM, 1, "Save VideoScape|Alt W",	0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 9, "Save the current scene to a file in VideoScape format");


	/*
	if (LICENSE_KEY_VALID) {
		uiDefBut(block, SEPR, 0, "",						0, xco-=6, 160, 6, NULL, 0.0, 0.0, 1, 0, "");
		uiDefBut(block, BUTM, 1, "Show License Key", 0, xco-=20,	140, 19, NULL, 0.0, 0.0, 1, 21, "Show the personal information stored in your Blender License Key");
		uiDefIconBut(block, BUTM, 1, ICON_PUBLISHER,			 141,xco,	19,  19, NULL, 0.0, 0.0, 1, 21, "Show the personal information stored in your Blender License Key");
	} else if (I_AM_PUBLISHER) {
		uiDefBut(block, SEPR, 0, "",						0, xco-=6, 160, 6, NULL, 0.0, 0.0, 1, 0, "");
		uiDefBut(block, BUTM, 1, "Install License Key", 0, xco-=20,	140, 19, NULL, 0.0, 0.0, 1, 20, "Install your Blender License Key");
		uiDefIconBut(block, BUTM, 1, ICON_PUBLISHER,			 141,xco,	19,  19, NULL, 0.0, 0.0, 1, 20, "Install your Blender License Key");
	}
	*/


	uiDefBut(block, SEPR, 0, "",						0, xco-=6, 160, 6, NULL, 0.0, 0.0, 1, 0, "");

#ifdef EXPERIMENTAL_MENUS
	uiDefBut(block, BUTM, 1, "Pack Data",						0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 10, "");
	uiDefBut(block, BUTM, 1, "Unpack Data to current dir",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 11, "");
	uiDefBut(block, BUTM, 1, "Advanced Unpack",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 12, "");
	uiDefBut(block, SEPR, 0, "",						0, xco-=6, 160, 6, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Quit | Q",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 13, "Quit Blender immediately");
#else /* EXPERIMENTAL_MENUS */
	uiDefBut(block, BUTM, 1, "Quit | Q",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 10, "Quit Blender immediately");
#endif /* EXPERIMENTAL_MENUS */
	uiBlockSetDirection(block, UI_DOWN);

	
	return block;
}

static void do_info_editmenu(void *arg, int event)
{
	int oldqual;

	switch(event) {
		
	case 0:
		/* (De)Select All */
		if(select_area(SPACE_VIEW3D)) mainqenter(AKEY, 1);
		break;
		/* Border Select */
	case 1:
		if(select_area(SPACE_VIEW3D)) mainqenter(BKEY, 1);
		break;
	case 2:
		/* Circle Select */
		/*if(select_area(SPACE_VIEW3D)) {
			;
		}*/
		break;
	case 3:
		/* Duplicate */
		if(select_area(SPACE_VIEW3D)) {
			duplicate_context_selected();
		}
		break;
	case 4:
		/* Delete */
		if(select_area(SPACE_VIEW3D)) {
			delete_context_selected();
		}
		break;
	case 5:
		/* Edit Mode */
		if(select_area(SPACE_VIEW3D)) {
			blenderqread(TABKEY, 1);
		}
		break;
	case 6:
		/* Grabber */
		if(select_area(SPACE_VIEW3D)) {
			transform('g');
		}
		break;
	case 7:
		/* Rotate */
		if(select_area(SPACE_VIEW3D)) {
			transform('r');
		}
		break;
	case 8:
		/* Scale */
		if(select_area(SPACE_VIEW3D)) {
			transform('s');
		}
		break;
	case 9:
		/* Shear */
		if (!G.obedit) {
			enter_editmode();
			/* ### put these into a deselectall_gen() */
			if(G.obedit->type==OB_MESH) deselectall_mesh();
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) deselectall_nurb();
			else if(G.obedit->type==OB_MBALL) deselectall_mball();
			else if(G.obedit->type==OB_LATTICE) deselectall_Latt();
			/* ### */
		}	
		if(select_area(SPACE_VIEW3D)) {
			transform('S');
		}
		break;
	case 10:
		/* Warp/Bend */
		if (!G.obedit) {
			enter_editmode();
			/* ### put these into a deselectall_gen() */
			if(G.obedit->type==OB_MESH) deselectall_mesh();
			else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) deselectall_nurb();
			else if(G.obedit->type==OB_MBALL) deselectall_mball();
			else if(G.obedit->type==OB_LATTICE) deselectall_Latt();
			/* ### */
		}	
		if(select_area(SPACE_VIEW3D)) {
			transform('w');
		}
		break;
	case 11:
		/* Snap */
		if(select_area(SPACE_VIEW3D)) {
			snapmenu();
		}
		break;
	}
	allqueue(REDRAWINFO, 0);
}


static uiBlock *info_editmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "editmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_editmenu, NULL);

	uiDefBut(block, BUTM, 1, "(De)Select All|A",	0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 0, "Select all objects in the scene or empty the selection");
	uiDefBut(block, BUTM, 1, "Border Select|B",		0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 1, "Select objects in a rectangular area (press B again to activate circle select in edit mode)");

	/* uiDefBut(block, BUTM, 1, "Circle Select",		0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 2, "Select objects in a circular area"); */
	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Duplicate|Shift D",	0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 3, "Duplicate the selected object(s)");
	uiDefBut(block, BUTM, 1, "Delete|X",			0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 4, "Delete the selected object(s)");
	uiDefBut(block, BUTM, 1, "Edit Mode|Tab",		0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 5, "Toggle between object and edit mode");
	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Grabber|G",			0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 6, "Move the selected object(s)");
	uiDefBut(block, BUTM, 1, "Rotate|R",			0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 7, "Rotate the selected object(s)");
	uiDefBut(block, BUTM, 1, "Scale|S",				0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 8, "Scale the selected object(s)");
	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Shear|Ctrl S",		0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 9, "Shear the selected object(s)");
	uiDefBut(block, BUTM, 1, "Warp/Bend|Shift W",	0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 10, "Warp or bend the selected objects");
	uiDefBut(block, BUTM, 1, "Snap Menu|Shift S",	0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 11, "Activate the snap menu");
	
	uiBlockSetDirection(block, UI_DOWN);
		
	return block;
}

static void do_info_add_meshmenu(void *arg, int event)
{

	switch(event) {		
#ifdef EXPERIMENTAL_MENUS
	/* Maarten's proposal for a new Add Mesh menu */
		case 0:
			/* Line */
			//add_primitiveMesh(4);
			break;
		case 1:
			/* Circle */
			if(select_area(SPACE_VIEW3D)) {
				add_primitiveMesh(4);
			}
			break;
		case 2:
			/* Plane */
			add_primitiveMesh(0);
			break;
		case 3:
			/* Cube */
			add_primitiveMesh(1);
			break;
		case 4:
			/* UVsphere */
			add_primitiveMesh(11);
			break;
		case 5:
			/* IcoSphere */
			add_primitiveMesh(12);
			break;
		case 6:
			/* Cylinder */
			add_primitiveMesh(5);
			break;
		case 7:
			/* Tube */
			add_primitiveMesh(6);
			break;
		case 8:
			/* Cone */
			add_primitiveMesh(7);
			break;
		case 9:
			/* Grid */
			add_primitiveMesh(10);
			break;
#else /* EXPERIMENTAL_MENUS*/ 
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
#endif /* EXPERIMENTAL_MENUS */
		default:
			break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_add_meshmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "add_meshmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_add_meshmenu, NULL);

#ifdef EXPERIMENTAL_MENUS
	/* Maarten's proposal for a new Add Mesh menu */
	uiDefBut(block, BUTM, 1, "Line|",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "Add a Mesh Line");
	uiDefBut(block, BUTM, 1, "Circle|",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "Add a Mesh Circle");
	uiDefBut(block, SEPR, 0, "",						0, xco-=6,  160, 6,  NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Plane|",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "Add a Mesh Plane");
	uiDefBut(block, BUTM, 1, "Cube|",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "Add a Mesh Cube");
	uiDefBut(block, BUTM, 1, "UVsphere",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "Add a Mesh Sphere");
	uiDefBut(block, BUTM, 1, "IcoSphere|",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 5, "Add a Mesh Isocohedron Sphere");
	uiDefBut(block, BUTM, 1, "Cylinder With Caps|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 6, "Add a Mesh Cylinder with caps");
	uiDefBut(block, BUTM, 1, "Cylinder Without Caps|",	0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 7, "Add a Mesh Cylinder without caps");
	uiDefBut(block, BUTM, 1, "Cone|",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 8, "Add a Mesh Cone");
	uiDefBut(block, BUTM, 1, "Grid|",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 9, "Add a Mesh Grid");
#else /* EXPERIMENTAL_MENUS */
	uiDefBut(block, BUTM, 1, "Plane|",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "Add a Mesh Plane");
	uiDefBut(block, BUTM, 1, "Cube|",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "Add a Mesh Cube");
	uiDefBut(block, BUTM, 1, "Circle|",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "Add a Mesh Circle");
	uiDefBut(block, BUTM, 1, "UVsphere",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "Add a Mesh Sphere");
	uiDefBut(block, BUTM, 1, "IcoSphere|",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "Add a Mesh Isocohedron Sphere");
	uiDefBut(block, BUTM, 1, "Cylinder|",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 5, "Add a Mesh Cylinder");
	uiDefBut(block, BUTM, 1, "Tube|",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 6, "Add a Mesh Tube");
	uiDefBut(block, BUTM, 1, "Cone|",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 7, "Add a Mesh Cone");
	uiDefBut(block, SEPR, 0, "",					0, xco-=6,  160, 6,  NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Grid|",				0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 8, "Add a Mesh Grid");
#endif /* EXPERIMENTAL_MENUS */

	uiBlockSetDirection(block, UI_RIGHT);
		
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
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "add_curvemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_add_curvemenu, NULL);

	uiDefBut(block, BUTM, 1, "Bezier Curve|",	0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "Add a Bezier curve");
	uiDefBut(block, BUTM, 1, "Bezier Circle|",	0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "Add a Bezier circle");
	uiDefBut(block, BUTM, 1, "NURBS Curve|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "Add a NURB curve");
	uiDefBut(block, BUTM, 1, "NURBS Circle",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "Add a NURB circle");
	uiDefBut(block, BUTM, 1, "Path|",			0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "Add a path");
	
	uiBlockSetDirection(block, UI_RIGHT);
		
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
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "add_surfacemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_info_add_surfacemenu, NULL);

	uiDefBut(block, BUTM, 1, "NURBS Curve|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "Add a curve");
	uiDefBut(block, BUTM, 1, "NURBS Circle|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "Add a circle");
	uiDefBut(block, BUTM, 1, "NURBS Surface|",	0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "Add a surface");
	uiDefBut(block, BUTM, 1, "NURBS Tube",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 3, "Add a tube");
	uiDefBut(block, BUTM, 1, "NURBS Sphere|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 4, "Add a sphere");
	uiDefBut(block, BUTM, 1, "NURBS Donut|",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 5, "Add a donut");

	uiBlockSetDirection(block, UI_RIGHT);
		
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
			/* Text (argument is discarded) */
			add_primitiveFont(event);
			break;
		case 4:
			/* Metaball  (argument is discarded) */
			add_primitiveMball(event);
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
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "addmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_addmenu, NULL);

	uiDefBlockBut(block, info_add_meshmenu, NULL, "Mesh|>>", 0, xco-=20, 120, 19, "Click to open the Add Mesh menu");
	uiDefBlockBut(block, info_add_curvemenu, NULL, "Curve|>>", 0, xco-=20, 120, 19, "Click to open the Add Curve menu");
	uiDefBlockBut(block, info_add_surfacemenu, NULL, "Surface|>>", 0, xco-=20, 120, 19, "Click to open the Add Surface menu");

	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Text|",				0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefBut(block, BUTM, 1, "Metaball|",			0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefBut(block, BUTM, 1, "Empty|",				0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 5, "");
	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Camera|",				0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 6, "");
	uiDefBut(block, BUTM, 1, "Lamp|",				0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 7, "");
//	uiDefBut(block, BUTM, 1, "Armature|",			0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 8, "");
	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Lattice|",			0, xco-=20, 120, 19, NULL, 0.0, 0.0, 1, 9, "");
	
	uiBlockSetDirection(block, UI_DOWN);
		
	return block;
}

static void do_info_viewmenu(void *arg, int event)
{
	switch(event) {
	case 0:
		if(select_area(SPACE_VIEW3D)) mainqenter(PAD1, 1);
		break;
	case 1:
		if(select_area(SPACE_VIEW3D)) mainqenter(PAD3, 1);
		break;
	case 2:
		if(select_area(SPACE_VIEW3D)) mainqenter(PAD7, 1);
		break;
	case 3:
		if(select_area(SPACE_VIEW3D)) mainqenter(PAD0, 1);
		break;
	case 4:
		if(select_area(SPACE_VIEW3D)) mainqenter(PADPLUSKEY, 1);
		break;
	case 5:
		if(select_area(SPACE_VIEW3D)) mainqenter(PADMINUS, 1);
		break;
	case 6:
		if(select_area(SPACE_VIEW3D)) mainqenter(CKEY, 1);
		break;
	case 7:
		if(select_area(SPACE_VIEW3D)) mainqenter(HOMEKEY, 1);
		break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_viewmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "filemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_viewmenu, NULL);

	// uiBlockSetCol(block, BUTBLUE);
	
	uiDefBut(block, BUTM, 1, "Front|NumPad 1",		0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Right|NumPad 3",		0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefBut(block, BUTM, 1, "Top|NumPad 7",		0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefBut(block, BUTM, 1, "Camera|NumPad 0",		0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 3, "");

	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 140, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Zoom In|NumPad +",	0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefBut(block, BUTM, 1, "Zoom Out|NumPad -",	0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 5, "");
	
	uiDefBut(block, SEPR, 0, "",					0, xco-=6, 140, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, BUTM, 1, "Center|C",			0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 6, "");
	uiDefBut(block, BUTM, 1, "View All|Home",		0, xco-=20, 140, 19, NULL, 0.0, 0.0, 0, 7, "");
	
	uiBlockSetDirection(block, UI_DOWN);
	
	return block;
}

static void do_game_menu(void *arg, int event)
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
	uiBlock *block;
	short yco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "gamemenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTBLUE);
	uiBlockSetDirection(block, UI_DOWN);
#if GAMEBLENDER == 1
	uiDefBut(block, BUTM, B_STARTGAME, "Start Game|P", 
			 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 0, 0, 
			 "Start the game (press the Escape key to stop)");
	
	uiDefBut(block, SEPR, 0, "",
			-20, yco-=6, 180, 6, NULL, 0.0, 0.0, 1, 0, "");
#endif
	/* flags are case-values */
	uiBlockSetButmFunc(block, do_game_menu, NULL);
	uiDefBut(block, BUTM, 1, "Enable All Frames",
			 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, G_FILE_ENABLE_ALL_FRAMES_BIT,
			 "Toggle between draw all frames on (no frames dropped) and off (full speed)");
	uiDefBut(block, BUTM, 1, "Show framerate and profile",
			 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, G_FILE_SHOW_FRAMERATE_BIT,
			 "Toggle between showing and not showing the framerate and profile");
	uiDefBut(block, BUTM, 1, "Show debug properties",
			 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, G_FILE_SHOW_DEBUG_PROPS_BIT,
			 "Toggle between showing and not showing debug properties");
	uiDefBut(block, SEPR, 0, "",				-20, yco-=6, 180, 6, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Autostart",
			 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, G_FILE_AUTOPLAY_BIT,
			 "Toggle between automatic game start on and off");
	
	/* Toggle buttons */
#if GAMEBLENDER == 1
	yco= -26;
#else
	yco= 0;
#endif
	uiBlockSetEmboss(block, UI_EMBOSSW);
	/* flags are defines */

	uiBlockSetButmFunc(block, NULL, NULL); // to prevent it from calling the menu function
	uiDefIconButI(block, ICONTOG|BIT|G_FILE_ENABLE_ALL_FRAMES_BIT,
			 0, ICON_CHECKBOX_DEHLT, -20, yco-=20, 19, 19,
			 &G.fileflags, 0.0, 0.0, 0, 0, "");
	uiDefIconButI(block, ICONTOG|BIT|G_FILE_SHOW_FRAMERATE_BIT,
			 0, ICON_CHECKBOX_DEHLT, -20, yco-=20, 19, 19,
			 &G.fileflags, 0.0, 0.0, 0, 0, "");
	uiDefIconButI(block, ICONTOG|BIT|G_FILE_SHOW_DEBUG_PROPS_BIT,
			 0, ICON_CHECKBOX_DEHLT, -20, yco-=20, 19, 19,
			 &G.fileflags, 0.0, 0.0, 0, 0, "");
	yco-=6;
	uiDefIconButI(block, ICONTOG|BIT|G_FILE_AUTOPLAY_BIT,
			 0, ICON_CHECKBOX_DEHLT, -20, yco-=20, 19, 19,
			 &G.fileflags, 0.0, 0.0, 0, 0, "");

	return block;
}

#ifndef EXPERIMENTAL_MENUS
/* In the Maarten's new menu structure proposal, the tools menu moved to the file menu */
static void do_info_toolsmenu(void *arg, int event)
{
	
	switch(event) {
	case 0:
		check_packAll();
		break;
	case 1:
		unpackAll(PF_WRITE_LOCAL);
		G.fileflags &= ~G_AUTOPACK;
		break;
	case 2:
		if (buttons_do_unpack() != RET_CANCEL) {
			// clear autopack bit only if 
			// user selected one of the unpack options
			G.fileflags &= ~G_AUTOPACK;
		}
		break;
	}
	allqueue(REDRAWINFO, 0);
}

static uiBlock *info_toolsmenu(void *arg_unused)
{
/*  	static short tog=0; */
	uiBlock *block;
	short xco= 0;
	
	block= uiNewBlock(&curarea->uiblocks, "toolsmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_info_toolsmenu, NULL);
	// uiBlockSetCol(block, BUTBLUE);
	
	uiDefBut(block, BUTM, 1, "Pack Data",						0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, 1, "Unpack Data to current dir",		0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefBut(block, BUTM, 1, "Advanced Unpack",					0, xco-=20, 160, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiBlockSetDirection(block, UI_DOWN);
	
	return block;
}
#endif /* EXPERIMENTAL_MENUS */


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
		hsize = 4 + (120.0 * g_done);
		fac1 = 0.5 * g_done; // do some rainbow colours on progress
		fac2 = 1.0;
		fac3 = 0.9;
	} else {
		hsize = 124;
		/* promise! Never change these lines again! */
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
	BMF_DrawString(G.fonts, headerstr);
		
	glRasterPos2i(x+120,  y);
	BMF_DrawString(G.fonts, infostr);
}

void info_buttons(void)
{
	uiBlock *block;
	short xco= 32;
	char naam[20];

	sprintf(naam, "header %d", curarea->headwin);	
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSM, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREY);

	uiDefBlockBut(block, info_filemenu, NULL, "File",	xco, 3, 40, 15, "");
	xco+= 40;
	uiDefBlockBut(block, info_editmenu, NULL, "Edit",	xco, 3, 40, 15, "");
	xco+= 40;

	uiDefBlockBut(block, info_addmenu, NULL, "Add",	xco, 3, 40, 15, "");
	xco+= 40;

	uiDefBlockBut(block, info_viewmenu, NULL, "View",	xco, 3, 40, 15, "");
	xco+= 40;

	uiDefBlockBut(block, info_gamemenu, NULL, "Game",	xco, 3, 40, 15, "");
	xco+= 40;

#ifndef EXPERIMENTAL_MENUS
	/* In the Maarten's new menu structure proposal, the tools menu moved to the file menu */
	uiDefBlockBut(block, info_toolsmenu, NULL, "Tools",	xco, 3, 40, 15, "");
	xco+= 40;
#endif /* EXPERIMENTAL_MENUS */

	/* pack icon indicates a packed file */
		
	if (G.fileflags & G_AUTOPACK) {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		uiDefIconBut(block, LABEL, 0, ICON_PACKAGE, xco, 0, XIC, YIC, &G.fileflags, 0.0, 0.0, 0, 0, "This is a Packed file. See File menu.");
		xco += 24;
		uiBlockSetEmboss(block, UI_EMBOSSX);
	}

	uiBlockSetEmboss(block, UI_EMBOSSX);
	
	if (curarea->full == 0) {
		curarea->butspacetype= SPACE_INFO;
		uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type ");
		
		/* STD SCREEN BUTTONS */
		xco+= XIC;
		xco= std_libbuttons(block, xco, 0, NULL, B_INFOSCR, (ID *)G.curscreen, 0, &G.curscreen->screennr, 1, 1, B_INFODELSCR, 0, 0);
	
		/* STD SCENE BUTTONS */
		xco+= 5;
		xco= std_libbuttons(block, xco, 0, NULL, B_INFOSCE, (ID *)G.scene, 0, &G.curscreen->scenenr, 1, 1, B_INFODELSCE, 0, 0);
	}
	else xco= 430;
	
	info_text(xco+24, 6);
	
	uiBlockSetEmboss(block, UI_EMBOSSN);
	uiDefIconBut(block, BUT, B_SHOWSPLASH, ICON_BLENDER, xco+1, 0,XIC,YIC, 0, 0, 0, 0, 0, "");
	uiBlockSetEmboss(block, UI_EMBOSSX);

	uiBlockSetEmboss(block, UI_EMBOSSN);
	uiDefIconBut(block, LABEL, 0, ICON_PUBLISHER, xco+125, 0,XIC,YIC, 0, 0, 0, 0, 0, "");
	uiBlockSetEmboss(block, UI_EMBOSSX);

	/* altijd als laatste doen */
	curarea->headbutlen= xco+2*XIC;
	
	if(curarea->headbutlen + 4*XIC < curarea->winx) {
		uiDefIconBut(block, BUT, B_FILEMENU, ICON_HELP, (short)(curarea->winx-XIC-2), 0,XIC,YIC, 0, 0, 0, 0, 0, "Toolbox menu, hotkey: SPACE");
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type ");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_SEQHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");	
	xco+= XIC;
	
	/* IMAGE */
	uiDefIconButS(block, TOG, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &sseq->mainb, 0, 0, 0, 0, "Toggles image display");

	/* ZOOM en BORDER */
	xco+= XIC;
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zoom view (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zoom view to area");

	/* CLEAR MEM */
	xco+= XIC;
	uiDefBut(block, BUT, B_SEQCLEAR, "Clear",	xco+=XIC,0,2*XIC,YIC, 0, 0, 0, 0, 0, "Forces a clear of all buffered images in memory");
	
	uiDrawBlock(block);
}

/* ********************** END SEQ ****************************** */
/* ********************** VIEW3D ****************************** */

void do_view3d_buttons(short event)
{
	int bit;

	/* pas op: als curarea->win niet bestaat, afvangen als direkt tekenroutines worden aangeroepen */

	switch(event) {
	case B_HOME:
		view3d_home(0);
		break;
	case B_SCENELOCK:
		if(G.vd->scenelock) {
			G.vd->lay= G.scene->lay;
			/* layact zoeken */
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

	default:

		if(event>=B_LAY && event<B_LAY+31) {
			if(G.vd->lay!=0 && (G.qual & LR_SHIFTKEY)) {
				
				/* wel actieve layer zoeken */
				
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
	/* redraw lijkt dubbelop: wordt in queue netjes afgehandeld */
	scrarea_queue_headredraw(curarea);
	
	if(curarea->spacetype==SPACE_OOPS) allqueue(REDRAWVIEW3D, 1);	/* 1==ook headwin */
	
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type ");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_NLAHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");	
	xco+= XIC;
	
	/* IMAGE */
//	uiDefIconButS(block, TOG, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &sseq->mainb, 0, 0, 0, 0, "Toggles image display");

	/* ZOOM en BORDER */
//	xco+= XIC;
//	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zoom view (CTRL+MiddleMouse)");
//	uiDefIconBut(block, BUT, B_NLABORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zoom view to area");

	/* draw LOCK */
	xco+= XIC/2;
	uiDefIconButI(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.snla->lock), 0, 0, 0, 0, "Lock redraw of other windows while editing");

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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_ACTHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");

	
	/* NAME ETC */
	ob=OBACT;
	from = (ID*) ob;

	xco= std_libbuttons(block, xco+1.5*XIC, B_ACTPIN, &G.saction->pin, B_ACTIONBROWSE, (ID*)G.saction->action, from, &(G.saction->actnr), B_ACTALONE, B_ACTLOCAL, B_ACTIONDELETE, 0, 0);	

#ifdef __NLA_BAKE
	/* Draw action baker */
	uiDefBut(block, BUT, B_ACTBAKE, "Bake", xco+=XIC, 0, 64, YIC, 0, 0, 0, 0, 0, "Generate an action with the constraint effects converted into ipo keys");
	xco+=64;
#endif
	uiClearButLock();

	/* draw LOCK */
	xco+= XIC/2;
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.saction->lock), 0, 0, 0, 0, "Lock redraw of other windows while editing");


	/* always as last  */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}

void view3d_buttons(void)
{
	uiBlock *block;
	int a;
	short xco;
	char naam[20];
	
	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTBLUE);	

	curarea->butspacetype= SPACE_VIEW3D;
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type ");
	

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_HOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");	

	xco+= XIC+8;
	if(G.vd->localview==0) {
		/* LAYERS */
		for(a=0; a<10; a++) {
			uiDefButI(block, TOG|BIT|(a+10), B_LAY+10+a, "",(short)(xco+a*(XIC/2)), 0,			XIC/2, (YIC)/2, &(G.vd->lay), 0, 0, 0, 0, "Layers");
			uiDefButI(block, TOG|BIT|a, B_LAY+a, "",	(short)(xco+a*(XIC/2)), (short)(YIC/2),(short)(XIC/2),(short)(YIC/2), &(G.vd->lay), 0, 0, 0, 0, "Layers");
			if(a==4) xco+= 5;
		}
		xco+= (a-2)*(XIC/2)+5;

		/* LOCK */
		uiDefIconButS(block, ICONTOG, B_SCENELOCK, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.vd->scenelock), 0, 0, 0, 0, "Lock layers and used Camera to Scene");
		xco+= XIC;

	}
	else xco+= (10+2)*(XIC/2)+10;
	
	/* LOCALVIEW */
	uiDefIconButS(block, ICONROW, B_LOCALVIEW, ICON_LOCALVIEW,	xco+=XIC,0,XIC,YIC, &(G.vd->localview), 0.0, 1.0, 0, 0, "Local View (NumPad /)");
	
	/* PERSP */
	xco+= XIC/2;
	uiDefIconButS(block, ICONROW, B_PERSP, ICON_ORTHO,	xco+=XIC,0,XIC,YIC, &(G.vd->persp), 0.0, 2.0, 0, 0, "Perspective mode (NumPad 5, Numpad 0)");
	
	xco+= XIC/2;
	/* AANZICHT */
	
	if(G.vd->view==7) G.vd->viewbut= 1;
	else if(G.vd->view==1) G.vd->viewbut= 2;
	else if(G.vd->view==3) G.vd->viewbut= 3;
	else G.vd->viewbut= 0;
	
	uiDefIconButS(block, ICONROW, B_VIEWBUT, ICON_VIEW_AXIS_NONE2, xco+=XIC,0,XIC,YIC, &G.vd->viewbut, 0.0, 3.0, 0, 0, "Top/Front or Side views (Numpad 7, 1, 3)");
	
	/* DRAWTYPE */
	xco+= XIC/2;
	uiDefIconButS(block, ICONROW, B_REDR, ICON_BBOX,	xco+=XIC,0,XIC,YIC, &(G.vd->drawtype), 1.0, 5.0, 0, 0, "Drawtype: boundbox/wire/solid/shaded (ZKEY, SHIFT+Z)");

	/* VIEWMOVE */
	xco+= XIC/2;
	uiDefIconButI(block, TOG, B_VIEWTRANS, ICON_VIEWMOVE,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Translate view (SHIFT+MiddleMouse)");
	uiDefIconButI(block, TOG, B_VIEWZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zoom view (CTRL+MiddleMouse)");

	/* around */
	xco+= XIC/2;
	uiDefIconButS(block, ROW, 1, ICON_ROTATE,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 0.0, 0, 0, "Rotation/Scaling around boundbox center (COMMAKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECENTER,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 3.0, 0, 0, "Rotation/Scaling around median point");
	uiDefIconButS(block, ROW, 1, ICON_CURSOR,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 1.0, 0, 0, "Rotation/Scaling around cursor (DOTKEY)");
	uiDefIconButS(block, ROW, 1, ICON_ROTATECOLLECTION,	xco+=XIC,0,XIC,YIC, &G.vd->around, 3.0, 2.0, 0, 0, "Rotation/Scaling around individual centers");

	/* mode */
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
	
	xco+= XIC/2;
	/*	If there is a posable object hilighted & selected, display this button */
	if (OBACT){
		if (OBACT->type==OB_ARMATURE){
			uiDefIconButS(block, ICONTOG|BIT|7, B_POSEMODE, ICON_POSE_DEHLT,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "PoseMode (CTRL+TAB)");
		}
	}
	uiDefIconButS(block, ICONTOG|BIT|4, B_EDITMODE, ICON_EDITMODE_DEHLT,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "EditMode (TAB)");
	if (!G.obpose && !G.obedit)
	{
		xco+= XIC/2;
		/* Only if mesh is selected */
		if (OBACT && OBACT->type == OB_MESH){
			uiDefIconButS(block, ICONTOG|BIT|5, B_VPAINT, ICON_VPAINT_DEHLT,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "VertexPaint Mode (VKEY)");
			/* Only if deformable mesh is selected */
			if ((((Mesh*)(OBACT->data))->dvert))
				uiDefIconButS(block, ICONTOG|BIT|9, B_WPAINT, ICON_WPAINT_DEHLT,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "WeightPaint Mode");
#ifdef NAN_TPT
			uiDefIconButS(block, ICONTOG|BIT|8, B_TEXTUREPAINT, ICON_TPAINT_DEHLT,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "TexturePaint Mode");
#endif /* NAN_TPT */
			uiDefIconButS(block, ICONTOG|BIT|6, B_FACESEL, ICON_FACESEL_DEHLT,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "FaceSelect Mode (FKEY)");
		}
	}
	if (G.obpose){
		/* Copy paste - WAS in action window */
		xco+= XIC/2;
	//	xco-= XIC/2;	//	Used in action window
		if(curarea->headertype==HEADERTOP) {
			uiDefIconBut(block, BUT, B_ACTCOPY, ICON_COPYUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the current pose to the buffer");
			uiSetButLock(G.obpose->id.lib!=0, "Can't edit library data");
			uiDefIconBut(block, BUT, B_ACTPASTE, ICON_PASTEUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the pose from the buffer");
			uiDefIconBut(block, BUT, B_ACTPASTEFLIP, ICON_PASTEFLIPUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the flipped pose from the buffer");
		}
		else {
			uiDefIconBut(block, BUT, B_ACTCOPY, ICON_COPYDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the current pose to the buffer");
			uiSetButLock(G.obpose->id.lib!=0, "Can't edit library data");
			uiDefIconBut(block, BUT, B_ACTPASTE, ICON_PASTEDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the pose from the buffer");
			uiDefIconBut(block, BUT, B_ACTPASTEFLIP, ICON_PASTEFLIPDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the flipped pose from the buffer");
		}
//		xco+=XIC/2;	//	Used in action window
	}
	if(G.vd->bgpic) {
		xco+= XIC/2;
		uiDefIconButS(block, TOG|BIT|1, B_REDR, ICON_IMAGE_COL,	xco+=XIC,0,XIC,YIC, &G.vd->flag, 0, 0, 0, 0, "Display Background picture");
	}
	if(G.obedit) {
		extern int prop_mode;
		xco+= XIC/2;
		uiDefIconButI(block, ICONTOG|BIT|14, B_PROPTOOL, ICON_GRID,	xco+=XIC,0,XIC,YIC, &G.f, 0, 0, 0, 0, "Proportional vertex editing (OKEY)");
		if(G.f & G_PROPORTIONAL) {
			uiDefIconButI(block, ROW, 0, ICON_SHARPCURVE,	xco+=XIC,0,XIC,YIC, &prop_mode, 4.0, 0.0, 0, 0, "Sharp falloff (SHIFT+OKEY)");
			uiDefIconButI(block, ROW, 0, ICON_SMOOTHCURVE,	xco+=XIC,0,XIC,YIC, &prop_mode, 4.0, 1.0, 0, 0, "Smooth falloff (SHIFT+OKEY)");
		}
	}
	
	xco+=XIC;
	uiDefIconBut(block, BUT, B_VIEWRENDER, ICON_SCENE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Render this view. (Hold SHIFT for Anim render)");
	xco+=XIC;
#if GAMEBLENDER == 1
	uiDefIconBut(block, BUT, B_STARTGAME, ICON_GAME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Start game (PKEY)");
#endif

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

		/* beetje uitzoomen */
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
		/* waarde omkeren vanwege winqread */
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type ");

	test_editipo();	/* test of huidige editipo klopt, make_editipo zet de v2d->cur */

		/* FULL WINDOW en HOME */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_IPOHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");
	uiDefIconButS(block, ICONTOG, B_IPOSHOWKEY, ICON_KEY_DEHLT,	xco+=XIC,0,XIC,YIC, &G.sipo->showkey, 0, 0, 0, 0, "Toggles curve/key display (KKEY)");

	/* mainmenu, only when data is there and no pin */
	uiSetButLock(G.sipo->pin, "Can't change because of pinned data");
	
	ob= OBACT;
	xco+= XIC/2;
	uiDefIconButS(block, ROW, B_IPOMAIN, ICON_OBJECT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_OB, 0, 0, "Display Object Ipos ");
	
	if(ob && give_current_material(ob, ob->actcol)) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_MATERIAL,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_MA, 0, 0, "Display Material Ipos ");
		if(G.sipo->blocktype==ID_MA) {
			uiDefButS(block, NUM, B_IPOMAIN, "", 	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Channel Number of the active Material texture");
			xco-= 4;
		}
	}
	if(G.scene->world) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_WORLD,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_WO, 0, 0, "Display World Ipos");
		if(G.sipo->blocktype==ID_WO) {
			uiDefButS(block, NUM, B_IPOMAIN, "", 	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Channel Number of the active World texture");
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
			uiDefButS(block, NUM, B_IPOMAIN, "", 	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Channel Number of the active Lamp texture");
			xco-= 4;
		}
	}
	
	if(ob) {
		if ELEM4(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_LATTICE)
			uiDefIconButS(block, ROW, B_IPOMAIN, ICON_EDIT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_KE, 0, 0, "Display VertexKeys Ipos");
		if (ob->action)
			uiDefIconButS(block, ROW, B_IPOMAIN, ICON_ACTION,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_AC, 0, 0, "Display Action Ipos");
#ifdef __CON_IPO
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_CONSTRAINT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)IPO_CO, 0, 0, "Display Constraint Ipos");
#endif
	}
	uiDefIconButS(block, ROW, B_IPOMAIN, ICON_SEQUENCE,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_SEQ, 0, 0, "Display Sequence Ipos");

	if(G.buts && G.buts->mainb == BUTS_SOUND && G.buts->lockpoin) 
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_SOUND,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_SO, 0, 0, "Display Sound Ipos");

	
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
	uiDefIconBut(block, BUT, B_IPOCYCLIC, ICON_CYCLICLINEAR,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0,  "Sets the extend mode to cyclic");
	uiDefIconBut(block, BUT, B_IPOCYCLICX, ICON_CYCLIC,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0,  "Sets the extend mode to cyclic extrapolation");
	xco+= XIC/2;

	uiClearButLock();
	/* ZOOM en BORDER */
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zoom view (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zoom view to area");
	
	/* draw LOCK */
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.sipo->lock), 0, 0, 0, 0, "Lock redraw of other windows while editing");

	/* altijd als laatste doen */
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
			/* vrijgeven huidige mat */
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

static void validate_bonebutton(void *bonev, void *data2_unused){
	Bone *bone= bonev;
	bArmature *arm;

	arm = get_armature(G.obpose);
	unique_bone_name(bone, arm);
}


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
	short type;
	void *data=NULL;
	char str[256];

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREY);

	curarea->butspacetype= SPACE_BUTS;
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type ");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");

	/* HOME */
	uiDefIconBut(block, BUT, B_BUTSHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");
	
	/* keuzemenu */
	xco+= 2*XIC;
	uiDefIconButS(block, ROW, B_REDR,			ICON_EYE,	xco+=XIC, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_VIEW, 0, 0, "View buttons");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_LAMP,	xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_LAMP, 0, 0, "Lamp buttons (F4)");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_MATERIAL,	xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_MAT, 0, 0, "Material buttons (F5)");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_TEXTURE,	xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_TEX, 0, 0, "Texture buttons (F6)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_ANIM,	xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_ANIM, 0, 0, "Animation buttons (F7)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_GAME,	xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_GAME, 0, 0, "Realtime buttons (F8)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_EDIT,	xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_EDIT, 0, 0, "Edit buttons (F9)");
	uiDefIconButS(block, ROW, B_REDR,			ICON_CONSTRAINT,xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_CONSTRAINT, 0, 0, "Constraint buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_SPEAKER,xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_SOUND, 0, 0, "Sound buttons");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,		ICON_WORLD,	xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_WORLD, 0, 0, "World buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_PAINT,xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_FPAINT, 0, 0, "Paint buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_RADIO,xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_RADIO, 0, 0, "Radiosity buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_SCRIPT,xco+=30, 0, 30, YIC, &(G.buts->mainb), 1.0, (float)BUTS_SCRIPT, 0, 0, "Script buttons");
	uiDefIconButS(block, ROW, B_REDR,			ICON_SCENE,	xco+=30, 0, 50, YIC, &(G.buts->mainb), 1.0, (float)BUTS_RENDER, 0, 0, "Display buttons (F10)");
	xco+= 80;
	
	ob= OBACT;
	
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
			uiDefIconBut(block, BUT, B_MATCOPY, ICON_COPYUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copy Material to the buffer");
			uiSetButLock(id && id->lib, "Can't edit library data");
			uiDefIconBut(block, BUT, B_MATPASTE, ICON_PASTEUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Paste Material from the buffer");
		}
		else {
			uiDefIconBut(block, BUT, B_MATCOPY, ICON_COPYDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copy Material to the buffer");
			uiSetButLock(id && id->lib, "Can't edit library data");
			uiDefIconBut(block, BUT, B_MATPASTE, ICON_PASTEDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Paste Material from the buffer");
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
				but= uiDefBut(block, TEX, B_IDNAME, "GR:",	xco, 0, 135, YIC, group->id.name+2, 0.0, 19.0, 0, 0, "Active Group name");
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
			but= uiDefBut(block, TEX, B_IDNAME, "OB:",	xco, 0, 135, YIC, ob->id.name+2, 0.0, 19.0, 0, 0, "Active Object name");
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
				but= uiDefBut(block, TEX, 1, "BO:",	xco, 0, 135, YIC, ((Bone*)data)->name, 0.0, 19.0, 0, 0, "Active Bone name");
				uiButSetFunc(but, validate_bonebutton, data, NULL);
#else
				but= uiDefBut(block, LABEL, 1, str,	xco, 0, 135, YIC, ((Bone*)data)->name, 0.0, 19.0, 0, 0, "Active Bone name");
#endif
				xco+= 135;
			}
		}
	}
	else if(G.buts->mainb==BUTS_SCRIPT) {
		if(ob)
			uiDefIconButS(block, ROW, B_REDR, ICON_OBJECT, xco,0,XIC,YIC, &G.buts->scriptblock,  2.0, (float)ID_OB, 0, 0, "Display Object script links");

		if(ob && give_current_material(ob, ob->actcol))
			uiDefIconButS(block, ROW, B_REDR, ICON_MATERIAL,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_MA, 0, 0, "Display Material script links ");

		if(G.scene->world) 
			uiDefIconButS(block, ROW, B_REDR, ICON_WORLD,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_WO, 0, 0, "Display World script links");
	
		if(ob && ob->type==OB_CAMERA)
			uiDefIconButS(block, ROW, B_REDR, ICON_CAMERA,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_CA, 0, 0, "Display Camera script links");

		if(ob && ob->type==OB_LAMP)
			uiDefIconButS(block, ROW, B_REDR, ICON_LAMP,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_LA, 0, 0, "Display Lamp script links");

		xco+= 20;
	}
	
	uiDefButS(block, NUM, B_NEWFRAME, "",	(short)(xco+20),0,60,YIC, &(G.scene->r.cfra), 1.0, 18000.0, 0, 0, "Current Frame");
	xco+= 80;

	G.buts->mainbo= G.buts->mainb;

	/* altijd als laatste doen */
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	
	/* SORT TYPE */
	xco+=XIC;
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTALPHA,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 0.0, 0, 0, "Sort files alphabetically");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTTIME,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 1.0, 0, 0, "Sort files by time");
	uiDefIconButS(block, ROW, B_SORTFILELIST, ICON_SORTSIZE,	xco+=XIC,0,XIC,YIC, &sfile->sort, 1.0, 2.0, 0, 0, "Sort files by size");	

	cpack(0x0);
	glRasterPos2i(xco+=XIC+10,  5);
	BMF_DrawString(uiBlockGetCurFont(block), sfile->title);
	
	xco+= BMF_GetStringWidth(G.font, sfile->title);
	
	uiDefIconButS(block, ICONTOG|BIT|0, B_SORTFILELIST, ICON_LONGDISPLAY,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Toggle long info");
	uiDefIconButS(block, TOG|BIT|3, B_RELOADDIR, ICON_GHOST,xco+=XIC,0,XIC,YIC, &sfile->flag, 0, 0, 0, 0, "Hide dot files");

	xco+=XIC+10;

	if(sfile->type==FILE_LOADLIB) {
		uiDefButS(block, TOGN|BIT|2, B_REDR, "Append",		xco+=XIC,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Causes selected data to be copied into current file");
		uiDefButS(block, TOG|BIT|2, B_REDR, "Link",	xco+=100,0,100,YIC, &sfile->flag, 0, 0, 0, 0, "Causes selected data to be linked by current file");
	}

	if(sfile->type==FILE_UNIX) {
		df= BLI_diskfree(sfile->dir)/(1048576.0);

		filesel_statistics(sfile, &totfile, &selfile, &totlen, &sellen);
		
		sprintf(naam, "Free: %.3f Mb   Files: (%d) %d    (%.3f) %.3f Mb", 
					df, selfile,totfile, sellen, totlen);
		
		cpack(0x0);
		glRasterPos2i(xco,  5);
		BMF_DrawString(uiBlockGetCurFont(block), naam);
	}
	/* altijd als laatste doen */
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	
	/* HOME */
	uiDefIconBut(block, BUT, B_OOPSHOME, ICON_HOME,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");	
	xco+= XIC;
	
	/* ZOOM en BORDER */
	xco+= XIC;
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	(short)(xco+=XIC),0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zoom view (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	(short)(xco+=XIC),0,XIC,YIC, 0, 0, 0, 0, 0, "Zoom view to area");

	/* VISIBLE */
	xco+= XIC;
	uiDefButS(block, TOG|BIT|10,B_NEWOOPS, "lay",		(short)(xco+=XIC),0,XIC+10,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Objects based on layer");
	uiDefIconButS(block, TOG|BIT|0, B_NEWOOPS, ICON_SCENE_HLT,	(short)(xco+=XIC+10),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Scene data");
	uiDefIconButS(block, TOG|BIT|1, B_NEWOOPS, ICON_OBJECT_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Object data");
	uiDefIconButS(block, TOG|BIT|2, B_NEWOOPS, ICON_MESH_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Mesh data");
	uiDefIconButS(block, TOG|BIT|3, B_NEWOOPS, ICON_CURVE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Curve/Surface/Font data");
	uiDefIconButS(block, TOG|BIT|4, B_NEWOOPS, ICON_MBALL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Metaball data");
	uiDefIconButS(block, TOG|BIT|5, B_NEWOOPS, ICON_LATTICE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Lattice data");
	uiDefIconButS(block, TOG|BIT|6, B_NEWOOPS, ICON_LAMP_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Lamp data");
	uiDefIconButS(block, TOG|BIT|7, B_NEWOOPS, ICON_MATERIAL_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Material data");
	uiDefIconButS(block, TOG|BIT|8, B_NEWOOPS, ICON_TEXTURE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Texture data");
	uiDefIconButS(block, TOG|BIT|9, B_NEWOOPS, ICON_IPO_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Ipo data");
	uiDefIconButS(block, TOG|BIT|12, B_NEWOOPS, ICON_IMAGE_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Image data");
	uiDefIconButS(block, TOG|BIT|11, B_NEWOOPS, ICON_LIBRARY_HLT,	(short)(xco+=XIC),0,XIC,YIC, &soops->visiflag, 0, 0, 0, 0, "Display Library data");

	/* naam */
	if(G.soops->lockpoin) {
		oops= G.soops->lockpoin;
		if(oops->type==ID_LI) strcpy(naam, ((Library *)oops->id)->name);
		else strcpy(naam, oops->id->name);
		
		cpack(0x0);
		glRasterPos2i(xco+=XIC+10,  5);
		BMF_DrawString(uiBlockGetCurFont(block), naam);

	}

	/* altijd als laatste doen */
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
		if(!okee("Really delete text?")) return;
		
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
		
	if(st->showlinenrs)
		uiDefIconBut(block, BUT, B_TEXTLINENUM, ICON_SHORTDISPLAY, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Hide line numbers");
	else
		uiDefIconBut(block, BUT, B_TEXTLINENUM, ICON_LONGDISPLAY, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Display line numbers");


	/* STD TEXT BUTTONS */
	if (!BPY_spacetext_is_pywin(st)) {
		xco+= 2*XIC;
		xco= std_libbuttons(block, xco, 0, NULL, B_TEXTBROWSE, (ID*)st->text, 0, &(st->menunr), 0, 0, B_TEXTDELETE, 0, 0);

		/*
		if (st->text) {
			if (st->text->flags & TXT_ISDIRTY && (st->text->flags & TXT_ISEXT || !(st->text->flags & TXT_ISMEM)))
				uiDefIconBut(block, BUT,0, ICON_ERROR, xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "The text has been changed");
			if (st->text->flags & TXT_ISEXT) 
				uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Store text in .blend file");
			else 
				uiDefBut(block, BUT,B_TEXTSTORE, ICON(),	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Don't store text in .blend file");
			xco+=10;
		}
		*/		

		xco+=XIC;
		if(st->font_id>1) st->font_id= 0;
		uiDefButI(block, MENU, B_TEXTFONT, "Screen 12 %x0|Screen 15%x1", xco,0,100,YIC, &st->font_id, 0, 0, 0, 0, "Font display menu");
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
	View2D	*v2d;

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
		v2d= &(G.saction->v2d);

//		v2d->cur.xmin = 0;
		v2d->cur.ymin=-SCROLLB;
		
		if (!G.saction->action){
			v2d->cur.xmax=100;
		}
		else
		{
			v2d->cur.xmin=calc_action_start(G.saction->action)-1;
			v2d->cur.xmax=calc_action_end(G.saction->action)+1;
		}
		
		
//		G.v2d->cur= G.v2d->tot;
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

			if(idtest==0) {	/* geen new */
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_SOUNDHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");

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

void load_space_image(char *str)	/* aangeroepen vanuit fileselect */
{
	Image *ima=0;
	
	if(G.obedit) {
		error("Can't perfom this in editmode");
		return;
	}

	ima= add_image(str);
	if(ima) {
		
		G.sima->image= ima;
		
		free_image_buffers(ima);	/* forceer opnieuw inlezen */
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

void replace_space_image(char *str)		/* aangeroepen vanuit fileselect */
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
		
		free_image_buffers(ima);	/* forceer opnieuw inlezen */
		ima->ok= 1;
		/* replace kent ook toe: */
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
		if(idtest==0) {	/* geen new */
			return;
		}
	
		if(idtest!=id) {
			G.sima->image= (Image *)idtest;
			if(idtest->us==0) idtest->us= 1;
			allqueue(REDRAWIMAGE, 0);
		}
		image_changed(G.sima, 0);	/* ook als image gelijk is: assign! 0==geen tileflag */
		
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
		image_changed(G.sima, 2);		/* 2: alleen tileflag */
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type ");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Restore smaller windows (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Make fullscreen window (CTRL+Down arrow)");
	
	/* HOME*/
	uiDefIconBut(block, BUT, B_SIMAGEHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Home (HOMEKEY)");
	uiDefIconButS(block, TOG|BIT|0, B_BE_SQUARE, ICON_KEEPRECT,	xco+=XIC,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Keep UV polygons square while editing");
	uiDefIconButS(block, ICONTOG|BIT|2, B_CLIP_UV, ICON_CLIPUV_DEHLT,xco+=XIC,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "Clip UV with image size");		

	xco= std_libbuttons(block, xco+40, 0, NULL, B_SIMABROWSE, (ID *)G.sima->image, 0, &(G.sima->imanr), 0, 0, B_IMAGEDELETE, 0, 0);

	if (G.sima->image) {
		if (G.sima->image->packedfile) {
			headerbuttons_packdummy = 1;
		}
		uiDefIconButI(block, TOG|BIT|0, B_SIMAPACKIMA, ICON_PACKAGE,	xco,0,XIC,YIC, &headerbuttons_packdummy, 0, 0, 0, 0, "Pack/Unpack this Image");
		xco += XIC;
	}
	
	uiBlockSetCol(block, BUTSALMON);
	uiDefBut(block, BUT, B_SIMAGELOAD, "Load",		xco+=XIC,0,2*XIC,YIC, 0, 0, 0, 0, 0, "Load image - thumbnail view");

	uiBlockSetCol(block, BUTGREY);
	uiDefBut(block, BUT, B_SIMAGELOAD1, "",		(short)(xco+=2*XIC+2),0,10,YIC, 0, 0, 0, 0, 0, "Load image - file select view");
	xco+=XIC/2;

	if (G.sima->image) {
		uiBlockSetCol(block, BUTSALMON);
		uiDefBut(block, BUT, B_SIMAGEREPLACE, "Replace",xco+=XIC,0,(short)(3*XIC),YIC, 0, 0, 0, 0, 0, "Replace current image - thumbnail view");
		
		uiBlockSetCol(block, BUTGREY);
		uiDefBut(block, BUT, B_SIMAGEREPLACE1, "",	(short)(xco+=3*XIC+2),0,10,YIC, 0, 0, 0, 0, 0, "Replace current image - file select view");
		xco+=XIC/2;
	
		uiDefIconButS(block, TOG|BIT|4, 0, ICON_ENVMAP, xco+=XIC,0,XIC,YIC, &G.sima->image->flag, 0, 0, 0, 0, "Use this image as a reflection map (UV coordinates are ignored)");
		xco+=XIC/2;

		uiDefIconButS(block, TOG|BIT|0, B_SIMAGEDRAW1, ICON_GRID, xco+=XIC,0,XIC,YIC, &G.sima->image->flag, 0, 0, 0, 0, "");
		uiDefButS(block, NUM, B_SIMAGEDRAW, "",	xco+=XIC,0,XIC,YIC, &G.sima->image->xrep, 1.0, 16.0, 0, 0, "");
		uiDefButS(block, NUM, B_SIMAGEDRAW, "",	xco+=XIC,0,XIC,YIC, &G.sima->image->yrep, 1.0, 16.0, 0, 0, "");
	
		uiDefButS(block, TOG|BIT|1, B_TWINANIM, "Anim", xco+=XIC,0,(short)(2*XIC),YIC, &G.sima->image->tpageflag, 0, 0, 0, 0, "");
		uiDefButS(block, NUM, B_TWINANIM, "",	(short)(xco+=2*XIC),0,XIC,YIC, &G.sima->image->twsta, 0.0, 128.0, 0, 0, "");
		uiDefButS(block, NUM, B_TWINANIM, "",	xco+=XIC,0,XIC,YIC, &G.sima->image->twend, 0.0, 128.0, 0, 0, "");
//		uiDefButS(block, TOG|BIT|2, 0, "Cycle", xco+=XIC,0,2*XIC,YIC, &G.sima->image->tpageflag, 0, 0, 0, 0, "");
		uiDefButS(block, NUM, 0, "Speed", xco+=(2*XIC),0,4*XIC,YIC, &G.sima->image->animspeed, 1.0, 100.0, 0, 0, "Speed of the animation in frames per second");

#ifdef NAN_TPT
		xco+= 4*XIC;
		uiDefIconButS(block, ICONTOG|BIT|3, B_SIMAGEPAINTTOOL, ICON_TPAINT_DEHLT, xco+=XIC,0,XIC,YIC, &G.sima->flag, 0, 0, 0, 0, "TexturePaint Mode");
		if (G.sima->image && G.sima->image->ibuf && (G.sima->image->ibuf->userflags & IB_BITMAPDIRTY)) {
			uiDefBut(block, BUT, B_SIMAGESAVE, "Save",		xco+=XIC,0,2*XIC,YIC, 0, 0, 0, 0, 0, "Save image");
			xco += XIC;
		}
#endif /* NAN_TPT */
		xco+= XIC;
	}

	/* draw LOCK */
	xco+= XIC/2;
	uiDefIconButS(block, ICONTOG, 0, ICON_UNLOCKED,	(short)(xco+=XIC),0,XIC,YIC, &(G.sima->lock), 0, 0, 0, 0, "Lock redraw of other windows while editing");

	
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
	uiDefIconButC(block, ICONROW,B_NEWSPACE, ICON_VIEW3D, 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Current window type");
	
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
	
	uiDefIconButS(block, TOG|BIT|0, B_REDR, ICON_BPIBFOLDERGREY, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "");/* dir   */
	uiDefIconButS(block, TOG|BIT|1, B_REDR, ICON_INFO, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "");/* info  */
	uiDefIconButS(block, TOG|BIT|2, B_REDR, ICON_IMAGE_COL, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "");/* image */
	uiDefIconButS(block, TOG|BIT|3, B_REDR, ICON_MAGNIFY, xco+=XIC,0,XIC,YIC, &simasel->mode, 0, 0, 0, 0, "");/* loep */
	
	/* altijd als laatste doen */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}

/* ********************** IMASEL ****************************** */

/* ******************** ALGEMEEN ********************** */

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

