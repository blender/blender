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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "DNA_oops_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"

#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_butspace.h"
#include "BIF_drawscene.h"
#include "BIF_drawtext.h"
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editdeform.h"
#include "BIF_editnla.h"
#include "BIF_editview.h"
#include "BIF_editconstraint.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_mywindow.h"
#include "BIF_outliner.h"
#include "BIF_language.h"
#include "BIF_mainqueue.h"
#include "BIF_poseobject.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "BDR_editobject.h"
#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_view.h"

#include "PIL_time.h" 

#include "blendef.h"
#include "mydevice.h"

#define OL_H	19
#define OL_X	18

#define OL_TOG_RESTRICT_VIEWX	54
#define OL_TOG_RESTRICT_SELECTX	36
#define OL_TOG_RESTRICT_RENDERX	18

#define OL_TOGW				OL_TOG_RESTRICT_VIEWX

#define TS_CHUNK	128

#define TREESTORE(a) ((a)?soops->treestore->data+(a)->store_index:NULL)

#ifdef WITH_VERSE
extern ListBase session_list;
extern ListBase server_list;
#endif


/* ******************** PROTOTYPES ***************** */
static void outliner_draw_tree_element(SpaceOops *soops, TreeElement *te, int startx, int *starty);
static void outliner_do_object_operation(SpaceOops *soops, ListBase *lb, 
										 void (*operation_cb)(TreeElement *, TreeStoreElem *, TreeStoreElem *));


/* ******************** PERSISTANT DATA ***************** */

static void outliner_storage_cleanup(SpaceOops *soops)
{
	TreeStore *ts= soops->treestore;
	
	if(ts) {
		TreeStoreElem *tselem;
		int a, unused= 0;
		
		/* each element used once, for ID blocks with more users to have each a treestore */
		for(a=0, tselem= ts->data; a<ts->usedelem; a++, tselem++) tselem->used= 0;

		/* cleanup only after reading file or undo step */
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
		(*h)++;
		te= te->next;
	}
}

static void outliner_width(SpaceOops *soops, ListBase *lb, int *w)
{
	TreeElement *te= lb->first;
	while(te) {
		TreeStoreElem *tselem= TREESTORE(te);
		if(tselem->flag & TSE_CLOSED) {
			if (te->xend > *w)
				*w = te->xend;
		}
		outliner_width(soops, &te->subtree, w);
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
		if(te->idcode==idcode && tselem->type==0) return tselem->id;
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
	
	te= outliner_add_element(soops, &tenla->subtree, id, tenla, TSE_R_PASS, SCE_PASS_RADIO);
	te->name= "Radiosity";
	te->directdata= &srl->passflag;
	
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
	
	if(sce->scriptlink.scripts) {
		int a= 0;
		tenla= outliner_add_element(soops,  lb, sce, te, TSE_SCRIPT_BASE, 0);
		tenla->name= "Scripts";
		for (a=0; a<sce->scriptlink.totscript; a++) {
			outliner_add_element(soops, &tenla->subtree, sce->scriptlink.scripts[a], tenla, 0, 0);
		}
	}

}

static TreeElement *outliner_add_element(SpaceOops *soops, ListBase *lb, void *idv, 
										 TreeElement *parent, short type, short index)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	ID *id= idv;
	int a;
	
	if(id==NULL) return NULL;

	te= MEM_callocN(sizeof(TreeElement), "tree elem");
	/* add to the visual tree */
	BLI_addtail(lb, te);
	/* add to the storage */
	check_persistant(soops, te, id, type, index);
	tselem= TREESTORE(te);	
	
	te->parent= parent;
	te->index= index;	// for data arays
	te->name= id->name+2; // default, can be overridden by Library or non-ID data
	te->idcode= GS(id->name);
	
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
				
				if(ob->proxy && ob->id.lib==NULL)
					outliner_add_element(soops, &te->subtree, ob->proxy, te, TSE_PROXY, 0);
				
				outliner_add_element(soops, &te->subtree, ob->data, te, 0, 0);
				
				if(ob->pose) {
					bPoseChannel *pchan;
					TreeElement *ten;
					TreeElement *tenla= outliner_add_element(soops, &te->subtree, ob, te, TSE_POSE_BASE, 0);
					
					tenla->name= "Pose";
					
					if(ob!=G.obedit && (ob->flag & OB_POSEMODE)) {	// channels undefined in editmode, but we want the 'tenla' pose icon itself
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
				
				outliner_add_element(soops, &te->subtree, ob->ipo, te, 0, 0);
				outliner_add_element(soops, &te->subtree, ob->action, te, 0, 0);
				
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
				if(ob->scriptlink.scripts) {
					TreeElement *tenla= outliner_add_element(soops, &te->subtree, ob, te, TSE_SCRIPT_BASE, 0);
					int a= 0;
					
					tenla->name= "Scripts";
					for (a=0; a<ob->scriptlink.totscript; a++) {							/*  ** */
						outliner_add_element(soops, &tenla->subtree, ob->scriptlink.scripts[a], te, 0, 0);
					}
				}
				
				if(ob->dup_group)
					outliner_add_element(soops, &te->subtree, ob->dup_group, te, 0, 0);

				if(ob->nlastrips.first) {
					bActionStrip *strip;
					TreeElement *ten;
					TreeElement *tenla= outliner_add_element(soops, &te->subtree, ob, te, TSE_NLA, 0);
					int a= 0;
					
					tenla->name= "NLA strips";
					for (strip=ob->nlastrips.first; strip; strip=strip->next, a++) {
						ten= outliner_add_element(soops, &tenla->subtree, strip->act, tenla, TSE_NLA_ACTION, a);
						if(ten) ten->directdata= strip;
					}
				}
				
			}
			break;
		case ID_ME:
			{
				Mesh *me= (Mesh *)id;
				outliner_add_element(soops, &te->subtree, me->ipo, te, 0, 0);
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
			
			outliner_add_element(soops, &te->subtree, ma->ipo, te, 0, 0);
			for(a=0; a<MAX_MTEX; a++) {
				if(ma->mtex[a]) outliner_add_element(soops, &te->subtree, ma->mtex[a]->tex, te, 0, a);
			}
		}
			break;
		case ID_TE:
			{
				Tex *tex= (Tex *)id;
				
				outliner_add_element(soops, &te->subtree, tex->ipo, te, 0, 0);
				outliner_add_element(soops, &te->subtree, tex->ima, te, 0, 0);
			}
			break;
		case ID_CA:
			{
				Camera *ca= (Camera *)id;
				outliner_add_element(soops, &te->subtree, ca->ipo, te, 0, 0);
			}
			break;
		case ID_LA:
			{
				Lamp *la= (Lamp *)id;
				outliner_add_element(soops, &te->subtree, la->ipo, te, 0, 0);
				for(a=0; a<MAX_MTEX; a++) {
					if(la->mtex[a]) outliner_add_element(soops, &te->subtree, la->mtex[a]->tex, te, 0, a);
				}
			}
			break;
		case ID_WO:
			{
				World *wrld= (World *)id;
				outliner_add_element(soops, &te->subtree, wrld->ipo, te, 0, 0);
				for(a=0; a<MAX_MTEX; a++) {
					if(wrld->mtex[a]) outliner_add_element(soops, &te->subtree, wrld->mtex[a]->tex, te, 0, a);
				}
			}
			break;
		case ID_KE:
			{
				Key *key= (Key *)id;
				outliner_add_element(soops, &te->subtree, key->ipo, te, 0, 0);
			}
			break;
		case ID_IP:
			{
				Ipo *ipo= (Ipo *)id;
				IpoCurve *icu;
				Object *lastadded= NULL;
				
				for (icu= ipo->curve.first; icu; icu= icu->next) {
					if (icu->driver && icu->driver->ob) {
						if (lastadded != icu->driver->ob) {
							outliner_add_element(soops, &te->subtree, icu->driver->ob, te, TSE_LINKED_OB, 0);
							lastadded= icu->driver->ob;
						}
					}
				}
			}
			break;
		case ID_AC:
			{
				bAction *act= (bAction *)id;
				bActionChannel *chan;
				int a= 0;
				
				tselem= TREESTORE(parent);
				for (chan=act->chanbase.first; chan; chan=chan->next, a++) {
					outliner_add_element(soops, &te->subtree, chan->ipo, te, 0, a);
				}
			}
			break;
		case ID_AR:
			{
				bArmature *arm= (bArmature *)id;
				int a= 0;
				
				if(G.obedit && G.obedit->data==arm) {
					EditBone *ebone;
					TreeElement *ten;
					
					for (ebone = G.edbo.first; ebone; ebone=ebone->next, a++) {
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
					if( GS(tselem->id->name)==ID_OB && ((Object *)tselem->id)->flag & OB_POSEMODE);
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
#ifdef WITH_VERSE
	else if(type==ID_VS) {
		struct VerseSession *session = (VerseSession*)idv;
		te->name = session->address;
		te->directdata = (void*)session;
		te->idcode = ID_VS;
	}
	else if(type==ID_MS) {
		te->name = "Available Verse Servers";
		te->idcode = ID_MS;
	}
	else if(type==ID_SS) {
		struct VerseServer *server = (VerseServer *)idv;
		te->name = server->name;
		te->directdata = (void *)server;
		te->idcode = ID_SS;
	}
	else if(type==ID_VN) {
		struct VNode *vnode = (VNode*)idv;
		te->name = vnode->name;
		te->idcode = ID_VN;
		if(vnode->type==V_NT_OBJECT) {
			struct TreeElement *ten;
			struct VNode *child_node;
			struct VLink *vlink;
			
			vlink = ((VObjectData*)vnode->data)->links.lb.first;
			while(vlink) {
				child_node = vlink->target;
				if(child_node && child_node->type==V_NT_GEOMETRY) {
					ten = outliner_add_element(soops, &te->subtree, child_node, te, ID_VN, 0);
					ten->directdata = child_node;
				}
				vlink = vlink->next;
			}
		}
	}
#endif
	return te;
}

static void outliner_make_hierarchy(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te, *ten, *tep;
	TreeStoreElem *tselem;

	/* build hierarchy */
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

static void outliner_build_tree(SpaceOops *soops)
{
	Base *base;
	Object *ob;
	TreeElement *te, *ten;
	TreeStoreElem *tselem;
	int show_opened= soops->treestore==NULL; /* on first view, we open scenes */
#ifdef WITH_VERSE
	struct VerseSession *session;
#endif

	if(soops->tree.first && (soops->storeflag & SO_TREESTORE_REDRAW))
	   return;
	   
	outliner_free_tree(&soops->tree);
	outliner_storage_cleanup(soops);
	
	/* clear ob id.new flags */
	for(ob= G.main->object.first; ob; ob= ob->id.next) ob->id.newid= NULL;
	
	/* options */
	if(soops->outlinevis == SO_LIBRARIES) {
		Library *lib;
		
		for(lib= G.main->library.first; lib; lib= lib->id.next) {
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
		for(lib= G.main->library.first; lib; lib= lib->id.next)
			lib->id.newid= NULL;
		
	}
	else if(soops->outlinevis == SO_ALL_SCENES) {
		Scene *sce;
		for(sce= G.main->scene.first; sce; sce= sce->id.next) {
			te= outliner_add_element(soops, &soops->tree, sce, NULL, 0, 0);
			tselem= TREESTORE(te);
			if(sce==G.scene && show_opened) 
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
		
		outliner_add_scene_contents(soops, &soops->tree, G.scene, NULL);
		
		for(base= G.scene->base.first; base; base= base->next) {
			ten= outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
			ten->directdata= base;
		}
		outliner_make_hierarchy(soops, &soops->tree);
	}
	else if(soops->outlinevis == SO_VISIBLE) {
		for(base= G.scene->base.first; base; base= base->next) {
			if(base->lay & G.scene->lay)
				outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
		}
		outliner_make_hierarchy(soops, &soops->tree);
	}
	else if(soops->outlinevis == SO_GROUPS) {
		Group *group;
		GroupObject *go;
		
		for(group= G.main->group.first; group; group= group->id.next) {
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
			for(base= G.scene->base.first; base; base= base->next) {
				if(base->object->type==ob->type) {
					ten= outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
					ten->directdata= base;
				}
			}
			outliner_make_hierarchy(soops, &soops->tree);
		}
	}
	else if(soops->outlinevis == SO_SELECTED) {
		for(base= G.scene->base.first; base; base= base->next) {
			if(base->lay & G.scene->lay) {
				if(base==BASACT || (base->flag & SELECT)) {
					ten= outliner_add_element(soops, &soops->tree, base->object, NULL, 0, 0);
					ten->directdata= base;
				}
			}
		}
		outliner_make_hierarchy(soops, &soops->tree);
	}
#ifdef WITH_VERSE
	else if(soops->outlinevis == SO_VERSE_SESSION) {
		/* add all session to the "root" of hierarchy */
		for(session=session_list.first; session; session = session->next) {
			struct VNode *vnode;
			if(session->flag & VERSE_CONNECTED) {
				te= outliner_add_element(soops, &soops->tree, session, NULL, ID_VS, 0);
				/* add all object nodes as childreen of session */
				for(vnode=session->nodes.lb.first; vnode; vnode=vnode->next) {
					if(vnode->type==V_NT_OBJECT) {
						ten= outliner_add_element(soops, &te->subtree, vnode, te, ID_VN, 0);
						ten->directdata= vnode;
					}
					else if(vnode->type==V_NT_BITMAP) {
						ten= outliner_add_element(soops, &te->subtree, vnode, te, ID_VN, 0);
						ten->directdata= vnode;
					}
				}
			}
		}
	}
	else if(soops->outlinevis == SO_VERSE_MS) {
		te= outliner_add_element(soops, &soops->tree, "MS", NULL, ID_MS, 0);
		if(server_list.first!=NULL) {
			struct VerseServer *server;
			/* add one main entry to root of hierarchy */
			for(server=server_list.first; server; server=server->next) {
				ten= outliner_add_element(soops, &te->subtree, server, te, ID_SS, 0);
				ten->directdata= server;
			}
		}
	}
#endif
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

void object_toggle_visibility_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, G.scene);
	if(base) {
		base->object->restrictflag^=OB_RESTRICT_VIEW;
	}
}

void outliner_toggle_visibility(struct ScrArea *sa)
{
	SpaceOops *soops= sa->spacedata.first;

	outliner_do_object_operation(soops, &soops->tree, object_toggle_visibility_cb);
	
	BIF_undo_push("Outliner toggle selectability");

	allqueue(REDRAWVIEW3D, 1);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWINFO, 1);
}

static void object_toggle_selectability_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, G.scene);
	if(base) {
		base->object->restrictflag^=OB_RESTRICT_SELECT;
	}
}

void outliner_toggle_selectability(struct ScrArea *sa)
{
	SpaceOops *soops= sa->spacedata.first;
	
	outliner_do_object_operation(soops, &soops->tree, object_toggle_selectability_cb);
	
	BIF_undo_push("Outliner toggle selectability");

	allqueue(REDRAWVIEW3D, 1);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWINFO, 1);
}

void object_toggle_renderability_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, G.scene);
	if(base) {
		base->object->restrictflag^=OB_RESTRICT_RENDER;
	}
}

void outliner_toggle_renderability(struct ScrArea *sa)
{
	SpaceOops *soops= sa->spacedata.first;

	outliner_do_object_operation(soops, &soops->tree, object_toggle_renderability_cb);
	
	BIF_undo_push("Outliner toggle renderability");

	allqueue(REDRAWVIEW3D, 1);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWINFO, 1);
}

void outliner_toggle_visible(struct ScrArea *sa)
{
	SpaceOops *soops= sa->spacedata.first;
	
	if( outliner_has_one_flag(soops, &soops->tree, TSE_CLOSED, 1))
		outliner_set_flag(soops, &soops->tree, TSE_CLOSED, 0);
	else 
		outliner_set_flag(soops, &soops->tree, TSE_CLOSED, 1);

	BIF_undo_push("Outliner toggle visible");
	scrarea_queue_redraw(sa);
}

void outliner_toggle_selected(struct ScrArea *sa)
{
	SpaceOops *soops= sa->spacedata.first;
	
	if( outliner_has_one_flag(soops, &soops->tree, TSE_SELECTED, 1))
		outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
	else 
		outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 1);
	
	BIF_undo_push("Outliner toggle selected");
	soops->storeflag |= SO_TREESTORE_REDRAW;
	scrarea_queue_redraw(sa);
}


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

void outliner_one_level(struct ScrArea *sa, int add)
{
	SpaceOops *soops= sa->spacedata.first;
	int level;
	
	level= outliner_has_one_flag(soops, &soops->tree, TSE_CLOSED, 1);
	if(add==1) {
		if(level) outliner_openclose_level(soops, &soops->tree, 1, level, 1);
	}
	else {
		if(level==0) level= outliner_count_levels(soops, &soops->tree, 0);
		if(level) outliner_openclose_level(soops, &soops->tree, 1, level-1, 0);
	}
	
	BIF_undo_push("Outliner show/hide one level");
	scrarea_queue_redraw(sa);
}

void outliner_page_up_down(ScrArea *sa, int up)
{
	SpaceOops *soops= sa->spacedata.first;
	int dy= soops->v2d.mask.ymax-soops->v2d.mask.ymin;
	
	if(up == -1) dy= -dy;
	soops->v2d.cur.ymin+= dy;
	soops->v2d.cur.ymax+= dy;
	
	soops->storeflag |= SO_TREESTORE_REDRAW;
	scrarea_queue_redraw(sa);
}

/* **** do clicks on items ******* */

static int tree_element_active_renderlayer(TreeElement *te, TreeStoreElem *tselem, int set)
{
	Scene *sce;
	
	/* paranoia check */
	if(te->idcode!=ID_SCE)
		return 0;
	sce= (Scene *)tselem->id;
	
	if(set) {
		sce->r.actlay= tselem->nr;
		allqueue(REDRAWBUTSSCENE, 0);
	}
	else {
		return sce->r.actlay==tselem->nr;
	}
	return 0;
}

static void tree_element_active_object(SpaceOops *soops, TreeElement *te)
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
	if(sce && G.scene != sce) {
		if(G.obedit) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
		set_scene(sce);
	}
	
	/* find associated base in current scene */
	for(base= FIRSTBASE; base; base= base->next) 
		if(base->object==ob) break;
	if(base) {
		if(G.qual & LR_SHIFTKEY) {
			/* swap select */
			if(base->flag & SELECT) base->flag &= ~SELECT;
			else if ((base->object->restrictflag & OB_RESTRICT_VIEW)==0) base->flag |= SELECT;
			base->object->flag= base->flag;
		}
		else {
			Base *b;
			/* deleselect all */
			for(b= FIRSTBASE; b; b= b->next) {
				b->flag &= ~SELECT;
				b->object->flag= b->flag;
			}
			if ((base->object->restrictflag & OB_RESTRICT_VIEW)==0) {
				base->flag |= SELECT;
				base->object->flag |= SELECT;
			}
		}
		set_active_base(base);	/* editview.c */
		
		allqueue(REDRAWVIEW3D, 1);
		allqueue(REDRAWOOPS, 0);
		allqueue(REDRAWINFO, 1);
	}
	
	if(ob!=G.obedit) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
}

static int tree_element_active_material(SpaceOops *soops, TreeElement *te, int set)
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
			ob->colbits |= (1<<te->index);	// make ob material active too
		}
		else {
			if(ob->actcol == te->index+1) 
				if(ob->colbits & (1<<te->index)) return 1;
		}
	}
	/* or we search for obdata material */
	else {
		if(set) {
			ob->actcol= te->index+1;
			ob->colbits &= ~(1<<te->index);	// make obdata material active too
		}
		else {
			if(ob->actcol == te->index+1)
				if( (ob->colbits & (1<<te->index))==0 ) return 1;
		}
	}
	if(set) {
		extern_set_butspace(F5KEY, 0);	// force shading buttons
		BIF_preview_changed(ID_MA);
		allqueue(REDRAWBUTSSHADING, 1);
		allqueue(REDRAWNODE, 0);
		allqueue(REDRAWOOPS, 0);
		allqueue(REDRAWIPO, 0);
	}
	return 0;
}

static int tree_element_active_texture(SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tep;
	TreeStoreElem *tselem, *tselemp;
	Object *ob=OBACT;
	ScrArea *sa;
	SpaceButs *sbuts=NULL;
	
	if(ob==NULL) return 0; // no active object
	
	tselem= TREESTORE(te);
	
	/* find buttons area (note, this is undefined really still, needs recode in blender) */
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==SPACE_BUTS) break;
		sa= sa->next;
	}
	if(sa) sbuts= sa->spacedata.first;
	
	/* where is texture linked to? */
	tep= te->parent;
	tselemp= TREESTORE(tep);
	
	if(tep->idcode==ID_WO) {
		World *wrld= (World *)tselemp->id;

		if(set) {
			if(sbuts) {
				sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				sbuts->texfrom= 1;
			}
			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			wrld->texact= te->index;
		}
		else if(tselemp->id == (ID *)(G.scene->world)) {
			if(wrld->texact==te->index) return 1;
		}
	}
	else if(tep->idcode==ID_LA) {
		Lamp *la= (Lamp *)tselemp->id;
		if(set) {
			if(sbuts) {
				sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				sbuts->texfrom= 2;
			}
			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
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
				sbuts->texfrom= 0;
			}
			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			ma->texact= te->index;
			
			/* also set active material */
			ob->actcol= tep->index+1;
		}
		else if(tep->flag & TE_ACTIVE) {	// this is active material
			if(ma->texact==te->index) return 1;
		}
	}
	
	return 0;
}


static int tree_element_active_lamp(SpaceOops *soops, TreeElement *te, int set)
{
	Object *ob;
	
	/* we search for the object parent */
	ob= (Object *)outliner_search_back(soops, te, ID_OB);
	if(ob==NULL || ob!=OBACT) return 0;	// just paranoia
	
	if(set) {
		extern_set_butspace(F5KEY, 0);
		BIF_preview_changed(ID_LA);
		allqueue(REDRAWBUTSSHADING, 1);
		allqueue(REDRAWOOPS, 0);
		allqueue(REDRAWIPO, 0);
	}
	else return 1;
	
	return 0;
}

static int tree_element_active_world(SpaceOops *soops, TreeElement *te, int set)
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
		if(sce && G.scene != sce) {
			if(G.obedit) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
			set_scene(sce);
		}
	}
	
	if(tep==NULL || tselem->id == (ID *)G.scene) {
		if(set) {
			extern_set_butspace(F8KEY, 0);
		}
		else {
			return 1;
		}
	}
	return 0;
}

static int tree_element_active_ipo(SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tes;
	TreeStoreElem *tselems=NULL;
	Object *ob;
	
	/* we search for the object parent */
	ob= (Object *)outliner_search_back(soops, te, ID_OB);
	if(ob==NULL || ob!=OBACT) return 0;	// just paranoia
	
	/* the parent of ipo */
	tes= te->parent;
	tselems= TREESTORE(tes);
	
	if(set) {
		if(tes->idcode==ID_AC) {
			if(ob->ipoflag & OB_ACTION_OB)
				ob->ipowin= ID_OB;
			else if(ob->ipoflag & OB_ACTION_KEY)
				ob->ipowin= ID_KE;
			else 
				ob->ipowin= ID_PO;
		}
		else ob->ipowin= tes->idcode;
		
		if(ob->ipowin==ID_MA) tree_element_active_material(soops, tes, 1);
		else if(ob->ipowin==ID_AC) {
			bActionChannel *chan;
			short a=0;
			for(chan=ob->action->chanbase.first; chan; chan= chan->next) {
				if(a==te->index) break;
				if(chan->ipo) a++;
			}
			deselect_actionchannels(ob->action, 0);
			select_channel(ob->action, chan, SELECT_ADD);
			allqueue(REDRAWACTION, ob->ipowin);
			allqueue(REDRAWVIEW3D, ob->ipowin);
		}
		
		allqueue(REDRAWIPO, ob->ipowin);
	}
	else {
		if(tes->idcode==ID_AC) {
			if(ob->ipoflag & OB_ACTION_OB)
				return ob->ipowin==ID_OB;
			else if(ob->ipoflag & OB_ACTION_KEY)
				return ob->ipowin==ID_KE;
			else if(ob->ipowin==ID_AC) {
				bActionChannel *chan;
				short a=0;
				for(chan=ob->action->chanbase.first; chan; chan= chan->next) {
					if(a==te->index) break;
					if(chan->ipo) a++;
				}
				if(chan==get_hilighted_action_channel(ob->action)) return 1;
			}
		}
		else if(ob->ipowin==tes->idcode) {
			if(ob->ipowin==ID_MA) {
				Material *ma= give_current_material(ob, ob->actcol);
				if(ma==(Material *)tselems->id) return 1;
			}
			else return 1;
		}
	}
	return 0;
}	

static int tree_element_active_defgroup(TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob;
	
	/* id in tselem is object */
	ob= (Object *)tselem->id;
	if(set) {
		ob->actdef= te->index+1;
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, ob->ipowin);
	}
	else {
		if(ob==OBACT)
			if(ob->actdef== te->index+1) return 1;
	}
	return 0;
}

static int tree_element_active_nla_action(TreeElement *te, TreeStoreElem *tselem, int set)
{
	if(set) {
		bActionStrip *strip= te->directdata;
		if(strip) {
			deselect_nlachannel_keys(0);
			strip->flag |= ACTSTRIP_SELECT;
			allqueue(REDRAWNLA, 0);
		}
	}
	else {
		/* id in tselem is action */
		bActionStrip *strip= te->directdata;
		if(strip) {
			if(strip->flag & ACTSTRIP_SELECT) return 1;
		}
	}
	return 0;
}

static int tree_element_active_posegroup(TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	
	if(set) {
		if (ob->pose) {
			ob->pose->active_group= te->index+1;
			allqueue(REDRAWBUTSEDIT, 0);
		}
	}
	else {
		if(ob==OBACT && ob->pose) {
			if (ob->pose->active_group== te->index+1) return 1;
		}
	}
	return 0;
}

static int tree_element_active_posechannel(TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	bPoseChannel *pchan= te->directdata;
	
	if(set) {
		if(!(pchan->bone->flag & BONE_HIDDEN_P)) {
			
			if(G.qual & LR_SHIFTKEY) deselectall_posearmature(ob, 2, 0);	// 2 = clear active tag
			else deselectall_posearmature(ob, 0, 0);	// 0 = deselect 
			pchan->bone->flag |= BONE_SELECTED|BONE_ACTIVE;
			
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWACTION, 0);
		}
	}
	else {
		if(ob==OBACT && ob->pose) {
			if (pchan->bone->flag & BONE_SELECTED) return 1;
		}
	}
	return 0;
}

static int tree_element_active_bone(TreeElement *te, TreeStoreElem *tselem, int set)
{
	bArmature *arm= (bArmature *)tselem->id;
	Bone *bone= te->directdata;
	
	if(set) {
		if(!(bone->flag & BONE_HIDDEN_P)) {
			if(G.qual & LR_SHIFTKEY) deselectall_posearmature(OBACT, 2, 0);	// 2 is clear active tag
			else deselectall_posearmature(OBACT, 0, 0);
			bone->flag |= BONE_SELECTED|BONE_ACTIVE;
			
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWACTION, 0);
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
static int tree_element_active_ebone(TreeElement *te, TreeStoreElem *tselem, int set)
{
	EditBone *ebone= te->directdata;
	
	if(set) {
		if(!(ebone->flag & BONE_HIDDEN_A)) {
			
			if(G.qual & LR_SHIFTKEY) deselectall_armature(2, 0);	// only clear active tag
			else deselectall_armature(0, 0);	// deselect

			ebone->flag |= BONE_SELECTED|BONE_ROOTSEL|BONE_TIPSEL|BONE_ACTIVE;
			// flush to parent?
			if(ebone->parent && (ebone->flag & BONE_CONNECTED)) ebone->parent->flag |= BONE_TIPSEL;
			
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWACTION, 0);
		}
	}
	else {
		if (ebone->flag & BONE_SELECTED) return 1;
	}
	return 0;
}

static int tree_element_active_modifier(TreeElement *te, TreeStoreElem *tselem, int set)
{
	if(set) {
		extern_set_butspace(F9KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_constraint(TreeElement *te, TreeStoreElem *tselem, int set)
{
	if(set) {
		extern_set_butspace(F7KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_text(SpaceOops *soops, TreeElement *te, int set)
{
	ScrArea *sa=NULL;
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_TEXT) break;
	}
	if(sa) {
		SpaceText *st= sa->spacedata.first;
		TreeStoreElem *tselem= TREESTORE(te);
		
		if(set) {
			st->text= (Text *)tselem->id;
			st->top= 0;
			scrarea_queue_redraw(sa);
		}
		else if(st->text==(Text *)tselem->id) return 1;
	}
	return 0;
}

/* generic call for ID data check or make/check active in UI */
static int tree_element_active(SpaceOops *soops, TreeElement *te, int set)
{

	switch(te->idcode) {
		case ID_MA:
			return tree_element_active_material(soops, te, set);
		case ID_WO:
			return tree_element_active_world(soops, te, set);
		case ID_LA:
			return tree_element_active_lamp(soops, te, set);
		case ID_IP:
			return tree_element_active_ipo(soops, te, set);
		case ID_TE:
			return tree_element_active_texture(soops, te, set);
		case ID_TXT:
			return tree_element_active_text(soops, te, set);
	}
	return 0;
}

static int tree_element_active_pose(TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	
	if(set) {
		if(G.obedit) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
		if(ob->flag & OB_POSEMODE) exit_posemode();
		else enter_posemode();
	}
	else {
		if(ob->flag & OB_POSEMODE) return 1;
	}
	return 0;
}

/* generic call for non-id data to make/check active in UI */
static int tree_element_type_active(SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, int set)
{
	
	switch(tselem->type) {
		case TSE_NLA_ACTION:
			return tree_element_active_nla_action(te, tselem, set);
		case TSE_DEFGROUP:
			return tree_element_active_defgroup(te, tselem, set);
		case TSE_BONE:
			return tree_element_active_bone(te, tselem, set);
		case TSE_EBONE:
			return tree_element_active_ebone(te, tselem, set);
		case TSE_MODIFIER:
			return tree_element_active_modifier(te, tselem, set);
		case TSE_LINKED_OB:
			if(set) tree_element_active_object(soops, te);
			else if(tselem->id==(ID *)OBACT) return 1;
			break;
		case TSE_POSE_BASE:
			return tree_element_active_pose(te, tselem, set);
			break;
		case TSE_POSE_CHANNEL:
			return tree_element_active_posechannel(te, tselem, set);
		case TSE_CONSTRAINT:
			return tree_element_active_constraint(te, tselem, set);
		case TSE_R_LAYER:
			return tree_element_active_renderlayer(te, tselem, set);
		case TSE_POSEGRP:
			return tree_element_active_posegroup(te, tselem, set);
	}
	return 0;
}

#ifdef WITH_VERSE
static void verse_operation_menu(TreeElement *te)
{
	short event=0;
	if(te->idcode==ID_VS) {
		struct VerseSession *session = (VerseSession*)te->directdata;
		struct VNode *vnode;
		if(!(session->flag & VERSE_AUTOSUBSCRIBE)) {
			event = pupmenu("VerseSession %t| End Session %x1| Subscribe to All Nodes %x2| Start Autosubscribe %x3");
		}
		else {
			event = pupmenu("VerseSession %t| End Session %x1| Subscribe to All Nodes %x2| Stop Autosubscribe %x4");
		}
		switch(event) {
			case 1:
				end_verse_session(session);
				break;
			case 2:
				vnode = session->nodes.lb.first;
				while(vnode) {
					b_verse_pop_node(vnode);
					vnode = vnode->next;
				}
				break;
			case 3:
				vnode = session->nodes.lb.first;
				while(vnode) {
					b_verse_pop_node(vnode);
					vnode = vnode->next;
				}
				session->flag |= VERSE_AUTOSUBSCRIBE;
				break;
			case 4:
				session->flag &= ~VERSE_AUTOSUBSCRIBE;
				break;
		}
	}
	else if(te->idcode==ID_VN) {
		struct VNode *vnode = (VNode*)te->directdata;
		event = pupmenu("VerseNode %t| Subscribe %x1| Unsubscribe %x2");
		switch(event) {
			case 1:
				b_verse_pop_node(vnode);
				break;
			case 2:
				/* Global */
				b_verse_unsubscribe(vnode);
				break;
		}
	}
	else if(te->idcode==ID_MS) {
			event = pupmenu("Verse Master Server %t| Refresh %x1");
			b_verse_ms_get();
	}
	else if(te->idcode==ID_SS) {
		struct VerseServer *vserver = (VerseServer*)te->directdata;

		if(!(vserver->flag & VERSE_CONNECTING) && !(vserver->flag & VERSE_CONNECTED)) {
			event = pupmenu("VerseServer %t| Connect %x1");
		} else if((vserver->flag & VERSE_CONNECTING) && !(vserver->flag & VERSE_CONNECTED)) {
			event = pupmenu("VerseServer %t| Connecting %x2");
		} else if(!(vserver->flag & VERSE_CONNECTING) && (vserver->flag & VERSE_CONNECTED)) {
			event = pupmenu("VerseServer %t| Disconnect %x3");
		}
		switch(event) {
			case 1:
				b_verse_connect(vserver->ip);
				vserver->flag |= VERSE_CONNECTING;
				break;
			case 2:
				break;
			case 3:
				end_verse_session(vserver->session);
				break;
		}
	}
}
#endif


static int do_outliner_mouse_event(SpaceOops *soops, TreeElement *te, short event, float *mval)
{
	
	if(mval[1]>te->ys && mval[1]<te->ys+OL_H) {
		TreeStoreElem *tselem= TREESTORE(te);
		int openclose= 0;

		/* open close icon, three things to check */
		if(event==RETKEY || event==PADENTER) openclose= 1; // enter opens/closes always
		else if((te->flag & TE_ICONROW)==0) {				// hidden icon, no open/close
			if( mval[0]>te->xs && mval[0]<te->xs+OL_X) openclose= 1;
		}

		if(openclose) {
			
			/* all below close/open? */
			if( (G.qual & LR_SHIFTKEY) ) {
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
			
			/* activate a name button? */
			if(event==LEFTMOUSE) {
			
				if (G.qual == LR_CTRLKEY) {
					if(ELEM9(tselem->type, TSE_NLA, TSE_DEFGROUP_BASE, TSE_CONSTRAINT_BASE, TSE_MODIFIER_BASE, TSE_SCRIPT_BASE, TSE_POSE_BASE, TSE_POSEGRP_BASE, TSE_R_LAYER_BASE, TSE_R_PASS)) 
						error("Cannot edit builtin name");
					else if(tselem->id->lib) {
						error_libdata();
					} else if(te->idcode == ID_LI && te->parent) {
						error("Cannot edit the path of an indirectly linked library");
					} else {
						tselem->flag |= TSE_TEXTBUT;
					}
				} else {
					/* always makes active object */
					tree_element_active_object(soops, te);
					
					if(tselem->type==0) { // the lib blocks
						/* editmode? */
						if(te->idcode==ID_SCE) {
							if(G.scene!=(Scene *)tselem->id) {
								if(G.obedit) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
								set_scene((Scene *)tselem->id);
							}
						}
						else if(ELEM5(te->idcode, ID_ME, ID_CU, ID_MB, ID_LT, ID_AR)) {
							if(G.obedit) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
							else {
								enter_editmode(EM_WAITCURSOR);
								extern_set_butspace(F9KEY, 0);
							}
						} else {	// rest of types
							tree_element_active(soops, te, 1);
						}
						
					}
					else tree_element_type_active(soops, te, tselem, 1);
				}
			}
			else if(event==RIGHTMOUSE) {
#ifdef WITH_VERSE
				if(ELEM4(te->idcode, ID_VS, ID_VN, ID_MS, ID_SS))
					verse_operation_menu(te);
				else
#endif
				/* select object that's clicked on and popup context menu */
				if (!(tselem->flag & TSE_SELECTED)) {
				
					if ( outliner_has_one_flag(soops, &soops->tree, TSE_SELECTED, 1) )
						outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
				
					tselem->flag |= TSE_SELECTED;
					/* redraw, same as outliner_select function */
					soops->storeflag |= SO_TREESTORE_REDRAW;
					scrarea_do_windraw(soops->area);
					screen_swapbuffers();
				}
				
				outliner_operation_menu(soops->area);
			}
			return 1;
		}
	}
	
	for(te= te->subtree.first; te; te= te->next) {
		if(do_outliner_mouse_event(soops, te, event, mval)) return 1;
	}
	return 0;
}

/* event can enterkey, then it opens/closes */
void outliner_mouse_event(ScrArea *sa, short event)
{
	SpaceOops *soops= sa->spacedata.first;
	TreeElement *te;
	float fmval[2];
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(&soops->v2d, mval, fmval, fmval+1);
	
	for(te= soops->tree.first; te; te= te->next) {
		if(do_outliner_mouse_event(soops, te, event, fmval)) break;
	}
	
	if(te) {
		BIF_undo_push("Outliner click event");
		allqueue(REDRAWOOPS, 0);
	}
	else 
		outliner_select(sa);

}
/* recursive helper for function below */
static void outliner_set_coordinates_element(SpaceOops *soops, TreeElement *te, int startx, int *starty)
{
	TreeStoreElem *tselem= TREESTORE(te);
	
	/* store coord and continue, we need coordinates for elements outside view too */
	te->xs= startx;
	te->ys= *starty;
	*starty-= OL_H;
	
	if((tselem->flag & TSE_CLOSED)==0) {
		TreeElement *ten;
		for(ten= te->subtree.first; ten; ten= ten->next) {
			outliner_set_coordinates_element(soops, ten, startx+OL_X, starty);
		}
	}
	
}

/* to retrieve coordinates with redrawing the entire tree */
static void outliner_set_coordinates(SpaceOops *soops)
{
	TreeElement *te;
	int starty= soops->v2d.tot.ymax-OL_H;
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

void outliner_show_active(struct ScrArea *sa)
{
	SpaceOops *so= sa->spacedata.first;
	TreeElement *te;
	int xdelta, ytop;
	
	if(OBACT == NULL) return;
	
	te= outliner_find_id(so, &so->tree, (ID *)OBACT);
	if(te) {
		/* make te->ys center of view */
		ytop= te->ys + (so->v2d.mask.ymax-so->v2d.mask.ymin)/2;
		if(ytop>0) ytop= 0;
		so->v2d.cur.ymax= ytop;
		so->v2d.cur.ymin= ytop-(so->v2d.mask.ymax-so->v2d.mask.ymin);
		
		/* make te->xs ==> te->xend center of view */
		xdelta = te->xs - so->v2d.cur.xmin;
		so->v2d.cur.xmin += xdelta;
		so->v2d.cur.xmax += xdelta;
		
		so->storeflag |= SO_TREESTORE_REDRAW;
		scrarea_queue_redraw(sa);
	}
}

void outliner_show_selected(struct ScrArea *sa)
{
	SpaceOops *so= sa->spacedata.first;
	TreeElement *te;
	int xdelta, ytop;
	
	te= outliner_find_id(so, &so->tree, (ID *)OBACT);
	if(te) {
		/* make te->ys center of view */
		ytop= te->ys + (so->v2d.mask.ymax-so->v2d.mask.ymin)/2;
		if(ytop>0) ytop= 0;
		so->v2d.cur.ymax= ytop;
		so->v2d.cur.ymin= ytop-(so->v2d.mask.ymax-so->v2d.mask.ymin);
		
		/* make te->xs ==> te->xend center of view */
		xdelta = te->xs - so->v2d.cur.xmin;
		so->v2d.cur.xmin += xdelta;
		so->v2d.cur.xmax += xdelta;
		
		so->storeflag |= SO_TREESTORE_REDRAW;
		scrarea_queue_redraw(sa);
	}
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
		if(tselem->id==tse->id) {
			if((tse->type==0 && tselem->type==0) || (tselem->type==tse->type && tselem->nr==tse->nr)) {
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
void outliner_find_panel(struct ScrArea *sa, int again, int flags) 
{
	SpaceOops *soops= sa->spacedata.first;
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
		if (sbutton(name, 0, sizeof(name)-1, "Find: ") && name[0]) {
			te= outliner_find_named(soops, &soops->tree, name, flags, NULL, &prevFound);
		}
		else return; /* XXX RETURN! XXX */
	}

	/* do selection and reveil */
	if (te) {
		tselem= TREESTORE(te);
		if (tselem) {
			/* expand branches so that it will be visible, we need to get correct coordinates */
			if( outliner_open_back(soops, te))
				outliner_set_coordinates(soops);
			
			/* deselect all visible, and select found element */
			outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
			tselem->flag |= TSE_SELECTED;
			
			/* make te->ys center of view */
			ytop= te->ys + (soops->v2d.mask.ymax-soops->v2d.mask.ymin)/2;
			if(ytop>0) ytop= 0;
			soops->v2d.cur.ymax= ytop;
			soops->v2d.cur.ymin= ytop-(soops->v2d.mask.ymax-soops->v2d.mask.ymin);
			
			/* make te->xs ==> te->xend center of view */
			xdelta = te->xs - soops->v2d.cur.xmin;
			soops->v2d.cur.xmin += xdelta;
			soops->v2d.cur.xmax += xdelta;
			
			/* store selection */
			soops->search_tse= *tselem;
			
			BLI_strncpy(soops->search_string, name, 33);
			soops->search_flags= flags;
			
			/* redraw */
			soops->storeflag |= SO_TREESTORE_REDRAW;
			scrarea_queue_redraw(sa);
		}
	}
	else {
		/* no tree-element found */
		error("Not found: %s", name);
	}
}

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

static void tree_element_show_hierarchy(SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;

	/* open all object elems, close others */
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		if(tselem->type==0) {
			if(te->idcode==ID_SCE) {
				if(tselem->id!=(ID *)G.scene) tselem->flag |= TSE_CLOSED;
					else tselem->flag &= ~TSE_CLOSED;
			}
			else if(te->idcode==ID_OB) {
				if(subtree_has_objects(soops, &te->subtree)) tselem->flag &= ~TSE_CLOSED;
				else tselem->flag |= TSE_CLOSED;
			}
		}
		else tselem->flag |= TSE_CLOSED;

		if(tselem->flag & TSE_CLOSED); else tree_element_show_hierarchy(soops, &te->subtree);
	}
	
}

/* show entire object level hierarchy */
void outliner_show_hierarchy(struct ScrArea *sa)
{
	SpaceOops *so= sa->spacedata.first;
	
	tree_element_show_hierarchy(so, &so->tree);
	scrarea_queue_redraw(sa);
	
	BIF_undo_push("Outliner show hierarchy");
}

static void do_outliner_select(SpaceOops *soops, ListBase *lb, float y1, float y2, short *selecting)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	if(y1>y2) SWAP(float, y1, y2);
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		if(te->ys + OL_H < y1) return;
		if(te->ys < y2) {
			if((te->flag & TE_ICONROW)==0) {
				if(*selecting == -1) {
					if( tselem->flag & TSE_SELECTED) *selecting= 0;
					else *selecting= 1;
				}
				if(*selecting) tselem->flag |= TSE_SELECTED;
				else tselem->flag &= ~TSE_SELECTED;
			}
		}
		if((tselem->flag & TSE_CLOSED)==0) do_outliner_select(soops, &te->subtree, y1, y2, selecting);
	}
}

/* its own redraw loop... urm */
void outliner_select(struct ScrArea *sa )
{
	SpaceOops *so= sa->spacedata.first;
	float fmval[2], y1, y2;
	short mval[2], yo=-1, selecting= -1;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(&so->v2d, mval, fmval, fmval+1);
	y1= fmval[1];

	while (get_mbut() & (L_MOUSE|R_MOUSE)) {
		getmouseco_areawin(mval);
		areamouseco_to_ipoco(&so->v2d, mval, fmval, fmval+1);
		y2= fmval[1];
		
		if(yo!=mval[1]) {
			/* select the 'ouliner row' */
			do_outliner_select(so, &so->tree, y1, y2, &selecting);
			yo= mval[1];
			
			so->storeflag |= SO_TREESTORE_REDRAW;
			scrarea_do_windraw(sa);
			screen_swapbuffers();
		
			y1= y2;
		}
		else PIL_sleep_ms(30);
	}
	
	BIF_undo_push("Outliner selection");

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
#ifdef WITH_VERSE
				if(te->idcode==ID_VS) *datalevel= TSE_VERSE_SESSION;
				else if(te->idcode==ID_VN) *datalevel= TSE_VERSE_OBJ_NODE;
				else if(*datalevel==0) *datalevel= tselem->type;
#else
				if(*datalevel==0) *datalevel= tselem->type;
#endif
				else if(*datalevel!=tselem->type) *datalevel= -1;
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

static void unlink_material_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
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

static void unlink_texture_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
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

static void unlink_group_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
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

static void outliner_do_libdata_operation(SpaceOops *soops, ListBase *lb, 
										 void (*operation_cb)(TreeElement *, TreeStoreElem *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te=lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->flag & TSE_SELECTED) {
			if(tselem->type==0) {
				TreeStoreElem *tsep= TREESTORE(te->parent);
				operation_cb(te, tsep, tselem);
			}
		}
		if((tselem->flag & TSE_CLOSED)==0) {
			outliner_do_libdata_operation(soops, &te->subtree, operation_cb);
		}
	}
}

/* */

static void object_select_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, G.scene);
	if(base && ((base->object->restrictflag & OB_RESTRICT_VIEW)==0)) {
		base->flag |= SELECT;
		base->object->flag |= SELECT;
	}
}

static void object_deselect_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, G.scene);
	if(base) {
		base->flag &= ~SELECT;
		base->object->flag &= ~SELECT;
	}
}

static void object_delete_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Base *base= (Base *)te->directdata;
	
	if(base==NULL) base= object_in_scene((Object *)tselem->id, G.scene);
	if(base) {
		// check also library later
		if(G.obedit==base->object) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR);
		
		if(base==BASACT) {
			G.f &= ~(G_VERTEXPAINT+G_TEXTUREPAINT+G_WEIGHTPAINT+G_SCULPTMODE);
			setcursor_space(SPACE_VIEW3D, CURSOR_STD);
		}
		
		free_and_unlink_base(base);
		te->directdata= NULL;
		tselem->id= NULL;
	}
}

static void id_local_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	if(tselem->id->lib && (tselem->id->flag & LIB_EXTERN)) {
		tselem->id->lib= NULL;
		tselem->id->flag= LIB_LOCAL;
		new_id(0, tselem->id, 0);
	}
}

static void group_linkobs2scene_cb(TreeElement *te, TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Group *group= (Group *)tselem->id;
	GroupObject *gob;
	Base *base;
	
	for(gob=group->gobject.first; gob; gob=gob->next) {
		base= object_in_scene(gob->ob, G.scene);
		if (base) {
			base->object->flag |= SELECT;
			base->flag |= SELECT;
		} else {
			/* link to scene */
			base= MEM_callocN( sizeof(Base), "add_base");
			BLI_addhead(&G.scene->base, base);
			base->lay= (1<<20)-1; /*G.vd->lay;*/ /* would be nice to use the 3d layer but the include's not here */
			gob->ob->flag |= SELECT;
			base->flag = gob->ob->flag;
			base->object= gob->ob;
			id_lib_extern((ID *)gob->ob); /* incase these are from a linked group */
		}
	}
}

static void outliner_do_object_operation(SpaceOops *soops, ListBase *lb, 
										 void (*operation_cb)(TreeElement *, TreeStoreElem *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te=lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(tselem->flag & TSE_SELECTED) {
			if(tselem->type==0 && te->idcode==ID_OB) {
				// when objects selected in other scenes... dunno if that should be allowed
				Scene *sce= (Scene *)outliner_search_back(soops, te, ID_SCE);
				if(sce && G.scene != sce) set_scene(sce);
				
				operation_cb(te, NULL, tselem);
			}
		}
		if((tselem->flag & TSE_CLOSED)==0) {
			outliner_do_object_operation(soops, &te->subtree, operation_cb);
		}
	}
}

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

#ifdef WITH_VERSE
static void vsession_cb(int event, TreeElement *te, TreeStoreElem *tselem)
{
/*	struct VerseSession *vsession =(VerseSession*)te->directdata;*/

	if(event==1) {
		printf("\tending verse session\n");
	}
}
#endif

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

void outliner_del(ScrArea *sa)
{
	SpaceOops *soops= sa->spacedata.first;
	outliner_do_object_operation(soops, &soops->tree, object_delete_cb);
	DAG_scene_sort(G.scene);
	countall();	
	BIF_undo_push("Delete Objects");
	allqueue(REDRAWALL, 0);	
}


void outliner_operation_menu(ScrArea *sa)
{
	SpaceOops *soops= sa->spacedata.first;
	int scenelevel=0, objectlevel=0, idlevel=0, datalevel=0;
	
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	if(scenelevel) {
		if(objectlevel || datalevel || idlevel) error("Mixed selection");
		//else pupmenu("Scene Operations%t|Delete");
	}
	else if(objectlevel) {
		short event= pupmenu("Select%x1|Deselect%x2|Delete%x4|Toggle Visible%x6|Toggle Selectable%x7|Toggle Renderable%x8");	/* make local: does not work... it doesn't set lib_extern flags... so data gets lost */
		if(event>0) {
			char *str="";
			
			if(event==1) {
				Scene *sce= G.scene;	// to be able to delete, scenes are set...
				outliner_do_object_operation(soops, &soops->tree, object_select_cb);
				if(G.scene != sce) set_scene(sce);
				
				str= "Select Objects";
			}
			else if(event==2) {
				outliner_do_object_operation(soops, &soops->tree, object_deselect_cb);
				str= "Deselect Objects";
			}
			else if(event==4) {
				outliner_do_object_operation(soops, &soops->tree, object_delete_cb);
				DAG_scene_sort(G.scene);
				str= "Delete Objects";
			}
			else if(event==5) {	/* disabled, see above (ton) */
				outliner_do_object_operation(soops, &soops->tree, id_local_cb);
				str= "Localized Objects";
			}
			else if(event==6) {
				outliner_do_object_operation(soops, &soops->tree, object_toggle_visibility_cb);
				str= "Toggle Visibility";
			}
			else if(event==7) {
				outliner_do_object_operation(soops, &soops->tree, object_toggle_selectability_cb);
				str= "Toggle Selectability";
			}
			else if(event==8) {
				outliner_do_object_operation(soops, &soops->tree, object_toggle_renderability_cb);
				str= "Toggle Renderability";
			}
			countall();
			
			BIF_undo_push(str);
			allqueue(REDRAWALL, 0);
		}
	}
	else if(idlevel) {
		if(idlevel==-1 || datalevel) error("Mixed selection");
		else {
			short event;
			if (idlevel==ID_GR)
				event = pupmenu("Unlink %x1|Make Local %x2|Link Group Objects to Scene%x3");
			else
				event = pupmenu("Unlink %x1|Make Local %x2");
			
			
			if(event==1) {
				switch(idlevel) {
					case ID_MA:
						outliner_do_libdata_operation(soops, &soops->tree, unlink_material_cb);
						BIF_undo_push("Unlink material");
						allqueue(REDRAWBUTSSHADING, 1);
						break;
					case ID_TE:
						outliner_do_libdata_operation(soops, &soops->tree, unlink_texture_cb);
						allqueue(REDRAWBUTSSHADING, 1);
						BIF_undo_push("Unlink texture");
						break;
					case ID_GR:
						outliner_do_libdata_operation(soops, &soops->tree, unlink_group_cb);
						BIF_undo_push("Unlink group");
						break;
					default:
						error("Not yet...");
				}
				allqueue(REDRAWALL, 0);
			}
			else if(event==2) {
				outliner_do_libdata_operation(soops, &soops->tree, id_local_cb);
				BIF_undo_push("Localized Data");
				allqueue(REDRAWALL, 0); 
			}
			else if(event==3 && idlevel==ID_GR) {
				outliner_do_libdata_operation(soops, &soops->tree, group_linkobs2scene_cb);
				BIF_undo_push("Link Group Objects to Scene");
			}
		}
	}
	else if(datalevel) {
		if(datalevel==-1) error("Mixed selection");
		else {
			if(datalevel==TSE_POSE_CHANNEL) {
				short event= pupmenu("PoseChannel Operations%t|Select%x1|Deselect%x2|Hide%x3|Unhide%x4");
				if(event>0) {
					outliner_do_data_operation(soops, datalevel, event, &soops->tree, pchan_cb);
					BIF_undo_push("PoseChannel operation");
				}
			}
			else if(datalevel==TSE_BONE) {
				short event= pupmenu("Bone Operations%t|Select%x1|Deselect%x2|Hide%x3|Unhide%x4");
				if(event>0) {
					outliner_do_data_operation(soops, datalevel, event, &soops->tree, bone_cb);
					BIF_undo_push("Bone operation");
				}
			}
			else if(datalevel==TSE_EBONE) {
				short event= pupmenu("EditBone Operations%t|Select%x1|Deselect%x2|Hide%x3|Unhide%x4");
				if(event>0) {
					outliner_do_data_operation(soops, datalevel, event, &soops->tree, ebone_cb);
					BIF_undo_push("EditBone operation");
				}
			}
#ifdef WITH_VERSE
			else if(datalevel==TSE_VERSE_SESSION) {
				short event= pupmenu("VerseSession %t| End %x1");
				if(event>0) {
					outliner_do_data_operation(soops, datalevel, event, &soops->tree, vsession_cb);
				}
			}
#endif
			
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWBUTSALL, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}


/* ***************** DRAW *************** */

static void tselem_draw_icon(float x, float y, TreeStoreElem *tselem, TreeElement *te)
{
	if(tselem->type) {
		switch( tselem->type) {
			case TSE_NLA:
				BIF_icon_draw(x, y, ICON_NLA); break;
			case TSE_NLA_ACTION:
				BIF_icon_draw(x, y, ICON_ACTION); break;
			case TSE_DEFGROUP_BASE:
				BIF_icon_draw(x, y, ICON_VERTEXSEL); break;
			case TSE_BONE:
			case TSE_EBONE:
				BIF_icon_draw(x, y, ICON_WPAINT_DEHLT); break;
			case TSE_CONSTRAINT_BASE:
				BIF_icon_draw(x, y, ICON_CONSTRAINT); break;
			case TSE_MODIFIER_BASE:
				BIF_icon_draw(x, y, ICON_MODIFIER); break;
			case TSE_LINKED_OB:
				BIF_icon_draw(x, y, ICON_OBJECT); break;
			case TSE_MODIFIER:
			{
				Object *ob= (Object *)tselem->id;
				ModifierData *md= BLI_findlink(&ob->modifiers, tselem->nr);
				switch(md->type) {
					case eModifierType_Subsurf: 
						BIF_icon_draw(x, y, ICON_MOD_SUBSURF); break;
					case eModifierType_Armature: 
						BIF_icon_draw(x, y, ICON_ARMATURE); break;
					case eModifierType_Lattice: 
						BIF_icon_draw(x, y, ICON_LATTICE); break;
					case eModifierType_Curve: 
						BIF_icon_draw(x, y, ICON_CURVE); break;
					case eModifierType_Build: 
						BIF_icon_draw(x, y, ICON_MOD_BUILD); break;
					case eModifierType_Mirror: 
						BIF_icon_draw(x, y, ICON_MOD_MIRROR); break;
					case eModifierType_Decimate: 
						BIF_icon_draw(x, y, ICON_MOD_DECIM); break;
					case eModifierType_Wave: 
						BIF_icon_draw(x, y, ICON_MOD_WAVE); break;
					case eModifierType_Hook: 
						BIF_icon_draw(x, y, ICON_HOOK); break;
					case eModifierType_Softbody: 
						BIF_icon_draw(x, y, ICON_MOD_SOFT); break;
					case eModifierType_Boolean: 
						BIF_icon_draw(x, y, ICON_MOD_BOOLEAN); break;
					default:
						BIF_icon_draw(x, y, ICON_DOT); break;
				}
				break;
			}
			case TSE_SCRIPT_BASE:
				BIF_icon_draw(x, y, ICON_TEXT); break;
			case TSE_POSE_BASE:
				BIF_icon_draw(x, y, ICON_ARMATURE_DEHLT); break;
			case TSE_POSE_CHANNEL:
				BIF_icon_draw(x, y, ICON_WPAINT_DEHLT); break;
			case TSE_PROXY:
				BIF_icon_draw(x, y, ICON_GHOST); break;
			case TSE_R_LAYER_BASE:
				BIF_icon_draw(x, y, ICON_RESTRICT_RENDER_OFF); break;
			case TSE_R_LAYER:
				BIF_icon_draw(x, y, ICON_IMAGE_DEHLT); break;
			case TSE_LINKED_LAMP:
				BIF_icon_draw(x, y, ICON_LAMP_DEHLT); break;
			case TSE_LINKED_MAT:
				BIF_icon_draw(x, y, ICON_MATERIAL_DEHLT); break;
			case TSE_POSEGRP_BASE:
				BIF_icon_draw(x, y, ICON_VERTEXSEL); break;
				
#ifdef WITH_VERSE
			case ID_VS:
			case ID_MS:
			case ID_SS:
				BIF_icon_draw(x, y, ICON_VERSE); break;
			case ID_VN:
				BIF_icon_draw(x, y, ICON_VERSE); break;
#endif
			default:
				BIF_icon_draw(x, y, ICON_DOT); break;
		}
	}
	else {
		switch( GS(tselem->id->name)) {
			case ID_SCE:
				BIF_icon_draw(x, y, ICON_SCENE_DEHLT); break;
			case ID_OB:
				BIF_icon_draw(x, y, ICON_OBJECT); break;
			case ID_ME:
				BIF_icon_draw(x, y, ICON_MESH); break;
			case ID_CU:
				BIF_icon_draw(x, y, ICON_CURVE); break;
			case ID_MB:
				BIF_icon_draw(x, y, ICON_MBALL); break;
			case ID_LT:
				BIF_icon_draw(x, y, ICON_LATTICE); break;
			case ID_LA:
				BIF_icon_draw(x, y, ICON_LAMP_DEHLT); break;
			case ID_MA:
				BIF_icon_draw(x, y, ICON_MATERIAL_DEHLT); break;
			case ID_TE:
				BIF_icon_draw(x, y, ICON_TEXTURE_DEHLT); break;
			case ID_IP:
				BIF_icon_draw(x, y, ICON_IPO_DEHLT); break;
			case ID_IM:
				BIF_icon_draw(x, y, ICON_IMAGE_DEHLT); break;
			case ID_SO:
				BIF_icon_draw(x, y, ICON_SPEAKER); break;
			case ID_AR:
				BIF_icon_draw(x, y, ICON_ARMATURE); break;
			case ID_CA:
				BIF_icon_draw(x, y, ICON_CAMERA_DEHLT); break;
			case ID_KE:
				BIF_icon_draw(x, y, ICON_EDIT_DEHLT); break;
			case ID_WO:
				BIF_icon_draw(x, y, ICON_WORLD_DEHLT); break;
			case ID_AC:
				BIF_icon_draw(x, y, ICON_ACTION); break;
			case ID_NLA:
				BIF_icon_draw(x, y, ICON_NLA); break;
			case ID_TXT:
				BIF_icon_draw(x, y, ICON_SCRIPT); break;
			case ID_GR:
				BIF_icon_draw(x, y, ICON_CIRCLE_DEHLT); break;
			case ID_LI:
				BIF_icon_draw(x, y, ICON_LIBRARY_DEHLT); break;
		}
	}
}

static void outliner_draw_iconrow(SpaceOops *soops, ListBase *lb, int level, int *offsx, int ys)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int active;

	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		/* object hierarchy always, further constrained on level */
		if(level<1 || (tselem->type==0 && te->idcode==ID_OB)) {

			/* active blocks get white circle */
			active= 0;
			if(tselem->type==0) {
				if(te->idcode==ID_OB) active= (OBACT==(Object *)tselem->id);
				else if(G.obedit && G.obedit->data==tselem->id) active= 1;
				else active= tree_element_active(soops, te, 0);
			}
			else active= tree_element_type_active(soops, te, tselem, 0);
			
			if(active) {
				uiSetRoundBox(15);
				glColor4ub(255, 255, 255, 100);
				uiRoundBox( (float)*offsx-0.5, (float)ys-1.0, (float)*offsx+OL_H-3.0, (float)ys+OL_H-3.0, OL_H/2.0-2.0);
				glEnable(GL_BLEND);
			}
			
			tselem_draw_icon(*offsx, ys, tselem, te);
			te->xs= *offsx;
			te->ys= ys;
			te->xend= *offsx+OL_X;
			te->flag |= TE_ICONROW;	// for click
			
			(*offsx) += OL_X;
		}
		
		/* this tree element always has same amount of branches, so dont draw */
		if(tselem->type!=TSE_R_LAYER)
			outliner_draw_iconrow(soops, &te->subtree, level+1, offsx, ys);
	}
	
}

static void outliner_draw_tree_element(SpaceOops *soops, TreeElement *te, int startx, int *starty)
{
	TreeElement *ten;
	TreeStoreElem *tselem;
	int offsx= 0, active=0; // active=1 active obj, else active data
	
	tselem= TREESTORE(te);

	if(*starty >= soops->v2d.cur.ymin && *starty<= soops->v2d.cur.ymax) {
	
		glEnable(GL_BLEND);

		/* colors for active/selected data */
		if(tselem->type==0) {
			if(te->idcode==ID_SCE) {
				if(tselem->id == (ID *)G.scene) {
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
						BIF_GetThemeColorType4ubv(TH_ACTIVE, SPACE_VIEW3D, col);
						/* so black text is drawn when active and not selected */
						if (ob->flag & SELECT) active= 1;
					}
					else BIF_GetThemeColorType4ubv(TH_SELECT, SPACE_VIEW3D, col);
					col[3]= 100;
					glColor4ubv((GLubyte *)col);
				}

#ifdef WITH_VERSE
				if(ob->vnode) {
					if (active==0) active=2;
					if (ob==OBACT)
						glColor4ub(0,255,0,100);
					else
						glColor4ub(0,128,0,100);
				}
#endif
			}
			else if(G.obedit && G.obedit->data==tselem->id) {
				glColor4ub(255, 255, 255, 100);
				active= 2;
			}
			else {
				if(tree_element_active(soops, te, 0)) {
					glColor4ub(220, 220, 255, 100);
					active= 2;
				}
			}
		} else if (tselem->type==ID_SS) {
#ifdef WITH_VERSE
			/* draw colored circle behind verse server icon */
			struct VerseServer *server = (VerseServer *)te->directdata;
			if(server->flag & VERSE_CONNECTING) {
				glColor4ub(255,128,64,100);
				active = 2;
			} else if(server->flag & VERSE_CONNECTED) {
				glColor4ub(0,128,0,100);
				active = 2;
			}
#endif
		}
		else if (tselem->type==ID_VN) {
#ifdef WITH_VERSE
			struct VNode *vnode = (VNode *)te->directdata;
			if(vnode->type==V_NT_OBJECT) {
				if(((VObjectData*)vnode->data)->object) {
					glColor4ub(0,128,0,100);
					active = 2;
				}
			}
			else if(vnode->type==V_NT_GEOMETRY) {
				if(((VGeomData*)vnode->data)->mesh || ((VGeomData*)vnode->data)->editmesh) {
					glColor4ub(0,128,0,100);
					active = 2;
				}
			}
			if(vnode->type==V_NT_BITMAP) {
				if(((VBitmapData*)vnode->data)->image) {
					glColor4ub(0,128,0,100);
					active = 2;
				}
			}
#endif
		}
		else {
			if( tree_element_type_active(soops, te, tselem, 0) ) active= 2;
			glColor4ub(220, 220, 255, 100);
		}
		
		/* active circle */
		if(active) {
			uiSetRoundBox(15);
			uiRoundBox( (float)startx+OL_H-1.5, (float)*starty+2.0, (float)startx+2*OL_H-4.0, (float)*starty+OL_H-1.0, OL_H/2.0-2.0);
			glEnable(GL_BLEND);	/* roundbox disables it */
			
			te->flag |= TE_ACTIVE; // for lookup in display hierarchies
		}
		
		/* open/close icon, only when sublevels, except for scene */
		if(te->subtree.first || (te->idcode==ID_SCE && tselem->type==0)) {
			int icon_x;
			if((tselem->type==0 && ELEM(te->idcode, ID_OB, ID_SCE)) || ELEM4(te->idcode,ID_VN,ID_VS, ID_MS, ID_SS))
				icon_x = startx;
			else
				icon_x = startx+5;

				// icons a bit higher
			if(tselem->flag & TSE_CLOSED) 
				BIF_icon_draw(icon_x, *starty+2, ICON_TRIA_RIGHT);
			else
				BIF_icon_draw(icon_x, *starty+2, ICON_TRIA_DOWN);
		}
		offsx+= OL_X;
		
		/* datatype icon */
		
			// icons a bit higher
		tselem_draw_icon(startx+offsx, *starty+2, tselem, te);
		offsx+= OL_X;
		
		if(tselem->id->lib && tselem->type==0) {
			glPixelTransferf(GL_ALPHA_SCALE, 0.5);
			if(tselem->id->flag & LIB_INDIRECT)
				BIF_icon_draw(startx+offsx, *starty+2, ICON_DATALIB);
			else
				BIF_icon_draw(startx+offsx, *starty+2, ICON_PARLIB);
			glPixelTransferf(GL_ALPHA_SCALE, 1.0);
			offsx+= OL_X;
		}		
		glDisable(GL_BLEND);

		/* name */
		if(active==1) BIF_ThemeColor(TH_TEXT_HI); 
		else BIF_ThemeColor(TH_TEXT);
		glRasterPos2i(startx+offsx, *starty+5);
		BIF_RasterPos(startx+offsx, *starty+5);
#ifdef WITH_VERSE
		if(te->name) {
#endif
			BIF_DrawString(G.font, te->name, 0);
			offsx+= OL_X + BIF_GetStringWidth(G.font, te->name, 0);
#ifdef WITH_VERSE
		}
#endif
		
		/* closed item, we draw the icons, not when it's a scene, or master-server list though */
		if(tselem->flag & TSE_CLOSED) {
			if(te->subtree.first) {
				if(tselem->type==0 && te->idcode==ID_SCE);
#ifdef WITH_VERSE
				else if(tselem->type==ID_MS) {
					char server_buf[50];
					int nr_servers = 0;
					struct VerseServer *server = server_list.first;
					while (server) {
						nr_servers++;
						server = server->next;
					}
					sprintf(server_buf, "(%d server%s", nr_servers, nr_servers==1?")":"s)");
					glRasterPos2i(startx+offsx-10, *starty+5);
					BIF_RasterPos(startx+offsx-10, *starty+5);
					BIF_DrawString(G.font, server_buf, 0);
					offsx+= OL_X + BIF_GetStringWidth(G.font, server_buf, 0);
				}
#endif
				else if(tselem->type!=TSE_R_LAYER) { /* this tree element always has same amount of branches, so dont draw */
					int tempx= startx+offsx;
					// divider
					BIF_ThemeColorShade(TH_BACK, -40);
					glRecti(tempx -10, *starty+4, tempx -8, *starty+OL_H-4);

					glEnable(GL_BLEND);
					glPixelTransferf(GL_ALPHA_SCALE, 0.5);

					outliner_draw_iconrow(soops, &te->subtree, 0, &tempx, *starty+2);
					
					glPixelTransferf(GL_ALPHA_SCALE, 1.0);
					glDisable(GL_BLEND);
				}
			}
		}
	}	
	/* store coord and continue, we need coordinates for elements outside view too */
	te->xs= startx;
	te->ys= *starty;
	te->xend= startx+offsx;
		
	*starty-= OL_H;

	if((tselem->flag & TSE_CLOSED)==0) {
		for(ten= te->subtree.first; ten; ten= ten->next) {
			outliner_draw_tree_element(soops, ten, startx+OL_X, starty);
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
		if((tselem->type==0 && (te->idcode==ID_OB || te->idcode==ID_SCE)) || ELEM4(te->idcode,ID_VS,ID_VN,ID_MS,ID_SS))
			glRecti(startx, *starty, startx+OL_X, *starty-1);
			
		*starty-= OL_H;
		
		if((tselem->flag & TSE_CLOSED)==0)
			outliner_draw_hierarchy(soops, &te->subtree, startx+OL_X, starty);
	}
	
	/* vertical line */
	te= lb->last;
	if(te->parent || lb->first!=lb->last) {
		tselem= TREESTORE(te);
		if((tselem->type==0 && te->idcode==ID_OB) || ELEM4(te->idcode,ID_VS,ID_VN,ID_MS,ID_SS)) {
			
			glRecti(startx, y1+OL_H, startx+1, y2);
		}
	}
}

static void outliner_draw_selection(SpaceOops *soops, ListBase *lb, int *starty) 
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		
		/* selection status */
		if(tselem->flag & TSE_SELECTED) {
			glRecti(0, *starty+1, (int)soops->v2d.cur.xmax, *starty+OL_H-1);
		}
		*starty-= OL_H;
		if((tselem->flag & TSE_CLOSED)==0) outliner_draw_selection(soops, &te->subtree, starty);
	}
}


static void outliner_draw_tree(SpaceOops *soops)
{
	TreeElement *te;
	int starty, startx;
	float col[4];
	
#ifdef INTERNATIONAL
	FTF_SetFontSize('l');
	BIF_SetScale(1.0);
#endif
	
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); // only once
	
	// selection first
	BIF_GetThemeColor3fv(TH_BACK, col);
	glColor3f(col[0]+0.06f, col[1]+0.08f, col[2]+0.10f);
	starty= soops->v2d.tot.ymax-OL_H;
	outliner_draw_selection(soops, &soops->tree, &starty);
	
	// grey hierarchy lines
	BIF_ThemeColorBlend(TH_BACK, TH_TEXT, 0.2);
	starty= soops->v2d.tot.ymax-OL_H/2;
	startx= 6;
	outliner_draw_hierarchy(soops, &soops->tree, startx, &starty);
	
	// items themselves
	starty= soops->v2d.tot.ymax-OL_H;
	startx= 0;
	for(te= soops->tree.first; te; te= te->next) {
		outliner_draw_tree_element(soops, te, startx, &starty);
	}
}


static void outliner_back(SpaceOops *soops)
{
	int ystart;
	
	BIF_ThemeColorShade(TH_BACK, 6);
	ystart= soops->v2d.tot.ymax;
	ystart= OL_H*(ystart/(OL_H));
	
	while(ystart > soops->v2d.cur.ymin) {
		glRecti(0, ystart, (int)soops->v2d.cur.xmax, ystart+OL_H);
		ystart-= 2*OL_H;
	}
}

static void outliner_draw_restrictcols(SpaceOops *soops)
{
	int ystart;
	
	/* background underneath */
	BIF_ThemeColor(TH_BACK);
	glRecti((int)soops->v2d.cur.xmax-OL_TOGW, soops->v2d.cur.ymin, (int)soops->v2d.cur.xmax, soops->v2d.cur.ymax);
	
	BIF_ThemeColorShade(TH_BACK, 6);
	ystart= soops->v2d.tot.ymax;
	ystart= OL_H*(ystart/(OL_H));
	
	while(ystart > soops->v2d.cur.ymin) {
		glRecti((int)soops->v2d.cur.xmax-OL_TOGW, ystart, (int)soops->v2d.cur.xmax, ystart+OL_H);
		ystart-= 2*OL_H;
	}
	
	BIF_ThemeColorShadeAlpha(TH_BACK, -15, -200);

	/* view */
	fdrawline(soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX,
		soops->v2d.cur.ymax,
		soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX,
		soops->v2d.cur.ymin);

	/* render */
	fdrawline(soops->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX,
		soops->v2d.cur.ymax,
		soops->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX,
		soops->v2d.cur.ymin);

	/* render */
	fdrawline(soops->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX,
		soops->v2d.cur.ymax,
		soops->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX,
		soops->v2d.cur.ymin);
}

static void restrictbutton_view_cb(void *poin, void *poin2)
{
	Base *base;
	Object *ob = (Object *)poin;
	
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

	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWVIEW3D, 0);
}

static void restrictbutton_sel_cb(void *poin, void *poin2)
{
	Base *base;
	Object *ob = (Object *)poin;
	
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

	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWVIEW3D, 0);
}

static void restrictbutton_rend_cb(void *poin, void *poin2)
{
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWVIEW3D, 0);
}

static void restrictbutton_r_lay_cb(void *poin, void *poin2)
{
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWNODE, 0);
	allqueue(REDRAWBUTSSCENE, 0);
}

static void restrictbutton_modifier_cb(void *poin, void *poin2)
{
	Object *ob = (Object *)poin;
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	object_handle_update(ob);
	countall();

	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}

static void restrictbutton_bone_cb(void *poin, void *poin2)
{
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

static void namebutton_cb(void *tep, void *oldnamep)
{
	SpaceOops *soops= curarea->spacedata.first;
	TreeStore *ts= soops->treestore;
	TreeElement *te= tep;
	
	if(ts && te) {
		TreeStoreElem *tselem= TREESTORE(te);
		
		if(tselem->type==0) {
			test_idbutton(tselem->id->name+2);	// library.c, unique name and alpha sort
			
			/* Check the library target exists */
			if (te->idcode == ID_LI) {
				char expanded[FILE_MAXDIR + FILE_MAXFILE];
				BLI_strncpy(expanded, ((Library *)tselem->id)->name, FILE_MAXDIR + FILE_MAXFILE);
				BLI_convertstringcode(expanded, G.sce);
				if (!BLI_exists(expanded)) {
					error("This path does not exist, correct this before saving");
				}
			}
		}
		else {
			switch(tselem->type) {
			case TSE_DEFGROUP:
				unique_vertexgroup_name(te->directdata, (Object *)tselem->id); //	id = object
				allqueue(REDRAWBUTSEDIT, 0);
				break;
			case TSE_NLA_ACTION:
				test_idbutton(tselem->id->name+2);
				break;
			case TSE_EBONE:
				if(G.obedit && G.obedit->data==(ID *)tselem->id) {
					EditBone *ebone= te->directdata;
					char newname[32];
					
					/* restore bone name */
					BLI_strncpy(newname, ebone->name, 32);
					BLI_strncpy(ebone->name, oldnamep, 32);
					armature_bone_rename(G.obedit->data, oldnamep, newname);
				}
				allqueue(REDRAWOOPS, 0);
				allqueue(REDRAWVIEW3D, 1);
				allqueue(REDRAWBUTSEDIT, 0);
				break;

			case TSE_BONE:
				{
					Bone *bone= te->directdata;
					Object *ob;
					char newname[32];
					
					// always make current object active
					tree_element_active_object(soops, te);
					ob= OBACT;
					
					/* restore bone name */
					BLI_strncpy(newname, bone->name, 32);
					BLI_strncpy(bone->name, oldnamep, 32);
					armature_bone_rename(ob->data, oldnamep, newname);
				}
				allqueue(REDRAWOOPS, 0);
				allqueue(REDRAWVIEW3D, 1);
				allqueue(REDRAWBUTSEDIT, 0);
				break;
			case TSE_POSE_CHANNEL:
				{
					bPoseChannel *pchan= te->directdata;
					Object *ob;
					char newname[32];
					
					// always make current object active
					tree_element_active_object(soops, te);
					ob= OBACT;
					
					/* restore bone name */
					BLI_strncpy(newname, pchan->name, 32);
					BLI_strncpy(pchan->name, oldnamep, 32);
					armature_bone_rename(ob->data, oldnamep, newname);
				}
				allqueue(REDRAWOOPS, 0);
				allqueue(REDRAWVIEW3D, 1);
				allqueue(REDRAWBUTSEDIT, 0);
				break;
			case TSE_POSEGRP:
				{
					Object *ob= (Object *)tselem->id; // id = object
					bActionGroup *grp= te->directdata;
					
					BLI_uniquename(&ob->pose->agroups, grp, "Group", offsetof(bActionGroup, name), 32);
					allqueue(REDRAWBUTSEDIT, 0);
				}
				break;
			case TSE_R_LAYER:
				allqueue(REDRAWOOPS, 0);
				allqueue(REDRAWBUTSSCENE, 0);
				break;
			}
		}
	}
	scrarea_queue_redraw(curarea);
}

static void outliner_draw_restrictbuts(uiBlock *block, SpaceOops *soops, ListBase *lb)
{	
	uiBut *bt;
	TreeElement *te;
	TreeStoreElem *tselem;
	Object *ob = NULL;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(te->ys >= soops->v2d.cur.ymin && te->ys <= soops->v2d.cur.ymax) {	
			/* objects have toggle-able restriction flags */
			if(tselem->type==0 && te->idcode==ID_OB) {
				ob = (Object *)tselem->id;
				
				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitS(block, ICONTOG, OB_RESTRICT_VIEW, REDRAWALL, ICON_RESTRICT_VIEW_OFF, 
						(int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, te->ys, 17, OL_H-1, &(ob->restrictflag), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_view_cb, ob, NULL);
				uiButSetFlag(bt, UI_NO_HILITE);
				
				bt= uiDefIconButBitS(block, ICONTOG, OB_RESTRICT_SELECT, REDRAWALL, ICON_RESTRICT_SELECT_OFF, 
						(int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX, te->ys, 17, OL_H-1, &(ob->restrictflag), 0, 0, 0, 0, "Restrict/Allow selection in the 3D View");
				uiButSetFunc(bt, restrictbutton_sel_cb, ob, NULL);
				uiButSetFlag(bt, UI_NO_HILITE);
				
				bt= uiDefIconButBitS(block, ICONTOG, OB_RESTRICT_RENDER, REDRAWALL, ICON_RESTRICT_RENDER_OFF, 
						(int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX, te->ys, 17, OL_H-1, &(ob->restrictflag), 0, 0, 0, 0, "Restrict/Allow renderability");
				uiButSetFunc(bt, restrictbutton_rend_cb, NULL, NULL);
				uiButSetFlag(bt, UI_NO_HILITE);
				
				uiBlockSetEmboss(block, UI_EMBOSS);
			}
			/* scene render layers and passes have toggle-able flags too! */
			else if(tselem->type==TSE_R_LAYER) {
				uiBlockSetEmboss(block, UI_EMBOSSN);
				
				bt= uiDefIconButBitI(block, ICONTOGN, SCE_LAY_DISABLE, REDRAWBUTSSCENE, ICON_CHECKBOX_HLT-1, 
									 (int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, te->ys, 17, OL_H-1, te->directdata, 0, 0, 0, 0, "Render this RenderLayer");
				uiButSetFunc(bt, restrictbutton_r_lay_cb, NULL, NULL);
				
				uiBlockSetEmboss(block, UI_EMBOSS);
			}
			else if(tselem->type==TSE_R_PASS) {
				int *layflag= te->directdata;
				uiBlockSetEmboss(block, UI_EMBOSSN);
				
				/* NOTE: tselem->nr is short! */
				bt= uiDefIconButBitI(block, ICONTOG, tselem->nr, REDRAWBUTSSCENE, ICON_CHECKBOX_HLT-1, 
									 (int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, te->ys, 17, OL_H-1, layflag, 0, 0, 0, 0, "Render this Pass");
				uiButSetFunc(bt, restrictbutton_r_lay_cb, NULL, NULL);
				
				layflag++;	/* is lay_xor */
				if(ELEM6(tselem->nr, SCE_PASS_SPEC, SCE_PASS_SHADOW, SCE_PASS_AO, SCE_PASS_REFLECT, SCE_PASS_REFRACT, SCE_PASS_RADIO))
					bt= uiDefIconButBitI(block, TOG, tselem->nr, REDRAWBUTSSCENE, (*layflag & tselem->nr)?ICON_DOT:ICON_BLANK1, 
									 (int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_SELECTX, te->ys, 17, OL_H-1, layflag, 0, 0, 0, 0, "Exclude this Pass from Combined");
				uiButSetFunc(bt, restrictbutton_r_lay_cb, NULL, NULL);
				
				uiBlockSetEmboss(block, UI_EMBOSS);
			}
			else if(tselem->type==TSE_MODIFIER)  {
				ModifierData *md= (ModifierData *)te->directdata;
				ob = (Object *)tselem->id;
				
				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitI(block, ICONTOGN, eModifierMode_Realtime, REDRAWALL, ICON_RESTRICT_VIEW_OFF, 
						(int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, te->ys, 17, OL_H-1, &(md->mode), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_modifier_cb, ob, NULL);
				uiButSetFlag(bt, UI_NO_HILITE);
				
				bt= uiDefIconButBitI(block, ICONTOGN, eModifierMode_Render, REDRAWALL, ICON_RESTRICT_RENDER_OFF, 
						(int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_RENDERX, te->ys, 17, OL_H-1, &(md->mode), 0, 0, 0, 0, "Restrict/Allow renderability");
				uiButSetFunc(bt, restrictbutton_modifier_cb, ob, NULL);
				uiButSetFlag(bt, UI_NO_HILITE);
			}
			else if(tselem->type==TSE_POSE_CHANNEL)  {
				bPoseChannel *pchan= (bPoseChannel *)te->directdata;
				Bone *bone = pchan->bone;

				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitI(block, ICONTOG, BONE_HIDDEN_P, REDRAWALL, ICON_RESTRICT_VIEW_OFF, 
						(int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, te->ys, 17, OL_H-1, &(bone->flag), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_bone_cb, NULL, NULL);
				uiButSetFlag(bt, UI_NO_HILITE);
			}
			else if(tselem->type==TSE_EBONE)  {
				EditBone *ebone= (EditBone *)te->directdata;

				uiBlockSetEmboss(block, UI_EMBOSSN);
				bt= uiDefIconButBitI(block, ICONTOG, BONE_HIDDEN_A, REDRAWALL, ICON_RESTRICT_VIEW_OFF, 
						(int)soops->v2d.cur.xmax-OL_TOG_RESTRICT_VIEWX, te->ys, 17, OL_H-1, &(ebone->flag), 0, 0, 0, 0, "Restrict/Allow visibility in the 3D View");
				uiButSetFunc(bt, restrictbutton_bone_cb, NULL, NULL);
				uiButSetFlag(bt, UI_NO_HILITE);
			}
		}
		
		if((tselem->flag & TSE_CLOSED)==0) outliner_draw_restrictbuts(block, soops, &te->subtree);
	}
}

static void outliner_buttons(uiBlock *block, SpaceOops *soops, ListBase *lb)
{
	uiBut *bt;
	TreeElement *te;
	TreeStoreElem *tselem;
	int dx, len;
	
	for(te= lb->first; te; te= te->next) {
		tselem= TREESTORE(te);
		if(te->ys >= soops->v2d.cur.ymin && te->ys <= soops->v2d.cur.ymax) {
			
			if(tselem->flag & TSE_TEXTBUT) {
				if(tselem->type == TSE_POSE_BASE) continue; // prevent crash when trying to rename 'pose' entry of armature
				
				if(tselem->type==TSE_EBONE) len = sizeof(((EditBone*) 0)->name);
				else if (tselem->type==TSE_MODIFIER) len = sizeof(((ModifierData*) 0)->name);
				else if(tselem->id && GS(tselem->id->name)==ID_LI) len = sizeof(((Library*) 0)->name);
				else len= sizeof(((ID*) 0)->name)-2;
				
				dx= BIF_GetStringWidth(G.font, te->name, 0);
				if(dx<50) dx= 50;
				
				bt= uiDefBut(block, TEX, OL_NAMEBUTTON, "",  te->xs+2*OL_X-4, te->ys, dx+10, OL_H-1, te->name, 1.0, (float)len-1, 0, 0, "");
				uiButSetFunc(bt, namebutton_cb, te, NULL);

				// signal for button to open
				addqueue(curarea->win, BUT_ACTIVATE, OL_NAMEBUTTON);
				
				/* otherwise keeps open on ESC */
				tselem->flag &= ~TSE_TEXTBUT;
			}
		}
		
		if((tselem->flag & TSE_CLOSED)==0) outliner_buttons(block, soops, &te->subtree);
	}
}

void draw_outliner(ScrArea *sa, SpaceOops *soops)
{
	uiBlock *block;
	int sizey, sizex;
	short ofsx, ofsy;
	
	/* version patch for old outliners here - do_versions patch doesn't work */
	if (G.v2d->scroll != L_SCROLL+B_SCROLLO) {
		init_v2d_oops(curarea, soops);
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		calc_scrollrcts(sa, G.v2d, sa->winx, sa->winy);
	}
	else
		calc_scrollrcts(sa, G.v2d, sa->winx, sa->winy);
	
	if(sa->winx>SCROLLB+10 && sa->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= sa->winrct.xmin;	/* because mywin */
			ofsy= sa->winrct.ymin;
			glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
			glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
		}
	}
	
	outliner_build_tree(soops); // always 
	sizey = sizex = 0;
	outliner_height(soops, &soops->tree, &sizey);
	outliner_width(soops, &soops->tree, &sizex);
	
	/* we init all tot rect vars, only really needed on window size change though */
	G.v2d->tot.xmin= 0.0;
	G.v2d->tot.xmax= (G.v2d->mask.xmax-G.v2d->mask.xmin);
	if(soops->flag & SO_HIDE_RESTRICTCOLS) {
		if(G.v2d->tot.xmax <= sizex)
			G.v2d->tot.xmax= 2*sizex;
	}
	else {
		if(G.v2d->tot.xmax-OL_TOGW <= sizex)
			G.v2d->tot.xmax= 2*sizex;
	}
	G.v2d->tot.ymax= 0.0;
	G.v2d->tot.ymin= -sizey*OL_H;
	test_view2d(G.v2d, sa->winx, sa->winy);

	// align on top window if cur bigger than tot
	if(G.v2d->cur.ymax-G.v2d->cur.ymin > sizey*OL_H) {
		G.v2d->cur.ymax= 0.0;
		G.v2d->cur.ymin= -(G.v2d->mask.ymax-G.v2d->mask.ymin);
	}

	myortho2(G.v2d->cur.xmin-0.375, G.v2d->cur.xmax-0.375, G.v2d->cur.ymin-0.375, G.v2d->cur.ymax-0.375);

	/* draw outliner stuff (background and hierachy lines) */
	outliner_back(soops);
	outliner_draw_tree(soops);

	/* restore viewport */
	mywinset(sa->win);
	
	/* ortho corrected - 'pixel space' */
	myortho2(G.v2d->cur.xmin-SCROLLB-0.375, G.v2d->cur.xmax-0.375, G.v2d->cur.ymin-SCROLLH-0.375, G.v2d->cur.ymax-0.375);
	
	/* draw icons and names */
	block= uiNewBlock(&sa->uiblocks, "outliner buttons", UI_EMBOSS, UI_HELV, sa->win);
	outliner_buttons(block, soops, &soops->tree);
	
	/* draw restriction columns */
	if (!(soops->flag & SO_HIDE_RESTRICTCOLS)) {
		outliner_draw_restrictcols(soops);
		outliner_draw_restrictbuts(block, soops, &soops->tree);
	}
	
	uiDrawBlock(block);
	
	/* clear flag that allows quick redraws */
	soops->storeflag &= ~SO_TREESTORE_REDRAW;
	
	/* drawoopsspace handles sliders */
}
