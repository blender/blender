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
from modules.mesh_test import MeshTest, OperatorSpecEditMode, RunTest

# Central vertical loop of Suzanne
MONKEY_LOOP_VERT = {68, 69, 71, 73, 74, 75, 76, 77, 90, 129, 136, 175, 188, 189, 198, 207,
                    216, 223, 230, 301, 302, 303, 304, 305, 306, 307, 308}
MONKEY_LOOP_EDGE = {131, 278, 299, 305, 307, 334, 337, 359, 384, 396, 399, 412, 415, 560,
                    567, 572, 577, 615, 622, 627, 632, 643, 648, 655, 660, 707}


def main():
    tests = [
        # bisect
        MeshTest("CubeBisect", "testCubeBisect", "expectedCubeBisect",
                 [OperatorSpecEditMode("bisect",
                                       {"plane_co": (0, 0, 0), "plane_no": (0, 1, 1), "clear_inner": True,
                                        "use_fill": True}, 'FACE', {0, 1, 2, 3, 4, 5}, )]),

        # blend from shape
        MeshTest("CubeBlendFromShape", "testCubeBlendFromShape", "expectedCubeBlendFromShape",
                 [OperatorSpecEditMode("blend_from_shape", {"shape": "Key 1"}, 'FACE', {0, 1, 2, 3, 4, 5})]),

        # bridge edge loops
        MeshTest("CubeBridgeEdgeLoop", "testCubeBrigeEdgeLoop", "expectedCubeBridgeEdgeLoop",
                 [OperatorSpecEditMode("bridge_edge_loops", {}, "FACE", {0, 1})]),

        # decimate
        MeshTest("MonkeyDecimate", "testMonkeyDecimate", "expectedMonkeyDecimate",
                 [OperatorSpecEditMode("decimate",
                                       {"ratio": 0.1}, "FACE", {i for i in range(500)})]),

        # delete
        MeshTest("CubeDeleteVertices", "testCubeDeleteVertices", "expectedCubeDeleteVertices",
                 [OperatorSpecEditMode("delete", {}, "VERT", {3})]),
        MeshTest("CubeDeleteFaces", "testCubeDeleteFaces", "expectedCubeDeleteFaces",
                 [OperatorSpecEditMode("delete", {}, "FACE", {0})]),
        MeshTest("CubeDeleteEdges", "testCubeDeleteEdges", "expectedCubeDeleteEdges",
                 [OperatorSpecEditMode("delete", {}, "EDGE", {0, 1, 2, 3})]),

        # delete edge loop
        MeshTest("MonkeyDeleteEdgeLoopVertices", "testMokneyDeleteEdgeLoopVertices",
                 "expectedMonkeyDeleteEdgeLoopVertices",
                 [OperatorSpecEditMode("delete_edgeloop", {}, "VERT", MONKEY_LOOP_VERT)]),

        MeshTest("MonkeyDeleteEdgeLoopEdges", "testMokneyDeleteEdgeLoopEdges",
                 "expectedMonkeyDeleteEdgeLoopEdges",
                 [OperatorSpecEditMode("delete_edgeloop", {}, "EDGE", MONKEY_LOOP_EDGE)]),

        # delete loose
        MeshTest("CubeDeleteLooseVertices", "testCubeDeleteLooseVertices",
                 "expectedCubeDeleteLooseVertices",
                 [OperatorSpecEditMode("delete_loose", {"use_verts": True, "use_edges": False, "use_faces": False},
                                       "VERT",
                                       {i for i in range(12)})]),
        MeshTest("CubeDeleteLooseEdges", "testCubeDeleteLooseEdges",
                 "expectedCubeDeleteLooseEdges",
                 [OperatorSpecEditMode("delete_loose", {"use_verts": False, "use_edges": True, "use_faces": False},
                                       "EDGE",
                                       {i for i in range(14)})]),
        MeshTest("CubeDeleteLooseFaces", "testCubeDeleteLooseFaces",
                 "expectedCubeDeleteLooseFaces",
                 [OperatorSpecEditMode("delete_loose", {"use_verts": False, "use_edges": False, "use_faces": True},
                                       "FACE",
                                       {i for i in range(7)})]),

        # dissolve degenerate
        MeshTest("CubeDissolveDegenerate", "testCubeDissolveDegenerate",
                 "expectedCubeDissolveDegenerate",
                 [OperatorSpecEditMode("dissolve_degenerate", {}, "VERT", {i for i in range(8)})]),

        # dissolve edges
        MeshTest("CylinderDissolveEdges", "testCylinderDissolveEdges", "expectedCylinderDissolveEdges",
                 [OperatorSpecEditMode("dissolve_edges", {}, "EDGE", {0, 5, 6, 9})]),

        # dissolve faces
        MeshTest("CubeDissolveFaces", "testCubeDissolveFaces", "expectedCubeDissolveFaces",
                 [OperatorSpecEditMode("dissolve_faces", {}, "VERT", {5, 34, 47, 49, 83, 91, 95})]),

        # dissolve verts
        MeshTest("CubeDissolveVerts", "testCubeDissolveVerts", "expectedCubeDissolveVerts",
                 [OperatorSpecEditMode("dissolve_verts", {}, "VERT", {16, 20, 22, 23, 25})]),

        # duplicate
        MeshTest("ConeDuplicateVertices", "testConeDuplicateVertices",
                 "expectedConeDuplicateVertices",
                 [OperatorSpecEditMode("duplicate", {}, "VERT", {i for i in range(33)} - {23})]),

        MeshTest("ConeDuplicateOneVertex", "testConeDuplicateOneVertex", "expectedConeDuplicateOneVertex",
                 [OperatorSpecEditMode("duplicate", {}, "VERT", {23})]),
        MeshTest("ConeDuplicateFaces", "testConeDuplicateFaces", "expectedConeDuplicateFaces",
                 [OperatorSpecEditMode("duplicate", {}, "FACE", {6, 9})]),
        MeshTest("ConeDuplicateEdges", "testConeDuplicateEdges", "expectedConeDuplicateEdges",
                 [OperatorSpecEditMode("duplicate", {}, "EDGE", {i for i in range(64)})]),

        # edge collapse
        MeshTest("CylinderEdgeCollapse", "testCylinderEdgeCollapse", "expectedCylinderEdgeCollapse",
                 [OperatorSpecEditMode("edge_collapse", {}, "EDGE", {1, 9, 4})]),

        # edge face add
        MeshTest("CubeEdgeFaceAddFace", "testCubeEdgeFaceAddFace", "expectedCubeEdgeFaceAddFace",
                 [OperatorSpecEditMode("edge_face_add", {}, "VERT", {1, 3, 4, 5, 7})]),
        MeshTest("CubeEdgeFaceAddEdge", "testCubeEdgeFaceAddEdge", "expectedCubeEdgeFaceAddEdge",
                 [OperatorSpecEditMode("edge_face_add", {}, "VERT", {4, 5})]),

        # edge rotate
        MeshTest("CubeEdgeRotate", "testCubeEdgeRotate", "expectedCubeEdgeRotate",
                 [OperatorSpecEditMode("edge_rotate", {}, "EDGE", {1})]),

        # edge split
        MeshTest("CubeEdgeSplit", "testCubeEdgeSplit", "expectedCubeEdgeSplit",
                 [OperatorSpecEditMode("edge_split", {}, "EDGE", {2, 5, 8, 11, 14, 17, 20, 23})]),

        ### 25
        # edge ring select - Cannot be tested. Need user input.
        # MeshTest("CubeEdgeRingSelect", "testCubeEdgeRingSelect", "expectedCubeEdgeRingSelect",
        #         [OperatorSpecEditMode("edgering_select", {}, "EDGE", {5, 20, 25, 26})]),
        # MeshTest("EmptyMeshEdgeRingSelect", "testGridEdgeRingSelect", "expectedGridEdgeRingSelect",
        #         [OperatorSpecEditMode("edgering_select", {}, "VERT", {65, 66, 67})]),
        # MeshTest("EmptyMeshEdgeRingSelect", "testEmptyMeshdgeRingSelect", "expectedEmptyMeshEdgeRingSelect",
        #         [OperatorSpecEditMode("edgering_select", {}, "VERT", {})]),

        # face make planar
        MeshTest("MonkeyFaceMakePlanar", "testMonkeyFaceMakePlanar",
                 "expectedMonkeyFaceMakePlanar",
                 [OperatorSpecEditMode("face_make_planar", {}, "FACE", {i for i in range(500)})]),

        # face split by edges
        MeshTest("PlaneFaceSplitByEdges", "testPlaneFaceSplitByEdges",
                 "expectedPlaneFaceSplitByEdges",
                 [OperatorSpecEditMode("face_split_by_edges", {}, "VERT", {i for i in range(6)})]),

        # faces select linked flat
        MeshTest("CubeFacesSelectLinkedFlat", "testCubeFaceSelectLinkedFlat", "expectedCubeFaceSelectLinkedFlat",
                 [OperatorSpecEditMode("faces_select_linked_flat", {}, "FACE", {7})]),
        MeshTest("PlaneFacesSelectLinkedFlat", "testPlaneFaceSelectLinkedFlat", "expectedPlaneFaceSelectLinkedFlat",
                 [OperatorSpecEditMode("faces_select_linked_flat", {}, "VERT", {1})]),
        MeshTest("EmptyMeshFacesSelectLinkedFlat", "testEmptyMeshFaceSelectLinkedFlat",
                 "expectedEmptyMeshFaceSelectLinkedFlat",
                 [OperatorSpecEditMode("faces_select_linked_flat", {}, "VERT", {})]),

        # fill
        MeshTest("IcosphereFill", "testIcosphereFill", "expectedIcosphereFill",
                 [OperatorSpecEditMode("fill", {}, "EDGE", {20, 21, 22, 23, 24, 45, 46, 47, 48, 49})]),
        MeshTest("IcosphereFillUseBeautyFalse",
                 "testIcosphereFillUseBeautyFalse", "expectedIcosphereFillUseBeautyFalse",
                 [OperatorSpecEditMode("fill", {"use_beauty": False}, "EDGE",
                                       {20, 21, 22, 23, 24, 45, 46, 47, 48, 49})]),

        # fill grid
        MeshTest("PlaneFillGrid", "testPlaneFillGrid",
                 "expectedPlaneFillGrid",
                 [OperatorSpecEditMode("fill_grid", {}, "EDGE", {1, 2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 15})]),
        MeshTest("PlaneFillGridSimpleBlending",
                 "testPlaneFillGridSimpleBlending",
                 "expectedPlaneFillGridSimpleBlending",
                 [OperatorSpecEditMode("fill_grid", {"use_interp_simple": True}, "EDGE",
                                       {1, 2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 15})]),

        # fill holes
        MeshTest("SphereFillHoles", "testSphereFillHoles", "expectedSphereFillHoles",
                 [OperatorSpecEditMode("fill_holes", {"sides": 9}, "VERT", {i for i in range(481)})]),

        # inset faces
        MeshTest("CubeInset",
                 "testCubeInset", "expectedCubeInset", [OperatorSpecEditMode("inset", {"thickness": 0.2}, "VERT",
                                                                             {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50,
                                                                              52,
                                                                              59, 61, 62, 65, 83, 91, 95})]),
        MeshTest("CubeInsetEvenOffsetFalse",
                 "testCubeInsetEvenOffsetFalse", "expectedCubeInsetEvenOffsetFalse",
                 [OperatorSpecEditMode("inset", {"thickness": 0.2, "use_even_offset": False}, "VERT",
                                       {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50, 52, 59, 61, 62, 65, 83, 91, 95})]),
        MeshTest("CubeInsetDepth",
                 "testCubeInsetDepth",
                 "expectedCubeInsetDepth", [OperatorSpecEditMode("inset", {"thickness": 0.2, "depth": 0.2}, "VERT",
                                                                 {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50, 52, 59, 61,
                                                                  62,
                                                                  65, 83, 91, 95})]),
        MeshTest("GridInsetRelativeOffset", "testGridInsetRelativeOffset",
                 "expectedGridInsetRelativeOffset",
                 [OperatorSpecEditMode("inset", {"thickness": 0.4,
                                                 "use_relative_offset": True}, "FACE",
                                       {35, 36, 37, 45, 46, 47, 55, 56, 57})]),
    ]

    operators_test = RunTest(tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            operators_test.do_compare = True
            operators_test.run_all_tests()
            break
        elif cmd == "--run-test":
            operators_test.do_compare = False
            name = command[i + 1]
            operators_test.run_test(name)
            break


if __name__ == "__main__":
    main()
