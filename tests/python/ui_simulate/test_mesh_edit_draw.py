# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui


def _view3d_context_override(window):
    import bpy
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    region = next(region for region in area.regions if region.type == 'WINDOW')
    return bpy.context.temp_override(window=window, area=area, region=region)


def view3d_mesh_edit_draw_modifiers():
    """
    Enter edit mode and draw the mesh with different modifier stacks.

    Only some of these keep the evaluated mesh a wrapper around the edit-mode BMesh,
    the others give an evaluated mesh that holds its own data.
    The viewport's edit-mode drawing must handle both.
    """
    import bmesh
    import bpy
    e, t, window = ui.test_window()

    yield e.shift.f5()                  # 3D Viewport.
    yield e.ctrl.alt.space()            # Full-screen.
    yield e.a()                         # Select all.
    yield e.delete().ret()              # Delete all.

    with _view3d_context_override(window):
        bpy.ops.mesh.primitive_cube_add()
    ob = window.view_layer.objects.active
    # Edit-mode drawing only looks up the active UV face when there is a UV map.
    t.assertTrue(ob.data.uv_layers)
    yield

    # Each entry is a modifier stack, as (type, show_on_cage) pairs.
    modifier_stacks = (
        (),
        (('SIMPLE_DEFORM', False),),
        (('MIRROR', False),),
        (('MIRROR', True),),
        (('SIMPLE_DEFORM', True), ('ARRAY', False)),
    )
    for modifier_stack in modifier_stacks:
        ob.modifiers.clear()
        for modifier_type, show_on_cage in modifier_stack:
            modifier = ob.modifiers.new(name=modifier_type, type=modifier_type)
            modifier.show_in_editmode = True
            modifier.show_on_cage = show_on_cage

        yield e.tab()                   # Edit mode.
        yield
        t.assertEqual(bpy.context.mode, 'EDIT_MESH', modifier_stack)

        # Draw again with an active face, which is looked up for the UV editing overlay.
        bm = bmesh.from_edit_mesh(ob.data)
        bm.faces.ensure_lookup_table()
        bm.faces[0].select = True
        bm.faces.active = bm.faces[0]
        bmesh.update_edit_mesh(ob.data)
        yield
        yield

        yield e.tab()                   # Object mode.
        t.assertEqual(bpy.context.mode, 'OBJECT', modifier_stack)
