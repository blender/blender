/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BSP_FragNode_h

#define BSP_FragNode_h

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "BSP_FragTree.h"
#include "BSP_MeshFragment.h"
#include "MT_Plane3.h"

class BSP_CSGISplitter;

class BSP_FragNode : public MEM_NonCopyable
{
private:

	/**
	 * The plane defining this node.
	 */

	MT_Plane3 m_plane;

	/**
	 * Children of this node.
	 */

	BSP_FragTree m_in_tree;
	BSP_FragTree m_out_tree;

private :

	BSP_FragNode(
		const MT_Plane3 & plane,
		BSP_CSGMesh *mesh
	);
	
public :

	/**
	 * Public methods
	 * Should only be called by BSP_FragTree
	 */

	~BSP_FragNode(
	);

	static
		MEM_SmartPtr<BSP_FragNode>
	New(
		const MT_Plane3 & plane,
		BSP_CSGMesh *mesh
	);

		void
	Build(
		BSP_MeshFragment *frag,
		BSP_CSGISplitter & splitter
	);

		void
	Push(
		BSP_MeshFragment *in_frag,
		BSP_MeshFragment *output,
		const BSP_Classification keep,
		const bool dominant,
		BSP_CSGISplitter & splitter
	);		

		void
	Classify(
		BSP_MeshFragment * frag,
		BSP_MeshFragment *in_frag,
		BSP_MeshFragment *out_frag,
		BSP_MeshFragment *on_frag,
		BSP_CSGISplitter & splitter
	);

	/**
	 * Accessor methods
	 */

		BSP_FragTree &
	InTree(
	);

		BSP_FragTree &
	OutTree(
	);
		
		MT_Plane3&
	Plane(
	);

};

#endif

