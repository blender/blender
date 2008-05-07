/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_oops_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_action_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BIF_interface.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"

#include "BIF_oops.h"

#include "BSE_drawipo.h"
#include "BSE_drawoops.h"
#include "MEM_guardedalloc.h"
#include "blendef.h"


#include "depsgraph_private.h"

#ifdef WIN32
#else
#include <sys/time.h>
#endif



void boundbox_deps()
{
	DagNode *node;
	float min[2], max[2];
	
	if(G.soops==0) return;
	
	min[0]= 1000.0;
	max[0]= -10000.0;
	min[1]= 1000.0;
	max[1]= -1000.0;
	
	node = getMainDag()->DagNode.first;
	while(node) {
		min[0]= MIN2(min[0], node->x);
		max[0]= MAX2(max[0], node->x+OOPSX);
		min[1]= MIN2(min[1], node->y);
		max[1]= MAX2(max[1], node->y+OOPSY);
		
		node= node->next;
	}
		
	G.v2d->tot.xmin= min[0];
	G.v2d->tot.xmax= max[0];
	G.v2d->tot.ymin= min[1];
	G.v2d->tot.ymax= max[1];
}

static unsigned int get_line_color(DagAdjList *child)
{
	switch  (child->type) {
		case DAG_RL_SCENE :
			return 0x00000;
		case DAG_RL_DATA :
			return 0xFF0000;
		case DAG_RL_OB_OB :
			return 0x00FF00;
		case DAG_RL_OB_DATA :
			return 0xFFFF00;
		case DAG_RL_DATA_OB :
			return 0x000000;
		case DAG_RL_DATA_DATA :
			return 0x0000FF;
		default :
			return 0xFF00FF;
	}
	//return 0x00000;
}


static void draw_deps(DagNode *node)
{
	float v1[2], x1, y1, x2, y2;
	unsigned int body, border;
	short line= 0;
	char str[32];
	DagAdjList *itA = node->child;

	x1= node->x; 
	x2= node->x+DEPSX;
	y1= node->y; 
	y2= node->y+DEPSY;

	if(x2 < G.v2d->cur.xmin || x1 > G.v2d->cur.xmax) return;
	if(y2 < G.v2d->cur.ymin || y1 > G.v2d->cur.ymax) return;

	body =  give_oops_color(node->type, 0, &border);

	line= 0;
//	border= 00;
	cpack(body);

	glRectf(x1,  y1,  x2,  y2);
	
	v1[0]= x1; 
	v1[1]= (y1+y2)/2 -0.3f;
	sprintf(str, "     %s", ((ID *) node->ob)->name+2);
	
	calc_oopstext(str, v1);
	
		/* ICON */
//	if(str[1] && oopscalex>1.1) {
	draw_icon_oops(v1, node->type);
//	}

	
	cpack(0x0);
	glRasterPos3f(v1[0],  v1[1], 0.0);
	BMF_DrawString(G.fonts, str);

	
	if(line) setlinestyle(2);
	cpack(border);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glRectf(x1,  y1,  x2,  y2);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if(line) setlinestyle(0);

	while (itA) { /* draw connection lines */
		cpack(get_line_color(itA));
		glBegin(GL_LINE_STRIP);
			glVertex2f(node->x+DEPSX, node->y+ 0.5*DEPSY);
			glVertex2f(itA->node->x, itA->node->y+ 0.5*DEPSY);			
		glEnd();
		itA = itA->next;
	}
			/* Draw the little rounded connection point */
		glColor3ub(0, 0, 0);
		glPushMatrix();

		glTranslatef(node->x , node->y+ 0.5*DEPSY, 0.0);
		glutil_draw_filled_arc(-M_PI/2, M_PI, 0.07*DEPSX, 7);

		glPopMatrix();

}

void	draw_all_deps(void)
{
	DagNode *node;
	DagForest *dag;
	
	dag = getMainDag();
	 node = dag->DagNode.first;
	//node = node->next;
	while(node) {
		draw_deps(node);
		node = node->next;
	}
	free_forest(dag);
	MEM_freeN(dag);
	setMainDag(NULL);
}




int build_deps(short mask)
{
	Base *base;
	Object *ob = NULL;
	DagNode * node = NULL; 
	DagNode * node2 = NULL ;
DagNode * scenenode;
	DagForest *dag;

#ifdef DEPS_DEBUG
	//timers
	struct timeval tp1, tp2, tp3, tp4;
	
	gettimeofday(&tp1,NULL);
#endif
	
	DagNodeQueue *retqueue;
	
//	float y = 0;
//	int maxlev = 0;

	if(G.soops==0) return -1;
	
	
	// rebuilt each time for now
	dag = getMainDag();
	if ( dag)
		free_forest( dag ); 
	else {
		dag = dag_init();
		setMainDag(dag);
		}
		
	// add base node for scene. scene is always the first node in DAG
	scenenode = dag_add_node(dag, G.scene);
	set_node_xy(scenenode,0.0, 0.0); 	
		/* blocks from this scene */
		
		
		/* targets in object struct yet to be added. should even they ?
				struct Ipo *ipo;
			ListBase nlastrips;
			ListBase hooks;
		*/
		
		
	base= FIRSTBASE;
	while(base) { // add all objects in any case
		int addtoroot = 1;
		
		//				 graph_print_adj_list();
		ob= (Object *) base->object;

		node = dag_get_node(dag,ob);
	
		if ((ob->data) && (mask&DAG_RL_DATA)) {
			node2 = dag_get_node(dag,ob->data);
			dag_add_relation(dag,node,node2,DAG_RL_DATA, "Object-Data Relation");
			node2->first_ancestor = ob;
			node2->ancestor_count += 1;
			
		} 
		
		if (addtoroot == 1 )
			dag_add_relation(dag,scenenode,node,DAG_RL_SCENE, "Scene Relation");
		
		base= base->next;
	}

//graph_print_adj_list();
//fprintf(stderr,"building deps\n");
#ifdef DEPS_DEBUG
	gettimeofday(&tp2,NULL);
#endif

//graph_bfs(); //set levels

#ifdef DEPS_DEBUG
gettimeofday(&tp3,NULL);
#endif


retqueue = graph_dfs(); //set levels
#ifdef DEPS_DEBUG
gettimeofday(&tp4,NULL);
fprintf(stderr,"************************************\n");
graph_print_queue_dist(retqueue);
//graph_print_queue(retqueue);

fprintf(stderr,"TIME BUILD %d %d BFS %d %d DFS  %d %d\n",tp2.tv_sec-tp1.tv_sec ,tp2.tv_usec-tp1.tv_usec
														, tp3.tv_sec-tp2.tv_sec ,tp3.tv_usec-tp2.tv_usec
														, tp4.tv_sec-tp3.tv_sec ,tp4.tv_usec-tp3.tv_usec);
#endif

queue_delete(retqueue);

//graph_print_adj_list();
return 0;
}
