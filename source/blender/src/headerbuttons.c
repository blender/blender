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
#include "DNA_script_types.h"
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

#include "BPY_extern.h"

#include "mydevice.h"
#include "blendef.h"
#include "render.h"
#include "ipo.h"
#include "nla.h"	/* __NLA : To be removed later */
#include "butspace.h"  // test_idbutton

#include "TPT_DependKludge.h"

#include "BIF_poseobject.h"

#include "SYS_System.h"

 /* WATCH IT:  always give all headerbuttons for same window the same name
	*			event B_REDR is a standard redraw
	*
	*/

char *windowtype_pup(void)
{
	static char string[1024];

	strcpy(string, "Window type:%t"); //14
	strcat(string, "|3D View %x1"); //30

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

	strcat(string, "|Image Browser %x10"); //273
	strcat(string, "|File Browser %x5"); //290

	strcat(string, "|%l"); //293

	strcat(string, "|Scripts Window %x14"); //313

	return (string);
}

int GetButStringLength(char *str) {
	int rt;

	rt= BIF_GetStringWidth(G.font, str, (U.transopts & TR_BUTTONS));

	return rt + 15;
}

/* ********************** GLOBAL ****************************** */

int std_libbuttons(uiBlock *block, short xco, short yco,
							int pin, short *pinpoin, int browse, ID *id,
							ID *parid, short *menupoin, int users, int lib,
							int del, int autobut, int keepbut)
{
	ListBase *lb;
	Object *ob;
	Ipo *ipo;
	uiBut *but;
	int len, idwasnul=0, idtype, oldcol, add_addbutton=0;
	char *str=NULL, str1[10];

	uiBlockBeginAlign(block);
	oldcol= uiBlockGetCol(block);

	if(id && pin) {
		uiDefIconButS(block, ICONTOG, pin, ICON_PIN_DEHLT, xco,yco,XIC,YIC, pinpoin, 0, 0, 0, 0, "Keeps this view displaying the current data regardless of what object is selected");
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
				if(ob) id= G.main->action.first;
			}
			else if(curarea->spacetype==SPACE_NLA) {
				id= NULL;
			}
			else if(curarea->spacetype==SPACE_IPO) {
				id= G.main->ipo.first;
				
				/* test for ipotype */
				while(id) {
					ipo= (Ipo *)id;
					if(G.sipo->blocktype==ipo->blocktype) break;
					id= id->next;
				}
				if(ob==NULL) {
					if(G.sipo->blocktype!=ID_SEQ && G.sipo->blocktype!=ID_WO) {
						id= NULL; 
						idwasnul= 0;
					}
				}
			}
			else if(curarea->spacetype==SPACE_BUTS) {
				if(browse==B_WORLDBROWSE) {
					id= G.main->world.first;
				}
				else if(ob && ob->type && (ob->type<=OB_LAMP)) {
					if(G.buts->mainb==CONTEXT_SHADING) {
						int tab= G.buts->tab[CONTEXT_SHADING];
						
						if(tab==TAB_SHADING_MAT) id= G.main->mat.first;
						else if(tab==TAB_SHADING_TEX) id= G.main->tex.first;
						
						add_addbutton= 1;
					}
				}
			}
			else if(curarea->spacetype==SPACE_TEXT) {
				id= G.main->text.first;
			}
			else if(curarea->spacetype==SPACE_SCRIPT) {
				id= G.main->script.first;
			}
		}
		if(id) {
			char *extrastr= NULL;
			
			idtype= GS(id->name);
			lb= wich_libbase(G.main, GS(id->name));
			
			if(idwasnul) id= NULL;
			else if(id->us>1) uiBlockSetCol(block, TH_BUT_SETTING1);

			if (pin && *pinpoin) {
				uiBlockSetCol(block, TH_BUT_SETTING2);
			}
			
			if ELEM7( idtype, ID_SCE, ID_SCR, ID_MA, ID_TE, ID_WO, ID_IP, ID_AC) extrastr= "ADD NEW %x 32767";
			else if (idtype==ID_TXT) extrastr= "OPEN NEW %x 32766 |ADD NEW %x 32767";
			else if (idtype==ID_SO) extrastr= "OPEN NEW %x 32766";
			
			uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
			if( idtype==ID_SCE || idtype==ID_SCR ) uiClearButLock();
			
			if(curarea->spacetype==SPACE_BUTS)
				uiSetButLock(idtype!=ID_SCR && G.obedit!=0 && G.buts->mainb==CONTEXT_EDITING, NULL);
			
			if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");

			if (lb) {
				if( idtype==ID_IP)
					IPOnames_to_pupstring(&str, NULL, extrastr, lb, id, menupoin, G.sipo->blocktype);
				else
					IDnames_to_pupstring(&str, NULL, extrastr, lb, id, menupoin);
			}
			
			uiDefButS(block, MENU, browse, str, xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses existing choices or adds NEW");
			
			uiClearButLock();
		
			MEM_freeN(str);
		}
		else if(curarea->spacetype==SPACE_BUTS) {
			if(G.buts->mainb==CONTEXT_SHADING) {
				uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
				if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");
				uiDefButS(block, MENU, browse, "ADD NEW %x 32767",xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
				uiClearButLock();
			} else if (G.buts->mainb == CONTEXT_SCENE) {
				if(G.buts->tab[CONTEXT_SCENE]== TAB_SCENE_SOUND) {
					uiDefButS(block, MENU, browse, "OPEN NEW %x 32766",xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
				}
			}
		}
		else if(curarea->spacetype==SPACE_TEXT) {
			uiDefButS(block, MENU, browse, "OPEN NEW %x 32766 | ADD NEW %x 32767", xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
		}
		else if(curarea->spacetype==SPACE_SCRIPT) {
			uiDefButS(block, MENU, browse, "No running scripts", xco, yco, XIC, YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
		}
		else if(curarea->spacetype==SPACE_SOUND) {
			uiDefButS(block, MENU, browse, "OPEN NEW %x 32766",xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
		}
		else if(curarea->spacetype==SPACE_ACTION) {
			uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
			if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");

			uiDefButS(block, MENU, browse, "ADD NEW %x 32767", xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
			uiClearButLock();
		}
		else if(curarea->spacetype==SPACE_IPO) {
			if(idwasnul) {
				uiSetButLock(G.scene->id.lib!=0, "Can't edit library data");
				if(parid) uiSetButLock(parid->lib!=0, "Can't edit library data");
	
				uiDefButS(block, MENU, browse, "ADD NEW %x 32767", xco,yco,XIC,YIC, menupoin, 0, 0, 0, 0, "Browses Datablock");
				uiClearButLock();
			}
		}
		
		xco+= XIC;
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

		uiSetButLock(id->lib!=0, "Can't edit library data");
		
		str1[0]= id->name[0];
		str1[1]= id->name[1];
		str1[2]= ':';
		str1[3]= 0;
		if(strcmp(str1, "SC:")==0) strcpy(str1, "SCE:");
		else if(strcmp(str1, "SR:")==0) strcpy(str1, "SCR:");
		
		if( GS(id->name)==ID_IP) len= 110;
		else if(yco) len= 140;	// comes from button panel
		else len= 120;
		
		but= uiDefBut(block, TEX, B_IDNAME, str1,xco, yco, (short)len, YIC, id->name+2, 0.0, 19.0, 0, 0, "Displays current Datablock name. Click to change.");
		uiButSetFunc(but, test_idbutton_cb, id->name, NULL);

		uiClearButLock();

		xco+= len;
		
		if(id->lib) {
			
			if(parid && parid->lib) uiDefIconBut(block, BUT, 0, ICON_DATALIB,xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Displays name of the current Indirect Library Datablock. Click to change.");
			else uiDefIconBut(block, BUT, lib, ICON_PARLIB, xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Displays current Library Datablock name. Click to make local.");
			
			xco+= XIC;
		}
		
		
		if(users && id->us>1) {
			uiSetButLock (pin && *pinpoin, "Can't make pinned data single-user");
			
			sprintf(str1, "%d", id->us);
			if(id->us<100) {
				
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
			uiDefBut(block, BUT, keepbut, "F", xco,yco,XIC,YIC, 0, 0, 0, 0, 0, "Saves this datablock even if it has no users");  
			xco+= XIC;
		}
	}
	else if(add_addbutton) {	/* "add new" button */
		uiBlockSetCol(block, oldcol);
		uiDefButS(block, TOG, browse, "Add New" ,xco, yco, 110, YIC, menupoin, (float)*menupoin, 32767.0, 0, 0, "Add new data block");
		xco+= 110;
	}
	xco+=XIC;
	
	uiBlockSetCol(block, oldcol);
	uiBlockEndAlign(block);

	return xco;
}

void do_update_for_newframe(int mute)
{
	extern void audiostream_scrub(unsigned int frame);	/* seqaudio.c */
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION,0);
	allqueue(REDRAWNLA,0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWINFO, 1);
	allqueue(REDRAWSEQ, 1);
	allqueue(REDRAWSOUND, 1);
	allqueue(REDRAWBUTSHEAD, 0);
	allqueue(REDRAWBUTSSHADING, 0);
	allqueue(REDRAWBUTSOBJECT, 0);

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
			if(idtest==0) { /* new mat */
				if(id)	idtest= (ID *)copy_material((Material *)id);
				else {
					idtest= (ID *)add_material("Material");
				}
				idtest->us--;
			}
			if(idtest!=id) {
				assign_material(ob, (Material *)idtest, ob->actcol);
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSSHADING, 0);
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
				allqueue(REDRAWBUTSSHADING, 0);
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
						allqueue(REDRAWBUTSSHADING, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(G.buts);
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
						wrld->mtex[ wrld->texact ]= 0;
						allqueue(REDRAWBUTSSHADING, 0);
						allqueue(REDRAWIPO, 0);
						BIF_preview_changed(G.buts);
					}
				}
			}
			else {	/* from lamp */
				la= ob->data;
				if(la && ob->type==OB_LAMP) { /* to be sure */
					mtex= la->mtex[ la->texact ];
					if(mtex) {
						if(mtex->tex) mtex->tex->id.us--;
						MEM_freeN(mtex);
						la->mtex[ la->texact ]= 0;
						allqueue(REDRAWBUTSSHADING, 0);
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
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSSHADING, 0);
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
							idtest= (ID *)add_ipo("CoIpo", IPO_CO); /* BLEARGH! */
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
					allqueue(REDRAWBUTSSHADING, 0);
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
					allqueue(REDRAWBUTSSHADING, 0);
				}
				else if(ipo->blocktype==ID_LA) {
					( (Lamp *)from)->ipo= ipo;
					id_us_plus(idtest);
					allqueue(REDRAWBUTSSHADING, 0);
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
		
		editipo_changed(G.sipo, 1); /* doredraw */
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
		if(idtest==0) { /* new world */
			if(id) idtest= (ID *)copy_world((World *)id);
			else idtest= (ID *)add_world("World");
			idtest->us--;
		}
		if(idtest!=id) {
			G.scene->world= (World *)idtest;
			id_us_plus(idtest);
			if(id) id->us--;
			
			allqueue(REDRAWBUTSHEAD, 0);
			allqueue(REDRAWBUTSSHADING, 0);
			allqueue(REDRAWIPO, 0);
			BIF_preview_changed(G.buts);
		}
		break;
	case B_WORLDDELETE:
		if(G.scene->world) {
			G.scene->world->id.us--;
			G.scene->world= 0;
			allqueue(REDRAWBUTSSHADING, 0);
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
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSSHADING, 0);
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
		if(idtest==0) { /* no new lamp */
			return;
		}
		if(idtest!=id) {
			ob->data= (Lamp *)idtest;
			id_us_plus(idtest);
			if(id) id->us--;
			
			allqueue(REDRAWBUTSHEAD, 0);
			allqueue(REDRAWBUTSSHADING, 0);
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
				
				allqueue(REDRAWBUTSHEAD, 0);
				allqueue(REDRAWBUTSSHADING, 0);
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
		if(G.buts->mainb==CONTEXT_SHADING) {
			if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_TEX) {
				autotexname(G.buts->lockpoin);
				allqueue(REDRAWBUTSSHADING, 0);
			}
			else if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_MAT) {
				ma= G.buts->lockpoin;
				if(ma->mtex[ ma->texact]) autotexname(ma->mtex[ma->texact]->tex);
				allqueue(REDRAWBUTSSHADING, 0);
			}
			else if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_WORLD) {
				wrld= G.buts->lockpoin;
				if(wrld->mtex[ wrld->texact]) autotexname(wrld->mtex[wrld->texact]->tex);
				allqueue(REDRAWBUTSSHADING, 0);
			}
			else if(G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_LAMP) {
				la= G.buts->lockpoin;
				if(la->mtex[ la->texact]) autotexname(la->mtex[la->texact]->tex);
				allqueue(REDRAWBUTSSHADING, 0);
			}
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
	case B_LOADTEMP:	/* is button from space.c */
		BIF_read_autosavefile();
		break;

	case B_USERPREF:
		allqueue(REDRAWINFO, 0);
		break;

	case B_DRAWINFO:	/* is button from space.c  *info* */
		allqueue(REDRAWVIEW3D, 0);
		break;

	case B_FLIPINFOMENU:	/* is button from space.c  *info* */
		scrarea_queue_headredraw(curarea);
		break;

#ifdef _WIN32 // FULLSCREEN
	case B_FLIPFULLSCREEN:
		if(U.uiflag & FLIPFULLSCREEN)
			U.uiflag &= ~FLIPFULLSCREEN;
		else
			U.uiflag |= FLIPFULLSCREEN;
		mainwindow_toggle_fullscreen((U.uiflag & FLIPFULLSCREEN));
		break;
#endif

	/* Fileselect windows for user preferences file paths */

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
		FTF_SetSize(U.fontsize); 
		allqueue(REDRAWALL, 0);
		break;
		
	case B_SETTRANSBUTS:	/* is button from space.c  *info* */
		allqueue(REDRAWALL, 0);
		break;

	case B_DOLANGUIFONT:	/* is button from space.c  *info* */
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
	else if(event<800) do_action_buttons(event);
	else if(event<900) do_nla_buttons(event);
}

