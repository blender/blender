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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENSUBDIV_PATITIONED_H__
#define __OPENSUBDIV_PATITIONED_H__

#include <opensubdiv/osd/glMesh.h>
#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/glVertexBuffer.h>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {
namespace Osd {

/* TODO(sergey): Re-implement partitioning. */

#if 0
template <class PATCH_TABLE>
class PartitionedMeshInterface : public MeshInterface<PATCH_TABLE> {
	typedef PATCH_TABLE PatchTable;
	typedef typename PatchTable::VertexBufferBinding VertexBufferBinding;

public:
};

typedef PartitionedMeshInterface<GLPatchTable> PartitionedGLMeshInterface;

#endif

#if 0
template <typename VERTEX_BUFFER,
          typename STENCIL_TABLE,
          typename EVALUATOR,
          typename PATCH_TABLE,
          typename DEVICE_CONTEXT = void>
class PartitionedMesh : public Mesh<VERTEX_BUFFER,
                                    STENCIL_TABLE,
                                    EVALUATOR,
                                    PATCH_TABLE,
                                    DEVICE_CONTEXT>
{
};
#endif

#define PartitionedGLMeshInterface GLMeshInterface
#define PartitionedMesh Mesh

}  /* namespace Osd */
}  /* namespace OPENSUBDIV_VERSION */

using namespace OPENSUBDIV_VERSION;

}  /* namespace OpenSubdiv */

#endif  /* __OPENSUBDIV_PATITIONED_H__ */
