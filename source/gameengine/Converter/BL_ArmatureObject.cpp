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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Converter/BL_ArmatureObject.cpp
 *  \ingroup bgeconv
 */


#include "BL_ArmatureObject.h"
#include "BL_ActionActuator.h"
#include "KX_BlenderSceneConverter.h"
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BIK_api.h"
#include "BKE_action.h"
#include "BKE_armature.h"

#include "BKE_constraint.h"
#include "CTR_Map.h"
#include "CTR_HashedPtr.h"
#include "MEM_guardedalloc.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_nla_types.h"
#include "DNA_constraint_types.h"
#include "KX_PythonSeq.h"
#include "KX_PythonInit.h"
#include "KX_KetsjiEngine.h"

#include "MT_Matrix4x4.h"

/** 
 * Move here pose function for game engine so that we can mix with GE objects
 * Principle is as follow:
 * Use Blender structures so that where_is_pose can be used unchanged
 * Copy the constraint so that they can be enabled/disabled/added/removed at runtime
 * Don't copy the constraints for the pose used by the Action actuator, it does not need them.
 * Scan the constraint structures so that the KX equivalent of target objects are identified and 
 * stored in separate list.
 * When it is about to evaluate the pose, set the KX object position in the obmat of the corresponding
 * Blender objects and restore after the evaluation.
 */
void game_copy_pose(bPose **dst, bPose *src, int copy_constraint)
{
	bPose *out;
	bPoseChannel *pchan, *outpchan;
	GHash *ghash;
	
	/* the game engine copies the current armature pose and then swaps
	 * the object pose pointer. this makes it possible to change poses
	 * without affecting the original blender data. */

	if (!src) {
		*dst=NULL;
		return;
	}
	else if (*dst==src) {
		printf("copy_pose source and target are the same\n");
		*dst=NULL;
		return;
	}
	
	out= (bPose*)MEM_dupallocN(src);
	out->chanhash = NULL;
	out->agroups.first= out->agroups.last= NULL;
	out->ikdata = NULL;
	out->ikparam = MEM_dupallocN(out->ikparam);
	out->flag |= POSE_GAME_ENGINE;
	BLI_duplicatelist(&out->chanbase, &src->chanbase);

	/* remap pointers */
	ghash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "game_copy_pose gh");

	pchan= (bPoseChannel*)src->chanbase.first;
	outpchan= (bPoseChannel*)out->chanbase.first;
	for (; pchan; pchan=pchan->next, outpchan=outpchan->next)
		BLI_ghash_insert(ghash, pchan, outpchan);

	for (pchan=(bPoseChannel*)out->chanbase.first; pchan; pchan=(bPoseChannel*)pchan->next) {
		pchan->parent= (bPoseChannel*)BLI_ghash_lookup(ghash, pchan->parent);
		pchan->child= (bPoseChannel*)BLI_ghash_lookup(ghash, pchan->child);

		if (copy_constraint) {
			ListBase listb;
			// copy all constraint for backward compatibility
			copy_constraints(&listb, &pchan->constraints, FALSE);  // copy_constraints NULLs listb, no need to make extern for this operation.
			pchan->constraints= listb;
		} else {
			pchan->constraints.first = NULL;
			pchan->constraints.last = NULL;
		}

		// fails to link, props are not used in the BGE yet.
		/* if(pchan->prop)
			pchan->prop= IDP_CopyProperty(pchan->prop); */
		pchan->prop= NULL;
	}

	BLI_ghash_free(ghash, NULL, NULL);
	// set acceleration structure for channel lookup
	make_pose_channels_hash(out);
	*dst=out;
}



/* Only allowed for Poses with identical channels */
void game_blend_poses(bPose *dst, bPose *src, float srcweight/*, short mode*/)
{
	short mode= ACTSTRIPMODE_BLEND;
	
	bPoseChannel *dchan;
	const bPoseChannel *schan;
	bConstraint *dcon, *scon;
	float dstweight;
	int i;

	switch (mode){
	case ACTSTRIPMODE_BLEND:
		dstweight = 1.0F - srcweight;
		break;
	case ACTSTRIPMODE_ADD:
		dstweight = 1.0F;
		break;
	default :
		dstweight = 1.0F;
	}
	
	schan= (bPoseChannel*)src->chanbase.first;
	for (dchan = (bPoseChannel*)dst->chanbase.first; dchan; dchan=(bPoseChannel*)dchan->next, schan= (bPoseChannel*)schan->next){
		// always blend on all channels since we don't know which one has been set
		/* quat interpolation done separate */
		if (schan->rotmode == ROT_MODE_QUAT) {
			float dquat[4], squat[4];
			
			copy_qt_qt(dquat, dchan->quat);
			copy_qt_qt(squat, schan->quat);
			if (mode==ACTSTRIPMODE_BLEND)
				interp_qt_qtqt(dchan->quat, dquat, squat, srcweight);
			else {
				mul_fac_qt_fl(squat, srcweight);
				mul_qt_qtqt(dchan->quat, dquat, squat);
			}
			
			normalize_qt(dchan->quat);
		}

		for (i=0; i<3; i++) {
			/* blending for loc and scale are pretty self-explanatory... */
			dchan->loc[i] = (dchan->loc[i]*dstweight) + (schan->loc[i]*srcweight);
			dchan->size[i] = 1.0f + ((dchan->size[i]-1.0f)*dstweight) + ((schan->size[i]-1.0f)*srcweight);
			
			/* euler-rotation interpolation done here instead... */
			// FIXME: are these results decent?
			if (schan->rotmode)
				dchan->eul[i] = (dchan->eul[i]*dstweight) + (schan->eul[i]*srcweight);
		}
		for(dcon= (bConstraint*)dchan->constraints.first, scon= (bConstraint*)schan->constraints.first; dcon && scon; dcon= (bConstraint*)dcon->next, scon= (bConstraint*)scon->next) {
			/* no 'add' option for constraint blending */
			dcon->enforce= dcon->enforce*(1.0f-srcweight) + scon->enforce*srcweight;
		}
	}
	
	/* this pose is now in src time */
	dst->ctime= src->ctime;
}

void game_free_pose(bPose *pose)
{
	if (pose) {
		/* free pose-channels and constraints */
		free_pose_channels(pose);
		
		/* free IK solver state */
		BIK_clear_data(pose);

		/* free IK solver param */
		if (pose->ikparam)
			MEM_freeN(pose->ikparam);

		MEM_freeN(pose);
	}
}

BL_ArmatureObject::BL_ArmatureObject(
				void* sgReplicationInfo, 
				SG_Callbacks callbacks, 
				Object *armature,
				Scene *scene,
				int vert_deform_type)

:	KX_GameObject(sgReplicationInfo,callbacks),
	m_controlledConstraints(),
	m_poseChannels(),
	m_objArma(armature),
	m_framePose(NULL),
	m_scene(scene), // maybe remove later. needed for where_is_pose
	m_lastframe(0.0),
	m_timestep(0.040),
	m_activeAct(NULL),
	m_activePriority(999),
	m_vert_deform_type(vert_deform_type),
	m_constraintNumber(0),
	m_channelNumber(0),
	m_lastapplyframe(0.0)
{
	m_armature = (bArmature *)armature->data;

	/* we make a copy of blender object's pose, and then always swap it with
	 * the original pose before calling into blender functions, to deal with
	 * replica's or other objects using the same blender object */
	m_pose = NULL;
	game_copy_pose(&m_pose, m_objArma->pose, 1);
	// store the original armature object matrix
	memcpy(m_obmat, m_objArma->obmat, sizeof(m_obmat));
}

BL_ArmatureObject::~BL_ArmatureObject()
{
	BL_ArmatureConstraint* constraint;
	while ((constraint = m_controlledConstraints.Remove()) != NULL) {
		delete constraint;
	}
	BL_ArmatureChannel* channel;
	while ((channel = static_cast<BL_ArmatureChannel*>(m_poseChannels.Remove())) != NULL) {
		delete channel;
	}
	if (m_pose)
		game_free_pose(m_pose);
	if (m_framePose)
		game_free_pose(m_framePose);
}


void BL_ArmatureObject::LoadConstraints(KX_BlenderSceneConverter* converter)
{
	// first delete any existing constraint (should not have any)
	while (!m_controlledConstraints.Empty()) {
		BL_ArmatureConstraint* constraint = m_controlledConstraints.Remove();
		delete constraint;
	}
	m_constraintNumber = 0;

	// list all the constraint and convert them to BL_ArmatureConstraint
	// get the persistent pose structure
	bPoseChannel* pchan;
	bConstraint* pcon;
	bConstraintTypeInfo* cti;
	Object* blendtarget;
	KX_GameObject* gametarget;
	KX_GameObject* gamesubtarget;

	// and locate the constraint
	for (pchan = (bPoseChannel*)m_pose->chanbase.first; pchan; pchan=(bPoseChannel*)pchan->next) {
		for (pcon = (bConstraint*)pchan->constraints.first; pcon; pcon=(bConstraint*)pcon->next) {
			if (pcon->flag & CONSTRAINT_DISABLE)
				continue;
			// which constraint should we support?
			switch (pcon->type) {
			case CONSTRAINT_TYPE_TRACKTO:
			case CONSTRAINT_TYPE_DAMPTRACK:
			case CONSTRAINT_TYPE_KINEMATIC:
			case CONSTRAINT_TYPE_ROTLIKE:
			case CONSTRAINT_TYPE_LOCLIKE:
			case CONSTRAINT_TYPE_MINMAX:
			case CONSTRAINT_TYPE_SIZELIKE:
			case CONSTRAINT_TYPE_LOCKTRACK:
			case CONSTRAINT_TYPE_STRETCHTO:
			case CONSTRAINT_TYPE_CLAMPTO:
			case CONSTRAINT_TYPE_TRANSFORM:
			case CONSTRAINT_TYPE_DISTLIMIT:
			case CONSTRAINT_TYPE_TRANSLIKE:
				cti = constraint_get_typeinfo(pcon);
				gametarget = gamesubtarget = NULL;
				if (cti && cti->get_constraint_targets) {
					ListBase listb = { NULL, NULL };
					cti->get_constraint_targets(pcon, &listb);
					if (listb.first) {
						bConstraintTarget* target = (bConstraintTarget*)listb.first;
						if (target->tar && target->tar != m_objArma) {
							// only remember external objects, self target is handled automatically
							blendtarget = target->tar;
							gametarget = converter->FindGameObject(blendtarget);
						}
						if (target->next != NULL) {
							// secondary target
							target = (bConstraintTarget*)target->next;
							if (target->tar && target->tar != m_objArma) {
								// only track external object
								blendtarget = target->tar;
								gamesubtarget = converter->FindGameObject(blendtarget);
							}
						}
					}
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(pcon, &listb, 1);
				}
				BL_ArmatureConstraint* constraint = new BL_ArmatureConstraint(this, pchan, pcon, gametarget, gamesubtarget);
				m_controlledConstraints.AddBack(constraint);
				m_constraintNumber++;
			}
		}
	}
}

BL_ArmatureConstraint* BL_ArmatureObject::GetConstraint(const char* posechannel, const char* constraintname)
{
	SG_DList::iterator<BL_ArmatureConstraint> cit(m_controlledConstraints);
	for (cit.begin(); !cit.end(); ++cit) {
		BL_ArmatureConstraint* constraint = *cit;
		if (constraint->Match(posechannel, constraintname))
			return constraint;
	}
	return NULL;
}

BL_ArmatureConstraint* BL_ArmatureObject::GetConstraint(const char* posechannelconstraint)
{
	// performance: use hash string instead of plain string compare
	SG_DList::iterator<BL_ArmatureConstraint> cit(m_controlledConstraints);
	for (cit.begin(); !cit.end(); ++cit) {
		BL_ArmatureConstraint* constraint = *cit;
		if (!strcmp(constraint->GetName(), posechannelconstraint))
			return constraint;
	}
	return NULL;
}

BL_ArmatureConstraint* BL_ArmatureObject::GetConstraint(int index)
{
	SG_DList::iterator<BL_ArmatureConstraint> cit(m_controlledConstraints);
	for (cit.begin(); !cit.end() && index; ++cit, --index);
	return (cit.end()) ? NULL : *cit;
}

/* this function is called to populate the m_poseChannels list */
void BL_ArmatureObject::LoadChannels()
{
	if (m_poseChannels.Empty()) {
		bPoseChannel* pchan;
		BL_ArmatureChannel* proxy;
	
		m_channelNumber = 0;
		for (pchan = (bPoseChannel*)m_pose->chanbase.first; pchan; pchan=(bPoseChannel*)pchan->next) {
			proxy = new BL_ArmatureChannel(this, pchan);
			m_poseChannels.AddBack(proxy);
			m_channelNumber++;
		}
	}
}

BL_ArmatureChannel* BL_ArmatureObject::GetChannel(bPoseChannel* pchan)
{
	LoadChannels();
	SG_DList::iterator<BL_ArmatureChannel> cit(m_poseChannels);
	for (cit.begin(); !cit.end(); ++cit) 
	{
		BL_ArmatureChannel* channel = *cit;
		if (channel->m_posechannel == pchan)
			return channel;
	}
	return NULL;
}

BL_ArmatureChannel* BL_ArmatureObject::GetChannel(const char* str)
{
	LoadChannels();
	SG_DList::iterator<BL_ArmatureChannel> cit(m_poseChannels);
	for (cit.begin(); !cit.end(); ++cit) 
	{
		BL_ArmatureChannel* channel = *cit;
		if (!strcmp(channel->m_posechannel->name, str))
			return channel;
	}
	return NULL;
}

BL_ArmatureChannel* BL_ArmatureObject::GetChannel(int index)
{
	LoadChannels();
	if (index < 0 || index >= m_channelNumber)
		return NULL;
	SG_DList::iterator<BL_ArmatureChannel> cit(m_poseChannels);
	for (cit.begin(); !cit.end() && index; ++cit, --index);
	return (cit.end()) ? NULL : *cit;
}

CValue* BL_ArmatureObject::GetReplica()
{
	BL_ArmatureObject* replica = new BL_ArmatureObject(*this);
	replica->ProcessReplica();
	return replica;
}

void BL_ArmatureObject::ProcessReplica()
{
	bPose *pose= m_pose;
	KX_GameObject::ProcessReplica();

	m_pose = NULL;
	m_framePose = NULL;
	game_copy_pose(&m_pose, pose, 1);	
}

void BL_ArmatureObject::ReParentLogic()
{
	SG_DList::iterator<BL_ArmatureConstraint> cit(m_controlledConstraints);
	for (cit.begin(); !cit.end(); ++cit) {
		(*cit)->ReParent(this);
	}
	KX_GameObject::ReParentLogic();
}

void BL_ArmatureObject::Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map)
{
	SG_DList::iterator<BL_ArmatureConstraint> cit(m_controlledConstraints);
	for (cit.begin(); !cit.end(); ++cit) {
		(*cit)->Relink(obj_map);
	}
	KX_GameObject::Relink(obj_map);
}

bool BL_ArmatureObject::UnlinkObject(SCA_IObject* clientobj)
{
	// clientobj is being deleted, make sure we don't hold any reference to it
	bool res = false;
	SG_DList::iterator<BL_ArmatureConstraint> cit(m_controlledConstraints);
	for (cit.begin(); !cit.end(); ++cit) {
		res |= (*cit)->UnlinkObject(clientobj);
	}
	return res;
}

void BL_ArmatureObject::ApplyPose()
{
	m_armpose = m_objArma->pose;
	m_objArma->pose = m_pose;
	// in the GE, we use ctime to store the timestep
	m_pose->ctime = (float)m_timestep;
	//m_scene->r.cfra++;
	if(m_lastapplyframe != m_lastframe) {
		// update the constraint if any, first put them all off so that only the active ones will be updated
		SG_DList::iterator<BL_ArmatureConstraint> cit(m_controlledConstraints);
		for (cit.begin(); !cit.end(); ++cit) {
			(*cit)->UpdateTarget();
		}
		// update ourself
		UpdateBlenderObjectMatrix(m_objArma);
		where_is_pose(m_scene, m_objArma); // XXX
		// restore ourself
		memcpy(m_objArma->obmat, m_obmat, sizeof(m_obmat));
		// restore active targets
		for (cit.begin(); !cit.end(); ++cit) {
			(*cit)->RestoreTarget();
		}
		m_lastapplyframe = m_lastframe;
	}
}

void BL_ArmatureObject::RestorePose()
{
	m_objArma->pose = m_armpose;
	m_armpose = NULL;
}

void BL_ArmatureObject::SetPose(bPose *pose)
{
	extract_pose_from_pose(m_pose, pose);
	m_lastapplyframe = -1.0;
}

bool BL_ArmatureObject::SetActiveAction(BL_ActionActuator *act, short priority, double curtime)
{
	if (curtime != m_lastframe){
		m_activePriority = 9999;
		// compute the timestep for the underlying IK algorithm
		m_timestep = curtime-m_lastframe;
		m_lastframe= curtime;
		m_activeAct = NULL;
		// remember the pose at the start of the frame
		GetPose(&m_framePose);
	}

	if (act) 
	{
		if (priority<=m_activePriority)
		{
			if (priority<m_activePriority) {
				// this action overwrites the previous ones, start from initial pose to cancel their effects
				SetPose(m_framePose);
				if (m_activeAct && (m_activeAct!=act))
					/* Reset the blend timer since this new action cancels the old one */
					m_activeAct->SetBlendTime(0.0);	
			}
			m_activeAct = act;
			m_activePriority = priority;
			m_lastframe = curtime;
		
			return true;
		}
		else{
			act->SetBlendTime(0.0);
			return false;
		}
	}
	return false;
}

BL_ActionActuator * BL_ArmatureObject::GetActiveAction()
{
	return m_activeAct;
}

void BL_ArmatureObject::GetPose(bPose **pose)
{
	/* If the caller supplies a null pose, create a new one. */
	/* Otherwise, copy the armature's pose channels into the caller-supplied pose */
		
	if (!*pose) {
		/*	probably not to good of an idea to
			duplicate everying, but it clears up 
			a crash and memory leakage when 
			&BL_ActionActuator::m_pose is freed
		*/
		game_copy_pose(pose, m_pose, 0);
	}
	else {
		if (*pose == m_pose)
			// no need to copy if the pointers are the same
			return;

		extract_pose_from_pose(*pose, m_pose);
	}
}

void BL_ArmatureObject::GetMRDPose(bPose **pose)
{
	/* If the caller supplies a null pose, create a new one. */
	/* Otherwise, copy the armature's pose channels into the caller-supplied pose */

	if (!*pose)
		game_copy_pose(pose, m_pose, 0);
	else
		extract_pose_from_pose(*pose, m_pose);
}

short BL_ArmatureObject::GetActivePriority()
{
	return m_activePriority;
}

double BL_ArmatureObject::GetLastFrame()
{
	return m_lastframe;
}

bool BL_ArmatureObject::GetBoneMatrix(Bone* bone, MT_Matrix4x4& matrix)
{
	bPoseChannel *pchan;

	ApplyPose();
	pchan = get_pose_channel(m_objArma->pose, bone->name);
	if(pchan)
		matrix.setValue(&pchan->pose_mat[0][0]);
	RestorePose();

	return (pchan != NULL);
}

float BL_ArmatureObject::GetBoneLength(Bone* bone) const
{
	return (float)(MT_Point3(bone->head) - MT_Point3(bone->tail)).length();
}

#ifdef WITH_PYTHON

// PYTHON

PyTypeObject BL_ArmatureObject::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BL_ArmatureObject",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	&KX_GameObject::Sequence,
	&KX_GameObject::Mapping,
	0,0,0,
	NULL,
	NULL,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&KX_GameObject::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef BL_ArmatureObject::Methods[] = {

	KX_PYMETHODTABLE_NOARGS(BL_ArmatureObject, update),
	{NULL,NULL} //Sentinel
};

PyAttributeDef BL_ArmatureObject::Attributes[] = {

	KX_PYATTRIBUTE_RO_FUNCTION("constraints",		BL_ArmatureObject, pyattr_get_constraints),
	KX_PYATTRIBUTE_RO_FUNCTION("channels",		BL_ArmatureObject, pyattr_get_channels),
	{NULL} //Sentinel
};

PyObject* BL_ArmatureObject::pyattr_get_constraints(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return KX_PythonSeq_CreatePyObject((static_cast<BL_ArmatureObject*>(self_v))->m_proxy, KX_PYGENSEQ_OB_TYPE_CONSTRAINTS);
}

PyObject* BL_ArmatureObject::pyattr_get_channels(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ArmatureObject* self = static_cast<BL_ArmatureObject*>(self_v);
	self->LoadChannels(); // make sure we have the channels
	return KX_PythonSeq_CreatePyObject((static_cast<BL_ArmatureObject*>(self_v))->m_proxy, KX_PYGENSEQ_OB_TYPE_CHANNELS);
}

KX_PYMETHODDEF_DOC_NOARGS(BL_ArmatureObject, update, 
						  "update()\n"
						  "Make sure that the armature will be updated on next graphic frame.\n"
						  "This is automatically done if a KX_ArmatureActuator with mode run is active\n"
						  "or if an action is playing. This function is useful in other cases.\n")
{
	SetActiveAction(NULL, 0, KX_GetActiveEngine()->GetFrameTime());
	Py_RETURN_NONE;
}

#endif // WITH_PYTHON
