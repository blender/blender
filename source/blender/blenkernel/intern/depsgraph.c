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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_tracking.h"

#include "atomic_ops.h"

#include "depsgraph_private.h"

static SpinLock threaded_update_lock;

void DAG_init(void)
{
	BLI_spin_init(&threaded_update_lock);
}

void DAG_exit(void)
{
	BLI_spin_end(&threaded_update_lock);
}

/* Queue and stack operations for dag traversal 
 *
 * the queue store a list of freenodes to avoid successive alloc/dealloc
 */

DagNodeQueue *queue_create(int slots)
{
	DagNodeQueue *queue;
	DagNodeQueueElem *elem;
	int i;
	
	queue = MEM_mallocN(sizeof(DagNodeQueue), "DAG queue");
	queue->freenodes = MEM_mallocN(sizeof(DagNodeQueue), "DAG queue");
	queue->count = 0;
	queue->maxlevel = 0;
	queue->first = queue->last = NULL;
	elem = MEM_mallocN(sizeof(DagNodeQueueElem), "DAG queue elem3");
	elem->node = NULL;
	elem->next = NULL;
	queue->freenodes->first = queue->freenodes->last = elem;
	
	for (i = 1; i < slots; i++) {
		elem = MEM_mallocN(sizeof(DagNodeQueueElem), "DAG queue elem4");
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
	DagNodeQueueElem *elem;
	
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
	DagNodeQueueElem *elem;
	DagNodeQueueElem *temp;
	
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
	DagNodeQueueElem *elem;
	int i;

	if (node == NULL) {
		fprintf(stderr, "pushing null node\n");
		return;
	}
	/*fprintf(stderr, "BFS push : %s %d\n", ((ID *) node->ob)->name, queue->count);*/

	elem = queue->freenodes->first;
	if (elem != NULL) {
		queue->freenodes->first = elem->next;
		if (queue->freenodes->last == elem) {
			queue->freenodes->last = NULL;
			queue->freenodes->first = NULL;
		}
		queue->freenodes->count--;
	}
	else { /* alllocating more */
		elem = MEM_mallocN(sizeof(DagNodeQueueElem), "DAG queue elem1");
		elem->node = NULL;
		elem->next = NULL;
		queue->freenodes->first = queue->freenodes->last = elem;

		for (i = 1; i < DAGQUEUEALLOC; i++) {
			elem = MEM_mallocN(sizeof(DagNodeQueueElem), "DAG queue elem2");
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
	DagNodeQueueElem *elem;
	int i;

	elem = queue->freenodes->first;
	if (elem != NULL) {
		queue->freenodes->first = elem->next;
		if (queue->freenodes->last == elem) {
			queue->freenodes->last = NULL;
			queue->freenodes->first = NULL;
		}
		queue->freenodes->count--;
	}
	else { /* alllocating more */
		elem = MEM_mallocN(sizeof(DagNodeQueueElem), "DAG queue elem1");
		elem->node = NULL;
		elem->next = NULL;
		queue->freenodes->first = queue->freenodes->last = elem;

		for (i = 1; i < DAGQUEUEALLOC; i++) {
			elem = MEM_mallocN(sizeof(DagNodeQueueElem), "DAG queue elem2");
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


DagNode *pop_queue(DagNodeQueue *queue)
{
	DagNodeQueueElem *elem;
	DagNode *node;

	elem = queue->first;
	if (elem) {
		queue->first = elem->next;
		if (queue->last == elem) {
			queue->last = NULL;
			queue->first = NULL;
		}
		queue->count--;
		if (queue->freenodes->last)
			queue->freenodes->last->next = elem;
		queue->freenodes->last = elem;
		if (queue->freenodes->first == NULL)
			queue->freenodes->first = elem;
		node = elem->node;
		elem->node = NULL;
		elem->next = NULL;
		queue->freenodes->count++;
		return node;
	}
	else {
		fprintf(stderr, "return null\n");
		return NULL;
	}
}

DagNode *get_top_node_queue(DagNodeQueue *queue)
{
	return queue->first->node;
}

DagForest *dag_init(void)
{
	DagForest *forest;
	/* use callocN to init all zero */
	forest = MEM_callocN(sizeof(DagForest), "DAG root");
	forest->ugly_hack_sorry = true;
	return forest;
}

/* isdata = object data... */
/* XXX this needs to be extended to be more flexible (so that not only objects are evaluated via depsgraph)... */
static void dag_add_driver_relation(AnimData *adt, DagForest *dag, DagNode *node, int isdata)
{
	FCurve *fcu;
	DagNode *node1;
	
	for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
		ChannelDriver *driver = fcu->driver;
		DriverVar *dvar;
		int isdata_fcu = (isdata) || (fcu->rna_path && strstr(fcu->rna_path, "modifiers["));
		
		/* loop over variables to get the target relationships */
		for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
			/* only used targets */
			DRIVER_TARGETS_USED_LOOPER(dvar) 
			{
				if (dtar->id) {
					/* FIXME: other data types need to be added here so that they can work! */
					if (GS(dtar->id->name) == ID_OB) {
						Object *ob = (Object *)dtar->id;
						
						/* normal channel-drives-channel */
						node1 = dag_get_node(dag, dtar->id);
						
						/* check if bone... */
						if ((ob->type == OB_ARMATURE) &&
						    ( ((dtar->rna_path) && strstr(dtar->rna_path, "pose.bones[")) ||
						      ((dtar->flag & DTAR_FLAG_STRUCT_REF) && (dtar->pchan_name[0])) ))
						{
							dag_add_relation(dag, node1, node, isdata_fcu ? DAG_RL_DATA_DATA : DAG_RL_DATA_OB, "Driver");
						}
						/* check if ob data */
						else if (dtar->rna_path && strstr(dtar->rna_path, "data."))
							dag_add_relation(dag, node1, node, isdata_fcu ? DAG_RL_DATA_DATA : DAG_RL_DATA_OB, "Driver");
						/* normal */
						else
							dag_add_relation(dag, node1, node, isdata_fcu ? DAG_RL_OB_DATA : DAG_RL_OB_OB, "Driver");
					}
				}
			}
			DRIVER_TARGETS_LOOPER_END
		}
	}
}

/* XXX: forward def for material driver handling... */
static void dag_add_material_driver_relations(DagForest *dag, DagNode *node, Material *ma);

/* recursive handling for shader nodetree drivers */
static void dag_add_shader_nodetree_driver_relations(DagForest *dag, DagNode *node, bNodeTree *ntree)
{
	bNode *n;

	/* nodetree itself */
	if (ntree->adt) {
		dag_add_driver_relation(ntree->adt, dag, node, 1);
	}
	
	/* nodetree's nodes... */
	for (n = ntree->nodes.first; n; n = n->next) {
		if (n->id) {
			if (GS(n->id->name) == ID_MA) {
				dag_add_material_driver_relations(dag, node, (Material *)n->id);
			}
			else if (n->type == NODE_GROUP) {
				dag_add_shader_nodetree_driver_relations(dag, node, (bNodeTree *)n->id);
			}
		}
	}
}

/* recursive handling for material drivers */
static void dag_add_material_driver_relations(DagForest *dag, DagNode *node, Material *ma)
{
	/* Prevent infinite recursion by checking (and tagging the material) as having been visited 
	 * already (see build_dag()). This assumes ma->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (ma->id.flag & LIB_DOIT)
		return;

	ma->id.flag |= LIB_DOIT;
	
	/* material itself */
	if (ma->adt)
		dag_add_driver_relation(ma->adt, dag, node, 1);

	/* textures */
	// TODO...
	//dag_add_texture_driver_relations(DagForest *dag, DagNode *node, ID *id);

	/* material's nodetree */
	if (ma->nodetree)
		dag_add_shader_nodetree_driver_relations(dag, node, ma->nodetree);

	ma->id.flag &= ~LIB_DOIT;
}

/* recursive handling for lamp drivers */
static void dag_add_lamp_driver_relations(DagForest *dag, DagNode *node, Lamp *la)
{
	/* Prevent infinite recursion by checking (and tagging the lamp) as having been visited 
	 * already (see build_dag()). This assumes la->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017]
	 */
	if (la->id.flag & LIB_DOIT)
		return;

	la->id.flag |= LIB_DOIT;
	
	/* lamp itself */
	if (la->adt)
		dag_add_driver_relation(la->adt, dag, node, 1);

	/* textures */
	// TODO...
	//dag_add_texture_driver_relations(DagForest *dag, DagNode *node, ID *id);

	/* lamp's nodetree */
	if (la->nodetree)
		dag_add_shader_nodetree_driver_relations(dag, node, la->nodetree);

	la->id.flag &= ~LIB_DOIT;
}

static void check_and_create_collision_relation(DagForest *dag, Object *ob, DagNode *node, Object *ob1, int skip_forcefield, bool no_collision)
{
	DagNode *node2;
	if (ob1->pd && (ob1->pd->deflect || ob1->pd->forcefield) && (ob1 != ob)) {
		if ((skip_forcefield && ob1->pd->forcefield == skip_forcefield) || (no_collision && ob1->pd->forcefield == 0))
			return;
		node2 = dag_get_node(dag, ob1);
		dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Field Collision");
	}
}

static void dag_add_collision_field_relation(DagForest *dag, Scene *scene, Object *ob, DagNode *node, int skip_forcefield, bool no_collision)
{
	Base *base;
	ParticleSystem *particle_system;

	for (particle_system = ob->particlesystem.first;
	     particle_system;
	     particle_system = particle_system->next)
	{
		EffectorWeights *effector_weights = particle_system->part->effector_weights;
		if (effector_weights->group) {
			GroupObject *group_object;

			for (group_object = effector_weights->group->gobject.first;
			     group_object;
			     group_object = group_object->next)
			{
				if ((group_object->ob->lay & ob->lay)) {
					check_and_create_collision_relation(dag, ob, node, group_object->ob, skip_forcefield, no_collision);
				}
			}
		}
	}

	/* would be nice to have a list of colliders here
	 * so for now walk all objects in scene check 'same layer rule' */
	for (base = scene->base.first; base; base = base->next) {
		if ((base->lay & ob->lay)) {
			Object *ob1 = base->object;
			check_and_create_collision_relation(dag, ob, node, ob1, skip_forcefield, no_collision);
		}
	}
}

static void build_dag_object(DagForest *dag, DagNode *scenenode, Scene *scene, Object *ob, int mask)
{
	bConstraint *con;
	DagNode *node;
	DagNode *node2;
	DagNode *node3;
	Key *key;
	ParticleSystem *psys;
	int addtoroot = 1;
	
	node = dag_get_node(dag, ob);
	
	if ((ob->data) && (mask & DAG_RL_DATA)) {
		node2 = dag_get_node(dag, ob->data);
		dag_add_relation(dag, node, node2, DAG_RL_DATA, "Object-Data Relation");
		node2->first_ancestor = ob;
		node2->ancestor_count += 1;
	}

	/* also build a custom data mask for dependencies that need certain layers */
	node->customdata_mask = 0;
	
	if (ob->type == OB_ARMATURE) {
		if (ob->pose) {
			bPoseChannel *pchan;
			
			for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				for (con = pchan->constraints.first; con; con = con->next) {
					bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct = targets.first; ct; ct = ct->next) {
							if (ct->tar && ct->tar != ob) {
								// fprintf(stderr, "armature %s target :%s\n", ob->id.name, target->id.name);
								node3 = dag_get_node(dag, ct->tar);
								
								if (ct->subtarget[0]) {
									dag_add_relation(dag, node3, node, DAG_RL_OB_DATA | DAG_RL_DATA_DATA, cti->name);
									if (ct->tar->type == OB_MESH)
										node3->customdata_mask |= CD_MASK_MDEFORMVERT;
								}
								else if (ELEM(con->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO, CONSTRAINT_TYPE_SPLINEIK))
									dag_add_relation(dag, node3, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, cti->name);
								else
									dag_add_relation(dag, node3, node, DAG_RL_OB_DATA, cti->name);
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
	if (ob->nlastrips.first) {
		bActionStrip *strip;
		bActionChannel *chan;
		for (strip = ob->nlastrips.first; strip; strip = strip->next) {
			if (strip->modifiers.first) {
				bActionModifier *amod;
				for (amod = strip->modifiers.first; amod; amod = amod->next) {
					if (amod->ob) {
						node2 = dag_get_node(dag, amod->ob);
						dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "NLA Strip Modifier");
					}
				}
			}
		}
	}
#endif // XXX old animation system
	if (ob->adt)
		dag_add_driver_relation(ob->adt, dag, node, (ob->type == OB_ARMATURE));  // XXX isdata arg here doesn't give an accurate picture of situation
		
	key = BKE_key_from_object(ob);
	if (key && key->adt)
		dag_add_driver_relation(key->adt, dag, node, 1);

	if (ob->modifiers.first) {
		ModifierData *md;
		
		for (md = ob->modifiers.first; md; md = md->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);
			
			if (mti->updateDepgraph) mti->updateDepgraph(md, dag, scene, ob, node);
		}
	}
	if (ob->parent) {
		node2 = dag_get_node(dag, ob->parent);
		
		switch (ob->partype) {
			case PARSKEL:
				dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_OB, "Parent");
				break;
			case PARVERT1: case PARVERT3:
				dag_add_relation(dag, node2, node, DAG_RL_DATA_OB | DAG_RL_OB_OB, "Vertex Parent");
				node2->customdata_mask |= CD_MASK_ORIGINDEX;
				break;
			case PARBONE:
				dag_add_relation(dag, node2, node, DAG_RL_DATA_OB | DAG_RL_OB_OB, "Bone Parent");
				break;
			default:
				if (ob->parent->type == OB_LATTICE)
					dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_OB, "Lattice Parent");
				else if (ob->parent->type == OB_CURVE) {
					Curve *cu = ob->parent->data;
					if (cu->flag & CU_PATH) 
						dag_add_relation(dag, node2, node, DAG_RL_DATA_OB | DAG_RL_OB_OB, "Curve Parent");
					else
						dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Curve Parent");
				}
				else
					dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Parent");
				break;
		}
		/* exception case: parent is duplivert */
		if (ob->type == OB_MBALL && (ob->parent->transflag & OB_DUPLIVERTS)) {
			dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_OB, "Duplivert");
		}
		
		addtoroot = 0;
	}
	if (ob->proxy) {
		node2 = dag_get_node(dag, ob->proxy);
		dag_add_relation(dag, node, node2, DAG_RL_DATA_DATA | DAG_RL_OB_OB, "Proxy");
		/* inverted relation, so addtoroot shouldn't be set to zero */
	}
	
	if (ob->transflag & OB_DUPLI) {
		if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group) {
			GroupObject *go;
			for (go = ob->dup_group->gobject.first; go; go = go->next) {
				if (go->ob) {
					node2 = dag_get_node(dag, go->ob);
					/* node2 changes node1, this keeps animations updated in groups?? not logical? */
					dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Dupligroup");
				}
			}
		}
	}

	/* softbody collision  */
	if ((ob->type == OB_MESH) || (ob->type == OB_CURVE) || (ob->type == OB_LATTICE)) {
		if (ob->particlesystem.first ||
		    modifiers_isModifierEnabled(ob, eModifierType_Softbody) ||
		    modifiers_isModifierEnabled(ob, eModifierType_Cloth) ||
		    modifiers_isModifierEnabled(ob, eModifierType_DynamicPaint))
		{
			dag_add_collision_field_relation(dag, scene, ob, node, 0, false);  /* TODO: use effectorweight->group */
		}
		else if (modifiers_isModifierEnabled(ob, eModifierType_Smoke)) {
			dag_add_collision_field_relation(dag, scene, ob, node, PFIELD_SMOKEFLOW, false);
		}
		else if (ob->rigidbody_object) {
			dag_add_collision_field_relation(dag, scene, ob, node, 0, true);
		}
	}
	
	/* object data drivers */
	if (ob->data) {
		AnimData *adt = BKE_animdata_from_id((ID *)ob->data);
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
				dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Camera DoF");
			}
			break;
		}
		case OB_MBALL: 
		{
			Object *mom = BKE_mball_basis_find(scene, ob);
			
			if (mom != ob) {
				node2 = dag_get_node(dag, mom);
				dag_add_relation(dag, node, node2, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Metaball");  /* mom depends on children! */
			}
			break;
		}
		case OB_CURVE:
		case OB_FONT:
		{
			Curve *cu = ob->data;
			
			if (cu->bevobj) {
				node2 = dag_get_node(dag, cu->bevobj);
				dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Curve Bevel");
			}
			if (cu->taperobj) {
				node2 = dag_get_node(dag, cu->taperobj);
				dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Curve Taper");
			}
			if (ob->type == OB_FONT) {
				/* Really rather dirty hack. needs to support font family to work
				 * reliably on render export.
				 *
				 * This totally mimics behavior of regular verts duplication with
				 * parenting. The only tricky thing here is to get list of objects
				 * used for the custom "font".
				 *
				 * This shouldn't harm so much because this code only runs on DAG
				 * rebuild and this feature is not that commonly used.
				 *
				 *                                                 - sergey -
				 */
				if (cu->family[0] != '\n') {
					ListBase *duplilist;
					DupliObject *dob;
					duplilist = object_duplilist(G.main->eval_ctx, scene, ob);
					for (dob= duplilist->first; dob; dob = dob->next) {
						node2 = dag_get_node(dag, dob->ob);
						dag_add_relation(dag, node, node2, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Object Font");
					}
					free_object_duplilist(duplilist);
				}

				if (cu->textoncurve) {
					node2 = dag_get_node(dag, cu->textoncurve);
					/* Text on curve requires path to be evaluated for the target curve. */
					node2->eval_flags |= DAG_EVAL_NEED_CURVE_PATH;
					dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Texture On Curve");
				}
			}
			break;
		}
	}
	
	/* material drivers */
	if (ob->totcol) {
		int a;
		
		for (a = 1; a <= ob->totcol; a++) {
			Material *ma = give_current_material(ob, a);
			
			if (ma) {
				/* recursively figure out if there are drivers, and hook these up to this object */
				dag_add_material_driver_relations(dag, node, ma);
			}
		}
	}
	else if (ob->type == OB_LAMP) {
		dag_add_lamp_driver_relations(dag, node, ob->data);
	}
	
	/* particles */
	psys = ob->particlesystem.first;
	if (psys) {
		GroupObject *go;

		for (; psys; psys = psys->next) {
			BoidRule *rule = NULL;
			BoidState *state = NULL;
			ParticleSettings *part = psys->part;
			ListBase *effectors = NULL;
			EffectorCache *eff;

			dag_add_relation(dag, node, node, DAG_RL_OB_DATA, "Particle-Object Relation");

			if (!psys_check_enabled(ob, psys))
				continue;

			if (ELEM(part->phystype, PART_PHYS_KEYED, PART_PHYS_BOIDS)) {
				ParticleTarget *pt = psys->targets.first;

				for (; pt; pt = pt->next) {
					if (pt->ob && BLI_findlink(&pt->ob->particlesystem, pt->psys - 1)) {
						node2 = dag_get_node(dag, pt->ob);
						dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Particle Targets");
					}
				}
			}

			if (part->ren_as == PART_DRAW_OB && part->dup_ob) {
				node2 = dag_get_node(dag, part->dup_ob);
				/* note that this relation actually runs in the wrong direction, the problem
				 * is that dupli system all have this (due to parenting), and the render
				 * engine instancing assumes particular ordering of objects in list */
				dag_add_relation(dag, node, node2, DAG_RL_OB_OB, "Particle Object Visualization");
				if (part->dup_ob->type == OB_MBALL)
					dag_add_relation(dag, node, node2, DAG_RL_DATA_DATA, "Particle Object Visualization");
			}

			if (part->ren_as == PART_DRAW_GR && part->dup_group) {
				for (go = part->dup_group->gobject.first; go; go = go->next) {
					node2 = dag_get_node(dag, go->ob);
					dag_add_relation(dag, node2, node, DAG_RL_OB_OB, "Particle Group Visualization");
				}
			}

			effectors = pdInitEffectors(scene, ob, psys, part->effector_weights, false);

			if (effectors) {
				for (eff = effectors->first; eff; eff = eff->next) {
					if (eff->psys) {
						node2 = dag_get_node(dag, eff->ob);
						dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Particle Field");
					}
				}
			}

			pdEndEffectors(&effectors);

			if (part->boids) {
				for (state = part->boids->states.first; state; state = state->next) {
					for (rule = state->rules.first; rule; rule = rule->next) {
						Object *ruleob = NULL;
						if (rule->type == eBoidRuleType_Avoid)
							ruleob = ((BoidRuleGoalAvoid *)rule)->ob;
						else if (rule->type == eBoidRuleType_FollowLeader)
							ruleob = ((BoidRuleFollowLeader *)rule)->ob;

						if (ruleob) {
							node2 = dag_get_node(dag, ruleob);
							dag_add_relation(dag, node2, node, DAG_RL_OB_DATA, "Boid Rule");
						}
					}
				}
			}
		}
	}
	
	/* object constraints */
	for (con = ob->constraints.first; con; con = con->next) {
		bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
		ListBase targets = {NULL, NULL};
		bConstraintTarget *ct;
		
		if (!cti)
			continue;

		/* special case for camera tracking -- it doesn't use targets to define relations */
		if (ELEM(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER, CONSTRAINT_TYPE_OBJECTSOLVER)) {
			int depends_on_camera = 0;

			if (cti->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
				bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;

				if ((data->clip || data->flag & FOLLOWTRACK_ACTIVECLIP) && data->track[0])
					depends_on_camera = 1;

				if (data->depth_ob) {
					node2 = dag_get_node(dag, data->depth_ob);
					dag_add_relation(dag, node2, node, DAG_RL_DATA_OB | DAG_RL_OB_OB, cti->name);
				}
			}
			else if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER)
				depends_on_camera = 1;

			if (depends_on_camera && scene->camera) {
				node2 = dag_get_node(dag, scene->camera);
				dag_add_relation(dag, node2, node, DAG_RL_DATA_OB | DAG_RL_OB_OB, cti->name);
			}

			dag_add_relation(dag, scenenode, node, DAG_RL_SCENE, "Scene Relation");
			addtoroot = 0;
		}
		else if (cti->get_constraint_targets) {
			cti->get_constraint_targets(con, &targets);
			
			for (ct = targets.first; ct; ct = ct->next) {
				Object *obt;
				
				if (ct->tar)
					obt = ct->tar;
				else
					continue;
				
				node2 = dag_get_node(dag, obt);
				if (ELEM(con->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO))
					dag_add_relation(dag, node2, node, DAG_RL_DATA_OB | DAG_RL_OB_OB, cti->name);
				else {
					if (ELEM(obt->type, OB_ARMATURE, OB_MESH, OB_LATTICE) && (ct->subtarget[0])) {
						dag_add_relation(dag, node2, node, DAG_RL_DATA_OB | DAG_RL_OB_OB, cti->name);
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

	if (addtoroot == 1)
		dag_add_relation(dag, scenenode, node, DAG_RL_SCENE, "Scene Relation");
}

static void build_dag_group(DagForest *dag, DagNode *scenenode, Scene *scene, Group *group, short mask)
{
	GroupObject *go;

	if (group->id.flag & LIB_DOIT)
		return;
	
	group->id.flag |= LIB_DOIT;

	for (go = group->gobject.first; go; go = go->next) {
		build_dag_object(dag, scenenode, scene, go->ob, mask);
		if (go->ob->dup_group)
			build_dag_group(dag, scenenode, scene, go->ob->dup_group, mask);
	}
}

DagForest *build_dag(Main *bmain, Scene *sce, short mask)
{
	Base *base;
	Object *ob;
	DagNode *node;
	DagNode *scenenode;
	DagForest *dag;
	DagAdjList *itA;

	dag = sce->theDag;
	if (dag)
		free_forest(dag);
	else {
		dag = dag_init();
		sce->theDag = dag;
	}
	
	/* clear "LIB_DOIT" flag from all materials, to prevent infinite recursion problems later [#32017] */
	BKE_main_id_tag_idcode(bmain, ID_MA, false);
	BKE_main_id_tag_idcode(bmain, ID_LA, false);
	BKE_main_id_tag_idcode(bmain, ID_GR, false);
	
	/* add base node for scene. scene is always the first node in DAG */
	scenenode = dag_add_node(dag, sce);
	
	/* add current scene objects */
	for (base = sce->base.first; base; base = base->next) {
		ob = base->object;
		
		build_dag_object(dag, scenenode, sce, ob, mask);
		if (ob->proxy)
			build_dag_object(dag, scenenode, sce, ob->proxy, mask);
		if (ob->dup_group) 
			build_dag_group(dag, scenenode, sce, ob->dup_group, mask);
	}
	
	BKE_main_id_tag_idcode(bmain, ID_GR, false);
	
	/* Now all relations were built, but we need to solve 1 exceptional case;
	 * When objects have multiple "parents" (for example parent + constraint working on same object)
	 * the relation type has to be synced. One of the parents can change, and should give same event to child */
	
	/* nodes were callocced, so we can use node->color for temporal storage */
	for (node = sce->theDag->DagNode.first; node; node = node->next) {
		if (node->type == ID_OB) {
			for (itA = node->child; itA; itA = itA->next) {
				if (itA->node->type == ID_OB) {
					itA->node->color |= itA->type;
				}
			}

			/* also flush custom data mask */
			((Object *)node->ob)->customdata_mask = node->customdata_mask;
		}
	}
	/* now set relations equal, so that when only one parent changes, the correct recalcs are found */
	for (node = sce->theDag->DagNode.first; node; node = node->next) {
		if (node->type == ID_OB) {
			for (itA = node->child; itA; itA = itA->next) {
				if (itA->node->type == ID_OB) {
					itA->type |= itA->node->color;
				}
			}
		}
	}
	
	/* cycle detection and solving */
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
	Dag->nodeHash = NULL;
	Dag->DagNode.first = NULL;
	Dag->DagNode.last = NULL;
	Dag->numNodes = 0;

}

DagNode *dag_find_node(DagForest *forest, void *fob)
{
	if (forest->nodeHash)
		return BLI_ghash_lookup(forest->nodeHash, fob);

	return NULL;
}

static int dag_print_dependencies = 0;  /* debugging */

/* no checking of existence, use dag_find_node first or dag_get_node */
DagNode *dag_add_node(DagForest *forest, void *fob)
{
	DagNode *node;
		
	node = MEM_callocN(sizeof(DagNode), "DAG node");
	if (node) {
		node->ob = fob;
		node->color = DAG_WHITE;

		if (forest->ugly_hack_sorry) node->type = GS(((ID *) fob)->name);  /* sorry, done for pose sorting */
		if (forest->numNodes) {
			((DagNode *) forest->DagNode.last)->next = node;
			forest->DagNode.last = node;
			forest->numNodes++;
		}
		else {
			forest->DagNode.last = node;
			forest->DagNode.first = node;
			forest->numNodes = 1;
		}

		if (!forest->nodeHash)
			forest->nodeHash = BLI_ghash_ptr_new("dag_add_node gh");
		BLI_ghash_insert(forest->nodeHash, fob, node);
	}

	return node;
}

DagNode *dag_get_node(DagForest *forest, void *fob)
{
	DagNode *node;
	
	node = dag_find_node(forest, fob);
	if (!node) 
		node = dag_add_node(forest, fob);
	return node;
}



DagNode *dag_get_sub_node(DagForest *forest, void *fob)
{
	DagNode *node;
	DagAdjList *mainchild, *prev = NULL;
	
	mainchild = ((DagNode *) forest->DagNode.first)->child;
	/* remove from first node (scene) adj list if present */
	while (mainchild) {
		if (mainchild->node == fob) {
			if (prev) {
				prev->next = mainchild->next;
				MEM_freeN(mainchild);
				break;
			}
			else {
				((DagNode *) forest->DagNode.first)->child = mainchild->next;
				MEM_freeN(mainchild);
				break;
			}
		}
		prev = mainchild;
		mainchild = mainchild->next;
	}
	node = dag_find_node(forest, fob);
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
	itA = MEM_mallocN(sizeof(DagAdjList), "DAG adj list");
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
	itA = MEM_mallocN(sizeof(DagAdjList), "DAG adj list");
	itA->node = fob2;
	itA->type = rel;
	itA->count = 1;
	itA->next = fob1->child;
	itA->name = name;
	fob1->child = itA;
}

static const char *dag_node_name(DagForest *dag, DagNode *node)
{
	if (node->ob == NULL)
		return "null";
	else if (dag->ugly_hack_sorry)
		return ((ID *)(node->ob))->name + 2;
	else
		return ((bPoseChannel *)(node->ob))->name;
}

static void dag_node_print_dependencies(DagForest *dag, DagNode *node)
{
	DagAdjList *itA;

	printf("%s depends on:\n", dag_node_name(dag, node));

	for (itA = node->parent; itA; itA = itA->next)
		printf("  %s through %s\n", dag_node_name(dag, itA->node), itA->name);
	printf("\n");
}

static int dag_node_print_dependency_recurs(DagForest *dag, DagNode *node, DagNode *endnode)
{
	DagAdjList *itA;

	if (node->color == DAG_BLACK)
		return 0;

	node->color = DAG_BLACK;

	if (node == endnode)
		return 1;

	for (itA = node->parent; itA; itA = itA->next) {
		if (dag_node_print_dependency_recurs(dag, itA->node, endnode)) {
			printf("  %s depends on %s through %s.\n", dag_node_name(dag, node), dag_node_name(dag, itA->node), itA->name);
			return 1;
		}
	}

	return 0;
}

static void dag_node_print_dependency_cycle(DagForest *dag, DagNode *startnode, DagNode *endnode, const char *name)
{
	DagNode *node;

	for (node = dag->DagNode.first; node; node = node->next)
		node->color = DAG_WHITE;

	printf("  %s depends on %s through %s.\n", dag_node_name(dag, endnode), dag_node_name(dag, startnode), name);
	dag_node_print_dependency_recurs(dag, startnode, endnode);
	printf("\n");
}

static int dag_node_recurs_level(DagNode *node, int level)
{
	DagAdjList *itA;
	int newlevel;

	node->color = DAG_BLACK; /* done */
	newlevel = ++level;
	
	for (itA = node->parent; itA; itA = itA->next) {
		if (itA->node->color == DAG_WHITE) {
			itA->node->ancestor_count = dag_node_recurs_level(itA->node, level);
			newlevel = MAX2(newlevel, level + itA->node->ancestor_count);
		}
		else
			newlevel = MAX2(newlevel, level + itA->node->ancestor_count);
	}
	
	return newlevel;
}

static void dag_check_cycle(DagForest *dag)
{
	DagNode *node;
	DagAdjList *itA;

	dag->is_acyclic = true;

	/* debugging print */
	if (dag_print_dependencies)
		for (node = dag->DagNode.first; node; node = node->next)
			dag_node_print_dependencies(dag, node);

	/* tag nodes unchecked */
	for (node = dag->DagNode.first; node; node = node->next)
		node->color = DAG_WHITE;
	
	for (node = dag->DagNode.first; node; node = node->next) {
		if (node->color == DAG_WHITE) {
			node->ancestor_count = dag_node_recurs_level(node, 0);
		}
	}
	
	/* check relations, and print errors */
	for (node = dag->DagNode.first; node; node = node->next) {
		for (itA = node->parent; itA; itA = itA->next) {
			if (itA->node->ancestor_count > node->ancestor_count) {
				if (node->ob && itA->node->ob) {
					dag->is_acyclic = false;
					printf("Dependency cycle detected:\n");
					dag_node_print_dependency_cycle(dag, itA->node, node, itA->name);
				}
			}
		}
	}

	/* parent relations are only needed for cycle checking, so free now */
	for (node = dag->DagNode.first; node; node = node->next) {
		while (node->parent) {
			itA = node->parent->next;
			MEM_freeN(node->parent);
			node->parent = itA;
		}
	}
}

/* debug test functions */

void graph_print_queue(DagNodeQueue *nqueue)
{	
	DagNodeQueueElem *queueElem;
	
	queueElem = nqueue->first;
	while (queueElem) {
		fprintf(stderr, "** %s %i %i-%i ", ((ID *) queueElem->node->ob)->name, queueElem->node->color, queueElem->node->DFS_dvtm, queueElem->node->DFS_fntm);
		queueElem = queueElem->next;
	}
	fprintf(stderr, "\n");
}

void graph_print_queue_dist(DagNodeQueue *nqueue)
{	
	DagNodeQueueElem *queueElem;
	int count;
	
	queueElem = nqueue->first;
	count = 0;
	while (queueElem) {
		fprintf(stderr, "** %25s %2.2i-%2.2i ", ((ID *) queueElem->node->ob)->name, queueElem->node->DFS_dvtm, queueElem->node->DFS_fntm);
		while (count < queueElem->node->DFS_dvtm - 1) { fputc(' ', stderr); count++; }
		fputc('|', stderr);
		while (count < queueElem->node->DFS_fntm - 2) { fputc('-', stderr); count++; }
		fputc('|', stderr);
		fputc('\n', stderr);
		count = 0;
		queueElem = queueElem->next;
	}
	fprintf(stderr, "\n");
}

void graph_print_adj_list(DagForest *dag)
{
	DagNode *node;
	DagAdjList *itA;
	
	node = dag->DagNode.first;
	while (node) {
		fprintf(stderr, "node : %s col: %i", ((ID *) node->ob)->name, node->color);
		itA = node->child;
		while (itA) {
			fprintf(stderr, "-- %s ", ((ID *) itA->node->ob)->name);
			
			itA = itA->next;
		}
		fprintf(stderr, "\n");
		node = node->next;
	}
}

/* ************************ API *********************** */

/* mechanism to allow editors to be informed of depsgraph updates,
 * to do their own updates based on changes... */
static void (*EditorsUpdateIDCb)(Main *bmain, ID *id) = NULL;
static void (*EditorsUpdateSceneCb)(Main *bmain, Scene *scene, int updated) = NULL;

void DAG_editors_update_cb(void (*id_func)(Main *bmain, ID *id), void (*scene_func)(Main *bmain, Scene *scene, int updated))
{
	EditorsUpdateIDCb = id_func;
	EditorsUpdateSceneCb = scene_func;
}

static void dag_editors_id_update(Main *bmain, ID *id)
{
	if (EditorsUpdateIDCb)
		EditorsUpdateIDCb(bmain, id);
}

static void dag_editors_scene_update(Main *bmain, Scene *scene, int updated)
{
	if (EditorsUpdateSceneCb)
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
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		ob->id.flag &= ~LIB_DOIT;
		ob->id.newid = NULL; /* newid abuse for GroupObject */
	}
	for (base = sce->base.first; base; base = base->next)
		base->object->id.flag |= LIB_DOIT;
	
	for (group = bmain->group.first; group; group = group->id.next) {
		for (go = group->gobject.first; go; go = go->next) {
			if ((go->ob->id.flag & LIB_DOIT) == 0)
				break;
		}
		/* this group is entirely in this scene */
		if (go == NULL) {
			ListBase listb = {NULL, NULL};
			
			for (go = group->gobject.first; go; go = go->next)
				go->ob->id.newid = (ID *)go;
			
			/* in order of sorted bases we reinsert group objects */
			for (base = sce->base.first; base; base = base->next) {
				
				if (base->object->id.newid) {
					go = (GroupObject *)base->object->id.newid;
					base->object->id.newid = NULL;
					BLI_remlink(&group->gobject, go);
					BLI_addtail(&listb, go);
				}
			}
			/* copy the newly sorted listbase */
			group->gobject = listb;
		}
	}
}

/* free the depency graph */
static void dag_scene_free(Scene *sce)
{
	if (sce->theDag) {
		free_forest(sce->theDag);
		MEM_freeN(sce->theDag);
		sce->theDag = NULL;
	}
}

/* Chech whether object data needs to be evaluated before it
 * might be used by others.
 *
 * Means that mesh object needs to have proper derivedFinal,
 * curves-typed objects are to have proper curve cache.
 *
 * Other objects or objects which are tagged for data update are
 * not considered to be in need of evaluation.
 */
static bool check_object_needs_evaluation(Object *object)
{
	if (object->recalc & OB_RECALC_ALL) {
		/* Object is tagged for update anyway, no need to re-tag it. */
		return false;
	}

	if (object->type == OB_MESH) {
		return object->derivedFinal == NULL;
	}
	else if (ELEM(object->type, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_LATTICE)) {
		return object->curve_cache == NULL;
	}

	return false;
}

/* Check whether object data is tagged for update. */
static bool check_object_tagged_for_update(Object *object)
{
	if (object->recalc & OB_RECALC_ALL) {
		return true;
	}

	if (ELEM(object->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_LATTICE)) {
		ID *data_id = object->data;
		return (data_id->flag & (LIB_ID_RECALC_DATA | LIB_ID_RECALC)) != 0;
	}

	return false;
}

/* Flush changes from tagged objects in the scene to their
 * dependencies which are not evaluated yet.
 *
 * This is needed to ensure all the dependencies are met
 * before objects gets handled by object_handle_update(),
 *
 * This is needed when visible layers are changed or changing
 * scene graph layout which involved usage of objects which
 * aren't in the scene or weren't visible yet.
 */
static void dag_invisible_dependencies_flush(Scene *scene)
{
	DagNode *root_node = scene->theDag->DagNode.first, *node;
	DagNodeQueue *queue;

	for (node = root_node; node != NULL; node = node->next) {
		node->color = DAG_WHITE;
	}

	queue = queue_create(DAGQUEUEALLOC);

	for (node = root_node; node != NULL; node = node->next) {
		if (node->color == DAG_WHITE) {
			push_stack(queue, node);
			node->color = DAG_GRAY;

			while (queue->count) {
				DagNode *current_node = get_top_node_queue(queue);
				DagAdjList *itA;
				bool skip = false;

				for (itA = current_node->child; itA; itA = itA->next) {
					if (itA->node->color == DAG_WHITE) {
						itA->node->color = DAG_GRAY;
						push_stack(queue, itA->node);
						skip = true;
						break;
					}
				}

				if (!skip) {
					current_node = pop_queue(queue);

					if (current_node->type == ID_OB) {
						Object *current_object = current_node->ob;
						if (check_object_needs_evaluation(current_object)) {
							for (itA = current_node->child; itA; itA = itA->next) {
								if (itA->node->type == ID_OB) {
									Object *object = itA->node->ob;
									if (check_object_tagged_for_update(object)) {
										current_object->recalc |= OB_RECALC_OB | OB_RECALC_DATA;
									}
								}
							}
						}
					}
					node->color = DAG_BLACK;
				}
			}
		}
	}

	queue_delete(queue);
}

static void dag_invisible_dependencies_check_flush(Main *bmain, Scene *scene)
{
	if (DAG_id_type_tagged(bmain, ID_OB) ||
	    DAG_id_type_tagged(bmain, ID_ME) ||  /* Mesh */
	    DAG_id_type_tagged(bmain, ID_CU) ||  /* Curve */
	    DAG_id_type_tagged(bmain, ID_MB) ||  /* MetaBall */
	    DAG_id_type_tagged(bmain, ID_LT))    /* Lattice */
	{
		dag_invisible_dependencies_flush(scene);
	}
}

/* sort the base list on dependency order */
static void dag_scene_build(Main *bmain, Scene *sce)
{
	DagNode *node, *rootnode;
	DagNodeQueue *nqueue;
	DagAdjList *itA;
	int time;
	int skip = 0;
	ListBase tempbase;
	Base *base;

	BLI_listbase_clear(&tempbase);
	
	build_dag(bmain, sce, DAG_RL_ALL_BUT_DATA);
	
	dag_check_cycle(sce->theDag);

	nqueue = queue_create(DAGQUEUEALLOC);
	
	for (node = sce->theDag->DagNode.first; node; node = node->next) {
		node->color = DAG_WHITE;
	}
	
	time = 1;
	
	rootnode = sce->theDag->DagNode.first;
	rootnode->color = DAG_GRAY;
	time++;
	push_stack(nqueue, rootnode);
	
	while (nqueue->count) {
		
		skip = 0;
		node = get_top_node_queue(nqueue);
		
		itA = node->child;
		while (itA != NULL) {
			if (itA->node->color == DAG_WHITE) {
				itA->node->DFS_dvtm = time;
				itA->node->color = DAG_GRAY;
				
				time++;
				push_stack(nqueue, itA->node);
				skip = 1;
				break;
			}
			itA = itA->next;
		}
		
		if (!skip) {
			if (node) {
				node = pop_queue(nqueue);
				if (node->ob == sce)  /* we are done */
					break;
				node->color = DAG_BLACK;
				
				time++;
				base = sce->base.first;
				while (base && base->object != node->ob)
					base = base->next;
				if (base) {
					BLI_remlink(&sce->base, base);
					BLI_addhead(&tempbase, base);
				}
			}
		}
	}
	
	/* temporal correction for circular dependencies */
	base = sce->base.first;
	while (base) {
		BLI_remlink(&sce->base, base);
		BLI_addhead(&tempbase, base);
		//if (G.debug & G_DEBUG)
		printf("cyclic %s\n", base->object->id.name);
		base = sce->base.first;
	}
	
	sce->base = tempbase;
	queue_delete(nqueue);
	
	/* all groups with objects in this scene gets resorted too */
	scene_sort_groups(bmain, sce);
	
	if (G.debug & G_DEBUG) {
		printf("\nordered\n");
		for (base = sce->base.first; base; base = base->next) {
			printf(" %s\n", base->object->id.name);
		}
	}

	/* temporal...? */
	sce->recalc |= SCE_PRV_CHANGED; /* test for 3d preview */

	/* Make sure that new dependencies which came from invisble layers
	 * are tagged for update (if they're needed for objects which were
	 * tagged for update).
	 */
	dag_invisible_dependencies_check_flush(bmain, sce);
}

/* clear all dependency graphs */
void DAG_relations_tag_update(Main *bmain)
{
	Scene *sce;

	for (sce = bmain->scene.first; sce; sce = sce->id.next)
		dag_scene_free(sce);
}

/* rebuild dependency graph only for a given scene */
void DAG_scene_relations_rebuild(Main *bmain, Scene *sce)
{
	dag_scene_free(sce);
	DAG_scene_relations_update(bmain, sce);
}

/* create dependency graph if it was cleared or didn't exist yet */
void DAG_scene_relations_update(Main *bmain, Scene *sce)
{
	if (!sce->theDag)
		dag_scene_build(bmain, sce);
}

void DAG_scene_free(Scene *sce)
{
	if (sce->theDag) {
		free_forest(sce->theDag);
		MEM_freeN(sce->theDag);
		sce->theDag = NULL;
	}
}

static void lib_id_recalc_tag(Main *bmain, ID *id)
{
	id->flag |= LIB_ID_RECALC;
	DAG_id_type_tag(bmain, GS(id->name));
}

static void lib_id_recalc_data_tag(Main *bmain, ID *id)
{
	id->flag |= LIB_ID_RECALC_DATA;
	DAG_id_type_tag(bmain, GS(id->name));
}

/* node was checked to have lasttime != curtime and is if type ID_OB */
static void flush_update_node(Main *bmain, DagNode *node, unsigned int layer, int curtime)
{
	DagAdjList *itA;
	Object *ob, *obc;
	int oldflag;
	bool changed = false;
	unsigned int all_layer;
	
	node->lasttime = curtime;
	
	ob = node->ob;
	if (ob && (ob->recalc & OB_RECALC_ALL)) {
		all_layer = node->scelay;

		/* got an object node that changes, now check relations */
		for (itA = node->child; itA; itA = itA->next) {
			all_layer |= itA->lay;
			/* the relationship is visible */
			if ((itA->lay & layer)) { // XXX || (itA->node->ob == obedit)
				if (itA->node->type == ID_OB) {
					obc = itA->node->ob;
					oldflag = obc->recalc;
					
					/* got a ob->obc relation, now check if flag needs flush */
					if (ob->recalc & OB_RECALC_OB) {
						if (itA->type & DAG_RL_OB_OB) {
							//printf("ob %s changes ob %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_OB;
							lib_id_recalc_tag(bmain, &obc->id);
						}
						if (itA->type & DAG_RL_OB_DATA) {
							//printf("ob %s changes obdata %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_DATA;
							lib_id_recalc_data_tag(bmain, &obc->id);
						}
					}
					if (ob->recalc & OB_RECALC_DATA) {
						if (itA->type & DAG_RL_DATA_OB) {
							//printf("obdata %s changes ob %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_OB;
							lib_id_recalc_tag(bmain, &obc->id);
						}
						if (itA->type & DAG_RL_DATA_DATA) {
							//printf("obdata %s changes obdata %s\n", ob->id.name, obc->id.name);
							obc->recalc |= OB_RECALC_DATA;
							lib_id_recalc_data_tag(bmain, &obc->id);
						}
					}
					if (oldflag != obc->recalc) changed = 1;
				}
			}
		}
		/* even nicer, we can clear recalc flags...  */
		if ((all_layer & layer) == 0) { // XXX && (ob != obedit)) {
			/* but existing displaylists or derivedmesh should be freed */
			if (ob->recalc & OB_RECALC_DATA)
				BKE_object_free_derived_caches(ob);
			
			ob->recalc &= ~OB_RECALC_ALL;
		}
	}
	
	/* check case where child changes and parent forcing obdata to change */
	/* should be done regardless if this ob has recalc set */
	/* could merge this in with loop above...? (ton) */
	for (itA = node->child; itA; itA = itA->next) {
		/* the relationship is visible */
		if ((itA->lay & layer)) {       // XXX  || (itA->node->ob == obedit)
			if (itA->node->type == ID_OB) {
				obc = itA->node->ob;
				/* child moves */
				if ((obc->recalc & OB_RECALC_ALL) == OB_RECALC_OB) {
					/* parent has deforming info */
					if (itA->type & (DAG_RL_OB_DATA | DAG_RL_DATA_DATA)) {
						// printf("parent %s changes ob %s\n", ob->id.name, obc->id.name);
						obc->recalc |= OB_RECALC_DATA;
						lib_id_recalc_data_tag(bmain, &obc->id);
					}
				}
			}
		}
	}
	
	/* we only go deeper if node not checked or something changed  */
	for (itA = node->child; itA; itA = itA->next) {
		if (changed || itA->node->lasttime != curtime)
			flush_update_node(bmain, itA->node, layer, curtime);
	}
	
}

/* node was checked to have lasttime != curtime, and is of type ID_OB */
static unsigned int flush_layer_node(Scene *sce, DagNode *node, int curtime)
{
	DagAdjList *itA;
	
	node->lasttime = curtime;
	node->lay = node->scelay;
	
	for (itA = node->child; itA; itA = itA->next) {
		if (itA->node->type == ID_OB) {
			if (itA->node->lasttime != curtime) {
				itA->lay = flush_layer_node(sce, itA->node, curtime);  /* lay is only set once for each relation */
			}
			else {
				itA->lay = itA->node->lay;
			}
			
			node->lay |= itA->lay;
		}
	}

	return node->lay;
}

/* node was checked to have lasttime != curtime, and is of type ID_OB */
static void flush_pointcache_reset(Main *bmain, Scene *scene, DagNode *node, int curtime, int reset)
{
	DagAdjList *itA;
	Object *ob;
	
	node->lasttime = curtime;
	
	for (itA = node->child; itA; itA = itA->next) {
		if (itA->node->type == ID_OB) {
			if (itA->node->lasttime != curtime) {
				ob = (Object *)(itA->node->ob);

				if (reset || (ob->recalc & OB_RECALC_ALL)) {
					if (BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_DEPSGRAPH)) {
						ob->recalc |= OB_RECALC_DATA;
						lib_id_recalc_data_tag(bmain, &ob->id);
					}

					flush_pointcache_reset(bmain, scene, itA->node, curtime, 1);
				}
				else
					flush_pointcache_reset(bmain, scene, itA->node, curtime, 0);
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

	firstnode = sce->theDag->DagNode.first;  /* always scene node */

	for (itA = firstnode->child; itA; itA = itA->next)
		itA->lay = 0;

	sce->theDag->time++;  /* so we know which nodes were accessed */
	lasttime = sce->theDag->time;

	/* update layer flags in nodes */
	for (base = sce->base.first; base; base = base->next) {
		node = dag_get_node(sce->theDag, base->object);
		node->scelay = base->object->lay;
	}

	/* ensure cameras are set as if they are on a visible layer, because
	 * they ared still used for rendering or setting the camera view
	 *
	 * XXX, this wont work for local view / unlocked camera's */
	if (sce->camera) {
		node = dag_get_node(sce->theDag, sce->camera);
		node->scelay |= lay;
	}

#ifdef DURIAN_CAMERA_SWITCH
	{
		TimeMarker *m;

		for (m = sce->markers.first; m; m = m->next) {
			if (m->camera) {
				node = dag_get_node(sce->theDag, m->camera);
				node->scelay |= lay;
			}
		}
	}
#endif

	/* flush layer nodes to dependencies */
	for (itA = firstnode->child; itA; itA = itA->next)
		if (itA->node->lasttime != lasttime && itA->node->type == ID_OB)
			flush_layer_node(sce, itA->node, lasttime);
}

static void dag_tag_renderlayers(Scene *sce, unsigned int lay)
{
	if (sce->nodetree) {
		bNode *node;
		Base *base;
		unsigned int lay_changed = 0;
		
		for (base = sce->base.first; base; base = base->next)
			if (base->lay & lay)
				if (base->object->recalc)
					lay_changed |= base->lay;
			
		for (node = sce->nodetree->nodes.first; node; node = node->next) {
			if (node->id == (ID *)sce) {
				SceneRenderLayer *srl = BLI_findlink(&sce->r.layers, node->custom1);
				if (srl && (srl->lay & lay_changed))
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
	
	if (sce->theDag == NULL) {
		printf("DAG zero... not allowed to happen!\n");
		DAG_scene_relations_update(bmain, sce);
	}
	
	firstnode = sce->theDag->DagNode.first;  /* always scene node */

	/* first we flush the layer flags */
	dag_scene_flush_layers(sce, lay);

	/* then we use the relationships + layer info to flush update events */
	sce->theDag->time++;  /* so we know which nodes were accessed */
	lasttime = sce->theDag->time;
	for (itA = firstnode->child; itA; itA = itA->next)
		if (itA->node->lasttime != lasttime && itA->node->type == ID_OB)
			flush_update_node(bmain, itA->node, lay, lasttime);

	/* if update is not due to time change, do pointcache clears */
	if (!time) {
		sce->theDag->time++;  /* so we know which nodes were accessed */
		lasttime = sce->theDag->time;
		for (itA = firstnode->child; itA; itA = itA->next) {
			if (itA->node->lasttime != lasttime && itA->node->type == ID_OB) {
				ob = (Object *)(itA->node->ob);

				if (ob->recalc & OB_RECALC_ALL) {
					if (BKE_ptcache_object_reset(sce, ob, PTCACHE_RESET_DEPSGRAPH)) {
						ob->recalc |= OB_RECALC_DATA;
						lib_id_recalc_data_tag(bmain, &ob->id);
					}

					flush_pointcache_reset(bmain, sce, itA->node, lasttime, 1);
				}
				else
					flush_pointcache_reset(bmain, sce, itA->node, lasttime, 0);
			}
		}
	}
	
	dag_tag_renderlayers(sce, lay);
}

static int object_modifiers_use_time(Object *ob)
{
	ModifierData *md;
	
	/* check if a modifier in modifier stack needs time input */
	for (md = ob->modifiers.first; md; md = md->next)
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
		
		/* XXX: also, should check NLA strips, though for now assume that nobody uses
		 * that and we can omit that for performance reasons... */
	}
	
	return 0;
}

static short animdata_use_time(AnimData *adt)
{
	NlaTrack *nlt;
	
	if (adt == NULL) return 0;
	
	/* check action - only if assigned, and it has anim curves */
	if (adt->action && adt->action->curves.first)
		return 1;
	
	/* check NLA tracks + strips */
	for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
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

static void dag_object_time_update_flags(Main *bmain, Scene *scene, Object *ob)
{
	if (ob->constraints.first) {
		bConstraint *con;
		for (con = ob->constraints.first; con; con = con->next) {
			bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			if (cti) {
				/* special case for camera tracking -- it doesn't use targets to define relations */
				if (ELEM(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER, CONSTRAINT_TYPE_OBJECTSOLVER)) {
					ob->recalc |= OB_RECALC_OB;
				}
				else if (cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct = targets.first; ct; ct = ct->next) {
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
	
	if (ob->parent) {
		/* motion path or bone child */
		if (ob->parent->type == OB_CURVE || ob->parent->type == OB_ARMATURE) ob->recalc |= OB_RECALC_OB;
	}
	
#if 0 // XXX old animation system
	if (ob->nlastrips.first) {
		if (ob->dup_group) {
			bActionStrip *strip;
			/* this case is for groups with nla, whilst nla target has no action or nla */
			for (strip = ob->nlastrips.first; strip; strip = strip->next) {
				if (strip->object)
					strip->object->recalc |= OB_RECALC_ALL;
			}
		}
	}
#endif // XXX old animation system
	
	if (animdata_use_time(ob->adt)) {
		ob->recalc |= OB_RECALC_OB;
		ob->adt->recalc |= ADT_RECALC_ANIM;
	}
	
	if ((ob->adt) && (ob->type == OB_ARMATURE)) ob->recalc |= OB_RECALC_DATA;
	
	if (object_modifiers_use_time(ob)) ob->recalc |= OB_RECALC_DATA;
	if ((ob->pose) && (ob->pose->flag & POSE_CONSTRAINTS_TIMEDEPEND)) ob->recalc |= OB_RECALC_DATA;
	
	// XXX: scene here may not be the scene that contains the rigidbody world affecting this!
	if (ob->rigidbody_object && BKE_scene_check_rigidbody_active(scene))
		ob->recalc |= OB_RECALC_OB;
	
	{
		AnimData *adt = BKE_animdata_from_id((ID *)ob->data);
		Mesh *me;
		Curve *cu;
		Lattice *lt;
		
		switch (ob->type) {
			case OB_MESH:
				me = ob->data;
				if (me->key) {
					if (!(ob->shapeflag & OB_SHAPE_LOCK)) {
						ob->recalc |= OB_RECALC_DATA;
					}
				}
				if (ob->particlesystem.first)
					ob->recalc |= OB_RECALC_DATA;
				break;
			case OB_CURVE:
			case OB_SURF:
				cu = ob->data;
				if (cu->key) {
					if (!(ob->shapeflag & OB_SHAPE_LOCK)) {
						ob->recalc |= OB_RECALC_DATA;
					}
				}
				break;
			case OB_FONT:
				cu = ob->data;
				if (BLI_listbase_is_empty(&cu->nurb) && cu->str && cu->vfont)
					ob->recalc |= OB_RECALC_DATA;
				break;
			case OB_LATTICE:
				lt = ob->data;
				if (lt->key) {
					if (!(ob->shapeflag & OB_SHAPE_LOCK)) {
						ob->recalc |= OB_RECALC_DATA;
					}
				}
				break;
			case OB_MBALL:
				if (ob->transflag & OB_DUPLI) ob->recalc |= OB_RECALC_DATA;
				break;
			case OB_EMPTY:
				/* update animated images */
				if (ob->empty_drawtype == OB_EMPTY_IMAGE && ob->data)
					if (BKE_image_is_animated(ob->data))
						ob->recalc |= OB_RECALC_DATA;
				break;
		}
		
		if (animdata_use_time(adt)) {
			ob->recalc |= OB_RECALC_DATA;
			adt->recalc |= ADT_RECALC_ANIM;
		}

		if (ob->particlesystem.first) {
			ParticleSystem *psys = ob->particlesystem.first;

			for (; psys; psys = psys->next) {
				if (psys_check_enabled(ob, psys)) {
					ob->recalc |= OB_RECALC_DATA;
					break;
				}
			}
		}
	}

	if (ob->recalc & OB_RECALC_OB)
		lib_id_recalc_tag(bmain, &ob->id);
	if (ob->recalc & OB_RECALC_DATA)
		lib_id_recalc_data_tag(bmain, &ob->id);

}

/* recursively update objects in groups, each group is done at most once */
static void dag_group_update_flags(Main *bmain, Scene *scene, Group *group, const bool do_time)
{
	GroupObject *go;

	if (group->id.flag & LIB_DOIT)
		return;
	
	group->id.flag |= LIB_DOIT;

	for (go = group->gobject.first; go; go = go->next) {
		if (do_time)
			dag_object_time_update_flags(bmain, scene, go->ob);
		if (go->ob->dup_group)
			dag_group_update_flags(bmain, scene, go->ob->dup_group, do_time);
	}
}

/* flag all objects that need recalc, for changes in time for example */
/* do_time: make this optional because undo resets objects to their animated locations without this */
void DAG_scene_update_flags(Main *bmain, Scene *scene, unsigned int lay, const bool do_time, const bool do_invisible_flush)
{
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	Scene *sce_iter;

	BKE_main_id_tag_idcode(bmain, ID_GR, false);

	/* set ob flags where animated systems are */
	for (SETLOOPER(scene, sce_iter, base)) {
		ob = base->object;

		if (do_time) {
			/* now if DagNode were part of base, the node->lay could be checked... */
			/* we do all now, since the scene_flush checks layers and clears recalc flags even */
			
			/* NOTE: "sce_iter" not "scene" so that rigidbodies in background scenes work 
			 * (i.e. muting + rbw availability can be checked and tagged properly) [#33970] 
			 */
			dag_object_time_update_flags(bmain, sce_iter, ob);
		}

		/* recursively tag groups with LIB_DOIT, and update flags for objects */
		if (ob->dup_group)
			dag_group_update_flags(bmain, scene, ob->dup_group, do_time);
	}

	for (sce_iter = scene; sce_iter; sce_iter = sce_iter->set)
		DAG_scene_flush_update(bmain, sce_iter, lay, 1);
	
	if (do_time) {
		/* test: set time flag, to disable baked systems to update */
		for (SETLOOPER(scene, sce_iter, base)) {
			ob = base->object;
			if (ob->recalc & OB_RECALC_ALL)
				ob->recalc |= OB_RECALC_TIME;
		}

		/* hrmf... an exception to look at once, for invisible camera object we do it over */
		if (scene->camera)
			dag_object_time_update_flags(bmain, scene, scene->camera);
	}

	/* and store the info in groupobject */
	for (group = bmain->group.first; group; group = group->id.next) {
		if (group->id.flag & LIB_DOIT) {
			for (go = group->gobject.first; go; go = go->next) {
				go->recalc = go->ob->recalc;
				// printf("ob %s recalc %d\n", go->ob->id.name, go->recalc);
			}
			group->id.flag &= ~LIB_DOIT;
		}
	}

	if (do_invisible_flush) {
		dag_invisible_dependencies_check_flush(bmain, scene);
	}
}

/* struct returned by DagSceneLayer */
typedef struct DagSceneLayer {
	struct DagSceneLayer *next, *prev;
	Scene *scene;
	unsigned int layer;
} DagSceneLayer;

/* returns visible scenes with valid DAG */
static void dag_current_scene_layers(Main *bmain, ListBase *lb)
{
	wmWindowManager *wm;
	wmWindow *win;
	
	BLI_listbase_clear(lb);

	/* if we have a windowmanager, look into windows */
	if ((wm = bmain->wm.first)) {
		
		BKE_main_id_flag_listbase(&bmain->scene, LIB_DOIT, 1);

		for (win = wm->windows.first; win; win = win->next) {
			if (win->screen && win->screen->scene->theDag) {
				Scene *scene = win->screen->scene;
				DagSceneLayer *dsl;

				if (scene->id.flag & LIB_DOIT) {
					dsl = MEM_mallocN(sizeof(DagSceneLayer), "dag scene layer");

					BLI_addtail(lb, dsl);

					dsl->scene = scene;
					dsl->layer = BKE_screen_visible_layers(win->screen, scene);

					scene->id.flag &= ~LIB_DOIT;
				}
				else {
					/* It is possible that multiple windows shares the same scene
					 * and have different layers visible.
					 *
					 * Here we deal with such cases by squashing layers bits from
					 * multiple windoew to the DagSceneLayer.
					 *
					 * TODO(sergey): Such a lookup could be optimized perhaps,
					 * however should be fine for now since we usually have only
					 * few open windows.
					 */
					for (dsl = lb->first; dsl; dsl = dsl->next) {
						if (dsl->scene == scene) {
							dsl->layer |= BKE_screen_visible_layers(win->screen, scene);
							break;
						}
					}
				}
			}
		}
	}
	else {
		/* if not, use the first sce */
		DagSceneLayer *dsl = MEM_mallocN(sizeof(DagSceneLayer), "dag scene layer");
		
		BLI_addtail(lb, dsl);
		
		dsl->scene = bmain->scene.first;
		dsl->layer = dsl->scene->lay;

		/* XXX for background mode, we should get the scene
		 * from somewhere, for the -S option, but it's in
		 * the context, how to get it here? */
	}
}

static void dag_group_on_visible_update(Group *group)
{
	GroupObject *go;

	if (group->id.flag & LIB_DOIT)
		return;
	
	group->id.flag |= LIB_DOIT;

	for (go = group->gobject.first; go; go = go->next) {
		if (ELEM(go->ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_LATTICE)) {
			go->ob->recalc |= OB_RECALC_DATA;
			go->ob->id.flag |= LIB_DOIT;
			lib_id_recalc_tag(G.main, &go->ob->id);
		}
		if (go->ob->proxy_from) {
			go->ob->recalc |= OB_RECALC_OB;
			go->ob->id.flag |= LIB_DOIT;
			lib_id_recalc_tag(G.main, &go->ob->id);
		}

		if (go->ob->dup_group)
			dag_group_on_visible_update(go->ob->dup_group);
	}
}

void DAG_on_visible_update(Main *bmain, const bool do_time)
{
	ListBase listbase;
	DagSceneLayer *dsl;
	
	/* get list of visible scenes and layers */
	dag_current_scene_layers(bmain, &listbase);
	
	for (dsl = listbase.first; dsl; dsl = dsl->next) {
		Scene *scene = dsl->scene;
		Scene *sce_iter;
		Base *base;
		Object *ob;
		DagNode *node;
		unsigned int lay = dsl->layer, oblay;

		/* derivedmeshes and displists are not saved to file so need to be
		 * remade, tag them so they get remade in the scene update loop,
		 * note armature poses or object matrices are preserved and do not
		 * require updates, so we skip those */
		for (sce_iter = scene; sce_iter; sce_iter = sce_iter->set)
			dag_scene_flush_layers(sce_iter, lay);

		BKE_main_id_tag_idcode(bmain, ID_GR, false);

		for (SETLOOPER(scene, sce_iter, base)) {
			ob = base->object;
			node = (sce_iter->theDag) ? dag_get_node(sce_iter->theDag, ob) : NULL;
			oblay = (node) ? node->lay : ob->lay;

			if ((oblay & lay) & ~scene->lay_updated) {
				if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_LATTICE)) {
					ob->recalc |= OB_RECALC_DATA;
					lib_id_recalc_tag(bmain, &ob->id);
				}
				if (ob->proxy && (ob->proxy_group == NULL)) {
					ob->proxy->recalc |= OB_RECALC_DATA;
					lib_id_recalc_tag(bmain, &ob->id);
				}
				if (ob->dup_group)
					dag_group_on_visible_update(ob->dup_group);
			}
		}

		BKE_main_id_tag_idcode(bmain, ID_GR, false);

		/* now tag update flags, to ensure deformers get calculated on redraw */
		DAG_scene_update_flags(bmain, scene, lay, do_time, true);
		scene->lay_updated |= lay;
	}
	
	BLI_freelistN(&listbase);

	/* hack to get objects updating on layer changes */
	DAG_id_type_tag(bmain, ID_OB);

	/* so masks update on load */
	if (bmain->mask.first) {
		Mask *mask;

		for (mask = bmain->mask.first; mask; mask = mask->id.next) {
			DAG_id_tag_update(&mask->id, 0);
		}
	}
}

static void dag_id_flush_update__isDependentTexture(void *userData, Object *UNUSED(ob), ID **idpoin)
{
	struct { ID *id; bool is_dependent; } *data = userData;
	
	if (*idpoin && GS((*idpoin)->name) == ID_TE) {
		if (data->id == (*idpoin))
			data->is_dependent = 1;
	}
}

static void dag_id_flush_update(Main *bmain, Scene *sce, ID *id)
{
	Object *obt, *ob = NULL;
	short idtype;

	/* here we flush a few things before actual scene wide flush, mostly
	 * due to only objects and not other datablocks being in the depsgraph */

	/* set flags & pointcache for object */
	if (GS(id->name) == ID_OB) {
		ob = (Object *)id;
		BKE_ptcache_object_reset(sce, ob, PTCACHE_RESET_DEPSGRAPH);

		/* So if someone tagged object recalc directly,
		 * id_tag_update bit-field stays relevant
		 */
		if (ob->recalc & OB_RECALC_ALL) {
			DAG_id_type_tag(bmain, GS(id->name));
		}

		if (ob->recalc & OB_RECALC_DATA) {
			/* all users of this ob->data should be checked */
			id = ob->data;

			/* no point in trying in this cases */
			if (id && id->us <= 1) {
				dag_editors_id_update(bmain, id);
				id = NULL;
			}
		}
	}

	/* set flags & pointcache for object data */
	if (id) {
		idtype = GS(id->name);


		if (OB_DATA_SUPPORT_ID(idtype)) {
			for (obt = bmain->object.first; obt; obt = obt->id.next) {
				if (!(ob && obt == ob) && obt->data == id) {
					obt->recalc |= OB_RECALC_DATA;
					lib_id_recalc_data_tag(bmain, &obt->id);
					BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
				}
			}
		}
		
		/* set flags based on textures - can influence depgraph via modifiers */
		if (idtype == ID_TE) {
			for (obt = bmain->object.first; obt; obt = obt->id.next) {
				struct { ID *id; bool is_dependent; } data;
				data.id = id;
				data.is_dependent = 0;

				modifiers_foreachIDLink(obt, dag_id_flush_update__isDependentTexture, &data);
				if (data.is_dependent) {
					obt->recalc |= OB_RECALC_DATA;
					lib_id_recalc_data_tag(bmain, &obt->id);
				}

				/* particle settings can use the texture as well */
				if (obt->particlesystem.first) {
					ParticleSystem *psys = obt->particlesystem.first;
					MTex **mtexp, *mtex;
					int a;
					for (; psys; psys = psys->next) {
						mtexp = psys->part->mtex;
						for (a = 0; a < MAX_MTEX; a++, mtexp++) {
							mtex = *mtexp;
							if (mtex && mtex->tex == (Tex *)id) {
								obt->recalc |= OB_RECALC_DATA;
								lib_id_recalc_data_tag(bmain, &obt->id);

								if (mtex->mapto & PAMAP_INIT)
									psys->recalc |= PSYS_RECALC_RESET;
								if (mtex->mapto & PAMAP_CHILD)
									psys->recalc |= PSYS_RECALC_CHILD;

								BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
							}
						}
					}
				}
			}
		}
		
		/* set flags based on ShapeKey */
		if (idtype == ID_KE) {
			for (obt = bmain->object.first; obt; obt = obt->id.next) {
				Key *key = BKE_key_from_object(obt);
				if (!(ob && obt == ob) && ((ID *)key == id)) {
					obt->flag |= (OB_RECALC_OB | OB_RECALC_DATA);
					lib_id_recalc_tag(bmain, &obt->id);
					lib_id_recalc_data_tag(bmain, &obt->id);
					BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
				}
			}
		}
		
		/* set flags based on particle settings */
		if (idtype == ID_PA) {
			ParticleSystem *psys;
			for (obt = bmain->object.first; obt; obt = obt->id.next)
				for (psys = obt->particlesystem.first; psys; psys = psys->next)
					if (&psys->part->id == id)
						BKE_ptcache_object_reset(sce, obt, PTCACHE_RESET_DEPSGRAPH);
		}

		if (ELEM(idtype, ID_MA, ID_TE)) {
			const bool new_shading_nodes = BKE_scene_use_new_shading_nodes(sce);
			for (obt = bmain->object.first; obt; obt = obt->id.next) {
				if (obt->mode & OB_MODE_TEXTURE_PAINT) {
					obt->recalc |= OB_RECALC_DATA;
					BKE_texpaint_slots_refresh_object(obt, new_shading_nodes);
					lib_id_recalc_data_tag(bmain, &obt->id);
				}
			}
		}

		if (idtype == ID_MC) {
			MovieClip *clip = (MovieClip *) id;

			BKE_tracking_dopesheet_tag_update(&clip->tracking);

			for (obt = bmain->object.first; obt; obt = obt->id.next) {
				bConstraint *con;
				for (con = obt->constraints.first; con; con = con->next) {
					bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
					if (ELEM(cti->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_CAMERASOLVER,
					          CONSTRAINT_TYPE_OBJECTSOLVER))
					{
						obt->recalc |= OB_RECALC_OB;
						lib_id_recalc_tag(bmain, &obt->id);
						break;
					}
				}
			}

			if (sce->nodetree) {
				bNode *node;

				for (node = sce->nodetree->nodes.first; node; node = node->next) {
					if (node->id == id) {
						nodeUpdate(sce->nodetree, node);
					}
				}
			}
		}

		/* Not pretty to iterate all the nodes here, but it's as good as it
		 * could be with the current depsgraph design/
		 */
		if (idtype == ID_IM) {
			FOREACH_NODETREE(bmain, ntree, parent_id) {
				if (ntree->type == NTREE_SHADER) {
					bNode *node;
					for (node = ntree->nodes.first; node; node = node->next) {
						if (node->id == id) {
							lib_id_recalc_tag(bmain, &ntree->id);
							break;
						}
					}
				}
			} FOREACH_NODETREE_END
		}

		if (idtype == ID_MSK) {
			if (sce->nodetree) {
				bNode *node;

				for (node = sce->nodetree->nodes.first; node; node = node->next) {
					if (node->id == id) {
						nodeUpdate(sce->nodetree, node);
					}
				}
			}
		}

		/* camera's matrix is used to orient reconstructed stuff,
		 * so it should happen tracking-related constraints recalculation
		 * when camera is changing (sergey) */
		if (sce->camera && &sce->camera->id == id) {
			MovieClip *clip = BKE_object_movieclip_get(sce, sce->camera, true);

			if (clip)
				dag_id_flush_update(bmain, sce, &clip->id);
		}

		/* update editors */
		dag_editors_id_update(bmain, id);
	}
}

void DAG_ids_flush_tagged(Main *bmain)
{
	ListBase listbase;
	DagSceneLayer *dsl;
	ListBase *lbarray[MAX_LIBARRAY];
	int a;
	bool do_flush = false;
	
	/* get list of visible scenes and layers */
	dag_current_scene_layers(bmain, &listbase);

	if (BLI_listbase_is_empty(&listbase))
		return;

	/* loop over all ID types */
	a  = set_listbasepointers(bmain, lbarray);

	while (a--) {
		ListBase *lb = lbarray[a];
		ID *id = lb->first;

		/* we tag based on first ID type character to avoid 
		 * looping over all ID's in case there are no tags */
		if (id && bmain->id_tag_update[id->name[0]]) {
			for (; id; id = id->next) {
				if (id->flag & (LIB_ID_RECALC | LIB_ID_RECALC_DATA)) {
					
					for (dsl = listbase.first; dsl; dsl = dsl->next)
						dag_id_flush_update(bmain, dsl->scene, id);
					
					do_flush = true;
				}
			}
		}
	}

	/* flush changes to other objects */
	if (do_flush) {
		for (dsl = listbase.first; dsl; dsl = dsl->next)
			DAG_scene_flush_update(bmain, dsl->scene, dsl->layer, 0);
	}
	
	BLI_freelistN(&listbase);
}

void DAG_ids_check_recalc(Main *bmain, Scene *scene, bool time)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;
	bool updated = false;

	/* loop over all ID types */
	a  = set_listbasepointers(bmain, lbarray);

	while (a--) {
		ListBase *lb = lbarray[a];
		ID *id = lb->first;

		/* we tag based on first ID type character to avoid 
		 * looping over all ID's in case there are no tags */
		if (id && bmain->id_tag_update[id->name[0]]) {
			updated = true;
			break;
		}
	}

	dag_editors_scene_update(bmain, scene, (updated || time));
}

/* It is possible that scene_update_post and frame_update_post handlers
 * will modify objects. The issue is that DAG_ids_clear_recalc is called
 * just after callbacks, which leaves objects with recalc flags but no
 * corresponding bit in ID recalc bitfield. This leads to some kind of
 * regression when using ID type tag fields to check whether there objects
 * to be updated internally comparing threaded DAG with legacy one.
 *
 * For now let's have a workaround which will preserve tag for ID_OB
 * if there're objects with OB_RECALC_ALL bits. This keeps behavior
 * unchanged comparing with 2.69 release.
 *
 * TODO(sergey): Need to get rid of such a workaround.
 *
 *                                                 - sergey -
 */

#define POST_UPDATE_HANDLER_WORKAROUND

void DAG_ids_clear_recalc(Main *bmain)
{
	ListBase *lbarray[MAX_LIBARRAY];
	bNodeTree *ntree;
	int a;

#ifdef POST_UPDATE_HANDLER_WORKAROUND
	bool have_updated_objects = false;

	if (DAG_id_type_tagged(bmain, ID_OB)) {
		ListBase listbase;
		DagSceneLayer *dsl;

		/* We need to check all visible scenes, otherwise resetting
		 * OB_ID changed flag will only work fine for first scene of
		 * multiple visible and all the rest will skip update.
		 *
		 * This could also lead to wrong behavior scene update handlers
		 * because of missing ID datablock changed flags.
		 *
		 * This is a bit of a bummer to allocate list here, but likely
		 * it wouldn't become too much bad because it only happens when
		 * objects were actually changed.
		 */
		dag_current_scene_layers(bmain, &listbase);

		for (dsl = listbase.first; dsl; dsl = dsl->next) {
			Scene *scene = dsl->scene;
			DagNode *node;
			for (node = scene->theDag->DagNode.first;
			     node != NULL && have_updated_objects == false;
			     node = node->next)
			{
				if (node->type == ID_OB) {
					Object *object = (Object *) node->ob;
					if (object->recalc & OB_RECALC_ALL) {
						have_updated_objects = true;
						break;
					}
				}
			}
		}

		BLI_freelistN(&listbase);
	}
#endif

	/* loop over all ID types */
	a  = set_listbasepointers(bmain, lbarray);

	while (a--) {
		ListBase *lb = lbarray[a];
		ID *id = lb->first;

		/* we tag based on first ID type character to avoid 
		 * looping over all ID's in case there are no tags */
		if (id && bmain->id_tag_update[id->name[0]]) {
			for (; id; id = id->next) {
				if (id->flag & (LIB_ID_RECALC | LIB_ID_RECALC_DATA))
					id->flag &= ~(LIB_ID_RECALC | LIB_ID_RECALC_DATA);

				/* some ID's contain semi-datablock nodetree */
				ntree = ntreeFromID(id);
				if (ntree && (ntree->id.flag & (LIB_ID_RECALC | LIB_ID_RECALC_DATA)))
					ntree->id.flag &= ~(LIB_ID_RECALC | LIB_ID_RECALC_DATA);
			}
		}
	}

	memset(bmain->id_tag_update, 0, sizeof(bmain->id_tag_update));

#ifdef POST_UPDATE_HANDLER_WORKAROUND
	if (have_updated_objects) {
		DAG_id_type_tag(bmain, ID_OB);
	}
#endif
}

void DAG_id_tag_update_ex(Main *bmain, ID *id, short flag)
{
	if (id == NULL) return;

	if (G.debug & G_DEBUG_DEPSGRAPH) {
		printf("%s: id=%s flag=%d\n", __func__, id->name, flag);
	}

	/* tag ID for update */
	if (flag) {
		if (flag & OB_RECALC_OB)
			lib_id_recalc_tag(bmain, id);
		if (flag & (OB_RECALC_DATA | PSYS_RECALC))
			lib_id_recalc_data_tag(bmain, id);
	}
	else
		lib_id_recalc_tag(bmain, id);

	/* flag is for objects and particle systems */
	if (flag) {
		Object *ob;
		short idtype = GS(id->name);

		if (idtype == ID_OB) {
			/* only quick tag */
			ob = (Object *)id;
			ob->recalc |= (flag & OB_RECALC_ALL);
		}
		else if (idtype == ID_PA) {
			ParticleSystem *psys;
			/* this is weak still, should be done delayed as well */
			for (ob = bmain->object.first; ob; ob = ob->id.next) {
				for (psys = ob->particlesystem.first; psys; psys = psys->next) {
					if (&psys->part->id == id) {
						ob->recalc |= (flag & OB_RECALC_ALL);
						psys->recalc |= (flag & PSYS_RECALC);
						lib_id_recalc_tag(bmain, &ob->id);
						lib_id_recalc_data_tag(bmain, &ob->id);
					}
				}
			}
		}
		else if (idtype == ID_VF) {
			/* this is weak still, should be done delayed as well */
			for (ob = bmain->object.first; ob; ob = ob->id.next) {
				if (ob->type == OB_FONT) {
					Curve *cu = ob->data;

					if (ELEM((struct VFont *)id, cu->vfont, cu->vfontb, cu->vfonti, cu->vfontbi)) {
						ob->recalc |= (flag & OB_RECALC_ALL);
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

void DAG_id_tag_update(ID *id, short flag)
{
	DAG_id_tag_update_ex(G.main, id, flag);
}

void DAG_id_type_tag(Main *bmain, short idtype)
{
	if (idtype == ID_NT) {
		/* stupid workaround so parent datablocks of nested nodetree get looped
		 * over when we loop over tagged datablock types */
		DAG_id_type_tag(bmain, ID_MA);
		DAG_id_type_tag(bmain, ID_TE);
		DAG_id_type_tag(bmain, ID_LA);
		DAG_id_type_tag(bmain, ID_WO);
		DAG_id_type_tag(bmain, ID_SCE);
	}

	bmain->id_tag_update[((char *)&idtype)[0]] = 1;
}

int DAG_id_type_tagged(Main *bmain, short idtype)
{
	return bmain->id_tag_update[((char *)&idtype)[0]];
}

#if 0 // UNUSED
/* recursively descends tree, each node only checked once */
/* node is checked to be of type object */
static int parent_check_node(DagNode *node, int curtime)
{
	DagAdjList *itA;
	
	node->lasttime = curtime;
	
	if (node->color == DAG_GRAY)
		return DAG_GRAY;
	
	for (itA = node->child; itA; itA = itA->next) {
		if (itA->node->type == ID_OB) {
			
			if (itA->node->color == DAG_GRAY)
				return DAG_GRAY;

			/* descend if not done */
			if (itA->node->lasttime != curtime) {
				itA->node->color = parent_check_node(itA->node, curtime);
			
				if (itA->node->color == DAG_GRAY)
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
	bPose *pose = ob->pose;
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
	dag->ugly_hack_sorry = false;  /* no ID structs */

	rootnode = dag_add_node(dag, NULL);  /* node->ob becomes NULL */
	
	/* we add the hierarchy and the constraints */
	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
		int addtoroot = 1;
		
		node = dag_get_node(dag, pchan);
		
		if (pchan->parent) {
			node2 = dag_get_node(dag, pchan->parent);
			dag_add_relation(dag, node2, node, 0, "Parent Relation");
			addtoroot = 0;
		}
		for (con = pchan->constraints.first; con; con = con->next) {
			bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(con, &targets);
				
				for (ct = targets.first; ct; ct = ct->next) {
					if (ct->tar == ob && ct->subtarget[0]) {
						bPoseChannel *target = BKE_pose_channel_find_name(ob->pose, ct->subtarget);
						if (target) {
							node2 = dag_get_node(dag, target);
							dag_add_relation(dag, node2, node, 0, "Pose Constraint");
							
							if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
								bKinematicConstraint *data = (bKinematicConstraint *)con->data;
								bPoseChannel *parchan;
								int segcount = 0;
								
								/* exclude tip from chain? */
								if (!(data->flag & CONSTRAINT_IK_TIP))
									parchan = pchan->parent;
								else
									parchan = pchan;
								
								/* Walk to the chain's root */
								while (parchan) {
									node3 = dag_get_node(dag, parchan);
									dag_add_relation(dag, node2, node3, 0, "IK Constraint");
									
									segcount++;
									if (segcount == data->rootbone || segcount > 255) break;  /* 255 is weak */
									parchan = parchan->parent;
								}
							}
						}
					}
				}
				
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 1);
			}
		}
		if (addtoroot == 1) {
			dag_add_relation(dag, rootnode, node, 0, "Root Bone Relation");
		}
	}

	dag_check_cycle(dag);
	
	/* now we try to sort... */
	BLI_listbase_clear(&tempbase);

	nqueue = queue_create(DAGQUEUEALLOC);
	
	/* tag nodes unchecked */
	for (node = dag->DagNode.first; node; node = node->next)
		node->color = DAG_WHITE;
	
	rootnode->color = DAG_GRAY;
	push_stack(nqueue, rootnode);  
	
	while (nqueue->count) {
		
		skip = 0;
		node = get_top_node_queue(nqueue);
		
		itA = node->child;
		while (itA != NULL) {
			if (itA->node->color == DAG_WHITE) {
				itA->node->color = DAG_GRAY;
				push_stack(nqueue, itA->node);
				skip = 1;
				break;
			}
			itA = itA->next;
		}
		
		if (!skip) {
			if (node) {
				node = pop_queue(nqueue);
				if (node->ob == NULL)  /* we are done */
					break;
				node->color = DAG_BLACK;
				
				/* put node in new list */
				BLI_remlink(&pose->chanbase, node->ob);
				BLI_addhead(&tempbase, node->ob);
			}
		}
	}
	
	/* temporal correction for circular dependencies */
	while (pose->chanbase.first) {
		pchan = pose->chanbase.first;
		BLI_remlink(&pose->chanbase, pchan);
		BLI_addhead(&tempbase, pchan);

		printf("cyclic %s\n", pchan->name);
	}
	
	pose->chanbase = tempbase;
	queue_delete(nqueue);
	
//	printf("\nordered\n");
//	for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
//		printf(" %s\n", pchan->name);
//	}
	
	free_forest(dag);
	MEM_freeN(dag);
}

/* ************************  DAG FOR THREADED UPDATE  ********************* */

/* Initialize run-time data in the graph needed for traversing it
 * from multiple threads and start threaded tree traversal by adding
 * the root node to the queue.
 *
 * This will mark DAG nodes as object/non-object and will calculate
 * num_pending_parents of nodes (which is how many non-updated parents node
 * have, which helps a lot checking whether node could be scheduled
 * already or not).
 */
void DAG_threaded_update_begin(Scene *scene,
                               void (*func)(void *node, void *user_data),
                               void *user_data)
{
	DagNode *node;

	/* We reset num_pending_parents to zero first and tag node as not scheduled yet... */
	for (node = scene->theDag->DagNode.first; node; node = node->next) {
		node->num_pending_parents = 0;
		node->scheduled = false;
	}

	/* ... and then iterate over all the nodes and
	 * increase num_pending_parents for node childs.
	 */
	for (node = scene->theDag->DagNode.first; node; node = node->next) {
		DagAdjList *itA;

		for (itA = node->child; itA; itA = itA->next) {
			if (itA->node != node) {
				itA->node->num_pending_parents++;
			}
		}
	}

	/* Add root nodes to the queue. */
	BLI_spin_lock(&threaded_update_lock);
	for (node = scene->theDag->DagNode.first; node; node = node->next) {
		if (node->num_pending_parents == 0) {
			node->scheduled = true;
			func(node, user_data);
		}
	}
	BLI_spin_unlock(&threaded_update_lock);
}

/* This function is called when handling node is done.
 *
 * This function updates num_pending_parents for all childs and
 * schedules them if they're ready.
 */
void DAG_threaded_update_handle_node_updated(void *node_v,
                                             void (*func)(void *node, void *user_data),
                                             void *user_data)
{
	DagNode *node = node_v;
	DagAdjList *itA;

	for (itA = node->child; itA; itA = itA->next) {
		DagNode *child_node = itA->node;
		if (child_node != node) {
			atomic_sub_uint32(&child_node->num_pending_parents, 1);

			if (child_node->num_pending_parents == 0) {
				bool need_schedule;

				BLI_spin_lock(&threaded_update_lock);
				need_schedule = child_node->scheduled == false;
				child_node->scheduled = true;
				BLI_spin_unlock(&threaded_update_lock);

				if (need_schedule) {
					func(child_node, user_data);
				}
			}
		}
	}
}

/* ************************ DAG DEBUGGING ********************* */

void DAG_print_dependencies(Main *bmain, Scene *scene, Object *ob)
{
	/* utility for debugging dependencies */
	dag_print_dependencies = 1;

	if (ob && (ob->mode & OB_MODE_POSE)) {
		printf("\nDEPENDENCY RELATIONS for %s\n\n", ob->id.name + 2);
		DAG_pose_sort(ob);
	}
	else {
		printf("\nDEPENDENCY RELATIONS for %s\n\n", scene->id.name + 2);
		DAG_scene_relations_rebuild(bmain, scene);
	}
	
	dag_print_dependencies = 0;
}

/* ************************ DAG querying ********************* */

/* Will return Object ID if node represents Object,
 * and will return NULL otherwise.
 */
Object *DAG_get_node_object(void *node_v)
{
	DagNode *node = node_v;

	if (node->type == ID_OB) {
		return node->ob;
	}

	return NULL;
}

/* Returns node name, used for debug output only, atm. */
const char *DAG_get_node_name(Scene *scene, void *node_v)
{
	DagNode *node = node_v;

	return dag_node_name(scene->theDag, node);
}

short DAG_get_eval_flags_for_object(Scene *scene, void *object)
{
	DagNode *node;

	if (scene->theDag == NULL) {
		/* Happens when converting objects to mesh from a python script
		 * after modifying scene graph.
		 *
		 * Currently harmless because it's only called for temporary
		 * objects which are out of the DAG anyway.
		 */
		return 0;
	}

	node = dag_find_node(scene->theDag, object);

	if (node) {
		return node->eval_flags;
	}
	else {
		/* Happens when external render engine exports temporary objects
		 * which are not in the DAG.
		 */

		/* TODO(sergey): Doublecheck objects with Curve Deform exports all fine. */

		/* TODO(sergey): Weak but currently we can't really access proper DAG from
		 * the modifiers stack. This is because in most cases modifier is to use
		 * the foreground scene, but to access evaluation flags we need to know
		 * active background scene, which we don't know.
		 */
		if (scene->set) {
			return DAG_get_eval_flags_for_object(scene->set, object);
		}
		return 0;
	}
}

bool DAG_is_acyclic(Scene *scene)
{
	return scene->theDag->is_acyclic;
}
