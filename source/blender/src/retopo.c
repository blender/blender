/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the Retopo tools
 *
 * BIF_retopo.h
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BDR_editobject.h"

#include "BIF_editmesh.h"
#include "BIF_editmode_undo.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_mesh.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BSE_drawview.h"
#include "BSE_edit.h"
#include "BSE_view.h"

#include "editmesh.h"
#include "mydevice.h"

#ifdef WIN32
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct RetopoPaintHit {
	struct RetopoPaintHit *next, *prev;
	RetopoPaintPoint *intersection;
	short index;
	float where;
} RetopoPaintHit;

void retopo_do_2d(View3D *v3d, double proj[2], float *v, char adj);
void retopo_paint_debug_print(RetopoPaintData *rpd);

/* Painting */
RetopoPaintData *get_retopo_paint_data()
{
	if(!retopo_mesh_paint_check()) return NULL;
	if(!G.editMesh) return NULL;
	return G.editMesh->retopo_paint_data;
}

char retopo_mesh_paint_check()
{
	return retopo_mesh_check() && G.scene->toolsettings->retopo_mode & RETOPO_PAINT;
}

void retopo_free_paint_data(RetopoPaintData *rpd)
{
	if(rpd) {
		RetopoPaintLine *l;
		for(l= rpd->lines.first; l; l= l->next) {
			BLI_freelistN(&l->points);
			BLI_freelistN(&l->hitlist);
		}
		BLI_freelistN(&rpd->lines);
		
		BLI_freelistN(&rpd->intersections);

		MEM_freeN(rpd);
	}
}

void retopo_free_paint()
{
	retopo_free_paint_data(G.editMesh->retopo_paint_data);
	G.editMesh->retopo_paint_data= NULL;
}

char line_intersection_2d(const vec2s *a, const vec2s *b, const vec2s *c, const vec2s *d, vec2s *out,
			  float *r, float *s)
{
	float den;
	*r= (a->y - c->y) * (d->x - c->x) - (a->x - c->x) * (d->y - c->y);
	*s= (a->y - c->y) * (b->x - a->x) - (a->x - c->x) * (b->y - a->y);
	den= (b->x - a->x) * (d->y - c->y) - (b->y - a->y) * (d->x - c->x);

	if((a->x==b->x && a->y==b->y) || (c->x==d->x && c->y==d->y)) return 0;

	if(!den) return 0;

	*r/= den;
	*s/= den;

	if(*s<0 || *s>=1 || *r<0 || *r>=1) return 0;

	out->x= a->x + *r*(b->x - a->x);
	out->y= a->y + *r*(b->y - a->y);
	return 1;
}

void retopo_paint_add_line_hit(RetopoPaintLine *l, RetopoPaintPoint *p, RetopoPaintPoint *intersection, float w)
{
	RetopoPaintHit *prev, *hit= MEM_callocN(sizeof(RetopoPaintHit),"RetopoPaintHit");
	
	hit->intersection= intersection;
	hit->index= p->index;
	hit->where= w;

	prev= l->hitlist.first;
	if(!prev) {
		BLI_addtail(&l->hitlist,hit);
	}
	else if(prev->index>hit->index) {
		BLI_addhead(&l->hitlist,hit);
	}
	else {
		/* Move forward until we hit the next highest index */
		while(prev->next) {
			if(prev->next->index > hit->index) break;
			prev= prev->next;
		}
		/* Move backward until we hit the next lowest where */
		while(prev->prev && prev->prev->index==prev->index &&
		      prev->where > hit->where)
			prev=prev->prev;
		BLI_insertlink(&l->hitlist,prev,hit);
	}
	
	/* Removed duplicate intersections */
	if(hit->prev && hit->prev->intersection==hit->intersection) {
		BLI_freelinkN(&l->hitlist,hit);
	}
}

char retopo_paint_add_intersection(RetopoPaintData *rpd, RetopoPaintLine *l1, RetopoPaintPoint *p1,
				   RetopoPaintLine *l2, RetopoPaintPoint *p2, vec2s *out, float r, float s)
{
	RetopoPaintPoint *p, *hit;
	char found= 0;

	for(p=rpd->intersections.first; p; p= p->next) {
		if(sqrt(pow(p->loc.x-out->x,2)+pow(p->loc.y-out->y,2))<7) {
			found= 1;
			break;
		}
	}

	if(!found) {
		hit= MEM_callocN(sizeof(RetopoPaintPoint),"Retopo paint intersection");
		hit->loc.x= out->x;
		hit->loc.y= out->y;
		BLI_addtail(&rpd->intersections,hit);
	} else {
		hit= p;
	}

	retopo_paint_add_line_hit(l1,p1,hit,r);
	retopo_paint_add_line_hit(l2,p2,hit,s);

	return !found;
}


/* Returns 1 if a new intersection was added */
char do_line_intersection(RetopoPaintData *rpd, RetopoPaintLine *l1, RetopoPaintPoint *p1,
			  RetopoPaintLine *l2, RetopoPaintPoint *p2)
{
	vec2s out;
	float r,s;
	if(line_intersection_2d(&p1->loc, &p1->next->loc,
				&p2->loc, &p2->next->loc,
				&out,&r,&s)) {
		if(retopo_paint_add_intersection(rpd,l1,p1,l2,p2,&out,r,s))
			return 1;
	}
	return 0;
}

typedef struct FaceNode {
	struct FaceNode *next, *prev;
	MFace f;
} FaceNode;

char faces_equal(EditFace *f1, EditFace *f2)
{
	return editface_containsVert(f2,f1->v1) &&
	       editface_containsVert(f2,f1->v2) &&
	       editface_containsVert(f2,f1->v3) &&
	       (f1->v4 ? editface_containsVert(f2,f1->v4) : 1);
}

EditFace *addfaceif(EditMesh *em, EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4)
{
	EditFace *efa;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(editface_containsVert(efa,v1) &&
		   editface_containsVert(efa,v2) &&
		   editface_containsVert(efa,v3) &&
		   (v4 ? editface_containsVert(efa,v4) : 1))
			return NULL;
	}

	return addfacelist(v1,v2,v3,v4,NULL,NULL);
}

void retopo_paint_apply()
{
	RetopoPaintData *rpd= G.editMesh->retopo_paint_data;
	EditVert *eve;

	if(rpd) {
		RetopoPaintLine *l1, *l2;
		RetopoPaintPoint *p1, *p2;
		unsigned hitcount= 0;
		unsigned i;
		RetopoPaintHit *h;
		float hitco[3];
		
		/* Find intersections */
		BLI_freelistN(&rpd->intersections);
		for(l1= rpd->lines.first; l1; l1= l1->next) {
			for(l2= rpd->lines.first; l2; l2= l2->next) {
				if(l1!=l2) {
					for(p1= l1->points.first; p1 && p1!=l1->points.last; p1= p1->next) {
						for(p2= l2->points.first; p2 && p2!=l2->points.last; p2= p2->next) {
							if(p1!=p2) {
								if(do_line_intersection(rpd,l1,p1,l2,p2))
									++hitcount;
							}
						}
					}
				}
			}
		}

		/*topoPaintHit *hit;
		l1= rpd->lines.first;
		for(hit= l1->hitlist.first; hit; hit= hit->next) {
			printf("\nhit(%p,%d) ",hit->intersection,hit->index);
		}
		fflush(stdout);*/

		/* Deselect */
		for(eve= G.editMesh->verts.first; eve; eve= eve->next)
			eve->f &= ~SELECT;
		EM_deselect_flush();

		for(i=0; i<hitcount; ++i) {
			RetopoPaintPoint *intersection= BLI_findlink(&rpd->intersections,i);
			double proj[2] = {intersection->loc.x, intersection->loc.y};
			retopo_do_2d(rpd->paint_v3d, proj, hitco, 1);
			intersection->eve= addvertlist(hitco, NULL);
			intersection->eve->f= SELECT;
		}
		
		for(l1= rpd->lines.first; l1; l1= l1->next) {
			unsigned etcount= BLI_countlist(&l1->hitlist);
			if(etcount>=2) {
				for(h= l1->hitlist.first; (h && h->next); h= h->next)
					addedgelist(h->intersection->eve,h->next->intersection->eve,NULL);
				if(etcount>=3 && l1->cyclic)
					addedgelist(((RetopoPaintHit*)l1->hitlist.first)->intersection->eve,
						    ((RetopoPaintHit*)l1->hitlist.last)->intersection->eve, NULL);
			}
		}
		
		addfaces_from_edgenet();
	}

	retopo_free_paint();
}

void add_rppoint(RetopoPaintLine *l, short x, short y)
{
	RetopoPaintPoint *p= MEM_callocN(sizeof(RetopoPaintPoint),"RetopoPaintPoint");
	double proj[2];
	p->loc.x= x;
	p->loc.y= y;
	BLI_addtail(&l->points,p);
	p->index= p->prev?p->prev->index+1:0;

	proj[0] = p->loc.x;
	proj[1] = p->loc.y;

	retopo_do_2d(G.editMesh->retopo_paint_data->paint_v3d, proj, p->co, 1);
}
RetopoPaintLine *add_rpline(RetopoPaintData *rpd)
{
	RetopoPaintLine *l= MEM_callocN(sizeof(RetopoPaintLine),"RetopoPaintLine");
	BLI_addtail(&rpd->lines,l);
	return l;
}

void retopo_paint_toggle_cyclic(RetopoPaintLine *l)
{
	if(l==NULL)
		return;
	if(!l->cyclic) {
		RetopoPaintPoint *pf= l->points.first;

		if(pf) {
			add_rppoint(l, pf->loc.x, pf->loc.y);
			l->cyclic= l->points.last;
		}
	} else {
		BLI_freelinkN(&l->points,l->cyclic);
		l->cyclic= NULL;
	}
}

void retopo_paint_add_line(RetopoPaintData *rpd, short mouse[2])
{
	RetopoPaintLine *l= add_rpline(rpd);
	float range[2]= {mouse[0]-rpd->sloc[0],mouse[1]-rpd->sloc[1]};
	int i;

	/* Add initial point */
	add_rppoint(l,rpd->sloc[0],rpd->sloc[1]);
	for(i=0; i<G.scene->toolsettings->line_div; ++i) {
		const float mul= (i+1.0f) / G.scene->toolsettings->line_div;
		add_rppoint(l,rpd->sloc[0] + range[0]*mul,rpd->sloc[1] + range[1]*mul);
	}

	allqueue(REDRAWVIEW3D,0);
}

void retopo_paint_add_ellipse(RetopoPaintData *rpd, short mouse[2])
{
	int i;

	add_rpline(rpd);
	for (i=0; i<G.scene->toolsettings->ellipse_div; i++) {
		float t= (float) i / G.scene->toolsettings->ellipse_div;
		float cur= t*(M_PI*2);
		
		float w= abs(mouse[0]-rpd->sloc[0]);
		float h= abs(mouse[1]-rpd->sloc[1]);

		add_rppoint(rpd->lines.last,cos(cur)*w+rpd->sloc[0],sin(cur)*h+rpd->sloc[1]);
	}

	retopo_paint_toggle_cyclic(rpd->lines.last);

	allqueue(REDRAWVIEW3D,0);
}

void retopo_end_okee()
{
	if(okee("Apply retopo paint?"))
		retopo_paint_apply();
	else
		retopo_free_paint();
	G.scene->toolsettings->retopo_mode &= ~RETOPO_PAINT;
}

void retopo_paint_toggle(void *a, void *b)
{
	/* Note that these operations are reversed because mode bit has already been set! */
	if(retopo_mesh_paint_check()) { /* Activate retopo paint */
		RetopoPaintData *rpd= MEM_callocN(sizeof(RetopoPaintData),"RetopoPaintData");
		
		G.editMesh->retopo_paint_data= rpd;
		G.scene->toolsettings->retopo_paint_tool= RETOPO_PEN;
		rpd->seldist= 15;
		rpd->nearest.line= NULL;
		G.scene->toolsettings->line_div= 25;
		G.scene->toolsettings->ellipse_div= 25;
		G.scene->toolsettings->retopo_hotspot= 1;
	} else retopo_end_okee();

	BIF_undo_push("Retopo toggle");

	allqueue(REDRAWVIEW3D, 1);
}

void retopo_paint_view_update(struct View3D *v3d)
{
	RetopoPaintData *rpd= get_retopo_paint_data();

	if(rpd && rpd->paint_v3d==v3d) {
		RetopoPaintLine *l;
		RetopoPaintPoint *p;
		double ux, uy, uz;
		
		for(l= rpd->lines.first; l; l= l->next) {
			for(p= l->points.first; p; p= p->next) {
				gluProject(p->co[0],p->co[1],p->co[2], v3d->retopo_view_data->mats.modelview,
					   v3d->retopo_view_data->mats.projection,
					   (GLint *)v3d->retopo_view_data->mats.viewport, &ux, &uy, &uz);
				p->loc.x= ux;
				p->loc.y= uy;
			}
		}
	}
}

void retopo_force_update()
{
	RetopoPaintData *rpd= get_retopo_paint_data();
	
	if(rpd) {
		View3D *vd= rpd->paint_v3d;
		
		if(vd) {
			if(vd->depths) vd->depths->damaged= 1;
			retopo_queue_updates(vd);
			if(retopo_mesh_paint_check() && vd->retopo_view_data)
				allqueue(REDRAWVIEW3D, 0);
		}
	}
}

/* Returns 1 if event should be processed by caller, 0 otherwise */
char retopo_paint(const unsigned short event)
{
	RetopoPaintData *rpd= get_retopo_paint_data();

	if(!event) return 1;
	if(rpd) {
		RetopoPaintLine *l;
		short mouse[2];
		char lbut= get_mbut() & (U.flag & USER_LMOUSESELECT ? R_MOUSE : L_MOUSE);
		
		if(rpd->paint_v3d && rpd->paint_v3d!=G.vd) return 1;
	
		getmouseco_areawin(mouse);

		if(rpd->in_drag && !lbut) { /* End drag */
			rpd->in_drag= 0;

			switch(G.scene->toolsettings->retopo_paint_tool) {
			case RETOPO_PEN:
				break;
			case RETOPO_LINE:
				retopo_paint_add_line(rpd, mouse);
				break;
			case RETOPO_ELLIPSE:
				retopo_paint_add_ellipse(rpd, mouse);
				break;
			}
			BIF_undo_push("Retopo paint");
		}

		switch(event) {
		case MOUSEX:
		case MOUSEY:
			switch(G.scene->toolsettings->retopo_paint_tool) {
			case RETOPO_PEN:
				if(rpd->in_drag && rpd->lines.last) {
					l= rpd->lines.last;

					if(((RetopoPaintPoint*)l->points.last)->loc.x != mouse[0] ||
					   ((RetopoPaintPoint*)l->points.last)->loc.y != mouse[1]) {
						add_rppoint(l,mouse[0],mouse[1]);
					}
					rpd->nearest.line= NULL;
					
					break;
				} else if(G.scene->toolsettings->retopo_hotspot) { /* Find nearest endpoint */
					float sdist;
					RetopoPaintLine *l= rpd->lines.first;
					RetopoPaintSel n= {NULL,NULL,l,1};
					sdist= rpd->seldist + 10;
					for(l= rpd->lines.first; l; l= l->next) {
						float tdist;
						RetopoPaintPoint *p1= l->points.first, *p2= l->points.last;
						
						tdist= sqrt(pow(mouse[0] - p1->loc.x,2)+pow(mouse[1] - p1->loc.y,2));
						if(tdist < sdist && tdist < rpd->seldist) {
							sdist= tdist;
							n.line= l;
							n.first= 1;
						} else {
							tdist= sqrt(pow(mouse[0] - p2->loc.x,2)+pow(mouse[1] - p2->loc.y,2));
							if(tdist < sdist && tdist < rpd->seldist) {
								sdist= tdist;
								n.line= l;
								n.first= 0;
							}
						}
					}
					
					if(sdist < rpd->seldist)
						rpd->nearest= n;
					else rpd->nearest.line= NULL;
				}
				break;
			case RETOPO_LINE:
				break;
			case RETOPO_ELLIPSE:
				break;
			}
			allqueue(REDRAWVIEW3D,0);
			break;
		case RETKEY:
		case PADENTER:
			retopo_paint_apply();
		case ESCKEY:
			G.scene->toolsettings->retopo_mode&= ~RETOPO_PAINT;
			retopo_free_paint();

			BIF_undo_push("Retopo toggle");
		
			allqueue(REDRAWVIEW3D, 1);
			allqueue(REDRAWBUTSEDIT, 0);
			break;
		case CKEY:
			retopo_paint_toggle_cyclic(rpd->lines.last);
			BIF_undo_push("Retopo toggle cyclic");
			allqueue(REDRAWVIEW3D, 0);
			break;
		case EKEY:
			G.scene->toolsettings->retopo_paint_tool= RETOPO_ELLIPSE;
			allqueue(REDRAWVIEW3D, 1);
			break;
		case HKEY:
			G.scene->toolsettings->retopo_hotspot= !G.scene->toolsettings->retopo_hotspot;
			allqueue(REDRAWVIEW3D, 1);
			break;
		case LKEY:
			G.scene->toolsettings->retopo_paint_tool= RETOPO_LINE;
			allqueue(REDRAWVIEW3D, 1);
			break;
		case PKEY:
			G.scene->toolsettings->retopo_paint_tool= RETOPO_PEN;
			allqueue(REDRAWVIEW3D, 1);
			break;
		case XKEY:
		case DELKEY:
			l= rpd->lines.last;
			if(l) {
				BLI_freelistN(&l->points);
				BLI_freelistN(&l->hitlist);
				BLI_freelinkN(&rpd->lines, l);
				if(rpd->nearest.line == l)
					rpd->nearest.line= NULL;
				BIF_undo_push("Erase line");
				allqueue(REDRAWVIEW3D, 0);
			}
			break;
		case LEFTMOUSE:
			if(!rpd->in_drag) { /* Start new drag */
				rpd->in_drag= 1;
				
				if(!rpd->paint_v3d)
					rpd->paint_v3d= G.vd;
				
				/* Location of mouse down */
				rpd->sloc[0]= mouse[0];
				rpd->sloc[1]= mouse[1];
				
				switch(G.scene->toolsettings->retopo_paint_tool) {
				case RETOPO_PEN:
					if(rpd->nearest.line) {
						RetopoPaintPoint *p, *pt;
						int i;
						
						BLI_remlink(&rpd->lines,rpd->nearest.line);
						BLI_addtail(&rpd->lines,rpd->nearest.line);
						
						/* Check if we need to reverse the line */
						if(rpd->nearest.first) {
							for(p= rpd->nearest.line->points.first; p; p= p->prev) {
								pt= p->prev;
								p->prev= p->next;
								p->next= pt;
							}
							pt= rpd->nearest.line->points.first;
							rpd->nearest.line->points.first= rpd->nearest.line->points.last;
							rpd->nearest.line->points.last= pt;
							
							/* Reverse indices */
							i= 0;
							for(p= rpd->nearest.line->points.first; p; p= p->next)
								p->index= i++;
						}
					} else {
						add_rpline(rpd);
						add_rppoint(rpd->lines.last,mouse[0],mouse[1]);
					}
					break;
				case RETOPO_LINE:
					break;
				case RETOPO_ELLIPSE:
					break;
				}
				allqueue(REDRAWVIEW3D, 0);
			}
			break;
		case MIDDLEMOUSE:
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
		case PAD0: case PAD1: case PAD2: case PAD3: case PAD4:
		case PAD5: case PAD6: case PAD7: case PAD8: case PAD9:
		case PADMINUS: case PADPLUSKEY:
			return 1;
		}
		return 0;
	} else return 1;
}
void retopo_draw_paint_lines()
{
	RetopoPaintData *rpd= get_retopo_paint_data();

	if(rpd && rpd->paint_v3d==G.vd) {
		RetopoPaintLine *l;
		RetopoPaintPoint *p;

		glColor3f(0,0,0);
		glLineWidth(2);

		/* Draw existing lines */
		for(l= rpd->lines.first; l; l= l->next) {
			if(l==rpd->lines.last)
				glColor3f(0.3,0,0);
			glBegin(l->cyclic?GL_LINE_LOOP:GL_LINE_STRIP);
			for(p= l->points.first; p; p= p->next) {
				glVertex2s(p->loc.x,p->loc.y);
			}
			glEnd();
		}

		/* Draw ellipse */
		if(G.scene->toolsettings->retopo_paint_tool==RETOPO_ELLIPSE && rpd->in_drag) {
			short mouse[2];
			getmouseco_areawin(mouse);
		
			setlinestyle(3);
			fdrawXORellipse(rpd->sloc[0],rpd->sloc[1],abs(mouse[0]-rpd->sloc[0]),abs(mouse[1]-rpd->sloc[1]));
			setlinestyle(0);
		}
		else if(G.scene->toolsettings->retopo_paint_tool==RETOPO_LINE && rpd->in_drag) {
			short mouse[2];
			getmouseco_areawin(mouse);

			setlinestyle(3);
			sdrawXORline(rpd->sloc[0],rpd->sloc[1],mouse[0],mouse[1]);
			setlinestyle(0);
		}
		else if(rpd->nearest.line) { /* Draw selection */
			RetopoPaintPoint *p= rpd->nearest.first ? rpd->nearest.line->points.first :
				rpd->nearest.line->points.last;
			if(p)
				fdrawXORcirc(p->loc.x, p->loc.y, rpd->seldist);
		}

		glLineWidth(1);
	}
}

RetopoPaintData *retopo_paint_data_copy(RetopoPaintData *rpd)
{
	RetopoPaintData *copy;
	RetopoPaintLine *l, *lcp;
	RetopoPaintPoint *p, *pcp;

	if(!rpd) return NULL;

	copy= MEM_mallocN(sizeof(RetopoPaintData),"RetopoPaintDataCopy");

	memcpy(copy,rpd,sizeof(RetopoPaintData));
	copy->lines.first= copy->lines.last= NULL;
	for(l= rpd->lines.first; l; l= l->next) {
		lcp= MEM_mallocN(sizeof(RetopoPaintLine),"RetopoPaintLineCopy");
		memcpy(lcp,l,sizeof(RetopoPaintLine));
		BLI_addtail(&copy->lines,lcp);
		
		lcp->hitlist.first= lcp->hitlist.last= NULL;
		lcp->points.first= lcp->points.last= NULL;
		for(p= l->points.first; p; p= p->next) {
			pcp= MEM_mallocN(sizeof(RetopoPaintPoint),"RetopoPaintPointCopy");
			memcpy(pcp,p,sizeof(RetopoPaintPoint));
			BLI_addtail(&lcp->points,pcp);
		}
	}

	copy->intersections.first= copy->intersections.last= NULL;

	return copy;
}

char retopo_mesh_check()
{
	return G.obedit && G.obedit->type==OB_MESH && (G.scene->toolsettings->retopo_mode & RETOPO);
}
char retopo_curve_check()
{
	return G.obedit && (G.obedit->type==OB_CURVE ||
		            G.obedit->type==OB_SURF) && (((Curve*)G.obedit->data)->flag & CU_RETOPO);
}

void retopo_toggle(void *j1,void *j2)
{
	if(retopo_mesh_check() || retopo_curve_check()) {
		if(G.vd->depths) G.vd->depths->damaged= 1;
		retopo_queue_updates(G.vd);
	} else {
		if(G.editMesh && G.scene->toolsettings->retopo_mode & RETOPO_PAINT)
			retopo_end_okee();
	}

	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

void retopo_do_2d(View3D *v3d, double proj[2], float *v, char adj)
{
	/* Check to make sure vert is visible in window */
	if(proj[0]>0 && proj[1]>0 && proj[0] < v3d->depths->w && proj[1] < v3d->depths->h) {
		float depth= v3d->depths->depths[((int)proj[1])*v3d->depths->w+((int)proj[0])];
		double px, py, pz;
	
		/* Don't modify the point if it'll be mapped to the background */
		if(depth==v3d->depths->depth_range[1]) {
			if(adj) {
				/* Find the depth of (0,0,0); */
				gluProject(0,0,0,v3d->retopo_view_data->mats.modelview,
					   v3d->retopo_view_data->mats.projection,
					   (GLint *)v3d->retopo_view_data->mats.viewport,&px,&py,&pz);
				depth= pz;
			}
			else return;
		}
		
		/* Find 3D location with new depth (unproject) */
		gluUnProject(proj[0],proj[1],depth,v3d->retopo_view_data->mats.modelview,
			     v3d->retopo_view_data->mats.projection,
			     (GLint *)v3d->retopo_view_data->mats.viewport,&px,&py,&pz);
		
		v[0]= px;
		v[1]= py;
		v[2]= pz;
	}
}

void retopo_do_vert(View3D *v3d, float *v)
{
	double proj[3];

	/* Find 2D location (project) */
	gluProject(v[0],v[1],v[2],v3d->retopo_view_data->mats.modelview,v3d->retopo_view_data->mats.projection,
		   (GLint *)v3d->retopo_view_data->mats.viewport,&proj[0],&proj[1],&proj[2]);
	
	retopo_do_2d(v3d,proj,v,0);
}

void retopo_do_all()
{
	RetopoViewData *rvd= G.vd->retopo_view_data;
	if(retopo_mesh_check()) {
		if(rvd) {
			EditMesh *em= G.editMesh;
			EditVert *eve;
			
			/* Apply retopo to all selected vertices */
			eve= em->verts.first;
			while(eve) {
				if(eve->f & SELECT)
					retopo_do_vert(G.vd,eve->co);
				eve= eve->next;
			}
			
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
	else if(retopo_curve_check()) {
		if(rvd) {
			extern ListBase editNurb;
			Nurb *nu;
			BPoint *bp;
			int i, j;

			for(nu= editNurb.first; nu; nu= nu->next)
			{
				if(nu->type & CU_2D) {
					/* Can't wrap a 2D curve onto a 3D surface */
				}
				else if(nu->type & CU_BEZIER) {
					for(i=0; i<nu->pntsu; ++i) {
						if(nu->bezt[i].f1 & SELECT)
							retopo_do_vert(G.vd, nu->bezt[i].vec[0]);
						if(nu->bezt[i].f2 & SELECT)
							retopo_do_vert(G.vd, nu->bezt[i].vec[1]);
						if(nu->bezt[i].f3 & SELECT)
							retopo_do_vert(G.vd, nu->bezt[i].vec[2]);
					}
				}
				else {
					bp= nu->bp;
					for(i=0; i<nu->pntsv; ++i) {
						for(j=0; j<nu->pntsu; ++j, ++bp) {
							if(bp->f1 & SELECT)
								retopo_do_vert(G.vd,bp->vec);
						}
					}
				}
				
				testhandlesNurb(nu);
			}
			
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);			
		}
	}
}

void retopo_do_all_cb(void *j1, void *j2)
{
	/* This is called from editbuttons, so user needs to specify view */
	if(!select_area(SPACE_VIEW3D)) return;

	if(G.vd->drawtype == OB_WIRE) {
		error("Cannot apply retopo in wireframe mode");
		return;
	}

	retopo_do_all();
	BIF_undo_push("Retopo all");
}

void retopo_queue_updates(View3D *v3d)
{
	if(retopo_mesh_check() || retopo_curve_check()) {
		if(!v3d->retopo_view_data)
			v3d->retopo_view_data= MEM_callocN(sizeof(RetopoViewData),"RetopoViewData");
		
		v3d->retopo_view_data->queue_matrix_update= 1;
		
		allqueue(REDRAWVIEW3D, 0);
	}
}

void retopo_matrix_update(View3D *v3d)
{
	RetopoPaintData *rpd= get_retopo_paint_data();
	if((retopo_mesh_check() || retopo_curve_check()) && (!rpd || rpd->paint_v3d==v3d)) {
		RetopoViewData *rvd= v3d->retopo_view_data;
		if(!rvd) {
			rvd= MEM_callocN(sizeof(RetopoViewData),"RetopoViewData");
			v3d->retopo_view_data= rvd;
			rvd->queue_matrix_update= 1;
		}
		if(rvd && rvd->queue_matrix_update) {
			bgl_get_mats(&rvd->mats);

			rvd->queue_matrix_update= 0;
		}
	}
}

void retopo_free_view_data(View3D *v3d)
{
	if(v3d->retopo_view_data) {
		MEM_freeN(v3d->retopo_view_data);
		v3d->retopo_view_data= NULL;
	}
}

void retopo_paint_debug_print(RetopoPaintData *rpd)
{
	RetopoPaintLine *l;
	RetopoPaintPoint *p;

	for(l= rpd->lines.first; l; l= l->next) {
		printf("Line:\n");
		for(p= l->points.first; p; p= p->next) {
			printf("   Point(%d: %d,%d)\n",p->index,p->loc.x,p->loc.y);
		}
	}

	fflush(stdout);
}
