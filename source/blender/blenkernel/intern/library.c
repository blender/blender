
/*  library.c 
 * 
 *  Contains management of ID's and libraries
 *  allocate and free of all library data
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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_lattice_types.h"
#include "DNA_curve_types.h"
#include "DNA_meta_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_ika_types.h"
#include "DNA_image_types.h"
#include "DNA_wave_types.h"
#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_world_types.h"
#include "DNA_screen_types.h"
#include "DNA_vfont_types.h"
#include "DNA_text_types.h"
#include "DNA_script_types.h"
#include "DNA_sound_types.h"
#include "DNA_group_types.h"
#include "DNA_armature_types.h"
#include "DNA_action_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "BKE_bad_level_calls.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_sound.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_script.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_curve.h"
#include "BKE_mball.h"
#include "BKE_text.h"
#include "BKE_texture.h"
#include "BKE_scene.h"
#include "BKE_image.h"
#include "BKE_ika.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_world.h"
#include "BKE_font.h"
#include "BKE_group.h"
#include "BKE_lattice.h"
#include "BKE_armature.h"
#include "BKE_action.h"

#define MAX_IDPUP		30	/* was 24 */
#define MAX_LIBARRAY	100 /* was 30 */

/* ************* general ************************ */

void id_lib_extern(ID *id)
{
	if(id) {
		if(id->flag & LIB_INDIRECT) {
			id->flag -= LIB_INDIRECT;
			id->flag |= LIB_EXTERN;
		}
	}
}

void id_us_plus(ID *id)
{
	if(id) {
		id->us++;
		if(id->flag & LIB_INDIRECT) {
			id->flag -= LIB_INDIRECT;
			id->flag |= LIB_EXTERN;
		}
	}
}

ListBase *wich_libbase(Main *mainlib, short type)
{
	switch( type ) {
		case ID_SCE:
			return &(mainlib->scene);
		case ID_LI:
			return &(mainlib->library);
		case ID_OB:
			return &(mainlib->object);
		case ID_ME:
			return &(mainlib->mesh);
		case ID_CU:
			return &(mainlib->curve);
		case ID_MB:
			return &(mainlib->mball);
		case ID_MA:
			return &(mainlib->mat);
		case ID_TE:
			return &(mainlib->tex);
		case ID_IM:
			return &(mainlib->image);
		case ID_IK:
			return &(mainlib->ika);
		case ID_WV:
			return &(mainlib->wave);
		case ID_LT:
			return &(mainlib->latt);
		case ID_LA:
			return &(mainlib->lamp);
		case ID_CA:
			return &(mainlib->camera);
		case ID_IP:
			return &(mainlib->ipo);
		case ID_KE:
			return &(mainlib->key);
		case ID_WO:
			return &(mainlib->world);
		case ID_SCR:
			return &(mainlib->screen);
		case ID_VF:
			return &(mainlib->vfont);
		case ID_TXT:
			return &(mainlib->text);
		case ID_SCRIPT:
			return &(mainlib->script);
		case ID_SO:
			return &(mainlib->sound);
		case ID_SAMPLE:
			/* declared as an external in sound.h !!! */
			return (samples);
		case ID_GR:
			return &(mainlib->group);
		case ID_AR:
			return &(mainlib->armature);
		case ID_AC:
			return &(mainlib->action);
	}
	return 0;
}

int set_listbasepointers(Main *main, ListBase **lb)
{
	/* BACKWARDS! also watch order of free-ing! (mesh<->mat) */

	lb[0]= &(main->ipo);
	lb[1]= &(main->key);
	lb[2]= &(main->image);
	lb[3]= &(main->tex);
	lb[4]= &(main->mat);
	lb[5]= &(main->vfont);
	
	/* Important!: When adding a new object type,
	 * the specific data should be inserted here 
	 */

	lb[6]= &(main->armature);
	lb[7]= &(main->action);

	lb[8]= &(main->mesh);
	lb[9]= &(main->curve);
	lb[10]= &(main->mball);
	lb[11]= &(main->ika);
	lb[12]= &(main->wave);
	lb[13]= &(main->latt);
	lb[14]= &(main->lamp);
	lb[15]= &(main->camera);

	lb[16]= &(main->world);
	lb[17]= &(main->screen);
	lb[18]= &(main->object);
	lb[19]= &(main->scene);
	lb[20]= &(main->library);
	lb[21]= &(main->text);
	lb[22]= &(main->sound);
	lb[23]= &(main->group);

	lb[24]= samples;
	lb[25]= &(main->script);
	lb[26]=0;

	return 26;
}

/* *********** ALLOC AND FREE *****************
  
free_libblock(ListBase *lb, ID *id )
	provide a list-basis and datablock, but only ID is read

void *alloc_libblock(ListBase *lb, type, name)
	inserts in list and returns a new ID

 ***************************** */

static ID *alloc_libblock_notest(short type)
{
	ID *id= 0;
	
	switch( type ) {
		case ID_SCE:
			id= MEM_callocN(sizeof(Scene), "scene");
			break;
		case ID_LI:
			id= MEM_callocN(sizeof(Library), "library");
			break;
		case ID_OB:
			id= MEM_callocN(sizeof(Object), "object");
			break;
		case ID_ME:
			id= MEM_callocN(sizeof(Mesh), "mesh");
			break;
		case ID_CU:
			id= MEM_callocN(sizeof(Curve), "curve");
			break;
		case ID_MB:
			id= MEM_callocN(sizeof(MetaBall), "mball");
			break;
		case ID_MA:
			id= MEM_callocN(sizeof(Material), "mat");
			break;
		case ID_TE:
			id= MEM_callocN(sizeof(Tex), "tex");
			break;
		case ID_IM:
			id= MEM_callocN(sizeof(Image), "image");
			break;
		case ID_IK:
			id= MEM_callocN(sizeof(Ika), "ika");
			break;
		case ID_WV:
			id= MEM_callocN(sizeof(Wave), "wave");
			break;
		case ID_LT:
			id= MEM_callocN(sizeof(Lattice), "latt");
			break;
		case ID_LA:
			id= MEM_callocN(sizeof(Lamp), "lamp");
			break;
		case ID_CA:
			id= MEM_callocN(sizeof(Camera), "camera");
			break;
		case ID_IP:
			id= MEM_callocN(sizeof(Ipo), "ipo");
			break;
		case ID_KE:
			id= MEM_callocN(sizeof(Key), "key");
			break;
		case ID_WO:
			id= MEM_callocN(sizeof(World), "world");
			break;
		case ID_SCR:
			id= MEM_callocN(sizeof(bScreen), "screen");
			break;
		case ID_VF:
			id= MEM_callocN(sizeof(VFont), "vfont");
			break;
		case ID_TXT:
			id= MEM_callocN(sizeof(Text), "text");
			break;
		case ID_SCRIPT:
			id= MEM_callocN(sizeof(Script), "script");
			break;
		case ID_SO:
			id= MEM_callocN(sizeof(bSound), "sound");
			break;
		case ID_SAMPLE:
			id = MEM_callocN(sizeof(bSample), "sound");
			break;
		case ID_GR:
			id= MEM_callocN(sizeof(Group), "sound");
			break;
		case ID_AR:
			id = MEM_callocN(sizeof(bArmature), "armature");
			break;
		case ID_AC:
			id = MEM_callocN(sizeof(bAction), "action");
			break;
	}
	return id;
}

// used everywhere in blenkernel and text.c
void *alloc_libblock(ListBase *lb, short type, char *name)
{
	ID *id= 0;
	
	id= alloc_libblock_notest(type);
	if(id) {
		BLI_addtail(lb, id);
		id->us= 1;
		*( (short *)id->name )= type;
		new_id(lb, id, name);
		/* alphabetic insterion: is in new_id */
	}
	return id;
}

/* GS reads the memory pointed at in a specific ordering. There are,
 * however two definitions for it. I have jotted them down here, both,
 * but I think the first one is actually used. The thing is that
 * big-endian systems might read this the wrong way round. OTOH, we
 * constructed the IDs that are read out with this macro explicitly as
 * well. I expect we'll sort it out soon... */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

/* from misc_util: flip the bytes from x  */
/*#define GS(x) (((unsigned char *)(x))[0] << 8 | ((unsigned char *)(x))[1]) */

// used everywhere in blenkernel and text.c
void *copy_libblock(void *rt)
{
	ID *idn, *id;
	ListBase *lb;
	char *cp, *cpn;
	int idn_len;
	
	id= rt;

	lb= wich_libbase(G.main, GS(id->name));
	idn= alloc_libblock(lb, GS(id->name), id->name+2);
	
	idn_len= MEM_allocN_len(idn);
	if(idn_len - sizeof(ID) > 0) {
		cp= (char *)id;
		cpn= (char *)idn;
		memcpy(cpn+sizeof(ID), cp+sizeof(ID), idn_len - sizeof(ID));
	}
	
	id->newid= idn;
	idn->flag |= LIB_NEW;
	
	return idn;
}

static void free_library(Library *lib)
{
    /* no freeing needed for libraries yet */
}

// used in headerbuttons.c image.c mesh.c screen.c sound.c and library.c
void free_libblock(ListBase *lb, void *idv)
{
	ID *id= idv;
	
	switch( GS(id->name) ) {	/* GetShort from util.h */
		case ID_SCE:
			free_scene((Scene *)id);
			break;
		case ID_LI:
			free_library((Library *)id);
			break;
		case ID_OB:
			free_object((Object *)id);
			break;
		case ID_ME:
			free_mesh((Mesh *)id);
			break;
		case ID_CU:
			free_curve((Curve *)id);
			break;
		case ID_MB:
			free_mball((MetaBall *)id);
			break;
		case ID_MA:
			free_material((Material *)id);
			break;
		case ID_TE:
			free_texture((Tex *)id);
			break;
		case ID_IM:
			free_image((Image *)id);
			break;
		case ID_IK:
			free_ika((Ika *)id);
			break;
		case ID_WV:
			/* free_wave(id); */
			break;
		case ID_LT:
			free_lattice((Lattice *)id);
			break;
		case ID_LA:
			free_lamp((Lamp *)id);
			break;
		case ID_CA:
			free_camera((Camera*) id);
			break;
		case ID_IP:
			free_ipo((Ipo *)id);
			break;
		case ID_KE:
			free_key((Key *)id);
			break;
		case ID_WO:
			free_world((World *)id);
			break;
		case ID_SCR:
			free_screen((bScreen *)id);
			break;
		case ID_VF:
			free_vfont((VFont *)id);
			break;
		case ID_TXT:
			free_text((Text *)id);
			break;
		case ID_SCRIPT:
			free_script((Script *)id);
			break;
		case ID_SO:
			sound_free_sound((bSound *)id);
			break;
		case ID_SAMPLE:
			sound_free_sample((bSample *)id);
			break;
		case ID_GR:
			free_group((Group *)id);
			break;
		case ID_AR:
			free_armature((bArmature *)id);
			break;
		case ID_AC:
			free_action((bAction *)id);
			break;
	}

	BLI_remlink(lb, id);
	MEM_freeN(id);

	/* should not be here!! this is an interface-thing */
	allspace(OOPS_TEST, 0);
}

void free_libblock_us(ListBase *lb, void *idv)		/* test users */
{
	ID *id= idv;
	
	id->us--;

	if(id->us<0) {
		if(id->lib) printf("ERROR block %s %s users %d\n", id->lib->name, id->name, id->us);
		else printf("ERROR block %s users %d\n", id->name, id->us);
	}
	if(id->us==0) {
		if( GS(id->name)==ID_OB ) unlink_object((Object *)id);
		
		free_libblock(lb, id);
	}
}


void free_main(Main *mainvar)
{
	/* also call when reading a file, erase all, etc */
	ListBase *lbarray[MAX_LIBARRAY];
	int a;

	a= set_listbasepointers(mainvar, lbarray);
	while(a--) {
		ListBase *lb= lbarray[a];
		ID *id;
		
		while (id= lb->first) {
			free_libblock(lb, id);
		}
	}

	MEM_freeN(mainvar);
}

/* ***************** ID ************************ */

// only used in exotic.c
ID *find_id(char *type, char *name)		/* type: "OB" or "MA" etc */
{
	ID *id;
	ListBase *lb;
	
	lb= wich_libbase(G.main, GS(type));
	
	id= lb->first;
	while(id) {
		if( strcmp(id->name+2, name)==0 ) return id;
		id= id->next;
	}
	return 0;
}

static void get_flags_for_id(ID *id, char *buf) {
	int isfake= id->flag & LIB_FAKEUSER;

		/* Writeout the flags for the entry, note there
		 * is a small hack that writes 5 spaces instead
		 * of 4 if no flags are displayed... this makes
		 * things usually line up ok - better would be
		 * to have that explicit, oh well - zr
		 */

	if (id->us<0)
		sprintf(buf, "-1W ");
	else if (!id->lib && !isfake && id->us)
		sprintf(buf, "     ");
	else
		sprintf(buf, "%c%c%c ", id->lib?'L':' ', isfake?'F':' ', (id->us==0)?'O':' ');
}

static void IDnames_to_dyn_pupstring(DynStr *pupds, ListBase *lb, ID *link, short *nr)
{
	int i, nids= BLI_countlist(lb);
		
	*nr= -1;
	
	if (nids>MAX_IDPUP) {
		BLI_dynstr_append(pupds, "DataBrowse %x-2");
	} else {
		ID *id;
		
		for (i=0, id= lb->first; id; id= id->next, i++) {
			char buf[32];
			
			if (id==link)
				*nr= i+1;
			
			get_flags_for_id(id, buf);
				
			BLI_dynstr_append(pupds, buf);
			BLI_dynstr_append(pupds, id->name+2);
			sprintf(buf, "%%x%d", i+1);
			BLI_dynstr_append(pupds, buf);
			
			if(id->next)
				BLI_dynstr_append(pupds, "|");
		}
	}
}

	/* Silly routine, the only difference between the one
	 * above is that it only adds items with a matching
	 * blocktype... this should be unified somehow... - zr
	 */
static void IPOnames_to_dyn_pupstring(DynStr *pupds, ListBase *lb, ID *link, short *nr, int blocktype)
{
	ID *id;
	int i, nids;
	
	for (id= lb->first, nids= 0; id; id= id->next) {
		Ipo *ipo= (Ipo*) id;
		
		if (ipo->blocktype==blocktype)
			nids++;
	}
	
	if (nids>MAX_IDPUP) {
		BLI_dynstr_append(pupds, "DataBrowse %x-2");
	} else {
		for (i=0, id= lb->first; id; id= id->next) {
			Ipo *ipo= (Ipo*) id;
			
			if (ipo->blocktype==blocktype) {
				char buf[32];
			
				if (id==link)
					*nr= i+1;
					
				get_flags_for_id(id, buf);
				
				BLI_dynstr_append(pupds, buf);
				BLI_dynstr_append(pupds, id->name+2);
				sprintf(buf, "%%x%d", i+1);
				BLI_dynstr_append(pupds, buf);
				
				if(id->next)
					BLI_dynstr_append(pupds, "|");
				
				i++;
			}
		}
	}
}

// used by headerbuttons.c buttons.c editobject.c editseq.c
void IDnames_to_pupstring(char **str, char *title, char *extraops, ListBase *lb, ID *link, short *nr)
{
	DynStr *pupds= BLI_dynstr_new();

	if (title) {
		BLI_dynstr_append(pupds, title);
		BLI_dynstr_append(pupds, "%t|");
	}
	
	if (extraops) {
		BLI_dynstr_append(pupds, extraops);
		if (BLI_dynstr_get_len(pupds))
			BLI_dynstr_append(pupds, "|");
	}

	IDnames_to_dyn_pupstring(pupds, lb, link, nr);
	
	*str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
}

// only used by headerbuttons.c
void IPOnames_to_pupstring(char **str, char *title, char *extraops, ListBase *lb, ID *link, short *nr, int blocktype)
{
	DynStr *pupds= BLI_dynstr_new();
	
	if (title) {
		BLI_dynstr_append(pupds, title);
		BLI_dynstr_append(pupds, "%t|");
	}
	
	if (extraops) {
		BLI_dynstr_append(pupds, extraops);
		if (BLI_dynstr_get_len(pupds))
			BLI_dynstr_append(pupds, "|");
	}

	IPOnames_to_dyn_pupstring(pupds, lb, link, nr, blocktype);	
	
	*str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
}

// used by buttons.c library.c mball.c
void splitIDname(char *name, char *left, int *nr)
{
	int a;
	
	*nr= 0;
	strncpy(left, name, 21);
	
	a= strlen(name);
	if(a>1 && name[a-1]=='.') return;
	
	while(a--) {
		if( name[a]=='.' ) {
			left[a]= 0;
			*nr= atol(name+a+1);
			return;
		}
		if( isdigit(name[a])==0 ) break;
		
		left[a]= 0;
	}
	strcpy(left, name);	
}

static void sort_alpha_id(ListBase *lb, ID *id)
{
	ID *idtest;
	
	/* insert alphabetically */
	if(lb->first!=lb->last) {
		BLI_remlink(lb, id);
		
		idtest= lb->first;
		while(idtest) {
			if(strcasecmp(idtest->name, id->name)>0 || idtest->lib) {
				BLI_insertlinkbefore(lb, idtest, id);
				break;
			}
			idtest= idtest->next;
		}
		/* as last */
		if(idtest==0) {
			BLI_addtail(lb, id);
		}
	}
	
}

int new_id(ListBase *lb, ID *id, char *tname)
/* only for local blocks: external en indirect blocks already have a unique ID */
/* return 1: created a new name */
{
	ID *idtest;
	int nr= 0, nrtest, maxtest=32, a;
	char aname[32], *name, left[24], leftest[24], in_use[32];
	
	/* - split name
	 * - search
	 */

	if(id->lib) return 0;

	if(tname==0) name= id->name+2;
	else {
		/* tname can be const */
		strncpy(aname, tname, 21);
		name= aname;
		
		if( strlen(name) > 21 ) name[21]= 0;
	}

	if(lb==0) lb= wich_libbase(G.main, GS(id->name));

	/* phase 1: id already exists? */
	idtest= lb->first;
	while(idtest) {
	
		if(id!=idtest && idtest->lib==0) {
			
			/* do not test alphabetic! */
			/* optimized */
			if( idtest->name[2] == name[0] ) {
				if(strcmp(name, idtest->name+2)==0) break;
			}
		}
		
		idtest= idtest->next;
	}	

	/* if there is no double return */
	if(idtest==0) {
		strcpy(id->name+2, name);
		return 0;
	}
	
	memset(in_use, 0, maxtest);

	splitIDname(name, left, &nr);
	if(nr>999 && strlen(left)>16) left[16]= 0;
	else if(strlen(left)>17) left[17]= 0;


	idtest= lb->first;
	while(idtest) {
	
		if(id!=idtest && idtest->lib==0) {
			
			splitIDname(idtest->name+2, leftest, &nrtest);
			if(strcmp(left, leftest)==0) {
				
				if(nrtest<maxtest) in_use[nrtest]= 1;
				if(nr <= nrtest) nr= nrtest+1;
			}
		}
		
		idtest= idtest->next;
	}
	
	for(a=0; a<maxtest; a++) {
		if(a>=nr) break;
		if( in_use[a]==0 ) {
			nr= a;
			break;
		}
	}
	
	if(nr==0) sprintf(id->name+2, "%s", left);
	else {
		if (nr >= 1000 && strlen(left) > 16) {
			// this would overflow name buffer
			left[16]= 0;
			return (new_id(lb, id, left));
		}
		/* this format specifier is from hell... */
		sprintf(id->name+2, "%s.%0.3d", left, nr);
	}
	
	sort_alpha_id(lb, id);	

	return 1;
}

// next to indirect usage in read/writefile also in editobject.c scene.c
void clear_id_newpoins()
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id;
	int a;

	a= set_listbasepointers(G.main, lbarray);
	while(a--) {
		id= lbarray[a]->first;
		while(id) {
			id->newid= 0;
			id->flag &= ~LIB_NEW;
			id= id->next;
		}
	}
}

void all_local(void)
{
	ListBase *lbarray[MAX_LIBARRAY], tempbase={0, 0};
	ID *id, *idn;
	int a;

	a= set_listbasepointers(G.main, lbarray);
	while(a--) {
		id= lbarray[a]->first;
		
		while(id) {
			id->newid= 0;
			id->flag &= ~(LIB_EXTERN|LIB_INDIRECT|LIB_NEW);
			
			idn= id->next;		/* id is possibly being inserted again */
			if(id->lib) {
				id->lib= 0;
				new_id(lbarray[a], id, 0);	/* new_id only does it with double names */
				sort_alpha_id(lbarray[a], id);
			}
			else {
				/* patch: check for alphabetic ordering */
			        /* has been removed... why!? (ton) */
/*
				if(idn) {
					if(strcasecmp(id->name, idn->name)>0) {
						remlink(lbarray[a], id);
						addtail(&tempbase, id);
					}
					else if(id->prev) {
						idp= id->prev;
						if(strcasecmp(idp->name, id->name)>0) {
							remlink(lbarray[a], id);
							addtail(&tempbase, id);
						}
					}
				}
*/				
			}
			
			id= idn;
		}
		
		/* patch2: make it aphabetically */
		while( (id=tempbase.first) ) {
			BLI_remlink(&tempbase, id);
			BLI_addtail(lbarray[a], id);
			new_id(lbarray[a], id, 0);
		}
	}
}


void test_idbutton(char *name)
{
	/* called from buttons: when name already exists: call new_id */
	ListBase *lb;
	ID *idtest;
	

	lb= wich_libbase(G.main, GS(name-2) );
	if(lb==0) return;
	
	/* search for id */
	idtest= lb->first;
	while(idtest) {
		if( strcmp(idtest->name+2, name)==0) break;
		idtest= idtest->next;
	}

	if(idtest) if( new_id(lb, idtest, name)==0 ) sort_alpha_id(lb, idtest);
}

void rename_id(ID *id, char *name)
{
	ListBase *lb;
	
	strncpy(id->name+2, name, 21);
	lb= wich_libbase(G.main, GS(id->name) );
	
	new_id(lb, id, name);				
}

