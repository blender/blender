# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
blender -b --factory-startup --python tests/python/bl_animation_armature.py
"""

import unittest
from typing import TypeAlias

import bpy
from mathutils import Vector
from bpy.types import EditBone, Object, Armature

Vectorish: TypeAlias = Vector | tuple[float, float, float]


class BoneCollectionTest(unittest.TestCase):
    arm_ob: bpy.types.Object
    arm: bpy.types.Armature

    def setUp(self):
        bpy.ops.wm.read_homefile(use_factory_startup=True)
        self.arm_ob, self.arm = self.create_armature()

    def create_armature(self) -> tuple[bpy.types.Object, bpy.types.Armature]:
        arm = bpy.data.armatures.new('Armature')
        arm_ob = bpy.data.objects.new('ArmObject', arm)

        # Link to the scene just for giggles. And ease of debugging when things
        # go bad.
        bpy.context.scene.collection.objects.link(arm_ob)

        return arm_ob, arm

    def add_bones(self, arm_ob: bpy.types.Object) -> dict[str, bpy.types.Bone]:
        """Add some test bones to the armature."""

        # Switch to edit mode to add some bones.
        bpy.context.view_layer.objects.active = arm_ob
        bpy.ops.object.mode_set(mode='EDIT')
        self.assertEqual('EDIT', arm_ob.mode, 'Armature should be in edit mode now')

        arm = arm_ob.data
        bone_names = ('root', 'child_L', 'child_R', 'child_L_L', 'child_L_R', 'child_R_L', 'child_R_R')
        try:
            for bone_name in bone_names:
                ebone = arm.edit_bones.new(name=bone_name)
                # Bones have to have a length, or they will be removed when exiting edit mode.
                ebone.tail = (1, 0, 0)

            arm.edit_bones['child_L'].parent = arm.edit_bones['root']
            arm.edit_bones['child_R'].parent = arm.edit_bones['root']
            arm.edit_bones['child_L_L'].parent = arm.edit_bones['child_L']
            arm.edit_bones['child_L_R'].parent = arm.edit_bones['child_L']
            arm.edit_bones['child_R_L'].parent = arm.edit_bones['child_R']
            arm.edit_bones['child_R_R'].parent = arm.edit_bones['child_R']

        finally:
            bpy.ops.object.mode_set(mode='OBJECT')

        # Return the bones, not the editbones.
        return {bone_name: arm.bones[bone_name]
                for bone_name in bone_names}

    def test_bone_collection_api(self):
        # Just to keep the rest of the code shorter.
        bcolls = self.arm.collections
        bcolls_all = self.arm.collections_all

        self.assertEqual([], list(bcolls), "By default an Armature should have no collections")

        # Build a hierarchy.
        root1 = bcolls.new('root1')
        r1_child1 = bcolls.new('r1_child1', parent=root1)
        r1_child1_001 = bcolls.new('r1_child1', parent=root1)
        root2 = bcolls.new('root2')
        r2_child1 = bcolls.new('r2_child1', parent=root2)
        r2_child2 = bcolls.new('r2_child2', parent=root2)

        self.assertEqual('r1_child1.001', r1_child1_001.name, 'Names should be unique')

        # Check the hierarchy.
        self.assertEqual([root1, root2], list(bcolls), 'armature.collections should reflect only the roots')
        self.assertEqual([r1_child1, r1_child1_001], list(root1.children), 'root1.children should have its children')
        self.assertEqual([r2_child1, r2_child2], list(root2.children), 'root2.children should have its children')
        self.assertEqual([], list(r1_child1.children))
        self.assertIsNone(root1.parent)
        self.assertEqual(root1, r1_child1.parent)

        # Check the array order.
        self.assertEqual([root1, root2, r1_child1, r1_child1_001, r2_child1, r2_child2], list(bcolls_all))

        # Move root2 to become the child of r1_child1.
        root2.parent = r1_child1

        # Check the hierarchy.
        self.assertEqual([root1], list(bcolls), 'armature.collections should reflect only the roots')
        self.assertEqual([root2], list(r1_child1.children))
        self.assertEqual([r1_child1, r1_child1_001], list(root1.children), 'root1.children should have its children')
        self.assertEqual([r2_child1, r2_child2], list(root2.children), 'root2.children should have its children')
        self.assertEqual(r1_child1, root2.parent)

        # Check the array order.
        self.assertEqual([root1, r1_child1, r1_child1_001, r2_child1, r2_child2, root2], list(bcolls_all))

        # Move root2 between r1_child1 and r1_child1_001.
        root2.parent = root1
        root2.child_number = 1

        # Check the hierarchy.
        self.assertEqual([root1], list(bcolls), 'armature.collections should reflect only the roots')
        self.assertEqual([r1_child1, root2, r1_child1_001], list(
            root1.children), 'root1.children should have its children')
        self.assertEqual([r2_child1, r2_child2], list(root2.children), 'root2.children should have its children')

        # Check the array order.
        self.assertEqual([root1, r1_child1, root2, r1_child1_001, r2_child1, r2_child2], list(bcolls_all))

    def test_parent_property(self):
        # Just to keep the rest of the code shorter.
        bcolls = self.arm.collections

        self.assertEqual([], list(bcolls), "By default an Armature should have no collections")

        # Build a hierarchy.
        root1 = bcolls.new('root1')
        r1_child1 = bcolls.new('r1_child1', parent=root1)
        r1_child2 = bcolls.new('r1_child2', parent=root1)
        root2 = bcolls.new('root2')
        r2_child1 = bcolls.new('r2_child1', parent=root2)
        r2_child2 = bcolls.new('r2_child2', parent=root2)

        # Check getting the parent.
        self.assertEqual(root1, r1_child1.parent)
        self.assertEqual(root1, r1_child2.parent)
        self.assertEqual(root2, r2_child1.parent)
        self.assertEqual(root2, r2_child2.parent)

        # Move r1_child1 to be a child of root2 by assigning the parent.
        r1_child1.parent = root2
        self.assertEqual(root2, r1_child1.parent)

        # Check the sibling order.
        self.assertEqual([r2_child1, r2_child2, r1_child1], list(root2.children))

        # Make r1_child1 a root.
        r1_child1.parent = None
        self.assertIsNone(r1_child1.parent)

    def test_bone_collection_bones(self):
        # Build a hierarchy on the armature.
        bcolls = self.arm.collections
        bcoll_root = bcolls.new('root')
        bcoll_child1 = bcolls.new('child1', parent=bcoll_root)
        bcoll_child2 = bcolls.new('child2', parent=bcoll_child1)

        # Add bones to the armature & assign to collections.
        bone_dict = self.add_bones(self.arm_ob)
        bcoll_root.assign(bone_dict['root'])

        bcoll_child1_bone_names = {'child_L', 'child_L_L', 'child_L_R'}
        for bone_name in bcoll_child1_bone_names:
            bcoll_child1.assign(bone_dict[bone_name])

        bcoll_child2_bone_names = {'child_R', 'child_R_L', 'child_R_R'}
        for bone_name in bcoll_child2_bone_names:
            bcoll_child2.assign(bone_dict[bone_name])

        # Check that the `.bones` property returns the expected ones.
        self.assertEqual([self.arm.bones['root']], list(bcoll_root.bones))
        self.assertEqual(bcoll_child1_bone_names, {b.name for b in bcoll_child1.bones})
        self.assertEqual(bcoll_child2_bone_names, {b.name for b in bcoll_child2.bones})

        # Check that the `.bones_recursive` property returns the expected bones.
        all_bones = set(self.arm.bones)
        self.assertEqual(all_bones, set(bcoll_root.bones_recursive),
                         'All bones should have been assigned to at least one bone collection')

        self.assertEqual(bcoll_child1_bone_names | bcoll_child2_bone_names,
                         {b.name for b in bcoll_child1.bones_recursive},
                         "All bones of child1 and child2 should be in child1.bones_recursive")

        self.assertEqual(bcoll_child2_bone_names, {b.name for b in bcoll_child2.bones_recursive})

    def test_bone_collection_armature_join(self):
        other_arm_ob, other_arm = self.create_armature()

        # Build a hierarchy on the main armature.
        main_bcolls = self.arm.collections
        main_root = main_bcolls.new('root')
        main_child1 = main_bcolls.new('child1', parent=main_root)
        main_child2 = main_bcolls.new('child2', parent=main_root)

        # Build a hierarchy on the other armature.
        other_bcolls = other_arm.collections
        other_root = other_bcolls.new('root')
        other_child1 = other_bcolls.new('child1', parent=other_root)
        other_child3 = other_bcolls.new('child3', parent=other_root)

        # Create some custom properties on the collections.
        main_root['float'] = 0.2
        main_child1['string'] = 'main_string'
        main_child2['dict'] = {'agent': 47}

        other_root['float'] = 0.42
        other_child1['strange'] = 'other_string'
        other_child3['dict'] = {'agent': 327}

        # Join the two armatures together.
        self.assertEqual({'FINISHED'}, bpy.ops.object.select_all(action='DESELECT'))
        bpy.context.view_layer.objects.active = self.arm_ob
        self.arm_ob.select_set(True)
        other_arm_ob.select_set(True)
        self.assertEqual({'FINISHED'}, bpy.ops.object.join())

        # Check the custom properties.
        bcolls_all = self.arm.collections_all
        self.assertEqual(0.2, bcolls_all['root']['float'])
        self.assertEqual('main_string', bcolls_all['child1']['string'])
        self.assertEqual({'agent': 47}, bcolls_all['child2']['dict'].to_dict())
        self.assertNotIn(
            'strange',
            bcolls_all['child1'],
            'Bone collections that already existed in the active armature are not expected to be updated')
        self.assertEqual({'agent': 327}, bcolls_all['child3']['dict'].to_dict())


class ArmatureCreationTest(unittest.TestCase):
    arm_ob: Object
    arm: Armature

    def setUp(self) -> None:
        print("\033[92mloading empty homefile\033[0m")
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        self.arm_ob, self.arm = self.create_armature()

    @staticmethod
    def create_armature() -> tuple[Object, Armature]:
        """Create an Armature without any bones."""
        arm = bpy.data.armatures.new('Armature')
        arm_ob = bpy.data.objects.new('ArmObject', arm)

        # Remove any pre-existing bone.
        while arm.bones:
            arm.bones.remove(arm.bones[0])

        bpy.context.scene.collection.objects.link(arm_ob)
        bpy.context.view_layer.objects.active = arm_ob

        return arm_ob, arm

    def create_bone(self, name: str, parent: EditBone | None, head: Vectorish, tail: Vectorish) -> EditBone:
        bone = self.arm.edit_bones.new(name)
        bone.parent = parent
        bone.head = head
        bone.tail = tail
        bone.use_connect = True
        return bone

    def test_tiny_bones(self) -> None:
        """Tiny bones should be elongated."""

        # 'bpy.context.active_object' does not exist when Blender is running in
        # GUI mode. That's not the normal way to run this test, but very useful
        # to be able to do for debugging purposes.
        with bpy.context.temp_override(active_object=self.arm_ob):
            bpy.ops.object.mode_set(mode='EDIT')

        # Constants defined in `ED_armature_from_edit()`:
        bone_length_threshold = 0.000001
        adjusted_bone_length = 2 * bone_length_threshold

        # A value for which the vector (under_threshold, 0, under_threshold)
        # is still shorter than the bone length threshold.
        under_threshold = 0.0000006

        root = self.create_bone("root", None, (0, 0, 0), (0, 0, 1))

        tinychild_1 = self.create_bone(
            "tinychild_1",
            root,
            root.tail,
            root.tail + Vector((0, under_threshold, 0)),
        )
        self.create_bone(
            "tinychild_2",
            root,
            root.tail,
            root.tail + Vector((under_threshold, 0, under_threshold)),
        )
        self.create_bone(
            "zerochild_3",
            root,
            root.tail,
            root.tail,
        )

        # Give a tiny child a grandchild that is also tiny, in a perpendicular direction.
        self.create_bone(
            "tinygrandchild_1_1",
            tinychild_1,
            tinychild_1.tail,
            tinychild_1.tail + Vector((under_threshold, 0, 0)),
        )

        # Add a grandchild that is long enough.
        grandchild_1_2 = self.create_bone(
            "grandchild_1_2",
            tinychild_1,
            tinychild_1.tail,
            tinychild_1.tail + Vector((1, 0, 0)),
        )

        # Add a great-grandchild, it should remain connected to its parent.
        self.create_bone(
            "great_grandchild_1_2_1",
            grandchild_1_2,
            grandchild_1_2.tail,
            grandchild_1_2.tail + Vector((1, 0, 0)),
        )

        # Switch out and back into Armature Edit mode, to see how the bones survived the round-trip.
        with bpy.context.temp_override(active_object=self.arm_ob):
            bpy.ops.object.mode_set(mode='OBJECT')
            bpy.ops.object.mode_set(mode='EDIT')

        # Check that all bones still exist, and have the expected head/tail. This
        # comparison is done again in Armature Edit mode, so that all the numbers
        # mean the same thing as they meant when creating the bones.

        actual_names = sorted(bone.name for bone in self.arm.edit_bones)
        expect_names = sorted(["root", "tinychild_1", "tinychild_2", "zerochild_3", "tinygrandchild_1_1",
                               "grandchild_1_2", "great_grandchild_1_2_1"])
        self.assertEqual(expect_names, actual_names)

        def check_bone(
                name: str,
                expect_head: Vectorish,
                expect_tail: Vectorish,
                *,
                expect_connected: bool,
                msg: str,
                places=7,
        ) -> None:
            bone = self.arm.edit_bones[name]
            # Convert to tuples for nicer printing in failure messages.
            actual_head = bone.head.to_tuple()
            actual_tail = bone.tail.to_tuple()

            head_msg = "\n{}:\n  Expected head ({:.8f}, {:.8f}, {:.8f}),\n      Actual is ({:.8f}, {:.8f}, {:.8f}).\n  {}".format(
                name, expect_head[0], expect_head[1], expect_head[2], actual_head[0], actual_head[1], actual_head[2], msg)
            self.assertAlmostEqual(expect_head[0], actual_head[0], places=places, msg=head_msg)
            self.assertAlmostEqual(expect_head[1], actual_head[1], places=places, msg=head_msg)
            self.assertAlmostEqual(expect_head[2], actual_head[2], places=places, msg=head_msg)
            # print("\n{}:\n  Head is ({:.8f}, {:.8f}, {:.8f})".format(
            #     name, actual_head[0], actual_head[1], actual_head[2]))

            tail_msg = "\n{}:\n  Expected tail ({:.8f}, {:.8f}, {:.8f}),\n      Actual is ({:.8f}, {:.8f}, {:.8f}).\n  {}".format(
                name, expect_tail[0], expect_tail[1], expect_tail[2], actual_tail[0], actual_tail[1], actual_tail[2], msg)
            self.assertAlmostEqual(expect_tail[0], actual_tail[0], places=places, msg=tail_msg)
            self.assertAlmostEqual(expect_tail[1], actual_tail[1], places=places, msg=tail_msg)
            self.assertAlmostEqual(expect_tail[2], actual_tail[2], places=places, msg=tail_msg)
            # print("  Tail is ({:.8f}, {:.8f}, {:.8f})".format(
            #     actual_tail[0], actual_tail[1], actual_tail[2]))

            self.assertEqual(expect_connected, bone.use_connect, msg="{}: {}".format(bone.name, msg))

        check_bone("root", (0, 0, 0), (0, 0, 1),
                   expect_connected=True, msg="Should not have changed.")
        check_bone("tinychild_1", (0, 0, 1), (0, adjusted_bone_length, 1),
                   expect_connected=True, msg="Should have been elongated in the Y-direction")

        adjust = (Vector((under_threshold, 0, under_threshold)).normalized() * adjusted_bone_length).x
        check_bone("tinychild_2",
                   (0, 0, 1),
                   (adjust, 0, 1 + adjust),
                   expect_connected=True,
                   msg="Should have been elongated in the XZ-direction")

        check_bone("zerochild_3",
                   (0, 0, 1),
                   (0, 0, 1 + adjusted_bone_length),
                   expect_connected=True,
                   msg="Should have been elongated in the Z-direction")

        check_bone("tinygrandchild_1_1",
                   (0, under_threshold, 1),
                   (adjusted_bone_length, under_threshold, 1),
                   expect_connected=False,
                   msg="Should have been elongated in the X-direction and disconnected")

        check_bone("grandchild_1_2",
                   (0, under_threshold, 1),
                   (1, under_threshold, 1),
                   expect_connected=False,
                   msg="Should been disconnected")

        check_bone("great_grandchild_1_2_1",
                   (1, under_threshold, 1),
                   (2, under_threshold, 1),
                   expect_connected=True,
                   msg="Should been kept connected")


def main():
    import sys

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        # Avoid passing all of Blender's arguments to unittest.main()
        argv = [sys.argv[0]]

    unittest.main(argv=argv, exit=False)


if __name__ == "__main__":
    main()
