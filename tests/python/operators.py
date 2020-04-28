# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
import os
import sys
from random import shuffle, seed

seed(0)

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import OperatorTest, OperatorSpec

# Central vertical loop of Suzanne
MONKEY_LOOP_VERT = {68, 69, 71, 73, 74, 75, 76, 77, 90, 129, 136, 175, 188, 189, 198, 207,
                    216, 223, 230, 301, 302, 303, 304, 305, 306, 307, 308}
MONKEY_LOOP_EDGE = {131, 278, 299, 305, 307, 334, 337, 359, 384, 396, 399, 412, 415, 560,
                    567, 572, 577, 615, 622, 627, 632, 643, 648, 655, 660, 707}


def main():
    tests = [
        #### 0
        # bisect
        ['FACE', {0, 1, 2, 3, 4, 5}, "testCubeBisect", "expectedCubeBisect", "bisect",
         {"plane_co": (0, 0, 0), "plane_no": (0, 1, 1), "clear_inner": True, "use_fill": True}],

        # blend from shape
        ['FACE', {0, 1, 2, 3, 4, 5}, "testCubeBlendFromShape", "expectedCubeBlendFromShape", "blend_from_shape",
         {"shape": "Key 1"}],

        # bridge edge loops
        ["FACE", {0, 1}, "testCubeBrigeEdgeLoop", "expectedCubeBridgeEdgeLoop", "bridge_edge_loops", {}],

        # decimate
        ["FACE", {i for i in range(500)}, "testMonkeyDecimate", "expectedMonkeyDecimate", "decimate", {"ratio": 0.1}],

        ### 4
        # delete
        ["VERT", {3}, "testCubeDeleteVertices", "expectedCubeDeleteVertices", "delete", {}],
        ["FACE", {0}, "testCubeDeleteFaces", "expectedCubeDeleteFaces", "delete", {}],
        ["EDGE", {0, 1, 2, 3}, "testCubeDeleteEdges", "expectedCubeDeleteEdges", "delete", {}],

        # delete edge loop
        ["VERT", MONKEY_LOOP_VERT, "testMokneyDeleteEdgeLoopVertices", "expectedMonkeyDeleteEdgeLoopVertices",
         "delete_edgeloop", {}],
        ["EDGE", MONKEY_LOOP_EDGE, "testMokneyDeleteEdgeLoopEdges", "expectedMonkeyDeleteEdgeLoopEdges",
         "delete_edgeloop", {}],

        ### 9
        # delete loose
        ["VERT", {i for i in range(12)}, "testCubeDeleteLooseVertices", "expectedCubeDeleteLooseVertices",
         "delete_loose", {"use_verts": True, "use_edges": False, "use_faces": False}],
        ["EDGE", {i for i in range(14)}, "testCubeDeleteLooseEdges", "expectedCubeDeleteLooseEdges",
         "delete_loose", {"use_verts": False, "use_edges": True, "use_faces": False}],
        ["FACE", {i for i in range(7)}, "testCubeDeleteLooseFaces", "expectedCubeDeleteLooseFaces",
         "delete_loose", {"use_verts": False, "use_edges": False, "use_faces": True}],

        # dissolve degenerate
        ["VERT", {i for i in range(8)}, "testCubeDissolveDegenerate", "expectedCubeDissolveDegenerate",
         "dissolve_degenerate", {}],

        ### 13
        # dissolve edges
        ["EDGE", {0, 5, 6, 9}, "testCylinderDissolveEdges", "expectedCylinderDissolveEdges",
         "dissolve_edges", {}],

        # dissolve faces
        ["VERT", {5, 34, 47, 49, 83, 91, 95}, "testCubeDissolveFaces", "expectedCubeDissolveFaces", "dissolve_faces",
         {}],

        ### 15
        # dissolve verts
        ["VERT", {16, 20, 22, 23, 25}, "testCubeDissolveVerts", "expectedCubeDissolveVerts", "dissolve_verts", {}],

        # duplicate
        ["VERT", {i for i in range(33)} - {23}, "testConeDuplicateVertices", "expectedConeDuplicateVertices",
         "duplicate", {}],
        ["VERT", {23}, "testConeDuplicateOneVertex", "expectedConeDuplicateOneVertex", "duplicate", {}],
        ["FACE", {6, 9}, "testConeDuplicateFaces", "expectedConeDuplicateFaces", "duplicate", {}],
        ["EDGE", {i for i in range(64)}, "testConeDuplicateEdges", "expectedConeDuplicateEdges", "duplicate", {}],

        ### 20
        # edge collapse
        ["EDGE", {1, 9, 4}, "testCylinderEdgeCollapse", "expectedCylinderEdgeCollapse", "edge_collapse", {}],

        # edge face add
        ["VERT", {1, 3, 4, 5, 7}, "testCubeEdgeFaceAddFace", "expectedCubeEdgeFaceAddFace", "edge_face_add", {}],
        ["VERT", {4, 5}, "testCubeEdgeFaceAddEdge", "expectedCubeEdgeFaceAddEdge", "edge_face_add", {}],

        # edge rotate
        ["EDGE", {1}, "testCubeEdgeRotate", "expectedCubeEdgeRotate", "edge_rotate", {}],

        # edge split
        ["EDGE", {2, 5, 8, 11, 14, 17, 20, 23}, "testCubeEdgeSplit", "expectedCubeEdgeSplit", "edge_split", {}],

        ### 25
        # face make planar
        ["FACE", {i for i in range(500)}, "testMonkeyFaceMakePlanar", "expectedMonkeyFaceMakePlanar",
         "face_make_planar", {}],

        # face split by edges
        ["VERT", {i for i in range(6)}, "testPlaneFaceSplitByEdges", "expectedPlaneFaceSplitByEdges",
         "face_split_by_edges", {}],

        # fill
        ["EDGE", {20, 21, 22, 23, 24, 45, 46, 47, 48, 49}, "testIcosphereFill", "expectedIcosphereFill",
         "fill", {}],
        ["EDGE", {20, 21, 22, 23, 24, 45, 46, 47, 48, 49}, "testIcosphereFillUseBeautyFalse",
         "expectedIcosphereFillUseBeautyFalse", "fill", {"use_beauty": False}],

        # fill grid
        ["EDGE", {1, 2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 15}, "testPlaneFillGrid", "expectedPlaneFillGrid",
         "fill_grid", {}],
        ["EDGE", {1, 2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 15}, "testPlaneFillGridSimpleBlending",
         "expectedPlaneFillGridSimpleBlending", "fill_grid", {"use_interp_simple": True}],

        ### 31
        # fill holes
        ["VERT", {i for i in range(481)}, "testSphereFillHoles", "expectedSphereFillHoles", "fill_holes", {"sides": 9}],

        # inset faces
        ["VERT", {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50, 52, 59, 61, 62, 65, 83, 91, 95}, "testCubeInset",
         "expectedCubeInset", "inset", {"thickness": 0.2}],
        ["VERT", {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50, 52, 59, 61, 62, 65, 83, 91, 95},
         "testCubeInsetEvenOffsetFalse", "expectedCubeInsetEvenOffsetFalse",
         "inset", {"thickness": 0.2, "use_even_offset": False}],
        ["VERT", {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50, 52, 59, 61, 62, 65, 83, 91, 95}, "testCubeInsetDepth",
         "expectedCubeInsetDepth", "inset", {"thickness": 0.2, "depth": 0.2}],
        ["FACE", {35, 36, 37, 45, 46, 47, 55, 56, 57}, "testGridInsetRelativeOffset", "expectedGridInsetRelativeOffset",
         "inset", {"thickness": 0.4, "use_relative_offset": True}],
         ]

    operators_test = OperatorTest(tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            operators_test.run_all_tests()
            break
        elif cmd == "--run-test":
            operators_test.apply_modifiers = False
            index = int(command[i + 1])
            operators_test.run_test(index)
            break


if __name__ == "__main__":
    main()
