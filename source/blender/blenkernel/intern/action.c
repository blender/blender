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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>	/* for NULL */

#include "MEM_guardedalloc.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_action.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_action_types.h"
#include "DNA_nla_types.h"

#include "BKE_blender.h"
#include "BKE_ipo.h"
#include "BKE_object.h"
#include "BKE_library.h"
#include "BKE_anim.h"
#include "BKE_armature.h"

#include "nla.h"

#include "BKE_constraint.h"
#include "DNA_constraint_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Local function prototypes */
static 
	void 
do_pose_constraint_channels(
	bPose *pose,
	bAction *act,
	float ctime
);

static 
	void 
get_constraint_influence_from_pose (
	bPose *dst,
	bPose *src
);

static 
	void 
blend_constraints(
	ListBase *dst, 
	const ListBase *src, 
	float srcweight, 
	short mode
);

static 
	void 
rest_pose (
	bPose *pose,
	int clearflag
);

/* Implementation */

	bPoseChannel *
get_pose_channel (
	const bPose *pose, 
	const char *name
){
	bPoseChannel *chan;

	for (chan=pose->chanbase.first; chan; chan=chan->next){
		if (!strcmp (chan->name, name))
			return chan;
	}

	return NULL;
}

static 
	void 
rest_pose (
	bPose *pose,
	int clearflag
){
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
		if (clearflag)
			chan->flag =0;
	}
}

static 
	void 
blend_constraints(
	ListBase *dst, 
	const ListBase *src, 
	float srcweight, 
	short mode
){
	bConstraint *dcon;
	const bConstraint *scon;
	float dstweight = 0;

	switch (mode){
	case POSE_BLEND:
		dstweight = 1.0F - srcweight;
		break;
	case POSE_ADD:
		dstweight = 1.0F;
		break;
	}

	/* Blend constraints */
	for (dcon=dst->first; dcon; dcon=dcon->next){
		for (scon = src->first; scon; scon=scon->next){
			if (!strcmp(scon->name, dcon->name))
				break;
		}
		
		if (scon){
			dcon->enforce = (dcon->enforce*dstweight) + (scon->enforce*srcweight);
			if (mode == POSE_BLEND)
				dcon->enforce/=2.0;
			
			if (dcon->enforce>1.0)
				dcon->enforce=1.0;
			if (dcon->enforce<0.0)
				dcon->enforce=0.0;

		}
	}
}

void blend_poses ( bPose *dst, const bPose *src, float srcweight, short mode)
{
	bPoseChannel *dchan;
	const bPoseChannel *schan;
	float	dquat[4], squat[4], mat[3][3];
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
	
	for (dchan = dst->chanbase.first; dchan; dchan=dchan->next){
		schan = get_pose_channel(src, dchan->name);
		if (schan){
			if (schan->flag & (POSE_ROT|POSE_LOC|POSE_SIZE)){
				
				/* Convert both quats to matrices and then back again. 
				 * This prevents interpolation problems
				 * This sucks because it is slow and stupid
				 */
				
				QuatToMat3(dchan->quat, mat);
				Mat3ToQuat(mat, dquat);
				QuatToMat3(schan->quat, mat);
				Mat3ToQuat(mat, squat);
				
				/* Do the transformation blend */
				for (i=0; i<3; i++){
					if (schan->flag & POSE_LOC)
						dchan->loc[i] = (dchan->loc[i]*dstweight) + (schan->loc[i]*srcweight);
					if (schan->flag & POSE_SIZE)
						dchan->size[i] = 1.0f + ((dchan->size[i]-1.0f)*dstweight) + ((schan->size[i]-1.0f)*srcweight);
					if (schan->flag & POSE_ROT)
						dchan->quat[i+1] = (dquat[i+1]*dstweight) + (squat[i+1]*srcweight);
				}
				
				/* Do one more iteration for the quaternions only and normalize the quaternion if needed */
				if (schan->flag & POSE_ROT)
					dchan->quat[0] = 1.0f + ((dquat[0]-1.0f)*dstweight) + ((squat[0]-1.0f)*srcweight);
				if (mode==POSE_BLEND)
					NormalQuat (dchan->quat);
				dchan->flag |= schan->flag;
			}
		}
	}
}

void clear_pose_constraint_status ( Object *ob)
{
	bPoseChannel *chan;

	if (!ob)
		return;
	if (!ob->pose)
		return;

	for (chan = ob->pose->chanbase.first; chan; chan=chan->next){
		chan->flag &= ~PCHAN_DONE;
	}
}

float calc_action_start (const bAction *act)
{
	const bActionChannel *chan;
	const IpoCurve	*icu;
	float size=999999999.0f;
	int	i;
	int	foundvert=0;
	const bConstraintChannel *conchan;


	if (!act)
		return 0;

	for (chan=act->chanbase.first; chan; chan=chan->next){
		for (icu=chan->ipo->curve.first; icu; icu=icu->next)
			for (i=0; i<icu->totvert; i++){
				size = MIN2 (size, icu->bezt[i].vec[1][0]);
				foundvert=1;
				
			}
			for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
				for (icu=conchan->ipo->curve.first; icu; icu=icu->next)
					for (i=0; i<icu->totvert; i++){
						size = MIN2 (size, icu->bezt[i].vec[1][0]);
						foundvert=1;
					}
			}
	}
	
	if (!foundvert)
		return 0;
	else
		return size;
}

float calc_action_end (const bAction *act)
{
	const bActionChannel	*chan;
	const bConstraintChannel *conchan;
	const IpoCurve		*icu;
	float size=0;
	int	i;

	if (!act)
		return 0;

	for (chan=act->chanbase.first; chan; chan=chan->next){
		for (icu=chan->ipo->curve.first; icu; icu=icu->next)
			for (i=0; i<icu->totvert; i++)
				size = MAX2 (size, icu->bezt[i].vec[1][0]);
			
			for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
				for (icu=conchan->ipo->curve.first; icu; icu=icu->next)
					for (i=0; i<icu->totvert; i++)
						size = MAX2 (size, icu->bezt[i].vec[1][0]);
			}
	}
	return size;
}

void verify_pose_channel (bPose* pose, const char* name) 
{
	bPoseChannel *chan;

	if (!pose){
		return;
	}

	/*	See if this channel exists */
	for (chan=pose->chanbase.first; chan; chan=chan->next){
		if (!strcmp (name, chan->name))
			return;
	}

	/* If not, create it and add it */
	chan = MEM_callocN(sizeof(bPoseChannel), "verifyPoseChannel");

	strcpy (chan->name, name);
	chan->loc[0] = chan->loc[1] = chan->loc[2] = 0.0F;
	chan->quat[1] = chan->quat[2] = chan->quat[3] = 0.0F; chan->quat[0] = 1.0F;
	chan->size[0] = chan->size[1] = chan->size[2] = 1.0F;

	chan->flag |= POSE_ROT|POSE_SIZE|POSE_LOC;

	BLI_addtail (&pose->chanbase, chan);
}

void get_pose_from_pose (bPose **pose, const bPose *src)
{
	const bPoseChannel	*pchan;
	bPoseChannel *newchan;

	if (!src)
		return;
	if (!pose)
		return;

	/* If there is no pose, create one */
	if (!*pose){
		*pose=MEM_callocN (sizeof(bPose), "pose");
	}

	/* Copy the data from the action into the pose */
	for (pchan=src->chanbase.first; pchan; pchan=pchan->next){
		newchan = copy_pose_channel(pchan);
		verify_pose_channel(*pose, pchan->name);
		set_pose_channel(*pose, newchan);
	}
}

static void get_constraint_influence_from_pose (bPose *dst, bPose *src)
{
	bConstraint *dcon, *scon;

	if (!src || !dst)
		return;

	for (dcon = dst->chanbase.first; dcon; dcon=dcon->next){
		for (scon=src->chanbase.first; scon; scon=scon->next){
			if (!strcmp(scon->name, dcon->name))
				break;
		}
		if (scon){
			dcon->enforce = scon->enforce;
		}
	}
}

/* If the pose does not exist, a new one is created */

void get_pose_from_action ( bPose **pose, bAction *act, float ctime) 
{
	bActionChannel *achan;
	bPoseChannel	*pchan;
	Ipo				*ipo;
	IpoCurve		*curve;
	

	if (!act)
		return;
	if (!pose)
		return;

	/* If there is no pose, create one */
	if (!*pose){
		*pose=MEM_callocN (sizeof(bPose), "pose");
	}

	/* Copy the data from the action into the pose */
	for (achan=act->chanbase.first; achan; achan=achan->next){
		act->achan= achan;

		ipo = achan->ipo;
		if (ipo){
			pchan=MEM_callocN (sizeof(bPoseChannel), "gpfa_poseChannel");
			strcpy (pchan->name, achan->name);

			act->pchan=pchan;
			/* Evaluates and sets the internal ipo value */
			calc_ipo(ipo, ctime);

			/* Set the pchan flags */
			for (curve = achan->ipo->curve.first; curve; curve=curve->next){
				/*	Skip empty curves */
				if (!curve->totvert)
					continue;

				switch (curve->adrcode){
				case AC_QUAT_X:
				case AC_QUAT_Y:
				case AC_QUAT_Z:
				case AC_QUAT_W:
					pchan->flag |= POSE_ROT;
					break;
				case AC_LOC_X:
				case AC_LOC_Y:
				case AC_LOC_Z:
					pchan->flag |= POSE_LOC;
					break;
				case AC_SIZE_X:
				case AC_SIZE_Y:
				case AC_SIZE_Z:
					pchan->flag |= POSE_SIZE;
					break;
				}
			}

			execute_ipo((ID*)act, achan->ipo);
		
			set_pose_channel(*pose, pchan);
		}
	}
}

void do_all_actions()
{
	Base *base;
	bPose *apose=NULL;
	bPose *tpose=NULL;
	Object *ob;
	bActionStrip *strip;
	int	doit;
	float striptime, frametime, length, actlength;
	float blendfac, stripframe;

	int set;

	/* NEW: current scene ob ipo's */
	base= G.scene->base.first;
	set= 0;

	while(base) {
		
		ob = base->object;

		/* Retrieve data from the NLA */
		if(ob->type==OB_ARMATURE){

			doit=0;

			/* Clear pose */
			if (apose){
				clear_pose(apose);
				MEM_freeN(apose);
			}
			/* Clear pose */
			if (tpose){
				clear_pose(tpose);
				MEM_freeN(tpose);
			}

			copy_pose(&apose, ob->pose, 1);
			copy_pose(&tpose, ob->pose, 1);
			rest_pose(apose, 1);
 
			if (base->object->nlastrips.first){
				rest_pose(base->object->pose, 0);
			}

			for (strip=base->object->nlastrips.first; strip; strip=strip->next){
				doit = 0;
				if (strip->act){
			
					/* Determine if the current frame is within the strip's range */
					length = strip->end-strip->start;
					actlength = strip->actend-strip->actstart;
					striptime = (G.scene->r.cfra-(strip->start)) / length;
					stripframe = (G.scene->r.cfra-(strip->start)) ;


					if (striptime>=0.0){
						
						rest_pose(tpose, 1);

						/* Handle path */
						if (strip->flag & ACTSTRIP_USESTRIDE){
							if (ob->parent && ob->parent->type==OB_CURVE){
								Curve *cu = ob->parent->data;
								float ctime, pdist;

								if (cu->flag & CU_PATH){
									/* Ensure we have a valid path */
									if(cu->path==0 || cu->path->data==0) calc_curvepath(ob->parent);

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
									get_pose_from_action (&tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
#ifdef __NLA_BLENDCON
									do_pose_constraint_channels(tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
#endif
									doit=1;
								}
							}
						}

						/* Handle repeat */
		
						else if (striptime < 1.0){
							/* Mod to repeat */
							striptime*=strip->repeat;
							striptime = (float)fmod (striptime, 1.0);
							
							frametime = (striptime * actlength) + strip->actstart;
							get_pose_from_action (&tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
#ifdef __NLA_BLENDCON
							do_pose_constraint_channels(tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
#endif
							doit=1;
						}
						/* Handle extend */
						else{
							if (strip->flag & ACTSTRIP_HOLDLASTFRAME){
								striptime = 1.0;
								frametime = (striptime * actlength) + strip->actstart;
								get_pose_from_action (&tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
#ifdef __NLA_BLENDCON
								do_pose_constraint_channels(tpose, strip->act, bsystem_time(ob, 0, frametime, 0.0));
#endif
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

							/* Blend this pose with the accumulated pose */
							blend_poses (apose, tpose, blendfac, strip->mode);
#ifdef __NLA_BLENDCON
							blend_constraints(&apose->chanbase, &tpose->chanbase, blendfac, strip->mode);
#endif
						}
					}					
					if (apose){
						get_pose_from_pose(&ob->pose, apose);
#ifdef __NLA_BLENDCON
						get_constraint_influence_from_pose(ob->pose, apose);
#endif
					}
				}
				
			}

			/* Do local action (always overrides the nla actions) */
			/*	At the moment, only constraint ipos on the local action have any effect */
			if(base->object->action) {
				get_pose_from_action (&ob->pose, ob->action, bsystem_time(ob, 0, (float) G.scene->r.cfra, 0.0));
				do_pose_constraint_channels(ob->pose, ob->action, bsystem_time(ob, 0, (float) G.scene->r.cfra, 0.0));
				doit = 1;
			} 
			
			if (doit)
				apply_pose_armature(get_armature(ob), ob->pose, 1);

		}
		base= base->next;
		if(base==0 && set==0 && G.scene->set) {
			set= 1;
			base= G.scene->set->base.first; 
		}
		
	}

	if (apose){
		clear_pose(apose);
		MEM_freeN(apose);
		apose = NULL;
	}	
	if (tpose){
		clear_pose(tpose);
		MEM_freeN(tpose);
		apose = NULL;
	}

}

static void do_pose_constraint_channels(bPose *pose, bAction *act, float ctime)
{
	bPoseChannel *pchan;
	bActionChannel *achan;

	if (!pose || !act)
		return;
	
	for (pchan=pose->chanbase.first; pchan; pchan=pchan->next){
		achan=get_named_actionchannel(act, pchan->name);
		if (achan)
			do_constraint_channels(&pchan->constraints, &achan->constraintChannels, ctime);
	}
}

bActionChannel *get_named_actionchannel (bAction *act, const char *name)
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

void clear_pose ( bPose *pose) 
{
	bPoseChannel *chan;
	
	if (pose->chanbase.first){
		for (chan = pose->chanbase.first; chan; chan=chan->next){
			free_constraints(&chan->constraints);
		}
		BLI_freelistN (&pose->chanbase);
	}
}

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
					ob->activecon = NULL;
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

bAction* copy_action (const bAction *src)
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

bPoseChannel *copy_pose_channel (const bPoseChannel* src)
{
	bPoseChannel *dst;

	if (!src)
		return NULL;

	dst = MEM_callocN (sizeof(bPoseChannel), "copyPoseChannel");
	memcpy (dst, src, sizeof(bPoseChannel));
	dst->prev=dst->next=NULL;

	return dst;
}

void copy_pose(bPose **dst, const bPose *src, int copycon)
{
	bPose *outPose;
	const bPose * inPose;
	bPoseChannel	*newChan;
	const bPoseChannel *curChan;

	inPose = src;
	
	if (!inPose){
		*dst=NULL;
		return;
	}

	outPose=MEM_callocN(sizeof(bPose), "pose");

	for (curChan=inPose->chanbase.first; curChan; curChan=curChan->next){
		newChan=MEM_callocN(sizeof(bPoseChannel), "copyposePoseChannel");

		strcpy (newChan->name, curChan->name);
		newChan->flag=curChan->flag;

		memcpy (newChan->loc, curChan->loc, sizeof (curChan->loc));
		memcpy (newChan->size, curChan->size, sizeof (curChan->size));
		memcpy (newChan->quat, curChan->quat, sizeof (curChan->quat));
		Mat4CpyMat4 (newChan->obmat, curChan->obmat);

		BLI_addtail (&outPose->chanbase, newChan);
		if (copycon){
			copy_constraints(&newChan->constraints, &curChan->constraints);
		}
	}
	
	*dst=outPose;
}

bPoseChannel *set_pose_channel (bPose *pose, bPoseChannel *chan)
{
	/*	chan is no longer valid for the calling function.
		and should not be used by that function after calling
		this one
	*/
	bPoseChannel	*curChan;

	/*	Determine if an equivalent channel exists already */
	for (curChan=pose->chanbase.first; curChan; curChan=curChan->next){
		if (!strcmp (curChan->name, chan->name)){
			if (chan->flag & POSE_ROT)
				memcpy (curChan->quat, chan->quat, sizeof(chan->quat)); 
			if (chan->flag & POSE_SIZE) 
				memcpy (curChan->size, chan->size, sizeof(chan->size));
			if (chan->flag & POSE_LOC)
				memcpy (curChan->loc, chan->loc, sizeof(chan->loc));
			if (chan->flag & PCHAN_DONE)
				Mat4CpyMat4 (curChan->obmat, chan->obmat);

			curChan->flag |= chan->flag;
			MEM_freeN (chan);
			return curChan;
		}
	}

	MEM_freeN (chan);
	return NULL;
	/* If an equivalent channel doesn't exist, then don't bother setting it. */
}


