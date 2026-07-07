# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import modules.ui_test_utils as ui

# -----------------------------------------------------------------------------
# Viewport Rendering


def _interactive_rendering(engine):
    import bpy
    e, _, window = ui.test_window()

    rd = window.scene.render
    rd.engine = engine

    # Set up shading workspace with material editor and rendered viewport
    window.workspace = bpy.data.workspaces.get("Shading")
    yield
    properties_area = ui.get_window_area_by_type(window, 'PROPERTIES')
    properties_area.spaces.active.context = 'MATERIAL'
    view3d_area = ui.get_window_area_by_type(window, 'VIEW_3D')
    view3d_area.spaces.active.shading.type = 'RENDERED'
    yield

    # A few editing actions.
    def action_move():
        e.cursor_position_set(*ui.get_area_center(view3d_area), move=True)
        yield e.g().x().text("0.5").ret()

    def action_add_cube():
        e.cursor_position_set(*ui.get_area_center(view3d_area), move=True)
        bpy.ops.mesh.primitive_cube_add()
        yield

    def action_delete():
        e.cursor_position_set(*ui.get_area_center(view3d_area), move=True)
        yield e.delete().ret()

    def action_undo():
        yield e.ctrl.z()

    def action_redo():
        yield e.ctrl.shift.z()

    def action_add_node():
        ob = bpy.context.active_object
        tree = ob.active_material.node_tree
        tree.nodes.new(type="ShaderNodeMix")
        yield

    def action_unassign_material():
        ob = bpy.context.active_object
        ob.data.materials.pop()
        yield

    def action_assign_material():
        mat = bpy.data.materials.new(name="Test Material")
        ob = bpy.context.active_object
        ob.data.materials.append(mat)
        yield

    action_sequence = [
        action_move,
        action_add_cube,
        action_undo,
        action_redo,
        action_assign_material,
        action_add_node,
        action_unassign_material,
        action_delete,
    ]

    # Run actions one by one, for each giving a bit of time for
    # the interactive render to do some work.
    for action in action_sequence:
        yield from action()
        yield from ui.idle_until(lambda: False, timeout=0.2)


def interactive_rendering_cycles():
    yield from _interactive_rendering('CYCLES')


def interactive_rendering_eevee():
    yield from _interactive_rendering('BLENDER_EEVEE')

# -----------------------------------------------------------------------------
# Animation Rendering and Player


def _test_animation_player(t, window):
    # Launch animation player and verify it starts without crashing. It
    # doesn't support event simulation so not much we can test beyond that.
    import bpy
    import gpu
    import subprocess

    scene = window.scene
    rd = scene.render
    frame_path = rd.frame_path(frame=scene.frame_start)
    frame_path = bpy.path.abspath(frame_path)

    cmd = [
        bpy.app.binary_path,
        "--gpu-backend", gpu.platform.backend_type_get().lower(),
        "-a",
        "-f", str(rd.fps), str(rd.fps_base),
        "-s", str(scene.frame_start),
        "-e", str(scene.frame_end),
        "-j", str(scene.frame_step),
        frame_path,
    ]

    player_process = subprocess.Popen(cmd)

    # Wait a moment to verify it starts without immediately crashing.
    yield from ui.idle_until(lambda: False, timeout=2.0)
    t.assertIsNone(player_process.poll(), "Animation player process exited unexpectedly")

    # Terminate the player.
    player_process.terminate()
    try:
        player_process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        player_process.kill()


def _animation_rendering_and_player(temp_dir):
    import bpy
    from pathlib import Path
    e, t, window = ui.test_window()

    # Change engine to Cycles and make it render quick.
    scene = window.scene
    rd = scene.render
    rd.engine = 'CYCLES'
    scene.cycles.samples = 1
    scene.cycles.use_denoising = False
    rd.resolution_x = 128
    rd.resolution_y = 128
    scene.frame_start = 1
    scene.frame_end = 20

    # Keyframe cube in two different locations.
    cube = bpy.data.objects.get("Cube")
    scene.frame_set(1)
    cube.location = (0, 0, 0)
    cube.keyframe_insert(data_path="location", frame=1)
    scene.frame_set(20)
    cube.location = (5, 5, 5)
    cube.keyframe_insert(data_path="location", frame=20)
    yield

    # Render animation and cancel after a few frames.
    rd.filepath = str(Path(temp_dir) / "final_")
    start_path = Path(bpy.path.abspath(rd.frame_path(frame=scene.frame_start)))

    bpy.ops.render.render('INVOKE_DEFAULT', animation=True)
    yield
    yield from ui.idle_until(
        lambda: start_path.exists() and scene.frame_current > 2,
        timeout=20.0)
    yield e.esc()
    yield from ui.idle_until(
        lambda: not bpy.app.is_job_running('RENDER'),
        timeout=20.0)

    t.assertTrue(start_path.exists(), "Start frame was not rendered")

    # In 3D viewport, render complete playblast.
    rd.filepath = str(Path(temp_dir) / "playblast_")
    start_path = Path(bpy.path.abspath(rd.frame_path(frame=scene.frame_start)))
    end_path = Path(bpy.path.abspath(rd.frame_path(frame=scene.frame_end)))

    view3d_area = ui.get_window_area_by_type(window, 'VIEW_3D')
    e.cursor_position_set(*ui.get_area_center(view3d_area), move=True)
    yield from ui.call_menu(e, "Render Playblast")
    yield from ui.idle_until(
        lambda: end_path.exists() and not bpy.app.is_job_running('RENDER'),
        timeout=20.0)

    t.assertTrue(start_path.exists(), "Start frame was not rendered")
    t.assertTrue(end_path.exists(), "End frame was not rendered")

    # Test animation player.
    yield from _test_animation_player(t, window)
    yield


def animation_rendering_and_player():
    import tempfile
    with tempfile.TemporaryDirectory(prefix="blender_test_render_") as temp_dir:
        yield from _animation_rendering_and_player(temp_dir)
