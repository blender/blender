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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/*
 *  Contains management of ID's and libraries
 *  allocate and free of all library data
 * 
 */


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_material_types.h"
#include "DNA_wave_types.h"
#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_world_types.h"
#include "DNA_screen_types.h"
#include "DNA_vfont_types.h"
#include "DNA_text_types.h"
#include "DNA_sound_types.h"
#include "DNA_group_types.h"
#include "DNA_armature_types.h"
#include "DNA_node_types.h"
#include "DNA_nla_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_anim_types.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_sound.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_curve.h"
#include "BKE_mball.h"
#include "BKE_text.h"
#include "BKE_texture.h"
#include "BKE_scene.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_world.h"
#include "BKE_font.h"
#include "BKE_group.h"
#include "BKE_lattice.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_node.h"
#include "BKE_brush.h"
#include "BKE_idprop.h"
#include "BKE_particle.h"
#include "BKE_gpencil.h"
#include "BKE_fcurve.h"

#define MAX_IDPUP		60	/* was 24 */

/* GS reads the memory pointed at in a specific ordering. 
   only use this definition, makes little and big endian systems
   work fine, in conjunction with MAKE_ID */

/* from blendef: */
#define GS(a)	(*((short *)(a)))

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

void id_us_min(ID *id)
{
	if(id)
		id->us--;
}

int id_make_local(ID *id, int test)
{
	if(id->flag & LIB_INDIRECT)
		return 0;

	switch(GS(id->name)) {
		case ID_SCE:
			return 0; /* not implemented */
		case ID_LI:
			return 0; /* can't be linked */
		case ID_OB:
			if(!test) make_local_object((Object*)id);
			return 1;
		case ID_ME:
			if(!test) {
				make_local_mesh((Mesh*)id);
				make_local_key(((Mesh*)id)->key);
			}
			return 1;
		case ID_CU:
			if(!test) {
				make_local_curve((Curve*)id);
				make_local_key(((Curve*)id)->key);
			}
			return 1;
		case ID_MB:
			if(!test) make_local_mball((MetaBall*)id);
			return 1;
		case ID_MA:
			if(!test) make_local_material((Material*)id);
			return 1;
		case ID_TE:
			if(!test) make_local_texture((Tex*)id);
			return 1;
		case ID_IM:
			return 0; /* not implemented */
		case ID_WV:
			return 0; /* deprecated */
		case ID_LT:
			if(!test) {
				make_local_lattice((Lattice*)id);
				make_local_key(((Lattice*)id)->key);
			}
			return 1;
		case ID_LA:
			if(!test) make_local_lamp((Lamp*)id);
			return 1;
		case ID_CA:
			if(!test) make_local_camera((Camera*)id);
			return 1;
		case ID_IP:
			return 0; /* deprecated */
		case ID_KE:
			if(!test) make_local_key((Key*)id);
			return 1;
		case ID_WO:
			if(!test) make_local_world((World*)id);
			return 1;
		case ID_SCR:
			return 0; /* can't be linked */
		case ID_VF:
			return 0; /* not implemented */
		case ID_TXT:
			return 0; /* not implemented */
		case ID_SCRIPT:
			return 0; /* deprecated */
		case ID_SO:
			return 0; /* not implemented */
		case ID_GR:
			return 0; /* not implemented */
		case ID_AR:
			if(!test) make_local_armature((bArmature*)id);
			return 1;
		case ID_AC:
			if(!test) make_local_action((bAction*)id);
			return 1;
		case ID_NT:
			return 0; /* not implemented */
		case ID_BR:
			if(!test) make_local_brush((Brush*)id);
			return 1;
		case ID_PA:
			if(!test) make_local_particlesettings((ParticleSettings*)id);
			return 1;
		case ID_WM:
			return 0; /* can't be linked */
		case ID_GD:
			return 0; /* not implemented */
	}

	return 0;
}

int id_copy(ID *id, ID **newid, int test)
{
	if(!test) *newid= NULL;

	/* conventions:
	 * - make shallow copy, only this ID block
	 * - id.us of the new ID is set to 1 */
	switch(GS(id->name)) {
		case ID_SCE:
			return 0; /* can't be copied from here */
		case ID_LI:
			return 0; /* can't be copied from here */
		case ID_OB:
			if(!test) *newid= (ID*)copy_object((Object*)id);
			return 1;
		case ID_ME:
			if(!test) *newid= (ID*)copy_mesh((Mesh*)id);
			return 1;
		case ID_CU:
			if(!test) *newid= (ID*)copy_curve((Curve*)id);
			return 1;
		case ID_MB:
			if(!test) *newid= (ID*)copy_mball((MetaBall*)id);
			return 1;
		case ID_MA:
			if(!test) *newid= (ID*)copy_material((Material*)id);
			return 1;
		case ID_TE:
			if(!test) *newid= (ID*)copy_texture((Tex*)id);
			return 1;
		case ID_IM:
			return 0; /* not implemented */
		case ID_WV:
			return 0; /* deprecated */
		case ID_LT:
			if(!test) *newid= (ID*)copy_lattice((Lattice*)id);
			return 1;
		case ID_LA:
			if(!test) *newid= (ID*)copy_lamp((Lamp*)id);
			return 1;
		case ID_CA:
			if(!test) *newid= (ID*)copy_camera((Camera*)id);
			return 1;
		case ID_IP:
			return 0; /* deprecated */
		case ID_KE:
			if(!test) *newid= (ID*)copy_key((Key*)id);
			return 1;
		case ID_WO:
			if(!test) *newid= (ID*)copy_world((World*)id);
			return 1;
		case ID_SCR:
			return 0; /* can't be copied from here */
		case ID_VF:
			return 0; /* not implemented */
		case ID_TXT:
			if(!test) *newid= (ID*)copy_text((Text*)id);
			return 1;
		case ID_SCRIPT:
			return 0; /* deprecated */
		case ID_SO:
			return 0; /* not implemented */
		case ID_GR:
			if(!test) *newid= (ID*)copy_group((Group*)id);
			return 1;
		case ID_AR:
			if(!test) *newid= (ID*)copy_armature((bArmature*)id);
			return 1;
		case ID_AC:
			if(!test) *newid= (ID*)copy_action((bAction*)id);
			return 1;
		case ID_NT:
			if(!test) *newid= (ID*)ntreeCopyTree((bNodeTree*)id, 0);
			return 1;
		case ID_BR:
			if(!test) *newid= (ID*)copy_brush((Brush*)id);
			return 1;
		case ID_PA:
			if(!test) *newid= (ID*)psys_copy_settings((ParticleSettings*)id);
			return 1;
		case ID_WM:
			return 0; /* can't be copied from here */
		case ID_GD:
			return 0; /* not implemented */
	}
	
	return 0;
}

int id_unlink(ID *id, int test)
{
	Main *mainlib= G.main;
	ListBase *lb;

	switch(GS(id->name)) {
		case ID_TXT:
			if(test) return 1;
			unlink_text(mainlib, (Text*)id);
			break;
		case ID_GR:
			if(test) return 1;
			unlink_group((Group*)id);
			break;
		case ID_OB:
			if(test) return 1;
			unlink_object(NULL, (Object*)id);
			break;
	}

	if(id->us == 0) {
		if(test) return 1;

		lb= wich_libbase(mainlib, GS(id->name));
		free_libblock(lb, id);

		return 1;
	}

	return 0;
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
		case ID_GR:
			return &(mainlib->group);
		case ID_AR:
			return &(mainlib->armature);
		case ID_AC:
			return &(mainlib->action);
		case ID_NT:
			return &(mainlib->nodetree);
		case ID_BR:
			return &(mainlib->brush);
		case ID_PA:
			return &(mainlib->particle);
		case ID_WM:
			return &(mainlib->wm);
		case ID_GD:
			return &(mainlib->gpencil);
	}
	return 0;
}

/* Flag all ids in listbase */
void flag_listbase_ids(ListBase *lb, short flag, short value)
{
	ID *id;
	if (value) {
		for(id= lb->first; id; id= id->next) id->flag |= flag;
	} else {
		flag = ~flag;
		for(id= lb->first; id; id= id->next) id->flag &= flag;
	}
}

/* Flag all ids in listbase */
void flag_all_listbases_ids(short flag, short value)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;
	a= set_listbasepointers(G.main, lbarray);
	while(a--)	flag_listbase_ids(lbarray[a], flag, value);
}

void recalc_all_library_objects(Main *main)
{
	Object *ob;

	/* flag for full recalc */
	for(ob=main->object.first; ob; ob=ob->id.next)
		if(ob->id.lib)
			ob->recalc |= OB_RECALC;
}

/* note: MAX_LIBARRAY define should match this code */
int set_listbasepointers(Main *main, ListBase **lb)
{
	int a = 0;

	/* BACKWARDS! also watch order of free-ing! (mesh<->mat) */

	lb[a++]= &(main->ipo);
	lb[a++]= &(main->action); // xxx moved here to avoid problems when freeing with animato (aligorith)
	lb[a++]= &(main->key);
	lb[a++]= &(main->nodetree);
	lb[a++]= &(main->image);
	lb[a++]= &(main->tex);
	lb[a++]= &(main->mat);
	lb[a++]= &(main->vfont);
	
	/* Important!: When adding a new object type,
	 * the specific data should be inserted here 
	 */

	lb[a++]= &(main->armature);

	lb[a++]= &(main->mesh);
	lb[a++]= &(main->curve);
	lb[a++]= &(main->mball);

	lb[a++]= &(main->wave);
	lb[a++]= &(main->latt);
	lb[a++]= &(main->lamp);
	lb[a++]= &(main->camera);
	
	lb[a++]= &(main->text);
	lb[a++]= &(main->sound);
	lb[a++]= &(main->group);
	lb[a++]= &(main->brush);
	lb[a++]= &(main->script);
	lb[a++]= &(main->particle);
	
	lb[a++]= &(main->world);
	lb[a++]= &(main->screen);
	lb[a++]= &(main->object);
	lb[a++]= &(main->scene);
	lb[a++]= &(main->library);
	lb[a++]= &(main->wm);
	lb[a++]= &(main->gpencil);
	
	lb[a]= NULL;

	return a;
}

/* *********** ALLOC AND FREE *****************
  
free_libblock(ListBase *lb, ID *id )
	provide a list-basis and datablock, but only ID is read

void *alloc_libblock(ListBase *lb, type, name)
	inserts in list and returns a new ID

 ***************************** */

static ID *alloc_libblock_notest(short type)
{
	ID *id= NULL;
	
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
			//XXX id= MEM_callocN(sizeof(Script), "script");
			break;
		case ID_SO:
			id= MEM_callocN(sizeof(bSound), "sound");
			break;
		case ID_GR:
			id= MEM_callocN(sizeof(Group), "group");
			break;
		case ID_AR:
			id = MEM_callocN(sizeof(bArmature), "armature");
			break;
		case ID_AC:
			id = MEM_callocN(sizeof(bAction), "action");
			break;
		case ID_NT:
			id = MEM_callocN(sizeof(bNodeTree), "nodetree");
			break;
		case ID_BR:
			id = MEM_callocN(sizeof(Brush), "brush");
			break;
		case ID_PA:
			id = MEM_callocN(sizeof(ParticleSettings), "ParticleSettings");
			  break;
		case ID_WM:
			id = MEM_callocN(sizeof(wmWindowManager), "Window manager");
			  break;
		case ID_GD:
			id = MEM_callocN(sizeof(bGPdata), "Grease Pencil");
			break;
	}
	return id;
}

/* used everywhere in blenkernel and text.c */
void *alloc_libblock(ListBase *lb, short type, const char *name)
{
	ID *id= NULL;
	
	id= alloc_libblock_notest(type);
	if(id) {
		BLI_addtail(lb, id);
		id->us= 1;
		id->icon_id = 0;
		*( (short *)id->name )= type;
		new_id(lb, id, name);
		/* alphabetic insterion: is in new_id */
	}
	return id;
}

/* by spec, animdata is first item after ID */
/* and, trust that BKE_animdata_from_id() will only find AnimData for valid ID-types */
static void id_copy_animdata(ID *id)
{
	AnimData *adt= BKE_animdata_from_id(id);
	
	if (adt) {
		IdAdtTemplate *iat = (IdAdtTemplate *)id;
		iat->adt= BKE_copy_animdata(iat->adt);
	}
}

/* material nodes use this since they are not treated as libdata */
void copy_libblock_data(ID *id, const ID *id_from)
{
	if (id_from->properties)
		id->properties = IDP_CopyProperty(id_from->properties);

	/* the duplicate should get a copy of the animdata */
	id_copy_animdata(id);
}

/* used everywhere in blenkernel */
void *copy_libblock(void *rt)
{
	ID *idn, *id;
	ListBase *lb;
	char *cp, *cpn;
	int idn_len;
	
	id= rt;

	lb= wich_libbase(G.main, GS(id->name));
	idn= alloc_libblock(lb, GS(id->name), id->name+2);
	
	if(idn==NULL) {
		printf("ERROR: Illegal ID name for %s (Crashing now)\n", id->name);
	}
	
	idn_len= MEM_allocN_len(idn);
	if(idn_len - sizeof(ID) > 0) {
		cp= (char *)id;
		cpn= (char *)idn;
		memcpy(cpn+sizeof(ID), cp+sizeof(ID), idn_len - sizeof(ID));
	}
	
	id->newid= idn;
	idn->flag |= LIB_NEW;

	copy_libblock_data(idn, id);
	
	return idn;
}

static void free_library(Library *lib)
{
	/* no freeing needed for libraries yet */
}

static void (*free_windowmanager_cb)(bContext *, wmWindowManager *)= NULL;

void set_free_windowmanager_cb(void (*func)(bContext *C, wmWindowManager *) )
{
	free_windowmanager_cb= func;
}

void animdata_dtar_clear_cb(ID *id, AnimData *adt, void *userdata)
{
	ChannelDriver *driver;
	FCurve *fcu;

	/* find the driver this belongs to and update it */
	for (fcu=adt->drivers.first; fcu; fcu=fcu->next) {
		driver= fcu->driver;
		
		if (driver) {
			DriverVar *dvar;
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				DRIVER_TARGETS_USED_LOOPER(dvar) 
				{
					if (dtar->id == userdata)
						dtar->id= NULL;
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
	}
}


/* used in headerbuttons.c image.c mesh.c screen.c sound.c and library.c */
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
			//XXX free_script((Script *)id);
			break;
		case ID_SO:
			sound_free((bSound*)id);
			break;
		case ID_GR:
			free_group_objects((Group *)id);
			break;
		case ID_AR:
			free_armature((bArmature *)id);
			break;
		case ID_AC:
			free_action((bAction *)id);
			break;
		case ID_NT:
			ntreeFreeTree((bNodeTree *)id);
			break;
		case ID_BR:
			free_brush((Brush *)id);
			break;
		case ID_PA:
			psys_free_settings((ParticleSettings *)id);
			break;
		case ID_WM:
			if(free_windowmanager_cb)
				free_windowmanager_cb(NULL, (wmWindowManager *)id);
			break;
		case ID_GD:
			free_gpencil_data((bGPdata *)id);
			break;
	}

	if (id->properties) {
		IDP_FreeProperty(id->properties);
		MEM_freeN(id->properties);
	}

	BLI_remlink(lb, id);

	/* this ID may be a driver target! */
	BKE_animdata_main_cb(G.main, animdata_dtar_clear_cb, (void *)id);

	MEM_freeN(id);
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
		if( GS(id->name)==ID_OB ) unlink_object(NULL, (Object *)id);
		
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
		
		while ( (id= lb->first) ) {
			free_libblock(lb, id);
		}
	}

	MEM_freeN(mainvar);
}

/* ***************** ID ************************ */


ID *find_id(char *type, char *name)		/* type: "OB" or "MA" etc */
{
	ListBase *lb= wich_libbase(G.main, GS(type));
	return BLI_findstring(lb, name, offsetof(ID, name) + 2);
}

static void get_flags_for_id(ID *id, char *buf) 
{
	int isfake= id->flag & LIB_FAKEUSER;
	int isnode=0;
		/* Writeout the flags for the entry, note there
		 * is a small hack that writes 5 spaces instead
		 * of 4 if no flags are displayed... this makes
		 * things usually line up ok - better would be
		 * to have that explicit, oh well - zr
		 */

	if(GS(id->name)==ID_MA)
		isnode= ((Material *)id)->use_nodes;
	if(GS(id->name)==ID_TE)
		isnode= ((Tex *)id)->use_nodes;
	
	if (id->us<0)
		sprintf(buf, "-1W ");
	else if (!id->lib && !isfake && id->us && !isnode)
		sprintf(buf, "     ");
	else if(isnode)
		sprintf(buf, "%c%cN%c ", id->lib?'L':' ', isfake?'F':' ', (id->us==0)?'O':' ');
	else
		sprintf(buf, "%c%c%c ", id->lib?'L':' ', isfake?'F':' ', (id->us==0)?'O':' ');
}

#define IDPUP_NO_VIEWER 1

static void IDnames_to_dyn_pupstring(DynStr *pupds, ListBase *lb, ID *link, short *nr, int hideflag)
{
	int i, nids= BLI_countlist(lb);
		
	if (nr) *nr= -1;
	
	if (nr && nids>MAX_IDPUP) {
		BLI_dynstr_append(pupds, "DataBrowse %x-2");
		*nr= -2;
	} else {
		ID *id;
		
		for (i=0, id= lb->first; id; id= id->next, i++) {
			char buf[32];
			
			if (nr && id==link) *nr= i+1;

			if (U.uiflag & USER_HIDE_DOT && id->name[2]=='.')
				continue;
			if (hideflag & IDPUP_NO_VIEWER)
				if (GS(id->name)==ID_IM)
					if ( ((Image *)id)->source==IMA_SRC_VIEWER )
						continue;
			
			get_flags_for_id(id, buf);
				
			BLI_dynstr_append(pupds, buf);
			BLI_dynstr_append(pupds, id->name+2);
			sprintf(buf, "%%x%d", i+1);
			BLI_dynstr_append(pupds, buf);
			
			/* icon */
			switch(GS(id->name))
			{
			case ID_MA: /* fall through */
			case ID_TE: /* fall through */
			case ID_IM: /* fall through */
			case ID_WO: /* fall through */
			case ID_LA: /* fall through */
				sprintf(buf, "%%i%d", BKE_icon_getid(id) );
				BLI_dynstr_append(pupds, buf);
				break;
			default:
				break;
			}
			
			if(id->next)
				BLI_dynstr_append(pupds, "|");
		}
	}
}


/* used by headerbuttons.c buttons.c editobject.c editseq.c */
/* if nr==NULL no MAX_IDPUP, this for non-header browsing */
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

	IDnames_to_dyn_pupstring(pupds, lb, link, nr, 0);
	
	*str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
}

/* skips viewer images */
void IMAnames_to_pupstring(char **str, char *title, char *extraops, ListBase *lb, ID *link, short *nr)
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
	
	IDnames_to_dyn_pupstring(pupds, lb, link, nr, IDPUP_NO_VIEWER);
	
	*str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
}


/* used by buttons.c library.c mball.c */
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
			if(BLI_strcasecmp(idtest->name, id->name)>0 || idtest->lib) {
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

/*
 * Check to see if there is an ID with the same name as 'name'.
 * Returns the ID if so, if not, returns NULL
 */
static ID *is_dupid(ListBase *lb, ID *id, char *name)
{
	ID *idtest=NULL;
	
	for( idtest = lb->first; idtest; idtest = idtest->next ) {
		/* if idtest is not a lib */ 
		if( id != idtest && idtest->lib == NULL ) {
			/* do not test alphabetic! */
			/* optimized */
			if( idtest->name[2] == name[0] ) {
				if(strcmp(name, idtest->name+2)==0) break;
			}
		}
	}
	
	return idtest;
}

/* 
 * Check to see if an ID name is already used, and find a new one if so.
 * Return 1 if created a new name (returned in name).
 *
 * Normally the ID that's being check is already in the ListBase, so ID *id
 * points at the new entry.  The Python Library module needs to know what
 * the name of a datablock will be before it is appended; in this case ID *id
 * id is NULL;
 */

static int check_for_dupid(ListBase *lb, ID *id, char *name)
{
	ID *idtest;
	int nr= 0, nrtest, a;
	const int maxtest=32;
	char left[32], leftest[32], in_use[32];

	/* make sure input name is terminated properly */
	/* if( strlen(name) > 21 ) name[21]= 0; */
	/* removed since this is only ever called from one place - campbell */

	while (1) {

		/* phase 1: id already exists? */
		idtest = is_dupid(lb, id, name);

		/* if there is no double, done */
		if( idtest == NULL ) return 0;

		/* we have a dup; need to make a new name */
		/* quick check so we can reuse one of first 32 ids if vacant */
		memset(in_use, 0, maxtest);

		/* get name portion, number portion ("name.number") */
		splitIDname( name, left, &nr);

		/* if new name will be too long, truncate it */
		if(nr>999 && strlen(left)>16) left[16]= 0;
		else if(strlen(left)>17) left[17]= 0;

		for( idtest = lb->first; idtest; idtest = idtest->next ) {
			if( id != idtest && idtest->lib == NULL ) {
				splitIDname(idtest->name+2, leftest, &nrtest);
				/* if base names match... */
				/* optimized */
				if( *left == *leftest && strcmp(left, leftest)==0 ) {
					if(nrtest < maxtest)
						in_use[nrtest]= 1;	/* mark as used */
					if(nr <= nrtest)
						nr= nrtest+1;		/* track largest unused */
				}
			}
		}

		/* decide which value of nr to use */
		for(a=0; a<maxtest; a++) {
			if(a>=nr) break;	/* stop when we've check up to biggest */
			if( in_use[a]==0 ) { /* found an unused value */
				nr = a;
				break;
			}
		}

		/* If the original name has no numeric suffix, 
		 * rather than just chopping and adding numbers, 
		 * shave off the end chars until we have a unique name */
		if (nr==0) {
			int len = strlen(name)-1;
			idtest= is_dupid(lb, id, name);
			
			while (idtest && len> 1) {
				name[len--] = '\0';
				idtest= is_dupid(lb, id, name);
			}
			if (idtest == NULL) return 1;
			/* otherwise just continue and use a number suffix */
		}
		
		if(nr > 999 && strlen(left) > 16) {
			/* this would overflow name buffer */
			left[16] = 0;
			strcpy( name, left );
			continue;
		}
		/* this format specifier is from hell... */
		sprintf(name, "%s.%.3d", left, nr);

		return 1;
	}
}

/*
 * Only for local blocks: external en indirect blocks already have a
 * unique ID.
 *
 * return 1: created a new name
 */

int new_id(ListBase *lb, ID *id, const char *tname)
{
	int result;
	char name[22];

	/* if library, don't rename */
	if(id->lib) return 0;

	/* if no libdata given, look up based on ID */
	if(lb==NULL) lb= wich_libbase(G.main, GS(id->name));

	/* if no name given, use name of current ID
	 * else make a copy (tname args can be const) */
	if(tname==NULL)
		tname= id->name+2;

	strncpy(name, tname, sizeof(name)-1);

	/* if result > 21, strncpy don't put the final '\0' to name.
	 * easier to assign each time then to check if its needed */
	name[sizeof(name)-1]= 0;

	if(name[0] == '\0')
		strcpy(name, ID_FALLBACK_NAME);

	result = check_for_dupid(lb, id, name);
	strcpy(id->name+2, name);

	/* This was in 2.43 and previous releases
	 * however all data in blender should be sorted, not just duplicate names
	 * sorting should not hurt, but noting just incause it alters the way other
	 * functions work, so sort every time */
	/* if( result )
		sort_alpha_id(lb, id);*/
	
	sort_alpha_id(lb, id);
	
	return result;
}

/* next to indirect usage in read/writefile also in editobject.c scene.c */
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

/* only for library fixes */
static void image_fix_relative_path(Image *ima)
{
	if(ima->id.lib==NULL) return;
	if(strncmp(ima->name, "//", 2)==0) {
		BLI_path_abs(ima->name, ima->id.lib->filename);
		BLI_path_rel(ima->name, G.sce);
	}
}

#define LIBTAG(a)	if(a && a->id.lib) {a->id.flag &=~LIB_INDIRECT; a->id.flag |= LIB_EXTERN;}

static void lib_indirect_test_id(ID *id)
{
	
	if(id->lib)
		return;
	
	if(GS(id->name)==ID_OB) {		
		Object *ob= (Object *)id;
		bActionStrip *strip;
		Mesh *me;

		int a;
	
		// XXX old animation system! --------------------------------------
		for (strip=ob->nlastrips.first; strip; strip=strip->next){
			LIBTAG(strip->object); 
			LIBTAG(strip->act);
			LIBTAG(strip->ipo);
		}
		// XXX: new animation system needs something like this?
	
		for(a=0; a<ob->totcol; a++) {
			LIBTAG(ob->mat[a]);
		}
	
		LIBTAG(ob->dup_group);
		LIBTAG(ob->proxy);
		
		me= ob->data;
		LIBTAG(me);
	}
}

void tag_main(struct Main *mainvar, int tag)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id;
	int a;

	a= set_listbasepointers(mainvar, lbarray);
	while(a--) {
		for(id= lbarray[a]->first; id; id= id->next) {
			if(tag)	id->flag |= LIB_DOIT;
			else	id->flag &= ~LIB_DOIT;
		}
	}
}

/* if lib!=NULL, only all from lib local */
void all_local(Library *lib, int untagged_only)
{
	ListBase *lbarray[MAX_LIBARRAY], tempbase={0, 0};
	ID *id, *idn;
	int a;

	a= set_listbasepointers(G.main, lbarray);
	while(a--) {
		id= lbarray[a]->first;
		
		while(id) {
			id->newid= NULL;
			idn= id->next;		/* id is possibly being inserted again */
			
			/* The check on the second line (LIB_PRE_EXISTING) is done so its
			 * possible to tag data you dont want to be made local, used for
			 * appending data, so any libdata alredy linked wont become local
			 * (very nasty to discover all your links are lost after appending)  
			 * */
			if(id->flag & (LIB_EXTERN|LIB_INDIRECT|LIB_NEW) &&
			  (untagged_only==0 || !(id->flag & LIB_PRE_EXISTING)))
			{
				if(lib==NULL || id->lib==lib) {
					id->flag &= ~(LIB_EXTERN|LIB_INDIRECT|LIB_NEW);

					if(id->lib) {
						/* relative file patch */
						if(GS(id->name)==ID_IM)
							image_fix_relative_path((Image *)id);
						
						id->lib= NULL;
						new_id(lbarray[a], id, 0);	/* new_id only does it with double names */
						sort_alpha_id(lbarray[a], id);
					}
				}
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

	/* patch 3: make sure library data isn't indirect falsely... */
	a= set_listbasepointers(G.main, lbarray);
	while(a--) {
		for(id= lbarray[a]->first; id; id=id->next)
			lib_indirect_test_id(id);
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
	idtest= BLI_findstring(lb, name, offsetof(ID, name) + 2);

	if(idtest) if( new_id(lb, idtest, name)==0 ) sort_alpha_id(lb, idtest);
}

void text_idbutton(struct ID *id, char *text)
{
	if(id) {
		if(GS(id->name)==ID_SCE)
			strcpy(text, "SCE: ");
		else if(GS(id->name)==ID_SCE)
			strcpy(text, "SCR: ");
		else if(GS(id->name)==ID_MA && ((Material*)id)->use_nodes)
			strcpy(text, "NT: ");
		else {
			text[0]= id->name[0];
			text[1]= id->name[1];
			text[2]= ':';
			text[3]= ' ';
			text[4]= 0;
		}
	}
	else
		strcpy(text, "");
}

void rename_id(ID *id, char *name)
{
	ListBase *lb;

	strncpy(id->name+2, name, 21);
	lb= wich_libbase(G.main, GS(id->name) );
	
	new_id(lb, id, name);				
}

