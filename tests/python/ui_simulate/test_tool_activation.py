# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""

import modules.ui_test_utils as ui


def _setup_editor_area(area_type):
    import bpy

    bpy.ops.wm.read_homefile(use_empty=True)
    e, t, window = ui.test_window()
    area = ui.largest_area(window.screen)
    area.type = area_type
    yield

    space = area.spaces.active
    if hasattr(space, "show_region_toolbar"):
        space.show_region_toolbar = True
    yield

    return e, t, window, area


def _setup_view3d(tool_mode):
    import bpy

    e, t, window, area = yield from _setup_editor_area("VIEW_3D")
    bpy.ops.mesh.primitive_cube_add()

    if tool_mode == "OBJECT":
        pass

    elif tool_mode == "EDIT_MESH":
        bpy.ops.object.mode_set(mode="EDIT")

    elif tool_mode == "SCULPT":
        bpy.ops.object.mode_set(mode="SCULPT")

    else:
        raise RuntimeError(f"Unsupported mode {tool_mode}")

    return e, t, window, area


def _setup_image_editor(mode):
    import bpy

    e, t, window, area = yield from _setup_editor_area("IMAGE_EDITOR")
    space = area.spaces.active

    bpy.ops.mesh.primitive_cube_add()
    space.image = bpy.data.images.new("TestImage", width=256, height=256)
    space.mode = mode

    if mode == "PAINT":
        bpy.ops.object.mode_set(mode="TEXTURE_PAINT")
    elif mode == "UV":
        bpy.ops.object.mode_set(mode="EDIT")
    elif mode == "MASK":
        space.mask = bpy.data.masks.new("TestMask")

    return e, t, window, area


def _setup_sequencer(view_type):
    import bpy

    e, t, window, area = yield from _setup_editor_area("SEQUENCE_EDITOR")

    scene = bpy.context.scene
    scene.sequence_editor_create()

    scene.sequence_editor.strips.new_effect(
        name="Color",
        type='COLOR',
        channel=1,
        frame_start=1,
        length=50,
    )
    yield

    area.spaces.active.view_type = view_type
    yield

    return e, t, window, area


def _get_tool_items(space_type, mode=None):
    import bpy
    from bl_ui.space_toolsystem_common import ToolSelectPanelHelper

    tool_class = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    if tool_class is None:
        return []

    items = ToolSelectPanelHelper._tools_flatten(
        tool_class.tools_from_context(bpy.context, mode)
    )

    return [
        item
        for item in items
        if item is not None and getattr(item, "idname", None)
    ]


def _get_active_tool_id(workspace, area):
    import bpy

    space = area.spaces.active

    if area.type == "VIEW_3D":
        return workspace.tools.from_space_view3d_mode(
            bpy.context.mode,
            create=True,
        ).idname

    if area.type == "IMAGE_EDITOR":
        return workspace.tools.from_space_image_mode(
            space.mode,
            create=True,
        ).idname

    if area.type == "NODE_EDITOR":
        return workspace.tools.from_space_node(create=True).idname

    if area.type == "SEQUENCE_EDITOR":
        return workspace.tools.from_space_sequencer(
            space.view_type,
            create=True,
        ).idname

    raise RuntimeError(f"Unsupported space type {area.type!r}")


def _activate_tools_for_context(space_type, *, mode=None, setup_fn=None):
    import bpy

    if setup_fn is None:
        def setup_fn(): return _setup_editor_area(space_type)

    e, t, window, area = yield from setup_fn()

    if space_type == "SEQUENCE_EDITOR":
        if mode is None:
            mode = area.spaces.active.view_type
        region = next(r for r in area.regions if r.type == "WINDOW")
    elif space_type == "IMAGE_EDITOR":
        if mode is None:
            mode = area.spaces.active.mode
        region = next(r for r in area.regions if r.type == "WINDOW")
    else:
        region = None

    center = ui.get_area_center(area)
    for tool in _get_tool_items(space_type, mode):
        yield e.cursor_position_set(*center, move=True)
        yield

        if region is None:
            bpy.ops.wm.tool_set_by_id(
                name=tool.idname,
                space_type=space_type,
            )
        else:
            # Some editors (e.g. the Image Editor and Sequencer) require an
            # active window/area/region override for `wm.tool_set_by_id`.
            with bpy.context.temp_override(
                window=window,
                area=area,
                region=region,
            ):
                bpy.ops.wm.tool_set_by_id(
                    name=tool.idname,
                    space_type=space_type,
                )

        area.tag_redraw()
        yield

        active_tool_id = _get_active_tool_id(window.workspace, area)
        t.assertEqual(active_tool_id, tool.idname)


def view3d():
    for mode in ("OBJECT", "EDIT_MESH", "SCULPT"):
        yield from _activate_tools_for_context(
            "VIEW_3D",
            mode=mode,
            setup_fn=lambda mode=mode: _setup_view3d(mode),
        )


def image_editor():
    for mode in ("VIEW", "PAINT", "UV", "MASK"):
        yield from _activate_tools_for_context(
            "IMAGE_EDITOR",
            mode=mode,
            setup_fn=lambda mode=mode: _setup_image_editor(mode),
        )


def node_editor():
    yield from _activate_tools_for_context("NODE_EDITOR")


def sequencer():
    for view_type in (
        "SEQUENCER",
        "PREVIEW",
        "SEQUENCER_PREVIEW",
    ):
        yield from _activate_tools_for_context(
            "SEQUENCE_EDITOR",
            setup_fn=lambda view_type=view_type: _setup_sequencer(view_type),
        )
