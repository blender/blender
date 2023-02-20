# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import os
import sys
from random import shuffle, seed

seed(0)

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import SpecMeshTest, OperatorSpecEditMode, RunTest

# Central vertical loop of Suzanne
MONKEY_LOOP_VERT = {68, 69, 71, 73, 74, 75, 76, 77, 90, 129, 136, 175, 188, 189, 198, 207,
                    216, 223, 230, 301, 302, 303, 304, 305, 306, 307, 308}
MONKEY_LOOP_EDGE = {131, 278, 299, 305, 307, 334, 337, 359, 384, 396, 399, 412, 415, 560,
                    567, 572, 577, 615, 622, 627, 632, 643, 648, 655, 660, 707}


def main():
    tests = [
        # bisect
        SpecMeshTest(
            "CubeBisect", "testCubeBisect", "expectedCubeBisect",
            [OperatorSpecEditMode("bisect",
                                  {"plane_co": (0, 0, 0), "plane_no": (0, 1, 1), "clear_inner": True,
                                            "use_fill": True}, 'FACE', {0, 1, 2, 3, 4, 5}, )],
        ),

        # blend from shape
        SpecMeshTest(
            "CubeBlendFromShape", "testCubeBlendFromShape", "expectedCubeBlendFromShape",
            [OperatorSpecEditMode("blend_from_shape", {"shape": "Key 1"}, 'FACE', {0, 1, 2, 3, 4, 5})],
        ),

        # bridge edge loops
        SpecMeshTest(
            "CubeBridgeEdgeLoop", "testCubeBrigeEdgeLoop", "expectedCubeBridgeEdgeLoop",
            [OperatorSpecEditMode("bridge_edge_loops", {}, "FACE", {0, 1})],
        ),

        # decimate
        SpecMeshTest(
            "MonkeyDecimate", "testMonkeyDecimate", "expectedMonkeyDecimate",
            [OperatorSpecEditMode("decimate",
                                  {"ratio": 0.1}, "FACE", {i for i in range(500)})],
        ),

        # delete
        SpecMeshTest(
            "CubeDeleteVertices", "testCubeDeleteVertices", "expectedCubeDeleteVertices",
            [OperatorSpecEditMode("delete", {}, "VERT", {3})],
        ),
        SpecMeshTest(
            "CubeDeleteFaces", "testCubeDeleteFaces", "expectedCubeDeleteFaces",
            [OperatorSpecEditMode("delete", {}, "FACE", {0})],
        ),
        SpecMeshTest(
            "CubeDeleteEdges", "testCubeDeleteEdges", "expectedCubeDeleteEdges",
            [OperatorSpecEditMode("delete", {}, "EDGE", {0, 1, 2, 3})],
        ),

        # delete edge loop
        SpecMeshTest(
            "MonkeyDeleteEdgeLoopVertices", "testMokneyDeleteEdgeLoopVertices",
            "expectedMonkeyDeleteEdgeLoopVertices",
            [OperatorSpecEditMode("delete_edgeloop", {}, "VERT", MONKEY_LOOP_VERT)],
        ),

        SpecMeshTest(
            "MonkeyDeleteEdgeLoopEdges", "testMokneyDeleteEdgeLoopEdges",
            "expectedMonkeyDeleteEdgeLoopEdges",
            [OperatorSpecEditMode("delete_edgeloop", {}, "EDGE", MONKEY_LOOP_EDGE)],
        ),

        # delete loose
        SpecMeshTest(
            "CubeDeleteLooseVertices", "testCubeDeleteLooseVertices",
            "expectedCubeDeleteLooseVertices",
            [OperatorSpecEditMode("delete_loose", {"use_verts": True, "use_edges": False, "use_faces": False},
                                  "VERT",
                                  {i for i in range(12)})],
        ),
        SpecMeshTest(
            "CubeDeleteLooseEdges", "testCubeDeleteLooseEdges",
            "expectedCubeDeleteLooseEdges",
            [OperatorSpecEditMode("delete_loose", {"use_verts": False, "use_edges": True, "use_faces": False},
                                  "EDGE",
                                  {i for i in range(14)})],
        ),
        SpecMeshTest(
            "CubeDeleteLooseFaces", "testCubeDeleteLooseFaces",
            "expectedCubeDeleteLooseFaces",
            [OperatorSpecEditMode("delete_loose", {"use_verts": False, "use_edges": False, "use_faces": True},
                                  "FACE",
                                  {i for i in range(7)})],
        ),

        # dissolve degenerate
        SpecMeshTest(
            "CubeDissolveDegenerate", "testCubeDissolveDegenerate",
            "expectedCubeDissolveDegenerate",
            [OperatorSpecEditMode("dissolve_degenerate", {}, "VERT", {i for i in range(8)})],
        ),

        # dissolve edges
        SpecMeshTest(
            "CylinderDissolveEdges", "testCylinderDissolveEdges", "expectedCylinderDissolveEdges",
            [OperatorSpecEditMode("dissolve_edges", {}, "EDGE", {0, 5, 6, 9})],
        ),

        # dissolve faces
        SpecMeshTest(
            "CubeDissolveFaces", "testCubeDissolveFaces", "expectedCubeDissolveFaces",
            [OperatorSpecEditMode("dissolve_faces", {}, "VERT", {5, 34, 47, 49, 83, 91, 95})],
        ),

        # dissolve limited
        SpecMeshTest(
            "SphereDissolveLimited", "testSphereDissolveLimited", "expectedSphereDissolveLimited",
            [OperatorSpecEditMode("dissolve_limited", {"angle_limit": 0.610865}, "FACE", {20, 23, 26, 29, 32})],
        ),

        # dissolve mode
        SpecMeshTest(
            "PlaneDissolveMode", "testPlaneDissolveMode", "expectedPlaneDissolveMode",
            [OperatorSpecEditMode("dissolve_mode", {"use_verts": True}, "FACE", {0, 1, 2, 10, 12, 13})],
        ),

        # dissolve verts
        SpecMeshTest(
            "CubeDissolveVerts", "testCubeDissolveVerts", "expectedCubeDissolveVerts",
            [OperatorSpecEditMode("dissolve_verts", {}, "VERT", {16, 20, 22, 23, 25})],
        ),

        # duplicate
        SpecMeshTest(
            "ConeDuplicateVertices", "testConeDuplicateVertices",
            "expectedConeDuplicateVertices",
            [OperatorSpecEditMode("duplicate", {}, "VERT", {i for i in range(33)} - {23})],
        ),

        SpecMeshTest(
            "ConeDuplicateOneVertex", "testConeDuplicateOneVertex", "expectedConeDuplicateOneVertex",
            [OperatorSpecEditMode("duplicate", {}, "VERT", {23})],
        ),
        SpecMeshTest(
            "ConeDuplicateFaces", "testConeDuplicateFaces", "expectedConeDuplicateFaces",
            [OperatorSpecEditMode("duplicate", {}, "FACE", {6, 9})],
        ),
        SpecMeshTest(
            "ConeDuplicateEdges", "testConeDuplicateEdges", "expectedConeDuplicateEdges",
            [OperatorSpecEditMode("duplicate", {}, "EDGE", {i for i in range(64)})],
        ),

        # edge collapse
        SpecMeshTest(
            "CylinderEdgeCollapse", "testCylinderEdgeCollapse", "expectedCylinderEdgeCollapse",
            [OperatorSpecEditMode("edge_collapse", {}, "EDGE", {1, 9, 4})],
        ),

        # edge face add
        SpecMeshTest(
            "CubeEdgeFaceAddFace", "testCubeEdgeFaceAddFace", "expectedCubeEdgeFaceAddFace",
            [OperatorSpecEditMode("edge_face_add", {}, "VERT", {1, 3, 4, 5, 7})],
        ),
        SpecMeshTest(
            "CubeEdgeFaceAddEdge", "testCubeEdgeFaceAddEdge", "expectedCubeEdgeFaceAddEdge",
            [OperatorSpecEditMode("edge_face_add", {}, "VERT", {4, 5})],
        ),

        # edge rotate
        SpecMeshTest(
            "CubeEdgeRotate", "testCubeEdgeRotate", "expectedCubeEdgeRotate",
            [OperatorSpecEditMode("edge_rotate", {}, "EDGE", {1})],
        ),

        # edge split
        SpecMeshTest(
            "CubeEdgeSplit", "testCubeEdgeSplit", "expectedCubeEdgeSplit",
            [OperatorSpecEditMode("edge_split", {}, "EDGE", {2, 5, 8, 11, 14, 17, 20, 23})],
        ),

        # edge ring select - Cannot be tested. Need user input.
        # SpecMeshTest("CubeEdgeRingSelect", "testCubeEdgeRingSelect", "expectedCubeEdgeRingSelect",
        #         [OperatorSpecEditMode("edgering_select", {}, "EDGE", {5, 20, 25, 26})]),
        # SpecMeshTest("EmptyMeshEdgeRingSelect", "testGridEdgeRingSelect", "expectedGridEdgeRingSelect",
        #         [OperatorSpecEditMode("edgering_select", {}, "VERT", {65, 66, 67})]),
        # SpecMeshTest("EmptyMeshEdgeRingSelect", "testEmptyMeshdgeRingSelect", "expectedEmptyMeshEdgeRingSelect",
        #         [OperatorSpecEditMode("edgering_select", {}, "VERT", {})]),

        # edges select sharp
        SpecMeshTest(
            "CubeEdgesSelectSharp", "testCubeEdgeSelectSharp", "expectedCubeEdgeSelectSharp",
            [OperatorSpecEditMode("edges_select_sharp", {}, "EDGE", {20})],
        ),
        SpecMeshTest(
            "SphereEdgesSelectSharp", "testSphereEdgesSelectSharp", "expectedSphereEdgeSelectSharp",
            [OperatorSpecEditMode("edges_select_sharp", {"sharpness": 0.25}, "EDGE", {288})],
        ),
        SpecMeshTest(
            "HoledSphereEdgesSelectSharp", "testHoledSphereEdgesSelectSharp", "expectedHoledSphereEdgeSelectSharp",
            [OperatorSpecEditMode("edges_select_sharp", {"sharpness": 0.18}, "VERT", {})],
        ),
        SpecMeshTest(
            "EmptyMeshEdgesSelectSharp", "testEmptyMeshEdgeSelectSharp", "expectedEmptyMeshEdgeSelectSharp",
            [OperatorSpecEditMode("edges_select_sharp", {}, "VERT", {})],
        ),

        # face make planar
        SpecMeshTest(
            "MonkeyFaceMakePlanar", "testMonkeyFaceMakePlanar",
            "expectedMonkeyFaceMakePlanar",
            [OperatorSpecEditMode("face_make_planar", {}, "FACE", {i for i in range(500)})],
        ),

        # face split by edges
        SpecMeshTest(
            "PlaneFaceSplitByEdges", "testPlaneFaceSplitByEdges",
            "expectedPlaneFaceSplitByEdges",
            [OperatorSpecEditMode("face_split_by_edges", {}, "VERT", {i for i in range(6)})],
        ),

        # faces select linked flat
        SpecMeshTest(
            "CubeFacesSelectLinkedFlat", "testCubeFaceSelectLinkedFlat", "expectedCubeFaceSelectLinkedFlat",
            [OperatorSpecEditMode("faces_select_linked_flat", {}, "FACE", {7})],
        ),
        SpecMeshTest(
            "PlaneFacesSelectLinkedFlat", "testPlaneFaceSelectLinkedFlat", "expectedPlaneFaceSelectLinkedFlat",
            [OperatorSpecEditMode("faces_select_linked_flat", {}, "VERT", {1})],
        ),
        SpecMeshTest(
            "EmptyMeshFacesSelectLinkedFlat", "testEmptyMeshFaceSelectLinkedFlat",
            "expectedEmptyMeshFaceSelectLinkedFlat",
            [OperatorSpecEditMode("faces_select_linked_flat", {}, "VERT", {})],
        ),

        # fill
        SpecMeshTest(
            "IcosphereFill", "testIcosphereFill", "expectedIcosphereFill",
            [OperatorSpecEditMode("fill", {}, "EDGE", {20, 21, 22, 23, 24, 45, 46, 47, 48, 49})],
        ),
        SpecMeshTest(
            "IcosphereFillUseBeautyFalse",
            "testIcosphereFillUseBeautyFalse", "expectedIcosphereFillUseBeautyFalse",
            [OperatorSpecEditMode("fill", {"use_beauty": False}, "EDGE",
                                  {20, 21, 22, 23, 24, 45, 46, 47, 48, 49})],
        ),

        # fill grid
        SpecMeshTest(
            "PlaneFillGrid", "testPlaneFillGrid",
            "expectedPlaneFillGrid",
            [OperatorSpecEditMode("fill_grid", {}, "EDGE", {1, 2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 15})],
        ),
        SpecMeshTest(
            "PlaneFillGridSimpleBlending",
            "testPlaneFillGridSimpleBlending",
            "expectedPlaneFillGridSimpleBlending",
            [OperatorSpecEditMode("fill_grid", {"use_interp_simple": True}, "EDGE",
                                  {1, 2, 3, 4, 5, 7, 9, 10, 11, 12, 13, 15})],
        ),

        # fill holes
        SpecMeshTest(
            "SphereFillHoles", "testSphereFillHoles", "expectedSphereFillHoles",
            [OperatorSpecEditMode("fill_holes", {"sides": 9}, "VERT", {i for i in range(481)})],
        ),

        # face shade smooth (not a real test)
        SpecMeshTest(
            "CubeShadeSmooth", "testCubeShadeSmooth", "expectedCubeShadeSmooth",
            [OperatorSpecEditMode("faces_shade_smooth", {}, "VERT", {i for i in range(8)})],
        ),

        # faces shade flat (not a real test)
        SpecMeshTest(
            "CubeShadeFlat", "testCubeShadeFlat", "expectedCubeShadeFlat",
            [OperatorSpecEditMode("faces_shade_flat", {}, "FACE", {i for i in range(6)})],
        ),

        # hide
        SpecMeshTest(
            "HideFace", "testCubeHideFace", "expectedCubeHideFace",
            [OperatorSpecEditMode("hide", {}, "FACE", {3})],
        ),
        SpecMeshTest(
            "HideEdge", "testCubeHideEdge", "expectedCubeHideEdge",
            [OperatorSpecEditMode("hide", {}, "EDGE", {1})],
        ),
        SpecMeshTest(
            "HideVertex", "testCubeHideVertex", "expectedCubeHideVertex",
            [OperatorSpecEditMode("hide", {}, "VERT", {0})],
        ),

        # inset faces
        SpecMeshTest(
            "CubeInset",
            "testCubeInset", "expectedCubeInset", [OperatorSpecEditMode("inset", {"thickness": 0.2}, "VERT",
                                                                        {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50,
                                                                         52,
                                                                         59, 61, 62, 65, 83, 91, 95})],
        ),
        SpecMeshTest(
            "CubeInsetEvenOffsetFalse",
            "testCubeInsetEvenOffsetFalse", "expectedCubeInsetEvenOffsetFalse",
            [OperatorSpecEditMode("inset", {"thickness": 0.2, "use_even_offset": False}, "VERT",
                                  {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50, 52, 59, 61, 62, 65, 83, 91, 95})],
        ),
        SpecMeshTest(
            "CubeInsetDepth",
            "testCubeInsetDepth",
            "expectedCubeInsetDepth", [OperatorSpecEditMode("inset", {"thickness": 0.2, "depth": 0.2}, "VERT",
                                                            {5, 16, 17, 19, 20, 22, 23, 34, 47, 49, 50, 52, 59, 61,
                                                             62,
                                                             65, 83, 91, 95})],
        ),
        SpecMeshTest(
            "GridInsetRelativeOffset", "testGridInsetRelativeOffset",
            "expectedGridInsetRelativeOffset",
            [OperatorSpecEditMode("inset", {"thickness": 0.4,
                                            "use_relative_offset": True}, "FACE",
                                  {35, 36, 37, 45, 46, 47, 55, 56, 57})],
        ),

        # loop multi select
        SpecMeshTest(
            "MokeyLoopMultiSelect", "testMonkeyLoopMultiSelect", "expectedMonkeyLoopMultiSelect",
            [OperatorSpecEditMode("loop_multi_select", {}, "VERT", {355, 359, 73, 301, 302})],
        ),
        SpecMeshTest(
            "HoledGridLoopMultiSelect", "testGridLoopMultiSelect", "expectedGridLoopMultiSelect",
            [OperatorSpecEditMode("loop_multi_select", {}, "VERT", {257, 169, 202, 207, 274, 278, 63})],
        ),
        SpecMeshTest(
            "EmptyMeshLoopMultiSelect", "testEmptyMeshLoopMultiSelect", "expectedEmptyMeshLoopMultiSelect",
            [OperatorSpecEditMode("loop_multi_select", {}, "VERT", {})],
        ),

        # mark seam
        SpecMeshTest(
            "CubeMarkSeam", "testCubeMarkSeam", "expectedCubeMarkSeam",
            [OperatorSpecEditMode("mark_seam", {}, "EDGE", {1})],
        ),

        # merge normals
        SpecMeshTest(
            "CubeMergeNormals", "testCubeMergeNormals", "expectedCubeMergeNormals",
            [OperatorSpecEditMode("merge_normals", {}, "FACE", {3, 5})],
        ),

        # select all
        SpecMeshTest(
            "CircleSelectAll", "testCircleSelectAll", "expectedCircleSelectAll",
            [OperatorSpecEditMode("select_all", {}, "VERT", {1})],
        ),
        SpecMeshTest(
            "IsolatedVertsSelectAll", "testIsolatedVertsSelectAll", "expectedIsolatedVertsSelectAll",
            [OperatorSpecEditMode("select_all", {}, "VERT", {})],
        ),
        SpecMeshTest(
            "EmptyMeshSelectAll", "testEmptyMeshSelectAll", "expectedEmptyMeshSelectAll",
            [OperatorSpecEditMode("select_all", {}, "VERT", {})],
        ),

        # select axis - Cannot be tested. Needs active vert selection
        # SpecMeshTest("MonkeySelectAxisX", "testMonkeySelectAxisX", "expectedMonkeySelectAxisX",
        #          [OperatorSpecEditMode("select_axis", {"axis": "X"}, "VERT", {13})]),
        # SpecMeshTest("MonkeySelectAxisY", "testMonkeySelectAxisY", "expectedMonkeySelectAxisY",
        #          [OperatorSpecEditMode("select_axis", {"axis": "Y", "sign": "NEG"}, "FACE", {317})]),
        # SpecMeshTest("MonkeySelectAxisXYZ", "testMonkeySelectAxisXYZ", "expectedMonkeySelectAxisXYZ",
        #          [OperatorSpecEditMode("select_axis", {"axis": "X", "sign": "NEG"}, "FACE", {317}),
        #          OperatorSpecEditMode("select_axis", {"axis": "Y", "sign": "POS"}, "FACE", {}),
        #          OperatorSpecEditMode("select_axis", {"axis": "Z", "sign": "NEG"}, "FACE", {})]),

        # select faces by sides
        SpecMeshTest(
            "CubeSelectFacesBySide", "testCubeSelectFacesBySide", "expectedCubeSelectFacesBySide",
            [OperatorSpecEditMode("select_face_by_sides", {"number": 4}, "FACE", {})],
        ),
        SpecMeshTest(
            "CubeSelectFacesBySideGreater", "testCubeSelectFacesBySideGreater", "expectedCubeSelectFacesBySideGreater",
            [OperatorSpecEditMode("select_face_by_sides", {"number": 4,
                                  "type": "GREATER", "extend": True}, "FACE", {})],
        ),
        SpecMeshTest(
            "CubeSelectFacesBySideLess", "testCubeSelectFacesBySideLess", "expectedCubeSelectFacesBySideLess",
            [OperatorSpecEditMode("select_face_by_sides", {"number": 4,
                                  "type": "GREATER", "extend": True}, "FACE", {})],
        ),

        # select interior faces
        SpecMeshTest(
            "CubeSelectInteriorFaces", "testCubeSelectInteriorFaces", "expectedCubeSelectInteriorFaces",
            [OperatorSpecEditMode("select_face_by_sides", {"number": 4}, "FACE", {})],
        ),
        SpecMeshTest(
            "HoledCubeSelectInteriorFaces", "testHoledCubeSelectInteriorFaces", "expectedHoledCubeSelectInteriorFaces",
            [OperatorSpecEditMode("select_face_by_sides", {"number": 4}, "FACE", {})],
        ),
        SpecMeshTest(
            "EmptyMeshSelectInteriorFaces", "testEmptyMeshSelectInteriorFaces", "expectedEmptyMeshSelectInteriorFaces",
            [OperatorSpecEditMode("select_face_by_sides", {"number": 4}, "FACE", {})],
        ),

        # select less
        SpecMeshTest(
            "MonkeySelectLess", "testMonkeySelectLess", "expectedMonkeySelectLess",
            [OperatorSpecEditMode("select_less", {}, "VERT", {
                2, 8, 24, 34, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 68,
                69, 70, 71, 74, 75, 78, 80, 81, 82, 83, 90, 91, 93, 95, 97, 99,
                101, 109, 111, 115, 117, 119, 121, 123, 125, 127, 129, 130, 131,
                132, 133, 134, 135, 136, 138, 141, 143, 145, 147, 149, 151, 153,
                155, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174,
                175, 176, 177, 178, 181, 182, 184, 185, 186, 187, 188, 189, 190,
                192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
                206, 207, 208, 210, 216, 217, 218, 219, 220, 221, 222, 229, 230,
                231, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255,
                257, 259, 263, 267, 269, 271, 275, 277, 289, 291, 293, 295, 309,
                310, 311, 312, 316, 317, 318, 319, 320, 323, 325, 327, 329, 331,
                341, 347, 349, 350, 351, 354, 356, 359, 361, 363, 365, 367, 369,
                375, 379, 381, 382, 385, 386, 387, 388, 389, 390, 391, 392, 393,
                394, 395, 396, 397, 398, 399, 400, 401, 402, 403, 404, 405, 406,
                407, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 419,
                420, 421, 423, 425, 426, 427, 428, 429, 430, 431, 432, 433, 434,
                435, 436, 437, 438, 439, 440, 441, 442, 443, 444, 445, 446, 447,
                448, 449, 450, 451, 452, 454, 455, 456, 457, 458, 459, 460, 461,
                462, 463, 464, 471, 473, 474, 475, 476, 477, 478, 479, 480, 481,
                482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 495,
                496, 497, 498, 499, 502, 505,
            })],
        ),
        SpecMeshTest(
            "HoledCubeSelectLess", "testHoledCubeSelectLess", "expectedHoledCubeSelectLess",
            [OperatorSpecEditMode("select_face_by_sides", {}, "FACE", {})],
        ),
        SpecMeshTest(
            "EmptyMeshSelectLess", "testEmptyMeshSelectLess", "expectedEmptyMeshSelectLess",
            [OperatorSpecEditMode("select_face_by_sides", {}, "VERT", {})],
        ),

        # select linked
        SpecMeshTest(
            "PlanesSelectLinked", "testPlanesSelectLinked", "expectedPlanesSelectedLinked",
            [OperatorSpecEditMode("select_linked", {}, "VERT", {7})],
        ),
        SpecMeshTest(
            "CubesSelectLinked", "testCubesSelectLinked", "expectedCubesSelectLinked",
            [OperatorSpecEditMode("select_linked", {}, "VERT", {11})],
        ),
        SpecMeshTest(
            "EmptyMeshSelectLinked", "testEmptyMeshSelectLinked", "expectedEmptyMeshSelectLinked",
            [OperatorSpecEditMode("select_linked", {}, "VERT", {})],
        ),

        # select nth (checkered deselect)
        SpecMeshTest(
            "CircleSelect2nd", "testCircleSelect2nd", "expectedCircleSelect2nd",
            [OperatorSpecEditMode("select_nth", {}, "VERT", {i for i in range(32)})],
        ),

        # Subdivide edgering - Not currently functional, operator returns inconsistently
        # SpecMeshTest("SubdivideEdgeringSurface", "testCylinderSubdivideEdgering", "expectedCylinderSubdivideEdgeringSurface",
        #         [OperatorSpecEditMode("subdivide_edgering", {"number_cuts": 5, "interpolation": 'SURFACE', "profile_shape_factor": 0.1}, "EDGE", {0, (i for i in range(96) if (i % 3))})]),
        # SpecMeshTest("SubdivideEdgeringPath", "testCylinderSubdivideEdgering", "expectedCylinderSubdivideEdgeringPath",
        #         [OperatorSpecEditMode("subdivide_edgering", {"number_cuts": 5, "interpolation": 'PATH', "profile_shape_factor": 0.1}, "EDGE", {0, (i for i in range(96) if (i % 3))})]),
        # SpecMeshTest("SubdivideEdgeringLinear", "testCylinderSubdivideEdgering", "expectedCylinderSubdivideEdgeringLinear",
        #         [OperatorSpecEditMode("subdivide_edgering", {"number_cuts": 5, "interpolation": 'LINEAR', "profile_shape_factor": 0.1}, "EDGE", {0, (i for i in range(96) if (i % 3))})]),

        # Symmetry Snap
        SpecMeshTest(
            "SymmetrySnap", "testPlaneSymmetrySnap", "expectedPlaneSymmetrySnap",
            [OperatorSpecEditMode("symmetry_snap", {"direction": 'POSITIVE_X', "threshold": 1, "factor": 0.75,
                                                    "use_center": False}, "VERT", {i for i in range(5)})],
        ),
        SpecMeshTest(
            "SymmetrySnapCenter", "testPlaneSymmetrySnap", "expectedPlaneSymmetrySnapCenter",
            [OperatorSpecEditMode("symmetry_snap", {"direction": 'NEGATIVE_X', "threshold": 1, "factor": 0.75,
                                                    "use_center": True}, "VERT", {i for i in range(5)})],
        ),

        # Triangulate Faces
        SpecMeshTest(
            "Triangulate Faces", "testCubeTriangulate", "expectedCubeTriangulate",
            [OperatorSpecEditMode("quads_convert_to_tris", {}, "FACE", {i for i in range(6)})],
        ),

        # Tris to Quads
        SpecMeshTest(
            "TrisToQuads", "testPlanesTrisToQuad", "expectedPlanesTrisToQuad",
            [OperatorSpecEditMode("tris_convert_to_quads", {"face_threshold": 0.174533, "shape_threshold": 0.174533,
                                                            "uvs": True, "vcols": True, "seam": True, "sharp": True, "materials": True}, "VERT", {i for i in range(32)})],
        ),

        # unsubdivide
        # normal case
        SpecMeshTest(
            "CubeFaceUnsubdivide", "testCubeUnsubdivide", "expectedCubeUnsubdivide",
            [OperatorSpecEditMode("unsubdivide", {}, "FACE", {i for i in range(6)})],
        ),

        # UV Manipulation
        SpecMeshTest(
            "UVRotate", "testCubeUV", "expectedCubeUVRotate",
            [OperatorSpecEditMode("uvs_rotate", {}, "FACE", {2})],
        ),
        SpecMeshTest(
            "UVRotateCCW", "testCubeUV", "expectedCubeUVRotateCCW",
            [OperatorSpecEditMode("uvs_rotate", {"use_ccw": True}, "FACE", {2})],
        ),
        SpecMeshTest(
            "UVReverse", "testCubeUV", "expectedCubeUVReverse",
            [OperatorSpecEditMode("uvs_reverse", {}, "FACE", {2})],
        ),
        SpecMeshTest(
            "UVAdd", "testCubeUV", "expectedCubeUVAdd",
                     [OperatorSpecEditMode("uv_texture_add", {}, "FACE", {})],
        ),
        SpecMeshTest(
            "UVRemove", "testCubeUV", "expectedCubeUVRemove",
            [OperatorSpecEditMode("uv_texture_remove", {}, "FACE", {})],
        ),


        # Vert Connect Concave
        SpecMeshTest(
            "VertexConnectConcave", "testPlaneVertConnectConcave", "expectedPlaneVertConnectConcave",
            [OperatorSpecEditMode("vert_connect_concave", {}, "FACE", {0})],
        ),
        SpecMeshTest(
            "VertexConnectConcaveConvexPentagon", "testPentagonVertConnectConcave", "expectedPentagonVertConnectConcave",
            [OperatorSpecEditMode("vert_connect_concave", {}, "FACE", {0})],
        ),
        SpecMeshTest(
            "VertexConnectConcaveQuad", "testPlaneVertConnectConcaveQuad", "expectedPlaneVertConnectConcaveQuad",
            [OperatorSpecEditMode("vert_connect_concave", {}, "FACE", {0})],
        ),

        # Vert Connect Nonplanar
        SpecMeshTest(
            "VertexConnectNonplanar", "testPlaneVertConnectNonplanar", "expectedPlaneVertConnectNonplanar",
            [OperatorSpecEditMode("vert_connect_nonplanar", {
                                  "angle_limit": 0.17453292}, "VERT", {i for i in range(9)})],
        ),
        SpecMeshTest(
            "VertexConnectNonplanarNgon", "testPlaneVertConnectNonplanarNgon", "expectedPlaneVertConnectNonplanarNgon",
            [OperatorSpecEditMode("vert_connect_nonplanar", {"angle_limit": 0.218166}, "VERT", {i for i in range(6)})],
        ),


        # #87259 - test cases
        SpecMeshTest(
            "CubeEdgeUnsubdivide", "testCubeEdgeUnsubdivide", "expectedCubeEdgeUnsubdivide",
            [OperatorSpecEditMode("unsubdivide", {}, "EDGE", {i for i in range(6)})],
        ),
        SpecMeshTest(
            "UVSphereUnsubdivide", "testUVSphereUnsubdivide", "expectedUVSphereUnsubdivide",
            [OperatorSpecEditMode("unsubdivide", {'iterations': 9}, "FACE", {i for i in range(512)})],
        ),

        # vert connect path
        # Tip: It works only if there is an already existing face or more than 2 vertices.
        SpecMeshTest(
            "PlaneVertConnectPath", "testPlaneVertConnectPath", "expectedPlaneVertConnectPath",
            [OperatorSpecEditMode(
                "vert_connect_path", {}, "VERT", (0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14),
                select_history=True,
            )],
        ),

        # Laplacian Smooth
        SpecMeshTest(
            "LaplacianSmoothDefault", "testSphereLaplacianSmoothDefault", "expectedSphereLaplacianSmoothDefault",
            [OperatorSpecEditMode("vertices_smooth_laplacian", {
                                  "preserve_volume": False}, "VERT", {i for i in range(482)})],
        ),
        SpecMeshTest(
            "LaplacianSmoothHighValues", "testSphereLaplacianSmoothHigh", "expectedSphereLaplacianSmoothHigh",
            [OperatorSpecEditMode("vertices_smooth_laplacian",
                                  {"preserve_volume": False,
                                   "repeat": 100,
                                   "lambda_factor": 10.0},
                                  "VERT",
                                  {i for i in range(482)})],
        ),
        SpecMeshTest(
            "LaplacianSmoothBorder", "testCubeLaplacianSmoothBorder", "expectedCubeLaplacianSmoothBorder",
            [OperatorSpecEditMode("vertices_smooth_laplacian", {
                                  "preserve_volume": False, "lambda_border": 1.0}, "VERT", {i for i in range(25)})],
        ),
        SpecMeshTest(
            "LaplacianSmoothHighBorder", "testCubeLaplacianSmoothHighBorder", "expectedCubeLaplacianSmoothHighBorder",
            [OperatorSpecEditMode("vertices_smooth_laplacian", {
                                  "preserve_volume": False, "lambda_border": 100.0}, "VERT", {i for i in range(25)})],
        ),
        SpecMeshTest(
            "LaplacianSmoothPreserveVolume", "testSphereLaplacianSmoothPreserveVol", "expectedSphereLaplacianSmoothPreserveVol",
            [OperatorSpecEditMode("vertices_smooth_laplacian", {
                                  "preserve_volume": True}, "VERT", {i for i in range(482)})],
        ),


        # wireframe
        SpecMeshTest(
            "WireFrameDefault", "testCubeWireframeDefault", "expectedCubeWireframeDefault",
            [OperatorSpecEditMode("wireframe", {}, "FACE", {i for i in range(6)})],
        ),
        SpecMeshTest(
            "WireFrameAlt", "testCubeWireframeAlt", "expectedCubeWireframeAlt",
            [OperatorSpecEditMode(
                "wireframe", {
                    "use_boundary": False, "use_even_offset": False,
                    "use_relative_offset": True, "use_replace": False, "thickness": 0.3, "offset": 0.3,
                    "use_crease": True, "crease_weight": 0.01,
                }, "FACE", {i for i in range(6)})],
        ),

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
