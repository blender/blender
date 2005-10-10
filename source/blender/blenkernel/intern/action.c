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
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>	/* for NULL */

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_constraint.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "nla.h"
#include "render.h"

/* *********************** NOTE ON POSE AND ACTION **********************

  - Pose is the local (object level) component of armature. The current
    object pose is saved in files, and (will be) is presorted for dependency
  - Actions have fewer (or other) channels, and write data to a Pose
  - Currently ob->pose data is controlled in where_is_pose only. The (recalc)
    event system takes care of calling that
  - The NLA system (here too) uses Poses as interpolation format for Actions
  - Therefore we assume poses to be static, and duplicates of poses have channels in
    same order, for quick interpolation reasons

  ****************************** (ton) ************************************ */

/* ***************** Library data level operations on action ************** */

void make_local_action(bAction *act)
{
	Object *ob;
	bAction *actn;
	int local=0, lib=0;
	
	if(act->id.lib==0) return;
	if(act->id.us==1) {
		act->id.lib= 0;
		act->id.flag= LIB_LOCAL;
		new_id(0, (ID *)act, 0);
		return;
	}
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->action==act) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
	
	if(local && lib==0) {
		act->id.lib= 0;
		act->id.flag= LIB_LOCAL;
		new_id(0, (ID *)act, 0);
	}
	else if(local && lib) {
		actn= copy_action(act);
		actn->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->action==act) {
				
				if(ob->id.lib==0) {
					ob->action = actn;
					actn->id.us++;
					act->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}


void free_action(bAction *act)
{
	bActionChannel *chan;
	
	/* Free channels */
	for (chan=act->chanbase.first; chan; chan=chan->next){
		if (chan->ipo)
			chan->ipo->id.us--;
		free_constraint_channels(&chan->constraintChannels);
	}
	
	if (act->chanbase.first)
		BLI_freelistN (&act->chanbase);
}

bAction* copy_action(bAction *src)
{
	bAction *dst = NULL;
	bActionChannel *dchan, *schan;
	
	if(!src) return NULL;
	
	dst= copy_libblock(src);
	duplicatelist(&(dst->chanbase), &(src->chanbase));
	
	for (dchan=dst->chanbase.first, schan=src->chanbase.first; dchan; dchan=dchan->next, schan=schan->next){
		dchan->ipo = copy_ipo(dchan->ipo);
		copy_constraint_channels(&dchan->constraintChannels, &schan->constraintChannels);
	}
	dst->id.flag |= LIB_FAKEUSER;
	dst->id.us++;
	return dst;
}



/* ************************ Pose channels *************** */

/* usually used within a loop, so we got a N^2 slowdown */
bPoseChannel *get_pose_channel(const bPose *pose, const char *name)
{
	bPoseChannel *chan;

	if(pose==NULL) return NULL;
	
	for (chan=pose->chanbase.first; chan; chan=chan->next) {
		if(chan->name[0] == name[0])
			if (!strcmp (chan->name, name))
				return chan;
	}

	return NULL;
}

/* Use with care, not on Armature poses but for temporal ones */
/* (currently used for action constraints and in rebuild_pose) */
bPoseChannel *verify_pose_channel(bPose* pose, const char* name)
{
	bPoseChannel *chan;
	
	if (!pose){
		return NULL;
	}
	
	/*      See if this channel exists */
	for (chan=pose->chanbase.first; chan; chan=chan->next){
		if (!strcmp (name, chan->name))
			return chan;
	}
	
	/* If not, create it and add it */
	chan = MEM_callocN(sizeof(bPoseChannel), "verifyPoseChannel");
	
	strcpy (chan->name, name);
	/* init vars to prevent mat errors */
	chan->quat[0] = 1.0F;
	chan->size[0] = chan->size[1] = chan->size[2] = 1.0F;
	
	chan->limitmin[0]= chan->limitmin[1]= chan->limitmin[2]= -180.0f;
	chan->limitmax[0]= chan->limitmax[1]= chan->limitmax[2]= 180.0f;
	chan->stiffness[0]= chan->stiffness[1]= chan->stiffness[2]= 0.0f;
	
	BLI_addtail (&pose->chanbase, chan);
	
	return chan;
}


/* dst should be freed already, makes entire duplicate */
void copy_pose(bPose **dst, bPose *src, int copycon)
{
	bPose *outPose;
	bPoseChannel	*pchan;
	ListBase listb;
	
	if (!src){
		*dst=NULL;
		return;
	}
	
	outPose= MEM_callocN(sizeof(bPose), "pose");
	
	duplicatelist (&outPose->chanbase, &src->chanbase);
	
	if (copycon) {
		for (pchan=outPose->chanbase.first; pchan; pchan=pchan->next) {
			copy_constraints(&listb, &pchan->constraints);  // copy_constraints NULLs listb
			pchan->constraints= listb;
		}
	}
	
	*dst=outPose;
}

void free_pose_channels(bPose *pose) 
{
	bPoseChannel *chan;
	
	if (pose->chanbase.first){
		for (chan = pose->chanbase.first; chan; chan=chan->next){
			free_constraints(&chan->constraints);
		}
		BLI_freelistN (&pose->chanbase);
	}
}

static void copy_pose_channel_data(bPoseChannel *pchan, const bPoseChannel *chan)
{
	bConstraint *pcon, *con;
	
	VECCOPY(pchan->loc, chan->loc);
	VECCOPY(pchan->size, chan->size);
	QUATCOPY(pchan->quat, chan->quat);
	pchan->flag= chan->flag;
	
	con= chan->constraints.first;
	for(pcon= pchan->constraints.first; pcon; pcon= pcon->next)
		pcon->enforce= con->enforce;
}

/* checks for IK constraint, can do more constraints flags later */
/* pose should be entirely OK */
void update_pose_constraint_flags(bPose *pose)
{
	bPoseChannel *pchan, *parchan;
	bConstraint *con;
	
	/* clear */
	for (pchan = pose->chanbase.first; pchan; pchan=pchan->next) {
		pchan->constflag= 0;
	}
	/* detect */
	for (pchan = pose->chanbase.first; pchan; pchan=pchan->next) {
		for(con= pchan->constraints.first; con; con= con->next) {
			if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
				bKinematicConstraint *data = (bKinematicConstraint*)con->data;
				
				pchan->constflag |= PCHAN_HAS_IK;
				/* negative rootbone = recalc rootbone index. used in do_versions */
				if(data->rootbone<0) {
					data->rootbone= 0;
					
					if(data->flag & CONSTRAINT_IK_TIP) parchan= pchan;
					else parchan= pchan->parent;
					
					while(parchan) {
						data->rootbone++;
						if((parchan->bone->flag & BONE_CONNECTED)==0)
							break;
						parchan= parchan->parent;
					}
				}
			}
			else pchan->constflag |= PCHAN_HAS_CONST;
		}
	}
}


/* ************************ END Pose channels *************** */

/* ************************ Action channels *************** */


bActionChannel *get_action_channel(bAction *act, const char *name)
{
	bActionChannel *chan;
	
	if (!act)
		return NULL;
	
	for (chan = act->chanbase.first; chan; chan=chan->next){
		if (!strcmp (chan->name, name))
			return chan;
	}
	
	return NULL;
}

/* returns existing channel, or adds new one. In latter case it doesnt activate it, context is required for that*/
bActionChannel *verify_action_channel(bAction *act, const char *name)
{
	bActionChannel *chan;
	
	chan= get_action_channel(act, name);
	if(chan==NULL) {
		if (!chan) {
			chan = MEM_callocN (sizeof(bActionChannel), "actionChannel");
			strcpy (chan->name, name);
			BLI_addtail (&act->chanbase, chan);
		}
	}
	return chan;
}

/* ************************ Blending with NLA *************** */


/* Only allowed for Poses with identical channels */
void blend_poses(bPose *dst, const bPose *src, float srcweight, short mode)
{
	bPoseChannel *dchan;
	const bPoseChannel *schan;
	bConstraint *dcon, *scon;
	float	dquat[4], squat[4];
	float dstweight;
	int i;
	
	switch (mode){
	case POSE_BLEND:
		dstweight = 1.0F - srcweight;
		break;
	case POSE_ADD:
		dstweight = 1.0F;
		break;
	default :
		dstweight = 1.0F;
	}
	
	schan= src->chanbase.first;
	for (dchan = dst->chanbase.first; dchan; dchan=dchan->next, schan= schan->next){
		if (schan->flag & (POSE_ROT|POSE_LOC|POSE_SIZE)) {
			/* replaced quat->matrix->quat conversion with decent quaternion interpol (ton) */
			
			/* Do the transformation blend */
			if (schan->flag & POSE_ROT) {
				QUATCOPY(dquat, dchan->quat);
				QUATCOPY(squat, schan->quat);
				if(mode==POSE_BLEND)
					QuatInterpol(dchan->quat, dquat, squat, srcweight);
				else
					QuatAdd(dchan->quat, dquat, squat, srcweight);
				
				NormalQuat (dchan->quat);
			}

			for (i=0; i<3; i++){
				if (schan->flag & POSE_LOC)
					dchan->loc[i] = (dchan->loc[i]*dstweight) + (schan->loc[i]*srcweight);
				if (schan->flag & POSE_SIZE)
					dchan->size[i] = 1.0f + ((dchan->size[i]-1.0f)*dstweight) + ((schan->size[i]-1.0f)*srcweight);
			}
			dchan->flag |= schan->flag;
		}
		for(dcon= dchan->constraints.first, scon= schan->constraints.first; dcon && scon; dcon= dcon->next, scon= scon->next) {
			/* no 'add' option for constraint blending */
			dcon->enforce= dcon->enforce*(1.0f-srcweight) + scon->enforce*srcweight;
		}
	}
}


float calc_action_start(const bAction *act)
{
	const bActionChannel *chan;
	const IpoCurve	*icu;
	float size=999999999.0f;
	int	i;
	int	foundvert=0;
	const bConstraintChannel *conchan;


	if (!act)
		return 0;

	for (chan=act->chanbase.first; chan; chan=chan->next) {
		if(chan->ipo) {
			for (icu=chan->ipo->curve.first; icu; icu=icu->next) {
				for (i=0; i<icu->totvert; i++){
					size = MIN2 (size, icu->bezt[i].vec[1][0]);
					foundvert=1;
				}
			}
		}
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next) {
			if(conchan->ipo) {
				for (icu=conchan->ipo->curve.first; icu; icu=icu->next) {
					for (i=0; i<icu->totvert; i++){
						size = MIN2 (size, icu->bezt[i].vec[1][0]);
						foundvert=1;
					}
				}
			}
		}
	}
	
	if (!foundvert)
		return 0;
	else
		return size;
}

float calc_action_end(const bAction *act)
{
	const bActionChannel	*chan;
	const bConstraintChannel *conchan;
	const IpoCurve		*icu;
	float size=0;
	int	i;

	if (!act)
		return 0;

	for (chan=act->chanbase.first; chan; chan=chan->next) {
		if(chan->ipo) {
			for (icu=chan->ipo->curve.first; icu; icu=icu->next)
				for (i=0; i<icu->totvert; i++)
					size = MAX2 (size, icu->bezt[i].vec[1][0]);
		}
		
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
			if(conchan->ipo) {
				for (icu=conchan->ipo->curve.first; icu; icu=icu->next)
					for (i=0; i<icu->totvert; i++)
						size = MAX2 (size, icu->bezt[i].vec[1][0]);
			}
		}
	}
	return size;
}

/* Copy the data from the action-pose (src) into the pose */
/* both args are assumed to be valid */
/* exported to game engine */
void extract_pose_from_pose(bPose *pose, const bPose *src)
{
	const bPoseChannel *schan;
	bPoseChannel *pchan= pose->chanbase.first;

	for (schan=src->chanbase.first; schan; schan=schan->next, pchan= pchan->next) {
		copy_pose_channel_data(pchan, schan);
	}
}

/* Pose should exist, can have any number of channels too (used for constraint) */
void extract_pose_from_action(bPose *pose, bAction *act, float ctime) 
{
	bActionChannel *achan;
	bPoseChannel	*pchan;
	Ipo				*ipo;

	if (!act)
		return;
	if (!pose)
		return;
	
	/* Copy the data from the action into the pose */
	for (pchan= pose->chanbase.first; pchan; pchan=pchan->next) {
		achan= get_action_channel(act, pchan->name);
		pchan->flag= 0;
		if(achan) {
			ipo = achan->ipo;
			if (ipo) {
				
				/* Evaluates and sets the internal ipo value */
				calc_ipo(ipo, ctime);
				/* This call also sets the pchan flags */
				execute_action_ipo(achan, pchan);
				
				do_constraint_channels(&pchan->constraints, &achan->constraintChannels, ctime);
			}
		}
	}
}

/* for do_all_pose_actions, clears the pose */
static void rest_pose(bPose *pose)
{
	bPoseChannel *chan;
	int i;
	
	if (!pose)
		return;
	
	for (chan=pose->chanbase.first; chan; chan=chan->next){
		for (i=0; i<3; i++){
			chan->loc[i]=0.0;
			chan->quat[i+1]=0.0;
			chan->size[i]=1.0;
		}
		chan->quat[0]=1.0;
		
		chan->flag =0;
	}
}

/* ********** NLA with non-poses works with ipo channels ********** */

typedef struct NlaIpoChannel {
	struct NlaIpoChannel *next, *prev;
	float val;
	void *poin;
	int type;
} NlaIpoChannel;

static void extract_ipochannels_from_action(ListBase *lb, ID *id, bAction *act, char *name, float ctime)
{
	bActionChannel *achan= get_action_channel(act, name);
	IpoCurve *icu;
	NlaIpoChannel *nic;
	
	if(achan==NULL) return;
	
	if(achan->ipo) {
		calc_ipo(achan->ipo, ctime);
		
		for(icu= achan->ipo->curve.first; icu; icu= icu->next) {
			
			nic= MEM_callocN(sizeof(NlaIpoChannel), "NlaIpoChannel");
			BLI_addtail(lb, nic);
			nic->val= icu->curval;
			nic->poin= get_ipo_poin(id, icu, &nic->type);
		}
	}
	
	/* constraint channels only for objects */
	if(GS(id->name)==ID_OB) {
		Object *ob= (Object *)id;
		bConstraint *con;
		bConstraintChannel *conchan;
		
		for (con=ob->constraints.first; con; con=con->next) {
			conchan = get_constraint_channel(&achan->constraintChannels, con->name);
			
			if(conchan && conchan->ipo) {
				calc_ipo(conchan->ipo, ctime);

				icu= conchan->ipo->curve.first;	// only one ipo now
				if(icu) {
					nic= MEM_callocN(sizeof(NlaIpoChannel), "NlaIpoChannel constr");
					BLI_addtail(lb, nic);
					nic->val= icu->curval;
					nic->poin= &con->enforce;
					nic->type= IPO_FLOAT;
				}
			}
		}
	}
}

static NlaIpoChannel *find_nla_ipochannel(ListBase *lb, void *poin)
{
	NlaIpoChannel *nic;
	
	if(poin) {
		for(nic= lb->first; nic; nic= nic->next) {
			if(nic->poin==poin)
				return nic;
		}
	}
	return NULL;
}


static void blend_ipochannels(ListBase *dst, ListBase *src, float srcweight, int mode)
{
	NlaIpoChannel *snic, *dnic, *next;
	float dstweight;
	
	switch (mode){
		case POSE_BLEND:
			dstweight = 1.0F - srcweight;
			break;
		case POSE_ADD:
			dstweight = 1.0F;
			break;
		default :
			dstweight = 1.0F;
	}
	
	for(snic= src->first; snic; snic= next) {
		next= snic->next;
		
		dnic= find_nla_ipochannel(dst, snic->poin);
		if(dnic==NULL) {
			/* remove from src list, and insert in dest */
			BLI_remlink(src, snic);
			BLI_addtail(dst, snic);
		}
		else {
			/* we do the blend */
			dnic->val= dstweight*dnic->val + srcweight*snic->val;
		}
	}
}

static void execute_ipochannels(Object *ob, ListBase *lb)
{
	NlaIpoChannel *nic;
	
	for(nic= lb->first; nic; nic= nic->next) {
		if(nic->poin) write_ipo_poin(nic->poin, nic->type, nic->val);
	}
}


/* ************** time ****************** */

static bActionStrip *get_active_strip(Object *ob)
{
	bActionStrip *strip;
	
	if(ob->action==NULL)
		return NULL;
	
	for (strip=ob->nlastrips.first; strip; strip=strip->next)
		if(strip->flag & ACTSTRIP_ACTIVE)
			break;
	
	if(strip && strip->act==ob->action)
		return strip;
	return NULL;
}

/* non clipped mapping of strip */
static float get_actionstrip_frame(bActionStrip *strip, float cframe)
{
	float length, actlength, repeat;
	
	if (strip->flag & ACTSTRIP_USESTRIDE)
		repeat= 1.0f;
	else
		repeat= strip->repeat;
	
	length = strip->end-strip->start;
	if(length==0.0f)
		length= 1.0f;
	actlength = strip->actend-strip->actstart;
	
	return repeat*actlength*(cframe - strip->start)/length + strip->actstart;
}

/* if the conditions match, it converts current time to strip time */
float get_action_frame(Object *ob, float cframe)
{
	bActionStrip *strip= get_active_strip(ob);
	
	if(strip)
		return get_actionstrip_frame(strip, cframe);
	return cframe;
}

/* this now only used for repeating cycles, to enable fields and blur. */
/* the whole time control in blender needs serious thinking... */
static float nla_time(float cfra, float unit)
{
	extern float bluroffs;	// bad construct, borrowed from object.c for now
	
	/* 2nd field */
	if(R.flag & R_SEC_FIELD) {
		if(R.r.mode & R_FIELDSTILL); else cfra+= 0.5f*unit;
	}
	
	/* motion blur */
	cfra+= unit*bluroffs;
		
	/* global time */
	cfra*= G.scene->r.framelen;	


	/* decide later... */
//	if(no_speed_curve==0) if(ob && ob->ipo) cfra= calc_ipo_time(ob->ipo, cfra);

	return cfra;
}

/* ************** do the action ************ */

static void do_nla(Object *ob, int blocktype)
{
	bPose *tpose= NULL;
	ListBase tchanbase={NULL, NULL}, chanbase={NULL, NULL};
	bActionStrip *strip;
	float striptime, frametime, length, actlength;
	float blendfac, stripframe;
	int	doit;
	
	if(blocktype==ID_AR) {
		copy_pose(&tpose, ob->pose, 1);
		rest_pose(ob->pose);		// potentially destroying current not-keyed pose
	}
	
	for (strip=ob->nlastrips.first; strip; strip=strip->next){
		doit=0;
		
		if (strip->act){	/* so theres an action */
			
			/* Determine if the current frame is within the strip's range */
			length = strip->end-strip->start;
			actlength = strip->actend-strip->actstart;
			striptime = (G.scene->r.cfra-(strip->start)) / length;
			stripframe = (G.scene->r.cfra-(strip->start)) ;
			
			if (striptime>=0.0){
				
				if(blocktype==ID_AR) 
					rest_pose(tpose);
				
				/* Handle path */
				if (strip->flag & ACTSTRIP_USESTRIDE){
					if (ob->parent && ob->parent->type==OB_CURVE){
						Curve *cu = ob->parent->data;
						float ctime, pdist;
						
						if (cu->flag & CU_PATH){
							/* Ensure we have a valid path */
							if(cu->path==NULL || cu->path->data==NULL) makeDispListCurveTypes(ob->parent, 0);
							if(cu->path) {
								
								/* Find the position on the path */
								ctime= bsystem_time(ob, ob->parent, (float)G.scene->r.cfra, 0.0);
								
								if(calc_ipo_spec(cu->ipo, CU_SPEED, &ctime)==0) {
									ctime /= cu->pathlen;
									CLAMP(ctime, 0.0, 1.0);
								}
								pdist = ctime*cu->path->totdist;
								
								if (strip->stridelen)
									striptime = pdist / strip->stridelen;
								else
									striptime = 0;
								
								striptime = (float)fmod (striptime, 1.0);
								
								frametime = (striptime * actlength) + strip->actstart;
								
								if(blocktype==ID_AR)
									extract_pose_from_action (tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
								else if(blocktype==ID_OB)
									extract_ipochannels_from_action(&tchanbase, &ob->id, strip->act, "Object", bsystem_time(ob, 0, frametime, 0.0));
								
								doit=1;
							}
						}
					}
				}
				/* Handle repeat */
				else if (striptime < 1.0) {	
					/* Mod to repeat */
					striptime*=strip->repeat;
					striptime = (float)fmod (striptime, 1.0);
					
					frametime = (striptime * actlength) + strip->actstart;
					
					if(blocktype==ID_AR)
						extract_pose_from_action (tpose, strip->act, nla_time(frametime, (float)strip->repeat));
					else if(blocktype==ID_OB)
						extract_ipochannels_from_action(&tchanbase, &ob->id, strip->act, "Object", nla_time(frametime, (float)strip->repeat));
						
					doit=1;
				}
				/* Handle extend */
				else{
					if (strip->flag & ACTSTRIP_HOLDLASTFRAME){
						striptime = 1.0;
						frametime = (striptime * actlength) + strip->actstart;
						
						if(blocktype==ID_AR)
							extract_pose_from_action (tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
						else if(blocktype==ID_OB)
							extract_ipochannels_from_action(&tchanbase, &ob->id, strip->act, "Object", bsystem_time(ob, 0, frametime, 0.0));
						
						doit=1;
					}
				}
				
				/* Handle blendin & blendout */
				if (doit){
					/* Handle blendin */
					
					if (strip->blendin>0.0 && stripframe<=strip->blendin && G.scene->r.cfra>=strip->start){
						blendfac = stripframe/strip->blendin;
					}
					else if (strip->blendout>0.0 && stripframe>=(length-strip->blendout) && G.scene->r.cfra<=strip->end){
						blendfac = (length-stripframe)/(strip->blendout);
					}
					else
						blendfac = 1;
					
					if(blocktype==ID_AR) /* Blend this pose with the accumulated pose */
						blend_poses (ob->pose, tpose, blendfac, strip->mode);
					else {
						blend_ipochannels(&chanbase, &tchanbase, blendfac, strip->mode);
						BLI_freelistN(&tchanbase);
					}
				}
			}					
		}
	}
	
	if(blocktype==ID_OB) {
		execute_ipochannels(ob, &chanbase);
	}

	if (tpose){
		free_pose_channels(tpose);
		MEM_freeN(tpose);
	}
	if(chanbase.first)
		BLI_freelistN(&chanbase);
	
	
}

void do_all_pose_actions(Object *ob)
{

	// only to have safe calls from editor
	if(ob==NULL) return;
	if(ob->type!=OB_ARMATURE || ob->pose==NULL) return;

	if(ob->pose->flag & POSE_LOCKED) {  // no actions to execute while transform
		;
	}
	else if(ob->action && ((ob->nlaflag & OB_NLA_OVERRIDE)==0 || ob->nlastrips.first==NULL) ) {
		float cframe= (float) G.scene->r.cfra;
		
		cframe= get_action_frame(ob, cframe);
		
		extract_pose_from_action (ob->pose, ob->action, bsystem_time(ob, 0, cframe, 0.0));
	}
	else if(ob->nlastrips.first) {
		do_nla(ob, ID_AR);
	}
}

/* called from where_is_object */
void do_all_object_actions(Object *ob)
{
	if(ob==NULL) return;
	
	/* Do local action */
	if(ob->action && ((ob->nlaflag & OB_NLA_OVERRIDE)==0 || ob->nlastrips.first==NULL) ) {
		ListBase tchanbase= {NULL, NULL};
		float cframe= (float) G.scene->r.cfra;
		
		cframe= get_action_frame(ob, cframe);
		
		extract_ipochannels_from_action(&tchanbase, &ob->id, ob->action, "Object", bsystem_time(ob, 0, cframe, 0.0));
		if(tchanbase.first) {
			execute_ipochannels(ob, &tchanbase);
			BLI_freelistN(&tchanbase);
		}
	}
	else if(ob->nlastrips.first) {
		do_nla(ob, ID_OB);
	}
}

