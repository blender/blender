# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script, runs all operators in a number of different contexts,
it's a convenient way to find various bugs but is in no way a complete test.

It's good at catching errors where operators fail when run in unexpected contexts.

This can be run with the following command line arguments:

  ./blender.bin -P tests/utils/bl_run_operators.py -- --random --random-seed=123

Or from GDB:

   gdb - ./blender.bin -ex=r --args ./blender.bin -P tests/utils/bl_run_operators.py -- --random --random-seed=123
"""

__all__ = (
    "main",
)

import bpy
import sys
import random


# This block is included in the script referenced by: `--generate-script`.
# BEGIN UTILS TO EXPORT.

# TODO: support this via command line arguments.
USE_ATTRSET = False

STATE = {
    "counter": 0,
}


def reset_blend(random_screen_index=-1):
    bpy.ops.wm.read_factory_settings()
    for scene in bpy.data.scenes:
        # Reduce range so any bake action doesn't take too long.
        scene.frame_end = scene.frame_start + 5

    if random_screen_index != -1:
        for _ in range(random_screen_index % len(bpy.data.screens)):
            bpy.ops.screen.delete()


def reset_file(filepath):
    bpy.ops.wm.open_mainfile(filepath=filepath)


def temp_override_default_kwargs(
        context,
        area_type=None,
        region_type=None,
):
    window = context.window_manager.windows[0]
    screen = window.screen

    kwargs = {
        "window": window,
        "screen": screen,
    }

    if (
            (area_type is not None) and
            (area := next(iter([area for area in screen.areas if area.type == area_type]), None))
    ):
        kwargs["area"] = area
        if (
                (region_type is not None) and
                (region := next(iter([region for region in area.regions if region.type == region_type]), None))
        ):
            kwargs["region"] = region

    return kwargs


def run_op(
        context,
        op,
        area_type=None,
        region_type=None,
):
    op_id = op.idname_py()

    print("    operator: {:04d}, {:s}".format(STATE["counter"], op_id))
    STATE["counter"] += 1
    sys.stdout.flush()  # in case of crash

    with context.temp_override(**temp_override_default_kwargs(context, area_type, region_type)):
        for mode in {
                'EXEC_DEFAULT',
                'INVOKE_DEFAULT',
        }:
            try:
                op(mode)
            except Exception:
                # import traceback
                # traceback.print_exc()
                pass

            if USE_ATTRSET:
                attrset_data()


# Contexts.

def ctx_nop():
    # This only exists so there is a callback name to reference that does nothing.
    pass


def ctx_clear_scene():  # copied from batch_import.py
    bpy.ops.wm.read_factory_settings(use_empty=True)


def ctx_editmode_mesh():
    bpy.ops.wm.read_factory_settings(use_empty=False)
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_mesh_extra():
    bpy.ops.wm.read_factory_settings(use_empty=False)
    bpy.ops.object.vertex_group_add()
    bpy.ops.object.shape_key_add(from_mix=False)
    bpy.ops.object.shape_key_add(from_mix=True)
    bpy.ops.mesh.uv_texture_add()
    bpy.ops.object.material_slot_add()
    # editmode last!
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_mesh_empty():
    bpy.ops.wm.read_factory_settings(use_empty=False)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.delete()


def ctx_editmode_curves():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.curve.primitive_nurbs_circle_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_curves_empty():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.curve.primitive_nurbs_circle_add()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.curve.select_all(action='SELECT')
    bpy.ops.curve.delete(type='VERT')


def ctx_editmode_surface():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.surface.primitive_nurbs_surface_torus_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_hair():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.mesh.primitive_plane_add()
    bpy.ops.object.curves_empty_hair_add()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.curves.add_circle()


def ctx_editmode_hair_empty():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.mesh.primitive_plane_add()
    bpy.ops.object.curves_empty_hair_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_grease_pencil():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.grease_pencil_add(type='MONKEY')
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_grease_pencil_empty():
    bpy.ops.object.grease_pencil_add(type='EMPTY')
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_mball():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.metaball_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_text():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.text_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_armature():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.armature_add()
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_armature_empty():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.armature_add()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.armature.select_all(action='SELECT')
    bpy.ops.armature.delete()


def ctx_editmode_lattice():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.add(type='LATTICE')
    bpy.ops.object.mode_set(mode='EDIT')
    # bpy.ops.object.vertex_group_add()


def ctx_object_empty():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.add(type='EMPTY')


def ctx_object_pose():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.armature_add()
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')


def ctx_object_volume():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.add(type='VOLUME')


def ctx_object_empty_as_camera():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.object.add(type='EMPTY')
    # Can't use the active object as it may not be valid in this context.
    ob = bpy.data.objects[0]
    bpy.context.scene.camera = ob
    ob.data = bpy.data.images.new(name="Foo", width=1, height=1)


def ctx_object_paint_weight():
    bpy.ops.wm.read_factory_settings(use_empty=False)
    bpy.ops.object.mode_set(mode='WEIGHT_PAINT')


def ctx_object_paint_vertex():
    bpy.ops.wm.read_factory_settings(use_empty=False)
    bpy.ops.object.mode_set(mode='VERTEX_PAINT')


def ctx_object_paint_sculpt():
    bpy.ops.wm.read_factory_settings(use_empty=False)
    bpy.ops.object.mode_set(mode='SCULPT')


def ctx_object_paint_texture():
    bpy.ops.wm.read_factory_settings(use_empty=False)
    bpy.ops.object.mode_set(mode='TEXTURE_PAINT')


# END UTILS TO EXPORT.


operator_pattern_exclude = (
    "console.*",
    "preferences.extension_url_drop",
    "wm.context_*",
    "wm.path_open",
    "wm.properties_add",
    "wm.properties_remove",
    "wm.properties_edit",
    "wm.properties_context_change",
    "render.preset_add",
    "wm.operator_cheat_sheet",
)

# Harmless operators that get in the way of testing.
# Try enabling once in a while.
operator_pattern_exclude_for_performance = (
    "nla.bake",
    "object.bake_image",
    "object.paths_calculate",
    "object.paths_update",
    "object.quadriflow_remesh",
    "object.quick_fur",  # Actually quite slow.
    "ptcache.bake_all",
    "sound.bake_animation",
    "sound.mixdown",
    "wm.previews_ensure",

)

# These operators may change Blender's run-time state,
# don't use operators that would change Blender's preferences while it's running.
operator_pattern_exclude_for_valid_state = (
    "ed.undo",
    "ed.undo_push",
    "preferences.asset_library_remove",
    "preferences.keyitem_add",
    "preferences.studiolight_new",
    "scene.new",
    "screen.delete",
    "script.reload",
    "wm.keyconfig_preset_add",
    "wm.quit_blender",
    "wm.recover_auto_save",
    "wm.window_close",
)


# These operators attempt IO which may cause problems.
# Don't enable these as they could modify the installation.
operator_pattern_exclude_for_io = (
    "*.open_*",
    "*.read_*",
    "*.save_*",
    "anim.keying_set_export",
    "export*.*",
    "extensions.*",  # Don't manipulate installed extensions.
    "import*.*",
    "outliner.id_paste",
    "preferences.addon_*",
    "preferences.associate_blend",
    "preferences.copy_prev",
    "preferences.keyconfig_export",
    "preferences.studiolight_install",
    "preferences.theme_install",
    "preferences.unassociate_blend",
    "view3d.pastebuffer",
)

# In rare cases, operators have noisy output, flooding the STDOUT.
# Enabling is harmless, but leave disabled for usable output.
operator_pattern_exclude_for_silence = (
    "preferences.keyconfig_test",
    "wm.memory_statistics",
)

# These operators are disruptive.
operator_pattern_exclude_for_disruptive = (
    "image.external_edit",
    "image.project_edit",
    "render.play_rendered_anim",
    "render.render",
    "wm.url_open",
    "wm.doc_view",
    "wm.doc_view_manual",
    "wm.url_open_preset",
)


# Some operators crash or have problems (in background mode or not).
operator_pattern_exclude_for_bugs = (
    "brush.asset_save_as",  # Could report an error instead of asserting.
)

# Technically a regression as the regions type is no longer initialized in background mode, could be resolved.
operator_pattern_exclude_for_bugs_region_type_null_in_bg_mode = (
    "action.view_frame",
    "view2d.pan",
    "view2d.reset",
    "view2d.scroll_down",
    "view2d.scroll_left",
    "view2d.scroll_right",
    "view2d.scroll_up",
    "view2d.zoom",
    "view2d.zoom_border",
    "view2d.zoom_in",
    "view2d.zoom_out",
)

# If the undo stack is initialized in background mode, these could be enabled.
operator_pattern_exclude_for_bugs_needs_undo_stack = (
    "object.voxel_remesh",
    "mesh.paint_mask_slice",
    "paint.mask_flood_fill",
    "paint.vertex_color_brightness_contrast",
    "paint.vertex_color_hsv",
    "paint.vertex_color_invert",
    "paint.vertex_color_levels",
    "paint.vertex_color_set",
    "sculpt.color_filter",
    "sculpt.face_set_change_visibility",
    "sculpt.face_sets_create",
    "sculpt.face_sets_init",
    "sculpt.mask_filter",
    "sculpt.mask_from_boundary",
    "sculpt.mask_from_cavity",
    "sculpt.mask_init",
    "sculpt.mesh_filter",
    "sculpt.symmetrize",
)


# Crash in background.
operator_pattern_exclude_for_bugs_without_gui = (
    "buttons.clear_filter",  # Null `space->runtime` in background mode.
    "buttons.toggle_pin",  # Technically a bug but doesn't make sense in background mode.
    "gpencil.layer_annotation_remove",  # TODO: looks like this could be fixed.
    "outliner.animdata_operation",  # TODO: looks like poll should handle this.
    "outliner.collection_new",  # `space_outliner->runtime` is null.
    "outliner.delete",  # TODO: looks like poll should handle this.
    "outliner.modifier_operation",
    "screen.area_close",  # Hangs, could be investigated.
    "uv.select",  # Assert as the region has no valid size, a bug but low priority (also for other UV picking).
    "uv.select_edge_ring",
    "uv.select_linked_pick",
    "uv.select_loop",
    "uv.stitch",  # TODO: looks like this could be fixed.
    "view3d.object_mode_pie_or_toggle",
    "view3d.ruler_*",  # Depends on the gizmo, fails checking the areas tool is valid.
    "view3d.select",  # The region has no: RegionView3D.
    "view3d.view_orbit",  # The region has no: RegionView3D.
    "wm.toolbar",  # Technically a bug but doesn't make sense in background mode.

    *operator_pattern_exclude_for_bugs_needs_undo_stack,
    *operator_pattern_exclude_for_bugs_region_type_null_in_bg_mode,
)

operator_pattern_exclude_for_bugs_with_gui = (
)

operator_pattern_exclude_all = (
    *operator_pattern_exclude,
    *operator_pattern_exclude_for_disruptive,
    *operator_pattern_exclude_for_valid_state,
    *operator_pattern_exclude_for_performance,
    *operator_pattern_exclude_for_io,
    *operator_pattern_exclude_for_silence,

    *operator_pattern_exclude_for_bugs,
    *(
        operator_pattern_exclude_for_bugs_without_gui if bpy.app.background else
        operator_pattern_exclude_for_bugs_with_gui
    ),
)
assert len(operator_pattern_exclude_all) == len(set(operator_pattern_exclude_all))
operator_pattern_exclude_all_usage = [False] * len(operator_pattern_exclude_all)


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
        return (ext == ".blend")

    return list(sorted(file_list(mainpath, is_blend)))


def filter_op_list(operators):
    from fnmatch import fnmatchcase

    def is_op_ok(op):
        for i, op_match in enumerate(operator_pattern_exclude_all):
            if fnmatchcase(op, op_match):
                operator_pattern_exclude_all_usage[i] = True
                print("    skipping: {:s} ({:s})".format(op, op_match))
                return False
        return True

    operators[:] = [op for op in operators if is_op_ok(op[0])]


if USE_ATTRSET:
    def build_property_typemap(skip_classes):

        property_typemap = {}

        for attr in dir(bpy.types):
            cls = getattr(bpy.types, attr)
            if issubclass(cls, skip_classes):
                continue

            # # to support skip-save we can't get all props
            # properties = cls.bl_rna.properties.keys()
            properties = []
            for prop_id, prop in cls.bl_rna.properties.items():
                if not prop.is_skip_save:
                    properties.append(prop_id)

            properties.remove("rna_type")
            property_typemap[attr] = properties

        return property_typemap
    CLS_EXCLUDE = (
        bpy.types.BrushTextureSlot,
        bpy.types.Brush,
    )
    property_typemap = build_property_typemap(CLS_EXCLUDE)
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
                    for val, prop, _tp in id_walk(id_data, bpy.data):
                        # print(id_data)
                        for val_rnd in _random_values:
                            try:
                                setattr(val, prop, val_rnd)
                            except:
                                pass


def run_ops(
        operators,  # `list[str]`
        *,
        log_fn,  # `BytesIO | None`
        setup_fn,  # `Callable[[None], None]`
        use_random,  # `bool`
        random_reset,  # `float` (between 0 and 1).
        random_screen,  # `bool`
        blend_files,  # `list[str] | None`
):
    from bpy import context

    print("\nContext:", setup_fn.__name__)

    if log_fn is not None:
        # Only when operators run else this just produces empty headings.
        if operators:
            log_fn("# Context: {:s}\n".format(setup_fn.__name__))

    # This is more of a run-time check, ignore `log_fn`.
    if not operators:
        reset_blend()

        with context.temp_override(**temp_override_default_kwargs(context, area_type=None, region_type=None)):
            # Empty operators is a signal the setup functions are being tested.
            if use_random and operators:
                # we can't be sure it will work
                try:
                    setup_fn()
                except:
                    pass
            else:
                setup_fn()
        return

    for filepath in ((None, ) if blend_files is None else blend_files):
        is_first = True
        random_int = 0

        for op_id, op in operators:

            reset_test = True
            if use_random:
                if random.random() < (1.0 - random_reset):
                    reset_test = False

            # Always reset on the first iteration.
            if is_first:
                is_first = False
                reset_test = True

            if reset_test:
                if use_random:
                    random_int = random.randint(0, 0xffffffff)

                if filepath is not None:
                    if log_fn is not None:
                        log_fn("reset_file({!r})\n".format(filepath))
                    reset_file(filepath)
                else:
                    if random_screen:
                        random_screen_int = random.randint(0, 0xffffffff)
                    else:
                        random_screen_int = -1

                    if setup_fn is ctx_nop:
                        # When setting up the context does nothing, simply reload the blend.
                        if log_fn is not None:
                            log_fn("reset_blend({:d})\n".format(random_screen_int))
                        reset_blend(random_screen_int)
                    else:
                        # The setup function will reset the blend files state.
                        if log_fn is not None:
                            log_fn("{:s}()\n".format(setup_fn.__name__))
                        setup_fn()

            window = context.window_manager.windows[0]
            screen = window.screen

            # Get the area & region, when random is used they may randomly be None.
            areas = list(screen.areas)
            if use_random:
                areas.append(None)
            area = areas[random_int % len(areas)]
            del areas

            if area is not None:
                regions = list(area.regions)
                if use_random:
                    regions.append(None)
                region = regions[random_int % len(regions)]
                del regions
            else:
                region = None

            area_type = area.type if area else None
            region_type = region.type if (area and region) else None
            del reset_test
            del area, region

            with context.temp_override(**temp_override_default_kwargs(
                    context,
                    area_type=area_type,
                    region_type=region_type,
            )):
                if not op.poll():
                    continue

            if log_fn is not None:
                log_fn("run_op(context, bpy.ops.{:s}, {!r}, {!r})\n".format(
                    op_id,
                    area_type,
                    region_type,
                ))

            run_op(context, op, area_type, region_type)


def bpy_check_type_duplicates():
    # non essential sanity check
    bl_types = dir(bpy.types)
    bl_types_unique = set(bl_types)

    if len(bl_types) != len(bl_types_unique):
        print("Error, found duplicates in 'bpy.types'")
        for t in sorted(bl_types_unique):
            tot = bl_types.count(t)
            if tot > 1:
                print("    '{:s}', {:d}".format(t, tot))
        import sys
        sys.exit(1)


def extract_region_from_text_by_delimiters(text, mark_beg, mark_end):
    beg = text.find(mark_beg)
    end = text.find(mark_end)
    assert beg != -1 and end != -1
    return text[beg + len(mark_beg):end]


def run_all(
        log_fn,  # `Callable[[str], None] | None`
        *,
        use_random,  # `bool`
        random_reset,  # `float`
        random_multiply,  # `int`
        random_screen,  # `bool`
        blend_files,  # `list[str] | None`
):
    if log_fn is not None:
        log_fn(
            "import bpy\n"
            "import sys\n"
            "from bpy import context\n"
            "\n"
        )

        # Extract utility functions form this file.
        with open(__file__, "r", encoding="utf-8") as fh:
            log_fn("# Utility functions.")
            log_fn(extract_region_from_text_by_delimiters(
                fh.read(),
                "# BEGIN UTILS TO EXPORT.",
                "# END UTILS TO EXPORT.",
            ))
            log_fn("\n")

    # TODO: investigate having an undo stack in background mode.
    undo_stack_ensure = False
    if undo_stack_ensure:
        import bpy
        if bpy.app.background:
            bpy.ops.ed.undo_push()
            if log_fn is not None:
                log_fn("bpy.ops.ed.undo_push()\n")

    bpy_check_type_duplicates()

    # reset_blend()
    import bpy
    operators = []
    for mod_name in dir(bpy.ops):
        mod = getattr(bpy.ops, mod_name)
        for submod_name in dir(mod):
            op = getattr(mod, submod_name)
            operators.append(("{:s}.{:s}".format(mod_name, submod_name), op))

    operators.sort(key=lambda op: op[0])

    filter_op_list(operators)

    for op_match, op_match_used in zip(operator_pattern_exclude_all, operator_pattern_exclude_all_usage):
        if not op_match_used:
            print("WARNING, exclude pattern not used:", op_match)

    if blend_files:
        setup_fn_list = [
            ctx_nop,
        ]
    else:
        setup_fn_list = [
            ctx_clear_scene,
            # Object modes.
            ctx_object_empty,
            ctx_object_empty_as_camera,
            ctx_object_paint_sculpt,
            ctx_object_paint_texture,
            ctx_object_paint_vertex,
            ctx_object_paint_weight,
            ctx_object_pose,
            ctx_object_volume,

            # Mesh.
            ctx_editmode_mesh,
            ctx_editmode_mesh_extra,
            ctx_editmode_mesh_empty,
            # Armature.
            ctx_editmode_armature,
            ctx_editmode_armature_empty,
            # Curves.
            ctx_editmode_curves,
            ctx_editmode_curves_empty,
            ctx_editmode_surface,
            # Hair.
            ctx_editmode_hair,
            ctx_editmode_hair_empty,

            # Grease pencil.
            ctx_editmode_grease_pencil,
            ctx_editmode_grease_pencil_empty,

            # Other.
            ctx_editmode_mball,
            ctx_editmode_text,
            ctx_editmode_lattice,
        ]

    if use_random:
        operators = operators * max(1, random_multiply)
        random.shuffle(operators)

    # First just run `setup_fn` to make sure they work.
    for setup_fn in setup_fn_list:
        run_ops(
            (),
            log_fn=log_fn,
            setup_fn=setup_fn,
            use_random=False,
            random_reset=False,
            random_screen=False,
            blend_files=None,
        )
    print("All setup functions run fine!")

    for setup_fn in setup_fn_list:
        run_ops(
            operators,
            log_fn=log_fn,
            setup_fn=setup_fn,
            use_random=use_random,
            random_reset=random_reset,
            random_screen=random_screen,
            blend_files=blend_files,
        )

    print("Finished {!r}".format(__file__))


def parse_create():
    import argparse
    import os

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "--generate-script",
        dest="generate_script",
        metavar='FILEPATH',
        default="",
        type=str,
        help=(
            "When set, write a Python script to this destination.\n"
            "This can be used to replay events that crash more conveniently.\n"
            "\n"
            "To reply the script fun Blender, appending the arguments: --python FILEPATH"
        ),
    )

    parser.add_argument(
        "--random",
        dest="random",
        default=False,
        action='store_true',
        help="When set, randomize the operator call order and when the blend-file resets.",
    )

    parser.add_argument(
        "--random-screen",
        dest="random_screen",
        default=False,
        action='store_true',
        help="When set, randomize the screen used when resetting the blend-file.",
    )

    parser.add_argument(
        "--random-seed",
        dest="random_seed",
        metavar='INT',
        default=0,
        type=int,
        help="The seed to use for randomization.\n",
    )
    parser.add_argument(
        "--random-multiply",
        dest="random_multiply",
        metavar='INT',
        default=1,
        type=int,
        help="Expand the number of times operators are called.\n",
    )

    parser.add_argument(
        "--random-reset",
        dest="random_reset",
        metavar='UNIT',
        default=0.1,
        type=float,
        help=(
            "The probability of resetting the blend-file between calling each operator.\n"
            "A value of 0.1 means there is a 10%% chance that the file will be reset.\n"
            "A value of 0.9 means there is a 90%% chance.\n"
            "\n"
            "Very low values such as 0.01 (a 1%% chance), means operators could perform actions\n"
            "that exhaust system resources by randomly generating heavy scenes, use with care!"
        ),
    )

    parser.add_argument(
        "--blend-files",
        dest="blend_files",
        metavar='DIRECTORY',
        default="",
        type=str,
        help=(
            "Instead of using empty blend file, run operators.\n"
            "\n"
            "Multiple paths may be passed in separated by \"{:s}\"\n"
            "- Files ending with \".blend\" will be loaded.\n"
            "- Other paths will be recursively scanned for \".blend\" files.\n"
            "\n"
            "Note that all operators will run on each blend file."
        ).format(os.pathsep),
    )

    return parser


def main():
    import os
    import contextlib

    args = parse_create().parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])

    blend_files = None
    if args.blend_files:
        blend_files = []
        for path in args.blend_files.split(os.pathsep):
            if path.endswith(".blend"):
                blend_files.append(path)
            else:
                blend_files.extend(blend_list(args.blend_files))
        if not blend_files:
            print("No blend files found in: {:s}".format(args.blend_files))
            return 1

    if args.random:
        random.seed(args.random_seed)

    # Disable buffering so the script is complete if Blender crashes.
    with (
            open(args.generate_script, "wb", buffering=0) if args.generate_script else
            contextlib.nullcontext()

    ) as fh:
        run_all(
            log_fn=(
                None if fh is None else
                (lambda text: fh.write(text.encode("utf-8")))
            ),
            use_random=args.random,
            random_reset=args.random_reset,
            random_multiply=args.random_multiply,
            random_screen=args.random_screen,
            blend_files=blend_files,
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
