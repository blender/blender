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

	/* placed up here because of crappy
	 * winsock stuff.
	 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#include <process.h> /* getpid */
#else
#include <unistd.h> /* getpid */
#endif
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"
#ifdef INTERNATIONAL
#include "BIF_language.h"
#endif

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_linklist.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_mesh_types.h"

#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_exotic.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_packedFile.h"
#include "BKE_utildefines.h"

#include "BIF_fsmenu.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_usiblender.h"
#include "BIF_drawtext.h"
#include "BIF_editarmature.h"
#include "BIF_editlattice.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_editsound.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_drawview.h"
#include "BSE_headerbuttons.h"
#include "BSE_editipo.h"
#include "BSE_editaction.h"
#include "BSE_filesel.h"

#include "BLO_readfile.h"
#include "BLO_writefile.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_vpaint.h"

#include "BPY_extern.h"
#include "blendef.h"

#include "radio.h"
#include "render.h"
#include "datatoc.h"

#include "SYS_System.h"

#include "PIL_time.h"

/***/

void BIF_read_file(char *name)
{
	extern short winqueue_break; /* editscreen.c */

	//here?
	//sound_end_all_sounds();

	// first try to read exotic file formats...
	if (BKE_read_exotic(name) == 0) { /* throws first error box */
		/* we didn't succeed, now try to read Blender file
		   calls readfile, calls toolbox, throws one more, 
		   on failure calls the stream, and that is stubbed.... */
		BKE_read_file(name, NULL); 
	}

	sound_initialize_sounds();

	winqueue_break= 1;	/* leave queues everywhere */

}

int BIF_read_homefile(void)
{
	char tstr[FILE_MAXDIR+FILE_MAXFILE], scestr[FILE_MAXDIR];
	char *home= BLI_gethome();
	int success;
#ifdef _WIN32	// FULLSCREEN
	static int screenmode = -1;

	screenmode = U.uiflag & FLIPFULLSCREEN;
#endif

	BLI_make_file_string(G.sce, tstr, home, ".B.blend");
	strcpy(scestr, G.sce);	/* temporal store */
	
	/* only here free userdef themes... */
	BLI_freelistN(&U.themes);
	
	if (BLI_exists(tstr)) {
		success = BKE_read_file(tstr, NULL);
	} else {
		success = BKE_read_file_from_memory(datatoc_B_blend, datatoc_B_blend_size, NULL);
	}
	strcpy(G.sce, scestr);
	
	BIF_InitTheme();	// sets default again
	
	if (success) {
		G.save_over = 0;

		/*  disable autoplay in .B.blend... */
		G.fileflags &= ~G_FILE_AUTOPLAY;

#ifdef _WIN32	// FULLSCREEN
		/* choose window startmode */
		switch (G.windowstate){
			case G_WINDOWSTATE_USERDEF: /* use the usersetting */
				break;
			case G_WINDOWSTATE_FULLSCREEN: /* force fullscreen */
				U.uiflag |= FLIPFULLSCREEN;
				break;
			case G_WINDOWSTATE_BORDER: /* force with borders */
				U.uiflag &= ~FLIPFULLSCREEN;
		}

		if(screenmode != (U.uiflag & FLIPFULLSCREEN)) {
			mainwindow_toggle_fullscreen ((U.uiflag & FLIPFULLSCREEN));
			screenmode = (U.uiflag & FLIPFULLSCREEN);
		}
#endif

		if (BLI_streq(U.tempdir, "/")) {
			char *tmp= getenv("TEMP");
				
			strcpy(U.tempdir, tmp?tmp:"/tmp/");
		}
		if (U.savetime <= 0) {
			U.savetime = 1;
			error("%s is buggy, please cosider removing it.\n", 
				tstr);
		}
		if (G.main->versionfile <= 191) {
			strcpy(U.plugtexdir, U.textudir);
			strcpy(U.sounddir, "/");
		}
	
			/* patch to set Dupli Armature */
		if (G.main->versionfile < 220) {
			U.dupflag |= DUPARM;
		}

			/* userdef new option */
		if (G.main->versionfile <= 222) {
			U.vrmlflag= USERDEF_VRML_LAYERS;
		}

#ifndef __sgi
		/* startup 2.26 with aa fonts ! */
		if (G.main->versionfile <= 225) {
			U.transopts |= TR_ALL;
		}
#endif

		space_set_commmandline_options();
		
		if (U.undosteps==0) U.undosteps=32;

		reset_autosave();
	}

	return success;
}

static void get_autosave_location(char buf[FILE_MAXDIR+FILE_MAXFILE])
{
	char pidstr[32];

	sprintf(pidstr, "%d.blend", abs(getpid()));
	BLI_make_file_string("/", buf, U.tempdir, pidstr);
}

void BIF_read_autosavefile(void)
{
	char tstr[FILE_MAXDIR+FILE_MAXFILE], scestr[FILE_MAXDIR];
	int save_over;

	strcpy(scestr, G.sce);	/* temporal store */
	
	get_autosave_location(tstr);

	save_over = G.save_over;
	BKE_read_file(tstr, NULL);
	G.save_over = save_over;
	strcpy(G.sce, scestr);
}

/***/

static void readBlog(void)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	LinkNode *l, *lines;

	BLI_make_file_string("/", name, BLI_gethome(), ".Blog");
	lines= BLI_read_file_as_lines(name);

	if (lines && !BLI_streq(lines->link, "")) {
		strcpy(G.sce, lines->link);
	} else {
		BLI_make_file_string("/", G.sce, BLI_gethome(), "untitled.blend");
	}

	BLI_free_file_lines(lines);

#ifdef WIN32
	/* Add the drive names to the listing */
	{
		__int64 tmp;
		char tmps[4];
		int i;
			
		tmp= GetLogicalDrives();
		
		for (i=2; i < 26; i++) {
			if ((tmp>>i) & 1) {
				tmps[0]='a'+i;
				tmps[1]=':';
				tmps[2]='\\';
				tmps[3]=0;
				
				fsmenu_insert_entry(tmps, 0);
			}
		}
		
		fsmenu_append_seperator();
	}
#endif

	BLI_make_file_string(G.sce, name, BLI_gethome(), ".Bfs");
	lines= BLI_read_file_as_lines(name);

	for (l= lines; l; l= l->next) {
		char *line= l->link;
			
		if (!BLI_streq(line, "")) {
			fsmenu_insert_entry(line, 0);
		}
	}

	fsmenu_append_seperator();
	BLI_free_file_lines(lines);
}


static void writeBlog(void)
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	FILE *fp;

	BLI_make_file_string("/", name, BLI_gethome(), ".Blog");

	fp= fopen(name, "w");
	if (fp) {
		fprintf(fp, G.sce);
		fclose(fp);
	}
}

static void do_history(char *name)
{
	char tempname1[FILE_MAXDIR+FILE_MAXFILE], tempname2[FILE_MAXDIR+FILE_MAXFILE];
	int hisnr= U.versions;
	
	if(U.versions==0) return;
	if(strlen(name)<2) return;
		
	while(  hisnr > 1) {
		sprintf(tempname1, "%s%d", name, hisnr-1);
		sprintf(tempname2, "%s%d", name, hisnr);
	
		if(BLI_rename(tempname1, tempname2))
			error("Unable to make version backup");
			
		hisnr--;
	}
		
	/* is needed when hisnr==1 */
	sprintf(tempname1, "%s%d", name, hisnr);
	
	if(BLI_rename(name, tempname1))
		error("Unable to make version backup");
}

void BIF_write_file(char *target)
{
	Library *li;
	char di[FILE_MAXDIR];
	char *err;
	
	if (BLI_streq(target, "")) return;
	
	for (li= G.main->library.first; li; li= li->id.next) {
		if (BLI_streq(li->name, target)) {
			error("Cannot overwrite used library");
			return;
		}
	}
	
	if (!BLO_has_bfile_extension(target)) {
		sprintf(di, "%s.blend", target);
	} else {
		strcpy(di, target);
	}

	if (BLI_exists(di)) {
		if(!saveover(di))
			return; 
	}
	
	waitcursor(1);
	
	if(G.obedit) {
		exit_editmode(0);	/* 0 = no free data */
	}
	if (G.fileflags & G_AUTOPACK) {
		packAll();
	}

	do_history(di);
		
	if (BLO_write_file(di, G.fileflags, &err)) {
		strcpy(G.sce, di);
		strcpy(G.main->name, di);	/* is guarenteed current file */

		G.save_over = 1;

		writeBlog();
	} else {
		error("%s", err);
	}

	waitcursor(0);
}

void BIF_write_homefile(void)
{
	char *err, tstr[FILE_MAXDIR+FILE_MAXFILE];
	int write_flags;
	
	/* "change-over" period for Windows - Ctrl+U now saves in ~/.blender,
	   but blender still checks the old locations.
	   To be removed at v2.4 or so! ;)
	*/

#ifdef WIN32
	char dir[FILE_MAXDIR+FILE_MAXFILE];
	BLI_getInstallationDir(dir);

	strcat(dir,".blender/");

	BLI_make_file_string("/", tstr, dir, ".B.blend");
#else
	BLI_make_file_string("/", tstr, BLI_gethome(), ".B.blend");
#endif
		
		/*  force save as regular blend file */
	write_flags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_LOCK | G_FILE_SIGN);
	BLO_write_file(tstr, write_flags, &err);
}

void BIF_write_autosave(void)
{
	char *err, tstr[FILE_MAXDIR+FILE_MAXFILE];
	int write_flags;
	
	get_autosave_location(tstr);

		/*  force save as regular blend file */
	write_flags = G.fileflags & ~(G_FILE_COMPRESS | G_FILE_LOCK | G_FILE_SIGN);
	BLO_write_file(tstr, write_flags, &err);
}

static void delete_autosave(void)
{
	char tstr[FILE_MAXDIR+FILE_MAXFILE], pidstr[FILE_MAXFILE];
	
	sprintf(pidstr, "%d", abs(getpid()));
	BLI_make_file_string("/", tstr, U.tempdir, pidstr);
	
	if (BLI_exists(tstr)) {
		char str[FILE_MAXDIR+FILE_MAXFILE];
		BLI_make_file_string("/", str, U.tempdir, "quit.blend");
		BLI_rename(tstr, str);
	}
}

/***/

static void initbuttons(void)
{
	uiDefFont(UI_HELVB, 
				BMF_GetFont(BMF_kHelveticaBold14), 
				BMF_GetFont(BMF_kHelveticaBold12), 
				BMF_GetFont(BMF_kHelveticaBold10), 
				BMF_GetFont(BMF_kHelveticaBold8));
	uiDefFont(UI_HELV, 
				BMF_GetFont(BMF_kHelvetica12), 
				BMF_GetFont(BMF_kHelvetica12), 
				BMF_GetFont(BMF_kHelvetica10), 
				BMF_GetFont(BMF_kHelveticaBold8));
	
	BIF_resources_init();

	glClearColor(.7, .7, .6, 0.0);
	
	G.font= BMF_GetFont(BMF_kHelvetica12);
	G.fonts= BMF_GetFont(BMF_kHelvetica10);
	G.fontss= BMF_GetFont(BMF_kHelveticaBold8);

	clear_matcopybuf();
}

void BIF_init(void)
{
	BKE_font_register_builtin(datatoc_Bfont, datatoc_Bfont_size);

	initscreen();	/* for (visuele) speed, this first, then setscreen */
	initbuttons();
	
	init_draw_rects();	/* drawobject.c */
	BIF_read_homefile(); 
	init_gl_stuff();	/* drawview.c, after homefile */
	readBlog();
	strcpy(G.lib, G.sce);

#ifdef INTERNATIONAL
	read_languagefile();

	if(U.transopts & TR_ALL)
		start_interface_font();
	else
		G.ui_international = FALSE;
#endif // INTERNATIONAL
}

/***/

extern ListBase editNurb;
extern ListBase editelems;

void exit_usiblender(void)
{
	freeAllRad();
	BKE_freecubetable();

	if (G.background == 0)
		sound_end_all_sounds();

	if(G.obedit) {
		if(G.obedit->type==OB_FONT) {
			free_editText();
		}
		else if(G.obedit->type==OB_MBALL) BLI_freelistN(&editelems);
		free_editMesh();
	}

	free_editLatt();
	free_editArmature();
	free_posebuf();

	free_blender();	/* blender.c, does entire library */
	free_hashedgetab();
	free_matcopybuf();
	free_ipocopybuf();
	freefastshade();
	free_vertexpaint();
	
	/* editnurb can remain to exist outside editmode */
	freeNurblist(&editNurb);

	fsmenu_free();
#ifdef INTERNATIONAL
	free_languagemenu();
#endif	
	
	RE_free_render_data();
	RE_free_filt_mask();
	
	free_txt_data();

	sound_exit_audio();

#ifdef WITH_QUICKTIME
	quicktime_exit();
#endif
		
	BPY_end_python();

	if (!G.background) {
		BIF_resources_free();
	
		BIF_close_render_display();
		mainwindow_close();
	}

	if (G.undo_clear) G.undo_clear();

	BLI_freelistN(&U.themes);
	
	if(totblock!=0) {
		printf("Error Totblock: %d\n",totblock);
		MEM_printmemlist();
	}
	delete_autosave();
	
	printf("\nBlender quit\n");

#ifdef WIN32   
	// when debugging enter infinite loop to enable   
	// reading the printouts...   
	while(G.f & G_DEBUG) {PIL_sleep_ms(10);}   
#endif 


	SYS_DeleteSystem(SYS_GetSystem());

	exit(G.afbreek==1);
}
