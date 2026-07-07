# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
./blender.bin --background --factory-startup --python tests/python/bl_constraints.py -- --testdir /path/to/tests/files/constraints
"""

import pathlib
import sys
import unittest

import bpy
from bpy.types import Constraint
from mathutils import Matrix


class AbstractConstraintTests(unittest.TestCase):
    """Useful functionality for constraint tests."""

    layer_collection = ''  # When set, this layer collection will be enabled.

    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "constraints.blend"))

        # This allows developers to disable layer collections and declutter the
        # 3D Viewport while working in constraints.blend, without influencing
        # the actual unit tests themselves.
        if self.layer_collection:
            top_collection = bpy.context.view_layer.layer_collection
            collection = top_collection.children[self.layer_collection]
            collection.exclude = False

    def assert_matrix(self, actual_matrix, expect_matrix, object_name: str, places=None, delta=1e-6):
        """Asserts that the matrices almost equal."""
        self.assertEqual(len(actual_matrix), 4, 'Expected a 4x4 matrix')

        # TODO(Sybren): decompose the matrices and compare loc, rot, and scale separately.
        # That'll probably improve readability & understandability of test failures.
        for row, (act_row, exp_row) in enumerate(zip(actual_matrix, expect_matrix)):
            for col, (actual, expect) in enumerate(zip(act_row, exp_row)):
                self.assertAlmostEqual(
                    actual, expect, places=places, delta=delta,
                    msg=f'Matrix of object {object_name!r} failed: {actual} != {expect} at element [{row}][{col}]')

    def _get_eval_object(self, object_name: str) -> bpy.types.Object:
        """Return the evaluated object."""
        depsgraph = bpy.context.view_layer.depsgraph
        depsgraph.update()
        ob_orig = bpy.context.scene.objects[object_name]
        ob_eval = ob_orig.evaluated_get(depsgraph)
        return ob_eval

    def matrix(self, object_name: str) -> Matrix:
        """Return the evaluated world matrix."""
        ob_eval = self._get_eval_object(object_name)
        return ob_eval.matrix_world

    def bone_matrix(self, object_name: str, bone_name: str) -> Matrix:
        """Return the evaluated world matrix of the bone."""
        ob_eval = self._get_eval_object(object_name)
        bone = ob_eval.pose.bones[bone_name]
        return ob_eval.matrix_world @ bone.matrix

    def matrix_test(self, object_name: str, expect: Matrix):
        """Assert that the object's world matrix is as expected."""
        actual = self.matrix(object_name)
        self.assert_matrix(actual, expect, object_name)

    def bone_matrix_test(self, object_name: str, bone_name: str, expect: Matrix):
        """Assert that the bone's world matrix is as expected."""
        actual = self.bone_matrix(object_name, bone_name)
        self.assert_matrix(actual, expect, object_name)

    def constraint_context(self, constraint_name: str, owner_name: str = '') -> dict:
        """Return a context suitable for calling object constraint operators.

        Assumes the owner is called "{constraint_name}.owner" if owner_name=''.
        """
        owner = bpy.context.scene.objects[owner_name or f'{constraint_name}.owner']
        constraint = owner.constraints[constraint_name]
        context = {
            **bpy.context.copy(),
            'object': owner,
            'active_object': owner,
            'constraint': constraint,
        }
        return context

    def bone_constraint_context(self, constraint_name: str, owner_name: str = '', bone_name: str = '') -> dict:
        """Return a context suitable for calling bone constraint operators.

        Assumes the owner's object is called "{constraint_name}.owner" if owner_name=''.
        Assumes the bone is called "{constraint_name}.bone" if bone_name=''.
        """

        owner_name = owner_name or f'{constraint_name}.owner'
        bone_name = bone_name or f'{constraint_name}.bone'

        owner = bpy.context.scene.objects[owner_name]
        pose_bone = owner.pose.bones[bone_name]

        constraint = pose_bone.constraints[constraint_name]
        context = {
            **bpy.context.copy(),
            'object': owner,
            'active_object': owner,
            'active_pose_bone': pose_bone,
            'constraint': constraint,
            'owner': pose_bone,
        }
        return context


class ChildOfTest(AbstractConstraintTests):
    layer_collection = 'Child Of'

    def test_object_simple_parent(self):
        """Child Of: simple evaluation of object parent."""
        initial_matrix = Matrix((
            (0.5872668623924255, -0.3642929494380951, 0.29567837715148926, 1.0886117219924927),
            (0.31689348816871643, 0.7095895409584045, 0.05480116978287697, 2.178966999053955),
            (-0.21244174242019653, 0.06738340109586716, 0.8475662469863892, 3.2520291805267334),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.object.owner', initial_matrix)

        context_override = self.constraint_context('Child Of', owner_name='Child Of.object.owner')
        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_set_inverse(constraint='Child Of')
        self.matrix_test('Child Of.object.owner', Matrix((
            (0.9992385506629944, 0.019844001159071922, -0.03359175845980644, 0.10000011324882507),
            (-0.01744179055094719, 0.997369647026062, 0.07035345584154129, 0.1999998837709427),
            (0.034899525344371796, -0.06971397250890732, 0.9969563484191895, 0.3000001311302185),
            (0.0, 0.0, 0.0, 1.0),
        )))

        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_clear_inverse(constraint='Child Of')
        self.matrix_test('Child Of.object.owner', initial_matrix)

    def test_object_rotation_only(self):
        """Child Of: rotation only."""
        owner = bpy.context.scene.objects['Child Of.object.owner']
        constraint = owner.constraints['Child Of']
        constraint.use_location_x = constraint.use_location_y = constraint.use_location_z = False
        constraint.use_scale_x = constraint.use_scale_y = constraint.use_scale_z = False

        initial_matrix = Matrix((
            (0.8340795636177063, -0.4500490725040436, 0.31900957226753235, 0.10000000149011612),
            (0.4547243118286133, 0.8883093595504761, 0.06428192555904388, 0.20000000298023224),
            (-0.31230923533439636, 0.09144517779350281, 0.9455690383911133, 0.30000001192092896),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.object.owner', initial_matrix)

        context_override = self.constraint_context('Child Of', owner_name='Child Of.object.owner')
        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_set_inverse(constraint='Child Of')
        self.matrix_test('Child Of.object.owner', Matrix((
            (0.9992386102676392, 0.019843975082039833, -0.033591702580451965, 0.10000000149011612),
            (-0.017441781237721443, 0.9973695874214172, 0.0703534483909607, 0.20000000298023224),
            (0.03489946573972702, -0.06971397250890732, 0.9969563484191895, 0.30000001192092896),
            (0.0, 0.0, 0.0, 1.0),
        )))

        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_clear_inverse(constraint='Child Of')
        self.matrix_test('Child Of.object.owner', initial_matrix)

    def test_object_no_x_axis(self):
        """Child Of: loc/rot/scale on only Y and Z axes."""
        owner = bpy.context.scene.objects['Child Of.object.owner']
        constraint = owner.constraints['Child Of']
        constraint.use_location_x = False
        constraint.use_rotation_x = False
        constraint.use_scale_x = False

        initial_matrix = Matrix((
            (0.8294582366943359, -0.4013831615447998, 0.2102886438369751, 0.10000000149011612),
            (0.46277597546577454, 0.6895919442176819, 0.18639995157718658, 2.2317214012145996),
            (-0.31224438548088074, -0.06574578583240509, 0.8546382784843445, 3.219514846801758),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.object.owner', initial_matrix)

        context_override = self.constraint_context('Child Of', owner_name='Child Of.object.owner',)
        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_set_inverse(constraint='Child Of')
        self.matrix_test('Child Of.object.owner', Matrix((
            (0.9992386102676392, 0.019843991845846176, -0.03359176218509674, 0.10000000149011612),
            (-0.017441775649785995, 0.997369647026062, 0.0703534483909607, 0.2000001221895218),
            (0.034899499267339706, -0.06971398741006851, 0.996956467628479, 0.3000001311302185),
            (0.0, 0.0, 0.0, 1.0),
        )))

        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_clear_inverse(constraint='Child Of')
        self.matrix_test('Child Of.object.owner', initial_matrix)

    def test_bone_simple_parent(self):
        """Child Of: simple evaluation of bone parent."""
        initial_matrix = Matrix((
            (0.5133683681488037, -0.06418320536613464, -0.014910104684531689, -0.2277737855911255),
            (-0.03355155512690544, 0.12542974948883057, -1.080492377281189, 2.082599639892578),
            (0.019313642755150795, 1.0446348190307617, 0.1244703009724617, 2.8542778491973877),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.armature.owner', initial_matrix)

        context_override = self.constraint_context('Child Of', owner_name='Child Of.armature.owner')
        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_set_inverse(constraint='Child Of')

        self.matrix_test('Child Of.armature.owner', Matrix((
            (0.9992386102676392, 0.019843988120555878, -0.03359176218509674, 0.8358089923858643),
            (-0.017441775649785995, 0.997369647026062, 0.0703534483909607, 1.7178752422332764),
            (0.03489949554204941, -0.06971397995948792, 0.9969563484191895, -1.8132872581481934),
            (0.0, 0.0, 0.0, 1.0),
        )))

        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_clear_inverse(constraint='Child Of')
        self.matrix_test('Child Of.armature.owner', initial_matrix)

    def test_bone_owner(self):
        """Child Of: bone owns constraint, targeting object."""
        initial_matrix = Matrix((
            (0.9992387890815735, -0.03359174728393555, -0.019843988120555878, -2.999999523162842),
            (-0.02588011883199215, -0.1900751143693924, -0.9814283847808838, 2.0),
            (0.029196053743362427, 0.9811949133872986, -0.190799742937088, 0.9999999403953552),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.bone_matrix_test('Child Of.bone.owner', 'Child Of.bone', initial_matrix)

        context_override = self.bone_constraint_context('Child Of', owner_name='Child Of.bone.owner')
        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_set_inverse(constraint='Child Of', owner='BONE')

        self.bone_matrix_test('Child Of.bone.owner', 'Child Of.bone', Matrix((
            (0.9659260511398315, 0.2588191032409668, 4.656613428188905e-10, -2.999999761581421),
            (-3.725290742551124e-09, 1.4901162970204496e-08, -1.0, 0.9999999403953552),
            (-0.2588191032409668, 0.965925931930542, 0.0, 0.9999999403953552),
            (0.0, 0.0, 0.0, 1.0),
        )))

        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_clear_inverse(constraint='Child Of', owner='BONE')
        self.bone_matrix_test('Child Of.bone.owner', 'Child Of.bone', initial_matrix)

    def test_vertexgroup_simple_parent(self):
        """Child Of: simple evaluation of vertex group parent."""
        initial_matrix = Matrix((
            (-0.8076590895652771, 0.397272527217865, 0.4357309341430664, 1.188504934310913),
            (-0.4534659683704376, -0.8908230066299438, -0.028334975242614746, 1.7851561307907104),
            (0.3769024908542633, -0.22047416865825653, 0.8996308445930481, 3.4457669258117676),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.vertexgroup.owner', initial_matrix)

        context_override = self.constraint_context('Child Of', owner_name='Child Of.vertexgroup.owner')
        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_set_inverse(constraint='Child Of')

        self.matrix_test('Child Of.vertexgroup.owner', Matrix((
            (0.9992386102676392, 0.019843988120555878, -0.03359176218509674, 0.10000000149011612),
            (-0.017441775649785995, 0.997369647026062, 0.0703534483909607, 0.20000000298023224),
            (0.03489949554204941, -0.06971397995948792, 0.9969563484191895, 0.30000001192092896),
            (0.0, 0.0, 0.0, 1.0),
        )))

        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.childof_clear_inverse(constraint='Child Of')
        self.matrix_test('Child Of.vertexgroup.owner', initial_matrix)


class ObjectSolverTest(AbstractConstraintTests):
    layer_collection = 'Object Solver'

    def test_simple_parent(self):
        """Object Solver: simple evaluation of parent."""
        initial_matrix = Matrix((
            (-0.970368504524231, 0.03756079450249672, -0.23869234323501587, -0.681557297706604),
            (-0.24130365252494812, -0.2019258439540863, 0.9492092132568359, 6.148940086364746),
            (-0.012545102275907993, 0.9786801934242249, 0.20500603318214417, 2.2278831005096436),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Object Solver.owner', initial_matrix)

        context_override = self.constraint_context('Object Solver')
        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.objectsolver_set_inverse(constraint='Object Solver')
        self.matrix_test('Object Solver.owner', Matrix((
            (0.9992387294769287, 0.019843989983201027, -0.03359176591038704, 0.10000025480985641),
            (-0.017441747710108757, 0.9973697662353516, 0.07035345584154129, 0.1999993920326233),
            (0.034899502992630005, -0.06971398741006851, 0.996956467628479, 0.29999980330467224),
            (0.0, 0.0, 0.0, 1.0),
        )))

        with bpy.context.temp_override(**context_override):
            bpy.ops.constraint.objectsolver_clear_inverse(constraint='Object Solver')
        self.matrix_test('Object Solver.owner', initial_matrix)


class CustomSpaceTest(AbstractConstraintTests):
    layer_collection = 'Custom Space'

    def test_loc_like_object(self):
        """Custom Space: basic custom space evaluation for objects"""
        loc_like_constraint = bpy.data.objects["Custom Space.object.owner"].constraints["Copy Location"]
        loc_like_constraint.use_x = True
        loc_like_constraint.use_y = True
        loc_like_constraint.use_z = True
        self.matrix_test('Custom Space.object.owner', Matrix((
            (1.0, 0.0, -2.9802322387695312e-08, -0.01753106713294983),
            (0.0, 1.0, 0.0, -0.08039519190788269),
            (-2.9802322387695312e-08, 5.960464477539063e-08, 1.0, 0.1584688425064087),
            (0.0, 0.0, 0.0, 1.0),
        )))
        loc_like_constraint.use_x = False
        self.matrix_test('Custom Space.object.owner', Matrix((
            (1.0, 0.0, -2.9802322387695312e-08, 0.18370598554611206),
            (0.0, 1.0, 0.0, 0.47120195627212524),
            (-2.9802322387695312e-08, 5.960464477539063e-08, 1.0, -0.16521614789962769),
            (0.0, 0.0, 0.0, 1.0),
        )))
        loc_like_constraint.use_y = False
        self.matrix_test('Custom Space.object.owner', Matrix((
            (1.0, 0.0, -2.9802322387695312e-08, -0.46946945786476135),
            (0.0, 1.0, 0.0, 0.423120379447937),
            (-2.9802322387695312e-08, 5.960464477539063e-08, 1.0, -0.6532361507415771),
            (0.0, 0.0, 0.0, 1.0),
        )))
        loc_like_constraint.use_z = False
        loc_like_constraint.use_y = True
        self.matrix_test('Custom Space.object.owner', Matrix((
            (1.0, 0.0, -2.9802322387695312e-08, -0.346824586391449),
            (0.0, 1.0, 0.0, 1.0480815172195435),
            (-2.9802322387695312e-08, 5.960464477539063e-08, 1.0, 0.48802000284194946),
            (0.0, 0.0, 0.0, 1.0),
        )))

    def test_loc_like_armature(self):
        """Custom Space: basic custom space evaluation for bones"""
        loc_like_constraint = bpy.data.objects["Custom Space.armature.owner"].pose.bones["Bone"].constraints["Copy Location"]
        loc_like_constraint.use_x = True
        loc_like_constraint.use_y = True
        loc_like_constraint.use_z = True
        self.bone_matrix_test('Custom Space.armature.owner', 'Bone', Matrix((
            (0.4556015729904175, -0.03673229366540909, -0.8894257545471191, -0.01753103733062744),
            (-0.45956411957740784, -0.8654094934463501, -0.19966775178909302, -0.08039522171020508),
            (-0.762383222579956, 0.49971696734428406, -0.4111628830432892, 0.1584688425064087),
            (0.0, 0.0, 0.0, 1.0),
        )))
        loc_like_constraint.use_x = False
        self.bone_matrix_test('Custom Space.armature.owner', 'Bone', Matrix((
            (0.4556015729904175, -0.03673229366540909, -0.8894257545471191, -0.310153603553772),
            (-0.45956411957740784, -0.8654094934463501, -0.19966775178909302, -0.8824828863143921),
            (-0.762383222579956, 0.49971696734428406, -0.4111628830432892, 0.629145085811615),
            (0.0, 0.0, 0.0, 1.0),
        )))
        loc_like_constraint.use_y = False
        self.bone_matrix_test('Custom Space.armature.owner', 'Bone', Matrix((
            (0.4556015729904175, -0.03673229366540909, -0.8894257545471191, -1.0574829578399658),
            (-0.45956411957740784, -0.8654094934463501, -0.19966775178909302, -0.937495231628418),
            (-0.762383222579956, 0.49971696734428406, -0.4111628830432892, 0.07077804207801819),
            (0.0, 0.0, 0.0, 1.0),
        )))
        loc_like_constraint.use_z = False
        loc_like_constraint.use_y = True
        self.bone_matrix_test('Custom Space.armature.owner', 'Bone', Matrix((
            (0.4556015729904175, -0.03673229366540909, -0.8894257545471191, -0.25267064571380615),
            (-0.45956411957740784, -0.8654094934463501, -0.19966775178909302, -0.9449876546859741),
            (-0.762383222579956, 0.49971696734428406, -0.4111628830432892, 0.5583670735359192),
            (0.0, 0.0, 0.0, 1.0),
        )))


class CopyTransformsTest(AbstractConstraintTests):
    layer_collection = 'Copy Transforms'

    def test_mix_mode_object(self):
        """Copy Transforms: all mix modes for objects"""
        constraint = bpy.data.objects["Copy Transforms.object.owner"].constraints["Copy Transforms"]

        constraint.mix_mode = 'REPLACE'
        self.matrix_test('Copy Transforms.object.owner', Matrix((
            (-0.7818737626075745, 0.14389121532440186, 0.4845699667930603, -0.017531070858240128),
            (-0.2741589844226837, -0.591389000415802, -1.2397242784500122, -0.08039521425962448),
            (0.04909384995698929, -1.0109175443649292, 0.7942137122154236, 0.1584688276052475),
            (0.0, 0.0, 0.0, 1.0)
        )))

        constraint.mix_mode = 'BEFORE_FULL'
        self.matrix_test('Copy Transforms.object.owner', Matrix((
            (-1.0791258811950684, -0.021011866629123688, 0.3120136260986328, 0.9082338809967041),
            (0.2128538191318512, -0.3411901891231537, -1.7376484870910645, -0.39762523770332336),
            (-0.03584420680999756, -1.0162957906723022, 0.8004404306411743, -0.9015425443649292),
            (0.0, 0.0, 0.0, 1.0)
        )))

        constraint.mix_mode = 'BEFORE'
        self.matrix_test('Copy Transforms.object.owner', Matrix((
            (-0.9952367544174194, -0.03077685832977295, 0.05301344022154808, 0.9082338809967041),
            (-0.013416174799203873, -0.39984768629074097, -1.8665285110473633, -0.39762523770332336),
            (0.03660336509346962, -0.9833710193634033, 0.75728839635849, -0.9015425443649292),
            (0.0, 0.0, 0.0, 1.0)
        )))

        constraint.mix_mode = 'BEFORE_SPLIT'
        self.matrix_test('Copy Transforms.object.owner', Matrix((
            (-0.9952367544174194, -0.03077685832977295, 0.05301344022154808, -1.0175310373306274),
            (-0.013416174799203873, -0.39984768629074097, -1.8665285110473633, 0.9196047782897949),
            (0.03660336509346962, -0.9833710193634033, 0.75728839635849, 0.1584688276052475),
            (0.0, 0.0, 0.0, 1.0)
        )))

        constraint.mix_mode = 'AFTER_FULL'
        self.matrix_test('Copy Transforms.object.owner', Matrix((
            (-0.8939255475997925, -0.2866469621658325, 0.7563635110855103, -0.964445173740387),
            (-0.09460853785276413, -0.73727947473526, -1.0267245769500732, 0.9622588753700256),
            (0.37042146921157837, -1.1893107891082764, 1.0113294124603271, 0.21314144134521484),
            (0.0, 0.0, 0.0, 1.0)
        )))

        constraint.mix_mode = 'AFTER'
        self.matrix_test('Copy Transforms.object.owner', Matrix((
            (-0.9033845067024231, -0.2048732340335846, 0.7542480826377869, -0.964445173740387),
            (-0.1757974475622177, -0.6721230745315552, -1.5190268754959106, 0.9622588753700256),
            (0.38079890608787537, -0.7963172793388367, 1.0880682468414307, 0.21314144134521484),
            (0.0, 0.0, 0.0, 1.0)
        )))

        constraint.mix_mode = 'AFTER_SPLIT'
        self.matrix_test('Copy Transforms.object.owner', Matrix((
            (-0.9033845067024231, -0.2048732340335846, 0.7542480826377869, -1.0175310373306274),
            (-0.1757974475622177, -0.6721230745315552, -1.5190268754959106, 0.9196047782897949),
            (0.38079890608787537, -0.7963172793388367, 1.0880682468414307, 0.1584688276052475),
            (0.0, 0.0, 0.0, 1.0)
        )))


class ActionConstraintTest(AbstractConstraintTests):
    layer_collection = "Action"

    def constraint(self) -> Constraint:
        owner = bpy.context.scene.objects["Action.owner"]
        constraint = owner.constraints["Action"]
        return constraint

    def test_assign_action_slot_virgin(self):
        action = bpy.data.actions.new("Slotted")
        slot = action.slots.new('OBJECT', "Slot")

        con = self.constraint()
        con.action = action

        self.assertEqual(
            slot,
            con.action_slot,
            "Assigning an Action with a virgin slot should automatically select that slot")

    def test_mix_modes(self):
        owner = bpy.context.scene.objects["Action.owner"]
        target = bpy.context.scene.objects["Action.target"]

        action = bpy.data.actions.new("Slotted")
        slot = action.slots.new('OBJECT', "Slot")
        layer = action.layers.new(name="Layer")
        strip = layer.strips.new(type='KEYFRAME')
        strip.key_insert(slot, "location", 0, 2.0, 0.0)
        strip.key_insert(slot, "location", 0, 7.0, 10.0)

        con = owner.constraints["Action"]
        con.action = action
        con.action_slot = slot
        con.transform_channel = 'LOCATION_X'
        con.min = -1.0
        con.max = 1.0
        con.frame_start = 0
        con.frame_end = 10

        # Set the constrained object's location to something other than [0,0,0],
        # so we can verify that it's actually replaced/mixed as appropriate to
        # the mix mode.
        owner.location = (2.0, 3.0, 4.0)

        con.mix_mode = 'REPLACE'
        self.matrix_test("Action.owner", Matrix((
            (1.0, 0.0, 0.0, 4.5),
            (0.0, 1.0, 0.0, 0.0),
            (0.0, 0.0, 1.0, 0.0),
            (0.0, 0.0, 0.0, 1.0)
        )))

        con.mix_mode = 'BEFORE_SPLIT'
        self.matrix_test("Action.owner", Matrix((
            (1.0, 0.0, 0.0, 6.5),
            (0.0, 1.0, 0.0, 3.0),
            (0.0, 0.0, 1.0, 4.0),
            (0.0, 0.0, 0.0, 1.0)
        )))


class GeometryAttributeConstraintTest(AbstractConstraintTests):
    layer_collection = "Geometry Attribute"

    def test_mix_modes(self):
        owner = bpy.context.scene.objects["Geometry Attribute.owner"]
        con = owner.constraints["Geometry Attribute"]
        con.apply_target_transform = False

        # This should produce the matrix as stored in the geometry attribute.
        con.mix_mode = 'REPLACE'
        self.matrix_test(owner.name, Matrix(
            ((0.32139378786087036, -0.41721203923225403, 0.8500824570655823, -1.0),
             (0.5566704273223877, 0.809456467628479, 0.18681080639362335, -1.0),
             (-0.7660444378852844, 0.41317591071128845, 0.49240389466285706, 0.0),
             (0.0, 0.0, 0.0, 1.0))),
        )

        con.mix_mode = 'BEFORE_SPLIT'
        self.matrix_test(owner.name, Matrix(
            ((0.32139378786087036, -0.41721203923225403, 0.8500824570655823, -0.8999999761581421),
             (0.5566704273223877, 0.809456467628479, 0.18681080639362335, -0.800000011920929),
             (-0.7660444378852844, 0.41317591071128845, 0.49240389466285706, 0.30000001192092896),
             (0.0, 0.0, 0.0, 1.0))),
        )

    def test_apply_target_transform(self):
        owner = bpy.context.scene.objects["Geometry Attribute.owner"]
        con = owner.constraints["Geometry Attribute"]
        con.apply_target_transform = True

        con.mix_mode = 'REPLACE'
        self.matrix_test(owner.name, Matrix(
            ((0.5681133270263672, -0.2360532432794571, 0.788369357585907, -0.923704206943512),
             (0.3527687191963196, 0.9353533983230591, 0.02585163712501526, -0.5034461617469788),
             (-0.7435062527656555, 0.26342537999153137, 0.6146588921546936, 0.012183472514152527),
             (0.0, 0.0, 0.0, 1.0))),
        )


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
