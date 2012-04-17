/*
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
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenloader/intern/writefile.c
 *  \ingroup blenloader
 */


/*
FILEFORMAT: IFF-style structure  (but not IFF compatible!)

start file:
	BLENDER_V100	12 bytes  (versie 1.00)
					V = big endian, v = little endian
					_ = 4 byte pointer, - = 8 byte pointer

datablocks:		also see struct BHead
	<bh.code>			4 chars
	<bh.len>			int,  len data after BHead
	<bh.old>			void,  old pointer
	<bh.SDNAnr>			int
	<bh.nr>				int, in case of array: amount of structs
	data
	...
	...

Almost all data in Blender are structures. Each struct saved
gets a BHead header.  With BHead the struct can be linked again
and compared with StructDNA .

WRITE

Preferred writing order: (not really a must, but why would you do it random?)
Any case: direct data is ALWAYS after the lib block

(Local file data)
- for each LibBlock
	- write LibBlock
	- write associated direct data
(External file data)
- per library
	- write library block
	- per LibBlock
		- write the ID of LibBlock
- write TEST (128x128, blend file preview, optional)
- write FileGlobal (some global vars)
- write SDNA
- write USER if filename is ~/X.XX/config/startup.blend
*/


#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zlib.h"

#ifndef WIN32
#  include <unistd.h>
#else
#  include "winsock2.h"
#  include <io.h>
#  include <process.h> // for getpid
#  include "BLI_winstuff.h"
#endif

/* allow writefile to use deprecated functionality (for forward compatibility code) */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_actuator_types.h"
#include "DNA_brush_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_controller_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_genfile.h"
#include "DNA_group_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_lamp_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_packedFile_types.h"
#include "DNA_particle_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_sdna_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sensor_types.h"
#include "DNA_smoke_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_speaker_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_movieclip_types.h"

#include "MEM_guardedalloc.h" // MEM_freeN
#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_bpath.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_constraint.h"
#include "BKE_global.h" // for G
#include "BKE_library.h" // for  set_listbasepointers
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"
#include "BKE_utildefines.h"
#include "BKE_modifier.h"
#include "BKE_fcurve.h"
#include "BKE_pointcache.h"
#include "BKE_mesh.h"

#include "BLO_writefile.h"
#include "BLO_readfile.h"
#include "BLO_undofile.h"

#include "readfile.h"

#include <errno.h>

/* ********* my write, buffered writing with minimum size chunks ************ */

#define MYWRITE_BUFFER_SIZE	100000
#define MYWRITE_MAX_CHUNK	32768

typedef struct {
	struct SDNA *sdna;

	int file;
	unsigned char *buf;
	MemFile *compare, *current;
	
	int tot, count, error, memsize;

#ifdef USE_BMESH_SAVE_AS_COMPAT
	char use_mesh_compat; /* option to save with older mesh format */
#endif
} WriteData;

static WriteData *writedata_new(int file)
{
	WriteData *wd= MEM_callocN(sizeof(*wd), "writedata");

		/* XXX, see note about this in readfile.c, remove
		 * once we have an xp lock - zr
		 */

	if (wd == NULL) return NULL;

	wd->sdna= DNA_sdna_from_data(DNAstr, DNAlen, 0);

	wd->file= file;

	wd->buf= MEM_mallocN(MYWRITE_BUFFER_SIZE, "wd->buf");

	return wd;
}

static void writedata_do_write(WriteData *wd, const void *mem, int memlen)
{
	if ((wd == NULL) || wd->error || (mem == NULL) || memlen < 1) return;
	if (wd->error) return;

	/* memory based save */
	if (wd->current) {
		add_memfilechunk(NULL, wd->current, mem, memlen);
	}
	else {
		if (write(wd->file, mem, memlen) != memlen)
			wd->error= 1;
		
	}
}

static void writedata_free(WriteData *wd)
{
	DNA_sdna_free(wd->sdna);

	MEM_freeN(wd->buf);
	MEM_freeN(wd);
}

/***/

/**
 * Low level WRITE(2) wrapper that buffers data
 * \param adr Pointer to new chunk of data
 * \param len Length of new chunk of data
 * \warning Talks to other functions with global parameters
 */
 
#define MYWRITE_FLUSH		NULL

static void mywrite( WriteData *wd, const void *adr, int len)
{
	if (wd->error) return;

	/* flush helps compression for undo-save */
	if (adr==MYWRITE_FLUSH) {
		if (wd->count) {
			writedata_do_write(wd, wd->buf, wd->count);
			wd->count= 0;
		}
		return;
	}

	wd->tot+= len;
	
	/* if we have a single big chunk, write existing data in
	 * buffer and write out big chunk in smaller pieces */
	if (len>MYWRITE_MAX_CHUNK) {
		if (wd->count) {
			writedata_do_write(wd, wd->buf, wd->count);
			wd->count= 0;
		}

		do {
			int writelen= MIN2(len, MYWRITE_MAX_CHUNK);
			writedata_do_write(wd, adr, writelen);
			adr = (const char *)adr + writelen;
			len -= writelen;
		} while (len > 0);

		return;
	}

	/* if data would overflow buffer, write out the buffer */
	if (len+wd->count>MYWRITE_BUFFER_SIZE-1) {
		writedata_do_write(wd, wd->buf, wd->count);
		wd->count= 0;
	}

	/* append data at end of buffer */
	memcpy(&wd->buf[wd->count], adr, len);
	wd->count+= len;
}

/**
 * BeGiN initializer for mywrite
 * \param file File descriptor
 * \param compare Previous memory file (can be NULL).
 * \param current The current memory file (can be NULL).
 * \warning Talks to other functions with global parameters
 */
static WriteData *bgnwrite(int file, MemFile *compare, MemFile *current)
{
	WriteData *wd= writedata_new(file);

	if (wd == NULL) return NULL;

	wd->compare= compare;
	wd->current= current;
	/* this inits comparing */
	add_memfilechunk(compare, NULL, NULL, 0);
	
	return wd;
}

/**
 * END the mywrite wrapper
 * \return 1 if write failed
 * \return unknown global variable otherwise
 * \warning Talks to other functions with global parameters
 */
static int endwrite(WriteData *wd)
{
	int err;

	if (wd->count) {
		writedata_do_write(wd, wd->buf, wd->count);
		wd->count= 0;
	}
	
	err= wd->error;
	writedata_free(wd);

	return err;
}

/* ********** WRITE FILE ****************** */

static void writestruct(WriteData *wd, int filecode, const char *structname, int nr, void *adr)
{
	BHead bh;
	short *sp;

	if (adr==NULL || nr==0) return;

	/* init BHead */
	bh.code= filecode;
	bh.old= adr;
	bh.nr= nr;

	bh.SDNAnr= DNA_struct_find_nr(wd->sdna, structname);
	if (bh.SDNAnr== -1) {
		printf("error: can't find SDNA code <%s>\n", structname);
		return;
	}
	sp= wd->sdna->structs[bh.SDNAnr];

	bh.len= nr*wd->sdna->typelens[sp[0]];

	if (bh.len==0) return;

	mywrite(wd, &bh, sizeof(BHead));
	mywrite(wd, adr, bh.len);
}

static void writedata(WriteData *wd, int filecode, int len, const void *adr)  /* do not use for structs */
{
	BHead bh;

	if (adr==NULL) return;
	if (len==0) return;

	len += 3;
	len -= (len % 4);

	/* init BHead */
	bh.code   = filecode;
	bh.old    = (void *)adr;  /* this is safe to cast from const */
	bh.nr     = 1;
	bh.SDNAnr = 0;
	bh.len    = len;

	mywrite(wd, &bh, sizeof(BHead));
	if (len) mywrite(wd, adr, len);
}

/* *************** writing some direct data structs used in more code parts **************** */
/*These functions are used by blender's .blend system for file saving/loading.*/
void IDP_WriteProperty_OnlyData(IDProperty *prop, void *wd);
void IDP_WriteProperty(IDProperty *prop, void *wd);

static void IDP_WriteArray(IDProperty *prop, void *wd)
{
	/*REMEMBER to set totalen to len in the linking code!!*/
	if (prop->data.pointer) {
		writedata(wd, DATA, MEM_allocN_len(prop->data.pointer), prop->data.pointer);

		if (prop->subtype == IDP_GROUP) {
			IDProperty **array= prop->data.pointer;
			int a;

			for (a=0; a<prop->len; a++)
				IDP_WriteProperty(array[a], wd);
		}
	}
}

static void IDP_WriteIDPArray(IDProperty *prop, void *wd)
{
	/*REMEMBER to set totalen to len in the linking code!!*/
	if (prop->data.pointer) {
		IDProperty *array = prop->data.pointer;
		int a;

		writestruct(wd, DATA, "IDProperty", prop->len, array);

		for (a=0; a<prop->len; a++)
			IDP_WriteProperty_OnlyData(&array[a], wd);
	}
}

static void IDP_WriteString(IDProperty *prop, void *wd)
{
	/*REMEMBER to set totalen to len in the linking code!!*/
	writedata(wd, DATA, prop->len+1, prop->data.pointer);
}

static void IDP_WriteGroup(IDProperty *prop, void *wd)
{
	IDProperty *loop;

	for (loop=prop->data.group.first; loop; loop=loop->next) {
		IDP_WriteProperty(loop, wd);
	}
}

/* Functions to read/write ID Properties */
void IDP_WriteProperty_OnlyData(IDProperty *prop, void *wd)
{
	switch (prop->type) {
		case IDP_GROUP:
			IDP_WriteGroup(prop, wd);
			break;
		case IDP_STRING:
			IDP_WriteString(prop, wd);
			break;
		case IDP_ARRAY:
			IDP_WriteArray(prop, wd);
			break;
		case IDP_IDPARRAY:
			IDP_WriteIDPArray(prop, wd);
			break;
	}
}

void IDP_WriteProperty(IDProperty *prop, void *wd)
{
	writestruct(wd, DATA, "IDProperty", 1, prop);
	IDP_WriteProperty_OnlyData(prop, wd);
}

static void write_fmodifiers(WriteData *wd, ListBase *fmodifiers)
{
	FModifier *fcm;
	
	/* Modifiers */
	for (fcm= fmodifiers->first; fcm; fcm= fcm->next) {
		FModifierTypeInfo *fmi= fmodifier_get_typeinfo(fcm);
		
		/* Write the specific data */
		if (fmi && fcm->data) {
			/* firstly, just write the plain fmi->data struct */
			writestruct(wd, DATA, fmi->structName, 1, fcm->data);
			
			/* do any modifier specific stuff */
			switch (fcm->type) {
				case FMODIFIER_TYPE_GENERATOR:
				{
					FMod_Generator *data= (FMod_Generator *)fcm->data;
					
					/* write coefficients array */
					if (data->coefficients)
						writedata(wd, DATA, sizeof(float)*(data->arraysize), data->coefficients);
				}
					break;
				case FMODIFIER_TYPE_ENVELOPE:
				{
					FMod_Envelope *data= (FMod_Envelope *)fcm->data;
					
					/* write envelope data */
					if (data->data)
						writestruct(wd, DATA, "FCM_EnvelopeData", data->totvert, data->data);
				}
					break;
				case FMODIFIER_TYPE_PYTHON:
				{
					FMod_Python *data = (FMod_Python *)fcm->data;
					
					/* Write ID Properties -- and copy this comment EXACTLY for easy finding
					 of library blocks that implement this.*/
					IDP_WriteProperty(data->prop, wd);
				}
					break;
			}
		}
		
		/* Write the modifier */
		writestruct(wd, DATA, "FModifier", 1, fcm);
	}
}

static void write_fcurves(WriteData *wd, ListBase *fcurves)
{
	FCurve *fcu;
	
	for (fcu=fcurves->first; fcu; fcu=fcu->next) {
		/* F-Curve */
		writestruct(wd, DATA, "FCurve", 1, fcu);
		
		/* curve data */
		if (fcu->bezt)  	
			writestruct(wd, DATA, "BezTriple", fcu->totvert, fcu->bezt);
		if (fcu->fpt)
			writestruct(wd, DATA, "FPoint", fcu->totvert, fcu->fpt);
			
		if (fcu->rna_path)
			writedata(wd, DATA, strlen(fcu->rna_path)+1, fcu->rna_path);
		
		/* driver data */
		if (fcu->driver) {
			ChannelDriver *driver= fcu->driver;
			DriverVar *dvar;
			
			writestruct(wd, DATA, "ChannelDriver", 1, driver);
			
			/* variables */
			for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
				writestruct(wd, DATA, "DriverVar", 1, dvar);
				
				DRIVER_TARGETS_USED_LOOPER(dvar)
				{
					if (dtar->rna_path)
						writedata(wd, DATA, strlen(dtar->rna_path)+1, dtar->rna_path);
				}
				DRIVER_TARGETS_LOOPER_END
			}
		}
		
		/* write F-Modifiers */
		write_fmodifiers(wd, &fcu->modifiers);
	}
}

static void write_actions(WriteData *wd, ListBase *idbase)
{
	bAction	*act;
	bActionGroup *grp;
	TimeMarker *marker;
	
	for (act=idbase->first; act; act= act->id.next) {
		if (act->id.us>0 || wd->current) {
			writestruct(wd, ID_AC, "bAction", 1, act);
			if (act->id.properties) IDP_WriteProperty(act->id.properties, wd);
			
			write_fcurves(wd, &act->curves);
			
			for (grp=act->groups.first; grp; grp=grp->next) {
				writestruct(wd, DATA, "bActionGroup", 1, grp);
			}
			
			for (marker=act->markers.first; marker; marker=marker->next) {
				writestruct(wd, DATA, "TimeMarker", 1, marker);
			}
		}
	}
	
	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_keyingsets(WriteData *wd, ListBase *list)
{
	KeyingSet *ks;
	KS_Path *ksp;
	
	for (ks= list->first; ks; ks= ks->next) {
		/* KeyingSet */
		writestruct(wd, DATA, "KeyingSet", 1, ks);
		
		/* Paths */
		for (ksp= ks->paths.first; ksp; ksp= ksp->next) {
			/* Path */
			writestruct(wd, DATA, "KS_Path", 1, ksp);
			
			if (ksp->rna_path)
				writedata(wd, DATA, strlen(ksp->rna_path)+1, ksp->rna_path);
		}
	}
}

static void write_nlastrips(WriteData *wd, ListBase *strips)
{
	NlaStrip *strip;
	
	for (strip= strips->first; strip; strip= strip->next) {
		/* write the strip first */
		writestruct(wd, DATA, "NlaStrip", 1, strip);
		
		/* write the strip's F-Curves and modifiers */
		write_fcurves(wd, &strip->fcurves);
		write_fmodifiers(wd, &strip->modifiers);
		
		/* write the strip's children */
		write_nlastrips(wd, &strip->strips);
	}
}

static void write_nladata(WriteData *wd, ListBase *nlabase)
{
	NlaTrack *nlt;
	
	/* write all the tracks */
	for (nlt= nlabase->first; nlt; nlt= nlt->next) {
		/* write the track first */
		writestruct(wd, DATA, "NlaTrack", 1, nlt);
		
		/* write the track's strips */
		write_nlastrips(wd, &nlt->strips);
	}
}

static void write_animdata(WriteData *wd, AnimData *adt)
{
	AnimOverride *aor;
	
	/* firstly, just write the AnimData block */
	writestruct(wd, DATA, "AnimData", 1, adt);
	
	/* write drivers */
	write_fcurves(wd, &adt->drivers);
	
	/* write overrides */
	// FIXME: are these needed?
	for (aor= adt->overrides.first; aor; aor= aor->next) {
		/* overrides consist of base data + rna_path */
		writestruct(wd, DATA, "AnimOverride", 1, aor);
		writedata(wd, DATA, strlen(aor->rna_path)+1, aor->rna_path);
	}
	
	// TODO write the remaps (if they are needed)
	
	/* write NLA data */
	write_nladata(wd, &adt->nla_tracks);
}

static void write_curvemapping(WriteData *wd, CurveMapping *cumap)
{
	int a;
	
	writestruct(wd, DATA, "CurveMapping", 1, cumap);
	for (a=0; a<CM_TOT; a++)
		writestruct(wd, DATA, "CurveMapPoint", cumap->cm[a].totpoint, cumap->cm[a].curve);
}

static void write_node_socket(WriteData *wd, bNodeSocket *sock)
{
	bNodeSocketType *stype= ntreeGetSocketType(sock->type);

	/* forward compatibility code, so older blenders still open */
	sock->stack_type = 1;
	
	if (sock->default_value) {
		bNodeSocketValueFloat *valfloat;
		bNodeSocketValueVector *valvector;
		bNodeSocketValueRGBA *valrgba;
		
		switch (sock->type) {
		case SOCK_FLOAT:
			valfloat = sock->default_value;
			sock->ns.vec[0] = valfloat->value;
			sock->ns.min = valfloat->min;
			sock->ns.max = valfloat->max;
			break;
		case SOCK_VECTOR:
			valvector = sock->default_value;
			copy_v3_v3(sock->ns.vec, valvector->value);
			sock->ns.min = valvector->min;
			sock->ns.max = valvector->max;
			break;
		case SOCK_RGBA:
			valrgba = sock->default_value;
			copy_v4_v4(sock->ns.vec, valrgba->value);
			sock->ns.min = 0.0f;
			sock->ns.max = 1.0f;
			break;
		}
	}

	/* actual socket writing */
	writestruct(wd, DATA, "bNodeSocket", 1, sock);
	if (sock->default_value)
		writestruct(wd, DATA, stype->value_structname, 1, sock->default_value);
}

/* this is only direct data, tree itself should have been written */
static void write_nodetree(WriteData *wd, bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	
	/* for link_list() speed, we write per list */
	
	if (ntree->adt) write_animdata(wd, ntree->adt);
	
	for (node= ntree->nodes.first; node; node= node->next)
		writestruct(wd, DATA, "bNode", 1, node);

	for (node= ntree->nodes.first; node; node= node->next) {
		for (sock= node->inputs.first; sock; sock= sock->next)
			write_node_socket(wd, sock);
		for (sock= node->outputs.first; sock; sock= sock->next)
			write_node_socket(wd, sock);

		
		if (node->storage && node->type!=NODE_DYNAMIC) {
			/* could be handlerized at some point, now only 1 exception still */
			if (ntree->type==NTREE_SHADER && (node->type==SH_NODE_CURVE_VEC || node->type==SH_NODE_CURVE_RGB))
				write_curvemapping(wd, node->storage);
			else if (ntree->type==NTREE_COMPOSIT && ELEM4(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB, CMP_NODE_HUECORRECT))
				write_curvemapping(wd, node->storage);
			else if (ntree->type==NTREE_TEXTURE && (node->type==TEX_NODE_CURVE_RGB || node->type==TEX_NODE_CURVE_TIME) )
				write_curvemapping(wd, node->storage);
			else if (ntree->type==NTREE_COMPOSIT && node->type==CMP_NODE_MOVIEDISTORTION)
				/* pass */;
			else
				writestruct(wd, DATA, node->typeinfo->storagename, 1, node->storage);
		}
		
		if (node->type==CMP_NODE_OUTPUT_FILE) {
			/* inputs have own storage data */
			for (sock=node->inputs.first; sock; sock=sock->next)
				writestruct(wd, DATA, "NodeImageMultiFileSocket", 1, sock->storage);
		}
	}
	
	for (link= ntree->links.first; link; link= link->next)
		writestruct(wd, DATA, "bNodeLink", 1, link);
	
	/* external sockets */
	for (sock= ntree->inputs.first; sock; sock= sock->next)
		write_node_socket(wd, sock);
	for (sock= ntree->outputs.first; sock; sock= sock->next)
		write_node_socket(wd, sock);
}

static void current_screen_compat(Main *mainvar, bScreen **screen)
{
	wmWindowManager *wm;
	wmWindow *window;

	/* find a global current screen in the first open window, to have
	 * a reasonable default for reading in older versions */
	wm= mainvar->wm.first;
	window= (wm)? wm->windows.first: NULL;
	*screen= (window)? window->screen: NULL;
}

static void write_renderinfo(WriteData *wd, Main *mainvar)		/* for renderdeamon */
{
	bScreen *curscreen;
	Scene *sce;
	int data[8];

	/* XXX in future, handle multiple windows with multiple screnes? */
	current_screen_compat(mainvar, &curscreen);

	for (sce= mainvar->scene.first; sce; sce= sce->id.next) {
		if (sce->id.lib==NULL  && ( sce==curscreen->scene || (sce->r.scemode & R_BG_RENDER)) ) {
			data[0]= sce->r.sfra;
			data[1]= sce->r.efra;

			memset(data+2, 0, sizeof(int)*6);
			BLI_strncpy((char *)(data+2), sce->id.name+2, sizeof(sce->id.name)-2);

			writedata(wd, REND, 32, data);
		}
	}
}

static void write_keymapitem(WriteData *wd, wmKeyMapItem *kmi)
{
	writestruct(wd, DATA, "wmKeyMapItem", 1, kmi);
	if (kmi->properties)
		IDP_WriteProperty(kmi->properties, wd);
}

static void write_userdef(WriteData *wd)
{
	bTheme *btheme;
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;
	wmKeyMapDiffItem *kmdi;
	bAddon *bext;
	uiStyle *style;
	
	writestruct(wd, USER, "UserDef", 1, &U);

	for (btheme= U.themes.first; btheme; btheme=btheme->next)
		writestruct(wd, DATA, "bTheme", 1, btheme);

	for (keymap= U.user_keymaps.first; keymap; keymap=keymap->next) {
		writestruct(wd, DATA, "wmKeyMap", 1, keymap);

		for (kmdi=keymap->diff_items.first; kmdi; kmdi=kmdi->next) {
			writestruct(wd, DATA, "wmKeyMapDiffItem", 1, kmdi);
			if (kmdi->remove_item)
				write_keymapitem(wd, kmdi->remove_item);
			if (kmdi->add_item)
				write_keymapitem(wd, kmdi->add_item);
		}

		for (kmi=keymap->items.first; kmi; kmi=kmi->next)
			write_keymapitem(wd, kmi);
	}

	for (bext= U.addons.first; bext; bext=bext->next)
		writestruct(wd, DATA, "bAddon", 1, bext);
	
	for (style= U.uistyles.first; style; style= style->next) {
		writestruct(wd, DATA, "uiStyle", 1, style);
	}
}

static void write_boid_state(WriteData *wd, BoidState *state)
{
	BoidRule *rule = state->rules.first;
	//BoidCondition *cond = state->conditions.first;

	writestruct(wd, DATA, "BoidState", 1, state);

	for (; rule; rule=rule->next) {
		switch(rule->type) {
			case eBoidRuleType_Goal:
			case eBoidRuleType_Avoid:
				writestruct(wd, DATA, "BoidRuleGoalAvoid", 1, rule);
				break;
			case eBoidRuleType_AvoidCollision:
				writestruct(wd, DATA, "BoidRuleAvoidCollision", 1, rule);
				break;
			case eBoidRuleType_FollowLeader:
				writestruct(wd, DATA, "BoidRuleFollowLeader", 1, rule);
				break;
			case eBoidRuleType_AverageSpeed:
				writestruct(wd, DATA, "BoidRuleAverageSpeed", 1, rule);
				break;
			case eBoidRuleType_Fight:
				writestruct(wd, DATA, "BoidRuleFight", 1, rule);
				break;
			default:
				writestruct(wd, DATA, "BoidRule", 1, rule);
				break;
		}
	}
	//for (; cond; cond=cond->next)
	//	writestruct(wd, DATA, "BoidCondition", 1, cond);
}

/* update this also to readfile.c */
static const char *ptcache_data_struct[] = {
	"", // BPHYS_DATA_INDEX
	"", // BPHYS_DATA_LOCATION
	"", // BPHYS_DATA_VELOCITY
	"", // BPHYS_DATA_ROTATION
	"", // BPHYS_DATA_AVELOCITY / BPHYS_DATA_XCONST */
	"", // BPHYS_DATA_SIZE:
	"", // BPHYS_DATA_TIMES:	
	"BoidData" // case BPHYS_DATA_BOIDS:
};
static const char *ptcache_extra_struct[] = {
	"",
	"ParticleSpring"
};
static void write_pointcaches(WriteData *wd, ListBase *ptcaches)
{
	PointCache *cache = ptcaches->first;
	int i;

	for (; cache; cache=cache->next) {
		writestruct(wd, DATA, "PointCache", 1, cache);

		if ((cache->flag & PTCACHE_DISK_CACHE)==0) {
			PTCacheMem *pm = cache->mem_cache.first;

			for (; pm; pm=pm->next) {
				PTCacheExtra *extra = pm->extradata.first;

				writestruct(wd, DATA, "PTCacheMem", 1, pm);
				
				for (i=0; i<BPHYS_TOT_DATA; i++) {
					if (pm->data[i] && pm->data_types & (1<<i)) {
						if (ptcache_data_struct[i][0]=='\0')
							writedata(wd, DATA, MEM_allocN_len(pm->data[i]), pm->data[i]);
						else
							writestruct(wd, DATA, ptcache_data_struct[i], pm->totpoint, pm->data[i]);
					}
				}

				for (; extra; extra=extra->next) {
					if (ptcache_extra_struct[extra->type][0]=='\0')
						continue;
					writestruct(wd, DATA, "PTCacheExtra", 1, extra);
					writestruct(wd, DATA, ptcache_extra_struct[extra->type], extra->totdata, extra->data);
				}
			}
		}
	}
}
static void write_particlesettings(WriteData *wd, ListBase *idbase)
{
	ParticleSettings *part;
	ParticleDupliWeight *dw;
	GroupObject *go;
	int a;

	part= idbase->first;
	while (part) {
		if (part->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_PA, "ParticleSettings", 1, part);
			if (part->id.properties) IDP_WriteProperty(part->id.properties, wd);
			if (part->adt) write_animdata(wd, part->adt);
			writestruct(wd, DATA, "PartDeflect", 1, part->pd);
			writestruct(wd, DATA, "PartDeflect", 1, part->pd2);
			writestruct(wd, DATA, "EffectorWeights", 1, part->effector_weights);

			dw = part->dupliweights.first;
			for (; dw; dw=dw->next) {
				/* update indices */
				dw->index = 0;
				if (part->dup_group) { /* can be NULL if lining fails or set to None */
					go = part->dup_group->gobject.first;
					while (go && go->ob != dw->ob) {
						go=go->next;
						dw->index++;
					}
				}
				writestruct(wd, DATA, "ParticleDupliWeight", 1, dw);
			}

			if (part->boids && part->phystype == PART_PHYS_BOIDS) {
				BoidState *state = part->boids->states.first;

				writestruct(wd, DATA, "BoidSettings", 1, part->boids);

				for (; state; state=state->next)
					write_boid_state(wd, state);
			}
			if (part->fluid && part->phystype == PART_PHYS_FLUID) {
				writestruct(wd, DATA, "SPHFluidSettings", 1, part->fluid); 
			}

			for (a=0; a<MAX_MTEX; a++) {
				if (part->mtex[a]) writestruct(wd, DATA, "MTex", 1, part->mtex[a]);
			}
		}
		part= part->id.next;
	}
}
static void write_particlesystems(WriteData *wd, ListBase *particles)
{
	ParticleSystem *psys= particles->first;
	ParticleTarget *pt;
	int a;

	for (; psys; psys=psys->next) {
		writestruct(wd, DATA, "ParticleSystem", 1, psys);

		if (psys->particles) {
			writestruct(wd, DATA, "ParticleData", psys->totpart ,psys->particles);

			if (psys->particles->hair) {
				ParticleData *pa = psys->particles;

				for (a=0; a<psys->totpart; a++, pa++)
					writestruct(wd, DATA, "HairKey", pa->totkey, pa->hair);
			}

			if (psys->particles->boid && psys->part->phystype == PART_PHYS_BOIDS)
				writestruct(wd, DATA, "BoidParticle", psys->totpart, psys->particles->boid);

			if (psys->part->fluid && psys->part->phystype == PART_PHYS_FLUID && (psys->part->fluid->flag & SPH_VISCOELASTIC_SPRINGS))
				writestruct(wd, DATA, "ParticleSpring", psys->tot_fluidsprings, psys->fluid_springs);
		}
		pt = psys->targets.first;
		for (; pt; pt=pt->next)
			writestruct(wd, DATA, "ParticleTarget", 1, pt);

		if (psys->child) writestruct(wd, DATA, "ChildParticle", psys->totchild ,psys->child);

		if (psys->clmd) {
			writestruct(wd, DATA, "ClothModifierData", 1, psys->clmd);
			writestruct(wd, DATA, "ClothSimSettings", 1, psys->clmd->sim_parms);
			writestruct(wd, DATA, "ClothCollSettings", 1, psys->clmd->coll_parms);
		}

		write_pointcaches(wd, &psys->ptcaches);
	}
}

static void write_properties(WriteData *wd, ListBase *lb)
{
	bProperty *prop;

	prop= lb->first;
	while (prop) {
		writestruct(wd, DATA, "bProperty", 1, prop);

		if (prop->poin && prop->poin != &prop->data)
			writedata(wd, DATA, MEM_allocN_len(prop->poin), prop->poin);

		prop= prop->next;
	}
}

static void write_sensors(WriteData *wd, ListBase *lb)
{
	bSensor *sens;

	sens= lb->first;
	while (sens) {
		writestruct(wd, DATA, "bSensor", 1, sens);

		writedata(wd, DATA, sizeof(void *)*sens->totlinks, sens->links);

		switch(sens->type) {
		case SENS_NEAR:
			writestruct(wd, DATA, "bNearSensor", 1, sens->data);
			break;
		case SENS_MOUSE:
			writestruct(wd, DATA, "bMouseSensor", 1, sens->data);
			break;
		case SENS_TOUCH:
			writestruct(wd, DATA, "bTouchSensor", 1, sens->data);
			break;
		case SENS_KEYBOARD:
			writestruct(wd, DATA, "bKeyboardSensor", 1, sens->data);
			break;
		case SENS_PROPERTY:
			writestruct(wd, DATA, "bPropertySensor", 1, sens->data);
			break;
		case SENS_ARMATURE:
			writestruct(wd, DATA, "bArmatureSensor", 1, sens->data);
			break;
		case SENS_ACTUATOR:
			writestruct(wd, DATA, "bActuatorSensor", 1, sens->data);
			break;
		case SENS_DELAY:
			writestruct(wd, DATA, "bDelaySensor", 1, sens->data);
			break;
		case SENS_COLLISION:
			writestruct(wd, DATA, "bCollisionSensor", 1, sens->data);
			break;
		case SENS_RADAR:
			writestruct(wd, DATA, "bRadarSensor", 1, sens->data);
			break;
		case SENS_RANDOM:
			writestruct(wd, DATA, "bRandomSensor", 1, sens->data);
			break;
		case SENS_RAY:
			writestruct(wd, DATA, "bRaySensor", 1, sens->data);
			break;
		case SENS_MESSAGE:
			writestruct(wd, DATA, "bMessageSensor", 1, sens->data);
			break;
		case SENS_JOYSTICK:
			writestruct(wd, DATA, "bJoystickSensor", 1, sens->data);
			break;
		default:
			; /* error: don't know how to write this file */
		}

		sens= sens->next;
	}
}

static void write_controllers(WriteData *wd, ListBase *lb)
{
	bController *cont;

	cont= lb->first;
	while (cont) {
		writestruct(wd, DATA, "bController", 1, cont);

		writedata(wd, DATA, sizeof(void *)*cont->totlinks, cont->links);

		switch(cont->type) {
		case CONT_EXPRESSION:
			writestruct(wd, DATA, "bExpressionCont", 1, cont->data);
			break;
		case CONT_PYTHON:
			writestruct(wd, DATA, "bPythonCont", 1, cont->data);
			break;
		default:
			; /* error: don't know how to write this file */
		}

		cont= cont->next;
	}
}

static void write_actuators(WriteData *wd, ListBase *lb)
{
	bActuator *act;

	act= lb->first;
	while (act) {
		writestruct(wd, DATA, "bActuator", 1, act);

		switch(act->type) {
		case ACT_ACTION:
		case ACT_SHAPEACTION:
			writestruct(wd, DATA, "bActionActuator", 1, act->data);
			break;
		case ACT_SOUND:
			writestruct(wd, DATA, "bSoundActuator", 1, act->data);
			break;
		case ACT_OBJECT:
			writestruct(wd, DATA, "bObjectActuator", 1, act->data);
			break;
		case ACT_IPO:
			writestruct(wd, DATA, "bIpoActuator", 1, act->data);
			break;
		case ACT_PROPERTY:
			writestruct(wd, DATA, "bPropertyActuator", 1, act->data);
			break;
		case ACT_CAMERA:
			writestruct(wd, DATA, "bCameraActuator", 1, act->data);
			break;
		case ACT_CONSTRAINT:
			writestruct(wd, DATA, "bConstraintActuator", 1, act->data);
			break;
		case ACT_EDIT_OBJECT:
			writestruct(wd, DATA, "bEditObjectActuator", 1, act->data);
			break;
		case ACT_SCENE:
			writestruct(wd, DATA, "bSceneActuator", 1, act->data);
			break;
		case ACT_GROUP:
			writestruct(wd, DATA, "bGroupActuator", 1, act->data);
			break;
		case ACT_RANDOM:
			writestruct(wd, DATA, "bRandomActuator", 1, act->data);
			break;
		case ACT_MESSAGE:
			writestruct(wd, DATA, "bMessageActuator", 1, act->data);
			break;
		case ACT_GAME:
			writestruct(wd, DATA, "bGameActuator", 1, act->data);
			break;
		case ACT_VISIBILITY:
			writestruct(wd, DATA, "bVisibilityActuator", 1, act->data);
			break;
		case ACT_2DFILTER:
			writestruct(wd, DATA, "bTwoDFilterActuator", 1, act->data);
			break;
		case ACT_PARENT:
			writestruct(wd, DATA, "bParentActuator", 1, act->data);
			break;
		case ACT_STATE:
			writestruct(wd, DATA, "bStateActuator", 1, act->data);
			break;
		case ACT_ARMATURE:
			writestruct(wd, DATA, "bArmatureActuator", 1, act->data);
			break;
		case ACT_STEERING:
			writestruct(wd, DATA, "bSteeringActuator", 1, act->data);
			break;
		default:
			; /* error: don't know how to write this file */
		}

		act= act->next;
	}
}

static void write_motionpath(WriteData *wd, bMotionPath *mpath)
{
	/* sanity checks */
	if (mpath == NULL)
		return;
	
	/* firstly, just write the motionpath struct */
	writestruct(wd, DATA, "bMotionPath", 1, mpath);
	
	/* now write the array of data */
	writestruct(wd, DATA, "bMotionPathVert", mpath->length, mpath->points);
}

static void write_constraints(WriteData *wd, ListBase *conlist)
{
	bConstraint *con;

	for (con=conlist->first; con; con=con->next) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
		
		/* Write the specific data */
		if (cti && con->data) {
			/* firstly, just write the plain con->data struct */
			writestruct(wd, DATA, cti->structName, 1, con->data);
			
			/* do any constraint specific stuff */
			switch (con->type) {
				case CONSTRAINT_TYPE_PYTHON:
				{
					bPythonConstraint *data = (bPythonConstraint *)con->data;
					bConstraintTarget *ct;
					
					/* write targets */
					for (ct= data->targets.first; ct; ct= ct->next)
						writestruct(wd, DATA, "bConstraintTarget", 1, ct);
					
					/* Write ID Properties -- and copy this comment EXACTLY for easy finding
					 of library blocks that implement this.*/
					IDP_WriteProperty(data->prop, wd);
				}
					break;
				case CONSTRAINT_TYPE_SPLINEIK: 
				{
					bSplineIKConstraint *data= (bSplineIKConstraint*)con->data;
					
					/* write points array */
					writedata(wd, DATA, sizeof(float)*(data->numpoints), data->points);
				}
					break;
			}
		}
		
		/* Write the constraint */
		writestruct(wd, DATA, "bConstraint", 1, con);
	}
}

static void write_pose(WriteData *wd, bPose *pose)
{
	bPoseChannel *chan;
	bActionGroup *grp;

	/* Write each channel */
	if (!pose)
		return;

	/* Write channels */
	for (chan=pose->chanbase.first; chan; chan=chan->next) {
		/* Write ID Properties -- and copy this comment EXACTLY for easy finding
		 of library blocks that implement this.*/
		if (chan->prop)
			IDP_WriteProperty(chan->prop, wd);
		
		write_constraints(wd, &chan->constraints);
		
		write_motionpath(wd, chan->mpath);
		
		/* prevent crashes with autosave, when a bone duplicated in editmode has not yet been assigned to its posechannel */
		if (chan->bone) 
			chan->selectflag= chan->bone->flag & BONE_SELECTED; /* gets restored on read, for library armatures */
		
		writestruct(wd, DATA, "bPoseChannel", 1, chan);
	}
	
	/* Write groups */
	for (grp=pose->agroups.first; grp; grp=grp->next) 
		writestruct(wd, DATA, "bActionGroup", 1, grp);

	/* write IK param */
	if (pose->ikparam) {
		char *structname = (char *)get_ikparam_name(pose);
		if (structname)
			writestruct(wd, DATA, structname, 1, pose->ikparam);
	}

	/* Write this pose */
	writestruct(wd, DATA, "bPose", 1, pose);

}

static void write_defgroups(WriteData *wd, ListBase *defbase)
{
	bDeformGroup	*defgroup;

	for (defgroup=defbase->first; defgroup; defgroup=defgroup->next)
		writestruct(wd, DATA, "bDeformGroup", 1, defgroup);
}

static void write_modifiers(WriteData *wd, ListBase *modbase)
{
	ModifierData *md;

	if (modbase == NULL) return;
	for (md=modbase->first; md; md= md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		if (mti == NULL) return;
		
		writestruct(wd, DATA, mti->structName, 1, md);
			
		if (md->type==eModifierType_Hook) {
			HookModifierData *hmd = (HookModifierData*) md;
			
			writedata(wd, DATA, sizeof(int)*hmd->totindex, hmd->indexar);
		}
		else if (md->type==eModifierType_Cloth) {
			ClothModifierData *clmd = (ClothModifierData*) md;
			
			writestruct(wd, DATA, "ClothSimSettings", 1, clmd->sim_parms);
			writestruct(wd, DATA, "ClothCollSettings", 1, clmd->coll_parms);
			writestruct(wd, DATA, "EffectorWeights", 1, clmd->sim_parms->effector_weights);
			write_pointcaches(wd, &clmd->ptcaches);
		} 
		else if (md->type==eModifierType_Smoke) {
			SmokeModifierData *smd = (SmokeModifierData*) md;
			
			if (smd->type & MOD_SMOKE_TYPE_DOMAIN)
			{
				if (smd->domain)
				{
					write_pointcaches(wd, &(smd->domain->ptcaches[0]));

					/* create fake pointcache so that old blender versions can read it */
					smd->domain->point_cache[1] = BKE_ptcache_add(&smd->domain->ptcaches[1]);
					smd->domain->point_cache[1]->flag |= PTCACHE_DISK_CACHE|PTCACHE_FAKE_SMOKE;
					smd->domain->point_cache[1]->step = 1;

					write_pointcaches(wd, &(smd->domain->ptcaches[1]));
				}
				
				writestruct(wd, DATA, "SmokeDomainSettings", 1, smd->domain);

				if (smd->domain) {
					/* cleanup the fake pointcache */
					BKE_ptcache_free_list(&smd->domain->ptcaches[1]);
					smd->domain->point_cache[1] = NULL;
					
					writestruct(wd, DATA, "EffectorWeights", 1, smd->domain->effector_weights);
				}
			}
			else if (smd->type & MOD_SMOKE_TYPE_FLOW)
				writestruct(wd, DATA, "SmokeFlowSettings", 1, smd->flow);
			else if (smd->type & MOD_SMOKE_TYPE_COLL)
				writestruct(wd, DATA, "SmokeCollSettings", 1, smd->coll);
		} 
		else if (md->type==eModifierType_Fluidsim) {
			FluidsimModifierData *fluidmd = (FluidsimModifierData*) md;
			
			writestruct(wd, DATA, "FluidsimSettings", 1, fluidmd->fss);
		}
		else if (md->type==eModifierType_DynamicPaint) {
			DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;
			
			if (pmd->canvas)
			{
				DynamicPaintSurface *surface;
				writestruct(wd, DATA, "DynamicPaintCanvasSettings", 1, pmd->canvas);
				
				/* write surfaces */
				for (surface=pmd->canvas->surfaces.first; surface; surface=surface->next)
					writestruct(wd, DATA, "DynamicPaintSurface", 1, surface);
				/* write caches and effector weights */
				for (surface=pmd->canvas->surfaces.first; surface; surface=surface->next) {
					write_pointcaches(wd, &(surface->ptcaches));

					writestruct(wd, DATA, "EffectorWeights", 1, surface->effector_weights);
				}
			}
			if (pmd->brush)
			{
				writestruct(wd, DATA, "DynamicPaintBrushSettings", 1, pmd->brush);
				writestruct(wd, DATA, "ColorBand", 1, pmd->brush->paint_ramp);
				writestruct(wd, DATA, "ColorBand", 1, pmd->brush->vel_ramp);
			}
		} 
		else if (md->type==eModifierType_Collision) {
			
			/*
			CollisionModifierData *collmd = (CollisionModifierData*) md;
			// TODO: CollisionModifier should use pointcache 
			// + have proper reset events before enabling this
			writestruct(wd, DATA, "MVert", collmd->numverts, collmd->x);
			writestruct(wd, DATA, "MVert", collmd->numverts, collmd->xnew);
			writestruct(wd, DATA, "MFace", collmd->numfaces, collmd->mfaces);
			*/
		}
		else if (md->type==eModifierType_MeshDeform) {
			MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;
			int size = mmd->dyngridsize;

			writestruct(wd, DATA, "MDefInfluence", mmd->totinfluence, mmd->bindinfluences);
			writedata(wd, DATA, sizeof(int)*(mmd->totvert+1), mmd->bindoffsets);
			writedata(wd, DATA, sizeof(float)*3*mmd->totcagevert,
				mmd->bindcagecos);
			writestruct(wd, DATA, "MDefCell", size*size*size, mmd->dyngrid);
			writestruct(wd, DATA, "MDefInfluence", mmd->totinfluence, mmd->dyninfluences);
			writedata(wd, DATA, sizeof(int)*mmd->totvert, mmd->dynverts);
		}
		else if (md->type==eModifierType_Warp) {
			WarpModifierData *tmd = (WarpModifierData*) md;
			if (tmd->curfalloff) {
				write_curvemapping(wd, tmd->curfalloff);
			}
		}
		else if (md->type==eModifierType_WeightVGEdit) {
			WeightVGEditModifierData *wmd = (WeightVGEditModifierData*) md;

			if (wmd->cmap_curve)
				write_curvemapping(wd, wmd->cmap_curve);
		}
	}
}

static void write_objects(WriteData *wd, ListBase *idbase)
{
	Object *ob;
	
	ob= idbase->first;
	while (ob) {
		if (ob->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_OB, "Object", 1, ob);
			
			/*Write ID Properties -- and copy this comment EXACTLY for easy finding
			  of library blocks that implement this.*/
			if (ob->id.properties) IDP_WriteProperty(ob->id.properties, wd);
			
			if (ob->adt) write_animdata(wd, ob->adt);
			
			/* direct data */
			writedata(wd, DATA, sizeof(void *)*ob->totcol, ob->mat);
			writedata(wd, DATA, sizeof(char)*ob->totcol, ob->matbits);
			/* write_effects(wd, &ob->effect); */ /* not used anymore */
			write_properties(wd, &ob->prop);
			write_sensors(wd, &ob->sensors);
			write_controllers(wd, &ob->controllers);
			write_actuators(wd, &ob->actuators);

			if (ob->type == OB_ARMATURE) {
				bArmature *arm = ob->data;
				if (arm && ob->pose && arm->act_bone) {
					BLI_strncpy(ob->pose->proxy_act_bone, arm->act_bone->name, sizeof(ob->pose->proxy_act_bone));
				}
			}

			write_pose(wd, ob->pose);
			write_defgroups(wd, &ob->defbase);
			write_constraints(wd, &ob->constraints);
			write_motionpath(wd, ob->mpath);
			
			writestruct(wd, DATA, "PartDeflect", 1, ob->pd);
			writestruct(wd, DATA, "SoftBody", 1, ob->soft);
			if (ob->soft) {
				write_pointcaches(wd, &ob->soft->ptcaches);
				writestruct(wd, DATA, "EffectorWeights", 1, ob->soft->effector_weights);
			}
			writestruct(wd, DATA, "BulletSoftBody", 1, ob->bsoft);
			
			write_particlesystems(wd, &ob->particlesystem);
			write_modifiers(wd, &ob->modifiers);
		}
		ob= ob->id.next;
	}

	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}


static void write_vfonts(WriteData *wd, ListBase *idbase)
{
	VFont *vf;
	PackedFile * pf;

	vf= idbase->first;
	while (vf) {
		if (vf->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_VF, "VFont", 1, vf);
			if (vf->id.properties) IDP_WriteProperty(vf->id.properties, wd);

			/* direct data */

			if (vf->packedfile) {
				pf = vf->packedfile;
				writestruct(wd, DATA, "PackedFile", 1, pf);
				writedata(wd, DATA, pf->size, pf->data);
			}
		}

		vf= vf->id.next;
	}
}


static void write_keys(WriteData *wd, ListBase *idbase)
{
	Key *key;
	KeyBlock *kb;

	key= idbase->first;
	while (key) {
		if (key->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_KE, "Key", 1, key);
			if (key->id.properties) IDP_WriteProperty(key->id.properties, wd);
			
			if (key->adt) write_animdata(wd, key->adt);
			
			/* direct data */
			kb= key->block.first;
			while (kb) {
				writestruct(wd, DATA, "KeyBlock", 1, kb);
				if (kb->data) writedata(wd, DATA, kb->totelem*key->elemsize, kb->data);
				kb= kb->next;
			}
		}

		key= key->id.next;
	}
	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_cameras(WriteData *wd, ListBase *idbase)
{
	Camera *cam;

	cam= idbase->first;
	while (cam) {
		if (cam->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_CA, "Camera", 1, cam);
			if (cam->id.properties) IDP_WriteProperty(cam->id.properties, wd);
			
			if (cam->adt) write_animdata(wd, cam->adt);
		}

		cam= cam->id.next;
	}
}

static void write_mballs(WriteData *wd, ListBase *idbase)
{
	MetaBall *mb;
	MetaElem *ml;

	mb= idbase->first;
	while (mb) {
		if (mb->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_MB, "MetaBall", 1, mb);
			if (mb->id.properties) IDP_WriteProperty(mb->id.properties, wd);

			/* direct data */
			writedata(wd, DATA, sizeof(void *)*mb->totcol, mb->mat);
			if (mb->adt) write_animdata(wd, mb->adt);

			ml= mb->elems.first;
			while (ml) {
				writestruct(wd, DATA, "MetaElem", 1, ml);
				ml= ml->next;
			}
		}
		mb= mb->id.next;
	}
}

static int amount_of_chars(char *str)
{
	// Since the data is saved as UTF-8 to the cu->str
	// The cu->len is not same as the strlen(cu->str)
	return strlen(str);
}

static void write_curves(WriteData *wd, ListBase *idbase)
{
	Curve *cu;
	Nurb *nu;

	cu= idbase->first;
	while (cu) {
		if (cu->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_CU, "Curve", 1, cu);
			
			/* direct data */
			writedata(wd, DATA, sizeof(void *)*cu->totcol, cu->mat);
			if (cu->id.properties) IDP_WriteProperty(cu->id.properties, wd);
			if (cu->adt) write_animdata(wd, cu->adt);
			
			if (cu->vfont) {
				writedata(wd, DATA, amount_of_chars(cu->str)+1, cu->str);
				writestruct(wd, DATA, "CharInfo", cu->len+1, cu->strinfo);
				writestruct(wd, DATA, "TextBox", cu->totbox, cu->tb);				
			}
			else {
				/* is also the order of reading */
				nu= cu->nurb.first;
				while (nu) {
					writestruct(wd, DATA, "Nurb", 1, nu);
					nu= nu->next;
				}
				nu= cu->nurb.first;
				while (nu) {
					if (nu->type == CU_BEZIER)
						writestruct(wd, DATA, "BezTriple", nu->pntsu, nu->bezt);
					else {
						writestruct(wd, DATA, "BPoint", nu->pntsu*nu->pntsv, nu->bp);
						if (nu->knotsu) writedata(wd, DATA, KNOTSU(nu)*sizeof(float), nu->knotsu);
						if (nu->knotsv) writedata(wd, DATA, KNOTSV(nu)*sizeof(float), nu->knotsv);
					}
					nu= nu->next;
				}
			}
		}
		cu= cu->id.next;
	}

	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_dverts(WriteData *wd, int count, MDeformVert *dvlist)
{
	if (dvlist) {
		int i;
		
		/* Write the dvert list */
		writestruct(wd, DATA, "MDeformVert", count, dvlist);
		
		/* Write deformation data for each dvert */
		for (i=0; i<count; i++) {
			if (dvlist[i].dw)
				writestruct(wd, DATA, "MDeformWeight", dvlist[i].totweight, dvlist[i].dw);
		}
	}
}

static void write_mdisps(WriteData *wd, int count, MDisps *mdlist, int external)
{
	if (mdlist) {
		int i;
		
		writestruct(wd, DATA, "MDisps", count, mdlist);
		for (i = 0; i < count; ++i) {
			MDisps *md = &mdlist[i];
			if (md->disps) {
				if (!external)
					writedata(wd, DATA, sizeof(float)*3*md->totdisp, md->disps);
			}
			
			if (md->hidden)
				writedata(wd, DATA, BLI_BITMAP_SIZE(md->totdisp), md->hidden);
		}
	}
}

static void write_customdata(WriteData *wd, ID *id, int count, CustomData *data, int partial_type, int partial_count)
{
	int i;

	/* write external customdata (not for undo) */
	if (data->external && !wd->current)
		CustomData_external_write(data, id, CD_MASK_MESH, count, 0);

	writestruct(wd, DATA, "CustomDataLayer", data->maxlayer, data->layers);

	for (i=0; i<data->totlayer; i++) {
		CustomDataLayer *layer= &data->layers[i];
		const char *structname;
		int structnum, datasize;

		if (layer->type == CD_MDEFORMVERT) {
			/* layer types that allocate own memory need special handling */
			write_dverts(wd, count, layer->data);
		}
		else if (layer->type == CD_MDISPS) {
			write_mdisps(wd, count, layer->data, layer->flag & CD_FLAG_EXTERNAL);
		}
		else {
			CustomData_file_write_info(layer->type, &structname, &structnum);
			if (structnum) {
				/* when using partial visibility, the MEdge and MFace layers
				   are smaller than the original, so their type and count is
				   passed to make this work */
				if (layer->type != partial_type) datasize= structnum*count;
				else datasize= structnum*partial_count;

				writestruct(wd, DATA, structname, datasize, layer->data);
			}
			else
				printf("%s error: layer '%s':%d - can't be written to file\n",
				       __func__, structname, layer->type);
		}
	}

	if (data->external)
		writestruct(wd, DATA, "CustomDataExternal", 1, data->external);
}

static void write_meshs(WriteData *wd, ListBase *idbase)
{
	Mesh *mesh;
	int save_for_old_blender= 0;

#ifdef USE_BMESH_SAVE_AS_COMPAT
	save_for_old_blender = wd->use_mesh_compat; /* option to save with older mesh format */
#endif

	mesh= idbase->first;
	while (mesh) {
		if (mesh->id.us>0 || wd->current) {
			/* write LibData */
			if (!save_for_old_blender) {

#ifdef USE_BMESH_SAVE_WITHOUT_MFACE
				Mesh backup_mesh = {{0}};
				/* cache only - don't write */
				backup_mesh.mface = mesh->mface;
				mesh->mface = NULL;
				/* -- */
				backup_mesh.totface = mesh->totface;
				mesh->totface = 0;
				/* -- */
#endif /* USE_BMESH_SAVE_WITHOUT_MFACE */

				writestruct(wd, ID_ME, "Mesh", 1, mesh);

				/* direct data */
				if (mesh->id.properties) IDP_WriteProperty(mesh->id.properties, wd);
				if (mesh->adt) write_animdata(wd, mesh->adt);

				writedata(wd, DATA, sizeof(void *)*mesh->totcol, mesh->mat);
				writedata(wd, DATA, sizeof(MSelect) * mesh->totselect, mesh->mselect);

				write_customdata(wd, &mesh->id, mesh->totvert, &mesh->vdata, -1, 0);
				write_customdata(wd, &mesh->id, mesh->totedge, &mesh->edata, -1, 0);
				/* fdata is really a dummy - written so slots align */
				write_customdata(wd, &mesh->id, mesh->totface, &mesh->fdata, -1, 0);
				write_customdata(wd, &mesh->id, mesh->totloop, &mesh->ldata, -1, 0);
				write_customdata(wd, &mesh->id, mesh->totpoly, &mesh->pdata, -1, 0);

#ifdef USE_BMESH_SAVE_WITHOUT_MFACE
				/* cache only - don't write */
				mesh->mface = backup_mesh.mface;
				/* -- */
				mesh->totface = backup_mesh.totface;
#endif /* USE_BMESH_SAVE_WITHOUT_MFACE */

			}
			else {

#ifdef USE_BMESH_SAVE_AS_COMPAT

				Mesh backup_mesh = {{0}};

				/* backup */
				backup_mesh.mpoly = mesh->mpoly;
				mesh->mpoly = NULL;
				/* -- */
				backup_mesh.mface = mesh->mface;
				mesh->mface = NULL;
				/* -- */
				backup_mesh.totface = mesh->totface;
				mesh->totface = 0;
				/* -- */
				backup_mesh.totpoly = mesh->totpoly;
				mesh->totpoly = 0;
				/* -- */
				backup_mesh.totloop = mesh->totloop;
				mesh->totloop = 0;
				/* -- */
				backup_mesh.fdata = mesh->fdata;
				memset(&mesh->fdata, 0, sizeof(CustomData));
				/* -- */
				backup_mesh.pdata = mesh->pdata;
				memset(&mesh->pdata, 0, sizeof(CustomData));
				/* -- */
				backup_mesh.ldata = mesh->ldata;
				memset(&mesh->ldata, 0, sizeof(CustomData));
				/* -- */
				backup_mesh.edit_btmesh = mesh->edit_btmesh;
				mesh->edit_btmesh = NULL;
				/* backup */


				/* now fill in polys to mfaces*/
				mesh->totface= mesh_mpoly_to_mface(&mesh->fdata, &backup_mesh.ldata, &backup_mesh.pdata,
				                                   mesh->totface, backup_mesh.totloop, backup_mesh.totpoly);

				mesh_update_customdata_pointers(mesh, FALSE);

				writestruct(wd, ID_ME, "Mesh", 1, mesh);

				/* direct data */
				if (mesh->id.properties) IDP_WriteProperty(mesh->id.properties, wd);
				if (mesh->adt) write_animdata(wd, mesh->adt);

				writedata(wd, DATA, sizeof(void *)*mesh->totcol, mesh->mat);
				/* writedata(wd, DATA, sizeof(MSelect) * mesh->totselect, mesh->mselect); */ /* pre-bmesh NULL's */

				write_customdata(wd, &mesh->id, mesh->totvert, &mesh->vdata, -1, 0);
				write_customdata(wd, &mesh->id, mesh->totedge, &mesh->edata, -1, 0);
				write_customdata(wd, &mesh->id, mesh->totface, &mesh->fdata, -1, 0);
				/* harmless for older blender versioins but _not_ writing these keeps file size down */
				/*
				write_customdata(wd, &mesh->id, mesh->totloop, &mesh->ldata, -1, 0);
				write_customdata(wd, &mesh->id, mesh->totpoly, &mesh->pdata, -1, 0);
				*/

				/* restore */
				mesh->mpoly = backup_mesh.mpoly;
				/* -- */
				mesh->mface = backup_mesh.mface;
				/* -- */
				CustomData_free(&mesh->fdata, mesh->totface);
				/* -- */
				mesh->fdata= backup_mesh.fdata;
				/* -- */
				mesh->pdata= backup_mesh.pdata;
				/* -- */
				mesh->ldata= backup_mesh.ldata;
				/* -- */
				mesh->totface = backup_mesh.totface;
				mesh->totpoly = backup_mesh.totpoly;
				mesh->totloop = backup_mesh.totloop;
				/* -- */
				mesh_update_customdata_pointers(mesh, FALSE);
				/* --*/
				mesh->edit_btmesh = backup_mesh.edit_btmesh; /* keep this after updating custom pointers */
				/* restore */

#endif /* USE_BMESH_SAVE_AS_COMPAT */
			}
		}
		mesh= mesh->id.next;
	}
}

static void write_lattices(WriteData *wd, ListBase *idbase)
{
	Lattice *lt;
	
	lt= idbase->first;
	while (lt) {
		if (lt->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_LT, "Lattice", 1, lt);
			if (lt->id.properties) IDP_WriteProperty(lt->id.properties, wd);
			
			/* write animdata */
			if (lt->adt) write_animdata(wd, lt->adt);
			
			/* direct data */
			writestruct(wd, DATA, "BPoint", lt->pntsu*lt->pntsv*lt->pntsw, lt->def);
			
			write_dverts(wd, lt->pntsu*lt->pntsv*lt->pntsw, lt->dvert);
			
		}
		lt= lt->id.next;
	}
}

static void write_previews(WriteData *wd, PreviewImage *prv)
{
	if (prv) {
		short w = prv->w[1];
		short h = prv->h[1];
		unsigned int *rect = prv->rect[1];
		/* don't write out large previews if not requested */
		if (!(U.flag & USER_SAVE_PREVIEWS)) {
			prv->w[1] = 0;
			prv->h[1] = 0;
			prv->rect[1] = NULL;
		}
		writestruct(wd, DATA, "PreviewImage", 1, prv);
		if (prv->rect[0]) writedata(wd, DATA, prv->w[0]*prv->h[0]*sizeof(unsigned int), prv->rect[0]);
		if (prv->rect[1]) writedata(wd, DATA, prv->w[1]*prv->h[1]*sizeof(unsigned int), prv->rect[1]);

		/* restore preview, we still want to keep it in memory even if not saved to file */
		if (!(U.flag & USER_SAVE_PREVIEWS) ) {
			prv->w[1] = w;
			prv->h[1] = h;
			prv->rect[1] = rect;
		}
	}
}

static void write_images(WriteData *wd, ListBase *idbase)
{
	Image *ima;
	PackedFile * pf;


	ima= idbase->first;
	while (ima) {
		if (ima->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_IM, "Image", 1, ima);
			if (ima->id.properties) IDP_WriteProperty(ima->id.properties, wd);

			if (ima->packedfile) {
				pf = ima->packedfile;
				writestruct(wd, DATA, "PackedFile", 1, pf);
				writedata(wd, DATA, pf->size, pf->data);
			}

			write_previews(wd, ima->preview);
		}
		ima= ima->id.next;
	}
	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_textures(WriteData *wd, ListBase *idbase)
{
	Tex *tex;

	tex= idbase->first;
	while (tex) {
		if (tex->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_TE, "Tex", 1, tex);
			if (tex->id.properties) IDP_WriteProperty(tex->id.properties, wd);

			if (tex->adt) write_animdata(wd, tex->adt);

			/* direct data */
			if (tex->type == TEX_PLUGIN && tex->plugin) writestruct(wd, DATA, "PluginTex", 1, tex->plugin);
			if (tex->coba) writestruct(wd, DATA, "ColorBand", 1, tex->coba);
			if (tex->type == TEX_ENVMAP && tex->env) writestruct(wd, DATA, "EnvMap", 1, tex->env);
			if (tex->type == TEX_POINTDENSITY && tex->pd) {
				writestruct(wd, DATA, "PointDensity", 1, tex->pd);
				if (tex->pd->coba) writestruct(wd, DATA, "ColorBand", 1, tex->pd->coba);
				if (tex->pd->falloff_curve) write_curvemapping(wd, tex->pd->falloff_curve);
			}
			if (tex->type == TEX_VOXELDATA) writestruct(wd, DATA, "VoxelData", 1, tex->vd);
			if (tex->type == TEX_OCEAN && tex->ot) writestruct(wd, DATA, "OceanTex", 1, tex->ot);
			
			/* nodetree is integral part of texture, no libdata */
			if (tex->nodetree) {
				writestruct(wd, DATA, "bNodeTree", 1, tex->nodetree);
				write_nodetree(wd, tex->nodetree);
			}
			
			write_previews(wd, tex->preview);
		}
		tex= tex->id.next;
	}

	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_materials(WriteData *wd, ListBase *idbase)
{
	Material *ma;
	int a;

	ma= idbase->first;
	while (ma) {
		if (ma->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_MA, "Material", 1, ma);
			
			/*Write ID Properties -- and copy this comment EXACTLY for easy finding
			  of library blocks that implement this.*/
			/*manually set head group property to IDP_GROUP, just in case it hadn't been
			  set yet :) */
			if (ma->id.properties) IDP_WriteProperty(ma->id.properties, wd);
			
			if (ma->adt) write_animdata(wd, ma->adt);

			for (a=0; a<MAX_MTEX; a++) {
				if (ma->mtex[a]) writestruct(wd, DATA, "MTex", 1, ma->mtex[a]);
			}
			
			if (ma->ramp_col) writestruct(wd, DATA, "ColorBand", 1, ma->ramp_col);
			if (ma->ramp_spec) writestruct(wd, DATA, "ColorBand", 1, ma->ramp_spec);
			
			/* nodetree is integral part of material, no libdata */
			if (ma->nodetree) {
				writestruct(wd, DATA, "bNodeTree", 1, ma->nodetree);
				write_nodetree(wd, ma->nodetree);
			}

			write_previews(wd, ma->preview);			
		}
		ma= ma->id.next;
	}
}

static void write_worlds(WriteData *wd, ListBase *idbase)
{
	World *wrld;
	int a;

	wrld= idbase->first;
	while (wrld) {
		if (wrld->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_WO, "World", 1, wrld);
			if (wrld->id.properties) IDP_WriteProperty(wrld->id.properties, wd);
			
			if (wrld->adt) write_animdata(wd, wrld->adt);
			
			for (a=0; a<MAX_MTEX; a++) {
				if (wrld->mtex[a]) writestruct(wd, DATA, "MTex", 1, wrld->mtex[a]);
			}

			/* nodetree is integral part of lamps, no libdata */
			if (wrld->nodetree) {
				writestruct(wd, DATA, "bNodeTree", 1, wrld->nodetree);
				write_nodetree(wd, wrld->nodetree);
			}
			
			write_previews(wd, wrld->preview);
		}
		wrld= wrld->id.next;
	}
}

static void write_lamps(WriteData *wd, ListBase *idbase)
{
	Lamp *la;
	int a;

	la= idbase->first;
	while (la) {
		if (la->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_LA, "Lamp", 1, la);
			if (la->id.properties) IDP_WriteProperty(la->id.properties, wd);
			
			if (la->adt) write_animdata(wd, la->adt);
			
			/* direct data */
			for (a=0; a<MAX_MTEX; a++) {
				if (la->mtex[a]) writestruct(wd, DATA, "MTex", 1, la->mtex[a]);
			}
			
			if (la->curfalloff)
				write_curvemapping(wd, la->curfalloff);	
			
			/* nodetree is integral part of lamps, no libdata */
			if (la->nodetree) {
				writestruct(wd, DATA, "bNodeTree", 1, la->nodetree);
				write_nodetree(wd, la->nodetree);
			}

			write_previews(wd, la->preview);
			
		}
		la= la->id.next;
	}
}


static void write_scenes(WriteData *wd, ListBase *scebase)
{
	Scene *sce;
	Base *base;
	Editing *ed;
	Sequence *seq;
	MetaStack *ms;
	Strip *strip;
	TimeMarker *marker;
	TransformOrientation *ts;
	SceneRenderLayer *srl;
	ToolSettings *tos;
	
	sce= scebase->first;
	while (sce) {
		/* write LibData */
		writestruct(wd, ID_SCE, "Scene", 1, sce);
		if (sce->id.properties) IDP_WriteProperty(sce->id.properties, wd);
		
		if (sce->adt) write_animdata(wd, sce->adt);
		write_keyingsets(wd, &sce->keyingsets);
		
		/* direct data */
		base= sce->base.first;
		while (base) {
			writestruct(wd, DATA, "Base", 1, base);
			base= base->next;
		}
		
		tos = sce->toolsettings;
		writestruct(wd, DATA, "ToolSettings", 1, tos);
		if (tos->vpaint) {
			writestruct(wd, DATA, "VPaint", 1, tos->vpaint);
		}
		if (tos->wpaint) {
			writestruct(wd, DATA, "VPaint", 1, tos->wpaint);
		}
		if (tos->sculpt) {
			writestruct(wd, DATA, "Sculpt", 1, tos->sculpt);
		}
		if (tos->uvsculpt) {
			writestruct(wd, DATA, "UvSculpt", 1, tos->uvsculpt);
		}

		// write_paint(wd, &tos->imapaint.paint);

		ed= sce->ed;
		if (ed) {
			writestruct(wd, DATA, "Editing", 1, ed);
			
			/* reset write flags too */
			
			SEQ_BEGIN(ed, seq) {
				if (seq->strip) seq->strip->done= 0;
				writestruct(wd, DATA, "Sequence", 1, seq);
			}
			SEQ_END
			
			SEQ_BEGIN(ed, seq) {
				if (seq->strip && seq->strip->done==0) {
					/* write strip with 'done' at 0 because readfile */
					
					if (seq->plugin) writestruct(wd, DATA, "PluginSeq", 1, seq->plugin);
					if (seq->effectdata) {
						switch(seq->type) {
						case SEQ_COLOR:
							writestruct(wd, DATA, "SolidColorVars", 1, seq->effectdata);
							break;
						case SEQ_SPEED:
							writestruct(wd, DATA, "SpeedControlVars", 1, seq->effectdata);
							break;
						case SEQ_WIPE:
							writestruct(wd, DATA, "WipeVars", 1, seq->effectdata);
							break;
						case SEQ_GLOW:
							writestruct(wd, DATA, "GlowVars", 1, seq->effectdata);
							break;
						case SEQ_TRANSFORM:
							writestruct(wd, DATA, "TransformVars", 1, seq->effectdata);
							break;
						}
					}
					
					strip= seq->strip;
					writestruct(wd, DATA, "Strip", 1, strip);
					if (seq->flag & SEQ_USE_CROP && strip->crop) {
						writestruct(wd, DATA, "StripCrop", 1, strip->crop);
					}
					if (seq->flag & SEQ_USE_TRANSFORM && strip->transform) {
						writestruct(wd, DATA, "StripTransform", 1, strip->transform);
					}
					if (seq->flag & SEQ_USE_PROXY && strip->proxy) {
						writestruct(wd, DATA, "StripProxy", 1, strip->proxy);
					}
					if (seq->flag & SEQ_USE_COLOR_BALANCE && strip->color_balance) {
						writestruct(wd, DATA, "StripColorBalance", 1, strip->color_balance);
					}
					if (seq->type==SEQ_IMAGE)
						writestruct(wd, DATA, "StripElem", MEM_allocN_len(strip->stripdata) / sizeof(struct StripElem), strip->stripdata);
					else if (seq->type==SEQ_MOVIE || seq->type==SEQ_RAM_SOUND || seq->type == SEQ_HD_SOUND)
						writestruct(wd, DATA, "StripElem", 1, strip->stripdata);
					
					strip->done= 1;
				}
			}
			SEQ_END
				
			/* new; meta stack too, even when its nasty restore code */
			for (ms= ed->metastack.first; ms; ms= ms->next) {
				writestruct(wd, DATA, "MetaStack", 1, ms);
			}
		}
		
		if (sce->r.avicodecdata) {
			writestruct(wd, DATA, "AviCodecData", 1, sce->r.avicodecdata);
			if (sce->r.avicodecdata->lpFormat) writedata(wd, DATA, sce->r.avicodecdata->cbFormat, sce->r.avicodecdata->lpFormat);
			if (sce->r.avicodecdata->lpParms) writedata(wd, DATA, sce->r.avicodecdata->cbParms, sce->r.avicodecdata->lpParms);
		}

		if (sce->r.qtcodecdata) {
			writestruct(wd, DATA, "QuicktimeCodecData", 1, sce->r.qtcodecdata);
			if (sce->r.qtcodecdata->cdParms) writedata(wd, DATA, sce->r.qtcodecdata->cdSize, sce->r.qtcodecdata->cdParms);
		}
		if (sce->r.ffcodecdata.properties) {
			IDP_WriteProperty(sce->r.ffcodecdata.properties, wd);
		}

		/* writing dynamic list of TimeMarkers to the blend file */
		for (marker= sce->markers.first; marker; marker= marker->next)
			writestruct(wd, DATA, "TimeMarker", 1, marker);
		
		/* writing dynamic list of TransformOrientations to the blend file */
		for (ts = sce->transform_spaces.first; ts; ts = ts->next)
			writestruct(wd, DATA, "TransformOrientation", 1, ts);
		
		for (srl= sce->r.layers.first; srl; srl= srl->next)
			writestruct(wd, DATA, "SceneRenderLayer", 1, srl);
		
		if (sce->nodetree) {
			writestruct(wd, DATA, "bNodeTree", 1, sce->nodetree);
			write_nodetree(wd, sce->nodetree);
		}
		
		sce= sce->id.next;
	}
	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_gpencils(WriteData *wd, ListBase *lb)
{
	bGPdata *gpd;
	bGPDlayer *gpl;
	bGPDframe *gpf;
	bGPDstroke *gps;
	
	for (gpd= lb->first; gpd; gpd= gpd->id.next) {
		if (gpd->id.us>0 || wd->current) {
			/* write gpd data block to file */
			writestruct(wd, ID_GD, "bGPdata", 1, gpd);
			
			/* write grease-pencil layers to file */
			for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
				writestruct(wd, DATA, "bGPDlayer", 1, gpl);
				
				/* write this layer's frames to file */
				for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
					writestruct(wd, DATA, "bGPDframe", 1, gpf);
					
					/* write strokes */
					for (gps= gpf->strokes.first; gps; gps= gps->next) {
						writestruct(wd, DATA, "bGPDstroke", 1, gps);
						writestruct(wd, DATA, "bGPDspoint", gps->totpoints, gps->points);				
					}
				}
			}
		}
	}
}

static void write_windowmanagers(WriteData *wd, ListBase *lb)
{
	wmWindowManager *wm;
	wmWindow *win;
	
	for (wm= lb->first; wm; wm= wm->id.next) {
		writestruct(wd, ID_WM, "wmWindowManager", 1, wm);
		
		for (win= wm->windows.first; win; win= win->next)
			writestruct(wd, DATA, "wmWindow", 1, win);
	}
}

static void write_region(WriteData *wd, ARegion *ar, int spacetype)
{	
	writestruct(wd, DATA, "ARegion", 1, ar);
	
	if (ar->regiondata) {
		switch(spacetype) {
			case SPACE_VIEW3D:
				if (ar->regiontype==RGN_TYPE_WINDOW) {
					RegionView3D *rv3d= ar->regiondata;
					writestruct(wd, DATA, "RegionView3D", 1, rv3d);
					
					if (rv3d->localvd)
						writestruct(wd, DATA, "RegionView3D", 1, rv3d->localvd);
					if (rv3d->clipbb)
						writestruct(wd, DATA, "BoundBox", 1, rv3d->clipbb);

				}
				else
					printf("regiondata write missing!\n");
				break;
			default:
				printf("regiondata write missing!\n");
		}
	}
}

static void write_screens(WriteData *wd, ListBase *scrbase)
{
	bScreen *sc;
	ScrArea *sa;
	ScrVert *sv;
	ScrEdge *se;

	sc= scrbase->first;
	while (sc) {
		
		/* write LibData */
		/* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
		writestruct(wd, ID_SCRN, "Screen", 1, sc);
		if (sc->id.properties) 
			IDP_WriteProperty(sc->id.properties, wd);
		
		/* direct data */
		for (sv= sc->vertbase.first; sv; sv= sv->next)
			writestruct(wd, DATA, "ScrVert", 1, sv);
		
		for (se= sc->edgebase.first; se; se= se->next)
			writestruct(wd, DATA, "ScrEdge", 1, se);
		
		for (sa= sc->areabase.first; sa; sa= sa->next) {
			SpaceLink *sl;
			Panel *pa;
			ARegion *ar;
			
			writestruct(wd, DATA, "ScrArea", 1, sa);
			
			for (ar= sa->regionbase.first; ar; ar= ar->next) {
				write_region(wd, ar, sa->spacetype);
				
				for (pa= ar->panels.first; pa; pa= pa->next)
					writestruct(wd, DATA, "Panel", 1, pa);
			}
			
			sl= sa->spacedata.first;
			while (sl) {
				for (ar= sl->regionbase.first; ar; ar= ar->next)
					write_region(wd, ar, sl->spacetype);
				
				if (sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D *) sl;
					BGpic *bgpic;
					writestruct(wd, DATA, "View3D", 1, v3d);
					for (bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next)
						writestruct(wd, DATA, "BGpic", 1, bgpic);
					if (v3d->localvd) writestruct(wd, DATA, "View3D", 1, v3d->localvd);
				}
				else if (sl->spacetype==SPACE_IPO) {
					SpaceIpo *sipo= (SpaceIpo *)sl;
					ListBase tmpGhosts = sipo->ghostCurves;
					
					/* temporarily disable ghost curves when saving */
					sipo->ghostCurves.first= sipo->ghostCurves.last= NULL;
					
					writestruct(wd, DATA, "SpaceIpo", 1, sl);
					if (sipo->ads) writestruct(wd, DATA, "bDopeSheet", 1, sipo->ads);
					
					/* reenable ghost curves */
					sipo->ghostCurves= tmpGhosts;
				}
				else if (sl->spacetype==SPACE_BUTS) {
					writestruct(wd, DATA, "SpaceButs", 1, sl);
				}
				else if (sl->spacetype==SPACE_FILE) {
					SpaceFile *sfile= (SpaceFile *)sl;

					writestruct(wd, DATA, "SpaceFile", 1, sl);
					if (sfile->params)
						writestruct(wd, DATA, "FileSelectParams", 1, sfile->params);
				}
				else if (sl->spacetype==SPACE_SEQ) {
					writestruct(wd, DATA, "SpaceSeq", 1, sl);
				}
				else if (sl->spacetype==SPACE_OUTLINER) {
					SpaceOops *so= (SpaceOops *)sl;
					
					writestruct(wd, DATA, "SpaceOops", 1, so);

					/* outliner */
					if (so->treestore) {
						writestruct(wd, DATA, "TreeStore", 1, so->treestore);
						if (so->treestore->data)
							writestruct(wd, DATA, "TreeStoreElem", so->treestore->usedelem, so->treestore->data);
					}
				}
				else if (sl->spacetype==SPACE_IMAGE) {
					SpaceImage *sima= (SpaceImage *)sl;
					
					writestruct(wd, DATA, "SpaceImage", 1, sl);
					if (sima->cumap)
						write_curvemapping(wd, sima->cumap);
				}
				else if (sl->spacetype==SPACE_TEXT) {
					writestruct(wd, DATA, "SpaceText", 1, sl);
				}
				else if (sl->spacetype==SPACE_SCRIPT) {
					SpaceScript *scr = (SpaceScript*)sl;
					scr->but_refs = NULL;
					writestruct(wd, DATA, "SpaceScript", 1, sl);
				}
				else if (sl->spacetype==SPACE_ACTION) {
					writestruct(wd, DATA, "SpaceAction", 1, sl);
				}
				else if (sl->spacetype==SPACE_NLA) {
					SpaceNla *snla= (SpaceNla *)sl;
					
					writestruct(wd, DATA, "SpaceNla", 1, snla);
					if (snla->ads) writestruct(wd, DATA, "bDopeSheet", 1, snla->ads);
				}
				else if (sl->spacetype==SPACE_TIME) {
					writestruct(wd, DATA, "SpaceTime", 1, sl);
				}
				else if (sl->spacetype==SPACE_NODE) {
					writestruct(wd, DATA, "SpaceNode", 1, sl);
				}
				else if (sl->spacetype==SPACE_LOGIC) {
					writestruct(wd, DATA, "SpaceLogic", 1, sl);
				}
				else if (sl->spacetype==SPACE_CONSOLE) {
					SpaceConsole *con = (SpaceConsole*)sl;
					ConsoleLine *cl;

					for (cl=con->history.first; cl; cl=cl->next) {
						/* 'len_alloc' is invalid on write, set from 'len' on read */
						writestruct(wd, DATA, "ConsoleLine", 1, cl);
						writedata(wd, DATA, cl->len+1, cl->line);
					}
					writestruct(wd, DATA, "SpaceConsole", 1, sl);

				}
				else if (sl->spacetype==SPACE_USERPREF) {
					writestruct(wd, DATA, "SpaceUserPref", 1, sl);
				}
				else if (sl->spacetype==SPACE_CLIP) {
					writestruct(wd, DATA, "SpaceClip", 1, sl);
				}

				sl= sl->next;
			}
		}

		sc= sc->id.next;
	}
}

static void write_libraries(WriteData *wd, Main *main)
{
	ListBase *lbarray[MAX_LIBARRAY];
	ID *id;
	int a, tot, foundone;

	for (; main; main= main->next) {

		a=tot= set_listbasepointers(main, lbarray);

		/* test: is lib being used */
		foundone= 0;
		while (tot--) {
			for (id= lbarray[tot]->first; id; id= id->next) {
				if (id->us>0 && (id->flag & LIB_EXTERN)) {
					foundone= 1;
					break;
				}
			}
			if (foundone) break;
		}

		if (foundone) {
			writestruct(wd, ID_LI, "Library", 1, main->curlib);

			while (a--) {
				for (id= lbarray[a]->first; id; id= id->next) {
					if (id->us>0 && (id->flag & LIB_EXTERN)) {
						writestruct(wd, ID_ID, "ID", 1, id);
					}
				}
			}
		}
	}
}

static void write_bone(WriteData *wd, Bone* bone)
{
	Bone*	cbone;

	// PATCH for upward compatibility after 2.37+ armature recode
	bone->size[0]= bone->size[1]= bone->size[2]= 1.0f;
		
	// Write this bone
	writestruct(wd, DATA, "Bone", 1, bone);

	/* Write ID Properties -- and copy this comment EXACTLY for easy finding
	 of library blocks that implement this.*/
	if (bone->prop)
		IDP_WriteProperty(bone->prop, wd);
	
	// Write Children
	cbone= bone->childbase.first;
	while (cbone) {
		write_bone(wd, cbone);
		cbone= cbone->next;
	}
}

static void write_armatures(WriteData *wd, ListBase *idbase)
{
	bArmature	*arm;
	Bone		*bone;

	arm=idbase->first;
	while (arm) {
		if (arm->id.us>0 || wd->current) {
			writestruct(wd, ID_AR, "bArmature", 1, arm);
			if (arm->id.properties) IDP_WriteProperty(arm->id.properties, wd);

			if (arm->adt) write_animdata(wd, arm->adt);

			/* Direct data */
			bone= arm->bonebase.first;
			while (bone) {
				write_bone(wd, bone);
				bone=bone->next;
			}
		}
		arm=arm->id.next;
	}

	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_texts(WriteData *wd, ListBase *idbase)
{
	Text *text;
	TextLine *tmp;
	TextMarker *mrk;

	text= idbase->first;
	while (text) {
		if ( (text->flags & TXT_ISMEM) && (text->flags & TXT_ISEXT)) text->flags &= ~TXT_ISEXT;

		/* write LibData */
		writestruct(wd, ID_TXT, "Text", 1, text);
		if (text->name) writedata(wd, DATA, strlen(text->name)+1, text->name);
		if (text->id.properties) IDP_WriteProperty(text->id.properties, wd);

		if (!(text->flags & TXT_ISEXT)) {
			/* now write the text data, in two steps for optimization in the readfunction */
			tmp= text->lines.first;
			while (tmp) {
				writestruct(wd, DATA, "TextLine", 1, tmp);
				tmp= tmp->next;
			}

			tmp= text->lines.first;
			while (tmp) {
				writedata(wd, DATA, tmp->len+1, tmp->line);
				tmp= tmp->next;
			}

			/* write markers */
			mrk= text->markers.first;
			while (mrk) {
				writestruct(wd, DATA, "TextMarker", 1, mrk);
				mrk= mrk->next;
			}
		}


		text= text->id.next;
	}

	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_speakers(WriteData *wd, ListBase *idbase)
{
	Speaker *spk;

	spk= idbase->first;
	while (spk) {
		if (spk->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_SPK, "Speaker", 1, spk);
			if (spk->id.properties) IDP_WriteProperty(spk->id.properties, wd);

			if (spk->adt) write_animdata(wd, spk->adt);
		}
		spk= spk->id.next;
	}
}

static void write_sounds(WriteData *wd, ListBase *idbase)
{
	bSound *sound;

	PackedFile * pf;

	sound= idbase->first;
	while (sound) {
		if (sound->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_SO, "bSound", 1, sound);
			if (sound->id.properties) IDP_WriteProperty(sound->id.properties, wd);

			if (sound->packedfile) {
				pf = sound->packedfile;
				writestruct(wd, DATA, "PackedFile", 1, pf);
				writedata(wd, DATA, pf->size, pf->data);
			}
		}
		sound= sound->id.next;
	}

	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

static void write_groups(WriteData *wd, ListBase *idbase)
{
	Group *group;
	GroupObject *go;

	for (group= idbase->first; group; group= group->id.next) {
		if (group->id.us>0 || wd->current) {
			/* write LibData */
			writestruct(wd, ID_GR, "Group", 1, group);
			if (group->id.properties) IDP_WriteProperty(group->id.properties, wd);

			go= group->gobject.first;
			while (go) {
				writestruct(wd, DATA, "GroupObject", 1, go);
				go= go->next;
			}
		}
	}
}

static void write_nodetrees(WriteData *wd, ListBase *idbase)
{
	bNodeTree *ntree;
	
	for (ntree=idbase->first; ntree; ntree= ntree->id.next) {
		if (ntree->id.us>0 || wd->current) {
			writestruct(wd, ID_NT, "bNodeTree", 1, ntree);
			write_nodetree(wd, ntree);
			
			if (ntree->id.properties) IDP_WriteProperty(ntree->id.properties, wd);
			
			if (ntree->adt) write_animdata(wd, ntree->adt);
		}
	}
}

static void write_brushes(WriteData *wd, ListBase *idbase)
{
	Brush *brush;
	
	for (brush=idbase->first; brush; brush= brush->id.next) {
		if (brush->id.us>0 || wd->current) {
			writestruct(wd, ID_BR, "Brush", 1, brush);
			if (brush->id.properties) IDP_WriteProperty(brush->id.properties, wd);
			
			writestruct(wd, DATA, "MTex", 1, &brush->mtex);
			
			if (brush->curve)
				write_curvemapping(wd, brush->curve);
		}
	}
}

static void write_scripts(WriteData *wd, ListBase *idbase)
{
	Script *script;
	
	for (script=idbase->first; script; script= script->id.next) {
		if (script->id.us>0 || wd->current) {
			writestruct(wd, ID_SCRIPT, "Script", 1, script);
			if (script->id.properties) IDP_WriteProperty(script->id.properties, wd);
		}
	}
}

static void write_movieTracks(WriteData *wd, ListBase *tracks)
{
	MovieTrackingTrack *track;

	track= tracks->first;
	while (track) {
		writestruct(wd, DATA, "MovieTrackingTrack", 1, track);

		if (track->markers)
			writestruct(wd, DATA, "MovieTrackingMarker", track->markersnr, track->markers);

		track= track->next;
	}
}

static void write_movieReconstruction(WriteData *wd, MovieTrackingReconstruction *reconstruction)
{
	if (reconstruction->camnr)
		writestruct(wd, DATA, "MovieReconstructedCamera", reconstruction->camnr, reconstruction->cameras);
}

static void write_movieclips(WriteData *wd, ListBase *idbase)
{
	MovieClip *clip;

	clip= idbase->first;
	while (clip) {
		if (clip->id.us>0 || wd->current) {
			MovieTracking *tracking= &clip->tracking;
			MovieTrackingObject *object;
			writestruct(wd, ID_MC, "MovieClip", 1, clip);

			if (clip->adt)
				write_animdata(wd, clip->adt);

			write_movieTracks(wd, &tracking->tracks);
			write_movieReconstruction(wd, &tracking->reconstruction);

			object= tracking->objects.first;
			while (object) {
				writestruct(wd, DATA, "MovieTrackingObject", 1, object);

				write_movieTracks(wd, &object->tracks);
				write_movieReconstruction(wd, &object->reconstruction);

				object= object->next;
			}
		}

		clip= clip->id.next;
	}

	/* flush helps the compression for undo-save */
	mywrite(wd, MYWRITE_FLUSH, 0);
}

/* context is usually defined by WM, two cases where no WM is available:
 * - for forward compatibility, curscreen has to be saved
 * - for undofile, curscene needs to be saved */
static void write_global(WriteData *wd, int fileflags, Main *mainvar)
{
	FileGlobal fg;
	bScreen *screen;
	char subvstr[8];
	
	/* prevent mem checkers from complaining */
	fg.pads= fg.pad= 0;
	memset(fg.filename, 0, sizeof(fg.filename));

	current_screen_compat(mainvar, &screen);

	/* XXX still remap G */
	fg.curscreen= screen;
	fg.curscene= screen->scene;
	fg.displaymode= G.displaymode;
	fg.winpos= G.winpos;

	/* prevent to save this, is not good convention, and feature with concerns... */
	fg.fileflags= (fileflags & ~(G_FILE_NO_UI|G_FILE_RELATIVE_REMAP|G_FILE_MESH_COMPAT));

	fg.globalf= G.f;
	BLI_strncpy(fg.filename, mainvar->name, sizeof(fg.filename));

	sprintf(subvstr, "%4d", BLENDER_SUBVERSION);
	memcpy(fg.subvstr, subvstr, 4);
	
	fg.subversion= BLENDER_SUBVERSION;
	fg.minversion= BLENDER_MINVERSION;
	fg.minsubversion= BLENDER_MINSUBVERSION;
#ifdef WITH_BUILDINFO
	{
		extern char build_rev[];
		fg.revision= atoi(build_rev);
	}
#else
	fg.revision= 0;
#endif
	writestruct(wd, GLOB, "FileGlobal", 1, &fg);
}

/* preview image, first 2 values are width and height
 * second are an RGBA image (unsigned char)
 * note, this uses 'TEST' since new types will segfault on file load for older blender versions.
 */
static void write_thumb(WriteData *wd, const int *img)
{
	if (img)
		writedata(wd, TEST, (2 + img[0] * img[1]) * sizeof(int), img);
}

/* if MemFile * there's filesave to memory */
static int write_file_handle(Main *mainvar, int handle, MemFile *compare, MemFile *current, 
                             int write_user_block, int write_flags, const int *thumb)
{
	BHead bhead;
	ListBase mainlist;
	char buf[16];
	WriteData *wd;

	blo_split_main(&mainlist, mainvar);

	wd= bgnwrite(handle, compare, current);

#ifdef USE_BMESH_SAVE_AS_COMPAT
	wd->use_mesh_compat = (write_flags & G_FILE_MESH_COMPAT) != 0;
#endif

	sprintf(buf, "BLENDER%c%c%.3d", (sizeof(void*)==8)?'-':'_', (ENDIAN_ORDER==B_ENDIAN)?'V':'v', BLENDER_VERSION);
	mywrite(wd, buf, 12);

	write_renderinfo(wd, mainvar);
	write_thumb(wd, thumb);
	write_global(wd, write_flags, mainvar);

	/* no UI save in undo */
	if (current==NULL) {
		write_windowmanagers(wd, &mainvar->wm);
		write_screens  (wd, &mainvar->screen);
	}
	write_movieclips (wd, &mainvar->movieclip);
	write_scenes   (wd, &mainvar->scene);
	write_curves   (wd, &mainvar->curve);
	write_mballs   (wd, &mainvar->mball);
	write_images   (wd, &mainvar->image);
	write_cameras  (wd, &mainvar->camera);
	write_lamps    (wd, &mainvar->lamp);
	write_lattices (wd, &mainvar->latt);
	write_vfonts   (wd, &mainvar->vfont);
	write_keys     (wd, &mainvar->key);
	write_worlds   (wd, &mainvar->world);
	write_texts    (wd, &mainvar->text);
	write_speakers (wd, &mainvar->speaker);
	write_sounds   (wd, &mainvar->sound);
	write_groups   (wd, &mainvar->group);
	write_armatures(wd, &mainvar->armature);
	write_actions  (wd, &mainvar->action);
	write_objects  (wd, &mainvar->object);
	write_materials(wd, &mainvar->mat);
	write_textures (wd, &mainvar->tex);
	write_meshs    (wd, &mainvar->mesh);
	write_particlesettings(wd, &mainvar->particle);
	write_nodetrees(wd, &mainvar->nodetree);
	write_brushes  (wd, &mainvar->brush);
	write_scripts  (wd, &mainvar->script);
	write_gpencils (wd, &mainvar->gpencil);
	write_libraries(wd,  mainvar->next);

	if (write_user_block) {
		write_userdef(wd);
	}
							
	/* dna as last, because (to be implemented) test for which structs are written */
	writedata(wd, DNA1, wd->sdna->datalen, wd->sdna->data);

	/* end of file */
	memset(&bhead, 0, sizeof(BHead));
	bhead.code= ENDB;
	mywrite(wd, &bhead, sizeof(BHead));

	blo_join_main(&mainlist);

	return endwrite(wd);
}

/* do reverse file history: .blend1 -> .blend2, .blend -> .blend1 */
/* return: success(0), failure(1) */
static int do_history(const char *name, ReportList *reports)
{
	char tempname1[FILE_MAX], tempname2[FILE_MAX];
	int hisnr= U.versions;
	
	if (U.versions==0) return 0;
	if (strlen(name)<2) {
		BKE_report(reports, RPT_ERROR, "Unable to make version backup: filename too short");
		return 1;
	}
		
	while (hisnr > 1) {
		BLI_snprintf(tempname1, sizeof(tempname1), "%s%d", name, hisnr-1);
		BLI_snprintf(tempname2, sizeof(tempname2), "%s%d", name, hisnr);
	
		if (BLI_rename(tempname1, tempname2)) {
			BKE_report(reports, RPT_ERROR, "Unable to make version backup");
			return 1;
		}	
		hisnr--;
	}

	/* is needed when hisnr==1 */
	BLI_snprintf(tempname1, sizeof(tempname1), "%s%d", name, hisnr);

	if (BLI_rename(name, tempname1)) {
		BKE_report(reports, RPT_ERROR, "Unable to make version backup");
		return 1;
	}

	return 0;
}

/* return: success (1) */
int BLO_write_file(Main *mainvar, const char *filepath, int write_flags, ReportList *reports, const int *thumb)
{
	char userfilename[FILE_MAX];
	char tempname[FILE_MAX+1];
	int file, err, write_user_block;

	/* open temporary file, so we preserve the original in case we crash */
	BLI_snprintf(tempname, sizeof(tempname), "%s@", filepath);

	file = BLI_open(tempname,O_BINARY+O_WRONLY+O_CREAT+O_TRUNC, 0666);
	if (file == -1) {
		BKE_reportf(reports, RPT_ERROR, "Can't open file %s for writing: %s.", tempname, strerror(errno));
		return 0;
	}

	/* remapping of relative paths to new file location */
	if (write_flags & G_FILE_RELATIVE_REMAP) {
		char dir1[FILE_MAX];
		char dir2[FILE_MAX];
		BLI_split_dir_part(filepath, dir1, sizeof(dir1));
		BLI_split_dir_part(mainvar->name, dir2, sizeof(dir2));

		/* just in case there is some subtle difference */
		BLI_cleanup_dir(mainvar->name, dir1);
		BLI_cleanup_dir(mainvar->name, dir2);

		if (BLI_path_cmp(dir1, dir2)==0) {
			write_flags &= ~G_FILE_RELATIVE_REMAP;
		}
		else {
			if (G.relbase_valid) {
				/* blend may not have been saved before. Tn this case
				 * we should not have any relative paths, but if there
				 * is somehow, an invalid or empty G.main->name it will
				 * print an error, don't try make the absolute in this case. */
				makeFilesAbsolute(mainvar, G.main->name, NULL);
			}
		}
	}

	BLI_make_file_string(G.main->name, userfilename, BLI_get_folder_create(BLENDER_USER_CONFIG, NULL), BLENDER_STARTUP_FILE);
	write_user_block= (BLI_path_cmp(filepath, userfilename) == 0);

	if (write_flags & G_FILE_RELATIVE_REMAP)
		makeFilesRelative(mainvar, filepath, NULL); /* note, making relative to something OTHER then G.main->name */

	/* actual file writing */
	err= write_file_handle(mainvar, file, NULL,NULL, write_user_block, write_flags, thumb);
	close(file);

	if (err) {
		BKE_report(reports, RPT_ERROR, strerror(errno));
		remove(tempname);

		return 0;
	}

	/* file save to temporary file was successful */
	/* now do reverse file history (move .blend1 -> .blend2, .blend -> .blend1) */
	if (write_flags & G_FILE_HISTORY) { 
		int err_hist = do_history(filepath, reports);
		if (err_hist) {
			BKE_report(reports, RPT_ERROR, "Version backup failed. File saved with @");
			return 0;
		}
	}

	if (write_flags & G_FILE_COMPRESS) {
		/* compressed files have the same ending as regular files... only from 2.4!!! */
		char gzname[FILE_MAX+4];
		int ret;

		/* first write compressed to separate @.gz */
		BLI_snprintf(gzname, sizeof(gzname), "%s@.gz", filepath);
		ret = BLI_file_gzip(tempname, gzname);
		
		if (0==ret) {
			/* now rename to real file name, and delete temp @ file too */
			if (BLI_rename(gzname, filepath) != 0) {
				BKE_report(reports, RPT_ERROR, "Can't change old file. File saved with @.");
				return 0;
			}

			BLI_delete(tempname, 0, 0);
		}
		else if (-1==ret) {
			BKE_report(reports, RPT_ERROR, "Failed opening .gz file.");
			return 0;
		}
		else if (-2==ret) {
			BKE_report(reports, RPT_ERROR, "Failed opening .blend file for compression.");
			return 0;
		}
	}
	else if (BLI_rename(tempname, filepath) != 0) {
		BKE_report(reports, RPT_ERROR, "Can't change old file. File saved with @");
		return 0;
	}

	return 1;
}

/* return: success (1) */
int BLO_write_file_mem(Main *mainvar, MemFile *compare, MemFile *current, int write_flags)
{
	int err;

	err= write_file_handle(mainvar, 0, compare, current, 0, write_flags, NULL);
	
	if (err==0) return 1;
	return 0;
}
