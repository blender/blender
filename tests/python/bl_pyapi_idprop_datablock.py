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

import bpy
import sys
import os
import tempfile
import traceback
import inspect
from bpy.types import UIList

arr_len = 100
ob_cp_count = 100
lib_path = os.path.join(tempfile.gettempdir(), "lib.blend")
test_path = os.path.join(tempfile.gettempdir(), "test.blend")


def print_fail_msg_and_exit(msg):
    def __LINE__():
        try:
            raise Exception
        except:
            return sys.exc_info()[2].tb_frame.f_back.f_back.f_back.f_lineno

    def __FILE__():
        return inspect.currentframe().f_code.co_filename

    print("'%s': %d >> %s" % (__FILE__(), __LINE__(), msg), file=sys.stderr)
    sys.stderr.flush()
    sys.stdout.flush()
    os._exit(1)


def abort_if_false(expr, msg=None):
    if not expr:
        if not msg:
            msg = "test failed"
        print_fail_msg_and_exit(msg)


class TestClass(bpy.types.PropertyGroup):
    test_prop = bpy.props.PointerProperty(type=bpy.types.Object)
    name = bpy.props.StringProperty()


def get_scene(lib_name, sce_name):
    for s in bpy.data.scenes:
        if s.name == sce_name:
            if (
                    (s.library and s.library.name == lib_name) or
                    (lib_name is None and s.library is None)
            ):
                return s


def check_crash(fnc, args=None):
    try:
        fnc(args) if args else fnc()
    except:
        return
    print_fail_msg_and_exit("test failed")


def init():
    bpy.utils.register_class(TestClass)
    bpy.types.Object.prop_array = bpy.props.CollectionProperty(
        name="prop_array",
        type=TestClass)
    bpy.types.Object.prop = bpy.props.PointerProperty(type=bpy.types.Object)


def make_lib():
    bpy.ops.wm.read_factory_settings()

    # datablock pointer to the Camera object
    bpy.data.objects["Cube"].prop = bpy.data.objects['Camera']

    # array of datablock pointers to the Light object
    for i in range(0, arr_len):
        a = bpy.data.objects["Cube"].prop_array.add()
        a.test_prop = bpy.data.objects['Light']
        a.name = a.test_prop.name

    # make unique named copy of the cube
    ob = bpy.data.objects["Cube"].copy()
    bpy.context.scene.objects.link(ob)

    bpy.data.objects["Cube.001"].name = "Unique_Cube"

    # duplicating of Cube
    for i in range(0, ob_cp_count):
        ob = bpy.data.objects["Cube"].copy()
        bpy.context.scene.objects.link(ob)

    # nodes
    bpy.data.scenes["Scene"].use_nodes = True
    bpy.data.scenes["Scene"].node_tree.nodes['Render Layers']["prop"] =\
        bpy.data.objects['Camera']

    # rename scene and save
    bpy.data.scenes["Scene"].name = "Scene_lib"
    bpy.ops.wm.save_as_mainfile(filepath=lib_path)


def check_lib():
    # check pointer
    abort_if_false(bpy.data.objects["Cube"].prop == bpy.data.objects['Camera'])

    # check array of pointers in duplicated object
    for i in range(0, arr_len):
        abort_if_false(bpy.data.objects["Cube.001"].prop_array[i].test_prop ==
                       bpy.data.objects['Light'])


def check_lib_linking():
    # open startup file
    bpy.ops.wm.read_factory_settings()

    # link scene to the startup file
    with bpy.data.libraries.load(lib_path, link=True) as (data_from, data_to):
        data_to.scenes = ["Scene_lib"]

    o = bpy.data.scenes["Scene_lib"].objects['Unique_Cube']

    abort_if_false(o.prop_array[0].test_prop == bpy.data.scenes["Scene_lib"].objects['Light'])
    abort_if_false(o.prop == bpy.data.scenes["Scene_lib"].objects['Camera'])
    abort_if_false(o.prop.library == o.library)

    bpy.ops.wm.save_as_mainfile(filepath=test_path)


def check_linked_scene_copying():
    # full copy of the scene with datablock props
    bpy.ops.wm.open_mainfile(filepath=test_path)
    bpy.data.screens['Default'].scene = bpy.data.scenes["Scene_lib"]
    bpy.ops.scene.new(type='FULL_COPY')

    # check save/open
    bpy.ops.wm.save_as_mainfile(filepath=test_path)
    bpy.ops.wm.open_mainfile(filepath=test_path)

    intern_sce = get_scene(None, "Scene_lib")
    extern_sce = get_scene("Lib", "Scene_lib")

    # check node's props
    # we made full copy from linked scene, so pointers must equal each other
    abort_if_false(intern_sce.node_tree.nodes['Render Layers']["prop"] and
                   intern_sce.node_tree.nodes['Render Layers']["prop"] ==
                   extern_sce.node_tree.nodes['Render Layers']["prop"])


def check_scene_copying():
    # full copy of the scene with datablock props
    bpy.ops.wm.open_mainfile(filepath=lib_path)
    bpy.data.screens['Default'].scene = bpy.data.scenes["Scene_lib"]
    bpy.ops.scene.new(type='FULL_COPY')

    path = test_path + "_"
    # check save/open
    bpy.ops.wm.save_as_mainfile(filepath=path)
    bpy.ops.wm.open_mainfile(filepath=path)

    first_sce = get_scene(None, "Scene_lib")
    second_sce = get_scene(None, "Scene_lib.001")

    # check node's props
    # must point to own scene camera
    abort_if_false(not (first_sce.node_tree.nodes['Render Layers']["prop"] ==
                        second_sce.node_tree.nodes['Render Layers']["prop"]))


# count users
def test_users_counting():
    bpy.ops.wm.read_factory_settings()
    Light_us = bpy.data.objects["Light"].data.users
    n = 1000
    for i in range(0, n):
        bpy.data.objects["Cube"]["a%s" % i] = bpy.data.objects["Light"].data
    abort_if_false(bpy.data.objects["Light"].data.users == Light_us + n)

    for i in range(0, int(n / 2)):
        bpy.data.objects["Cube"]["a%s" % i] = 1
    abort_if_false(bpy.data.objects["Light"].data.users == Light_us + int(n / 2))


# linking
def test_linking():
    make_lib()
    check_lib()
    check_lib_linking()
    check_linked_scene_copying()
    check_scene_copying()


# check restrictions for datablock pointers for some classes; GUI for manual testing
def test_restrictions1():
    class TEST_Op(bpy.types.Operator):
        bl_idname = 'scene.test_op'
        bl_label = 'Test'
        bl_options = {"INTERNAL"}
        str_prop = bpy.props.StringProperty(name="str_prop")

        # disallow registration of datablock properties in operators
        # will be checked in the draw method (test manually)
        # also, see console:
        #   ValueError: bpy_struct "SCENE_OT_test_op" doesn't support datablock properties
        id_prop = bpy.props.PointerProperty(type=bpy.types.Object)

        def execute(self, context):
            return {'FINISHED'}

    # just panel for testing the poll callback with lots of objects
    class TEST_PT_DatablockProp(bpy.types.Panel):
        bl_label = "Datablock IDProp"
        bl_space_type = "PROPERTIES"
        bl_region_type = "WINDOW"
        bl_context = "render"

        def draw(self, context):
            self.layout.prop_search(context.scene, "prop", bpy.data,
                                    "objects")
            self.layout.template_ID(context.scene, "prop1")
            self.layout.prop_search(context.scene, "prop2", bpy.data, "node_groups")

            op = self.layout.operator("scene.test_op")
            op.str_prop = "test string"

            def test_fnc(op):
                op["ob"] = bpy.data.objects['Unique_Cube']
            check_crash(test_fnc, op)
            abort_if_false(not hasattr(op, "id_prop"))

    bpy.utils.register_class(TEST_PT_DatablockProp)
    bpy.utils.register_class(TEST_Op)

    def poll(self, value):
        return value.name in bpy.data.scenes["Scene_lib"].objects

    def poll1(self, value):
        return True

    bpy.types.Scene.prop = bpy.props.PointerProperty(type=bpy.types.Object)
    bpy.types.Scene.prop1 = bpy.props.PointerProperty(type=bpy.types.Object, poll=poll)
    bpy.types.Scene.prop2 = bpy.props.PointerProperty(type=bpy.types.NodeTree, poll=poll1)

    # check poll effect on UI (poll returns false => red alert)
    bpy.context.scene.prop = bpy.data.objects["Light.001"]
    bpy.context.scene.prop1 = bpy.data.objects["Light.001"]

    # check incorrect type assignment
    def sub_test():
        # NodeTree id_prop
        bpy.context.scene.prop2 = bpy.data.objects["Light.001"]

    check_crash(sub_test)

    bpy.context.scene.prop2 = bpy.data.node_groups.new("Shader", "ShaderNodeTree")

    print("Please, test GUI performance manually on the Render tab, '%s' panel" %
          TEST_PT_DatablockProp.bl_label, file=sys.stderr)
    sys.stderr.flush()


# check some possible regressions
def test_regressions():
    bpy.types.Object.prop_str = bpy.props.StringProperty(name="str")
    bpy.data.objects["Unique_Cube"].prop_str = "test"

    bpy.types.Object.prop_gr = bpy.props.PointerProperty(
        name="prop_gr",
        type=TestClass,
        description="test")

    bpy.data.objects["Unique_Cube"].prop_gr = None


# test restrictions for datablock pointers
def test_restrictions2():
    class TestClassCollection(bpy.types.PropertyGroup):
        prop = bpy.props.CollectionProperty(
            name="prop_array",
            type=TestClass)
    bpy.utils.register_class(TestClassCollection)

    class TestPrefs(bpy.types.AddonPreferences):
        bl_idname = "testprefs"
        # expecting crash during registering
        my_prop2 = bpy.props.PointerProperty(type=TestClass)

        prop = bpy.props.PointerProperty(
            name="prop",
            type=TestClassCollection,
            description="test")

    bpy.types.Addon.a = bpy.props.PointerProperty(type=bpy.types.Object)

    class TestUIList(UIList):
        test = bpy.props.PointerProperty(type=bpy.types.Object)

        def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
            layout.prop(item, "name", text="", emboss=False, icon_value=icon)

    check_crash(bpy.utils.register_class, TestPrefs)
    check_crash(bpy.utils.register_class, TestUIList)

    bpy.utils.unregister_class(TestClassCollection)


def main():
    init()
    test_users_counting()
    test_linking()
    test_restrictions1()
    check_crash(test_regressions)
    test_restrictions2()


if __name__ == "__main__":
    try:
        main()
    except:
        import traceback

        traceback.print_exc()
        sys.stderr.flush()
        os._exit(1)
