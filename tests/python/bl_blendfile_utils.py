# SPDX-FileCopyrightText: 2020-2022 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import os
import pprint
import unittest


class TestHelper(unittest.TestCase):

    def __init__(self, args):
        super().__init__()
        self.args = args

    @staticmethod
    def id_to_uid(id_data):
        return (type(id_data).__name__,
                id_data.name_full,
                id_data.users)

    @classmethod
    def blender_data_to_tuple(cls, bdata, pprint_name=None):
        ret = sorted(tuple((cls.id_to_uid(k), sorted(tuple(cls.id_to_uid(vv) for vv in v)))
                           for k, v in bdata.user_map().items()))
        if pprint_name is not None:
            print("\n%s:" % pprint_name)
            pprint.pprint(ret)
        return ret

    @staticmethod
    def ensure_path(path):
        if not os.path.exists(path):
            os.makedirs(path)

    def run_all_tests(self):
        for inst_attr_id in dir(self):
            if not inst_attr_id.startswith("test_"):
                continue
            inst_attr = getattr(self, inst_attr_id)
            if not callable(inst_attr):
                continue

            self.setUp()
            try:
                inst_attr()
            finally:
                self.tearDown()


class TestBlendLibLinkHelper(TestHelper):
    """
    Generate relatively complex data layout across several blendfiles.

    Useful for testing link/append/etc., but also data relationships e.g.
    """

    def __init__(self, args):
        assert hasattr(args, "src_test_dir")
        assert hasattr(args, "output_dir")
        super().__init__(args)

    @staticmethod
    def reset_blender():
        bpy.ops.wm.read_homefile(use_empty=True, use_factory_startup=True)
        bpy.data.orphans_purge(do_recursive=True)

    def unique_blendfile_name(self, base_name):
        return base_name + self.__class__.__name__ + ".blend"

    # Utils generating common data patterns.
    # Assume that the current blendfile is cleared and empty.

    def gen_indirect_library_data_(self):
        im = bpy.data.images.load(os.path.join(self.args.src_test_dir,
                                               "imbuf_io",
                                               "reference",
                                               "jpeg-rgb-90__from__rgba08.jpg"))
        im.name = "LibMaterial"
        self.assertTrue(len(im.pixels) > 0)
        self.assertTrue(im.has_data)

        ma = bpy.data.materials.new("LibMaterial")
        ma.use_fake_user = True
        out_node = ma.node_tree.nodes.new("ShaderNodeOutputMaterial")
        bsdf_node = ma.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
        ma.node_tree.links.new(bsdf_node.outputs["BSDF"], out_node.inputs["Surface"])
        teximage_node = ma.node_tree.nodes.new("ShaderNodeTexImage")
        teximage_node.image = im
        ma.node_tree.links.new(bsdf_node.inputs["Base Color"], teximage_node.outputs["Color"])

    def gen_library_data_(self):
        me = bpy.data.meshes.new("LibMesh")
        ob = bpy.data.objects.new("LibMesh", me)
        coll = bpy.data.collections.new("LibMesh")
        coll.objects.link(ob)
        bpy.context.scene.collection.children.link(coll)

    # Actual init code, generating blendfiles for various linking/appending scenarii.

    def init_lib_data_basic(self):
        self.reset_blender()

        self.gen_library_data_()

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_basic"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        return output_lib_path

    def init_lib_data_animated(self):
        self.reset_blender()

        self.gen_library_data_()

        ob = bpy.data.objects[0]
        ob_ctrl = bpy.data.objects.new("LibController", None)
        coll = bpy.data.collections[0]
        coll.objects.link(ob_ctrl)

        # Add some action & driver animation to `LibMesh`.
        # Animate Y location.
        ob.location[1] = 0.0
        ob.keyframe_insert("location", index=1, frame=1)
        ob.location[1] = -5.0
        ob.keyframe_insert("location", index=1, frame=10)

        # Drive X location.
        ob_drv = ob.driver_add("location", 0)
        ob_drv.driver.type = 'AVERAGE'
        ob_drv_var = ob_drv.driver.variables.new()
        ob_drv_var.type = 'TRANSFORMS'
        ob_drv_var.targets[0].id = ob_ctrl
        ob_drv_var.targets[0].transform_type = 'LOC_X'

        # Add some action & driver animation to `LibController`.
        # Animate X location.
        ob_ctrl.location[0] = 0.0
        ob_ctrl.keyframe_insert("location", index=0, frame=1)
        ob_ctrl.location[0] = 5.0
        ob_ctrl.keyframe_insert("location", index=0, frame=10)

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_animated"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        return output_lib_path

    def init_lib_data_indirect_lib(self):
        output_dir = self.args.output_dir
        self.ensure_path(output_dir)

        # Create an indirect library containing a material, and an image texture.
        self.reset_blender()

        self.gen_indirect_library_data_()

        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_indirect_material"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        # Create a main library containing object etc., and linking material from indirect library.
        self.reset_blender()

        link_dir = os.path.join(output_lib_path, "Material")
        bpy.ops.wm.link(directory=link_dir, filename="LibMaterial")
        ma = bpy.data.materials[0]

        self.gen_library_data_()

        me = bpy.data.meshes[0]
        me.materials.append(ma)

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_indirect_main"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        return output_lib_path

    def init_lib_data_packed_indirect_lib(self):
        output_dir = self.args.output_dir
        self.ensure_path(output_dir)

        # Create an indirect library containing a material, and an image texture.
        self.reset_blender()

        self.gen_indirect_library_data_()

        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_indirect_material"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        # Create a main library containing object etc., and linking material from indirect library.
        self.reset_blender()

        self.gen_library_data_()

        link_dir = os.path.join(output_lib_path, "Material")
        bpy.ops.wm.link(directory=link_dir, filename="LibMaterial")

        ma = bpy.data.pack_linked_ids_hierarchy(bpy.data.materials[0])

        me = bpy.data.meshes[0]
        me.materials.append(ma)

        bpy.ops.outliner.orphans_purge()

        self.assertEqual(len(bpy.data.materials), 1)
        self.assertTrue(bpy.data.materials[0].is_linked_packed)

        self.assertEqual(len(bpy.data.images), 1)
        self.assertTrue(bpy.data.images[0].is_linked_packed)

        self.assertEqual(len(bpy.data.libraries), 2)
        self.assertFalse(bpy.data.libraries[0].is_archive)
        self.assertTrue(bpy.data.libraries[1].is_archive)
        self.assertIn(bpy.data.libraries[1].name, bpy.data.libraries[0].archive_libraries)

        output_dir = self.args.output_dir
        self.ensure_path(output_dir)
        # Take care to keep the name unique so multiple test jobs can run at once.
        output_lib_path = os.path.join(output_dir, self.unique_blendfile_name("blendlib_indirect_main"))

        bpy.ops.wm.save_as_mainfile(filepath=output_lib_path, check_existing=False, compress=False)

        return output_lib_path
