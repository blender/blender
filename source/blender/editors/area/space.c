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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_global.h"
#include "BKE_colortools.h"

#include "BLO_readfile.h"

#include "WM_api.h"

#include "ED_area.h"
#include "ED_screen.h"

void freespacelist(ScrArea *sa)
{
	SpaceLink *sl;
	
	for (sl= sa->spacedata.first; sl; sl= sl->next) {
		if(sl->spacetype==SPACE_FILE) {
			SpaceFile *sfile= (SpaceFile*) sl;
			if(sfile->libfiledata)	
				BLO_blendhandle_close(sfile->libfiledata);
			if(sfile->filelist)
				; // XXX freefilelist(sfile);
			if(sfile->pupmenu)
				MEM_freeN(sfile->pupmenu);
		}
		else if(sl->spacetype==SPACE_BUTS) {
			SpaceButs *buts= (SpaceButs*) sl;
//			if(buts->ri) { 
//				if (buts->ri->rect) MEM_freeN(buts->ri->rect);
//				MEM_freeN(buts->ri);
// XXX			}
			if(G.buts==buts) G.buts= NULL;
		}
		else if(sl->spacetype==SPACE_IPO) {
			SpaceIpo *si= (SpaceIpo*) sl;
			if(si->editipo) MEM_freeN(si->editipo);
// XXX			free_ipokey(&si->ipokey);
			if(G.sipo==si) G.sipo= NULL;
		}
		else if(sl->spacetype==SPACE_VIEW3D) {
			View3D *vd= (View3D*) sl;
			if(vd->bgpic) {
				if(vd->bgpic->ima) vd->bgpic->ima->id.us--;
				MEM_freeN(vd->bgpic);
			}
			if(vd->localvd) MEM_freeN(vd->localvd);
			if(vd->clipbb) MEM_freeN(vd->clipbb);
			if(vd->depths) {
// XXX				if(vd->depths->depths) MEM_freeN(vd->depths->depths);
				MEM_freeN(vd->depths);
				vd->depths= NULL;
			}
// XXX			retopo_free_view_data(vd);
			if(vd->properties_storage) MEM_freeN(vd->properties_storage);
			if(G.vd==vd) G.vd= NULL;
			if(vd->ri) { 
// XXX				BIF_view3d_previewrender_free(vd);
			}
		}
		else if(sl->spacetype==SPACE_OOPS) {
// XXX			SpaceOops *so= (SpaceOops *) sl;
// XXX			free_oopspace(so);
		}
		else if(sl->spacetype==SPACE_IMASEL) {
// XXX			SpaceImaSel *simasel= (SpaceImaSel*) sl;
// XXX			free_imasel(simasel);
		}
		else if(sl->spacetype==SPACE_ACTION) {
// XXX			free_actionspace((SpaceAction*)sl);
		}
		else if(sl->spacetype==SPACE_NLA){
			/*			free_nlaspace((SpaceNla*)sl);	*/
		}
		else if(sl->spacetype==SPACE_TEXT) {
// XXX			free_textspace((SpaceText *)sl);
		}
		else if(sl->spacetype==SPACE_SCRIPT) {
// XXX			free_scriptspace((SpaceScript *)sl);
		}
		else if(sl->spacetype==SPACE_SOUND) {
// XXX			free_soundspace((SpaceSound *)sl);
		}
		else if(sl->spacetype==SPACE_IMAGE) {
			SpaceImage *sima= (SpaceImage *)sl;
			if(sima->cumap)
				curvemapping_free(sima->cumap);
			if(sima->info_str)
				MEM_freeN(sima->info_str);
			if(sima->info_spare)
				MEM_freeN(sima->info_spare);
			if(sima->spare)
				IMB_freeImBuf(sima->spare);
		}
		else if(sl->spacetype==SPACE_NODE) {
			/*			SpaceNode *snode= (SpaceNode *)sl; */
		}
	}
	
	BLI_freelistN(&sa->spacedata);
}

/* can be called for area-full, so it should keep interesting stuff */
void duplicatespacelist(ScrArea *newarea, ListBase *lb1, ListBase *lb2)
{
	SpaceLink *sl;
	
	BLI_duplicatelist(lb1, lb2);
	
	/* lb1 is copy from lb2, from lb2 we free stuff, rely on event system to properly re-alloc */
	
	sl= lb2->first;
	while(sl) {
		if(sl->spacetype==SPACE_FILE) {
			SpaceFile *sfile= (SpaceFile*) sl;
			sfile->libfiledata= NULL;
			sfile->filelist= NULL;
			sfile->pupmenu= NULL;
			sfile->menup= NULL;
		}
		else if(sl->spacetype==SPACE_VIEW3D) {
			View3D *v3d= (View3D*)sl;
// XXX			BIF_view3d_previewrender_free(v3d);
			v3d->depths= NULL;
			v3d->retopo_view_data= NULL;
		}
		else if(sl->spacetype==SPACE_OOPS) {
			SpaceOops *so= (SpaceOops *)sl;
			so->oops.first= so->oops.last= NULL;
			so->tree.first= so->tree.last= NULL;
			so->treestore= NULL;
		}
		else if(sl->spacetype==SPACE_IMASEL) {
			SpaceImaSel *simasel= (SpaceImaSel*) sl;
			simasel->pupmenu= NULL;
			simasel->menup= NULL;
// XXX			simasel->files = BIF_filelist_new();	
// XXX			BIF_filelist_setdir(simasel->files, simasel->dir);
// XXX			BIF_filelist_settype(simasel->files, simasel->type);
			/* see SPACE_FILE - elubie */
		}
		else if(sl->spacetype==SPACE_NODE) {
			SpaceNode *snode= (SpaceNode *)sl;
			snode->nodetree= NULL;
		}
		
		sl= sl->next;
	}
	
	/* but some things we copy */
	
	sl= lb1->first;
	while(sl) {
		sl->area= newarea;
		
		if(sl->spacetype==SPACE_BUTS) {
			SpaceButs *buts= (SpaceButs *)sl;
			buts->ri= NULL;
		}
		else if(sl->spacetype==SPACE_FILE) {
			SpaceFile *sfile= (SpaceFile*) sl;
			sfile->menup= NULL;
		}
		else if(sl->spacetype==SPACE_IPO) {
			SpaceIpo *si= (SpaceIpo *)sl;
			si->editipo= NULL;
			si->ipokey.first= si->ipokey.last= NULL;
		}
		else if(sl->spacetype==SPACE_VIEW3D) {
			View3D *vd= (View3D *)sl;
			if(vd->bgpic) {
				vd->bgpic= MEM_dupallocN(vd->bgpic);
				if(vd->bgpic->ima) vd->bgpic->ima->id.us++;
			}
			vd->clipbb= MEM_dupallocN(vd->clipbb);
			vd->ri= NULL;
			vd->properties_storage= NULL;
		}
		else if(sl->spacetype==SPACE_IMAGE) {
			SpaceImage *sima= (SpaceImage *)sl;
			if(sima->cumap)
				sima->cumap= curvemapping_copy(sima->cumap);
			if(sima->info_str)
				sima->info_str= MEM_dupallocN(sima->info_str);
			if(sima->info_spare)
				sima->info_spare= MEM_dupallocN(sima->info_spare);
		}
		sl= sl->next;
	}
	
	/* again: from old View3D restore localview (because full) */
	sl= lb2->first;
	while(sl) {
		if(sl->spacetype==SPACE_VIEW3D) {
			View3D *v3d= (View3D*) sl;
			if(v3d->localvd) {
// XXX				restore_localviewdata(v3d);
				v3d->localvd= NULL;
				v3d->properties_storage= NULL;
				v3d->localview= 0;
				v3d->lay &= 0xFFFFFF;
			}
		}
		sl= sl->next;
	}
}



