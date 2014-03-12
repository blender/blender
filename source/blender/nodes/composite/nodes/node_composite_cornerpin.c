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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/composite/nodes/node_composite_cornerpin.c
 *  \ingroup cmpnodes
 */


#include "node_composite_util.h"

static bNodeSocketTemplate inputs[] = {
	{   SOCK_RGBA,      1, N_("Image"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{   SOCK_VECTOR,    1, N_("Upper Left"), 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{   SOCK_VECTOR,    1, N_("Upper Right"), 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{   SOCK_VECTOR,    1, N_("Lower Left"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{   SOCK_VECTOR,    1, N_("Lower Right"), 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
	{   -1, 0, ""   }
};

static bNodeSocketTemplate outputs[] = {
	{	SOCK_RGBA,   0,  N_("Image")},
	{	SOCK_FLOAT,  0,  N_("Plane")},
	{	-1, 0, ""	}
};

void register_node_type_cmp_cornerpin(void)
{
	static bNodeType ntype;

	cmp_node_type_base(&ntype, CMP_NODE_CORNERPIN, "Corner Pin", NODE_CLASS_DISTORT, 0);
	node_type_socket_templates(&ntype, inputs, outputs);

	nodeRegisterType(&ntype);
}
