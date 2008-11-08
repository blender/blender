/**
 * $Id:  $
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BIF_RETARGET_H
#define BIF_RETARGET_H

#include "DNA_listBase.h"

#include "BLI_graph.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "reeb.h"

struct EditBone;

struct RigJoint;
struct RigGraph;
struct RigNode;
struct RigArc;
struct RigEdge;

#define USE_THREADS

typedef struct RigGraph {
	ListBase	arcs;
	ListBase	nodes;

	float length;
	
	FreeArc			free_arc;
	FreeNode		free_node;
	RadialSymmetry	radial_symmetry;
	AxialSymmetry	axial_symmetry;
	/*********************************/

	ListBase	controls;
	ListBase*	editbones;
	
	struct RigNode *head;
	ReebGraph *link_mesh;
	
	
	struct ThreadedWorker *worker;
	
	GHash *bones_map;	/* map of editbones by name */
	GHash *controls_map;	/* map of rigcontrols by bone pointer */
	
	Object *ob;
} RigGraph;

typedef struct RigNode {
	void *next, *prev;
	float p[3];
	int flag;

	int degree;
	struct BArc **arcs;

	int subgraph_index;

	int symmetry_level;
	int symmetry_flag;
	float symmetry_axis[3];
	/*********************************/

	ReebNode *link_mesh;
} RigNode;

typedef struct RigArc {
	void *next, *prev;
	RigNode *head, *tail;
	int flag;

	float length;

	int symmetry_level;
	int symmetry_group;
	int symmetry_flag;
	/*********************************/
	
	ListBase edges;
	int count;
	ReebArc *link_mesh;
} RigArc;

typedef struct RigEdge {
	struct RigEdge *next, *prev;
	float head[3], tail[3];
	float length;
	float angle;
	EditBone *bone;
	float up_axis[3];
} RigEdge;

/* Control flags */
#define RIG_CTRL_HEAD_DONE		1
#define RIG_CTRL_TAIL_DONE		2
#define RIG_CTRL_PARENT_DEFORM	4
#define RIG_CTRL_FIT_ROOT		8
#define RIG_CTRL_FIT_BONE		16

#define RIG_CTRL_DONE	(RIG_CTRL_HEAD_DONE|RIG_CTRL_TAIL_DONE)


typedef struct RigControl {
	struct RigControl *next, *prev;
	float head[3], tail[3];
	EditBone *bone;
	EditBone *link;
	EditBone *link_tail;
	float	up_axis[3];
	float	offset[3];
	int		flag;
} RigControl;

void BIF_retargetArc(ReebArc *earc);

#endif /* BIF_RETARGET_H */
