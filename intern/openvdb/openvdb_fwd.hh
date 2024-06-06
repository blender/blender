/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/* NOTE: version header included here to enable correct forward declaration of some types. No other
 * OpenVDB headers should be included here, especially openvdb.h, to avoid affecting other
 * compilation units. */
#include <openvdb/Types.h>
#include <openvdb/version.h>

/* -------------------------------------------------------------------- */
/** \name OpenVDB Forward Declaration
 * \{ */

/* Forward declaration for basic OpenVDB types. */
namespace openvdb {
OPENVDB_USE_VERSION_NAMESPACE
namespace OPENVDB_VERSION_NAME {

class GridBase;
class MetaMap;
template<typename TreeType> class Grid;

namespace math {
class Transform;
}

namespace tree {
class TreeBase;
template<typename T, Index Log2Dim> class LeafNode;
template<typename ChildNodeType, Index Log2Dim> class InternalNode;
template<typename ChildNodeType> class RootNode;
template<typename RootNodeType> class Tree;

/* Forward-declared version of Tree4, can't use the actual Tree4 alias because it can't be
 * forward-declared. */
template<typename T, Index N1 = 5, Index N2 = 4, Index N3 = 3> struct Tree4Fwd {
  using Type = openvdb::tree::Tree<openvdb::tree::RootNode<
      openvdb::tree::InternalNode<openvdb::tree::InternalNode<openvdb::tree::LeafNode<T, N3>, N2>,
                                  N1>>>;
};
}  // namespace tree

namespace tools {
template<typename T, Index Log2Dim> struct PointIndexLeafNode;
using PointIndexTree = tree::Tree<tree::RootNode<
    tree::InternalNode<tree::InternalNode<PointIndexLeafNode<PointIndex32, 3>, 4>, 5>>>;
using PointIndexGrid = Grid<PointIndexTree>;
}  // namespace tools

namespace points {
template<typename T, Index Log2Dim> class PointDataLeafNode;
using PointDataTree = tree::Tree<tree::RootNode<
    tree::InternalNode<tree::InternalNode<PointDataLeafNode<PointDataIndex32, 3>, 4>, 5>>>;
using PointDataGrid = Grid<PointDataTree>;
struct NullCodec;
template<typename ValueType, typename Codec> class TypedAttributeArray;
}  // namespace points

/// Common tree types
using BoolTree = tree::Tree4Fwd<bool, 5, 4, 3>::Type;
using DoubleTree = tree::Tree4Fwd<double, 5, 4, 3>::Type;
using FloatTree = tree::Tree4Fwd<float, 5, 4, 3>::Type;
using Int8Tree = tree::Tree4Fwd<int8_t, 5, 4, 3>::Type;
using Int32Tree = tree::Tree4Fwd<int32_t, 5, 4, 3>::Type;
using Int64Tree = tree::Tree4Fwd<int64_t, 5, 4, 3>::Type;
using MaskTree = tree::Tree4Fwd<ValueMask, 5, 4, 3>::Type;
using UInt32Tree = tree::Tree4Fwd<uint32_t, 5, 4, 3>::Type;
using Vec2DTree = tree::Tree4Fwd<Vec2d, 5, 4, 3>::Type;
using Vec2ITree = tree::Tree4Fwd<Vec2i, 5, 4, 3>::Type;
using Vec2STree = tree::Tree4Fwd<Vec2s, 5, 4, 3>::Type;
using Vec3DTree = tree::Tree4Fwd<Vec3d, 5, 4, 3>::Type;
using Vec3ITree = tree::Tree4Fwd<Vec3i, 5, 4, 3>::Type;
using Vec3STree = tree::Tree4Fwd<Vec3f, 5, 4, 3>::Type;
using Vec4STree = tree::Tree4Fwd<Vec4f, 5, 4, 3>::Type;
using ScalarTree = FloatTree;
using TopologyTree = MaskTree;
using Vec3dTree = Vec3DTree;
using Vec3fTree = Vec3STree;
using Vec4fTree = Vec4STree;
using VectorTree = Vec3fTree;

/// Common grid types
using BoolGrid = Grid<BoolTree>;
using DoubleGrid = Grid<DoubleTree>;
using FloatGrid = Grid<FloatTree>;
using Int8Grid = Grid<Int8Tree>;
using Int32Grid = Grid<Int32Tree>;
using Int64Grid = Grid<Int64Tree>;
using UInt32Grid = Grid<UInt32Tree>;
using MaskGrid = Grid<MaskTree>;
using Vec3DGrid = Grid<Vec3DTree>;
using Vec2IGrid = Grid<Vec2ITree>;
using Vec3IGrid = Grid<Vec3ITree>;
using Vec2SGrid = Grid<Vec2STree>;
using Vec3SGrid = Grid<Vec3STree>;
using Vec4SGrid = Grid<Vec4STree>;
using ScalarGrid = FloatGrid;
using TopologyGrid = MaskGrid;
using Vec3dGrid = Vec3DGrid;
using Vec2fGrid = Vec2SGrid;
using Vec3fGrid = Vec3SGrid;
using Vec4fGrid = Vec4SGrid;
using VectorGrid = Vec3fGrid;

}  // namespace OPENVDB_VERSION_NAME
}  // namespace openvdb

/** \} */
