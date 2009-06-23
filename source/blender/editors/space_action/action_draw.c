/**
 * $Id: drawaction.c 17746 2008-12-08 11:19:44Z aligorith $
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

/* Types --------------------------------------------------------------- */

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
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_global.h" 	// XXX remove me!
#include "BKE_context.h"
#include "BKE_utildefines.h"

/* Everything from source (BIF, BDR, BSE) ------------------------------ */ 

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#if 0 // XXX old includes for reference only
	#include "BIF_editaction.h"
	#include "BIF_editkey.h"
	#include "BIF_editnla.h"
	#include "BIF_drawgpencil.h"
	#include "BIF_keyframing.h"
	#include "BIF_language.h"
	#include "BIF_space.h"
	
	#include "BDR_editcurve.h"
	#include "BDR_gpencil.h"

	#include "BSE_drawnla.h"
	#include "BSE_drawipo.h"
	#include "BSE_drawview.h"
	#include "BSE_editaction_types.h"
	#include "BSE_editipo.h"
	#include "BSE_headerbuttons.h"
	#include "BSE_time.h"
	#include "BSE_view.h"
#endif // XXX old defines for reference only

/* XXX */
extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);

/********************************** Slider Stuff **************************** */

#if 0 // XXX all of this slider stuff will need a rethink!
/* sliders for shapekeys */
static void meshactionbuts(SpaceAction *saction, Object *ob, Key *key)
{
	int           i;
	char          str[64];
	float	      x, y;
	uiBlock       *block;
	uiBut 		  *but;
	
	/* lets make the shapekey sliders */
	
	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375f, curarea->winx-0.375f, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, UI_EMBOSS);

	x = ACHANNEL_NAMEWIDTH + 1;
    y = 0.0f;
	
	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (!(G.saction->flag & SACTION_SLIDERS)) {
		ACTWIDTH = ACHANNEL_NAMEWIDTH;
		but=uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  ACHANNEL_NAMEWIDTH - XIC - 5, (short)y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Show action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);
	}
	else {
		but= uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  ACHANNEL_NAMEWIDTH - XIC - 5, (short)y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Hide action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);
		
		ACTWIDTH = ACHANNEL_NAMEWIDTH + SLIDERWIDTH;
		
		/* sliders are open so draw them */
		BIF_ThemeColor(TH_FACE); 
		
		glRects(ACHANNEL_NAMEWIDTH,  0,  ACHANNEL_NAMEWIDTH+SLIDERWIDTH,  curarea->winy);
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (i=1; i < key->totkey; i++) {
			make_rvk_slider(block, ob, i, 
							(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-1, "Slider to control Shape Keys");
			
			y-=CHANNELHEIGHT+CHANNELSKIP;
			
			/* see sliderval array in editkey.c */
			if (i >= 255) break;
		}
	}
	uiDrawBlock(C, block);
}

static void icu_slider_func(void *voidicu, void *voidignore) 
{
	/* the callback for the icu sliders ... copies the
	 * value from the icu->curval into a bezier at the
	 * right frame on the right ipo curve (creating both the
	 * ipo curve and the bezier if needed).
	 */
	IpoCurve  *icu= voidicu;
	BezTriple *bezt=NULL;
	float cfra, icuval;

	cfra = frame_to_float(CFRA);
	if (G.saction->pin==0 && OBACT)
		cfra= get_action_frame(OBACT, cfra);
	
	/* if the ipocurve exists, try to get a bezier
	 * for this frame
	 */
	bezt = get_bezt_icu_time(icu, &cfra, &icuval);

	/* create the bezier triple if one doesn't exist,
	 * otherwise modify it's value
	 */
	if (bezt == NULL) {
		insert_vert_icu(icu, cfra, icu->curval, 0);
	}
	else {
		bezt->vec[1][1] = icu->curval;
	}

	/* make sure the Ipo's are properly processed and
	 * redraw as necessary
	 */
	sort_time_ipocurve(icu);
	testhandles_ipocurve(icu);
	
	/* nla-update (in case this affects anything) */
	synchronize_action_strips();
	
	/* do redraw pushes, and also the depsgraph flushes */
	if (OBACT->pose || ob_get_key(OBACT))
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC);
	else
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_OB);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWBUTSALL, 0);
}

static void make_icu_slider(uiBlock *block, IpoCurve *icu,
					 int x, int y, int w, int h, char *tip)
{
	/* create a slider for the ipo-curve*/
	uiBut *but;
	
	if(icu == NULL) return;
	
	if (IS_EQ(icu->slide_max, icu->slide_min)) {
		if (IS_EQ(icu->ymax, icu->ymin)) {
			if (ELEM(icu->blocktype, ID_CO, ID_KE)) {
				/* hack for constraints and shapekeys (and maybe a few others) */
				icu->slide_min= 0.0;
				icu->slide_max= 1.0;
			}
			else {
				icu->slide_min= -100;
				icu->slide_max= 100;
			}
		}
		else {
			icu->slide_min= icu->ymin;
			icu->slide_max= icu->ymax;
		}
	}
	if (icu->slide_min >= icu->slide_max) {
		SWAP(float, icu->slide_min, icu->slide_max);
	}

	but=uiDefButF(block, NUMSLI, REDRAWVIEW3D, "",
				  x, y , w, h,
				  &(icu->curval), icu->slide_min, icu->slide_max, 
				  10, 2, tip);
	
	uiButSetFunc(but, icu_slider_func, icu, NULL);
	
	// no hilite, the winmatrix is not correct later on...
	uiButSetFlag(but, UI_NO_HILITE);
}

/* sliders for ipo-curves of active action-channel */
static void action_icu_buts(SpaceAction *saction)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	char          str[64];
	float	        x, y;
	uiBlock       *block;

	/* lets make the action sliders */

	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375f, curarea->winx-0.375f, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, UI_EMBOSS);

	x = (float)ACHANNEL_NAMEWIDTH + 1;
    y = 0.0f;
	
	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (G.saction->flag & SACTION_SLIDERS) {
		/* sliders are open so draw them */
		
		/* get editor data */
		data= get_action_context(&datatype);
		if (data == NULL) return;
		
		/* build list of channels to draw */
		filter= (ACTFILTER_FORDRAWING|ACTFILTER_VISIBLE|ACTFILTER_CHANNELS);
		actdata_filter(&act_data, filter, data, datatype);
		
		/* draw backdrop first */
		BIF_ThemeColor(TH_FACE); // change this color... it's ugly
		glRects(ACHANNEL_NAMEWIDTH,  (short)G.v2d->cur.ymin,  ACHANNEL_NAMEWIDTH+SLIDERWIDTH,  (short)G.v2d->cur.ymax);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (ale= act_data.first; ale; ale= ale->next) {
			const float yminc= y-CHANNELHEIGHT/2;
			const float ymaxc= y+CHANNELHEIGHT/2;
			
			/* check if visible */
			if ( IN_RANGE(yminc, G.v2d->cur.ymin, G.v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, G.v2d->cur.ymin, G.v2d->cur.ymax) ) 
			{
				/* determine what needs to be drawn */
				switch (ale->type) {
					case ACTTYPE_CONCHAN: /* constraint channel */
					{
						bActionChannel *achan = (bActionChannel *)ale->owner;
						IpoCurve *icu = (IpoCurve *)ale->key_data;
						
						/* only show if owner is selected */
						if ((ale->ownertype == ACTTYPE_OBJECT) || SEL_ACHAN(achan)) {
							make_icu_slider(block, icu,
											(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
											"Slider to control current value of Constraint Influence");
						}
					}
						break;
					case ACTTYPE_ICU: /* ipo-curve channel */
					{
						bActionChannel *achan = (bActionChannel *)ale->owner;
						IpoCurve *icu = (IpoCurve *)ale->key_data;
						
						/* only show if owner is selected */
						if ((ale->ownertype == ACTTYPE_OBJECT) || SEL_ACHAN(achan)) {
							make_icu_slider(block, icu,
											(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
											"Slider to control current value of IPO-Curve");
						}
					}
						break;
					case ACTTYPE_SHAPEKEY: /* shapekey channel */
					{
						Object *ob= (Object *)ale->id;
						IpoCurve *icu= (IpoCurve *)ale->key_data;
						
						// TODO: only show if object is active 
						if (icu) {
							make_icu_slider(block, icu,
										(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
										"Slider to control ShapeKey");
						}
						else if (ob && ale->index) {
							make_rvk_slider(block, ob, ale->index, 
									(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-1, "Slider to control Shape Keys");
						}
					}
						break;
				}
			}
			
			/* adjust y-position for next one */
			y-=CHANNELHEIGHT+CHANNELSKIP;
		}
		
		/* free tempolary channels */
		BLI_freelistN(&act_data);
	}
	uiDrawBlock(C, block);
}

#endif // XXX all of this slider stuff will need a rethink 


/* ************************************************************************* */
/* Channel List */

/* left hand part */
void draw_channel_names(bAnimContext *ac, SpaceAction *saction, ARegion *ar) 
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
	 *	- offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= ((items*ACHANNEL_STEP) + (ACHANNEL_HEIGHT*2));
	if (height > (v2d->mask.ymax - v2d->mask.ymin)) {
		/* don't use totrect set, as the width stays the same 
		 * (NOTE: this is ok here, the configuration is pretty straightforward) 
		 */
		v2d->tot.ymin= (float)(-height);
	}
	
	/* loop through channels, and set up drawing depending on their type  */	
	y= (float)ACHANNEL_FIRST;
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			bActionGroup *grp = NULL;
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
						expand= ICON_TRIA_DOWN;
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
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_OBJC(base);
					strcpy(name, ob->id.name+2);
				}
					break;
				case ANIMTYPE_FILLACTD: /* action widget */
				{
					bAction *act= (bAction *)ale->data;
					
					group = 4;
					indent= 1;
					special= ICON_ACTION;
					
					if (EXPANDED_ACTC(act))
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_ACTC(act);
					strcpy(name, act->id.name+2);
				}
					break;
				case ANIMTYPE_FILLMATD: /* object materials (dopesheet) expand widget */
				{
					Object *ob = (Object *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_MATERIAL_DATA;
					
					if (FILTER_MAT_OBJC(ob))
						expand = ICON_TRIA_DOWN;
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
						expand = ICON_TRIA_DOWN;
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
						expand = ICON_TRIA_DOWN;
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
						expand = ICON_TRIA_DOWN;
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
						expand = ICON_TRIA_DOWN;
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
						expand = ICON_TRIA_DOWN;
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
					
				
				case ANIMTYPE_GROUP: /* action group */
				{
					bActionGroup *agrp= (bActionGroup *)ale->data;
					
					group= 2;
					indent= 0;
					special= -1;
					
					if (ale->id) {
						/* special exception for materials */
						if (GS(ale->id->name) == ID_MA) 
							offset= 25;
						else
							offset= 14;
					}
					else
						offset= 0;
					
					/* only show expand if there are any channels */
					if (agrp->channels.first) {
						if (EXPANDED_AGRP(agrp))
							expand = ICON_TRIA_DOWN;
						else
							expand = ICON_TRIA_RIGHT;
					}
					
					if (EDITABLE_AGRP(agrp))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					sel = SEL_AGRP(agrp);
					strcpy(name, agrp->name);
				}
					break;
				case ANIMTYPE_FCURVE: /* F-Curve channel */
				{
					FCurve *fcu = (FCurve *)ale->data;
					
					indent = 0;
					
					group= (fcu->grp) ? 1 : 0;
					grp= fcu->grp;
					
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
					
					if (fcu->flag & FCURVE_MUTED)
						mute = ICON_MUTE_IPO_ON;
					else	
						mute = ICON_MUTE_IPO_OFF;
						
					if (fcu->bezt) {
						if (EDITABLE_FCU(fcu))
							protect = ICON_UNLOCKED;
						else
							protect = ICON_LOCKED;
					}
					else
						protect = ICON_ZOOMOUT; // XXX editability is irrelevant here, but this icon is temp...
					
					sel = SEL_FCU(fcu);
					
					getname_anim_fcurve(name, ale->id, fcu);
				}
					break;
				
				case ANIMTYPE_SHAPEKEY: /* shapekey channel */
				{
					KeyBlock *kb = (KeyBlock *)ale->data;
					
					indent = 0;
					special = -1;
					
					offset= (ale->id) ? 21 : 0;
					
					if (kb->name[0] == '\0')
						sprintf(name, "Key %d", ale->index);
					else
						strcpy(name, kb->name);
				}
					break;
				
				case ANIMTYPE_GPDATABLOCK: /* gpencil datablock */
				{
					bGPdata *gpd = (bGPdata *)ale->data;
					ScrArea *sa = (ScrArea *)ale->owner;
					
					indent = 0;
					group= 3;
					
					/* only show expand if there are any channels */
					if (gpd->layers.first) {
						if (gpd->flag & GP_DATA_EXPAND)
							expand = ICON_TRIA_DOWN;
						else
							expand = ICON_TRIA_RIGHT;
					}
					
					switch (sa->spacetype) {
						case SPACE_VIEW3D:
						{
							/* this shouldn't cause any overflow... */
							//sprintf(name, "3DView:%s", view3d_get_name(sa->spacedata.first)); // XXX missing func..
							strcpy(name, "3dView");
							special= ICON_VIEW3D;
						}
							break;
						case SPACE_NODE:
						{
							SpaceNode *snode= sa->spacedata.first;
							char treetype[12];
							
							if (snode->treetype == 1)
								strcpy(treetype, "Composite");
							else
								strcpy(treetype, "Material");
							sprintf(name, "Nodes:%s", treetype);
							
							special= ICON_NODE;
						}
							break;
						case SPACE_SEQ:
						{
							SpaceSeq *sseq= sa->spacedata.first;
							char imgpreview[10];
							
							switch (sseq->mainb) {
								case 1: 	sprintf(imgpreview, "Image..."); 	break;
								case 2: 	sprintf(imgpreview, "Luma..."); 	break;
								case 3: 	sprintf(imgpreview, "Chroma...");	break;
								case 4: 	sprintf(imgpreview, "Histogram");	break;
								
								default:	sprintf(imgpreview, "Sequence");	break;
							}
							sprintf(name, "Sequencer:%s", imgpreview);
							
							special= ICON_SEQUENCE;
						}
							break;
						case SPACE_IMAGE:
						{
							SpaceImage *sima= sa->spacedata.first;
							
							if (sima->image)
								sprintf(name, "Image:%s", sima->image->id.name+2);
							else
								strcpy(name, "Image:<None>");
								
							special= ICON_IMAGE_COL;
						}
							break;
						
						default:
						{
							sprintf(name, "<Unknown GP-Data Source>");
							special= -1;
						}
							break;
					}
				}
					break;
				case ANIMTYPE_GPLAYER: /* gpencil layer */
				{
					bGPDlayer *gpl = (bGPDlayer *)ale->data;
					
					indent = 0;
					special = -1;
					expand = -1;
					group = 1;
					
					if (EDITABLE_GPL(gpl))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					if (gpl->flag & GP_LAYER_HIDE)
						mute = ICON_MUTE_IPO_ON;
					else
						mute = ICON_MUTE_IPO_OFF;
					
					sel = SEL_GPL(gpl);
					BLI_snprintf(name, 32, gpl->info);
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
					uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
					gl_round_box(GL_POLYGON, x+offset,  yminc, (float)ACHANNEL_NAMEWIDTH, ymaxc, 8);
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
			else if (group == 3) {
				/* only for gp-data channels */
				UI_ThemeColorShade(TH_GROUP, 20);
				uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
				gl_round_box(GL_POLYGON, x+offset,  yminc, (float)ACHANNEL_NAMEWIDTH, ymaxc, 8);
			}
			else if (group == 2) {
				/* only for action group channels */
				if (ale->flag & AGRP_ACTIVE)
					UI_ThemeColorShade(TH_GROUP_ACTIVE, 10);
				else
					UI_ThemeColorShade(TH_GROUP, 20);
				uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
				gl_round_box(GL_POLYGON, x+offset,  yminc, (float)ACHANNEL_NAMEWIDTH, ymaxc, 8);
			}
			else {
				/* for normal channels 
				 *	- use 3 shades of color group/standard color for 3 indention level
				 *	- only use group colors if allowed to, and if actually feasible
				 */
				if ( !(saction->flag & SACTION_NODRAWGCOLORS) && 
					 (grp) && (grp->customCol) ) 
				{
					char cp[3];
					
					if (indent == 2) {
						VECCOPY(cp, grp->cs.solid);
					}
					else if (indent == 1) {
						VECCOPY(cp, grp->cs.select);
					}
					else {
						VECCOPY(cp, grp->cs.active);
					}
					
					glColor3ub(cp[0], cp[1], cp[2]);
				}
				else
					UI_ThemeColorShade(TH_HEADER, ((indent==0)?20: (indent==1)?-20: -40));
				
				indent += group;
				offset += 7 * indent;
				glBegin(GL_QUADS);
					glVertex2f(x+offset, yminc);
					glVertex2f(x+offset, ymaxc);
					glVertex2f((float)ACHANNEL_NAMEWIDTH, ymaxc);
					glVertex2f((float)ACHANNEL_NAMEWIDTH, yminc);
				glEnd();
			}
			
			/* draw expand/collapse triangle */
			if (expand > 0) {
				UI_icon_draw(x+offset, yminc, expand);
				offset += 17;
			}
			
			/* draw special icon indicating certain data-types */
			if (special > -1) {
				if (ELEM(group, 3, 4)) {
					/* for gpdatablock channels */
					UI_icon_draw(x+offset, yminc, special);
					offset += 17;
				}
				else {
					/* for normal channels */
					UI_icon_draw(x+offset, yminc, special);
					offset += 17;
				}
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
				UI_icon_draw((float)ACHANNEL_NAMEWIDTH-offset, yminc, protect);
			}
			
			/* draw mute 'eye' */
			if (mute > -1) {
				offset += 16;
				UI_icon_draw((float)(ACHANNEL_NAMEWIDTH-offset), yminc, mute);
			}
			glDisable(GL_BLEND);
		}
		
		/* adjust y-position for next one */
		y -= ACHANNEL_STEP;
	}
	
	/* free tempolary channels */
	BLI_freelistN(&anim_data);
}

/* ************************************************************************* */
/* Keyframes */

ActKeysInc *init_aki_data(bAnimContext *ac, bAnimListElem *ale)
{
	static ActKeysInc aki;
	
	/* no need to set settings if wrong context */
	if ((ac->data == NULL) || ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET)==0)
		return NULL;
	
	/* if strip is mapped, store settings */
	aki.adt= ANIM_nla_mapping_get(ac, ale);
	
	if (ac->datatype == ANIMCONT_DOPESHEET)
		aki.ads= (bDopeSheet *)ac->data;
	else
		aki.ads= NULL;
	aki.actmode= ac->datatype;
		
	/* always return pointer... */
	return &aki;
}


/* draw keyframes in each channel */
void draw_channel_strips(bAnimContext *ac, SpaceAction *saction, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ar->v2d;
	AnimData *adt= NULL;
	gla2DDrawInfo *di;
	rcti scr_rct;
	
	int act_start, act_end, dummy;
	int height, items;
	float y, sta, end;
	
	char col1[3], col2[3];
	char col1a[3], col2a[3];
	char col1b[3], col2b[3];
	
	
	/* get theme colors */
	UI_GetThemeColor3ubv(TH_BACK, col2);
	UI_GetThemeColor3ubv(TH_HILITE, col1);
	UI_GetThemeColor3ubv(TH_GROUP, col2a);
	UI_GetThemeColor3ubv(TH_GROUP_ACTIVE, col1a);
	
	UI_GetThemeColor3ubv(TH_DOPESHEET_CHANNELOB, col1b);
	UI_GetThemeColor3ubv(TH_DOPESHEET_CHANNELSUBOB, col2b);
	
	/* set view-mapping rect (only used for x-axis), for NLA-scaling mapping with less calculation */
	scr_rct.xmin= ar->winrct.xmin + ar->v2d.mask.xmin;
	scr_rct.ymin= ar->winrct.ymin + ar->v2d.mask.ymin;
	scr_rct.xmax= ar->winrct.xmin + ar->v2d.hor.xmax;
	scr_rct.ymax= ar->winrct.ymin + ar->v2d.mask.ymax; 
	di= glaBegin2DDraw(&scr_rct, &v2d->cur);

	/* if in NLA there's a strip active, map the view */
	if (ac->datatype == ANIMCONT_ACTION) {
		adt= ANIM_nla_mapping_get(ac, NULL);
		
		if (adt)
			ANIM_nla_mapping_draw(di, adt, 0);
		
		/* start and end of action itself */
		calc_action_range(ac->data, &sta, &end, 0);
		gla2DDrawTranslatePt(di, sta, 0.0f, &act_start, &dummy);
		gla2DDrawTranslatePt(di, end, 0.0f, &act_end, &dummy);
		
		if (adt)
			ANIM_nla_mapping_draw(di, adt, 1);
	}
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CHANNELS);
	items= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 * 	- this is done to allow the channel list to be scrollable, but must be done here
	 * 	  to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= ((items*ACHANNEL_STEP) + (ACHANNEL_HEIGHT*2));
	/* don't use totrect set, as the width stays the same 
	 * (NOTE: this is ok here, the configuration is pretty straightforward) 
	 */
	v2d->tot.ymin= (float)(-height);
	
	/* first backdrop strips */
	y= (float)(-ACHANNEL_HEIGHT);
	glEnable(GL_BLEND);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			int frame1_x, channel_y, sel=0;
			
			/* determine if any need to draw channel */
			if (ale->datatype != ALE_NONE) {
				/* determine if channel is selected */
				switch (ale->type) {
					case ANIMTYPE_SCENE:
					{
						Scene *sce= (Scene *)ale->data;
						sel = SEL_SCEC(sce);
					}
						break;
					case ANIMTYPE_OBJECT:
					{
						Base *base= (Base *)ale->data;
						sel = SEL_OBJC(base);
					}
						break;
					case ANIMTYPE_GROUP:
					{
						bActionGroup *agrp = (bActionGroup *)ale->data;
						sel = SEL_AGRP(agrp);
					}
						break;
					case ANIMTYPE_FCURVE:
					{
						FCurve *fcu = (FCurve *)ale->data;
						sel = SEL_FCU(fcu);
					}
						break;
					case ANIMTYPE_GPLAYER:
					{
						bGPDlayer *gpl = (bGPDlayer *)ale->data;
						sel = SEL_GPL(gpl);
					}
						break;
				}
				
				if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET)) {
					gla2DDrawTranslatePt(di, v2d->cur.xmin, y, &frame1_x, &channel_y);
					
					switch (ale->type) {
						case ANIMTYPE_SCENE:
						case ANIMTYPE_OBJECT:
						{
							if (sel) glColor4ub(col1b[0], col1b[1], col1b[2], 0x45); 
							else glColor4ub(col1b[0], col1b[1], col1b[2], 0x22); 
						}
							break;
						
						case ANIMTYPE_FILLACTD:
						case ANIMTYPE_FILLMATD:
						case ANIMTYPE_DSSKEY:
						case ANIMTYPE_DSWOR:
						{
							if (sel) glColor4ub(col2b[0], col2b[1], col2b[2], 0x45); 
							else glColor4ub(col2b[0], col2b[1], col2b[2], 0x22); 
						}
							break;
						
						case ANIMTYPE_GROUP:
						{
							if (sel) glColor4ub(col1a[0], col1a[1], col1a[2], 0x22);
							else glColor4ub(col2a[0], col2a[1], col2a[2], 0x22);
						}
							break;
						
						default:
						{
							if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x22);
							else glColor4ub(col2[0], col2[1], col2[2], 0x22);
						}
							break;
					}
					
					/* draw region twice: firstly backdrop, then the current range */
					glRectf((float)frame1_x,  (float)channel_y-ACHANNEL_HEIGHT_HALF,  (float)v2d->hor.xmax,  (float)channel_y+ACHANNEL_HEIGHT_HALF);
					
					if (ac->datatype == ANIMCONT_ACTION)
						glRectf((float)act_start,  (float)channel_y-ACHANNEL_HEIGHT_HALF,  (float)act_end,  (float)channel_y+ACHANNEL_HEIGHT_HALF);
				}
				else if (ac->datatype == ANIMCONT_SHAPEKEY) {
					gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
					
					/* all frames that have a frame number less than one
					 * get a desaturated orange background
					 */
					glColor4ub(col2[0], col2[1], col2[2], 0x22);
					glRectf(0.0f, (float)channel_y-ACHANNEL_HEIGHT_HALF, (float)frame1_x, (float)channel_y+ACHANNEL_HEIGHT_HALF);
					
					/* frames one and higher get a saturated orange background */
					glColor4ub(col2[0], col2[1], col2[2], 0x44);
					glRectf((float)frame1_x, (float)channel_y-ACHANNEL_HEIGHT_HALF, (float)v2d->hor.xmax,  (float)channel_y+ACHANNEL_HEIGHT_HALF);
				}
				else if (ac->datatype == ANIMCONT_GPENCIL) {
					gla2DDrawTranslatePt(di, v2d->cur.xmin, y, &frame1_x, &channel_y);
					
					/* frames less than one get less saturated background */
					if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x22);
					else glColor4ub(col2[0], col2[1], col2[2], 0x22);
					glRectf(0.0f, (float)channel_y-ACHANNEL_HEIGHT_HALF, (float)frame1_x, (float)channel_y+ACHANNEL_HEIGHT_HALF);
					
					/* frames one and higher get a saturated background */
					if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x44);
					else glColor4ub(col2[0], col2[1], col2[2], 0x44);
					glRectf((float)frame1_x, (float)channel_y-ACHANNEL_HEIGHT_HALF, (float)v2d->hor.xmax,  (float)channel_y+ACHANNEL_HEIGHT_HALF);
				}
			}
		}
		
		/*	Increment the step */
		y -= ACHANNEL_STEP;
	}		
	glDisable(GL_BLEND);
	
	/* Draw keyframes 
	 *	1) Only channels that are visible in the Action Editor get drawn/evaluated.
	 *	   This is to try to optimise this for heavier data sets
	 *	2) Keyframes which are out of view horizontally are disregarded 
	 */
	y= (float)(-ACHANNEL_HEIGHT);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			/* check if anything to show for this channel */
			if (ale->datatype != ALE_NONE) {
				ActKeysInc *aki= init_aki_data(ac, ale); 
				adt= ANIM_nla_mapping_get(ac, ale);
				
				if (adt)
					ANIM_nla_mapping_draw(di, adt, 0);
				
				/* draw 'keyframes' for each specific datatype */
				switch (ale->datatype) {
					case ALE_SCE:
						draw_scene_channel(di, aki, ale->key_data, y);
						break;
					case ALE_OB:
						draw_object_channel(di, aki, ale->key_data, y);
						break;
					case ALE_ACT:
						draw_action_channel(di, aki, ale->key_data, y);
						break;
					case ALE_GROUP:
						draw_agroup_channel(di, aki, ale->data, y);
						break;
					case ALE_FCURVE:
						draw_fcurve_channel(di, aki, ale->key_data, y);
						break;
					case ALE_GPFRAME:
						draw_gpl_channel(di, aki, ale->data, y);
						break;
				}
				
				if (adt) 
					ANIM_nla_mapping_draw(di, adt, 1);
			}
		}
		
		y-= ACHANNEL_STEP;
	}
	
	/* free tempolary channels used for drawing */
	BLI_freelistN(&anim_data);

	/* black line marking 'current frame' for Time-Slide transform mode */
	if (saction->flag & SACTION_MOVING) {
		int frame1_x;
		
		gla2DDrawTranslatePt(di, saction->timeslide, 0, &frame1_x, &dummy);
		cpack(0x0);
		
		glBegin(GL_LINES);
			glVertex2f((float)frame1_x, (float)v2d->mask.ymin - 100);
			glVertex2f((float)frame1_x, (float)v2d->mask.ymax);
		glEnd();
	}
	
	glaEnd2DDraw(di);
}
