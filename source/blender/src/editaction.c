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

#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_lattice_types.h"

#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_displist.h"
#include "BKE_curve.h"
#include "BKE_ipo.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_action.h"
#include "BKE_key.h"

#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_toolbox.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_butspace.h"
#include "BIF_interface.h"
#include "BIF_editview.h"
#include "BIF_poseobject.h"
#include "BIF_editarmature.h"
#include "BIF_editaction.h"

#include "BSE_edit.h"
#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_editipo.h"
#include "BSE_editaction.h"
#include "BSE_trans_types.h"
#include "BSE_editaction_types.h"

#include "BDR_editobject.h"

#include "mydevice.h"
#include "blendef.h"
#include "nla.h"

static bPose	*g_posebuf=NULL;
extern int count_action_levels (bAction *act);

#define BEZSELECTED(bezt)   (((bezt)->f1 & 1) || ((bezt)->f2 & 1) || ((bezt)->f3 & 1))

/* Local Function prototypes */

static void insertactionkey(bAction *act, bActionChannel *achan, bPoseChannel *chan, int adrcode, short makecurve, float time);
static void flip_name (char *name);
static void mouse_actionchannels(bAction *act, short *mval, 
                                 short *mvalo, int selectmode);
static void mouse_action(int selectmode);
static void mouse_mesh_action(int selectmode, Key *key);
static bActionChannel *get_nearest_actionchannel_key (float *index, short *sel, bConstraintChannel **conchan);
static void delete_actionchannels(void);
static void transform_actionchannel_keys(char mode);
static void transform_meshchannel_keys(char mode, Key *key);
static void select_poseelement_by_name (char *name, int select);
static void hilight_channel (bAction *act, bActionChannel *chan, short hilight);
static void set_action_key_time (bAction *act, bPoseChannel *chan, int adrcode, short makecurve, float time);
/* Implementation */

short showsliders = 0;
short ACTWIDTH = NAMEWIDTH;

static void select_poseelement_by_name (char *name, int select)
{
	/* Synchs selection of channels with selection of object elements in posemode */

	Object *ob;

	ob = G.obpose;
	if (!ob)
		return;

	switch (ob->type){
	case OB_ARMATURE:
		select_bone_by_name ((bArmature*)ob->data, name, select);
		break;
	default:
		break;
	}
}
#ifdef __NLA_BAKE
bAction* bake_action_with_client (bAction *act, Object *armob, float tolerance)
{
	bAction			*result=NULL;
	bActionChannel *achan;
	float			actlen;
	int				curframe;
	char			newname[64];
	bArmature		*arm;
	Bone			*bone;
	float			oldframe;
	bAction			*temp;
	bPoseChannel *pchan;

	if (!act)
		return NULL;

	arm = get_armature(armob);

	
	if (G.obedit){
		error ("Not in editmode");
		return NULL;
	}

	if (!arm){
		error ("Must have an armature selected");
		return NULL;
	}
	/* Get a new action */
	result = add_empty_action();

	/* Assign the new action a unique name */
	sprintf (newname, "%s.BAKED", act->id.name+2);
	rename_id(&result->id, newname);

	actlen = calc_action_end(act);

	oldframe = G.scene->r.cfra;

	temp = armob->action;
	armob->action = act;
	
	for (curframe=1; curframe<ceil(actlen+1); curframe++){

		/* Apply the old action */
		
		G.scene->r.cfra = curframe;

		/* Apply the object ipo */
		get_pose_from_action(&armob->pose, act, curframe);
		apply_pose_armature(arm, armob->pose, 1);
		clear_object_constraint_status(armob);
		where_is_armature_time(armob, curframe);
		
		/* For each channel: set avail keys with current values */
		for (pchan=armob->pose->chanbase.first; pchan; pchan=pchan->next){	

			/* Copy the constraints from the armature (if any) */

			bone = get_named_bone(arm, pchan->name);
			if (bone){

				Mat4ToQuat(pchan->obmat, pchan->quat);
				Mat4ToSize(pchan->obmat, pchan->size);
				VECCOPY(pchan->loc, pchan->obmat[3]);
			
				/* Apply to keys */	 
				set_action_key_time (result, pchan, AC_QUAT_X, 1, curframe);
				set_action_key_time (result, pchan, AC_QUAT_Y, 1, curframe);
				set_action_key_time (result, pchan, AC_QUAT_Z, 1, curframe);
				set_action_key_time (result, pchan, AC_QUAT_W, 1, curframe);
				set_action_key_time (result, pchan, AC_LOC_X, 1, curframe);
				set_action_key_time (result, pchan, AC_LOC_Y, 1, curframe);
				set_action_key_time (result, pchan, AC_LOC_Z, 1, curframe);
			}
		}
	}

	/* Make another pass to ensure all keyframes are set to linear interpolation mode */
	for (achan = result->chanbase.first; achan; achan=achan->next){
		IpoCurve* icu;
		for (icu = achan->ipo->curve.first; icu; icu=icu->next){
			icu->ipo= IPO_LIN;
		}
	}

	notice ("Made new action \"%s\"", newname);
	G.scene->r.cfra = oldframe;
	armob->action = temp;
	return result;
}
#endif

void select_actionchannel_by_name (bAction *act, char *name, int select)
{
	bActionChannel *chan;

	if (!act)
		return;

	for (chan = act->chanbase.first; chan; chan=chan->next){
		if (!strcmp (chan->name, name)){
			act->achan = chan;
			if (select){
				chan->flag |= ACHAN_SELECTED;
				hilight_channel (act, chan, 1);
			}
			else{
				chan->flag &= ~ACHAN_SELECTED;
				hilight_channel (act, chan, 0);
			}
			return;
		}
	}
}

void remake_action_ipos(bAction *act)
{
	bActionChannel *chan;
	bConstraintChannel *conchan;
	IpoCurve		*icu;

	for (chan= act->chanbase.first; chan; chan=chan->next){
		if (chan->ipo){
			for (icu = chan->ipo->curve.first; icu; icu=icu->next){
				sort_time_ipocurve(icu);
				testhandles_ipocurve(icu);
			}
		}
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
			if (conchan->ipo){
				for (icu = conchan->ipo->curve.first; icu; icu=icu->next){
					sort_time_ipocurve(icu);
					testhandles_ipocurve(icu);
				}
			}
		}
	}
}

void remake_meshaction_ipos(Ipo *ipo)
{
	/* this puts the bezier triples in proper
	 * order and makes sure the bezier handles
	 * aren't too strange.
	 */
	IpoCurve *icu;

	for (icu = ipo->curve.first; icu; icu=icu->next){
		sort_time_ipocurve(icu);
		testhandles_ipocurve(icu);
	}
}

static void meshkey_do_redraw(Key *key)
{
	remake_meshaction_ipos(key->ipo);
    do_all_ipos();
    do_spec_key(key);

	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);

}

void duplicate_meshchannel_keys(Key *key)
{
	duplicate_ipo_keys(key->ipo);
	transform_meshchannel_keys ('g', key);
}


void duplicate_actionchannel_keys(void)
{
	bAction *act;
	bActionChannel *chan;
	bConstraintChannel *conchan;

	act=G.saction->action;
	if (!act)
		return;

	/* Find selected items */
	for (chan = act->chanbase.first; chan; chan=chan->next){
		duplicate_ipo_keys(chan->ipo);
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
			duplicate_ipo_keys(conchan->ipo);
	}

	transform_actionchannel_keys ('g');
}

static bActionChannel *get_nearest_actionchannel_key (float *index, short *sel, bConstraintChannel **rchan){
	bAction *act;
	bActionChannel *chan;
	IpoCurve *icu;
	bActionChannel *firstchan=NULL;
	bConstraintChannel *conchan, *firstconchan=NULL;
	int	foundsel=0;
	float firstvert=-1, foundx=-1;
		int i;
	short mval[2];
	float ymin, ymax;
	rctf	rectf;
	*index=0;

	*rchan=NULL;
	act=G.saction->action; /* We presume that we are only called during a valid action */
	
	getmouseco_areawin (mval);

	mval[0]-=7;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);

	mval[0]+=14;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

	ymax = count_action_levels (act) * (CHANNELHEIGHT + CHANNELSKIP);

	*sel=0;

	for (chan=act->chanbase.first; chan; chan=chan->next){

		/* Check action channel */
		ymin=ymax-(CHANNELHEIGHT+CHANNELSKIP);
		if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
			for (icu=chan->ipo->curve.first; icu; icu=icu->next){
				for (i=0; i<icu->totvert; i++){
					if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
						if (!firstchan){
							firstchan=chan;
							firstvert=icu->bezt[i].vec[1][0];
							*sel = icu->bezt[i].f2 & 1;	
						}
						
						if (icu->bezt[i].f2 & 1){ 
							if (!foundsel){
								foundsel=1;
								foundx = icu->bezt[i].vec[1][0];
							}
						}
						else if (foundsel && icu->bezt[i].vec[1][0] != foundx){
							*index=icu->bezt[i].vec[1][0];
							*sel = 0;
							return chan;
						}
					}
				}
			}
		}
		ymax=ymin;
		
		/* Check constraint channels */
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
			ymin=ymax-(CHANNELHEIGHT+CHANNELSKIP);
			if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
				for (icu=conchan->ipo->curve.first; icu; icu=icu->next){
					for (i=0; i<icu->totvert; i++){
						if (icu->bezt[i].vec[1][0] > rectf.xmin && icu->bezt[i].vec[1][0] <= rectf.xmax ){
							if (!firstchan){
								firstchan=chan;
								firstconchan=conchan;
								firstvert=icu->bezt[i].vec[1][0];
								*sel = icu->bezt[i].f2 & 1;	
							}
							
							if (icu->bezt[i].f2 & 1){ 
								if (!foundsel){
									foundsel=1;
									foundx = icu->bezt[i].vec[1][0];
								}
							}
							else if (foundsel && icu->bezt[i].vec[1][0] != foundx){
								*index=icu->bezt[i].vec[1][0];
								*sel = 0;
								*rchan = conchan;
								return chan;
							}
						}
					}
				}
			}
			ymax=ymin;
		}
	}	
	
	*rchan = firstconchan;
	*index=firstvert;
	return firstchan;
}

static IpoCurve *get_nearest_meshchannel_key (float *index, short *sel)
{
	/* This function tries to find the RVK key that is
	 * closest to the user's mouse click
	 */
    Key      *key;
    IpoCurve *icu; 
    IpoCurve *firsticu=NULL;
    int	     foundsel=0;
    float    firstvert=-1, foundx=-1;
	int      i;
    short    mval[2];
    float    ymin, ymax, ybase;
    rctf     rectf;

    *index=0;

    key = get_action_mesh_key();
	
    /* lets get the mouse position and process it so 
     * we can start testing selections
     */
    getmouseco_areawin (mval);
    mval[0]-=7;
    areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
    mval[0]+=14;
    areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

    ybase = key->totkey * (CHANNELHEIGHT + CHANNELSKIP);
    *sel=0;

    /* lets loop through the IpoCurves trying to find the closest
     * bezier
     */
    for (icu = key->ipo->curve.first; icu ; icu = icu->next) {
        /* lets not deal with the "speed" Ipo
         */
        if (!icu->adrcode) continue;

        ymax = ybase	- (CHANNELHEIGHT+CHANNELSKIP)*(icu->adrcode-1);
        ymin=ymax-(CHANNELHEIGHT+CHANNELSKIP);

        /* Does this curve coorespond to the right
         * strip?
         */
        if (!((ymax < rectf.ymin) || (ymin > rectf.ymax))){
			
            /* loop through the beziers in the curve
             */
            for (i=0; i<icu->totvert; i++){

                /* Is this bezier in the right area?
                 */
                if (icu->bezt[i].vec[1][0] > rectf.xmin && 
                    icu->bezt[i].vec[1][0] <= rectf.xmax ){

                    /* if no other curves have been picked ...
                     */
                    if (!firsticu){
                        /* mark this curve/bezier as the first
                         * selected
                         */
                        firsticu=icu;
                        firstvert=icu->bezt[i].vec[1][0];

                        /* sel = (is the bezier is already selected) ? 1 : 0;
                         */
                        *sel = icu->bezt[i].f2 & 1;	
                    }

                    /* if the bezier is selected ...
                     */
                    if (icu->bezt[i].f2 & 1){ 
                        /* if we haven't found a selected one yet ...
                         */
                        if (!foundsel){
                            /* record the found x value
                             */
                            foundsel=1;
                            foundx = icu->bezt[i].vec[1][0];
                        }
                    }

                    /* if the bezier is unselected and not at the x
                     * position of a previous found selected bezier ...
                     */
                    else if (foundsel && icu->bezt[i].vec[1][0] != foundx){
                        /* lets return this found curve/bezier
                         */
                        *index=icu->bezt[i].vec[1][0];
                        *sel = 0;
                        return icu;
                    }
                }
            }
        }
	}
	
    /* return what we've found
     */
    *index=firstvert;
    return firsticu;
}

static void mouse_action(int selectmode)
{
	bAction	*act;
	short sel;
	float	selx;
	bActionChannel *chan;
	bConstraintChannel *conchan;
	short	mval[2];

	act=G.saction->action;
	if (!act)
		return;

	getmouseco_areawin (mval);

	chan=get_nearest_actionchannel_key(&selx, &sel, &conchan);

	if (chan){
		if (selectmode == SELECT_REPLACE) {
			if (sel == 0)
				selectmode = SELECT_ADD;
			else
				selectmode = SELECT_SUBTRACT;
			deselect_actionchannel_keys(act, 0);
			deselect_actionchannels(act, 0);
			act->achan = chan;
			chan->flag |= ACHAN_SELECTED;
			hilight_channel (act, chan, 1);
		}
		
		if (conchan)
			select_ipo_key(conchan->ipo, selx, selectmode);
		else
			select_ipo_key(chan->ipo, selx, selectmode);

		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWNLA, 0);

	}
}

static void mouse_mesh_action(int selectmode, Key *key)
{
	/* Handle a right mouse click selection in an
	 * action window displaying RVK data
	 */

    IpoCurve *icu;
    short  sel;
    float  selx;
    short  mval[2];

    /* going to assume that the only reason 
     * we got here is because it has been 
     * determined that we are a mesh with
     * the right properties (i.e., have key
     * data, etc)
     */

	/* get the click location, and the cooresponding
	 * ipo curve and selection time value
	 */
    getmouseco_areawin (mval);
    icu = get_nearest_meshchannel_key(&selx, &sel);

    if (icu){
        if (selectmode == SELECT_REPLACE) {
			/* if we had planned to replace the
			 * selection, then we will first deselect
			 * all of the keys, and if the clicked on
			 * key had been unselected, we will select 
			 * it, otherwise, we are done.
			 */
            deselect_meshchannel_keys(key, 0);

            if (sel == 0)
                selectmode = SELECT_ADD;
            else
				/* the key is selected so we should
				 * deselect -- but everything is now deselected
				 * so we are done.
				 */
				return;
        }
		
		/* select the key using the given mode
		 * and redraw as mush stuff as needed.
		 */
		select_icu_key(icu, selx, selectmode);

        allqueue(REDRAWIPO, 0);
        allqueue(REDRAWVIEW3D, 0);
        allqueue(REDRAWACTION, 0);
        allqueue(REDRAWNLA, 0);

    }
}

void borderselect_action(void)
{ 
	rcti rect;
	rctf rectf;
	int val, selectmode;		
	short	mval[2];
	bActionChannel *chan;
	bConstraintChannel *conchan;
	bAction	*act;
	float	ymin, ymax;

	act=G.saction->action;


	if (!act)
		return;

	if ( (val = get_border(&rect, 3)) ){
    if (val == LEFTMOUSE)
      selectmode = SELECT_ADD;
    else
      selectmode = SELECT_SUBTRACT;

		mval[0]= rect.xmin;
		mval[1]= rect.ymin+2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax-2;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
		
		ymax=count_action_levels(act) * (CHANNELHEIGHT+CHANNELSKIP);
		for (chan=act->chanbase.first; chan; chan=chan->next){
			
			/* Check action */
			ymin=ymax-(CHANNELHEIGHT+CHANNELSKIP);
			if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
          borderselect_ipo_key(chan->ipo, rectf.xmin, rectf.xmax,
                               selectmode);

			ymax=ymin;

			/* Check constraints */
			for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
				ymin=ymax-(CHANNELHEIGHT+CHANNELSKIP);
				if (!((ymax < rectf.ymin) || (ymin > rectf.ymax)))
					borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax,
                               selectmode);
				
				ymax=ymin;
			}
		}	
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWIPO, 0);
	}
}

void borderselect_mesh(Key *key)
{ 
	rcti     rect;
	int      val, adrcodemax, adrcodemin;
	short	 mval[2];
	float	 xmin, xmax;
	int      (*select_function)(BezTriple *);
	IpoCurve *icu;

	if ( (val = get_border(&rect, 3)) ){
		/* set the selection function based on what
		 * mouse button had been used in the border
		 * select
		 */
		if (val == LEFTMOUSE)
			select_function = select_bezier_add;
		else
			select_function = select_bezier_subtract;

		/* get the minimum and maximum adrcode numbers
		 * for the IpoCurves (this is the number that
		 * relates an IpoCurve to the keyblock it
		 * controls).
		 */
		mval[0]= rect.xmin;
		mval[1]= rect.ymin+2;
		adrcodemax = get_nearest_key_num(key, mval, &xmin);
		adrcodemax = (adrcodemax >= key->totkey) ? key->totkey : adrcodemax;

		mval[0]= rect.xmax;
		mval[1]= rect.ymax-2;
		adrcodemin = get_nearest_key_num(key, mval, &xmax);
		adrcodemin = (adrcodemin < 1) ? 1 : adrcodemin;

		/* Lets loop throug the IpoCurves and do borderselect
		 * on the curves with adrcodes in our selected range.
		 */
		for (icu = key->ipo->curve.first; icu ; icu = icu->next) {
			/* lets not deal with the "speed" Ipo
			 */
			if (!icu->adrcode) continue;
			if ( (icu->adrcode >= adrcodemin) && 
				 (icu->adrcode <= adrcodemax) ) {
				borderselect_icu_key(icu, xmin, xmax, select_function);
			}
		}

		/* redraw stuff */
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWIPO, 0);
	}
}


bActionChannel* get_hilighted_action_channel(bAction* action)
{
	bActionChannel *chan;

	if (!action)
		return NULL;

	for (chan=action->chanbase.first; chan; chan=chan->next){
		if (chan->flag & ACHAN_SELECTED && chan->flag & ACHAN_HILIGHTED)
			return chan;
	}

	return NULL;

}

void set_exprap_action(int mode)
{
	if(G.saction->action && G.saction->action->id.lib) return;

	error ("Not yet implemented!");
}

void free_posebuf(void) 
{
	if (g_posebuf){
		clear_pose(g_posebuf);
		MEM_freeN (g_posebuf);
	}
	g_posebuf=NULL;
}

void copy_posebuf (void)
{
	Object *ob;

	free_posebuf();

	ob=G.obpose;
	if (!ob){
		error ("Copybuf is empty");
		return;
	}

	filter_pose_keys();
	copy_pose(&g_posebuf, ob->pose, 0);

}

static void flip_name (char *name)
{

	char	prefix[128]={""};	/* The part before the facing */
	char	suffix[128]={""};	/* The part after the facing */
	char	replace[128]={""};	/* The replacement string */

	char	*index=NULL;
	/* Find the last period */

	strcpy (prefix, name);

	/* Check for an instance of .Right */
	if (!index){
		index = strstr (prefix, "Right");
		if (index){
			*index=0;
			strcpy (replace, "Left");
			strcpy (suffix, index+6);
		}
	}

	/* Che ck for an instance of .RIGHT */
	if (!index){
		index = strstr (prefix, "RIGHT");
		if (index){
			*index=0;
			strcpy (replace, "LEFT");
			strcpy (suffix, index+6);
		}
	}


	/* Check for an instance of .right */
	if (!index){
		index = strstr (prefix, "right");
		if (index){
			*index=0;
			strcpy (replace, "left");
			strcpy (suffix, index+6);
		}
	}

	/* Check for an instance of .left */
	if (!index){
		index = strstr (prefix, "left");
		if (index){
			*index=0;
			strcpy (replace, "right");
			strcpy (suffix, index+5);
		}
	}

	/* Check for an instance of .LEFT */
	if (!index){
		index = strstr (prefix, "LEFT");
		if (index){
			*index=0;
			strcpy (replace, "RIGHT");
			strcpy (suffix, index+5);
		}
	}

	/* Check for an instance of .Left */
	if (!index){
		index = strstr (prefix, "Left");
		if (index){
			*index=0;
			strcpy (replace, "Right");
			strcpy (suffix, index+5);
		}
	}

	/* check for an instance of .L */
	if (!index){
		index = strstr (prefix, ".L");
		if (index){
			*index=0;
			strcpy (replace, ".R");
			strcpy (suffix, index+2);
		}
	}

	/* check for an instance of .l */
	if (!index){
		index = strstr (prefix, ".l");
		if (index){
			*index=0;
			strcpy (replace, ".r");
			strcpy (suffix, index+2);
		}
	}

	/* Checl for an instance of .R */
	if (!index){
		index = strstr (prefix, ".R");
		if (index){
			*index=0;
			strcpy (replace, ".L");
			strcpy (suffix, index+2);
		}
	}

	/* Checl for an instance of .r */
	if (!index){
		index = strstr (prefix, ".r");
		if (index){
			*index=0;
			strcpy (replace, ".l");
			strcpy (suffix, index+2);
		}
	}

	sprintf (name, "%s%s%s", prefix, replace, suffix);
}

void paste_posebuf (int flip){
	Object *ob;
	bPoseChannel *temp, *chan;
	float eul[4];
	Base	*base;
	int		newchan = 0;

	ob=G.obpose;
	if (!ob)
		return;

	if (!g_posebuf){
		error ("Copybuf is empty");
		return;
	};
	
	collect_pose_garbage(ob);

	/* Safely merge all of the channels in this pose into
	any existing pose */
	if (ob->pose){
		if (U.uiflag & USER_KEYINSERTACT){
			/* Display "Avail, all" dialog */
		}
		for (chan=g_posebuf->chanbase.first; chan; chan=chan->next){
			if (chan->flag & POSE_KEY){
				temp = copy_pose_channel (chan);
				if (flip){
					flip_name (temp->name);
					temp->loc[0]*=-1;

					QuatToEul(temp->quat, eul);
					eul[1]*=-1;
					eul[2]*=-1;
					EulToQuat(eul, temp->quat);
				}

				temp = set_pose_channel (ob->pose, temp);

				if (U.uiflag & USER_KEYINSERTACT){
					/* Set keys on pose */
					if (chan->flag & POSE_ROT){
						set_action_key(ob->action, temp, AC_QUAT_X, newchan);
						set_action_key(ob->action, temp, AC_QUAT_Y, newchan);
						set_action_key(ob->action, temp, AC_QUAT_Z, newchan);
						set_action_key(ob->action, temp, AC_QUAT_W, newchan);
					};
					if (chan->flag & POSE_SIZE){
						set_action_key(ob->action, temp, AC_SIZE_X, newchan);
						set_action_key(ob->action, temp, AC_SIZE_Y, newchan);
						set_action_key(ob->action, temp, AC_SIZE_Z, newchan);
					};
					if (chan->flag & POSE_LOC){
						set_action_key(ob->action, temp, AC_LOC_X, newchan);
						set_action_key(ob->action, temp, AC_LOC_Y, newchan);
						set_action_key(ob->action, temp, AC_LOC_Z, newchan);
					};					
				}
			}
		}

		if (U.uiflag & USER_KEYINSERTACT){
			remake_action_ipos(ob->action);
			allqueue (REDRAWIPO, 0);
			allqueue (REDRAWVIEW3D, 0);
			allqueue (REDRAWACTION, 0);		
			allqueue(REDRAWNLA, 0);
		}

		/* Update deformation children */
		if (G.obpose->type == OB_ARMATURE){
			for (base= FIRSTBASE; base; base= base->next){
				if (G.obpose==base->object->parent){
					if (base->object->partype==PARSKEL)
						makeDispList(base->object);
				}
			}
		}
	}
}

void set_action_key (struct bAction *act, struct bPoseChannel *chan, int adrcode, short makecurve)
{
	set_action_key_time (act, chan, adrcode, makecurve, frame_to_float(CFRA));
}

static void set_action_key_time (bAction *act, bPoseChannel *chan, int adrcode, short makecurve, float time)
{
	bActionChannel	*achan;
	char	ipstr[256];

	if (!act)
		return;

	if (!chan)
		return;
	/* See if this action channel exists already */	
	for (achan=act->chanbase.first; achan; achan=achan->next){
		if (!strcmp (chan->name, achan->name))
			break;
	}

	if (!achan){
		if (!makecurve)
			return;
		achan = MEM_callocN (sizeof(bActionChannel), "actionChannel");
		strcpy (achan->name, chan->name);
		BLI_addtail (&act->chanbase, achan);
	}

	/* Ensure the channel appears selected in the action window */
	/* ton: added flag hilighted, for display in ipowin. dunno what the difference is between select/hilite */
	achan->flag |= ACHAN_SELECTED|ACHAN_HILIGHTED;

	/* Ensure this action channel has a valid Ipo */
	if (!achan->ipo){
		sprintf (ipstr, "%s.%s", act->id.name+2, chan->name);
		ipstr[23]=0;
		achan->ipo=	add_ipo(ipstr, ID_AC);	
	}

	insertactionkey(act, achan, chan, adrcode, makecurve, time);

}

static void insertactionkey(bAction *act, bActionChannel *achan, bPoseChannel *chan, int adrcode, short makecurve, float cfra)
{
	IpoCurve *icu;
	void *poin;
	float curval;
	int type;
	ID *id;
	
	if (!act){
		return;
	}
	if (act->id.lib){
		error ("Can't pose libactions");
		return;
	}
	act->achan=achan;
	act->pchan=chan;

	id=(ID*) act;

	/* First see if this curve exists */
	if (!makecurve){
		if (!achan->ipo)
			return;

		for (icu = achan->ipo->curve.first; icu; icu=icu->next){
			if (icu->adrcode == adrcode)
				break;
		}
		if (!icu)
			return;
	}

	
	icu = get_ipocurve (id, GS(id->name), adrcode, achan->ipo);

	if(icu) {
		poin= get_ipo_poin(id, icu, &type);
		if(poin) {
			curval= read_ipo_poin(poin, type);
	//		cfra= frame_to_float(CFRA);
			insert_vert_ipo(icu, cfra, curval);
		}
	}
	
}

bAction *add_empty_action(void)
{
	bAction *act;

	act= alloc_libblock(&G.main->action, ID_AC, "Action");
	act->id.flag |= LIB_FAKEUSER;
	act->id.us++;
	return act;
}

static void transform_actionchannel_keys(char mode)
{
	bAction	*act;
	TransVert *tv;
	int /*sel=0,*/  i;
	bActionChannel	*chan;
	short	mvals[2], mvalc[2], cent[2];
	float	sval[2], cval[2], lastcval[2];
	short	cancel=0;
	float	fac=0.0F;
	int		loop=1;
	int		tvtot=0;
	float	deltax, startx;
	float	cenf[2];
	int		invert=0, firsttime=1;
	char	str[256];
	bConstraintChannel *conchan;

	act=G.saction->action;

	/* Ensure that partial selections result in beztriple selections */
	for (chan=act->chanbase.first; chan; chan=chan->next){
		tvtot+=fullselect_ipo_keys(chan->ipo);

		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
			tvtot+=fullselect_ipo_keys(conchan->ipo);
	}
	
	/* If nothing is selected, bail out */
	if (!tvtot)
		return;
	
	
	/* Build the transvert structure */
	tv = MEM_callocN (sizeof(TransVert) * tvtot, "transVert");
	tvtot=0;
	for (chan=act->chanbase.first; chan; chan=chan->next){
		/* Add the actionchannel */
		tvtot = add_trans_ipo_keys(chan->ipo, tv, tvtot);
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
			tvtot = add_trans_ipo_keys(conchan->ipo, tv, tvtot);
	}

	/* Do the event loop */
	cent[0] = curarea->winx + (G.saction->v2d.hor.xmax)/2;
	cent[1] = curarea->winy + (G.saction->v2d.hor.ymax)/2;
	areamouseco_to_ipoco(G.v2d, cent, &cenf[0], &cenf[1]);

	getmouseco_areawin (mvals);
	areamouseco_to_ipoco(G.v2d, mvals, &sval[0], &sval[1]);

	startx=sval[0];
	while (loop) {
		/*		Get the input */
		/*		If we're cancelling, reset transformations */
		/*			Else calc new transformation */
		/*		Perform the transformations */
		while (qtest()) {
			short val;
			unsigned short event= extern_qread(&val);

			if (val) {
				switch (event) {
				case LEFTMOUSE:
				case SPACEKEY:
				case RETKEY:
					loop=0;
					break;
				case XKEY:
					break;
				case ESCKEY:
				case RIGHTMOUSE:
					cancel=1;
					loop=0;
					break;
				default:
					arrows_move_cursor(event);
					break;
				};
			}
		}

		if (cancel) {
			for (i=0; i<tvtot; i++) {
				tv[i].loc[0]=tv[i].oldloc[0];
				tv[i].loc[1]=tv[i].oldloc[1];
			}
		} else {
			getmouseco_areawin (mvalc);
			areamouseco_to_ipoco(G.v2d, mvalc, &cval[0], &cval[1]);

			if (!firsttime && lastcval[0]==cval[0] && lastcval[1]==cval[1]) {
				PIL_sleep_ms(1);
			} else {
				for (i=0; i<tvtot; i++){
					tv[i].loc[0]=tv[i].oldloc[0];

					switch (mode){
					case 'g':
						deltax = cval[0]-sval[0];
						fac= deltax;
						
						apply_keyb_grid(&fac, 0.0, 1.0, 0.1, U.flag & USER_AUTOGRABGRID);

						tv[i].loc[0]+=fac;
						break;
					case 's':
						startx=mvals[0]-(ACTWIDTH/2+(curarea->winrct.xmax-curarea->winrct.xmin)/2);
						deltax=mvalc[0]-(ACTWIDTH/2+(curarea->winrct.xmax-curarea->winrct.xmin)/2);
						fac= fabs(deltax/startx);
						
						apply_keyb_grid(&fac, 0.0, 0.2, 0.1, U.flag & USER_AUTOSIZEGRID);
		
						if (invert){
							if (i % 03 == 0){
								memcpy (tv[i].loc, tv[i].oldloc, sizeof(tv[i+2].oldloc));
							}
							if (i % 03 == 2){
								memcpy (tv[i].loc, tv[i].oldloc, sizeof(tv[i-2].oldloc));
							}
	
							fac*=-1;
						}
						startx= (G.scene->r.cfra);
					
						tv[i].loc[0]-= startx;
						tv[i].loc[0]*=fac;
						tv[i].loc[0]+= startx;
		
						break;
					}
				}
			}
	
			if (mode=='s'){
				sprintf(str, "sizeX: %.3f", fac);
				headerprint(str);
			}
			else if (mode=='g'){
				sprintf(str, "deltaX: %.3f", fac);
				headerprint(str);
			}
	
			if (G.saction->lock){
				do_all_actions();
				allqueue (REDRAWVIEW3D, 0);
				allqueue (REDRAWACTION, 0);
				allqueue (REDRAWIPO, 0);
				allqueue(REDRAWNLA, 0);
				force_draw_all();
			}
			else {
				addqueue (curarea->win, REDRAWALL, 0);
				force_draw ();
			}
		}
		
		lastcval[0]= cval[0];
		lastcval[1]= cval[1];
		firsttime= 0;
	}
	
	/*		Update the curve */
	/*		Depending on the lock status, draw necessary views */

	do_all_actions();
	remake_action_ipos(act);
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWIPO, 0);
	MEM_freeN (tv);
}

static void transform_meshchannel_keys(char mode, Key *key)
{
	/* this is the function that determines what happens
	 * to those little blocky rvk key things you have selected 
	 * after you press a 'g' or an 's'. I'd love to say that
	 * I have an intimate knowledge of all of what this function
	 * is doing, but instead I'm just going to pretend.
	 */
    TransVert *tv;
    int /*sel=0,*/  i;
    short	mvals[2], mvalc[2], cent[2];
    float	sval[2], cval[2], lastcval[2];
    short	cancel=0;
    float	fac=0.0F;
    int		loop=1;
    int		tvtot=0;
    float	deltax, startx;
    float	cenf[2];
    int		invert=0, firsttime=1;
    char	str[256];

	/* count all of the selected beziers, and
	 * set all 3 control handles to selected
	 */
    tvtot=fullselect_ipo_keys(key->ipo);
    
    /* If nothing is selected, bail out 
	 */
    if (!tvtot)
        return;
	
	
    /* Build the transvert structure 
	 */
    tv = MEM_callocN (sizeof(TransVert) * tvtot, "transVert");
    tvtot=0;

    tvtot = add_trans_ipo_keys(key->ipo, tv, tvtot);

    /* Do the event loop 
	 */
    cent[0] = curarea->winx + (G.saction->v2d.hor.xmax)/2;
    cent[1] = curarea->winy + (G.saction->v2d.hor.ymax)/2;
    areamouseco_to_ipoco(G.v2d, cent, &cenf[0], &cenf[1]);

    getmouseco_areawin (mvals);
    areamouseco_to_ipoco(G.v2d, mvals, &sval[0], &sval[1]);

    startx=sval[0];
    while (loop) {
        /* Get the input
		 * If we're cancelling, reset transformations
		 * Else calc new transformation
		 * Perform the transformations 
		 */
        while (qtest()) {
            short val;
            unsigned short event= extern_qread(&val);

            if (val) {
                switch (event) {
                case LEFTMOUSE:
                case SPACEKEY:
                case RETKEY:
                    loop=0;
                    break;
                case XKEY:
                    break;
                case ESCKEY:
                case RIGHTMOUSE:
                    cancel=1;
                    loop=0;
                    break;
                default:
                    arrows_move_cursor(event);
                    break;
                };
            }
        }
        
        if (cancel) {
            for (i=0; i<tvtot; i++) {
                tv[i].loc[0]=tv[i].oldloc[0];
                tv[i].loc[1]=tv[i].oldloc[1];
            }
        } 
		else {
            getmouseco_areawin (mvalc);
            areamouseco_to_ipoco(G.v2d, mvalc, &cval[0], &cval[1]);
			
            if (!firsttime && lastcval[0]==cval[0] && lastcval[1]==cval[1]) {
                PIL_sleep_ms(1);
            } else {
                for (i=0; i<tvtot; i++){
                    tv[i].loc[0]=tv[i].oldloc[0];

                    switch (mode){
                    case 'g':
                        deltax = cval[0]-sval[0];
                        fac= deltax;
						
                        apply_keyb_grid(&fac, 0.0, 1.0, 0.1, 
                                        U.flag & USER_AUTOGRABGRID);

                        tv[i].loc[0]+=fac;
                        break;
                    case 's':
                        startx=mvals[0]-(ACTWIDTH/2+(curarea->winrct.xmax
                                                     -curarea->winrct.xmin)/2);
                        deltax=mvalc[0]-(ACTWIDTH/2+(curarea->winrct.xmax
                                                     -curarea->winrct.xmin)/2);
                        fac= fabs(deltax/startx);
						
                        apply_keyb_grid(&fac, 0.0, 0.2, 0.1, 
                                        U.flag & USER_AUTOSIZEGRID);
		
                        if (invert){
                            if (i % 03 == 0){
                                memcpy (tv[i].loc, tv[i].oldloc, 
                                        sizeof(tv[i+2].oldloc));
                            }
                            if (i % 03 == 2){
                                memcpy (tv[i].loc, tv[i].oldloc, 
                                        sizeof(tv[i-2].oldloc));
                            }
							
                            fac*=-1;
                        }
                        startx= (G.scene->r.cfra);
                        
                        tv[i].loc[0]-= startx;
                        tv[i].loc[0]*=fac;
                        tv[i].loc[0]+= startx;
		
                        break;
                    }
                }
            }
			/* Display a message showing the magnitude of
			 * the grab/scale we are performing
			 */
            if (mode=='s'){
                sprintf(str, "sizeX: %.3f", fac);
                headerprint(str);
            }
            else if (mode=='g'){
                sprintf(str, "deltaX: %.3f", fac);
                headerprint(str);
            }
	
            if (G.saction->lock){
				/* doubt any of this code ever gets
				 * executed, but it might in the
				 * future
				 */
				 
                do_all_actions();
                allqueue (REDRAWVIEW3D, 0);
                allqueue (REDRAWACTION, 0);
                allqueue (REDRAWIPO, 0);
                allqueue(REDRAWNLA, 0);
                force_draw_all();
            }
            else {
                addqueue (curarea->win, REDRAWALL, 0);
                force_draw ();
            }
        }
		
        lastcval[0]= cval[0];
        lastcval[1]= cval[1];
        firsttime= 0;
    }
	
	/* fix up the Ipocurves and redraw stuff
	 */
    meshkey_do_redraw(key);

    MEM_freeN (tv);

	/* did you understand all of that? I pretty much understand
	 * what it does, but the specifics seem a little weird and crufty.
	 */
}


void deselect_actionchannel_keys (bAction *act, int test)
{
	bActionChannel	*chan;
	bConstraintChannel *conchan;
	int		sel=1;;

	if (!act)
		return;

	/* Determine if this is selection or deselection */
	
	if (test){
		for (chan=act->chanbase.first; chan; chan=chan->next){
			/* Test the channel ipos */
			if (is_ipo_key_selected(chan->ipo)){
				sel = 0;
				break;
			}

			/* Test the constraint ipos */
			for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
				if (is_ipo_key_selected(conchan->ipo)){
					sel = 0;
					break;
				}
			}

			if (sel == 0)
				break;
		}
	}
	else
		sel=0;
	
	/* Set the flags */
	for (chan=act->chanbase.first; chan; chan=chan->next){
		set_ipo_key_selection(chan->ipo, sel);
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
			set_ipo_key_selection(conchan->ipo, sel);
	}
}

void deselect_meshchannel_keys (Key *key, int test)
{
	/* should deselect the rvk keys
	 */
    int		sel=1;

    /* Determine if this is selection or deselection */
    if (test){
        if (is_ipo_key_selected(key->ipo)){
            sel = 0;
        }
    }
    else {
        sel=0;
    }
	
    /* Set the flags */
    set_ipo_key_selection(key->ipo, sel);
}

void deselect_actionchannels (bAction *act, int test)
{
	bActionChannel *chan;
	bConstraintChannel *conchan;
	int			sel=1;	

	if (!act)
		return;

	/* See if we should be selecting or deselecting */
	if (test){
		for (chan=act->chanbase.first; chan; chan=chan->next){
			if (!sel)
				break;

			if (chan->flag & ACHAN_SELECTED){
				sel=0;
				break;
			}
			if (sel){
				for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
					if (conchan->flag & CONSTRAINT_CHANNEL_SELECT){
						sel=0;
						break;
					}
				}
			}
		}
	}
	else
		sel=0;

	/* Now set the flags */
	for (chan=act->chanbase.first; chan; chan=chan->next){
		select_poseelement_by_name(chan->name, sel);

		if (sel)
			chan->flag |= ACHAN_SELECTED;
		else
			chan->flag &= ~ACHAN_SELECTED;

		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next){
			if (sel)
				conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
			else
				conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
		}
	}

}

static void hilight_channel (bAction *act, bActionChannel *chan, short select)
{
	bActionChannel *curchan;

	if (!act)
		return;

	for (curchan=act->chanbase.first; curchan; curchan=curchan->next){
		if (curchan==chan && select)
			curchan->flag |= ACHAN_HILIGHTED;
		else
			curchan->flag &= ~ACHAN_HILIGHTED;
	}
}

/* select_mode = SELECT_REPLACE
 *             = SELECT_ADD
 *             = SELECT_SUBTRACT
 *             = SELECT_INVERT
 */

static int select_channel(bAction *act, bActionChannel *chan,
                          int selectmode) {
	/* Select the channel based on the selection mode
	 */
	int flag;

	switch (selectmode) {
	case SELECT_ADD:
		chan->flag |= ACHAN_SELECTED;
		break;
	case SELECT_SUBTRACT:
		chan->flag &= ~ACHAN_SELECTED;
		break;
	case SELECT_INVERT:
		chan->flag ^= ACHAN_SELECTED;
		break;
	}
	flag = (chan->flag & ACHAN_SELECTED) ? 1 : 0;

	hilight_channel(act, chan, flag);
	select_poseelement_by_name(chan->name, flag);

	return flag;
}

static int select_constraint_channel(bAction *act, 
                                     bConstraintChannel *conchan, 
                                     int selectmode) {
	/* Select the constraint channel based on the selection mode
	 */
	int flag;

	switch (selectmode) {
	case SELECT_ADD:
		conchan->flag |= CONSTRAINT_CHANNEL_SELECT;
		break;
	case SELECT_SUBTRACT:
		conchan->flag &= ~CONSTRAINT_CHANNEL_SELECT;
		break;
	case SELECT_INVERT:
		conchan->flag ^= CONSTRAINT_CHANNEL_SELECT;
		break;
	}
	flag = (conchan->flag & CONSTRAINT_CHANNEL_SELECT) ? 1 : 0;

	return flag;
}


static void mouse_actionchannels(bAction *act, short *mval,
                                 short *mvalo, int selectmode) {
	/* Select action channels, based on mouse values.
	 * If mvalo is NULL we assume it is a one click
	 * action, other wise we treat it like it is a
	 * border select with mval[0],mval[1] and
	 * mvalo[0], mvalo[1] forming the corners of
	 * a rectangle.
	 */
	bActionChannel *chan;
	float	click;
	int   clickmin, clickmax;
	int		wsize, sel;
	bConstraintChannel *conchan;

	if (!act)
		return;
  
	if (selectmode == SELECT_REPLACE) {
		deselect_actionchannels (act, 0);
		selectmode = SELECT_ADD;
	}

	/* wsize is the greatest possible height (in pixels) that would be
	 * needed to draw all of the action channels and constraint
	 * channels.
	 */
	wsize = (count_action_levels (act)*(CHANNELHEIGHT+CHANNELSKIP));

	click = (wsize-(mval[1]+G.v2d->cur.ymin));
	click += CHANNELHEIGHT/2;
	click /= (CHANNELHEIGHT+CHANNELSKIP);
	
	clickmin = (int) click;

	/* Only one click */
	if (mvalo == NULL) {
		clickmax = clickmin;
	}
	/* Two click values (i.e., border select */
	else {
		click = (wsize-(mvalo[1]+G.v2d->cur.ymin));
		click += CHANNELHEIGHT/2;
		click /= (CHANNELHEIGHT+CHANNELSKIP);

		if ( ((int) click) < clickmin) {
			clickmax = clickmin;
			clickmin = (int) click;
		}
		else {
			clickmax = (int) click;
		}
	}

	if (clickmax < 0) {
		return;
	}

	/* clickmin and clickmax now coorespond to indices into
	 * the collection of channels and constraint channels.
	 * What we need to do is apply the selection mode on all
	 * channels and constraint channels between these indices.
	 * This is done by traversing the channels and constraint
	 * channels, for each item decrementing clickmin and clickmax.
	 * When clickmin is less than zero we start selecting stuff,
	 * until clickmax is less than zero or we run out of channels
	 * and constraint channels.
	 */

	for (chan = act->chanbase.first; chan; chan=chan->next){
		if (clickmax < 0) break;

		if ( clickmin <= 0) {
			/* Select the channel with the given mode. If the
			 * channel is freshly selected then set it to the
			 * active channel for the action
			 */
			sel = (chan->flag & ACHAN_SELECTED);
			if ( select_channel(act, chan, selectmode) && !sel ) {
				act->achan = chan;
			}
		}
		--clickmin;
		--clickmax;

		/* Check for click in a constraint */
		for (conchan=chan->constraintChannels.first; 
			 conchan; conchan=conchan->next){
			if (clickmax < 0) break;
			if ( clickmin <= 0) {
				select_constraint_channel(act, conchan, selectmode);
			}
			--clickmin;
			--clickmax;
		}
	}

	allqueue (REDRAWIPO, 0);
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
}

void delete_meshchannel_keys(Key *key)
{
	if (!okee("Erase selected keys"))
		return;

	delete_ipo_keys(key->ipo);

	meshkey_do_redraw(key);
}

void delete_actionchannel_keys(void)
{
	bAction *act;
	bActionChannel *chan;
	bConstraintChannel *conchan;

	act = G.saction->action;
	if (!act)
		return;

	if (!okee("Erase selected keys"))
		return;

	for (chan = act->chanbase.first; chan; chan=chan->next){

		/* Check action channel keys*/
		delete_ipo_keys(chan->ipo);

		/* Delete constraint channel keys */
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
			delete_ipo_keys(conchan->ipo);
	}

	remake_action_ipos (act);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);

}
static void delete_actionchannels (void)
{
	bConstraintChannel *conchan=NULL, *nextconchan;
	bActionChannel *chan, *next;
	bAction	*act;
	int freechan;

	act=G.saction->action;

	if (!act)
		return;

	for (chan=act->chanbase.first; chan; chan=chan->next){
		if (chan->flag & ACHAN_SELECTED)
			break;
		for (conchan=chan->constraintChannels.first; conchan; conchan=conchan->next)
		{
			if (conchan->flag & CONSTRAINT_CHANNEL_SELECT){
				chan=act->chanbase.last;
				break;
			}
		}
	}

	if (!chan && !conchan)
		return;

	if (!okee("Erase selected channels"))
		return;

	for (chan=act->chanbase.first; chan; chan=next){
		freechan = 0;
		next=chan->next;
		
		/* Remove action channels */
		if (chan->flag & ACHAN_SELECTED){
			if (chan->ipo)
				chan->ipo->id.us--;	/* Release the ipo */
			freechan = 1;
		}
		
		/* Remove constraint channels */
		for (conchan=chan->constraintChannels.first; conchan; conchan=nextconchan){
			nextconchan=conchan->next;
			if (freechan || conchan->flag & CONSTRAINT_CHANNEL_SELECT){
				if (conchan->ipo)
					conchan->ipo->id.us--;
				BLI_freelinkN(&chan->constraintChannels, conchan);
			}
		}
		
		if (freechan)
			BLI_freelinkN (&act->chanbase, chan);

	}

	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);

}

void sethandles_meshchannel_keys(int code, Key *key)
{
    sethandles_ipo_keys(key->ipo, code);

	meshkey_do_redraw(key);
}

void sethandles_actionchannel_keys(int code)
{
	bAction *act;
	bActionChannel *chan;

	/* Get the selected action, exit if none are selected 
	 */
	act = G.saction->action;
	if (!act)
		return;

	/* Loop through the channels and set the beziers
	 * of the selected keys based on the integer code
	 */
	for (chan = act->chanbase.first; chan; chan=chan->next){
		sethandles_ipo_keys(chan->ipo, code);
	}

	/* Clean up and redraw stuff
	 */
	remake_action_ipos (act);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

void set_ipotype_actionchannels(int ipotype) {

	bAction *act; 
	bActionChannel *chan;
	short event;

	/* Get the selected action, exit if none are selected 
	 */
	act = G.saction->action;
	if (!act)
		return;

	if (ipotype == SET_IPO_POPUP) {
		/* Present a popup menu asking the user what type
		 * of IPO curve he/she/GreenBTH wants. ;)
		 */
		event
			=  pupmenu("Channel Ipo Type %t|"
					   "Constant %x1|"
					   "Linear %x2|"
					   "Bezier %x3");
		if(event < 1) return;
		ipotype = event;
	}
	
	/* Loop through the channels and for the selected ones set
	 * the type for each Ipo curve in the channel Ipo (based on
	 * the value from the popup).
	 */
	for (chan = act->chanbase.first; chan; chan=chan->next){
		if (chan->flag & ACHAN_SELECTED){
			if (chan->ipo)
				setipotype_ipo(chan->ipo, ipotype);
		}
	}

	/* Clean up and redraw stuff
	 */
	remake_action_ipos (act);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

void select_all_keys_frames(bAction *act, short *mval, 
							short *mvalo, int selectmode) {
	
	/* This function tries to select all action keys in
	 * every channel for a given range of keyframes that
	 * are within the mouse values mval and mvalo (usually
	 * the result of a border select). If mvalo is passed as
	 * NULL then the selection is treated as a one-click and
	 * the function tries to select all keys within half a
	 * frame of the click point.
	 */
	
	rcti rect;
	rctf rectf;
	bActionChannel *chan;
	bConstraintChannel *conchan;

	if (!act)
		return;

	if (selectmode == SELECT_REPLACE) {
		deselect_actionchannel_keys(act, 0);
		selectmode = SELECT_ADD;
	}

	if (mvalo == NULL) {
		rect.xmin = rect.xmax = mval[0];
		rect.ymin = rect.ymax = mval[1];
	}
	else {
		if (mval[0] < mvalo[0] ) {
			rect.xmin = mval[0];
			rect.xmax = mvalo[0];
		}
		else {
			rect.xmin = mvalo[0];
			rect.xmax = mval[0];
		}
		if (mval[1] < mvalo[1] ) {
			rect.ymin = mval[1];
			rect.ymax = mvalo[1];
		}
		else {
			rect.ymin = mvalo[1];
			rect.ymax = mval[1];
		}
	}

	mval[0]= rect.xmin;
	mval[1]= rect.ymin+2;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
	mval[0]= rect.xmax;
	mval[1]= rect.ymax-2;
	areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

	if (mvalo == NULL) {
		rectf.xmin = rectf.xmin - 0.5;
		rectf.xmax = rectf.xmax + 0.5;
	}
    
	for (chan=act->chanbase.first; chan; chan=chan->next){
		borderselect_ipo_key(chan->ipo, rectf.xmin, rectf.xmax,
							 selectmode);
		for (conchan=chan->constraintChannels.first; conchan; 
			 conchan=conchan->next){
			borderselect_ipo_key(conchan->ipo, rectf.xmin, rectf.xmax,
								 selectmode);
		}
	}	
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
}


void select_all_keys_channels(bAction *act, short *mval, 
                              short *mvalo, int selectmode) {
	bActionChannel    *chan;
	float              click;
	int                clickmin, clickmax;
	int                wsize;
	bConstraintChannel *conchan;

	/* This function selects all the action keys that
	 * are in the mouse selection range defined by
	 * the ordered pairs mval and mvalo (usually
	 * these 2 are obtained from a border select).
	 * If mvalo is NULL, then the selection is
	 * treated like a one-click action, and at most
	 * one channel is selected.
	 */

	/* If the action is null then abort
	 */
	if (!act)
		return;

	if (selectmode == SELECT_REPLACE) {
		deselect_actionchannel_keys(act, 0);
		selectmode = SELECT_ADD;
	}

	/* wsize is the greatest possible height (in pixels) that would be
	 * needed to draw all of the action channels and constraint
	 * channels.
	 */
	wsize = (count_action_levels (act)*(CHANNELHEIGHT+CHANNELSKIP));

	click = (wsize-(mval[1]+G.v2d->cur.ymin));
	click += CHANNELHEIGHT/2;
	click /= (CHANNELHEIGHT+CHANNELSKIP);

	clickmin = (int) click;

	/* Only one click */
	if (mvalo == NULL) {
		clickmax = clickmin;
	}
	/* Two click values (i.e., border select) */
	else {
		click = (wsize-(mvalo[1]+G.v2d->cur.ymin));
		click += CHANNELHEIGHT/2;
		click /= (CHANNELHEIGHT+CHANNELSKIP);

		if ( ((int) click) < clickmin) {
			clickmax = clickmin;
			clickmin = (int) click;
		}
		else {
			clickmax = (int) click;
		}
	}

	if (clickmax < 0) {
		return;
	}

	for (chan = act->chanbase.first; chan; chan=chan->next){
		if (clickmax < 0) break;

		if ( clickmin <= 0) {
			/* Select the channel with the given mode. If the
			 * channel is freshly selected then set it to the
			 * active channel for the action
			 */
			select_ipo_bezier_keys(chan->ipo, selectmode);
		}
		--clickmin;
		--clickmax;

		/* Check for click in a constraint */
		for (conchan=chan->constraintChannels.first; 
			 conchan; conchan=conchan->next){
			if (clickmax < 0) break;
			if ( clickmin <= 0) {
				select_ipo_bezier_keys(chan->ipo, selectmode);
			}
			--clickmin;
			--clickmax;
		}
	}
  
	allqueue (REDRAWIPO, 0);
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue (REDRAWNLA, 0);
  
}

static void borderselect_function(void (*select_func)(bAction *act, 
                                                     short *mval, 
                                                     short *mvalo, 
                                                     int selectmode)) {
	/* This function executes an arbitrary selection
	 * function as part of a border select. This
	 * way the same function that is used for
	 * right click selection points can generally
	 * be used as the argument to this function
	 */
	rcti rect;
	short	mval[2], mvalo[2];
	bAction	*act;
	int val;		

	/* Get the selected action, exit if none are selected 
	 */
	act=G.saction->action;
	if (!act)
		return;

	/* Let the user draw a border (or abort)
	 */
	if ( (val=get_border (&rect, 3)) ) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin+2;
		mvalo[0]= rect.xmax;
		mvalo[1]= rect.ymax-2;

		/* if the left mouse was used, do an additive
		 * selection with the user defined selection
		 * function.
		 */
		if (val == LEFTMOUSE)
			select_func(act, mval, mvalo, SELECT_ADD);
		
		/* if the right mouse was used, do a subtractive
		 * selection with the user defined selection
		 * function.
		 */
		else if (val == RIGHTMOUSE)
			select_func(act, mval, mvalo, SELECT_SUBTRACT);
	}
	
}

void winqreadactionspace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	unsigned short event= evt->event;
	short val= evt->val;
	SpaceAction *saction;
	bAction	*act;
	int doredraw= 0;
	short	mval[2];
	float dx,dy;
	int	cfra;
	Key *key;

	if(curarea->win==0) return;

	saction= curarea->spacedata.first;
	if (!saction)
		return;

	act=saction->action;
	if(val) {
		
		if( uiDoBlocks(&curarea->uiblocks, event)!=UI_NOTHING ) event= 0;
		
		getmouseco_areawin(mval);

		key = get_action_mesh_key();

		switch(event) {
		case UI_BUT_EVENT:
			do_butspace(val); // abuse!
			break;
		case HOMEKEY:
			do_action_buttons(B_ACTHOME);
			break;

		case CKEY:
			/* scroll the window so the current
			 * frame is in the center.
			 */
			center_currframe();
			break;

		case DKEY:
			if (key) {
				if (G.qual & LR_SHIFTKEY && mval[0]>ACTWIDTH) {
					duplicate_meshchannel_keys(key);
				}
			}
			else {
				if (G.qual & LR_SHIFTKEY && mval[0]>ACTWIDTH){
					duplicate_actionchannel_keys();
					remake_action_ipos(act);
				}
			}
			break;

		case DELKEY:

		case XKEY:
			if (key) {
				delete_meshchannel_keys(key);
			}
			else {
				if (mval[0]<NAMEWIDTH)
					delete_actionchannels ();
				else
					delete_actionchannel_keys ();
			}
			break;

		case GKEY:
			if (mval[0]>=ACTWIDTH) {
				if (key) {
					transform_meshchannel_keys('g', key);
				}
				else {
					transform_actionchannel_keys ('g');
				}
			}
			break;

		case SKEY:
			if (mval[0]>=ACTWIDTH) {
				if (key) {
					transform_meshchannel_keys('s', key);
				}
				else {
					transform_actionchannel_keys ('s');
				}
			}
			break;

		case AKEY:
			if (key) {
				if (mval[0]<ACTWIDTH){
					/* to do ??? */
				}
				else{
					deselect_meshchannel_keys(key, 1);
					allqueue (REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue (REDRAWIPO, 0);
				}
			}
			else {
				if (mval[0]<NAMEWIDTH){
					deselect_actionchannels (act, 1);
					allqueue (REDRAWVIEW3D, 0);
					allqueue (REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue (REDRAWIPO, 0);
				}
				else if (mval[0]>ACTWIDTH){
					deselect_actionchannel_keys (act, 1);
					allqueue (REDRAWACTION, 0);
					allqueue(REDRAWNLA, 0);
					allqueue (REDRAWIPO, 0);
				}
			}
			break;

		case VKEY:
			if (key) {
				sethandles_meshchannel_keys(HD_VECT, key);
				/* to do */
			}
			else {
				sethandles_actionchannel_keys(HD_VECT);
			}
			break;
		case HKEY:
			if (key) {
				if(G.qual & LR_SHIFTKEY) {
					sethandles_meshchannel_keys(HD_AUTO, key);
				}
				else { 
					sethandles_meshchannel_keys(HD_ALIGN, key);
				}
			}
			else {
				if(G.qual & LR_SHIFTKEY) {
					sethandles_actionchannel_keys(HD_AUTO);
				}
				else { 
					sethandles_actionchannel_keys(HD_ALIGN);
				}
			}
			break;

			/*** set the Ipo type  ***/
		case TKEY:
			if (key) {
				/* to do */
			}
			else {
				set_ipotype_actionchannels(SET_IPO_POPUP);
			}
			break;

		case BKEY:
			if (key) {
				if (mval[0]<ACTWIDTH){
					/* to do?? */
				}
				else {
					borderselect_mesh(key);
				}
			}
			else {

				/* If the border select is initiated in the
				 * part of the action window where the channel
				 * names reside, then select the channels
				 */
				if (mval[0]<NAMEWIDTH){
					borderselect_function(mouse_actionchannels);
				}
				else if (mval[0]>ACTWIDTH){

					/* If the border select is initiated in the
					 * vertical scrollbar, then (de)select all keys
					 * for the channels in the selection region
					 */
					if (IN_2D_VERT_SCROLL(mval)) {
						borderselect_function(select_all_keys_channels);
					}

					/* If the border select is initiated in the
					 * horizontal scrollbar, then (de)select all keys
					 * for the keyframes in the selection region
					 */
					else if (IN_2D_HORIZ_SCROLL(mval)) {
						borderselect_function(select_all_keys_frames);
					}

					/* Other wise, select the action keys
					 */
					else {
						borderselect_action();
					}
				}
			}
			break;

		case LEFTMOUSE:
			if (mval[0]>ACTWIDTH){
				do {
					getmouseco_areawin(mval);
					
					areamouseco_to_ipoco(G.v2d, mval, &dx, &dy);
					
					cfra= (int)dx;
					if(cfra< 1) cfra= 1;
					
					if( cfra!=CFRA ) {
						CFRA= cfra;
						update_for_newframe();
						force_draw_plus(SPACE_VIEW3D);
						force_draw_plus(SPACE_IPO);
						force_draw_plus(SPACE_BUTS);
					}
					
				} while(get_mbut()&L_MOUSE);
				break;
			}
			/* no break here, we allow leftmouse click too */
		case RIGHTMOUSE:
			/* Right clicking in the channel area selects the
			 * channel or constraint channel
			 */
			if (mval[0]<NAMEWIDTH) {
				if(G.qual & LR_SHIFTKEY)
					mouse_actionchannels(act, mval, NULL, 
										 SELECT_INVERT);
				else
					mouse_actionchannels(act, mval, NULL, 
										 SELECT_REPLACE);
			}
			else if (mval[0]>ACTWIDTH) {

				/* Right clicking in the vertical scrollbar selects
				 * all of the keys for that channel at that height
				 */
				if (IN_2D_VERT_SCROLL(mval)) {
					if(G.qual & LR_SHIFTKEY)
						select_all_keys_channels(act, mval, NULL, 
												 SELECT_INVERT);
					else
						select_all_keys_channels(act, mval, NULL, 
												 SELECT_REPLACE);
				}

				/* Right clicking in the horizontal scrollbar selects
				 * all of the keys within 0.5 of the nearest integer
				 * frame
				 */
				else if (IN_2D_HORIZ_SCROLL(mval)) {
					if(G.qual & LR_SHIFTKEY)
						select_all_keys_frames(act, mval, NULL, 
											   SELECT_INVERT);
					else
						select_all_keys_frames(act, mval, NULL, 
											   SELECT_REPLACE);
				}
				
				/* Clicking in the main area of the action window
				 * selects keys
				 */
				else {
					if (key) {
						if(G.qual & LR_SHIFTKEY)
							mouse_mesh_action(SELECT_INVERT, key);
						else
							mouse_mesh_action(SELECT_REPLACE, key);
					}
					else {
						if(G.qual & LR_SHIFTKEY)
							mouse_action(SELECT_INVERT);
						else
							mouse_action(SELECT_REPLACE);
					}
				}
			}
			break;

		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
		}
	}

	if(doredraw) addqueue(curarea->win, REDRAW, 1);
	
}

Key *get_action_mesh_key(void) {
	/* gets the key data from the currently selected
	 * mesh/lattice. If a mesh is not selected, or does not have
	 * key data, then we return NULL (currently only
	 * returns key data for RVK type meshes). If there
	 * is an action that is pinned, return null
	 */
    Object *ob;
    Key    *key;

    ob = OBACT;
    if (!ob) return NULL;

	if (G.saction->pin) return NULL;

    if (ob->type==OB_MESH ) {
		key = ((Mesh *)ob->data)->key;
    }
	else if (ob->type==OB_LATTICE ) {
		key = ((Lattice *)ob->data)->key;
	}
	else return NULL;

	if (key) {
		if (key->type == KEY_RELATIVE)
			return key;
	}

    return NULL;
}

int get_nearest_key_num(Key *key, short *mval, float *x) {
	/* returns the key num that cooresponds to the
	 * y value of the mouse click. Does not check
	 * if this is a valid keynum. Also gives the Ipo
	 * x coordinate.
	 */
    int num;
    float ybase, y;

    areamouseco_to_ipoco(G.v2d, mval, x, &y);

    ybase = key->totkey * (CHANNELHEIGHT + CHANNELSKIP);
    num = (int) ((ybase - y) / (CHANNELHEIGHT+CHANNELSKIP));

    return (num + 1);
}

static void clever_keyblock_names(Key *key, short* mval){
    int        but=0, i, keynum;
    char       str[64];
	float      x;
	KeyBlock   *kb;
	/* get the keynum cooresponding to the y value
	 * of the mouse pointer, return if this is
	 * an invalid key number (and we don't deal
	 * with the speed ipo).
	 */

    keynum = get_nearest_key_num(key, mval, &x);
    if ( (keynum < 1) || (keynum >= key->totkey) )
        return;

	kb= key->block.first;
	for (i=0; i<keynum; ++i) kb = kb->next; 

	if (kb->name[0] == '\0') {
		sprintf(str, "Key %d", keynum);
	}
	else {
		strcpy(str, kb->name);
	}

	if ( (kb->slidermin >= kb->slidermax) ) {
		kb->slidermin = 0.0;
		kb->slidermax = 1.0;
	}

    add_numbut(but++, TEX, "KB: ", 0, 24, str, 
               "Does this really need a tool tip?");
	add_numbut(but++, NUM|FLO, "Slider min:", 
			   -10000, kb->slidermax, &kb->slidermin, 0);
	add_numbut(but++, NUM|FLO, "Slider max:", 
			   kb->slidermin, 10000, &kb->slidermax, 0);

    if (do_clever_numbuts(str, but, REDRAW)) {
		strcpy(kb->name, str);
        allqueue (REDRAWACTION, 0);
        allqueue (REDRAWIPO, 0);
	}

	
}

void stupid_damn_numbuts_action(void){
    /* I think this function might have been
     * deemed clever if it could have been
     * called from the event processing
     * routine in this file -- rather than having
     * to go from the NKEY event from blenderqread
     * in toets.c (which returns 0 so nobody else
     * can use the NKEY) then into the clever_numbuts
     * routine in toolbox.c, the finally to this
     * function. Grumble, grumble, grumble ...
     */

    Key *key;
    short mval[2];

    if ( (key = get_action_mesh_key()) ) {
        getmouseco_areawin (mval);
		if (mval[0]<NAMEWIDTH) {
			clever_keyblock_names(key, mval);
		}
    }
}
