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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "DNA_listBase.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_markers.h"

#include "nla_intern.h"	// own include

/* XXX */
extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);

/* *********************************************** */

/* *********************************************** */
/* Channel List */

void draw_nla_channel_list (bAnimContext *ac, SpaceNla *snla, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ar->v2d;
	float x= 0.0f, y= 0.0f;
	int items, height;
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CHANNELS);
	items= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 * 	- this is done to allow the channel list to be scrollable, but must be done here
	 * 	  to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of NLACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= ((items*NLACHANNEL_STEP) + (NLACHANNEL_HEIGHT*2));
	if (height > (v2d->mask.ymax - v2d->mask.ymin)) {
		/* don't use totrect set, as the width stays the same 
		 * (NOTE: this is ok here, the configuration is pretty straightforward) 
		 */
		v2d->tot.ymax= (float)(height);
	}
	
	/* loop through channels, and set up drawing depending on their type  */	
	y= (float)(-NLACHANNEL_FIRST);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		const float yminc= (float)(y - NLACHANNEL_HEIGHT_HALF);
		const float ymaxc= (float)(y + NLACHANNEL_HEIGHT_HALF);
		const float ydatac= (float)(y - 7);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			short indent= 0, offset= 0, sel= 0, group= 0;
			int expand= -1, protect = -1, special= -1, mute = -1;
			char name[128];
			
			/* determine what needs to be drawn */
			switch (ale->type) {
				case ANIMTYPE_SCENE: /* scene */
				{
					Scene *sce= (Scene *)ale->data;
					
					group= 4;
					indent= 0;
					
					special= ICON_SCENE_DATA;
						
					/* only show expand if there are any channels */
					if (EXPANDED_SCEC(sce))
						expand= ICON_TRIA_UP;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_SCEC(sce);
					strcpy(name, sce->id.name+2);
				}
					break;
				case ANIMTYPE_OBJECT: /* object */
				{
					Base *base= (Base *)ale->data;
					Object *ob= base->object;
					
					group= 4;
					indent= 0;
					
					/* icon depends on object-type */
					if (ob->type == OB_ARMATURE)
						special= ICON_ARMATURE_DATA;
					else	
						special= ICON_OBJECT_DATA;
						
					/* only show expand if there are any channels */
					if (EXPANDED_OBJC(ob))
						expand= ICON_TRIA_UP;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_OBJC(base);
					strcpy(name, ob->id.name+2);
				}
					break;
				case ANIMTYPE_FILLMATD: /* object materials (dopesheet) expand widget */
				{
					Object *ob = (Object *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_MATERIAL_DATA;
					
					if (FILTER_MAT_OBJC(ob))
						expand = ICON_TRIA_UP;
					else
						expand = ICON_TRIA_RIGHT;
						
					strcpy(name, "Materials");
				}
					break;
				
				
				case ANIMTYPE_DSMAT: /* single material (dopesheet) expand widget */
				{
					Material *ma = (Material *)ale->data;
					
					group = 0;
					indent = 0;
					special = ICON_MATERIAL_DATA;
					offset = 21;
					
					if (FILTER_MAT_OBJD(ma))
						expand = ICON_TRIA_UP;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, ma->id.name+2);
				}
					break;
				case ANIMTYPE_DSLAM: /* lamp (dopesheet) expand widget */
				{
					Lamp *la = (Lamp *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_LAMP_DATA;
					
					if (FILTER_LAM_OBJD(la))
						expand = ICON_TRIA_UP;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, la->id.name+2);
				}
					break;
				case ANIMTYPE_DSCAM: /* camera (dopesheet) expand widget */
				{
					Camera *ca = (Camera *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_CAMERA_DATA;
					
					if (FILTER_CAM_OBJD(ca))
						expand = ICON_TRIA_UP;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, ca->id.name+2);
				}
					break;
				case ANIMTYPE_DSCUR: /* curve (dopesheet) expand widget */
				{
					Curve *cu = (Curve *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_CURVE_DATA;
					
					if (FILTER_CUR_OBJD(cu))
						expand = ICON_TRIA_UP;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, cu->id.name+2);
				}
					break;
				case ANIMTYPE_DSSKEY: /* shapekeys (dopesheet) expand widget */
				{
					Key *key= (Key *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_SHAPEKEY_DATA; // XXX 
					
					if (FILTER_SKE_OBJD(key))	
						expand = ICON_TRIA_UP;
					else
						expand = ICON_TRIA_RIGHT;
						
					//sel = SEL_OBJC(base);
					strcpy(name, "Shape Keys");
				}
					break;
				case ANIMTYPE_DSWOR: /* world (dopesheet) expand widget */
				{
					World *wo= (World *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_WORLD_DATA;
					
					if (FILTER_WOR_SCED(wo))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, wo->id.name+2);
				}
					break;
				
				case ANIMTYPE_NLATRACK: /* NLA Track */
				{
					NlaTrack *nlt= (NlaTrack *)ale->data;
					
					indent= 0;
					
					if (ale->id) {
						/* special exception for materials */
						if (GS(ale->id->name) == ID_MA) {
							offset= 21;
							indent= 1;
						}
						else
							offset= 14;
					}
					else
						offset= 0;
					
					/* FIXME: 'solo' as the 'special' button?
					 *	- need special icons for these
					 */
					if (nlt->flag & NLATRACK_SOLO)
						special= ICON_LAYER_ACTIVE;
					else
						special= ICON_LAYER_USED;
						
					if (nlt->flag & NLATRACK_MUTED)
						mute = ICON_MUTE_IPO_ON;
					else	
						mute = ICON_MUTE_IPO_OFF;
						
					if (EDITABLE_NLT(nlt))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					strcpy(name, nlt->name);
				}
					break;
				case ANIMTYPE_NLAACTION: /* NLA Action-Line */
				{
					bAction *act= (bAction *)ale->data;
					
					group = 5;
					
					if (ale->id) {
						/* special exception for materials */
						if (GS(ale->id->name) == ID_MA) {
							offset= 21;
							indent= 1;
						}
						else
							offset= 14;
					}
					else
						offset= 0;
					
					special = ICON_ACTION;
					
					if (act)
						sprintf(name, "ActAction: <%s>", act->id.name+2);
					else
						sprintf(name, "<No Action>");
				}
					break;
			}	
			
			/* now, start drawing based on this information */
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			
			/* draw backing strip behind channel name */
			if (group == 4) {
				/* only used in dopesheet... */
				if (ELEM(ale->type, ANIMTYPE_SCENE, ANIMTYPE_OBJECT)) {
					/* object channel - darker */
					UI_ThemeColor(TH_DOPESHEET_CHANNELOB);
					uiSetRoundBox((expand == ICON_TRIA_UP)? (8):(1|8));
					gl_round_box(GL_POLYGON, x+offset,  yminc, (float)NLACHANNEL_NAMEWIDTH, ymaxc, 8);
				}
				else {
					/* sub-object folders - lighter */
					UI_ThemeColor(TH_DOPESHEET_CHANNELSUBOB);
					
					offset += 7 * indent;
					glBegin(GL_QUADS);
						glVertex2f(x+offset, yminc);
						glVertex2f(x+offset, ymaxc);
						glVertex2f((float)ACHANNEL_NAMEWIDTH, ymaxc);
						glVertex2f((float)ACHANNEL_NAMEWIDTH, yminc);
					glEnd();
					
					/* clear group value, otherwise we cause errors... */
					group = 0;
				}
			}
			else if (group == 5) {
				/* Action Line */
				if (ale->data)
					glColor3f(0.8f, 0.2f, 0.0f);	// reddish color - hardcoded for now 
				else
					glColor3f(0.6f, 0.5f, 0.5f); 	// greyish-red color - hardcoded for now
					
				offset += 7 * indent;
				uiSetRoundBox(15);
				gl_round_box(GL_POLYGON, x+offset,  yminc, (float)NLACHANNEL_NAMEWIDTH, ymaxc, 8);
				
				/* clear group value, otherwise we cause errors... */
				group = 0;
			}
			else {
				/* for normal channels 
				 *	- use 3 shades of color group/standard color for 3 indention level
				 */
				UI_ThemeColorShade(TH_HEADER, ((indent==0)?20: (indent==1)?-20: -40));
				
				indent += group;
				offset += 7 * indent;
				glBegin(GL_QUADS);
					glVertex2f(x+offset, yminc);
					glVertex2f(x+offset, ymaxc);
					glVertex2f((float)NLACHANNEL_NAMEWIDTH, ymaxc);
					glVertex2f((float)NLACHANNEL_NAMEWIDTH, yminc);
				glEnd();
			}
			
			/* draw expand/collapse triangle */
			if (expand > 0) {
				UI_icon_draw(x+offset, ydatac, expand);
				offset += 17;
			}
			
			/* draw special icon indicating certain data-types */
			if (special > -1) {
				/* for normal channels */
				UI_icon_draw(x+offset, ydatac, special);
				offset += 17;
			}
			glDisable(GL_BLEND);
			
			/* draw name */
			if (sel)
				UI_ThemeColor(TH_TEXT_HI);
			else
				UI_ThemeColor(TH_TEXT);
			offset += 3;
			UI_DrawString(x+offset, y-4, name);
			
			/* reset offset - for RHS of panel */
			offset = 0;
			
			/* set blending again, as text drawing may clear it */
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			
			/* draw protect 'lock' */
			if (protect > -1) {
				offset = 16;
				UI_icon_draw((float)NLACHANNEL_NAMEWIDTH-offset, ydatac, protect);
			}
			
			/* draw mute 'eye' */
			if (mute > -1) {
				offset += 16;
				UI_icon_draw((float)(NLACHANNEL_NAMEWIDTH-offset), ydatac, mute);
			}
			
			/* draw action 'push-down' */
			if (ale->type == ANIMTYPE_NLAACTION) {
				offset += 16;
				UI_icon_draw((float)NLACHANNEL_NAMEWIDTH-offset, ydatac, ICON_FREEZE);
			}
			
			glDisable(GL_BLEND);
		}
		
		/* adjust y-position for next one */
		y += NLACHANNEL_STEP;
	}
	
	/* free tempolary channels */
	BLI_freelistN(&anim_data);
}

/* *********************************************** */
