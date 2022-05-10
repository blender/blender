# SPDX-License-Identifier: GPL-2.0-or-later

# <pep8 compliant>

import math
import os
import sys
from random import shuffle, seed

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import RunTest, ModifierSpec, SpecMeshTest

seed(0)


def get_generate_modifiers_list(test_object_name, randomize=False):
    """
    Construct a list of 'Generate' modifiers with default parameters.
    :param test_object_name: str - name of test object. Some modifiers like boolean need an extra parameter beside
                                    the default one. E.g. boolean needs object, mask needs vertex group etc...
                                    The extra parameter name will be <test_object_name>_<modifier_type>
    :param randomize: bool - if True shuffle the list of modifiers.
    :return: list of 'Generate' modifiers with default parameters.
    """

    boolean_test_object = bpy.data.objects[test_object_name + "_boolean"]

    generate_modifiers = [
        ModifierSpec('array', 'ARRAY', {}),
        ModifierSpec('bevel', 'BEVEL', {'width': 0.1, 'limit_method': 'NONE'}),
        ModifierSpec('boolean', 'BOOLEAN', {'object': boolean_test_object, 'solver': 'FAST'}),
        ModifierSpec('build', 'BUILD', {'frame_start': 1, 'frame_duration': 1}, 2),
        ModifierSpec('decimate', 'DECIMATE', {}),
        ModifierSpec('edge split', 'EDGE_SPLIT', {}),

        # mask can effectively delete the mesh since the vertex group need to be updated after each
        # applied modifier. Needs to be tested separately.
        # ModifierSpec('mask', 'MASK', {'vertex_group': mask_vertex_group}, False),

        ModifierSpec('mirror', 'MIRROR', {}),
        ModifierSpec('multires', 'MULTIRES', {}),

        # remesh can also generate an empty mesh. Skip.
        # ModifierSpec('remesh', 'REMESH', {}),

        # ModifierSpec('screw', 'SCREW', {}), # screw can make the test very slow. Skipping for now.

        ModifierSpec('solidify', 'SOLIDIFY', {}),
        # Opensubdiv results might differ slightly when compiled with different optimization flags.
        #ModifierSpec('subsurf', 'SUBSURF', {}),
        ModifierSpec('triangulate', 'TRIANGULATE', {}),
        ModifierSpec('wireframe', 'WIREFRAME', {})

    ]

    if randomize:
        shuffle(generate_modifiers)

    return generate_modifiers


def main():
    mask_first_list = get_generate_modifiers_list("testCubeMaskFirst", randomize=True)
    mask_vertex_group = "testCubeMaskFirst" + "_mask"
    mask_first_list.insert(0, ModifierSpec('mask', 'MASK', {'vertex_group': mask_vertex_group}))

    tests = [
        ###############################
        # List of 'Generate' modifiers on a cube
        ###############################
        # 0
        # SpecMeshTest("testCube", "expectedCube", get_generate_modifiers_list("testCube")),
        SpecMeshTest("CubeRandom", "testCubeRandom", "expectedCubeRandom",
                     get_generate_modifiers_list("testCubeRandom", randomize=True)),
        SpecMeshTest("CubeMaskFirst", "testCubeMaskFirst", "expectedCubeMaskFirst", mask_first_list),

        SpecMeshTest("CollapseDecimate", "testCollapseDecimate", "expectedCollapseDecimate",
                     [ModifierSpec('decimate', 'DECIMATE',
                                   {'decimate_type': 'COLLAPSE', 'ratio': 0.25, 'use_collapse_triangulate': True})]),
        SpecMeshTest("PlanarDecimate", "testPlanarDecimate", "expectedPlanarDecimate",
                     [ModifierSpec('decimate', 'DECIMATE',
                                   {'decimate_type': 'DISSOLVE', 'angle_limit': math.radians(30)})]),
        SpecMeshTest("UnsubdivideDecimate", "testUnsubdivideDecimate", "expectedUnsubdivideDecimate",
                     [ModifierSpec('decimate', 'DECIMATE', {'decimate_type': 'UNSUBDIV', 'iterations': 2})]),

        # 5
        SpecMeshTest("RadialBisectMirror", "testRadialBisectMirror", "expectedRadialBisectMirror",
                     [ModifierSpec('mirror1', 'MIRROR', {'use_bisect_axis': (True, False, False)}),
                      ModifierSpec('mirror2', 'MIRROR', {'use_bisect_axis': (True, False, False),
                                                         'mirror_object': bpy.data.objects[
                                                         "testRadialBisectMirrorHelper"]}),
                      ModifierSpec('mirror3', 'MIRROR',
                                   {'use_axis': (False, True, False), 'use_bisect_axis': (False, True, False),
                                    'use_bisect_flip_axis': (False, True, False),
                                    'mirror_object': bpy.data.objects["testRadialBisectMirrorHelper"]})]),
        SpecMeshTest("T58411Mirror", "regressT58411Mirror", "expectedT58411Mirror",
                     [ModifierSpec('mirror', 'MIRROR', {}),
                      ModifierSpec('bevel', 'BEVEL', {'segments': 2, 'limit_method': 'WEIGHT'}),
                      ModifierSpec('subd', 'SUBSURF', {'levels': 1})]),

        SpecMeshTest("BasicScrew", "testBasicScrew", "expectedBasicScrew",
                     [ModifierSpec('mirror', 'MIRROR', {'mirror_object': bpy.data.objects["testBasicScrewHelper"]}),
                      ModifierSpec("screw", 'SCREW',
                                   {'angle': math.radians(400), 'steps': 20, 'iterations': 2, 'screw_offset': 2,
                                    'use_normal_calculate': True})]),
        SpecMeshTest("ObjectScrew", "testObjectScrew", "expectedObjectScrew",
                     [ModifierSpec('mirror', 'MIRROR', {'mirror_object': bpy.data.objects["testObjectScrewHelper2"]}),
                      ModifierSpec("screw", 'SCREW',
                                   {"angle": math.radians(600), 'steps': 32, 'iterations': 1,
                                    'use_object_screw_offset': True,
                                    'use_normal_calculate': True, 'object': bpy.data.objects["testObjectScrewHelper1"]})]),

        # 9
        SpecMeshTest("MergedScrewWeld", "testMergedScrewWeld", "expectedMergedScrewWeld",
                     [ModifierSpec("screw", 'SCREW',
                                   {'angle': math.radians(360), 'steps': 12, 'iterations': 1, 'screw_offset': 1,
                                    'use_normal_calculate': True, 'use_merge_vertices': True}),
                      ModifierSpec("weld", 'WELD', {"merge_threshold": 0.001})]),
        SpecMeshTest("T72380Weld", "regressT72380Weld", "expectedT72380Weld",
                     [ModifierSpec('vedit', 'VERTEX_WEIGHT_EDIT',
                                   {'vertex_group': 'Group', 'use_remove': True, 'remove_threshold': 1}),
                      ModifierSpec("weld", 'WELD', {"merge_threshold": 0.2, "vertex_group": "Group"})]),
        SpecMeshTest("T72792Weld", "regressT72792Weld", "expectedT72792Weld",
                     [ModifierSpec('array', 'ARRAY', {'fit_type': 'FIXED_COUNT', 'count': 2}),
                      ModifierSpec("weld", 'WELD', {"merge_threshold": 0.1, "vertex_group": "Group"})]),

        ############################################
        # One 'Generate' modifier on primitive meshes
        #############################################
        # 12
        SpecMeshTest("CubeArray", "testCubeArray", "expectedCubeArray",
                     [ModifierSpec('array', 'ARRAY', {})]),
        SpecMeshTest("CapArray", "testCapArray", "expectedCapArray",
                     [ModifierSpec('array', 'ARRAY',
                                   {'fit_type': 'FIT_LENGTH', 'fit_length': 2.0,
                                    'start_cap': bpy.data.objects["testCapStart"],
                                    'end_cap': bpy.data.objects["testCapEnd"]})]),
        SpecMeshTest("CurveArray", "testCurveArray", "expectedCurveArray",
                     [ModifierSpec('array', 'ARRAY',
                                   {'fit_type': 'FIT_CURVE', 'curve': bpy.data.objects["testCurveArrayHelper"],
                                    'use_relative_offset': False, 'use_constant_offset': True,
                                    'constant_offset_displace': (0.5, 0, 0)})]),
        SpecMeshTest("RadialArray", "testRadialArray", "expectedRadialArray",
                     [ModifierSpec('array', 'ARRAY', {'fit_type': 'FIXED_COUNT', 'count': 3, 'use_merge_vertices': True,
                                                      'use_merge_vertices_cap': True, 'use_relative_offset': False,
                                                      'use_object_offset': True,
                                                      'offset_object': bpy.data.objects["testRadialArrayHelper"]})]),

        SpecMeshTest("CylinderBuild", "testCylinderBuild", "expectedCylinderBuild",
                     [ModifierSpec('build', 'BUILD', {'frame_start': 1, 'frame_duration': 1}, 2)]),

        # 17
        SpecMeshTest("ConeDecimate", "testConeDecimate", "expectedConeDecimate",
                     [ModifierSpec('decimate', 'DECIMATE', {'ratio': 0.5})]),
        SpecMeshTest("CubeEdgeSplit", "testCubeEdgeSplit", "expectedCubeEdgeSplit",
                     [ModifierSpec('edge split', 'EDGE_SPLIT', {})]),

        SpecMeshTest("SphereMirror", "testSphereMirror", "expectedSphereMirror",
                     [ModifierSpec('mirror', 'MIRROR', {})]),
        SpecMeshTest("LocalMirror", "testLocalMirror", "expectedLocalMirror",
                     [ModifierSpec('mirror', 'MIRROR', {'use_clip': True})]),
        SpecMeshTest("ObjectOffsetMirror", "testObjectOffsetMirror", "expectedObjectOffsetMirror",
                     [ModifierSpec('mirror', 'MIRROR',
                                   {'mirror_object': bpy.data.objects["testObjectOffsetMirrorHelper"]})]),

        SpecMeshTest("CylinderMask", "testCylinderMask", "expectedCylinderMask",
                     [ModifierSpec('mask', 'MASK', {'vertex_group': "mask_vertex_group"})]),
        SpecMeshTest("ConeMultiRes", "testConeMultiRes", "expectedConeMultiRes",
                     [ModifierSpec('multires', 'MULTIRES', {})]),

        # 24
        SpecMeshTest("CubeScrew", "testCubeScrew", "expectedCubeScrew",
                     [ModifierSpec('screw', 'SCREW', {})]),

        SpecMeshTest("CubeSolidify", "testCubeSolidify", "expectedCubeSolidify",
                     [ModifierSpec('solidify', 'SOLIDIFY', {})]),
        SpecMeshTest("ComplexSolidify", "testComplexSolidify", "expectedComplexSolidify",
                     [ModifierSpec('solidify', 'SOLIDIFY', {'solidify_mode': 'NON_MANIFOLD', 'thickness': 0.05, 'offset': 0,
                                                            'nonmanifold_thickness_mode': 'CONSTRAINTS'})]),
        SpecMeshTest("T63063Solidify", "regressT63063Solidify", "expectedT63063Solidify",
                     [ModifierSpec('solid', 'SOLIDIFY', {'thickness': 0.1, 'offset': 0.7})]),
        SpecMeshTest("T61979Solidify", "regressT61979Solidify", "expectedT61979Solidify",
                     [ModifierSpec('solid', 'SOLIDIFY',
                                   {'thickness': -0.25, 'use_even_offset': True, 'use_quality_normals': True})]),

        SpecMeshTest("MonkeySubsurf", "testMonkeySubsurf", "expectedMonkeySubsurf",
                     [ModifierSpec('subsurf', 'SUBSURF', {})]),
        SpecMeshTest("CatmullClarkSubdivisionSurface", "testCatmullClarkSubdivisionSurface",
                     "expectedCatmullClarkSubdivisionSurface",
                     [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2})]),
        SpecMeshTest("SimpleSubdivisionSurface", "testSimpleSubdivisionSurface", "expectedSimpleSubdivisionSurface",
                     [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2, 'subdivision_type': 'SIMPLE'})]),
        SpecMeshTest("Crease2dSubdivisionSurface", "testCrease2dSubdivisionSurface", "expectedCrease2dSubdivisionSurface",
                     [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2})]),
        SpecMeshTest("Crease3dSubdivisionSurface", "testCrease3dSubdivisionSurface", "expectedCrease3dSubdivisionSurface",
                     [ModifierSpec("subdivision", 'SUBSURF', {"levels": 2})]),

        # 34

        SpecMeshTest("SphereTriangulate", "testSphereTriangulate", "expectedSphereTriangulate",
                     [ModifierSpec('triangulate', 'TRIANGULATE', {})]),
        SpecMeshTest("MonkeyWireframe", "testMonkeyWireframe", "expectedMonkeyWireframe",
                     [ModifierSpec('wireframe', 'WIREFRAME', {})]),

        # Duplicate the object, test object and expected object have same world coordinates.
        SpecMeshTest("Skin", "testObjPlaneSkin", "expObjPlaneSkin",
                     [ModifierSpec('skin', 'SKIN', {})]),

        SpecMeshTest("MergedWeld", "testMergedWeld", "expectedMergedWeld",
                     [ModifierSpec("weld", 'WELD', {"merge_threshold": 0.021})]),
        SpecMeshTest("MergedAllWeld", "testMergedAllWeld", "expectedMergedAllWeld",
                     [ModifierSpec("weld", 'WELD', {"merge_threshold": 1.8})]),
        SpecMeshTest("MergedNoneWeld", "testMergedNoneWeld", "expectedMergedNoneWeld",
                     [ModifierSpec("weld", 'WELD', {"merge_threshold": 0.019})]),


        #############################################
        # One 'Deform' modifier on primitive meshes
        #############################################
        # 39
        SpecMeshTest("MonkeyArmature", "testMonkeyArmature", "expectedMonkeyArmature",
                     [ModifierSpec('armature', 'ARMATURE',
                                   {'object': bpy.data.objects['testArmature'], 'use_vertex_groups': True})]),
        SpecMeshTest("TorusCast", "testTorusCast", "expectedTorusCast",
                     [ModifierSpec('cast', 'CAST', {'factor': 2.64})]),
        SpecMeshTest("CubeCurve", "testCubeCurve", "expectedCubeCurve",
                     [ModifierSpec('curve', 'CURVE', {'object': bpy.data.objects['testBezierCurve']})]),
        SpecMeshTest("MonkeyDisplace", "testMonkeyDisplace", "expectedMonkeyDisplace",
                     [ModifierSpec('displace', "DISPLACE", {})]),

        # Hook modifier requires moving the hook object to get a mesh change
        # so can't test it with the current framework
        # SpecMeshTest("MonkeyHook", "testMonkeyHook", "expectedMonkeyHook",
        #  [ModifierSpec('hook', 'HOOK', {'object': bpy.data.objects["EmptyHook"], 'vertex_group':
        #  "HookVertexGroup"})]),

        # 43
        # ModifierSpec('laplacian_deform', 'LAPLACIANDEFORM', {}) Laplacian requires a more complex mesh
        SpecMeshTest("CubeLattice", "testCubeLattice", "expectedCubeLattice",
                     [ModifierSpec('lattice', 'LATTICE', {'object': bpy.data.objects["testLattice"]})]),

        SpecMeshTest("PlaneShrinkWrap", "testPlaneShrinkWrap", "expectedPlaneShrinkWrap",
                     [ModifierSpec('shrinkwrap', 'SHRINKWRAP',
                                   {'target': bpy.data.objects["testCubeWrap"], 'offset': 0.5})]),

        SpecMeshTest("CylinderSimpleDeform", "testCylinderSimpleDeform", "expectedCylinderSimpleDeform",
                     [ModifierSpec('simple_deform', 'SIMPLE_DEFORM', {'angle': math.radians(180), 'deform_axis': 'Z'})]),

        SpecMeshTest("PlaneSmooth", "testPlaneSmooth", "expectedPlaneSmooth",
                     [ModifierSpec('smooth', 'SMOOTH', {'iterations': 11})]),

        # Smooth corrective requires a complex mesh.

        SpecMeshTest("BalloonLaplacianSmooth", "testBalloonLaplacianSmooth", "expectedBalloonLaplacianSmooth",
                     [ModifierSpec('laplaciansmooth', 'LAPLACIANSMOOTH', {'lambda_factor': 12, 'lambda_border': 12})]),

        # Gets updated often
        SpecMeshTest("WavePlane", "testObjPlaneWave", "expObjPlaneWave",
                     [ModifierSpec('wave', 'WAVE', {})]),

        #############################################
        # CURVES Generate Modifiers
        #############################################
        # Caution: Make sure test object has no modifier in "added" state, the test may fail.
        SpecMeshTest("BezCurveArray", "testObjBezierCurveArray", "expObjBezierCurveArray",
                     [ModifierSpec('array', 'ARRAY', {})]),

        SpecMeshTest("CurveBevel", "testObjBezierCurveBevel", "expObjBezierCurveBevel",
                     [ModifierSpec('bevel', 'BEVEL', {'limit_method': 'NONE'})]),

        SpecMeshTest("CurveBuild", "testObjBezierCurveBuild", "expObjBezierCurveBuild",
                     [ModifierSpec('build', 'BUILD', {'frame_start': 1, 'frame_duration': 1}, 2)]),

        SpecMeshTest("CurveDecimate", "testObjBezierCurveDecimate", "expObjBezierCurveDecimate",
                     [ModifierSpec('decimate', 'DECIMATE', {'ratio': 0.5})]),

        SpecMeshTest("CurveEdgeSplit", "testObjBezierCurveEdgeSplit", "expObjBezierCurveEdgeSplit",
                     [ModifierSpec('edgeSplit', 'EDGE_SPLIT', {})]),

        SpecMeshTest("CurveMirror", "testObjBezierCurveMirror", "expObjBezierCurveMirror",
                     [ModifierSpec('mirror', 'MIRROR', {'use_axis': (True, True, False)})]),

        SpecMeshTest("CurveScrew", "testObjBezierCurveScrew", "expObjBezierCurveScrew",
                     [ModifierSpec('screw', 'SCREW', {})]),

        SpecMeshTest("CurveSolidify", "testObjBezierCurveSolidify", "expObjBezierCurveSolidify",
                     [ModifierSpec('solidify', 'SOLIDIFY', {'thickness': 1})]),

        SpecMeshTest("CurveSubSurf", "testObjBezierCurveSubSurf", "expObjBezierCurveSubSurf",
                     [ModifierSpec('subSurf', 'SUBSURF', {})]),

        SpecMeshTest("CurveTriangulate", "testObjBezierCurveTriangulate", "expObjBezierCurveTriangulate",
                     [ModifierSpec('triangulate', 'TRIANGULATE', {})]),

        # Test 60
        # Caution Weld: if the distance is increased beyond a limit, the object disappears
        SpecMeshTest("CurveWeld", "testObjBezierCurveWeld", "expObjBezierCurveWeld",
                     [ModifierSpec('weld', 'WELD', {})]),

        SpecMeshTest("CurveWeld2", "testObjBezierCurveWeld2", "expObjBezierCurveWeld2",
                     [ModifierSpec('weld', 'WELD', {})]),

        #############################################
        # Curves Deform Modifiers
        #############################################
        # Test 62
        SpecMeshTest("CurveCast", "testObjBezierCurveCast", "expObjBezierCurveCast",
                     [ModifierSpec('Cast', 'CAST', {'cast_type': 'CYLINDER', 'factor': 10})]),

        SpecMeshTest("CurveShrinkWrap", "testObjBezierCurveShrinkWrap", "expObjBezierCurveShrinkWrap",
                     [ModifierSpec('ShrinkWrap', 'SHRINKWRAP',
                                   {'target': bpy.data.objects['testShrinkWrapHelperSuzanne']})]),

        SpecMeshTest("CurveSimpleDeform", "testObjBezierCurveSimpleDeform", "expObjBezierCurveSimpleDeform",
                     [ModifierSpec('simple_deform', 'SIMPLE_DEFORM', {'angle': math.radians(90)})]),

        SpecMeshTest("CurveSmooth", "testObjBezierCurveSmooth", "expObjBezierCurveSmooth",
                     [ModifierSpec('smooth', 'SMOOTH', {'factor': 10})]),

        SpecMeshTest("CurveWave", "testObjBezierCurveWave", "expObjBezierCurveWave",
                     [ModifierSpec('curve_wave', 'WAVE', {'time_offset': -1.5})]),

        SpecMeshTest("CurveCurve", "testObjBezierCurveCurve", "expObjBezierCurveCurve",
                     [ModifierSpec('curve_Curve', 'CURVE', {'object': bpy.data.objects['NurbsCurve']})]),

    ]

    modifiers_test = RunTest(tests)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            modifiers_test.apply_modifiers = True
            modifiers_test.do_compare = True
            modifiers_test.run_all_tests()
            break
        elif cmd == "--run-test":
            modifiers_test.apply_modifiers = False
            modifiers_test.do_compare = False
            name = command[i + 1]
            modifiers_test.run_test(name)
            break


if __name__ == "__main__":
    main()
