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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_color_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_material.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "BIF_cursors.h"
#include "BIF_editview.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_imasel.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_renderwin.h"
#include "BIF_space.h"
#include "BIF_scrarea.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_node.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_storage_types.h"

#include "BDR_editobject.h"
#include "BDR_gpencil.h"

#include "RE_pipeline.h"
#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "butspace.h"
#include "PIL_time.h"
#include "mydevice.h"
#include "winlay.h"


/* currently called from BIF_preview_changed */
void snode_tag_dirty(SpaceNode *snode)
{
	bNode *node;
	
	if(snode->treetype==NTREE_SHADER) {
		if(snode->nodetree) {
			for(node= snode->nodetree->nodes.first; node; node= node->next) {
				if(node->type==SH_NODE_OUTPUT)
					node->lasty= 0;
			}
			snode->flag |= SNODE_DO_PREVIEW;	/* this adds an afterqueue on a redraw, to allow button previews to work first */
		}
	}
	allqueue(REDRAWNODE, 1);
}

static void shader_node_previewrender(ScrArea *sa, SpaceNode *snode)
{
	bNode *node;
	
	if(snode->id==NULL) return;
	if( ((Material *)snode->id )->use_nodes==0 ) return;

	for(node= snode->nodetree->nodes.first; node; node= node->next) {
		if(node->type==SH_NODE_OUTPUT) {
			if(node->flag & NODE_DO_OUTPUT) {
				if(node->lasty<PREVIEW_RENDERSIZE-2) {
					RenderInfo ri;	
//					int test= node->lasty;
					
					ri.curtile = 0;
					ri.tottile = 0;
					ri.rect = NULL;
					ri.pr_rectx = PREVIEW_RENDERSIZE;
					ri.pr_recty = PREVIEW_RENDERSIZE;
					
					BIF_previewrender(snode->id, &ri, NULL, PR_DO_RENDER);	/* sends redraw event */
					if(ri.rect) MEM_freeN(ri.rect);
					
					/* when not finished... */
					if(ri.curtile<ri.tottile)
						addafterqueue(sa->win, RENDERPREVIEW, 1);
//					if(test!=node->lasty)
//						printf("node rendered to %d\n", node->lasty);

					break;
				}
			}
		}
	}
}


static void snode_handle_recalc(SpaceNode *snode)
{
	if(snode->treetype==NTREE_SHADER) {
		BIF_preview_changed(ID_MA);	 /* signals buttons windows and node editors */
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		if(G.scene->use_nodes) {
			snode->nodetree->timecursor= set_timecursor;
			G.afbreek= 0;
			snode->nodetree->test_break= blender_test_break;
			
			BIF_store_spare();
			
			ntreeCompositExecTree(snode->nodetree, &G.scene->r, 1);	/* 1 is do_previews */
			
			snode->nodetree->timecursor= NULL;
			snode->nodetree->test_break= NULL;
			waitcursor(0);
			
			allqueue(REDRAWIMAGE, 1);
			if(G.scene->r.scemode & R_DOCOMP) {
				BIF_redraw_render_rect();	/* seems to screwup display? */
				mywinset(curarea->win);
			}
		}

		allqueue(REDRAWNODE, 1);
	}
}

static void shader_node_event(SpaceNode *snode, short event)
{
	switch(event) {
		case B_REDR:
			allqueue(REDRAWNODE, 1);
			break;
		default:
			/* B_NODE_EXEC */
			snode_handle_recalc(snode);
			break;
			
	}
}

static int image_detect_file_sequence(int *start_p, int *frames_p, char *str)
{
	SpaceFile *sfile;
	char name[FILE_MAX], head[FILE_MAX], tail[FILE_MAX], filename[FILE_MAX];
	int a, frame, totframe, found, minframe;
	unsigned short numlen;

	sfile= scrarea_find_space_of_type(curarea, SPACE_FILE);
	if(sfile==NULL || sfile->filelist==NULL)
		return 0;

	/* find first frame */
	found= 0;
	minframe= 0;

	for(a=0; a<sfile->totfile; a++) {
		if(sfile->filelist[a].flags & ACTIVE) {
			BLI_strncpy(name, sfile->filelist[a].relname, sizeof(name));
			frame= BLI_stringdec(name, head, tail, &numlen);

			if(!found || frame < minframe) {
				BLI_strncpy(filename, name, sizeof(name));
				minframe= frame;
				found= 1;
			}
		}
	}

	/* not one frame found */
	if(!found)
		return 0;

	/* counter number of following frames */
	found= 1;
	totframe= 0;

	for(frame=minframe; found; frame++) {
		found= 0;
		BLI_strncpy(name, filename, sizeof(name));
		BLI_stringenc(name, head, tail, numlen, frame);

		for(a=0; a<sfile->totfile; a++) {
			if(sfile->filelist[a].flags & ACTIVE) {
				if(strcmp(sfile->filelist[a].relname, name) == 0) {
					found= 1;
					totframe++;
					break;
				}
			}
		}
	}

	if(totframe > 1) {
		BLI_strncpy(str, sfile->dir, sizeof(name));
		strcat(str, filename);

		*start_p= minframe;
		*frames_p= totframe;
		return 1;
	}

	return 0;
}

static void load_node_image(char *str)	/* called from fileselect */
{
	SpaceNode *snode= curarea->spacedata.first;
	bNode *node= nodeGetActive(snode->edittree);
	Image *ima= NULL;
	ImageUser *iuser= node->storage;
	char filename[FILE_MAX];
	int start=0, frames=0, sequence;

	sequence= image_detect_file_sequence(&start, &frames, filename);
	if(sequence)
		str= filename;
	
	ima= BKE_add_image_file(str);
	if(ima) {
		if(node->id)
			node->id->us--;
		
		node->id= &ima->id;
		id_us_plus(node->id);

		BLI_strncpy(node->name, node->id->name+2, 21);

		if(sequence) {
			ima->source= IMA_SRC_SEQUENCE;
			iuser->frames= frames;
			iuser->offset= start-1;
		}
				   
		BKE_image_signal(ima, node->storage, IMA_SIGNAL_RELOAD);
		
		NodeTagChanged(snode->edittree, node);
		snode_handle_recalc(snode);
		allqueue(REDRAWNODE, 0); 
	}
}

static void set_node_imagepath(char *str)	/* called from fileselect */
{
	SpaceNode *snode= curarea->spacedata.first;
	bNode *node= nodeGetActive(snode->edittree);
	BLI_strncpy(((NodeImageFile *)node->storage)->name, str, sizeof( ((NodeImageFile *)node->storage)->name ));
}

static bNode *snode_get_editgroup(SpaceNode *snode)
{
	bNode *gnode;
	
	/* get the groupnode */
	for(gnode= snode->nodetree->nodes.first; gnode; gnode= gnode->next)
		if(gnode->flag & NODE_GROUP_EDIT)
			break;
	return gnode;
}

/* node has to be of type 'render layers' */
/* is a bit clumsy copying renderdata here... scene nodes use render size of current render */
static void composite_node_render(SpaceNode *snode, bNode *node)
{
	RenderData rd;
	Scene *scene= NULL;
	int scemode, actlay;
	
	/* the button press won't show up otherwise, button hilites disabled */
	force_draw(0);
	
	if(node->id && node->id!=(ID *)G.scene) {
		scene= G.scene;
		set_scene_bg((Scene *)node->id);
		rd= G.scene->r;
		G.scene->r.xsch= scene->r.xsch;
		G.scene->r.ysch= scene->r.ysch;
		G.scene->r.size= scene->r.size;
		G.scene->r.mode &= ~(R_BORDER|R_DOCOMP);
		G.scene->r.mode |= scene->r.mode & R_BORDER;
		G.scene->r.border= scene->r.border;
		G.scene->r.cfra= scene->r.cfra;
	}
	
	scemode= G.scene->r.scemode;
	actlay= G.scene->r.actlay;

	G.scene->r.scemode |= R_SINGLE_LAYER|R_COMP_RERENDER;
	G.scene->r.actlay= node->custom1;
	
	BIF_do_render(0);
	
	G.scene->r.scemode= scemode;
	G.scene->r.actlay= actlay;

	node->custom2= 0;
	
	if(scene) {
		G.scene->r= rd;
		set_scene_bg(scene);
	}
}

static void composit_node_event(SpaceNode *snode, short event)
{
	
	switch(event) {
		case B_REDR:
			allqueue(REDRAWNODE, 1);
			break;
		case B_NODE_LOADIMAGE:
		{
			bNode *node= nodeGetActive(snode->edittree);
			char name[FILE_MAXDIR+FILE_MAXFILE];
			
			if(node->id)
				strcpy(name, ((Image *)node->id)->name);
			else strcpy(name, U.textudir);
			if (G.qual & LR_CTRLKEY) {
				activate_imageselect(FILE_SPECIAL, "SELECT IMAGE", name, load_node_image);
			} else {
				activate_fileselect(FILE_SPECIAL, "SELECT IMAGE", name, load_node_image);
			}
			break;
		}
		case B_NODE_SETIMAGE:
		{
			bNode *node= nodeGetActive(snode->edittree);
			char name[FILE_MAXDIR+FILE_MAXFILE];
			
			strcpy(name, ((NodeImageFile *)node->storage)->name);
			if (G.qual & LR_CTRLKEY) {
				activate_imageselect(FILE_SPECIAL, "SELECT OUTPUT DIR", name, set_node_imagepath);
			} else {
				activate_fileselect(FILE_SPECIAL, "SELECT OUTPUT DIR", name, set_node_imagepath);
			}
			break;
		}
		case B_NODE_TREE_EXEC:
			snode_handle_recalc(snode);
			break;		
		default:
			/* B_NODE_EXEC */
		{
			bNode *node= BLI_findlink(&snode->edittree->nodes, event-B_NODE_EXEC);
			if(node) {
				NodeTagChanged(snode->edittree, node);
				/* don't use NodeTagIDChanged, it gives far too many recomposites for image, scene layers, ... */
				
				/* not the best implementation of the world... but we need it to work now :) */
				if(node->type==CMP_NODE_R_LAYERS && node->custom2) {
					/* add event for this window (after render curarea can be changed) */
					addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
					
					composite_node_render(snode, node);
					snode_handle_recalc(snode);
					
					/* add another event, a render can go fullscreen and open new window */
					addqueue(curarea->win, UI_BUT_EVENT, B_NODE_TREE_EXEC);
				}
				else {
					node= snode_get_editgroup(snode);
					if(node)
						NodeTagIDChanged(snode->nodetree, node->id);
					
					snode_handle_recalc(snode);
				}
			}
		}			
	}
}


/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void node_shader_default(Material *ma)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	
	/* but lets check it anyway */
	if(ma->nodetree) {
		printf("error in shader initialize\n");
		return;
	}
	
	ma->nodetree= ntreeAddTree(NTREE_SHADER);
	
	out= nodeAddNodeType(ma->nodetree, SH_NODE_OUTPUT, NULL, NULL);
	out->locx= 300.0f; out->locy= 300.0f;
	
	in= nodeAddNodeType(ma->nodetree, SH_NODE_MATERIAL, NULL, NULL);
	in->locx= 10.0f; in->locy= 300.0f;
	nodeSetActive(ma->nodetree, in);
	
	/* only a link from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(ma->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(ma->nodetree);	/* needed for pointers */
}

/* assumes nothing being done in ntree yet, sets the default in/out node */
/* called from shading buttons or header */
void node_composit_default(Scene *sce)
{
	bNode *in, *out;
	bNodeSocket *fromsock, *tosock;
	
	/* but lets check it anyway */
	if(sce->nodetree) {
		printf("error in composit initialize\n");
		return;
	}
	
	sce->nodetree= ntreeAddTree(NTREE_COMPOSIT);
	
	out= nodeAddNodeType(sce->nodetree, CMP_NODE_COMPOSITE, NULL, NULL);
	out->locx= 300.0f; out->locy= 400.0f;
	
	in= nodeAddNodeType(sce->nodetree, CMP_NODE_R_LAYERS, NULL, NULL);
	in->locx= 10.0f; in->locy= 400.0f;
	nodeSetActive(sce->nodetree, in);
	
	/* links from color to color */
	fromsock= in->outputs.first;
	tosock= out->inputs.first;
	nodeAddLink(sce->nodetree, in, fromsock, out, tosock);
	
	ntreeSolveOrder(sce->nodetree);	/* needed for pointers */
	
	ntreeCompositForceHidden(sce->nodetree);
}

/* Here we set the active tree(s), even called for each redraw now, so keep it fast :) */
void snode_set_context(SpaceNode *snode)
{
	Object *ob= OBACT;
	bNode *node= NULL;
	
	snode->nodetree= NULL;
	snode->id= snode->from= NULL;
	
	if(snode->treetype==NTREE_SHADER) {
		/* need active object, or we allow pinning... */
		if(ob) {
			Material *ma= give_current_material(ob, ob->actcol);
			if(ma) {
				snode->from= material_from(ob, ob->actcol);
				snode->id= &ma->id;
				snode->nodetree= ma->nodetree;
			}
		}
	}
	else if(snode->treetype==NTREE_COMPOSIT) {
		snode->from= NULL;
		snode->id= &G.scene->id;
		
		/* bit clumsy but reliable way to see if we draw first time */
		if(snode->nodetree==NULL)
			ntreeCompositForceHidden(G.scene->nodetree);
		
		snode->nodetree= G.scene->nodetree;
	}
	
	/* find editable group */
	if(snode->nodetree)
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			if(node->flag & NODE_GROUP_EDIT)
				break;
	
	if(node && node->id)
		snode->edittree= (bNodeTree *)node->id;
	else
		snode->edittree= snode->nodetree;
}

/* on activate image viewer, check if we show it */
static void node_active_image(Image *ima)
{
	ScrArea *sa;
	SpaceImage *sima= NULL;
	
	/* find an imagewindow showing render result */
	for(sa=G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_IMAGE) {
			sima= sa->spacedata.first;
			if(sima->image && sima->image->source!=IMA_SRC_VIEWER)
				break;
		}
	}
	if(sa && sima) {
		sima->image= ima;
		scrarea_queue_winredraw(sa);
		scrarea_queue_headredraw(sa);
	}
}


static void node_set_active(SpaceNode *snode, bNode *node)
{
	
	nodeSetActive(snode->edittree, node);
	
	if(node->type!=NODE_GROUP) {
		
		/* tree specific activate calls */
		if(snode->treetype==NTREE_SHADER) {
			
			/* when we select a material, active texture is cleared, for buttons */
			if(node->id && GS(node->id->name)==ID_MA)
				nodeClearActiveID(snode->edittree, ID_TE);
			if(node->id)
				BIF_preview_changed(-1);	/* temp hack to force texture preview to update */
			
			allqueue(REDRAWBUTSSHADING, 1);
			allqueue(REDRAWIPO, 0);
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			/* make active viewer, currently only 1 supported... */
			if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				bNode *tnode;
				int was_output= node->flag & NODE_DO_OUTPUT;

				for(tnode= snode->edittree->nodes.first; tnode; tnode= tnode->next)
					if( ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))
						tnode->flag &= ~NODE_DO_OUTPUT;
				
				node->flag |= NODE_DO_OUTPUT;
				if(was_output==0) {
					bNode *gnode;
					
					NodeTagChanged(snode->edittree, node);
					
					/* if inside group, tag entire group */
					gnode= snode_get_editgroup(snode);
					if(gnode)
						NodeTagIDChanged(snode->nodetree, gnode->id);
					
					snode_handle_recalc(snode);
				}
				
				/* addnode() doesnt link this yet... */
				node->id= (ID *)BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
			}
			else if(node->type==CMP_NODE_IMAGE) {
				if(node->id)
					node_active_image((Image *)node->id);
			}
			else if(node->type==CMP_NODE_R_LAYERS) {
				if(node->id==NULL || node->id==(ID *)G.scene) {
					G.scene->r.actlay= node->custom1;
					allqueue(REDRAWBUTSSCENE, 0);
				}
			}
		}
	}
}

void snode_make_group_editable(SpaceNode *snode, bNode *gnode)
{
	bNode *node;
	
	/* make sure nothing has group editing on */
	for(node= snode->nodetree->nodes.first; node; node= node->next)
		node->flag &= ~NODE_GROUP_EDIT;
	
	if(gnode==NULL) {
		/* with NULL argument we do a toggle */
		if(snode->edittree==snode->nodetree)
			gnode= nodeGetActive(snode->nodetree);
	}
	
	if(gnode && gnode->type==NODE_GROUP && gnode->id) {
		if(gnode->id->lib) {
			if(okee("Make Group Local"))
				ntreeMakeLocal((bNodeTree *)gnode->id);
			else
				return;
		}
		gnode->flag |= NODE_GROUP_EDIT;
		snode->edittree= (bNodeTree *)gnode->id;
		
		/* deselect all other nodes, so we can also do grabbing of entire subtree */
		for(node= snode->nodetree->nodes.first; node; node= node->next)
			node->flag &= ~SELECT;
		gnode->flag |= SELECT;
		
	}
	else 
		snode->edittree= snode->nodetree;
	
	ntreeSolveOrder(snode->nodetree);
	
	/* finally send out events for new active node */
	if(snode->treetype==NTREE_SHADER) {
		allqueue(REDRAWBUTSSHADING, 0);
		
		BIF_preview_changed(-1);	/* temp hack to force texture preview to update */
	}
	
	allqueue(REDRAWNODE, 0);
}

void node_ungroup(SpaceNode *snode)
{
	bNode *gnode;

	/* are we inside of a group? */
	gnode= snode_get_editgroup(snode);
	if(gnode)
		snode_make_group_editable(snode, NULL);
	
	gnode= nodeGetActive(snode->edittree);
	if(gnode==NULL) return;
	
	if(gnode->type!=NODE_GROUP)
		error("Not a group");
	else {
		if(nodeGroupUnGroup(snode->edittree, gnode)) {
			
			BIF_undo_push("Deselect all nodes");
			allqueue(REDRAWNODE, 0);
		}
		else
			error("Can't ungroup");
	}
}

/* when links in groups change, inputs/outputs change, nodes added/deleted... */
static void snode_verify_groups(SpaceNode *snode)
{
	bNode *gnode;
	
	gnode= snode_get_editgroup(snode);
	
	/* does all materials */
	if(gnode)
		nodeVerifyGroup((bNodeTree *)gnode->id);
	
}

static void node_addgroup(SpaceNode *snode)
{
	bNodeTree *ngroup;
	int tot= 0, offs, val;
	char *strp;
	
	if(snode->edittree!=snode->nodetree) {
		error("Can not add a Group in a Group");
		return;
	}
	
	/* construct menu with choices */
	for(ngroup= G.main->nodetree.first; ngroup; ngroup= ngroup->id.next) {
		if(ngroup->type==snode->treetype)
			tot++;
	}
	if(tot==0) {
		error("No groups available in database");
		return;
	}
	strp= MEM_mallocN(32*tot+32, "menu");
	strcpy(strp, "Add Group %t");
	offs= strlen(strp);
	
	for(tot=0, ngroup= G.main->nodetree.first; ngroup; ngroup= ngroup->id.next, tot++) {
		if(ngroup->type==snode->treetype)
			offs+= sprintf(strp+offs, "|%s %%x%d", ngroup->id.name+2, tot);
	}	
	
	val= pupmenu(strp);
	if(val>=0) {
		ngroup= BLI_findlink(&G.main->nodetree, val);
		if(ngroup) {
			bNode *node= nodeAddNodeType(snode->edittree, NODE_GROUP, ngroup, NULL);
			
			/* generics */
			if(node) {
				float locx, locy;
				short mval[2];

				node_deselectall(snode, 0);
				
				getmouseco_areawin(mval);
				areamouseco_to_ipoco(G.v2d, mval, &locx, &locy);
				
				node->locx= locx;
				node->locy= locy + 60.0f;		// arbitrary.. so its visible
				node->flag |= SELECT;
				
				id_us_plus(node->id);
				
				node_set_active(snode, node);
				BIF_undo_push("Add Node");
			}
		}			
	}
	MEM_freeN(strp);
}


/* ************************** Node generic ************** */

/* allows to walk the list in order of visibility */
static bNode *next_node(bNodeTree *ntree)
{
	static bNode *current=NULL, *last= NULL;
	
	if(ntree) {
		/* set current to the first selected node */
		for(current= ntree->nodes.last; current; current= current->prev)
			if(current->flag & NODE_SELECT)
				break;
		
		/* set last to the first unselected node */
		for(last= ntree->nodes.last; last; last= last->prev)
			if((last->flag & NODE_SELECT)==0)
				break;
		
		if(current==NULL)
			current= last;
		
		return NULL;
	}
	/* no nodes, or we are ready */
	if(current==NULL)
		return NULL;
	
	/* now we walk the list backwards, but we always return current */
	if(current->flag & NODE_SELECT) {
		bNode *node= current;
		
		/* find previous selected */
		current= current->prev;
		while(current && (current->flag & NODE_SELECT)==0)
			current= current->prev;
		
		/* find first unselected */
		if(current==NULL)
			current= last;
		
		return node;
	}
	else {
		bNode *node= current;
		
		/* find previous unselected */
		current= current->prev;
		while(current && (current->flag & NODE_SELECT))
			current= current->prev;
		
		return node;
	}
	
	return NULL;
}

/* is rct in visible part of node? */
static bNode *visible_node(SpaceNode *snode, rctf *rct)
{
	bNode *tnode;
	
	for(next_node(snode->edittree); (tnode=next_node(NULL));) {
		if(BLI_isect_rctf(&tnode->totr, rct, NULL))
			break;
	}
	return tnode;
}

void snode_home(ScrArea *sa, SpaceNode *snode)
{
	bNode *node;
	int first= 1;
	
	snode->v2d.cur.xmin= snode->v2d.cur.ymin= 0.0f;
	snode->v2d.cur.xmax= sa->winx;
	snode->v2d.cur.xmax= sa->winy;
	
	if(snode->edittree) {
		for(node= snode->edittree->nodes.first; node; node= node->next) {
			if(first) {
				first= 0;
				snode->v2d.cur= node->totr;
			}
			else {
				BLI_union_rctf(&snode->v2d.cur, &node->totr);
			}
		}
	}
	snode->v2d.tot= snode->v2d.cur;
	
	snode->xof = snode->yof = 0.0;
	
	test_view2d(G.v2d, sa->winx, sa->winy);
	
}

void snode_zoom_out(ScrArea *sa)
{
	float dx;
	
	dx= (float)(0.15*(G.v2d->cur.xmax-G.v2d->cur.xmin));
	G.v2d->cur.xmin-= dx;
	G.v2d->cur.xmax+= dx;
	dx= (float)(0.15*(G.v2d->cur.ymax-G.v2d->cur.ymin));
	G.v2d->cur.ymin-= dx;
	G.v2d->cur.ymax+= dx;
	test_view2d(G.v2d, sa->winx, sa->winy);
}

void snode_zoom_in(ScrArea *sa)
{
	float dx;
	
	dx= (float)(0.1154*(G.v2d->cur.xmax-G.v2d->cur.xmin));
	G.v2d->cur.xmin+= dx;
	G.v2d->cur.xmax-= dx;
	dx= (float)(0.1154*(G.v2d->cur.ymax-G.v2d->cur.ymin));
	G.v2d->cur.ymin+= dx;
	G.v2d->cur.ymax-= dx;
	test_view2d(G.v2d, sa->winx, sa->winy);
}

static void snode_bg_viewmove(SpaceNode *snode)
{
	ScrArea *sa;
	Image *ima;
	ImBuf *ibuf;
	Window *win;
	short mval[2], mvalo[2];
	short rectx, recty, xmin, xmax, ymin, ymax, pad;
	int oldcursor;
	
	ima= BKE_image_verify_viewer(IMA_TYPE_COMPOSITE, "Viewer Node");
	ibuf= BKE_image_get_ibuf(ima, NULL);
	
	sa = snode->area;
	
	if(ibuf) {
		rectx = ibuf->x;
		recty = ibuf->y;
	} else {
		rectx = recty = 1;
	}
	
	pad = 10;
	xmin = -(sa->winx/2) - rectx/2 + pad;
	xmax = sa->winx/2 + rectx/2 - pad;
	ymin = -(sa->winy/2) - recty/2 + pad;
	ymax = sa->winy/2 + recty/2 - pad;
	
	getmouseco_sc(mvalo);
	
	/* store the old cursor to temporarily change it */
	oldcursor=get_cursor();
	win=winlay_get_active_window();
	
	SetBlenderCursor(BC_NSEW_SCROLLCURSOR);
	
	while(get_mbut()&(L_MOUSE|M_MOUSE)) {
		
		getmouseco_sc(mval);
		
		if(mvalo[0]!=mval[0] || mvalo[1]!=mval[1]) {
			
			snode->xof -= (mvalo[0]-mval[0]);
			snode->yof -= (mvalo[1]-mval[1]);
			
			/* prevent dragging image outside of the window and losing it! */
			CLAMP(snode->xof, xmin, xmax);
			CLAMP(snode->yof, ymin, ymax);
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		else BIF_wait_for_statechange();
	}
	
	window_set_cursor(win, oldcursor);
}

static void reset_sel_socket(SpaceNode *snode, int in_out)
{
	bNode *node;
	bNodeSocket *sock;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(in_out & SOCK_IN) {
			for(sock= node->inputs.first; sock; sock= sock->next)
				if(sock->flag & SOCK_SEL) sock->flag&= ~SOCK_SEL;
		}
		if(in_out & SOCK_OUT) {
			for(sock= node->outputs.first; sock; sock= sock->next)
				if(sock->flag & SOCK_SEL) sock->flag&= ~SOCK_SEL;
		}
	}
}

/* checks mouse position, and returns found node/socket */
/* type is SOCK_IN and/or SOCK_OUT */
static int find_indicated_socket(SpaceNode *snode, bNode **nodep, bNodeSocket **sockp, int in_out)
{
	bNode *node;
	bNodeSocket *sock;
	rctf rect;
	short mval[2];
	
	getmouseco_areawin(mval);
	
	/* check if we click in a socket */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
	
		areamouseco_to_ipoco(G.v2d, mval, &rect.xmin, &rect.ymin);
		
		rect.xmin -= NODE_SOCKSIZE+3;
		rect.ymin -= NODE_SOCKSIZE+3;
		rect.xmax = rect.xmin + 2*NODE_SOCKSIZE+6;
		rect.ymax = rect.ymin + 2*NODE_SOCKSIZE+6;
		
		if (!(node->flag & NODE_HIDDEN)) {
			/* extra padding inside and out - allow dragging on the text areas too */
			if (in_out == SOCK_IN) {
				rect.xmax += NODE_SOCKSIZE;
				rect.xmin -= NODE_SOCKSIZE*4;
			} else if (in_out == SOCK_OUT) {
				rect.xmax += NODE_SOCKSIZE*4;
				rect.xmin -= NODE_SOCKSIZE;
			}
		}
			
		if(in_out & SOCK_IN) {
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if(node == visible_node(snode, &rect)) {
							*nodep= node;
							*sockp= sock;
							return 1;
						}
					}
				}
			}
		}
		if(in_out & SOCK_OUT) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
					if(BLI_in_rctf(&rect, sock->locx, sock->locy)) {
						if(node == visible_node(snode, &rect)) {
							*nodep= node;
							*sockp= sock;
							return 1;
						}
					}
				}
			}
		}
	}
	return 0;
}

/* ********************* transform ****************** */

/* releases on event, only intern (for extern see below) */
/* we need argument ntree to allow operations on edittree or nodetree */
static void transform_nodes(bNodeTree *ntree, char mode, char *undostr)
{
	bNode *node;
	float mxstart, mystart, mx, my, *oldlocs, *ol;
	int cont=1, tot=0, cancel=0, firsttime=1;
	short mval[2], mvalo[2];
	
	/* count total */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & SELECT) tot++;
	
	if(tot==0) return;
	
	/* store oldlocs */
	ol= oldlocs= MEM_mallocN(sizeof(float)*2*tot, "oldlocs transform");
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			ol[0]= node->locx; ol[1]= node->locy;
			ol+= 2;
		}
	}
	
	getmouseco_areawin(mvalo);
	areamouseco_to_ipoco(G.v2d, mvalo, &mxstart, &mystart);
	
	while(cont) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {

			firsttime= 0;
			
			areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			for(ol= oldlocs, node= ntree->nodes.first; node; node= node->next) {
				if(node->flag & SELECT) {
					node->locx= ol[0] + mx-mxstart;
					node->locy= ol[1] + my-mystart;
					ol+= 2;
				}
			}
			
			force_draw(0);
		}
		else
			PIL_sleep_ms(10);
		
		while (qtest()) {
			short val;
			unsigned short event= extern_qread(&val);
			
			switch (event) {
				case LEFTMOUSE:
				case SPACEKEY:
				case RETKEY:
					cont=0;
					break;
				case ESCKEY:
				case RIGHTMOUSE:
					if(val) {
						cancel=1;
						cont=0;
					}
					break;
				default:
					if(val) arrows_move_cursor(event);
					break;
			}
		}
		
	}
	
	if(cancel) {
		for(ol= oldlocs, node= ntree->nodes.first; node; node= node->next) {
			if(node->flag & SELECT) {
				node->locx= ol[0];
				node->locy= ol[1];
				ol+= 2;
			}
		}
		
	}
	else
		BIF_undo_push(undostr);
	
	allqueue(REDRAWNODE, 1);
	MEM_freeN(oldlocs);
}

/* external call, also for callback */
void node_transform_ext(int mode, int unused)
{
	SpaceNode *snode= curarea->spacedata.first;
	
	transform_nodes(snode->edittree, 'g', "Move Node");
}


/* releases on event, only 1 node */
static void scale_node(SpaceNode *snode, bNode *node)
{
	float mxstart, mystart, mx, my, oldwidth;
	int cont=1, cancel=0;
	short mval[2], mvalo[2];
	
	/* store old */
	if(node->flag & NODE_HIDDEN)
		oldwidth= node->miniwidth;
	else
		oldwidth= node->width;
		
	getmouseco_areawin(mvalo);
	areamouseco_to_ipoco(G.v2d, mvalo, &mxstart, &mystart);
	
	while(cont) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1]) {
			
			areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(node->flag & NODE_HIDDEN) {
				node->miniwidth= oldwidth + mx-mxstart;
				CLAMP(node->miniwidth, 0.0f, 100.0f);
			}
			else {
				node->width= oldwidth + mx-mxstart;
				CLAMP(node->width, node->typeinfo->minwidth, node->typeinfo->maxwidth);
			}
			
			force_draw(0);
		}
		else
			PIL_sleep_ms(10);
		
		while (qtest()) {
			short val;
			unsigned short event= extern_qread(&val);
			
			switch (event) {
				case LEFTMOUSE:
				case SPACEKEY:
				case RETKEY:
					cont=0;
					break;
				case ESCKEY:
				case RIGHTMOUSE:
					if(val) {
						cancel=1;
						cont=0;
					}
					break;
			}
		}
		
	}
	
	if(cancel) {
		node->width= oldwidth;
	}
	else
		BIF_undo_push("Scale Node");
	
	allqueue(REDRAWNODE, 1);
}

/* ******************** rename ******************* */

void node_rename(SpaceNode *snode)
{
	bNode *node, *rename_node;
	short found_node= 0;

	/* check if a node is selected */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			found_node= 1;
			break;
		}
	}

	if(found_node) {
		rename_node= nodeGetActive(snode->edittree);
		node_rename_but((char *)rename_node->username);
		BIF_undo_push("Rename Node");
	
		allqueue(REDRAWNODE, 1);
	}
}

/* ********************** select ******************** */

/* used in buttons to check context, also checks for edited groups */
bNode *editnode_get_active_idnode(bNodeTree *ntree, short id_code)
{
	return nodeGetActiveID(ntree, id_code);
}

/* used in buttons to check context, also checks for edited groups */
Material *editnode_get_active_material(Material *ma)
{
	if(ma && ma->use_nodes && ma->nodetree) {
		bNode *node= editnode_get_active_idnode(ma->nodetree, ID_MA);
		if(node)
			return (Material *)node->id;
		else
			return NULL;
	}
	return ma;
}

/* used in buttons to check context, also checks for edited groups */
bNode *editnode_get_active(bNodeTree *ntree)
{
	bNode *node;
	
	/* check for edited group */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_GROUP_EDIT)
			break;
	if(node)
		return nodeGetActive((bNodeTree *)node->id);
	else
		return nodeGetActive(ntree);
}


/* no undo here! */
void node_deselectall(SpaceNode *snode, int swap)
{
	bNode *node;
	
	if(swap) {
		for(node= snode->edittree->nodes.first; node; node= node->next)
			if(node->flag & SELECT)
				break;
		if(node==NULL) {
			for(node= snode->edittree->nodes.first; node; node= node->next)
				node->flag |= SELECT;
			allqueue(REDRAWNODE, 0);
			return;
		}
		/* else pass on to deselect */
	}
	
	for(node= snode->edittree->nodes.first; node; node= node->next)
		node->flag &= ~SELECT;
	
	allqueue(REDRAWNODE, 0);
}

int node_has_hidden_sockets(bNode *node)
{
	bNodeSocket *sock;
	
	for(sock= node->inputs.first; sock; sock= sock->next)
		if(sock->flag & SOCK_HIDDEN)
			return 1;
	for(sock= node->outputs.first; sock; sock= sock->next)
		if(sock->flag & SOCK_HIDDEN)
			return 1;
	return 0;
}


static void node_hide_unhide_sockets(SpaceNode *snode, bNode *node)
{
	bNodeSocket *sock;
	
	/* unhide all */
	if( node_has_hidden_sockets(node) ) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_HIDDEN;
		for(sock= node->outputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_HIDDEN;
	}
	else {
		bNode *gnode= snode_get_editgroup(snode);
		
		/* hiding inside group should not break links in other group users */
		if(gnode) {
			nodeGroupSocketUseFlags((bNodeTree *)gnode->id);
			for(sock= node->inputs.first; sock; sock= sock->next)
				if(!(sock->flag & SOCK_IN_USE))
					if(sock->link==NULL)
						sock->flag |= SOCK_HIDDEN;
			for(sock= node->outputs.first; sock; sock= sock->next)
				if(!(sock->flag & SOCK_IN_USE))
					if(nodeCountSocketLinks(snode->edittree, sock)==0)
						sock->flag |= SOCK_HIDDEN;
		}
		else {
			/* hide unused sockets */
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(sock->link==NULL)
					sock->flag |= SOCK_HIDDEN;
			}
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(nodeCountSocketLinks(snode->edittree, sock)==0)
					sock->flag |= SOCK_HIDDEN;
			}
		}
	}

	allqueue(REDRAWNODE, 1);
	snode_verify_groups(snode);
	BIF_undo_push("Hide/Unhide sockets");

}

static int do_header_node(SpaceNode *snode, bNode *node, float mx, float my)
{
	rctf totr= node->totr;
	
	totr.ymin= totr.ymax-20.0f;
	
	totr.xmax= totr.xmin+15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		node->flag |= NODE_HIDDEN;
		allqueue(REDRAWNODE, 0);
		return 1;
	}	
	
	totr.xmax= node->totr.xmax;
	totr.xmin= totr.xmax-18.0f;
	if(node->typeinfo->flag & NODE_PREVIEW) {
		if(BLI_in_rctf(&totr, mx, my)) {
			node->flag ^= NODE_PREVIEW;
			allqueue(REDRAWNODE, 0);
			return 1;
		}
		totr.xmin-=18.0f;
	}
	if(node->type == NODE_GROUP) {
		if(BLI_in_rctf(&totr, mx, my)) {
			snode_make_group_editable(snode, node);
			return 1;
		}
		totr.xmin-=18.0f;
	}
	if(node->typeinfo->flag & NODE_OPTIONS) {
		if(BLI_in_rctf(&totr, mx, my)) {
			node->flag ^= NODE_OPTIONS;
			allqueue(REDRAWNODE, 0);
			return 1;
		}
		totr.xmin-=18.0f;
	}
	/* hide unused sockets */
	if(BLI_in_rctf(&totr, mx, my)) {
		node_hide_unhide_sockets(snode, node);
	}
	
	
	totr= node->totr;
	totr.xmin= totr.xmax-10.0f;
	totr.ymax= totr.ymin+10.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		scale_node(snode, node);
		return 1;
	}
	return 0;
}

static int do_header_hidden_node(SpaceNode *snode, bNode *node, float mx, float my)
{
	rctf totr= node->totr;
	
	totr.xmax= totr.xmin+15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		node->flag &= ~NODE_HIDDEN;
		allqueue(REDRAWNODE, 0);
		return 1;
	}	
	
	totr.xmax= node->totr.xmax;
	totr.xmin= node->totr.xmax-15.0f;
	if(BLI_in_rctf(&totr, mx, my)) {
		scale_node(snode, node);
		return 1;
	}
	return 0;
}

static void node_link_viewer(SpaceNode *snode, bNode *tonode)
{
	bNode *node;

	/* context check */
	if(tonode==NULL || tonode->outputs.first==NULL)
		return;
	if( ELEM(tonode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) 
		return;
	
	/* get viewer */
	for(node= snode->edittree->nodes.first; node; node= node->next)
		if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) 
			if(node->flag & NODE_DO_OUTPUT)
				break;
		
	if(node) {
		bNodeLink *link;
		
		/* get link to viewer */
		for(link= snode->edittree->links.first; link; link= link->next)
			if(link->tonode==node)
				break;

		if(link) {
			link->fromnode= tonode;
			link->fromsock= tonode->outputs.first;
			NodeTagChanged(snode->edittree, node);
			
			snode_handle_recalc(snode);
		}
	}
}


void node_active_link_viewer(SpaceNode *snode)
{
	bNode *node= editnode_get_active(snode->edittree);
	if(node)
		node_link_viewer(snode, node);
}

/* return 0: nothing done */
static int node_mouse_select(SpaceNode *snode, unsigned short event)
{
	bNode *node;
	float mx, my;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	for(next_node(snode->edittree); (node=next_node(NULL));) {
		
		/* first check for the headers or scaling widget */
		if(node->flag & NODE_HIDDEN) {
			if(do_header_hidden_node(snode, node, mx, my))
				return 1;
		}
		else {
			if(do_header_node(snode, node, mx, my))
				return 1;
		}
		
		/* node body */
		if(BLI_in_rctf(&node->totr, mx, my))
			break;
	}
	if(node) {
		if((G.qual & LR_SHIFTKEY)==0)
			node_deselectall(snode, 0);
		
		if(G.qual & LR_SHIFTKEY) {
			if(node->flag & SELECT)
				node->flag &= ~SELECT;
			else
				node->flag |= SELECT;
		}
		else 
			node->flag |= SELECT;
		
		node_set_active(snode, node);
		
		/* viewer linking */
		if(G.qual & LR_CTRLKEY)
			node_link_viewer(snode, node);
		
		/* not so nice (no event), but function below delays redraw otherwise */
		force_draw(0);
		
		std_rmouse_transform(node_transform_ext);	/* does undo push for select */
		
		return 1;
	}
	return 0;
}

/* return 0, nothing done */
static int node_mouse_groupheader(SpaceNode *snode)
{
	bNode *gnode;
	float mx, my;
	short mval[2];
	
	gnode= snode_get_editgroup(snode);
	if(gnode==NULL) return 0;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* click in header or outside? */
	if(BLI_in_rctf(&gnode->totr, mx, my)==0) {
		rctf rect= gnode->totr;
		
		rect.ymax += NODE_DY;
		if(BLI_in_rctf(&rect, mx, my)==0)
			snode_make_group_editable(snode, NULL);	/* toggles, so exits editmode */
		else
			transform_nodes(snode->nodetree, 'g', "Move group");
		
		return 1;
	}
	return 0;
}

static int node_socket_hilights(SpaceNode *snode, int in_out)
{
	bNode *node;
	bNodeSocket *sock, *tsock, *socksel= NULL;
	float mx, my;
	short mval[2], redraw= 0;
	
	if(snode->edittree==NULL) return 0;
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &mx, &my);
	
	/* deselect socks */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next) {
			if(sock->flag & SELECT) {
				sock->flag &= ~SELECT;
				redraw++;
				socksel= sock;
			}
		}
		for(sock= node->outputs.first; sock; sock= sock->next) {
			if(sock->flag & SELECT) {
				sock->flag &= ~SELECT;
				redraw++;
				socksel= sock;
			}
		}
	}
	
	if(find_indicated_socket(snode, &node, &tsock, in_out)) {
		tsock->flag |= SELECT;
		if(redraw==1 && tsock==socksel) redraw= 0;
		else redraw= 1;
	}
	
	return redraw;
}

void node_border_select(SpaceNode *snode)
{
	bNode *node;
	rcti rect;
	rctf rectf;
	short val, mval[2];
	
	if ( (val = get_border(&rect, 3)) ) {
		
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);
		
		for(node= snode->edittree->nodes.first; node; node= node->next) {
			if(BLI_isect_rctf(&rectf, &node->totr, NULL)) {
				if(val==LEFTMOUSE)
					node->flag |= SELECT;
				else
					node->flag &= ~SELECT;
			}
		}
		allqueue(REDRAWNODE, 1);
		BIF_undo_push("Border select nodes");
	}		
}

/* ****************** Add *********************** */

void snode_autoconnect(SpaceNode *snode, bNode *node_to, int flag)
{
	bNodeSocket *sock, *sockfrom[8];
	bNode *node, *nodefrom[8];
	int totsock= 0, socktype=0;

	if(node_to==NULL || node_to->inputs.first==NULL)
		return;
	
	/* no inputs for node allowed (code it) */

	/* connect first 1 socket type now */
	for(sock= node_to->inputs.first; sock; sock= sock->next)
		if(socktype<sock->type)
			socktype= sock->type;

	
	/* find potential sockets, max 8 should work */
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if((node->flag & flag) && node!=node_to) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(!(sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL))) {
					sockfrom[totsock]= sock;
					nodefrom[totsock]= node;
					totsock++;
					if(totsock>7)
						break;
				}
			}
		}
		if(totsock>7)
			break;
	}

	/* now just get matching socket types and create links */
	for(sock= node_to->inputs.first; sock; sock= sock->next) {
		int a;
		
		for(a=0; a<totsock; a++) {
			if(sockfrom[a]) {
				if(sock->type==sockfrom[a]->type && sock->type==socktype) {
					nodeAddLink(snode->edittree, nodefrom[a], sockfrom[a], node_to, sock);
					sockfrom[a]= NULL;
					break;
				}
			}
		}
	}
	
	ntreeSolveOrder(snode->edittree);
}

/* can be called from menus too, but they should do own undopush and redraws */
bNode *node_add_node(SpaceNode *snode, int type, float locx, float locy)
{
	bNode *node= NULL, *gnode;
	
	node_deselectall(snode, 0);
	
	if(type>=NODE_DYNAMIC_MENU) {
		node= nodeAddNodeType(snode->edittree, type, NULL, NULL);
	}
	else if(type>=NODE_GROUP_MENU) {
		if(snode->edittree!=snode->nodetree) {
			error("Can not add a Group in a Group");
			return NULL;
		}
		else {
			bNodeTree *ngroup= BLI_findlink(&G.main->nodetree, type-NODE_GROUP_MENU);
			if(ngroup)
				node= nodeAddNodeType(snode->edittree, NODE_GROUP, ngroup, NULL);
		}
	}
	else
		node= nodeAddNodeType(snode->edittree, type, NULL, NULL);
	
	/* generics */
	if(node) {
		node->locx= locx;
		node->locy= locy + 60.0f;		// arbitrary.. so its visible
		node->flag |= SELECT;
		
		gnode= snode_get_editgroup(snode);
		if(gnode) {
			node->locx -= gnode->locx;
			node->locy -= gnode->locy;
		}

		snode_verify_groups(snode);
		node_set_active(snode, node);
		
		if(node->id)
			id_us_plus(node->id);
		
		if(snode->nodetree->type==NTREE_COMPOSIT)
			ntreeCompositForceHidden(snode->edittree);
		
		NodeTagChanged(snode->edittree, node);
	}
	return node;
}

void node_mute(SpaceNode *snode)
{
	bNode *node;

	/* no disabling inside of groups */
	if(snode_get_editgroup(snode))
		return;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->inputs.first && node->outputs.first) {
				if(node->flag & NODE_MUTED)
					node->flag &= ~NODE_MUTED;
				else
					node->flag |= NODE_MUTED;
			}
		}
	}
	
	allqueue(REDRAWNODE, 0);
	BIF_undo_push("Enable/Disable nodes");

}

void node_adduplicate(SpaceNode *snode)
{
	
	ntreeCopyTree(snode->edittree, 1);	/* 1 == internally selected nodes */
	
	ntreeSolveOrder(snode->edittree);
	snode_verify_groups(snode);
	snode_handle_recalc(snode);

	transform_nodes(snode->edittree, 'g', "Duplicate");
}

#if 0
static void node_insert_convertor(SpaceNode *snode, bNodeLink *link)
{
	bNode *newnode= NULL;
	
	if(link->fromsock->type==SOCK_RGBA && link->tosock->type==SOCK_VALUE) {
		if(snode->edittree->type==NTREE_SHADER)
			newnode= node_add_node(snode, SH_NODE_RGBTOBW, 0.0f, 0.0f);
		else if(snode->edittree->type==NTREE_COMPOSIT)
			newnode= node_add_node(snode, CMP_NODE_RGBTOBW, 0.0f, 0.0f);
		else
			newnode= NULL;
	}
	else if(link->fromsock->type==SOCK_VALUE && link->tosock->type==SOCK_RGBA) {
		if(snode->edittree->type==NTREE_SHADER)
			newnode= node_add_node(snode, SH_NODE_VALTORGB, 0.0f, 0.0f);
		else if(snode->edittree->type==NTREE_COMPOSIT)
			newnode= node_add_node(snode, CMP_NODE_VALTORGB, 0.0f, 0.0f);
		else
			newnode= NULL;
	}
	
	if(newnode) {
		/* dangerous assumption to use first in/out socks, but thats fine for now */
		newnode->flag |= NODE_HIDDEN;
		newnode->locx= 0.5f*(link->fromsock->locx + link->tosock->locx);
		newnode->locy= 0.5f*(link->fromsock->locy + link->tosock->locy) + HIDDEN_RAD;
		
		nodeAddLink(snode->edittree, newnode, newnode->outputs.first, link->tonode, link->tosock);
		link->tonode= newnode;
		link->tosock= newnode->inputs.first;
	}
}

#endif

static void node_remove_extra_links(SpaceNode *snode, bNodeSocket *tsock, bNodeLink *link)
{
	bNodeLink *tlink;
	bNodeSocket *sock;
	
	if(tsock && nodeCountSocketLinks(snode->edittree, link->tosock) > tsock->limit) {
		
		for(tlink= snode->edittree->links.first; tlink; tlink= tlink->next) {
			if(link!=tlink && tlink->tosock==link->tosock)
				break;
		}
		if(tlink) {
			/* is there a free input socket with same type? */
			for(sock= tlink->tonode->inputs.first; sock; sock= sock->next) {
				if(sock->type==tlink->fromsock->type)
					if(nodeCountSocketLinks(snode->edittree, sock) < sock->limit)
						break;
			}
			if(sock) {
				tlink->tosock= sock;
				sock->flag &= ~SOCK_HIDDEN;
			}
			else {
				nodeRemLink(snode->edittree, tlink);
			}
		}
	}
}

/* loop that adds a nodelink, called by function below  */
/* in_out = starting socket */
static int node_add_link_drag(SpaceNode *snode, bNode *node, bNodeSocket *sock, int in_out)
{
	bNode *tnode;
	bNodeSocket *tsock= NULL;
	bNodeLink *link= NULL;
	short mval[2], mvalo[2], firsttime=1;	/* firsttime reconnects a link broken by caller */
	
	/* we make a temporal link */
	if(in_out==SOCK_OUT)
		link= nodeAddLink(snode->edittree, node, sock, NULL, NULL);
	else
		link= nodeAddLink(snode->edittree, NULL, NULL, node, sock);
	
	getmouseco_areawin(mvalo);
	while (get_mbut() & L_MOUSE) {
		
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || firsttime) {
			firsttime= 0;
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			if(in_out==SOCK_OUT) {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_IN)) {
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if(tnode!=node  && link->tonode!=tnode && link->tosock!= tsock) {
							link->tonode= tnode;
							link->tosock= tsock;
							ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
						}
					}
				}
				else {
					link->tonode= NULL;
					link->tosock= NULL;
				}
			}
			else {
				if(find_indicated_socket(snode, &tnode, &tsock, SOCK_OUT)) {
					if(nodeFindLink(snode->edittree, sock, tsock)==NULL) {
						if(nodeCountSocketLinks(snode->edittree, tsock) < tsock->limit) {
							if(tnode!=node && link->fromnode!=tnode && link->fromsock!= tsock) {
								link->fromnode= tnode;
								link->fromsock= tsock;
								ntreeSolveOrder(snode->edittree);	/* for interactive red line warning */
							}
						}
					}
				}
				else {
					link->fromnode= NULL;
					link->fromsock= NULL;
				}
			}
			/* hilight target sockets only */
			node_socket_hilights(snode, in_out==SOCK_OUT?SOCK_IN:SOCK_OUT);
			
			force_draw(0);
		}
		else BIF_wait_for_statechange();		
	}
	
	/* remove link? */
	if(link->tonode==NULL || link->fromnode==NULL) {
		nodeRemLink(snode->edittree, link);
	}
	else {
		/* send changed events for original tonode and new */
		if(link->tonode) 
			NodeTagChanged(snode->edittree, link->tonode);
		
		/* we might need to remove a link */
		if(in_out==SOCK_OUT) node_remove_extra_links(snode, tsock, link);
	}
	
	ntreeSolveOrder(snode->edittree);
	snode_verify_groups(snode);
	snode_handle_recalc(snode);
	
	allqueue(REDRAWNODE, 0);
	BIF_undo_push("Add link");

	return 1;
}

/* return 1 when socket clicked */
static int node_add_link(SpaceNode *snode)
{
	bNode *node;
	bNodeLink *link;
	bNodeSocket *sock;
	
	/* output indicated? */
	if(find_indicated_socket(snode, &node, &sock, SOCK_OUT)) {
		if(nodeCountSocketLinks(snode->edittree, sock)<sock->limit)
			return node_add_link_drag(snode, node, sock, SOCK_OUT);
		else {
			/* find if we break a link */
			for(link= snode->edittree->links.first; link; link= link->next) {
				if(link->fromsock==sock)
					break;
			}
			if(link) {
				node= link->tonode;
				sock= link->tosock;
				nodeRemLink(snode->edittree, link);
				return node_add_link_drag(snode, node, sock, SOCK_IN);
			}
		}
	}
	/* or an input? */
	else if(find_indicated_socket(snode, &node, &sock, SOCK_IN)) {
		if(nodeCountSocketLinks(snode->edittree, sock)<sock->limit)
			return node_add_link_drag(snode, node, sock, SOCK_IN);
		else {
			/* find if we break a link */
			for(link= snode->edittree->links.first; link; link= link->next) {
				if(link->tosock==sock)
					break;
			}
			if(link) {
				/* send changed event to original tonode */
				if(link->tonode) 
					NodeTagChanged(snode->edittree, link->tonode);
				
				node= link->fromnode;
				sock= link->fromsock;
				nodeRemLink(snode->edittree, link);
				return node_add_link_drag(snode, node, sock, SOCK_OUT);
			}
		}
	}
	
	return 0;
}

void node_delete(SpaceNode *snode)
{
	bNode *node, *next;
	bNodeSocket *sock;
	
	for(node= snode->edittree->nodes.first; node; node= next) {
		next= node->next;
		if(node->flag & SELECT) {
			/* set selin and selout NULL if the sockets belong to a node to be deleted */
			for(sock= node->inputs.first; sock; sock= sock->next)
				if(snode->edittree->selin == sock) snode->edittree->selin= NULL;

			for(sock= node->outputs.first; sock; sock= sock->next)
				if(snode->edittree->selout == sock) snode->edittree->selout= NULL;

			/* check id user here, nodeFreeNode is called for free dbase too */
			if(node->id)
				node->id->us--;
			nodeFreeNode(snode->edittree, node);
		}
	}
	
	snode_verify_groups(snode);
	snode_handle_recalc(snode);
	BIF_undo_push("Delete nodes");
	allqueue(REDRAWNODE, 1);
}

void node_hide(SpaceNode *snode)
{
	bNode *node;
	int nothidden=0, ishidden=0;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if(node->flag & NODE_HIDDEN)
				ishidden++;
			else
				nothidden++;
		}
	}
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->flag & SELECT) {
			if( (ishidden && nothidden) || ishidden==0)
				node->flag |= NODE_HIDDEN;
			else 
				node->flag &= ~NODE_HIDDEN;
		}
	}
	BIF_undo_push("Hide nodes");
	allqueue(REDRAWNODE, 1);
}

void node_insert_key(SpaceNode *snode)
{
	bNode *node= editnode_get_active(snode->edittree);

	if(node->type==CMP_NODE_TIME) {
		if(node->custom1<node->custom2) {

			CurveMapping *cumap= node->storage;
			float fval, curval;
		
			curval= (float)(CFRA - node->custom1)/(float)(node->custom2-node->custom1);
			fval= curvemapping_evaluateF(cumap, 0, curval);
			
			if(fbutton(&fval, 0.0f, 1.0f, 10, 10, "Insert Value")) {
				curvemap_insert(cumap->cm, curval, fval);

				BIF_undo_push("Insert key in Time node");
				allqueue(REDRAWNODE, 1);
			}
		}
	}
}

void node_select_linked(SpaceNode *snode, int out)
{
	bNodeLink *link;
	bNode *node;
	
	/* NODE_TEST is the free flag */
	for(node= snode->edittree->nodes.first; node; node= node->next)
		node->flag &= ~NODE_TEST;

	for(link= snode->edittree->links.first; link; link= link->next) {
		if(out) {
			if(link->fromnode->flag & NODE_SELECT)
				link->tonode->flag |= NODE_TEST;
		}
		else {
			if(link->tonode->flag & NODE_SELECT)
				link->fromnode->flag |= NODE_TEST;
		}
	}
	
	for(node= snode->edittree->nodes.first; node; node= node->next)
		if(node->flag & NODE_TEST)
			node->flag |= NODE_SELECT;
	
	BIF_undo_push("Select Linked nodes");
	allqueue(REDRAWNODE, 1);
}

/* makes a link between selected output and input sockets */
void node_make_link(SpaceNode *snode)
{
	bNode *fromnode, *tonode;
	bNodeLink *link;
	bNodeSocket *outsock= snode->edittree->selout;
	bNodeSocket *insock= snode->edittree->selin;

	if(!insock || !outsock) return;
	if(nodeFindLink(snode->edittree, outsock, insock)) return;

	if(nodeFindNode(snode->edittree, outsock, &fromnode, NULL) &&
		nodeFindNode(snode->edittree, insock, &tonode, NULL)) {
		link= nodeAddLink(snode->edittree, fromnode, outsock, tonode, insock);
		NodeTagChanged(snode->edittree, tonode);
		node_remove_extra_links(snode, insock, link);
	}
	else return;

	ntreeSolveOrder(snode->edittree);
	snode_verify_groups(snode);
	snode_handle_recalc(snode);

	allqueue(REDRAWNODE, 0);
	BIF_undo_push("Make Link Between Sockets");
}

static void node_border_link_delete(SpaceNode *snode)
{
	rcti rect;
	short val, mval[2], mvalo[2];

	/* to make this work more friendly, we first wait for a mouse move */
	getmouseco_areawin(mvalo);
	while (get_mbut() & L_MOUSE) {
		getmouseco_areawin(mval);
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1])
			break;
		else BIF_wait_for_statechange();
	}
	if((get_mbut() & L_MOUSE)==0)
		return;
	
	/* now change cursor and draw border */
	setcursor_space(SPACE_NODE, CURSOR_VPAINT);
	
	if ( (val = get_border(&rect, 2)) ) {
		if(rect.xmin<rect.xmax && rect.ymin<rect.ymax) {
			//#define NODE_MAXPICKBUF	256
			bNodeLink *link, *next;
			GLuint buffer[256];
			rctf rectf;
			int code=0, hits;
			
			mval[0]= rect.xmin;
			mval[1]= rect.ymin;
			areamouseco_to_ipoco(&snode->v2d, mval, &rectf.xmin, &rectf.ymin);
			mval[0]= rect.xmax;
			mval[1]= rect.ymax;
			areamouseco_to_ipoco(&snode->v2d, mval, &rectf.xmax, &rectf.ymax);
			
			glLoadIdentity();
			myortho2(rectf.xmin, rectf.xmax, rectf.ymin, rectf.ymax);
			
			glSelectBuffer(256, buffer); 
			glRenderMode(GL_SELECT);
			glInitNames();
			glPushName(-1);
			
			/* draw links */
			for(link= snode->edittree->links.first; link; link= link->next) {
				glLoadName(code++);
				node_draw_link(snode, link);
			}
			
			hits= glRenderMode(GL_RENDER);
			glPopName();
			if(hits>0) {
				int a;
				for(a=0; a<hits; a++) {
					bNodeLink *link= BLI_findlink(&snode->edittree->links, buffer[ (4 * a) + 3]);
					if(link)
						link->fromnode= NULL;	/* first tag for delete, otherwise indices are wrong */
				}
				for(link= snode->edittree->links.first; link; link= next) {
					next= link->next;
					if(link->fromnode==NULL) {
						NodeTagChanged(snode->edittree, link->tonode);
						nodeRemLink(snode->edittree, link);
					}
				}
				ntreeSolveOrder(snode->edittree);
				snode_verify_groups(snode);
				snode_handle_recalc(snode);
			}
			allqueue(REDRAWNODE, 0);
			BIF_undo_push("Erase links");
		}
	}
	
	setcursor_space(SPACE_NODE, CURSOR_STD);
}

/* goes over all scenes, reads render layerss */
void node_read_renderlayers(SpaceNode *snode)
{
	Scene *scene;
	bNode *node;

	/* first tag scenes unread */
	for(scene= G.main->scene.first; scene; scene= scene->id.next) 
		scene->id.flag |= LIB_DOIT;

	for(node= snode->edittree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			ID *id= node->id;
			if(id==NULL) id= (ID *)G.scene;
			if(id->flag & LIB_DOIT) {
				RE_ReadRenderResult(G.scene, (Scene *)id);
				ntreeCompositTagRender((Scene *)id);
				id->flag &= ~LIB_DOIT;
			}
		}
	}
	
	/* own render result should be read/allocated */
	if(G.scene->id.flag & LIB_DOIT)
		RE_ReadRenderResult(G.scene, G.scene);
	
	snode_handle_recalc(snode);
}

void node_read_fullsamplelayers(SpaceNode *snode)
{
	Render *re= RE_NewRender(G.scene->id.name);

	waitcursor(1);

	BIF_init_render_callbacks(re, 1);
	RE_MergeFullSample(re, G.scene, snode->nodetree);
	BIF_end_render_callbacks();
	
	allqueue(REDRAWNODE, 1);
	allqueue(REDRAWIMAGE, 1);
	
	waitcursor(0);
}

/* called from header_info, when deleting a scene
 * goes over all scenes other than the input, checks if they have
 * render layer nodes referencing the to-be-deleted scene, and
 * resets them to NULL. */
void clear_scene_in_nodes(Scene *sce)
{
	Scene *sce1;
	bNode *node;

	sce1= G.main->scene.first;
	while(sce1) {
		if(sce1!=sce) {
			if (sce1->nodetree) {
				for(node= sce1->nodetree->nodes.first; node; node= node->next) {
					if(node->type==CMP_NODE_R_LAYERS) {
						Scene *nodesce= (Scene *)node->id;
						
						if (nodesce==sce) node->id = NULL;
					}
				}
			}
		}
		sce1= sce1->id.next;
	}
}


/* gets active viewer user */
struct ImageUser *ntree_get_active_iuser(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree)
		for(node= ntree->nodes.first; node; node= node->next)
			if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) 
				if(node->flag & NODE_DO_OUTPUT)
					return node->storage;
	return NULL;
}

void imagepaint_composite_tags(bNodeTree *ntree, Image *image, ImageUser *iuser)
{
	bNode *node;
	
	if(ntree==NULL)
		return;
	
	/* search for renderresults */
	if(image->type==IMA_TYPE_R_RESULT) {
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->type==CMP_NODE_R_LAYERS && node->id==NULL) {
				/* imageuser comes from ImageWin, so indexes are offset 1 */
				if(node->custom1==iuser->layer-1)
					NodeTagChanged(ntree, node);
			}
		}
	}
	else {
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->id== &image->id)
				NodeTagChanged(ntree, node);
		}
	}
}

/* ********************** */

void node_make_group(SpaceNode *snode)
{
	bNode *gnode;
	
	if(snode->edittree!=snode->nodetree) {
		error("Can not add a new Group in a Group");
		return;
	}
	
	/* for time being... is too complex to handle */
	if(snode->treetype==NTREE_COMPOSIT) {
		for(gnode=snode->nodetree->nodes.first; gnode; gnode= gnode->next) {
			if(gnode->flag & SELECT)
				if(gnode->type==CMP_NODE_R_LAYERS)
					break;
		}
		if(gnode) {
			error("Can not add RenderLayer in a Group");
			return;
		}
	}
	
	gnode= nodeMakeGroupFromSelected(snode->nodetree);
	if(gnode==NULL) {
		error("Can not make Group");
	}
	else {
		nodeSetActive(snode->nodetree, gnode);
		ntreeSolveOrder(snode->nodetree);
		allqueue(REDRAWNODE, 0);
		BIF_undo_push("Make Node Group");
	}
}

/* ******************** main event loop ****************** */

/* special version to prevent overlapping buttons, has a bit of hack... */
/* yes, check for example composit_node_event(), file window use... */
static int node_uiDoBlocks(ScrArea *sa, short event)
{
	SpaceNode *snode= sa->spacedata.first;
	ListBase *lb= &sa->uiblocks;
	ListBase listb= *lb;
	uiBlock *block;
	bNode *node;
	rctf rect;
	void *prev, *next;
	int retval= UI_NOTHING;
	short mval[2];
	
	getmouseco_areawin(mval);
	areamouseco_to_ipoco(G.v2d, mval, &rect.xmin, &rect.ymin);

	/* this happens after filesel usage... */
	if(lb->first==NULL) {
		return UI_NOTHING;
	}
	
	/* evil hack: try to do grease-pencil floating panel (like for nodes) */
	block= uiGetBlock("nodes_panel_gpencil", sa);
	if (block) {
		/* try to process events here... if failed, just carry on */
			/* when there's menus, the prev pointer becomes zero! */
		prev= ((struct Link *)block)->prev;
		next= ((struct Link *)block)->next;
		((struct Link *)block)->prev= NULL;
		((struct Link *)block)->next= NULL;
		
		lb->first= lb->last= block;
		retval= uiDoBlocks(lb, event, 1);
		
		((struct Link *)block)->prev= prev;
		((struct Link *)block)->next= next;
		
		*lb= listb;
		
		/* if something happened, get the heck outta here */
		if (retval != UI_NOTHING)
			return retval;
	}
	
	
	rect.xmin -= 2.0f;
	rect.ymin -= 2.0f;
	rect.xmax = rect.xmin + 4.0f;
	rect.ymax = rect.ymin + 4.0f;
	
	for(node= snode->edittree->nodes.first; node; node= node->next) {
		char str[32];
		
		/* retreive unique block name, see also drawnode.c */
		sprintf(str, "node buttons %p", node);
		block= uiGetBlock(str, sa);
		
		if(block) {
			if(node == visible_node(snode, &rect)) {

				/* when there's menus, the prev pointer becomes zero! */
				prev= ((struct Link *)block)->prev;
				next= ((struct Link *)block)->next;
				((struct Link *)block)->prev= NULL;
				((struct Link *)block)->next= NULL;
				
				lb->first= lb->last= block;
				retval= uiDoBlocks(lb, event, 1);
				
				((struct Link *)block)->prev= prev;
				((struct Link *)block)->next= next;

				break;
			}
		}
	}
	
	*lb= listb;
	
	return retval;
}

void winqreadnodespace(ScrArea *sa, void *spacedata, BWinEvent *evt)
{
	SpaceNode *snode= spacedata;
	bNode *actnode;
	bNodeSocket *actsock;
	unsigned short event= evt->event;
	short val= evt->val, doredraw=0, fromlib= 0;
	
	if(sa->win==0) return;
	if(snode->nodetree==NULL) return;
	
	if(val) {
		if( node_uiDoBlocks(sa, event)!=UI_NOTHING ) event= 0;
		
		fromlib= (snode->id && snode->id->lib);
		
		switch(event) {
		case LEFTMOUSE:
			if(gpencil_do_paint(sa, L_MOUSE)) {
				return;
			}
			else if(fromlib) {
				if(node_mouse_groupheader(snode)==0)
					node_mouse_select(snode, event);
			}
			else {
				
				if(G.qual & LR_CTRLKEY)
					if(gesture())
						break;
					
				if(node_add_link(snode)==0)
					if(node_mouse_groupheader(snode)==0)
						if(node_mouse_select(snode, event)==0)
							node_border_link_delete(snode);
			}
			break;
			
		case RIGHTMOUSE: 
			if(gpencil_do_paint(sa, R_MOUSE)) {
				return;
			}
			else if(find_indicated_socket(snode, &actnode, &actsock, SOCK_IN)) {
				if(actsock->flag & SOCK_SEL) {
					snode->edittree->selin= NULL;
					actsock->flag&= ~SOCK_SEL;
				}
				else {
					snode->edittree->selin= actsock;
					reset_sel_socket(snode, SOCK_IN);
					actsock->flag|= SOCK_SEL;
				}
			}
			else if(find_indicated_socket(snode, &actnode, &actsock, SOCK_OUT)) {
				if(actsock->flag & SOCK_SEL) {
					snode->edittree->selout= NULL;
					actsock->flag&= ~SOCK_SEL;
				}
				else {
					snode->edittree->selout= actsock;
					reset_sel_socket(snode, SOCK_OUT);
					actsock->flag|= SOCK_SEL;
				}
			}
			else if(!node_mouse_select(snode, event)) 
				toolbox_n();

			break;
		case MIDDLEMOUSE:
			if((snode->flag & SNODE_BACKDRAW) && (snode->treetype==NTREE_COMPOSIT)
			   && (G.qual==LR_SHIFTKEY)) {
				snode_bg_viewmove(snode);
			} else {
				view2dmove(event);
			}
		case WHEELUPMOUSE:
		case WHEELDOWNMOUSE:
			view2dmove(event);	/* in drawipo.c */
			break;
			
		case MOUSEY:
			doredraw= node_socket_hilights(snode, SOCK_IN|SOCK_OUT);
			break;
		
		case UI_BUT_EVENT:
			/* future: handlerize this! */
			if(snode->treetype==NTREE_SHADER)
				shader_node_event(snode, val);
			else if(snode->treetype==NTREE_COMPOSIT)
				composit_node_event(snode, val);
			break;
			
		case RENDERPREVIEW:
			if(snode->treetype==NTREE_SHADER)
				shader_node_previewrender(sa, snode);
			break;
			
		case PADPLUSKEY:
			snode_zoom_in(sa);
			doredraw= 1;
			break;
		case PADMINUS:
			snode_zoom_out(sa);
			doredraw= 1;
			break;
		case HOMEKEY:
			snode_home(sa, snode);
			doredraw= 1;
			break;
		case TABKEY:
			if(fromlib) fromlib= -1;
			else snode_make_group_editable(snode, NULL);
			break;
			
		case AKEY:
			if(G.qual==LR_SHIFTKEY) {
				if(fromlib) fromlib= -1;
				else toolbox_n_add();
			}
			else if(G.qual==0) {
				node_deselectall(snode, 1);
				BIF_undo_push("Deselect all nodes");
			}
			break;
		case BKEY:
			if(G.qual==0)
				node_border_select(snode);
			break;
		case CKEY:	/* sort again, showing cyclics */
			ntreeSolveOrder(snode->edittree);
			doredraw= 1;
			break;
		case DKEY:
			if(G.qual==LR_SHIFTKEY) {
				if(fromlib) fromlib= -1;
				else node_adduplicate(snode);
			}
			break;
		case EKEY:
			snode_handle_recalc(snode);
			break;
		case FKEY:
			node_make_link(snode);
			break;
		case GKEY:
			if(fromlib) fromlib= -1;
			else {
				if(G.qual==LR_CTRLKEY) {
					if(okee("Make Group"))
						node_make_group(snode);
				}
				else if(G.qual==LR_ALTKEY) {
					if(okee("Ungroup"))
						node_ungroup(snode);
				}
				else if(G.qual==LR_SHIFTKEY) {
					node_addgroup(snode);
				}
				else
					transform_nodes(snode->edittree, 'g', "Move Node");
			}
			break;
		case HKEY:
			node_hide(snode);
			break;
		case IKEY:
			node_insert_key(snode);
			break;
		case LKEY:
			node_select_linked(snode, G.qual==LR_SHIFTKEY);
			break;
		case MKEY:
			node_mute(snode);
			break;
		case RKEY:
			if(G.qual==LR_CTRLKEY) {
				node_rename(snode);
			} 
			else if(G.qual==LR_SHIFTKEY) {
				if(okee("Read saved Full Sample Layers"))
					node_read_fullsamplelayers(snode);
			}
			else {
				if(okee("Read saved Render Layers"))
					node_read_renderlayers(snode);
			}
			break;
		case DELKEY:
		case XKEY:
			if(G.qual==LR_ALTKEY) {
				gpencil_delete_menu();
			}
			else {
				if(fromlib) fromlib= -1;
				else node_delete(snode);
			}
			break;
		}
	}

	if(fromlib==-1)
		error_libdata();
	if(doredraw)
		scrarea_queue_winredraw(sa);
	if(doredraw==2)
		scrarea_queue_headredraw(sa);
}


