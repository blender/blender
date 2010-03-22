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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_outliner_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_world_types.h"
#include "DNA_sequence_types.h"

#include "BLI_blenlib.h"

#include "IMB_imbuf_types.h"

#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_utildefines.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "outliner_intern.h"

#include "PIL_time.h" 


#define OL_H	19
#define OL_X	18

#define OL_TOG_RESTRICT_VIEWX	54
#define OL_TOG_RESTRICT_SELECTX	36
#define OL_TOG_RESTRICT_RENDERX	18

#define OL_TOGW OL_TOG_RESTRICT_VIEWX

#define OL_RNA_COLX			300
#define OL_RNA_COL_SIZEX	150
#define OL_RNA_COL_SPACEX	50

#define TS_CHUNK	128

#define TREESTORE(a) ((a)?soops->treestore->data+(a)->store_index:NULL)

/* ************* XXX **************** */

static void error() {}

/* ********************************** */


/* ******************** PROTOTYPES ***************** */
static void outliner_draw_tree_element(bContext *C, uiBlock *block, Scene *scene, ARegion *ar, SpaceOops *soops, TreeElement *te, int startx, int *starty);
static void outliner_do_object_operation(bContext *C, Scene *scene, SpaceOops *soops, ListBase *lb, 
										 void (*operation_cb)(bContext *C, Scene *scene, TreeElement *, TreeStoreElem *, TreeStoreElem *));


/* ******************** PERSISTANT DATA ***************** */

static void outliner_storage_cleanup(SpaceOops *soops)
{
	TreeStore *ts= soops->treestore;
	
	if(ts) {
		TreeStoreElem *tselem;
		int a, unused= 0;
		
		/* each element used once, for ID blocks with more users to have each a treestore */
		for(a=0, tselem= ts->data; a<ts->usedelem; a++, tselem++) tselem->used= 0;

		/* cleanup only after reading file or undo step, and always for
		 * RNA datablocks view in order to save memory */
		if(soops->storeflag & SO_TREESTORE_CLEANUP) {
			
			for(a=0, tselem= ts->data; a<ts->usedelem; a++, tselem++) {
				if(tselem->id==NULL) unused++;
			}

			if(unused) {
				if(ts->usedelem == unused) {
					MEM_freeN(ts->data);
					ts->data= NULL;
					ts->usedelem= ts->totelem= 0;
				}
				else {
					TreeStoreElem *tsnewar, *tsnew;
					
					tsnew=tsnewar= MEM_mallocN((ts->usedelem-unused)*sizeof(TreeStoreElem), "new tselem");
					for(a=0, tselem= ts->data; a<ts->usedelem; a++, tselem++) {
						if(tselem->id) {
							*tsnew= *tselem;
							tsnew++;
						}
					}
					MEM_freeN(ts->data);
					ts->data= tsnewar;
					ts->usedelem-= unused;
					ts->totelem= ts->usedelem;
				}
			}
		}
	}
}

static void check_persistant(SpaceOops *soops, TreeElement *te, ID *id, short type, short nr)
{
	TreeStore *ts;
	TreeStoreElem *tselem;
	int a;
	
	/* case 1; no TreeStore */
	if(soops->treestore==NULL) {
		ts= soops->treestore= MEM_callocN(sizeof(TreeStore), "treestore");
	}
	ts= soops->treestore;
	
	/* check if 'te' is in treestore */
	tselem= ts->data;
	for(a=0; a<ts->usedelem; a++, tselem++) {
		if(tselem->id==id && tselem->used==0) {
			if((type==0 && tselem->type==0) ||(tselem->type==type && tselem->nr==nr)) {
				te->store_index= a;
				tselem->used= 1;
				return;
			}
		}
	}
	
	/* add 1 element to treestore */
	if(ts->usedelem==ts->totelem) {
		TreeStoreElem *tsnew;
		
		tsnew= MEM_mallocN((ts->totelem+TS_CHUNK)*sizeof(TreeStoreElem), "treestore data");
		if(ts->data) {
			memcpy(tsnew, ts->data, ts->totelem*sizeof(TreeStoreElem));
			MEM_freeN(ts->data);
		}
		ts->data= tsnew;
		ts->totelem+= TS_CHUNK;
	}
	
	tselem= ts->data+ts->usedelem;
	
	tselem->type= type;
	if(type) tselem->nr= nr; // we're picky! :)
	else tselem->nr= 0;
	tselem->id= id;
	tselem->used = 0;
	tselem->flag= TSE_CLOSED;
	te->store_index= ts->usedelem;
	
	ts->usedelem++;
}

/* ******************** TREE MANAGEMENT ****************** */

void outliner_free_tree(ListBase *lb)
{
	
	while(lb->first) {
		TreeElement *te= lb->first;
		
		outliner_free_tree(&te->subtree);
		BLI_remlink(lb, te);

		if(te->flag & TE_FREE_NAME) MEM_freeN(te->name);
		MEM_freeN(te);
	}
}

static void outliner_height(SpaceOops *soops, ListBase *lb, int *h)
{
	TreeElement *te= lb->first;
	while(te) {
		TreeStoreElem *tselem= TREESTORE(te);
		if((tselem->flag & TSE_CLOSED)==0) 
			outliner_height(soops, &te->subtree, h);
		(*h) += OL_H;
		te= te->next;
	}
}

#if 0  // XXX this is currently disabled until te->xend is set correctly
static void outliner_width(SpaceOops *soops, ListBase *lb, int *w)
{
	TreeElement *te= lb->first;
	while(te) {
//		TreeStoreElem *tselem= TREESTORE(te);
		
		// XXX fixme... te->xend is not set yet
		if(tselem->flag & TSE_CLOSED) {
			if (te->xend > *w)
				*w = te->xend;
		}
		outliner_width(soops, &te->subtree, w);
		te= te->next;
	}
}
#endif

static void outliner_rna_width(SpaceOops *soops, ListBase *lb, int *w, int startx)
{
	TreeElement *te= lb->first;
	while(te) {
		TreeStoreElem *tselem= TREESTORE(te);
			// XXX fixme... (currently, we're using a fixed length of 100)!
		/*if(te->xend) {
			if(te->xend > *w)
				*w = te->xend;
		}*/
		if(startx+100 > *w)
			*w = startx+100;

		if((tselem->flag & TSE_CLOSED)==0)
			outliner_rna_width(soops, &te->subtree, w, startx+OL_X);
		te= te->next;
	}
}

static TreeElement *outliner_find_tree_element(ListBase *lb, int store_index)
{
	TreeElement *te= lb->first, *tes;
	while(te) {
		if(te->store_index==store_index) return te;
		tes= outliner_find_tree_element(&te->subtree, store_index);
		if(tes) return tes;
		te= te->next;
	}
	return NULL;
}



static ID *outliner_search_back(SpaceOops *soops, TreeElement *te, short idcode)
{
	TreeStoreElem *tselem;
	te= te->parent;
	
	while(te) {
		tselem= TREESTORE(te);
		if(tselem->type==0 && te->idcode==idcode) return tselem->id;
		te= te->parent;
	}
	return NULL;
}

struct treesort {
	TreeElement *te;
	ID *id;
	char *name;
	short idcode;
};

static int treesort_alpha(const void *v1, const void *v2)
{
	const struct treesort *x1= v1, *x2= v2;
	int comp;
	
	/* first put objects last (hierarchy) */
	comp= (x1->idcode==ID_OB);
	if(x2->idcode==ID_OB) comp+=2;
	
	if(comp==1) return 1;
	else if(comp==2) return -1;
	else if(comp==3) {
		int comp= strcmp(x1->name, x2->name);
		
		if( comp>0 ) return 1;
		else if( comp<0) return -1;
		return 0;
	}
	return 0;
}

/* this is nice option for later? doesnt look too useful... */
#if 0
static int treesort_obtype_alpha(const void *v1, const void *v2)
{
	const struct treesort *x1= v1, *x2= v2;
	
	/* first put objects last (hierarchy) */
	if(x1->idcode==ID_OB && x2->idcode!=ID_OB) return 1;
	else if(x2->idcode==ID_OB && x1->idcode!=ID_OB) return -1;
	else {
		/* 2nd we check ob type */
		if(x1->idcode==ID_OB && x2->idcode==ID_OB) {
			if( ((Object *)x1->id)->type > ((Object *)x2->id)->type) return 1;
			else if( ((Object *)x1->id)->type > ((Object *)x2->id)->type) return -1;
			else return 0;
		}
		else {
			int comp= strcmp(x1->name, x2->name);
			
			if( comp>0 ) return 1;
			else if( comp<0) return -1;
			return 0;
		}
	}
}
#endif

/* sort happens on each subtree individual */
static void outliner_sort(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int totelem=0;
	
	te= lb->last;
	if(te==NULL) return;
	tselem= TREESTORE(te);
	
	/* sorting rules; only object lists or deformgroups */
	if( (tselem->type==TSE_DEFGROUP) || (tselem->type==0 && te->idcode==ID_OB)) {
		
		/* count first */
		for(te= lb->first; te; te= te->next) totelem++;
		
		if(totelem>1) {
			struct treesort *tear= MEM_mallocN(totelem*sizeof(struct treesort), "tree sort array");
			struct treesort *tp=tear;
			int skip= 0;
			
			for(te= lb->first; te; te= te->next, tp++) {
				tselem= TREESTORE(te);
				tp->te= te;
				tp->name= te->name;
				tp->idcode= te->idcode;
				if(tselem->type && tselem->type!=TSE_DEFGROUP) tp->idcode= 0;	// dont sort this
				tp->id= tselem->id;
			}
			/* keep beginning of list */
			for(tp= tear, skip=0; skip<totelem; skip++, tp++)
				if(tp->idcode) break;
			
			if(skip<totelem)
				qsort(tear+skip, totelem-skip, sizeof(struct treesort), treesort_alpha);
			
			lb->first=lb->last= NULL;
			tp= tear;
			while(totelem--) {
				BLI_addtail(lb, tp->te);
				tp++;
			}
			MEM_freeN(tear);
		}
	}
	
	for(te= lb->first; te; te= te->next) {
		outliner_sort(soops, &te->subtree);
	}
}

/* Prototype, see functions below */
static TreeElement *outliner_add_element(SpaceOops *soops, ListBase *lb, void *idv, 
										 TreeElement *parent, short type, short index);


static void outliner_add_passes(SpaceOops *soops, TreeElement *tenla, ID *id, SceneRenderLayer *srl)
{
	TreeStoreElem *tselem= TREESTORE(tenla);
	TreeElement *te;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_COMBINED);
	te->name= "Combined";
	te->directdata= &srl->passflag;
	
	/* save cpu cycles, but we add the first to invoke an open/close triangle */
	if(tselem->flag & TSE_CLOSED)
		return;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_Z);
	te->name= "Z";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_VECTOR);
	te->name= "Vector";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_NORMAL);
	te->name= "Normal";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_UV);
	te->name= "UV";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_MIST);
	te->name= "Mist";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_INDEXOB);
	te->name= "Index Object";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_RGBA);
	te->name= "Color";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_DIFFUSE);
	te->name= "Diffuse";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_SPEC);
	te->name= "Specular";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_SHADOW);
	te->name= "Shadow";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_AO);
	te->name= "AO";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_REFLECT);
	te->name= "Reflection";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_REFRACT);
	te->name= "Refraction";
	te->directdata= &srl->passflag;
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_INDIRECT);
	te->name= "Indirect";
	te->directdata= &srl->passflag;

	/* TODO SCE_PASS_ENVIRONMENT/EMIT overflow short..

	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_ENVIRONMENT);
	te->name= "Environment";
	te->directdata= &srl->passflag;

	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_EMIT);
	te->name= "Emit";
	te->directdata= &srl->passflag;*/
}


/* special handling of hierarchical non-lib data */
static void outliner_add_bone(SpaceOops *soops, ListBase *lb, ID *id, Bone *curBone, 
							  TreeElement *parent, int *a)
{
	TreeElement *te= outliner_add_element(soops, lb, id, parent, TSE_BONE, *a);
	
	(*a)++;
	te->name= curBone->name;
	te->directdata= curBone;
	
	for(curBone= curBone->childbase.first; curBone; curBone=curBone->next) {
		outliner_add_bone(soops, &te->subtree, id, curBone, te, a);
	}
}

static void outliner_add_scene_contents(SpaceOops *soops, ListBase *lb, Scene *sce, TreeElement *te)
{
	SceneRenderLayer *srl;
	TreeElement *tenla= outliner_add_element(soops, lb, sce, te, TSE_R_LAYER_BASE, 0);
	int a;
	
	tenla->name= "RenderLayers";
	for(a=0, srl= sce->r.layers.first; srl; srl= srl->next, a++) {
		TreeElement *tenlay= outliner_add_element(soops, &tenla->subtree, sce, te, TSE_R_LAYER, a);
		tenlay->name= srl->name;
		tenlay->directdata= &srl->passflag;
		
		if(srl->light_override)
			outliner_add_element(soops, &tenlay->subtree, srl->light_override, tenlay, TSE_LINKED_LAMP, 0);
		if(srl->mat_override)
			outliner_add_element(soops, &tenlay->subtree, srl->mat_override, tenlay, TSE_LINKED_MAT, 0);
		
		outliner_add_passes(soops, tenlay, &sce->id, srl);
	}
	
	outliner_add_element(soops,  lb, sce->world, te, 0, 0);
}

static TreeElement *outliner_add_element(SpaceOops *soops, ListBase *lb, void *idv, 
										 TreeElement *parent, short type, short index)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	ID *id= idv;
	int a = 0;
	
	if(ELEM3(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		id= ((PointerRNA*)idv)->id.data;
		if(!id) id= ((PointerRNA*)idv)->data;
	}

	if(id==NULL) return NULL;

	te= MEM_callocN(sizeof(TreeElement), "tree elem");
	/* add to the visual tree */
	BLI_addtail(lb, te);
	/* add to the storage */
	check_persistant(soops, te, id, type, index);
	tselem= TREESTORE(te);	
	
	te->parent= parent;
	te->index= index;	// for data arays
	if(ELEM3(type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP));
	else if(ELEM3(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM));
	else if(type==TSE_ANIM_DATA);
	else {
		te->name= id->name+2; // default, can be overridden by Library or non-ID data
		te->idcode= GS(id->name);
	}
	
	if(type==0) {

		/* tuck pointer back in object, to construct hierarchy */
		if(GS(id->name)==ID_OB) id->newid= (ID *)te;
		
		/* expand specific data always */
		switch(GS(id->name)) {
		case ID_LI:
			te->name= ((Library *)id)->name;
			break;
		case ID_SCE:
			outliner_add_scene_contents(soops, &te->subtree, (Scene *)id, te);
			break;
		case ID_OB:
			{
				Object *ob= (Object *)id;
				
				outliner_add_element(soops, &te->subtree, ob->adt, te, TSE_ANIM_DATA, 0);
				outliner_add_element(soops, &te->subtree, ob->poselib, te, 0, 0); // XXX FIXME.. add a special type for this
				
				if(ob->proxy && ob->id.lib==NULL)
					outliner_add_element(soops, &te->subtree, ob->proxy, te, TSE_PROXY, 0);
				
				outliner_add_element(soops, &te->subtree, ob->data, te, 0, 0);
				
				if(ob->pose) {
					bArmature *arm= ob->data;
					bPoseChannel *pchan;
					TreeElement *ten;
					TreeElement *tenla= outliner_add_element(soops, &te->subtree, ob, te, TSE_POSE_BASE, 0);
					
					tenla->name= "Pose";
					
					if(arm->edbo==NULL && (ob->mode & OB_MODE_POSE)) {	// channels undefined in editmode, but we want the 'tenla' pose icon itself
						int a= 0, const_index= 1000;	/* ensure unique id for bone constraints */
						
						for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next, a++) {
							ten= outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_POSE_CHANNEL, a);
							ten->name= pchan->name;
							ten->directdata= pchan;
							pchan->prev= (bPoseChannel *)ten;
							
							if(pchan->constraints.first) {
								//Object *target;
								bConstraint *con;
								TreeElement *ten1;
								TreeElement *tenla1= outliner_add_element(soops, &ten->subtree, ob, ten, TSE_CONSTRAINT_BASE, 0);
								//char *str;
								
								tenla1->name= "Constraints";
								for(con= pchan->constraints.first; con; con= con->next, const_index++) {
									ten1= outliner_add_element(soops, &tenla1->subtree, ob, tenla1, TSE_CONSTRAINT, const_index);
#if 0 /* disabled as it needs to be reworked for recoded constraints system */
									target= get_constraint_target(con, &str);
									if(str && str[0]) ten1->name= str;
									else if(target) ten1->name= target->id.name+2;
									else ten1->name= con->name;
#endif
									ten1->name= con->name;
									ten1->directdata= con;
									/* possible add all other types links? */
								}
							}
						}
						/* make hierarchy */
						ten= tenla->subtree.first;
						while(ten) {
							TreeElement *nten= ten->next, *par;
							tselem= TREESTORE(ten);
							if(tselem->type==TSE_POSE_CHANNEL) {
								pchan= (bPoseChannel *)ten->directdata;
								if(pchan->parent) {
									BLI_remlink(&tenla->subtree, ten);
									par= (TreeElement *)pchan->parent->prev;
									BLI_addtail(&par->subtree, ten);
									ten->parent= par;
								}
							}
							ten= nten;
						}
						/* restore prev pointers */
						pchan= ob->pose->chanbase.first;
						if(pchan) pchan->prev= NULL;
						for(; pchan; pchan= pchan->next) {
							if(pchan->next) pchan->next->prev= pchan;
						}
					}
					
					/* Pose Groups */
					if(ob->pose->agroups.first) {
						bActionGroup *agrp;
						TreeElement *ten;
						TreeElement *tenla= outliner_add_element(soops, &te->subtree, ob, te, TSE_POSEGRP_BASE, 0);
						int a= 0;
						
						tenla->name= "Bone Groups";
						for (agrp=ob->pose->agroups.first; agrp; agrp=agrp->next, a++) {
							ten= outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_POSEGRP, a);
							ten->name= agrp->name;
							ten->directdata= agrp;
						}
					}
				}
				
				for(a=0; a<ob->totcol; a++) 
					outliner_add_element(soops, &te->subtree, ob->mat[a], te, 0, a);
				
				if(ob->constraints.first) {
					//Object *target;
					bConstraint *con;
					TreeElement *ten;
					TreeElement *tenla= outliner_add_element(soops, &te->subtree, ob, te, TSE_CONSTRAINT_BASE, 0);
					int a= 0;
					//char *str;
					
					tenla->name= "Constraints";
					for(con= ob->constraints.first; con; con= con->next, a++) {
						ten= outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_CONSTRAINT, a);
#if 0 /* disabled due to constraints system targets recode... code here needs review */
						target= get_constraint_target(con, &str);
						if(str && str[0]) ten->name= str;
						else if(target) ten->name= target->id.name+2;
						else ten->name= con->name;
#endif
						ten->name= con->name;
						ten->directdata= con;
						/* possible add all other types links? */
					}
				}
				
				if(ob->modifiers.first) {
					ModifierData *md;
					TreeElement *temod = outliner_add_element(soops, &te->subtree, ob, te, TSE_MODIFIER_BASE, 0);
					int index;

					temod->name = "Modifiers";
					for (index=0,md=ob->modifiers.first; md; index++,md=md->next) {
						TreeElement *te = outliner_add_element(soops, &temod->subtree, ob, temod, TSE_MODIFIER, index);
						te->name= md->name;
						te->directdata = md;

						if (md->type==eModifierType_Lattice) {
							outliner_add_element(soops, &te->subtree, ((LatticeModifierData*) md)->object, te, TSE_LINKED_OB, 0);
						} else if (md->type==eModifierType_Curve) {
							outliner_add_element(soops, &te->subtree, ((CurveModifierData*) md)->object, te, TSE_LINKED_OB, 0);
						} else if (md->type==eModifierType_Armature) {
							outliner_add_element(soops, &te->subtree, ((ArmatureModifierData*) md)->object, te, TSE_LINKED_OB, 0);
						} else if (md->type==eModifierType_Hook) {
							outliner_add_element(soops, &te->subtree, ((HookModifierData*) md)->object, te, TSE_LINKED_OB, 0);
						} else if (md->type==eModifierType_ParticleSystem) {
							TreeElement *ten;
							ParticleSystem *psys= ((ParticleSystemModifierData*) md)->psys;
							
							ten = outliner_add_element(soops, &te->subtree, ob, te, TSE_LINKED_PSYS, 0);
							ten->directdata = psys;
							ten->name = psys->part->id.name+2;
						}
					}
				}
				if(ob->defbase.first) {
					bDeformGroup *defgroup;
					TreeElement *ten;
					TreeElement *tenla= outliner_add_element(soops, &te->subtree, ob, te, TSE_DEFGROUP_BASE, 0);
					int a= 0;
					
					tenla->name= "Vertex Groups";
					for (defgroup=ob->defbase.first; defgroup; defgroup=defgroup->next, a++) {
						ten= outliner_add_element(soops, &tenla->subtree, ob, tenla, TSE_DEFGROUP, a);
						ten->name= defgroup->name;
						ten->directdata= defgroup;
					}
				}
				
				if(ob->dup_group)
					outliner_add_element(soops, &te->subtree, ob->dup_group, te, 0, 0);	
				
			}
			break;
		case ID_ME:
			{
				Mesh *me= (Mesh *)id;
				
				//outliner_add_element(soops, &te->subtree, me->adt, te, TSE_ANIM_DATA, 0);
				
				outliner_add_element(soops, &te->subtree, me->key, te, 0, 0);
				for(a=0; a<me->totcol; a++) 
					outliner_add_element(soops, &te->subtree, me->mat[a], te, 0, a);
				/* could do tfaces with image links, but the images are not grouped nicely.
				   would require going over all tfaces, sort images in use. etc... */
			}
			break;
		case ID_CU:
			{
				Curve *cu= (Curve *)id;
				
				outliner_add_element(soops, &te->subtree, cu->adt, te, TSE_ANIM_DATA, 0);
				
				for(a=0; a<cu->totcol; a++) 
					outliner_add_element(soops, &te->subtree, cu->mat[a], te, 0, a);
			}
			break;
		case ID_MB:
			{
				MetaBall *mb= (MetaBall *)id;
				for(a=0; a<mb->totcol; a++) 
					outliner_add_element(soops, &te->subtree, mb->mat[a], te, 0, a);
			}
			break;
		case ID_MA:
		{
			Material *ma= (Material *)id;
			
			outliner_add_element(soops, &te->subtree, ma->adt, te, TSE_ANIM_DATA, 0);
			
			for(a=0; a<MAX_MTEX; a++) {
				if(ma->mtex[a]) outliner_add_element(soops, &te->subtree, ma->mtex[a]->tex, te, 0, a);
			}
		}
			break;
		case ID_TE:
			{
				Tex *tex= (Tex *)id;
				
				outliner_add_element(soops, &te->subtree, tex->adt, te, TSE_ANIM_DATA, 0);
				outliner_add_element(soops, &te->subtree, tex->ima, te, 0, 0);
			}
			break;
		case ID_CA:
			{
				Camera *ca= (Camera *)id;
				outliner_add_element(soops, &te->subtree, ca->adt, te, TSE_ANIM_DATA, 0);
			}
			break;
		case ID_LA:
			{
				Lamp *la= (Lamp *)id;
				
				outliner_add_element(soops, &te->subtree, la->adt, te, TSE_ANIM_DATA, 0);
				
				for(a=0; a<MAX_MTEX; a++) {
					if(la->mtex[a]) outliner_add_element(soops, &te->subtree, la->mtex[a]->tex, te, 0, a);
				}
			}
			break;
		case ID_WO:
			{
				World *wrld= (World *)id;
				
				outliner_add_element(soops, &te->subtree, wrld->adt, te, TSE_ANIM_DATA, 0);
				
				for(a=0; a<MAX_MTEX; a++) {
					if(wrld->mtex[a]) outliner_add_element(soops, &te->subtree, wrld->mtex[a]->tex, te, 0, a);
				}
			}
			break;
		case ID_KE:
			{
				Key *key= (Key *)id;
				
				outliner_add_element(soops, &te->subtree, key->adt, te, TSE_ANIM_DATA, 0);
			}
			break;
		case ID_AC:
			{
				// XXX do we want to be exposing the F-Curves here?
				//bAction *act= (bAction *)id;
			}
			break;
		case ID_AR:
			{
				bArmature *arm= (bArmature *)id;
				int a= 0;
				
				if(arm->edbo) {
					EditBone *ebone;
					TreeElement *ten;
					
					for (ebone = arm->edbo->first; ebone; ebone=ebone->next, a++) {
						ten= outliner_add_element(soops, &te->subtree, id, te, TSE_EBONE, a);
						ten->directdata= ebone;
						ten->name= ebone->name;
						ebone->temp= ten;
					}
					/* make hierarchy */
					ten= te->subtree.first;
					while(ten) {
						TreeElement *nten= ten->next, *par;
						ebone= (EditBone *)ten->directdata;
						if(ebone->parent) {
							BLI_remlink(&te->subtree, ten);
							par= ebone->parent->temp;
							BLI_addtail(&par->subtree, ten);
							ten->parent= par;
						}
						ten= nten;
					}
				}
				else {
					/* do not extend Armature when we have posemode */
					tselem= TREESTORE(te->parent);
					if( GS(tselem->id->name)==ID_OB && ((Object *)tselem->id)->mode & OB_MODE_POSE);
					else {
						Bone *curBone;
						for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
							outliner_add_bone(soops, &te->subtree, id, curBone, te, &a);
						}
					}
				}
			}
			break;
		}
	}
	else if(type==TSE_ANIM_DATA) {
		AnimData *adt= (AnimData *)idv;
		
		/* this element's info */
		te->name= "Animation";
		
		/* Action */
		outliner_add_element(soops, &te->subtree, adt->action, te, 0, 0);
		
		/* Drivers */
		if (adt->drivers.first) {
			TreeElement *ted= outliner_add_element(soops, &te->subtree, adt, te, TSE_DRIVER_BASE, 0);
			ID *lastadded= NULL;
			FCurve *fcu;
			
			ted->name= "Drivers";
		
			for (fcu= adt->drivers.first; fcu; fcu= fcu->next) {
				if (fcu->driver && fcu->driver->variables.first)  {
					ChannelDriver *driver= fcu->driver;
					DriverVar *dvar;
					
					for (dvar= driver->variables.first; dvar; dvar= dvar->next) {
						/* loop over all targets used here */
						DRIVER_TARGETS_USED_LOOPER(dvar) 
						{
							if (lastadded != dtar->id) {
								// XXX this lastadded check is rather lame, and also fails quite badly...
								outliner_add_element(soops, &ted->subtree, dtar->id, ted, TSE_LINKED_OB, 0);
								lastadded= dtar->id;
							}
						}
						DRIVER_TARGETS_LOOPER_END
					}
				}
			}
		}
		
		/* NLA Data */
		if (adt->nla_tracks.first) {
			TreeElement *tenla= outliner_add_element(soops, &te->subtree, adt, te, TSE_NLA, 0);
			NlaTrack *nlt;
			int a= 0;
			
			tenla->name= "NLA Tracks";
			
			for (nlt= adt->nla_tracks.first; nlt; nlt= nlt->next) {
				TreeElement *tenlt= outliner_add_element(soops, &tenla->subtree, nlt, tenla, TSE_NLA_TRACK, a);
				NlaStrip *strip;
				TreeElement *ten;
				int b= 0;
				
				tenlt->name= nlt->name;
				
				for (strip=nlt->strips.first; strip; strip=strip->next, b++) {
					ten= outliner_add_element(soops, &tenlt->subtree, strip->act, tenlt, TSE_NLA_ACTION, b);
					if(ten) ten->directdata= strip;
				}
			}
		}
	}
	else if(type==TSE_SEQUENCE) {
		Sequence *seq= (Sequence*) idv;
		Sequence *p;

		/*
		 * The idcode is a little hack, but the outliner
		 * only check te->idcode if te->type is equal to zero,
		 * so this is "safe".
		 */
		te->idcode= seq->type;
		te->directdata= seq;

		if(seq->type<7) {
			/*
			 * This work like the sequence.
			 * If the sequence have a name (not default name)
			 * show it, in other case put the filename.
			 */
			if(strcmp(seq->name, "SQ"))
				te->name= seq->name;
			else {
				if((seq->strip) && (seq->strip->stripdata))
					te->name= seq->strip->stripdata->name;
				else if((seq->strip) && (seq->strip->tstripdata) && (seq->strip->tstripdata->ibuf))
					te->name= seq->strip->tstripdata->ibuf->name;
				else
					te->name= "SQ None";
			}

			if(seq->type==SEQ_META) {
				te->name= "Meta Strip";
				p= seq->seqbase.first;
				while(p) {
					outliner_add_element(soops, &te->subtree, (void*)p, te, TSE_SEQUENCE, index);
					p= p->next;
				}
			}
			else
				outliner_add_element(soops, &te->subtree, (void*)seq->strip, te, TSE_SEQ_STRIP, index);
		}
		else
			te->name= "Effect";
	}
	else if(type==TSE_SEQ_STRIP) {
		Strip *strip= (Strip *)idv;

		if(strip->dir)
			te->name= strip->dir;
		else
			te->name= "Strip None";
		te->directdata= strip;
	}
	else if(type==TSE_SEQUENCE_DUP) {
		Sequence *seq= (Sequence*)idv;

		te->idcode= seq->type;
		te->directdata= seq;
		te->name= seq->strip->stripdata->name;
	}
	else if(ELEM3(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
		PointerRNA pptr, propptr, *ptr= (PointerRNA*)idv;
		PropertyRNA *prop, *iterprop;
		PropertyType proptype;
		int a, tot;

		/* we do lazy build, for speed and to avoid infinite recusion */

		if(ptr->data == NULL) {
			te->name= "(empty)";
		}
		else if(type == TSE_RNA_STRUCT) {
			/* struct */
			te->name= RNA_struct_name_get_alloc(ptr, NULL, 0);

			if(te->name)
				te->flag |= TE_FREE_NAME;
			else
				te->name= (char*)RNA_struct_ui_name(ptr->type);

			iterprop= RNA_struct_iterator_property(ptr->type);
			tot= RNA_property_collection_length(ptr, iterprop);

			/* auto open these cases */
			if(!parent || (RNA_property_type(parent->directdata)) == PROP_POINTER)
				if(!tselem->used)
					tselem->flag &= ~TSE_CLOSED;

			if(!(tselem->flag & TSE_CLOSED)) {
				for(a=0; a<tot; a++)
					outliner_add_element(soops, &te->subtree, (void*)ptr, te, TSE_RNA_PROPERTY, a);
			}
			else if(tot)
				te->flag |= TE_LAZY_CLOSED;

			te->rnaptr= *ptr;
		}
		else if(type == TSE_RNA_PROPERTY) {
			/* property */
			iterprop= RNA_struct_iterator_property(ptr->type);
			RNA_property_collection_lookup_int(ptr, iterprop, index, &propptr);

			prop= propptr.data;
			proptype= RNA_property_type(prop);

			te->name= (char*)RNA_property_ui_name(prop);
			te->directdata= prop;
			te->rnaptr= *ptr;

			if(proptype == PROP_POINTER) {
				pptr= RNA_property_pointer_get(ptr, prop);

				if(pptr.data) {
					if(!(tselem->flag & TSE_CLOSED))
						outliner_add_element(soops, &te->subtree, (void*)&pptr, te, TSE_RNA_STRUCT, -1);
					else
						te->flag |= TE_LAZY_CLOSED;
				}
			}
			else if(proptype == PROP_COLLECTION) {
				tot= RNA_property_collection_length(ptr, prop);

				if(!(tselem->flag & TSE_CLOSED)) {
					for(a=0; a<tot; a++) {
						RNA_property_collection_lookup_int(ptr, prop, a, &pptr);
						outliner_add_element(soops, &te->subtree, (void*)&pptr, te, TSE_RNA_STRUCT, -1);
					}
				}
				else if(tot)
					te->flag |= TE_LAZY_CLOSED;
			}
			else if(ELEM3(proptype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
				tot= RNA_property_array_length(ptr, prop);

				if(!(tselem->flag & TSE_CLOSED)) {
					for(a=0; a<tot; a++)
						outliner_add_element(soops, &te->subtree, (void*)ptr, te, TSE_RNA_ARRAY_ELEM, a);
				}
				else if(tot)
					te->flag |= TE_LAZY_CLOSED;
			}
		}
		else if(type == TSE_RNA_ARRAY_ELEM) {
			char c;

			prop= parent->directdata;

			te->directdata= prop;
			te->rnaptr= *ptr;
			te->index= index;

			c= RNA_property_array_item_char(prop, index);

			te->name= MEM_callocN(sizeof(char)*20, "OutlinerRNAArrayName");
			if(c) sprintf(te->name, "  %c", c);
			else sprintf(te->name, "  %d", index+1);
			te->flag |= TE_FREE_NAME;
		}
	}
	else if(type == TSE_KEYMAP) {
		wmKeyMap *km= (wmKeyMap *)idv;
		wmKeyMapItem *kmi;
		char opname[OP_MAX_TYPENAME];
		
		te->directdata= idv;
		te->name= km->idname;
		
		if(!(tselem->flag & TSE_CLOSED)) {
			a= 0;
			
			for (kmi= km->items.first; kmi; kmi= kmi->next, a++) {
				const char *key= WM_key_event_string(kmi->type);
				
				if(key[0]) {
					wmOperatorType *ot= NULL;
					
					if(kmi->propvalue);
					else ot= WM_operatortype_find(kmi->idname, 0);
					
					if(ot || kmi->propvalue) {
						TreeElement *ten= outliner_add_element(soops, &te->subtree, kmi, te, TSE_KEYMAP_ITEM, a);
						
						ten->directdata= kmi;
						
						if(kmi->propvalue) {
							ten->name= "Modal map, not yet";
						}
						else {
							WM_operator_py_idname(opname, ot->idname);
							ten->name= BLI_strdup(opname);
							ten->flag |= TE_FREE_NAME;
						}
					}
				}
			}
		}
		else 
			te->flag |= TE_LAZY_CLOSED;
	}

	return te;
}

static void outliner_make_hierarchy(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te, *ten, *tep;
	TreeStoreElem *tselem;

	/* build hierarchy */
	// XXX also, set extents here...
	te= lb->first;
	while(te) {
		ten= te->next;
		tselem= TREESTORE(te);
		
		if(tselem->type==0 && te->idcode==ID_OB) {
			Object *ob= (Object *)tselem->id;
			if(ob->parent && ob->parent->id.newid) {
				BLI_remlink(lb, te);
				tep= (TreeElement *)ob->parent->id.newid;
				BLI_addtail(&tep->subtree, te);
				// set correct parent pointers
				for(te=tep->subtree.first; te; te= te->next) te->parent= tep;
			}
		}
		te= ten;
	}
}

/* Helped function to put duplicate sequence in the same tree. */
int need_add_seq_dup(Sequence *seq)
{
	Sequence *p;

	if((!seq->strip) || (!seq->strip->stripdata) || (!seq->strip->stripdata->name))
		return(1);

	/*
	 * First check backward, if we found a duplicate
	 * sequence before this, don't need it, just return.
	 */
	p= seq->prev;
	while(p) {
		if((!p->strip) || (!p->strip->stripdata) || (!p->strip->stripdata->name)) {
			p= p->prev;
			continue;
		}

		if(!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
			return(2);
		p= p->prev;
	}

	p= seq->next;
	while(p) {
		if((!p->strip) || (!p->strip->stripdata) || (!p->strip->stripdata->name)) {
			p= p->next;
			continue;
		}

		if(!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
			return(0);
		p= p->next;
	}
	return(1);
}

void add_seq_dup(SpaceOops *soops, Sequence *seq, TreeElement *te, short index)
{
	TreeElement *ch;
	Sequence *p;

	p= seq;
	while(p) {
		if((!p->strip) || (!p->strip->stripdata) || (!p->strip->stripdata->name)) {
			p= p->next;
			continue;
		}

		if(!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
			ch= outliner_add_element(soops, &te->subtree, (void*)p, te, TSE_SEQUENCE, index);
		p= p->next;
	}
}

static void outliner_build_tree(Main *mainvar, Scene *scene, SpaceOops *soops)
{
	Base *base;
	Object *ob;
	TreeElement *te=NULL, *ten;
	TreeStoreElem *tselem;
	int show_opened= (soops->treestore==NULL); /* on first view, we open scenes */

	if(soops->tree.first && (soops->storeflag & SO_TREESTORE_REDRAW))
	   return;
	   
	outliner_free_tree(&soops->tree);
	outliner_storage_cleanup(soops);
	
	/* clear ob id.new flags */
	for(ob= mainvar->object.first; ob; ob= ob->id.next) ob->id.newid= NULL;
	
	/* options */
	if(soops->outlinevis == SO_LIBRARIES) {
		Library *lib;
		
		for(lib= mainvar->library.first; lib; lib= lib->id.next) {
			ten= outliner_add_element(soops, &soops->tree, lib, NULL, 0, 0);
			lib->id.newid= (ID *)ten;
		}
		/* make hierarchy */
		ten= soops->tree.first;
		while(ten) {
			TreeElement *nten= ten->next, *par;
			tselem= TREESTORE(ten);
			lib= (Library *)tselem->id;
			if(lib->parent) {
				BLI_remlink(&soops->tree, ten);
				par= (TreeElement *)lib->parent->id.newid;
				BLI_addtail(&par->subtree, ten);
				ten->parent= par;
			}
			ten= nten;
		}
		/* restore newid pointers */
		for(lib= mainvar->library.first; lib; lib= lib->id.next)
			lib->id.newid= NULL;
		
	}
	else if(soops->outlinevis == SO_ALL_SCENES) {
		Scene *sce;
		for(sce= mainvar->scene.first; sce; sce= sce->id.next) {
			te= outliner_add_element(soops, &soops->tree, sce, NULL, 0, 0);
			tselem= TREESTORE(te);
			if(sce==scene && show_opened) 
				tselem->flag &= ~TSE_CLOSED;
			
			for(base= sce->base.first; base; base= base->next) {
				ten= outliner_add_element(soops, &te->subtree, base->object, te, 0, 0);
				ten->directdata= base;
			}
			outliner_make_hierarchy(soops, &te->subtree);
			/* clear id.newid, to prevent objects be inserted in wrong scenes (parent in other scene) */
			for(base= sce->base.first; base; base= base->next) base->object->id.newid= NULL;
		}
	}
	else if(soops->outlinevis == SO_CUR_SCENE) {
		
		outliner_add_scene_contents(soops, &soops->tree, scene, NULL);
		
		for(base= scene->base.first; base; base= base->next) {
			ten= outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
			ten->directdata= base;
		}
		outliner_make_hierarchy(soops, &soops->tree);
	}
	else if(soops->outlinevis == SO_VISIBLE) {
		for(base= scene->base.first; base; base= base->next) {
			if(base->lay & scene->lay)
				outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
		}
		outliner_make_hierarchy(soops, &soops->tree);
	}
	else if(soops->outlinevis == SO_GROUPS) {
		Group *group;
		GroupObject *go;
		
		for(group= mainvar->group.first; group; group= group->id.next) {
			if(group->id.us) {
				te= outliner_add_element(soops, &soops->tree, group, NULL, 0, 0);
				tselem= TREESTORE(te);
				
				for(go= group->gobject.first; go; go= go->next) {
					ten= outliner_add_element(soops, &te->subtree, go->ob, te, 0, 0);
					ten->directdata= NULL; /* eh, why? */
				}
				outliner_make_hierarchy(soops, &te->subtree);
				/* clear id.newid, to prevent objects be inserted in wrong scenes (parent in other scene) */
				for(go= group->gobject.first; go; go= go->next) go->ob->id.newid= NULL;
			}
		}
	}
	else if(soops->outlinevis == SO_SAME_TYPE) {
		Object *ob= OBACT;
		if(ob) {
			for(base= scene->base.first; base; base= base->next) {
				if(base->object->type==ob->type) {
					ten= outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
					ten->directdata= base;
				}
			}
			outliner_make_hierarchy(soops, &soops->tree);
		}
	}
	else if(soops->outlinevis == SO_SELECTED) {
		for(base= scene->base.first; base; base= base->next) {
			if(base->lay & scene->lay) {
				if(base==BASACT || (base->flag & SELECT)) {
					ten= outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
					ten->directdata= base;
				}
			}
		}
		outliner_make_hierarchy(soops, &soops->tree);
	}
	else if(soops->outlinevis==SO_SEQUENCE) {
		Sequence *seq;
		Editing *ed= seq_give_editing(scene, FALSE);
		int op;

		if(ed==NULL)
			return;

		seq= ed->seqbasep->first;
		if(!seq)
			return;

		while(seq) {
			op= need_add_seq_dup(seq);
			if(op==1)
				ten= outliner_add_element(soops, &soops->tree, (void*)seq, NULL, TSE_SEQUENCE, 0);
			else if(op==0) {
				ten= outliner_add_element(soops, &soops->tree, (void*)seq, NULL, TSE_SEQUENCE_DUP, 0);
				add_seq_dup(soops, seq, ten, 0);
			}
			seq= seq->next;
		}
	}
	else if(soops->outlinevis==SO_DATABLOCKS) {
		PointerRNA mainptr;

		RNA_main_pointer_create(mainvar, &mainptr);

		ten= outliner_add_element(soops, &soops->tree, (void*)&mainptr, NULL, TSE_RNA_STRUCT, -1);

		if(show_opened)  {
			tselem= TREESTORE(ten);
			tselem->flag &= ~TSE_CLOSED;
		}
	}
	else if(soops->outlinevis==SO_USERDEF) {
		PointerRNA userdefptr;

		RNA_pointer_create(NULL, &RNA_UserPreferences, &U, &userdefptr);

		ten= outliner_add_element(soops, &soops->tree, (void*)&userdefptr, NULL, TSE_RNA_STRUCT, -1);

		if(show_opened)  {
			tselem= TREESTORE(ten);
			tselem->flag &= ~TSE_CLOSED;
		}
	}
	else if(soops->outlinevis==SO_KEYMAP) {
		wmWindowManager *wm= mainvar->wm.first;
		wmKeyMap *km;
		
		for(km= wm->defaultconf->keymaps.first; km; km= km->next) {
			ten= outliner_add_element(soops, &soops->tree, (void*)km, NULL, TSE_KEYMAP, 0);
		}
	}
	else {
		ten= outliner_add_element(soops, &soops->tree, OBACT, NULL, 0, 0);
		if(ten) ten->directdata= BASACT;
	}

	outliner_sort(soops, &soops->tree);
}

/* **************** INTERACTIVE ************* */

static int outliner_count_levels(SpaceOops *soops, ListBase *lb, int curlevel)
{
	TreeElement *te;
	int level=curlevel, lev;
	
	for(te= lb->first; te; te= te->next) {
		
		lev= outliner_count_levels(soops, &te->subtree, curlevel+1);
		if(lev>level) level= lev;
	}
	return level;
}

static int outliner_has_one_flag(SpaceOops *soops, ListBase *lb, short flag, short curlevel)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int level;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->flag & flag) return curlevel;
		
		level= outliner_has_one_flag(soops, &te->subtree, flag, curlevel+1);
		if(level) return level;
	}
	return 0;
}

static void outliner_set_flag(SpaceOops *soops, ListBase *lb, short flag, short set)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(set==0) tselem->flag &= ~flag;
		else tselem->flag |= flag;
		outliner_set_flag(soops, &te->subtree, flag, set);
	}
}

/* --- */

void object_toggle_visibility_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, scene);
	if(base) {
		base->object->restrictflag^=OB_RESTRICT_VIEW;
	}
}

static int outliner_toggle_visibility_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	
	outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_visibility_cb);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_visibility_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Visability";
	ot->idname= "OUTLINER_OT_visibility_toggle";
	ot->description= "Toggle the visibility of selected items";
	
	/* callbacks */
	ot->exec= outliner_toggle_visibility_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* --- */

static void object_toggle_selectability_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, scene);
	if(base) {
		base->object->restrictflag^=OB_RESTRICT_SELECT;
	}
}

static int outliner_toggle_selectability_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	
	outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_selectability_cb);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_selectability_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Selectability";
	ot->idname= "OUTLINER_OT_selectability_toggle";
	ot->description= "Toggle the selectability";
	
	/* callbacks */
	ot->exec= outliner_toggle_selectability_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* --- */

void object_toggle_renderability_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, scene);
	if(base) {
		base->object->restrictflag^=OB_RESTRICT_RENDER;
	}
}

static int outliner_toggle_renderability_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	
	outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_renderability_cb);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_renderability_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Renderability";
	ot->idname= "OUTLINER_OT_renderability_toggle";
	ot->description= "Toggle the renderbility of selected items";
	
	/* callbacks */
	ot->exec= outliner_toggle_renderability_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* --- */

static int outliner_toggle_expanded_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	ARegion *ar= CTX_wm_region(C);
	
	if (outliner_has_one_flag(soops, &soops->tree, TSE_CLOSED, 1))
		outliner_set_flag(soops, &soops->tree, TSE_CLOSED, 0);
	else 
		outliner_set_flag(soops, &soops->tree, TSE_CLOSED, 1);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_expanded_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Expand/Collapse All";
	ot->idname= "OUTLINER_OT_expanded_toggle";
	ot->description= "Expand/Collapse all items";
	
	/* callbacks */
	ot->exec= outliner_toggle_expanded_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* --- */

static int outliner_toggle_selected_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	ARegion *ar= CTX_wm_region(C);
	
	if (outliner_has_one_flag(soops, &soops->tree, TSE_SELECTED, 1))
		outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
	else 
		outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 1);
	
	soops->storeflag |= SO_TREESTORE_REDRAW;
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_selected_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toggle Selected";
	ot->idname= "OUTLINER_OT_selected_toggle";
	ot->description= "Toggle the Outliner selection of items";
	
	/* callbacks */
	ot->exec= outliner_toggle_selected_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* --- */

/* helper function for Show/Hide one level operator */
static void outliner_openclose_level(SpaceOops *soops, ListBase *lb, int curlevel, int level, int open)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		if(open) {
			if(curlevel<=level) tselem->flag &= ~TSE_CLOSED;
		}
		else {
			if(curlevel>=level) tselem->flag |= TSE_CLOSED;
		}
		
		outliner_openclose_level(soops, &te->subtree, curlevel+1, level, open);
	}
}

static int outliner_one_level_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	ARegion *ar= CTX_wm_region(C);
	int add= RNA_boolean_get(op->ptr, "open");
	int level;
	
	level= outliner_has_one_flag(soops, &soops->tree, TSE_CLOSED, 1);
	if(add==1) {
		if(level) outliner_openclose_level(soops, &soops->tree, 1, level, 1);
	}
	else {
		if(level==0) level= outliner_count_levels(soops, &soops->tree, 0);
		if(level) outliner_openclose_level(soops, &soops->tree, 1, level-1, 0);
	}
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_one_level(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Show/Hide One Level";
	ot->idname= "OUTLINER_OT_show_one_level";
	
	/* callbacks */
	ot->exec= outliner_one_level_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "open", 1, "Open", "Expand all entries one level deep.");
}

/* return 1 when levels were opened */
static int outliner_open_back(SpaceOops *soops, TreeElement *te)
{
	TreeStoreElem *tselem;
	int retval= 0;
	
	for (te= te->parent; te; te= te->parent) {
		tselem= TREESTORE(te);
		if (tselem->flag & TSE_CLOSED) { 
			tselem->flag &= ~TSE_CLOSED;
			retval= 1;
		}
	}
	return retval;
}

/* This is not used anywhere at the moment */
#if 0
static void outliner_open_reveal(SpaceOops *soops, ListBase *lb, TreeElement *teFind, int *found)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te= lb->first; te; te= te->next) {
		/* check if this tree-element was the one we're seeking */
		if (te == teFind) {
			*found= 1;
			return;
		}
		
		/* try to see if sub-tree contains it then */
		outliner_open_reveal(soops, &te->subtree, teFind, found);
		if (*found) {
			tselem= TREESTORE(te);
			if (tselem->flag & TSE_CLOSED) 
				tselem->flag &= ~TSE_CLOSED;
			return;
		}
	}
}
#endif

// XXX just use View2D ops for this?
void outliner_page_up_down(Scene *scene, ARegion *ar, SpaceOops *soops, int up)
{
	int dy= ar->v2d.mask.ymax-ar->v2d.mask.ymin;
	
	if(up == -1) dy= -dy;
	ar->v2d.cur.ymin+= dy;
	ar->v2d.cur.ymax+= dy;
	
	soops->storeflag |= SO_TREESTORE_REDRAW;
}

/* **** do clicks on items ******* */

static int tree_element_active_renderlayer(bContext *C, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Scene *sce;
	
	/* paranoia check */
	if(te->idcode!=ID_SCE)
		return 0;
	sce= (Scene *)tselem->id;
	
	if(set) {
		sce->r.actlay= tselem->nr;
		WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, sce);
	}
	else {
		return sce->r.actlay==tselem->nr;
	}
	return 0;
}

static void tree_element_set_active_object(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeStoreElem *tselem= TREESTORE(te);
	Scene *sce;
	Base *base;
	Object *ob= NULL;
	
	/* if id is not object, we search back */
	if(te->idcode==ID_OB) ob= (Object *)tselem->id;
	else {
		ob= (Object *)outliner_search_back(soops, te, ID_OB);
		if(ob==OBACT) return;
	}
	if(ob==NULL) return;
	
	sce= (Scene *)outliner_search_back(soops, te, ID_SCE);
	if(sce && scene != sce) {
		ED_screen_set_scene(C, sce);
	}
	
	/* find associated base in current scene */
	for(base= FIRSTBASE; base; base= base->next) 
		if(base->object==ob) break;
	if(base) {
		if(set==2) {
			/* swap select */
			if(base->flag & SELECT)
				ED_base_object_select(base, BA_DESELECT);
			else 
				ED_base_object_select(base, BA_SELECT);
		}
		else {
			Base *b;
			/* deleselect all */
			for(b= FIRSTBASE; b; b= b->next) {
				b->flag &= ~SELECT;
				b->object->flag= b->flag;
			}
			ED_base_object_select(base, BA_SELECT);
		}
		if(C)
			ED_base_object_activate(C, base); /* adds notifier */
	}
	
	if(ob!=scene->obedit) 
		ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO);
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, scene);

}

static int tree_element_active_material(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tes;
	Object *ob;
	
	/* we search for the object parent */
	ob= (Object *)outliner_search_back(soops, te, ID_OB);
	if(ob==NULL || ob!=OBACT) return 0;	// just paranoia
	
	/* searching in ob mat array? */
	tes= te->parent;
	if(tes->idcode==ID_OB) {
		if(set) {
			ob->actcol= te->index+1;
			ob->matbits[te->index]= 1;	// make ob material active too
			ob->colbits |= (1<<te->index);
		}
		else {
			if(ob->actcol == te->index+1) 
				if(ob->matbits[te->index]) return 1;
		}
	}
	/* or we search for obdata material */
	else {
		if(set) {
			ob->actcol= te->index+1;
			ob->matbits[te->index]= 0;	// make obdata material active too
			ob->colbits &= ~(1<<te->index);
		}
		else {
			if(ob->actcol == te->index+1)
				if(ob->matbits[te->index]==0) return 1;
		}
	}
	if(set) {
		WM_event_add_notifier(C, NC_MATERIAL|ND_SHADING, NULL);
	}
	return 0;
}

static int tree_element_active_texture(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tep;
	TreeStoreElem *tselem, *tselemp;
	Object *ob=OBACT;
	SpaceButs *sbuts=NULL;
	
	if(ob==NULL) return 0; // no active object
	
	tselem= TREESTORE(te);
	
	/* find buttons area (note, this is undefined really still, needs recode in blender) */
	/* XXX removed finding sbuts */
	
	/* where is texture linked to? */
	tep= te->parent;
	tselemp= TREESTORE(tep);
	
	if(tep->idcode==ID_WO) {
		World *wrld= (World *)tselemp->id;

		if(set) {
			if(sbuts) {
				// XXX sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				// XXX sbuts->texfrom= 1;
			}
// XXX			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			wrld->texact= te->index;
		}
		else if(tselemp->id == (ID *)(scene->world)) {
			if(wrld->texact==te->index) return 1;
		}
	}
	else if(tep->idcode==ID_LA) {
		Lamp *la= (Lamp *)tselemp->id;
		if(set) {
			if(sbuts) {
				// XXX sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				// XXX sbuts->texfrom= 2;
			}
// XXX			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			la->texact= te->index;
		}
		else {
			if(tselemp->id == ob->data) {
				if(la->texact==te->index) return 1;
			}
		}
	}
	else if(tep->idcode==ID_MA) {
		Material *ma= (Material *)tselemp->id;
		if(set) {
			if(sbuts) {
				//sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				// XXX sbuts->texfrom= 0;
			}
// XXX			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			ma->texact= (char)te->index;
			
			/* also set active material */
			ob->actcol= tep->index+1;
		}
		else if(tep->flag & TE_ACTIVE) {	// this is active material
			if(ma->texact==te->index) return 1;
		}
	}
	
	return 0;
}


static int tree_element_active_lamp(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	Object *ob;
	
	/* we search for the object parent */
	ob= (Object *)outliner_search_back(soops, te, ID_OB);
	if(ob==NULL || ob!=OBACT) return 0;	// just paranoia
	
	if(set) {
// XXX		extern_set_butspace(F5KEY, 0);
	}
	else return 1;
	
	return 0;
}

static int tree_element_active_world(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tep;
	TreeStoreElem *tselem=NULL;
	Scene *sce=NULL;
	
	tep= te->parent;
	if(tep) {
		tselem= TREESTORE(tep);
		sce= (Scene *)tselem->id;
	}
	
	if(set) {	// make new scene active
		if(sce && scene != sce) {
			ED_screen_set_scene(C, sce);
		}
	}
	
	if(tep==NULL || tselem->id == (ID *)scene) {
		if(set) {
// XXX			extern_set_butspace(F8KEY, 0);
		}
		else {
			return 1;
		}
	}
	return 0;
}

static int tree_element_active_defgroup(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob;
	
	/* id in tselem is object */
	ob= (Object *)tselem->id;
	if(set) {
		ob->actdef= te->index+1;
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	}
	else {
		if(ob==OBACT)
			if(ob->actdef== te->index+1) return 1;
	}
	return 0;
}

static int tree_element_active_posegroup(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	
	if(set) {
		if (ob->pose) {
			ob->pose->active_group= te->index+1;
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
		}
	}
	else {
		if(ob==OBACT && ob->pose) {
			if (ob->pose->active_group== te->index+1) return 1;
		}
	}
	return 0;
}

static int tree_element_active_posechannel(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	bArmature *arm= ob->data;
	bPoseChannel *pchan= te->directdata;
	
	if(set) {
		if(!(pchan->bone->flag & BONE_HIDDEN_P)) {
			
			if(set==2) ED_pose_deselectall(ob, 2, 0);	// 2 = clear active tag
			else ED_pose_deselectall(ob, 0, 0);	// 0 = deselect 
			
			if(set==2 && (pchan->bone->flag & BONE_SELECTED)) {
				pchan->bone->flag &= ~BONE_SELECTED;
				if(arm->act_bone==pchan->bone)
					arm->act_bone= NULL;
			} else {
				pchan->bone->flag |= BONE_SELECTED;
				arm->act_bone= pchan->bone;
			}
			
			WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, ob);

		}
	}
	else {
		if(ob==OBACT && ob->pose) {
			if (pchan->bone->flag & BONE_SELECTED) return 1;
		}
	}
	return 0;
}

static int tree_element_active_bone(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	bArmature *arm= (bArmature *)tselem->id;
	Bone *bone= te->directdata;
	
	if(set) {
		if(!(bone->flag & BONE_HIDDEN_P)) {
			if(set==2) ED_pose_deselectall(OBACT, 2, 0);	// 2 is clear active tag
			else ED_pose_deselectall(OBACT, 0, 0);
			
			if(set==2 && (bone->flag & BONE_SELECTED)) {
				bone->flag &= ~BONE_SELECTED;
				if(arm->act_bone==bone)
					arm->act_bone= NULL;
			} else {
				bone->flag |= BONE_SELECTED;
				arm->act_bone= bone;
			}
			
			WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, OBACT);
		}
	}
	else {
		Object *ob= OBACT;
		
		if(ob && ob->data==arm) {
			if (bone->flag & BONE_SELECTED) return 1;
		}
	}
	return 0;
}


/* ebones only draw in editmode armature */
static int tree_element_active_ebone(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	EditBone *ebone= te->directdata;
	
	if(set) {
		if(!(ebone->flag & BONE_HIDDEN_A)) {
			bArmature *arm= scene->obedit->data;
			if(set==2) ED_armature_deselectall(scene->obedit, 2, 0);	// only clear active tag
			else ED_armature_deselectall(scene->obedit, 0, 0);	// deselect

			ebone->flag |= BONE_SELECTED|BONE_ROOTSEL|BONE_TIPSEL;
			arm->act_edbone= ebone;

			// flush to parent?
			if(ebone->parent && (ebone->flag & BONE_CONNECTED)) ebone->parent->flag |= BONE_TIPSEL;
			
			WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, scene->obedit);
		}
	}
	else {
		if (ebone->flag & BONE_SELECTED) return 1;
	}
	return 0;
}

static int tree_element_active_modifier(bContext *C, TreeElement *te, TreeStoreElem *tselem, int set)
{
	if(set) {
		Object *ob= (Object *)tselem->id;
		
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);

// XXX		extern_set_butspace(F9KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_psys(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	if(set) {
		Object *ob= (Object *)tselem->id;
		
		WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE_DATA, ob);
		
// XXX		extern_set_butspace(F7KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_constraint(bContext *C, TreeElement *te, TreeStoreElem *tselem, int set)
{
	if(set) {
		Object *ob= (Object *)tselem->id;
		
		WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
// XXX		extern_set_butspace(F7KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_text(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	// XXX removed
	return 0;
}

/* generic call for ID data check or make/check active in UI */
static int tree_element_active(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{

	switch(te->idcode) {
		case ID_MA:
			return tree_element_active_material(C, scene, soops, te, set);
		case ID_WO:
			return tree_element_active_world(C, scene, soops, te, set);
		case ID_LA:
			return tree_element_active_lamp(C, scene, soops, te, set);
		case ID_TE:
			return tree_element_active_texture(C, scene, soops, te, set);
		case ID_TXT:
			return tree_element_active_text(C, scene, soops, te, set);
	}
	return 0;
}

static int tree_element_active_pose(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	Base *base= object_in_scene(ob, scene);
	
	if(set) {
		if(scene->obedit) 
			ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO);
		
		if(ob->mode & OB_MODE_POSE) 
			ED_armature_exit_posemode(C, base);
		else 
			ED_armature_enter_posemode(C, base);
	}
	else {
		if(ob->mode & OB_MODE_POSE) return 1;
	}
	return 0;
}

static int tree_element_active_sequence(bContext *C, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Sequence *seq= (Sequence*) te->directdata;

	if(set) {
// XXX		select_single_seq(seq, 1);
	}
	else {
		if(seq->flag & SELECT)
			return(1);
	}
	return(0);
}

static int tree_element_active_sequence_dup(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Sequence *seq, *p;
	Editing *ed= seq_give_editing(scene, FALSE);

	seq= (Sequence*)te->directdata;
	if(set==0) {
		if(seq->flag & SELECT)
			return(1);
		return(0);
	}

// XXX	select_single_seq(seq, 1);
	p= ed->seqbasep->first;
	while(p) {
		if((!p->strip) || (!p->strip->stripdata) || (!p->strip->stripdata->name)) {
			p= p->next;
			continue;
		}

//		if(!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
// XXX			select_single_seq(p, 0);
		p= p->next;
	}
	return(0);
}

static int tree_element_active_keymap_item(bContext *C, TreeElement *te, TreeStoreElem *tselem, int set)
{
	wmKeyMapItem *kmi= te->directdata;
	
	if(set==0) {
		if(kmi->flag & KMI_INACTIVE) return 0;
		return 1;
	}
	else {
		kmi->flag ^= KMI_INACTIVE;
	}
	return 0;
}


/* generic call for non-id data to make/check active in UI */
/* Context can be NULL when set==0 */
static int tree_element_type_active(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, int set)
{
	
	switch(tselem->type) {
		case TSE_DEFGROUP:
			return tree_element_active_defgroup(C, scene, te, tselem, set);
		case TSE_BONE:
			return tree_element_active_bone(C, scene, te, tselem, set);
		case TSE_EBONE:
			return tree_element_active_ebone(C, scene, te, tselem, set);
		case TSE_MODIFIER:
			return tree_element_active_modifier(C, te, tselem, set);
		case TSE_LINKED_OB:
			if(set) tree_element_set_active_object(C, scene, soops, te, set);
			else if(tselem->id==(ID *)OBACT) return 1;
			break;
		case TSE_LINKED_PSYS:
			return tree_element_active_psys(C, scene, te, tselem, set);
		case TSE_POSE_BASE:
			return tree_element_active_pose(C, scene, te, tselem, set);
		case TSE_POSE_CHANNEL:
			return tree_element_active_posechannel(C, scene, te, tselem, set);
		case TSE_CONSTRAINT:
			return tree_element_active_constraint(C, te, tselem, set);
		case TSE_R_LAYER:
			return tree_element_active_renderlayer(C, te, tselem, set);
		case TSE_POSEGRP:
			return tree_element_active_posegroup(C, scene, te, tselem, set);
		case TSE_SEQUENCE:
			return tree_element_active_sequence(C, te, tselem, set);
		case TSE_SEQUENCE_DUP:
			return tree_element_active_sequence_dup(C, scene, te, tselem, set);
		case TSE_KEYMAP_ITEM:
			return tree_element_active_keymap_item(C, te, tselem, set);
			
	}
	return 0;
}

static int do_outliner_item_activate(bContext *C, Scene *scene, ARegion *ar, SpaceOops *soops, TreeElement *te, int extend, float *mval)
{
	
	if(mval[1]>te->ys && mval[1]<te->ys+OL_H) {
		TreeStoreElem *tselem= TREESTORE(te);
		int openclose= 0;

		/* open close icon */
		if((te->flag & TE_ICONROW)==0) {				// hidden icon, no open/close
			if( mval[0]>te->xs && mval[0]<te->xs+OL_X) 
				openclose= 1;
		}

		if(openclose) {
			/* all below close/open? */
			if(extend) {
				tselem->flag &= ~TSE_CLOSED;
				outliner_set_flag(soops, &te->subtree, TSE_CLOSED, !outliner_has_one_flag(soops, &te->subtree, TSE_CLOSED, 1));
			}
			else {
				if(tselem->flag & TSE_CLOSED) tselem->flag &= ~TSE_CLOSED;
				else tselem->flag |= TSE_CLOSED;
				
			}

			return 1;
		}
		/* name and first icon */
		else if(mval[0]>te->xs && mval[0]<te->xend) {
			
			/* always makes active object */
			if(tselem->type!=TSE_SEQUENCE && tselem->type!=TSE_SEQ_STRIP && tselem->type!=TSE_SEQUENCE_DUP)
				tree_element_set_active_object(C, scene, soops, te, 1 + (extend!=0 && tselem->type==0));
			
			if(tselem->type==0) { // the lib blocks
				/* editmode? */
				if(te->idcode==ID_SCE) {
					if(scene!=(Scene *)tselem->id) {
						ED_screen_set_scene(C, (Scene *)tselem->id);
					}
				}
				else if(ELEM5(te->idcode, ID_ME, ID_CU, ID_MB, ID_LT, ID_AR)) {
					Object *obedit= CTX_data_edit_object(C);
					if(obedit) 
						ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO);
					else {
						ED_object_enter_editmode(C, EM_WAITCURSOR);
						// XXX extern_set_butspace(F9KEY, 0);
					}
				} else {	// rest of types
					tree_element_active(C, scene, soops, te, 1);
				}
				
			}
			else tree_element_type_active(C, scene, soops, te, tselem, 1+(extend!=0));

			return 1;
		}
	}
	
	for(te= te->subtree.first; te; te= te->next) {
		if(do_outliner_item_activate(C, scene, ar, soops, te, extend, mval)) return 1;
	}
	return 0;
}

/* event can enterkey, then it opens/closes */
static int outliner_item_activate(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	int extend= RNA_boolean_get(op->ptr, "extend");
	
	UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, fmval, fmval+1);
	
	for(te= soops->tree.first; te; te= te->next) {
		if(do_outliner_item_activate(C, scene, ar, soops, te, extend, fmval)) break;
	}
	
	if(te) {
		ED_undo_push(C, "Outliner click event");
	}
	else {
		short selecting= -1;
		int row;
		
		/* get row number - 100 here is just a dummy value since we don't need the column */
		UI_view2d_listview_view_to_cell(&ar->v2d, 1000, OL_H, 0.0f, 0.0f, 
						fmval[0], fmval[1], NULL, &row);
		
		/* select relevant row */
		outliner_select(soops, &soops->tree, &row, &selecting);
		
		soops->storeflag |= SO_TREESTORE_REDRAW;
		
		ED_undo_push(C, "Outliner selection event");
	}
	
	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_item_activate(wmOperatorType *ot)
{
	ot->name= "Activate Item";
	ot->idname= "OUTLINER_OT_item_activate";
	
	ot->invoke= outliner_item_activate;
	
	ot->poll= ED_operator_outliner_active;
	
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection for activation.");
}

/* *********** */

static int do_outliner_item_openclose(bContext *C, SpaceOops *soops, TreeElement *te, int all, float *mval)
{
	
	if(mval[1]>te->ys && mval[1]<te->ys+OL_H) {
		TreeStoreElem *tselem= TREESTORE(te);

		/* all below close/open? */
		if(all) {
			tselem->flag &= ~TSE_CLOSED;
			outliner_set_flag(soops, &te->subtree, TSE_CLOSED, !outliner_has_one_flag(soops, &te->subtree, TSE_CLOSED, 1));
		}
		else {
			if(tselem->flag & TSE_CLOSED) tselem->flag &= ~TSE_CLOSED;
			else tselem->flag |= TSE_CLOSED;
		}

		return 1;
	}
	
	for(te= te->subtree.first; te; te= te->next) {
		if(do_outliner_item_openclose(C, soops, te, all, mval)) 
			return 1;
	}
	return 0;
	
}

/* event can enterkey, then it opens/closes */
static int outliner_item_openclose(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	int all= RNA_boolean_get(op->ptr, "all");
	
	UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, fmval, fmval+1);
	
	for(te= soops->tree.first; te; te= te->next) {
		if(do_outliner_item_openclose(C, soops, te, all, fmval)) 
			break;
	}

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_item_openclose(wmOperatorType *ot)
{
	ot->name= "Open/Close Item";
	ot->idname= "OUTLINER_OT_item_openclose";
	
	ot->invoke= outliner_item_openclose;
	
	ot->poll= ED_operator_outliner_active;
	
	RNA_def_boolean(ot->srna, "all", 1, "All", "Close or open all items.");

}


/* ********************************************** */

static int do_outliner_item_rename(bContext *C, ARegion *ar, SpaceOops *soops, TreeElement *te, float *mval)
{
	
	if(mval[1]>te->ys && mval[1]<te->ys+OL_H) {
		TreeStoreElem *tselem= TREESTORE(te);
		
		/* name and first icon */
		if(mval[0]>te->xs && mval[0]<te->xend) {
			
			/* can't rename rna datablocks entries */
			if(ELEM3(tselem->type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM))
			   ;
			else if(ELEM10(tselem->type, TSE_ANIM_DATA, TSE_NLA, TSE_DEFGROUP_BASE, TSE_CONSTRAINT_BASE, TSE_MODIFIER_BASE, TSE_SCRIPT_BASE, TSE_POSE_BASE, TSE_POSEGRP_BASE, TSE_R_LAYER_BASE, TSE_R_PASS)) 
					error("Cannot edit builtin name");
			else if(ELEM3(tselem->type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP))
				error("Cannot edit sequence name");
			else if(tselem->id->lib) {
				// XXX						error_libdata();
			} else if(te->idcode == ID_LI && te->parent) {
				error("Cannot edit the path of an indirectly linked library");
			} else {
				tselem->flag |= TSE_TEXTBUT;
				ED_region_tag_redraw(ar);
			}
		}
		return 1;
	}
	
	for(te= te->subtree.first; te; te= te->next) {
		if(do_outliner_item_rename(C, ar, soops, te, mval)) return 1;
	}
	return 0;
}

static int outliner_item_rename(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	
	UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, fmval, fmval+1);
	
	for(te= soops->tree.first; te; te= te->next) {
		if(do_outliner_item_rename(C, ar, soops, te, fmval)) break;
	}
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_item_rename(wmOperatorType *ot)
{
	ot->name= "Rename Item";
	ot->idname= "OUTLINER_OT_item_rename";
	
	ot->invoke= outliner_item_rename;
	
	ot->poll= ED_operator_outliner_active;
}



/* recursive helper for function below */
static void outliner_set_coordinates_element(SpaceOops *soops, TreeElement *te, int startx, int *starty)
{
	TreeStoreElem *tselem= TREESTORE(te);
	
	/* store coord and continue, we need coordinates for elements outside view too */
	te->xs= (float)startx;
	te->ys= (float)(*starty);
	*starty-= OL_H;
	
	if((tselem->flag & TSE_CLOSED)==0) {
		TreeElement *ten;
		for(ten= te->subtree.first; ten; ten= ten->next) {
			outliner_set_coordinates_element(soops, ten, startx+OL_X, starty);
		}
	}
	
}

/* to retrieve coordinates with redrawing the entire tree */
static void outliner_set_coordinates(ARegion *ar, SpaceOops *soops)
{
	TreeElement *te;
	int starty= (int)(ar->v2d.tot.ymax)-OL_H;
	int startx= 0;
	
	for(te= soops->tree.first; te; te= te->next) {
		outliner_set_coordinates_element(soops, te, startx, &starty);
	}
}

static TreeElement *outliner_find_id(SpaceOops *soops, ListBase *lb, ID *id)
{
	TreeElement *te, *tes;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->type==0) {
			if(tselem->id==id) return te;
			/* only deeper on scene or object */
			if( te->idcode==ID_OB || te->idcode==ID_SCE) { 
				tes= outliner_find_id(soops, &te->subtree, id);
				if(tes) return tes;
			}
		}
	}
	return NULL;
}

static int outliner_show_active_exec(bContext *C, wmOperator *op)
{
	SpaceOops *so= CTX_wm_space_outliner(C);
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	View2D *v2d= &ar->v2d;
	
	TreeElement *te;
	int xdelta, ytop;
	
	// TODO: make this get this info from context instead...
	if (OBACT == NULL) 
		return OPERATOR_CANCELLED;
	
	te= outliner_find_id(so, &so->tree, (ID *)OBACT);
	if (te) {
		/* make te->ys center of view */
		ytop= (int)(te->ys + (v2d->mask.ymax - v2d->mask.ymin)/2);
		if (ytop>0) ytop= 0;
		
		v2d->cur.ymax= (float)ytop;
		v2d->cur.ymin= (float)(ytop-(v2d->mask.ymax - v2d->mask.ymin));
		
		/* make te->xs ==> te->xend center of view */
		xdelta = (int)(te->xs - v2d->cur.xmin);
		v2d->cur.xmin += xdelta;
		v2d->cur.xmax += xdelta;
		
		so->storeflag |= SO_TREESTORE_REDRAW;
	}
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_active(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Show Active";
	ot->idname= "OUTLINER_OT_show_active";
	ot->description= "Adjust the view so that the active Object is shown centered";
	
	/* callbacks */
	ot->exec= outliner_show_active_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* find next element that has this name */
static TreeElement *outliner_find_named(SpaceOops *soops, ListBase *lb, char *name, int flags, TreeElement *prev, int *prevFound)
{
	TreeElement *te, *tes;
	
	for (te= lb->first; te; te= te->next) {
		int found;
		
		/* determine if match */
		if(flags==OL_FIND)
			found= BLI_strcasestr(te->name, name)!=NULL;
		else if(flags==OL_FIND_CASE)
			found= strstr(te->name, name)!=NULL;
		else if(flags==OL_FIND_COMPLETE)
			found= BLI_strcasecmp(te->name, name)==0;
		else
			found= strcmp(te->name, name)==0;
		
		if(found) {
			/* name is right, but is element the previous one? */
			if (prev) {
				if ((te != prev) && (*prevFound)) 
					return te;
				if (te == prev) {
					*prevFound = 1;
				}
			}
			else
				return te;
		}
		
		tes= outliner_find_named(soops, &te->subtree, name, flags, prev, prevFound);
		if(tes) return tes;
	}

	/* nothing valid found */
	return NULL;
}

/* tse is not in the treestore, we use its contents to find a match */
static TreeElement *outliner_find_tse(SpaceOops *soops, TreeStoreElem *tse)
{
	TreeStore *ts= soops->treestore;
	TreeStoreElem *tselem;
	int a;
	
	if(tse->id==NULL) return NULL;
	
	/* check if 'tse' is in treestore */
	tselem= ts->data;
	for(a=0; a<ts->usedelem; a++, tselem++) {
		if((tse->type==0 && tselem->type==0) || (tselem->type==tse->type && tselem->nr==tse->nr)) {
			if(tselem->id==tse->id) {
				break;
			}
		}
	}
	if(tselem) 
		return outliner_find_tree_element(&soops->tree, a);
	
	return NULL;
}


/* Called to find an item based on name.
 */
void outliner_find_panel(Scene *scene, ARegion *ar, SpaceOops *soops, int again, int flags) 
{
	TreeElement *te= NULL;
	TreeElement *last_find;
	TreeStoreElem *tselem;
	int ytop, xdelta, prevFound=0;
	char name[33];
	
	/* get last found tree-element based on stored search_tse */
	last_find= outliner_find_tse(soops, &soops->search_tse);
	
	/* determine which type of search to do */
	if (again && last_find) {
		/* no popup panel - previous + user wanted to search for next after previous */		
		BLI_strncpy(name, soops->search_string, 33);
		flags= soops->search_flags;
		
		/* try to find matching element */
		te= outliner_find_named(soops, &soops->tree, name, flags, last_find, &prevFound);
		if (te==NULL) {
			/* no more matches after previous, start from beginning again */
			prevFound= 1;
			te= outliner_find_named(soops, &soops->tree, name, flags, last_find, &prevFound);
		}
	}
	else {
		/* pop up panel - no previous, or user didn't want search after previous */
		strcpy(name, "");
// XXX		if (sbutton(name, 0, sizeof(name)-1, "Find: ") && name[0]) {
//			te= outliner_find_named(soops, &soops->tree, name, flags, NULL, &prevFound);
//		}
//		else return; /* XXX RETURN! XXX */
	}

	/* do selection and reveal */
	if (te) {
		tselem= TREESTORE(te);
		if (tselem) {
			/* expand branches so that it will be visible, we need to get correct coordinates */
			if( outliner_open_back(soops, te))
				outliner_set_coordinates(ar, soops);
			
			/* deselect all visible, and select found element */
			outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
			tselem->flag |= TSE_SELECTED;
			
			/* make te->ys center of view */
			ytop= (int)(te->ys + (ar->v2d.mask.ymax-ar->v2d.mask.ymin)/2);
			if(ytop>0) ytop= 0;
			ar->v2d.cur.ymax= (float)ytop;
			ar->v2d.cur.ymin= (float)(ytop-(ar->v2d.mask.ymax-ar->v2d.mask.ymin));
			
			/* make te->xs ==> te->xend center of view */
			xdelta = (int)(te->xs - ar->v2d.cur.xmin);
			ar->v2d.cur.xmin += xdelta;
			ar->v2d.cur.xmax += xdelta;
			
			/* store selection */
			soops->search_tse= *tselem;
			
			BLI_strncpy(soops->search_string, name, 33);
			soops->search_flags= flags;
			
			/* redraw */
			soops->storeflag |= SO_TREESTORE_REDRAW;
		}
	}
	else {
		/* no tree-element found */
		error("Not found: %s", name);
	}
}

/* helper function for tree_element_shwo_hierarchy() - recursively checks whether subtrees have any objects*/
static int subtree_has_objects(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->type==0 && te->idcode==ID_OB) return 1;
		if( subtree_has_objects(soops, &te->subtree)) return 1;
	}
	return 0;
}

/* recursive helper function for Show Hierarchy operator */
static void tree_element_show_hierarchy(Scene *scene, SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;

	/* open all object elems, close others */
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		if(tselem->type==0) {
			if(te->idcode==ID_SCE) {
				if(tselem->id!=(ID *)scene) tselem->flag |= TSE_CLOSED;
					else tselem->flag &= ~TSE_CLOSED;
			}
			else if(te->idcode==ID_OB) {
				if(subtree_has_objects(soops, &te->subtree)) tselem->flag &= ~TSE_CLOSED;
				else tselem->flag |= TSE_CLOSED;
			}
		}
		else tselem->flag |= TSE_CLOSED;
		
		if(tselem->flag & TSE_CLOSED); else tree_element_show_hierarchy(scene, soops, &te->subtree);
	}
}

/* show entire object level hierarchy */
static int outliner_show_hierarchy_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	ARegion *ar= CTX_wm_region(C);
	Scene *scene= CTX_data_scene(C);
	
	/* recursively open/close levels */
	tree_element_show_hierarchy(scene, soops, &soops->tree);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_hierarchy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Show Hierarchy";
	ot->idname= "OUTLINER_OT_show_hierarchy";
	ot->description= "Open all object entries and close all others";
	
	/* callbacks */
	ot->exec= outliner_show_hierarchy_exec;
	ot->poll= ED_operator_outliner_active; //  TODO: shouldn't be allowed in RNA views...
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void outliner_select(SpaceOops *soops, ListBase *lb, int *index, short *selecting)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te= lb->first; te && *index >= 0; te=te->next, (*index)--) {
		tselem= TREESTORE(te);
		
		/* if we've encountered the right item, set its 'Outliner' selection status */
		if (*index == 0) {
			/* this should be the last one, so no need to do anything with index */
			if ((te->flag & TE_ICONROW)==0) {
				/* -1 value means toggle testing for now... */
				if (*selecting == -1) {
					if (tselem->flag & TSE_SELECTED) 
						*selecting= 0;
					else 
						*selecting= 1;
				}
				
				/* set selection */
				if (*selecting) 
					tselem->flag |= TSE_SELECTED;
				else 
					tselem->flag &= ~TSE_SELECTED;
			}
		}
		else if ((tselem->flag & TSE_CLOSED)==0) {
			/* Only try selecting sub-elements if we haven't hit the right element yet
			 *
			 * Hack warning:
			 * 	Index must be reduced before supplying it to the sub-tree to try to do
			 * 	selection, however, we need to increment it again for the next loop to 
			 * 	function correctly
			 */
			(*index)--;
			outliner_select(soops, &te->subtree, index, selecting);
			(*index)++;
		}
	}
}

/* ************ SELECTION OPERATIONS ********* */

static void set_operation_types(SpaceOops *soops, ListBase *lb,
				int *scenelevel,
				int *objectlevel,
				int *idlevel,
				int *datalevel)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->flag & TSE_SELECTED) {
			if(tselem->type) {
				if(*datalevel==0) 
					*datalevel= tselem->type;
				else if(*datalevel!=tselem->type) 
					*datalevel= -1;
			}
			else {
				int idcode= GS(tselem->id->name);
				switch(idcode) {
					case ID_SCE:
						*scenelevel= 1;
						break;
					case ID_OB:
						*objectlevel= 1;
						break;
						
					case ID_ME: case ID_CU: case ID_MB: case ID_LT:
					case ID_LA: case ID_AR: case ID_CA:
					case ID_MA: case ID_TE: case ID_IP: case ID_IM:
					case ID_SO: case ID_KE: case ID_WO: case ID_AC:
					case ID_NLA: case ID_TXT: case ID_GR:
						if(*idlevel==0) *idlevel= idcode;
						else if(*idlevel!=idcode) *idlevel= -1;
							break;
				}
			}
		}
		if((tselem->flag & TSE_CLOSED)==0) {
			set_operation_types(soops, &te->subtree,
								scenelevel, objectlevel, idlevel, datalevel);
		}
	}
}

static void unlink_material_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Material **matar=NULL;
	int a, totcol=0;
	
	if( GS(tsep->id->name)==ID_OB) {
		Object *ob= (Object *)tsep->id;
		totcol= ob->totcol;
		matar= ob->mat;
	}
	else if( GS(tsep->id->name)==ID_ME) {
		Mesh *me= (Mesh *)tsep->id;
		totcol= me->totcol;
		matar= me->mat;
	}
	else if( GS(tsep->id->name)==ID_CU) {
		Curve *cu= (Curve *)tsep->id;
		totcol= cu->totcol;
		matar= cu->mat;
	}
	else if( GS(tsep->id->name)==ID_MB) {
		MetaBall *mb= (MetaBall *)tsep->id;
		totcol= mb->totcol;
		matar= mb->mat;
	}

	for(a=0; a<totcol; a++) {
		if(a==te->index && matar[a]) {
			matar[a]->id.us--;
			matar[a]= NULL;
		}
	}
}

static void unlink_texture_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	MTex **mtex= NULL;
	int a;
	
	if( GS(tsep->id->name)==ID_MA) {
		Material *ma= (Material *)tsep->id;
		mtex= ma->mtex;
	}
	else if( GS(tsep->id->name)==ID_LA) {
		Lamp *la= (Lamp *)tsep->id;
		mtex= la->mtex;
	}
	else if( GS(tsep->id->name)==ID_WO) {
		World *wrld= (World *)tsep->id;
		mtex= wrld->mtex;
	}
	else return;
	
	for(a=0; a<MAX_MTEX; a++) {
		if(a==te->index && mtex[a]) {
			if(mtex[a]->tex) {
				mtex[a]->tex->id.us--;
				mtex[a]->tex= NULL;
			}
		}
	}
}

static void unlink_group_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Group *group= (Group *)tselem->id;
	
	if(tsep) {
		if( GS(tsep->id->name)==ID_OB) {
			Object *ob= (Object *)tsep->id;
			ob->dup_group= NULL;
			group->id.us--;
		}
	}
	else {
		unlink_group(group);
	}
}

static void outliner_do_libdata_operation(bContext *C, Scene *scene, SpaceOops *soops, ListBase *lb, 
										 void (*operation_cb)(bContext *C, Scene *scene, TreeElement *, TreeStoreElem *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te=lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->flag & TSE_SELECTED) {
			if(tselem->type==0) {
				TreeStoreElem *tsep= TREESTORE(te->parent);
				operation_cb(C, scene, te, tsep, tselem);
			}
		}
		if((tselem->flag & TSE_CLOSED)==0) {
			outliner_do_libdata_operation(C, scene, soops, &te->subtree, operation_cb);
		}
	}
}

/* */

static void object_select_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, scene);
	if(base && ((base->object->restrictflag & OB_RESTRICT_VIEW)==0)) {
		base->flag |= SELECT;
		base->object->flag |= SELECT;
	}
}

static void object_deselect_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, scene);
	if(base) {
		base->flag &= ~SELECT;
		base->object->flag &= ~SELECT;
	}
}

static void object_delete_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) 
		base= object_in_scene((Object *)tselem->id, scene);
	if(base) {
		// check also library later
		if(scene->obedit==base->object) 
			ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO);
		
		ED_base_object_free_and_unlink(scene, base);
		te->directdata= NULL;
		tselem->id= NULL;
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, scene);

}

static void id_local_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	if(tselem->id->lib && (tselem->id->flag & LIB_EXTERN)) {
		tselem->id->lib= NULL;
		tselem->id->flag= LIB_LOCAL;
		new_id(0, tselem->id, 0);
	}
}

static void group_linkobs2scene_cb(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Group *group= (Group *)tselem->id;
	GroupObject *gob;
	Base *base;
	
	for(gob=group->gobject.first; gob; gob=gob->next) {
		base= object_in_scene(gob->ob, scene);
		if (base) {
			base->object->flag |= SELECT;
			base->flag |= SELECT;
		} else {
			/* link to scene */
			base= MEM_callocN( sizeof(Base), "add_base");
			BLI_addhead(&scene->base, base);
			base->lay= (1<<20)-1; /*v3d->lay;*/ /* would be nice to use the 3d layer but the include's not here */
			gob->ob->flag |= SELECT;
			base->flag = gob->ob->flag;
			base->object= gob->ob;
			id_lib_extern((ID *)gob->ob); /* incase these are from a linked group */
		}
	}
}

static void outliner_do_object_operation(bContext *C, Scene *scene, SpaceOops *soops, ListBase *lb, 
										 void (*operation_cb)(bContext *C, Scene *scene, TreeElement *, TreeStoreElem *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te=lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->flag & TSE_SELECTED) {
			if(tselem->type==0 && te->idcode==ID_OB) {
				// when objects selected in other scenes... dunno if that should be allowed
				Scene *sce= (Scene *)outliner_search_back(soops, te, ID_SCE);
				if(sce && scene != sce) {
					ED_screen_set_scene(C, sce);
				}
				
				operation_cb(C, scene, te, NULL, tselem);
			}
		}
		if((tselem->flag & TSE_CLOSED)==0) {
			outliner_do_object_operation(C, scene, soops, &te->subtree, operation_cb);
		}
	}
}

/* ******************************************** */

static void pchan_cb(int event, TreeElement *te, TreeStoreElem *tselem)
{
	bPoseChannel *pchan= (bPoseChannel *)te->directdata;
	
	if(event==1)
		pchan->bone->flag |= BONE_SELECTED;
	else if(event==2)
		pchan->bone->flag &= ~BONE_SELECTED;
	else if(event==3) {
		pchan->bone->flag |= BONE_HIDDEN_P;
		pchan->bone->flag &= ~BONE_SELECTED;
	}
	else if(event==4)
		pchan->bone->flag &= ~BONE_HIDDEN_P;
}

static void bone_cb(int event, TreeElement *te, TreeStoreElem *tselem)
{
	Bone *bone= (Bone *)te->directdata;
	
	if(event==1)
		bone->flag |= BONE_SELECTED;
	else if(event==2)
		bone->flag &= ~BONE_SELECTED;
	else if(event==3) {
		bone->flag |= BONE_HIDDEN_P;
		bone->flag &= ~BONE_SELECTED;
	}
	else if(event==4)
		bone->flag &= ~BONE_HIDDEN_P;
}

static void ebone_cb(int event, TreeElement *te, TreeStoreElem *tselem)
{
	EditBone *ebone= (EditBone *)te->directdata;
	
	if(event==1)
		ebone->flag |= BONE_SELECTED;
	else if(event==2)
		ebone->flag &= ~BONE_SELECTED;
	else if(event==3) {
		ebone->flag |= BONE_HIDDEN_A;
		ebone->flag &= ~BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
	}
	else if(event==4)
		ebone->flag &= ~BONE_HIDDEN_A;
}

static void sequence_cb(int event, TreeElement *te, TreeStoreElem *tselem)
{
//	Sequence *seq= (Sequence*) te->directdata;
	if(event==1) {
// XXX		select_single_seq(seq, 1);
	}
}

static void outliner_do_data_operation(SpaceOops *soops, int type, int event, ListBase *lb, 
										 void (*operation_cb)(int, TreeElement *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te=lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->flag & TSE_SELECTED) {
			if(tselem->type==type) {
				operation_cb(event, te, tselem);
			}
		}
		if((tselem->flag & TSE_CLOSED)==0) {
			outliner_do_data_operation(soops, type, event, &te->subtree, operation_cb);
		}
	}
}

void outliner_del(bContext *C, Scene *scene, ARegion *ar, SpaceOops *soops)
{
	
	if(soops->outlinevis==SO_SEQUENCE)
		;//		del_seq();
	else {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_delete_cb);
		DAG_scene_sort(scene);
		ED_undo_push(C, "Delete Objects");
	}
}

/* **************************************** */

static EnumPropertyItem prop_object_op_types[] = {
	{1, "SELECT", 0, "Select", ""},
	{2, "DESELECT", 0, "Deselect", ""},
	{4, "DELETE", 0, "Delete", ""},
	{6, "TOGVIS", 0, "Toggle Visible", ""},
	{7, "TOGSEL", 0, "Toggle Selectable", ""},
	{8, "TOGREN", 0, "Toggle Renderable", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_object_operation_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	int event;
	char *str= NULL;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event= RNA_enum_get(op->ptr, "type");

	if(event==1) {
		Scene *sce= scene;	// to be able to delete, scenes are set...
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_select_cb);
		if(scene != sce) {
			ED_screen_set_scene(C, sce);
		}
		
		str= "Select Objects";
	}
	else if(event==2) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_deselect_cb);
		str= "Deselect Objects";
	}
	else if(event==4) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_delete_cb);
		DAG_scene_sort(scene);
		str= "Delete Objects";
	}
	else if(event==5) {	/* disabled, see above (ton) */
		outliner_do_object_operation(C, scene, soops, &soops->tree, id_local_cb);
		str= "Localized Objects";
	}
	else if(event==6) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_visibility_cb);
		str= "Toggle Visibility";
	}
	else if(event==7) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_selectability_cb);
		str= "Toggle Selectability";
	}
	else if(event==8) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_renderability_cb);
		str= "Toggle Renderability";
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);

	ED_undo_push(C, str);
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_object_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Outliner Object Operation";
	ot->idname= "OUTLINER_OT_object_operation";
	ot->description= "";
	
	/* callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= outliner_object_operation_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= 0;

	ot->prop= RNA_def_enum(ot->srna, "type", prop_object_op_types, 0, "Object Operation", "");
}

/* **************************************** */

static EnumPropertyItem prop_group_op_types[] = {
	{1, "UNLINK", 0, "Unlink", ""},
	{2, "LOCAL", 0, "Make Local", ""},
	{3, "LINK", 0, "Link Group Objects to Scene", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_group_operation_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	int event;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event= RNA_enum_get(op->ptr, "type");
	
	if(event==1) {
		outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_group_cb);
		ED_undo_push(C, "Unlink group");
	}
	else if(event==2) {
		outliner_do_libdata_operation(C, scene, soops, &soops->tree, id_local_cb);
		ED_undo_push(C, "Localized Data");
	}
	else if(event==3) {
		outliner_do_libdata_operation(C, scene, soops, &soops->tree, group_linkobs2scene_cb);
		ED_undo_push(C, "Link Group Objects to Scene");
	}
	
	
	WM_event_add_notifier(C, NC_GROUP, NULL);
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_group_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Outliner Group Operation";
	ot->idname= "OUTLINER_OT_group_operation";
	ot->description= "";
	
	/* callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= outliner_group_operation_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= 0;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_group_op_types, 0, "Group Operation", "");
}

/* **************************************** */

static EnumPropertyItem prop_id_op_types[] = {
	{1, "UNLINK", 0, "Unlink", ""},
	{2, "LOCAL", 0, "Make Local", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_id_operation_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	int scenelevel=0, objectlevel=0, idlevel=0, datalevel=0;
	int event;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	event= RNA_enum_get(op->ptr, "type");
	
	if(event==1) {
		switch(idlevel) {
			case ID_MA:
				outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_material_cb);
				ED_undo_push(C, "Unlink material");
				break;
			case ID_TE:
				outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_texture_cb);
				ED_undo_push(C, "Unlink texture");
				break;
			default:
				BKE_report(op->reports, RPT_WARNING, "Not Yet");
		}
	}
	else if(event==2) {
		outliner_do_libdata_operation(C, scene, soops, &soops->tree, id_local_cb);
		ED_undo_push(C, "Localized Data");
	}
	
	/* wrong notifier still... */
	WM_event_add_notifier(C, NC_OBJECT, NULL);
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_id_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Outliner ID data Operation";
	ot->idname= "OUTLINER_OT_id_operation";
	ot->description= "";
	
	/* callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= outliner_id_operation_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= 0;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_id_op_types, 0, "ID data Operation", "");
}

/* **************************************** */

static EnumPropertyItem prop_data_op_types[] = {
	{1, "SELECT", 0, "Select", ""},
	{2, "DESELECT", 0, "Deselect", ""},
	{3, "HIDE", 0, "Hide", ""},
	{4, "UNHIDE", 0, "Unhide", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_data_operation_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	int scenelevel=0, objectlevel=0, idlevel=0, datalevel=0;
	int event;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event= RNA_enum_get(op->ptr, "type");
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	if(datalevel==TSE_POSE_CHANNEL) {
		if(event>0) {
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, pchan_cb);
			WM_event_add_notifier(C, NC_OBJECT|ND_POSE, NULL);
			ED_undo_push(C, "PoseChannel operation");
		}
	}
	else if(datalevel==TSE_BONE) {
		if(event>0) {
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, bone_cb);
			WM_event_add_notifier(C, NC_OBJECT|ND_POSE, NULL);
			ED_undo_push(C, "Bone operation");
		}
	}
	else if(datalevel==TSE_EBONE) {
		if(event>0) {
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, ebone_cb);
			WM_event_add_notifier(C, NC_OBJECT|ND_POSE, NULL);
			ED_undo_push(C, "EditBone operation");
		}
	}
	else if(datalevel==TSE_SEQUENCE) {
		if(event>0) {
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, sequence_cb);
		}
	}
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_data_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Outliner Data Operation";
	ot->idname= "OUTLINER_OT_data_operation";
	ot->description= "";
	
	/* callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= outliner_data_operation_exec;
	ot->poll= ED_operator_outliner_active;
	
	ot->flag= 0;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_data_op_types, 0, "Data Operation", "");
}


/* ******************** */


static int do_outliner_operation_event(bContext *C, Scene *scene, ARegion *ar, SpaceOops *soops, TreeElement *te, wmEvent *event, float *mval)
{
	
	if(mval[1]>te->ys && mval[1]<te->ys+OL_H) {
		int scenelevel=0, objectlevel=0, idlevel=0, datalevel=0;
		TreeStoreElem *tselem= TREESTORE(te);
		
		/* select object that's clicked on and popup context menu */
		if (!(tselem->flag & TSE_SELECTED)) {
			
			if ( outliner_has_one_flag(soops, &soops->tree, TSE_SELECTED, 1) )
				outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
			
			tselem->flag |= TSE_SELECTED;
			/* redraw, same as outliner_select function */
			soops->storeflag |= SO_TREESTORE_REDRAW;
			ED_region_tag_redraw(ar);
		}
		
		set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
		
		if(scenelevel) {
			//if(objectlevel || datalevel || idlevel) error("Mixed selection");
			//else pupmenu("Scene Operations%t|Delete");
		}
		else if(objectlevel) {
			WM_operator_name_call(C, "OUTLINER_OT_object_operation", WM_OP_INVOKE_REGION_WIN, NULL);
		}
		else if(idlevel) {
			if(idlevel==-1 || datalevel) error("Mixed selection");
			else {
				if (idlevel==ID_GR)
					WM_operator_name_call(C, "OUTLINER_OT_group_operation", WM_OP_INVOKE_REGION_WIN, NULL);
				else
					WM_operator_name_call(C, "OUTLINER_OT_id_operation", WM_OP_INVOKE_REGION_WIN, NULL);
			}
		}
		else if(datalevel) {
			if(datalevel==-1) error("Mixed selection");
			else {
				WM_operator_name_call(C, "OUTLINER_OT_data_operation", WM_OP_INVOKE_REGION_WIN, NULL);
			}
		}
		
		return 1;
	}
	
	for(te= te->subtree.first; te; te= te->next) {
		if(do_outliner_operation_event(C, scene, ar, soops, te, event, mval)) 
			return 1;
	}
	return 0;
}


static int outliner_operation(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	
	UI_view2d_region_to_view(&ar->v2d, event->x - ar->winrct.xmin, event->y - ar->winrct.ymin, fmval, fmval+1);
	
	for(te= soops->tree.first; te; te= te->next) {
		if(do_outliner_operation_event(C, scene, ar, soops, te, event, fmval)) break;
	}
	
	return OPERATOR_FINISHED;
}

/* Menu only! Calls other operators */
void OUTLINER_OT_operation(wmOperatorType *ot)
{
	ot->name= "Execute Operation";
	ot->idname= "OUTLINER_OT_operation";
	
	ot->invoke= outliner_operation;
	
	ot->poll= ED_operator_outliner_active;
}



/* ***************** ANIMATO OPERATIONS ********************************** */
/* KeyingSet and Driver Creation - Helper functions */

/* specialised poll callback for these operators to work in Datablocks view only */
static int ed_operator_outliner_datablocks_active(bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	if ((sa) && (sa->spacetype==SPACE_OUTLINER)) {
		SpaceOops *so= CTX_wm_space_outliner(C);
		return (so->outlinevis == SO_DATABLOCKS);
	}
	return 0;
}


/* Helper func to extract an RNA path from selected tree element 
 * NOTE: the caller must zero-out all values of the pointers that it passes here first, as
 * this function does not do that yet 
 */
static void tree_element_to_path(SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, 
							ID **id, char **path, int *array_index, short *flag, short *groupmode)
{
	ListBase hierarchy = {NULL, NULL};
	LinkData *ld;
	TreeElement *tem, *temnext, *temsub;
	TreeStoreElem *tse, *tsenext;
	PointerRNA *ptr, *nextptr;
	PropertyRNA *prop;
	char *newpath=NULL;
	
	/* optimise tricks:
	 *	- Don't do anything if the selected item is a 'struct', but arrays are allowed
	 */
	if (tselem->type == TSE_RNA_STRUCT)
		return;
	
	/* Overview of Algorithm:
	 * 	1. Go up the chain of parents until we find the 'root', taking note of the 
	 *	   levels encountered in reverse-order (i.e. items are added to the start of the list
	 *      for more convenient looping later)
	 * 	2. Walk down the chain, adding from the first ID encountered 
	 *	   (which will become the 'ID' for the KeyingSet Path), and build a  
	 * 		path as we step through the chain
	 */
	 
	/* step 1: flatten out hierarchy of parents into a flat chain */
	for (tem= te->parent; tem; tem= tem->parent) {
		ld= MEM_callocN(sizeof(LinkData), "LinkData for tree_element_to_path()");
		ld->data= tem;
		BLI_addhead(&hierarchy, ld);
	}
	
	/* step 2: step down hierarchy building the path (NOTE: addhead in previous loop was needed so that we can loop like this) */
	for (ld= hierarchy.first; ld; ld= ld->next) {
		/* get data */
		tem= (TreeElement *)ld->data;
		tse= TREESTORE(tem);
		ptr= &tem->rnaptr;
		prop= tem->directdata;
		
		/* check if we're looking for first ID, or appending to path */
		if (*id) {
			/* just 'append' property to path 
			 *	- to prevent memory leaks, we must write to newpath not path, then free old path + swap them
			 */
			if(tse->type == TSE_RNA_PROPERTY) {
				if(RNA_property_type(prop) == PROP_POINTER) {
					/* for pointer we just append property name */
					newpath= RNA_path_append(*path, ptr, prop, 0, NULL);
				}
				else if(RNA_property_type(prop) == PROP_COLLECTION) {
					char buf[128], *name;
					
					temnext= (TreeElement*)(ld->next->data);
					tsenext= TREESTORE(temnext);
					
					nextptr= &temnext->rnaptr;
					name= RNA_struct_name_get_alloc(nextptr, buf, sizeof(buf));
					
					if(name) {
						/* if possible, use name as a key in the path */
						newpath= RNA_path_append(*path, NULL, prop, 0, name);
						
						if(name != buf)
							MEM_freeN(name);
					}
					else {
						/* otherwise use index */
						int index= 0;
						
						for(temsub=tem->subtree.first; temsub; temsub=temsub->next, index++)
							if(temsub == temnext)
								break;
						
						newpath= RNA_path_append(*path, NULL, prop, index, NULL);
					}
					
					ld= ld->next;
				}
			}
			
			if(newpath) {
				if (*path) MEM_freeN(*path);
				*path= newpath;
				newpath= NULL;
			}
		}
		else {
			/* no ID, so check if entry is RNA-struct, and if that RNA-struct is an ID datablock to extract info from */
			if (tse->type == TSE_RNA_STRUCT) {
				/* ptr->data not ptr->id.data seems to be the one we want, since ptr->data is sometimes the owner of this ID? */
				if(RNA_struct_is_ID(ptr->type)) {
					*id= (ID *)ptr->data;
					
					/* clear path */
					if(*path) {
						MEM_freeN(*path);
						path= NULL;
					}
				}
			}
		}
	}

	/* step 3: if we've got an ID, add the current item to the path */
	if (*id) {
		/* add the active property to the path */
		ptr= &te->rnaptr;
		prop= te->directdata;
		
		/* array checks */
		if (tselem->type == TSE_RNA_ARRAY_ELEM) {
			/* item is part of an array, so must set the array_index */
			*array_index= te->index;
		}
		else if (RNA_property_array_length(ptr, prop)) {
			/* entire array was selected, so keyframe all */
			*flag |= KSP_FLAG_WHOLE_ARRAY;
		}
		
		/* path */
		newpath= RNA_path_append(*path, NULL, prop, 0, NULL);
		if (*path) MEM_freeN(*path);
		*path= newpath;
	}

	/* free temp data */
	BLI_freelistN(&hierarchy);
}

/* ***************** KEYINGSET OPERATIONS *************** */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
	DRIVERS_EDITMODE_ADD	= 0,
	DRIVERS_EDITMODE_REMOVE,
} eDrivers_EditModes;

/* Utilities ---------------------------------- */ 

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_drivers_editop(SpaceOops *soops, ListBase *tree, short mode)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te= tree->first; te; te=te->next) {
		tselem= TREESTORE(te);
		
		/* if item is selected, perform operation */
		if (tselem->flag & TSE_SELECTED) {
			ID *id= NULL;
			char *path= NULL;
			int array_index= 0;
			short flag= 0;
			short groupmode= KSP_GROUP_KSNAME;
			
			/* check if RNA-property described by this selected element is an animateable prop */
			if ((tselem->type == TSE_RNA_PROPERTY) && RNA_property_animateable(&te->rnaptr, te->directdata)) {
				/* get id + path + index info from the selected element */
				tree_element_to_path(soops, te, tselem, 
						&id, &path, &array_index, &flag, &groupmode);
			}
			
			/* only if ID and path were set, should we perform any actions */
			if (id && path) {
				int arraylen;
				
				/* array checks */
				if (flag & KSP_FLAG_WHOLE_ARRAY) {
					/* entire array was selected, so add drivers for all */
					arraylen= RNA_property_array_length(&te->rnaptr, te->directdata);
				}
				else
					arraylen= array_index;
				
				/* we should do at least one step */
				if (arraylen == array_index)
					arraylen++;
				
				/* for each array element we should affect, add driver */
				for (; array_index < arraylen; array_index++) {
					/* action depends on mode */
					switch (mode) {
						case DRIVERS_EDITMODE_ADD:
						{
							/* add a new driver with the information obtained (only if valid) */
							ANIM_add_driver(id, path, array_index, flag, DRIVER_TYPE_PYTHON);
						}
							break;
						case DRIVERS_EDITMODE_REMOVE:
						{
							/* remove driver matching the information obtained (only if valid) */
							ANIM_remove_driver(id, path, array_index, flag);
						}
							break;
					}
				}
				
				/* free path, since it had to be generated */
				MEM_freeN(path);
			}
			
			
		}
		
		/* go over sub-tree */
		if ((tselem->flag & TSE_CLOSED)==0)
			do_outliner_drivers_editop(soops, &te->subtree, mode);
	}
}

/* Add Operator ---------------------------------- */

static int outliner_drivers_addsel_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner= CTX_wm_space_outliner(C);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_drivers_editop(soutliner, &soutliner->tree, DRIVERS_EDITMODE_ADD);
	
	/* send notifiers */
	WM_event_add_notifier(C, ND_KEYS, NULL); // XXX
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_add_selected(wmOperatorType *ot)
{
	/* api callbacks */
	ot->idname= "OUTLINER_OT_drivers_add_selected";
	ot->name= "Add Drivers for Selected";
	ot->description= "Add drivers to selected items";
	
	/* api callbacks */
	ot->exec= outliner_drivers_addsel_exec;
	ot->poll= ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}


/* Remove Operator ---------------------------------- */

static int outliner_drivers_deletesel_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner= CTX_wm_space_outliner(C);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_drivers_editop(soutliner, &soutliner->tree, DRIVERS_EDITMODE_REMOVE);
	
	/* send notifiers */
	WM_event_add_notifier(C, ND_KEYS, NULL); // XXX
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_delete_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname= "OUTLINER_OT_drivers_delete_selected";
	ot->name= "Delete Drivers for Selected";
	ot->description= "Delete drivers assigned to selected items";
	
	/* api callbacks */
	ot->exec= outliner_drivers_deletesel_exec;
	ot->poll= ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/* ***************** KEYINGSET OPERATIONS *************** */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
	KEYINGSET_EDITMODE_ADD	= 0,
	KEYINGSET_EDITMODE_REMOVE,
} eKeyingSet_EditModes;

/* Utilities ---------------------------------- */ 
 
/* find the 'active' KeyingSet, and add if not found (if adding is allowed) */
// TODO: should this be an API func?
static KeyingSet *verify_active_keyingset(Scene *scene, short add)
{
	KeyingSet *ks= NULL;
	
	/* sanity check */
	if (scene == NULL)
		return NULL;
	
	/* try to find one from scene */
	if (scene->active_keyingset > 0)
		ks= BLI_findlink(&scene->keyingsets, scene->active_keyingset-1);
		
	/* add if none found */
	// XXX the default settings have yet to evolve
	if ((add) && (ks==NULL)) {
		ks= BKE_keyingset_add(&scene->keyingsets, NULL, KEYINGSET_ABSOLUTE, 0);
		scene->active_keyingset= BLI_countlist(&scene->keyingsets);
	}
	
	return ks;
}

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_keyingset_editop(SpaceOops *soops, KeyingSet *ks, ListBase *tree, short mode)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te= tree->first; te; te=te->next) {
		tselem= TREESTORE(te);
		
		/* if item is selected, perform operation */
		if (tselem->flag & TSE_SELECTED) {
			ID *id= NULL;
			char *path= NULL;
			int array_index= 0;
			short flag= 0;
			short groupmode= KSP_GROUP_KSNAME;
			
			/* check if RNA-property described by this selected element is an animateable prop */
			if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM) && RNA_property_animateable(&te->rnaptr, te->directdata)) {
				/* get id + path + index info from the selected element */
				tree_element_to_path(soops, te, tselem, 
						&id, &path, &array_index, &flag, &groupmode);
			}
			
			/* only if ID and path were set, should we perform any actions */
			if (id && path) {
				/* action depends on mode */
				switch (mode) {
					case KEYINGSET_EDITMODE_ADD:
					{
						/* add a new path with the information obtained (only if valid) */
						// TODO: what do we do with group name? for now, we don't supply one, and just let this use the KeyingSet name
						BKE_keyingset_add_path(ks, id, NULL, path, array_index, flag, groupmode);
						ks->active_path= BLI_countlist(&ks->paths);
					}
						break;
					case KEYINGSET_EDITMODE_REMOVE:
					{
						/* find the relevant path, then remove it from the KeyingSet */
						KS_Path *ksp= BKE_keyingset_find_path(ks, id, NULL, path, array_index, groupmode);
						
						if (ksp) {
							/* free path's data */
							// TODO: we probably need an API method for this 
							if (ksp->rna_path) MEM_freeN(ksp->rna_path);
							ks->active_path= 0;
							
							/* remove path from set */
							BLI_freelinkN(&ks->paths, ksp);
						}
					}
						break;
				}
				
				/* free path, since it had to be generated */
				MEM_freeN(path);
			}
		}
		
		/* go over sub-tree */
		if ((tselem->flag & TSE_CLOSED)==0)
			do_outliner_keyingset_editop(soops, ks, &te->subtree, mode);
	}
}

/* Add Operator ---------------------------------- */

static int outliner_keyingset_additems_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner= CTX_wm_space_outliner(C);
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks= verify_active_keyingset(scene, 1);
	
	/* check for invalid states */
	if (ks == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an Active Keying Set");
		return OPERATOR_CANCELLED;
	}
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_ADD);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_add_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname= "OUTLINER_OT_keyingset_add_selected";
	ot->name= "Keyingset Add Selected";
	
	/* api callbacks */
	ot->exec= outliner_keyingset_additems_exec;
	ot->poll= ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}


/* Remove Operator ---------------------------------- */

static int outliner_keyingset_removeitems_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner= CTX_wm_space_outliner(C);
	Scene *scene= CTX_data_scene(C);
	KeyingSet *ks= verify_active_keyingset(scene, 1);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_REMOVE);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE|ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_remove_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname= "OUTLINER_OT_keyingset_remove_selected";
	ot->name= "Keyingset Remove Selected";
	
	/* api callbacks */
	ot->exec= outliner_keyingset_removeitems_exec;
	ot->poll= ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_UNDO;
}

/* ***************** DRAW *************** */

/* make function calls a bit compacter */
struct DrawIconArg {
	uiBlock *block;
	ID *id;
	int xmax, x, y;
	float alpha;
};

static void tselem_draw_icon_uibut(struct DrawIconArg *arg, int icon)
{
	/* restrict collumn clip... it has been coded by simply overdrawing, doesnt work for buttons */
	if(arg->x >= arg->xmax) 
		UI_icon_draw(arg->x, arg->y, icon);
	else {
		uiBut *but= uiDefIconBut(arg->block, LABEL, 0, icon, arg->x-4, arg->y, ICON_DEFAULT_WIDTH, ICON_DEFAULT_WIDTH, NULL, 0.0, 0.0, 1.0, arg->alpha, "");
		if(arg->id)
			uiButSetDragID(but, arg->id);
	}
	
}

static void tselem_draw_icon(uiBlock *block, int xmax, float x, float y, TreeStoreElem *tselem, TreeElement *te, float alpha)
{
	struct DrawIconArg arg;
	
	/* make function calls a bit compacter */
	arg.block= block;
	arg.id= tselem->id;
	arg.xmax= xmax;
	arg.x= x;
	arg.y= y;
	arg.alpha= alpha;
	
	if(tselem->type) {
		switch( tselem->type) {
			case TSE_ANIM_DATA:
				UI_icon_draw(x, y, ICON_ANIM_DATA); break; // xxx
			case TSE_NLA:
				UI_icon_draw(x, y, ICON_NLA); break;
			case TSE_NLA_TRACK:
				UI_icon_draw(x, y, ICON_NLA); break; // XXX
			case TSE_NLA_ACTION:
				UI_icon_draw(x, y, ICON_ACTION); break;
			case TSE_DEFGROUP_BASE:
				UI_icon_draw(x, y, ICON_GROUP_VERTEX); break;
			case TSE_BONE:
			case TSE_EBONE:
				UI_icon_draw(x, y, ICON_BONE_DATA); break;
			case TSE_CONSTRAINT_BASE:
				UI_icon_draw(x, y, ICON_CONSTRAINT); break;
			case TSE_MODIFIER_BASE:
				UI_icon_draw(x, y, ICON_MODIFIER); break;
			case TSE_LINKED_OB:
				UI_icon_draw(x, y, ICON_OBJECT_DATA); break;
			case TSE_LINKED_PSYS:
				UI_icon_draw(x, y, ICON_PARTICLES); break;
			case TSE_MODIFIER:
			{
				Object *ob= (Object *)tselem->id;
				ModifierData *md= BLI_findlink(&ob->modifiers, tselem->nr);
				switch(md->type) {
					case eModifierType_Subsurf: 
						UI_icon_draw(x, y, ICON_MOD_SUBSURF); break;
					case eModifierType_Armature: 
						UI_icon_draw(x, y, ICON_MOD_ARMATURE); break;
					case eModifierType_Lattice: 
						UI_icon_draw(x, y, ICON_MOD_LATTICE); break;
					case eModifierType_Curve: 
						UI_icon_draw(x, y, ICON_MOD_CURVE); break;
					case eModifierType_Build: 
						UI_icon_draw(x, y, ICON_MOD_BUILD); break;
					case eModifierType_Mirror: 
						UI_icon_draw(x, y, ICON_MOD_MIRROR); break;
					case eModifierType_Decimate: 
						UI_icon_draw(x, y, ICON_MOD_DECIM); break;
					case eModifierType_Wave: 
						UI_icon_draw(x, y, ICON_MOD_WAVE); break;
					case eModifierType_Hook: 
						UI_icon_draw(x, y, ICON_HOOK); break;
					case eModifierType_Softbody: 
						UI_icon_draw(x, y, ICON_MOD_SOFT); break;
					case eModifierType_Boolean: 
						UI_icon_draw(x, y, ICON_MOD_BOOLEAN); break;
					case eModifierType_ParticleSystem: 
						UI_icon_draw(x, y, ICON_MOD_PARTICLES); break;
					case eModifierType_ParticleInstance:
						UI_icon_draw(x, y, ICON_MOD_PARTICLES); break;
					case eModifierType_EdgeSplit:
						UI_icon_draw(x, y, ICON_MOD_EDGESPLIT); break;
					case eModifierType_Array:
						UI_icon_draw(x, y, ICON_MOD_ARRAY); break;
					case eModifierType_UVProject:
						UI_icon_draw(x, y, ICON_MOD_UVPROJECT); break;
					case eModifierType_Displace:
						UI_icon_draw(x, y, ICON_MOD_DISPLACE); break;
					case eModifierType_Shrinkwrap:
						UI_icon_draw(x, y, ICON_MOD_SHRINKWRAP); break;
					case eModifierType_Cast:
						UI_icon_draw(x, y, ICON_MOD_CAST); break;
					case eModifierType_MeshDeform:
						UI_icon_draw(x, y, ICON_MOD_MESHDEFORM); break;
					case eModifierType_Bevel:
						UI_icon_draw(x, y, ICON_MOD_BEVEL); break;
					case eModifierType_Smooth:
						UI_icon_draw(x, y, ICON_MOD_SMOOTH); break;
					case eModifierType_SimpleDeform:
						UI_icon_draw(x, y, ICON_MOD_SIMPLEDEFORM); break;
					case eModifierType_Mask:
						UI_icon_draw(x, y, ICON_MOD_MASK); break;
					case eModifierType_Cloth:
						UI_icon_draw(x, y, ICON_MOD_CLOTH); break;
					case eModifierType_Explode:
						UI_icon_draw(x, y, ICON_MOD_EXPLODE); break;
					case eModifierType_Collision:
						UI_icon_draw(x, y, ICON_MOD_PHYSICS); break;
					case eModifierType_Fluidsim:
						UI_icon_draw(x, y, ICON_MOD_FLUIDSIM); break;
					case eModifierType_Multires:
						UI_icon_draw(x, y, ICON_MOD_MULTIRES); break;
					case eModifierType_Smoke:
						UI_icon_draw(x, y, ICON_MOD_SMOKE); break;
					case eModifierType_Solidify:
						UI_icon_draw(x, y, ICON_MOD_SOLIDIFY); break;
					case eModifierType_Screw:
						UI_icon_draw(x, y, ICON_MOD_SCREW); break;
					default:
						UI_icon_draw(x, y, ICON_DOT); break;
				}
				break;
			}
			case TSE_SCRIPT_BASE:
				UI_icon_draw(x, y, ICON_TEXT); break;
			case TSE_POSE_BASE:
				UI_icon_draw(x, y, ICON_ARMATURE_DATA); break;
			case TSE_POSE_CHANNEL:
				UI_icon_draw(x, y, ICON_BONE_DATA); break;
			case TSE_PROXY:
				UI_icon_draw(x, y, ICON_GHOST); break;
			case TSE_R_LAYER_BASE:
				UI_icon_draw(x, y, ICON_RENDERLAYERS); break;
			case TSE_R_LAYER:
				UI_icon_draw(x, y, ICON_RENDER_RESULT); break;
			case TSE_LINKED_LAMP:
				UI_icon_draw(x, y, ICON_LAMP_DATA); break;
			case TSE_LINKED_MAT:
				UI_icon_draw(x, y, ICON_MATERIAL_DATA); break;
			case TSE_POSEGRP_BASE:
				UI_icon_draw(x, y, ICON_VERTEXSEL); break;
			case TSE_SEQUENCE:
				if(te->idcode==SEQ_MOVIE)
					UI_icon_draw(x, y, ICON_SEQUENCE);
				else if(te->idcode==SEQ_META)
					UI_icon_draw(x, y, ICON_DOT);
				else if(te->idcode==SEQ_SCENE)
					UI_icon_draw(x, y, ICON_SCENE);
				else if(te->idcode==SEQ_SOUND)
					UI_icon_draw(x, y, ICON_SOUND);
				else if(te->idcode==SEQ_IMAGE)
					UI_icon_draw(x, y, ICON_IMAGE_COL);
				else
					UI_icon_draw(x, y, ICON_PARTICLES);
				break;
			case TSE_SEQ_STRIP:
				UI_icon_draw(x, y, ICON_LIBRARY_DATA_DIRECT);
				break;
			case TSE_SEQUENCE_DUP:
				UI_icon_draw(x, y, ICON_OBJECT_DATA);
				break;
			case TSE_RNA_STRUCT:
				if(RNA_struct_is_ID(te->rnaptr.type)) {
					arg.id= (ID *)te->rnaptr.data;
					tselem_draw_icon_uibut(&arg, RNA_struct_ui_icon(te->rnaptr.type));
				}
				else
					UI_icon_draw(x, y, RNA_struct_ui_icon(te->rnaptr.type));
				break;
			default:
				UI_icon_draw(x, y, ICON_DOT); break;
		}
	}
	else if (GS(tselem->id->name) == ID_OB) {
		Object *ob= (Object *)tselem->id;
		switch (ob->type) {
			case OB_LAMP:
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_LAMP); break;
			case OB_MESH: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_MESH); break;
			case OB_CAMERA: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_CAMERA); break;
			case OB_CURVE: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_CURVE); break;
			case OB_MBALL: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_META); break;
			case OB_LATTICE: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_LATTICE); break;
			case OB_ARMATURE: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_ARMATURE); break;
			case OB_FONT: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_FONT); break;
			case OB_SURF: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_SURFACE); break;
			case OB_EMPTY: 
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_OB_EMPTY); break;
		
		}
	}
	else {
		switch( GS(tselem->id->name)) {
			case ID_SCE:
				tselem_draw_icon_uibut(&arg, ICON_SCENE_DATA); break;
			case ID_ME:
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_DATA_MESH); break;
			case ID_CU:
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_DATA_CURVE); break;
			case ID_MB:
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_DATA_META); break;
			case ID_LT:
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_DATA_LATTICE); break;
			case ID_LA:
			{
				Lamp *la= (Lamp *)tselem->id;
				
				switch(la->type) {
					case LA_LOCAL:
						tselem_draw_icon_uibut(&arg, ICON_LAMP_POINT); break;
					case LA_SUN:
						tselem_draw_icon_uibut(&arg, ICON_LAMP_SUN); break;
					case LA_SPOT:
						tselem_draw_icon_uibut(&arg, ICON_LAMP_SPOT); break;
					case LA_HEMI:
						tselem_draw_icon_uibut(&arg, ICON_LAMP_HEMI); break;
					case LA_AREA:
						tselem_draw_icon_uibut(&arg, ICON_LAMP_AREA); break;
					default:
						tselem_draw_icon_uibut(&arg, ICON_OUTLINER_DATA_LAMP); break;
				}
				break;
			}
			case ID_MA:
				tselem_draw_icon_uibut(&arg, ICON_MATERIAL_DATA); break;
			case ID_TE:
				tselem_draw_icon_uibut(&arg, ICON_TEXTURE_DATA); break;
			case ID_IM:
				tselem_draw_icon_uibut(&arg, ICON_IMAGE_DATA); break;
			case ID_SO:
				tselem_draw_icon_uibut(&arg, ICON_SPEAKER); break;
			case ID_AR:
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_DATA_ARMATURE); break;
			case ID_CA:
				tselem_draw_icon_uibut(&arg, ICON_OUTLINER_DATA_CAMERA); break;
			case ID_KE:
				tselem_draw_icon_uibut(&arg, ICON_SHAPEKEY_DATA); break;
			case ID_WO:
				tselem_draw_icon_uibut(&arg, ICON_WORLD_DATA); break;
			case ID_AC:
				tselem_draw_icon_uibut(&arg, ICON_ACTION); break;
			case ID_NLA:
				tselem_draw_icon_uibut(&arg, ICON_NLA); break;
			case ID_TXT:
				tselem_draw_icon_uibut(&arg, ICON_SCRIPT); break;
			case ID_GR:
				tselem_draw_icon_uibut(&arg, ICON_GROUP); break;
			case ID_LI:
				tselem_draw_icon_uibut(&arg, ICON_LIBRARY_DATA_DIRECT); break;
		}
	}
}

static void outliner_draw_iconrow(bContext *C, uiBlock *block, Scene *scene, SpaceOops *soops, ListBase *lb, int level, int xmax, int *offsx, int ys)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int active;

	for(te= lb->first; te; te= te->next) {
		
		/* exit drawing early */
		if((*offsx) - OL_X > xmax)
			break;

		tselem= TREESTORE(te);
		
		/* object hierarchy always, further constrained on level */
		if(level<1 || (tselem->type==0 && te->idcode==ID_OB)) {

			/* active blocks get white circle */
			active= 0;
			if(tselem->type==0) {
				if(te->idcode==ID_OB) active= (OBACT==(Object *)tselem->id);
				else if(scene->obedit && scene->obedit->data==tselem->id) active= 1;	// XXX use context?
				else active= tree_element_active(C, scene, soops, te, 0);
			}
			else active= tree_element_type_active(NULL, scene, soops, te, tselem, 0);
			
			if(active) {
				uiSetRoundBox(15);
				glColor4ub(255, 255, 255, 100);
				uiRoundBox( (float)*offsx-0.5f, (float)ys-1.0f, (float)*offsx+OL_H-3.0f, (float)ys+OL_H-3.0f, OL_H/2.0f-2.0f);
				glEnable(GL_BLEND); /* roundbox disables */
			}
			
			tselem_draw_icon(block, xmax, (float)*offsx, (float)ys, tselem, te, 0.5f);
			te->xs= (float)*offsx;
			te->ys= (float)ys;
			te->xend= (short)*offsx+OL_X;
			te->flag |= TE_ICONROW;	// for click
			
			(*offsx) += OL_X;
		}
		
		/* this tree element always has same amount of branches, so dont draw */
		if(tselem->type!=TSE_R_LAYER)
			outliner_draw_iconrow(C, block, scene, soops, &te->subtree, level+1, xmax, offsx, ys);
	}
	
}

static void outliner_draw_tree_element(bContext *C, uiBlock *block, Scene *scene, ARegion *ar, SpaceOops *soops, TreeElement *te, int startx, int *starty)
{
	TreeElement *ten;
	TreeStoreElem *tselem;
	int offsx= 0, active=0; // active=1 active obj, else active data
	
	tselem= TREESTORE(te);

	if(*starty+2*OL_H >= ar->v2d.cur.ymin && *starty<= ar->v2d.cur.ymax) {
		int xmax= ar->v2d.cur.xmax;
		
		/* icons can be ui buts, we dont want it to overlap with restrict */
		if((soops->flag & SO_HIDE_RESTRICTCOLS)==0)
			xmax-= OL_TOGW+ICON_DEFAULT_WIDTH;
		
		glEnable(GL_BLEND);

		/* colors for active/selected data */
		if(tselem->type==0) {
			if(te->idcode==ID_SCE) {
				if(tselem->id == (ID *)scene) {
					glColor4ub(255, 255, 255, 100);
					active= 2;
				}
			}
			else if(te->idcode==ID_OB) {
				Object *ob= (Object *)tselem->id;
				
				if(ob==OBACT || (ob->flag & SELECT)) {
					char col[4];
					
					active= 2;
					if(ob==OBACT) {
						UI_GetThemeColorType4ubv(TH_ACTIVE, SPACE_VIEW3D, col);
						/* so black text is drawn when active and not selected */
						if (ob->flag & SELECT) active= 1;
					}
					else UI_GetThemeColorType4ubv(TH_SELECT, SPACE_VIEW3D, col);
					col[3]= 100;
					glColor4ubv((GLubyte *)col);
				}

			}
			else if(scene->obedit && scene->obedit->data==tselem->id) {
				glColor4ub(255, 255, 255, 100);
				active= 2;
			}
			else {
				if(tree_element_active(C, scene, soops, te, 0)) {
					glColor4ub(220, 220, 255, 100);
					active= 2;
				}
			}
		}
		else {
			if( tree_element_type_active(NULL, scene, soops, te, tselem, 0) ) active= 2;
			glColor4ub(220, 220, 255, 100);
		}
		
		/* active circle */
		if(active) {
			uiSetRoundBox(15);
			uiRoundBox( (float)startx+OL_H-1.5f, (float)*starty+2.0f, (float)startx+2.0f*OL_H-4.0f, (float)*starty+OL_H-1.0f, OL_H/2.0f-2.0f);
			glEnable(GL_BLEND);	/* roundbox disables it */
			
			te->flag |= TE_ACTIVE; // for lookup in display hierarchies
		}
		
		/* open/close icon, only when sublevels, except for scene */
		if(te->subtree.first || (tselem->type==0 && te->idcode==ID_SCE) || (te->flag & TE_LAZY_CLOSED)) {
			int icon_x;
			if(tselem->type==0 && ELEM(te->idcode, ID_OB, ID_SCE))
				icon_x = startx;
			else
				icon_x = startx+5;

				// icons a bit higher
			if(tselem->flag & TSE_CLOSED) 
				UI_icon_draw((float)icon_x, (float)*starty+2, ICON_DISCLOSURE_TRI_RIGHT);
			else
				UI_icon_draw((float)icon_x, (float)*starty+2, ICON_DISCLOSURE_TRI_DOWN);
		}
		offsx+= OL_X;
		
		/* datatype icon */
		
		if(!(ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM))) {
			// icons a bit higher
			tselem_draw_icon(block, xmax, (float)startx+offsx, (float)*starty+2, tselem, te, 1.0f);
			
			offsx+= OL_X;
		}
		else
			offsx+= 2;
		
		if(tselem->type==0 && tselem->id->lib) {
			glPixelTransferf(GL_ALPHA_SCALE, 0.5f);
			if(tselem->id->flag & LIB_INDIRECT)
				UI_icon_draw((float)startx+offsx, (float)*starty+2, ICON_LIBRARY_DATA_INDIRECT);
			else
				UI_icon_draw((float)startx+offsx, (float)*starty+2, ICON_LIBRARY_DATA_DIRECT);
			glPixelTransferf(GL_ALPHA_SCALE, 1.0f);
			offsx+= OL_X;
		}		
		glDisable(GL_BLEND);

		/* name */
		if(active==1) UI_ThemeColor(TH_TEXT_HI);
		else if(ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) UI_ThemeColorBlend(TH_BACK, TH_TEXT, 0.75f);
		else UI_ThemeColor(TH_TEXT);
		
		UI_DrawString(startx+offsx, *starty+5, te->name);
		
		offsx+= (int)(OL_X + UI_GetStringWidth(te->name));
		
		/* closed item, we draw the icons, not when it's a scene, or master-server list though */
		if(tselem->flag & TSE_CLOSED) {
			if(te->subtree.first) {
				if(tselem->type==0 && te->idcode==ID_SCE);
				else if(tselem->type!=TSE_R_LAYER) { /* this tree element always has same amount of branches, so dont draw */
					int tempx= startx+offsx;
					
					// divider
					UI_ThemeColorShade(TH_BACK, -40);
					glRecti(tempx -10, *starty+4, tempx -8, *starty+OL_H-4);

					glEnable(GL_BLEND);
					glPixelTransferf(GL_ALPHA_SCALE, 0.5);

					outliner_draw_iconrow(C, block, scene, soops, &te->subtree, 0, xmax, &tempx, *starty+2);

					glPixelTransferf(GL_ALPHA_SCALE, 1.0);
					glDisable(GL_BLEND);
				}
			}
		}
	}	
	/* store coord and continue, we need coordinates for elements outside view too */
	te->xs= (float)startx;
	te->ys= (float)*starty;
	te->xend= startx+offsx;
		
	*starty-= OL_H;

	if((tselem->flag & TSE_CLOSED)==0) {
		for(ten= te->subtree.first; ten; ten= ten->next) {
			outliner_draw_tree_element(C, block, scene, ar, soops, ten, startx+OL_X, starty);
		}
	}	
}

static void outliner_draw_hierarchy(SpaceOops *soops, ListBase *lb, int startx, int *starty)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int y1, y2;
	
	if(lb->first==NULL) return;
	
	y1=y2= *starty; /* for vertical lines between objects */
	for(te=lb->first; te; te= te->next) {
		y2= *starty;
		tselem= TREESTORE(te);
		
		/* horizontal line? */
		if(tselem->type==0 && (te->idcode==ID_OB || te->idcode==ID_SCE))
			glRecti(startx, *starty, startx+OL_X, *starty-1);
			
		*starty-= OL_H;
		
		if((tselem->flag & TSE_CLOSED)==0)
			outliner_draw_hierarchy(soops, &te->subtree, startx+OL_X, starty);
	}
	
	/* vertical line */
	te= lb->last;
	if(te->parent || lb->first!=lb->last) {
		tselem= TREESTORE(te);
		if(tselem->type==0 && te->idcode==ID_OB) {
			
			glRecti(startx, y1+OL_H, startx+1, y2);
		}
	}
}

static void outliner_draw_struct_marks(ARegion *ar, SpaceOops *soops, ListBase *lb, int *starty) 
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		/* selection status */
		if((tselem->flag & TSE_CLOSED)==0)
			if(tselem->type == TSE_RNA_STRUCT)
				glRecti(0, *starty+1, (int)ar->v2d.cur.xmax, *starty+OL_H-1);

		*starty-= OL_H;
		if((tselem->flag & TSE_CLOSED)==0) {
			outliner_draw_struct_marks(ar, soops, &te->subtree, starty);
			if(tselem->type == TSE_RNA_STRUCT)
				fdrawline(0, (float)*starty+OL_H-1, ar->v2d.cur.xmax, (float)*starty+OL_H-1);
		}
	}
}

static void outliner_draw_selection(ARegion *ar, SpaceOops *soops, ListBase *lb, int *starty) 
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		/* selection status */
		if(tselem->flag & TSE_SELECTED) {
			glRecti(0, *starty+1, (int)ar->v2d.cur.xmax, *starty+OL_H-1);
		}
		*starty-= OL_H;
		if((tselem->flag & TSE_CLOSED)==0) outliner_draw_selection(ar, soops, &te->subtree, starty);
	}
}


static void outliner_draw_tree(bContext *C, uiBlock *block, Scene *scene, ARegion *ar, SpaceOops *soops)
{
	TreeElement *te;
	int starty, startx;
	float col[4];
		
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); // only once
	
	if (ELEM(soops->outlinevis, SO_DATABLOCKS, SO_USERDEF)) {
		/* struct marks */
		UI_ThemeColorShadeAlpha(TH_BACK, -15, -200);
		//UI_ThemeColorShade(TH_BACK, -20);
		starty= (int)ar->v2d.tot.ymax-OL_H;
		outliner_draw_struct_marks(ar, soops, &soops->tree, &starty);
	}
	
	/* always draw selection fill before hierarchy */
	UI_GetThemeColor3fv(TH_BACK, col);
	glColor3f(col[0]+0.06f, col[1]+0.08f, col[2]+0.10f);
	starty= (int)ar->v2d.tot.ymax-OL_H;
	outliner_draw_selection(ar, soops, &soops->tree, &starty);
	
	// grey hierarchy lines
	UI_ThemeColorBlend(TH_BACK, TH_TEXT, 0.2f);
	starty= (int)ar->v2d.tot.ymax-OL_H/2;
	startx= 6;
	outliner_draw_hierarchy(soops, &soops->tree, startx, &starty);
	
	// items themselves
	starty= (int)ar->v2d.tot.ymax-OL_H;
	startx= 0;
	for(te= soops->tree.first; te; te= te->next) {
		outliner_draw_tree_element(C, block, scene, ar, soops, te, startx, &starty);
	}
}


static void outliner_back(ARegion *ar, SpaceOops *soops)
{
	int ystart;
	
	UI_ThemeColorShade(TH_BACK, 6);
	ystart= (int)ar->v2d.tot.ymax;
	ystart= OL_H*(ystart/(OL_H));
	
	while(ystart+2*OL_H > ar->v2d.cur.ymin) {
		glRecti(0, ystart, (int)ar->v2d.cur.xmax, ystart+OL_H);
		ystart-= 2*OL_H;
	}
}

static void outliner_draw_restrictcols(ARegion *ar, SpaceOops *soops)
{
	int ystart;
	
	/* background underneath */
	UI_ThemeColor(TH_BACK);
	glRecti((int)ar->v2d.cur.xmax-OL_TOGW, (int)ar->v2d.cur.ymin, (int)ar->v2d.cur.xmax, (int)ar->v2d.cur.ymax);
	
	UI_ThemeColorShade(TH_BACK, 6);
	ystart= (int)ar->v2d.tot.ymax;
	ystart= OL_H*(ystart/(OL_H));
	
	while(ystart+2*OL_H > ar->v2d.cur.ymin) {
		glRecti((int)ar->v2d.cur.xmax-OL_TOGW, ystart, (int)ar->v2d.cur.xmax, ystart+OL_H);
		ystart-= 2*OL_H;
	}
	
	UI_ThemeColorShadeAlpha(TH_BACK, -15, -200);

	/* view */
	fdrawline(ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX,
		ar->v2d.cur.ymax,
		ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX,
		ar->v2d.cur.ymin);

	/* render */
	fdrawline(ar->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX,
		ar->v2d.cur.ymax,
		ar->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX,
		ar->v2d.cur.ymin);

	/* render */
	fdrawline(ar->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX,
		ar->v2d.cur.ymax,
		ar->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX,
		ar->v2d.cur.ymin);
}

static void restrictbutton_view_cb(bContext *C, void *poin, void *poin2)
{
	Base *base;
	Scene *scene = (Scene *)poin;
	Object *ob = (Object *)poin2;
	
	/* deselect objects that are invisible */
	if (ob->restrictflag & OB_RESTRICT_VIEW) {
	
		/* Ouch! There is no backwards pointer from Object to Base, 
		 * so have to do loop to find it. */
		for(base= FIRSTBASE; base; base= base->next) {
			if(base->object==ob) {
				base->flag &= ~SELECT;
				base->object->flag= base->flag;
			}
		}
	}
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);

}

static void restrictbutton_sel_cb(bContext *C, void *poin, void *poin2)
{
	Base *base;
	Scene *scene = (Scene *)poin;
	Object *ob = (Object *)poin2;
	
	/* if select restriction has just been turned on */
	if (ob->restrictflag & OB_RESTRICT_SELECT) {
	
		/* Ouch! There is no backwards pointer from Object to Base, 
		 * so have to do loop to find it. */
		for(base= FIRSTBASE; base; base= base->next) {
			if(base->object==ob) {
				base->flag &= ~SELECT;
				base->object->flag= base->flag;
			}
		}
	}
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);

}

static void restrictbutton_rend_cb(bContext *C, void *poin, void *poin2)
{
	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, poin);
}

static void restrictbutton_r_lay_cb(bContext *C, void *poin, void *poin2)
{
	WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, poin);
}

static void restrictbutton_modifier_cb(bContext *C, void *poin, void *poin2)
{
	Scene *scene = (Scene *)poin;
	Object *ob = (Object *)poin2;
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	object_handle_update(scene, ob);

	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
}

static void restrictbutton_bone_cb(bContext *C, void *poin, void *poin2)
{
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, NULL);
}

static void namebutton_cb(bContext *C, void *tsep, char *oldname)
{
	SpaceOops *soops= CTX_wm_space_outliner(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	TreeStore *ts= soops->treestore;
	TreeStoreElem *tselem= tsep;
	
	if(ts && tselem) {
		TreeElement *te= outliner_find_tse(soops, tselem);
		
		if(tselem->type==0) {
			test_idbutton(tselem->id->name+2);	// library.c, unique name and alpha sort
			
			switch(GS(tselem->id->name)) {
				case ID_MA:
					WM_event_add_notifier(C, NC_MATERIAL, NULL); break;
				case ID_TE:
					WM_event_add_notifier(C, NC_TEXTURE, NULL); break;
				case ID_IM:
					WM_event_add_notifier(C, NC_IMAGE, NULL); break;
				case ID_SCE:
					WM_event_add_notifier(C, NC_SCENE, NULL); break;
				default:
					WM_event_add_notifier(C, NC_ID|NA_RENAME, NULL); break;
			}					
			/* Check the library target exists */
			if (te->idcode == ID_LI) {
				char expanded[FILE_MAXDIR + FILE_MAXFILE];
				BLI_strncpy(expanded, ((Library *)tselem->id)->name, FILE_MAXDIR + FILE_MAXFILE);
				BLI_path_abs(expanded, G.sce);
				if (!BLI_exists(expanded)) {
					error("This path does not exist, correct this before saving");
				}
			}
		}
		else {
			switch(tselem->type) {
			case TSE_DEFGROUP:
				defgroup_unique_name(te->directdata, (Object *)tselem->id); //	id = object
				break;
			case TSE_NLA_ACTION:
				test_idbutton(tselem->id->name+2);
				break;
			case TSE_EBONE:
			{
				bArmature *arm= (bArmature *)tselem->id;
				if(arm->edbo) {
					EditBone *ebone= te->directdata;
					char newname[32];
					
					/* restore bone name */
					BLI_strncpy(newname, ebone->name, 32);
					BLI_strncpy(ebone->name, oldname, 32);
					ED_armature_bone_rename(obedit->data, oldname, newname);
					WM_event_add_notifier(C, NC_OBJECT|ND_POSE, OBACT);
				}
			}
				break;

			case TSE_BONE:
				{
					Bone *bone= te->directdata;
					Object *ob;
					char newname[32];
					
					// always make current object active
					tree_element_set_active_object(C, scene, soops, te, 1);
					ob= OBACT;
					
					/* restore bone name */
					BLI_strncpy(newname, bone->name, 32);
					BLI_strncpy(bone->name, oldname, 32);
					ED_armature_bone_rename(ob->data, oldname, newname);
					WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
				}
				break;
			case TSE_POSE_CHANNEL:
				{
					bPoseChannel *pchan= te->directdata;
					Object *ob;
					char newname[32];
					
					// always make current object active
					tree_element_set_active_object(C, scene, soops, te, 1);
					ob= OBACT;
					
					/* restore bone name */
					BLI_strncpy(newname, pchan->name, 32);
					BLI_strncpy(pchan->name, oldname, 32);
					ED_armature_bone_rename(ob->data, oldname, newname);
					WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
				}
				break;
			case TSE_POSEGRP:
				{
					Object *ob= (Object *)tselem->id; // id = object
					bActionGroup *grp= te->directdata;
					
					BLI_uniquename(&ob->pose->agroups, grp, "Group", '.', offsetof(bActionGroup, name), sizeof(grp->name));
					WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
				}
				break;
			case TSE_R_LAYER:
				break;
			}
		}
		tselem->flag &= ~TSE_TEXTBUT;
	}
}

static void outliner_draw_restrictbuts(uiBlock *block, Scene *scene, ARegion *ar, SpaceOops *soops, ListBase *lb)
{	
	uiBut *bt;
	TreeElement *te;
	TreeStoreElem *tselem;
	Object *ob = NULL;

	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(te->ys+2*OL_H >= ar->v2d.cur.ymin && te->ys <= ar->v2d.cur.ymax) {	
			/* objects have toggle-able restriction flags */
			if(tselem->type==0 && te->idcode==ID_OB) {
				ob = (Object *)tselem->id;
				
				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitS(block, ICONTOG, OB_RESTRICT_VIEW, 0, ICON_RESTRICT_VIEW_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, (short)te->ys, 17, OL_H-1, &(ob->restrictflag), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_view_cb, scene, ob);
				
				bt= uiDefIconButBitS(block, ICONTOG, OB_RESTRICT_SELECT, 0, ICON_RESTRICT_SELECT_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX, (short)te->ys, 17, OL_H-1, &(ob->restrictflag), 0, 0, 0, 0, "Restrict/Allow selection in the 3D View");
				uiButSetFunc(bt, restrictbutton_sel_cb, scene, ob);
				
				bt= uiDefIconButBitS(block, ICONTOG, OB_RESTRICT_RENDER, 0, ICON_RESTRICT_RENDER_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX, (short)te->ys, 17, OL_H-1, &(ob->restrictflag), 0, 0, 0, 0, "Restrict/Allow renderability");
				uiButSetFunc(bt, restrictbutton_rend_cb, scene, ob);
				
				uiBlockSetEmboss(block, UI_EMBOSS);
			}
			/* scene render layers and passes have toggle-able flags too! */
			else if(tselem->type==TSE_R_LAYER) {
				uiBlockSetEmboss(block, UI_EMBOSSN);
				
				bt= uiDefIconButBitI(block, ICONTOGN, SCE_LAY_DISABLE, 0, ICON_CHECKBOX_HLT-1, 
									 (int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, (short)te->ys, 17, OL_H-1, te->directdata, 0, 0, 0, 0, "Render this RenderLayer");
				uiButSetFunc(bt, restrictbutton_r_lay_cb, tselem->id, NULL);
				
				uiBlockSetEmboss(block, UI_EMBOSS);
			}
			else if(tselem->type==TSE_R_PASS) {
				int *layflag= te->directdata;
				uiBlockSetEmboss(block, UI_EMBOSSN);
				
				/* NOTE: tselem->nr is short! */
				bt= uiDefIconButBitI(block, ICONTOG, tselem->nr, 0, ICON_CHECKBOX_HLT-1, 
									 (int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, (short)te->ys, 17, OL_H-1, layflag, 0, 0, 0, 0, "Render this Pass");
				uiButSetFunc(bt, restrictbutton_r_lay_cb, tselem->id, NULL);
				
				layflag++;	/* is lay_xor */
				if(ELEM8(tselem->nr, SCE_PASS_SPEC, SCE_PASS_SHADOW, SCE_PASS_AO, SCE_PASS_REFLECT, SCE_PASS_REFRACT, SCE_PASS_INDIRECT, SCE_PASS_EMIT, SCE_PASS_ENVIRONMENT))
					bt= uiDefIconButBitI(block, TOG, tselem->nr, 0, (*layflag & tselem->nr)?ICON_DOT:ICON_BLANK1, 
									 (int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX, (short)te->ys, 17, OL_H-1, layflag, 0, 0, 0, 0, "Exclude this Pass from Combined");
				uiButSetFunc(bt, restrictbutton_r_lay_cb, tselem->id, NULL);
				
				uiBlockSetEmboss(block, UI_EMBOSS);
			}
			else if(tselem->type==TSE_MODIFIER)  {
				ModifierData *md= (ModifierData *)te->directdata;
				ob = (Object *)tselem->id;
				
				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitI(block, ICONTOGN, eModifierMode_Realtime, 0, ICON_RESTRICT_VIEW_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, (short)te->ys, 17, OL_H-1, &(md->mode), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_modifier_cb, scene, ob);
				
				bt= uiDefIconButBitI(block, ICONTOGN, eModifierMode_Render, 0, ICON_RESTRICT_RENDER_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX, (short)te->ys, 17, OL_H-1, &(md->mode), 0, 0, 0, 0, "Restrict/Allow renderability");
				uiButSetFunc(bt, restrictbutton_modifier_cb, scene, ob);
			}
			else if(tselem->type==TSE_POSE_CHANNEL)  {
				bPoseChannel *pchan= (bPoseChannel *)te->directdata;
				Bone *bone = pchan->bone;
				
				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitI(block, ICONTOG, BONE_HIDDEN_P, 0, ICON_RESTRICT_VIEW_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, (short)te->ys, 17, OL_H-1, &(bone->flag), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_bone_cb, NULL, NULL);
				
				bt= uiDefIconButBitI(block, ICONTOG, BONE_UNSELECTABLE, 0, ICON_RESTRICT_SELECT_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX, (short)te->ys, 17, OL_H-1, &(bone->flag), 0, 0, 0, 0, "Restrict/Allow selection in the 3D View");
				uiButSetFunc(bt, restrictbutton_bone_cb, NULL, NULL);
			}
			else if(tselem->type==TSE_EBONE)  {
				EditBone *ebone= (EditBone *)te->directdata;
				
				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitI(block, ICONTOG, BONE_HIDDEN_A, 0, ICON_RESTRICT_VIEW_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, (short)te->ys, 17, OL_H-1, &(ebone->flag), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_bone_cb, NULL, NULL);
				
				bt= uiDefIconButBitI(block, ICONTOG, BONE_UNSELECTABLE, 0, ICON_RESTRICT_SELECT_OFF, 
						(int)ar->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX, (short)te->ys, 17, OL_H-1, &(ebone->flag), 0, 0, 0, 0, "Restrict/Allow selection in the 3D View");
				uiButSetFunc(bt, restrictbutton_bone_cb, NULL, NULL);
			}
		}
		
		if((tselem->flag & TSE_CLOSED)==0) outliner_draw_restrictbuts(block, scene, ar, soops, &te->subtree);
	}
}

static void outliner_draw_rnacols(ARegion *ar, SpaceOops *soops, int sizex)
{
	View2D *v2d= &ar->v2d;
	
	UI_ThemeColorShadeAlpha(TH_BACK, -15, -200);

	/* draw column separator lines */
	fdrawline((float)sizex,
		v2d->cur.ymax,
		(float)sizex,
		v2d->cur.ymin);

	fdrawline((float)sizex+OL_RNA_COL_SIZEX,
		v2d->cur.ymax,
		(float)sizex+OL_RNA_COL_SIZEX,
		v2d->cur.ymin);
}

static void outliner_draw_rnabuts(uiBlock *block, Scene *scene, ARegion *ar, SpaceOops *soops, int sizex, ListBase *lb)
{	
	TreeElement *te;
	TreeStoreElem *tselem;
	PointerRNA *ptr;
	PropertyRNA *prop;
	
	uiBlockSetEmboss(block, UI_EMBOSST);

	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(te->ys+2*OL_H >= ar->v2d.cur.ymin && te->ys <= ar->v2d.cur.ymax) {	
			if(tselem->type == TSE_RNA_PROPERTY) {
				ptr= &te->rnaptr;
				prop= te->directdata;
				
				if(!(RNA_property_type(prop) == PROP_POINTER && (tselem->flag & TSE_CLOSED)==0))
					uiDefAutoButR(block, ptr, prop, -1, "", 0, sizex, (int)te->ys, OL_RNA_COL_SIZEX, OL_H-1);
			}
			else if(tselem->type == TSE_RNA_ARRAY_ELEM) {
				ptr= &te->rnaptr;
				prop= te->directdata;
				
				uiDefAutoButR(block, ptr, prop, te->index, "", 0, sizex, (int)te->ys, OL_RNA_COL_SIZEX, OL_H-1);
			}
		}
		
		if((tselem->flag & TSE_CLOSED)==0) outliner_draw_rnabuts(block, scene, ar, soops, sizex, &te->subtree);
	}
}

static void operator_call_cb(struct bContext *C, void *arg_kmi, void *arg2)
{
	wmOperatorType *ot= arg2;
	wmKeyMapItem *kmi= arg_kmi;
	
	if(ot)
		BLI_strncpy(kmi->idname, ot->idname, OP_MAX_TYPENAME);
}

static void operator_search_cb(const struct bContext *C, void *arg_kmi, char *str, uiSearchItems *items)
{
	wmOperatorType *ot = WM_operatortype_first();
	
	for(; ot; ot= ot->next) {
		
		if(BLI_strcasestr(ot->idname, str)) {
			char name[OP_MAX_TYPENAME];
			
			/* display name for menu */
			WM_operator_py_idname(name, ot->idname);
			
			if(0==uiSearchItemAdd(items, name, ot, 0))
				break;
		}
	}
}

/* operator Search browse menu, open */
static uiBlock *operator_search_menu(bContext *C, ARegion *ar, void *arg_kmi)
{
	static char search[OP_MAX_TYPENAME];
	wmEvent event;
	wmWindow *win= CTX_wm_window(C);
	wmKeyMapItem *kmi= arg_kmi;
	wmOperatorType *ot= WM_operatortype_find(kmi->idname, 0);
	uiBlock *block;
	uiBut *but;
	
	/* clear initial search string, then all items show */
	search[0]= 0;
	
	block= uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_LOOP|UI_BLOCK_REDRAW|UI_BLOCK_RET_1);
	
	/* fake button, it holds space for search items */
	uiDefBut(block, LABEL, 0, "", 10, 15, 150, uiSearchBoxhHeight(), NULL, 0, 0, 0, 0, NULL);
	
	but= uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, 256, 10, 0, 150, 19, 0, 0, "");
	uiButSetSearchFunc(but, operator_search_cb, arg_kmi, operator_call_cb, ot);
	
	uiBoundsBlock(block, 6);
	uiBlockSetDirection(block, UI_DOWN);	
	uiEndBlock(C, block);
	
	event= *(win->eventstate);	/* XXX huh huh? make api call */
	event.type= EVT_BUT_OPEN;
	event.val= KM_PRESS;
	event.customdata= but;
	event.customdatafree= FALSE;
	wm_event_add(win, &event);
	
	return block;
}

#define OL_KM_KEYBOARD		0
#define OL_KM_MOUSE			1
#define OL_KM_TWEAK			2
#define OL_KM_SPECIALS		3

static short keymap_menu_type(short type)
{
	if(ISKEYBOARD(type)) return OL_KM_KEYBOARD;
	if(ISTWEAK(type)) return OL_KM_TWEAK;
	if(ISMOUSE(type)) return OL_KM_MOUSE;
//	return OL_KM_SPECIALS;
	return 0;
}

static char *keymap_type_menu(void)
{
	static char string[500];
	static char formatstr[] = "|%s %%x%d";
	char *str= string;
	
	str += sprintf(str, "Event Type %%t");
	
	str += sprintf(str, formatstr, "Keyboard", OL_KM_KEYBOARD);
	str += sprintf(str, formatstr, "Mouse", OL_KM_MOUSE);
	str += sprintf(str, formatstr, "Tweak", OL_KM_TWEAK);
//	str += sprintf(str, formatstr, "Specials", OL_KM_SPECIALS);
	
	return string;
}	

static char *keymap_mouse_menu(void)
{
	static char string[500];
	static char formatstr[] = "|%s %%x%d";
	char *str= string;
	
	str += sprintf(str, "Mouse Event %%t");
	
	str += sprintf(str, formatstr, "Left Mouse", LEFTMOUSE);
	str += sprintf(str, formatstr, "Middle Mouse", MIDDLEMOUSE);
	str += sprintf(str, formatstr, "Right Mouse", RIGHTMOUSE);
	str += sprintf(str, formatstr, "Button4 Mouse ", BUTTON4MOUSE);
	str += sprintf(str, formatstr, "Button5 Mouse ", BUTTON5MOUSE);
	str += sprintf(str, formatstr, "Action Mouse", ACTIONMOUSE);
	str += sprintf(str, formatstr, "Select Mouse", SELECTMOUSE);
	str += sprintf(str, formatstr, "Mouse Move", MOUSEMOVE);
	str += sprintf(str, formatstr, "Wheel Up", WHEELUPMOUSE);
	str += sprintf(str, formatstr, "Wheel Down", WHEELDOWNMOUSE);
	str += sprintf(str, formatstr, "Wheel In", WHEELINMOUSE);
	str += sprintf(str, formatstr, "Wheel Out", WHEELOUTMOUSE);
	str += sprintf(str, formatstr, "Mouse/Trackpad Pan", MOUSEPAN);
	str += sprintf(str, formatstr, "Mouse/Trackpad Zoom", MOUSEZOOM);
	str += sprintf(str, formatstr, "Mouse/Trackpad Rotate", MOUSEROTATE);
	
	return string;
}

static char *keymap_tweak_menu(void)
{
	static char string[500];
	static char formatstr[] = "|%s %%x%d";
	char *str= string;
	
	str += sprintf(str, "Tweak Event %%t");
	
	str += sprintf(str, formatstr, "Left Mouse", EVT_TWEAK_L);
	str += sprintf(str, formatstr, "Middle Mouse", EVT_TWEAK_M);
	str += sprintf(str, formatstr, "Right Mouse", EVT_TWEAK_R);
	str += sprintf(str, formatstr, "Action Mouse", EVT_TWEAK_A);
	str += sprintf(str, formatstr, "Select Mouse", EVT_TWEAK_S);
	
	return string;
}

static char *keymap_tweak_dir_menu(void)
{
	static char string[500];
	static char formatstr[] = "|%s %%x%d";
	char *str= string;
	
	str += sprintf(str, "Tweak Direction %%t");
	
	str += sprintf(str, formatstr, "Any", KM_ANY);
	str += sprintf(str, formatstr, "North", EVT_GESTURE_N);
	str += sprintf(str, formatstr, "North-East", EVT_GESTURE_NE);
	str += sprintf(str, formatstr, "East", EVT_GESTURE_E);
	str += sprintf(str, formatstr, "Sout-East", EVT_GESTURE_SE);
	str += sprintf(str, formatstr, "South", EVT_GESTURE_S);
	str += sprintf(str, formatstr, "South-West", EVT_GESTURE_SW);
	str += sprintf(str, formatstr, "West", EVT_GESTURE_W);
	str += sprintf(str, formatstr, "North-West", EVT_GESTURE_NW);
	
	return string;
}


static void keymap_type_cb(bContext *C, void *kmi_v, void *unused_v)
{
	wmKeyMapItem *kmi= kmi_v;
	short maptype= keymap_menu_type(kmi->type);
	
	if(maptype!=kmi->maptype) {
		switch(kmi->maptype) {
			case OL_KM_KEYBOARD:
				kmi->type= AKEY;
				kmi->val= KM_PRESS;
				break;
			case OL_KM_MOUSE:
				kmi->type= LEFTMOUSE;
				kmi->val= KM_PRESS;
				break;
			case OL_KM_TWEAK:
				kmi->type= EVT_TWEAK_L;
				kmi->val= KM_ANY;
				break;
			case OL_KM_SPECIALS:
				kmi->type= AKEY;
				kmi->val= KM_PRESS;
		}
		ED_region_tag_redraw(CTX_wm_region(C));
	}
}

static void outliner_draw_keymapbuts(uiBlock *block, ARegion *ar, SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	uiBlockSetEmboss(block, UI_EMBOSST);
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(te->ys+2*OL_H >= ar->v2d.cur.ymin && te->ys <= ar->v2d.cur.ymax) {
			uiBut *but;
			char *str;
			int xstart= 240;
			int butw1= 20; /* operator */
			int butw2= 90; /* event type, menus */
			int butw3= 43; /* modifiers */

			if(tselem->type == TSE_KEYMAP_ITEM) {
				wmKeyMapItem *kmi= te->directdata;
				
				/* modal map? */
				if(kmi->propvalue);
				else {
					uiDefBlockBut(block, operator_search_menu, kmi, "", xstart, (int)te->ys+1, butw1, OL_H-1, "Assign new Operator");
				}
				xstart+= butw1+10;
				
				/* map type button */
				kmi->maptype= keymap_menu_type(kmi->type);
				
				str= keymap_type_menu();
				but= uiDefButS(block, MENU, 0, str,	xstart, (int)te->ys+1, butw2, OL_H-1, &kmi->maptype, 0, 0, 0, 0, "Event type");
				uiButSetFunc(but, keymap_type_cb, kmi, NULL);
				xstart+= butw2+5;
				
				/* edit actual event */
				switch(kmi->maptype) {
					case OL_KM_KEYBOARD:
						uiDefKeyevtButS(block, 0, "", xstart, (int)te->ys+1, butw2, OL_H-1, &kmi->type, "Key code");
						xstart+= butw2+5;
						break;
					case OL_KM_MOUSE:
						str= keymap_mouse_menu();
						uiDefButS(block, MENU, 0, str, xstart,(int)te->ys+1, butw2, OL_H-1, &kmi->type, 0, 0, 0, 0,  "Mouse button");	
						xstart+= butw2+5;
						break;
					case OL_KM_TWEAK:
						str= keymap_tweak_menu();
						uiDefButS(block, MENU, 0, str, xstart, (int)te->ys+1, butw2, OL_H-1, &kmi->type, 0, 0, 0, 0,  "Tweak gesture");	
						xstart+= butw2+5;
						str= keymap_tweak_dir_menu();
						uiDefButS(block, MENU, 0, str, xstart, (int)te->ys+1, butw2, OL_H-1, &kmi->val, 0, 0, 0, 0,  "Tweak gesture direction");	
						xstart+= butw2+5;
						break;
				}
				
				/* modifiers */
				uiDefButS(block, OPTION, 0, "Shift",	xstart, (int)te->ys+1, butw3+5, OL_H-1, &kmi->shift, 0, 0, 0, 0, "Modifier"); xstart+= butw3+5;
				uiDefButS(block, OPTION, 0, "Ctrl",	xstart, (int)te->ys+1, butw3, OL_H-1, &kmi->ctrl, 0, 0, 0, 0, "Modifier"); xstart+= butw3;
				uiDefButS(block, OPTION, 0, "Alt",	xstart, (int)te->ys+1, butw3, OL_H-1, &kmi->alt, 0, 0, 0, 0, "Modifier"); xstart+= butw3;
				uiDefButS(block, OPTION, 0, "Cmd",	xstart, (int)te->ys+1, butw3, OL_H-1, &kmi->oskey, 0, 0, 0, 0, "Modifier"); xstart+= butw3;
				xstart+= 5;
				uiDefKeyevtButS(block, 0, "", xstart, (int)te->ys+1, butw3, OL_H-1, &kmi->keymodifier, "Key Modifier code");
				xstart+= butw3+5;
				
				/* rna property */
				if(kmi->ptr && kmi->ptr->data)
					uiDefBut(block, LABEL, 0, "(RNA property)",	xstart, (int)te->ys+1, butw2, OL_H-1, &kmi->oskey, 0, 0, 0, 0, ""); xstart+= butw2;
					
				
			}
		}
		
		if((tselem->flag & TSE_CLOSED)==0) outliner_draw_keymapbuts(block, ar, soops, &te->subtree);
	}
}


static void outliner_buttons(const bContext *C, uiBlock *block, ARegion *ar, SpaceOops *soops, ListBase *lb)
{
	uiBut *bt;
	TreeElement *te;
	TreeStoreElem *tselem;
	int dx, len;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(te->ys+2*OL_H >= ar->v2d.cur.ymin && te->ys <= ar->v2d.cur.ymax) {
			
			if(tselem->flag & TSE_TEXTBUT) {
				
				/* If we add support to rename Sequence.
				 * need change this.
				 */
				if(tselem->type == TSE_POSE_BASE) continue; // prevent crash when trying to rename 'pose' entry of armature
				
				if(tselem->type==TSE_EBONE) len = sizeof(((EditBone*) 0)->name);
				else if (tselem->type==TSE_MODIFIER) len = sizeof(((ModifierData*) 0)->name);
				else if(tselem->id && GS(tselem->id->name)==ID_LI) len = sizeof(((Library*) 0)->name);
				else len= sizeof(((ID*) 0)->name)-2;
				

				dx= (int)UI_GetStringWidth(te->name);
				if(dx<100) dx= 100;
				
				bt= uiDefBut(block, TEX, OL_NAMEBUTTON, "",  (short)te->xs+2*OL_X-4, (short)te->ys, dx+10, OL_H-1, te->name, 1.0, (float)len-1, 0, 0, "");
				uiButSetRenameFunc(bt, namebutton_cb, tselem);
				
				/* returns false if button got removed */
				if( 0 == uiButActiveOnly(C, block, bt) )
					tselem->flag &= ~TSE_TEXTBUT;
			}
		}
		
		if((tselem->flag & TSE_CLOSED)==0) outliner_buttons(C, block, ar, soops, &te->subtree);
	}
}

void draw_outliner(const bContext *C)
{
	Main *mainvar= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	View2D *v2d= &ar->v2d;
	SpaceOops *soops= CTX_wm_space_outliner(C);
	uiBlock *block;
	int sizey= 0, sizex= 0, sizex_rna= 0;
	
	outliner_build_tree(mainvar, scene, soops); // always 
	
	/* get extents of data */
	outliner_height(soops, &soops->tree, &sizey);

	if (ELEM3(soops->outlinevis, SO_DATABLOCKS, SO_USERDEF, SO_KEYMAP)) {
		/* RNA has two columns:
		 * 	- column 1 is (max_width + OL_RNA_COL_SPACEX) or
		 *				 (OL_RNA_COL_X), whichever is wider...
		 *	- column 2 is fixed at OL_RNA_COL_SIZEX
		 *
		 *  (*) XXX max width for now is a fixed factor of OL_X*(max_indention+100)
		 */
		 
		/* get actual width of column 1 */
		outliner_rna_width(soops, &soops->tree, &sizex_rna, 0);
		sizex_rna= MAX2(OL_RNA_COLX, sizex_rna+OL_RNA_COL_SPACEX);
		
		/* get width of data (for setting 'tot' rect, this is column 1 + column 2 + a bit extra) */
		if (soops->outlinevis == SO_KEYMAP) 
			sizex= sizex_rna + OL_RNA_COL_SIZEX*3 + 50; // XXX this is only really a quick hack to make this wide enough...
		else
			sizex= sizex_rna + OL_RNA_COL_SIZEX + 50;
	}
	else {
		/* width must take into account restriction columns (if visible) so that entries will still be visible */
		//outliner_width(soops, &soops->tree, &sizex);
		outliner_rna_width(soops, &soops->tree, &sizex, 0); // XXX should use outliner_width instead when te->xend will be set correctly...
		
		/* constant offset for restriction columns */
		// XXX this isn't that great yet...
		if ((soops->flag & SO_HIDE_RESTRICTCOLS)==0)
			sizex += OL_TOGW*3;
	}
	
	/* tweak to display last line (when list bigger than window) */
	sizey += V2D_SCROLL_HEIGHT;
	
	/* update size of tot-rect (extents of data/viewable area) */
	UI_view2d_totRect_set(v2d, sizex, sizey);

	/* set matrix for 2d-view controls */
	UI_view2d_view_ortho(C, v2d);

	/* draw outliner stuff (background and hierachy lines) */
	outliner_back(ar, soops);
	block= uiBeginBlock(C, ar, "outliner buttons", UI_EMBOSS);
	outliner_draw_tree((bContext *)C, block, scene, ar, soops);

	/* draw icons and names */
	outliner_buttons(C, block, ar, soops, &soops->tree);
	
	if(ELEM(soops->outlinevis, SO_DATABLOCKS, SO_USERDEF)) {
		/* draw rna buttons */
		outliner_draw_rnacols(ar, soops, sizex_rna);
		outliner_draw_rnabuts(block, scene, ar, soops, sizex_rna, &soops->tree);
	}
	else if(soops->outlinevis == SO_KEYMAP) {
		outliner_draw_keymapbuts(block, ar, soops, &soops->tree);
	}
	else if (!(soops->flag & SO_HIDE_RESTRICTCOLS)) {
		/* draw restriction columns */
		outliner_draw_restrictcols(ar, soops);
		outliner_draw_restrictbuts(block, scene, ar, soops, &soops->tree);
	}
	
	uiEndBlock(C, block);
	uiDrawBlock(C, block);
	
	/* clear flag that allows quick redraws */
	soops->storeflag &= ~SO_TREESTORE_REDRAW;
}

