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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "openvdb_dense_convert.h"

#include <openvdb/tools/ValueTransformer.h>  /* for tools::foreach */

namespace internal {

openvdb::Mat4R convertMatrix(const float mat[4][4])
{
	return openvdb::Mat4R(
	        mat[0][0], mat[0][1], mat[0][2], mat[0][3],
	        mat[1][0], mat[1][1], mat[1][2], mat[1][3],
	        mat[2][0], mat[2][1], mat[2][2], mat[2][3],
	        mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
}


class MergeScalarGrids {
	typedef openvdb::FloatTree ScalarTree;

	openvdb::tree::ValueAccessor<const ScalarTree> m_acc_x, m_acc_y, m_acc_z;

public:
	MergeScalarGrids(const ScalarTree *x_tree, const ScalarTree *y_tree, const ScalarTree *z_tree)
	    : m_acc_x(*x_tree)
	    , m_acc_y(*y_tree)
	    , m_acc_z(*z_tree)
	{}

	MergeScalarGrids(const MergeScalarGrids &other)
	    : m_acc_x(other.m_acc_x)
	    , m_acc_y(other.m_acc_y)
	    , m_acc_z(other.m_acc_z)
	{}

	void operator()(const openvdb::Vec3STree::ValueOnIter &it) const
	{
		using namespace openvdb;

		const math::Coord xyz = it.getCoord();
		float x = m_acc_x.getValue(xyz);
		float y = m_acc_y.getValue(xyz);
		float z = m_acc_z.getValue(xyz);

		it.setValue(math::Vec3s(x, y, z));
	}
};

openvdb::GridBase *OpenVDB_export_vector_grid(
        OpenVDBWriter *writer,
        const openvdb::Name &name,
        const float *data_x, const float *data_y, const float *data_z,
        const int res[3],
        float fluid_mat[4][4],
        openvdb::VecType vec_type,
        const bool is_color,
        const openvdb::FloatGrid *mask)
{
	using namespace openvdb;

	math::CoordBBox bbox(Coord(0), Coord(res[0] - 1, res[1] - 1, res[2] - 1));
	Mat4R mat = convertMatrix(fluid_mat);
	math::Transform::Ptr transform = math::Transform::createLinearTransform(mat);

	FloatGrid::Ptr grid[3];

	grid[0] = FloatGrid::create(0.0f);
	tools::Dense<const float, tools::LayoutXYZ> dense_grid_x(bbox, data_x);
	tools::copyFromDense(dense_grid_x, grid[0]->tree(), TOLERANCE);

	grid[1] = FloatGrid::create(0.0f);
	tools::Dense<const float, tools::LayoutXYZ> dense_grid_y(bbox, data_y);
	tools::copyFromDense(dense_grid_y, grid[1]->tree(), TOLERANCE);

	grid[2] = FloatGrid::create(0.0f);
	tools::Dense<const float, tools::LayoutXYZ> dense_grid_z(bbox, data_z);
	tools::copyFromDense(dense_grid_z, grid[2]->tree(), TOLERANCE);

	Vec3SGrid::Ptr vecgrid = Vec3SGrid::create(Vec3s(0.0f));

	/* Activate voxels in the vector grid based on the scalar grids to ensure
	 * thread safety later on */
	for (int i = 0; i < 3; ++i) {
		vecgrid->tree().topologyUnion(grid[i]->tree());
	}

	MergeScalarGrids op(&(grid[0]->tree()), &(grid[1]->tree()), &(grid[2]->tree()));
	tools::foreach(vecgrid->beginValueOn(), op, true, false);

	vecgrid->setTransform(transform);

	/* Avoid clipping against an empty grid. */
	if (mask && !mask->tree().empty()) {
		vecgrid = tools::clip(*vecgrid, *mask);
	}

	vecgrid->setName(name);
	vecgrid->setIsInWorldSpace(false);
	vecgrid->setVectorType(vec_type);
	vecgrid->insertMeta("is_color", BoolMetadata(is_color));
	vecgrid->setGridClass(GRID_STAGGERED);

	writer->insert(vecgrid);

	return vecgrid.get();
}

void OpenVDB_import_grid_vector(
        OpenVDBReader *reader,
        const openvdb::Name &name,
        float **data_x, float **data_y, float **data_z,
        const int res[3])
{
	using namespace openvdb;

	if (!reader->hasGrid(name)) {
		std::fprintf(stderr, "OpenVDB grid %s not found in file!\n", name.c_str());
		memset(*data_x, 0, sizeof(float) * res[0] * res[1] * res[2]);
		memset(*data_y, 0, sizeof(float) * res[0] * res[1] * res[2]);
		memset(*data_z, 0, sizeof(float) * res[0] * res[1] * res[2]);
		return;
	}

	Vec3SGrid::Ptr vgrid = gridPtrCast<Vec3SGrid>(reader->getGrid(name));
	Vec3SGrid::ConstAccessor acc = vgrid->getConstAccessor();
	math::Coord xyz;
	int &x = xyz[0], &y = xyz[1], &z = xyz[2];

	size_t index = 0;
	for (z = 0; z < res[2]; ++z) {
		for (y = 0; y < res[1]; ++y) {
			for (x = 0; x < res[0]; ++x, ++index) {
				math::Vec3s value = acc.getValue(xyz);
				(*data_x)[index] = value.x();
				(*data_y)[index] = value.y();
				(*data_z)[index] = value.z();
			}
		}
	}
}

}  /* namespace internal */
