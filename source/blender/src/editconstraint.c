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

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"

#include "BKE_utildefines.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_constraint.h"
#include "BKE_ipo.h"

#include "BIF_editarmature.h"
#include "BIF_editconstraint.h"
#include "BIF_interface.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_editaction.h"

#include "blendef.h"
#include "nla.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static short add_constraint_element (Object *owner, const char *substring, Object *parent, const char *parentstring);
static short detect_constraint_loop (Object *owner, const char* substring, int disable, char type);
static void test_bonelist_constraints (Object *owner, ListBase *list);
static void clear_object_constraint_loop_flags(Object *ob);
//static int is_child_of(struct Object *owner, struct Object *parent);
//static int is_bonechild_of(struct Bone *bone, struct Bone *parent);
static int is_child_of_ex(Object *owner, const char *ownersubstr, Object *parent, const char *parsubstr);

ListBase g_conBase;
const char *g_conString;
Object *g_conObj;


static int is_child_of_ex(Object *owner, const char *ownersubstr, Object *parent, const char *parsubstr)
{
	Object *curob;
	Bone *bone = NULL;
	Bone *parbone= NULL;

	curob=owner;

	/* If this is a bone */
	if (strlen(ownersubstr))
		bone = get_named_bone(get_armature(owner->parent), ownersubstr);
	
	if (strlen(parsubstr))
		parbone = get_named_bone(get_armature(parent), parsubstr);


	/* Traverse the scene graph */
	while (curob && !bone){
		switch (curob->partype){
		case PARBONE:
			if (strlen(parsubstr)){
				bone = get_named_bone(get_armature(curob->parent), curob->parsubstr);
				break;
			}
			/* The break is supposed to be missing */
		default:
			if (curob==parent){
				if (parbone)
					return 0;
				else
					return 1;
			}
		}
		curob=curob->parent;
	}


	/* Descend into the armature scene graph */
	while (bone){
		if (bone==parbone)
			return 1;
		bone=bone->parent;
	}
	
	return 0;
}
/*
static int is_child_of(Object *owner, Object *parent)
{
	Object *curpar;

	for (curpar = owner->parent; curpar; curpar=curpar->parent){
		if (curpar==parent)
			return 1;
	}

	return 0;
}


static int is_bonechild_of(Bone *bone, Bone *parent)
{
	Bone *curpar;

	if (!bone)
		return 0;

	for (curpar = bone->parent; curpar; curpar=curpar->parent){
		if (curpar==parent)
			return 1;
	}
	return 0;
}
*/
static short add_constraint_element (Object *owner, const char *substring, Object *parent, const char *parentstring)
{
	
	if (!owner)
		return 0;

	/* See if this is the original object */
	if (parent == owner){
		if (!strcmp (parentstring, substring))
				return 1;
	}

	if (owner == g_conObj){
		if (!strcmp (g_conString, substring))
			return 1;
	}

	/* See if this is a child of the adding object */
	if (parent){
//		if (is_child_of (owner, parent))
		if (is_child_of_ex (owner, substring, parent, parentstring))
			return 1;
		/* Parent is a bone */
/*		if ((owner==parent) && (owner->type == OB_ARMATURE)){
			if (strlen (substring) && strlen(parentstring)){
				if (is_bonechild_of(get_named_bone(owner->data, substring), get_named_bone(parent->data, parentstring)))
					return 1;
			}
		}
		*/
	}
	return 0;
}

static void test_bonelist_constraints (Object *owner, ListBase *list)
{
	Bone *bone;
	Base	*base1;


	for (bone = list->first; bone; bone=bone->next){
		for (base1 = G.scene->base.first; base1; base1=base1->next){
			clear_object_constraint_loop_flags(base1->object);
		}
		test_constraints(owner, bone->name, 1);
		test_bonelist_constraints (owner, &bone->childbase);
	}
}


static void clear_object_constraint_loop_flags(Object *ob)
{
	bConstraint *con;

	if (!ob)
		return;

	/* Test object constraints */
	for (con = ob->constraints.first; con; con=con->next){
		con->flag &= ~CONSTRAINT_LOOPTESTED;
	}

	switch (ob->type){
	case OB_ARMATURE:
		if (ob->pose){
			bPoseChannel *pchan;
			for (pchan = ob->pose->chanbase.first; pchan; pchan=pchan->next){
				for (con = pchan->constraints.first; con; con=con->next){
					con->flag &= ~CONSTRAINT_LOOPTESTED;
				}
			}
		}
		break;
	default:
		break;
	}
}
void test_scene_constraints (void)
{
	Base *base, *base1;

/*	Clear the "done" flags of all constraints */

	for (base = G.scene->base.first; base; base=base->next){
		clear_object_constraint_loop_flags(base->object);
	}

	/*	Test all constraints */
	for (base = G.scene->base.first; base; base=base->next){
		/* Test the object */
		
		for (base1 = G.scene->base.first; base1; base1=base1->next)
			clear_object_constraint_loop_flags(base1->object);
		

		test_constraints (base->object, "", 1);

	
		/* Test the subobject constraints */
		switch (base->object->type){
		case OB_ARMATURE:
			{
				bArmature *arm;
				arm = get_armature(base->object);
				if (arm)
					test_bonelist_constraints (base->object, &arm->bonebase);
			}
			break;
		default:
			break;
		}


	}
}

int test_constraints (Object *owner, const char *substring, int disable)
{
/*	init_constraint_elements();*/
	g_conObj = owner;
	g_conString = substring;

	if (detect_constraint_loop (owner, substring, disable, 0))
		return 1;
	else
		return 0;

/*	free_constraint_elements();	*/
}

static short detect_constraint_loop (Object *owner, const char* substring, int disable, char typefrom)
{

	bConstraint *curcon;
	ListBase *conlist;
	int		type;
	int		result = 0;

	if (!owner)
		return result;

	/* Check parents */
	/* Get the constraint list for this object */

	if (strlen (substring)){
		switch (owner->type){
		case OB_ARMATURE:
			type = TARGET_BONE;
			break;
		default:
			type = TARGET_OBJECT;
			break;
		}
	}
	else
		type = TARGET_OBJECT;


	switch (type){
	case TARGET_OBJECT:
		conlist = &owner->constraints;
		/* Check parents */
#if 0
		if (owner->parent && (ELEM (owner->partype, PAROBJECT, PARBONE))){
			if (add_constraint_element (owner->parent, "", NULL, NULL)){
				return 1;
			}
		/*	if (detect_constraint_loop (owner->parent, "", disable)){
				return 1;
			}
		*/
		}
		/* Check tracking */
		if (owner->track && (ELEM (owner->partype, PAROBJECT, PARBONE))){
			if (add_constraint_element (owner->track, "", NULL, NULL)){
				return 1;
			}
		/*	if (detect_constraint_loop (owner->track, "", disable)){
				return 1;
			}
		*/
		}
#else
		if (owner->parent && (owner->partype==PAROBJECT))
			if (add_constraint_element (owner->parent, "", NULL, NULL))
				return 1;

		if (owner->parent && (owner->partype==PARBONE))
			if (add_constraint_element (owner->parent, owner->parsubstr, NULL, NULL))
				return 1;

		/* Check tracking */
		if (owner->track)
			if (add_constraint_element (owner->track, "", NULL, NULL))
				return 1;
#endif

		break;
	case TARGET_BONE:
		{
			Bone *bone;
			bPoseChannel *chan;

			bone = get_named_bone(((bArmature*)owner->data), substring);
			chan = get_pose_channel (owner->pose, substring);
			if (bone){
				conlist = &chan->constraints;
				if (bone->parent){
					if (add_constraint_element (owner, bone->parent->name, NULL, NULL))
						return 1;
					if (detect_constraint_loop (owner, bone->parent->name, disable, 0))
						return 1;
				}
				else{
					if (add_constraint_element (owner, "", NULL, NULL))
						return 1;
					if (detect_constraint_loop (owner, "", disable, 0))
						return 1;
				}
			}
			else
				conlist = NULL;
		}
		break;
	default:
		conlist = NULL;
		break;
	}
	
	/* Cycle constraints */
	if (conlist){
		for (curcon = conlist->first; curcon; curcon=curcon->next){
			
			/* Clear the disable flag */
			
			if (curcon->flag & CONSTRAINT_LOOPTESTED){
				return 0;
			}
			else {
				curcon->flag &= ~CONSTRAINT_DISABLE;
				curcon->flag |= CONSTRAINT_LOOPTESTED;
				switch (curcon->type){
				case CONSTRAINT_TYPE_ACTION:
					{
						bActionConstraint *data = curcon->data;

						if (!exist_object(data->tar)){
							data->tar = NULL;
							break;
						}
						
						if (add_constraint_element (data->tar, data->subtarget, owner, substring)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (detect_constraint_loop (data->tar, data->subtarget, disable, CONSTRAINT_TYPE_ACTION)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
					}
					break;
				case CONSTRAINT_TYPE_LOCLIKE:
					{
						bLocateLikeConstraint *data = curcon->data;
					
						if (!exist_object(data->tar)){
							data->tar = NULL;
							break;
						}
						
						if (add_constraint_element (data->tar, data->subtarget, owner, substring)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (detect_constraint_loop (data->tar, data->subtarget, disable, CONSTRAINT_TYPE_LOCLIKE)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
					}
					break;
				case CONSTRAINT_TYPE_ROTLIKE:
					{
						bRotateLikeConstraint *data = curcon->data;
					
						if (!exist_object(data->tar)){
							data->tar = NULL;
							break;
						}
						
						if (add_constraint_element (data->tar, data->subtarget, owner, substring)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (detect_constraint_loop (data->tar, data->subtarget, disable, CONSTRAINT_TYPE_ROTLIKE)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
					}
					break;
				case CONSTRAINT_TYPE_KINEMATIC:
					{
						bKinematicConstraint *data = curcon->data;
						if (!exist_object(data->tar)){
							data->tar = NULL;
							break;
						}
						
						if (add_constraint_element (data->tar, data->subtarget, owner, substring)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//	return 1;
						}
						if (detect_constraint_loop (data->tar, data->subtarget, disable, CONSTRAINT_TYPE_KINEMATIC)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
					}
					break;
				case CONSTRAINT_TYPE_TRACKTO:
					{
						bTrackToConstraint *data = curcon->data;
						if (!exist_object(data->tar)) data->tar = NULL;
						
						if (typefrom != CONSTRAINT_TYPE_TRACKTO && typefrom != CONSTRAINT_TYPE_LOCKTRACK){
							if (add_constraint_element (data->tar, data->subtarget, owner, substring)){
								curcon->flag |= CONSTRAINT_DISABLE;
								result = 1;
								break;
								//	return 1;
							}
						}
						if (typefrom == CONSTRAINT_TYPE_TRACKTO && typefrom == CONSTRAINT_TYPE_LOCKTRACK){
							curcon->flag |= CONSTRAINT_NOREFRESH;
						}
						if (detect_constraint_loop (data->tar, data->subtarget, disable, CONSTRAINT_TYPE_TRACKTO)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (data->reserved2==data->reserved1){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (data->reserved2+3==data->reserved1){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
					}
					break;
				case CONSTRAINT_TYPE_LOCKTRACK:
					{
						bLockTrackConstraint *data = curcon->data;
					
						if (!exist_object(data->tar)){
							data->tar = NULL;
							break;
						}
						
						if (typefrom != CONSTRAINT_TYPE_TRACKTO && typefrom != CONSTRAINT_TYPE_LOCKTRACK){
							if (add_constraint_element (data->tar, data->subtarget, owner, substring)){
								curcon->flag |= CONSTRAINT_DISABLE;
								result = 1;
								break;
								//		return 1;
							}
						}
						if (typefrom == CONSTRAINT_TYPE_TRACKTO && typefrom == CONSTRAINT_TYPE_LOCKTRACK){
							curcon->flag |= CONSTRAINT_NOREFRESH;
						}
						if (detect_constraint_loop (data->tar, data->subtarget, disable, CONSTRAINT_TYPE_LOCKTRACK)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (data->lockflag==data->trackflag){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (data->lockflag+3==data->trackflag){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
					}
					break;
				case CONSTRAINT_TYPE_FOLLOWPATH:
					{
						bFollowPathConstraint *data = curcon->data;
					
						if (!exist_object(data->tar)){
							data->tar = NULL;
							break;
						}
						if (data->tar->type != OB_CURVE){
							data->tar = NULL;
							break;
						}
						if (add_constraint_element (data->tar, "", owner, substring)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (detect_constraint_loop (data->tar, "", disable, CONSTRAINT_TYPE_FOLLOWPATH)){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (data->upflag==data->trackflag){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
						if (data->upflag+3==data->trackflag){
							curcon->flag |= CONSTRAINT_DISABLE;
							result = 1;
							break;
							//		return 1;
						}
					}
					break;
				}
			}
		}
	}
	
	return result;
}

ListBase *get_constraint_client_channels (int forcevalid)
{

	Object *ob;
	char ipstr[64];

	ob=OBACT;
	
	if (!ob)
		return NULL;
	
	/* See if we are a bone constraint */
	if (G.obpose){
		switch (G.obpose->type){
		case OB_ARMATURE:
			{
				bActionChannel *achan;
				Bone *bone;

				bone = get_first_selected_bone();
				if (!bone) break;
				
				/* Make sure we have an action */
				if (!G.obpose->action){
					if (!forcevalid)
						return NULL;
					
					G.obpose->action=add_empty_action();
				}
				
				/* Make sure we have an actionchannel */
				achan = get_named_actionchannel(G.obpose->action, bone->name);
				if (!achan){
					if (!forcevalid)
						return NULL;
					
					achan = MEM_callocN (sizeof(bActionChannel), "actionChannel");

					strcpy (achan->name, bone->name);
					sprintf (ipstr, "%s.%s", G.obpose->action->id.name+2, achan->name);
					ipstr[23]=0;
					achan->ipo=	add_ipo(ipstr, ID_AC);	
					
					BLI_addtail (&G.obpose->action->chanbase, achan);
				}
				
				return &achan->constraintChannels;
			}
		}
	}
	
	return &ob->constraintChannels;
}

ListBase *get_constraint_client(char *name, short *clientType, void **clientdata)
{
	Object *ob;
	ListBase *list;

	ob=OBACT;
	if (clientType)
		*clientType = -1;

	if (!ob)
		return NULL;

	list = &ob->constraints;

	/* Prep the object's constraint channels */
	if (clientType)
		*clientType = TARGET_OBJECT;
	
	if (name)
		strcpy (name, ob->id.name+2);

	if (G.obpose){
		switch (G.obpose->type){
		case OB_ARMATURE:
			{
				Bone *bone;

				bone = get_first_selected_bone();
				if (!bone) break;

				{
					bPoseChannel	*chan;
					
					/* Is the bone the client? */
					if (clientType)
						*clientType = TARGET_BONE;
					if (clientdata)
						*clientdata = bone;
					if (name)
						sprintf (name, "%s>>%s", name, bone->name);
					verify_pose_channel(G.obpose->pose, bone->name);
					chan = get_pose_channel (G.obpose->pose, bone->name);
					list = &chan->constraints;

				}			
			}
			break;
		}
	}

	return list;
}

bConstraint * add_new_constraint(char type)
{
	bConstraint *con;

	con = MEM_callocN(sizeof(bConstraint), "constraint");

	/* Set up a generic constraint datablock */
	con->type = type;
	con->flag |= CONSTRAINT_EXPAND;
	con->enforce=1.0F;
	/* Load the data for it */
	con->data = new_constraint_data(con->type);
	strcpy (con->name, "Const");
	return con;
}

void add_constraint_to_object(bConstraint *con, Object *ob)
{
	ListBase *list;
	list = &ob->constraints;
	if (list)
	{
		unique_constraint_name(con, list);
		BLI_addtail(list, con);
	}
}

void add_constraint_to_client(bConstraint *con)
{
	ListBase *list;
	short type;
	list = get_constraint_client(NULL, &type, NULL);
	if (list)
	{
		unique_constraint_name(con, list);
		BLI_addtail(list, con);
	}
}

bConstraintChannel *add_new_constraint_channel(const char* name)
{
	bConstraintChannel *chan = NULL;

	chan = MEM_callocN(sizeof(bConstraintChannel), "constraintChannel");
	strcpy(chan->name, name);
	
	return chan;
}

void add_influence_key_to_constraint (bConstraint *con){
	printf("doesn't do anything yet\n");
}
