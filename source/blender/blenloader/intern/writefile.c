/* writefile.c
 *
 * .blend file writing
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
- write FileGlobal (some global vars)
- write SDNA
- write USER if filename is ~/.B.blend
*/

/* for version 2.2+
Important to know is that 'streaming' has been added to files, for Blender Publisher
*/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h>
#else
#include "winsock2.h"
#include "BLI_winstuff.h"
#include <io.h>
#include <process.h> // for getpid
#endif

#include <math.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "nla.h" //  __NLA is defined

#include "DNA_packedFile_types.h"
#include "DNA_sdna_types.h"
#include "DNA_property_types.h"
#include "DNA_sensor_types.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"
#include "DNA_effect_types.h"
#include "DNA_object_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_lattice_types.h"
#include "DNA_armature_types.h"
#include "DNA_sequence_types.h"
#include "DNA_ika_types.h"
#include "DNA_group_types.h"
#include "DNA_oops_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_lamp_types.h"
#include "DNA_fileglobal_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_scene_types.h"
#include "DNA_constraint_types.h"
#include "DNA_listBase.h" /* for Listbase, the type of samples, ...*/
#include "DNA_action_types.h"
#include "DNA_nla_types.h"

#include "MEM_guardedalloc.h" // MEM_freeN 
#include "BLI_blenlib.h"
#include "BLI_linklist.h"

#include "BKE_action.h"
#include "BKE_utildefines.h" // for KNOTSU KNOTSV WHILE_SEQ END_SEQ defines
#include "BKE_bad_level_calls.h" // build_seqar (from WHILE_SEQ) free_oops error
#include "BKE_constraint.h"
#include "BKE_main.h" // G.main
#include "BKE_global.h" // for G
#include "BKE_screen.h" // for waitcursor
#include "BKE_packedFile.h" // for packAll
#include "BKE_library.h" // for  set_listbasepointers
#include "BKE_sound.h" /* ... and for samples */

#include "GEN_messaging.h"

#include "BLO_writefile.h"
#include "BLO_readfile.h"

#include "readfile.h"
#include "genfile.h"

/* *******  MYWRITE ********* */

#include "BLO_writeStreamGlue.h"

/***/

typedef struct {
	struct SDNA *sdna;

	int file;
	unsigned char *buf;
	
	int tot, count, error;
	
	int is_publisher;
	struct writeStreamGlueStruct *streamGlue;
} WriteData;

static WriteData *writedata_new(int file, int is_publisher)
{
	extern char DNAstr[];	/* DNA.c */
	extern int DNAlen;
	
	WriteData *wd= MEM_callocN(sizeof(*wd), "writedata");

		/* XXX, see note about this in readfile.c, remove
		 * once we have an xp lock - zr
		 */
	wd->sdna= dna_sdna_from_data(DNAstr, DNAlen, 0);
	
	wd->file= file;
	wd->is_publisher= is_publisher;

	wd->buf= MEM_mallocN(100000, "wd->buf");
	
	return wd;
}

static void writedata_do_write(WriteData *wd, void *mem, int memlen)
{
	if (wd->error) return;
	
	if (wd->is_publisher) {
		wd->error = writeStreamGlue(Global_streamGlueControl, &wd->streamGlue, mem, memlen, 0);
	} else {
		if (write(wd->file, mem, memlen) != memlen)
			wd->error= 1;
	}	
}

static void writedata_free(WriteData *wd) 
{
	dna_freestructDNA(wd->sdna);

	MEM_freeN(wd->buf);
	MEM_freeN(wd);
}

/***/

struct streamGlueControlStruct *Global_streamGlueControl;
int mywfile;

/**
 * Low level WRITE(2) wrapper that buffers data
 * @param adr Pointer to new chunk of data
 * @param len Length of new chunk of data
 * @warning Talks to other functions with global parameters
 */
	static void
mywrite(
	WriteData *wd, 
	void *adr,
	int len)
{
	if (wd->error) return;

	wd->tot+= len;

	if(len>50000) {
		if(wd->count) {
			writedata_do_write(wd, wd->buf, wd->count);
			wd->count= 0;
		}
		writedata_do_write(wd, adr, len);
		return;
	}
	if(len+wd->count>99999) {
		writedata_do_write(wd, wd->buf, wd->count);
		wd->count= 0; 
	}
	memcpy(&wd->buf[wd->count], adr, len);
	wd->count+= len;
}

/**
 * BeGiN initializer for mywrite
 * @param file File descriptor
 * @param write_flags Write parameters
 * @warning Talks to other functions with global parameters
 */
	static WriteData *
bgnwrite(
	int file, 
	int write_flags)
{
	int is_publisher= (write_flags & (G_FILE_COMPRESS | G_FILE_LOCK | G_FILE_SIGN | G_FILE_PUBLISH));
	WriteData *wd= writedata_new(file, is_publisher);
	
	if (is_publisher) {
		mywfile= file;
		wd->streamGlue = NULL;
		Global_streamGlueControl = streamGlueControlConstructor();
		streamGlueControlAppendAction(Global_streamGlueControl, DUMPFROMMEMORY);
		if (write_flags & G_FILE_COMPRESS) {
			streamGlueControlAppendAction(Global_streamGlueControl, DEFLATE);
		}
		if (write_flags & G_FILE_LOCK) {
			streamGlueControlAppendAction(Global_streamGlueControl, ENCRYPT);
		}
		if (write_flags & G_FILE_SIGN) {
			streamGlueControlAppendAction(Global_streamGlueControl, SIGN);
		}
		streamGlueControlAppendAction(Global_streamGlueControl, WRITEBLENFILE);
	}
	
	return wd;
}

/**
 * END the mywrite wrapper
 * @return 1 if write failed
 * @return unknown global variable otherwise
 * @warning Talks to other functions with global parameters
 */
	static int
endwrite(
	WriteData *wd)
{
	int err;
	
	if (wd->count) {
		writedata_do_write(wd, wd->buf, wd->count);
		wd->count= 0; 
	}
	if (wd->is_publisher) {
		writeStreamGlue(Global_streamGlueControl, &wd->streamGlue, NULL, 0, 1);
		streamGlueControlDestructor(Global_streamGlueControl);
		// final writestream error handling goes here
		if (wd->error) {
			int err = wd->error;
			int errFunction = BWS_GETFUNCTION(err);
			int errGeneric =  BWS_GETGENERR(err);
			int errSpecific = BWS_GETSPECERR(err);
			char *errFunctionStrings[] = {
				"",
				"The write stream",
				"The deflation",
				"The encryption",
				"The signing",
				"Writing the blendfile"
			};
			char *errGenericStrings[] = {
				"",
				"generated an out of memory error",
				"is not allowed in this version",
				"has problems with your key"
			};
			char *errWriteStreamGlueStrings[] = {
				"",
				"does not know how to proceed"
			};
			char *errDeflateStrings[] = {
				"",
				"bumped on a compress error"
			};
			char *errEncryptStrings[] = {
				"",
				"could not write the key",
				"bumped on an encrypt error"
			};
			char *errSignStrings[] = {
				"",
				"could not write the key",
				"failed"
			};
			char *errWriteBlenFileStrings[] = {
				"",
				"encountered problems writing the filedescription",
				"encountered problems writing the blendfile",
				"encountered problems writing one (or more) parameters"
			};
			char *errFunctionString= errFunctionStrings[errFunction];
			char *errExtraString= "";
			
			if (errGeneric)
			{
				errExtraString= errGenericStrings[errGeneric];
			}
			else if (errSpecific)
			{
				switch (errFunction)
				{
				case BWS_WRITESTREAMGLUE:
					errExtraString= errWriteStreamGlueStrings[errSpecific];
					break;
				case BWS_DEFLATE:
					errExtraString= errDeflateStrings[errSpecific];
					break;
				case BWS_ENCRYPT:
					errExtraString= errEncryptStrings[errSpecific];
					break;
				case BWS_SIGN:
					errExtraString= errSignStrings[errSpecific];
					break;
				case BWS_WRITEBLENFILE:
					errExtraString= errWriteBlenFileStrings[errSpecific];
					break;
				default:
					break;
				}
			}
			
				// call Blender error popup window
			error("%s %s", errFunctionString, errExtraString);
		}
	}
	
	err= wd->error;
	writedata_free(wd);
	
	return err;
}

/* ********** WRITE FILE ****************** */

static void writestruct(WriteData *wd, int filecode, char *structname, int nr, void *adr)
{
	BHead bh;
	short *sp;
	
	if(adr==0 || nr==0) return;
	
	/* init BHead */
	bh.code= filecode;
	bh.old= adr;
	bh.nr= nr;
	
	bh.SDNAnr= dna_findstruct_nr(wd->sdna, structname);
	if(bh.SDNAnr== -1) {
		printf("error: can't find SDNA code %s\n", structname);
		return;
	}
	sp= wd->sdna->structs[bh.SDNAnr];
	
	bh.len= nr*wd->sdna->typelens[sp[0]];
	
	if(bh.len==0) return;
		
	mywrite(wd, &bh, sizeof(BHead));
	mywrite(wd, adr, bh.len);
}

static void writedata(WriteData *wd, int filecode, int len, void *adr)	/* do not use for structs */
{
	BHead bh;
	
	if(adr==0) return;
	if(len==0) return;
	
	len+= 3;
	len-= ( len % 4);
	
	/* init BHead */
	bh.code= filecode;
	bh.old= adr;
	bh.nr= 1;
	bh.SDNAnr= 0;	
	bh.len= len;
	
	mywrite(wd, &bh, sizeof(BHead));
	if(len) mywrite(wd, adr, len);
}

static void write_scriptlink(WriteData *wd, ScriptLink *slink)
{
	writedata(wd, DATA, sizeof(void *)*slink->totscript, slink->scripts);	
	writedata(wd, DATA, sizeof(short)*slink->totscript, slink->flag);	
}

static void write_renderinfo(WriteData *wd)		/* for renderdaemon */
{
	Scene *sce;
	int data[8];
	
	sce= G.main->scene.first;
	while(sce) {
		if(sce->id.lib==0  && ( sce==G.scene || (sce->r.scemode & R_BG_RENDER)) ) {
			data[0]= sce->r.sfra;
			data[1]= sce->r.efra;
			
			strncpy((char *)(data+2), sce->id.name+2, 23);
			
			writedata(wd, REND, 32, data);
		}
		sce= sce->id.next;
	}
}

static void write_userdef(WriteData *wd)
{
	writestruct(wd, USER, "UserDef", 1, &U);
}

static void write_effects(WriteData *wd, ListBase *lb)
{
	Effect *eff;
	
	eff= lb->first;
	while(eff) {
		
		switch(eff->type) {
		case EFF_BUILD:
			writestruct(wd, DATA, "BuildEff", 1, eff);
			break;	
		case EFF_PARTICLE:
			writestruct(wd, DATA, "PartEff", 1, eff);
			break;	
		case EFF_WAVE:
			writestruct(wd, DATA, "WaveEff", 1, eff);
			break;	
		default:
			writedata(wd, DATA, MEM_allocN_len(eff), eff);
		}
		
		eff= eff->next;
	}
}

static void write_properties(WriteData *wd, ListBase *lb)
{
	bProperty *prop;
	
	prop= lb->first;
	while(prop) {
		writestruct(wd, DATA, "bProperty", 1, prop);
	
		if(prop->poin && prop->poin != &prop->data) 
			writedata(wd, DATA, MEM_allocN_len(prop->poin), prop->poin);
	
		prop= prop->next;
	}
}

static void write_sensors(WriteData *wd, ListBase *lb)
{
	bSensor *sens;
	
	sens= lb->first;
	while(sens) {
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
	while(cont) {
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
	while(act) {
		writestruct(wd, DATA, "bActuator", 1, act);
	
		switch(act->type) {
		case ACT_ACTION:
			writestruct(wd, DATA, "bActionActuator", 1, act->data);
			break;
		case ACT_SOUND:
			writestruct(wd, DATA, "bSoundActuator", 1, act->data);
			break;
		case ACT_CD:
			writestruct(wd, DATA, "bCDActuator", 1, act->data);
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
		default:
			; /* error: don't know how to write this file */
		}
	
		act= act->next;
	}
}

static void write_nlastrips(WriteData *wd, ListBase *nlabase)
{
	bActionStrip *strip;

	for (strip=nlabase->first; strip; strip=strip->next)
		writestruct(wd, DATA, "bActionStrip", 1, strip);
}

static void write_constraints(WriteData *wd, ListBase *conlist)
{
	bConstraint *con;
	
	for (con=conlist->first; con; con=con->next) {
		/* Write the specific data */
		switch (con->type) {
		case CONSTRAINT_TYPE_NULL:
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			writestruct(wd, DATA, "bTrackToConstraint", 1, con->data);
			break;
		case CONSTRAINT_TYPE_KINEMATIC:
			writestruct(wd, DATA, "bKinematicConstraint", 1, con->data);
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			writestruct(wd, DATA, "bRotateLikeConstraint", 1, con->data);
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			writestruct(wd, DATA, "bLocateLikeConstraint", 1, con->data);
			break;
		case CONSTRAINT_TYPE_ACTION:
			writestruct(wd, DATA, "bActionConstraint", 1, con->data);
			break;
		default:
			break;
		}
		/* Write the constraint */
		writestruct(wd, DATA, "bConstraint", 1, con);
	}
}

static void write_pose(WriteData *wd, bPose *pose)
{
	bPoseChannel	*chan;
	
	/* Write each channel */
	
	if (!pose)
		return;
	
	// Write channels
	for (chan=pose->chanbase.first; chan; chan=chan->next) {
		write_constraints(wd, &chan->constraints);
		writestruct(wd, DATA, "bPoseChannel", 1, chan);
	}
	
	// Write this pose 
	writestruct(wd, DATA, "bPose", 1, pose);
}

static void write_defgroups(WriteData *wd, ListBase *defbase)
{
	bDeformGroup	*defgroup;
	
	for (defgroup=defbase->first; defgroup; defgroup=defgroup->next)
		writestruct(wd, DATA, "bDeformGroup", 1, defgroup);
}

static void write_constraint_channels(WriteData *wd, ListBase *chanbase)
{
	bConstraintChannel *chan;

	for (chan = chanbase->first; chan; chan=chan->next)
		writestruct(wd, DATA, "bConstraintChannel", 1, chan);
	
}

static void write_objects(WriteData *wd, ListBase *idbase)
{
	Object *ob;
	
	ob= idbase->first;
	while(ob) {
		if(ob->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_OB, "Object", 1, ob);
			
			/* direct data */
			writedata(wd, DATA, sizeof(void *)*ob->totcol, ob->mat);
			write_effects(wd, &ob->effect);
			write_properties(wd, &ob->prop);
			write_sensors(wd, &ob->sensors);
			write_controllers(wd, &ob->controllers);
			write_actuators(wd, &ob->actuators);
			write_scriptlink(wd, &ob->scriptlink);
			write_pose(wd, ob->pose);
			write_defgroups(wd, &ob->defbase);
			write_constraints(wd, &ob->constraints);
			write_constraint_channels(wd, &ob->constraintChannels);
			write_nlastrips(wd, &ob->nlastrips);
		}
		ob= ob->id.next;
	}
}


static void write_vfonts(WriteData *wd, ListBase *idbase)
{
	VFont *vf;
	PackedFile * pf;
	
	vf= idbase->first;
	while(vf) {
		if(vf->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_VF, "VFont", 1, vf);
		
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

static void write_ipos(WriteData *wd, ListBase *idbase)
{
	Ipo *ipo;
	IpoCurve *icu;
	
	ipo= idbase->first;
	while(ipo) {
		if(ipo->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_IP, "Ipo", 1, ipo);
		
			/* direct data */
			icu= ipo->curve.first;
			while(icu) {
				writestruct(wd, DATA, "IpoCurve", 1, icu);
				icu= icu->next;
			}
	
			icu= ipo->curve.first;
			while(icu) {
				if(icu->bezt)  writestruct(wd, DATA, "BezTriple", icu->totvert, icu->bezt);
				if(icu->bp)  writestruct(wd, DATA, "BPoint", icu->totvert, icu->bp);
				icu= icu->next;
			}
		}
		
		ipo= ipo->id.next;
	}
}

static void write_keys(WriteData *wd, ListBase *idbase)
{
	Key *key;
	KeyBlock *kb;
	
	key= idbase->first;
	while(key) {
		if(key->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_KE, "Key", 1, key);
		
			/* direct data */
			kb= key->block.first;
			while(kb) {
				writestruct(wd, DATA, "KeyBlock", 1, kb);
				if(kb->data) writedata(wd, DATA, kb->totelem*key->elemsize, kb->data);
				kb= kb->next;
			}
		}
		
		key= key->id.next;
	}
}

static void write_cameras(WriteData *wd, ListBase *idbase)
{
	Camera *cam;
	
	cam= idbase->first;
	while(cam) {
		if(cam->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_CA, "Camera", 1, cam);
		
			/* direct data */
			write_scriptlink(wd, &cam->scriptlink);
		}
		
		cam= cam->id.next;
	}
}

static void write_mballs(WriteData *wd, ListBase *idbase)
{
	MetaBall *mb;
	MetaElem *ml;
	
	mb= idbase->first;
	while(mb) {
		if(mb->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_MB, "MetaBall", 1, mb);
			
			/* direct data */
			writedata(wd, DATA, sizeof(void *)*mb->totcol, mb->mat);
			
			ml= mb->elems.first;
			while(ml) {
				writestruct(wd, DATA, "MetaElem", 1, ml);
				ml= ml->next;
			}
		}
		mb= mb->id.next;
	}
}

static void write_curves(WriteData *wd, ListBase *idbase)
{
	Curve *cu;
	Nurb *nu;
	
	cu= idbase->first;
	while(cu) {
		if(cu->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_CU, "Curve", 1, cu);
			
			/* direct data */
			writedata(wd, DATA, sizeof(void *)*cu->totcol, cu->mat);
			
			if(cu->vfont) {
				writedata(wd, DATA, cu->len+1, cu->str);
			}
			else {
				/* is also the order of reading */
				nu= cu->nurb.first;
				while(nu) {
					writestruct(wd, DATA, "Nurb", 1, nu);
					nu= nu->next;
				}
				nu= cu->nurb.first;
				while(nu) {
					if( (nu->type & 7)==CU_BEZIER) 
						writestruct(wd, DATA, "BezTriple", nu->pntsu, nu->bezt);
					else {
						writestruct(wd, DATA, "BPoint", nu->pntsu*nu->pntsv, nu->bp);
						if(nu->knotsu) writedata(wd, DATA, KNOTSU(nu)*sizeof(float), nu->knotsu);
						if(nu->knotsv) writedata(wd, DATA, KNOTSV(nu)*sizeof(float), nu->knotsv);
					}
					nu= nu->next;
				}
			}
		}
		cu= cu->id.next;
	}
}

static void write_dverts(WriteData *wd, int count, MDeformVert *dvlist)
{
	int	i;
		
	/* Write the dvert list */
	writestruct(wd, DATA, "MDeformVert", count, dvlist);
	
	/* Write deformation data for each dvert */
	if (dvlist) {
		for (i=0; i<count; i++) {
			if (dvlist[i].dw)
				writestruct(wd, DATA, "MDeformWeight", dvlist[i].totweight, dvlist[i].dw); 
		}
	}
}

static void write_meshs(WriteData *wd, ListBase *idbase)
{
	Mesh *mesh;
	
	mesh= idbase->first;
	while(mesh) {
		if(mesh->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_ME, "Mesh", 1, mesh);
			
			/* direct data */
			writedata(wd, DATA, sizeof(void *)*mesh->totcol, mesh->mat);
			writestruct(wd, DATA, "MVert", mesh->totvert, mesh->mvert);
			write_dverts(wd, mesh->totvert, mesh->dvert);
			writestruct(wd, DATA, "MFace", mesh->totface, mesh->mface);
			writestruct(wd, DATA, "TFace", mesh->totface, mesh->tface);
			writestruct(wd, DATA, "MCol", 4*mesh->totface, mesh->mcol);
			writestruct(wd, DATA, "MSticky", mesh->totvert, mesh->msticky);
	
		}
		mesh= mesh->id.next;
	}
}

static void write_images(WriteData *wd, ListBase *idbase)
{
	Image *ima;
	PackedFile * pf;
	
	ima= idbase->first;
	while(ima) {
		if(ima->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_IM, "Image", 1, ima);
	
			if (ima->packedfile) {
				pf = ima->packedfile;
				writestruct(wd, DATA, "PackedFile", 1, pf);
				writedata(wd, DATA, pf->size, pf->data);
			}
		}
		ima= ima->id.next;
	}
}

static void write_textures(WriteData *wd, ListBase *idbase)
{
	Tex *tex;
	
	tex= idbase->first;
	while(tex) {
		if(tex->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_TE, "Tex", 1, tex);
			
			/* direct data */
			if(tex->plugin) writestruct(wd, DATA, "PluginTex", 1, tex->plugin);
			if(tex->coba) writestruct(wd, DATA, "ColorBand", 1, tex->coba);
			if(tex->env) writestruct(wd, DATA, "EnvMap", 1, tex->env);
		}
		tex= tex->id.next;
	}
}

static void write_materials(WriteData *wd, ListBase *idbase)
{
	Material *ma;
	int a;
	
	ma= idbase->first;
	while(ma) {
		if(ma->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_MA, "Material", 1, ma);
			
			for(a=0; a<8; a++) {
				if(ma->mtex[a]) writestruct(wd, DATA, "MTex", 1, ma->mtex[a]);
			}
	
			write_scriptlink(wd, &ma->scriptlink);
		}
		ma= ma->id.next;
	}
}

static void write_worlds(WriteData *wd, ListBase *idbase)
{
	World *wrld;
	int a;
	
	wrld= idbase->first;
	while(wrld) {
		if(wrld->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_WO, "World", 1, wrld);
	
			for(a=0; a<8; a++) {
				if(wrld->mtex[a]) writestruct(wd, DATA, "MTex", 1, wrld->mtex[a]);
			}
	
			write_scriptlink(wd, &wrld->scriptlink);
		}
		wrld= wrld->id.next;
	}
}

static void write_lamps(WriteData *wd, ListBase *idbase)
{
	Lamp *la;
	int a;
	
	la= idbase->first;
	while(la) {
		if(la->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_LA, "Lamp", 1, la);
		
			/* direct data */
			for(a=0; a<8; a++) {
				if(la->mtex[a]) writestruct(wd, DATA, "MTex", 1, la->mtex[a]);
			}
	
			write_scriptlink(wd, &la->scriptlink);
		}
		la= la->id.next;
	}
}

static void write_lattices(WriteData *wd, ListBase *idbase)
{
	Lattice *lt;
	
	lt= idbase->first;
	while(lt) {
		if(lt->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_LT, "Lattice", 1, lt);
		
			/* direct data */
			writestruct(wd, DATA, "BPoint", lt->pntsu*lt->pntsv*lt->pntsw, lt->def);
		}
		lt= lt->id.next;
	}
}

static void write_ikas(WriteData *wd, ListBase *idbase)
{
	Ika *ika;
	Limb *li;
	
	ika= idbase->first;
	while(ika) {
		if(ika->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_IK, "Ika", 1, ika);
			
			/* direct data */
			li= ika->limbbase.first;
			while(li) {
				writestruct(wd, DATA, "Limb", 1, li);
				li= li->next;
			}
			
			writestruct(wd, DATA, "Deform", ika->totdef, ika->def);
		}
		ika= ika->id.next;
	}
}

static void write_scenes(WriteData *wd, ListBase *scebase)
{
	Scene *sce;
	Base *base;
	Editing *ed;
	Sequence *seq;
	Strip *strip;
	
	sce= scebase->first;
	while(sce) {
		/* write LibData */
		writestruct(wd, ID_SCE, "Scene", 1, sce);
		
		/* direct data */
		base= sce->base.first;
		while(base) {
			writestruct(wd, DATA, "Base", 1, base);
			base= base->next;
		}
		
		writestruct(wd, DATA, "Radio", 1, sce->radio);
		writestruct(wd, DATA, "FreeCamera", 1, sce->fcam);
		
		ed= sce->ed;
		if(ed) {
			writestruct(wd, DATA, "Editing", 1, ed);
	
			/* reset write flags too */
			WHILE_SEQ(&ed->seqbase) {
				if(seq->strip) seq->strip->done= 0;
				writestruct(wd, DATA, "Sequence", 1, seq);
			}
			END_SEQ
			
			WHILE_SEQ(&ed->seqbase) {
				if(seq->strip && seq->strip->done==0) {
					/* write strip with 'done' at 0 because readfile */
					
					if(seq->plugin) writestruct(wd, DATA, "PluginSeq", 1, seq->plugin);
					
					strip= seq->strip;
					writestruct(wd, DATA, "Strip", 1, strip);
					
					if(seq->type==SEQ_IMAGE) 
						writestruct(wd, DATA, "StripElem", strip->len, strip->stripdata);
					else if(seq->type==SEQ_MOVIE)
						writestruct(wd, DATA, "StripElem", 1, strip->stripdata);
						
					strip->done= 1;
				}
			}
			END_SEQ
		}
	
		write_scriptlink(wd, &sce->scriptlink);

		if (sce->r.avicodecdata) {
			writestruct(wd, DATA, "AviCodecData", 1, sce->r.avicodecdata);
			if (sce->r.avicodecdata->lpFormat) writedata(wd, DATA, sce->r.avicodecdata->cbFormat, sce->r.avicodecdata->lpFormat);
			if (sce->r.avicodecdata->lpParms) writedata(wd, DATA, sce->r.avicodecdata->cbParms, sce->r.avicodecdata->lpParms);
		}
		
		sce= sce->id.next;
	}
}

static void write_screens(WriteData *wd, ListBase *scrbase)
{
	bScreen *sc;
	ScrArea *sa;
	ScrVert *sv;
	ScrEdge *se;
	
	sc= scrbase->first;
	while(sc) {
		/* write LibData */
		writestruct(wd, ID_SCR, "Screen", 1, sc);
		
		/* direct data */
		sv= sc->vertbase.first;
		while(sv) {
			writestruct(wd, DATA, "ScrVert", 1, sv);
			sv= sv->next;
		}
	
		se= sc->edgebase.first;
		while(se) {
			writestruct(wd, DATA, "ScrEdge", 1, se);
			se= se->next;
		}
	
		sa= sc->areabase.first;
		while(sa) {
			SpaceLink *sl;

			writestruct(wd, DATA, "ScrArea", 1, sa);
			
			sl= sa->spacedata.first;
			while(sl) {
				if(sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;
					writestruct(wd, DATA, "View3D", 1, v3d);
					if(v3d->bgpic) writestruct(wd, DATA, "BGpic", 1, v3d->bgpic);
					if(v3d->localvd) writestruct(wd, DATA, "View3D", 1, v3d->localvd);
				}
				else if(sl->spacetype==SPACE_IPO) {
					writestruct(wd, DATA, "SpaceIpo", 1, sl);
				}
				else if(sl->spacetype==SPACE_BUTS) {
					writestruct(wd, DATA, "SpaceButs", 1, sl);
				}
				else if(sl->spacetype==SPACE_FILE) {
					writestruct(wd, DATA, "SpaceFile", 1, sl);
				}
				else if(sl->spacetype==SPACE_SEQ) {
					writestruct(wd, DATA, "SpaceSeq", 1, sl);
				}
				else if(sl->spacetype==SPACE_OOPS) {
					SpaceOops *so= (SpaceOops *)sl;
					Oops *oops;
					
					/* cleanup */
					oops= so->oops.first;
					while(oops) {
						Oops *oopsn= oops->next;
						if(oops->id==0) {
							BLI_remlink(&so->oops, oops);
							free_oops(oops);
						}
						oops= oopsn;
					}
	
					/* ater cleanup, because of listbase! */
					writestruct(wd, DATA, "SpaceOops", 1, so);
	
					oops= so->oops.first;
					while(oops) {
						writestruct(wd, DATA, "Oops", 1, oops);
						oops= oops->next;
					}
				}
				else if(sl->spacetype==SPACE_IMAGE) {
					writestruct(wd, DATA, "SpaceImage", 1, sl);
				}
				else if(sl->spacetype==SPACE_IMASEL) {
					writestruct(wd, DATA, "SpaceImaSel", 1, sl);
				}
				else if(sl->spacetype==SPACE_TEXT) {
					writestruct(wd, DATA, "SpaceText", 1, sl);
				}
				else if(sl->spacetype==SPACE_ACTION) {
					writestruct(wd, DATA, "SpaceAction", 1, sl);
				}
				else if(sl->spacetype==SPACE_SOUND) {
					writestruct(wd, DATA, "SpaceSound", 1, sl);
				}
				else if(sl->spacetype==SPACE_NLA){
					writestruct(wd, DATA, "SpaceNla", 1, sl);
				}
				sl= sl->next;
			}
			
			sa= sa->next;
		}
	
		sc= sc->id.next;
	}
}

static void write_libraries(WriteData *wd, Main *main)
{
	ListBase *lbarray[30];
	ID *id;
	int a, tot, foundone;
	
	while(main) {
	
		a=tot= set_listbasepointers(main, lbarray);
	
		/* test: is lib being used */
		foundone= 0;
		while(tot--) {
			id= lbarray[tot]->first;
			while(id) {
				if(id->us>0 && (id->flag & LIB_EXTERN)) {
					foundone= 1;
					break;
				}
				id= id->next;
			}
			if(foundone) break;
		}
		
		if(foundone) {	
			writestruct(wd, ID_LI, "Library", 1, main->curlib);
	
			while(a--) {
				id= lbarray[a]->first;
				while(id) {
					if(id->us>0 && (id->flag & LIB_EXTERN)) {
						
						writestruct(wd, ID_ID, "ID", 1, id);
					}
					id= id->next;
				}
			}
		}
		
		main= main->next;
	}
}

static void write_bone(WriteData *wd, Bone* bone)
{
	Bone*	cbone;
	
//	write_constraints(wd, &bone->constraints);	
	
	// Write this bone 
	writestruct(wd, DATA, "Bone", 1, bone);
	
	// Write Children 
	cbone= bone->childbase.first;
	while(cbone) {
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
		if (arm->id.us>0) {
			writestruct(wd, ID_AR, "bArmature", 1, arm);
	
			/* Direct data */
			bone= arm->bonebase.first;
			while(bone) {
				write_bone(wd, bone);
				bone=bone->next;
			}
		}
		arm=arm->id.next;
	}
}

static void write_actions(WriteData *wd, ListBase *idbase)
{
	bAction			*act;
	bActionChannel	*chan;
	act=idbase->first;
	while (act) {
		if (act->id.us>0) {
			writestruct(wd, ID_AC, "bAction", 1, act);
			
			for (chan=act->chanbase.first; chan; chan=chan->next) {
				writestruct(wd, DATA, "bActionChannel", 1, chan);
				write_constraint_channels(wd, &chan->constraintChannels);
			}
		}
		act=act->id.next;
	}
}

static void write_texts(WriteData *wd, ListBase *idbase)
{
	Text *text;
	TextLine *tmp;
	
	text= idbase->first;
	while(text) {
		if ( (text->flags & TXT_ISMEM) && (text->flags & TXT_ISEXT)) text->flags &= ~TXT_ISEXT;
		
		/* write LibData */
		writestruct(wd, ID_TXT, "Text", 1, text);
		if(text->name) writedata(wd, DATA, strlen(text->name)+1, text->name);
	
		if(!(text->flags & TXT_ISEXT)) {			
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
		}
		text= text->id.next;
	}
}

static void write_sounds(WriteData *wd, ListBase *idbase)
{
	bSound *sound;
	bSample *sample;
	
	PackedFile * pf;
	
	// set all samples to unsaved status
	
	sample = samples->first;
	while (sample) {
		sample->flags |= SAMPLE_NEEDS_SAVE;
		sample = sample->id.next;
	}
	
	sound= idbase->first;
	while(sound) {
		if(sound->id.us>0) {
			// do we need to save the packedfile as well ?
			sample = sound->sample;
			if (sample) {
				if (sample->flags & SAMPLE_NEEDS_SAVE) {
					sound->newpackedfile = sample->packedfile;
					sample->flags &= ~SAMPLE_NEEDS_SAVE;
				} else {
					sound->newpackedfile = NULL;
				}
			}
	
			/* write LibData */
			writestruct(wd, ID_SO, "bSound", 1, sound);
	
			if (sound->newpackedfile) {
				pf = sound->newpackedfile;
				writestruct(wd, DATA, "PackedFile", 1, pf);
				writedata(wd, DATA, pf->size, pf->data);
			}
	
			if (sample) {
				sound->newpackedfile = sample->packedfile;
			}
		}
		sound= sound->id.next;
	}
}

static void write_groups(WriteData *wd, ListBase *idbase)
{
	Group *group;
	GroupKey *gk;
	GroupObject *go;
	ObjectKey *ok;
	
	group= idbase->first;
	while(group) {
		if(group->id.us>0) {
			/* write LibData */
			writestruct(wd, ID_GR, "Group", 1, group);
			
			gk= group->gkey.first;
			while(gk) {
				writestruct(wd, DATA, "GroupKey", 1, gk);
				gk= gk->next;
			}
	
			go= group->gobject.first;
			while(go) {
				writestruct(wd, DATA, "GroupObject", 1, go);
				go= go->next;
			}
			go= group->gobject.first;
			while(go) {
				ok= go->okey.first;
				while(ok) {
					writestruct(wd, DATA, "ObjectKey", 1, ok);
					ok= ok->next;
				}
				go= go->next;
			}
			
		}
		group= group->id.next;
	}
}

static void write_global(WriteData *wd)
{
	FileGlobal fg;
	
	fg.curscreen= G.curscreen;
	fg.displaymode= R.displaymode;
	fg.winpos= R.winpos;
	fg.fileflags= G.fileflags;
	
	writestruct(wd, GLOB, "FileGlobal", 1, &fg);
}

static int write_file_handle(int handle, int write_user_block, int write_flags) 
{
	ListBase mainlist;
	char buf[13];
	WriteData *wd;
	int data;
	
	mainlist.first= mainlist.last= G.main;
	G.main->next= NULL;
	
	blo_split_main(&mainlist);
	
	wd= bgnwrite(handle, write_flags);

	sprintf(buf, "BLENDER%c%c%.3d", (sizeof(void*)==8)?'-':'_', (G.order==B_ENDIAN)?'V':'v', G.version);
	mywrite(wd, buf, 12);
	
	write_renderinfo(wd);
	
	write_screens  (wd, &G.main->screen);
	write_scenes   (wd, &G.main->scene);
	write_objects  (wd, &G.main->object);
	write_meshs    (wd, &G.main->mesh);
	write_curves   (wd, &G.main->curve);
	write_mballs   (wd, &G.main->mball);
	write_materials(wd, &G.main->mat);
	write_textures (wd, &G.main->tex);
	write_images   (wd, &G.main->image);
	write_cameras  (wd, &G.main->camera);
	write_lamps    (wd, &G.main->lamp);
	write_lattices (wd, &G.main->latt);
	write_ikas     (wd, &G.main->ika);
	write_vfonts   (wd, &G.main->vfont);
	write_ipos     (wd, &G.main->ipo);
	write_keys     (wd, &G.main->key);
	write_worlds   (wd, &G.main->world);
	write_texts    (wd, &G.main->text);
	write_sounds   (wd, &G.main->sound);
	write_groups   (wd, &G.main->group);
	write_armatures(wd, &G.main->armature);
	write_actions  (wd, &G.main->action);
	write_libraries(wd,  G.main->next);

	write_global(wd);
	if (write_user_block) {
		write_userdef(wd);
	}
	
	/* dna as last, because (to be implemented) test for which structs are written */
	writedata(wd, DNA1, wd->sdna->datalen, wd->sdna->data);
	
	data= ENDB;
	mywrite(wd, &data, 4);
	
	data= 0;
	mywrite(wd, &data, 4);
	
	blo_join_main(&mainlist);
	G.main= mainlist.first;
	
	return endwrite(wd);
}

int BLO_write_file(char *dir, int write_flags, char **error_r)
{
	char userfilename[FILE_MAXDIR+FILE_MAXFILE];
	char tempname[FILE_MAXDIR+FILE_MAXFILE];
	int file, fout, write_user_block;
	
	sprintf(tempname, "%s@", dir);

	file = open(tempname,O_BINARY+O_WRONLY+O_CREAT+O_TRUNC, 0666);
	if(file == -1) {
		*error_r= "Unable to open";
		return 0;
	}
	
	BLI_make_file_string(G.sce, userfilename, BLI_gethome(), ".B.blend");
	write_user_block= BLI_streq(dir, userfilename);

	fout= write_file_handle(file, write_user_block, write_flags);
	close(file);
	
	if(!fout) {
		if(BLI_rename(tempname, dir) < 0) {
			*error_r= "Can't change old file. File saved with @";
			return 0;
		}
	} else {
		remove(tempname);

		*error_r= "Not enough diskspace";
		return 0;
	}
	
	return 1;
}

	/* Runtime writing */

#ifdef WIN32
#define PATHSEPERATOR		"\\"
#else
#define PATHSEPERATOR		"/"
#endif	

static char *get_install_dir(void) {
	extern char bprogname[];
	char *tmpname = BLI_strdup(bprogname);
	char *cut;

#ifdef __APPLE__
	cut = strstr(tmpname, ".app");
	if (cut) cut[0] = 0;
#endif

	cut = BLI_last_slash(tmpname);
	
	if (cut) {
		cut[0] = 0;
		return tmpname;
	} else {
		MEM_freeN(tmpname);
		return NULL;
	}
}

static char *get_runtime_path(char *exename) {
	char *installpath= get_install_dir();
	
	if (!installpath) {
		return NULL;
	} else {
		char *path= MEM_mallocN(strlen(installpath)+strlen(PATHSEPERATOR)+strlen(exename)+1, "runtimepath");
		strcpy(path, installpath);
		strcat(path, PATHSEPERATOR);
		strcat(path, exename);
	
		MEM_freeN(installpath);
	
		return path;
	}
}

#ifdef __APPLE__

static int recursive_copy_runtime(char *outname, char *exename, char **cause_r) {
	char *cause = NULL, *runtime = get_runtime_path(exename);
	char command[2 * (FILE_MAXDIR+FILE_MAXFILE) + 32];
	int progfd = -1;
	
	if (!runtime) {
		cause= "Unable to find runtime";
		goto cleanup;
	}
	
	progfd= open(runtime, O_BINARY|O_RDONLY, 0);
	if (progfd==-1) {
		cause= "Unable to find runtime";
		goto cleanup;
	}

	sprintf(command, "/bin/cp -R %s %s", runtime, outname);
	if (system(command) == -1) {
		cause = "Couldn't copy runtime";
	}

cleanup:
	if (progfd!=-1)
		close(progfd);
	if (runtime)
		MEM_freeN(runtime);
	
	if (cause) {
		*cause_r= cause;
		return 0;
	} else
		return 1;
}

void BLO_write_runtime(char *file, char *exename) {
	char gamename[FILE_MAXDIR+FILE_MAXFILE];
	int outfd = -1;
	char *cause= NULL;
	
	// remove existing file / bundle
	BLI_delete(file, NULL, TRUE);
	
	if (!recursive_copy_runtime(file, exename, &cause))
		goto cleanup;
		
	strcpy(gamename, file);
	strcat(gamename, "/Contents/Resources/game.blend");
	
	outfd= open(gamename, O_BINARY|O_WRONLY|O_CREAT|O_TRUNC, 0777);
	if (outfd != -1) {

		/* Ensure runtime's are built with Publisher files */
		write_file_handle(outfd, 0, G.fileflags|G_FILE_PUBLISH);
		
		if (write(outfd, " ", 1) != 1) {
			cause= "Unable to write to output file";
			goto cleanup;
		}
	} else {
		cause = "Unable to open blenderfile";
	}
	
cleanup:
	if (outfd!=-1)
		close(outfd);
	
	if (cause)
		error("Unable to make runtime: %s", cause);
}

#else /* !__APPLE__ */

static int handle_append_runtime(int handle, char *exename, char **cause_r) {
	char *cause= NULL, *runtime= get_runtime_path(exename);
	unsigned char buf[1024];
	int count, progfd= -1;
	
	if (!runtime) {
		cause= "Unable to find runtime";
		goto cleanup;
	}
	
	progfd= open(runtime, O_BINARY|O_RDONLY, 0);
	if (progfd==-1) {
		cause= "Unable to find runtime";
		goto cleanup;
	}

	while ((count= read(progfd, buf, sizeof(buf)))>0) {
		if (write(handle, buf, count)!=count) {
			cause= "Unable to write to output file";
			goto cleanup;
		}
	}
	
cleanup:
	if (progfd!=-1)
		close(progfd);
	if (runtime)
		MEM_freeN(runtime);
	
	if (cause) {
		*cause_r= cause;
		return 0;
	} else
		return 1;
}

static int handle_write_msb_int(int handle, int i) {
	unsigned char buf[4];
	buf[0]= (i>>24)&0xFF;
	buf[1]= (i>>16)&0xFF;
	buf[2]= (i>>8)&0xFF;
	buf[3]= (i>>0)&0xFF;
	
	return (write(handle, buf, 4)==4);
}

void BLO_write_runtime(char *file, char *exename) {
	int outfd= open(file, O_BINARY|O_WRONLY|O_CREAT|O_TRUNC, 0777);
	char *cause= NULL;
	int datastart;
	
	if (!outfd) {
		cause= "Unable to open output file";
		goto cleanup;
	}
	if (!handle_append_runtime(outfd, exename, &cause))
		goto cleanup;
		
	datastart= lseek(outfd, 0, SEEK_CUR);

		/* Ensure runtime's are built with Publisher files */
	write_file_handle(outfd, 0, G.fileflags|G_FILE_PUBLISH);
	
	if (!handle_write_msb_int(outfd, datastart) || (write(outfd, "BRUNTIME", 8)!=8)) {
		cause= "Unable to write to output file";
		goto cleanup;
	}
	
cleanup:
	if (outfd!=-1)
		close(outfd);
	
	if (cause)
		error("Unable to make runtime: %s", cause);
}

#endif /* !__APPLE__ */
