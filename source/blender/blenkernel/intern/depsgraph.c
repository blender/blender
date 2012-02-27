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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/depsgraph.c
 *  \ingroup bke
 */

 
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_winstuff.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_lattice_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_movieclip_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "depsgraph_private.h"
 
/* Queue and stack operations for dag traversal 
 *
 * the queue store a list of freenodes to avoid successives alloc/dealloc
 */

DagNodeQueue * queue_create (int slots) 
{
	DagNodeQueue * queue;
	DagNodeQueueElem * elem;
	int i;
	
	queue = MEM_mallocN(sizeof(DagNodeQueue),"DAG queue");
	queue->freenodes = MEM_mallocN(sizeof(DagNodeQueue),"DAG queue");
	queue->count = 0;
	queue->maxlevel = 0;
	queue->first = queue->last = NULL;
	elem = MEM_mallocN(sizeof(DagNodeQueueElem),"DAG queue elem3");
	elem->node = NULL;
	elem->next = NULL;
	queue->freenodes->first = queue->freenodes->last = elem;
	
	for (i = 1; i <slots;i++) {
		elem = MEM_mallocN(sizeof(DagNodeQueueElem),"DAG queue elem4");
		elem->node = NULL;
		elem->next = NULL;
		queue->freenodes->last->next = elem;
		queue->freenodes->last = elem;
	}
	queue->freenodes->count = slots;
	return queue;
}

void queue_raz(DagNodeQueue *queue)
{
	DagNodeQueueElem * elem;
	
	elem = queue->first;
	if (queue->freenodes->last)
		queue->freenodes->last->next = elem;
	else
		queue->freenodes->first = queue->freenodes->last = elem;
	
	elem->node = NULL;
	queue->freenodes->count++;
	while (elem->next) {
		elem = elem->next;
		elem->node = NULL;
		queue->freenodes->count++;
	}
	queue->freenodes->last = elem;
	queue->count = 0;
}

void queue_delete(DagNodeQueue *queue)
{
	DagNodeQueueElem * elem;
	DagNodeQueueElem * temp;
	
	elem = queue->first;
	while (elem) {
		temp = elem;
		elem = elem->next;
		MEM_freeN(temp);
	}
	
	elem = queue->freenodes->first;
	while (elem) {
		temp = elem;
		elem = elem->next;
		MEM_freeN(temp);
	}
	
	MEM_freeN(queue->freenodes);			
	MEM_freeN(queue);			
}

/* insert in queue, remove in front */
void push_queue(DagNodeQueue *queue, DagNode *node)
{
	DagNodeQueueElem * elem;
	int i;

	if (node == NULL) {
		fprintf(stderr,"pushing null node \n");
		return;
	}
	/*fprintf(stderr,"BFS push : %s %d\n",((ID *) node->ob)->name, queue->count);*/

	elem = queue->freenodes->first;
	if (elem != NULL) {
		queue->freenodes->first = elem->next;
		if ( queue->freenodes->last == elem) {
			queue->freenodes->last = NULL;
			queue->freenodes->first = NULL;
		}
		queue->freenodes->count--;
	} else { /* alllocating more */		
		elem = MEM_mallocN(sizeof(DagNodeQueueElem),"DAG queue elem1");
		elem->node = NULL;
		elem->next = NULL;
		queue->freenodes->first = queue->freenodes->last = elem;

		for (i = 1; i <DAGQUEUEALLOC;i++) {
			elem = MEM_mallocN(sizeof(DagNodeQueueElem),"DAG queue elem2");
			elem->node = NULL;
			elem->next = NULL;
			queue->freenodes->last->next = elem;
			queue->freenodes->last = elem;
		}
		queue->freenodes->count = DAGQUEUEALLOC;
			
		elem = queue->freenodes->first;	
		queue->freenodes->first = elem->next;	
	}
	elem->next = NULL;
	elem->node = node;
	if (queue->last != NULL)
		queue->last->next = elem;
	queue->last = elem;
	if (queue->first == NULL) {
		queue->first = elem;
	}
	queue->count++;
}


/* insert in front, remove in front */
void push_stack(DagNodeQueue *queue, DagNode *node)
{
	DagNodeQueueElem * elem;
	int i;

	elem = queue->freenodes->first;	
	if (elem != NULL) {
		queue->freenodes->first = elem->next;
		if ( queue->freenodes->last == elem) {
			queue->freenodes->last = NULL;
			queue->freenodes->first = NULL;
		}
		queue->freenodes->count--;
	} else { /* alllocating more */
		elem = MEM_mallocN(sizeof(DagNodeQueueElem),"DAG queue elem1");
		elem->node = NULL;
		elem->next = NULL;
		queue->freenodes->first = queue->freenodes->last = elem;

		for (i = 1; i <DAGQUEUEALLOC;i++) {
			elem = MEM_mallocN(sizeof(DagNodeQueueElem),"DAG queue elem2");
			elem->node = NULL;
			elem->next = NULL;
			queue->freenodes->last->next = elem;
			queue->freenodes->last = elem;
		}
		queue->freenodes->count = DAGQUEUEALLOC;
			
		elem = queue->freenodes->first;	
		queue->freenodes->first = elem->next;	
	}
	elem->next = queue->first;
	elem->node = node;
	queue->first = elem;
	if (queue->last == NULL)
		queue->last = elem;
	queue->count++;
}


DagNode * pop_queue(DagNodeQueue *queue)
{
	DagNodeQueueElem * elem;
	DagNode *node;

	elem = queue->first;
	if (elem) {
		queue->first = elem->next;
		if (queue->last == elem) {
			queue->last=NULL;
			queue->first=NULL;
		}
		queue->count--;
		if (queue->freenodes->last)
			queue->freenodes->last->next=elem;
		queue->freenodes->last=elem;
		if (queue->freenodes->first == NULL)
			queue->freenodes->first=elem;
		node = elem->node;
		elem->node = NULL;
		elem->next = NULL;
		queue->freenodes->count++;
		return node;
	} else {
		fprintf(stderr,"return null \n");
		return NULL;
	}
}

void	*pop_ob_queue(struct DagNodeQueue *queue)
{
	return(pop_queue(queue)->ob);
}

DagNode * get_top_node_queue(DagNodeQueue *queue) 
{
	return queue->first->node;
}

int		queue_count(struct DagNodeQueue *queue)
{
	return queue->count;
}


DagForest *dag_init(void)
{
	DagForest *forest;
	/* use callocN to init all zero */
	forest = MEM_callocN(sizeof(DagForest),"DAG root");
	return forest;
}

/* isdata = object data... */
// XXX this needs to be extended to be more flexible (so that not only objects are evaluated via depsgraph)...
static void dag_add_driver_relation(AnimData *adt, DagForest *dag, DagNode *node, int isdata)
{
	FCurve *fcu;
	DagNode *node1;
	
	for (fcu= adt->drivers.first; fcu; fcu= fcu->next) {
		ChannelDriver *driver= fcu->driver;
		DriverVar *dvar;
		int isdata_fcu = isdata || (fcu->rna_path && strstr(fcu->rna_path, "modifiers["));
		
		/* loop over variables to get the target relationships */
		for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
			/* only used targets */
			DRIVER_TARGETS_USED_LOOPER(dvar) 
			{
				if (dtar->id) {
					// FIXME: other data types need to be added here so that they can work!
					if (GS(dtar->id->name)==ID_OB) {
						Object *ob= (Object *)dtar->id;
						
						/* normal channel-drives-channel */
						node1 = dag_get_node(dag, dtar->id);
						
						/* check if bone... */
						if ((ob->type==OB_ARMATURE) && 
							( ((dtar->rna_path) && strstr(dtar->rna_path, "pose.bones[")) || 
							  ((dtar->flag & DTAR_FLAG_STRUCT_REF) && (dtar->pchan_name[0])) )) 
						{
							dag_add_relation(dag, node1, node, isdata_fcu?DAG_RL_DATA_DATA:DAG_RL_DATA_OB, "Driver");
						}
						/* check if ob data */
						else if (dtar->rna_path && strstr(dtar->rna_path, "data."))
							dag_add_relation(dag, node1, node, isdata_fcu?DAG_RL_DATA_DATA:DAG_RL_DATA_OB, "Driver");
						/* normal */
						else
							dag_add_relation(dag, node1, node, isdata_fcu?DAG_RL_OB_DATA:DAG_RL_OB_OB, "Driver");
					}
				}
			}
			DRIVER_TARGETS_LOOPER_END
		}
	}
}

static void dag_add_collision_field_relation(DagForest *dag, Scene *scene, Object *ob, DagNode *node)
{
	Base *base;
	DagNode *node2;

	// would be nice to have a list of colliders here
	// so for now walk all objects in scene check 'same layer rule'
	for(base = scene->base.first; base; base= base->next) {
		if((base->lay & ob->lay) && base->object->pd) {
			Object *ob1= base->object;
			if((ob1->pd->deflect || ob1->pd->forcefield) && (ob1 != ob)) {
				node2 = dag_get_node(dag, ob1);					
				dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Field Collision");
			}
		}
	}
}

static void build_dag_object(DagForest *dag, DagNode *scenenode, Scene *scene, Object *ob, int mask)
{
	bConstraint *con;
	DagNode * node;
	DagNode * node2;
	DagNode * node3;
	Key *key;
	ParticleSystem *psys;
	int addtoroot= 1;
	
	node = dag_get_node(dag, ob);
	
	if ((ob->data) && (mask&DAG_RL_DATA)) {
		node2 = dag_get_node(dag,ob->data);
		dag_add_relation(dag,node,node2,DAG_RL_DATA, "Object-Data Relation");
		node2->first_ancestor = ob;
		node2->ancestor_count += 1;
	}

	/* also build a custom data mask for dependencies that need certain layers */
	node->customdata_mask= 0;
	
	if (ob->type == OB_ARMATURE) {
		if (ob->pose) {
			bPoseChannel *pchan;
			bConstraint *con;
			
			for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next) {
				for (con = pchan->constraints.first; con; con=con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							if (ct->tar && ct->tar != ob) {
								// fprintf(stderr,"armature %s target :%s \n", ob->id.name, target->id.name);
								node3 = dag_get_node(dag, ct->tar);
								
								if (ct->subtarget[0]) {
									dag_add_relation(dag,node3,node, DAG_RL_OB_DATA|DAG_RL_DATA_DATA, cti->name);
									if(ct->tar->type == OB_MESH)
										node3->customdata_mask |= CD_MASK_MDEFORMVERT;
								}
								else if(ELEM3(con->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO, CONSTRAINT_TYPE_SPLINEIK)) 	
									dag_add_relation(dag,node3,node, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, cti->name);
								else
									dag_add_relation(dag,node3,node, DAG_RL_OB_DATA, cti->name);
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 1);
					}
					
				}
			}
		}
	}
	
	/* driver dependencies, nla modifiers */
#if 0 // XXX old animation system
	if(ob->nlastrips.first) {
		bActionStrip *strip;
		bActionChannel *chan;
		for(strip= ob->nlastrips.first; strip; strip= strip->next) {
			if(strip->modifiers.first) {
				bActionModifier *amod;
				for(amod= strip->modifiers.first; amod; amod= amod->next) {
					if(amod->ob) {
						node2 = dag_get_node(dag, amod->ob);
						dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "NLA Strip Modifier");
					}
				}
			}
		}
	}
#endif // XXX old animation system
	if (ob->adt)
		dag_add_driver_relation(ob->adt, dag, node, (ob->type == OB_ARMATURE)); // XXX isdata arg here doesn't give an accurate picture of situation
		
	key= ob_get_key(ob);
	if (key && key->adt)
		dag_add_driver_relation(key->adt, dag, node, 1);

	if (ob->modifiers.first) {
		ModifierData *md;
		
		for(md=ob->modifiers.first; md; md=md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);
			
			if (mti->updateDepgraph) mti->updateDepgraph(md, dag, scene, ob, node);
		}
	}
	if (ob->parent) {
		node2 = dag_get_node(dag,ob->parent);
		
		switch(ob->partype) {
			case PARSKEL:
				dag_add_relation(dag,node2,node,DAG_RL_DATA_DATA|DAG_RL_OB_OB, "Parent");
				break;
			case PARVERT1: case PARVERT3:
				dag_add_relation(dag,node2,node,DAG_RL_DATA_OB|DAG_RL_OB_OB, "Vertex Parent");
				node2->customdata_mask |= CD_MASK_ORIGINDEX;
				break;
			case PARBONE:
				dag_add_relation(dag,node2,node,DAG_RL_DATA_OB|DAG_RL_OB_OB, "Bone Parent");
				break;
			default:
				if(ob->parent->type==OB_LATTICE) 
					dag_add_relation(dag,node2,node,DAG_RL_DATA_DATA|DAG_RL_OB_OB, "Lattice Parent");
				else if(ob->parent->type==OB_CURVE) {
					Curve *cu= ob->parent->data;
					if(cu->flag & CU_PATH) 
						dag_add_relation(dag,node2,node,DAG_RL_DATA_OB|DAG_RL_OB_OB, "Curve Parent");
					else
						dag_add_relation(dag,node2,node,DAG_RL_OB_OB, "Curve Parent");
				}
				else
					dag_add_relation(dag,node2,node,DAG_RL_OB_OB, "Parent");
		}
		/* exception case: parent is duplivert */
		if(ob->type==OB_MBALL && (ob->parent->transflag & OB_DUPLIVERTS)) {
			dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA|DAG_RL_OB_OB, "Duplivert");
		}
		
		addtoroot = 0;
	}
	if (ob->proxy) {
		node2 = dag_get_node(dag, ob->proxy);
		dag_add_relation(dag, node, node2, DAG_RL_DATA_DATA|DAG_RL_OB_OB, "Proxy");
		/* inverted relation, so addtoroot shouldn't be set to zero */
	}
	
	if (ob->transflag & OB_DUPLI) {
		if((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
			GroupObject *go;
			for(go= ob->dup_group->gobject.first; go; go= go->next) {
				if(go->ob) {
					node2 = dag_get_node(dag, go->ob);
					/* node2 changes node1, this keeps animations updated in groups?? not logical? */
					dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Dupligroup");
				}
			}
		}
	}

	/* softbody collision  */
	if ((ob->type==OB_MESH) || (ob->type==OB_CURVE) || (ob->type==OB_LATTICE)) {
		if(modifiers_isSoftbodyEnabled(ob) || modifiers_isClothEnabled(ob) || ob->particlesystem.first)
			dag_add_collision_field_relation(dag, scene, ob, node); /* TODO: use effectorweight->group */
	}
	
	/* object data drivers */
	if (ob->data) {
		AnimData *adt= BKE_animdata_from_id((ID *)ob->data);
		if (adt)
			dag_add_driver_relation(adt, dag, node, 1);
	}
	
	/* object type/data relationships */
	switch (ob->type) {
		case OB_CAMERA:
		{
			Camera *cam = (Camera *)ob->data;
			
			if (cam->dof_ob) {
				node2 = dag_get_node(dag, cam->dof_ob);
				dag_add_relation(dag,node2,node,DAG_RL_OB_OB, "Camera DoF");
			}
		}
			break;
		case OB_MBALL: 
		{
			Object *mom= find_basis_mball(scene, ob);
			
			if(mom!=ob) {
				node2 = dag_get_node(dag, mom);
				dag_add_relation(dag,node,node2,DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Metaball");  // mom depends on children!
			}
		}
			break;
		case OB_CURVE:
		case OB_FONT:
		{
			Curve *cu= ob->data;
			
			if(cu->bevobj) {
				node2 = dag_get_node(dag, cu->bevobj);
				dag_add_relation(dag,node2,node,DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Curve Bevel");
			}
			if(cu->taperobj) {
				node2 = dag_get_node(dag, cu->taperobj);
				dag_add_relation(dag,node2,node,DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Curve Taper");
			}
			if(ob->type == OB_FONT) {
				if(cu->textoncurve) {
					node2 = dag_get_node(dag, cu->textoncurve);
					dag_add_relation(dag,node2,node,DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Texture On Curve");
				}
			}
		}
			break;
	}
	
	/* particles */
	psys= ob->particlesystem.first;
	if(psys) {
		GroupObject *go;

		for(; psys; psys=psys->next) {
			BoidRule *rule = NULL;
			BoidState *state = NULL;
			ParticleSettings *part= psys->part;
			ListBase *effectors = NULL;
			EffectorCache *eff;

			dag_add_relation(dag, node, node, DAG_RL_OB_DATA, "Particle-Object Relation");

			if(!psys_check_enabled(ob, psys))
				continue;

			if(ELEM(part->phystype,PART_PHYS_KEYED,PART_PHYS_BOIDS)) {
				ParticleTarget *pt = psys->targets.first;

				for(; pt; pt=pt->next) {
					if(pt->ob && BLI_findlink(&pt->ob->particlesystem, pt->psys-1)) {
						node2 = dag_get_node(dag, pt->ob);
						dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Particle Targets");
					}
			   }
			}

			if(part->ren_as == PART_DRAW_OB && part->dup_ob) {
				node2 = dag_get_node(dag, part->dup_ob);
				/* note that this relation actually runs in the wrong direction, the problem
				   is that dupli system all have this (due to parenting), and the render
				   engine instancing assumes particular ordering of objects in list */
				dag_add_relation(dag, node, node2, DAG_RL_OB_OB, "Particle Object Visualisation");
				if(part->dup_ob->type == OB_MBALL)
					dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA, "Particle Object Visualisation");
			}

			if(part->ren_as == PART_DRAW_GR && part->dup_group) {
				for(go=part->dup_group->gobject.first; go; go=go->next) {
					node2 = dag_get_node(dag, go->ob);
					dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Particle Group Visualisation");
				}
			}

			effectors = pdInitEffectors(scene, ob, psys, part->effector_weights);

			if(effectors) for(eff = effectors->first; eff; eff=eff->next) {
				if(eff->psys) {
					node2 = dag_get_node(dag, eff->ob);
					dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Particle Field");
				}
			}

			pdEndEffectors(&effectors);

			if(part->boids) {
				for(state = part->boids->states.first; state; state=state->next) {
					for(rule = state->rules.first; rule; rule=rule->next) {
						Object *ruleob = NULL;
						if(rule->type==eBoidRuleType_Avoid)
							ruleob = ((BoidRuleGoalAvoid*)rule)->ob;
						else if(rule->type==eBoidRuleType_FollowLeader)
							ruleob = ((BoidRuleFollowLeader*)rule)->ob;

						if(ruleob) {
							node2 = dag_get_node(dag, ruleob);
							dag_add_relation(dag, node2, node, DAG_RL_OB_DATA, "Boid Rule");
						}
					}
				}
			}
		}
	}
	
	/* object constraints */
	for (con = ob->constraints.first; con; con=con->next) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
		ListBase targets = {NULL, NULL};
		bConstraintTarget *ct;
		
		if(!cti)
			continue;

		/* special case for camera tracking -- it doesn't use targets to define relations */
		if(ELEM3(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER, CONSTRAINT_TYPE_OBJECTSOLVER)) {
			int depends_on_camera= 0;

			if(cti->type==CONSTRAINT_TYPE_FOLLOWTRACK) {
				bFollowTrackConstraint *data= (bFollowTrackConstraint *)con->data;

				if((data->clip || data->flag&FOLLOWTRACK_ACTIVECLIP) && data->track[0])
					depends_on_camera= 1;

				if(data->depth_ob) {
					node2 = dag_get_node(dag, data->depth_ob);
					dag_add_relation(dag, node2, node, DAG_RL_DATA_OB|DAG_RL_OB_OB, cti->name);
				}
			}
			else if(cti->type==CONSTRAINT_TYPE_OBJECTSOLVER)
				depends_on_camera= 1;

			if(depends_on_camera && scene->camera) {
				node2 = dag_get_node(dag, scene->camera);
				dag_add_relation(dag, node2, node, DAG_RL_DATA_OB|DAG_RL_OB_OB, cti->name);
			}

			dag_add_relation(dag,scenenode,node,DAG_RL_SCENE, "Scene Relation");
			addtoroot = 0;
		}
		else if (cti->get_constraint_targets) {
			cti->get_constraint_targets(con, &targets);
			
			for (ct= targets.first; ct; ct= ct->next) {
				Object *obt;
				
				if (ct->tar)
					obt= ct->tar;
				else
					continue;
				
				node2 = dag_get_node(dag, obt);
				if (ELEM(con->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO))
					dag_add_relation(dag, node2, node, DAG_RL_DATA_OB|DAG_RL_OB_OB, cti->name);
				else {
					if (ELEM3(obt->type, OB_ARMATURE, OB_MESH, OB_LATTICE) && (ct->subtarget[0])) {
						dag_add_relation(dag, node2, node, DAG_RL_DATA_OB|DAG_RL_OB_OB, cti->name);
						if (obt->type == OB_MESH)
							node2->customdata_mask |= CD_MASK_MDEFORMVERT;
					}
					else
						dag_add_relation(dag, node2, node, DAG_RL_OB_OB, cti->name);
				}
				addtoroot = 0;
			}
			
			if (cti->flush_constraint_targets)
				cti->flush_constraint_targets(con, &targets, 1);
		}
	}

	if (addtoroot == 1 )
		dag_add_relation(dag,scenenode,node,DAG_RL_SCENE, "Scene Relation");
}

struct DagForest *build_dag(Main *bmain, Scene *sce, short mask) 
{
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	DagNode *node;
	DagNode *scenenode;
	DagForest *dag;
	DagAdjList *itA;

	dag = sce->theDag;
	sce->dagisvalid=1;
	if ( dag)
		free_forest( dag ); 
	else {
		dag = dag_init();
		sce->theDag = dag;
	}
	
	/* add base node for scene. scene is always the first node in DAG */
	scenenode = dag_add_node(dag, sce);	
	
	/* add current scene objects */
	for(base = sce->base.first; base; base= base->next) {
		ob= base->object;
		
		build_dag_object(dag, scenenode, sce, ob, mask);
		if(ob->proxy)
			build_dag_object(dag, scenenode, sce, ob->proxy, mask);
		
		/* handled in next loop */
		if(ob->dup_group) 
			ob->dup_group->id.flag |= LIB_DOIT;
	}
	
	/* add groups used in current scene objects */
	for(group= bmain->group.first; group; group= group->id.next) {
		if(group->id.flag & LIB_DOIT) {
			for(go= group->gobject.first; go; go= go->next) {
				build_dag_object(dag, scenenode, sce, go->ob, mask);
			}
			group->id.flag &= ~LIB_DOIT;
		}
	}
	
	/* Now all relations were built, but we need to solve 1 exceptional case;
	   When objects have multiple "parents" (for example parent + constraint working on same object)
	   the relation type has to be synced. One of the parents can change, and should give same event to child */
	
	/* nodes were callocced, so we can use node->color for temporal storage */
	for(node = sce->theDag->DagNode.first; node; node= node->next) {
		if(node->type==ID_OB) {
			for(itA = node->child; itA; itA= itA->next) {
				if(itA->node->type==ID_OB) {
					itA->node->color |= itA->type;
				}
			}

			/* also flush custom data mask */
			((Object*)node->ob)->customdata_mask= node->customdata_mask;
		}
	}
	/* now set relations equal, so that when only one parent changes, the correct recalcs are found */
	for(node = sce->theDag->DagNode.first; node; node= node->next) {
		if(node->type==ID_OB) {
			for(itA = node->child; itA; itA= itA->next) {
				if(itA->node->type==ID_OB) {
					itA->type |= itA->node->color;
				}
			}
		}
	}
	
	// cycle detection and solving
	// solve_cycles(dag);	
	
	return dag;
}


void free_forest(DagForest *Dag) 
{  /* remove all nodes and deps */
	DagNode *tempN;
	DagAdjList *tempA;	
	DagAdjList *itA;
	DagNode *itN = Dag->DagNode.first;
	
	while (itN) {
		itA = itN->child;	
		while (itA) {
			tempA = itA;
			itA = itA->next;
			MEM_freeN(tempA);			
		}
		
		itA = itN->parent;	
		while (itA) {
			tempA = itA;
			itA = itA->next;
			MEM_freeN(tempA);			
		}
		
		tempN = itN;
		itN = itN->next;
		MEM_freeN(tempN);
	}

	BLI_ghash_free(Dag->nodeHash, NULL, NULL);
	Dag->nodeHash= NULL;
	Dag->DagNode.first = NULL;
	Dag->DagNode.last = NULL;
	Dag->numNodes = 0;

}

DagNode * dag_find_node (DagForest *forest,void * fob)
{
	if(forest->nodeHash)
		return BLI_ghash_lookup(forest->nodeHash, fob);

	return NULL;
}

static int ugly_hack_sorry= 1;	// prevent type check
static int dag_print_dependencies= 0; // debugging

/* no checking of existence, use dag_find_node first or dag_get_node */
DagNode * dag_add_node (DagForest *forest, void * fob)
{
	DagNode *node;
		
	node = MEM_callocN(sizeof(DagNode),"DAG node");
	if (node) {
		node->ob = fob;
		node->color = DAG_WHITE;

		if(ugly_hack_sorry) node->type = GS(((ID *) fob)->name);	// sorry, done for pose sorting
		if (forest->numNodes) {
			((DagNode *) forest->DagNode.last)->next = node;
			forest->DagNode.last = node;
			forest->numNodes++;
		} else {
			forest->DagNode.last = node;
			forest->DagNode.first = node;
			forest->numNodes = 1;
		}

		if(!forest->nodeHash)
			forest->nodeHash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "dag_add_node gh");
		BLI_ghash_insert(forest->nodeHash, fob, node);
	}

	return node;
}

DagNode * dag_get_node (DagForest *forest,void * fob)
{
	DagNode *node;
	
	node = dag_find_node (forest, fob);	
	if (!node) 
		node = dag_add_node(forest, fob);
	return node;
}



DagNode * dag_get_sub_node (DagForest *forest,void * fob)
{
	DagNode *node;
	DagAdjList *mainchild, *prev=NULL;
	
	mainchild = ((DagNode *) forest->DagNode.first)->child;
	/* remove from first node (scene) adj list if present */
	while (mainchild) {
		if (mainchild->node == fob) {
			if (prev) {
				prev->next = mainchild->next;
				MEM_freeN(mainchild);
				break;
			} else {
				((DagNode *) forest->DagNode.first)->child = mainchild->next;
				MEM_freeN(mainchild);
				break;
			}
		}
		prev = mainchild;
		mainchild = mainchild->next;
	}
	node = dag_find_node (forest, fob);	
	if (!node) 
		node = dag_add_node(forest, fob);
	return node;
}

static void dag_add_parent_relation(DagForest *UNUSED(forest), DagNode *fob1, DagNode *fob2, short rel, const char *name) 
{
	DagAdjList *itA = fob2->parent;
	
	while (itA) { /* search if relation exist already */
		if (itA->node == fob1) {
			itA->type |= rel;
			itA->count += 1;
			return;
		}
		itA = itA->next;
	}
	/* create new relation and insert at head. MALLOC alert! */
	itA = MEM_mallocN(sizeof(DagAdjList),"DAG adj list");
	itA->node = fob1;
	itA->type = rel;
	itA->count = 1;
	itA->next = fob2->parent;
	itA->name = name;
	fob2->parent = itA;
}

void dag_add_relation(DagForest *forest, DagNode *fob1, DagNode *fob2, short rel, const char *name) 
{
	DagAdjList *itA = fob1->child;
	
	/* parent relation is for cycle checking */
	dag_add_parent_relation(forest, fob1, fob2, rel, name);

	while (itA) { /* search if relation exist already */
		if (itA->node == fob2) {
			itA->type |= rel;
			itA->count += 1;
			return;
		}
		itA = itA->next;
	}
	/* create new relation and insert at head. MALLOC alert! */
	itA = MEM_mallocN(sizeof(DagAdjList),"DAG adj list");
	itA->node = fob2;
	itA->type = rel;
	itA->count = 1;
	itA->next = fob1->child;
	itA->name = name;
	fob1->child = itA;
}

static const char *dag_node_name(DagNode *node)
{
	if(node->ob == NULL)
		return "null";
	else if(ugly_hack_sorry)
		return ((ID*)(node->ob))->name+2;
	else
		return ((bPoseChannel*)(node->ob))->name;
}

static void dag_node_print_dependencies(DagNode *node)
{
	DagAdjList *itA;

	printf("%s depends on:\n", dag_node_name(node));

	for(itA= node->parent; itA; itA= itA->next)
		printf("  %s through %s\n", dag_node_name(itA->node), itA->name);
	printf("\n");
}

static int dag_node_print_dependency_recurs(DagNode *node, DagNode *endnode)
{
	DagAdjList *itA;

	if(node->color == DAG_BLACK)
		return 0;

	node->color= DAG_BLACK;

	if(node == endnode)
		return 1;

	for(itA= node->parent; itA; itA= itA->next) {
		if(dag_node_print_dependency_recurs(itA->node, endnode)) {
			printf("  %s depends on %s through %s.\n", dag_node_name(node), dag_node_name(itA->node), itA->name);
			return 1;
		}
	}

	return 0;
}

static void dag_node_print_dependency_cycle(DagForest *dag, DagNode *startnode, DagNode *endnode, const char *name)
{
	DagNode *node;

	for(node = dag->DagNode.first; node; node= node->next)
		node->color= DAG_WHITE;

	printf("  %s depends on %s through %s.\n", dag_node_name(endnode), dag_node_name(startnode), name);
	dag_node_print_dependency_recurs(startnode, endnode);
	printf("\n");
}

static int dag_node_recurs_level(DagNode *node, int level)
{
	DagAdjList *itA;
	int newlevel;

	node->color= DAG_BLACK;	/* done */
	newlevel= ++level;
	
	for(itA= node->parent; itA; itA= itA->next) {
		if(itA->node->color==DAG_WHITE) {
			itA->node->ancestor_count= dag_node_recurs_level(itA->node, level);
			newlevel= MAX2(newlevel, level+itA->node->ancestor_count);
		}
		else
			newlevel= MAX2(newlevel, level+itA->node->ancestor_count);
	}
	
	return newlevel;
}

static void dag_check_cycle(DagForest *dag)
{
	DagNode *node;
	DagAdjList *itA;

	/* debugging print */
	if(dag_print_dependencies)
		for(node = dag->DagNode.first; node; node= node->next)
			dag_node_print_dependencies(node);

	/* tag nodes unchecked */
	for(node = dag->DagNode.first; node; node= node->next)
		node->color= DAG_WHITE;
	
	for(node = dag->DagNode.first; node; node= node->next) {
		if(node->color==DAG_WHITE) {
			node->ancestor_count= dag_node_recurs_level(node, 0);
		}
	}
	
	/* check relations, and print errors */
	for(node = dag->DagNode.first; node; node= node->next) {
		for(itA= node->parent; itA; itA= itA->next) {
			if(itA->node->ancestor_count > node->ancestor_count) {
				if(node->ob && itA->node->ob) {
					printf("Dependency cycle detected:\n");
					dag_node_print_dependency_cycle(dag, itA->node, node, itA->name);
				}
			}
		}
	}

	/* parent relations are only needed for cycle checking, so free now */
	for(node = dag->DagNode.first; node; node= node->next) {
		while (node->parent) {
			itA = node->parent->next;
			MEM_freeN(node->parent);			
			node->parent = itA;
		}
	}
}

/*
 * MainDAG is the DAG of all objects in current scene
 * used only for drawing there is one also in each scene
 */
static DagForest * MainDag = NULL;

DagForest *getMainDag(void)
{
	return MainDag;
}


void setMainDag(DagForest *dag)
{
	MainDag = dag;
}


/*
 * note for BFS/DFS
 * in theory we should sweep the whole array
 * but in our case the first node is the scene
 * and is linked to every other object
 *
 * for general case we will need to add outer loop
 */

/*
 * ToDo : change pos kludge
 */

/* adjust levels for drawing in oops space */
void graph_bfs(void)
{
	DagNode *node;
	DagNodeQueue *nqueue;
	int pos[50];
	int i;
	DagAdjList *itA;
	int minheight;
	
	/* fprintf(stderr,"starting BFS \n ------------\n"); */	
	nqueue = queue_create(DAGQUEUEALLOC);
	for ( i=0; i<50; i++)
		pos[i] = 0;
	
	/* Init
	 * dagnode.first is alway the root (scene) 
	 */
	node = MainDag->DagNode.first;
	while(node) {
		node->color = DAG_WHITE;
		node->BFS_dist = 9999;
		node->k = 0;
		node = node->next;
	}
	
	node = MainDag->DagNode.first;
	if (node->color == DAG_WHITE) {
		node->color = DAG_GRAY;
		node->BFS_dist = 1;
		push_queue(nqueue,node);  
		while(nqueue->count) {
			node = pop_queue(nqueue);
			
			minheight = pos[node->BFS_dist];
			itA = node->child;
			while(itA != NULL) {
				if(itA->node->color == DAG_WHITE) {
					itA->node->color = DAG_GRAY;
					itA->node->BFS_dist = node->BFS_dist + 1;
					itA->node->k = (float) minheight;
					push_queue(nqueue,itA->node);
				}
				
				else {
					fprintf(stderr,"bfs not dag tree edge color :%i \n",itA->node->color);
				}

				
				itA = itA->next;
			}
			if (pos[node->BFS_dist] > node->k ) {
				pos[node->BFS_dist] += 1;				
				node->k = (float) pos[node->BFS_dist];
			} else {
				pos[node->BFS_dist] = (int) node->k +1;
			}
			set_node_xy(node, node->BFS_dist*DEPSX*2, pos[node->BFS_dist]*DEPSY*2);
			node->color = DAG_BLACK;
			/*
			fprintf(stderr,"BFS node : %20s %i %5.0f %5.0f\n",((ID *) node->ob)->name,node->BFS_dist, node->x, node->y);
			*/
		}
	}
	queue_delete(nqueue);
}

int pre_and_post_BFS(DagForest *dag, short mask, graph_action_func pre_func, graph_action_func post_func, void **data)
{
	DagNode *node;
	
	node = dag->DagNode.first;
	return pre_and_post_source_BFS(dag, mask,  node,  pre_func,  post_func, data);
}


int pre_and_post_source_BFS(DagForest *dag, short mask, DagNode *source, graph_action_func pre_func, graph_action_func post_func, void **data)
{
	DagNode *node;
	DagNodeQueue *nqueue;
	DagAdjList *itA;
	int	retval = 0;
	/* fprintf(stderr,"starting BFS \n ------------\n"); */	
	
	/* Init
		* dagnode.first is alway the root (scene) 
		*/
	node = dag->DagNode.first;
	nqueue = queue_create(DAGQUEUEALLOC);
	while(node) {
		node->color = DAG_WHITE;
		node->BFS_dist = 9999;
		node = node->next;
	}
	
	node = source;
	if (node->color == DAG_WHITE) {
		node->color = DAG_GRAY;
		node->BFS_dist = 1;
		pre_func(node->ob,data);
		
		while(nqueue->count) {
			node = pop_queue(nqueue);
			
			itA = node->child;
			while(itA != NULL) {
				if((itA->node->color == DAG_WHITE) && (itA->type & mask)) {
					itA->node->color = DAG_GRAY;
					itA->node->BFS_dist = node->BFS_dist + 1;
					push_queue(nqueue,itA->node);
					pre_func(node->ob,data);
				}
				
				else { // back or cross edge
					retval = 1;
				}
				itA = itA->next;
			}
			post_func(node->ob,data);
			node->color = DAG_BLACK;
			/*
			fprintf(stderr,"BFS node : %20s %i %5.0f %5.0f\n",((ID *) node->ob)->name,node->BFS_dist, node->x, node->y);
			*/
		}
	}
	queue_delete(nqueue);
	return retval;
}

/* non recursive version of DFS, return queue -- outer loop present to catch odd cases (first level cycles)*/
DagNodeQueue * graph_dfs(void)
{
	DagNode *node;
	DagNodeQueue *nqueue;
	DagNodeQueue *retqueue;
	int pos[50];
	int i;
	DagAdjList *itA;
	int time;
	int skip = 0;
	int minheight;
	int maxpos=0;
	/* int	is_cycle = 0; */ /* UNUSED */
	/*
	 *fprintf(stderr,"starting DFS \n ------------\n");
	 */	
	nqueue = queue_create(DAGQUEUEALLOC);
	retqueue = queue_create(MainDag->numNodes);
	for ( i=0; i<50; i++)
		pos[i] = 0;
	
	/* Init
	 * dagnode.first is alway the root (scene) 
	 */
	node = MainDag->DagNode.first;
	while(node) {
		node->color = DAG_WHITE;
		node->DFS_dist = 9999;
		node->DFS_dvtm = node->DFS_fntm = 9999;
		node->k = 0;
		node =  node->next;
	}
	
	time = 1;
	
	node = MainDag->DagNode.first;

	do {
	if (node->color == DAG_WHITE) {
		node->color = DAG_GRAY;
		node->DFS_dist = 1;
		node->DFS_dvtm = time;
		time++;
		push_stack(nqueue,node);  
			
		while(nqueue->count) {
			//graph_print_queue(nqueue);

			skip = 0;
			node = get_top_node_queue(nqueue);
			
			minheight = pos[node->DFS_dist];

			itA = node->child;
			while(itA != NULL) {
				if(itA->node->color == DAG_WHITE) {
					itA->node->DFS_dvtm = time;
					itA->node->color = DAG_GRAY;

					time++;
					itA->node->DFS_dist = node->DFS_dist + 1;
					itA->node->k = (float) minheight;
					push_stack(nqueue,itA->node);
					skip = 1;
					break;
				} else { 
					if (itA->node->color == DAG_GRAY) { // back edge
						fprintf(stderr,"dfs back edge :%15s %15s \n",((ID *) node->ob)->name, ((ID *) itA->node->ob)->name);
						/* is_cycle = 1; */ /* UNUSED */
					} else if (itA->node->color == DAG_BLACK) {
						;
						/* already processed node but we may want later to change distance either to shorter to longer.
						 * DFS_dist is the first encounter  
						*/
						/*if (node->DFS_dist >= itA->node->DFS_dist)
							itA->node->DFS_dist = node->DFS_dist + 1;

							fprintf(stderr,"dfs forward or cross edge :%15s %i-%i %15s %i-%i \n",
								((ID *) node->ob)->name,
								node->DFS_dvtm, 
								node->DFS_fntm, 
								((ID *) itA->node->ob)->name, 
								itA->node->DFS_dvtm,
								itA->node->DFS_fntm);
					*/
					} else 
						fprintf(stderr,"dfs unknown edge \n");
				}
				itA = itA->next;
			}			

			if (!skip) {
				node = pop_queue(nqueue);
				node->color = DAG_BLACK;

				node->DFS_fntm = time;
				time++;
				if (node->DFS_dist > maxpos)
					maxpos = node->DFS_dist;
				if (pos[node->DFS_dist] > node->k ) {
					pos[node->DFS_dist] += 1;				
					node->k = (float) pos[node->DFS_dist];
				} else {
					pos[node->DFS_dist] = (int) node->k +1;
				}
				set_node_xy(node, node->DFS_dist*DEPSX*2, pos[node->DFS_dist]*DEPSY*2);
				
				/*
				fprintf(stderr,"DFS node : %20s %i %i %i %i\n",((ID *) node->ob)->name,node->BFS_dist, node->DFS_dist, node->DFS_dvtm, node->DFS_fntm );
				*/
				push_stack(retqueue,node);
				
			}
		}
	}
		node = node->next;
	} while (node);
//	fprintf(stderr,"i size : %i \n", maxpos);

	queue_delete(nqueue);
	return(retqueue);
}

/* unused */
int pre_and_post_DFS(DagForest *dag, short mask, graph_action_func pre_func, graph_action_func post_func, void **data)
{
	DagNode *node;

	node = dag->DagNode.first;
	return pre_and_post_source_DFS(dag, mask,  node,  pre_func,  post_func, data);
}

int pre_and_post_source_DFS(DagForest *dag, short mask, DagNode *source, graph_action_func pre_func, graph_action_func post_func, void **data)
{
	DagNode *node;
	DagNodeQueue *nqueue;
	DagAdjList *itA;
	int time;
	int skip = 0;
	int retval = 0;
	/*
	 *fprintf(stderr,"starting DFS \n ------------\n");
	 */	
	nqueue = queue_create(DAGQUEUEALLOC);
	
	/* Init
		* dagnode.first is alway the root (scene) 
		*/
	node = dag->DagNode.first;
	while(node) {
		node->color = DAG_WHITE;
		node->DFS_dist = 9999;
		node->DFS_dvtm = node->DFS_fntm = 9999;
		node->k = 0;
		node =  node->next;
	}
	
	time = 1;
	
	node = source;
	do {
		if (node->color == DAG_WHITE) {
			node->color = DAG_GRAY;
			node->DFS_dist = 1;
			node->DFS_dvtm = time;
			time++;
			push_stack(nqueue,node);  
			pre_func(node->ob,data);

			while(nqueue->count) {
				skip = 0;
				node = get_top_node_queue(nqueue);
								
				itA = node->child;
				while(itA != NULL) {
					if((itA->node->color == DAG_WHITE) && (itA->type & mask) ) {
						itA->node->DFS_dvtm = time;
						itA->node->color = DAG_GRAY;
						
						time++;
						itA->node->DFS_dist = node->DFS_dist + 1;
						push_stack(nqueue,itA->node);
						pre_func(node->ob,data);

						skip = 1;
						break;
					} else {
						if (itA->node->color == DAG_GRAY) {// back edge
							retval = 1;
						}
//						else if (itA->node->color == DAG_BLACK) { // cross or forward
//
//						}
					}
					itA = itA->next;
				}			
				
				if (!skip) {
					node = pop_queue(nqueue);
					node->color = DAG_BLACK;
					
					node->DFS_fntm = time;
					time++;
					post_func(node->ob,data);
				}
			}
		}
		node = node->next;
	} while (node);
	queue_delete(nqueue);
	return(retval);
}


// used to get the obs owning a datablock
struct DagNodeQueue *get_obparents(struct DagForest	*dag, void *ob) 
{
	DagNode * node, *node1;
	DagNodeQueue *nqueue;
	DagAdjList *itA;

	node = dag_find_node(dag,ob);
	if(node==NULL) {
		return NULL;
	}
	else if (node->ancestor_count == 1) { // simple case
		nqueue = queue_create(1);
		push_queue(nqueue,node);
	} else {	// need to go over the whole dag for adj list
		nqueue = queue_create(node->ancestor_count);
		
		node1 = dag->DagNode.first;
		do {
			if (node1->DFS_fntm > node->DFS_fntm) { // a parent is finished after child. must check adj list
				itA = node->child;
				while(itA != NULL) {
					if ((itA->node == node) && (itA->type == DAG_RL_DATA)) {
						push_queue(nqueue,node);
					}
					itA = itA->next;
				}
			}
			node1 = node1->next;
		} while (node1);
	}
	return nqueue;
}

struct DagNodeQueue *get_first_ancestors(struct DagForest	*dag, void *ob)
{
	DagNode * node, *node1;
	DagNodeQueue *nqueue;
	DagAdjList *itA;
	
	node = dag_find_node(dag,ob);
	
	// need to go over the whole dag for adj list
	nqueue = queue_create(node->ancestor_count);
	
	node1 = dag->DagNode.first;
	do {
		if (node1->DFS_fntm > node->DFS_fntm) { 
			itA = node->child;
			while(itA != NULL) {
				if (itA->node == node) {
					push_queue(nqueue,node);
				}
				itA = itA->next;
			}
		}
		node1 = node1->next;
	} while (node1);
	
	return nqueue;	
}

// standard DFS list
struct DagNodeQueue *get_all_childs(struct DagForest	*dag, void *ob)
{
	DagNode *node;
	DagNodeQueue *nqueue;
	DagNodeQueue *retqueue;
	DagAdjList *itA;
	int time;
	int skip = 0;

	nqueue = queue_create(DAGQUEUEALLOC);
	retqueue = queue_create(dag->numNodes); // was MainDag... why? (ton)
	
	node = dag->DagNode.first;
	while(node) {
		node->color = DAG_WHITE;
		node =  node->next;
	}
	
	time = 1;
	
	node = dag_find_node(dag, ob);   // could be done in loop above (ton)
	if(node) { // can be null for newly added objects
		
		node->color = DAG_GRAY;
		time++;
		push_stack(nqueue,node);  
		
		while(nqueue->count) {
			
			skip = 0;
			node = get_top_node_queue(nqueue);
					
			itA = node->child;
			while(itA != NULL) {
				if(itA->node->color == DAG_WHITE) {
					itA->node->DFS_dvtm = time;
					itA->node->color = DAG_GRAY;
					
					time++;
					push_stack(nqueue,itA->node);
					skip = 1;
					break;
				} 
				itA = itA->next;
			}			
			
			if (!skip) {
				node = pop_queue(nqueue);
				node->color = DAG_BLACK;
				
				time++;
				push_stack(retqueue,node);			
			}
		}
	}
	queue_delete(nqueue);
	return(retqueue);
}

/* unused */
short	are_obs_related(struct DagForest	*dag, void *ob1, void *ob2)
{
	DagNode * node;
	DagAdjList *itA;
	
	node = dag_find_node(dag, ob1);
	
	itA = node->child;
	while(itA != NULL) {
		if(itA->node->ob == ob2) {
			return itA->node->type;
		} 
		itA = itA->next;
	}
	return DAG_NO_RELATION;
}

int	is_acyclic( DagForest	*dag)
{
	return dag->is_acyclic;
}

void set_node_xy(DagNode *node, float x, float y)
{
	node->x = x;
	node->y = y;
}


/* debug test functions */

void graph_print_queue(DagNodeQueue *nqueue)
{	
	DagNodeQueueElem *queueElem;
	
	queueElem = nqueue->first;
	while(queueElem) {
		fprintf(stderr,"** %s %i %i-%i ",((ID *) queueElem->node->ob)->name,queueElem->node->color,queueElem->node->DFS_dvtm,queueElem->node->DFS_fntm);
		queueElem = queueElem->next;		
	}
	fprintf(stderr,"\n");
}

void graph_print_queue_dist(DagNodeQueue *nqueue)
{	
	DagNodeQueueElem *queueElem;
	int count;
	
	queueElem = nqueue->first;
	count = 0;
	while(queueElem) {
		fprintf(stderr,"** %25s %2.2i-%2.2i ",((ID *) queueElem->node->ob)->name,queueElem->node->DFS_dvtm,queueElem->node->DFS_fntm);
		while (count < queueElem->node->DFS_dvtm-1) { fputc(' ',stderr); count++;}
		fputc('|',stderr); 
		while (count < queueElem->node->DFS_fntm-2) { fputc('-',stderr); count++;}
		fputc('|',stderr);
		fputc('\n',stderr);
		count = 0;
		queueElem = queueElem->next;		
	}
	fprintf(stderr,"\n");
}

void graph_print_adj_list(void)
{
	DagNode *node;
	DagAdjList *itA;
	
	node = (getMainDag())->DagNode.first;
	while(node) {
		fprintf(stderr,"node : %s col: %i",((ID *) node->ob)->name, node->color);		
		itA = node->child;
		while (itA) {
			fprintf(stderr,"-- %s ",((ID *) itA->node->ob)->name);
			
			itA = itA->next;
		}
		fprintf(stderr,"\n");
		node = node->next;
	}
}

/* ************************ API *********************** */

/* mechanism to allow editors to be informed of depsgraph updates,
   to do their own updates based on changes... */
static void (*EditorsUpdateIDCb)(Main *bmain, ID *id)= NULL;
static void (*EditorsUpdateSceneCb)(Main *bmain, Scene *scene, int updated)= NULL;

void DAG_editors_update_cb(void (*id_func)(Main *bmain, ID *id), void (*scene_func)(Main *bmain, Scene *scene, int updated))
{
	EditorsUpdateIDCb= id_func;
	EditorsUpdateSceneCb= scene_func;
}

static void dag_editors_id_update(Main *bmain, ID *id)
{
	if(EditorsUpdateIDCb)
		EditorsUpdateIDCb(bmain, id);
}

static void dag_editors_scene_update(Main *bmain, Scene *scene, int updated)
{
	if(EditorsUpdateSceneCb)
		EditorsUpdateSceneCb(bmain, scene, updated);
}

/* groups with objects in this scene need to be put in the right order as well */
static void scene_sort_groups(Main *bmain, Scene *sce)
{
	Base *base;
	Group *group;
	GroupObject *go;
	Object *ob;
	
	/* test; are group objects all in this scene? */
	for(ob= bmain->object.first; ob; ob= ob->id.next) {
		ob->id.flag &= ~LIB_DOIT;
		ob->id.newid= NULL;	/* newid abuse for GroupObject */
	}
	for(base = sce->base.first; base; base= base->next)
		base->object->id.flag |= LIB_DOIT;
	
	for(group= bmain->group.first; group; group= group->id.next) {
		for(go= group->gobject.first; go; go= go->next) {
			if((go->ob->id.flag & LIB_DOIT)==0)
				break;
		}
		/* this group is entirely in this scene */
		if(go==NULL) {
			ListBase listb= {NULL, NULL};
			
			for(go= group->gobject.first; go; go= go->next)
				go->ob->id.newid= (ID *)go;
			
			/* in order of sorted bases we reinsert group objects */
			for(base = sce->base.first; base; base= base->next) {
				
				if(base->object->id.newid) {
					go= (GroupObject *)base->object->id.newid;
					base->object->id.newid= NULL;
					BLI_remlink( &group->gobject, go);
					BLI_addtail( &listb, go);
				}
			}
			/* copy the newly sorted listbase */
			group->gobject= listb;
		}
	}
}

/* sort the base list on dependency order */
void DAG_scene_sort(Main *bmain, Scene *sce)
{
	DagNode *node, *rootnode;
	DagNodeQueue *nqueue;
	DagAdjList *itA;
	int time;
	int skip = 0;
	ListBase tempbase;
	Base *base;
	
	tempbase.first= tempbase.last= NULL;
	
	build_dag(bmain, sce, DAG_RL_ALL_BUT_DATA);
	
	dag_check_cycle(sce->theDag);

	nqueue = queue_create(DAGQUEUEALLOC);
	
	for(node = sce->theDag->DagNode.first; node; node= node->next) {
		node->color = DAG_WHITE;
	}
	
	time = 1;
	
	rootnode = sce->theDag->DagNode.first;
	rootnode->color = DAG_GRAY;
	time++;
	push_stack(nqueue,rootnode);  
	
	while(nqueue->count) {
		
		skip = 0;
		node = get_top_node_queue(nqueue);
		
		itA = node->child;
		while(itA != NULL) {
			if(itA->node->color == DAG_WHITE) {
				itA->node->DFS_dvtm = time;
				itA->node->color = DAG_GRAY;
				
				time++;
				push_stack(nqueue,itA->node);
				skip = 1;
				break;
			} 
			itA = itA->next;
		}			
		
		if (!skip) {
			if (node) {
				node = pop_queue(nqueue);
				if (node->ob == sce)	// we are done
					break;
				node->color = DAG_BLACK;
				
				time++;
				base = sce->base.first;
				while (base && base->object != node->ob)
					base = base->next;
				if(base) {
					BLI_remlink(&sce->base,base);
					BLI_addhead(&tempbase,base);
				}
			}	
		}
	}
	
	// temporal correction for circular dependancies
	base = sce->base.first;
	while (base) {
		BLI_remlink(&sce->base,base);
		BLI_addhead(&tempbase,base);
		//if(G.f & G_DEBUG) 
			printf("cyclic %s\n", base->object->id.name);
		base = sce->base.first;
	}
	
	sce->base = tempbase;
	queue_delete(nqueue);
	
	/* all groups with objects in this scene gets resorted too */
	scene_sort_groups(bmain, sce);
	
	if(G.f & G_DEBUG) {
		printf("\nordered\n");
		for(base = sce->base.first; base; base= base->next) {
			printf(" %s\n", base->object->id.name);
		}
	}
	/* temporal...? */
	sce->recalc |= SCE_PRV_CHANGED;	/* test for 3d preview */
}

static void lib_id_recalc_tag(Main *bmain, ID *id)
{
	id->flag |= LIB_ID_RECALC;
	bmain->id_tag_update[id->name[0]] = 1;
}

static void lib_id_recalc_data_tag(Main *bmain, ID *id)
{
	id->flag |= LIB_ID_RECALC_DATA;
	bmain->id_tag_update[id->name[0]] = 1;
}

/* node was checked to have lasttime != curtime and is if type ID_OB */
static void flush_update_node(DagNode *node, unsigned int layer, int curtime)
{
	Main *bmain= G.main;
	DagAdjList *itA;
	Object *ob, *obc;
	int oldflag, changed=0;
	unsigned int all_layer;
	
	node->lasttime= curtime;
	
	ob= node->ob;
	if(ob && (ob->recalc & OB_RECALC_ALL)) {
		all_layer= node->scelay;

		/* got an object node that changes, now check relations */
		for(itA = node->child; itA; itA= itA->next) {
			all_layer |= itA->lay;
			/* the relationship is visible */
			if((itA->lay & layer)) { // XXX || (itA->node->ob == obedit)
				if(itA->node->type==ID_OB) {
					obc= itA->node->ob;
					oldflag= obc->recalc;
					
					/* got a ob->obc relation, now check if flag needs flush */
					if(ob->recalc & OB_RECALC_OB) {
						if(itA->type & DAG_RL_OB_OB) {
							//printf("ob %s changes ob %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_OB;
							lib_id_recalc_tag(bmain, &obc->id);
						}
						if(itA->type & DAG_RL_OB_DATA) {
							//printf("ob %s changes obdata %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_DATA;
							lib_id_recalc_data_tag(bmain, &obc->id);
						}
					}
					if(ob->recalc & OB_RECALC_DATA) {
						if(itA->type & DAG_RL_DATA_OB) {
							//printf("obdata %s changes ob %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_OB;
							lib_id_recalc_tag(bmain, &obc->id);
						}
						if(itA->type & DAG_RL_DATA_DATA) {
							//printf("obdata %s changes obdata %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_DATA;
							lib_id_recalc_data_tag(bmain, &obc->id);
						}
					}
					if(oldflag!=obc->recalc) changed= 1;
				}
			}
		}
		/* even nicer, we can clear recalc flags...  */
		if((all_layer & layer)==0) { // XXX && (ob != obedit)) {
			/* but existing displaylists or derivedmesh should be freed */
			if(ob->recalc & OB_RECALC_DATA)
				object_free_display(ob);
			
			ob->recalc &= ~OB_RECALC_ALL;
		}
	}
	
	/* check case where child changes and parent forcing obdata to change */
	/* should be done regardless if this ob has recalc set */
	/* could merge this in with loop above...? (ton) */
	for(itA = node->child; itA; itA= itA->next) {
		/* the relationship is visible */
		if((itA->lay & layer)) {		// XXX  || (itA->node->ob == obedit)
			if(itA->node->type==ID_OB) {
				obc= itA->node->ob;
				/* child moves */
				if((obc->recalc & OB_RECALC_ALL)==OB_RECALC_OB) {
					/* parent has deforming info */
					if(itA->type & (DAG_RL_OB_DATA|DAG_RL_DATA_DATA)) {
						// printf("parent %s changes ob %s\n", ob->id.name, obc->id.name);
						obc->recalc |= OB_RECALC_DATA;
						lib_id_recalc_data_tag(bmain, &obc->id);
					}
				}
			}
		}
	}
	
	/* we only go deeper if node not checked or something changed  */
	for(itA = node->child; itA; itA= itA->next) {
		if(changed || itA->node->lasttime!=curtime) 
			flush_update_node(itA->node, layer, curtime);
	}
	
}

/* node was checked to have lasttime != curtime , and is of type ID_OB */
static unsigned int flush_layer_node(Scene *sce, DagNode *node, int curtime)
{
	DagAdjList *itA;
	
	node->lasttime= curtime;
	node->lay= node->scelay;
	
	for(itA = node->child; itA; itA= itA->next) {
		if(itA->node->type==ID_OB) {
			if(itA->node->lasttime!=curtime) {
				itA->lay= flush_layer_node(sce, itA->node, curtime);  // lay is only set once for each relation
			}
			else itA->lay= itA->node->lay;
			
			node->lay |= itA->lay;
		}
	}

	return node->lay;
}

/* node was checked to have lasttime != curtime , and is of type ID_OB */
static void flush_pointcache_reset(Scene *scene, DagNode *node, int curtime, int reset)
{
	Main *bmain= G.main;
	DagAdjList *itA;
	Object *ob;
	
	node->lasttime= curtime;
	
	for(itA = node->child; itA; itA= itA->next) {
		if(itA->node->type==ID_OB) {
			if(itA->node->lasttime!=curtime) {
				ob= (Object*)(itA->node->ob);

				if(reset || (ob->recalc & OB_RECALC_ALL)) {
					if(BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_DEPSGRAPH)) {
						ob->recalc |= OB_RECALC_DATA;
						lib_id_recalc_data_tag(bmain, &ob->id);
					}

					flush_pointcache_reset(scene, itA->node, curtime, 1);
				}
				else
					flush_pointcache_reset(scene, itA->node, curtime, 0);
			}
		}
	}
}

/* flush layer flags to dependencies */
static void dag_scene_flush_layers(Scene *sce, int lay)
{
	DagNode *node, *firstnode;
	DagAdjList *itA;
	Base *base;
	int lasttime;

	firstnode= sce->theDag->DagNode.first;  // always scene node

	for(itA = firstnode->child; itA; itA= itA->next)
		itA->lay= 0;

	sce->theDag->time++;	// so we know which nodes were accessed
	lasttime= sce->theDag->time;

	/* update layer flags in nodes */
	for(base= sce->base.first; base; base= base->next) {
		node= dag_get_node(sce->theDag, base->object);
		node->scelay= base->object->lay;
	}

	/* ensure cameras are set as if they are on a visible layer, because
	 * they ared still used for rendering or setting the camera view
	 *
	 * XXX, this wont work for local view / unlocked camera's */
	if(sce->camera) {
		node= dag_get_node(sce->theDag, sce->camera);
		node->scelay |= lay;
	}

#ifdef DURIAN_CAMERA_SWITCH
	{
		TimeMarker *m;

		for(m= sce->markers.first; m; m= m->next) {
			if(m->camera) {
				node= dag_get_node(sce->theDag, m->camera);
				node->scelay |= lay;
			}
		}
	}
#endif

	/* flush layer nodes to dependencies */
	for(itA = firstnode->child; itA; itA= itA->next)
		if(itA->node->lasttime!=lasttime && itA->node->type==ID_OB) 
			flush_layer_node(sce, itA->node, lasttime);
}

static void dag_tag_renderlayers(Scene *sce, unsigned int lay)
{
	if(sce->nodetree) {
		bNode *node;
		Base *base;
		unsigned int lay_changed= 0;
		
		for(base= sce->base.first; base; base= base->next)
			if(base->lay & lay)
				if(base->object->recalc)
					lay_changed |= base->lay;
			
		for(node= sce->nodetree->nodes.first; node; node= node->next) {
			if(node->id==(ID *)sce) {
				SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
				if(srl && (srl->lay & lay_changed))
					nodeUpdate(sce->nodetree, node);
			}
		}
	}
}

/* flushes all recalc flags in objects down the dependency tree */
void DAG_scene_flush_update(Main *bmain, Scene *sce, unsigned int lay, const short time)
{
	DagNode *firstnode;
	DagAdjList *itA;
	Object *ob;
	int lasttime;
	
	if(sce->theDag==NULL) {
		printf("DAG zero... not allowed to happen!\n");
		DAG_scene_sort(bmain, sce);
	}
	
	firstnode= sce->theDag->DagNode.first;  // always scene node

	/* first we flush the layer flags */
	dag_scene_flush_layers(sce, lay);

	/* then we use the relationships + layer info to flush update events */
	sce->theDag->time++;	// so we know which nodes were accessed
	lasttime= sce->theDag->time;
	for(itA = firstnode->child; itA; itA= itA->next)
		if(itA->node->lasttime!=lasttime && itA->node->type==ID_OB)
			flush_update_node(itA->node, lay, lasttime);

	/* if update is not due to time change, do pointcache clears */
	if(!time) {
		sce->theDag->time++;	// so we know which nodes were accessed
		lasttime= sce->theDag->time;
		for(itA = firstnode->child; itA; itA= itA->next) {
			if(itA->node->lasttime!=lasttime && itA->node->type==ID_OB) {
				ob= (Object*)(itA->node->ob);

				if(ob->recalc & OB_RECALC_ALL) {
					if(BKE_ptcache_object_reset(sce, ob, PTCACHE_RESET_DEPSGRAPH)) {
						ob->recalc |= OB_RECALC_DATA;
						lib_id_recalc_data_tag(bmain, &ob->id);
					}

					flush_pointcache_reset(sce, itA->node, lasttime, 1);
				}
				else
					flush_pointcache_reset(sce, itA->node, lasttime, 0);
			}
		}
	}
	
	dag_tag_renderlayers(sce, lay);
}

static int object_modifiers_use_time(Object *ob)
{
	ModifierData *md;
	
	/* check if a modifier in modifier stack needs time input */
	for (md=ob->modifiers.first; md; md=md->next)
		if (modifier_dependsOnTime(md))
			return 1;
	
	/* check whether any modifiers are animated */
	if (ob->adt) {
		AnimData *adt = ob->adt;
		FCurve *fcu;
		
		/* action - check for F-Curves with paths containing 'modifiers[' */
		if (adt->action) {
			for (fcu = adt->action->curves.first; fcu; fcu = fcu->next) {
				if (fcu->rna_path && strstr(fcu->rna_path, "modifiers["))
					return 1;
			}
		}
		
		/* This here allows modifier properties to get driven and still update properly
		 *
		 * Workaround to get [#26764] (e.g. subsurf levels not updating when animated/driven)
		 * working, without the updating problems ([#28525] [#28690] [#28774] [#28777]) caused
		 * by the RNA updates cache introduced in r.38649
		 */
		for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
			if (fcu->rna_path && strstr(fcu->rna_path, "modifiers["))
				return 1;
		}
		
		// XXX: also, should check NLA strips, though for now assume that nobody uses
		// that and we can omit that for performance reasons...
	}
	
	return 0;
}

static short animdata_use_time(AnimData *adt)
{
	NlaTrack *nlt;
	
	if(adt==NULL) return 0;
	
	/* check action - only if assigned, and it has anim curves */
	if (adt->action && adt->action->curves.first)
		return 1;
	
	/* check NLA tracks + strips */
	for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
		if (nlt->strips.first)
			return 1;
	}
	
	/* If we have drivers, more likely than not, on a frame change
	 * they'll need updating because their owner changed
	 * 
	 * This is kindof a hack to get around a whole host of problems
	 * involving drivers using non-object datablock data (which the 
	 * depsgraph currently has no way of representing let alone correctly
	 * dependency sort+tagging). By doing this, at least we ensure that 
	 * some commonly attempted drivers (such as scene -> current frame;
	 * see "Driver updates fail" thread on Bf-committers dated July 2)
	 * will work correctly, and that other non-object datablocks will have
	 * their drivers update at least on frame change.
	 *
	 * -- Aligorith, July 4 2011
	 */
	if (adt->drivers.first)
		return 1;
	
	return 0;
}

static void dag_object_time_update_flags(Object *ob)
{
	if(ob->constraints.first) {
		bConstraint *con;
		for (con = ob->constraints.first; con; con=con->next) {
			bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			if (cti) {
				/* special case for camera tracking -- it doesn't use targets to define relations */
				if(ELEM3(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER, CONSTRAINT_TYPE_OBJECTSOLVER)) {
					ob->recalc |= OB_RECALC_OB;
				}
				else if (cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						if (ct->tar) {
							ob->recalc |= OB_RECALC_OB;
							break;
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 1);
				}
				
			}
		}
	}
	
	if(ob->parent) {
		/* motion path or bone child */
		if(ob->parent->type==OB_CURVE || ob->parent->type==OB_ARMATURE) ob->recalc |= OB_RECALC_OB;
	}
	
#if 0 // XXX old animation system
	if(ob->nlastrips.first) {
		if(ob->dup_group) {
			bActionStrip *strip;
			/* this case is for groups with nla, whilst nla target has no action or nla */
			for(strip= ob->nlastrips.first; strip; strip= strip->next) {
				if(strip->object)
					strip->object->recalc |= OB_RECALC_ALL;
			}
		}
	}
#endif // XXX old animation system
	
	if(animdata_use_time(ob->adt)) {
		ob->recalc |= OB_RECALC_OB;
		ob->adt->recalc |= ADT_RECALC_ANIM;
	}
	
	if((ob->adt) && (ob->type==OB_ARMATURE)) ob->recalc |= OB_RECALC_DATA;
	
	if(object_modifiers_use_time(ob)) ob->recalc |= OB_RECALC_DATA;
	if((ob->pose) && (ob->pose->flag & POSE_CONSTRAINTS_TIMEDEPEND)) ob->recalc |= OB_RECALC_DATA;
	
	{
		AnimData *adt= BKE_animdata_from_id((ID *)ob->data);
		Mesh *me;
		Curve *cu;
		Lattice *lt;
		
		switch(ob->type) {
			case OB_MESH:
				me= ob->data;
				if(me->key) {
					if(!(ob->shapeflag & OB_SHAPE_LOCK)) {
						ob->recalc |= OB_RECALC_DATA;
					}
				}
				if(ob->particlesystem.first)
					ob->recalc |= OB_RECALC_DATA;
				break;
			case OB_CURVE:
			case OB_SURF:
				cu= ob->data;
				if(cu->key) {
					if(!(ob->shapeflag & OB_SHAPE_LOCK)) {
						ob->recalc |= OB_RECALC_DATA;
					}
				}
				break;
			case OB_FONT:
				cu= ob->data;
				if(cu->nurb.first==NULL && cu->str && cu->vfont)
					ob->recalc |= OB_RECALC_DATA;
				break;
			case OB_LATTICE:
				lt= ob->data;
				if(lt->key) {
					if(!(ob->shapeflag & OB_SHAPE_LOCK)) {
						ob->recalc |= OB_RECALC_DATA;
					}
				}
					break;
			case OB_MBALL:
				if(ob->transflag & OB_DUPLI) ob->recalc |= OB_RECALC_DATA;
				break;
		}
		
		if(animdata_use_time(adt)) {
			ob->recalc |= OB_RECALC_DATA;
			adt->recalc |= ADT_RECALC_ANIM;
		}

		if(ob->particlesystem.first) {
			ParticleSystem *psys= ob->particlesystem.first;

			for(; psys; psys=psys->next) {
				if(psys_check_enabled(ob, psys)) {
					ob->recalc |= OB_RECALC_DATA;
					break;
				}
			}
		}
	}		

	if(ob->recalc & OB_RECALC_OB)
		lib_id_recalc_tag(G.main, &ob->id);
	if(ob->recalc & OB_RECALC_DATA)
		lib_id_recalc_data_tag(G.main, &ob->id);

}
/* flag all objects that need recalc, for changes in time for example */
/* do_time: make this optional because undo resets objects to their animated locations without this */
void DAG_scene_update_flags(Main *bmain, Scene *scene, unsigned int lay, const short do_time)
{
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	Scene *sce_iter;

	/* set ob flags where animated systems are */
	for(SETLOOPER(scene, sce_iter, base)) {
		ob= base->object;

		if(do_time) {
			/* now if DagNode were part of base, the node->lay could be checked... */
			/* we do all now, since the scene_flush checks layers and clears recalc flags even */
			dag_object_time_update_flags(ob);
		}

		/* handled in next loop */
		if(ob->dup_group)
			ob->dup_group->id.flag |= LIB_DOIT;
	}

	if(do_time) {
		/* we do groups each once */
		for(group= bmain->group.first; group; group= group->id.next) {
			if(group->id.flag & LIB_DOIT) {
				for(go= group->gobject.first; go; go= go->next) {
					dag_object_time_update_flags(go->ob);
				}
			}
		}
	}

	for(sce_iter= scene; sce_iter; sce_iter= sce_iter->set)
		DAG_scene_flush_update(bmain, sce_iter, lay, 1);
	
	if(do_time) {
		/* test: set time flag, to disable baked systems to update */
		for(SETLOOPER(scene, sce_iter, base)) {
			ob= base->object;
			if(ob->recalc)
				ob->recalc |= OB_RECALC_TIME;
		}

		/* hrmf... an exception to look at once, for invisible camera object we do it over */
		if(scene->camera)
			dag_object_time_update_flags(scene->camera);
	}

	/* and store the info in groupobject */
	for(group= bmain->group.first; group; group= group->id.next) {
		if(group->id.flag & LIB_DOIT) {
			for(go= group->gobject.first; go; go= go->next) {
				go->recalc= go->ob->recalc;
				// printf("ob %s recalc %d\n", go->ob->id.name, go->recalc);
			}
			group->id.flag &= ~LIB_DOIT;
		}
	}
	
}

static void dag_current_scene_layers(Main *bmain, Scene **sce, unsigned int *lay)
{
	wmWindowManager *wm;
	wmWindow *win;

	/* only one scene supported currently, making more scenes work
	   correctly requires changes beyond just the dependency graph */

	*sce= NULL;
	*lay= 0;

	if((wm= bmain->wm.first)) {
		/* if we have a windowmanager, look into windows */
		for(win=wm->windows.first; win; win=win->next) {
			if(win->screen) {
				if(!*sce) *sce= win->screen->scene;
				*lay |= BKE_screen_visible_layers(win->screen, win->screen->scene);
			}
		}
	}
	else {
		/* if not, use the first sce */
		*sce= bmain->scene.first;
		if(*sce) *lay= (*sce)->lay;

		/* XXX for background mode, we should get the scene
		   from somewhere, for the -S option, but it's in
		   the context, how to get it here? */
	}
}

void DAG_ids_flush_update(Main *bmain, int time)
{
	Scene *sce;
	unsigned int lay;

	dag_current_scene_layers(bmain, &sce, &lay);

	if(sce)
		DAG_scene_flush_update(bmain, sce, lay, time);
}

void DAG_on_visible_update(Main *bmain, const short do_time)
{
	Scene *scene;
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	DagNode *node;
	unsigned int lay, oblay;

	dag_current_scene_layers(bmain, &scene, &lay);

	if(scene && scene->theDag) {
		Scene *sce_iter;
		/* derivedmeshes and displists are not saved to file so need to be
		   remade, tag them so they get remade in the scene update loop,
		   note armature poses or object matrices are preserved and do not
		   require updates, so we skip those */
		dag_scene_flush_layers(scene, lay);

		for(SETLOOPER(scene, sce_iter, base)) {
			ob= base->object;
			node= (sce_iter->theDag)? dag_get_node(sce_iter->theDag, ob): NULL;
			oblay= (node)? node->lay: ob->lay;

			if((oblay & lay) & ~scene->lay_updated) {
				if(ELEM6(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_LATTICE))
					ob->recalc |= OB_RECALC_DATA;
				if(ob->dup_group) 
					ob->dup_group->id.flag |= LIB_DOIT;
			}
		}

		for(group= bmain->group.first; group; group= group->id.next) {
			if(group->id.flag & LIB_DOIT) {
				for(go= group->gobject.first; go; go= go->next) {
					if(ELEM6(go->ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_LATTICE))
						go->ob->recalc |= OB_RECALC_DATA;
					if(go->ob->proxy_from)
						go->ob->recalc |= OB_RECALC_OB;
				}
				
				group->id.flag &= ~LIB_DOIT;
			}
		}

		/* now tag update flags, to ensure deformers get calculated on redraw */
		DAG_scene_update_flags(bmain, scene, lay, do_time);
		scene->lay_updated |= lay;
	}

	/* hack to get objects updating on layer changes */
	DAG_id_type_tag(bmain, ID_OB);
}

static void dag_id_flush_update__isDependentTexture(void *userData, Object *UNUSED(ob), ID **idpoin)
{
	struct { ID *id; int is_dependent; } *data = userData;
	
	if(*idpoin && GS((*idpoin)->name)==ID_TE) {
		if (data->id == (*idpoin))
			data->is_dependent = 1;
	}
}

static void dag_id_flush_update(Scene *sce, ID *id)
{
	Main *bmain= G.main;
	Object *obt, *ob= NULL;
	short idtype;

	/* here we flush a few things before actual scene wide flush, mostly
	   due to only objects and not other datablocks being in the depsgraph */

	/* set flags & pointcache for object */
	if(GS(id->name) == ID_OB) {
		ob= (Object*)id;
		BKE_ptcache_object_reset(sce, ob, PTCACHE_RESET_DEPSGRAPH);

		if(ob->recalc & OB_RECALC_DATA) {
			/* all users of this ob->data should be checked */
			id= ob->data;

			/* no point in trying in this cases */
			if(id && id->us <= 1) {
				dag_editors_id_update(bmain, id);
				id= NULL;
			}
		}
	}

	/* set flags & pointcache for object data */
	if(id) {
		idtype= GS(id->name);

		if(ELEM8(idtype, ID_ME, ID_CU, ID_MB, ID_LA, ID_LT, ID_CA, ID_AR, ID_SPK)) {
			for(obt=bmain->object.first; obt; obt= obt->id.next) {
				if(!(ob && obt == ob) && obt->data == id) {
					obt->recalc |= OB_RECALC_DATA;
					lib_id_recalc_data_tag(bmain, &obt->id);
					BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
				}
			}
		}
		
		/* set flags based on textures - can influence depgraph via modifiers */
		if(idtype == ID_TE) {
			for(obt=bmain->object.first; obt; obt= obt->id.next) {
				struct { ID *id; int is_dependent; } data;
				data.id= id;
				data.is_dependent= 0;

				modifiers_foreachIDLink(obt, dag_id_flush_update__isDependentTexture, &data);
				if (data.is_dependent) {
					obt->recalc |= OB_RECALC_DATA;
					lib_id_recalc_data_tag(bmain, &obt->id);
				}

				/* particle settings can use the texture as well */
				if(obt->particlesystem.first) {
					ParticleSystem *psys = obt->particlesystem.first;
					MTex **mtexp, *mtex;
					int a;
					for(; psys; psys=psys->next) {
						mtexp = psys->part->mtex;
						for(a=0; a<MAX_MTEX; a++, mtexp++) {
							mtex = *mtexp;
							if(mtex && mtex->tex == (Tex*)id) {
								obt->recalc |= OB_RECALC_DATA;
								lib_id_recalc_data_tag(bmain, &obt->id);

								if(mtex->mapto & PAMAP_INIT)
									psys->recalc |= PSYS_RECALC_RESET;
								if(mtex->mapto & PAMAP_CHILD)
									psys->recalc |= PSYS_RECALC_CHILD;

								BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
							}
						}
					}
				}
			}
		}
		
		/* set flags based on ShapeKey */
		if(idtype == ID_KE) {
			for(obt=bmain->object.first; obt; obt= obt->id.next) {
				Key *key= ob_get_key(obt);
				if(!(ob && obt == ob) && ((ID *)key == id)) {
					obt->flag |= (OB_RECALC_OB|OB_RECALC_DATA);
					lib_id_recalc_tag(bmain, &obt->id);
					lib_id_recalc_data_tag(bmain, &obt->id);
					BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
				}
			}
		}
		
		/* set flags based on particle settings */
		if(idtype == ID_PA) {
			ParticleSystem *psys;
			for(obt=bmain->object.first; obt; obt= obt->id.next)
				for(psys=obt->particlesystem.first; psys; psys=psys->next)
					if(&psys->part->id == id)
						BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
		}

		if(idtype == ID_MC) {
			for(obt=bmain->object.first; obt; obt= obt->id.next) {
				bConstraint *con;
				for (con = obt->constraints.first; con; con=con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					if(ELEM3(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER,
					         CONSTRAINT_TYPE_OBJECTSOLVER))
					{
						obt->recalc |= OB_RECALC_OB;
						break;
					}
				}
			}

			if(sce->nodetree) {
				bNode *node;

				for(node= sce->nodetree->nodes.first; node; node= node->next) {
					if(node->id==id) {
						nodeUpdate(sce->nodetree, node);
					}
				}
			}
		}

		/* camera's matrix is used to orient reconstructed stuff,
		   so it should happen tracking-related constraints recalculation
		   when camera is changing (sergey) */
		if(sce->camera && &sce->camera->id == id && object_get_movieclip(sce, sce->camera, 1)) {
			dag_id_flush_update(sce, &sce->clip->id);
		}

		/* update editors */
		dag_editors_id_update(bmain, id);
	}
}

void DAG_ids_flush_tagged(Main *bmain)
{
	ListBase *lbarray[MAX_LIBARRAY];
	Scene *sce;
	unsigned int lay;
	int a, do_flush = 0;

	dag_current_scene_layers(bmain, &sce, &lay);

	if(!sce || !sce->theDag)
		return;

	/* loop over all ID types */
	a  = set_listbasepointers(bmain, lbarray);

	while(a--) {
		ListBase *lb = lbarray[a];
		ID *id = lb->first;

		/* we tag based on first ID type character to avoid 
		   looping over all ID's in case there are no tags */
		if(id && bmain->id_tag_update[id->name[0]]) {
			for(; id; id=id->next) {
				if(id->flag & (LIB_ID_RECALC|LIB_ID_RECALC_DATA)) {
					dag_id_flush_update(sce, id);
					do_flush = 1;
				}
			}
		}
	}

	/* flush changes to other objects */
	if(do_flush)
		DAG_scene_flush_update(bmain, sce, lay, 0);
}

void DAG_ids_check_recalc(Main *bmain, Scene *scene, int time)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a, updated = 0;

	/* loop over all ID types */
	a  = set_listbasepointers(bmain, lbarray);

	while(a--) {
		ListBase *lb = lbarray[a];
		ID *id = lb->first;

		/* we tag based on first ID type character to avoid 
		   looping over all ID's in case there are no tags */
		if(id && bmain->id_tag_update[id->name[0]]) {
			updated= 1;
			break;
		}
	}

	dag_editors_scene_update(bmain, scene, (updated || time));
}

void DAG_ids_clear_recalc(Main *bmain)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;

	/* loop over all ID types */
	a  = set_listbasepointers(bmain, lbarray);

	while(a--) {
		ListBase *lb = lbarray[a];
		ID *id = lb->first;

		/* we tag based on first ID type character to avoid 
		   looping over all ID's in case there are no tags */
		if(id && bmain->id_tag_update[id->name[0]]) {
			for(; id; id=id->next)
				if(id->flag & (LIB_ID_RECALC|LIB_ID_RECALC_DATA))
					id->flag &= ~(LIB_ID_RECALC|LIB_ID_RECALC_DATA);
		}
	}

	memset(bmain->id_tag_update, 0, sizeof(bmain->id_tag_update));
}

void DAG_id_tag_update(ID *id, short flag)
{
	Main *bmain= G.main;

	if(id==NULL) return;
	
	/* tag ID for update */
	if(flag) {
		if(flag & OB_RECALC_OB)
			lib_id_recalc_tag(bmain, id);
		if(flag & (OB_RECALC_DATA|PSYS_RECALC))
			lib_id_recalc_data_tag(bmain, id);
	}
	else
		lib_id_recalc_tag(bmain, id);

	/* flag is for objects and particle systems */
	if(flag) {
		Object *ob;
		ParticleSystem *psys;
		short idtype = GS(id->name);

		if(idtype == ID_OB) {
			/* only quick tag */
			ob = (Object*)id;
			ob->recalc |= (flag & OB_RECALC_ALL);
		}
		else if(idtype == ID_PA) {
			/* this is weak still, should be done delayed as well */
			for(ob=bmain->object.first; ob; ob=ob->id.next) {
				for(psys=ob->particlesystem.first; psys; psys=psys->next) {
					if(&psys->part->id == id) {
						ob->recalc |= (flag & OB_RECALC_ALL);
						psys->recalc |= (flag & PSYS_RECALC);
						lib_id_recalc_tag(bmain, &ob->id);
						lib_id_recalc_data_tag(bmain, &ob->id);
					}
				}
			}
		}
		else {
			/* disable because this is called on various ID types automatically.
			 * where printing warning is not useful. for now just ignore */
			/* BLI_assert(!"invalid flag for this 'idtype'"); */
		}
	}
}

void DAG_id_type_tag(struct Main *bmain, short idtype)
{
	bmain->id_tag_update[((char*)&idtype)[0]] = 1;
}

int DAG_id_type_tagged(Main *bmain, short idtype)
{
	return bmain->id_tag_update[((char*)&idtype)[0]];
}

#if 0 // UNUSED
/* recursively descends tree, each node only checked once */
/* node is checked to be of type object */
static int parent_check_node(DagNode *node, int curtime)
{
	DagAdjList *itA;
	
	node->lasttime= curtime;
	
	if(node->color==DAG_GRAY)
		return DAG_GRAY;
	
	for(itA = node->child; itA; itA= itA->next) {
		if(itA->node->type==ID_OB) {
			
			if(itA->node->color==DAG_GRAY)
				return DAG_GRAY;

			/* descend if not done */
			if(itA->node->lasttime!=curtime) {
				itA->node->color= parent_check_node(itA->node, curtime);
			
				if(itA->node->color==DAG_GRAY)
					return DAG_GRAY;
			}
		}
	}
	
	return DAG_WHITE;
}
#endif

/* ******************* DAG FOR ARMATURE POSE ***************** */

/* we assume its an armature with pose */
void DAG_pose_sort(Object *ob)
{
	bPose *pose= ob->pose;
	bPoseChannel *pchan;
	bConstraint *con;
	DagNode *node;
	DagNode *node2, *node3;
	DagNode *rootnode;
	DagForest *dag;
	DagNodeQueue *nqueue;
	DagAdjList *itA;
	ListBase tempbase;
	int skip = 0;
	
	dag = dag_init();
	ugly_hack_sorry= 0;	// no ID structs

	rootnode = dag_add_node(dag, NULL);	// node->ob becomes NULL
	
	/* we add the hierarchy and the constraints */
	for(pchan = pose->chanbase.first; pchan; pchan= pchan->next) {
		int addtoroot = 1;
		
		node = dag_get_node(dag, pchan);
		
		if(pchan->parent) {
			node2 = dag_get_node(dag, pchan->parent);
			dag_add_relation(dag, node2, node, 0, "Parent Relation");
			addtoroot = 0;
		}
		for (con = pchan->constraints.first; con; con=con->next) {
			bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(con, &targets);
				
				for (ct= targets.first; ct; ct= ct->next) {
					if (ct->tar==ob && ct->subtarget[0]) {
						bPoseChannel *target= get_pose_channel(ob->pose, ct->subtarget);
						if (target) {
							node2= dag_get_node(dag, target);
							dag_add_relation(dag, node2, node, 0, "Pose Constraint");
							
							if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
								bKinematicConstraint *data = (bKinematicConstraint *)con->data;
								bPoseChannel *parchan;
								int segcount= 0;
								
								/* exclude tip from chain? */
								if(!(data->flag & CONSTRAINT_IK_TIP))
									parchan= pchan->parent;
								else
									parchan= pchan;
								
								/* Walk to the chain's root */
								while (parchan) {
									node3= dag_get_node(dag, parchan);
									dag_add_relation(dag, node2, node3, 0, "IK Constraint");
									
									segcount++;
									if (segcount==data->rootbone || segcount>255) break; // 255 is weak
									parchan= parchan->parent;
								}
							}
						}
					}
				}
				
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 1);
			}
		}
		if (addtoroot == 1 ) {
			dag_add_relation(dag, rootnode, node, 0, "Root Bone Relation");
		}
	}

	dag_check_cycle(dag);
	
	/* now we try to sort... */
	tempbase.first= tempbase.last= NULL;

	nqueue = queue_create(DAGQUEUEALLOC);
	
	/* tag nodes unchecked */
	for(node = dag->DagNode.first; node; node= node->next) 
		node->color = DAG_WHITE;
	
	rootnode->color = DAG_GRAY;
	push_stack(nqueue, rootnode);  
	
	while(nqueue->count) {
		
		skip = 0;
		node = get_top_node_queue(nqueue);
		
		itA = node->child;
		while(itA != NULL) {
			if(itA->node->color == DAG_WHITE) {
				itA->node->color = DAG_GRAY;
				push_stack(nqueue,itA->node);
				skip = 1;
				break;
			} 
			itA = itA->next;
		}			
		
		if (!skip) {
			if (node) {
				node = pop_queue(nqueue);
				if (node->ob == NULL)	// we are done
					break;
				node->color = DAG_BLACK;
				
				/* put node in new list */
				BLI_remlink(&pose->chanbase, node->ob);
				BLI_addhead(&tempbase, node->ob);
			}	
		}
	}
	
	// temporal correction for circular dependancies
	while(pose->chanbase.first) {
		pchan= pose->chanbase.first;
		BLI_remlink(&pose->chanbase, pchan);
		BLI_addhead(&tempbase, pchan);

		printf("cyclic %s\n", pchan->name);
	}
	
	pose->chanbase = tempbase;
	queue_delete(nqueue);
	
//	printf("\nordered\n");
//	for(pchan = pose->chanbase.first; pchan; pchan= pchan->next) {
//		printf(" %s\n", pchan->name);
//	}
	
	free_forest( dag );
	MEM_freeN( dag );
	
	ugly_hack_sorry= 1;
}

/* ************************ DAG DEBUGGING ********************* */

void DAG_print_dependencies(Main *bmain, Scene *scene, Object *ob)
{
	/* utility for debugging dependencies */
	dag_print_dependencies= 1;

	if(ob && (ob->mode & OB_MODE_POSE)) {
		printf("\nDEPENDENCY RELATIONS for %s\n\n", ob->id.name+2);
		DAG_pose_sort(ob);
	}
	else {
		printf("\nDEPENDENCY RELATIONS for %s\n\n", scene->id.name+2);
		DAG_scene_sort(bmain, scene);
	}
	
	dag_print_dependencies= 0;
}

