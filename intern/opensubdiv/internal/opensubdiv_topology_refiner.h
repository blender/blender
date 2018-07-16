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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENSUBDIV_TOPOLOGY_REFINER_H__
#define __OPENSUBDIV_TOPOLOGY_REFINER_H__

#include <opensubdiv/far/topologyRefiner.h>

typedef struct OpenSubdiv_TopologyRefinerDescr {
	OpenSubdiv::Far::TopologyRefiner *osd_refiner;

	/* TODO(sergey): For now only, need to find better place
	 * after revisiting whole OSD drawing pipeline and Blender
	 * integration.
	 */
	std::vector<float> uvs;
} OpenSubdiv_TopologyRefinerDescr;

#endif  /* __OPENSUBDIV_TOPOLOGY_REFINER_H__ */
