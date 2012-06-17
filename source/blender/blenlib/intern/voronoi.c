/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
 * Fortune's algorithm implemented using explanation and some code snippets from
 * http://blog.ivank.net/fortunes-algorithm-and-implementation.html
 */

/** \file blender/blenkernel/intern/tracking.c
 *  \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_voronoi.h"
#include "BLI_utildefines.h"

#define VORONOI_EPS 1e-3

enum {
	voronoiEventType_Site = 0,
	voronoiEventType_Circle = 1
} voronoiEventType;

typedef struct VoronoiEvent {
	struct VoronoiEvent *next, *prev;

	int type;       /* type of event (site or circle) */
	float site[2];  /* site for which event was generated */

	struct VoronoiParabola *parabola;   /* parabola for which event was generated */
} VoronoiEvent;

typedef struct VoronoiParabola {
	struct VoronoiParabola *left, *right, *parent;
	VoronoiEvent *event;
	int is_leaf;
	float site[2];
	VoronoiEdge *edge;
} VoronoiParabola;

typedef struct VoronoiProcess {
	ListBase queue, edges;
	VoronoiParabola *root;
	int width, height;
	float current_y;
} VoronoiProcess;

/* event */

static void voronoi_insertEvent(VoronoiProcess *process, VoronoiEvent *event)
{
	VoronoiEvent *current_event = process->queue.first;

	while (current_event) {
		if (current_event->site[1] < event->site[1]) {
			break;
		}
		if (current_event->site[1] == event->site[1]) {
			event->site[1] -= VORONOI_EPS;
		}

		current_event = current_event->next;
	}

	BLI_insertlinkbefore(&process->queue, current_event, event);
}

/* edge */
static VoronoiEdge *voronoiEdge_new(float start[2], float left[2], float right[2])
{
	VoronoiEdge *edge = MEM_callocN(sizeof(VoronoiEdge), "voronoi edge");

	copy_v2_v2(edge->start, start);
	copy_v2_v2(edge->left, left);
	copy_v2_v2(edge->right, right);

	edge->neighbour = NULL;
	edge->end[0] = 0;
	edge->end[1] = 0;

	edge->f = (right[0] - left[0]) / (left[1] - right[1]);
	edge->g = start[1] - edge->f * start[0];

	edge->direction[0] = right[1] - left[1];
	edge->direction[1] = -(right[0] - left[0]);

	return edge;
}

/* parabola */

static VoronoiParabola *voronoiParabola_new(void)
{
	VoronoiParabola *parabola = MEM_callocN(sizeof(VoronoiParabola), "voronoi parabola");

	parabola->is_leaf = FALSE;
	parabola->event = NULL;
	parabola->edge = NULL;
	parabola->parent = 0;

	return parabola;
}

static VoronoiParabola *voronoiParabola_newSite(float site[2])
{
	VoronoiParabola *parabola = MEM_callocN(sizeof(VoronoiParabola), "voronoi parabola site");

	copy_v2_v2(parabola->site, site);
	parabola->is_leaf = TRUE;
	parabola->event = NULL;
	parabola->edge = NULL;
	parabola->parent = 0;

	return parabola;
}

/* returns the closest leave which is on the left of current node */
static VoronoiParabola *voronoiParabola_getLeftChild(VoronoiParabola *parabola)
{
	VoronoiParabola *current_parabola;

	if (!parabola)
		return NULL;

	current_parabola = parabola->left;
	while (!current_parabola->is_leaf) {
		current_parabola = current_parabola->right;
	}

	return current_parabola;
}

/* returns the closest leave which is on the right of current node */
static VoronoiParabola *voronoiParabola_getRightChild(VoronoiParabola *parabola)
{
	VoronoiParabola *current_parabola;

	if (!parabola)
		return NULL;

	current_parabola = parabola->right;
	while (!current_parabola->is_leaf) {
		current_parabola = current_parabola->left;
	}

	return current_parabola;
}

/* returns the closest parent which is on the left */
static VoronoiParabola *voronoiParabola_getLeftParent(VoronoiParabola *parabola)
{
	VoronoiParabola *current_par = parabola->parent;
	VoronoiParabola *last_parabola = parabola;

	while (current_par->left == last_parabola) {
		if (!current_par->parent)
			return NULL;

		last_parabola = current_par;
		current_par = current_par->parent;
	}

	return current_par;
}

/* returns the closest parent which is on the right */
static VoronoiParabola *voronoiParabola_getRightParent(VoronoiParabola *parabola)
{
	VoronoiParabola *current_parabola = parabola->parent;
	VoronoiParabola *last_parabola = parabola;

	while (current_parabola->right == last_parabola) {
		if (!current_parabola->parent)
			return NULL;

		last_parabola = current_parabola;
		current_parabola = current_parabola->parent;
	}

	return current_parabola;
}

static void voronoiParabola_setLeft(VoronoiParabola *parabola, VoronoiParabola *left)
{
	parabola->left = left;
	left->parent = parabola;
}

static void voronoiParabola_setRight(VoronoiParabola *parabola, VoronoiParabola *right)
{
	parabola->right = right;
	right->parent = parabola;
}

static float voronoi_getY(VoronoiProcess *process, float p[2], float x)
{
	float ly = process->current_y;

	float dp = 2 * (p[1] - ly);
	float a1 = 1 / dp;
	float b1 = -2 * p[0] / dp;
	float c1 = ly + dp / 4 + p[0] * p[0] / dp;

	return a1 * x * x + b1 * x + c1;
}

static float voronoi_getXOfEdge(VoronoiProcess *process, VoronoiParabola *par, float y)
{
	VoronoiParabola *left = voronoiParabola_getLeftChild(par);
	VoronoiParabola *right = voronoiParabola_getRightChild(par);
	float p[2], r[2];
	float dp, a1, b1, c1, a2, b2, c2, a, b, c, disc, ry, x1, x2;
	float ly = process->current_y;

	copy_v2_v2(p, left->site);
	copy_v2_v2(r, right->site);

	dp = 2.0f * (p[1] - y);
	a1 = 1.0f / dp;
	b1 = -2.0f * p[0] / dp;
	c1 = y + dp / 4 + p[0] * p[0] / dp;

	dp = 2.0f * (r[1] - y);
	a2 = 1.0f / dp;
	b2 = -2.0f * r[0] / dp;
	c2 = ly + dp / 4 + r[0] * r[0] / dp;

	a = a1 - a2;
	b = b1 - b2;
	c = c1 - c2;

	disc = b * b - 4 * a * c;
	x1 = (-b + sqrtf(disc)) / (2 * a);
	x2 = (-b - sqrtf(disc)) / (2 * a);

	if (p[1] < r[1])
		ry = MAX2(x1, x2);
	else
		ry = MIN2(x1, x2);

	return ry;
}

static VoronoiParabola *voronoi_getParabolaByX(VoronoiProcess *process, float xx)
{
	VoronoiParabola *par = process->root;
	float x = 0.0f;
	float ly = process->current_y;

	while (!par->is_leaf) {
		x = voronoi_getXOfEdge(process, par, ly);

		if (x > xx)
			par = par->left;
		else
			par = par->right;
	}

	return par;
}

static int voronoi_getEdgeIntersection(VoronoiEdge *a, VoronoiEdge *b, float p[2])
{
	float x = (b->g - a->g) / (a->f - b->f);
	float y = a->f * x + a->g;

	if ((x - a->start[0]) / a->direction[0] < 0)
		return 0;

	if ((y - a->start[1]) / a->direction[1] < 0)
		return 0;

	if ((x - b->start[0]) / b->direction[0] < 0)
		return 0;

	if ((y - b->start[1]) / b->direction[1] < 0)
		return 0;

	p[0] = x;
	p[1] = y;

	return 1;
}

static void voronoi_checkCircle(VoronoiProcess *process, VoronoiParabola *b)
{
	VoronoiParabola *lp = voronoiParabola_getLeftParent(b);
	VoronoiParabola *rp = voronoiParabola_getRightParent(b);

	VoronoiParabola *a  = voronoiParabola_getLeftChild(lp);
	VoronoiParabola *c  = voronoiParabola_getRightChild(rp);

	VoronoiEvent *event;

	float ly = process->current_y;
	float s[2], dx, dy, d;

	if (!a || !c || len_squared_v2v2(a->site, c->site) < VORONOI_EPS)
		return;

	if (!voronoi_getEdgeIntersection(lp->edge, rp->edge, s))
		return;

	dx = a->site[0] - s[0];
	dy = a->site[1] - s[1];

	d = sqrtf((dx * dx) + (dy * dy));

	if (s[1] - d >= ly)
		return;

	event = MEM_callocN(sizeof(VoronoiEvent), "voronoi circle event");

	event->type = voronoiEventType_Circle;

	event->site[0] = s[0];
	event->site[1] = s[1] - d;

	b->event = event;
	event->parabola = b;

	voronoi_insertEvent(process, event);
}

static void voronoi_addParabola(VoronoiProcess *process, float site[2])
{
	VoronoiParabola *root = process->root;
	VoronoiParabola *par, *p0, *p1, *p2;
	VoronoiEdge *el, *er;
	float start[2];

	if (!process->root) {
		process->root = voronoiParabola_newSite(site);

		return;
	}

	if (root->is_leaf && root->site[1] - site[1] < 0) {
		float *fp = root->site;
		float s[2];

		root->is_leaf = FALSE;
		voronoiParabola_setLeft(root, voronoiParabola_newSite(fp));
		voronoiParabola_setRight(root, voronoiParabola_newSite(site));

		s[0] = (site[0] + fp[0]) / 2.0f;
		s[1] = process->height;

		if (site[0] > fp[0])
			root->edge = voronoiEdge_new(s, fp, site);
		else
			root->edge = voronoiEdge_new(s, site, fp);

		BLI_addtail(&process->edges, root->edge);

		return;
	}

	par = voronoi_getParabolaByX(process, site[0]);

	if (par->event) {
		BLI_freelinkN(&process->queue, par->event);

		par->event = NULL;
	}

	start[0] = site[0];
	start[1] = voronoi_getY(process, par->site, site[0]);

	el = voronoiEdge_new(start, par->site, site);
	er = voronoiEdge_new(start, site, par->site);

	el->neighbour = er;
	BLI_addtail(&process->edges, el);

	par->edge = er;
	par->is_leaf = FALSE;

	p0 = voronoiParabola_newSite(par->site);
	p1 = voronoiParabola_newSite(site);
	p2 = voronoiParabola_newSite(par->site);

	voronoiParabola_setRight(par, p2);
	voronoiParabola_setLeft(par, voronoiParabola_new());
	par->left->edge = el;

	voronoiParabola_setLeft(par->left, p0);
	voronoiParabola_setRight(par->left, p1);

	voronoi_checkCircle(process, p0);
	voronoi_checkCircle(process, p2);
}

static void voronoi_removeParabola(VoronoiProcess *process, VoronoiEvent *event)
{
	VoronoiParabola *p1 = event->parabola;

	VoronoiParabola *xl = voronoiParabola_getLeftParent(p1);
	VoronoiParabola *xr = voronoiParabola_getRightParent(p1);

	VoronoiParabola *p0 = voronoiParabola_getLeftChild(xl);
	VoronoiParabola *p2 = voronoiParabola_getRightChild(xr);

	VoronoiParabola *higher = NULL, *par, *gparent;

	float p[2];

	if (p0->event) {
		BLI_freelinkN(&process->queue, p0->event);
		p0->event = NULL;
	}

	if (p2->event) {
		BLI_freelinkN(&process->queue, p2->event);
		p2->event = NULL;
	}

	p[0] = event->site[0];
	p[1] = voronoi_getY(process, p1->site, event->site[0]);

	copy_v2_v2(xl->edge->end, p);
	copy_v2_v2(xr->edge->end, p);

	par = p1;
	while (par != process->root) {
		par = par->parent;

		if (par == xl)
			higher = xl;
		if (par == xr)
			higher = xr;
	}

	higher->edge = voronoiEdge_new(p, p0->site, p2->site);
	BLI_addtail(&process->edges, higher->edge);

	gparent = p1->parent->parent;
	if (p1->parent->left == p1) {
		if (gparent->left == p1->parent)
			voronoiParabola_setLeft(gparent, p1->parent->right);
		if (gparent->right == p1->parent)
			voronoiParabola_setRight(gparent, p1->parent->right);
	}
	else {
		if (gparent->left == p1->parent)
			voronoiParabola_setLeft(gparent, p1->parent->left);
		if (gparent->right == p1->parent)
			voronoiParabola_setRight(gparent, p1->parent->left);
	}

	MEM_freeN(p1->parent);
	MEM_freeN(p1);

	voronoi_checkCircle(process, p0);
	voronoi_checkCircle(process, p2);
}

void voronoi_finishEdge(VoronoiProcess *process, VoronoiParabola *parabola)
{
	float mx;

	if (parabola->is_leaf) {
		MEM_freeN(parabola);
		return;
	}

	if (parabola->edge->direction[0] > 0.0f)
		mx = MAX2(process->width, parabola->edge->start[0] + 10);
	else
		mx = MIN2(0.0, parabola->edge->start[0] - 10);

	parabola->edge->end[0] = mx;
	parabola->edge->end[1] = mx * parabola->edge->f + parabola->edge->g;

	voronoi_finishEdge(process, parabola->left);
	voronoi_finishEdge(process, parabola->right);

	MEM_freeN(parabola);
}

void voronoi_clampEdgeVertex(int width, int height, float *coord, float *other_coord)
{
	const float corners[4][2] = {{0.0f, 0.0f},
								 {width - 1, 0.0f},
								 {width - 1, height - 1},
								 {0.0f, height - 1}};
	int i;

	if (IN_RANGE_INCL(coord[0], 0, width - 1) && IN_RANGE_INCL(coord[1], 0, height - 1)) {
		return;
	}

	for (i = 0; i < 4; i++) {
		float v1[2], v2[2];
		float p[2];

		copy_v2_v2(v1, corners[i]);

		if (i == 3)
			copy_v2_v2(v2, corners[0]);
		else
			copy_v2_v2(v2, corners[i + 1]);

		if (isect_seg_seg_v2_point(v1, v2, coord, other_coord, p) == 1) {
			if (i == 0 && coord[1] > p[1])
				continue;
			if (i == 1 && coord[0] < p[0])
				continue;
			if (i == 2 && coord[1] < p[1])
				continue;
			if (i == 3 && coord[0] > p[0])
				continue;

			copy_v2_v2(coord, p);
		}
	}
}

void voronoi_clampEdges(ListBase *edges, int width, int height, ListBase *clamped_edges)
{
	VoronoiEdge *edge;

	edge = edges->first;
	while (edge) {
		VoronoiEdge *new_edge = MEM_callocN(sizeof(VoronoiEdge), "clamped edge");

		*new_edge = *edge;
		BLI_addtail(clamped_edges, new_edge);

		voronoi_clampEdgeVertex(width, height, new_edge->start, new_edge->end);
		voronoi_clampEdgeVertex(width, height, new_edge->end, new_edge->start);

		edge = edge->next;
	}
}

static int voronoi_getNextSideCoord(ListBase *edges, float coord[2], int dim, int dir, float next_coord[2])
{
	VoronoiEdge *edge = edges->first;
	float distance = FLT_MAX;
	int other_dim = dim ? 0 : 1;

	while (edge) {
		int ok = FALSE;
		float co[2], cur_distance;

		if (fabsf(edge->start[other_dim] - coord[other_dim]) < VORONOI_EPS &&
		    len_squared_v2v2(coord, edge->start) > VORONOI_EPS)
		{
			copy_v2_v2(co, edge->start);
			ok = TRUE;
		}

		if (fabsf(edge->end[other_dim] - coord[other_dim]) < VORONOI_EPS &&
		    len_squared_v2v2(coord, edge->end) > VORONOI_EPS)
		{
			copy_v2_v2(co, edge->end);
			ok = TRUE;
		}

		if (ok) {
			if (dir > 0 && coord[dim] > co[dim]) {
				ok = FALSE;
			}
			else if (dir < 0 && coord[dim] < co[dim]) {
				ok = FALSE;
			}
		}

		if (ok) {
			cur_distance = len_squared_v2v2(coord, co);
			if (cur_distance < distance) {
				copy_v2_v2(next_coord, co);
				distance = cur_distance;
			}
		}

		edge = edge->next;
	}

	return distance < FLT_MAX;
}

static void voronoi_createBoundaryEdges(ListBase *edges, int width, int height)
{
	const float corners[4][2] = {{width - 1, 0.0f},
								 {width - 1, height - 1},
								 {0.0f, height - 1},
								 {0.0f, 0.0f}};
	int i, dim = 0, dir = 1;

	float coord[2] = {0.0f, 0.0f};
	float next_coord[2] = {0.0f, 0.0f};

	for (i = 0; i < 4; i++) {
		while (voronoi_getNextSideCoord(edges, coord, dim, dir, next_coord)) {
			VoronoiEdge *edge = MEM_callocN(sizeof(VoronoiEdge), "boundary edge");

			copy_v2_v2(edge->start, coord);
			copy_v2_v2(edge->end, next_coord);
			BLI_addtail(edges, edge);

			copy_v2_v2(coord, next_coord);
		}

		if (len_squared_v2v2(coord, corners[i]) > VORONOI_EPS) {
			VoronoiEdge *edge = MEM_callocN(sizeof(VoronoiEdge), "boundary edge");

			copy_v2_v2(edge->start, coord);
			copy_v2_v2(edge->end, corners[i]);
			BLI_addtail(edges, edge);
			copy_v2_v2(coord, corners[i]);
		}

		dim = dim ? 0 : 1;
		if (i == 1)
			dir = -1;
	}
}

void BLI_voronoi_compute(const VoronoiSite *sites, int sites_total, int width, int height, ListBase *edges)
{
	VoronoiProcess process;
	VoronoiEdge *edge;
	int i;

	memset(&process, 0, sizeof(VoronoiProcess));

	process.width = width;
	process.height = height;

	for (i = 0; i < sites_total; i++) {
		VoronoiEvent *event = MEM_callocN(sizeof(VoronoiEvent), "voronoi site event");

		event->type = voronoiEventType_Site;
		copy_v2_v2(event->site, sites[i].co);

		voronoi_insertEvent(&process, event);
	}

	while (process.queue.first) {
		VoronoiEvent *event = process.queue.first;

		process.current_y = event->site[1];

		if (event->type == voronoiEventType_Site) {
			voronoi_addParabola(&process, event->site);
		}
		else {
			voronoi_removeParabola(&process, event);
		}

		BLI_freelinkN(&process.queue, event);
	}

	voronoi_finishEdge(&process, process.root);

	edge = process.edges.first;
	while (edge) {
		if (edge->neighbour) {
			copy_v2_v2(edge->start, edge->neighbour->end);
			MEM_freeN(edge->neighbour);
		}

		edge = edge->next;
	}

	BLI_movelisttolist(edges, &process.edges);
}

static int testVoronoiEdge(const float site[2], const float point[2], const VoronoiEdge *edge)
{
	float p[2];

	if (isect_seg_seg_v2_point(site, point, edge->start, edge->end, p) == 1) {
		if (len_squared_v2v2(p, edge->start) > VORONOI_EPS &&
		    len_squared_v2v2(p, edge->end) > VORONOI_EPS)
		{
			return FALSE;
		}
	}

	return TRUE;
}

static int voronoi_addTriangulationPoint(const float coord[2], const float color[3],
                                         VoronoiTriangulationPoint **triangulated_points,
                                         int *triangulated_points_total)
{
	VoronoiTriangulationPoint *triangulation_point;
	int i;

	for (i = 0; i < *triangulated_points_total; i++) {
		if (equals_v2v2(coord, (*triangulated_points)[i].co)) {
			triangulation_point = &(*triangulated_points)[i];

			add_v3_v3(triangulation_point->color, color);
			triangulation_point->power++;

			return i;
		}
	}

	if (*triangulated_points) {
		*triangulated_points = MEM_reallocN(*triangulated_points,
		                                    sizeof(VoronoiTriangulationPoint) * (*triangulated_points_total + 1));
	}
	else {
		*triangulated_points = MEM_callocN(sizeof(VoronoiTriangulationPoint), "triangulation points");
	}

	triangulation_point = &(*triangulated_points)[(*triangulated_points_total)];
	copy_v2_v2(triangulation_point->co, coord);
	copy_v3_v3(triangulation_point->color, color);

	triangulation_point->power = 1;

	(*triangulated_points_total)++;

	return (*triangulated_points_total) - 1;
}

static void voronoi_addTriangle(int v1, int v2, int v3, int (**triangles)[3], int *triangles_total)
{
	int *triangle;

	if (*triangles) {
		*triangles = MEM_reallocN(*triangles, sizeof(int[3]) * (*triangles_total + 1));
	}
	else {
		*triangles = MEM_callocN(sizeof(int[3]), "trianglulation triangles");
	}

	triangle = (int *)&(*triangles)[(*triangles_total)];

	triangle[0] = v1;
	triangle[1] = v2;
	triangle[2] = v3;

	(*triangles_total)++;
}

void BLI_voronoi_triangulate(const VoronoiSite *sites, int sites_total, ListBase *edges, int width, int height,
                             VoronoiTriangulationPoint **triangulated_points_r, int *triangulated_points_total_r,
                             int (**triangles_r)[3], int *triangles_total_r)
{
	VoronoiTriangulationPoint *triangulated_points = NULL;
	int (*triangles)[3] = NULL;
	int triangulated_points_total = 0, triangles_total = 0;
	int i;
	ListBase boundary_edges = {NULL, NULL};

	voronoi_clampEdges(edges, width, height, &boundary_edges);
	voronoi_createBoundaryEdges(&boundary_edges, width, height);

	for (i = 0; i < sites_total; i++) {
		VoronoiEdge *edge;
		int v1;

		v1 = voronoi_addTriangulationPoint(sites[i].co, sites[i].color, &triangulated_points, &triangulated_points_total);

		edge = boundary_edges.first;
		while (edge) {
			VoronoiEdge *test_edge = boundary_edges.first;
			int ok_start = TRUE, ok_end = TRUE;

			while (test_edge) {
				float v1[2], v2[2];

				sub_v2_v2v2(v1, edge->start, sites[i].co);
				sub_v2_v2v2(v2, edge->end, sites[i].co);

				if (ok_start && !testVoronoiEdge(sites[i].co, edge->start, test_edge))
					ok_start = FALSE;

				if (ok_end && !testVoronoiEdge(sites[i].co, edge->end, test_edge))
					ok_end = FALSE;

				test_edge = test_edge->next;
			}

			if (ok_start && ok_end) {
				int v2, v3;

				v2 = voronoi_addTriangulationPoint(edge->start, sites[i].color, &triangulated_points, &triangulated_points_total);
				v3 = voronoi_addTriangulationPoint(edge->end, sites[i].color, &triangulated_points, &triangulated_points_total);

				voronoi_addTriangle(v1, v2, v3, &triangles, &triangles_total);
			}

			edge = edge->next;
		}
	}

	for (i = 0; i < triangulated_points_total; i++) {
		VoronoiTriangulationPoint *triangulation_point = &triangulated_points[i];

		mul_v3_fl(triangulation_point->color, 1.0f / triangulation_point->power);
	}

	*triangulated_points_r = triangulated_points;
	*triangulated_points_total_r = triangulated_points_total;

	*triangles_r = triangles;
	*triangles_total_r = triangles_total;

	BLI_freelistN(&boundary_edges);
}
