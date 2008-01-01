/**
 * $Id:
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "MEM_CacheLimiterC-Api.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mball.h"
#include "BKE_utildefines.h"
#include "BKE_packedFile.h"

#include "BMF_Api.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BLI_blenlib.h"

#include "RE_pipeline.h"		/* RE_ free stuff */

#include "radio.h"

#include "BPY_extern.h"

#include "SYS_System.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_files.h"
#include "wm_window.h"

static void initbuttons(void)
{
//	uiDefFont(UI_HELVB, 
//			  BMF_GetFont(BMF_kHelveticaBold14), 
//			  BMF_GetFont(BMF_kHelveticaBold12), 
//			  BMF_GetFont(BMF_kHelveticaBold10), 
//			  BMF_GetFont(BMF_kHelveticaBold8));
//	uiDefFont(UI_HELV, 
//			  BMF_GetFont(BMF_kHelvetica12), 
//			  BMF_GetFont(BMF_kHelvetica12), 
//			  BMF_GetFont(BMF_kHelvetica10), 
//			  BMF_GetFont(BMF_kHelveticaBold8));
	
//	glClearColor(.7f, .7f, .6f, 0.0);
	
	G.font= BMF_GetFont(BMF_kHelvetica12);
	G.fonts= BMF_GetFont(BMF_kHelvetica10);
	G.fontss= BMF_GetFont(BMF_kHelveticaBold8);
	
//	clear_matcopybuf(); /* XXX */
	
//	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}

/* XXX */
static void sound_init_listener(void)
{
	G.listener = MEM_callocN(sizeof(bSoundListener), "soundlistener");
	G.listener->gain = 1.0;
	G.listener->dopplerfactor = 1.0;
	G.listener->dopplervelocity = 340.29f;
}

/* only called once, for startup */
void WM_init(bContext *C)
{
	
	wm_ghost_init(C);	/* note: it assigns C to ghost! */
	wm_operatortype_init();
	
	set_free_windowmanager_cb(wm_close_and_free);	/* library.c */

	/* get the default database, plus a wm */
	WM_read_homefile(C, 0);
	
	wm_check(C); /* opens window(s), checks keymaps */
	
//	initscreen();	/* for (visual) speed, this first, then setscreen */
	initbuttons();
//	InitCursorData();
	sound_init_listener();
//	init_node_butfuncs();
	
// XXX	BIF_preview_init_dbase();
	
	
// XXX	BIF_resources_init();	/* after homefile, to dynamically load an icon file based on theme settings */
	
// XXX	BIF_filelist_init_icons();
	
//	init_gl_stuff();	/* drawview.c, after homefile */
	read_Blog();
	BLI_strncpy(G.lib, G.sce, FILE_MAX);
}

/* free strings of open recent files */
static void free_openrecent(void)
{
	struct RecentFile *recent;
	
	for(recent = G.recent_files.first; recent; recent=recent->next)
		MEM_freeN(recent->filename);
	
	BLI_freelistN(&(G.recent_files));
}


/* bad stuff*/

extern ListBase editNurb;
extern ListBase editelems;
extern wchar_t *copybuf;
extern wchar_t *copybufinfo;

/* called in creator.c even... tsk, split this! */
void WM_exit(bContext *C)
{
	wm_operatortype_free();
	
	free_ttfont(); /* bke_font.h */
	
#ifdef WITH_VERSE
	end_all_verse_sessions();
#endif
	free_openrecent();
	
	freeAllRad();
	BKE_freecubetable();
	
//	if (G.background == 0)
//		sound_end_all_sounds();
	
	if(G.obedit) {
		if(G.obedit->type==OB_FONT) {
//			free_editText();
		}
//		else if(G.obedit->type==OB_MBALL) BLI_freelistN(&editelems);
//		free_editMesh(G.editMesh);
	}
	
//	free_editLatt();
//	free_editArmature();
//	free_posebuf();
	
	/* before free_blender so py's gc happens while library still exists */
	/* needed at least for a rare sigsegv that can happen in pydrivers */
//	BPY_end_python();
	
//	fastshade_free_render();	/* shaded view */
	free_blender();				/* blender.c, does entire library */
//	free_matcopybuf();
//	free_ipocopybuf();
//	free_actcopybuf();
//	free_vertexpaint();
//	free_imagepaint();
	
	/* editnurb can remain to exist outside editmode */
	freeNurblist(&editNurb);
	
//	fsmenu_free();
	
#ifdef INTERNATIONAL
//	free_languagemenu();
#endif
	
	RE_FreeAllRender();
	
//	free_txt_data();
	
//	sound_exit_audio();
	if(G.listener) MEM_freeN(G.listener);
	
	
	libtiff_exit();
	
#ifdef WITH_QUICKTIME
	quicktime_exit();
#endif
	
	if (!G.background) {
// XXX		BIF_resources_free();
		
// XXX		BIF_filelist_free_icons();
	}
	
#ifdef INTERNATIONAL
	FTF_End();
#endif
	
//	if (copybuf) MEM_freeN(copybuf);
//	if (copybufinfo) MEM_freeN(copybufinfo);
	
	/* undo free stuff */
//	undo_editmode_clear();
	
	BKE_undo_save_quit();	// saves quit.blend if global undo is on
	BKE_reset_undo(); 
	
	BLI_freelistN(&U.themes);
// XXX	BIF_preview_free_dbase();
	
	MEM_freeN(C);
	
	if(totblock!=0) {
		printf("Error Totblock: %d\n",totblock);
		MEM_printmemlist();
	}
//	delete_autosave();
	
	printf("\nBlender quit\n");
	
#ifdef WIN32   
	/* ask user to press enter when in debug mode */
	if(G.f & G_DEBUG) {
		printf("press enter key to exit...\n\n");
		getchar();
	}
#endif 
	
	
	SYS_DeleteSystem(SYS_GetSystem());
	
	exit(G.afbreek==1);
}

