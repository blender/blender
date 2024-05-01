/* SPDX-FileCopyrightText: 2013 Pixar
 * SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Original code by Pixar with modifications by the Blender foundation. */

#ifndef OPENSUBDIV_PATCH_MAP_H_
#define OPENSUBDIV_PATCH_MAP_H_

#include <opensubdiv/far/patchTable.h>

namespace blender::opensubdiv {

/// \brief An quadtree-based map connecting coarse faces to their sub-patches
///
/// PatchTable::PatchArrays contain lists of patches that represent the limit
/// surface of a mesh, sorted by their topological type. These arrays break the
/// connection between coarse faces and their sub-patches.
///
/// The PatchMap provides a quad-tree based lookup structure that, given a singular
/// parametric location, can efficiently return a handle to the sub-patch that
/// contains this location.
///
class PatchMap {
 public:
  // Quadtree node with 4 children, tree is just a vector of nodes
  struct QuadNode {
    QuadNode()
    {
      std::memset(this, 0, sizeof(QuadNode));
    }

    struct Child {
      unsigned int isSet : 1;   // true if the child has been set
      unsigned int isLeaf : 1;  // true if the child is a QuadNode
      unsigned int index : 30;  // child index (either QuadNode or Handle)
    };

    // sets all the children to point to the patch of given index
    void SetChildren(int index);

    // sets the child in "quadrant" to point to the node or patch of the given index
    void SetChild(int quadrant, int index, bool isLeaf);

    Child children[4];
  };

  typedef OpenSubdiv::Far::PatchTable::PatchHandle Handle;

  /// \brief Constructor
  ///
  /// @param patchTable  A valid PatchTable
  ///
  PatchMap(OpenSubdiv::Far::PatchTable const &patchTable);

  /// \brief Returns a handle to the sub-patch of the face at the given (u,v).
  /// Note that the patch face ID corresponds to potentially quadrangulated
  /// face indices and not the base face indices (see Far::PtexIndices for more
  /// details).
  ///
  /// @param patchFaceId  The index of the patch (Ptex) face
  ///
  /// @param u       Local u parameter
  ///
  /// @param v       Local v parameter
  ///
  /// @return        A patch handle or 0 if the face is not supported (index
  ///                out of bounds) or is tagged as a hole
  ///
  Handle const *FindPatch(int patchFaceId, double u, double v) const;

  int getMinPatchFace() const
  {
    return _minPatchFace;
  }

  int getMaxPatchFace() const
  {
    return _maxPatchFace;
  }

  int getMaxDepth() const
  {
    return _maxDepth;
  }

  bool getPatchesAreTriangular() const
  {
    return _patchesAreTriangular;
  }

  const std::vector<Handle> &getHandles()
  {
    return _handles;
  }

  const std::vector<QuadNode> &nodes()
  {
    return _quadtree;
  }

 private:
  void initializeHandles(OpenSubdiv::Far::PatchTable const &patchTable);
  void initializeQuadtree(OpenSubdiv::Far::PatchTable const &patchTable);

  typedef std::vector<QuadNode> QuadTree;

  // Internal methods supporting quadtree construction and queries
  void assignRootNode(QuadNode *node, int index);
  QuadNode *assignLeafOrChildNode(QuadNode *node, bool isLeaf, int quadrant, int index);

  template<class T> static int transformUVToQuadQuadrant(T const &median, T &u, T &v);
  template<class T>
  static int transformUVToTriQuadrant(T const &median, T &u, T &v, bool &rotated);

 private:
  bool _patchesAreTriangular;  // tri and quad assembly and search requirements differ

  int _minPatchFace;  // minimum patch face index supported by the map
  int _maxPatchFace;  // maximum patch face index supported by the map
  int _maxDepth;      // maximum depth of a patch in the tree

  std::vector<Handle> _handles;     // all the patches in the PatchTable
  std::vector<QuadNode> _quadtree;  // quadtree nodes
};

//
//  Given a median value for both U and V, these methods transform a (u,v) pair
//  into the quadrant that contains them and returns the quadrant index.
//
//  Quadrant indexing for tri and quad patches -- consistent with PatchParam's
//  usage of UV bits:
//
//      (0,1) o-----o-----o (1,1)     (0,1) o     (1,0) o-----o-----o (0,0)
//            |     |     |                 |\           \  1 |\  0 |
//            |  2  |  3  |                 |  \           \  |  \  |
//            |     |     |                 | 2  \           \| 3  \|
//            o-----o-----o                 o-----o           o-----o
//            |     |     |                 |\  3 |\           \  2 |
//            |  0  |  1  |                 |  \  |  \           \  |
//            |     |     |                 | 0  \| 1  \           \|
//      (0,0) o-----o-----o (1,0)     (0,0) o-----o-----o (1,0)     o (0,1)
//
//  The triangular case also takes and returns/affects the rotation of the
//  quadrant being searched and identified (quadrant 3 imparts a rotation).
//
template<class T> inline int PatchMap::transformUVToQuadQuadrant(T const &median, T &u, T &v)
{

  int uHalf = (u >= median);
  if (uHalf) {
    u -= median;
  }

  int vHalf = (v >= median);
  if (vHalf) {
    v -= median;
  }

  return (vHalf << 1) | uHalf;
}

template<class T>
int inline PatchMap::transformUVToTriQuadrant(T const &median, T &u, T &v, bool &rotated)
{

  if (!rotated) {
    if (u >= median) {
      u -= median;
      return 1;
    }
    if (v >= median) {
      v -= median;
      return 2;
    }
    if ((u + v) >= median) {
      rotated = true;
      return 3;
    }
    return 0;
  }
  else {
    if (u < median) {
      v -= median;
      return 1;
    }
    if (v < median) {
      u -= median;
      return 2;
    }
    u -= median;
    v -= median;
    if ((u + v) < median) {
      rotated = false;
      return 3;
    }
    return 0;
  }
}

/// Returns a handle to the sub-patch of the face at the given (u,v).
inline PatchMap::Handle const *PatchMap::FindPatch(int faceid, double u, double v) const
{

  //
  //  Reject patch faces not supported by this map, or those corresponding
  //  to holes or otherwise unassigned (the root node for a patch will
  //  have all or no quadrants set):
  //
  if ((faceid < _minPatchFace) || (faceid > _maxPatchFace)) {
    return 0;
  }

  QuadNode const *node = &_quadtree[faceid - _minPatchFace];

  if (!node->children[0].isSet) {
    return 0;
  }

  //
  //  Search the tree for the sub-patch containing the given (u,v)
  //
  assert((u >= 0.0) && (u <= 1.0) && (v >= 0.0) && (v <= 1.0));

  double median = 0.5;
  bool triRotated = false;

  for (int depth = 0; depth <= _maxDepth; ++depth, median *= 0.5) {

    int quadrant = _patchesAreTriangular ? transformUVToTriQuadrant(median, u, v, triRotated) :
                                           transformUVToQuadQuadrant(median, u, v);

    //  holes should have been rejected at the root node of the face
    assert(node->children[quadrant].isSet);

    if (node->children[quadrant].isLeaf) {
      return &_handles[node->children[quadrant].index];
    }
    else {
      node = &_quadtree[node->children[quadrant].index];
    }
  }
  assert(0);
  return 0;
}

}  // namespace blender::opensubdiv

#endif  // OPENSUBDIV_PATCH_MAP_H_
