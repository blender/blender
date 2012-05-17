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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jeroen Bakker.
 *                 Monique Dewanchand
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_colorcorrection.c
 *  \ingroup cmpnodes
 */



#include "node_composite_util.h"


/* ******************* Color Balance ********************************* */
static bNodeSocketTemplate cmp_node_colorcorrection_in[]={
	{	SOCK_RGBA,1,"Image", 1.0f, 1.0f, 1.0f, 1.0f},
	{	SOCK_FLOAT, 1, "Mask",	1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_FACTOR},
	{	-1,0,""}
};

static bNodeSocketTemplate cmp_node_colorcorrection_out[]={
	{	SOCK_RGBA,0,"Image"},
	{	-1,0,""}
};

static void node_composit_init_colorcorrection(bNodeTree *UNUSED(ntree), bNode* node, bNodeTemplate *UNUSED(ntemp))
{
	NodeColorCorrection *n= node->storage= MEM_callocN(sizeof(NodeColorCorrection), "node colorcorrection");
	n->startmidtones = 0.2f;
	n->endmidtones = 0.7f;
	n->master.contrast = 1.0f;
	n->master.gain = 1.0f;
	n->master.gamma = 1.0f;
	n->master.lift= 0.0f;
	n->master.saturation= 1.0f;
	n->midtones.contrast = 1.0f;
	n->midtones.gain = 1.0f;
	n->midtones.gamma = 1.0f;
	n->midtones.lift= 0.0f;
	n->midtones.saturation= 1.0f;
	n->shadows.contrast = 1.0f;
	n->shadows.gain = 1.0f;
	n->shadows.gamma = 1.0f;
	n->shadows.lift= 0.0f;
	n->shadows.saturation= 1.0f;
	n->highlights.contrast = 1.0f;
	n->highlights.gain = 1.0f;
	n->highlights.gamma = 1.0f;
	n->highlights.lift= 0.0f;
	n->highlights.saturation= 1.0f;
	node->custom1 = 7; // red + green + blue enabled
}

void register_node_type_cmp_colorcorrection(bNodeTreeType *ttype)
{
	static bNodeType ntype;

	node_type_base(ttype, &ntype, CMP_NODE_COLORCORRECTION, "Color Correction", NODE_CLASS_OP_COLOR, NODE_OPTIONS);
	node_type_socket_templates(&ntype, cmp_node_colorcorrection_in, cmp_node_colorcorrection_out);
	node_type_size(&ntype, 400, 200, 400);
	node_type_init(&ntype, node_composit_init_colorcorrection);
	node_type_storage(&ntype, "NodeColorCorrection", node_free_standard_storage, node_copy_standard_storage);

	nodeRegisterType(ttype, &ntype);
}
