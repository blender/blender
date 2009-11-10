/**
 * $Id:
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
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 * autoarmature.c: Interface for automagically manipulating armature (retarget, created, ...)
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"
#include "BLI_graph.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

//#include "BDR_editobject.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_constraint.h"
#include "BKE_armature.h"
#include "BKE_context.h"

#include "ED_armature.h"
#include "ED_util.h"

#include "BIF_retarget.h"

#include "PIL_time.h"

//#include "mydevice.h"
#include "reeb.h" // FIX ME
//#include "blendef.h"

#include "armature_intern.h"

/************ RIG RETARGET DATA STRUCTURES ***************/

typedef struct MemoNode {
	float	weight;
	int 	next;
} MemoNode;

typedef struct RetargetParam {
	RigGraph	*rigg;
	RigArc		*iarc;
	RigNode		*inode_start;
	bContext	*context;
} RetargetParam;

typedef enum 
{
	RETARGET_LENGTH,
	RETARGET_AGGRESSIVE
} RetargetMode; 

typedef enum
{
	METHOD_BRUTE_FORCE = 0,
	METHOD_MEMOIZE = 1
} RetargetMethod;

typedef enum
{
	ARC_FREE = 0,
	ARC_TAKEN = 1,
	ARC_USED = 2
} ArcUsageFlags;

RigGraph *GLOBAL_RIGG = NULL;

/*******************************************************************************************************/

void *exec_retargetArctoArc(void *param);

static void RIG_calculateEdgeAngles(RigEdge *edge_first, RigEdge *edge_second);
float rollBoneByQuat(EditBone *bone, float old_up_axis[3], float qrot[4]);

/* two levels */
#define SHAPE_LEVELS (SHAPE_RADIX * SHAPE_RADIX) 

/*********************************** EDITBONE UTILS ****************************************************/

int countEditBoneChildren(ListBase *list, EditBone *parent)
{
	EditBone *ebone;
	int count = 0;
	
	for (ebone = list->first; ebone; ebone = ebone->next)
	{
		if (ebone->parent == parent)
		{
			count++;
		}
	}
	
	return count;
}

EditBone* nextEditBoneChild(ListBase *list, EditBone *parent, int n)
{
	EditBone *ebone;
	
	for (ebone = list->first; ebone; ebone = ebone->next)
	{
		if (ebone->parent == parent)
		{
			if (n == 0)
			{
				return ebone;
			}
			n--;
		}
	}
	
	return NULL;
}

void getEditBoneRollUpAxis(EditBone *bone, float roll, float up_axis[3])
{
	float mat[3][3], nor[3];

	VecSubf(nor, bone->tail, bone->head);
	
	vec_roll_to_mat3(nor, roll, mat);
	VECCOPY(up_axis, mat[2]);
}

float rollBoneByQuatAligned(EditBone *bone, float old_up_axis[3], float qrot[4], float qroll[4], float aligned_axis[3])
{
	float nor[3], new_up_axis[3], x_axis[3], z_axis[3];
	
	VECCOPY(new_up_axis, old_up_axis);
	QuatMulVecf(qrot, new_up_axis);
	
	VecSubf(nor, bone->tail, bone->head);
	
	Crossf(x_axis, nor, aligned_axis);
	Crossf(z_axis, x_axis, nor);
	
	Normalize(new_up_axis);
	Normalize(x_axis);
	Normalize(z_axis);
	
	if (Inpf(new_up_axis, x_axis) < 0)
	{
		VecMulf(x_axis, -1);
	}
	
	if (Inpf(new_up_axis, z_axis) < 0)
	{
		VecMulf(z_axis, -1);
	}
	
	if (NormalizedVecAngle2(x_axis, new_up_axis) < NormalizedVecAngle2(z_axis, new_up_axis))
	{
		RotationBetweenVectorsToQuat(qroll, new_up_axis, x_axis); /* set roll rotation quat */
		return ED_rollBoneToVector(bone, x_axis);
	}
	else
	{
		RotationBetweenVectorsToQuat(qroll, new_up_axis, z_axis); /* set roll rotation quat */
		return ED_rollBoneToVector(bone, z_axis);
	}
}

float rollBoneByQuatJoint(RigEdge *edge, RigEdge *previous, float qrot[4], float qroll[4], float up_axis[3])
{
	if (previous == NULL)
	{
		/* default to up_axis if no previous */
		return rollBoneByQuatAligned(edge->bone, edge->up_axis, qrot, qroll, up_axis);
	}
	else
	{
		float new_up_axis[3];
		float vec_first[3], vec_second[3], normal[3];
		
		if (previous->bone)
		{
			VecSubf(vec_first, previous->bone->tail, previous->bone->head);
		} 
		else if (previous->prev->bone)
		{
			VecSubf(vec_first, edge->bone->head, previous->prev->bone->tail);
		}
		else
		{
			/* default to up_axis if first bone in the chain is an offset */
			return rollBoneByQuatAligned(edge->bone, edge->up_axis, qrot, qroll, up_axis);
		}
		
		VecSubf(vec_second, edge->bone->tail, edge->bone->head);
	
		Normalize(vec_first);
		Normalize(vec_second);
		
		Crossf(normal, vec_first, vec_second);
		Normalize(normal);
		
		AxisAngleToQuat(qroll, vec_second, edge->up_angle);
		
		QuatMulVecf(qroll, normal);
			
		VECCOPY(new_up_axis, edge->up_axis);
		QuatMulVecf(qrot, new_up_axis);
		
		Normalize(new_up_axis);
		
		/* real qroll between normal and up_axis */
		RotationBetweenVectorsToQuat(qroll, new_up_axis, normal);

		return ED_rollBoneToVector(edge->bone, normal);
	}
}

float rollBoneByQuat(EditBone *bone, float old_up_axis[3], float qrot[4])
{
	float new_up_axis[3];
	
	VECCOPY(new_up_axis, old_up_axis);
	QuatMulVecf(qrot, new_up_axis);
	
	Normalize(new_up_axis);
	
	return ED_rollBoneToVector(bone, new_up_axis);
}

/************************************ DESTRUCTORS ******************************************************/

void RIG_freeRigArc(BArc *arc)
{
	BLI_freelistN(&((RigArc*)arc)->edges);
}

void RIG_freeRigGraph(BGraph *rg)
{
	RigGraph *rigg = (RigGraph*)rg;
	BNode *node;
	BArc *arc;
	
#ifdef USE_THREADS
	BLI_destroy_worker(rigg->worker);
#endif
	
	if (rigg->link_mesh)
	{
		REEB_freeGraph(rigg->link_mesh);
	}
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RIG_freeRigArc(arc);
	}
	BLI_freelistN(&rg->arcs);
	
	for (node = rg->nodes.first; node; node = node->next)
	{
		BLI_freeNode(rg, (BNode*)node);
	}
	BLI_freelistN(&rg->nodes);
	
	BLI_freelistN(&rigg->controls);

	BLI_ghash_free(rigg->bones_map, NULL, NULL);
	BLI_ghash_free(rigg->controls_map, NULL, NULL);
	
	if (rigg->flag & RIG_FREE_BONELIST)
	{
		BLI_freelistN(rigg->editbones);
		MEM_freeN(rigg->editbones);
	}
	
	MEM_freeN(rg);
}

/************************************* ALLOCATORS ******************************************************/

static RigGraph *newRigGraph()
{
	RigGraph *rg;
	int totthread;
	
	rg = MEM_callocN(sizeof(RigGraph), "rig graph");
	
	rg->head = NULL;
	
	rg->bones_map = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp);
	rg->controls_map = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp);
	
	rg->free_arc = RIG_freeRigArc;
	rg->free_node = NULL;
	
#ifdef USE_THREADS
//	if(G.scene->r.mode & R_FIXED_THREADS)
//	{
//		totthread = G.scene->r.threads;
//	}
//	else
//	{
		totthread = BLI_system_thread_count();
//	}
	
	rg->worker = BLI_create_worker(exec_retargetArctoArc, totthread, 20); /* fix number of threads */
#endif
	
	return rg;
}

static RigArc *newRigArc(RigGraph *rg)
{
	RigArc *arc;
	
	arc = MEM_callocN(sizeof(RigArc), "rig arc");
	arc->count = 0;
	BLI_addtail(&rg->arcs, arc);
	
	return arc;
}

static RigControl *newRigControl(RigGraph *rg)
{
	RigControl *ctrl;
	
	ctrl = MEM_callocN(sizeof(RigControl), "rig control");
	
	BLI_addtail(&rg->controls, ctrl);
	
	return ctrl;
}

static RigNode *newRigNodeHead(RigGraph *rg, RigArc *arc, float p[3])
{
	RigNode *node;
	node = MEM_callocN(sizeof(RigNode), "rig node");
	BLI_addtail(&rg->nodes, node);

	VECCOPY(node->p, p);
	node->degree = 1;
	node->arcs = NULL;
	
	arc->head = node;
	
	return node;
}

static void addRigNodeHead(RigGraph *rg, RigArc *arc, RigNode *node)
{
	node->degree++;

	arc->head = node;
}

static RigNode *newRigNode(RigGraph *rg, float p[3])
{
	RigNode *node;
	node = MEM_callocN(sizeof(RigNode), "rig node");
	BLI_addtail(&rg->nodes, node);

	VECCOPY(node->p, p);
	node->degree = 0;
	node->arcs = NULL;
	
	return node;
}

static RigNode *newRigNodeTail(RigGraph *rg, RigArc *arc, float p[3])
{
	RigNode *node = newRigNode(rg, p);
	
	node->degree = 1;
	arc->tail = node;

	return node;
}

static void RIG_appendEdgeToArc(RigArc *arc, RigEdge *edge)
{
	BLI_addtail(&arc->edges, edge);

	if (edge->prev == NULL)
	{
		VECCOPY(edge->head, arc->head->p);
	}
	else
	{
		RigEdge *last_edge = edge->prev;
		VECCOPY(edge->head, last_edge->tail);
		RIG_calculateEdgeAngles(last_edge, edge);
	}
	
	edge->length = VecLenf(edge->head, edge->tail);
	
	arc->length += edge->length;
	
	arc->count += 1;
}

static void RIG_addEdgeToArc(RigArc *arc, float tail[3], EditBone *bone)
{
	RigEdge *edge;

	edge = MEM_callocN(sizeof(RigEdge), "rig edge");

	VECCOPY(edge->tail, tail);
	edge->bone = bone;
	
	if (bone)
	{
		getEditBoneRollUpAxis(bone, bone->roll, edge->up_axis);
	}
	
	RIG_appendEdgeToArc(arc, edge);
}
/************************************** CLONING TEMPLATES **********************************************/

static void renameTemplateBone(char *name, char *template_name, ListBase *editbones, char *side_string, char *num_string)
{
	int i, j;
	
	for (i = 0, j = 0; template_name[i] != '\0' && i < 31 && j < 31; i++)
	{
		if (template_name[i] == '&')
		{
			if (template_name[i+1] == 'S' || template_name[i+1] == 's')
			{
				j += sprintf(name + j, "%s", side_string);
				i++;
			}
			else if (template_name[i+1] == 'N' || template_name[i+1] == 'n')
			{
				j += sprintf(name + j, "%s", num_string);
				i++;
			}
			else
			{
				name[j] = template_name[i];
				j++;
			}
		}
		else
		{
			name[j] = template_name[i];
			j++;
		}
	}
	
	name[j] = '\0';
	
	unique_editbone_name(editbones, name, NULL);
}

static RigControl *cloneControl(RigGraph *rg, RigGraph *src_rg, RigControl *src_ctrl, GHash *ptr_hash, char *side_string, char *num_string)
{
	RigControl *ctrl;
	char name[32];
	
	ctrl = newRigControl(rg);
	
	VECCOPY(ctrl->head, src_ctrl->head);
	VECCOPY(ctrl->tail, src_ctrl->tail);
	VECCOPY(ctrl->up_axis, src_ctrl->up_axis);
	VECCOPY(ctrl->offset, src_ctrl->offset);
	
	ctrl->tail_mode = src_ctrl->tail_mode;
	ctrl->flag = src_ctrl->flag;

	renameTemplateBone(name, src_ctrl->bone->name, rg->editbones, side_string, num_string);
	ctrl->bone = duplicateEditBoneObjects(src_ctrl->bone, name, rg->editbones, src_rg->ob, rg->ob);
	ctrl->bone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
	BLI_ghash_insert(ptr_hash, src_ctrl->bone, ctrl->bone);
	
	ctrl->link = src_ctrl->link;
	ctrl->link_tail = src_ctrl->link_tail;
	
	return ctrl;
}

static RigArc *cloneArc(RigGraph *rg, RigGraph *src_rg, RigArc *src_arc, GHash *ptr_hash, char *side_string, char *num_string)
{
	RigEdge *src_edge;
	RigArc  *arc;
	
	arc = newRigArc(rg);
	
	arc->head = BLI_ghash_lookup(ptr_hash, src_arc->head);
	arc->tail = BLI_ghash_lookup(ptr_hash, src_arc->tail);
	
	arc->head->degree++;
	arc->tail->degree++;
	
	arc->length = src_arc->length;

	arc->count = src_arc->count;
	
	for (src_edge = src_arc->edges.first; src_edge; src_edge = src_edge->next)
	{
		RigEdge *edge;
	
		edge = MEM_callocN(sizeof(RigEdge), "rig edge");

		VECCOPY(edge->head, src_edge->head);
		VECCOPY(edge->tail, src_edge->tail);
		VECCOPY(edge->up_axis, src_edge->up_axis);
		
		edge->length = src_edge->length;
		edge->angle = src_edge->angle;
		edge->up_angle = src_edge->up_angle;
		
		if (src_edge->bone != NULL)
		{
			char name[32];
			renameTemplateBone(name, src_edge->bone->name, rg->editbones, side_string, num_string);
			edge->bone = duplicateEditBoneObjects(src_edge->bone, name, rg->editbones, src_rg->ob, rg->ob);
			edge->bone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
			BLI_ghash_insert(ptr_hash, src_edge->bone, edge->bone);
		}

		BLI_addtail(&arc->edges, edge);
	}
	
	return arc;
}

static RigGraph *cloneRigGraph(RigGraph *src, ListBase *editbones, Object *ob, char *side_string, char *num_string)
{
	GHash	*ptr_hash;	
	RigNode *node;
	RigArc  *arc;
	RigControl *ctrl;
	RigGraph *rg;
	
	ptr_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);

	rg = newRigGraph();
	
	rg->ob = ob;
	rg->editbones = editbones;
	
	preEditBoneDuplicate(rg->editbones); /* prime bones for duplication */
	preEditBoneDuplicate(src->editbones); /* prime bones for duplication */
	
	/* Clone nodes */
	for (node = src->nodes.first; node; node = node->next)
	{
		RigNode *cloned_node = newRigNode(rg, node->p);
		BLI_ghash_insert(ptr_hash, node, cloned_node);
	}
	
	rg->head = BLI_ghash_lookup(ptr_hash, src->head);
	
	/* Clone arcs */
	for (arc = src->arcs.first; arc; arc = arc->next)
	{
		cloneArc(rg, src, arc, ptr_hash, side_string, num_string);
	}
	
	/* Clone controls */
	for (ctrl = src->controls.first; ctrl; ctrl = ctrl->next)
	{
		cloneControl(rg, src, ctrl, ptr_hash, side_string, num_string);
	}
	
	/* Relink bones properly */
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RigEdge *edge;
		
		for (edge = arc->edges.first; edge; edge = edge->next)
		{
			if (edge->bone != NULL)
			{
				EditBone *bone;
				
				updateDuplicateSubtargetObjects(edge->bone, src->editbones, src->ob, rg->ob);

				if (edge->bone->parent)
				{				
					bone = BLI_ghash_lookup(ptr_hash, edge->bone->parent);
		
					if (bone != NULL)
					{
						edge->bone->parent = bone;
					}
					else
					{
						/* disconnect since parent isn't cloned
						 * this will only happen when cloning from selected bones 
						 * */
						edge->bone->flag &= ~BONE_CONNECTED;
					}
				}
			}
		}
	}
	
	for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
	{
		EditBone *bone;
		
		updateDuplicateSubtargetObjects(ctrl->bone, src->editbones, src->ob, rg->ob);

		if (ctrl->bone->parent)
		{
			bone = BLI_ghash_lookup(ptr_hash, ctrl->bone->parent);
			
			if (bone != NULL)
			{
				ctrl->bone->parent = bone;
			}
			else
			{
				/* disconnect since parent isn't cloned
				 * this will only happen when cloning from selected bones 
				 * */
				ctrl->bone->flag &= ~BONE_CONNECTED;
			}
		}

		ctrl->link = BLI_ghash_lookup(ptr_hash, ctrl->link);
		ctrl->link_tail = BLI_ghash_lookup(ptr_hash, ctrl->link_tail);
	}
	
	BLI_ghash_free(ptr_hash, NULL, NULL);
	
	return rg;
}


/*******************************************************************************************************/

static void RIG_calculateEdgeAngles(RigEdge *edge_first, RigEdge *edge_second)
{
	float vec_first[3], vec_second[3];
	
	VecSubf(vec_first, edge_first->tail, edge_first->head); 
	VecSubf(vec_second, edge_second->tail, edge_second->head);

	Normalize(vec_first);
	Normalize(vec_second);
	
	edge_first->angle = NormalizedVecAngle2(vec_first, vec_second);
	
	if (edge_second->bone != NULL)
	{
		float normal[3];

		Crossf(normal, vec_first, vec_second);
		Normalize(normal);

		edge_second->up_angle = NormalizedVecAngle2(normal, edge_second->up_axis);
	}
}

/************************************ CONTROL BONES ****************************************************/

static void RIG_addControlBone(RigGraph *rg, EditBone *bone)
{
	RigControl *ctrl = newRigControl(rg);
	ctrl->bone = bone;
	VECCOPY(ctrl->head, bone->head);
	VECCOPY(ctrl->tail, bone->tail);
	getEditBoneRollUpAxis(bone, bone->roll, ctrl->up_axis);
	ctrl->tail_mode = TL_NONE;
	
	BLI_ghash_insert(rg->controls_map, bone->name, ctrl);
}

static int RIG_parentControl(RigControl *ctrl, EditBone *link)
{
	if (link)
	{
		float offset[3];
		int flag = 0;
		
		VecSubf(offset, ctrl->bone->head, link->head);

		/* if root matches, check for direction too */		
		if (Inpf(offset, offset) < 0.0001)
		{
			float vbone[3], vparent[3];
			
			flag |= RIG_CTRL_FIT_ROOT;
			
			VecSubf(vbone, ctrl->bone->tail, ctrl->bone->head);
			VecSubf(vparent, link->tail, link->head);
			
			/* test for opposite direction */
			if (Inpf(vbone, vparent) > 0)
			{
				float nor[3];
				float len;
				
				Crossf(nor, vbone, vparent);
				
				len = Inpf(nor, nor);
				if (len < 0.0001)
				{
					flag |= RIG_CTRL_FIT_BONE;
				}
			}
		}
		
		/* Bail out if old one is automatically better */
		if (flag < ctrl->flag)
		{
			return 0;
		}
		
		/* if there's already a link
		 * 	overwrite only if new link is higher in the chain */
		if (ctrl->link && flag == ctrl->flag)
		{
			EditBone *bone = NULL;
			
			for (bone = ctrl->link; bone; bone = bone->parent)
			{
				/* if link is in the chain, break and use that one */
				if (bone == link)
				{
					break;
				}
			}
			
			/* not in chain, don't update link */
			if (bone == NULL)
			{
				return 0;
			}
		}
		
		
		ctrl->link = link;
		ctrl->flag = flag;
		
		VECCOPY(ctrl->offset, offset);
		
		return 1;
	}
	
	return 0;
}

static void RIG_reconnectControlBones(RigGraph *rg)
{
	RigControl *ctrl;
	int change = 1;
	
	/* first pass, link to deform bones */
	for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
	{
		bPoseChannel *pchan;
		bConstraint *con;
		int found = 0;
		
		/* DO SOME MAGIC HERE */
		for (pchan= rg->ob->pose->chanbase.first; pchan; pchan= pchan->next)
		{
			for (con= pchan->constraints.first; con; con= con->next) 
			{
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				/* constraint targets */
				if (cti && cti->get_constraint_targets)
				{
					int target_index;
					
					cti->get_constraint_targets(con, &targets);
					
					for (target_index = 0, ct= targets.first; ct; target_index++, ct= ct->next)
					{
						if ((ct->tar == rg->ob) && strcmp(ct->subtarget, ctrl->bone->name) == 0)
						{
							/* SET bone link to bone corresponding to pchan */
							EditBone *link = BLI_ghash_lookup(rg->bones_map, pchan->name);
							
							/* Making sure bone is in this armature */
							if (link != NULL)
							{
								/* for pole targets, link to parent bone instead, if possible */
								if (con->type == CONSTRAINT_TYPE_KINEMATIC && target_index == 1)
								{
									if (link->parent && BLI_ghash_haskey(rg->bones_map, link->parent->name))
									{
										link = link->parent;
									}
								}
								
								found = RIG_parentControl(ctrl, link);
							}
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 0);
				}
			}
		}

		/* if not found yet, check parent */
		if (found == 0)
		{		
			if (ctrl->bone->parent)
			{
				/* make sure parent is a deforming bone
				 * NULL if not
				 *  */
				EditBone *link = BLI_ghash_lookup(rg->bones_map, ctrl->bone->parent->name);
				
				found = RIG_parentControl(ctrl, link);
			}
			
			/* check if bone is not superposed on another one */
			{
				RigArc *arc;
				RigArc *best_arc = NULL;
				EditBone *link = NULL;
				
				for (arc = rg->arcs.first; arc; arc = arc->next)
				{
					RigEdge *edge;
					for (edge = arc->edges.first; edge; edge = edge->next)
					{
						if (edge->bone)
						{
							int fit = 0;
							
							fit = VecLenf(ctrl->bone->head, edge->bone->head) < 0.0001;
							fit = fit || VecLenf(ctrl->bone->tail, edge->bone->tail) < 0.0001;
							
							if (fit)
							{
								/* pick the bone on the arc with the lowest symmetry level
								 * means you connect control to the trunk of the skeleton */
								if (best_arc == NULL || arc->symmetry_level < best_arc->symmetry_level)
								{
									best_arc = arc;
									link = edge->bone;
								}
							}
						}
					}
				}
				
				found = RIG_parentControl(ctrl, link);
			}
		}
		
		/* if not found yet, check child */		
		if (found == 0)
		{
			RigArc *arc;
			RigArc *best_arc = NULL;
			EditBone *link = NULL;
			
			for (arc = rg->arcs.first; arc; arc = arc->next)
			{
				RigEdge *edge;
				for (edge = arc->edges.first; edge; edge = edge->next)
				{
					if (edge->bone && edge->bone->parent == ctrl->bone)
					{
						/* pick the bone on the arc with the lowest symmetry level
						 * means you connect control to the trunk of the skeleton */
						if (best_arc == NULL || arc->symmetry_level < best_arc->symmetry_level)
						{
							best_arc = arc;
							link = edge->bone;
						}
					}
				}
			}
			
			found = RIG_parentControl(ctrl, link);
		}

	}
	
	
	/* second pass, make chains in control bones */
	while (change)
	{
		change = 0;
		
		for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
		{
			/* if control is not linked yet */
			if (ctrl->link == NULL)
			{
				bPoseChannel *pchan;
				bConstraint *con;
				RigControl *ctrl_parent = NULL;
				RigControl *ctrl_child;
				int found = 0;

				if (ctrl->bone->parent)
				{
					ctrl_parent = BLI_ghash_lookup(rg->controls_map, ctrl->bone->parent->name);
				}

				/* check constraints first */
				
				/* DO SOME MAGIC HERE */
				for (pchan= rg->ob->pose->chanbase.first; pchan; pchan= pchan->next)
				{
					for (con= pchan->constraints.first; con; con= con->next) 
					{
						bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
						ListBase targets = {NULL, NULL};
						bConstraintTarget *ct;
						
						/* constraint targets */
						if (cti && cti->get_constraint_targets)
						{
							cti->get_constraint_targets(con, &targets);
							
							for (ct= targets.first; ct; ct= ct->next)
							{
								if ((ct->tar == rg->ob) && strcmp(ct->subtarget, ctrl->bone->name) == 0)
								{
									/* SET bone link to ctrl corresponding to pchan */
									RigControl *link = BLI_ghash_lookup(rg->controls_map, pchan->name);

									/* if owner is a control bone, link with it */									
									if (link && link->link)
									{
										RIG_parentControl(ctrl, link->bone);
										found = 1;
										break;
									}
								}
							}
							
							if (cti->flush_constraint_targets)
								cti->flush_constraint_targets(con, &targets, 0);
						}
					}
				}			

				if (found == 0)
				{
					/* check if parent is already linked */
					if (ctrl_parent && ctrl_parent->link)
					{
						RIG_parentControl(ctrl, ctrl_parent->bone);
						change = 1;
					}
					else
					{
						/* check childs */
						for (ctrl_child = rg->controls.first; ctrl_child; ctrl_child = ctrl_child->next)
						{
							/* if a child is linked, link to that one */
							if (ctrl_child->link && ctrl_child->bone->parent == ctrl->bone)
							{
								RIG_parentControl(ctrl, ctrl_child->bone);
								change = 1;
								break;
							}
						}
					}
				}
			}
		}
	}
	
	/* third pass, link control tails */
	for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
	{
		/* fit bone already means full match, so skip those */
		if ((ctrl->flag & RIG_CTRL_FIT_BONE) == 0)
		{
			GHashIterator ghi;
			
			/* look on deform bones first */
			BLI_ghashIterator_init(&ghi, rg->bones_map);
			
			for( ; !BLI_ghashIterator_isDone(&ghi); BLI_ghashIterator_step(&ghi))
			{
				EditBone *bone = (EditBone*)BLI_ghashIterator_getValue(&ghi);
				
				/* don't link with parent */
				if (bone->parent != ctrl->bone)
				{
					if (VecLenf(ctrl->bone->tail, bone->head) < 0.01)
					{
						ctrl->tail_mode = TL_HEAD;
						ctrl->link_tail = bone;
						break;
					}
					else if (VecLenf(ctrl->bone->tail, bone->tail) < 0.01)
					{
						ctrl->tail_mode = TL_TAIL;
						ctrl->link_tail = bone;
						break;
					}
				}
			}
			
			/* if we haven't found one yet, look in control bones */
			if (ctrl->tail_mode == TL_NONE)
			{
			}
		}
	}
	
}

/*******************************************************************************************************/

static void RIG_joinArcs(RigGraph *rg, RigNode *node, RigArc *joined_arc1, RigArc *joined_arc2)
{
	RigEdge *edge, *next_edge;
	
	/* ignore cases where joint is at start or end */
	if (joined_arc1->head == joined_arc2->head || joined_arc1->tail == joined_arc2->tail)
	{
		return;
	}
	
	/* swap arcs to make sure arc1 is before arc2 */
	if (joined_arc1->head == joined_arc2->tail)
	{
		RigArc *tmp = joined_arc1;
		joined_arc1 = joined_arc2;
		joined_arc2 = tmp;
	}
	
	for (edge = joined_arc2->edges.first; edge; edge = next_edge)
	{
		next_edge = edge->next;
		
		RIG_appendEdgeToArc(joined_arc1, edge);
	}
	
	joined_arc1->tail = joined_arc2->tail;
	
	joined_arc2->edges.first = joined_arc2->edges.last = NULL;
	
	BLI_removeArc((BGraph*)rg, (BArc*)joined_arc2);
	
	BLI_removeNode((BGraph*)rg, (BNode*)node);
}

static void RIG_removeNormalNodes(RigGraph *rg)
{
	RigNode *node, *next_node;
	
	for (node = rg->nodes.first; node; node = next_node)
	{
		next_node = node->next;
		
		if (node->degree == 2)
		{
			RigArc *arc, *joined_arc1 = NULL, *joined_arc2 = NULL;
			
			for (arc = rg->arcs.first; arc; arc = arc->next)
			{
				if (arc->head == node || arc->tail == node)
				{
					if (joined_arc1 == NULL)
					{
						joined_arc1 = arc;
					}
					else
					{
						joined_arc2 = arc;
						break;
					}
				}
			}
			
			RIG_joinArcs(rg, node, joined_arc1, joined_arc2);
		}
	}
}

static void RIG_removeUneededOffsets(RigGraph *rg)
{
	RigArc *arc;
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RigEdge *first_edge, *last_edge;
		
		first_edge = arc->edges.first;
		last_edge = arc->edges.last;
		
		if (first_edge->bone == NULL)
		{
			if (first_edge->bone == NULL && VecLenf(first_edge->tail, arc->head->p) <= 0.001)
			{
				BLI_remlink(&arc->edges, first_edge);
				MEM_freeN(first_edge);
			}
			else if (arc->head->degree == 1)
			{
				RigNode *new_node = (RigNode*)BLI_FindNodeByPosition((BGraph*)rg, first_edge->tail, 0.001);
				
				if (new_node)
				{
					BLI_remlink(&arc->edges, first_edge);
					MEM_freeN(first_edge);
					BLI_replaceNodeInArc((BGraph*)rg, (BArc*)arc, (BNode*)new_node, (BNode*)arc->head);
				}
				else
				{
					RigEdge *next_edge = first_edge->next;
	
					if (next_edge)
					{
						BLI_remlink(&arc->edges, first_edge);
						MEM_freeN(first_edge);
						
						VECCOPY(arc->head->p, next_edge->head);
					}
				}
			}
			else
			{
				/* check if all arc connected start with a null edge */
				RigArc *other_arc;
				for (other_arc = rg->arcs.first; other_arc; other_arc = other_arc->next)
				{
					if (other_arc != arc)
					{
						RigEdge *test_edge;
						if (other_arc->head == arc->head)
						{
							test_edge = other_arc->edges.first;
							
							if (test_edge->bone != NULL)
							{
								break;
							}
						}
						else if (other_arc->tail == arc->head)
						{
							test_edge = other_arc->edges.last;
							
							if (test_edge->bone != NULL)
							{
								break;
							}
						}
					}
				}
				
				if (other_arc == NULL)
				{
					RigNode *new_node = (RigNode*)BLI_FindNodeByPosition((BGraph*)rg, first_edge->tail, 0.001);
					
					if (new_node)
					{
						/* remove null edge in other arcs too */
						for (other_arc = rg->arcs.first; other_arc; other_arc = other_arc->next)
						{
							if (other_arc != arc)
							{
								RigEdge *test_edge;
								if (other_arc->head == arc->head)
								{
									BLI_replaceNodeInArc((BGraph*)rg, (BArc*)other_arc, (BNode*)new_node, (BNode*)other_arc->head);
									test_edge = other_arc->edges.first;
									BLI_remlink(&other_arc->edges, test_edge);
									MEM_freeN(test_edge);
								}
								else if (other_arc->tail == arc->head)
								{
									BLI_replaceNodeInArc((BGraph*)rg, (BArc*)other_arc, (BNode*)new_node, (BNode*)other_arc->tail);
									test_edge = other_arc->edges.last;
									BLI_remlink(&other_arc->edges, test_edge);
									MEM_freeN(test_edge);
								}
							}
						}
						
						BLI_remlink(&arc->edges, first_edge);
						MEM_freeN(first_edge);
						BLI_replaceNodeInArc((BGraph*)rg, (BArc*)arc, (BNode*)new_node, (BNode*)arc->head);
					}
					else
					{
						RigEdge *next_edge = first_edge->next;
		
						if (next_edge)
						{
							BLI_remlink(&arc->edges, first_edge);
							MEM_freeN(first_edge);
							
							VECCOPY(arc->head->p, next_edge->head);
							
							/* remove null edge in other arcs too */
							for (other_arc = rg->arcs.first; other_arc; other_arc = other_arc->next)
							{
								if (other_arc != arc)
								{
									RigEdge *test_edge;
									if (other_arc->head == arc->head)
									{
										test_edge = other_arc->edges.first;
										BLI_remlink(&other_arc->edges, test_edge);
										MEM_freeN(test_edge);
									}
									else if (other_arc->tail == arc->head)
									{
										test_edge = other_arc->edges.last;
										BLI_remlink(&other_arc->edges, test_edge);
										MEM_freeN(test_edge);
									}
								}
							}
						}
					}
				}
			}
		}
		
		if (last_edge->bone == NULL)
		{
			if (VecLenf(last_edge->head, arc->tail->p) <= 0.001)
			{
				BLI_remlink(&arc->edges, last_edge);
				MEM_freeN(last_edge);
			}
			else if (arc->tail->degree == 1)
			{
				RigNode *new_node = (RigNode*)BLI_FindNodeByPosition((BGraph*)rg, last_edge->head, 0.001);
				
				if (new_node)
				{
					RigEdge *previous_edge = last_edge->prev;
					
					BLI_remlink(&arc->edges, last_edge);
					MEM_freeN(last_edge);
					BLI_replaceNodeInArc((BGraph*)rg, (BArc*)arc, (BNode*)new_node, (BNode*)arc->tail);
					
					/* set previous angle to 0, since there's no following edges */
					if (previous_edge)
					{
						previous_edge->angle = 0;
					}
				}
				else
				{
					RigEdge *previous_edge = last_edge->prev;
	
					if (previous_edge)
					{
						BLI_remlink(&arc->edges, last_edge);
						MEM_freeN(last_edge);
						
						VECCOPY(arc->tail->p, previous_edge->tail);
						previous_edge->angle = 0;
					}
				}
			}
		}
	}
}

static void RIG_arcFromBoneChain(RigGraph *rg, ListBase *list, EditBone *root_bone, RigNode *starting_node, int selected)
{
	EditBone *bone, *last_bone = root_bone;
	RigArc *arc = NULL;
	int contain_head = 0;
	
	for(bone = root_bone; bone; bone = nextEditBoneChild(list, bone, 0))
	{
		int nb_children;
		
		if (selected == 0 || (bone->flag & BONE_SELECTED))
		{ 
			if ((bone->flag & BONE_NO_DEFORM) == 0)
			{
				BLI_ghash_insert(rg->bones_map, bone->name, bone);
			
				if (arc == NULL)
				{
					arc = newRigArc(rg);
					
					if (starting_node == NULL)
					{
						starting_node = newRigNodeHead(rg, arc, root_bone->head);
					}
					else
					{
						addRigNodeHead(rg, arc, starting_node);
					}
				}
				
				if (bone->parent && (bone->flag & BONE_CONNECTED) == 0)
				{
					RIG_addEdgeToArc(arc, bone->head, NULL);
				}
				
				RIG_addEdgeToArc(arc, bone->tail, bone);
				
				last_bone = bone;
				
				if (strcmp(bone->name, "head") == 0)
				{
					contain_head = 1;
				}
			}
			else if ((bone->flag & BONE_EDITMODE_LOCKED) == 0) /* ignore locked bones */
			{
				RIG_addControlBone(rg, bone);
			}
		}
		
		nb_children = countEditBoneChildren(list, bone);
		if (nb_children > 1)
		{
			RigNode *end_node = NULL;
			int i;
			
			if (arc != NULL)
			{
				end_node = newRigNodeTail(rg, arc, bone->tail);
			}
			else
			{
				end_node = newRigNode(rg, bone->tail);
			}

			for (i = 0; i < nb_children; i++)
			{
				root_bone = nextEditBoneChild(list, bone, i);
				RIG_arcFromBoneChain(rg, list, root_bone, end_node, selected);
			}
			
			/* arc ends here, break */
			break;
		}
	}
	
	/* If the loop exited without forking */
	if (arc != NULL && bone == NULL)
	{
		newRigNodeTail(rg, arc, last_bone->tail);
	}

	if (contain_head)
	{
		rg->head = arc->tail;
	}
}

/*******************************************************************************************************/
static void RIG_findHead(RigGraph *rg)
{
	if (rg->head == NULL)
	{
		if (BLI_countlist(&rg->arcs) == 1)
		{
			RigArc *arc = rg->arcs.first;
			
			rg->head = (RigNode*)arc->head;
		}
		else
		{
			RigArc *arc;
			
			for (arc = rg->arcs.first; arc; arc = arc->next)
			{
				RigEdge *edge = arc->edges.last;
				
				if (edge->bone->flag & (BONE_TIPSEL|BONE_SELECTED))
				{
					rg->head = arc->tail;
					break;
				}
			}
		}
		
		if (rg->head == NULL)
		{
			rg->head = rg->nodes.first;
		}
	}
}

/*******************************************************************************************************/

void RIG_printNode(RigNode *node, char name[])
{
	printf("%s %p %i <%0.3f, %0.3f, %0.3f>\n", name, node, node->degree, node->p[0], node->p[1], node->p[2]);
	
	if (node->symmetry_flag & SYM_TOPOLOGICAL)
	{
		if (node->symmetry_flag & SYM_AXIAL)
			printf("Symmetry AXIAL\n");
		else if (node->symmetry_flag & SYM_RADIAL)
			printf("Symmetry RADIAL\n");
			
		printvecf("symmetry axis", node->symmetry_axis);
	}
}

void RIG_printArcBones(RigArc *arc)
{
	RigEdge *edge;

	for (edge = arc->edges.first; edge; edge = edge->next)
	{
		if (edge->bone)
			printf("%s ", edge->bone->name);
		else
			printf("---- ");
	}
	printf("\n");
}

void RIG_printCtrl(RigControl *ctrl, char *indent)
{
	char text[128];
	
	printf("%sBone: %s\n", indent, ctrl->bone->name);
	printf("%sLink: %s\n", indent, ctrl->link ? ctrl->link->name : "!NONE!");
	
	sprintf(text, "%soffset", indent);
	printvecf(text, ctrl->offset);
	
	printf("%sFlag: %i\n", indent, ctrl->flag);
}

void RIG_printLinkedCtrl(RigGraph *rg, EditBone *bone, int tabs)
{
	RigControl *ctrl;
	char indent[64];
	char *s = indent;
	int i;
	
	for (i = 0; i < tabs; i++)
	{
		s[0] = '\t';
		s++;
	}
	s[0] = 0;
	
	for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
	{
		if (ctrl->link == bone)
		{
			RIG_printCtrl(ctrl, indent);
			RIG_printLinkedCtrl(rg, ctrl->bone, tabs + 1);
		}
	}
}

void RIG_printArc(RigGraph *rg, RigArc *arc)
{
	RigEdge *edge;

	RIG_printNode((RigNode*)arc->head, "head");

	for (edge = arc->edges.first; edge; edge = edge->next)
	{
		printf("\tinner joints %0.3f %0.3f %0.3f\n", edge->tail[0], edge->tail[1], edge->tail[2]);
		printf("\t\tlength %f\n", edge->length);
		printf("\t\tangle %f\n", edge->angle * 180 / M_PI);
		if (edge->bone)
		{
			printf("\t\t%s\n", edge->bone->name);
			RIG_printLinkedCtrl(rg, edge->bone, 3);
		}
	}	
	printf("symmetry level: %i flag: %i group %i\n", arc->symmetry_level, arc->symmetry_flag, arc->symmetry_group);

	RIG_printNode((RigNode*)arc->tail, "tail");
}

void RIG_printGraph(RigGraph *rg)
{
	RigArc *arc;

	printf("---- ARCS ----\n");
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RIG_printArc(rg, arc);	
		printf("\n");
	}

	if (rg->head)
	{
		RIG_printNode(rg->head, "HEAD NODE:");
	}
	else
	{
		printf("HEAD NODE: NONE\n");
	}	
}

/*******************************************************************************************************/

RigGraph *RIG_graphFromArmature(const bContext *C, Object *ob, bArmature *arm)
{
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	EditBone *ebone;
	RigGraph *rg;
 	
	rg = newRigGraph();
	
	if (obedit == ob)
	{
		bArmature *arm = obedit->data;
		rg->editbones = arm->edbo;
	}
	else
	{
		rg->editbones = MEM_callocN(sizeof(ListBase), "EditBones");
		make_boneList(rg->editbones, &arm->bonebase, NULL, NULL);
		rg->flag |= RIG_FREE_BONELIST;
	}
	
	rg->ob = ob;

	/* Do the rotations */
	for (ebone = rg->editbones->first; ebone; ebone=ebone->next){
		if (ebone->parent == NULL)
		{
			RIG_arcFromBoneChain(rg, rg->editbones, ebone, NULL, 0);
		}
	}
	
	BLI_removeDoubleNodes((BGraph*)rg, 0.001);
	
	RIG_removeNormalNodes(rg);
	
	RIG_removeUneededOffsets(rg);
	
	BLI_buildAdjacencyList((BGraph*)rg);
	
	RIG_findHead(rg);

	BLI_markdownSymmetry((BGraph*)rg, (BNode*)rg->head, scene->toolsettings->skgen_symmetry_limit);
	
	RIG_reconnectControlBones(rg); /* after symmetry, because we use levels to find best match */
	
	if (BLI_isGraphCyclic((BGraph*)rg))
	{
		printf("armature cyclic\n");
	}
	
	return rg;
}

RigGraph *armatureSelectedToGraph(bContext *C, Object *ob, bArmature *arm)
{
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	EditBone *ebone;
	RigGraph *rg;
 	
	rg = newRigGraph();
	
	if (obedit == ob)
	{
		rg->editbones = arm->edbo;
	}
	else
	{
		rg->editbones = MEM_callocN(sizeof(ListBase), "EditBones");
		make_boneList(rg->editbones, &arm->bonebase, NULL, NULL);
		rg->flag |= RIG_FREE_BONELIST;
	}

	rg->ob = ob;

	/* Do the rotations */
	for (ebone = rg->editbones->first; ebone; ebone=ebone->next){
		if (ebone->parent == NULL)
		{
			RIG_arcFromBoneChain(rg, rg->editbones, ebone, NULL, 1);
		}
	}
	
	BLI_removeDoubleNodes((BGraph*)rg, 0.001);
	
	RIG_removeNormalNodes(rg);
	
	RIG_removeUneededOffsets(rg);
	
	BLI_buildAdjacencyList((BGraph*)rg);
	
	RIG_findHead(rg);

	BLI_markdownSymmetry((BGraph*)rg, (BNode*)rg->head, scene->toolsettings->skgen_symmetry_limit);
	
	RIG_reconnectControlBones(rg); /* after symmetry, because we use levels to find best match */
	
	if (BLI_isGraphCyclic((BGraph*)rg))
	{
		printf("armature cyclic\n");
	}
	
	return rg;
}
/************************************ GENERATING *****************************************************/

#if 0
static EditBone *add_editbonetolist(char *name, ListBase *list)
{
	EditBone *bone= MEM_callocN(sizeof(EditBone), "eBone");
	
	BLI_strncpy(bone->name, name, 32);
	unique_editbone_name(list, bone->name, NULL);
	
	BLI_addtail(list, bone);
	
	bone->flag |= BONE_TIPSEL;
	bone->weight= 1.0F;
	bone->dist= 0.25F;
	bone->xwidth= 0.1;
	bone->zwidth= 0.1;
	bone->ease1= 1.0;
	bone->ease2= 1.0;
	bone->rad_head= 0.10;
	bone->rad_tail= 0.05;
	bone->segments= 1;
	bone->layer=  1;//arm->layer;
	
	return bone;
}
#endif

void generateMissingArcsFromNode(RigGraph *rigg, ReebNode *node, int multi_level_limit)
{
	while (node->multi_level > multi_level_limit && node->link_up)
	{
		node = node->link_up;
	}
	
	while (node->multi_level < multi_level_limit && node->link_down)
	{
		node = node->link_down;
	}
	
	if (node->multi_level == multi_level_limit)
	{
		int i;
		
		for (i = 0; i < node->degree; i++)
		{
			ReebArc *earc = node->arcs[i];
			
			if (earc->flag == ARC_FREE && earc->head == node)
			{
				ReebNode *other = BIF_otherNodeFromIndex(earc, node);
				
				earc->flag = ARC_USED;
				
				//generateBonesForArc(rigg, earc, node, other);
				generateMissingArcsFromNode(rigg, other, multi_level_limit);
			}
		}
	}
}

void generateMissingArcs(RigGraph *rigg)
{
	ReebGraph *reebg = rigg->link_mesh;
	int multi_level_limit = 5;
	
	for (reebg = rigg->link_mesh; reebg; reebg = reebg->link_up)
	{
		ReebArc *earc;
		
		for (earc = reebg->arcs.first; earc; earc = earc->next)
		{
			if (earc->flag == ARC_USED)
			{
				generateMissingArcsFromNode(rigg, earc->head, multi_level_limit);
				generateMissingArcsFromNode(rigg, earc->tail, multi_level_limit);
			}
		}
	}
}

/************************************ RETARGETTING *****************************************************/

static void repositionControl(RigGraph *rigg, RigControl *ctrl, float head[3], float tail[3], float qrot[4], float resize);

static void repositionTailControl(RigGraph *rigg, RigControl *ctrl);

static void finalizeControl(RigGraph *rigg, RigControl *ctrl, float resize)
{
	if ((ctrl->flag & RIG_CTRL_DONE) == RIG_CTRL_DONE)
	{
		RigControl *ctrl_child;

#if 0		
		printf("CTRL: %s LINK: %s", ctrl->bone->name, ctrl->link->name);
		
		if (ctrl->link_tail)
		{
			printf(" TAIL: %s", ctrl->link_tail->name);
		}
		
		printf("\n");
#endif
		
		/* if there was a tail link: apply link, recalc resize factor and qrot */
		if (ctrl->tail_mode != TL_NONE)
		{
			float *tail_vec = NULL;
			float v1[3], v2[3], qtail[4];
			
			if (ctrl->tail_mode == TL_TAIL)
			{
				tail_vec = ctrl->link_tail->tail;
			}
			else if (ctrl->tail_mode == TL_HEAD)
			{
				tail_vec = ctrl->link_tail->head;
			}
			
			VecSubf(v1, ctrl->bone->tail, ctrl->bone->head);
			VecSubf(v2, tail_vec, ctrl->bone->head);
			
			VECCOPY(ctrl->bone->tail, tail_vec);
			
			RotationBetweenVectorsToQuat(qtail, v1, v2);
			QuatMul(ctrl->qrot, qtail, ctrl->qrot);
			
			resize = VecLength(v2) / VecLenf(ctrl->head, ctrl->tail);
		}
		
		ctrl->bone->roll = rollBoneByQuat(ctrl->bone, ctrl->up_axis, ctrl->qrot);
	
		/* Cascade to connected control bones */
		for (ctrl_child = rigg->controls.first; ctrl_child; ctrl_child = ctrl_child->next)
		{
			if (ctrl_child->link == ctrl->bone)
			{
				repositionControl(rigg, ctrl_child, ctrl->bone->head, ctrl->bone->tail, ctrl->qrot, resize);
			}
			if (ctrl_child->link_tail == ctrl->bone)
			{
				repositionTailControl(rigg, ctrl_child);
			}
		}
	}	
}

static void repositionTailControl(RigGraph *rigg, RigControl *ctrl)
{
	ctrl->flag |= RIG_CTRL_TAIL_DONE;

	finalizeControl(rigg, ctrl, 1); /* resize will be recalculated anyway so we don't need it */
}

static void repositionControl(RigGraph *rigg, RigControl *ctrl, float head[3], float tail[3], float qrot[4], float resize)
{
	float parent_offset[3], tail_offset[3];
	
	VECCOPY(parent_offset, ctrl->offset);
	VecMulf(parent_offset, resize);
	QuatMulVecf(qrot, parent_offset);
	
	VecAddf(ctrl->bone->head, head, parent_offset); 

	ctrl->flag |= RIG_CTRL_HEAD_DONE;

	QUATCOPY(ctrl->qrot, qrot); 

	if (ctrl->tail_mode == TL_NONE)
	{
		VecSubf(tail_offset, ctrl->tail, ctrl->head);
		VecMulf(tail_offset, resize);
		QuatMulVecf(qrot, tail_offset);

		VecAddf(ctrl->bone->tail, ctrl->bone->head, tail_offset);
		
		ctrl->flag |= RIG_CTRL_TAIL_DONE;
	}

	finalizeControl(rigg, ctrl, resize);
}

static void repositionBone(bContext *C, RigGraph *rigg, RigEdge *edge, float vec0[3], float vec1[3], float up_axis[3])
{
	Scene *scene = CTX_data_scene(C);
	EditBone *bone;
	RigControl *ctrl;
	float qrot[4], resize;
	float v1[3], v2[3];
	float l1, l2;
	
	bone = edge->bone;
	
	VecSubf(v1, edge->tail, edge->head);
	VecSubf(v2, vec1, vec0);
	
	l1 = Normalize(v1);
	l2 = Normalize(v2);

	resize = l2 / l1;
	
	RotationBetweenVectorsToQuat(qrot, v1, v2);
	
	VECCOPY(bone->head, vec0);
	VECCOPY(bone->tail, vec1);
	
	if (!VecIsNull(up_axis))
	{
		float qroll[4];

		if (scene->toolsettings->skgen_retarget_roll == SK_RETARGET_ROLL_VIEW)
		{
			bone->roll = rollBoneByQuatAligned(bone, edge->up_axis, qrot, qroll, up_axis);
		}
		else if (scene->toolsettings->skgen_retarget_roll == SK_RETARGET_ROLL_JOINT)
		{
			bone->roll = rollBoneByQuatJoint(edge, edge->prev, qrot, qroll, up_axis);
		}
		else
		{
			QuatOne(qroll);
		}
		
		QuatMul(qrot, qroll, qrot);
	}
	else
	{
		bone->roll = rollBoneByQuat(bone, edge->up_axis, qrot);
	}

	for (ctrl = rigg->controls.first; ctrl; ctrl = ctrl->next)
	{
		if (ctrl->link == bone)
		{
			repositionControl(rigg, ctrl, vec0, vec1, qrot, resize);
		}
		if (ctrl->link_tail == bone)
		{
			repositionTailControl(rigg, ctrl);
		}
	}
}

static RetargetMode detectArcRetargetMode(RigArc *arc);
static void retargetArctoArcLength(bContext *C, RigGraph *rigg, RigArc *iarc, RigNode *inode_start);


static RetargetMode detectArcRetargetMode(RigArc *iarc)
{
	RetargetMode mode = RETARGET_AGGRESSIVE;
	ReebArc *earc = iarc->link_mesh;
	RigEdge *edge;
	int large_angle = 0;
	float avg_angle = 0;
	float avg_length = 0;
	int nb_edges = 0;
	
	
	for (edge = iarc->edges.first; edge; edge = edge->next)
	{
		avg_angle += edge->angle;
		nb_edges++;
	}
	
	avg_angle /= nb_edges - 1; /* -1 because last edge doesn't have an angle */

	avg_length = iarc->length / nb_edges;
	
	
	if (nb_edges > 2)
	{
		for (edge = iarc->edges.first; edge; edge = edge->next)
		{
			if (fabs(edge->angle - avg_angle) > M_PI / 6)
			{
				large_angle = 1;
			}
		}
	}
	else if (nb_edges == 2 && avg_angle > 0)
	{
		large_angle = 1;
	}
		
	
	if (large_angle == 0)
	{
		mode = RETARGET_LENGTH;
	}
	
	if (earc->bcount <= (iarc->count - 1))
	{
		mode = RETARGET_LENGTH;
	}
	
	mode = RETARGET_AGGRESSIVE;
	
	return mode;
}

#ifndef USE_THREADS
static void printMovesNeeded(int *positions, int nb_positions)
{
	int moves = 0;
	int i;
	
	for (i = 0; i < nb_positions; i++)
	{
		moves += positions[i] - (i + 1);
	}
	
	printf("%i moves needed\n", moves);
}

static void printPositions(int *positions, int nb_positions)
{
	int i;
	
	for (i = 0; i < nb_positions; i++)
	{
		printf("%i ", positions[i]);
	}
	printf("\n");
}
#endif

#define MAX_COST FLT_MAX /* FIX ME */

static float costDistance(BArcIterator *iter, float *vec0, float *vec1, int i0, int i1, float distance_weight)
{
	EmbedBucket *bucket = NULL;
	float max_dist = 0;
	float v1[3], v2[3], c[3];
	float v1_inpf;

	if (distance_weight > 0)
	{
		VecSubf(v1, vec0, vec1);
		
		v1_inpf = Inpf(v1, v1);
		
		if (v1_inpf > 0)
		{
			int j;
			for (j = i0 + 1; j < i1 - 1; j++)
			{
				float dist;
				
				bucket = IT_peek(iter, j);
	
				VecSubf(v2, bucket->p, vec1);
		
				Crossf(c, v1, v2);
				
				dist = Inpf(c, c) / v1_inpf;
				
				max_dist = dist > max_dist ? dist : max_dist;
			}
			
			return distance_weight * max_dist;
		}
		else
		{
			return MAX_COST;
		}
	}
	else
	{
		return 0;
	}
}

static float costAngle(float original_angle, float vec_first[3], float vec_second[3], float angle_weight)
{
	if (angle_weight > 0)
	{
		float current_angle;
		
		if (!VecIsNull(vec_first) && !VecIsNull(vec_second))
		{
			current_angle = saacos(Inpf(vec_first, vec_second));

			return angle_weight * fabs(current_angle - original_angle);
		}
		else
		{
			return angle_weight * M_PI;
		}
	}
	else
	{
		return 0;
	}
}

static float costLength(float original_length, float current_length, float length_weight)
{
	if (current_length == 0)
	{
		return MAX_COST;
	}
	else
	{
		float length_ratio = fabs((current_length - original_length) / original_length);
		return length_weight * length_ratio * length_ratio;
	}
}

#if 0
static float calcCostLengthDistance(BArcIterator *iter, float **vec_cache, RigEdge *edge, float *vec1, float *vec2, int i1, int i2)
{
	float vec[3];
	float length;

	VecSubf(vec, vec2, vec1);
	length = Normalize(vec);

	return costLength(edge->length, length) + costDistance(iter, vec1, vec2, i1, i2);
}
#endif

static float calcCostAngleLengthDistance(BArcIterator *iter, float **vec_cache, RigEdge *edge, float *vec0, float *vec1, float *vec2, int i1, int i2, float angle_weight, float length_weight, float distance_weight)
{
	float vec_second[3], vec_first[3];
	float length2;
	float new_cost = 0;

	VecSubf(vec_second, vec2, vec1);
	length2 = Normalize(vec_second);


	/* Angle cost */	
	if (edge->prev)
	{
		VecSubf(vec_first, vec1, vec0); 
		Normalize(vec_first);
		
		new_cost += costAngle(edge->prev->angle, vec_first, vec_second, angle_weight);
	}

	/* Length cost */
	new_cost += costLength(edge->length, length2, length_weight);

	/* Distance cost */
	new_cost += costDistance(iter, vec1, vec2, i1, i2, distance_weight);

	return new_cost;
}

static int indexMemoNode(int nb_positions, int previous, int current, int joints_left)
{
	return joints_left * nb_positions * nb_positions + current * nb_positions + previous;
}

static void copyMemoPositions(int *positions, MemoNode *table, int nb_positions, int joints_left)
{
	int previous = 0, current = 0;
	int i = 0;
	
	for (i = 0; joints_left > 0; joints_left--, i++)
	{
		MemoNode *node;
		node = table + indexMemoNode(nb_positions, previous, current, joints_left);
		
		positions[i] = node->next;
		
		previous = current;
		current = node->next;
	}
}

static MemoNode * solveJoints(MemoNode *table, BArcIterator *iter, float **vec_cache, int nb_joints, int nb_positions, int previous, int current, RigEdge *edge, int joints_left, float angle_weight, float length_weight, float distance_weight)
{
	MemoNode *node;
	int index = indexMemoNode(nb_positions, previous, current, joints_left);
	
	node = table + index;
	
	if (node->weight != 0)
	{
		return node;
	}
	else if (joints_left == 0)
	{
		float *vec0 = vec_cache[previous];
		float *vec1 = vec_cache[current];
		float *vec2 = vec_cache[nb_positions + 1];

		node->weight = calcCostAngleLengthDistance(iter, vec_cache, edge, vec0, vec1, vec2, current, iter->length, angle_weight, length_weight, distance_weight);

		return node;
	}
	else
	{
		MemoNode *min_node = NULL;
		float *vec0 = vec_cache[previous];
		float *vec1 = vec_cache[current];
		float min_weight= 0.0f;
		int min_next= 0;
		int next;
		
		for (next = current + 1; next <= nb_positions - (joints_left - 1); next++)
		{
			MemoNode *next_node;
			float *vec2 = vec_cache[next];
			float weight = 0.0f;
			
			/* ADD WEIGHT OF PREVIOUS - CURRENT - NEXT triple */
			weight = calcCostAngleLengthDistance(iter, vec_cache, edge, vec0, vec1, vec2, current, next, angle_weight, length_weight, distance_weight);
			
			if (weight >= MAX_COST)
			{
				continue;
			}
			
			/* add node weight */
			next_node = solveJoints(table, iter, vec_cache, nb_joints, nb_positions, current, next, edge->next, joints_left - 1, angle_weight, length_weight, distance_weight);
			weight += next_node->weight;
			
			if (min_node == NULL || weight < min_weight)
			{
				min_weight = weight;
				min_node = next_node;
				min_next = next;
			}
		}
		
		if (min_node)
		{
			node->weight = min_weight;
			node->next = min_next;
			return node;
		}
		else
		{
			node->weight = MAX_COST;
			return node;
		}
	}
	
}

static int testFlipArc(RigArc *iarc, RigNode *inode_start)
{
	ReebArc *earc = iarc->link_mesh;
	ReebNode *enode_start = BIF_NodeFromIndex(earc, inode_start->link_mesh);
	
	/* no flip needed if both nodes are the same */
	if ((enode_start == earc->head && inode_start == iarc->head) || (enode_start == earc->tail && inode_start == iarc->tail))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

static void retargetArctoArcAggresive(bContext *C, RigGraph *rigg, RigArc *iarc, RigNode *inode_start)
{
	ReebArcIterator arc_iter;
	BArcIterator *iter = (BArcIterator*)&arc_iter;
	RigEdge *edge;
	EmbedBucket *bucket = NULL;
	ReebNode *node_start, *node_end;
	ReebArc *earc = iarc->link_mesh;
	float angle_weight = 1.0; // GET FROM CONTEXT
	float length_weight = 1.0;
	float distance_weight = 1.0;
	float min_cost = FLT_MAX;
	float *vec0, *vec1;
	int *best_positions;
	int nb_edges = BLI_countlist(&iarc->edges);
	int nb_joints = nb_edges - 1;
	RetargetMethod method = METHOD_MEMOIZE;
	int i;
	
	if (nb_joints > earc->bcount)
	{
		printf("NOT ENOUGH BUCKETS!\n");
		return;
	}

	best_positions = MEM_callocN(sizeof(int) * nb_joints, "Best positions");
	
	if (testFlipArc(iarc, inode_start))
	{
		node_start = earc->tail;
		node_end = earc->head;
	}
	else
	{
		node_start = earc->head;
		node_end = earc->tail;
	}

	/* equal number of joints and potential position, just fill them in */
	if (nb_joints == earc->bcount)
	{
		int i;
		
		/* init with first values */
		for (i = 0; i < nb_joints; i++)
		{
			best_positions[i] = i + 1;
		}
	}
	if (method == METHOD_MEMOIZE)
	{
		int nb_positions = earc->bcount;
		int nb_memo_nodes = nb_positions * nb_positions * (nb_joints + 1);
		MemoNode *table = MEM_callocN(nb_memo_nodes * sizeof(MemoNode), "memoization table");
		MemoNode *result;
		float **positions_cache = MEM_callocN(sizeof(float*) * (nb_positions + 2), "positions cache");
		int i;
		
		positions_cache[0] = node_start->p;
		positions_cache[nb_positions + 1] = node_end->p;
		
		initArcIterator(iter, earc, node_start);

		for (i = 1; i <= nb_positions; i++)
		{
			EmbedBucket *bucket = IT_peek(iter, i);
			positions_cache[i] = bucket->p;
		}

		result = solveJoints(table, iter, positions_cache, nb_joints, earc->bcount, 0, 0, iarc->edges.first, nb_joints, angle_weight, length_weight, distance_weight);
		
		min_cost = result->weight;
		copyMemoPositions(best_positions, table, earc->bcount, nb_joints);
		
		MEM_freeN(table);
		MEM_freeN(positions_cache);
	}

	vec0 = node_start->p;
	initArcIterator(iter, earc, node_start);
	
#ifndef USE_THREADS
	printPositions(best_positions, nb_joints);
	printMovesNeeded(best_positions, nb_joints);
	printf("min_cost %f\n", min_cost);
	printf("buckets: %i\n", earc->bcount);
#endif

	/* set joints to best position */
	for (edge = iarc->edges.first, i = 0;
		 edge;
		 edge = edge->next, i++)
	{
		float *no = NULL;
		if (i < nb_joints)
		{
			bucket = IT_peek(iter, best_positions[i]);
			vec1 = bucket->p;
			no = bucket->no;
		}
		else
		{
			vec1 = node_end->p;
			no = node_end->no;
		}
		
		if (edge->bone)
		{
			repositionBone(C, rigg, edge, vec0, vec1, no);
		}
		
		vec0 = vec1;
	}

	MEM_freeN(best_positions);
}

static void retargetArctoArcLength(bContext *C, RigGraph *rigg, RigArc *iarc, RigNode *inode_start)
{
	ReebArcIterator arc_iter;
	BArcIterator *iter = (BArcIterator*)&arc_iter;
	ReebArc *earc = iarc->link_mesh;
	ReebNode *node_start, *node_end;
	RigEdge *edge;
	EmbedBucket *bucket = NULL;
	float embedding_length = 0;
	float *vec0 = NULL;
	float *vec1 = NULL;
	float *previous_vec = NULL;

	
	if (testFlipArc(iarc, inode_start))
	{
		node_start = (ReebNode*)earc->tail;
		node_end = (ReebNode*)earc->head;
	}
	else
	{
		node_start = (ReebNode*)earc->head;
		node_end = (ReebNode*)earc->tail;
	}
	
	initArcIterator(iter, earc, node_start);

	bucket = IT_next(iter);
	
	vec0 = node_start->p;
	
	while (bucket != NULL)
	{
		vec1 = bucket->p;
		
		embedding_length += VecLenf(vec0, vec1);
		
		vec0 = vec1;
		bucket = IT_next(iter);
	}
	
	embedding_length += VecLenf(node_end->p, vec1);
	
	/* fit bones */
	initArcIterator(iter, earc, node_start);

	bucket = IT_next(iter);

	vec0 = node_start->p;
	previous_vec = vec0;
	vec1 = bucket->p;
	
	for (edge = iarc->edges.first; edge; edge = edge->next)
	{
		float new_bone_length = edge->length / iarc->length * embedding_length;
		float *no = NULL;
		float length = 0;

		while (bucket && new_bone_length > length)
		{
			length += VecLenf(previous_vec, vec1);
			bucket = IT_next(iter);
			previous_vec = vec1;
			vec1 = bucket->p;
			no = bucket->no;
		}
		
		if (bucket == NULL)
		{
			vec1 = node_end->p;
			no = node_end->no;
		}

		/* no need to move virtual edges (space between unconnected bones) */		
		if (edge->bone)
		{
			repositionBone(C, rigg, edge, vec0, vec1, no);
		}
		
		vec0 = vec1;
		previous_vec = vec1;
	}
}

static void retargetArctoArc(bContext *C, RigGraph *rigg, RigArc *iarc, RigNode *inode_start)
{
#ifdef USE_THREADS
	RetargetParam *p = MEM_callocN(sizeof(RetargetParam), "RetargetParam");
	
	p->rigg = rigg;
	p->iarc = iarc;
	p->inode_start = inode_start;
	p->context = C;
	
	BLI_insert_work(rigg->worker, p);
#else
	RetargetParam p;

	p.rigg = rigg;
	p.iarc = iarc;
	p.inode_start = inode_start;
	p.context = C;
	
	exec_retargetArctoArc(&p);
#endif
}

void *exec_retargetArctoArc(void *param)
{
	RetargetParam *p = (RetargetParam*)param;
	RigGraph *rigg = p->rigg;
	RigArc *iarc = p->iarc;
	bContext *C = p->context;	
	RigNode *inode_start = p->inode_start;
	ReebArc *earc = iarc->link_mesh;
	
	if (BLI_countlist(&iarc->edges) == 1)
	{
		RigEdge *edge = iarc->edges.first;

		if (testFlipArc(iarc, inode_start))
		{
			repositionBone(C, rigg, edge, earc->tail->p, earc->head->p, earc->head->no);
		}
		else
		{
			repositionBone(C, rigg, edge, earc->head->p, earc->tail->p, earc->tail->no);
		}
	}
	else
	{
		RetargetMode mode = detectArcRetargetMode(iarc);
		
		if (mode == RETARGET_AGGRESSIVE)
		{
			retargetArctoArcAggresive(C, rigg, iarc, inode_start);
		}
		else
		{		
			retargetArctoArcLength(C, rigg, iarc, inode_start);
		}
	}

#ifdef USE_THREADS
	MEM_freeN(p);
#endif
	
	return NULL;
}

static void matchMultiResolutionNode(RigGraph *rigg, RigNode *inode, ReebNode *top_node)
{
	ReebNode *enode = top_node;
	ReebGraph *reebg = BIF_graphForMultiNode(rigg->link_mesh, enode);
	int ishape, eshape;
	
	ishape = BLI_subtreeShape((BGraph*)rigg, (BNode*)inode, NULL, 0) % SHAPE_LEVELS;
	eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	
	inode->link_mesh = enode;

	while (ishape == eshape && enode->link_down)
	{
		inode->link_mesh = enode;

		enode = enode->link_down;
		reebg = BIF_graphForMultiNode(rigg->link_mesh, enode); /* replace with call to link_down once that exists */
		eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	} 
}

static void markMultiResolutionChildArc(ReebNode *end_enode, ReebNode *enode)
{
	int i;
	
	for(i = 0; i < enode->degree; i++)
	{
		ReebArc *earc = (ReebArc*)enode->arcs[i];
		
		if (earc->flag == ARC_FREE)
		{
			earc->flag = ARC_TAKEN;
			
			if (earc->tail->degree > 1 && earc->tail != end_enode)
			{
				markMultiResolutionChildArc(end_enode, earc->tail);
			}
			break;
		}
	}
}

static void markMultiResolutionArc(ReebArc *start_earc)
{
	if (start_earc->link_up)
	{
		ReebArc *earc;
		for (earc = start_earc->link_up ; earc; earc = earc->link_up)
		{
			earc->flag = ARC_TAKEN;
			
			if (earc->tail->index != start_earc->tail->index)
			{
				markMultiResolutionChildArc(earc->tail, earc->tail);
			}
		}
	}
}

static void matchMultiResolutionArc(RigGraph *rigg, RigNode *start_node, RigArc *next_iarc, ReebArc *next_earc)
{
	ReebNode *enode = next_earc->head;
	ReebGraph *reebg = BIF_graphForMultiNode(rigg->link_mesh, enode);
	int ishape, eshape;

	ishape = BLI_subtreeShape((BGraph*)rigg, (BNode*)start_node, (BArc*)next_iarc, 1) % SHAPE_LEVELS;
	eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, (BArc*)next_earc, 1) % SHAPE_LEVELS;
	
	while (ishape != eshape && next_earc->link_up)
	{
		next_earc->flag = ARC_TAKEN; // mark previous as taken, to prevent backtrack on lower levels
		
		next_earc = next_earc->link_up;
		reebg = reebg->link_up;
		enode = next_earc->head;
		eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, (BArc*)next_earc, 1) % SHAPE_LEVELS;
	} 

	next_earc->flag = ARC_USED;
	next_iarc->link_mesh = next_earc;
	
	/* mark all higher levels as taken too */
	markMultiResolutionArc(next_earc);
//	while (next_earc->link_up)
//	{
//		next_earc = next_earc->link_up;
//		next_earc->flag = ARC_TAKEN;
//	}
}

static void matchMultiResolutionStartingNode(RigGraph *rigg, ReebGraph *reebg, RigNode *inode)
{
	ReebNode *enode;
	int ishape, eshape;
	
	enode = reebg->nodes.first;
	
	ishape = BLI_subtreeShape((BGraph*)rigg, (BNode*)inode, NULL, 0) % SHAPE_LEVELS;
	eshape = BLI_subtreeShape((BGraph*)rigg->link_mesh, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	
	while (ishape != eshape && reebg->link_up)
	{
		reebg = reebg->link_up;
		
		enode = reebg->nodes.first;
		
		eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	} 

	inode->link_mesh = enode;
}

static void findCorrespondingArc(RigGraph *rigg, RigArc *start_arc, RigNode *start_node, RigArc *next_iarc, int root)
{
	ReebNode *enode = start_node->link_mesh;
	ReebArc *next_earc;
	int symmetry_level = next_iarc->symmetry_level;
	int symmetry_group = next_iarc->symmetry_group;
	int symmetry_flag = next_iarc->symmetry_flag;
	int i;
	
	next_iarc->link_mesh = NULL;
		
//	if (root)
//	{
//		printf("-----------------------\n");
//		printf("MATCHING LIMB\n");
//		RIG_printArcBones(next_iarc);
//	}
	
	for(i = 0; i < enode->degree; i++)
	{
		next_earc = (ReebArc*)enode->arcs[i];
		
//		if (next_earc->flag == ARC_FREE)
//		{
//			printf("candidate (level %i ?= %i) (flag %i ?= %i) (group %i ?= %i)\n",
//			symmetry_level, next_earc->symmetry_level,
//			symmetry_flag, next_earc->symmetry_flag, 
//			symmetry_group, next_earc->symmetry_flag);
//		}
		
		if (next_earc->flag == ARC_FREE &&
			next_earc->symmetry_flag == symmetry_flag &&
			next_earc->symmetry_group == symmetry_group &&
			next_earc->symmetry_level == symmetry_level)
		{
//			printf("CORRESPONDING ARC FOUND\n");
//			printf("flag %i -- level %i -- flag %i -- group %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag, next_earc->symmetry_group);
			
			matchMultiResolutionArc(rigg, start_node, next_iarc, next_earc);
			break;
		}
	}
	
	/* not found, try at higher nodes (lower node might have filtered internal arcs, messing shape of tree */
	if (next_iarc->link_mesh == NULL)
	{
//		printf("NO CORRESPONDING ARC FOUND - GOING TO HIGHER LEVELS\n");
		
		if (enode->link_up)
		{
			start_node->link_mesh = enode->link_up;
			findCorrespondingArc(rigg, start_arc, start_node, next_iarc, 0);
		}
	}

	/* still not found, print debug info */
	if (root && next_iarc->link_mesh == NULL)
	{
		start_node->link_mesh = enode; /* linking back with root node */
		
//		printf("NO CORRESPONDING ARC FOUND\n");
//		RIG_printArcBones(next_iarc);
//		
//		printf("ON NODE %i, multilevel %i\n", enode->index, enode->multi_level);
//		
//		printf("LOOKING FOR\n");
//		printf("flag %i -- level %i -- flag %i -- group %i\n", ARC_FREE, symmetry_level, symmetry_flag, symmetry_group);
//		
//		printf("CANDIDATES\n");
//		for(i = 0; i < enode->degree; i++)
//		{
//			next_earc = (ReebArc*)enode->arcs[i];
//			printf("flag %i -- level %i -- flag %i -- group %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag, next_earc->symmetry_group);
//		}
		
		/* Emergency matching */
		for(i = 0; i < enode->degree; i++)
		{
			next_earc = (ReebArc*)enode->arcs[i];
			
			if (next_earc->flag == ARC_FREE && next_earc->symmetry_level == symmetry_level)
			{
//				printf("USING: \n");
//				printf("flag %i -- level %i -- flag %i -- group %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag, next_earc->symmetry_group);
				matchMultiResolutionArc(rigg, start_node, next_iarc, next_earc);
				break;
			}
		}
	}

}

static void retargetSubgraph(bContext *C, RigGraph *rigg, RigArc *start_arc, RigNode *start_node)
{
	RigNode *inode = start_node;
	int i;

	/* no start arc on first node */
	if (start_arc)
	{		
		ReebNode *enode = start_node->link_mesh;
		ReebArc *earc = start_arc->link_mesh;
		
		retargetArctoArc(C, rigg, start_arc, start_node);
		
		enode = BIF_otherNodeFromIndex(earc, enode);
		inode = (RigNode*)BLI_otherNode((BArc*)start_arc, (BNode*)inode);
	
		/* match with lowest node with correct shape */
		matchMultiResolutionNode(rigg, inode, enode);
	}
	
	for(i = 0; i < inode->degree; i++)
	{
		RigArc *next_iarc = (RigArc*)inode->arcs[i];
		
		/* no back tracking */
		if (next_iarc != start_arc)
		{
			findCorrespondingArc(rigg, start_arc, inode, next_iarc, 1);
			if (next_iarc->link_mesh)
			{
				retargetSubgraph(C, rigg, next_iarc, inode);
			}
		}
	}
}

static void finishRetarget(RigGraph *rigg)
{
#ifdef USE_THREADS
	BLI_end_worker(rigg->worker);
#endif
}

static void adjustGraphs(bContext *C, RigGraph *rigg)
{
	bArmature *arm= rigg->ob->data;
	RigArc *arc;
	
	for (arc = rigg->arcs.first; arc; arc = arc->next)
	{
		if (arc->link_mesh)
		{
			retargetArctoArc(C, rigg, arc, arc->head);
		}
	}

	finishRetarget(rigg);

	/* Turn the list into an armature */
	arm->edbo = rigg->editbones;
	ED_armature_from_edit(rigg->ob);
	
	ED_undo_push(C, "Retarget Skeleton");
}

static void retargetGraphs(bContext *C, RigGraph *rigg)
{
	bArmature *arm= rigg->ob->data;
	ReebGraph *reebg = rigg->link_mesh;
	RigNode *inode;
	
	/* flag all ReebArcs as free */
	BIF_flagMultiArcs(reebg, ARC_FREE);
	
	/* return to first level */
	reebg = rigg->link_mesh;
	
	inode = rigg->head;
	
	matchMultiResolutionStartingNode(rigg, reebg, inode);

	retargetSubgraph(C, rigg, NULL, inode);
	
	//generateMissingArcs(rigg);
	
	finishRetarget(rigg);

	/* Turn the list into an armature */
	arm->edbo = rigg->editbones;
	ED_armature_from_edit(rigg->ob);
}

char *RIG_nameBone(RigGraph *rg, int arc_index, int bone_index)
{
	RigArc *arc = BLI_findlink(&rg->arcs, arc_index);
	RigEdge *iedge;

	if (arc == NULL)
	{
		return "None";
	}
	
	if (bone_index == BLI_countlist(&arc->edges))
	{
		return "Last joint";
	}

	iedge = BLI_findlink(&arc->edges, bone_index);
	
	if (iedge == NULL)
	{
		return "Done";
	}
	
	if (iedge->bone == NULL)
	{
		return "Bone offset";
	}
	
	return iedge->bone->name;
}

int RIG_nbJoints(RigGraph *rg)
{
	RigArc *arc;
	int total = 0;
	
	total += BLI_countlist(&rg->nodes);
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		total += BLI_countlist(&arc->edges) - 1; /* -1 because end nodes are already counted */
	}
	
	return total;
}

void BIF_freeRetarget()
{
	if (GLOBAL_RIGG)
	{
		RIG_freeRigGraph((BGraph*)GLOBAL_RIGG);
		GLOBAL_RIGG = NULL;
	}
}

void BIF_retargetArmature(bContext *C)
{
	ReebGraph *reebg;
	double start_time, end_time;
	double gstart_time, gend_time;
	double reeb_time, rig_time=0.0, retarget_time=0.0, total_time;
	
	gstart_time = start_time = PIL_check_seconds_timer();
	
	reebg = BIF_ReebGraphMultiFromEditMesh(C);
	
	end_time = PIL_check_seconds_timer();
	reeb_time = end_time - start_time;
	
	printf("Reeb Graph created\n");

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		Object *ob = base->object;

		if (ob->type==OB_ARMATURE)
		{
			RigGraph *rigg;
			bArmature *arm;
		 	
			arm = ob->data;
		
			/* Put the armature into editmode */
			
		
			start_time = PIL_check_seconds_timer();

			rigg = RIG_graphFromArmature(C, ob, arm);
			
			end_time = PIL_check_seconds_timer();
			rig_time = end_time - start_time;

			printf("Armature graph created\n");
	
			//RIG_printGraph(rigg);
			
			rigg->link_mesh = reebg;
			
			printf("retargetting %s\n", ob->id.name);
			
			start_time = PIL_check_seconds_timer();

			retargetGraphs(C, rigg);
			
			end_time = PIL_check_seconds_timer();
			retarget_time = end_time - start_time;

			BIF_freeRetarget();
			
			GLOBAL_RIGG = rigg;
			
			break; /* only one armature at a time */
		}
	}
	CTX_DATA_END;

	
	gend_time = PIL_check_seconds_timer();

	total_time = gend_time - gstart_time;

	printf("-----------\n");
	printf("runtime: \t%.3f\n", total_time);
	printf("reeb: \t\t%.3f (%.1f%%)\n", reeb_time, reeb_time / total_time * 100);
	printf("rig: \t\t%.3f (%.1f%%)\n", rig_time, rig_time / total_time * 100);
	printf("retarget: \t%.3f (%.1f%%)\n", retarget_time, retarget_time / total_time * 100);
	printf("-----------\n");
	
	ED_undo_push(C, "Retarget Skeleton");

	// XXX	
//	allqueue(REDRAWVIEW3D, 0);
}

void BIF_retargetArc(bContext *C, ReebArc *earc, RigGraph *template_rigg)
{
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	bArmature *armedit = obedit->data;
	Object *ob;
	RigGraph *rigg;
	RigArc *iarc;
	bArmature *arm;
	char *side_string = scene->toolsettings->skgen_side_string;
	char *num_string = scene->toolsettings->skgen_num_string;
	int free_template = 0;
	
	if (template_rigg)
	{
		ob = template_rigg->ob; 	
		arm = ob->data;
	}
	else
	{
		free_template = 1;
		ob = obedit; 	
		arm = ob->data;
		template_rigg = armatureSelectedToGraph(C, ob, arm);
	}
	
	if (template_rigg->arcs.first == NULL)
	{
//		XXX
//		error("No Template and no deforming bones selected");
		return;
	}
	
	rigg = cloneRigGraph(template_rigg, armedit->edbo, obedit, side_string, num_string);
	
	iarc = rigg->arcs.first;
	
	iarc->link_mesh = earc;
	iarc->head->link_mesh = earc->head;
	iarc->tail->link_mesh = earc->tail;
	
	retargetArctoArc(C, rigg, iarc, iarc->head);
	
	finishRetarget(rigg);
	
	/* free template if it comes from the edit armature */
	if (free_template)
	{
		RIG_freeRigGraph((BGraph*)template_rigg);
	}
	RIG_freeRigGraph((BGraph*)rigg);
	
	ED_armature_validate_active(armedit);

//	XXX
//	allqueue(REDRAWVIEW3D, 0);
}

void BIF_adjustRetarget(bContext *C)
{
	if (GLOBAL_RIGG)
	{
		adjustGraphs(C, GLOBAL_RIGG);
	}
}
