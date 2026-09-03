# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything; its methods are accessed by ``run_blender_setup.py``.

Tests for Blender's drop handler system — the operators invoked when data is
dropped onto UI elements or editor regions.

Blender's ``EVT_DROP`` events cannot be produced by ``event_simulate`` (they
are generated internally by the window manager when ``wm->runtime->drags`` is
non-empty).  These tests therefore exercise the drop operators directly via
their ``exec`` or ``invoke`` paths where possible, and verify registration
where the invoke path requires cursor-position context (``event->mval``).
"""

import modules.ui_test_utils as ui


def _window_area(window, area_type):
    area = ui.largest_area(window.screen)
    area.type = area_type
    return area


# ---------------------------------------------------------------------------
#
#    VIEW3D_OT_drop_world has an exec path that does not depend on
#    event->mval.  Drop a world into the scene and verify the scene's world
#    pointer is updated.
# ---------------------------------------------------------------------------

def view3d_drop_world():
    import bpy
    _e, t, window = ui.test_window()
    area = _window_area(window, "VIEW_3D")
    region = next(r for r in area.regions if r.type == 'WINDOW')
    yield

    original_world = bpy.context.scene.world

    world = bpy.data.worlds.new("DropTestWorld")

    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.view3d.drop_world(name=world.name)

    t.assertEqual(
        bpy.context.scene.world.name,
        "DropTestWorld",
        "Scene world must be set to the dropped world",
    )

    # Restore original world.
    bpy.context.scene.world = original_world


# ---------------------------------------------------------------------------
#
#    Same as above but use session_uid instead of name — this is how the
#    drop_copy callback actually populates the properties.
# ---------------------------------------------------------------------------

def view3d_drop_world_session_uid():
    import bpy
    _e, t, window = ui.test_window()
    area = _window_area(window, "VIEW_3D")
    region = next(r for r in area.regions if r.type == 'WINDOW')
    yield

    original_world = bpy.context.scene.world

    world = bpy.data.worlds.new("DropTestWorldUID")

    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.view3d.drop_world(session_uid=world.session_uid)

    t.assertEqual(
        bpy.context.scene.world.name,
        "DropTestWorldUID",
        "Scene world must match via session_uid lookup",
    )

    bpy.context.scene.world = original_world


# ---------------------------------------------------------------------------
#
#    Drop a world name that doesn't exist — the operator must cancel
#    without side effects.
# ---------------------------------------------------------------------------

def view3d_drop_world_cancel():
    import bpy
    _e, t, window = ui.test_window()
    area = _window_area(window, "VIEW_3D")
    region = next(r for r in area.regions if r.type == 'WINDOW')
    yield

    original_world = bpy.context.scene.world

    with bpy.context.temp_override(window=window, area=area, region=region):
        result = bpy.ops.view3d.drop_world(name="NonexistentWorld")

    t.assertTrue(
        result == {'CANCELLED'},
        "Dropping a nonexistent world must return CANCELLED",
    )
    t.assertEqual(
        bpy.context.scene.world,
        original_world,
        "Scene world must not change when drop is cancelled",
    )


# ---------------------------------------------------------------------------
#
#    UI_OT_drop_material has an exec path.  It needs ``material_slot`` and
#    ``object`` in the context.  Drop a material into a specific slot.
# ---------------------------------------------------------------------------

def ui_drop_material_slot():
    import bpy
    _e, t, window = ui.test_window()
    area = _window_area(window, "VIEW_3D")
    yield

    cube = bpy.data.objects["Cube"]
    original_material = bpy.data.materials.new("OriginalSlotMaterial")
    dropped_material = bpy.data.materials.new("DroppedSlotMaterial")
    cube.data.materials.append(original_material)
    yield

    # The drop_material exec path reads ``material_slot`` from context and
    # assigns the material to that slot's index.
    mat_slot = cube.material_slots[0]

    with bpy.context.temp_override(
        window=window, area=area,
        material_slot=mat_slot, object=cube,
    ):
        result = bpy.ops.ui.drop_material(session_uid=dropped_material.session_uid)

    t.assertEqual(result, {'FINISHED'})
    t.assertEqual(
        cube.material_slots[0].material.name,
        dropped_material.name,
        "Dropping a material must replace the target slot's material",
    )


# ---------------------------------------------------------------------------
#
#    SCENE_OT_drop_scene_asset has an exec path.  Drop a scene by session_uid
#    and verify it becomes the active scene.
# ---------------------------------------------------------------------------

def scene_drop_scene_asset():
    import bpy
    _e, t, _ = ui.test_window()
    yield

    original_scene = bpy.context.window.scene

    scene = bpy.data.scenes.new("DropTestScene")

    bpy.ops.scene.drop_scene_asset(session_uid=scene.session_uid)

    t.assertEqual(
        bpy.context.window.scene.name,
        "DropTestScene",
        "Active scene must be the dropped scene",
    )

    # Restore.
    bpy.context.window.scene = original_scene


# ---------------------------------------------------------------------------
#
#    Drop a nonexistent scene — must cancel cleanly.
# ---------------------------------------------------------------------------

def scene_drop_scene_asset_cancel():
    import bpy
    _e, t, _ = ui.test_window()
    yield

    original_scene = bpy.context.window.scene

    result = bpy.ops.scene.drop_scene_asset(session_uid=0)

    t.assertTrue(
        result == {'CANCELLED'},
        "Dropping nonexistent scene must return CANCELLED",
    )
    t.assertEqual(
        bpy.context.window.scene.name,
        original_scene.name,
        "Active scene must not change on cancel",
    )


# ---------------------------------------------------------------------------
#
#    A scene drop is handled by the window under the cursor. A child window
#    shares its scene with its parent, so verify the drop updates that shared
#    scene through the child-window context.
# ---------------------------------------------------------------------------

def scene_drop_scene_asset_child_window():
    import bpy
    _e, t, source_window = ui.test_window()
    scene = bpy.data.scenes.new("DropTargetWindowScene")

    bpy.ops.wm.window_new()
    yield
    target_window = next(
        window for window in bpy.context.window_manager.windows if window != source_window)

    with bpy.context.temp_override(window=target_window):
        result = bpy.ops.scene.drop_scene_asset(session_uid=scene.session_uid)

    t.assertEqual(result, {'FINISHED'})
    t.assertIs(target_window.scene, scene)
    t.assertIs(source_window.scene, scene)


# ---------------------------------------------------------------------------
#
#    FILE_OT_filepath_drop receives a path supplied by the dropbox callback.
#    Dropping a folder must navigate the target file-browser to that folder.
# ---------------------------------------------------------------------------

def file_browser_drop_folder():
    import os
    import tempfile
    import bpy

    _e, t, window = ui.test_window()
    area = _window_area(window, "FILE_BROWSER")
    region = next(r for r in area.regions if r.type == 'WINDOW')
    space = area.spaces.active
    yield

    with tempfile.TemporaryDirectory(prefix="blender-drop-folder-") as folder:
        with bpy.context.temp_override(window=window, area=area, region=region):
            result = bpy.ops.file.filepath_drop(filepath=folder)

        directory = space.params.directory
        if isinstance(directory, bytes):
            directory = directory.decode()
        t.assertEqual(result, {'FINISHED'})
        t.assertEqual(os.path.normpath(directory), os.path.normpath(folder))
