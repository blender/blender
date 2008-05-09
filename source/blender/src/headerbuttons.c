/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_storage_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
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

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_exotic.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_node.h"
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
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editfont.h"
#include "BIF_editlattice.h"
#include "BIF_editconstraint.h"
#include "BIF_editmesh.h"
#include "BIF_editmesh.h"
#include "BIF_editsima.h"
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
#include "BIF_toolbox.h"
#include "BIF_usiblender.h"
#include "BIF_previewrender.h"
#include "BIF_writeimage.h"
#include "BIF_butspace.h"

#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"
#include "BSE_view.h"
#include "BSE_sequence.h"
#include "BSE_editipo.h"
#include "BSE_drawipo.h"

#include "BDR_drawmesh.h"
#include "BDR_vpaint.h"
#include "BDR_editface.h"
#include "BDR_editobject.h"
#include "BDR_editcurve.h"
#include "BDR_editmball.h"
#include "BDR_sculptmode.h"

#include "BPY_extern.h"
#include "BPY_menus.h"

#include "mydevice.h"
#include "blendef.h"
#include "interface.h"
#include "nla.h"	/* __NLA : To be removed later */
#include "butspace.h"  // test_idbutton

#include "BIF_poseobject.h"

#include "SYS_System.h"

 /* WATCH IT:  always give all headerbuttons for same window the same name
	*			event B_REDR is a standard redraw
	*
	*/

char *windowtype_pup(void)
{
	return(
	"Window type:%t" //14
	"|3D View %x1" //30

	"|%l" // 33

	"|Ipo Curve Editor %x2" //54
	"|Action Editor %x12" //73
	"|NLA Editor %x13" //94

	"|%l" //97

	"|UV/Image Editor %x6" //117

	"|Video Sequence Editor %x8" //143
	"|Timeline %x15" //163
	"|Audio Window %x11" //163
	"|Text Editor %x9" //179

	"|%l" //192


	"|User Preferences %x7" //213
	"|Outliner %x3" //232
	"|Buttons Window %x4" //251
	"|Node Editor %x16"
	"|%l" //254

	"|Image Browser %x10" //273
	"|File Browser %x5" //290

	"|%l" //293

	"|Scripts Window %x14"//313
	);
}

int GetButStringLength(char *str) {
	int rt;

	rt= BIF_GetStringWidth(G.font, str, (U.transopts & USER_TR_BUTTONS));

	return rt + 15;
}

/* ********************** GLOBAL ****************************** */

int std_libbuttons(uiBlock *block, short xco, short yco,
							int pin, short *pinpoin, int browse, short id_code, short special, ID *id,
							ID *parid, short *menupoin, int users, int lib,
							int del, int autobut, int keepbut)
{
	ListBase *lb;
	uiBut *but;
	int len, oldcol, add_addbutton=0;
	char *str=NULL, str1[10];

	uiBlockBeginAlign(block);
	oldcol= uiBlockGetCol(block);

	if(id && pin) {
		uiDefIconButS(block, ICONTOG, pin, ICON_PIN_DEHLT, xco,yco,XIC,YIC, pinpoin, 0, 0, 0, 0, "Keeps this view displaying the current data regardless of what object is selected");
		xco+= XIC;
	}
	/* browse menu */
	if(browse) {
		char *extrastr= NULL;
		
		if(ELEM4(id_code, ID_MA, ID_TE, ID_BR, ID_PA)) add_addbutton= 1;
			
		lb= wich_libbase(G.main, id_code);
		
		if(id && id->us>1) uiBlockSetCol(block, TH_BUT_SETTING1);

		if (pin && *pinpoin) {
			uiBlockSetCol(block, TH_BUT_SETTING2);
		}
		
		if (ELEM8( id_code, ID_SCE, ID_SCR, ID_MA, ID_TE, ID_WO, ID_IP, ID_AC, ID_BR) || id_code == ID_PA) extrastr= "ADD NEW %x 32767";
		else if (id_code==ID_TXT) extrastr= "OPEN NEW %x 32766 |ADD NEW %x 32767";
		else if (id_code==ID_SO) extrastr= "OPEN NEW %x 32766";

		uiSetButLock(G.scene->id.lib!=0, ERROR_LIBDATA_MESSAGE);
		if( id_code==ID_SCE || id_code==ID_SCR ) uiClearButLock();
		
		if(curarea->spacetype==SPACE_BUTS)
			uiSetButLock(id_code!=ID_SCR && G.obedit!=0 && G.buts->mainb==CONTEXT_EDITING, "Cannot perform in EditMode");
		
		if(parid) uiSetButLock(parid->lib!=0, ERROR_LIBDATA_MESSAGE);

		if (lb) {
			if( id_code==ID_IP)
				IPOnames_to_pupstring(&str, NULL, extrastr, lb, id, menupoin, G.sipo->blocktype);
			else if(browse!=B_SIMABROWSE && id_code==ID_IM )
				IMAnames_to_pupstring(&str, NULL, extrastr, lb, id, menupoin);
			else
				IDnames_to_pupstring(&str, NULL, extrastr, lb, id, menupoin);
		}
		
		uiDefButS(block, MENU, browse, str, xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses existing choices or adds NEW");
		xco+= XIC;
		
		uiClearButLock();
	
		MEM_freeN(str);
	}

	uiBlockSetCol(block, oldcol);

	if(id) {	/* text button with name */
	
		/* name */
		if(id->us>1) uiBlockSetCol(block, TH_BUT_SETTING1);
		/* Pinned data ? */
		if (pin && *pinpoin) {
			uiBlockSetCol(block, TH_BUT_SETTING2);
		}
		/* Redalert overrides pin color */
		if(id->us<=0) uiBlockSetCol(block, TH_REDALERT);

		uiSetButLock(id->lib!=0, ERROR_LIBDATA_MESSAGE);
		
		if(GS(id->name)==ID_SCE) strcpy(str1, "SCE:");
		else if(GS(id->name)==ID_SCE) strcpy(str1, "SCR:");
		else if(GS(id->name)==ID_MA) {
			if( ((Material *)id)->use_nodes )
				strcpy(str1, "NT:");
			else
				strcpy(str1, "MA:");
		}
		else {
			str1[0]= id->name[0];
			str1[1]= id->name[1];
			str1[2]= ':';
			str1[3]= 0;
		}
		
		if( GS(id->name)==ID_IP) len= 110;
		else if((yco) && (GS(id->name)==ID_AC)) len= 100; // comes from button panel (poselib)
		else if(yco) len= 140;	// comes from button panel
		else len= 120;
		
		but= uiDefBut(block, TEX, B_IDNAME, str1,xco, yco, (short)len, YIC, id->name+2, 0.0, 21.0, 0, 0, "Displays current Datablock name. Click to change.");
		uiButSetFunc(but, test_idbutton_cb, id->name, NULL);

		uiClearButLock();

		xco+= len;
		
		if(id->lib) {
			
			if(id->flag & LIB_INDIRECT) uiDefIconBut(block, BUT, 0, ICON_DATALIB,xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Indirect Library Datablock. Cannot change.");
			else uiDefIconBut(block, BUT, lib, ICON_PARLIB, xco,yco,XIC,YIC, 0, 0, 0, 0, 0, 
							  lib?"Direct linked Library Datablock. Click to make local.":"Direct linked Library Datablock, cannot make local."
							  );
			
			xco+= XIC;
		}
		
		
		if(users && id->us>1) {
			uiSetButLock (pin && *pinpoin, "Can't make pinned data single-user");
			
			sprintf(str1, "%d", id->us);
			if(id->us<10) {
				
				uiDefBut(block, BUT, users, str1, xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Displays number of users of this data. Click to make a single-user copy.");
				xco+= XIC;
			}
			else {
				uiDefBut(block, BUT, users, str1, xco, yco, XIC+10, YIC, 0, 0, 0, 0, 0, "Displays number of users of this data. Click to make a single-user copy.");
				xco+= XIC+10;
			}
			
			uiClearButLock();
			
		}
		
		if(del) {
			uiSetButLock (pin && *pinpoin, "Can't unlink pinned data");
			if(parid && parid->lib);
			else {
				uiDefIconBut(block, BUT, del, ICON_X, xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Deletes link to this Datablock");
				xco+= XIC;
			}
			
			uiClearButLock();
		}

		if(autobut) {
			if(parid && parid->lib);
			else {
				uiDefIconBut(block, BUT, autobut, ICON_AUTO,xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Generates an automatic name");
				xco+= XIC;
			}
			
			
		}
		if(keepbut) {
			uiDefButBitS(block, TOG, LIB_FAKEUSER, keepbut, "F", xco,yco,XIC,YIC, &id->flag, 0, 0, 0, 0, "Saves this datablock even if it has no users");
			xco+= XIC;
		}
	}
	else if(add_addbutton) {	/* "add new" button */
		uiBlockSetCol(block, oldcol);
		if(parid) uiSetButLock(parid->lib!=0, ERROR_LIBDATA_MESSAGE);
		uiDefButS(block, TOG, browse, "Add New" ,xco, yco, 110, YIC, menupoin, (float)*menupoin, 32767.0, 0, 0, "Add new data block");
		xco+= 110;
	}
	//xco+=XIC;
	
	uiBlockSetCol(block, oldcol);
	uiBlockEndAlign(block);

	return xco;
}


/* results in fully updated anim system */
static void do_update_for_newframe(int mute, int events)
{
	extern void audiostream_scrub(unsigned int frame);	/* seqaudio.c */
	
	if(events) {
		allqueue(REDRAWALL, 0);
	}
	
	/* this function applies the changes too */
	scene_update_for_newframe(G.scene, screen_view3d_layers()); /* BKE_scene.h */

	if ( (CFRA>1) && (!mute) && (G.scene->audio.flag & AUDIO_SCRUB)) 
		audiostream_scrub( CFRA );
	
	/* 3d window, preview */
	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);

	/* all movie/sequence images */
	BIF_image_update_frame();
	
	/* composite */
	if(G.scene->use_nodes && G.scene->nodetree)
		ntreeCompositTagAnimated(G.scene->nodetree);
}

void update_for_newframe(void)
{
	do_update_for_newframe(0, 1);
}

void update_for_newframe_muted(void)
{
	do_update_for_newframe(1, 1);
}

/* used by new animated UI playback */
void update_for_newframe_nodraw(int nosound)
{
	do_update_for_newframe(nosound, 0);
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
	extern char * build_rev;
	extern char * build_platform;
	extern char * build_type;

	string = &buffer[0];
	sprintf(string,"Built on %s %s, Rev-%s    Version %s %s", build_date, build_time, build_rev, build_platform, build_type);
#endif

	splash((void *)datatoc_splash_jpg, datatoc_splash_jpg_size, string);
}


/* Functions for user preferences fileselect windows */

/* yafray: export dir select */
static void filesel_u_yfexportdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.yfexportdir, dir);
	allqueue(REDRAWALL, 0);
}

static void filesel_u_fontdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];
	
	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.fontdir, dir);
	allqueue(REDRAWALL, 0);
}

static void filesel_u_textudir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.textudir, dir);
	allqueue(REDRAWALL, 0);
}

static void filesel_u_plugtexdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.plugtexdir, dir);
	allqueue(REDRAWALL, 0);
}

static void filesel_u_plugseqdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.plugseqdir, dir);
	allqueue(REDRAWALL, 0);
}

static void filesel_u_renderdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.renderdir, dir);
	allqueue(REDRAWALL, 0);
}

static void filesel_u_pythondir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);
	
	strcpy(U.pythondir, dir);
	allqueue(REDRAWALL, 0);
	
	/* act on the change */
	if (BPY_path_update()==0) {
		error("Invalid scripts dir: check console");
	}
}

static void filesel_u_sounddir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.sounddir, dir);
	allqueue(REDRAWALL, 0);
}

static void filesel_u_tempdir(char *name)
{
	char dir[FILE_MAXDIR], file[FILE_MAXFILE];

	BLI_cleanup_dir(G.sce, name);
	BLI_split_dirfile(name, dir, file);

	strcpy(U.tempdir, dir);
	BLI_where_is_temp( btempdir, 1 );
	
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
	bAction *act;
	ID *id, *idtest, *from=NULL;
	ScrArea *sa;
	Brush *br;
	int nr= 1;
	
#ifdef INTERNATIONAL
	char buf[FILE_MAX];
#endif

	ob= OBACT;

	id= NULL;	/* id at null for texbrowse */


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
		if(ob==NULL) return;
		if(ob->id.lib) return;
		id= ob->data;
		if(id==NULL) return;

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
					}
					else if( ob->type==OB_ARMATURE) {
						armature_rebuild_pose(ob, ob->data);
					}
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
					
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
				
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
					
				BIF_undo_push("Browse Mesh");
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
	{
		void *lockpoin= NULL;
		short *menunr= 0;
		
		/* this is called now from Node editor too, buttons might not exist */
		if(curarea->spacetype==SPACE_NODE) {
			SpaceNode *snode= curarea->spacedata.first;
			menunr= &snode->menunr;
			lockpoin= snode->id;
		}
		else if(G.buts) {
			menunr= &G.buts->menunr;
			lockpoin= G.buts->lockpoin;
		}
		else return;
		
		if(*menunr== -2) {
			if(G.qual & LR_CTRLKEY) {
				activate_databrowse_imasel((ID *)lockpoin, ID_MA, 0, B_MATBROWSE, menunr, do_global_buttons);
			}
			else {
				activate_databrowse((ID *)lockpoin, ID_MA, 0, B_MATBROWSE, menunr, do_global_buttons);
			}
			return;
		}
		
		if(*menunr < 0) return;
		
		if(0) {	/* future pin */
			
		}
		else {
			
			ma= give_current_material(ob, ob->actcol);
			nr= 1;
			
			id= (ID *)ma;
			
			idtest= G.main->mat.first;
			while(idtest) {
				if(nr== *menunr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}
			if(idtest==0) { /* new mat */
				if(id)	idtest= (ID *)copy_material((Material *)id);
				else {
					idtest= (ID *)add_material("Material");
				}
				idtest->us--;
			}
			if(idtest!=id) {
				assign_material(ob, (Material *)idtest, ob->actcol);
				
				BIF_undo_push("Browse Material");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWNODE, 0);
				BIF_preview_changed(ID_MA);
			}
			
		}
	}
		break;
	case B_MATDELETE:
		if(0) {	/* future pin */
			
		}
		else {
			ma= give_current_material(ob, ob->actcol);
			if(ma) {
				assign_material(ob, 0, ob->actcol);
				BIF_undo_push("Unlink Material");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWOOPS, 0);
				allqueue(REDRAWVIEW3D, 0);
				BIF_preview_changed(ID_MA);
			}
		}
		break;
	case B_TEXDELETE:
		if(G.buts->pin) {
			
		}
		else {
			if(G.buts->texfrom==0) {	/* from mat */
				ma= give_current_material(ob, ob->actcol);
				ma= editnode_get_active_material(ma);
				if(ma) {
					mtex= ma->mtex[ ma->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						ma->mtex[ ma->texact ]= NULL;
						allqueue(REDRAWBUTSSHADING, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(ID_MA);
					}
				}
			}
			else if(G.buts->texfrom==1) { /* from world */
				wrld= G.scene->world;
				if(wrld) {
					mtex= wrld->mtex[ wrld->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						wrld->mtex[ wrld->texact ]= NULL;
						allqueue(REDRAWBUTSSHADING, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(ID_WO);
					}
				}
			}
			else if(G.buts->texfrom==2) {	/* from lamp */
				la= ob->data;
				if(la && ob->type==OB_LAMP) { /* to be sure */
					mtex= la->mtex[ la->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						la->mtex[ la->texact ]= NULL;
						allqueue(REDRAWBUTSSHADING, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(ID_LA);
					}
				}
			}
			else {	/* from brush */
				br= G.scene->toolsettings->imapaint.brush;
				if(G.f & G_SCULPTMODE) {
					sculptmode_rem_tex(NULL, NULL);
					allqueue(REDRAWBUTSSHADING, 0);
				} else if(br) {
					mtex= br->mtex[ br->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						br->mtex[ br->texact ]= NULL;
						allqueue(REDRAWBUTSSHADING, 0);
						allqueue(REDRAWIMAGE, 0);
						allqueue(REDRAWIPO, 0);
						/*BIF_preview_changed(ID_BR);*/
					}
				}
			}
			BIF_undo_push("Unlink Texture");
		}
		break;
	case B_EXTEXBROWSE: 
	case B_TEXBROWSE:

		if(G.buts->texnr== -2) {
			
			id= G.buts->lockpoin;
			if(event==B_EXTEXBROWSE) {
				id= NULL;
				ma= give_current_material(ob, ob->actcol);
				ma= editnode_get_active_material(ma);
				if(ma) {
					mtex= ma->mtex[ ma->texact ];
					if(mtex) id= (ID *)mtex->tex;
				}
			}
			if(G.qual & LR_CTRLKEY) {
				activate_databrowse_imasel(id, ID_TE, 0, B_TEXBROWSE, &G.buts->texnr, do_global_buttons);
			}
			else {
				activate_databrowse(id, ID_TE, 0, B_TEXBROWSE, &G.buts->texnr, do_global_buttons);
			}
			return;
		}
		if(G.buts->texnr < 0) break;
		
		if(G.buts->pin) {
			
		}
		else {
			id= NULL;
			
			ma= give_current_material(ob, ob->actcol);
			ma= editnode_get_active_material(ma);
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
			if(idtest==0) { /* new tex */
				if(id)	idtest= (ID *)copy_texture((Tex *)id);
				else idtest= (ID *)add_texture("Tex");
				idtest->us--;
			}
			if(idtest!=id && ma) {
				
				if( ma->mtex[ma->texact]==0) ma->mtex[ma->texact]= add_mtex();
				
				ma->mtex[ ma->texact ]->tex= (Tex *)idtest;
				id_us_plus(idtest);
				if(id) id->us--;
				
				BIF_undo_push("Browse Texture");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWOOPS, 0);
				BIF_preview_changed(ID_MA);
			}
		}
		break;
	case B_ACTIONDELETE:
		/* only available when not pinned */
		if (G.saction->pin == 0) {
			act= ob->action;
			
			/* decrement user-count of action (as ob no longer uses it) */
			if (act)
				act->id.us--;
			
			/* make sure object doesn't hold reference to it anymore */	
			ob->action=NULL;
			if (ob->pose) {		/* clear flag (POSE_LOC/ROT/SIZE), also used for draw colors */
				bPoseChannel *pchan;
				for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next)
					pchan->flag= 0;
			}
			
			BIF_undo_push("Unlink Action");
			
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWBUTSEDIT, 0);
		}
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
		
		if (G.saction->actnr < 0) break;
		
		/*	See if we have selected a valid action */
		for (idtest= G.main->action.first; idtest; idtest= idtest->next) {
			if (nr==G.saction->actnr)
				break;
			nr++;
		}

		if (G.saction->pin) {
			if (idtest == NULL) {
				/* assign new/copy of pinned action only - messy as it doesn't assign to any obj's */
				if (G.saction->action)
					G.saction->action= (bAction *)copy_action(G.saction->action);
				else
					G.saction->action= (bAction *)add_empty_action("PinnedAction");
			}
			else {
				G.saction->action= (bAction *)idtest;
			}
			allqueue(REDRAWACTION, 0);
		}
		else {
			/* Store current action */
			if (idtest == NULL) {
				/* 'Add New' option: 
				 * 	- make a copy of an exisiting action
				 *	- or make a new empty action if no existing action
				 */
				if (act) {
					idtest= (ID *)copy_action(act);
				} 
				else { 
					if ((ob->ipo) && (ob->ipoflag & OB_ACTION_OB)==0) {
						/* object ipo - like if B_IPO_ACTION_OB is triggered */
						bActionChannel *achan;
						
						if (has_ipo_code(ob->ipo, OB_LAY))
							notice("Note: Layer Ipo doesn't work in Actions");
						
						ob->ipoflag |= OB_ACTION_OB;
						
						act = add_empty_action("ObAction");
						idtest=(ID *)act;
						
						
						achan= verify_action_channel(act, "Object");
						achan->flag = (ACHAN_HILIGHTED|ACHAN_SELECTED|ACHAN_EXPANDED|ACHAN_SHOWIPO);
						
						if (achan->ipo==NULL) {
							achan->ipo= ob->ipo;
							ob->ipo= NULL;
							
							allqueue(REDRAWIPO, 0);
							allqueue(REDRAWOOPS, 0);
						}
						
						/* object constraints */
						if (ob->constraintChannels.first) {
							free_constraint_channels(&achan->constraintChannels);
							achan->constraintChannels= ob->constraintChannels;
							ob->constraintChannels.first= ob->constraintChannels.last= NULL;
						}
					}
					else if (ELEM(ob->type, OB_MESH, OB_LATTICE) && ob_get_key(ob)) {
						/* shapekey - like if B_IPO_ACTION_KEY is triggered */
						bActionChannel *achan;
						Key *key= ob_get_key(ob);
						
						ob->ipoflag |= OB_ACTION_KEY;
						
						act = add_empty_action("ShapeAction");
						idtest=(ID *)act;
						
						achan= verify_action_channel(act, "Shape");
						achan->flag = (ACHAN_HILIGHTED|ACHAN_SELECTED|ACHAN_EXPANDED|ACHAN_SHOWIPO);
						
						if ((achan->ipo==NULL) && (key->ipo)) {
							achan->ipo= key->ipo;
							key->ipo= NULL;
							
							allqueue(REDRAWIPO, 0);
							allqueue(REDRAWOOPS, 0);
						}
					}
					else {
						/* a plain action */
						idtest=(ID *)add_empty_action("Action");
					}
				}
				idtest->us--;
			}
			
			
			if ((idtest!=id) && (ob)) {
				act= (bAction *)idtest;
				
				ob->action= act;
				id_us_plus(idtest);
				
				if (id) id->us--;
				
				/* Update everything */
				BIF_undo_push("Browse Action");
				do_global_buttons (B_NEWFRAME);
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWNLA, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWHEADERS, 0); 
				allqueue(REDRAWBUTSEDIT, 0);
			}
		}
		
		break;
	case B_IPOBROWSE:

		ipo= G.sipo->ipo;
		from= G.sipo->from;
		id= (ID *)ipo;
		if(from==NULL) return;

		if(G.sipo->menunr== -2) {
			activate_databrowse((ID *)G.sipo->ipo, ID_IP, G.sipo->blocktype, B_IPOBROWSE, &G.sipo->menunr, do_global_buttons);
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
					nr= G.sipo->blocktype;
					if(nr==ID_OB) idtest= (ID *)add_ipo("ObIpo", ID_OB);
					else if(nr==ID_CO) idtest= (ID *)add_ipo("CoIpo", ID_CO);
					else if(nr==ID_PO) idtest= (ID *)add_ipo("ActIpo", nr);
					else if(nr==ID_MA) idtest= (ID *)add_ipo("MatIpo", nr);
					else if(nr==ID_TE) idtest= (ID *)add_ipo("TexIpo", nr);
					else if(nr==ID_SEQ) idtest= (ID *)add_ipo("MatSeq", nr);
					else if(nr==ID_CU) idtest= (ID *)add_ipo("CuIpo", nr);
					else if(nr==ID_KE) idtest= (ID *)add_ipo("KeyIpo", nr);
					else if(nr==ID_WO) idtest= (ID *)add_ipo("WoIpo", nr);
					else if(nr==ID_LA) idtest= (ID *)add_ipo("LaIpo", nr);
					else if(nr==ID_CA) idtest= (ID *)add_ipo("CaIpo", nr);
					else if(nr==ID_SO) idtest= (ID *)add_ipo("SndIpo", nr);
					else if(nr==ID_FLUIDSIM) idtest= (ID *)add_ipo("FluidsimIpo", nr);
					else if(nr==ID_PA) idtest= (ID *)add_ipo("PaIpo", nr);
					else error("Warn bugtracker!");
				}
				idtest->us--;
			}
			if(idtest!=id && from) {
				spaceipo_assign_ipo(G.sipo, (Ipo *)idtest);
				
				BIF_undo_push("Browse Ipo");
			}
		}
		break;
	case B_IPODELETE:
		ipo= G.sipo->ipo;
		from= G.sipo->from;
		
		spaceipo_assign_ipo(G.sipo, NULL);
		
		editipo_changed(G.sipo, 1); /* doredraw */
		
		BIF_undo_push("Unlink Ipo");
		
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
		if(idtest==0) { /* new world */
			if(id) idtest= (ID *)copy_world((World *)id);
			else idtest= (ID *)add_world("World");
			idtest->us--;
		}
		if(idtest!=id) {
			G.scene->world= (World *)idtest;
			id_us_plus(idtest);
			if(id) id->us--;
			
			BIF_undo_push("Browse World");
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(ID_WO);
		}
		break;
	case B_WORLDDELETE:
		if(G.scene->world) {
			G.scene->world->id.us--;
			G.scene->world= NULL;

			BIF_undo_push("Unlink World");
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWIPO, 0);
		}
		
		break;
	case B_WTEXBROWSE:

		if(G.buts->texnr== -2) {
			id= NULL;
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
			id= NULL;
			
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
			if(idtest==0) { /* new tex */
				if(id)	idtest= (ID *)copy_texture((Tex *)id);
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
				
				BIF_undo_push("Texture browse");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWOOPS, 0);
				BIF_preview_changed(ID_WO);
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
		if(idtest==0) { /* no new lamp */
			return;
		}
		if(idtest!=id) {
			ob->data= (Lamp *)idtest;
			id_us_plus(idtest);
			if(id) id->us--;
			
			BIF_undo_push("Lamp browse");
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWOOPS, 0);
			BIF_preview_changed(ID_LA);
		}
		break;
	
	case B_LTEXBROWSE:

		if(ob==0) return;
		if(ob->type!=OB_LAMP) return;

		if(G.buts->texnr== -2) {
			id= NULL;
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
			id= NULL;
			
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
			if(idtest==0) { /* new tex */
				if(id)	idtest= (ID *)copy_texture((Tex *)id);
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
				
				BIF_undo_push("Texture Browse");
				allqueue(REDRAWBUTSSHADING, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWOOPS, 0);
				BIF_preview_changed(ID_LA);
			}
		}
		break;

	case B_IMAGEDELETE:
		if(G.sima->image && (G.sima->image->type == IMA_TYPE_R_RESULT || G.sima->image->type == IMA_TYPE_COMPOSITE)) {
			/* Run if G.sima is render, remove the render and display the meshes image if it exists */
			G.sima->image= NULL;
			what_image(G.sima);
			allqueue(REDRAWIMAGE, 0);
		} else {
			/* Run on non render images, unlink normally */
			image_changed(G.sima, NULL);
			BIF_undo_push("Unlink Image");
			allqueue(REDRAWIMAGE, 0);
		}
		break;
	
	case B_AUTOMATNAME:
		/* this is called now from Node editor too, buttons might not exist */
		if(curarea->spacetype==SPACE_NODE) {
			SpaceNode *snode= curarea->spacedata.first;
			automatname((Material *)snode->id);
		}
		else if(G.buts) {
			automatname(G.buts->lockpoin);
		}
		else return;

		BIF_undo_push("Auto name");
		allqueue(REDRAWBUTSSHADING, 0);
		allqueue(REDRAWNODE, 0);
		allqueue(REDRAWOOPS, 0);
		break;		
	case B_AUTOTEXNAME:
		if(G.buts->mainb==CONTEXT_SHADING) {
			if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_TEX) {
				autotexname(G.buts->lockpoin);
			}
			else if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_MAT) {
				ma= G.buts->lockpoin;
				if(ma->mtex[ ma->texact]) autotexname(ma->mtex[ma->texact]->tex);
			}
			else if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_WORLD) {
				wrld= G.buts->lockpoin;
				if(wrld->mtex[ wrld->texact]) autotexname(wrld->mtex[wrld->texact]->tex);
			}
			else if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_LAMP) {
				la= G.buts->lockpoin;
				if(la->mtex[ la->texact]) autotexname(la->mtex[la->texact]->tex);
			}
			BIF_undo_push("Auto name");
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWIMAGE, 0);
		}
		else if(G.buts->mainb==CONTEXT_EDITING) {
			SculptData *sd= &G.scene->sculptdata;
			if(sd && sd->texact != -1) {
				if(sd->mtex[sd->texact]) autotexname(sd->mtex[sd->texact]->tex);

				BIF_undo_push("Auto name");
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWOOPS, 0);
			}
		}
		break;

	case B_RESETAUTOSAVE:
		reset_autosave();
		allqueue(REDRAWINFO, 0);
		break;
	case B_SOUNDTOGGLE:
		SYS_WriteCommandLineInt(SYS_GetSystem(), "noaudio", (U.gameflags & USER_DISABLE_SOUND));
		break;
	case B_SHOWSPLASH:
				show_splash();
		break;
	case B_MIPMAPCHANGED:
		set_mipmap(!(U.gameflags & USER_DISABLE_MIPMAP));
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_GLRESLIMITCHANGED:
		free_all_realtime_images(); /* force reloading with new res limit */
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_NEWSPACE:
		newspace(curarea, curarea->butspacetype);
		reset_filespace(curarea);
		reset_imaselspace(curarea);
		break;
	case B_LOADTEMP:	/* is button from space.c */
		BIF_read_autosavefile();
		break;

	case B_USERPREF:
		allqueue(REDRAWINFO, 0);
		break;

	case B_DRAWINFO:	/* is button from space.c  *info* */
		allqueue(REDRAWVIEW3D, 0);
		break;

	case B_PLAINMENUS:     /* is button from space.c  *info* */
		reset_toolbox();
		break;

	case B_FLIPINFOMENU:	/* is button from space.c  *info* */
		scrarea_queue_headredraw(curarea);
		break;

#if 0
//#ifdef _WIN32 // FULLSCREEN
	case B_FLIPFULLSCREEN:
		if(U.uiflag & USER_FLIPFULLSCREEN)
			U.uiflag &= ~USER_FLIPFULLSCREEN;
		else
			U.uiflag |= USER_FLIPFULLSCREEN;
		mainwindow_toggle_fullscreen((U.uiflag & USER_FLIPFULLSCREEN));
		break;
#endif

	/* Fileselect windows for user preferences file paths */

	/* yafray: xml export dir. select */
	case B_YAFRAYDIRFILESEL:	/* space.c */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT YFEXPORT PATH", U.yfexportdir, filesel_u_yfexportdir);
		break;

	case B_FONTDIRFILESEL:	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT FONT PATH", U.fontdir, filesel_u_fontdir);
		break;

	case B_TEXTUDIRFILESEL:		/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEXTURE PATH", U.textudir, filesel_u_textudir);
		break;
	
	case B_PLUGTEXDIRFILESEL:		/* is button form space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEX PLUGIN PATH", U.plugtexdir, filesel_u_plugtexdir);
		break;
	
	case B_PLUGSEQDIRFILESEL:		/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SEQ PLUGIN PATH", U.plugseqdir, filesel_u_plugseqdir);
		break;
	
	case B_RENDERDIRFILESEL:	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT RENDER PATH", U.renderdir, filesel_u_renderdir);
		break;

	case B_PYMENUEVAL: /* is button from space.c *info* */
		waitcursor( 1 ); /* can take some time */
		if (BPY_path_update() == 0) { /* re-eval scripts registration in menus */
			waitcursor( 0 );
			error("Invalid scripts dir: check console");
		}
		waitcursor( 0 );
		break;
	case B_PYTHONDIRFILESEL:	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SCRIPT PATH", U.pythondir, filesel_u_pythondir);
		break;

	case B_SOUNDDIRFILESEL:		/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT SOUND PATH", U.sounddir, filesel_u_sounddir);
		break;

	case B_TEMPDIRFILESEL:	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}

		activate_fileselect(FILE_SPECIAL, "SELECT TEMP FILE PATH", U.tempdir, filesel_u_tempdir);
		break;

	/* END Fileselect windows for user preferences file paths */

#ifdef INTERNATIONAL
	case B_LOADUIFONT:	/* is button from space.c  *info* */
		if(curarea->spacetype==SPACE_INFO) {
			sa= closest_bigger_area();
			areawinset(sa->win);
		}
		BLI_make_file_string("/", buf, U.fontdir, U.fontname);
		activate_fileselect(FILE_SPECIAL, "LOAD UI FONT", buf, set_interface_font);
		break;

	case B_SETLANGUAGE:		/* is button from space.c  *info* */
		lang_setlanguage();
		allqueue(REDRAWALL, 0);
		break;

	case B_SETFONTSIZE:		/* is button from space.c  *info* */
		refresh_interface_font();
		FTF_SetSize(U.fontsize); 
		allqueue(REDRAWALL, 0);
		break;
		
	case B_SETTRANSBUTS:	/* is button from space.c  *info* */
		allqueue(REDRAWALL, 0);
		break;

	case B_RESTOREFONT:		/* is button from space.c  *info* */
		U.fontsize= 0;
		start_interface_font();
		allqueue(REDRAWALL, 0);
		break;
		
	case B_USETEXTUREFONT:		/* is button from space.c  *info* */
		refresh_interface_font();
		allqueue(REDRAWALL, 0);
		break;

	case B_DOLANGUIFONT:	/* is button from space.c  *info* */
		if(U.transopts & USER_DOTRANSLATE)
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
		if (ob && ob->type==OB_MBALL) {
			DAG_scene_sort(G.scene);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
		/* redraw because name has changed: new pup */
		scrarea_queue_headredraw(curarea);
		allqueue(REDRAWINFO, 1);
		allqueue(REDRAWOOPS, 1);
		allqueue(REDRAWACTION, 1);
		allqueue(REDRAWNLA, 1);
		/* name scene also in set PUPmenu */
		allqueue(REDRAWBUTSALL, 0);
		allqueue(REDRAWIMAGE, 0);
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
		} else if(curarea->spacetype==SPACE_NODE) {
			id = ((SpaceNode *)curarea->spacedata.first)->id;
		} else if(curarea->spacetype==SPACE_ACTION) {
			id= (ID *)G.saction->action;
		}/* similar for other spacetypes ? */
		if (id) {
			/* flag was already toggled, just need to update user count */
			if(id->flag & LIB_FAKEUSER)
				id->us++;
			else
				id->us--;
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
	Brush *br;

	/* general:  Single User is allowed when from==LOCAL 
	 *			 Make Local is allowed when (from==LOCAL && id==LIB)
	 */
		
	if(event<B_LOCAL_ALONE) return;

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
					armature_rebuild_pose(ob, ob->data);
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
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWBUTSEDIT, 0);
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
					act->id.us--;
					allqueue(REDRAWACTION, 0);
					allqueue(REDRAWBUTSEDIT, 0);
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

					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);				
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
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
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
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
				}
			}
		}
		break;
		
	case B_TEXALONE:
		if(G.buts->texfrom==0) {	/* from mat */
			if(ob==0) return;
			ma= give_current_material(ob, ob->actcol);
			ma= editnode_get_active_material(ma);
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
		else if(G.buts->texfrom==1) { /* from world */
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
		else if(G.buts->texfrom==2) { /* from lamp */
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
		else if(G.buts->texfrom==3) { /* from brush */
			br= G.scene->toolsettings->imapaint.brush;
			if(br==0) return;
			if(br->id.lib==0) {
				mtex= br->mtex[ br->texact ];
				if(mtex->tex && mtex->tex->id.us>1) {
					if(okee("Single user")) {
						mtex->tex->id.us--;
						mtex->tex= copy_texture(mtex->tex);
						allqueue(REDRAWIMAGE, 0);
					}
				}
			}
		}
		break;
	case B_TEXLOCAL:
		if(G.buts->texfrom==0) {	/* from mat */
			if(ob==0) return;
			ma= give_current_material(ob, ob->actcol);
			ma= editnode_get_active_material(ma);
			if(ma && ma->id.lib==0) {
				mtex= ma->mtex[ ma->texact ];
				if(mtex->tex && mtex->tex->id.lib) {
					if(okee("Make local")) {
						make_local_texture(mtex->tex);
					}
				}
			}
		}
		else if(G.buts->texfrom==1) { /* from world */
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
		else if(G.buts->texfrom==2) { /* from lamp */
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
		else if(G.buts->texfrom==3) { /* from brush */
			br= G.scene->toolsettings->imapaint.brush;
			if(br==0) return;
			if(br->id.lib==0) {
				mtex= br->mtex[ br->texact ];
				if(mtex->tex && mtex->tex->id.lib) {
					if(okee("Make local")) {
						make_local_texture(mtex->tex);
						allqueue(REDRAWIMAGE, 0);
					}
				}
			}
		}
		break;
	
	case B_IPOALONE:
		ipo= G.sipo->ipo;
		idfrom= G.sipo->from;
		
		if(idfrom && idfrom->lib==NULL) {
			if(ipo->id.us>1) {
				if(okee("Single user")) {
					ipo= copy_ipo(ipo);
					ipo->id.us= 0;	/* assign_ipo adds users, copy_ipo sets to 1 */
					spaceipo_assign_ipo(G.sipo, ipo);
					allqueue(REDRAWIPO, 0);
				}
			}
		}
		break;
	case B_IPOLOCAL:
		ipo= G.sipo->ipo;
		idfrom= G.sipo->from;
		
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
					
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
					
					if(ob==G.obedit) allqueue(REDRAWVIEW3D, 0);
				}
			}
		}
		break;
	}
	
	BIF_undo_push("Make single user or local");
	allqueue(REDRAWBUTSALL, 0);
	allqueue(REDRAWOOPS, 0);
}

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
	else if(event<525) do_text_buttons(event);
	else if(event<550) do_script_buttons(event);
	else if(event<600) do_file_buttons(event);
	else if(event<650) do_seq_buttons(event);
	else if(event<700) do_sound_buttons(event);
	else if(event<750) do_action_buttons(event);
	else if(event<800) do_time_buttons(curarea, event);
	else if(event<850) do_nla_buttons(event);
	else if(event<900) do_node_buttons(curarea, event);
	else if(event>=REDRAWVIEW3D) allqueue(event, 0);
}

