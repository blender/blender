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
 * editarmature.c: Interface for creating and posing armature objects
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_mesh_types.h"

#include "BKE_utildefines.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_editarmature.h"
#include "BIF_poseobject.h"
#include "BIF_mywindow.h"
#include "BIF_editdeform.h"

#include "BDR_editobject.h"
#include "BDR_drawobject.h"

#include "BSE_edit.h"
#include "BSE_view.h"
#include "BSE_trans_types.h"
#include "BSE_editaction.h"

#include "mydevice.h"
#include "interface.h"
#include "blendef.h"
#include "nla.h"

/*  >>>>> FIXME: ARG!  Colours should be defined in a header somewhere! */
/*	Note, these came from drawobject.c  They really should be in a nice header file somewhere */
#define B_YELLOW	0x77FFFF
#define B_PURPLE	0xFF70FF

#define B_CYAN		0xFFFF00
#define	B_AQUA		0xFFBB55 /* 0xFF8833*/

extern	int tottrans;					/* Originally defined in editobject.c */
extern	struct TransOb *transmain;				/* Originally defined in editobject.c */
extern	float centre[3], centroid[3];	/* Originally defined in editobject.c */

/*	Macros	*/
#define TEST_EDITARMATURE {if(G.obedit==0) return; if( (G.vd->lay & G.obedit->lay)==0 ) return;}

/* Local Function Prototypes */
static void editbones_to_armature (ListBase *bones, Object *ob);
static int editbone_to_parnr (EditBone *bone);

static void validate_editbonebutton(EditBone *bone);
static void fix_bonelist_roll (ListBase *bonelist, ListBase *editbonelist);
static int	select_bonechildren_by_name (struct Bone *bone, char *name, int select);
static void build_bonestring (char *string, struct EditBone *bone);
static void draw_boneverti (float x, float y, float z, float size, int flag);
static void draw_bone (int armflag, int boneflag, unsigned int id, char *name, float length);
static void draw_bonechildren (struct Bone *bone, int flag, unsigned int *index);
static void add_bone_input (struct Object *ob);
static void make_boneList(struct ListBase* list, struct ListBase *bones, struct EditBone *parent);
static void make_bone_menu_children (struct Bone *bone, char *str, int *index);
static void delete_bone(struct EditBone* exBone);
static void clear_armature_children (struct Bone *bone, struct bPose *pose, char mode);
static void parnr_to_editbone(EditBone *bone);

static int	count_bones (struct bArmature *arm, int flagmask, int allbones);

static int	count_bonechildren (struct Bone *bone, int incount, int flagmask, int allbones);
static int	add_trans_bonechildren (struct Object* ob, struct Bone* bone, struct TransOb* buffer, int index, char mode);
static void deselect_bonechildren (struct Bone *bone, int mode);
static void selectconnected_posebonechildren (struct Bone *bone);

static int	editbone_name_exists (char* name);
static void unique_editbone_name (char* name);
static void *get_nearest_bone (int findunsel);
static EditBone * get_nearest_editbonepoint (int findunsel, int *selmask);

static void attach_bone_to_parent(EditBone *bone);
static Bone *get_first_selected_bonechildren (Bone *bone);


/* Functions */



void apply_rot_armature (Object *ob, float mat[3][3]){
	ListBase	list;
	EditBone *ebone;
	bArmature *arm;

	arm = get_armature(ob);

	if (!arm)
		return;

	/* Put the armature into editmode */
	list.first= list.last = NULL;
	make_boneList(&list, &arm->bonebase, NULL);

	/* Do the rotations */
	for (ebone = list.first; ebone; ebone=ebone->next){
	
		{

			/* Fixme: This is essentially duplicated from join_armature */

			float premat[4][4];
			float postmat[4][4];
			float difmat[4][4];
			float imat[4][4];
			float temp[4][4];
			float delta[3];
			float rmat[4][4];
			
			Mat4CpyMat3 (rmat, mat);
			/* Get the premat */
			VecSubf (delta, ebone->tail, ebone->head);
			make_boneMatrixvr(temp, delta, ebone->roll);
			Mat4MulMat4 (premat, temp, rmat);
			
			Mat4MulVecfl(rmat, ebone->head);
			Mat4MulVecfl(rmat, ebone->tail);
			
			/* Get the postmat */
			VecSubf (delta, ebone->tail, ebone->head);
			make_boneMatrixvr(postmat, delta, ebone->roll);
			
			/* Find the roll */
			Mat4Invert (imat, premat);
			Mat4MulMat4 (difmat, postmat, imat);
			
#if 0
			printmatrix4 ("Difmat", difmat);
#endif
			ebone->roll -=atan(difmat[2][0]/difmat[2][2]);
			
			if (difmat[0][0]<0)
				ebone->roll +=M_PI;
			
		}

		
	}
	
	/* Turn the list into an armature */
	editbones_to_armature(&list, ob);
	
	/* Free the editbones */
	if (list.first){
		BLI_freelistN (&list);
	}

}



static Bone *get_first_selected_bonechildren (Bone *bone)
{
	Bone *curbone, *result;

	if (bone->flag & BONE_SELECTED)
		return bone;

	for (curbone = bone->childbase.first; curbone; curbone=curbone->next){
		result = get_first_selected_bonechildren(curbone);
		if (result)
			return result;
	};

	return NULL;
}

Bone *get_first_selected_bone (void)
{
	Bone *curbone, *result;
	bArmature *arm;

	arm = get_armature(OBACT);
	if (!arm)
		return NULL;

	for (curbone = arm->bonebase.first; curbone; curbone=curbone->next){
		result = get_first_selected_bonechildren(curbone);
		if (result)
			return result;
	}

	return NULL;
}

void clever_numbuts_posearmature (void)
{
	/* heh -- 'clever numbuts'! */
	bArmature *arm;
	Bone *bone;
	bPoseChannel *chan;

	arm = get_armature(OBACT);
	if (!arm)
		return;

	bone = get_first_selected_bone();

	if (!bone)
		return;

	add_numbut(0, NUM|FLO, "Loc X:", -G.vd->far, G.vd->far, bone->loc, 0);
	add_numbut(1, NUM|FLO, "Loc Y:", -G.vd->far, G.vd->far, bone->loc+1, 0);
	add_numbut(2, NUM|FLO, "Loc Z:", -G.vd->far, G.vd->far, bone->loc+2, 0);

	add_numbut(3, NUM|FLO, "Quat X:", -G.vd->far, G.vd->far, bone->quat, 0);
	add_numbut(4, NUM|FLO, "Quat Y:", -G.vd->far, G.vd->far, bone->quat+1, 0);
	add_numbut(5, NUM|FLO, "Quat Z:", -G.vd->far, G.vd->far, bone->quat+2, 0);
	add_numbut(6, NUM|FLO, "Quat W:", -G.vd->far, G.vd->far, bone->quat+3, 0);

	add_numbut(7, NUM|FLO, "Size X:", -G.vd->far, G.vd->far, bone->size, 0);
	add_numbut(8, NUM|FLO, "Size Y:", -G.vd->far, G.vd->far, bone->size+1, 0);
	add_numbut(9, NUM|FLO, "Size Z:", -G.vd->far, G.vd->far, bone->size+2, 0);

	do_clever_numbuts("Active Bone", 10, REDRAW);

	/* This is similar to code in special_trans_update */
	
	if (!G.obpose->pose) G.obpose->pose= MEM_callocN(sizeof(bPose), "pose");
	chan = MEM_callocN (sizeof (bPoseChannel), "transPoseChannel");

	chan->flag |= POSE_LOC|POSE_ROT|POSE_SIZE;
	memcpy (chan->loc, bone->loc, sizeof (chan->loc));
	memcpy (chan->quat, bone->quat, sizeof (chan->quat));
	memcpy (chan->size, bone->size, sizeof (chan->size));
	strcpy (chan->name, bone->name);
	
	set_pose_channel (G.obpose->pose, chan);
	
}

void clever_numbuts_armature (void)
{
	EditBone *ebone, *child;
	
	ebone= G.edbo.first;

	for (ebone = G.edbo.first; ebone; ebone=ebone->next){
		if (ebone->flag & BONE_SELECTED)
			break;
	}

	if (!ebone)
		return;

	add_numbut(0, NUM|FLO, "Root X:", -G.vd->far, G.vd->far, ebone->head, 0);
	add_numbut(1, NUM|FLO, "Root Y:", -G.vd->far, G.vd->far, ebone->head+1, 0);
	add_numbut(2, NUM|FLO, "Root Z:", -G.vd->far, G.vd->far, ebone->head+2, 0);

	add_numbut(3, NUM|FLO, "Tip X:", -G.vd->far, G.vd->far, ebone->tail, 0);
	add_numbut(4, NUM|FLO, "Tip Y:", -G.vd->far, G.vd->far, ebone->tail+1, 0);
	add_numbut(5, NUM|FLO, "Tip Z:", -G.vd->far, G.vd->far, ebone->tail+2, 0);

	/* Convert roll to degrees */
	ebone->roll *= (180.0F/M_PI);
	add_numbut(6, NUM|FLO, "Roll:", -G.vd->far, G.vd->far, &ebone->roll, 0);

	do_clever_numbuts("Active Bone", 7, REDRAW);

	/* Convert roll to radians */
	ebone->roll /= (180.0F/M_PI);

	//	Update our parent
	if (ebone->parent && ebone->flag & BONE_IK_TOPARENT){
		VECCOPY (ebone->parent->tail, ebone->head);
	}

	//	Update our children if necessary
	for (child = G.edbo.first; child; child=child->next){
		if (child->parent == ebone && child->flag & BONE_IK_TOPARENT){
			VECCOPY (child->head, ebone->tail);
		}
	}
}

void select_bone_by_name (bArmature *arm, char *name, int select)
{
	Bone *bone;

	if (!arm)
		return;

	for (bone=arm->bonebase.first; bone; bone=bone->next)
		if (select_bonechildren_by_name (bone, name, select))
			break;
}

static int select_bonechildren_by_name (Bone *bone, char *name, int select)
{
	Bone *curBone;

	if (!strcmp (bone->name, name)){
		if (select)
			bone->flag |= BONE_SELECTED;
		else
			bone->flag &= ~BONE_SELECTED;
		return 1;
	}

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		if (select_bonechildren_by_name (curBone, name, select))
			return 1;
	}

	return 0;
}
void selectconnected_armature(void)
{
	EditBone *bone, *curBone, *next;

	if (G.qual & LR_SHIFTKEY)
		bone= get_nearest_bone(0);
	else
		bone= get_nearest_bone(1);

	if (!bone)
		return;

	/* Select parents */
	for (curBone=bone; curBone; curBone=next){
		if (G.qual & LR_SHIFTKEY){
			curBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
		}
		else{
			curBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
		}

		if (curBone->flag & BONE_IK_TOPARENT)
			next=curBone->parent;
		else
			next=NULL;
	}

	/* Select children */
	while (bone){
		for (curBone=G.edbo.first; curBone; curBone=next){
			next = curBone->next;
			if (curBone->parent == bone){
				if (curBone->flag & BONE_IK_TOPARENT){
					if (G.qual & LR_SHIFTKEY)
						curBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					else
						curBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					bone=curBone;
					break;
				}
				else{ 
					bone=NULL;
					break;
				}
			}
		}
		if (!curBone)
			bone=NULL;

	}

	countall();
	allqueue (REDRAWVIEW3D, 0);

}

void selectconnected_posearmature(void)
{
	Bone *bone, *curBone, *next;

	if (G.qual & LR_SHIFTKEY)
		bone= get_nearest_bone(0);
	else
		bone = get_nearest_bone(1);

	if (!bone)
		return;

	/* Select parents */
	for (curBone=bone; curBone; curBone=next){
		select_actionchannel_by_name (G.obpose->action, curBone->name, !(G.qual & LR_SHIFTKEY));
		if (G.qual & LR_SHIFTKEY)
			curBone->flag &= ~BONE_SELECTED;
		else
			curBone->flag |= BONE_SELECTED;

		if (curBone->flag & BONE_IK_TOPARENT)
			next=curBone->parent;
		else
			next=NULL;
	}

	/* Select children */
	for (curBone=bone->childbase.first; curBone; curBone=next){
		selectconnected_posebonechildren (curBone);
	}
	
	countall();
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
}

static void selectconnected_posebonechildren (Bone *bone)
{
	Bone *curBone;

	if (!(bone->flag & BONE_IK_TOPARENT))
		return;

	select_actionchannel_by_name (G.obpose->action, bone->name, !(G.qual & LR_SHIFTKEY));

	if (G.qual & LR_SHIFTKEY)
		bone->flag &= ~BONE_SELECTED;
	else
		bone->flag |= BONE_SELECTED;

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		selectconnected_posebonechildren (curBone);
	}
}


char *make_bone_menu (bArmature *arm)
{
	char *menustr=NULL;
	Bone *curBone;
	int		size;
	int		index=0;
	

	//	Count the bones
	size = (count_bones (arm, 0xFFFFFFFF, 1)*48) + 256;
	menustr = MEM_callocN(size, "bonemenu");

	sprintf (menustr, "Select Bone%%t");

	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		make_bone_menu_children (curBone, menustr, &index);
	}

	return menustr;
}

static void make_bone_menu_children (Bone *bone, char *str, int *index)
{
	Bone *curBone;

	sprintf (str, "%s|%s%%x%d", str, bone->name, *index);
	(*index) ++;

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next)
		make_bone_menu_children (curBone, str, index);
}

void free_editArmature(void)
{

	/*	Clear the editbones list */
	if (G.edbo.first){
		BLI_freelistN (&G.edbo);
	}
}

static EditBone * get_nearest_editbonepoint (int findunsel, int *selmask){
	EditBone	*ebone;
	GLuint		buffer[MAXPICKBUF];
	short		hits;
	int		 i, takeNext=0;
	int		sel;
	unsigned int	hitresult, hitbone, firstunSel=-1;

	glInitNames();
	hits=selectprojektie(buffer, 0, 0, 0, 0);

	/* See if there are any selected bones in this group */
	if (hits){
		for (i=0; i< hits; i++){
			hitresult = buffer[3+(i*4)];
			if (!(hitresult&BONESEL_NOSEL)){

				/* Determine which points are selected */
				hitbone = hitresult & ~(BONESEL_ROOT|BONESEL_TIP);

				/* Determine what the current bone is */
				ebone = BLI_findlink(&G.edbo, hitbone);

				/* See if it is selected */
				sel = 0;
				if ((hitresult & (BONESEL_TIP|BONESEL_ROOT)) == (BONESEL_TIP|BONESEL_ROOT))
					sel = (ebone->flag & (BONE_TIPSEL| BONE_ROOTSEL)) == (BONE_TIPSEL|BONE_ROOTSEL) ? 1 : 0;
				else if (hitresult & BONESEL_TIP)
					sel |= ebone->flag & BONE_TIPSEL;
				else if (hitresult & BONESEL_ROOT)
					sel |= ebone->flag & BONE_ROOTSEL;
				if (!findunsel)
					sel = !sel;
			
				if (sel)
					takeNext=1;
				else{
					if (firstunSel == -1)
						firstunSel = hitresult;
					if (takeNext){
						*selmask =0;
						if (hitresult & BONESEL_ROOT)
							*selmask |= BONE_ROOTSEL;
						if (hitresult & BONESEL_TIP)
							*selmask |= BONE_TIPSEL;
						return ebone;
					}
				}
			}
		}

		if (firstunSel != -1){
			*selmask = 0;
			if (firstunSel & BONESEL_ROOT)
				*selmask |= BONE_ROOTSEL;
			if (firstunSel & BONESEL_TIP)
				*selmask |= BONE_TIPSEL;
			return BLI_findlink(&G.edbo, firstunSel & ~(BONESEL_ROOT|BONESEL_TIP));
		}
#if 1
		else{
			*selmask = 0;
			if (buffer[3] & BONESEL_ROOT)
				*selmask |= BONE_ROOTSEL;
			if (buffer[3] & BONESEL_TIP)
				*selmask |= BONE_TIPSEL;
#if 1
			return BLI_findlink(&G.edbo, buffer[3] & ~(BONESEL_ROOT|BONESEL_TIP));
#else
			return NULL;
#endif
		}
#endif
	}

	*selmask = 0;
	return NULL;
}

static void * get_nearest_bone (int findunsel){
	void		*firstunSel=NULL, *data;
	GLuint		buffer[MAXPICKBUF];
	short		hits;
	int		 i, takeNext=0;
	int		sel;
	unsigned int	hitresult;
	Bone *bone;
	EditBone *ebone;

	glInitNames();
	hits=selectprojektie(buffer, 0, 0, 0, 0);


	/* See if there are any selected bones in this group */
	if (hits){
		for (i=0; i< hits; i++){
			hitresult = buffer[3+(i*4)];
			if (!(hitresult&BONESEL_NOSEL)){

				/* Determine which points are selected */
				hitresult &= ~(BONESEL_ROOT|BONESEL_TIP);

				/* Determine what the current bone is */
				if (!G.obedit){
					bone = get_indexed_bone(OBACT->data, hitresult);
					if (findunsel)
						sel = (bone->flag & BONE_SELECTED);
					else
						sel = !(bone->flag & BONE_SELECTED);					
					data = bone;
				}
				else{
					ebone = BLI_findlink(&G.edbo, hitresult);
					if (findunsel)
						sel = (ebone->flag & BONE_SELECTED);
					else
						sel = !(ebone->flag & BONE_SELECTED);
					
					data = ebone;
				}
				
				if (sel)
					takeNext=1;
				else{
					if (!firstunSel)
						firstunSel=data;
					if (takeNext)
						return data;
				}
			}
		}

		if (firstunSel)
			return firstunSel;
#if 1
		else{
#if 1
			if (G.obedit)
				return BLI_findlink(&G.edbo, buffer[3] & ~(BONESEL_ROOT|BONESEL_TIP));
			else
				return get_indexed_bone(OBACT->data, buffer[3] & ~(BONESEL_ROOT|BONESEL_TIP));
#else
			return NULL;
#endif
		}
#endif
	}

	return NULL;
}

void delete_armature(void)
{
	EditBone	*curBone, *next;
	
	TEST_EDITARMATURE;
	if(okee("Erase selected")==0) return;
	
	for (curBone=G.edbo.first;curBone;curBone=next){
		next=curBone->next;
		if (curBone->flag&BONE_SELECTED)
			delete_bone(curBone);
	}
	
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSCONSTRAINT, 0);
	countall();
}


static void delete_bone(EditBone* exBone)
{
	EditBone	*curBone;
	bPoseChannel *chan;
	
	/*	Find any bones that refer to this bone	*/
	for (curBone=G.edbo.first;curBone;curBone=curBone->next){
		if (curBone->parent==exBone){
			curBone->parent=exBone->parent;
			curBone->flag &= ~BONE_IK_TOPARENT;
		}
	}
	
	/*  Erase any associated pose channel */
	if (G.obedit->pose){
		for (chan=G.obedit->pose->chanbase.first; chan; chan=chan->next){
			if (!strcmp (chan->name, exBone->name)){
				free_constraints(&chan->constraints);
				BLI_freelinkN (&G.obedit->pose->chanbase, chan);
				break;
			}
		}
	}


	allqueue(REDRAWBUTSCONSTRAINT, 0);
	allqueue(REDRAWBUTSEDIT, 0);

	BLI_freelinkN (&G.edbo,exBone);
}

void remake_editArmature(void)
{
	if(okee("Reload Original data")==0) return;
	
	make_editArmature();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSHEAD, 0);
	allqueue(REDRAWBUTSCONSTRAINT, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}

void mouse_armature(void)
{
	EditBone*	nearBone = NULL;
	int	selmask;

	nearBone=get_nearest_editbonepoint(1, &selmask);

	if (nearBone){
		if ((G.qual & LR_SHIFTKEY) && (nearBone->flag & selmask))
			nearBone->flag &= ~selmask;
		else
			nearBone->flag |= selmask;

		if (!(G.qual & LR_SHIFTKEY)){
			deselectall_armature();
			nearBone->flag |= selmask;
		}
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWBUTSCONSTRAINT, 0);
	};
	countall();
	rightmouse_transform();
}

void make_editArmature(void)
{
	bArmature	*arm;
	
	if (G.obedit==0)
		return;
	
	free_editArmature();
	
	arm= get_armature(G.obedit);
	if (!arm)
		return;
	
	make_boneList (&G.edbo, &arm->bonebase,NULL);
}

static void editbones_to_armature (ListBase *list, Object *ob)
{
	bArmature *arm;
	EditBone *eBone;
	Bone	*newBone;

	arm = get_armature(ob);
	if (!list)
		return;
	if (!arm)
		return;

	free_bones(arm);
	

	/*	Copy the bones from the editData into the armature*/
	for (eBone=list->first;eBone;eBone=eBone->next){
		newBone=MEM_callocN (sizeof(Bone), "bone");
		eBone->temp= newBone;	/* Associate the real Bones with the EditBones */
		
		strcpy (newBone->name, eBone->name);
		memcpy (newBone->head, eBone->head, sizeof(float)*3);
		memcpy (newBone->tail, eBone->tail, sizeof(float)*3);
		newBone->flag=eBone->flag &~(BONE_SELECTED|BONE_HILIGHTED);
//		newBone->roll=eBone->roll;
		newBone->roll = 0.0F;

		/*	>>>>> FIXME: This isn't a very good system: a lot of
					pointless copying involved.  To be investigated
					once things are working better.
		*/

		newBone->weight = eBone->weight;
		newBone->dist = eBone->dist;
		newBone->boneclass = eBone->boneclass;

		memcpy (newBone->loc, eBone->loc, sizeof(eBone->loc));
		memcpy (newBone->dloc, eBone->dloc, sizeof(eBone->dloc));
/*		memcpy (newBone->orig, eBone->orig, sizeof(eBone->orig));*/
		memcpy (newBone->size, eBone->size, sizeof(eBone->size));
		memcpy (newBone->dsize, eBone->dsize, sizeof(eBone->dsize));
		memcpy (newBone->quat, eBone->quat, sizeof(eBone->quat));
		memcpy (newBone->dquat, eBone->dquat, sizeof(eBone->dquat));
		memcpy (newBone->obmat, eBone->obmat, sizeof(eBone->obmat));
	}

	/*	Fix parenting in a separate pass to ensure ebone->bone connections
		are valid at this point */
	for (eBone=list->first;eBone;eBone=eBone->next){
		newBone= (Bone*) eBone->temp;
		if (eBone->parent){
			newBone->parent=(Bone*) eBone->parent->temp;
			BLI_addtail (&newBone->parent->childbase,newBone);

			{
				float M_boneRest[4][4];
				float M_parentRest[4][4];
				float iM_parentRest[4][4];
				float	delta[3];
			
				/* Get the parent's global matrix (rotation only)*/
				VecSubf (delta, eBone->parent->tail, eBone->parent->head);
				make_boneMatrixvr(M_parentRest, delta, eBone->parent->roll);

				/* Get this bone's global matrix (rotation only)*/
				VecSubf (delta, eBone->tail, eBone->head);
				make_boneMatrixvr(M_boneRest, delta, eBone->roll);

				/* Invert the parent matrix */
				Mat4Invert (iM_parentRest, M_parentRest);

				/* Get the new head and tail */
				VecSubf (newBone->head, eBone->head, eBone->parent->tail);
				VecSubf (newBone->tail, eBone->tail, eBone->parent->tail);

				Mat4MulVecfl(iM_parentRest, newBone->head);
				Mat4MulVecfl(iM_parentRest, newBone->tail);
	

			}

		}
		/*	...otherwise add this bone to the armature's bonebase */
		else
			BLI_addtail (&arm->bonebase,newBone);
	}

	/* Make a pass through the new armature to fix rolling */
	fix_bonelist_roll (&arm->bonebase, list);
	/* Get rid of pose channels that may have belonged to deleted bones */
	collect_pose_garbage(ob);
	/* Ensure all bones have channels */
	apply_pose_armature(arm, ob->pose, 0);

	/* Calculate and cache the inverse restposition matrices (needed for deformation)*/
	precalc_bonelist_irestmats(&arm->bonebase);
}


void load_editArmature(void)
{
	bArmature		*arm;

	arm=get_armature(G.obedit);
	if (!arm)
		return;
	
#if 1
	editbones_to_armature(&G.edbo, G.obedit);
#else
	free_bones(arm);
	

	/*	Copy the bones from the editData into the armature*/
	for (eBone=G.edbo.first;eBone;eBone=eBone->next){
		newBone=MEM_callocN (sizeof(Bone), "bone");
		eBone->temp= newBone;	/* Associate the real Bones with the EditBones */
		
		strcpy (newBone->name, eBone->name);
		memcpy (newBone->head, eBone->head, sizeof(float)*3);
		memcpy (newBone->tail, eBone->tail, sizeof(float)*3);
		newBone->flag=eBone->flag &~(BONE_SELECTED|BONE_HILIGHTED);
	//	newBone->roll=eBone->roll;
		newBone->roll = 0.0F;

		/*	>>>>> FIXME: This isn't a very good system: a lot of
					pointless copying involved.  To be investigated
					once things are working better.
		*/

		newBone->weight = eBone->weight;
		newBone->dist = eBone->dist;
		newBone->boneclass = eBone->boneclass;

		memcpy (newBone->loc, eBone->loc, sizeof(eBone->loc));
		memcpy (newBone->dloc, eBone->dloc, sizeof(eBone->dloc));
/*		memcpy (newBone->orig, eBone->orig, sizeof(eBone->orig));*/
		memcpy (newBone->size, eBone->size, sizeof(eBone->size));
		memcpy (newBone->dsize, eBone->dsize, sizeof(eBone->dsize));
		memcpy (newBone->quat, eBone->quat, sizeof(eBone->quat));
		memcpy (newBone->dquat, eBone->dquat, sizeof(eBone->dquat));
		memcpy (newBone->obmat, eBone->obmat, sizeof(eBone->obmat));
	}

	/*	Fix parenting in a separate pass to ensure ebone->bone connections
		are valid at this point */
	for (eBone=G.edbo.first;eBone;eBone=eBone->next){
		newBone= (Bone*) eBone->temp;
		if (eBone->parent){
			newBone->parent=(Bone*) eBone->parent->temp;
			BLI_addtail (&newBone->parent->childbase,newBone);

			{
				float M_boneRest[4][4];
				float M_parentRest[4][4];
				float M_relativeBone[4][4];
				float iM_parentRest[4][4];
				float	delta[3];
			
				/* Get the parent's global matrix (rotation only)*/
				VecSubf (delta, eBone->parent->tail, eBone->parent->head);
				make_boneMatrixvr(M_parentRest, delta, eBone->parent->roll);

				/* Get this bone's global matrix (rotation only)*/
				VecSubf (delta, eBone->tail, eBone->head);
				make_boneMatrixvr(M_boneRest, delta, eBone->roll);

				/* Invert the parent matrix */
				Mat4Invert (iM_parentRest, M_parentRest);

				/* Get the new head and tail */
				VecSubf (newBone->head, eBone->head, eBone->parent->tail);
				VecSubf (newBone->tail, eBone->tail, eBone->parent->tail);

				Mat4MulVecfl(iM_parentRest, newBone->head);
				Mat4MulVecfl(iM_parentRest, newBone->tail);
	

			}

		}
		/*	...otherwise add this bone to the armature's bonebase */
		else
			BLI_addtail (&arm->bonebase,newBone);
	}

	/* Make a pass through the new armature to fix rolling */
	fix_bonelist_roll (&arm->bonebase, &G.edbo);

	/* Get rid of pose channels that may have belonged to deleted bones */
	collect_pose_garbage(G.obedit);
#endif
}

static void fix_bonelist_roll (ListBase *bonelist, ListBase *editbonelist)
{
	Bone *curBone;
	EditBone *ebone;

	for (curBone=bonelist->first; curBone; curBone=curBone->next){
		
		/* Fix this bone's roll */
#if 1
		{
			float premat[4][4];
			float postmat[4][4];
			float difmat[4][4];
			float imat[4][4];
			float delta[3];

			/* Find the associated editbone */
			for (ebone = editbonelist->first; ebone; ebone=ebone->next)
				if ((Bone*)ebone->temp == curBone)
					break;
			
				if (ebone){
					/* Get the premat */
					VecSubf (delta, ebone->tail, ebone->head);
					make_boneMatrixvr(premat, delta, ebone->roll);
					
					/* Get the postmat */
					get_objectspace_bone_matrix(curBone, postmat, 1, 0);
					postmat[3][0]=postmat[3][1]=postmat[3][2]=0.0F;
#if 1
					Mat4Invert (imat, premat);
					Mat4MulMat4 (difmat, postmat, imat);
#else
					Mat4Invert (imat, postmat);
					Mat4MulMat4 (difmat, premat, imat);
#endif
#if 0
					printf ("Bone %s\n", curBone->name);
					printmatrix4 ("premat", premat);
					printmatrix4 ("postmat", postmat);
					printmatrix4 ("difmat", difmat);
					printf ("Roll = %f\n",  (-atan(difmat[2][0]/difmat[2][2]) * (180.0/M_PI)));
#endif
					curBone->roll = -atan(difmat[2][0]/difmat[2][2]);
					
					if (difmat[0][0]<0)
						curBone->roll +=M_PI;
					
				}
		}
#endif
		
		fix_bonelist_roll (&curBone->childbase, editbonelist);
	}
}

void make_bone_parent(void)
{
/*	error ("Bone Parenting not yet implemented");	*/
	return;
}

static void make_boneList(ListBase* list, ListBase *bones, EditBone *parent)
{
	EditBone	*eBone;
	Bone		*curBone;
	
	for (curBone=bones->first; curBone; curBone=curBone->next){
		eBone=MEM_callocN(sizeof(EditBone),"make_editbone");
		
		/*	Copy relevant data from bone to eBone */
		eBone->parent=parent;
		strcpy (eBone->name, curBone->name);
		eBone->flag = curBone->flag & ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);

		/* Print bone matrix before and after */

		get_bone_root_pos(curBone, eBone->head, 0);
		get_bone_tip_pos(curBone, eBone->tail, 0);
//		eBone->roll=curBone->roll;
		eBone->roll=0;

#if 1
		{
			float delta[3];
			float premat[4][4];
			float postmat[4][4];
			float imat[4][4];
			float difmat[4][4];

			VecSubf (delta, eBone->tail, eBone->head);
			make_boneMatrixvr(postmat, delta, eBone->roll);

			get_objectspace_bone_matrix(curBone, premat, 1, 0);

#if 0
			Mat4Invert (imat, premat);
			Mat4MulMat4 (difmat, postmat, imat);
#else
			Mat4Invert (imat, postmat);
			Mat4MulMat4 (difmat, premat, imat);
#endif
#if 0
			printf ("Bone %s\n", curBone->name);
			printmatrix4 ("diffmat", difmat);
			printf ("Roll : atan of %f / %f = %f\n", difmat[2][0], difmat[2][2], (float)atan(difmat[2][0]/difmat[2][2])*(180.0F/M_PI));
#endif
			eBone->roll = atan(difmat[2][0]/difmat[2][2]);

			 if (difmat[0][0]<0)
				eBone->roll +=M_PI;
		}
#endif		
		eBone->dist= curBone->dist;
		eBone->weight= curBone->weight;
		eBone->boneclass = curBone->boneclass;
		memcpy (eBone->loc, curBone->loc, sizeof(curBone->loc));
		memcpy (eBone->dloc, curBone->dloc, sizeof(curBone->dloc));
		/*		memcpy (eBone->orig, curBone->orig, sizeof(curBone->orig));*/
		memcpy (eBone->size, curBone->size, sizeof(curBone->size));
		memcpy (eBone->dsize, curBone->dsize, sizeof(curBone->dsize));
		memcpy (eBone->quat, curBone->quat, sizeof(curBone->quat));
		memcpy (eBone->dquat, curBone->dquat, sizeof(curBone->dquat));
		memcpy (eBone->obmat, curBone->obmat, sizeof(curBone->obmat));
		

		BLI_addtail (list, eBone);
		
		/*	Add children if necessary */
		if (curBone->childbase.first) 
			make_boneList (list, &curBone->childbase, eBone);
	}
	

}

#if 0
static EditVert*	 add_armatureVert (float *loc)
{
	EditVert*	vert=NULL;

	vert = MEM_callocN (sizeof (EditVert), "armaturevert");
	if (vert){
		VECCOPY (vert->co, loc);
		BLI_addtail (&G.edve,vert);
	}

	return vert;

}

static EditVert* get_armatureVert (float *loc)
{
	EditVert*	vert;

	for (vert=G.edve.first;vert;vert=vert->next){
		if ((vert->co[0]==loc[0])&&(vert->co[1]==loc[1])&&(vert->co[2]==loc[2])){
			return (vert);
		}
	}

	return add_armatureVert (loc);
}
#endif

void draw_armature(Object *ob)
{
	bArmature	*arm;
	Bone		*bone;
	EditBone	*eBone;
	unsigned int	index;
	float		delta[3],offset[3];
	float		bmat[4][4];
	float	length;
	
	if (ob==NULL) return;
	
	arm= ob->data; 
	if (arm==NULL) return;

	if (!(ob->lay & G.vd->lay))
		return;

	if (arm->flag & ARM_DRAWXRAY) {
		if(G.zbuf) glDisable(GL_DEPTH_TEST);
	}

	/* If we're in editmode, draw the Global edit data */
	if(ob==G.obedit || (G.obedit && ob->data==G.obedit->data)) {
		cpack (0x000000);
		
		arm->flag |= ARM_EDITMODE;
		for (eBone=G.edbo.first, index=0; eBone; eBone=eBone->next, index++){
			if (ob==G.obedit){
	//			if ((eBone->flag&(BONE_TIPSEL | BONE_ROOTSEL))==(BONE_TIPSEL|BONE_ROOTSEL))
	//				cpack (B_YELLOW);
	//			else
					cpack (B_PURPLE);
			}
			else cpack (0x000000);
			
			glPushMatrix();
			
			/*	Compose the parent transforms (i.e. their translations) */
			VECCOPY (offset,eBone->head);

			glTranslatef (offset[0],offset[1],offset[2]);
			
			delta[0]=eBone->tail[0]-eBone->head[0];	
			delta[1]=eBone->tail[1]-eBone->head[1];	
			delta[2]=eBone->tail[2]-eBone->head[2];
			
			length = sqrt (delta[0]*delta[0] + delta[1]*delta[1] +delta[2]*delta[2]);

			make_boneMatrixvr(bmat, delta, eBone->roll);
			glMultMatrixf (bmat);
			draw_bone (arm->flag, eBone->flag, index, eBone->name, length);

			glPopMatrix();
			if (eBone->parent){
				glLoadName (-1);
				setlinestyle(3);
				
				glBegin(GL_LINES);
				glVertex3fv(eBone->parent->tail);
				glVertex3fv(eBone->head);
				glEnd();

				setlinestyle(0);
			}
		};
		arm->flag &= ~ARM_EDITMODE;
		cpack (B_YELLOW);
		
	}
	else{
		/*	Draw hierarchical bone list (non-edit data) */

		/* Ensure we're using the mose recent pose */
		if (!ob->pose)
			ob->pose=MEM_callocN (sizeof(bPose), "pose");

#if 1	/* Activate if there are problems with action lag */
		apply_pose_armature(arm, ob->pose, 0);
		where_is_armature (ob);
#endif

		if (G.obpose == ob){
			arm->flag |= ARM_POSEMODE;
#if 0	/* Depreciated interactive ik goal drawing */
			if (arm->chainbase.first){
				glPushMatrix();
				glTranslatef(((PoseChain*)arm->chainbase.first)->goal[0],
					((PoseChain*)arm->chainbase.first)->goal[1],
					((PoseChain*)arm->chainbase.first)->goal[2]);
				drawaxes(1.0);
				glPopMatrix();
			}
#endif
		}
		index = 0;
		for (bone=arm->bonebase.first; bone; bone=bone->next) {
			glPushMatrix();
			draw_bonechildren(bone, arm->flag, &index);
			glPopMatrix();
			if (arm->flag & ARM_POSEMODE)
				cpack (B_CYAN);
		}

		arm->flag &= ~ARM_POSEMODE; 
	}

	if (arm->flag & ARM_DRAWXRAY) {
		if(G.zbuf) glEnable(GL_DEPTH_TEST);
	}
}

static void draw_boneverti (float x, float y, float z, float size, int flag)
{
	GLUquadricObj	*qobj;

	size*=0.05;

/*
		Ultimately dots should be drawn in screenspace
		For now we'll just draw them any old way.
*/
	glPushMatrix();

	glTranslatef(x, y, z);

	qobj	= gluNewQuadric(); 
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	gluPartialDisk( qobj, 0,  1.0F*size, 32, 1, 360.0, 360.0);

	glRotatef (90, 0, 1, 0);
	gluPartialDisk( qobj, 0,  1.0F*size, 32, 1, 360.0, 360.0);

	glRotatef (90, 1, 0, 0);
	gluPartialDisk( qobj, 0,  1.0F*size, 32, 1, 360.0, 360.0);

	gluDeleteQuadric(qobj);  

	glPopMatrix();
}

static void draw_bonechildren (Bone *bone, int flag, unsigned int *index)
{
	Bone *cbone;
	float	delta[3];
	float	length;
	float M_objectspacemat[4][4];

	if (bone==NULL) return;
	
	if (flag & ARM_POSEMODE){
		if (bone->flag & BONE_SELECTED)
			cpack (B_CYAN);
		else
			cpack (B_AQUA);
	}
	
	//	Draw a line from our root to the parent's tip
	if (bone->parent && !(bone->flag & (BONE_IK_TOPARENT|BONE_HIDDEN)) ){
		float childMat[4][4];
		float boneMat[4][4];
		float tip[3], root[3];
		get_objectspace_bone_matrix(bone->parent, boneMat, 0, 1);
		get_objectspace_bone_matrix(bone, childMat, 1, 1);
		
		VECCOPY (tip, boneMat[3]);
		VECCOPY	(root, childMat[3]);
		
		if (flag & ARM_POSEMODE)
			glLoadName (-1);
		setlinestyle(3);
		glBegin(GL_LINES);
		glVertex3fv(tip);
		glVertex3fv(root);
		glEnd();
		setlinestyle(0);
	}
	
	
	/* Draw this bone in objectspace */
	delta[0]=bone->tail[0]-bone->head[0];
	delta[1]=bone->tail[1]-bone->head[1];
	delta[2]=bone->tail[2]-bone->head[2];
	length = sqrt (delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2] );
	
	/* Incorporates offset, rest rotation, user rotation and parent coordinates*/
	get_objectspace_bone_matrix(bone, M_objectspacemat, 1, 1);
	
	if (!(bone->flag & BONE_HIDDEN)){
		glPushMatrix();
		glMultMatrixf(M_objectspacemat);
		
		
		if (flag & ARM_POSEMODE)
			draw_bone(flag, (bone->parent && bone->parent->flag & BONE_HIDDEN) ? (bone->flag & ~BONE_IK_TOPARENT) : (bone->flag), *index, bone->name, length);
		else
			draw_bone(flag, (bone->parent && bone->parent->flag & BONE_HIDDEN) ? (bone->flag & ~BONE_IK_TOPARENT) : (bone->flag), -1, bone->name, length);
		glPopMatrix();
	}
	(*index)++;

	/* Draw the children */
	for (cbone= bone->childbase.first; cbone; cbone=cbone->next){
		draw_bonechildren (cbone, flag, index);
	}
}

static void draw_bone (int armflag, int boneflag, unsigned int id, char *name, float length)
{
	float vec[2];
	float	bulge;
	float	pointsize;
	/* 
	FIXME: This routine should probably draw the bones in screenspace using
	the projected start & end points of the transformed bone 
	*/
	
	pointsize = length;
	if (pointsize<0.1)
		pointsize=0.1;

	/*	Draw a 3d octahedral bone	*/
	
	bulge=length*0.1;

	if (id!=-1)
		glLoadName((GLuint) id );

	glPushMatrix();

	/*	Draw root point if we have no IK parent */
	if (!(boneflag & BONE_IK_TOPARENT)){
		if (id != -1)
			glLoadName (id | BONESEL_ROOT);
		if (armflag & ARM_EDITMODE){
			if (boneflag & BONE_ROOTSEL)
				cpack (B_YELLOW);
			else
				cpack (B_PURPLE);
		}
		draw_boneverti (0, 0, 0, pointsize, 0);
	}

	/*	Draw tip point (for selection only )*/

	if (id != -1)
		glLoadName (id | BONESEL_TIP);
	
	if (armflag & ARM_EDITMODE){
		if (boneflag & BONE_TIPSEL)
		 	cpack (B_YELLOW);
		else
			cpack (B_PURPLE);
	}
	
	draw_boneverti (0, length, 0, pointsize, 0);

	if (id != -1){
		if (armflag & ARM_POSEMODE)
			glLoadName((GLuint) id);
		else{
#if 1	/* Bones not selectable in editmode */	
			glLoadName((GLuint) -1);
#else
			glLoadName ((GLuint) id|BONESEL_TIP|BONESEL_ROOT);
#endif
		}
	}
	
	
	if (armflag & ARM_EDITMODE){
		if (boneflag & BONE_SELECTED)
			cpack (B_YELLOW);
		else
			cpack (B_PURPLE);
	}
	
	/*	Draw additional axes */
	if (armflag & ARM_DRAWAXES){
		drawaxes(length*0.25F);
	}
	
	/*	Section 1*/
	glBegin(GL_LINE_STRIP);
	vec[0]= vec[1]= 0;
	glVertex2fv(vec);
	
	vec[0]= -bulge; vec[1]= bulge;
	glVertex2fv(vec);
	
	vec[0]= 0; vec[1]= length;
	glVertex2fv(vec);
	
	vec[0]= bulge; vec[1]= bulge;
	glVertex2fv(vec);
	
	vec[0]= 0; vec[1]= 0;
	glVertex2fv(vec);
	glEnd();
	
	/*	Section 2*/
	glRotatef(90,0,1,0);
	glBegin(GL_LINE_STRIP);
	vec[0]= vec[1]= 0;
	glVertex2fv(vec);
	
	vec[0]= -bulge; vec[1]= bulge;
	glVertex2fv(vec);
	
	vec[0]= 0; vec[1]= length;
	glVertex2fv(vec);
	
	vec[0]= bulge; vec[1]= bulge;
	glVertex2fv(vec);
	
	vec[0]= 0; vec[1]= 0;
	glVertex2fv(vec);
	glEnd();
	
	/*	Square*/
	glTranslatef (0,bulge,0);
	glRotatef(45,0,1,0);
	glRotatef(90,1,0,0);
	glBegin(GL_LINE_STRIP);
	
	vec[0]= -bulge*.707;vec[1]=-bulge*.707;
	glVertex2fv(vec);
	
	vec[0]= bulge*.707;vec[1]= -bulge*.707;
	glVertex2fv(vec);
	
	vec[0]= bulge*.707;vec[1]= bulge*.707;
	glVertex2fv(vec);
	
	vec[0]= -bulge*.707;vec[1]= bulge*.707;
	glVertex2fv(vec);
	
	vec[0]= vec[1]= -bulge*.707;
	glVertex2fv(vec);
	glEnd();
	

	glPopMatrix();

	/*	Draw the bone name */
	if (armflag & ARM_DRAWNAMES){
		glRasterPos3f(0,  length/2.0,  0);
		BMF_DrawString(G.font, " ");
		BMF_DrawString(G.font, name);
	}
}



void add_primitiveArmature(int type)
{
	if(G.scene->id.lib) return;
	
	/* this function also comes from an info window */
	if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_INFO); else return;
	if(G.vd==NULL) return;
	
	check_editmode(OB_ARMATURE);
	
	/* If we're not the "obedit", make a new object and enter editmode */
	if(G.obedit==NULL) {
		add_object(OB_ARMATURE);
		base_init_from_view3d(BASACT, G.vd);
		G.obedit= BASACT->object;
		
		where_is_object(G.obedit);
		
		make_editArmature();
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
	}
	
	switch (type){
	default:
		add_bone_input (G.obedit);	
		break;
	};
	
	countall();

	allqueue(REDRAWALL, 0);
}

static void add_bone_input (Object *ob)
/*
	FUNCTION: 
	Creates a bone chain under user input.
	As the user clicks to accept each bone,
	a new bone is started as the child and IK
	child of the previous bone.  Pressing ESC
	cancels the current bone and leaves bone
	adding mode.

*/
{
	float		*cursLoc, cent[3], dx, dy;
	float		mat[3][3], curs[3],	cmat[3][3], imat[3][3], rmat[4][4], itmat[4][4];
	short		xo, yo, mval[2], afbreek=0;
	short		val;
	float		restmat[4][4], tempVec[4];
	EditBone	*bone;
	EditBone	*parent;
	unsigned short		event;
	float		angle, scale;
	float		length, lengths;
	float	newEnd[4];
	int			addbones=1;
	float		dvec[3], dvecp[3];
	
	cursLoc= give_cursor();
	
	VECCOPY (curs,cursLoc);
	
	while (addbones>0){
		afbreek=0;
		/*	Create an inverse matrix */
		Mat3CpyMat4(mat, G.obedit->obmat);
		VECCOPY(cent, curs);
		cent[0]-= G.obedit->obmat[3][0];
		cent[1]-= G.obedit->obmat[3][1];
		cent[2]-= G.obedit->obmat[3][2];
		
		Mat3CpyMat4(imat, G.vd->viewmat);
		Mat3MulVecfl(imat, cent);
		
		Mat3MulMat3(cmat, imat, mat);
		Mat3Inv(imat,cmat);
		
		/*	Create a temporary bone	*/
		bone= MEM_callocN(sizeof(EditBone), "eBone");
		
		/*	If we're the child of something, set that up now */
		if (addbones>1){
			parent=G.edbo.last;
			bone->parent=parent;
			bone->flag|=BONE_IK_TOPARENT;
		}
		
		strcpy (bone->name,"Bone");
		unique_editbone_name (bone->name);
	
		BLI_addtail(&G.edbo, bone);
	
		bone->flag |= BONE_QUATROT;
		bone->flag |= (BONE_SELECTED);
		deselectall_armature();
		bone->flag |= BONE_SELECTED|BONE_HILIGHTED|BONE_TIPSEL|BONE_ROOTSEL;
		
		bone->weight= 1.0F;
		bone->dist= 1.0F;
		bone->boneclass = BONE_SKINNABLE;

		/*	Project cursor center to screenspace. */
		getmouseco_areawin(mval);
		xo= mval[0];
		yo= mval[1];
		window_to_3d(dvecp, xo, yo);

		while (1) {

			getmouseco_areawin(mval);
			window_to_3d(dvec, mval[0], mval[1]);

			scale=1000;

			dx=	((float)mval[0]-(float)xo)*scale;
			dy= ((float)mval[1]-(float)yo)*scale;
	
			/*		Calc bone length*/
			lengths= sqrt((dx*dx)+(dy*dy));
			length = sqrt(((dvec[0]-dvecp[0])*(dvec[0]-dvecp[0]))+((dvec[1]-dvecp[1])*(dvec[1]-dvecp[1]))+((dvec[2]-dvecp[2])*(dvec[2]-dvecp[2])));

			/*		Find rotation around screen normal */
			if (lengths>0.0F) {
				angle= acos(dy/lengths);
				if (dx<0.0F)
					angle*= -1.0F;
			}
			else angle= 0.0F;
			
			/*		FIXME:	Is there a blender-defined way of making rot and trans matrices? */
			rmat[0][0]= cos (angle);
			rmat[0][1]= -sin (angle);
			rmat[0][2]= 0.0F;
			rmat[0][3]= 0.0F;
			rmat[1][0]= sin (angle);
			rmat[1][1]= cos (angle);
			rmat[1][2]= 0.0F;
			rmat[1][3]= 0.0F;
			rmat[2][0]= 0.0F;
			rmat[2][1]= 0.0F;
			rmat[2][2]= 1.0F;
			rmat[2][3]= 0.0F;
			rmat[3][0]= cent[0];
			rmat[3][1]= cent[1];
			rmat[3][2]= cent[2];
			rmat[3][3]= 1.0F;
			
			/*		Rotate object's inversemat by the bone's rotation
			to get the coordinate space of the bone */
			Mat4CpyMat3	(itmat, imat);
			Mat4MulMat4 (restmat, rmat, itmat);
			
			/*	Find the bone head */
			tempVec[0]=0; tempVec[1]=0.0F; tempVec[2]=0.0F; tempVec[3]=1.0F;
			Mat4MulVec4fl (restmat, tempVec);
			VECCOPY (bone->head, tempVec);
			
			/*	Find the bone tail */
			tempVec[0]=0; tempVec[1]=length; tempVec[2]=0.0F; tempVec[3]=1.0F;
			Mat4MulVec4fl (restmat, tempVec);
			VECCOPY (bone->tail, tempVec);
			
			/*	IF we're a child of something, add the parents' translates	*/
			
			/*	Offset of child is new cursor*/

			VECCOPY (newEnd,bone->tail); newEnd[3]=1;

			/*	Set the bone's transformations	*/
			Mat4One (bone->obmat);
			bone->size[0]=bone->size[1]=bone->size[2]=1.0F;

			force_draw();
			while(qtest()) {
				event= extern_qread(&val);
				if(val) {
					switch(event) {
					case ESCKEY:
					case RIGHTMOUSE:
						BLI_freelinkN (&G.edbo,bone);
						afbreek=1;
						addbones=0;
						break;
					case LEFTMOUSE:
					case MIDDLEMOUSE:
					case SPACEKEY:
					case RETKEY:
						afbreek= 1;
											
						Mat4MulVec4fl (G.obedit->obmat,newEnd);
						
						curs[0]=newEnd[0];
						curs[1]=newEnd[1];
						curs[2]=newEnd[2];
						addbones++;
						break;
					}	/*	End of case*/
				}	/*	End of if (val)*/
				if(afbreek) break;
			}	/*	Endd of Qtest loop	*/

		if(afbreek) break;
		}/*	End of positioning loop (while)*/
	}	/*	End of bone adding loop*/
	
	countall();

}

static void validate_editbonebutton_cb(void *bonev, void *arg2_unused)
{
	EditBone *curBone= bonev;
	validate_editbonebutton(curBone);
}
static void parnr_to_editbone_cb(void *bonev, void *arg2_unused)
{
	EditBone *curBone= bonev;
	parnr_to_editbone(curBone);
}
static void attach_bone_to_parent_cb(void *bonev, void *arg2_unused)
{
	EditBone *curBone= bonev;
	attach_bone_to_parent(curBone);
}

void armaturebuts(void)
{
	bArmature	*arm=NULL;
	Object		*ob=NULL;
	uiBlock		*block=NULL;
	char		str[64];
	int			bx=148, by=100;
	EditBone	*curBone;
	uiBut		*but;
	char		*boneString=NULL;
	int			index;

	ob= OBACT;
	if (ob==NULL) return;
	
	sprintf(str, "editbuttonswin %d", curarea->win);
	block= uiNewBlock (&curarea->uiblocks, str, UI_EMBOSSX, UI_HELV, curarea->win);
	
	arm= ob->data;
	if (arm==NULL) return;
	
	uiBlockSetCol(block, BUTGREEN);
	uiDefButI(block, TOG|BIT|ARM_RESTPOSBIT,REDRAWVIEW3D, "Rest Pos", bx,by,97,20, &arm->flag, 0, 0, 0, 0, "Disable all animation for this object");
	uiDefButI(block, TOG|BIT|ARM_DRAWAXESBIT,REDRAWVIEW3D, "Draw Axes", bx,by-46,97,20, &arm->flag, 0, 0, 0, 0, "Draw bone axes");
	uiDefButI(block, TOG|BIT|ARM_DRAWNAMESBIT,REDRAWVIEW3D, "Draw Names", bx,by-69,97,20, &arm->flag, 0, 0, 0, 0, "Draw bone names");
	uiDefButI(block, TOG|BIT|ARM_DRAWXRAYBIT,REDRAWVIEW3D, "X-Ray", bx,by-92,97,20, &arm->flag, 0, 0, 0, 0, "Draw armature in front of shaded objects");

	uiBlockSetCol(block, BUTGREY);
	
	/* Draw the bone name block */
	
	bx+=400; by=200;
	
	if (G.obedit==ob){
		uiDefBut(block, LABEL, 0, "Selected Bones",						bx,by,128,18, 0, 0, 0, 0, 0, "");
		by-=20;
		for (curBone=G.edbo.first, index=0; curBone; curBone=curBone->next, index++){
			if (curBone->flag & (BONE_SELECTED)){

				/* Hide in posemode flag */
				uiBlockSetCol(block, BUTGREEN);
				uiDefButI(block, TOG|BIT|BONE_HIDDENBIT, REDRAWVIEW3D, "Hide", bx-50,by,48,18, &curBone->flag, 0, 0, 0, 0, "Toggles display of this bone in posemode");
				
				/*	Bone naming button */
				uiBlockSetCol(block, BUTGREY);
				strcpy (curBone->oldname, curBone->name);
				but=uiDefBut(block, TEX, REDRAWVIEW3D, "BO:", bx,by,97,18, &curBone->name, 0, 24, 0, 0, "Change the bone name");
				uiButSetFunc(but, validate_editbonebutton_cb, curBone, NULL);
				
				uiDefBut(block, LABEL, 0, "child of", bx+106,by,100,18, NULL, 0.0, 0.0, 0.0, 0.0, "");

				boneString = malloc((BLI_countlist(&G.edbo) * 64)+64);
				build_bonestring (boneString, curBone);
				
				curBone->parNr = editbone_to_parnr(curBone->parent);
				but = uiDefButI(block, MENU,REDRAWVIEW3D, boneString, bx+164,by,97,18, &curBone->parNr, 0.0, 0.0, 0.0, 0.0, "Parent");
				uiButSetFunc(but, parnr_to_editbone_cb, curBone, NULL);

				free(boneString);

				/* IK to parent flag */
				if (curBone->parent){
					uiBlockSetCol(block, BUTGREEN);
					but=uiDefButI(block, TOG|BIT|BONE_IK_TOPARENTBIT, REDRAWVIEW3D, "IK", bx+275,by,32,18, &curBone->flag, 0.0, 0.0, 0.0, 0.0, "IK link to parent");
					uiButSetFunc(but, attach_bone_to_parent_cb, curBone, NULL);
				}

				/* Dist and weight buttons */
				uiBlockSetCol(block, BUTGREY);
				but=uiDefButI(block, MENU, REDRAWVIEW3D,
							  "Skinnable %x0|"
							  "Unskinnable %x1|"
							  "Head %x2|"
							  "Neck %x3|"
							  "Back %x4|"
							  "Shoulder %x5|"
							  "Arm %x6|"
							  "Hand %x7|"
							  "Finger %x8|"
							  "Thumb %x9|"
							  "Pelvis %x10|"
							  "Leg %x11|"
							  "Foot %x12|"
							  "Toe %x13|"
							  "Tentacle %x14",
							  bx+320,by,97,18,
							  &curBone->boneclass,
							  0.0, 0.0, 0.0, 0.0, 
							  "Classification of armature element");
				
				/* Dist and weight buttons */
				uiBlockSetCol(block, BUTGREY);
				uiDefButF(block, NUM,REDRAWVIEW3D, "Dist:", bx+425, by, 
						  110, 18, &curBone->dist, 0.0, 1000.0, 10.0, 0.0, 
						  "Bone deformation distance");
				uiDefButF(block, NUM,REDRAWVIEW3D, "Weight:", bx+543, by, 
						  110, 18, &curBone->weight, 0.0F, 1000.0F, 
						  10.0F, 0.0F, "Bone deformation weight");
				
				by-=19;	
			}
		}
	}

	uiDrawBlock (block);

}

static int editbone_to_parnr (EditBone *bone)
{
	EditBone *ebone;
	int	index;

	for (ebone=G.edbo.first, index=0; ebone; ebone=ebone->next, index++){
		if (ebone==bone)
			return index;
	}

	return -1;
}

static void parnr_to_editbone(EditBone *bone)
{
	if (bone->parNr == -1){
		bone->parent = NULL;
		bone->flag &= ~BONE_IK_TOPARENT;
	}
	else{
		bone->parent = BLI_findlink(&G.edbo, bone->parNr);
		attach_bone_to_parent(bone);
	}
}

static void attach_bone_to_parent(EditBone *bone)
{
	EditBone *curbone;

	if (bone->flag & BONE_IK_TOPARENT) {

	/* See if there are any other bones that refer to the same parent and disconnect them */
		for (curbone = G.edbo.first; curbone; curbone=curbone->next){
			if (curbone!=bone){
				if (curbone->parent && (curbone->parent == bone->parent) && (curbone->flag & BONE_IK_TOPARENT))
					curbone->flag &= ~BONE_IK_TOPARENT;
			}
		}

	/* Attach this bone to its parent */
		VECCOPY(bone->head, bone->parent->tail);
	}

}

static void build_bonestring (char *string, EditBone *bone){
	EditBone *curBone;
	EditBone *pBone;
	int		skip=0;
	int		index;

	sprintf (string, "Parent%%t| %%x%d", -1);	/* That space is there for a reason */
	
	for (curBone = G.edbo.first, index=0; curBone; curBone=curBone->next, index++){
		/* Make sure this is a valid child */
		if (curBone != bone){
			skip=0;
			for (pBone=curBone->parent; pBone; pBone=pBone->parent){
				if (pBone==bone){
					skip=1;
					break;
				}
			}
			
			if (skip)
				continue;
			
			sprintf (string, "%s|%s%%x%d", string, curBone->name, index);
		}
	}
}

static void validate_editbonebutton(EditBone *eBone){
	EditBone	*prev;
	bAction		*act;
	bActionChannel *chan;
	Base *base;

	/* Separate the bone from the G.edbo */
	prev=eBone->prev;
	BLI_remlink (&G.edbo, eBone);

	/*	Validate the name */
	unique_editbone_name (eBone->name);

	/* Re-insert the bone */
	if (prev)
		BLI_insertlink(&G.edbo, prev, eBone);
	else
		BLI_addhead (&G.edbo, eBone);

	/* Rename channel if necessary */
	if (G.obedit)
		act = G.obedit->action;

	if (act && !act->id.lib){
		//	Find the appropriate channel
		for (chan = act->chanbase.first; chan; chan=chan->next){
			if (!strcmp (chan->name, eBone->oldname)){
				strcpy (chan->name, eBone->name);
			}
		}
		allqueue(REDRAWACTION, 0);
	}

	/* Update the parenting info of any users */
	/*	Yes, I know this is the worst thing you have ever seen. */

	for (base = G.scene->base.first; base; base=base->next){
		Object *ob = base->object;

		/* See if an object is parented to this armature */
		if (ob->parent && ob->partype==PARBONE && (ob->parent->type==OB_ARMATURE) && (ob->parent->data == G.obedit->data)){
			if (!strcmp(ob->parsubstr, eBone->oldname))
				strcpy(ob->parsubstr, eBone->name);
		}
	}

	exit_editmode(0);	/* To ensure new names make it to the edit armature */

}

void deselectall_armature(void)
/*	Actually, it toggles selection, deselecting
	everything if anything is selected */
{
	EditBone	*eBone;
	int			sel=1;
	
	
	/*	Determine if there are any selected bones
		And therefore whether we are selecting or deselecting */
	for (eBone=G.edbo.first;eBone;eBone=eBone->next){
		if (eBone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)){
			sel=0;
			break;
		};
	};
	
	/*	Set the flags */
	for (eBone=G.edbo.first;eBone;eBone=eBone->next){
		if (sel)
			eBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
		else
			eBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
	};
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSHEAD, 0);
	allqueue(REDRAWBUTSCONSTRAINT, 0);
	countall();
}

void join_armature(void)
{
	
	Object	*ob;
	Base	*base, *nextbase;
	ListBase eblist;
	EditBone *curbone, *next;
	float	mat[4][4], imat[4][4];
	
	/*	Ensure we're not in editmode and that the active object is an armature*/
	if(G.obedit) return;
	
	ob= OBACT;
	if(ob->type!=OB_ARMATURE) return;
	
	/*	Make sure the user wants to continue*/
	if(okee("Join selected Armatures")==0) return;
	
	/*	Put the active armature into editmode and join the bones from the other one*/
#if 1
	enter_editmode();
#else
	baselist.first=baselist.last=0;
	make_boneList(&baselist, &((bArmature*)ob->data)->bonebase, NULL);
#endif
	
	for (base=FIRSTBASE; base; base=nextbase) {
		nextbase = base->next;
		if (TESTBASE(base)){
			if ((base->object->type==OB_ARMATURE) && (base->object!=ob)){
				/* Make a list of editbones */
				eblist.first=eblist.last=0;
				make_boneList (&eblist, &((bArmature*)base->object->data)->bonebase,NULL);
				/* Find the difference matrix */
				Mat4Invert(imat, ob->obmat);
				Mat4MulMat4(mat, base->object->obmat, imat);
				
				/* Copy bones from the object to the edit armature */
				for (curbone=eblist.first; curbone; curbone=next){
					next = curbone->next;
					
					/* Blank out tranformation data */
					curbone->loc[0]=curbone->loc[1]=curbone->loc[2]=0.0F;
					curbone->size[0]=curbone->size[1]=curbone->size[2]=1.0F;
					curbone->quat[0]=curbone->quat[1]=curbone->quat[2]=curbone->quat[3]=0.0F;
					
					unique_editbone_name (curbone->name);
					
					/* Transform the bone */
					{
						float premat[4][4];
						float postmat[4][4];
						float difmat[4][4];
						float imat[4][4];
						float temp[4][4];
						float delta[3];

						/* Get the premat */
						VecSubf (delta, curbone->tail, curbone->head);
						make_boneMatrixvr(temp, delta, curbone->roll);
						Mat4MulMat4 (premat, temp, mat);

						Mat4MulVecfl(mat, curbone->head);
						Mat4MulVecfl(mat, curbone->tail);

						/* Get the postmat */
						VecSubf (delta, curbone->tail, curbone->head);
						make_boneMatrixvr(postmat, delta, curbone->roll);
						
						/* Find the roll */
						Mat4Invert (imat, premat);
						Mat4MulMat4 (difmat, postmat, imat);
						
						curbone->roll -=atan(difmat[2][0]/difmat[2][2]);
						
						if (difmat[0][0]<0)
							curbone->roll +=M_PI;

					}
#if 1
					BLI_remlink(&eblist, curbone);
					BLI_addtail(&G.edbo, curbone);
#else
					BLI_remlink(&eblist, curbone);
					BLI_addtail(&baselist, curbone);
#endif
				}
				
				free_and_unlink_base(base);
			}
		}
	}
	
#if 1
	exit_editmode(1);
#else
	editbones_to_armature(&baselist, ob);
	if (baselist.first){
		BLI_freelistN (&baselist);
	}
#endif
	allqueue(REDRAWVIEW3D, 0);

}


static int	editbone_name_exists (char *name){
	EditBone	*eBone;
	
	for (eBone=G.edbo.first; eBone; eBone=eBone->next){
		if (!strcmp (name, eBone->name))
			return 1;
	}
	
	return 0;
		
}

static void unique_editbone_name (char *name){
	char		tempname[64];
	int			number;
	char		*dot;
	
	
	if (editbone_name_exists(name)){
		/*	Strip off the suffix */
		dot=strchr(name, '.');
		if (dot)
			*dot=0;
		
		for (number = 1; number <=999; number++){
			sprintf (tempname, "%s.%03d", name, number);
			if (!editbone_name_exists(tempname)){
				strcpy (name, tempname);
				return;
			}
		}
	}
}

void extrude_armature(void)
{
	EditBone *newbone, *curbone, *first=NULL, *partest;
	
	TEST_EDITARMATURE;
	
	
	if(okee("Extrude Bone Segments")==0) return;
	
	/* Duplicate the necessary bones */
	for (curbone = G.edbo.first; ((curbone) && (curbone!=first)); curbone=curbone->next){
		if (curbone->flag & (BONE_TIPSEL|BONE_SELECTED)){
			newbone = MEM_callocN(sizeof(EditBone), "extrudebone");
			
			
			VECCOPY (newbone->head, curbone->tail);
			VECCOPY (newbone->tail, newbone->head);
			newbone->parent = curbone;
			newbone->flag = BONE_TIPSEL;
			newbone->flag |= BONE_QUATROT;
			newbone->weight= curbone->weight;
			newbone->dist= curbone->dist;
			newbone->boneclass= curbone->boneclass;

			Mat4One(newbone->obmat);
			
			/* See if there are any ik children of the parent */
			for (partest = G.edbo.first; partest; partest=partest->next){
				if ((partest->parent == curbone) && (partest->flag & BONE_IK_TOPARENT))
					break;
			}
			
			if (!partest)
				newbone->flag |= BONE_IK_TOPARENT;
			
			strcpy (newbone->name, curbone->name);
			unique_editbone_name(newbone->name);
			
			/* Add the new bone to the list */
			BLI_addtail(&G.edbo, newbone);
			if (!first)
				first = newbone;
		}
		
		/* Deselect the old bone */
		curbone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
		
	}
	
	/* Transform the endpoints */
	countall();
	transform('g');		
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSCONSTRAINT, 0);
}

void addvert_armature(void)
{
/*	
	I haven't decided if it will be possible to add bones in this way.
	For the moment, we'll use Extrude, or explicit parenting.
	*/
}





void adduplicate_armature(void)
{
	EditBone	*eBone = NULL;
	EditBone	*curBone;
	EditBone	*firstDup=NULL;	/*	The beginning of the duplicated bones in the edbo list */

	countall();

	/*	Find the selected bones and duplicate them as needed */
	for (curBone=G.edbo.first; curBone && curBone!=firstDup; curBone=curBone->next){
		if (curBone->flag & BONE_SELECTED){

			eBone=MEM_callocN(sizeof(EditBone), "addup_editbone");
			eBone->flag |= BONE_SELECTED;

			/*	Copy data from old bone to new bone */
			memcpy (eBone, curBone, sizeof(EditBone));

			/* Blank out tranformation data */
			eBone->loc[0]=eBone->loc[1]=eBone->loc[2]=0.0F;
			eBone->size[0]=eBone->size[1]=eBone->size[2]=1.0F;
			eBone->quat[0]=eBone->quat[1]=eBone->quat[2]=eBone->quat[3]=0.0F;

			curBone->temp = eBone;
			eBone->temp = curBone;

			unique_editbone_name (eBone->name);
			BLI_addtail (&G.edbo, eBone);
			if (!firstDup)
				firstDup=eBone;
		}
	}

	if (eBone){
		/*	Fix the head and tail */	
		if (eBone->parent && !eBone->parent->flag & BONE_SELECTED){
			VecSubf (eBone->tail, eBone->tail, eBone->head);
			VecSubf (eBone->head, eBone->head, eBone->head);
		}
	}

	/*	Run though the list and fix the pointers */
	for (curBone=G.edbo.first; curBone && curBone!=firstDup; curBone=curBone->next){

		if (curBone->flag & BONE_SELECTED){
			eBone=(EditBone*) curBone->temp;

			/*	If this bone has no parent,
					Set the duplicate->parent to NULL
			*/
			if (!curBone->parent){
				eBone->parent = NULL;
			}
			/*	If this bone has a parent that IS selected,
					Set the duplicate->parent to the curBone->parent->duplicate
			*/
			 else if (curBone->parent->flag & BONE_SELECTED){
				eBone->parent=(EditBone*) curBone->parent->temp;
			}
			/*	If this bone has a parent that IS not selected,
					Set the duplicate->parent to the curBone->parent
			*/
			 else {
				eBone->parent=(EditBone*) curBone->parent; 
				eBone->flag &= ~BONE_IK_TOPARENT;
			}
			 
		}
	} 
	
	/*	Deselect the old bones and select the new ones */

	for (curBone=G.edbo.first; curBone && curBone!=firstDup; curBone=curBone->next){
		curBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
	}


	transform('g');
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSCONSTRAINT, 0);
}

/*
 *
 *	POSING FUNCTIONS: Maybe move these to a separate file at some point
 *
 *
 */


void clear_armature(Object *ob, char mode){
	Bone	*curBone;
	bArmature	*arm;

	arm=get_armature(ob);
	
	if (!arm)
		return;

	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		clear_armature_children (curBone, ob->pose, mode);
	}

	where_is_armature (ob);

}

static void clear_armature_children (Bone *bone, bPose *pose, char mode){
	Bone			*curBone;
	bPoseChannel	*chan;
	if (!bone)
		return;
	
	verify_pose_channel (pose, bone->name);
	chan=get_pose_channel (pose, bone->name);

	if (!chan)
		return;

	if (bone->flag & BONE_SELECTED){
		switch (mode){
		case 'r':
			chan->quat[1]=chan->quat[2]=chan->quat[3]=0.0F; chan->quat[0]=1.0F;
			break;
		case 'g':
			chan->loc[0]=chan->loc[1]=chan->loc[2]=0.0F;
			break;
		case 's':
			chan->size[0]=chan->size[1]=chan->size[2]=1.0F;
			break;
			
		}
	}

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		clear_armature_children (curBone, pose, mode);
	}

}

void mousepose_armature(void)
/*
	Handles right-clicking for selection
	of bones in armature pose modes.
*/
{
	Bone		*nearBone;

	if (!G.obpose)
		return;

	nearBone = get_nearest_bone(1);

	if (nearBone){
		if (!(G.qual & LR_SHIFTKEY)){
			deselectall_posearmature(0);
			nearBone->flag|=BONE_SELECTED;
			select_actionchannel_by_name(G.obpose->action, nearBone->name, 1);
		}
		else {
			if (nearBone->flag & BONE_SELECTED){
				nearBone->flag &= ~BONE_SELECTED;
				select_actionchannel_by_name(G.obpose->action, nearBone->name, 0);
			}
			else{
				nearBone->flag |= BONE_SELECTED;
				select_actionchannel_by_name(G.obpose->action, nearBone->name, 1);
			}
		};
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);		/* To force action ipo update */
	allqueue(REDRAWBUTSCONSTRAINT, 0);

//	countall();
	rightmouse_transform();
	
}

void make_trans_bones (char mode)
/*	Used in pose mode	*/
{
	bArmature		*arm;
	Bone			*curBone;
	int				count=0;

	transmain=NULL;

	arm=get_armature (G.obpose);
	if (!arm)
		return;

	if (arm->flag & ARM_RESTPOS){
		notice ("Transformation not possible while Rest Position is enabled");
		return;
	}


	if (!(G.obpose->lay & G.vd->lay))
		return;


	centroid[0]=centroid[1]=centroid[2]=0;

	apply_pose_armature(arm, G.obpose->pose, 0);
	where_is_armature (G.obpose);

	/*	Allocate memory for the transformation record */
	tottrans= count_bones (arm, BONE_SELECTED, 0);

	if (!tottrans)
		return;

	transmain= MEM_callocN(tottrans*sizeof(TransOb), "bonetransmain");

	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		count = add_trans_bonechildren (G.obpose, curBone, transmain, count, mode);
	}
	
	tottrans=count;
	
	if (tottrans){
		centroid[0]/=tottrans;
		centroid[1]/=tottrans;
		centroid[2]/=tottrans;
		Mat4MulVecfl (G.obpose->obmat, centroid);
	}
	else{
		MEM_freeN (transmain);
	}
	return;

}

static int	count_bones (bArmature *arm, int flagmask, int allbones)
{
	int	count=0;
	Bone	*curBone;

	if (!arm)
		return 0;

	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		count = count_bonechildren (curBone, count, flagmask, allbones);
	}

	return count;

}

static int count_bonechildren (Bone *bone, int incount, int flagmask, int allbones){

	Bone	*curBone;

	if (!bone)
		return incount;

	if (bone->flag & flagmask || flagmask == 0xFFFFFFFF){
		incount++;
		if (!allbones)
			return incount;
	}

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		incount=count_bonechildren (curBone, incount, flagmask, allbones);
	}

	return incount;
}


static int add_trans_bonechildren (Object* ob, Bone* bone, TransOb* buffer, int index, char mode)
{
	Bone	*curBone;
	TransOb	*curOb;
	float	parmat[4][4], tempmat[4][4];
	float tempobmat[4][4];
	float vec[3];
	if (!bone)
		return index;

	
	
	/* We don't let IK children get "grabbed" */
	if (bone->flag & BONE_SELECTED){
		if (!((mode=='g' || mode=='G') && (bone->flag & BONE_IK_TOPARENT))){
			
			get_bone_root_pos (bone, vec, 1);
		
			VecAddf (centroid, centroid, vec);
			
			curOb=&buffer[index];
			
			curOb->ob = ob;
			curOb->rot=NULL;

			curOb->quat= bone->quat;
			curOb->size= bone->size;
			curOb->loc = bone->loc;

			curOb->data = bone; //	FIXME: Dangerous
			
			memcpy (curOb->oldquat, bone->quat, sizeof (bone->quat));
			memcpy (curOb->oldsize, bone->size, sizeof (bone->size));
			memcpy (curOb->oldloc, bone->loc, sizeof (bone->loc));

#if 0
			if (bone->parent)
				get_objectspace_bone_matrix(bone->parent, tempmat, 1, 1);
			else
				Mat4One (tempmat);
#else
			/* Get the matrix of this bone minus the usertransform */
			Mat4CpyMat4 (tempobmat, bone->obmat);
			Mat4One (bone->obmat);
			get_objectspace_bone_matrix(bone, tempmat, 1, 1);
			Mat4CpyMat4 (bone->obmat, tempobmat);

			
#endif

#if 1
			Mat4MulMat4 (parmat, tempmat, ob->obmat);	/* Original */

			/* Get world transform */
			get_objectspace_bone_matrix(bone, tempmat, 1, 1);
			if (ob->parent){
				where_is_object(ob->parent);
				Mat4MulSerie (tempobmat, ob->parent->obmat, ob->obmat, tempmat, NULL, NULL, NULL, NULL, NULL);
			}
			else
				Mat4MulSerie (tempobmat, ob->obmat, tempmat, NULL, NULL, NULL, NULL, NULL, NULL);
			Mat3CpyMat4 (curOb->axismat, tempobmat);
			Mat3Ortho(curOb->axismat);

#else
			Mat4MulMat4 (parmat, ob->obmat, tempmat);
#endif
			Mat3CpyMat4 (curOb->parmat, parmat);
			Mat3Inv (curOb->parinv, curOb->parmat);

			Mat3CpyMat4 (curOb->obmat, bone->obmat);
			Mat3Inv (curOb->obinv, curOb->obmat);
			
			index++;
			return index;
		}

	}
	
	/*	Recursively search  */
	for (curBone = bone->childbase.first; curBone; curBone=curBone->next){
		index=add_trans_bonechildren (ob, curBone, buffer, index, mode);
	}

	return index;
}

static void deselect_bonechildren (Bone *bone, int mode)
{
	Bone	*curBone;

	if (!bone)
		return;

	if (mode==0)
		bone->flag &= ~BONE_SELECTED;
	else if (!(bone->flag & BONE_HIDDEN))
		bone->flag |= BONE_SELECTED;

	select_actionchannel_by_name(G.obpose->action, bone->name, mode);

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		deselect_bonechildren(curBone, mode);
	}
}


void deselectall_posearmature (int test){
	int	selectmode	=	0;
	Bone*	curBone;
	
	/*	Determine if we're selecting or deselecting	*/
	if (test){
		if (!count_bones (get_armature(G.obpose), BONE_SELECTED, 0))
			selectmode = 1;
	}
	
	/*	Set the flags accordingly	*/
	for (curBone=get_armature(G.obpose)->bonebase.first; curBone; curBone=curBone->next)
		deselect_bonechildren (curBone, selectmode);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION, 0);

}

void auto_align_armature(void)
/* Sets the roll value of selected bones so that their zaxes point upwards */
{
	EditBone *ebone;
	float	xaxis[3]={1.0, 0.0, 0.0}, yaxis[3], zaxis[3]={0.0, 0.0, 1.0};
	float	targetmat[4][4], imat[4][4];
	float	curmat[4][4], diffmat[4][4];
	float	delta[3];

	for (ebone = G.edbo.first; ebone; ebone=ebone->next){
		if (ebone->flag & BONE_SELECTED){
			/* Find the current bone matrix */
			VecSubf(delta, ebone->tail, ebone->head);
			make_boneMatrixvr (curmat, delta, 0.0);
			
			/* Make new matrix based on y axis & z-up */
			VECCOPY (yaxis, curmat[1]);

			Mat4One(targetmat);
			VECCOPY (targetmat[0], xaxis);
			VECCOPY (targetmat[1], yaxis);
			VECCOPY (targetmat[2], zaxis);
			Mat4Ortho(targetmat);

			/* Find the difference between the two matrices */

			Mat4Invert (imat, targetmat);
			Mat4MulMat4(diffmat, curmat, imat);

			ebone->roll = atan(diffmat[2][0]/diffmat[2][2]);
			
		}
	}
} 

int bone_looper(Object *ob, Bone *bone, void *data,
                        int (*bone_func)(Object *, Bone *, void *)) {

    /* We want to apply the function bone_func to every bone 
     * in an armature -- feed bone_looper the first bone and 
     * a pointer to the bone_func and watch it go!. The int count 
     * can be useful for counting bones with a certain property
     * (e.g. skinnable)
     */
    int count = 0;

    if (bone) {

        /* only do bone_func if the bone is non null
         */
        count += bone_func(ob, bone, data);

        /* try to execute bone_func for the first child
         */
        count += bone_looper(ob, bone->childbase.first, data,
                                    bone_func);

        /* try to execute bone_func for the next bone at this
         * depth of the recursion.
         */
        count += bone_looper(ob, bone->next, data, bone_func);
    }

    return count;
}

int add_defgroup_unique_bone(Object *ob, Bone *bone, void *data) {
    /* This group creates a vertex group to ob that has the
     * same name as bone (provided the bone is skinnable). 
	 * If such a vertex group aleady exist the routine exits.
     */
	if ( bone_skinnable(ob, bone, NULL) ) {
		if (!get_named_vertexgroup(ob,bone->name)) {
			add_defgroup_name(ob, bone->name);
			return 1;
		}
    }
    return 0;
}

int bone_skinnable(Object *ob, Bone *bone, void *data)
{
    /* Bones that are not of boneclass BONE_UNSKINNABLE
     * are regarded to be "skinnable" and are eligible for
     * auto-skinning.
     *
     * This function performs 2 functions:
     *
     *   a) It returns 1 if the bone is skinnable.
     *      If we loop over all bones with this 
     *      function, we can count the number of
     *      skinnable bones.
     *   b) If the pointer data is non null,
     *      it is treated like a handle to a
     *      bone pointer -- the bone pointer
     *      is set to point at this bone, and
     *      the pointer the handle points to
     *      is incremented to point to the
     *      next member of an array of pointers
     *      to bones. This way we can loop using
     *      this function to construct an array of
     *      pointers to bones that point to all
     *      skinnable bones.
     */
    Bone ***hbone;

    if ( bone->boneclass != BONE_UNSKINNABLE ) {
		if (data != NULL) {
			hbone = (Bone ***) data;
            **hbone = bone;
            ++*hbone;
        }
        return 1;
    }
    return 0;
}

int dgroup_skinnable(Object *ob, Bone *bone, void *data) {
    /* Bones that are not of boneclass BONE_UNSKINNABLE
     * are regarded to be "skinnable" and are eligible for
     * auto-skinning.
     *
     * This function performs 2 functions:
     *
     *   a) If the bone is skinnable, it creates 
     *      a vertex group for ob that has
     *      the name of the skinnable bone
     *      (if one doesn't exist already).
     *   b) If the pointer data is non null,
     *      it is treated like a handle to a
     *      bDeformGroup pointer -- the 
     *      bDeformGroup pointer is set to point
     *      to the deform group with the bone's
     *      name, and the pointer the handle 
     *      points to is incremented to point to the
     *      next member of an array of pointers
     *      to bDeformGroups. This way we can loop using
     *      this function to construct an array of
     *      pointers to bDeformGroups, all with names
     *      of skinnable bones.
     */
    bDeformGroup ***hgroup, *defgroup;

    if ( bone->boneclass != BONE_UNSKINNABLE ) {
        if ( !(defgroup = get_named_vertexgroup(ob, bone->name)) ) {
            defgroup = add_defgroup_name(ob, bone->name);
        }

        if (data != NULL) {
            hgroup = (bDeformGroup ***) data;
            **hgroup = defgroup;
            ++*hgroup;
        }
        return 1;
    }
    return 0;
}

void add_verts_to_closest_dgroup(Object *ob, Object *par)
{
    /* This function implements a crude form of 
     * auto-skinning: vertices are assigned to the
     * deformation groups associated with bones based
     * on thier proximity to a bone. Every vert is
     * given a weight of 1.0 to the weight group
     * cooresponding to the bone that it is
     * closest to. The vertex may also be assigned to
     * a deformation group associated to a bone
     * that is within 10% of the mninimum distance
     * between the bone and the nearest vert -- the
     * cooresponding weight will fall-off to zero
     * as the distance approaches the 10% tolerance mark.
	 * If the mesh has subsurf enabled then the verts
	 * on the subsurf limit surface is used to generate 
	 * the weights rather than the verts on the cage
	 * mesh.
     */

    bArmature *arm;
    Bone **bonelist, **bonehandle, *bone;
    bDeformGroup **dgrouplist, **dgrouphandle, *defgroup;
    float *distance, mindist = 0.0, weight = 0.0;
    float   root[3];
    float   tip[3];
    float real_co[3];
	float *subverts = NULL;
    float *subvert;
    Mesh  *mesh;
    MVert *vert;

    int numbones, i, j;

    /* If the parent object is not an armature exit */
    arm = get_armature(par);
    if (!arm)
        return;

    /* count the number of skinnable bones */
    numbones = bone_looper(ob, arm->bonebase.first, NULL,
                                  bone_skinnable);

    /* create an array of pointer to bones that are skinnable
     * and fill it with all of the skinnable bones
     */
    bonelist = MEM_mallocN(numbones*sizeof(Bone *), "bonelist");
    bonehandle = bonelist;
    bone_looper(ob, arm->bonebase.first, &bonehandle,
                       bone_skinnable);

    /* create an array of pointers to the deform groups that
     * coorespond to the skinnable bones (creating them
     * as necessary.
     */
    dgrouplist = MEM_mallocN(numbones*sizeof(bDeformGroup *), "dgrouplist");
    dgrouphandle = dgrouplist;
    bone_looper(ob, arm->bonebase.first, &dgrouphandle,
                       dgroup_skinnable);

    /* create an array of floats that will be used for each vert
     * to hold the distance to each bone.
     */
    distance = MEM_mallocN(numbones*sizeof(float), "distance");

    mesh = (Mesh*)ob->data;

	/* Is subsurf on? Lets use the verts on the limit surface then */
	if ( (mesh->flag&ME_SUBSURF) && (mesh->subdiv > 0) ) {
		subverts = MEM_mallocN(3*mesh->totvert*sizeof(float), "subverts");
		subsurf_calculate_limit_positions(mesh, subverts);
	}

    /* for each vertex in the mesh ...
     */
    for ( i=0 ; i < mesh->totvert ; ++i ) {
        /* get the vert in global coords
         */
		
		if (subverts) {
			subvert = subverts + i*3;
			VECCOPY (real_co, subvert);
		}
		else {
			vert = mesh->mvert + i;
			VECCOPY (real_co, vert->co);
		}
        Mat4MulVecfl(ob->obmat, real_co);


        /* for each skinnable bone ...
         */
        for (j=0; j < numbones; ++j) {
            bone = bonelist[j];

            /* get the root of the bone in global coords
             */
            get_bone_root_pos (bone, root, 0);
            Mat4MulVecfl(par->obmat, root);

            /* get the tip of the bone in global coords
             */
            get_bone_tip_pos (bone, tip, 0);
            Mat4MulVecfl(par->obmat, tip);

            /* store the distance from the bone to
             * the vert
             */
            distance[j] = dist_to_bone(real_co, root, tip);

            /* if this is the first bone, or if this
             * bone is less than mindist, then set this
             * distance to mindist
             */
            if (j == 0) {
                mindist = distance[j];
            }
            else if (distance[j] < mindist) {
                mindist = distance[j];
            }
        }

        /* for each deform group ...
         */
        for (j=0; j < numbones; ++j) {
            defgroup = dgrouplist[j];

            /* if the cooresponding bone is the closest one
             * add the vert to the deform group with weight 1
             */
            if (distance[j] <= mindist) {
                add_vert_to_defgroup (ob, defgroup, i, 1.0, WEIGHT_REPLACE);
            }

            /* if the cooresponding bone is within 10% of the
             * nearest distance, add the vert to the
             * deform group with a weight that declines with
             * distance
             */
            else if (distance[j] <= mindist*1.10) {
                if (mindist > 0)
                    weight = 1.0 - (distance[j] - mindist) / (mindist * 0.10);
                add_vert_to_defgroup (ob, defgroup, i, weight, WEIGHT_REPLACE);
            }
            
            /* if the cooresponding bone is outside of the 10% tolerance
             * then remove the vert from the weight group (if it is
             * in that group)
             */
            else {
                remove_vert_defgroup (ob, defgroup, i);
            }
        }
    }

    /* free the memory allocated
     */
    MEM_freeN(bonelist);
    MEM_freeN(dgrouplist);
    MEM_freeN(distance);
	if (subverts) MEM_freeN(subverts);
}

void create_vgroups_from_armature(Object *ob, Object *par)
{
	/* Lets try to create some vertex groups 
	 * based on the bones of the parent armature.
	 */

	bArmature *arm;
	short mode;

	/* If the parent object is not an armature exit */
	arm = get_armature(par);
	if (!arm)
		return;

	/* Prompt the user on whether/how they want the vertex groups
	 * added to the child mesh */
    mode= pupmenu("Create Vertex Groups? %t|"
				  "Don't Create Groups %x1|"
				  "Name Groups %x2|"
                  "Create From Closest Bones %x3");
	switch (mode){
	case 2:
		/* Traverse the bone list, trying to create empty vertex 
		 * groups cooresponding to the bone.
		 */
		bone_looper(ob, arm->bonebase.first, NULL,
					add_defgroup_unique_bone);
		break;

	case 3:
		/* Traverse the bone list, trying to create vertex groups 
		 * that are populated with the vertices for which the
		 * bone is closest.
		 */
		add_verts_to_closest_dgroup(ob, par);
		break;

	}
} 
