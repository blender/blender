/**
 * header_info.c oct-2003
 *
 * Functions to draw the "User Preferences" window header
 * and handle user events sent to it.
 * 
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_world_types.h"

#include "BDR_editcurve.h"
#include "BDR_editmball.h"
#include "BDR_editobject.h"
#include "BIF_editarmature.h"
#include "BIF_editfont.h"
#include "BIF_editmesh.h"
#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_language.h"
#include "BIF_mainqueue.h"
#include "BIF_previewrender.h"
#include "BIF_renderwin.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_usiblender.h"
#include "BKE_blender.h"
#include "BKE_exotic.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_scene.h"
#include "BKE_world.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLO_writefile.h"
#include "BPY_extern.h"
#include "BSE_editipo.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_sequence.h"

#include "IMB_imbuf_types.h"

#include "MEM_guardedalloc.h"

#include "render.h" // for R - should use BKE_bad_level_calls.h instead?

#include "blendef.h"
#include "interface.h"
#include "mydevice.h"

extern char versionstr[]; /* from blender.c */

/*----------------------------------*/
/* Progress bar vars and functions: */

/* strubi shamelessly abused the status line as a progress bar...
 * feel free to kill him after release */

static int g_progress_bar = 0;
static char *g_progress_info = 0;
static float g_done;

int start_progress_bar(void)
{
  g_progress_bar = 1;
  return 1;   // we never fail (yet)
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
/* -- End of progress bar definitions ------- */

extern char videosc_dir[];  /* exotic.c */

void write_videoscape_fs()
{
  if(G.obedit) {
    error("Can't save Videoscape. Press TAB to leave EditMode");
  }
  else {
    if(videosc_dir[0]==0) strcpy(videosc_dir, G.sce);
    activate_fileselect(FILE_SPECIAL, "SAVE VIDEOSCAPE", videosc_dir,
										write_videoscape);
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
/* ------------ */

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

  case B_INFOSCR:   /* menu select screen */

    if( G.curscreen->screennr== -2) {
      if(curarea->winy <50) {
        sa= closest_bigger_area();
        areawinset(sa->win);
      }
      activate_databrowse((ID *)G.curscreen, ID_SCR, 0, B_INFOSCR,
                      &G.curscreen->screennr, do_info_buttons);
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
/*do this event only with buttons, so it can never be called with full-window*/

    if(G.curscreen->id.prev) sc= G.curscreen->id.prev;
    else if(G.curscreen->id.next) sc= G.curscreen->id.next;
    else return;
    if(okee("Delete current screen")) {
      /* find new G.curscreen */

      oldscreen= G.curscreen;
      setscreen(sc);    /* this test if sc has a full */
      unlink_screen(oldscreen);
      free_libblock(&G.main->screen, oldscreen);
    }
    scrarea_queue_headredraw(curarea);

    break;
  case B_INFOSCE:   /* menu select scene */

    if( G.obedit) {
      error("Unable to perform function in EditMode");
      return;
    }
    if( G.curscreen->scenenr== -2) {
      if(curarea->winy <50) {
        sa= closest_bigger_area();
        areawinset(sa->win);
      }
      activate_databrowse((ID *)G.scene, ID_SCE, 0, B_INFOSCE,
                      &G.curscreen->scenenr, do_info_buttons);
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

/************************** MAIN MENU *****************************/
/************************** FILE *****************************/

void do_info_file_optionsmenu(void *arg, int event)
{
	G.fileflags ^= (1 << event);

	// allqueue(REDRAWINFO, 0);
}

#if 0
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

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 50);
		
	return block;
}
#endif

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
static void run_playanim(char *file)
{
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
		xco= std_libbuttons(block, xco, 0, 0, NULL, B_INFOSCR, (ID *)G.curscreen, 0, &G.curscreen->screennr, 1, 1, B_INFODELSCR, 0, 0);
	
		/* STD SCENE BUTTONS */
		xco+= 5;
		xco= std_libbuttons(block, xco, 0, 0, NULL, B_INFOSCE, (ID *)G.scene, 0, &G.curscreen->scenenr, 1, 1, B_INFODELSCE, 0, 0);
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
