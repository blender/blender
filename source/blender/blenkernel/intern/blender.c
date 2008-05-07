/*  blender.c   jan 94     MIXED MODEL
 * 
 * common help functions and data
 * 
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

#ifndef WIN32 
    #include <unistd.h> // for read close
    #include <sys/param.h> // for MAXPATHLEN
#else
    #include <io.h> // for open close read
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h> // for open

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_sdna_types.h"
#include "DNA_userdef_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "BLI_editVert.h"

#include "BLO_undofile.h"
#include "BLO_readfile.h" 
#include "BLO_writefile.h" 

#include "BKE_bad_level_calls.h" // for freeAllRad editNurb free_editMesh free_editText free_editArmature
#include "BKE_utildefines.h" // O_BINARY FALSE
#include "BIF_mainqueue.h" // mainqenter for onload script
#include "mydevice.h"
#include "nla.h"
#include "blendef.h"

Global G;
UserDef U;

char versionstr[48]= "";

/* ************************************************ */
/* pushpop facility: to store data temporally, FIFO! */

ListBase ppmain={0, 0};

typedef struct PushPop {
	struct PushPop *next, *prev;
	void *data;
	int len;
} PushPop;

void pushdata(void *data, int len)
{
	PushPop *pp;
	
	pp= MEM_mallocN(sizeof(PushPop), "pushpop");
	BLI_addtail(&ppmain, pp);
	pp->data= MEM_mallocN(len, "pushpop");
	pp->len= len;
	memcpy(pp->data, data, len);
}

void popfirst(void *data)
{
	PushPop *pp;
	
	pp= ppmain.first;
	if(pp) {
		memcpy(data, pp->data, pp->len);
		BLI_remlink(&ppmain, pp);
		MEM_freeN(pp->data);
		MEM_freeN(pp);
	}
	else printf("error in popfirst\n");
}

void poplast(void *data)
{
	PushPop *pp;
	
	pp= ppmain.last;
	if(pp) {
		memcpy(data, pp->data, pp->len);
		BLI_remlink(&ppmain, pp);
		MEM_freeN(pp->data);
		MEM_freeN(pp);
	}
	else printf("error in poplast\n");
}

void free_pushpop()
{
	PushPop *pp;

	pp= ppmain.first;
	while(pp) {
		BLI_remlink(&ppmain, pp);
		MEM_freeN(pp->data);
		MEM_freeN(pp);
	}	
}

void pushpop_test()
{
	if(ppmain.first) printf("pushpop not empty\n");
	free_pushpop();
}



/* ********** free ********** */

void free_blender(void)
{
	/* samples are in a global list..., also sets G.main->sound->sample NULL */
	sound_free_all_samples();
	
	free_main(G.main);
	G.main= NULL;

	IMB_freeImBufdata();		/* imbuf lib */
	
	free_nodesystem();	
}

void duplicatelist(ListBase *list1, ListBase *list2)  /* copy from 2 to 1 */
{
	struct Link *link1, *link2;
	
	list1->first= list1->last= 0;
	
	link2= list2->first;
	while(link2) {

		link1= MEM_dupallocN(link2);
		BLI_addtail(list1, link1);
		
		link2= link2->next;
	}	
}

static EditMesh theEditMesh;

void initglobals(void)
{
	memset(&G, 0, sizeof(Global));
	
	memset(&theEditMesh, 0, sizeof(theEditMesh));
	G.editMesh = &theEditMesh;

	U.savetime= 1;

	G.main= MEM_callocN(sizeof(Main), "initglobals");

	strcpy(G.ima, "//");

	G.version= BLENDER_VERSION;

	G.order= 1;
	G.order= (((char*)&G.order)[0])?L_ENDIAN:B_ENDIAN;

	if(BLENDER_SUBVERSION)
		sprintf(versionstr, "www.blender.org %d.%d", G.version, BLENDER_SUBVERSION);
	else
		sprintf(versionstr, "www.blender.org %d", G.version);

#ifdef _WIN32	// FULLSCREEN
	G.windowstate = G_WINDOWSTATE_USERDEF;
#endif

	clear_workob();	/* object.c */

	G.charstart = 0x0000;
	G.charmin = 0x0000;
	G.charmax = 0xffff;
}

/***/

static void clear_global(void) 
{
	extern short winqueue_break;	/* screen.c */

	freeAllRad();
	fastshade_free_render();	/* lamps hang otherwise */
	free_main(G.main);			/* free all lib data */

	/* force all queues to be left */
	winqueue_break= 1;
	
	if (G.obedit) {
		freeNurblist(&editNurb);
		free_editMesh(G.editMesh);
		free_editText();
		free_editArmature();
	}

	G.curscreen= NULL;
	G.scene= NULL;
	G.main= NULL;
	
	G.obedit= NULL;
	G.saction= NULL;
	G.buts= NULL;
	G.v2d= NULL;
	G.vd= NULL;
	G.soops= NULL;
	G.sima= NULL;
	G.sipo= NULL;
	
	free_vertexpaint();
	
	G.f &= ~(G_WEIGHTPAINT + G_VERTEXPAINT + G_FACESELECT + G_PARTICLEEDIT);
}

/* make sure path names are correct for OS */
static void clean_paths(Main *main)
{
	Image *image= main->image.first;
	bSound *sound= main->sound.first;
	Scene *scene= main->scene.first;
	Editing *ed;
	Sequence *seq;
	Strip *strip;
	
	while(image) {
		BLI_clean(image->name);
		image= image->id.next;
	}
	
	while(sound) {
		BLI_clean(sound->name);
		sound= sound->id.next;
	}
	
	while(scene) {
		ed= scene->ed;
		if(ed) {
			seq= ed->seqbasep->first;
			while(seq) {
				if(seq->plugin) {
					BLI_clean(seq->plugin->name);
				}
				strip= seq->strip;
				while(strip) {
					BLI_clean(strip->dir);
					strip= strip->next;
				}
				seq= seq->next;
			}
		}
		BLI_clean(scene->r.backbuf);
		BLI_clean(scene->r.pic);
		
		scene= scene->id.next;
	}
}

static void setup_app_data(BlendFileData *bfd, char *filename) 
{
	Object *ob;
	bScreen *curscreen= NULL;
	Scene *curscene= NULL;
	char mode;
	
	/* 'u' = undo save, 'n' = no UI load */
	if(bfd->main->screen.first==NULL) mode= 'u';
	else if(G.fileflags & G_FILE_NO_UI) mode= 'n';
	else mode= 0;
	
	clean_paths(bfd->main);
	
	/* no load screens? */
	if(mode) {
		/* comes from readfile.c */
		extern void lib_link_screen_restore(Main *, Scene *);
		
		SWAP(ListBase, G.main->screen, bfd->main->screen);
		SWAP(ListBase, G.main->script, bfd->main->script);
		
		/* we re-use current screen */
		curscreen= G.curscreen;
		/* but use new Scene pointer */
		curscene= bfd->curscene;
		if(curscene==NULL) curscene= bfd->main->scene.first;
		/* and we enforce curscene to be in current screen */
		curscreen->scene= curscene;

		/* clear_global will free G.main, here we can still restore pointers */
		lib_link_screen_restore(bfd->main, curscene);
	}
	
	clear_global();	/* free Main database */
	
	if(mode!='u') G.save_over = 1;
	
	G.main= bfd->main;
	if (bfd->user) {
		
		/* only here free userdef themes... */
		BLI_freelistN(&U.themes);

		U= *bfd->user;
		MEM_freeN(bfd->user);
		
	}
	
	/* samples is a global list... */
	sound_free_all_samples();
	
	/* case G_FILE_NO_UI or no screens in file */
	if(mode) {
		G.curscreen= curscreen;
		G.scene= curscene;
	}
	else {
		G.winpos= bfd->winpos;
		G.displaymode= bfd->displaymode;
		G.fileflags= bfd->fileflags;
		G.curscreen= bfd->curscreen;
		G.scene= G.curscreen->scene;
	}
	/* this can happen when active scene was lib-linked, and doesnt exist anymore */
	if(G.scene==NULL) {
		G.scene= G.main->scene.first;
		G.curscreen->scene= G.scene;
	}

	/* special cases, override loaded flags: */
	if (G.f & G_DEBUG) bfd->globalf |= G_DEBUG;
	else bfd->globalf &= ~G_DEBUG;

	if ((U.flag & USER_DONT_DOSCRIPTLINKS)) bfd->globalf &= ~G_DOSCRIPTLINKS;

	G.f= bfd->globalf;

	if (!G.background) {
		setscreen(G.curscreen);
	}
	
	/* baseflags, groups, make depsgraph, etc */
	set_scene_bg(G.scene);

	/* clear BONE_UNKEYED flags, these are not valid anymore for proxies */
	framechange_poses_clear_unkeyed();

	/* last stage of do_versions actually, that sets recalc flags for recalc poses */
	for(ob= G.main->object.first; ob; ob= ob->id.next) {
		if(ob->type==OB_ARMATURE)
			if(ob->recalc) object_handle_update(ob);
	}
	
	/* now tag update flags, to ensure deformers get calculated on redraw */
	DAG_scene_update_flags(G.scene, G.scene->lay);
	
	if (G.f & G_DOSCRIPTLINKS) {
		/* there's an onload scriptlink to execute in screenmain */
		mainqenter(ONLOAD_SCRIPT, 1);
	}
	if (G.sce != filename) /* these are the same at times, should never copy to the same location */
		strcpy(G.sce, filename);
	
	BLI_strncpy(G.main->name, filename, FILE_MAX); /* is guaranteed current file */
	
	MEM_freeN(bfd);
}

static void handle_subversion_warning(Main *main)
{
	if(main->minversionfile > BLENDER_VERSION ||
	   (main->minversionfile == BLENDER_VERSION && 
		 main->minsubversionfile > BLENDER_SUBVERSION)) {
		
		char str[128];
		
		sprintf(str, "File written by newer Blender binary: %d.%d , expect loss of data!", main->minversionfile, main->minsubversionfile);
		error(str);
	}
		
}

/* returns:
   0: no load file
   1: OK
   2: OK, and with new user settings
*/

int BKE_read_file(char *dir, void *type_r) 
{
	BlendReadError bre;
	BlendFileData *bfd;
	int retval= 1;
	
	if (!G.background)
		waitcursor(1);
		
	bfd= BLO_read_from_file(dir, &bre);
	if (bfd) {
		if(bfd->user) retval= 2;
		if (type_r)
			*((BlenFileType*)type_r)= bfd->type;
		
		setup_app_data(bfd, dir);
		
		handle_subversion_warning(G.main);
	} 
	else {
		error("Loading %s failed: %s", dir, BLO_bre_as_string(bre));
	}
	
	if (!G.background)
		waitcursor(0);
	
	return (bfd?retval:0);
}

int BKE_read_file_from_memory(char* filebuf, int filelength, void *type_r)
{
	BlendReadError bre;
	BlendFileData *bfd;
	
	if (!G.background)
		waitcursor(1);
		
	bfd= BLO_read_from_memory(filebuf, filelength, &bre);
	if (bfd) {
		if (type_r)
			*((BlenFileType*)type_r)= bfd->type;
		
		setup_app_data(bfd, "<memory2>");
	} else {
		error("Loading failed: %s", BLO_bre_as_string(bre));
	}
	
	if (!G.background)
		waitcursor(0);
	
	return (bfd?1:0);
}

/* memfile is the undo buffer */
int BKE_read_file_from_memfile(MemFile *memfile)
{
	BlendReadError bre;
	BlendFileData *bfd;
	
	if (!G.background)
		waitcursor(1);
		
	bfd= BLO_read_from_memfile(G.sce, memfile, &bre);
	if (bfd) {
		setup_app_data(bfd, "<memory1>");
	} else {
		error("Loading failed: %s", BLO_bre_as_string(bre));
	}
	
	if (!G.background)
		waitcursor(0);
	
	return (bfd?1:0);
}


/* ***************** GLOBAL UNDO *************** */

#define UNDO_DISK	0

#define MAXUNDONAME	64
typedef struct UndoElem {
	struct UndoElem *next, *prev;
	char str[FILE_MAXDIR+FILE_MAXFILE];
	char name[MAXUNDONAME];
	MemFile memfile;
} UndoElem;

static ListBase undobase={NULL, NULL};
static UndoElem *curundo= NULL;


static int read_undosave(UndoElem *uel)
{
	char scestr[FILE_MAXDIR+FILE_MAXFILE];
	int success=0, fileflags;
	
	strcpy(scestr, G.sce);	/* temporal store */
	fileflags= G.fileflags;
	G.fileflags |= G_FILE_NO_UI;

	if(UNDO_DISK) 
		success= BKE_read_file(uel->str, NULL);
	else
		success= BKE_read_file_from_memfile(&uel->memfile);
	
	/* restore */
	strcpy(G.sce, scestr);
	G.fileflags= fileflags;

	return success;
}

/* name can be a dynamic string */
void BKE_write_undo(char *name)
{
	int nr, success;
	UndoElem *uel;
	
	if( (U.uiflag & USER_GLOBALUNDO)==0) return;
	if( U.undosteps==0) return;
	
	/* remove all undos after (also when curundo==NULL) */
	while(undobase.last != curundo) {
		uel= undobase.last;
		BLI_remlink(&undobase, uel);
		BLO_free_memfile(&uel->memfile);
		MEM_freeN(uel);
	}
	
	/* make new */
	curundo= uel= MEM_callocN(sizeof(UndoElem), "undo file");
	strncpy(uel->name, name, MAXUNDONAME-1);
	BLI_addtail(&undobase, uel);
	
	/* and limit amount to the maximum */
	nr= 0;
	uel= undobase.last;
	while(uel) {
		nr++;
		if(nr==U.undosteps) break;
		uel= uel->prev;
	}
	if(uel) {
		while(undobase.first!=uel) {
			UndoElem *first= undobase.first;
			BLI_remlink(&undobase, first);
			/* the merge is because of compression */
			BLO_merge_memfile(&first->memfile, &first->next->memfile);
			MEM_freeN(first);
		}
	}


	/* disk save version */
	if(UNDO_DISK) {
		static int counter= 0;
		char *err, tstr[FILE_MAXDIR+FILE_MAXFILE];
		char numstr[32];
		
		/* calculate current filename */
		counter++;
		counter= counter % U.undosteps;	
	
		sprintf(numstr, "%d.blend", counter);
		BLI_make_file_string("/", tstr, btempdir, numstr);
	
		success= BLO_write_file(tstr, G.fileflags, &err);
		
		strcpy(curundo->str, tstr);
	}
	else {
		MemFile *prevfile=NULL;
		char *err;
		
		if(curundo->prev) prevfile= &(curundo->prev->memfile);
		
		success= BLO_write_file_mem(prevfile, &curundo->memfile, G.fileflags, &err);
		
	}
}

/* 1= an undo, -1 is a redo. we have to make sure 'curundo' remains at current situation */
void BKE_undo_step(int step)
{
	
	if(step==0) {
		read_undosave(curundo);
	}
	else if(step==1) {
		/* curundo should never be NULL, after restart or load file it should call undo_save */
		if(curundo==NULL || curundo->prev==NULL) error("No undo available");
		else {
			if(G.f & G_DEBUG) printf("undo %s\n", curundo->name);
			curundo= curundo->prev;
			read_undosave(curundo);
		}
	}
	else {
		
		/* curundo has to remain current situation! */
		
		if(curundo==NULL || curundo->next==NULL) error("No redo available");
		else {
			read_undosave(curundo->next);
			curundo= curundo->next;
			if(G.f & G_DEBUG) printf("redo %s\n", curundo->name);
		}
	}
}

void BKE_reset_undo(void)
{
	UndoElem *uel;
	
	uel= undobase.first;
	while(uel) {
		BLO_free_memfile(&uel->memfile);
		uel= uel->next;
	}
	
	BLI_freelistN(&undobase);
	curundo= NULL;
}

/* based on index nr it does a restore */
void BKE_undo_number(int nr)
{
	UndoElem *uel;
	int a=1;
	
	for(uel= undobase.first; uel; uel= uel->next, a++) {
		if(a==nr) break;
	}
	curundo= uel;
	BKE_undo_step(0);
}

char *BKE_undo_menu_string(void)
{
	UndoElem *uel;
	DynStr *ds= BLI_dynstr_new();
	char *menu;
	
	BLI_dynstr_append(ds, "Global Undo History %t");
	
	for(uel= undobase.first; uel; uel= uel->next) {
		BLI_dynstr_append(ds, "|");
		BLI_dynstr_append(ds, uel->name);
	}
	
	menu= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	return menu;
}

	/* saves quit.blend */
void BKE_undo_save_quit(void)
{
	UndoElem *uel;
	MemFileChunk *chunk;
	int file;
	char str[FILE_MAXDIR+FILE_MAXFILE];
	
	if( (U.uiflag & USER_GLOBALUNDO)==0) return;
	
	uel= curundo;
	if(uel==NULL) {
		printf("No undo buffer to save recovery file\n");
		return;
	}
	
	/* no undo state to save */
	if(undobase.first==undobase.last) return;
		
	BLI_make_file_string("/", str, btempdir, "quit.blend");

	file = open(str,O_BINARY+O_WRONLY+O_CREAT+O_TRUNC, 0666);
	if(file == -1) {
		error("Unable to save %s, check you have permissions", str);
		return;
	}

	chunk= uel->memfile.chunks.first;
	while(chunk) {
		if( write(file, chunk->buf, chunk->size) != chunk->size) break;
		chunk= chunk->next;
	}
	
	close(file);
	
	if(chunk) error("Unable to save %s, internal error", str);
	else printf("Saved session recovery to %s\n", str);
}

