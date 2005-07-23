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
 * support for animation modes - Reevan McKay
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_global.h"
#include "BKE_displist.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_screen.h"
#include "BIF_poseobject.h"

#include "BDR_editobject.h"

#include "BSE_edit.h"

#include "mydevice.h"
#include "blendef.h"

void enter_posemode(void)
{
	Base *base;
	Object *ob;
	bArmature *arm;
	
	if(G.scene->id.lib) return;
	base= BASACT;
	if(base==NULL) return;
	
	ob= base->object;
	
	if (ob->id.lib){
		error ("Can't pose libdata");
		return;
	}

	switch (ob->type){
	case OB_ARMATURE:
		arm= get_armature(ob);
		if( arm==NULL ) return;
		
		ob->flag |= OB_POSEMODE;
		base->flag= ob->flag;
		
		allqueue(REDRAWHEADERS, 0);	
		allqueue(REDRAWBUTSALL, 0);	
		allqueue(REDRAWOOPS, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	default:
		return;
	}

	if (G.obedit) exit_editmode(1);
	G.f &= ~(G_VERTEXPAINT | G_FACESELECT | G_TEXTUREPAINT | G_WEIGHTPAINT);
}

void set_pose_keys (Object *ob)
{
	bPoseChannel *chan;

	if (ob->pose){
		for (chan=ob->pose->chanbase.first; chan; chan=chan->next){
			Bone *bone= chan->bone;
			if(bone && (bone->flag & BONE_SELECTED)) {
				chan->flag |= POSE_KEY;		
			}
			else {
				chan->flag &= ~POSE_KEY;
			}
		}
	}
}


void exit_posemode(void)
{
	Object *ob= OBACT;
	Base *base= BASACT;

	if(ob==NULL) return;
	
	ob->flag &= ~OB_POSEMODE;
	base->flag= ob->flag;
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWHEADERS, 0);	
	allqueue(REDRAWBUTSALL, 0);	

	scrarea_queue_headredraw(curarea);
}

void pose_special_editmenu(void)
{
	Object *ob= OBACT;
	bPoseChannel *pchan;
	short nr;
	
	if(!ob && !ob->pose) return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next)
		if(pchan->bone->flag & BONE_ACTIVE) break;
	if(pchan==NULL) return;
	
	nr= pupmenu("Specials%t|Select constraint target%x1");
	if(nr==1) {
		bConstraint *con;
		
		for(con= pchan->constraints.first; con; con= con->next) {
			char *subtarget;
			Object *target= get_constraint_target(con, &subtarget);
			
			if(ob==target) {
				if(subtarget) {
					pchan= get_pose_channel(ob->pose, subtarget);
					pchan->bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				}
			}
		}
		allqueue(REDRAWVIEW3D, 0);
	}
}


