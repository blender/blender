# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Example usage:
#
# ./blender.bin --factory-startup \
#               --enable-event-simulate \
#               --python tools/utils_maintenance/blender_menu_search_coverage.py

import bpy

# Menu-ID -> class.
MENU_TYPE_MAP = {}

# Track which menus were called per-context,
# this ensures that menus were called at all.
MENU_CALLED_RUNTIME = set()

# Use for adding mouse movement (refreshing).
EVENT_ARGS_NOP = dict(type='MOUSEMOVE', value='NOTHING')

# List of shell-glob expressions to ignore.
OPERATOR_IGNORE = (
    "action.clickselect",
    "action.select_lasso",
    "armature.select_linked_pick",
    "armature.shortest_path_pick",
    "buttons.context_menu",
    "buttons.directory_browse",
    "buttons.file_browse",
    "clip.cursor_set",  # Interactive cursor placement.
    "clip.select_lasso",
    "console.*",
    "curve.draw",
    "curve.select_linked_pick",
    "ed.undo_redo",  # The UI exposes undo/redo operators.
    "file.bookmark_add",
    "file.bookmark_cleanup",
    "file.bookmark_delete",
    "file.bookmark_move",
    "file.cancel",
    "file.delete",
    "file.directory_new",
    "file.execute",
    "file.filenum",
    "file.filepath_drop",
    "file.hidedot",
    "file.highlight",
    "file.next",
    "file.parent",
    "file.previous",
    "file.refresh",
    "file.rename",
    "file.reset_recent",
    "file.select",
    "file.select_bookmark",
    "file.select_walk",
    "file.smoothscroll",
    "font.line_break",
    "font.move",
    "gizmogroup.gizmo_select",
    "gizmogroup.gizmo_tweak",
    "gpencil.draw",  # Interactive drawing.
    "gpencil.select_lasso",
    "gpencil.vertex_paint",
    "gpencil.weight_paint",
    "graph.click_insert",
    "graph.clickselect",
    "graph.cursor_set",  # Interactive cursor placement.
    "graph.select_lasso",
    "mask.select_lasso",
    "mask.select_linked_pick",
    "mesh.polybuild_*",  # Only accessed via tool.
    "mesh.primitive_cube_add_gizmo",  # Only accessed via tool.
    "mesh.rip",
    "mesh.rip_edge",
    "mesh.select_linked_pick",
    "mesh.shortest_path_pick",  # Uses mouse.
    "node.select_lasso",
    "object.add_named",
    "object.material_slot_move",
    "object.mode_set",
    "object.mode_set_with_submode",
    "object.posemode_toggle",
    "paint.face_select_linked_pick",
    "paint.weight_sample",
    "paint.weight_sample_group",
    "paintcurve.draw",  # Interactive drawing.
    "particle.select_linked_pick",
    "pose.select_linked_pick",
    "preferences.addon_disable",
    "preferences.addon_enable",
    "preferences.addon_install",
    "preferences.addon_refresh",
    "preferences.addon_remove",
    "preferences.copy_prev",
    "preferences.keyconfig_activate",
    "preferences.keyconfig_export",
    "preferences.keyconfig_import",
    "preferences.keyconfig_remove",
    "preferences.keyconfig_test",
    "preferences.keyitem_add",
    "preferences.keyitem_remove",
    "preferences.keyitem_restore",
    "preferences.keymap_restore",
    "preferences.reset_default_theme",
    "preferences.studiolight_copy_settings",
    "preferences.studiolight_install",
    "preferences.studiolight_new",
    "preferences.studiolight_uninstall",
    "preferences.theme_install",
    "scene.delete",
    "scene.light_cache_bake",
    "scene.light_cache_free",
    "scene.new",
    "scene.render_view_add",
    "scene.render_view_remove",
    "screen.animation_cancel",
    "screen.animation_step",
    "screen.area_swap",
    "screen.back_to_previous",
    "screen.delete",
    "screen.drivers_editor_show",
    "screen.frame_jump",
    "screen.frame_offset",
    "screen.header_toggle_menus",
    "screen.new",
    "screen.region_context_menu",
    "screen.region_flip",
    "screen.region_toggle",
    "screen.screen_set",
    "screen.space_context_cycle",
    "screen.space_type_set_or_cycle",  # Only makes sense from key binding.
    "script.execute_preset",
    "sequencer.select_linked_pick",
    "text.cursor_set",  # Interactive cursor placement.
    "text.find",  # text.start_find
    "text.indent_or_autocomplete",
    "text.insert",
    "text.line_break",
    "text.replace",
    "text.replace_set_selected",
    "text.resolve_conflict",
    "text.selection_set",
    "ui.*",
    "uv.rip",
    "uv.rip_move",
    "uv.select",
    "uv.select_edge_ring",
    "uv.select_lasso",
    "uv.select_linked_pick",
    "uv.select_loop",
    "uv.shortest_path_pick",
    "view2d.scroll_down",
    "view2d.scroll_left",
    "view2d.scroll_right",
    "view2d.scroll_up",
    "view2d.scroller_activate",
    "view3d.cursor3d",
    "view3d.select",
    "view3d.select_lasso",
    "view3d.select_menu",
    "view3d.view_center_pick",
    "wm.doc_view",
    "wm.doc_view_manual",
    "wm.doc_view_manual_ui_context",
    "wm.owner_disable",
    "wm.owner_enable",
    "wm.radial_control",
    "wm.search_operator",  # Only for users who prefer this behavior.
    "wm.tool_set_by_id",
    "wm.tool_set_by_index",
    "wm.toolbar",
    "wm.toolbar_fallback_pie",
    "wm.toolbar_prompt",
    "wm.window_close",
    "workspace.*",

)

# Operators found in menus.
OPERATOR_FOUND = set()

# -----------------------------------------------------------------------------
# Generate Operator List
#


def operator_list():
    """
    Filter this, allowing is to ignore some operators.
    """
    # Filter the list.
    from fnmatch import fnmatchcase

    def is_op_ok(op):
        for op_match in OPERATOR_IGNORE:
            if fnmatchcase(op, op_match):
                print("    skipping: {:s} ({:s})".format(op, op_match))
                return False
        return True

    operators = []
    for mod_name in dir(bpy.ops):
        mod = getattr(bpy.ops, mod_name)
        for submod_name in dir(mod):
            op = getattr(mod, submod_name)
            bl_options = op.bl_options
            if 'INTERNAL' in bl_options:
                continue

            op_id = "{:s}.{:s}".format(mod_name, submod_name)
            if not is_op_ok(op_id):
                continue

            operators.append((op_id, op))
    operators.sort(key=lambda op_pair: op_pair[0])
    return operators


# -----------------------------------------------------------------------------
# Setup Functions

def setup_contants():
    from bpy.types import Menu
    for cls in Menu.__subclasses__():
        if cls.is_registered:
            bl_idname = getattr(cls, "bl_idname", cls.__name__)
            MENU_TYPE_MAP[bl_idname] = cls


def setup_menu_wrap_draw_call_all():

    def operators_from_layout_introspect(layout_introspect):
        assert isinstance(layout_introspect, list)
        for item in layout_introspect:
            value = item.get("items")
            if value is not None:
                assert isinstance(value, list)
                yield from operators_from_layout_introspect(value)
            value = item.get("operator")
            if value is not None:
                assert isinstance(value, str)
                # We don't need the arguments at the moment.
                assert value.startswith("bpy.ops.")
                yield value[8:].split("(")[0]

    def menu_draw_introspect(self, _context):
        bl_idname = getattr(cls, "bl_idname", type(self).__name__)
        layout_introspect = self.layout.introspect()
        for op_id in operators_from_layout_introspect(layout_introspect):
            OPERATOR_FOUND.add(op_id)
        MENU_CALLED_RUNTIME.add(bl_idname)

    # Instead of monkey patching the draw function, use the built-in `Menu.append` method,
    # which allows us to access the layout which has been created so far.
    #
    # This works as long as this is the last append call to the menu
    # (add-ons will have already loaded).
    for cls in MENU_TYPE_MAP.values():
        cls.append(menu_draw_introspect)


# -----------------------------------------------------------------------------
# Simulate Events

def run_event_simulate(event_iter):
    TICKS = 1

    def event_step():
        # print("timer:", event_step._ticks)
        # Run once 'TICKS' is reached.
        if event_step._ticks < TICKS:
            event_step._ticks += 1
            return 0.0

        event_step._ticks = 0

        val = next(event_step.run_events, Ellipsis)
        if val is Ellipsis:
            bpy.app.use_event_simulate = False
            print("Finished simulation")
            return None

        # Run event simulation.
        win = bpy.context.window_manager.windows[0]
        if "x" not in val:
            val["x"] = win.width // 2
        if "y" not in val:
            val["y"] = win.height // 2

        # Fake event value, since press, release is so common.
        if val.get("value") == 'TAP':
            del val["value"]
            win = bpy.context.window_manager.windows[0]
            win.event_simulate(**val, value='PRESS')
            win = bpy.context.window_manager.windows[0]
            win.event_simulate(**val, value='RELEASE')
        else:
            win = bpy.context.window_manager.windows[0]
            win.event_simulate(**val)
        return 0.0

    event_step.run_events = iter(event_iter)
    event_step._ticks = 0

    bpy.app.timers.register(event_step, first_interval=1.0, persistent=True)


def setup_default_preferences(preferences):
    """ Set preferences useful for automation.
    """
    preferences.view.show_splash = False
    preferences.view.smooth_view = 0
    preferences.view.use_save_prompt = False
    preferences.view.show_developer_ui = True
    preferences.filepaths.use_auto_save_temporary_files = False


# -----------------------------------------------------------------------------
# Context Setup

# -------
# Default

def ctx_objectmode_default():
    pass

# ----
# Text


def ctx_text_default():
    found = False
    text = bpy.data.texts.new(name="Text")
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'TEXT_EDITOR':
                area.spaces.active.text = text
                found = True
    assert found


# -----
# Image

def ctx_image_view_default():
    found = False
    image = bpy.data.images.new(name="Image", width=1, height=1)
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'IMAGE_EDITOR':
                space_data = area.spaces.active
                space_data.image = image
                found = True
    assert found


def ctx_image_view_render():
    found = False
    image = bpy.data.images["Render Result"]
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'IMAGE_EDITOR':
                space_data = area.spaces.active
                space_data.image = image
                found = True
    assert found


def ctx_image_mask_default():
    found = False
    mask = bpy.data.masks.new(name="Mask")
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'IMAGE_EDITOR':
                space_data = area.spaces.active
                space_data.mode = 'MASK'
                space_data.mask = mask
                found = True
    assert found


def ctx_image_paint_default():
    found = False
    image = bpy.data.images.new(name="Image", width=1, height=1)
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'IMAGE_EDITOR':
                space_data = area.spaces.active
                space_data.mode = 'PAINT'
                space_data.image = image
                found = True
    assert found


# ----
# Clip

def ctx_clip_default():
    found = False
    # Load '.' is a trick so we don't need to read a real image.
    clip = bpy.data.movieclips.load(filepath=".")
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'CLIP_EDITOR':
                area.spaces.active.clip = clip
                found = True
    assert found


# ----------
# Edit Modes

def ctx_editmode_mesh():
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_mesh_extra():
    bpy.ops.object.vertex_group_add()
    bpy.ops.object.shape_key_add(from_mix=False)
    bpy.ops.object.shape_key_add(from_mix=True)
    bpy.ops.mesh.uv_texture_add()
    bpy.ops.mesh.vertex_color_add()
    bpy.ops.object.material_slot_add()
    # Edit-mode last!
    bpy.ops.object.mode_set(mode='EDIT')


def ctx_editmode_curve():
    bpy.ops.curve.primitive_nurbs_circle_add()
    bpy.ops.object.mode_set(mode='EDIT')


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


# -------------
# Grease Pencil


def ctx_gpencil_edit():
    bpy.ops.object.gpencil_add(type='STROKE')
    bpy.ops.object.mode_set(mode='EDIT_GPENCIL')


def ctx_gpencil_sculpt():
    bpy.ops.object.gpencil_add(type='STROKE')
    bpy.ops.object.mode_set(mode='SCULPT_GPENCIL')


def ctx_gpencil_paint_weight():
    bpy.ops.object.gpencil_add(type='STROKE')
    bpy.ops.object.mode_set(mode='WEIGHT_GPENCIL')


def ctx_gpencil_paint_vertex():
    bpy.ops.object.gpencil_add(type='STROKE')
    bpy.ops.object.mode_set(mode='VERTEX_GPENCIL')


def ctx_gpencil_paint_draw():
    bpy.ops.object.gpencil_add(type='STROKE')
    bpy.ops.object.mode_set(mode='PAINT_GPENCIL')


# ------------
# Object Modes

def ctx_object_pose():
    bpy.ops.object.armature_add()
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')


def ctx_object_particle_edit():
    bpy.ops.object.quick_fur()
    bpy.ops.object.mode_set(mode='PARTICLE_EDIT')


def ctx_object_volume():
    bpy.ops.object.add(type='VOLUME')


# -----------
# Paint Modes

def ctx_object_paint_weight():
    bpy.ops.object.mode_set(mode='WEIGHT_PAINT')


def ctx_object_paint_weight_with_vert_mask():
    bpy.ops.object.mode_set(mode='WEIGHT_PAINT')
    for mesh in bpy.data.meshes:
        mesh.use_paint_mask_vertex = True


def ctx_object_paint_vertex():
    bpy.ops.object.mode_set(mode='VERTEX_PAINT')


def ctx_object_paint_sculpt():
    bpy.ops.object.mode_set(mode='SCULPT')


def ctx_object_paint_texture():
    bpy.ops.object.mode_set(mode='TEXTURE_PAINT')


def ctx_object_paint_texture_with_face_mask():
    bpy.ops.object.mode_set(mode='TEXTURE_PAINT')
    for mesh in bpy.data.meshes:
        mesh.use_paint_mask = True


def perform_coverage_test():
    def open_and_close_menu_search():
        MENU_CALLED_RUNTIME.clear()
        yield dict(type='F3', value='TAP')
        yield dict(type='ESC', value='TAP')
        assert len(MENU_CALLED_RUNTIME) != 0

    operators_pairs_all = operator_list()

    for ctx_fn, space_ui_type in (
            (ctx_objectmode_default, 'VIEW_3D'),

            (ctx_object_particle_edit, 'VIEW_3D'),
            (ctx_object_pose, 'VIEW_3D'),
            (ctx_object_volume, 'VIEW_3D'),

            # Edit modes.
            (ctx_editmode_armature, 'VIEW_3D'),
            (ctx_editmode_curve, 'VIEW_3D'),
            (ctx_editmode_lattice, 'VIEW_3D'),
            (ctx_editmode_mball, 'VIEW_3D'),
            (ctx_editmode_mesh, 'VIEW_3D'),
            (ctx_editmode_mesh_extra, 'VIEW_3D'),
            (ctx_editmode_surface, 'VIEW_3D'),
            (ctx_editmode_text, 'VIEW_3D'),

            # Paint modes.
            (ctx_object_paint_sculpt, 'VIEW_3D'),
            (ctx_object_paint_texture, 'VIEW_3D'),
            (ctx_object_paint_texture_with_face_mask, 'VIEW_3D'),
            (ctx_object_paint_vertex, 'VIEW_3D'),
            (ctx_object_paint_weight, 'VIEW_3D'),
            (ctx_object_paint_weight_with_vert_mask, 'VIEW_3D'),

            # Grease pencil modes.
            (ctx_gpencil_edit, 'VIEW_3D'),
            (ctx_gpencil_paint_draw, 'VIEW_3D'),
            (ctx_gpencil_paint_vertex, 'VIEW_3D'),
            (ctx_gpencil_paint_weight, 'VIEW_3D'),
            (ctx_gpencil_sculpt, 'VIEW_3D'),

            # Other spaces.
            (ctx_clip_default, 'CLIP_EDITOR'),
            (ctx_editmode_mesh, 'UV'),
            (ctx_image_mask_default, 'IMAGE_EDITOR'),
            (ctx_image_paint_default, 'IMAGE_EDITOR'),
            (ctx_image_view_default, 'IMAGE_EDITOR'),
            (ctx_image_view_render, 'IMAGE_EDITOR'),

            (ctx_text_default, 'INFO'),
            (ctx_text_default, 'TEXT_EDITOR'),

            (ctx_objectmode_default, 'CONSOLE'),
            (ctx_objectmode_default, 'CompositorNodeTree'),
            (ctx_objectmode_default, 'DOPESHEET'),
            (ctx_objectmode_default, 'DRIVERS'),
            (ctx_objectmode_default, 'FCURVES'),
            (ctx_objectmode_default, 'FILE_BROWSER'),
            (ctx_objectmode_default, 'INFO'),
            (ctx_objectmode_default, 'NLA_EDITOR'),
            (ctx_objectmode_default, 'OUTLINER'),
            (ctx_objectmode_default, 'PREFERENCES'),
            (ctx_objectmode_default, 'PROPERTIES'),
            (ctx_objectmode_default, 'SEQUENCE_EDITOR'),
            (ctx_objectmode_default, 'ShaderNodeTree'),
            (ctx_objectmode_default, 'TIMELINE'),
            (ctx_objectmode_default, 'TextureNodeTree'),
    ):
        bpy.ops.wm.read_homefile(use_empty=False, use_factory_startup=True)
        # Set view full-screen.
        yield dict(type='SPACE', value='TAP', ctrl=True)
        if space_ui_type != 'VIEW_3D':
            win = bpy.context.window_manager.windows[0]
            win.screen.areas[0].ui_type = space_ui_type
            # import IPython; IPython.embed()
            yield EVENT_ARGS_NOP

        ctx_fn()
        yield from open_and_close_menu_search()

    operators_all = {op_pair[0] for op_pair in operators_pairs_all}

    # The menu might use some internal operators, that's fine but
    # could give confusing percentages.
    operators_menu = (operators_all & OPERATOR_FOUND)

    len_op = len(operators_all)
    len_op_menu = len(operators_menu)

    for op in sorted(operators_all - operators_menu):
        print(op)

    # Report:
    print(
        "Coverage {:.2f} ({:d} of {:d})".format(
            (len_op_menu / len_op) * 100.0,
            len_op_menu,
            len_op,
        ))

    # Quit!
    yield Ellipsis


def main():
    setup_default_preferences(bpy.context.preferences)
    setup_contants()
    setup_menu_wrap_draw_call_all()

    run_event_simulate(perform_coverage_test())


if __name__ == "__main__":
    main()
