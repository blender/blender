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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/shader/node_shader_util.c
 *  \ingroup nodes
 */


#include "DNA_node_types.h"

#include "node_shader_util.h"

#include "node_exec.h"


bool sh_node_poll_default(bNodeType *UNUSED(ntype), bNodeTree *ntree)
{
	return STREQ(ntree->idname, "ShaderNodeTree");
}

void sh_node_type_base(struct bNodeType *ntype, int type, const char *name, short nclass, short flag)
{
	node_type_base(ntype, type, name, nclass, flag);

	ntype->poll = sh_node_poll_default;
	ntype->insert_link = node_insert_link_default;
	ntype->update_internal_links = node_update_internal_links_default;
}

/* ****** */

void nodestack_get_vec(float *in, short type_in, bNodeStack *ns)
{
	const float *from = ns->vec;

	if (type_in == SOCK_FLOAT) {
		if (ns->sockettype == SOCK_FLOAT)
			*in = *from;
		else
			*in = (from[0] + from[1] + from[2]) / 3.0f;
	}
	else if (type_in == SOCK_VECTOR) {
		if (ns->sockettype == SOCK_FLOAT) {
			in[0] = from[0];
			in[1] = from[0];
			in[2] = from[0];
		}
		else {
			copy_v3_v3(in, from);
		}
	}
	else { /* type_in==SOCK_RGBA */
		if (ns->sockettype == SOCK_RGBA) {
			copy_v4_v4(in, from);
		}
		else if (ns->sockettype == SOCK_FLOAT) {
			in[0] = from[0];
			in[1] = from[0];
			in[2] = from[0];
			in[3] = 1.0f;
		}
		else {
			copy_v3_v3(in, from);
			in[3] = 1.0f;
		}
	}
}


void node_gpu_stack_from_data(struct GPUNodeStack *gs, int type, bNodeStack *ns)
{
	memset(gs, 0, sizeof(*gs));

	if (ns == NULL) {
		/* node_get_stack() will generate NULL bNodeStack pointers for unknown/unsuported types of sockets... */
		zero_v4(gs->vec);
		gs->link = NULL;
		gs->type = GPU_NONE;
		gs->hasinput = false;
		gs->hasoutput = false;
		gs->sockettype = type;
	}
	else {
		nodestack_get_vec(gs->vec, type, ns);
		gs->link = ns->data;

		if (type == SOCK_FLOAT)
			gs->type = GPU_FLOAT;
		else if (type == SOCK_VECTOR)
			gs->type = GPU_VEC3;
		else if (type == SOCK_RGBA)
			gs->type = GPU_VEC4;
		else if (type == SOCK_SHADER)
			gs->type = GPU_CLOSURE;
		else
			gs->type = GPU_NONE;

		gs->hasinput = ns->hasinput && ns->data;
		/* XXX Commented out the ns->data check here, as it seems it's not always set,
		 *     even though there *is* a valid connection/output... But that might need
		 *     further investigation.
		 */
		gs->hasoutput = ns->hasoutput /*&& ns->data*/;
		gs->sockettype = ns->sockettype;
	}
}

void node_data_from_gpu_stack(bNodeStack *ns, GPUNodeStack *gs)
{
	copy_v4_v4(ns->vec, gs->vec);
	ns->data = gs->link;
	ns->sockettype = gs->sockettype;
}

static void gpu_stack_from_data_list(GPUNodeStack *gs, ListBase *sockets, bNodeStack **ns)
{
	bNodeSocket *sock;
	int i;

	for (sock = sockets->first, i = 0; sock; sock = sock->next, i++)
		node_gpu_stack_from_data(&gs[i], sock->type, ns[i]);

	gs[i].end = true;
}

static void data_from_gpu_stack_list(ListBase *sockets, bNodeStack **ns, GPUNodeStack *gs)
{
	bNodeSocket *sock;
	int i;

	for (sock = sockets->first, i = 0; sock; sock = sock->next, i++)
		node_data_from_gpu_stack(ns[i], &gs[i]);
}

bNode *nodeGetActiveTexture(bNodeTree *ntree)
{
	/* this is the node we texture paint and draw in textured draw */
	bNode *node, *tnode, *inactivenode = NULL, *activetexnode = NULL, *activegroup = NULL;
	bool hasgroup = false;

	if (!ntree)
		return NULL;

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node->flag & NODE_ACTIVE_TEXTURE) {
			activetexnode = node;
			/* if active we can return immediately */
			if (node->flag & NODE_ACTIVE)
				return node;
		}
		else if (!inactivenode && node->typeinfo->nclass == NODE_CLASS_TEXTURE)
			inactivenode = node;
		else if (node->type == NODE_GROUP) {
			if (node->flag & NODE_ACTIVE)
				activegroup = node;
			else
				hasgroup = true;
		}
	}

	/* first, check active group for textures */
	if (activegroup) {
		tnode = nodeGetActiveTexture((bNodeTree *)activegroup->id);
		/* active node takes priority, so ignore any other possible nodes here */
		if (tnode)
			return tnode;
	}

	if (activetexnode)
		return activetexnode;

	if (hasgroup) {
		/* node active texture node in this tree, look inside groups */
		for (node = ntree->nodes.first; node; node = node->next) {
			if (node->type == NODE_GROUP) {
				tnode = nodeGetActiveTexture((bNodeTree *)node->id);
				if (tnode && ((tnode->flag & NODE_ACTIVE_TEXTURE) || !inactivenode))
					return tnode;
			}
		}
	}

	return inactivenode;
}

void ntreeExecGPUNodes(bNodeTreeExec *exec, GPUMaterial *mat, int do_outputs)
{
	bNodeExec *nodeexec;
	bNode *node;
	int n;
	bNodeStack *stack;
	bNodeStack *nsin[MAX_SOCKET];   /* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];  /* arbitrary... watch this */
	GPUNodeStack gpuin[MAX_SOCKET + 1], gpuout[MAX_SOCKET + 1];
	bool do_it;

	stack = exec->stack;

	for (n = 0, nodeexec = exec->nodeexec; n < exec->totnodes; ++n, ++nodeexec) {
		node = nodeexec->node;

		do_it = false;
		/* for groups, only execute outputs for edited group */
		if (node->typeinfo->nclass == NODE_CLASS_OUTPUT) {
			if (do_outputs && (node->flag & NODE_DO_OUTPUT))
				do_it = true;
		}
		else {
			do_it = true;
		}

		if (do_it) {
			if (node->typeinfo->gpufunc) {
				node_get_stack(node, stack, nsin, nsout);
				gpu_stack_from_data_list(gpuin, &node->inputs, nsin);
				gpu_stack_from_data_list(gpuout, &node->outputs, nsout);
				if (node->typeinfo->gpufunc(mat, node, &nodeexec->data, gpuin, gpuout))
					data_from_gpu_stack_list(&node->outputs, nsout, gpuout);
			}
		}
	}
}

void node_shader_gpu_tex_mapping(GPUMaterial *mat, bNode *node, GPUNodeStack *in, GPUNodeStack *UNUSED(out))
{
	NodeTexBase *base = node->storage;
	TexMapping *texmap = &base->tex_mapping;
	float domin = (texmap->flag & TEXMAP_CLIP_MIN) != 0;
	float domax = (texmap->flag & TEXMAP_CLIP_MAX) != 0;

	if (domin || domax || !(texmap->flag & TEXMAP_UNIT_MATRIX)) {
		GPUNodeLink *tmat = GPU_uniform((float *)texmap->mat);
		GPUNodeLink *tmin = GPU_uniform(texmap->min);
		GPUNodeLink *tmax = GPU_uniform(texmap->max);
		GPUNodeLink *tdomin = GPU_uniform(&domin);
		GPUNodeLink *tdomax = GPU_uniform(&domax);

		GPU_link(mat, "mapping", in[0].link, tmat, tmin, tmax, tdomin, tdomax, &in[0].link);

		if (texmap->type == TEXMAP_TYPE_NORMAL)
			GPU_link(mat, "texco_norm", in[0].link, &in[0].link);
	}
}
