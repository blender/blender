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

# semi-useful script, runs all operators in a number of different
# contexts, cheap way to find misc small bugs but is in no way a complete test.
#
# only error checked for here is a segfault.

import bpy
import sys

USE_ATTRSET = False
USE_FILES = ""  # "/mango/"
USE_RANDOM = False
USE_RANDOM_SCREEN = False
RANDOM_SEED = [1]  # so we can redo crashes
RANDOM_RESET = 0.1  # 10% chance of resetting on each new operator
RANDOM_MULTIPLY = 10

STATE = {
    "counter": 0,
}


op_blacklist = (
    "script.reload",
    "export*.*",
    "import*.*",
    "*.save_*",
    "*.read_*",
    "*.open_*",
    "*.link_append",
    "render.render",
    "render.play_rendered_anim",
    "sound.bake_animation",    # OK but slow
    "sound.mixdown",           # OK but slow
    "object.bake_image",       # OK but slow
    "object.paths_calculate",  # OK but slow
    "object.paths_update",     # OK but slow
    "ptcache.bake_all",        # OK but slow
    "nla.bake",                # OK but slow
    "*.*_export",
    "*.*_import",
    "ed.undo",
    "ed.undo_push",
    "script.autoexec_warn_clear",
    "screen.delete",           # already used for random screens
    "wm.blenderplayer_start",
    "wm.recover_auto_save",
    "wm.quit_blender",
    "wm.window_close",
    "wm.url_open",
    "wm.doc_view",
    "wm.doc_edit",
    "wm.doc_view_manual",
    "wm.path_open",
    "wm.copy_prev_settings",
    "wm.theme_install",
    "wm.context_*",
    "wm.properties_add",
    "wm.properties_remove",
    "wm.properties_edit",
    "wm.properties_context_change",
    "wm.operator_cheat_sheet",
    "wm.interface_theme_*",
    "wm.previews_ensure",       # slow - but harmless
    "wm.appconfig_*",           # just annoying - but harmless
    "wm.keyitem_add",           # just annoying - but harmless
    "wm.keyconfig_activate",    # just annoying - but harmless
    "wm.keyconfig_preset_add",  # just annoying - but harmless
    "wm.keyconfig_test",        # just annoying - but harmless
    "wm.memory_statistics",     # another annoying one
    "wm.dependency_relations",  # another annoying one
    "wm.keymap_restore",        # another annoying one
    "wm.addon_*",               # harmless, but dont change state
    "console.*",                # just annoying - but harmless
)


def blend_list(mainpath):
    import os
    from os.path import join, splitext

    def file_list(path, filename_check=None):
        for dirpath, dirnames, filenames in os.walk(path):
            # skip '.git'
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]

            for filename in filenames:
                filepath = join(dirpath, filename)
                if filename_check is None or filename_check(filepath):
                    yield filepath

    def is_blend(filename):
        ext = splitext(filename)[1]
        return (ext in {".blend", })

    return list(sorted(file_list(mainpath, is_blend)))


if USE_FILES:
    USE_FILES_LS = blend_list(USE_FILES)
    # print(USE_FILES_LS)


def filter_op_list(operators):
    from fnmatch import fnmatchcase

    def is_op_ok(op):
        for op_match in op_blacklist:
            if fnmatchcase(op, op_match):
                print("    skipping: %s (%s)" % (op, op_match))
                return False
        return True

    operators[:] = [op for op in operators if is_op_ok(op[0])]


def reset_blend():
    bpy.ops.wm.read_factory_settings()
    for scene in bpy.data.scenes:
        # reduce range so any bake action doesnt take too long
        scene.frame_start = 1
        scene.frame_end = 5

    if USE_RANDOM_SCREEN:
        import random
        for i in range(random.randint(0, len(bpy.data.screens))):
            bpy.ops.screen.delete()
        print("Scree IS", bpy.context.screen)


def reset_file():
    import random
    f = USE_FILES_LS[random.randint(0, len(USE_FILES_LS) - 1)]
    bpy.ops.wm.open_mainfile(filepath=f)


if USE_ATTRSET:
    def build_property_typemap(skip_classes):

        property_typemap = {}

        for attr in dir(bpy.types):
            cls = getattr(bpy.types, attr)
            if issubclass(cls, skip_classes):
                continue

            # # to support skip-save we cant get all props
            # properties = cls.bl_rna.properties.keys()
            properties = []
            for prop_id, prop in cls.bl_rna.properties.items():
                if not prop.is_skip_save:
                    properties.append(prop_id)

            properties.remove("rna_type")
            property_typemap[attr] = properties

        return property_typemap
    CLS_BLACKLIST = (
        bpy.types.BrushTextureSlot,
        bpy.types.Brush,
    )
    property_typemap = build_property_typemap(CLS_BLACKLIST)
    bpy_struct_type = bpy.types.Struct.__base__

    def id_walk(value, parent):
        value_type = type(value)
        value_type_name = value_type.__name__

        value_id = getattr(value, "id_data", Ellipsis)
        value_props = property_typemap.get(value_type_name, ())

        for prop in value_props:
            subvalue = getattr(value, prop)

            if subvalue == parent:
                continue
            # grr, recursive!
            if prop == "point_caches":
                continue
            subvalue_type = type(subvalue)
            yield value, prop, subvalue_type
            subvalue_id = getattr(subvalue, "id_data", Ellipsis)

            if value_id == subvalue_id:
                if subvalue_type == float:
                    pass
                elif subvalue_type == int:
                    pass
                elif subvalue_type == bool:
                    pass
                elif subvalue_type == str:
                    pass
                elif hasattr(subvalue, "__len__"):
                    for sub_item in subvalue[:]:
                        if isinstance(sub_item, bpy_struct_type):
                            subitem_id = getattr(sub_item, "id_data", Ellipsis)
                            if subitem_id == subvalue_id:
                                yield from id_walk(sub_item, value)

                if subvalue_type.__name__ in property_typemap:
                    yield from id_walk(subvalue, value)

    # main function
    _random_values = (
        None, object, type,
        1, 0.1, -1,  # float("nan"),
        "", "test", b"", b"test",
        (), [], {},
        (10,), (10, 20), (0, 0, 0),
        {0: "", 1: "hello", 2: "test"}, {"": 0, "hello": 1, "test": 2},
        set(), {"", "test", "."}, {None, ..., type},
        range(10), (" " * i for i in range(10)),
    )

    def attrset_data():
        for attr in dir(bpy.data):
            if attr == "window_managers":
                continue
            seq = getattr(bpy.data, attr)
            if seq.__class__.__name__ == 'bpy_prop_collection':
                for id_data in seq:
                    for val, prop, tp in id_walk(id_data, bpy.data):
                        # print(id_data)
                        for val_rnd in _random_values:
                            try:
                                setattr(val, prop, val_rnd)
                            except:
                                pass


def run_ops(operators, setup_func=None, reset=True):
    print("\ncontext:", setup_func.__name__)

    # first invoke
    for op_id, op in operators:
        if op.poll():
            print("    operator: %4d, %s" % (STATE["counter"], op_id))
            STATE["counter"] += 1
            sys.stdout.flush()  # in case of crash

            # disable will get blender in a bad state and crash easy!
            if reset:
                reset_test = True
                if USE_RANDOM:
                    import random
                    if random.random() < (1.0 - RANDOM_RESET):
                        reset_test = False

                if reset_test:
                    if USE_FILES:
                        reset_file()
                    else:
                        reset_blend()
                del reset_test

            if USE_RANDOM:
                # we can't be sure it will work
                try:
                    setup_func()
                except:
                    pass
            else:
                setup_func()

            for mode in {'EXEC_DEFAULT', 'INVOKE_DEFAULT'}:
                try:
                    op(mode)
                except:
                    # import traceback
                    # traceback.print_exc()
                    pass

                if USE_ATTRSET:
                    attrset_data()

    if not operators:
        # run test
        if reset:
            reset_blend()
        if USE_RANDOM:
            # we can't be sure it will work
            try:
                setup_func()
            except:
                pass
        else:
            setup_func()


# contexts
def ctx_clear_scene():  # copied from batch_import.py
    bpy.ops.wm.read_factory_settings(use_empty=True)


def ctx_editmode_mesh():
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_mesh_extra():
    bpy.ops.object.vertex_group_add()
    bpy.ops.object.shape_key_add(from_mix=False)
    bpy.ops.object.shape_key_add(from_mix=True)
    bpy.ops.mesh.uv_texture_add()
    bpy.ops.mesh.vertex_color_add()
    bpy.ops.object.material_slot_add()
    # editmode last!
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_mesh_empty():
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.delete()


def ctx_editmode_curves():
    bpy.ops.curve.primitive_nurbs_circle_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_curves_empty():
    bpy.ops.curve.primitive_nurbs_circle_add()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.curve.select_all(action='SELECT')
    bpy.ops.curve.delete(type='VERT')


def ctx_editmode_surface():
    bpy.ops.surface.primitive_nurbs_surface_torus_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_mball():
    bpy.ops.object.metaball_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_text():
    bpy.ops.object.text_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_armature():
    bpy.ops.object.armature_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_armature_empty():
    bpy.ops.object.armature_add()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.armature.select_all(action='SELECT')
    bpy.ops.armature.delete()


def ctx_editmode_lattice():
    bpy.ops.object.add(type='LATTICE')
    bpy.ops.object.mode_set(mode='EDIT')
    # bpy.ops.object.vertex_group_add()


def ctx_object_empty():
    bpy.ops.object.add(type='EMPTY')


def ctx_object_pose():
    bpy.ops.object.armature_add()
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')


def ctx_object_paint_weight():
    bpy.ops.object.mode_set(mode='WEIGHT_PAINT')


def ctx_object_paint_vertex():
    bpy.ops.object.mode_set(mode='VERTEX_PAINT')


def ctx_object_paint_sculpt():
    bpy.ops.object.mode_set(mode='SCULPT')


def ctx_object_paint_texture():
    bpy.ops.object.mode_set(mode='TEXTURE_PAINT')


def bpy_check_type_duplicates():
    # non essential sanity check
    bl_types = dir(bpy.types)
    bl_types_unique = set(bl_types)

    if len(bl_types) != len(bl_types_unique):
        print("Error, found duplicates in 'bpy.types'")
        for t in sorted(bl_types_unique):
            tot = bl_types.count(t)
            if tot > 1:
                print("    '%s', %d" % (t, tot))
        import sys
        sys.exit(1)


def main():

    bpy_check_type_duplicates()

    # reset_blend()
    import bpy
    operators = []
    for mod_name in dir(bpy.ops):
        mod = getattr(bpy.ops, mod_name)
        for submod_name in dir(mod):
            op = getattr(mod, submod_name)
            operators.append(("%s.%s" % (mod_name, submod_name), op))

    operators.sort(key=lambda op: op[0])

    filter_op_list(operators)

    # for testing, mix the list up.
    # operators.reverse()

    if USE_RANDOM:
        import random
        random.seed(RANDOM_SEED[0])
        operators = operators * RANDOM_MULTIPLY
        random.shuffle(operators)

    # 2 passes, first just run setup_func to make sure they are ok
    for operators_test in ((), operators):
        # Run the operator tests in different contexts
        run_ops(operators_test, setup_func=lambda: None)

        if USE_FILES:
            continue

        run_ops(operators_test, setup_func=ctx_clear_scene)
        # object modes
        run_ops(operators_test, setup_func=ctx_object_empty)
        run_ops(operators_test, setup_func=ctx_object_pose)
        run_ops(operators_test, setup_func=ctx_object_paint_weight)
        run_ops(operators_test, setup_func=ctx_object_paint_vertex)
        run_ops(operators_test, setup_func=ctx_object_paint_sculpt)
        run_ops(operators_test, setup_func=ctx_object_paint_texture)
        # mesh
        run_ops(operators_test, setup_func=ctx_editmode_mesh)
        run_ops(operators_test, setup_func=ctx_editmode_mesh_extra)
        run_ops(operators_test, setup_func=ctx_editmode_mesh_empty)
        # armature
        run_ops(operators_test, setup_func=ctx_editmode_armature)
        run_ops(operators_test, setup_func=ctx_editmode_armature_empty)
        # curves
        run_ops(operators_test, setup_func=ctx_editmode_curves)
        run_ops(operators_test, setup_func=ctx_editmode_curves_empty)
        run_ops(operators_test, setup_func=ctx_editmode_surface)
        # other
        run_ops(operators_test, setup_func=ctx_editmode_mball)
        run_ops(operators_test, setup_func=ctx_editmode_text)
        run_ops(operators_test, setup_func=ctx_editmode_lattice)

        if not operators_test:
            print("All setup functions run fine!")

    print("Finished %r" % __file__)


if __name__ == "__main__":
    # ~ for i in range(200):
        # ~ RANDOM_SEED[0] += 1
        #~ main()
    main()
