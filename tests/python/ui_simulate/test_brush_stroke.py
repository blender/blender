# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""
import collections

import modules.ui_test_utils as ui


COLOR_EPSILON = 1e-6

StrokeStats = collections.namedtuple(
    "StrokeStats",
    ("changed", "total", "non_finite", "changed_indices", "bounds_min", "bounds_max"),
)

Snapshot = collections.namedtuple("Snapshot", ("colors", "positions"))

SmoothStroke = collections.namedtuple("SmoothStroke", ("radius", "factor"))


def _activate_brush(t, brush_name):
    import bpy

    sculpt = bpy.context.tool_settings.sculpt
    if sculpt.brush.name == brush_name:
        return sculpt.brush

    result = bpy.ops.brush.asset_activate(
        asset_library_type='ESSENTIALS',
        relative_asset_identifier=(
            'brushes/essentials_brushes-mesh_sculpt.blend/Brush/{}'.format(brush_name)
        ),
    )
    t.assertEqual(result, {'FINISHED'})
    return sculpt.brush


def _configure_brush(
        t,
        brush_name="Paint Hard",
        *,
        stroke_method="SPACE",
        tip_roundness=1.0,
        spacing=10,
        size=120,
        smooth_stroke=None,
        rate=0.1):
    import bpy

    sculpt = bpy.context.tool_settings.sculpt
    brush = _activate_brush(t, brush_name)

    brush.use_unified_color = False
    brush.color = (0.0, 0.0, 0.0)
    brush.tip_roundness = tip_roundness
    brush.spacing = spacing

    brush.use_unified_size = False
    brush.size = size
    brush.use_locked_size = 'VIEW'
    brush.use_scene_spacing = 'VIEW'

    brush.strength = 1.0
    brush.use_unified_strength = False

    brush.stroke_method = stroke_method

    brush.rate = rate
    brush.flow = 1.0
    brush.use_flow_pressure = False
    brush.input_samples = 1
    brush.use_unified_input_samples = False
    brush.use_smooth_stroke = smooth_stroke is not None
    if smooth_stroke is not None:
        brush.smooth_stroke_radius = smooth_stroke.radius
        brush.smooth_stroke_factor = smooth_stroke.factor

    brush.use_pressure_size = False
    brush.use_pressure_strength = False

    brush.jitter = 0.0
    brush.jitter_absolute = 0
    brush.use_accumulate = False


def _snapshot(window):
    import numpy as np

    window.view_layer.update()
    obj = window.view_layer.objects.active
    tmp_mesh = obj.to_mesh(preserve_all_data_layers=True)

    num_verts = len(tmp_mesh.vertices)
    positions = np.empty(num_verts * 3, dtype=np.float32)
    tmp_mesh.vertices.foreach_get("co", positions)

    color_attr = tmp_mesh.color_attributes.get("Color")
    if color_attr is None:
        colors = np.ones((num_verts, 4), dtype=np.float32)
    else:
        colors = np.empty(len(color_attr.data) * 4, dtype=np.float32)
        color_attr.data.foreach_get("color", colors)
        colors = colors.reshape(-1, 4)

    obj.to_mesh_clear()
    return Snapshot(colors=colors, positions=positions.reshape(-1, 3))


def _color_stats(before, after, threshold=COLOR_EPSILON):
    import numpy as np

    delta = after.colors - before.colors
    finite = np.isfinite(after.colors).all(axis=1)
    color_distances = np.linalg.norm(delta, axis=1)
    changed_mask = finite & (color_distances > threshold)
    changed_indices = np.nonzero(changed_mask)[0]
    changed = int(changed_indices.size)

    if changed:
        changed_positions = after.positions[changed_mask]
        bounds_min = changed_positions.min(axis=0)
        bounds_max = changed_positions.max(axis=0)
    else:
        bounds_min = None
        bounds_max = None

    return StrokeStats(
        changed=changed,
        total=int(after.colors.shape[0]),
        non_finite=int((~finite).sum()),
        changed_indices=changed_indices,
        bounds_min=bounds_min,
        bounds_max=bounds_max,
    )


def _assert_changed(t, before, after):
    stats = _color_stats(before, after)
    t.assertEqual(stats.non_finite, 0)
    t.assertGreater(stats.changed, 0)
    return stats


def _assert_unchanged(t, before, after):
    stats = _color_stats(before, after)
    t.assertEqual(stats.non_finite, 0)
    t.assertEqual(stats.changed, 0)
    return stats


def _move_to(e, position):
    e.cursor_position_set(round(position[0]), round(position[1]), move=True)
    yield


def _stroke_from_to(e, start, end, steps=10):
    yield from _move_to(e, start)
    e.leftmouse.press()
    yield

    for i in range(1, steps + 1):
        factor = i / steps
        x = start[0] + (end[0] - start[0]) * factor
        y = start[1] + (end[1] - start[1]) * factor
        yield from _move_to(e, (x, y))

    e.leftmouse.release()
    yield


def _setup_sculpt_plane(e, window):
    """
    Set up a flat 32x32 quad grid (33x33 verts) at the origin and enter Sculpt Mode,
    viewed from above.
    """
    import bpy

    yield from ui.set_and_maximize_view3d_area(e)
    yield from ui.delete_all(e)

    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    region = next(r for r in area.regions if r.type == 'WINDOW')

    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.mesh.primitive_grid_add(
            x_subdivisions=32,
            y_subdivisions=32,
            size=2.0,
            location=(0.0, 0.0, 0.0),
        )

    # Top view, frame the grid.
    yield e.numpad_7().numpad_period()
    yield e.ctrl.tab().s()
    yield


def spacing_on_press():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')

    _configure_brush(t)

    before = _snapshot(window)

    yield from _move_to(e, center)
    e.leftmouse.press()
    yield
    e.leftmouse.release()
    yield

    _assert_changed(t, before, _snapshot(window))


def spacing_on_small_drag():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')

    # Setup a paint brush that needs movement.
    # Spacing is set to 100 to test whether spacing affects whether an early dab is put down. It should not.
    _configure_brush(t, tip_roundness=0.5, spacing=100)

    before = _snapshot(window)

    yield from _stroke_from_to(e, center, (center[0] + 10, center[1]), steps=10)

    _assert_changed(t, before, _snapshot(window))


def anchored():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')
    start = (center[0], center[1])
    end = (start[0] + 30, start[1])

    _configure_brush(t, stroke_method='ANCHORED')

    before = _snapshot(window)

    yield from _move_to(e, start)
    e.leftmouse.press()
    yield
    # Anchored strokes use the drag distance as the brush radius, pressing alone does nothing.
    _assert_unchanged(t, before, _snapshot(window))

    yield from _move_to(e, end)

    e.leftmouse.release()
    yield
    drag_stats = _assert_changed(t, before, _snapshot(window))

    t.assertIsNotNone(drag_stats.bounds_min)
    t.assertIsNotNone(drag_stats.bounds_max)
    # The affected region grows with the drag distance.
    t.assertGreater(drag_stats.bounds_max[0] - drag_stats.bounds_min[0], 0)


def dots():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')

    _configure_brush(t, stroke_method='DOTS')

    before = _snapshot(window)

    yield from _move_to(e, center)
    e.leftmouse.press()
    yield

    after = _snapshot(window)
    _assert_changed(t, before, after)
    before = after

    move_steps = 5
    pixels_per_step = 40

    for i in range(1, move_steps + 1):
        yield from _move_to(e, (center[0] + i * pixels_per_step, center[1]))

        after = _snapshot(window)
        # Every mouse movement adds another dab.
        _assert_changed(t, before, after)
        before = after

    e.leftmouse.release()
    yield


def drag_dot():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')

    _configure_brush(t, stroke_method='DRAG_DOT', size=40)

    before = _snapshot(window)
    previous_min = None

    yield from _move_to(e, center)
    e.leftmouse.press()
    yield

    move_steps = 3
    pixels_per_step = 60

    for i in range(1, move_steps + 1):
        yield from _move_to(e, (center[0] + i * pixels_per_step, center[1]))

        stats = _assert_changed(t, before, _snapshot(window))
        t.assertIsNotNone(stats.bounds_min)
        if previous_min is not None:
            # The previous dab is replaced instead of leaving a strip behind.
            t.assertGreater(stats.bounds_min[0], previous_min[0])
        previous_min = stats.bounds_min

    e.leftmouse.release()
    yield


def move_onto_mesh():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    outside = (area.x + int(area.width * 0.08), center[1])

    _configure_brush(t)

    before = _snapshot(window)

    yield from _move_to(e, outside)
    e.leftmouse.press()
    yield
    _assert_unchanged(t, before, _snapshot(window))

    # Moving while still outside the mesh should not start painting.
    yield from _move_to(e, (outside[0] + 40, outside[1]))
    _assert_unchanged(t, before, _snapshot(window))

    # Painting starts as soon as the cursor enters the mesh.
    yield from _move_to(e, center)
    _assert_changed(t, before, _snapshot(window))

    e.leftmouse.release()
    yield


def line():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')

    _configure_brush(t, stroke_method='LINE')

    before = _snapshot(window)

    yield from _move_to(e, center)
    e.leftmouse.press()
    yield
    _assert_unchanged(t, before, _snapshot(window))

    move_steps = 10
    pixels_per_step = 20

    for i in range(1, move_steps + 1):
        yield from _move_to(e, (center[0] + i * pixels_per_step, center[1]))

    # Nothing is painted until the stroke is confirmed.
    _assert_unchanged(t, before, _snapshot(window))

    e.leftmouse.release()
    yield

    _assert_changed(t, before, _snapshot(window))


def airbrush():
    import numpy as np

    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')

    # The airbrush keeps adding dabs while the brush is held down.
    _configure_brush(t, brush_name='Airbrush', stroke_method='AIRBRUSH', size=300, rate=0.01)

    before = _snapshot(window)

    yield from _move_to(e, center)
    e.leftmouse.press()
    yield
    # Let real time pass so the airbrush timer can add more dabs.
    yield from ui.idle_until(lambda: False, timeout=0.1)
    short = _snapshot(window)
    yield from ui.idle_until(lambda: False, timeout=0.1)
    long = _snapshot(window)
    e.leftmouse.release()
    yield

    _assert_changed(t, before, short)
    _assert_changed(t, short, long)

    # Holding the brush down longer paints more.
    short_amount = float(np.sum(1.0 - short.colors[:, :3]))
    long_amount = float(np.sum(1.0 - long.colors[:, :3]))
    t.assertGreater(long_amount, short_amount)


def smooth_stroke():
    e, t, window = ui.test_window()
    yield from _setup_sculpt_plane(e, window)

    center = ui.get_area_center_from_spacetype(window, 'VIEW_3D')

    # Using DOTS because bugs with the SPACE stroke method might cause more delay before first dab is applied.
    _configure_brush(
        t,
        stroke_method='DOTS',
        smooth_stroke=SmoothStroke(radius=20, factor=0.5),
    )

    before = _snapshot(window)

    yield from _move_to(e, center)
    e.leftmouse.press()
    yield

    yield from _move_to(e, (center[0] + 15, center[1]))

    _assert_unchanged(t, before, _snapshot(window))

    # Once the cursor leaves the smoothing radius, the stroke starts painting.
    yield from _move_to(e, (center[0] + 21, center[1]))
    _assert_changed(t, before, _snapshot(window))

    e.leftmouse.release()
    yield
