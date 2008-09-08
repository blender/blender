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
 */

#include <stdlib.h>  
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <sys/times.h>
#endif

#include "MEM_guardedalloc.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_depsgraph.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_storage_types.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"


#include "BIF_filelist.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_imasel.h"
#include "BIF_gl.h"
#include "BIF_fsmenu.h"
#include "BIF_editview.h"
#include "BIF_toolbox.h"

#include "BLO_readfile.h"

#include "BSE_drawipo.h"
#include "BSE_drawimasel.h"
#include "BSE_edit.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "mydevice.h"

/* for events */
#define NOTACTIVE			0
#define ACTIVATE			1
#define INACTIVATE			2
/* for state of file */
#define ACTIVE				2

static void imasel_select_objects(SpaceImaSel *simasel);

static int imasel_has_func(SpaceImaSel *simasel)
{
	if(simasel->returnfunc || simasel->returnfunc_event || simasel->returnfunc_args)
		return 1;
	return 0;
}

#if defined __BeOS
static int fnmatch(const char *pattern, const char *string, int flags)
{
	return 0;
}
#elif defined WIN32 && !defined _LIBC
	/* use fnmatch included in blenlib */
	#include "BLI_fnmatch.h"
#else
	#include <fnmatch.h>
#endif

static void imasel_split_file(SpaceImaSel *simasel, char *s1)
{
	char string[FILE_MAX], dir[FILE_MAX], file[FILE_MAX];

	BLI_strncpy(string, s1, sizeof(string));

	BLI_split_dirfile(string, dir, file);
	
	if(simasel->files) {
		BIF_filelist_free(simasel->files);
	}
	BLI_strncpy(simasel->file, file, sizeof(simasel->file));
	BLI_strncpy(simasel->dir, dir, sizeof(simasel->dir));

	BIF_filelist_setdir(simasel->files, dir);

	BLI_make_file_string(G.sce, simasel->dir, dir, "");
}

/**************** IMAGESELECT ******************************/

/* the complete call; pulldown menu, and three callback types */
static void activate_imageselect_(int type, char *title, char *file, short *menup, char *pupmenu,
										 void (*func)(char *),
										 void (*func_event)(unsigned short),
										 void (*func_args)(char *, void *arg1, void *arg2),
										 void *arg1, void *arg2)
{
	SpaceImaSel *simasel;
	char group[24], name[FILE_MAX], temp[FILE_MAX];
	
	if(curarea==0) return;
	if(curarea->win==0) return;
	
	newspace(curarea, SPACE_IMASEL);
	scrarea_queue_winredraw(curarea);
	
	/* sometime double, when area already is SPACE_IMASEL with a different file name */
	if(curarea->headwin) addqueue(curarea->headwin, CHANGED, 1);

	name[2]= 0;
	BLI_strncpy(name, file, sizeof(name));
	BLI_convertstringcode(name, G.sce);
	
	simasel= curarea->spacedata.first;

	simasel->returnfunc= func;
	simasel->returnfunc_event= func_event;
	simasel->returnfunc_args= func_args;
	simasel->arg1= arg1;
	simasel->arg2= arg2;
	
	simasel->type= type;
	simasel->scrollpos = 0.0f;

	if(simasel->pupmenu)
		MEM_freeN(simasel->pupmenu);
	simasel->pupmenu= pupmenu;
	simasel->menup= menup;
	
	/* sfile->act is used for databrowse: double names of library objects */
	simasel->active_file= -1;

	if(!simasel->files) {
		simasel->files = BIF_filelist_new();
	}

	if(G.relbase_valid && U.flag & USER_RELPATHS && type != FILE_BLENDER)
		simasel->flag |= FILE_STRINGCODE;
	else
		simasel->flag &= ~FILE_STRINGCODE;

	if (U.uiflag & USER_HIDE_DOT)
		simasel->flag |= FILE_HIDE_DOT;

	if(type==FILE_MAIN) {
		char *groupname;
		
		BLI_strncpy(simasel->file, name+2, sizeof(simasel->file));

		groupname = BLO_idcode_to_name( GS(name) );
		if (groupname) {
			BLI_strncpy(simasel->dir, groupname, sizeof(simasel->dir) - 1);
			strcat(simasel->dir, "/");
		}

		/* free all */
		if (simasel->files) {
			BIF_filelist_freelib(simasel->files);				
			BIF_filelist_free(simasel->files);
			BIF_filelist_setdir(simasel->files, simasel->dir);
			BIF_filelist_settype(simasel->files, type);
		}
	}
	else if(type==FILE_LOADLIB) {
		
		if( BIF_filelist_islibrary(simasel->files, temp, group) ) {
			/* force a reload of the library-filelist */
			BIF_filelist_free(simasel->files);
			BIF_filelist_freelib(simasel->files);
			BLI_strncpy(simasel->dir, name, sizeof(simasel->dir));
			BIF_filelist_setdir(simasel->files, simasel->dir);
			BIF_filelist_settype(simasel->files, type);
		}
		else {
			imasel_split_file(simasel, name);
			BIF_filelist_freelib(simasel->files);
			BIF_filelist_settype(simasel->files, type);			
		}
	}
	else {	/* FILE_BLENDER */
		imasel_split_file(simasel, name);
		BIF_filelist_settype(simasel->files, type);

		BLI_cleanup_dir(G.sce, simasel->dir);

		/* free: filelist and libfiledata became incorrect */
		BIF_filelist_freelib(simasel->files);
	}
	BLI_strncpy(simasel->title, title, sizeof(simasel->title));
	/* filetoname= 1; */ /* TODO: elubie - check what this means */
}

void activate_imageselect(int type, char *title, char *file, void (*func)(char *))
{
	activate_imageselect_(type, title, file, NULL, NULL, func, NULL, NULL, NULL, NULL);
}

void activate_imageselect_menu(int type, char *title, char *file, char *pupmenu, short *menup, void (*func)(char *))
{
	activate_imageselect_(type, title, file, menup, pupmenu, func, NULL, NULL, NULL, NULL);
}

void activate_imageselect_args(int type, char *title, char *file, void (*func)(char *, void *, void *), void *arg1, void *arg2)
{
	activate_imageselect_(type, title, file, NULL, NULL, NULL, NULL, func, arg1, arg2);
}

void activate_databrowse_imasel(ID *id, int idcode, int fromcode, int retval, short *menup, void (*func)(unsigned short))
{
	ListBase *lb;
	SpaceImaSel *simasel;
	char str[32];
	
	if(id==NULL) {
		lb= wich_libbase(G.main, idcode);
		id= lb->first;
	}
	
	if(id) BLI_strncpy(str, id->name, sizeof(str));
	else return;

	activate_imageselect_(FILE_MAIN, "SELECT DATABLOCK", str, menup, NULL, NULL, func, NULL, NULL, NULL);
	
	simasel= curarea->spacedata.first;
	simasel->retval= retval;
	simasel->menup= menup;

	BIF_filelist_setipotype(simasel->files, fromcode);
	BIF_filelist_hasfunc(simasel->files, imasel_has_func(simasel));
}


static void set_active_file(SpaceImaSel *simasel, short x, short y)
{
	short tilex, tiley;
	int active_tile;
	int active_file;
	int stridex;
	struct direntry* file;
	rcti viewrect = simasel->viewrect;
	int fileoffset;
	int rowoffset;
	int rowleftover;
	float scrollofs;
	int numfiles;
	int tilewidth = simasel->prv_w + TILE_BORDER_X*4;
	int tileheight = simasel->prv_h + TILE_BORDER_Y*4 + U.fontsize;

	numfiles = BIF_filelist_numfiles(simasel->files);
	
	if (simasel->numtilesx > 0) {
		fileoffset = numfiles*(simasel->scrollpos / simasel->scrollarea) + 0.5;
		rowoffset = (fileoffset / simasel->numtilesx)*simasel->numtilesx;
		rowleftover = fileoffset % simasel->numtilesx;
		scrollofs = (float)tileheight*(float)rowleftover/(float)simasel->numtilesx;
	
		stridex = (viewrect.xmax - viewrect.xmin) / (tilewidth);
		tilex = ( (x-viewrect.xmin)) / (tilewidth);
		tiley = (viewrect.ymax - viewrect.ymin + scrollofs - y) / (tileheight);
		if (tilex >= simasel->numtilesx) tilex = simasel->numtilesx-1;
		if (tiley >= simasel->numtilesy+1) tiley = simasel->numtilesy;
		if (tilex < 0) tilex=0;
		if (tiley < 0) tiley = 0;
		active_tile = tilex + stridex*tiley;
		active_file = rowoffset + active_tile;

		if (active_file >= 0 && active_file < BIF_filelist_numfiles(simasel->files) )
		{
			simasel->active_file = active_file;
			if (simasel->selstate & ACTIVATE) {
				file = BIF_filelist_file(simasel->files, simasel->active_file);
				file->flags |= ACTIVE;
			}
		} else {
			simasel->active_file = -1;
		}
	} else {
		simasel->active_file = -1;
	}
}

static void set_active_bookmark(SpaceImaSel *simasel, short y)
{
	int nentries = fsmenu_get_nentries();
	short posy = simasel->bookmarkrect.ymax - TILE_BORDER_Y - y;
	simasel->active_bookmark = ((float)posy / (U.fontsize*3.0f/2.0f));	
	if (simasel->active_bookmark < 0 || simasel->active_bookmark > nentries) {
		simasel->active_bookmark = -1;
	}
}

static void imasel_prevspace()
{
	SpaceImaSel *simasel;	

	simasel= curarea->spacedata.first;

	/* cleanup */
	if(simasel->spacetype==SPACE_IMASEL) {
		if(simasel->pupmenu) {
			MEM_freeN(simasel->pupmenu);
			simasel->pupmenu= NULL;
		}
	}

	if(simasel->next) {
	
		BLI_remlink(&curarea->spacedata, simasel);
		BLI_addtail(&curarea->spacedata, simasel);

		simasel= curarea->spacedata.first;

		if (simasel->spacetype == SPACE_SCRIPT) {
			SpaceScript *sc = (SpaceScript *)simasel;
			if (sc->script) sc->script->flags &=~SCRIPT_FILESEL;
		}

		newspace(curarea, simasel->spacetype);
	}
	else newspace(curarea, SPACE_INFO);
}

static void free_imasel_spec(char *dir)
{
	/* all filesels with 'dir' are freed */
	bScreen *sc;
		
	sc= G.main->screen.first;
	while(sc) {
		ScrArea *sa= sc->areabase.first;
		while(sa) {
			SpaceLink  *sl= sa->spacedata.first;
			while(sl) {
				if(sl->spacetype==SPACE_FILE) {
					SpaceImaSel *simasel= (SpaceImaSel*) sl;
					if (BLI_streq(simasel->dir, dir)) {
						BIF_filelist_free(simasel->files);
					}
				}
				sl= sl->next;
			}
			sa= sa->next;
		}
		sc= sc->id.next;
	}
}

static void do_library_append(SpaceImaSel *simasel)
{
	Library *lib;
	char dir[FILE_MAX], group[32];
	
	if ( BIF_filelist_islibrary(simasel->files, dir, group)==0 ) {
		error("Not a library");
	} else if (!BIF_filelist_lib(simasel->files) ) {
		error("Library not loaded");
	} else if (group[0]==0) {
		error("Nothing indicated");
	} else if (BLI_streq(G.main->name, dir)) {
		error("Cannot use current file as library");
	} else {
		Object *ob;
		int idcode = BIF_groupname_to_code(group);
				
		if((simasel->flag & FILE_LINK)==0) {
			/* tag everything, all untagged data can be made local */
			ID *id;
			ListBase *lbarray[MAX_LIBARRAY];
			int a;
			
			a= set_listbasepointers(G.main, lbarray);
			while(a--) {
				for(id= lbarray[a]->first; id; id= id->next) id->flag |= LIB_APPEND_TAG;
			}
		}

		BIF_filelist_append_library(simasel->files, dir, simasel->file, simasel->flag, idcode);

		/* DISPLISTS? */
		ob= G.main->object.first;
		while(ob) {
			if(ob->id.lib) {
				ob->recalc |= OB_RECALC;
			}
			ob= ob->id.next;
		}
	
		/* and now find the latest append lib file */
		lib= G.main->library.first;
		while(lib) {
			if (BLI_streq(dir, lib->filename)) break;
			lib= lib->id.next;
		}
		
		/* make local */
		if(lib) {
			if((simasel->flag & FILE_LINK)==0) 
				all_local(lib,1);
		}
		
		DAG_scene_sort(G.scene);

		/* in sfile->dir is the whole lib name */
		BLI_strncpy(G.lib, simasel->dir, sizeof(G.lib) );
		
	}
}

/* NOTE: this is called for file read, after the execfunc no UI memory is valid! */
static void imasel_execute(SpaceImaSel *simasel)
{
	struct direntry *file;
	char name[FILE_MAX];
	int a;
	int n;
	
	imasel_prevspace();

	if(simasel->type==FILE_LOADLIB) {
		if(simasel->flag & FILE_STRINGCODE) {
			if (!G.relbase_valid) {
				okee("You have to save the .blend file before using relative paths! Using absolute path instead.");
				simasel->flag &= ~FILE_STRINGCODE;
			}
		}

		do_library_append(simasel);
		BIF_undo_push("Append from file");
		allqueue(REDRAWALL, 1);
	}
	else if(imasel_has_func(simasel)) {
		fsmenu_insert_entry(simasel->dir, 1, 0);
	
		if(simasel->type==FILE_MAIN) { /* DATABROWSE */
			if (simasel->menup) {	/* with value pointing to ID block index */
				int notfound = 1;

				/*	Need special handling since hiding .* datablocks means that
					simasel->active_file is no longer the same as files->nr.

					Also, toggle HIDE_DOT on and off can make simasel->active_file not longer
					correct (meaning it doesn't point to the correct item in the filelist.
					
					simasel->file is always correct, so first with check if, for the item
					corresponding to simasel->active_file, the name is the same.

					If it isn't (or if simasel->active_file is not good), go over filelist and take
					the correct one.

					This means that selecting a datablock than hiding it makes it
					unselectable. Not really a problem.

					- theeth
				 */

				*simasel->menup= -1;
				n = BIF_filelist_numfiles(simasel->files);
				if(simasel->files) {
					if( (simasel->active_file>=0) && (simasel->active_file < n) ) {
						file = BIF_filelist_file(simasel->files, simasel->active_file);						
						if ( strcmp(file->relname, simasel->file)==0) {
							notfound = 0;
							*simasel->menup= file->nr;
						}
					}
					if (notfound) {					
						for(a=0; a<n; a++) {
							file = BIF_filelist_file(simasel->files, a);	
							if( strcmp(file->relname, simasel->file)==0) {
								*simasel->menup= file->nr;
								break;
							}
						}
					}
				}
			}
			if(simasel->returnfunc_event)
				simasel->returnfunc_event(simasel->retval);
			else if(simasel->returnfunc_args)
				simasel->returnfunc_args(NULL, simasel->arg1, simasel->arg2);
		}
		else {
			if(strncmp(simasel->title, "Save", 4)==0) free_imasel_spec(simasel->dir);
			if(strncmp(simasel->title, "Export", 6)==0) free_imasel_spec(simasel->dir);
			
			BLI_strncpy(name, simasel->dir, sizeof(name));
			strcat(name, simasel->file);
			
			if(simasel->flag & FILE_STRINGCODE) {
				/* still weak, but we don't want saving files to make relative paths */
				if(G.relbase_valid && strncmp(simasel->title, "Save", 4)) {
					BLI_makestringcode(G.sce, name);
				} else {
					/* if we don't have a valid relative base (.blend file hasn't been saved yet)
					   then we don't save the path as relative (for texture images, background image).	
					   Warning message not shown when saving files (doesn't make sense there)
					*/
					if (strncmp(simasel->title, "Save", 4)) {					
						printf("Relative path setting has been ignored because .blend file hasn't been saved yet.\n");
					}
					simasel->flag &= ~FILE_STRINGCODE;
				}
			}
			if(simasel->returnfunc)
				simasel->returnfunc(name);
			else if(simasel->returnfunc_args)
				simasel->returnfunc_args(name, simasel->arg1, simasel->arg2);
		}
	}
}

static void do_imasel_buttons(short event, SpaceImaSel *simasel)
{
	char butname[FILE_MAX];
	
	if (event == B_FS_FILENAME) {
		if (strchr(simasel->file, '*') || strchr(simasel->file, '?') || strchr(simasel->file, '[')) {
			int i, match = FALSE;
			struct direntry *file;
			int n = BIF_filelist_numfiles(simasel->files);
			for (i = 2; i < n; i++) {
				file = BIF_filelist_file(simasel->files, i);
				if (fnmatch(simasel->file, file->relname, 0) == 0) {
					file->flags |= ACTIVE;
					match = TRUE;
				}
			}
			if (match) simasel->file[0] = '\0';
			if(simasel->type==FILE_MAIN) imasel_select_objects(simasel);
			scrarea_queue_winredraw(curarea);
		}
	}
	else if(event== B_FS_DIRNAME) {
		
		/* convienence shortcut '~' -> $HOME
		 * If the first char is ~ then this is invalid on all OS's so its safe to replace with home */
		if ( simasel->dir[0] == '~' ) {
			if (simasel->dir[1] == '\0') {
				BLI_strncpy(simasel->dir, BLI_gethome(), sizeof(simasel->dir) );
			} else {
				/* replace ~ with home */
				char tmpstr[FILE_MAX];
				BLI_join_dirfile(tmpstr, BLI_gethome(), simasel->dir+1);
				BLI_strncpy(simasel->dir, tmpstr, sizeof(simasel->dir));
			}
		}
		
		/* reuse the butname vsariable */
		BLI_cleanup_dir(G.sce, simasel->dir);

		BLI_make_file_string(G.sce, butname, simasel->dir, "");
		BLI_strncpy(simasel->dir, butname, sizeof(simasel->dir));		

		/* strip the trailing slash if its a real dir */
		if (strlen(butname)!=1)
			butname[strlen(butname)-1]=0;
		
		/* updating the directory in the filelist */
		BIF_filelist_setdir(simasel->files, simasel->dir);

		if(simasel->type & FILE_UNIX) {
			if (!BLI_exists(butname)) {
				if (okee("Makedir")) {
					BLI_recurdir_fileops(butname);
					if (!BLI_exists(butname)) {
						BIF_filelist_free(simasel->files);
						BIF_filelist_parent(simasel->files);
						BLI_strncpy(simasel->dir, BIF_filelist_dir(simasel->files), 80);
					}
				} else {
					BIF_filelist_free(simasel->files);
					BIF_filelist_parent(simasel->files);
					BLI_strncpy(simasel->dir, BIF_filelist_dir(simasel->files), 80);
				}
			}
		}
		BIF_filelist_free(simasel->files);		
		simasel->file[0] = '\0';			
		simasel->scrollpos = 0;
		simasel->active_file = -1;
		scrarea_queue_winredraw(curarea);
	}
	else if(event== B_FS_DIR_MENU) {
		char *selected= fsmenu_get_entry(simasel->menu-1);
		
		/* which string */
		if (selected) {
			BLI_strncpy(simasel->dir, selected, sizeof(simasel->dir));
			BLI_cleanup_dir(G.sce, simasel->dir);
			BIF_filelist_free(simasel->files);	
			BIF_filelist_setdir(simasel->files, simasel->dir);
			simasel->file[0] = '\0';			
			simasel->scrollpos = 0;
			simasel->active_file = -1;
			scrarea_queue_redraw(curarea);
		}

		simasel->active_file = -1;
		
	}
	else if(event== B_FS_PARDIR) {
		BIF_filelist_free(simasel->files);
		BIF_filelist_parent(simasel->files);
		BLI_strncpy(simasel->dir, BIF_filelist_dir(simasel->files), 80);
		simasel->file[0] = '\0';
		simasel->active_file = -1;
		simasel->scrollpos = 0;
		scrarea_queue_redraw(curarea);
	}
	else if(event== B_FS_LOAD) {
		if(simasel->type) 
			imasel_execute(simasel);
	}
	else if(event== B_FS_CANCEL) 
		imasel_prevspace();
	else if(event== B_FS_LIBNAME) {
		Library *lib= BLI_findlink(&G.main->library, simasel->menu);
		if(lib) {
			BLI_strncpy(simasel->dir, lib->filename, sizeof(simasel->dir));
			BLI_make_exist(simasel->dir);
			BLI_cleanup_dir(G.sce, simasel->dir);
			BIF_filelist_free(simasel->files);
			BIF_filelist_setdir(simasel->files, simasel->dir);
			simasel->file[0] = '\0';			
			simasel->scrollpos = 0;
			simasel->active_file = -1;
			scrarea_queue_winredraw(curarea);
		}
	} else if(event== B_FS_BOOKMARK)  {
		char name[FILE_MAX];
		BLI_make_file_string(G.sce, name, BLI_gethome(), ".Bfs");
		fsmenu_insert_entry(simasel->dir, 1, 1);
		scrarea_queue_winredraw(curarea);
		fsmenu_write_file(name);
	}
	
}

static void imasel_home(ScrArea *sa, SpaceImaSel *simasel)
{
	simasel->v2d.cur.xmin= simasel->v2d.cur.ymin= 0.0f;
	simasel->v2d.cur.xmax= sa->winx;
	simasel->v2d.cur.ymax= sa->winy;
	
	simasel->v2d.tot= simasel->v2d.cur;
	test_view2d(G.v2d, sa->winx, sa->winy);
	
}

static struct direntry* get_hilited_entry(SpaceImaSel *simasel)
{
	struct direntry *file;
	file = BIF_filelist_file(simasel->files, simasel->active_file);
	return file;
}

static void do_filescroll(SpaceImaSel *simasel)
{
	short mval[2], oldy, yo;
	float scrollarea, scrollstep;

	/* for beauty */
	scrarea_do_windraw(curarea);
	screen_swapbuffers();

	getmouseco_areawin(mval);
	oldy= yo= mval[1];
	
	while(get_mbut()&L_MOUSE) {
		getmouseco_areawin(mval);
		
		if(yo!=mval[1]) {
			scrollarea = ((float)simasel->v2d.vert.ymax - (float)simasel->v2d.vert.ymin);
			scrollstep = yo - mval[1];	
			simasel->scrollpos += scrollstep;
			
			if (simasel->scrollpos<0) 
				simasel->scrollpos=0;
			if (simasel->scrollpos > scrollarea - simasel->scrollheight) 
				simasel->scrollpos = scrollarea - simasel->scrollheight;
			scrarea_do_windraw(curarea);
			screen_swapbuffers();

			yo= mval[1];
		}
		else BIF_wait_for_statechange();
	}

	/* for beauty */
	scrarea_do_windraw(curarea);
	screen_swapbuffers();
	
}

/* ******************* DATA SELECT ********************* */

static void imasel_select_objects(SpaceImaSel *simasel)
{
	Object *ob;
	Base *base;
	Scene *sce;
	struct direntry* file;
	int a;
	int totfile;

	/* only when F4 DATABROWSE */
	if(imasel_has_func(simasel)) return;
	
	totfile = BIF_filelist_numfiles(simasel->files);

	if( strcmp(simasel->dir, "Object/")==0 ) {
		for(a=0; a<totfile; a++) {
			file = BIF_filelist_file(simasel->files, a);
			ob= (Object *)file->poin;
			
			if(ob) {
				if(file->flags & ACTIVE) ob->flag |= SELECT;
				else ob->flag &= ~SELECT;
			}

		}
		base= FIRSTBASE;
		while(base) {
			base->flag= base->object->flag;
			base= base->next;
		}
		countall();
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWOOPS, 0);
	}
	else if( strcmp(simasel->dir, "Scene/")==0 ) {
		
		for(a=0; a<totfile; a++) {
			file = BIF_filelist_file(simasel->files, a);
			sce= (Scene *)file->poin;
			if(sce) {
				if(file->flags & ACTIVE) sce->r.scemode |= R_BG_RENDER;
				else sce->r.scemode &= ~R_BG_RENDER;
			}

		}
		allqueue(REDRAWBUTSSCENE, 0);
	}
}

static void active_imasel_object(SpaceImaSel *simasel)
{
	Object *ob;
	struct direntry* file;

	/* only when F4 DATABROWSE */
	if(imasel_has_func(simasel)) return;
	
	if( strcmp(simasel->dir, "Object/")==0 ) {
		int n = BIF_filelist_numfiles(simasel->files);
		if(simasel->active_file >= 0 && simasel->active_file < n) {
			file = BIF_filelist_file(simasel->files, simasel->active_file);
			ob= (Object *)file->poin;
			
			if(ob) {
				set_active_object(ob);
				if(BASACT && BASACT->object==ob) {
					BASACT->flag |= SELECT;
					file->flags |= ACTIVE;
					allqueue(REDRAWVIEW3D, 0);
					allqueue(REDRAWOOPS, 0);
					scrarea_queue_winredraw(curarea);
				}
			}
		}
	}
}



void winqreadimaselspace(ScrArea *, void *, BWinEvent *);


void winqreadimaselspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	SpaceImaSel *simasel;
	
	char str[FILE_MAXDIR+FILE_MAXFILE+12];
	short mval[2];
	short do_draw = 0;
	short do_headdraw = 0;
	int numfiles;
	struct direntry *file;
	float scrollstep = 0;
	float scrollarea;

	// if(val==0) return;
	simasel= curarea->spacedata.first;

	if (!simasel->files)
		return;

	if (BIF_filelist_empty(simasel->files))
		return;

	numfiles = BIF_filelist_numfiles(simasel->files);
	
	/* calc_scrollrcts(sa, &(simasel->v2d), sa->winx, sa->winy); */
	calc_imasel_rcts(simasel, sa->winx, sa->winy);	

	/* prevent looping */
	if(simasel->selstate && !(get_mbut() & R_MOUSE)) simasel->selstate= 0;

	if(val) {

		if( event!=RETKEY && event!=PADENTER)
			if( uiDoBlocks(&curarea->uiblocks, event, 1)!=UI_NOTHING ) event= 0;

		switch(event) {
		
		case UI_BUT_EVENT:
			do_imasel_buttons(val, simasel);
			break;		
		case RENDERPREVIEW:
			do_draw= 1; 
			/* draw_imasel_previews(sa, simasel);  */
			break;
		case REDRAWIMASEL:
			do_draw= 1;
			break;
		case WHEELDOWNMOUSE:
			numfiles = BIF_filelist_numfiles(simasel->files);
			scrollarea = ((float)simasel->v2d.vert.ymax - (float)simasel->v2d.vert.ymin);
			scrollstep = ((scrollarea-simasel->scrollheight)/numfiles)*simasel->numtilesx;	
			simasel->scrollpos += scrollstep;
			if (simasel->scrollpos > scrollarea - simasel->scrollheight) 
				simasel->scrollpos = scrollarea - simasel->scrollheight;
			do_draw= 1;
			break;
		case WHEELUPMOUSE:
			numfiles = BIF_filelist_numfiles(simasel->files);
			scrollarea = ((float)simasel->v2d.vert.ymax - (float)simasel->v2d.vert.ymin);
			scrollstep = ((scrollarea-simasel->scrollheight)/numfiles)*simasel->numtilesx;			
			simasel->scrollpos -= scrollstep;
			if (simasel->scrollpos<0) 
				simasel->scrollpos=0;
			do_draw= 1;
			break;
		case PAGEUPKEY:
			numfiles = BIF_filelist_numfiles(simasel->files);
			scrollarea = ((float)simasel->v2d.vert.ymax - (float)simasel->v2d.vert.ymin);
			scrollstep = ((scrollarea-simasel->scrollheight)/numfiles)
						 *simasel->numtilesx*simasel->numtilesy;
			simasel->scrollpos -= scrollstep;
			if (simasel->scrollpos<0) 
				simasel->scrollpos=0;
			do_draw= 1;
			break;
		case PAGEDOWNKEY:
			numfiles = BIF_filelist_numfiles(simasel->files);
			scrollarea = ((float)simasel->v2d.vert.ymax - (float)simasel->v2d.vert.ymin);
			scrollstep = ((scrollarea-simasel->scrollheight)/numfiles)
						 * simasel->numtilesx*simasel->numtilesy;
			simasel->scrollpos += scrollstep;
			if (simasel->scrollpos > scrollarea - simasel->scrollheight) 
				simasel->scrollpos = scrollarea - simasel->scrollheight;
			do_draw= 1;						
			break;
		case HOMEKEY:
			simasel->scrollpos=0;
			imasel_home(sa, simasel);
			do_draw= 1;
			break;
		case ENDKEY:
			simasel->scrollpos = simasel->scrollarea;
			do_draw= 1;
			break;

		case ESCKEY:
			BIF_filelist_free(simasel->files);
			imasel_prevspace();
			break;
		case PERIODKEY:
			BIF_filelist_free(simasel->files);
			simasel->active_file = -1;
			do_draw = 1;
			break;
		case LEFTMOUSE:
		case MIDDLEMOUSE:			
			getmouseco_areawin(mval);
			if(mval[0]>simasel->v2d.vert.xmin && mval[0]<simasel->v2d.vert.xmax && mval[1]>simasel->v2d.vert.ymin && mval[1]<simasel->v2d.vert.ymax) {
				do_filescroll(simasel);
			}
			else if(mval[0]>simasel->viewrect.xmin && mval[0]<simasel->viewrect.xmax 
				&& mval[1]>simasel->viewrect.ymin && mval[1]<simasel->viewrect.ymax) {	
				set_active_file(simasel, mval[0], mval[1]);
				if (simasel->active_file >= 0 && simasel->active_file < numfiles) {
					file = BIF_filelist_file(simasel->files, simasel->active_file);
					
					if(file && S_ISDIR(file->type)) {
						/* the path is too long and we are not going up! */
						if (strcmp(file->relname, ".") &&
							strcmp(file->relname, "..") &&
							strlen(simasel->dir) + strlen(file->relname) >= FILE_MAX ) 
						{
							error("Path too long, cannot enter this directory");
						} else {
							strcat(simasel->dir, file->relname);
							strcat(simasel->dir,"/");
							simasel->file[0] = '\0';
							BLI_cleanup_dir(G.sce, simasel->dir);
							BIF_filelist_setdir(simasel->files, simasel->dir);
							BIF_filelist_free(simasel->files);
							simasel->active_file = -1;
							simasel->scrollpos = 0;
							do_draw = 1;
							do_headdraw = 1;
						}
					}
					else if (file)
					{
						if (file->relname) {
							if (simasel->img) {
								IMB_freeImBuf(simasel->img);
								simasel->img = NULL;
							}
							BLI_strncpy(simasel->file, file->relname, FILE_MAXFILE);
							if(event==MIDDLEMOUSE && BIF_filelist_gettype(simasel->files)) 
								imasel_execute(simasel);
						}
						
					}	
					if(BIF_filelist_gettype(simasel->files)==FILE_MAIN) {
						active_imasel_object(simasel);
					}
				
					do_draw = 1;
				}
			}
			else {
				simasel->active_file = -1;
				if (simasel->flag & FILE_BOOKMARKS) {
					if(mval[0]>simasel->bookmarkrect.xmin && mval[0]<simasel->bookmarkrect.xmax && mval[1]>simasel->bookmarkrect.ymin && mval[1]<simasel->bookmarkrect.ymax) {
						int nentries = fsmenu_get_nentries();
						
						set_active_bookmark(simasel, mval[1]);
						if (simasel->active_bookmark >= 0 && simasel->active_bookmark < nentries) {
							char *selected= fsmenu_get_entry(simasel->active_bookmark);			
							/* which string */
							if (selected) {
								BLI_strncpy(simasel->dir, selected, sizeof(simasel->dir));
								BLI_cleanup_dir(G.sce, simasel->dir);
								BIF_filelist_free(simasel->files);	
								BIF_filelist_setdir(simasel->files, simasel->dir);
								simasel->file[0] = '\0';			
								simasel->scrollpos = 0;
								simasel->active_file = -1;
								do_headdraw = 1;
							}
						}
					} else {
						simasel->active_bookmark = -1;
					}
					do_draw= 1;
				}				
			}
			break;
		case RIGHTMOUSE:			
			getmouseco_areawin(mval);
			if(mval[0]>simasel->viewrect.xmin && mval[0]<simasel->viewrect.xmax 
				&& mval[1]>simasel->viewrect.ymin && mval[1]<simasel->viewrect.ymax) {
				set_active_file(simasel, mval[0], mval[1]);
				if(simasel->active_file >=0 && simasel->active_file<numfiles) {
					simasel->selstate = NOTACTIVE;
					file = BIF_filelist_file(simasel->files, simasel->active_file);
					if (file->flags & ACTIVE) {
						file->flags &= ~ACTIVE;
						simasel->selstate = INACTIVATE;
					}
					else {
						file->flags |= ACTIVE;
						simasel->selstate = ACTIVATE;
					}
					do_draw= 1;
				}
			}
			break;
		case MOUSEY:
		case MOUSEX:
			getmouseco_areawin(mval);
			if(mval[0]>simasel->viewrect.xmin && mval[0]<simasel->viewrect.xmax && mval[1]>simasel->viewrect.ymin && mval[1]<simasel->viewrect.ymax) {
				set_active_file(simasel, mval[0], mval[1]);
				simasel->active_bookmark = -1;
				if(simasel->active_file >=0 && simasel->active_file<numfiles) {
					file = BIF_filelist_file(simasel->files, simasel->active_file);
					if (simasel->selstate == INACTIVATE) {
						file->flags &= ~ACTIVE;
					}
					else if (simasel->selstate == ACTIVATE) {
						file->flags |= ACTIVE;
					}
					do_draw= 1;
				}
			} else {
				simasel->active_file = -1;			
				if (simasel->flag & FILE_BOOKMARKS) {
					if(mval[0]>simasel->bookmarkrect.xmin && mval[0]<simasel->bookmarkrect.xmax && mval[1]>simasel->bookmarkrect.ymin && mval[1]<simasel->bookmarkrect.ymax) {
						set_active_bookmark(simasel, mval[1]);						
					} else {
						simasel->active_bookmark = -1;
					}
					do_draw= 1;
				}
			}
			break;
		case AKEY:
			BIF_filelist_swapselect(simasel->files);
			if(simasel->type==FILE_MAIN) imasel_select_objects(simasel);
			do_draw= 1;
			break;
		case BKEY:
			toggle_blockhandler(sa, IMASEL_HANDLER_IMAGE, UI_PNL_UNSTOW);
			scrarea_queue_winredraw(sa);
			break;
		case HKEY:
			simasel->flag ^= FILE_HIDE_DOT;
			BIF_filelist_free(simasel->files);
			do_draw= 1;
			do_headdraw= 1;
			break;
		case PKEY:
			if(G.qual & LR_SHIFTKEY) {
				extern char bprogname[];	/* usiblender.c */
			
				sprintf(str, "%s -a \"%s%s\"", bprogname, simasel->dir, simasel->file);
				system(str);
			}
			else 
			{
				BIF_filelist_free(simasel->files);
				BIF_filelist_parent(simasel->files);
				BLI_strncpy(simasel->dir, BIF_filelist_dir(simasel->files), 80);
				simasel->file[0] = '\0';
				simasel->active_file = -1;
				simasel->scrollpos = 0;
				do_headdraw = 1;
			}
			do_draw = 1;	
			break;
		case XKEY:
			getmouseco_areawin(mval);			
			if (simasel->flag & FILE_BOOKMARKS) {
				if(mval[0]>simasel->bookmarkrect.xmin && mval[0]<simasel->bookmarkrect.xmax && mval[1]>simasel->bookmarkrect.ymin && mval[1]<simasel->bookmarkrect.ymax) {			
					int nentries = fsmenu_get_nentries();
					set_active_bookmark(simasel, mval[1]);
					if (simasel->active_bookmark >= 0 && simasel->active_bookmark < nentries) {
						char name[FILE_MAX];
						BLI_make_file_string(G.sce, name, BLI_gethome(), ".Bfs");
						fsmenu_remove_entry(simasel->active_bookmark);
						fsmenu_write_file(name);
						simasel->active_bookmark = -1;
						do_draw = 1;
					}
				}
			}			
			break;
		}		
	}
	else if(event==RIGHTMOUSE) {
		simasel->selstate = NOTACTIVE;		
		if(simasel->type==FILE_MAIN) imasel_select_objects(simasel);
	}
	else if(event==LEFTMOUSE) {
		if(simasel->type==FILE_MAIN) {
			getmouseco_areawin(mval);
			set_active_file(simasel, mval[0], mval[1]);
		}
	}
		/* XXX, stupid patch, curarea can become undone
		 * because of file loading... fixme zr
		 */
	if(curarea) {
		if(do_draw) scrarea_queue_winredraw(curarea);
		if(do_headdraw) scrarea_queue_headredraw(curarea);
	}
}


/* copied from filesel.c */
void clever_numbuts_imasel()
{
	SpaceImaSel *simasel;
	char orgname[FILE_MAXDIR+FILE_MAXFILE+12];
	char filename[FILE_MAXDIR+FILE_MAXFILE+12];
	char newname[FILE_MAXDIR+FILE_MAXFILE+12];
	struct direntry *file;
	int len;
	
	simasel= curarea->spacedata.first;

	if(BIF_filelist_gettype(simasel->files)==FILE_MAIN) return;
	
	len = 110;
	file = get_hilited_entry(simasel);

	if (file != NULL && !(S_ISDIR(file->type))){
		
		BLI_make_file_string(G.sce, orgname, simasel->dir, file->relname);
		BLI_strncpy(filename, file->relname, sizeof(filename));

		add_numbut(0, TEX, "", 0, len, filename, "Rename File");

		if( do_clever_numbuts("Rename File", 1, REDRAW) ) {
			BLI_make_file_string(G.sce, newname, simasel->dir, filename);

			if( strcmp(orgname, newname) != 0 ) {
				BLI_rename(orgname, newname);
				BIF_filelist_free(simasel->files);
			}
		}

		scrarea_queue_winredraw(curarea);
	}
}
