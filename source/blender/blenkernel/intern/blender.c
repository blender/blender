
/*  blender.c   jan 94     MIXED MODEL
 * 
 * common help functions and data
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

#include "BLI_blenlib.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "DNA_screen_types.h"

#include "BKE_library.h"
#include "BKE_blender.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_effect.h"
#include "BKE_curve.h"
#include "BKE_font.h"

#include "BKE_bad_level_calls.h" /* for BPY_do_pyscript */

#include "BLO_readfile.h" /* for BLO_read_file */

#include "BKE_bad_level_calls.h" // for freeAllRad editNurb free_editMesh free_editText free_editArmature
#include "BKE_utildefines.h" // O_BINARY FALSE

#include "nla.h"

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
	free_main(G.main);
	G.main= NULL;

	IMB_freeImBufdata();		/* imbuf lib */
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

void initglobals(void)
{
	memset(&G, 0, sizeof(Global));
	
	U.savetime= 1;

	G.animspeed= 4;

	G.main= MEM_callocN(sizeof(Main), "initglobals");

	strcpy(G.ima, "//");

	G.version= BLENDER_VERSION;

	G.order= 1;
	G.order= (((char*)&G.order)[0])?L_ENDIAN:B_ENDIAN;

	sprintf(versionstr, "www.blender.org %d", G.version);

	clear_workob();	/* object.c */
}

/***/

static void clear_global(void) {
	extern short winqueue_break;	/* screen.c */

	freeAllRad();
	free_main(G.main); /* free all lib data */
	freefastshade();	/* othwerwise old lamp settings stay active */


	/* prevent hanging vars */	
	R.backbuf= 0;
	
	/* force all queues to be left */
	winqueue_break= 1;
	
	if (G.obedit) {
		freeNurblist(&editNurb);
		free_editMesh();
		free_editText();
		free_editArmature();
	}

	G.curscreen= NULL;
	G.scene= NULL;
	G.main= NULL;
	
	G.obedit= NULL;
	G.obpose= NULL;
	G.saction= NULL;
	G.buts= NULL;
	G.v2d= NULL;
	G.vd= NULL;
	G.soops= NULL;
	G.sima= NULL;
	G.sipo= NULL;
	
	G.f &= ~(G_WEIGHTPAINT + G_VERTEXPAINT + G_FACESELECT);
}

static void setup_app_data(BlendFileData *bfd, char *filename) {
	Object *ob;
	
	clear_global();
	
	G.save_over = 1;
	
	G.main= bfd->main;
	if (bfd->user) {
		U= *bfd->user;
		MEM_freeN(bfd->user);
		if(U.wheellinescroll == 0) U.wheellinescroll = 3;
	}
	
	R.winpos= bfd->winpos;
	R.displaymode= bfd->displaymode;
	G.curscreen= bfd->curscreen;
	G.fileflags= bfd->fileflags;
	
	G.scene= G.curscreen->scene;
	
		/* few DispLists, but do text_to_curve */
	// this should be removed!!! But first a better displist system (ton)
	for (ob= G.main->object.first; ob; ob= ob->id.next) {
		if(ob->type==OB_FONT) {
			Curve *cu= ob->data;
			if(cu->nurb.first==0) text_to_curve(ob, 0);
		}
		else if(ob->type==OB_MESH) {
			makeDispList(ob);
			if(ob->effect.first) object_wave(ob);
		}
	}
	
	if (!G.background) {
		setscreen(G.curscreen);
	}
		/* baseflags */
	set_scene_bg(G.scene);
	
	if (G.f & G_SCENESCRIPT) {
		BPY_do_pyscript(&G.scene->id, SCRIPT_ONLOAD);
	}
	
	strcpy(G.sce, filename);
	strcpy(G.main->name, filename); /* is guaranteed current file */
	
	MEM_freeN(bfd);
}

int BKE_read_file(char *dir, void *type_r) {
	BlendReadError bre;
	BlendFileData *bfd;
	
	if (!G.background)
		waitcursor(1);
		
	bfd= BLO_read_from_file(dir, &bre);
	if (bfd) {
		if (type_r)
			*((BlenFileType*)type_r)= bfd->type;
		
		setup_app_data(bfd, dir);
	} else {
		error("Loading %s failed: %s", dir, BLO_bre_as_string(bre));
	}
	
	if (!G.background)
		waitcursor(0);
	
	return (bfd?1:0);
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
		
		setup_app_data(bfd, "<memory>");
	} else {
		error("Loading failed: %s", BLO_bre_as_string(bre));
	}
	
	if (!G.background)
		waitcursor(0);
	
	return (bfd?1:0);
}
