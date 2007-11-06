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

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_oops_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"

#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"

#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_editoops.h"
#include "BIF_editview.h"
#include "BIF_drawscene.h"
#include "BIF_mywindow.h"
#include "BIF_toolbox.h"
#include "BIF_interface.h"

#include "BDR_editobject.h"

#include "BSE_edit.h"
#include "BSE_drawipo.h"

#include "blendef.h"
#include "mydevice.h"


typedef struct TransOops {
	float *loc;
	float oldloc[2];
} TransOops;

struct ID *idt;


static void oops_to_select_objects(void)
{
	Oops *oops;
	Base *base;
	Object *ob;

	if(G.soops==0) return;	

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->type==ID_OB) {
				ob= (Object *)oops->id;
				if ((ob->restrictflag & OB_RESTRICT_VIEW)==0) {
					if(oops->flag & SELECT) ob->flag |= SELECT;
					else ob->flag &= ~SELECT;
				}
			}
		}
		oops= oops->next;
	}
	base= FIRSTBASE;
	while(base) {
		if(base->flag != base->object->flag) {
			base->flag= base->object->flag;
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
}

void swap_select_all_oops(void)
{
	Oops *oops;
	int sel= 0;
	
	if(G.soops==0) return;	

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->flag & SELECT) {
				sel= 1;
				break;
			}
		}
		oops= oops->next;
	}

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(sel) oops->flag &= ~SELECT;
			else oops->flag |= SELECT;
		}
		oops= oops->next;
	}
	
	oops_to_select_objects();	/* also redraw */
	
	G.soops->lockpoin= NULL;
}

/* never used... check CVS 1.12 for the code */
/*  static void select_swap_oops(void) */

static void deselect_all_oops(void)
{
	Oops *oops;
	
	if(G.soops==0) return;	

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			oops->flag &= ~SELECT;
		}
		oops= oops->next;
	}
	G.soops->lockpoin= NULL;
}

void set_select_flag_oops(void)	/* all areas */
{
	SpaceOops *so;
	ScrArea *sa;
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==SPACE_OOPS) {
			so= sa->spacedata.first;
			so->flag |= SO_NEWSELECTED;
		}
		sa= sa->next;
	}
	if(G.soops) G.soops->lockpoin= NULL;
}

void deselect_all_area_oops(void)	/* all areas */
{
	SpaceOops *so;
	Oops *oops;
	ScrArea *sa;
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==SPACE_OOPS) {
			so= sa->spacedata.first;
			
			oops= so->oops.first;
			while(oops) {
				oops->flag &= ~SELECT;
				oops= oops->next;
			}
		}
		sa= sa->next;
	}
	
	if(G.soops) G.soops->lockpoin= NULL;
}

void transform_oops(int mode, int context)
{
	TransOops *transmain, *tv;
	Oops *oops;
	float dx, dy, div, dvec[3], cent[3], min[3], max[3];
	float sizefac, size[2], xref=1.0, yref=1.0;
	int a, tot= 0, midtog= 0;
	unsigned short event = 0;
	short firsttime= 1, proj = 0, afbreek=0, xc, yc, xo, yo, xn, yn, mval[2];
	short val;
	char str[32];
	
	if(G.soops==0) return;	
		
	/* which oopses... */
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->flag & SELECT) {
				tot++;
			}
		}
		oops= oops->next;
	}
	
	if(tot==0) return;
	
	G.moving= 1;
	
	INIT_MINMAX(min, max);
	
	tv=transmain= MEM_callocN(tot*sizeof(TransOops), "transmain");
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->flag & SELECT) {
				tv->loc= &oops->x;
				tv->oldloc[0]= tv->loc[0];
				tv->oldloc[1]= tv->loc[1];
				DO_MINMAX2(tv->loc, min, max);
				tv++;
			}
		}
		oops= oops->next;
	}

	cent[0]= (min[0]+max[0])/2.0;
	cent[1]= (min[1]+max[1])/2.0;

	ipoco_to_areaco_noclip(G.v2d, cent, mval);
	xc= mval[0];
	yc= mval[1];
	
	getmouseco_areawin(mval);
	xo= xn= mval[0];
	yo= yn= mval[1];
	dvec[0]= dvec[1]= 0.0;

	sizefac= sqrt( (float)((yc-yn)*(yc-yn)+(xn-xc)*(xn-xc)) );
	if(sizefac<2.0) sizefac= 2.0;

	while(afbreek==0) {
		getmouseco_areawin(mval);
		if(mval[0]!=xo || mval[1]!=yo || firsttime) {
			
			if(mode=='g') {
			
				dx= mval[0]- xo;
				dy= mval[1]- yo;
	
				div= G.v2d->mask.xmax-G.v2d->mask.xmin;
				dvec[0]+= (G.v2d->cur.xmax-G.v2d->cur.xmin)*(dx)/div;
	
				div= G.v2d->mask.ymax-G.v2d->mask.ymin;
				dvec[1]+= (G.v2d->cur.ymax-G.v2d->cur.ymin)*(dy)/div;
				
				if(midtog) dvec[proj]= 0.0;
				
				tv= transmain;
				for(a=0; a<tot; a++, tv++) {
					
					tv->loc[0]= tv->oldloc[0]+dvec[0];
					tv->loc[1]= tv->oldloc[1]+dvec[1];
						
				}
			
				sprintf(str, "X: %.2f   Y: %.2f  ", dvec[0], dvec[1]);
				headerprint(str);
			}
			else if(mode=='s') {
				size[0]=size[1]= (sqrt( (float)((yc-mval[1])*(yc-mval[1])+(mval[0]-xc)*(mval[0]-xc)) ))/sizefac;
				
				if(midtog) size[proj]= 1.0;
				size[0]*= xref;
				size[1]*= yref;

				tv= transmain;
				for(a=0; a<tot; a++, tv++) {
				
					tv->loc[0]= size[0]*(tv->oldloc[0]-cent[0])+ cent[0];
					tv->loc[1]= size[1]*(tv->oldloc[1]-cent[1])+ cent[1];
					
				}
				
				sprintf(str, "sizeX: %.3f   sizeY: %.3f  ", size[0], size[1]);
				headerprint(str);
			}
			

			xo= mval[0];
			yo= mval[1];
			
			force_draw(0);
			
			firsttime= 0;
			
		}
		else BIF_wait_for_statechange();
		
		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case ESCKEY:
				case RIGHTMOUSE:
				case LEFTMOUSE:
				case SPACEKEY:
				case RETKEY:
					afbreek= 1;
					break;
				case MIDDLEMOUSE:
					
					midtog= ~midtog;
					if(midtog) {
						if( abs(mval[0]-xn) > abs(mval[1]-yn)) proj= 1;
						else proj= 0;
						firsttime= 1;
					}
				
					break;
				default:
					arrows_move_cursor(event);
				}
			}
			if(afbreek) break;
		}
	}
	
	if(event==ESCKEY || event==RIGHTMOUSE) {
		tv= transmain;
		for(a=0; a<tot; a++, tv++) {
			tv->loc[0]= tv->oldloc[0];
			tv->loc[1]= tv->oldloc[1];
		}
	}
	MEM_freeN(transmain);
			
	G.moving= 0;

	scrarea_queue_redraw(curarea);
}

static Oops *find_nearest_oops(void)
{
	Oops *oops;
	float x, y;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &x, &y);
	
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide == 0) {
			if(oops->x <=x && oops->x+OOPSX >= x) {
				if(oops->y <=y && oops->y+OOPSY >= y) {		
					return oops;
				}
			}
		}
		oops= oops->next;
	}
	return 0;
}

static void do_activate_oops(Oops *oops)
{
	Base *base;
	Object *ob;
	
	switch(oops->type) {
	case ID_SCE:
		if(oops->id) set_scene((Scene *)oops->id);
		break;
	case ID_OB:
		base= FIRSTBASE;
		while(base) {
			if(base->object == (Object *)oops->id) break;
			base= base->next;
		}
		if(base) {
			if(G.obedit==NULL) set_active_base(base);	/* editview.c */
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWINFO, 1);
		}
		break;
	case ID_MA:
		ob= OBACT;
		if(ob && oops->id) {
			assign_material(ob, (Material *)oops->id, ob->actcol);
			allqueue(REDRAWBUTSSHADING, 0);
			scrarea_queue_winredraw(curarea);
		}
		break;
		
	case ID_IM:
		if(oops->id && G.sima) {
			/* only set if the new image isnt alredy active */
			if ((ID *)G.sima->image != oops->id) {
				G.sima->image = (Image *)oops->id;
				allqueue(REDRAWIMAGE, 0);
				scrarea_queue_winredraw(curarea);
			}
		}
		break;
	/*
	case ID_IP:
		if(oops->id && G.sipo) {
			*//* only set if the new ipo isnt alredy active *//*
			if ((ID *)G.sipo->ipo != oops->id) {
				G.sipo->ipo = (Ipo *)oops->id;
				allqueue(REDRAWIPO, 0);
				scrarea_queue_winredraw(curarea);
			}
		}
		break;
	*/
	}
}

void mouse_select_oops(void)
{
	Oops *oops;
	extern float oopslastx, oopslasty;	/* oops.c */
	
	if(G.soops==0) return;	
		
	/* which oopses... */
	oops= G.soops->oops.first;

	oops= find_nearest_oops();
	if(oops==0) return;
	
	if((G.qual & LR_SHIFTKEY)==0) deselect_all_oops();
	
	if(oops) {
		/* last_seq= seq; */
		
		if(G.qual==0) {
			oops->flag |= SELECT;
		}
		else {
			if(oops->flag & SELECT) {
				oops->flag &= ~SELECT;
			}
			else {
				oops->flag |= SELECT;
			}
		}
		
		oopslastx= oops->x;
		oopslasty= oops->y;
		
		if(G.qual & LR_CTRLKEY) do_activate_oops(oops);
		G.soops->lockpoin= oops;
	}
	
	oops_to_select_objects();	/* also redraw */
	scrarea_queue_headredraw(curarea);
	
	force_draw(1);
	
	std_rmouse_transform(transform_oops);
}

void borderselect_oops(void)
{
	Oops *oops;
	rcti rect;
	rctf rectf, rq;
	int val;
	short mval[2];

	if(G.soops==0) return;	
	
	val= get_border(&rect, 3);

	if(val) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

		oops= G.soops->oops.first;
		while(oops) {
			if(oops->hide == 0) {
			
				rq.xmin= oops->x;
				rq.xmax= oops->x+OOPSX;
				rq.ymin= oops->y;
				rq.ymax= oops->y+OOPSY;
		
				if(BLI_isect_rctf(&rq, &rectf, 0)) {
					if(val==LEFTMOUSE) {
						oops->flag |= SELECT;
					}
					else {
						oops->flag &= ~SELECT;
					}
				}
			}
			oops= oops->next;
		}

		oops_to_select_objects();	/* also redraw */
	}
}

static void select_oops_lib(ID *id)
{
	Oops *oops;
	
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->id->lib== (Library *)id) oops->flag |= OOPS_DOSELECT;
		}
		oops= oops->next;
	}
}

void select_linked_oops(void)
{
	Oops *oops;
	OopsLink *ol;
	
	if(G.soops==0) return;	

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->flag & SELECT) {
				if(oops->type==ID_LI) select_oops_lib(oops->id);
				ol= oops->link.first;
				while(ol) {
					if(ol->to && ol->to->hide==0) ol->to->flag |= OOPS_DOSELECT;
					ol= ol->next;
				}
			}
		}
		oops= oops->next;
	}
	
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->flag & OOPS_DOSELECT) {
				oops->flag |= SELECT;
				oops->flag &= ~OOPS_DOSELECT;
			}
		}
		oops= oops->next;
	}
	
	oops_to_select_objects();	/* also redraw */
	
}

void select_backlinked_oops(void)
{
	Oops *oops;
	OopsLink *ol;
	
	if(G.soops==0) return;	

	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if( (oops->flag & SELECT)==0) {
				ol= oops->link.first;
				while(ol) {
					if(ol->to && ol->to->hide==0) {
						if(ol->to->flag & SELECT) oops->flag |= OOPS_DOSELECT;
					}
					ol= ol->next;
				}
			}
		}
		oops= oops->next;
	}
	
	oops= G.soops->oops.first;
	while(oops) {
		if(oops->hide==0) {	
			if(oops->flag & OOPS_DOSELECT) {
				oops->flag |= SELECT;
				oops->flag &= ~OOPS_DOSELECT;
			}
		}
		oops= oops->next;
	}
	
	oops_to_select_objects();	/* also redraw */
	
}


void clever_numbuts_oops()
{
	Oops *oops;
	Object *ob;
	char str1[10];
	static char naam[256];
	static char naam2[256];
	static short doit;
	int len;

	if(G.soops->lockpoin) {
		oops= G.soops->lockpoin;
		ob = (Object *)oops->id;
		if(oops->type==ID_LI) strcpy(naam, ((Library *)oops->id)->name);
		else strcpy(naam, oops->id->name);

		strcpy(naam2, naam+2);
		str1[0]= oops->id->name[0];
		str1[1]= oops->id->name[1];
		str1[2]= ':';
		str1[3]= 0;
		if(strcmp(str1, "SC:")==0) strcpy(str1, "SCE:");
		else if(strcmp(str1, "SR:")==0) strcpy(str1, "SCR:");
		
//		if( GS(id->name)==ID_IP) len= 110;
//		else len= 120;
		len = 110;

		add_numbut(0, TEX, str1, 0, len, naam2, "Rename Object");
		if((oops->type==ID_OB || oops->type==ID_ME) && ob->type != OB_EMPTY) {
	//		add_numbut(1, TEX, str1, 0, len, naam2, "Name Object");
			add_numbut(1, TOG|SHO, "Rename Linked Data", 0, 0, &doit, "Rename corresponding Datablock as well");
			do_clever_numbuts("Rename Datablock", 2, REDRAW); 
		} else {
			do_clever_numbuts("Rename Datablock", 1, REDRAW); 
		}

		rename_id((ID *)oops->id, naam2);
	}
}
