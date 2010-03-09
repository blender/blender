/**
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Robin Allen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../TEX_util.h"

/* **************** COMPOSITE ******************** */
static bNodeSocketType inputs[]= {
	{ SOCK_RGBA,   1, "Color",  0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{ SOCK_VECTOR, 1, "Normal", 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
	{ -1, 0, ""	}
};

/* applies to render pipeline */
static void exec(void *data, bNode *node, bNodeStack **in, bNodeStack **out)
{
	TexCallData *cdata = (TexCallData *)data;
	TexResult *target = cdata->target;
	
	if(cdata->do_preview) {
		TexParams params;
		params_from_cdata(&params, cdata);

		if(in[1]->hasinput && !in[0]->hasinput)
			tex_input_rgba(&target->tr, in[1], &params, cdata->thread);
		else
			tex_input_rgba(&target->tr, in[0], &params, cdata->thread);
		tex_do_preview(node, params.co, &target->tr);
	}
	else {
		/* 0 means don't care, so just use first */
		if(cdata->which_output == node->custom1 || (cdata->which_output == 0 && node->custom1 == 1)) {
			TexParams params;
			params_from_cdata(&params, cdata);
			
			tex_input_rgba(&target->tr, in[0], &params, cdata->thread);
		
			target->tin = (target->tr + target->tg + target->tb) / 3.0f;
			target->talpha = 1.0f;
		
			if(target->nor) {
				if(in[1]->hasinput)
					tex_input_vec(target->nor, in[1], &params, cdata->thread);
				else
					target->nor = 0;
			}
		}
	}
}

static void unique_name(bNode *node)
{
	TexNodeOutput *tno = (TexNodeOutput *)node->storage;
	char *new_name = 0;
	int new_len = 0;
	int suffix;
	bNode *i;
	char *name = tno->name;
	
	i = node;
	while(i->prev) i = i->prev;
	for(; i; i=i->next) {
		if(
			i == node ||
			i->type != TEX_NODE_OUTPUT ||
			strcmp(name, ((TexNodeOutput*)(i->storage))->name)
		)
			continue;
		
		if(!new_name) {
			int len = strlen(name);
			if(len >= 4 && sscanf(name + len - 4, ".%03d", &suffix) == 1) {
				new_len = len;
			} else {
				suffix = 0;
				new_len = len + 4;
				if(new_len > 31)
					new_len = 31;
			}
			
			new_name = MEM_mallocN(new_len + 1, "new_name");
			strcpy(new_name, name);
			name = new_name;
		}
		sprintf(new_name + new_len - 4, ".%03d", ++suffix);
	}
	
	if(new_name) {
		strcpy(tno->name, new_name);
		MEM_freeN(new_name);
	}
}

static void assign_index(struct bNode *node)
{
	bNode *tnode;
	int index = 1;
	
	tnode = node;
	while(tnode->prev)
		tnode = tnode->prev;
	
	check_index:
	for(; tnode; tnode= tnode->next)
		if(tnode->type == TEX_NODE_OUTPUT && tnode != node)
			if(tnode->custom1 == index) {
				index ++;
				goto check_index;
			}
			
	node->custom1 = index;
}

static void init(bNode *node)
{
	TexNodeOutput *tno = MEM_callocN(sizeof(TexNodeOutput), "TEX_output");
	node->storage= tno;
	
	strcpy(tno->name, "Default");
	unique_name(node);
	assign_index(node);
}

static void copy(bNode *orig, bNode *new)
{
	node_copy_standard_storage(orig, new);
	unique_name(new);
	assign_index(new);
}

bNodeType tex_node_output= {
	/* *next,*prev     */  NULL, NULL,
	/* type code       */  TEX_NODE_OUTPUT,
	/* name            */  "Output",
	/* width+range     */  150, 60, 200,
	/* class+opts      */  NODE_CLASS_OUTPUT, NODE_PREVIEW | NODE_OPTIONS, 
	/* input sock      */  inputs,
	/* output sock     */  NULL,
	/* storage         */  "TexNodeOutput",
	/* execfunc        */  exec,
	/* butfunc         */  NULL,
	/* initfunc        */  init,
	/* freestoragefunc */  node_free_standard_storage,
	/* copystoragefunc */  copy,  
	/* id              */  NULL
};

