# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.

Drawing and select-picking with the object type visibility & selectability toggles
in the 3D viewport header (the "Selectability & Visibility" settings), see: #159219.

Objects and instances are hidden by the type of the object they come from,
not the type of their instancer.
Selection checks the type of the object that's selected, for an instance that's its instancer,
see the "Scene Iteration" notes in ``draw_context.cc``.

Each test steps through toggle states, checking what's drawn and what's picked in each,
with a comment on what the state tests.

Helpers yield after changing anything, giving Blender a tick to process the change
before the next step.
"""
import math
import modules.ui_test_utils as ui

# The empty and the cube it instances share this location.
INSTANCER_LOCATION = (0.0, 0.0, 0.0)
# Inside the instanced cube, away from the empty's axes which are selectable too.
INSTANCE_PICK_CO = (1.0, 0.0, 1.0)
# Behind the cube (the front view looks along +Y).
BEHIND_LOCATION = (0.0, 8.0, 0.0)
# On the bone behind, above the cube.
BEHIND_PICK_CO = (0.0, 8.0, 4.0)
# The posed armature, with a single bone.
ARMATURE_LOCATION = (6.0, 0.0, 0.0)
# The middle of the posed armature's bone.
BONE_PICK_CO = (6.0, 0.0, 1.0)
# A vertical plane instancing a bone on each of its vertices.
VERT_INSTANCER_LOCATION = (-6.0, 0.0, 0.0)
# The bone instanced on the plane's bottom-left vertex (offset below the plane).
VERT_INSTANCE_PICK_CO = (-7.0, 0.0, -2.5)
# Behind the bone instance, at the depth of the bone behind the cube.
VERT_BEHIND_LOCATION = (VERT_INSTANCE_PICK_CO[0], BEHIND_LOCATION[1], VERT_INSTANCE_PICK_CO[2])
# Points to check, by name.
SCENE_POINTS = {
    "cube": INSTANCE_PICK_CO,
    "bone_behind": BEHIND_PICK_CO,
    "plane": VERT_INSTANCER_LOCATION,
    "bone_instance": VERT_INSTANCE_PICK_CO,
}
# The colors drawn & the objects picked at the points with everything shown.
SCENE_COLORS = {"cube": 'RED', "bone_behind": 'GREEN', "plane": 'CYAN', "bone_instance": 'GREEN'}
SCENE_PICKS = {
    "cube": "Instancer",
    "bone_behind": "Bone Behind",
    "plane": "Vertex Instancer",
    "bone_instance": "Vertex Instancer",
}

# Objects which generate their own geometry, see `_generated_geometry_add`.
METABALL_LOCATION = (-12.0, 0.0, 0.0)
TEXT_LOCATION = (-12.0, 0.0, 4.0)
# Inside the extruded letter, its origin is on the baseline.
TEXT_PICK_CO = (-12.0, 0.0, 4.3)
CURVE_LOCATION = (-12.0, 0.0, -4.0)
GENERATED_POINTS = {
    "metaball": METABALL_LOCATION,
    "text": TEXT_PICK_CO,
    "curve": CURVE_LOCATION,
}
GENERATED_COLORS = {"metaball": 'MAGENTA', "text": 'YELLOW', "curve": 'WHITE'}
GENERATED_PICKS = {"metaball": "Metaball", "text": "Text", "curve": "Curve"}

# An empty instancing a point light, see `_light_instancer_add`.
LIGHT_INSTANCER_LOCATION = (12.0, 0.0, 0.0)
# The instanced light, offset above the empty so they don't overlap.
LIGHT_INSTANCE_PICK_CO = (12.0, 0.0, 2.0)

# An empty instancing a collection which holds a vertex instancer, see `_nested_instancer_add`.
# At the light instancer's location, the two are never in the same scene.
NESTED_INSTANCER_LOCATION = (12.0, 0.0, 0.0)
# On the plane instance, away from the empty's axes.
NESTED_PLANE_PICK_CO = (12.75, 0.0, 0.75)
# The bone instanced on the plane's bottom-left vertex (offset below the plane).
NESTED_BONE_PICK_CO = (11.0, 0.0, -2.5)
NESTED_POINTS = {
    "plane": NESTED_PLANE_PICK_CO,
    "bone": NESTED_BONE_PICK_CO,
}
NESTED_COLORS = {"plane": 'MAGENTA', "bone": 'GREEN'}
NESTED_PICKS = {"plane": "Nested Instancer", "bone": "Nested Instancer"}

# Pixel colors classified by thresholding each channel.
COLOR_NAMES = {
    (False, False, False): 'BLACK',
    (True, False, False): 'RED',
    (False, True, False): 'GREEN',
    (False, False, True): 'BLUE',
    (False, True, True): 'CYAN',
    (True, False, True): 'MAGENTA',
    (True, True, False): 'YELLOW',
    (True, True, True): 'WHITE',
}


# -----------------------------------------------------------------------------
# Utilities

def _view3d_region(area):
    return next(region for region in area.regions if region.type == 'WINDOW')


def _view3d_location_from_world(area, co):
    """Region relative pixel location of a world-space coordinate."""
    from bpy_extras.view3d_utils import location_3d_to_region_2d
    region = _view3d_region(area)
    xy = location_3d_to_region_2d(region, region.data, co)
    return int(xy[0]), int(xy[1])


def _view3d_select_at(window, area, co):
    """Select-pick a world-space coordinate by calling ``view3d.select`` directly."""
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.view3d.select(location=_view3d_location_from_world(area, co), deselect_all=True)
    yield


def _view3d_colors(window, area, points):
    """
    The name of the color drawn at each point (see ``COLOR_NAMES``),
    ``points`` maps names to world-space coordinates.
    """
    # Let the event loop redraw before reading pixels.
    yield
    region = _view3d_region(area)
    pixels = window.screenshot(region=(
        (region.x, region.y),
        (region.x + region.width, region.y + region.height),
    ))
    colors = {}
    for name, co in points.items():
        x, y = _view3d_location_from_world(area, co)
        colors[name] = COLOR_NAMES[tuple(pixels[y, x, c] >= 128 for c in range(3))]
    return colors


def _view3d_click_at(e, area, co):
    """Select-pick a world-space coordinate with a simulated click."""
    region = _view3d_region(area)
    x, y = _view3d_location_from_world(area, co)
    e.cursor_position_set(region.x + x, region.y + y, move=True)
    yield
    e.leftmouse.tap()
    yield


def _show_object_reset(space, *, disable=()):
    """
    Show & allow selecting all object types except ``disable``:
    the ``show_object_*`` attributes without their prefix, e.g. "select_mesh".
    """
    prefix = "show_object_"
    toggles = {
        prop.identifier[len(prefix):]
        for prop in space.bl_rna.properties
        if prop.identifier.startswith((prefix + "viewport_", prefix + "select_"))
    }
    toggles_unknown = set(disable) - toggles
    assert not toggles_unknown, toggles_unknown
    for toggle in toggles:
        setattr(space, prefix + toggle, toggle not in disable)
    yield


def _collection_instancer_new(scene, name, location, objects, *, color):
    """
    Create an empty named ``name`` instancing a new collection which holds ``objects``.
    The objects are moved out of their collections so only their instances are visible.
    """
    import bpy
    instance_collection = bpy.data.collections.new(name)
    for ob in objects:
        for collection in ob.users_collection:
            collection.objects.unlink(ob)
        instance_collection.objects.link(ob)

    ob_instancer = bpy.data.objects.new(name, None)
    ob_instancer.location = location
    ob_instancer.empty_display_size = 0.5
    ob_instancer.color = color
    ob_instancer.instance_type = 'COLLECTION'
    ob_instancer.instance_collection = instance_collection
    scene.collection.objects.link(ob_instancer)
    return ob_instancer


def _envelope_bone_new(window, name, location, *, length, radius):
    """
    Create an armature named ``name`` with a single bone centered on ``location``.
    Envelope display so it has a solid surface to pick.
    Must run with a 3D viewport context.
    """
    import bpy
    from mathutils import Vector
    # Offset down by half the bone's length so it's centered on `location`.
    bpy.ops.object.armature_add(radius=length, location=Vector(location) - Vector((0.0, 0.0, length / 2.0)))
    ob_armature = window.view_layer.objects.active
    ob_armature.name = name
    ob_armature.data.display_type = 'ENVELOPE'
    ob_armature.data.bones[0].head_radius = radius
    ob_armature.data.bones[0].tail_radius = radius
    return ob_armature


def _scene_setup():
    """
    Replace the startup scene with, in a front orthographic view:

    - "Instancer": an empty instancing a collection which holds "Instanced Cube".
      The cube is a mesh, the instancer is an empty.
    - "Bone Behind": an armature behind the cube, larger than the cube so it shows around it.
      Envelope display so it has a solid surface to pick.
    - "Armature": a single bone, with "Weight Paint Cube" parented to it.
    - "Vertex Instancer": a vertical plane instancing "Instanced Bone" on its vertices.
      The instancer is a mesh, the instances are armatures.
    - "Bone Behind Instances": an armature behind the bone instanced on the plane's bottom-left vertex,
      larger than the instance so it shows around it.

    Flat shading with a distinct color per object (bones use the theme's solid bone color)
    on a black background, so it's clear what's drawn and pixels can be checked.
    Instances take the color of their instancer.
    """
    import bpy

    e, t, window = ui.test_window()
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    with bpy.context.temp_override(window=window, area=area):
        bpy.ops.screen.screen_full_area(use_hide_panels=True)
    # Maximizing creates a new screen, the area must be looked up again.
    yield
    area = ui.get_window_area_by_type(window, 'VIEW_3D')
    region = _view3d_region(area)
    scene = window.scene

    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob)

    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.mesh.primitive_cube_add(size=3.0)
        ob_cube = window.view_layer.objects.active
        ob_cube.name = "Instanced Cube"
        _collection_instancer_new(scene, "Instancer", INSTANCER_LOCATION, (ob_cube,), color=(1.0, 0.0, 0.0, 1.0))

        _envelope_bone_new(window, "Bone Behind", BEHIND_LOCATION, length=6.0, radius=3.0)

        bpy.ops.object.armature_add(radius=2.0, location=ARMATURE_LOCATION)
        ob_armature = window.view_layer.objects.active
        ob_armature.name = "Armature"

        bpy.ops.mesh.primitive_cube_add(size=1.5)
        ob_weight_paint_cube = window.view_layer.objects.active
        ob_weight_paint_cube.name = "Weight Paint Cube"
        ob_weight_paint_cube.color = (0.0, 0.0, 1.0, 1.0)
        ob_weight_paint_cube.parent = ob_armature
        ob_weight_paint_cube.parent_type = 'ARMATURE'
        ob_weight_paint_cube.location = (0.0, 0.0, -3.0)

        bpy.ops.mesh.primitive_plane_add(
            size=2.0, location=VERT_INSTANCER_LOCATION, rotation=(math.radians(90.0), 0.0, 0.0))
        ob_vert_instancer = window.view_layer.objects.active
        ob_vert_instancer.name = "Vertex Instancer"
        ob_vert_instancer.color = (0.0, 1.0, 1.0, 1.0)
        ob_vert_instancer.instance_type = 'VERTS'

        bpy.ops.object.armature_add(radius=1.0)
        ob_armature_instanced = window.view_layer.objects.active
        ob_armature_instanced.name = "Instanced Bone"
        ob_armature_instanced.data.display_type = 'ENVELOPE'
        ob_armature_instanced.data.bones[0].head_radius = 0.5
        ob_armature_instanced.data.bones[0].tail_radius = 0.5
        ob_armature_instanced.parent = ob_vert_instancer
        # Offset in the plane's local space so the instances & the original bone
        # (which is also drawn) don't overlap the plane's center.
        ob_armature_instanced.location = (0.0, -1.5, 0.0)

        _envelope_bone_new(window, "Bone Behind Instances", VERT_BEHIND_LOCATION, length=1.0, radius=0.75)

        bpy.ops.object.select_all(action='DESELECT')
        window.view_layer.objects.active = None

        bpy.ops.view3d.view_axis(type='FRONT')
        bpy.ops.view3d.view_all()

    shading = area.spaces.active.shading
    shading.type = 'SOLID'
    shading.light = 'FLAT'
    shading.color_type = 'OBJECT'
    shading.background_type = 'VIEWPORT'
    shading.background_color = (0.0, 0.0, 0.0)
    # Bones only take their color from the theme, each test runs in its own process with factory settings.
    bpy.context.preferences.themes[0].view_3d.bone_solid = (0.0, 1.0, 0.0)
    # Keep the background black where pixels are read.
    overlay = area.spaces.active.overlay
    overlay.show_ortho_grid = False
    overlay.show_floor = False
    overlay.show_axis_x = False
    overlay.show_axis_y = False
    overlay.show_axis_z = False
    overlay.show_cursor = False
    # Future proofing, colors are only read with nothing selected so in practice the outline
    # & origins (only drawn for selected objects) aren't drawn.
    # Hide them so a selection can't change the colors read, they may be handy to enable for visual feedback.
    overlay.show_outline_selected = False
    overlay.show_object_origins = False

    # Reset the toggles & draw once so the view matrices are valid before projecting locations.
    yield from _show_object_reset(area.spaces.active)
    return e, t, window, area


def _metaball_add(window, area):
    """Add "Metaball", a single ball."""
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.metaball_add(type='BALL', radius=2.0, location=METABALL_LOCATION)
        ob_metaball = window.view_layer.objects.active
        ob_metaball.name = "Metaball"

        bpy.ops.object.select_all(action='DESELECT')
        window.view_layer.objects.active = None
        bpy.ops.view3d.view_all()
    yield


def _generated_geometry_add(window, area):
    """
    Add objects whose visible geometry is a mesh they generate for themselves.
    The draw loop handles the mesh as an instance of the object, keeping the object's type.
    See ``GENERATED_POINTS`` for the points to check.
    """
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.text_add(location=TEXT_LOCATION)
        ob_text = window.view_layer.objects.active
        ob_text.name = "Text"
        ob_text.color = (1.0, 1.0, 0.0, 1.0)
        ob_text.data.body = "I"
        ob_text.data.align_x = 'CENTER'
        ob_text.data.extrude = 1.0

        # A curve object with geometry nodes which output a cube.
        bpy.ops.curve.primitive_bezier_circle_add(location=CURVE_LOCATION)
        ob_curve = window.view_layer.objects.active
        ob_curve.name = "Curve"
        ob_curve.color = (1.0, 1.0, 1.0, 1.0)
        node_group = bpy.data.node_groups.new("Cube", 'GeometryNodeTree')
        node_group.interface.new_socket("Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')
        group_output = node_group.nodes.new('NodeGroupOutput')
        mesh_cube = node_group.nodes.new('GeometryNodeMeshCube')
        mesh_cube.inputs["Size"].default_value = (2.0, 2.0, 2.0)
        node_group.links.new(mesh_cube.outputs["Mesh"], group_output.inputs[0])
        ob_curve.modifiers.new("Cube", 'NODES').node_group = node_group

    # Added last, it deselects & frames all objects.
    yield from _metaball_add(window, area)
    bpy.data.objects["Metaball"].color = (1.0, 0.0, 1.0, 1.0)
    yield


def _light_instancer_add(window, area):
    """
    Add "Light Instancer": an empty instancing a collection which holds a point light.
    The light is drawn as an overlay extra, not as geometry.
    """
    import bpy
    from mathutils import Vector
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        # Offset so the instance is at `LIGHT_INSTANCE_PICK_CO`.
        bpy.ops.object.light_add(
            type='POINT',
            location=Vector(LIGHT_INSTANCE_PICK_CO) - Vector(LIGHT_INSTANCER_LOCATION),
        )
        ob_light = window.view_layer.objects.active
        ob_light.name = "Instanced Light"
        # The color doesn't matter, only picking is checked for the light.
        _collection_instancer_new(
            window.scene, "Light Instancer", LIGHT_INSTANCER_LOCATION, (ob_light,), color=(1.0, 1.0, 1.0, 1.0),
        )

        bpy.ops.object.select_all(action='DESELECT')
        window.view_layer.objects.active = None
        bpy.ops.view3d.view_all()
    yield


def _nested_instancer_add(window, area):
    """
    Add "Nested Instancer": an empty instancing a collection which holds "Nested Plane",
    a vertical plane instancing "Nested Bone" on its vertices (as "Vertex Instancer").
    The plane and the bones are all instances of the empty, the dupli-list flattens them.
    """
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.mesh.primitive_plane_add(size=2.0, rotation=(math.radians(90.0), 0.0, 0.0))
        ob_plane = window.view_layer.objects.active
        ob_plane.name = "Nested Plane"
        ob_plane.instance_type = 'VERTS'

        bpy.ops.object.armature_add(radius=1.0)
        ob_armature_instanced = window.view_layer.objects.active
        ob_armature_instanced.name = "Nested Bone"
        ob_armature_instanced.data.display_type = 'ENVELOPE'
        ob_armature_instanced.data.bones[0].head_radius = 0.5
        ob_armature_instanced.data.bones[0].tail_radius = 0.5
        ob_armature_instanced.parent = ob_plane
        ob_armature_instanced.location = (0.0, -1.5, 0.0)

        _collection_instancer_new(
            window.scene,
            "Nested Instancer",
            NESTED_INSTANCER_LOCATION,
            (ob_plane, ob_armature_instanced),
            color=(1.0, 0.0, 1.0, 1.0),
        )

        bpy.ops.object.select_all(action='DESELECT')
        window.view_layer.objects.active = None
        bpy.ops.view3d.view_all()
    yield


def _deselect_all(window, area):
    """
    Deselect all & clear the active object so earlier picks don't affect the next.
    Picking steps past the active object when it's already selected.
    """
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.select_all(action='DESELECT')
    window.view_layer.objects.active = None
    yield


def _selected_name(t, window):
    """
    The name of the only selected object, None when nothing is selected.
    The active object is only checked when something is selected,
    as picking nothing leaves the previous active object in place.
    """
    selected = [ob for ob in window.view_layer.objects if ob.select_get()]
    t.assertLessEqual(len(selected), 1, [ob.name for ob in selected])
    if not selected:
        return None
    t.assertEqual(window.view_layer.objects.active, selected[0])
    return selected[0].name


def _view3d_picks(t, window, area, points):
    """
    The name of the object picked at each point, None when nothing is picked,
    ``points`` maps names to world-space coordinates. Everything is deselected after each pick.
    """
    picked = {}
    for name, co in points.items():
        yield from _view3d_select_at(window, area, co)
        picked[name] = _selected_name(t, window)
        yield from _deselect_all(window, area)
    return picked


def _view3d_assert(t, window, area, points, *, colors, picks):
    """
    Assert what's drawn and what's picked at ``points``: ``colors`` and ``picks`` map point names
    to the expected color and the name of the expected object (None for nothing).
    """
    colors_drawn = yield from _view3d_colors(window, area, points)
    t.assertEqual(colors_drawn, colors)
    picked = yield from _view3d_picks(t, window, area, points)
    t.assertEqual(picked, picks)


def _object_mode_enter(window, area):
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.mode_set(mode='OBJECT')
    yield


def _pose_mode_enter(window, area, ob_armature):
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.select_all(action='DESELECT')
        ob_armature.select_set(True)
        window.view_layer.objects.active = ob_armature
        bpy.ops.object.mode_set(mode='POSE')
    yield


def _edit_mode_enter(window, area, ob):
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        window.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')
    yield


def _weight_paint_mode_enter(window, area, ob_armature, ob):
    """Enter weight paint with the armature selected, so it enters pose mode too."""
    import bpy
    region = _view3d_region(area)
    with bpy.context.temp_override(window=window, area=area, region=region):
        bpy.ops.object.select_all(action='DESELECT')
        ob_armature.select_set(True)
        ob.select_set(True)
        window.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='WEIGHT_PAINT')
    yield


# -----------------------------------------------------------------------------
# Tests

def visibility_by_own_type():
    """
    Hiding a type hides the objects and instances of that type,
    not the instances of an instancer of that type.
    Hidden objects don't block clicks, the object behind is picked instead.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active

    def assert_points(*, colors, picks):
        yield from _view3d_assert(t, window, area, SCENE_POINTS, colors=colors, picks=picks)

    # Everything shown & selectable, the points hit what they're named for.
    yield from assert_points(colors=SCENE_COLORS, picks=SCENE_PICKS)

    # Hiding "Mesh" hides the cube instance & the plane, not the plane's bone instances.
    # The bone behind is picked where the hidden cube was.
    # The bone instances can't be picked but block clicks on the bone behind them, see "Hiding Empty" below.
    yield from _show_object_reset(space, disable={"viewport_mesh"})
    yield from assert_points(
        colors=SCENE_COLORS | {"cube": 'GREEN', "plane": 'BLACK'},
        picks=SCENE_PICKS | {"cube": "Bone Behind", "plane": None, "bone_instance": None},
    )

    # Hiding "Armature" hides the bones behind & the bone instances, nothing is picked there.
    yield from _show_object_reset(space, disable={"viewport_armature"})
    yield from assert_points(
        colors=SCENE_COLORS | {"bone_behind": 'BLACK', "bone_instance": 'BLACK'},
        picks=SCENE_PICKS | {"bone_behind": None, "bone_instance": None},
    )

    # Hiding "Empty" hides the empty, not its cube instance.
    # A hidden instancer's instances can't be picked but block clicks.
    # This checks the current behavior, not a requirement, see the "Scene Iteration" notes in `draw_context.cc`.
    yield from _show_object_reset(space, disable={"viewport_empty"})
    yield from assert_points(
        colors=SCENE_COLORS,
        picks=SCENE_PICKS | {"cube": None},
    )

    # Re-enabling everything restores the initial state.
    yield from _show_object_reset(space)
    yield from assert_points(colors=SCENE_COLORS, picks=SCENE_PICKS)


def selectability_by_instancer_type():
    """
    Disabling selection of a type prevents picking objects of that type and their instances,
    clicks pass through them to the object behind.
    An instance's own type isn't checked, picking it selects its instancer.
    Neither the type toggles nor the outliner's change what's drawn.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    objects = window.view_layer.objects

    def assert_points(*, picks):
        yield from _view3d_assert(t, window, area, SCENE_POINTS, colors=SCENE_COLORS, picks=picks)

    def assert_instancer_passes_through():
        # The empty's origin is drawn over the cube instance, the click passes through both.
        picked = yield from _view3d_picks(t, window, area, {"instancer": INSTANCER_LOCATION})
        t.assertEqual(picked, {"instancer": "Bone Behind"})

    # Everything shown & selectable, the points hit what they're named for.
    yield from assert_points(picks=SCENE_PICKS)

    # Disabling "Mesh" selection: the cube instance still picks the empty.
    # The plane can't be picked, nor can its bone instances, an unselectable instancer skips them
    # so they pass clicks through to the bone behind them.
    yield from _show_object_reset(space, disable={"select_mesh"})
    yield from assert_points(picks=SCENE_PICKS | {"plane": None, "bone_instance": "Bone Behind Instances"})

    # Disabling "Armature" selection: the bones behind can't be picked, the bone instances still pick the plane.
    yield from _show_object_reset(space, disable={"select_armature"})
    yield from assert_points(picks=SCENE_PICKS | {"bone_behind": None})

    # Disabling "Empty" selection: the empty & its cube instance pass clicks through to the bone behind.
    yield from _show_object_reset(space, disable={"select_empty"})
    yield from assert_points(picks=SCENE_PICKS | {"cube": "Bone Behind"})
    yield from assert_instancer_passes_through()

    # The outliner's selectability toggle on the instancers does the same as their type toggles.
    yield from _show_object_reset(space)
    objects["Instancer"].hide_select = True
    objects["Vertex Instancer"].hide_select = True
    yield
    yield from assert_points(
        picks=SCENE_PICKS | {"cube": "Bone Behind", "plane": None, "bone_instance": "Bone Behind Instances"},
    )
    yield from assert_instancer_passes_through()
    # Restore the outliner toggles.
    objects["Instancer"].hide_select = False
    objects["Vertex Instancer"].hide_select = False
    yield

    # An instancer both hidden & unselectable: clicks pass through its instances, unlike hidden alone
    # (see `visibility_by_own_type`), as selectability is checked before visibility.
    yield from _show_object_reset(space, disable={"viewport_empty", "select_empty"})
    yield from assert_points(picks=SCENE_PICKS | {"cube": "Bone Behind"})

    # Simulated clicks instead of the select operator: "Empty" selection disabled picks the bone behind.
    yield from _show_object_reset(space, disable={"select_empty"})
    yield from _view3d_click_at(e, area, INSTANCE_PICK_CO)
    t.assertEqual(_selected_name(t, window), "Bone Behind")
    yield from _deselect_all(window, area)

    # "Empty" selection enabled again picks the instancer.
    yield from _show_object_reset(space)
    yield from _view3d_click_at(e, area, INSTANCE_PICK_CO)
    t.assertEqual(_selected_name(t, window), "Instancer")


def pose_bone_select_with_armature_unselectable():
    """
    Bones remain selectable in pose mode when the armature can't be selected,
    whether by its type's selection toggle or the outliner's.
    The same applies to the type's toggle when weight painting an object the armature deforms.
    This checks the current behavior, not a requirement,
    see the element select note in ``draw_context.cc``.
    Drawing doesn't depend on selectability, so only picking is checked.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    objects = window.view_layer.objects
    ob_armature = objects["Armature"]

    def pick_bone():
        # Deselect the bone so the pick must select it.
        pose_bone = ob_armature.pose.bones[0]
        pose_bone.select = False
        yield from _view3d_select_at(window, area, BONE_PICK_CO)
        t.assertTrue(pose_bone.select)

    yield from _pose_mode_enter(window, area, ob_armature)
    t.assertEqual(ob_armature.mode, 'POSE')

    # Pose mode, "Armature" selection disabled: the bone is picked.
    yield from _show_object_reset(space, disable={"select_armature"})
    yield from pick_bone()

    # Pose mode, the armature unselectable in the outliner: the bone is picked.
    yield from _show_object_reset(space)
    ob_armature.hide_select = True
    yield
    yield from pick_bone()
    # Restore the outliner toggle.
    ob_armature.hide_select = False
    yield

    # Weight paint on an object the armature deforms, "Armature" selection disabled: the bone is picked.
    yield from _object_mode_enter(window, area)
    yield from _weight_paint_mode_enter(window, area, ob_armature, objects["Weight Paint Cube"])
    t.assertEqual(objects["Weight Paint Cube"].mode, 'WEIGHT_PAINT')
    t.assertEqual(ob_armature.mode, 'POSE')
    yield from _show_object_reset(space, disable={"select_armature"})
    yield from pick_bone()


def edit_mode_element_select_with_type_unselectable():
    """
    Edit bones & metaball elements remain selectable in edit mode when their object can't be selected,
    whether by its type's selection toggle or the outliner's.
    This checks the current behavior, not a requirement,
    see the element select note in ``draw_context.cc``.
    Drawing doesn't depend on selectability, so only picking is checked.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    yield from _metaball_add(window, area)
    objects = window.view_layer.objects

    def pick_edit_bone(ob):
        # Deselect the bone so the pick must select it.
        edit_bone = ob.data.edit_bones[0]
        edit_bone.select = False
        edit_bone.select_head = False
        edit_bone.select_tail = False
        yield from _view3d_select_at(window, area, BONE_PICK_CO)
        t.assertTrue(ob.data.edit_bones[0].select)

    def pick_metaball_element(ob):
        from mathutils import Vector
        # Deselect the element so the pick must select it.
        element = ob.data.elements[0]
        element.select = False
        # On the element's radius circle, which is drawn for selection.
        co = ob.matrix_world @ (element.co + Vector((element.radius, 0.0, 0.0)))
        yield from _view3d_select_at(window, area, co)
        t.assertTrue(ob.data.elements[0].select)

    for ob, toggle, pick in (
            (objects["Armature"], "select_armature", pick_edit_bone),
            (objects["Metaball"], "select_meta", pick_metaball_element),
    ):
        yield from _edit_mode_enter(window, area, ob)
        t.assertEqual(ob.mode, 'EDIT')

        # Everything selectable: the element is picked.
        yield from pick(ob)

        # The object's type selection disabled: the element is picked.
        yield from _show_object_reset(space, disable={toggle})
        yield from pick(ob)

        # The object unselectable in the outliner: the element is picked.
        yield from _show_object_reset(space)
        ob.hide_select = True
        yield
        yield from pick(ob)
        # Restore the outliner toggle.
        ob.hide_select = False
        yield

        yield from _object_mode_enter(window, area)


def generated_geometry_by_object_type():
    """
    Geometry an object generates for itself is hidden and made unselectable by the object's type,
    not by the geometry's type: text, meta-balls and a curve's geometry-nodes output are all meshes.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    yield from _generated_geometry_add(window, area)

    def assert_points(*, colors, picks):
        yield from _view3d_assert(t, window, area, GENERATED_POINTS, colors=colors, picks=picks)

    # Everything shown & selectable, the points hit what they're named for.
    yield from assert_points(colors=GENERATED_COLORS, picks=GENERATED_PICKS)

    # Hiding "Mesh" & disabling its selection changes nothing, the toggles don't apply to the generated meshes.
    yield from _show_object_reset(space, disable={"viewport_mesh", "select_mesh"})
    yield from assert_points(colors=GENERATED_COLORS, picks=GENERATED_PICKS)

    # Point names with the suffix of the toggles for their object type, e.g. "viewport_font".
    for name, toggle_type in (("metaball", "meta"), ("text", "font"), ("curve", "curve")):
        # Hiding the object's type hides its generated geometry.
        yield from _show_object_reset(space, disable={"viewport_" + toggle_type})
        yield from assert_points(colors=GENERATED_COLORS | {name: 'BLACK'}, picks=GENERATED_PICKS | {name: None})

        # Disabling selection of the object's type prevents picking its generated geometry.
        yield from _show_object_reset(space, disable={"select_" + toggle_type})
        yield from assert_points(colors=GENERATED_COLORS, picks=GENERATED_PICKS | {name: None})

    # Re-enabling everything restores the initial state.
    yield from _show_object_reset(space)
    yield from assert_points(colors=GENERATED_COLORS, picks=GENERATED_PICKS)


def instanced_light_by_own_type():
    """
    A light instanced by an empty is hidden by its own type, its selection by the empty's type.
    Lights are drawn as overlay extras, a different draw path from geometry.
    They have no fill color to read, so only picking is checked.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    yield from _light_instancer_add(window, area)

    def assert_picks(*, picks):
        picked = yield from _view3d_picks(t, window, area, {"light": LIGHT_INSTANCE_PICK_CO})
        t.assertEqual(picked, picks)

    # Everything shown & selectable, picking the light selects the empty.
    yield from assert_picks(picks={"light": "Light Instancer"})

    # Hiding "Light": the light can't be picked.
    yield from _show_object_reset(space, disable={"viewport_light"})
    yield from assert_picks(picks={"light": None})

    # Disabling "Light" selection: picking the light still selects the empty.
    yield from _show_object_reset(space, disable={"select_light"})
    yield from assert_picks(picks={"light": "Light Instancer"})

    # Disabling "Empty" selection: the instancer skips its instances, so the light can't be picked either.
    yield from _show_object_reset(space, disable={"select_empty"})
    yield from assert_picks(picks={"light": None})

    # Disabling "Mesh" selection doesn't affect the light.
    yield from _show_object_reset(space, disable={"select_mesh"})
    yield from assert_picks(picks={"light": "Light Instancer"})

    # Re-enabling everything restores the initial state.
    yield from _show_object_reset(space)
    yield from assert_picks(picks={"light": "Light Instancer"})


def nested_instances_by_own_type():
    """
    Nesting doesn't change the rule, each instance is hidden by its own type
    and its selection depends on its instancer's type.
    The dupli-list flattens the nesting so the empty is the instancer of the nested plane
    and the bones it instances alike, picking any of them selects the empty.
    """
    e, t, window, area = yield from _scene_setup()
    space = area.spaces.active
    yield from _nested_instancer_add(window, area)

    def assert_points(*, colors, picks):
        yield from _view3d_assert(t, window, area, NESTED_POINTS, colors=colors, picks=picks)

    # Everything shown & selectable, the points hit what they're named for.
    yield from assert_points(colors=NESTED_COLORS, picks=NESTED_PICKS)

    # Hiding "Empty" hides the empty, not the plane & bones, which then can't be picked, see `visibility_by_own_type`.
    yield from _show_object_reset(space, disable={"viewport_empty"})
    yield from assert_points(colors=NESTED_COLORS, picks={"plane": None, "bone": None})

    # Hiding "Mesh" hides the plane, not the bones it instances.
    yield from _show_object_reset(space, disable={"viewport_mesh"})
    yield from assert_points(colors=NESTED_COLORS | {"plane": 'BLACK'}, picks=NESTED_PICKS | {"plane": None})

    # Hiding "Armature" hides the bones, not the plane.
    yield from _show_object_reset(space, disable={"viewport_armature"})
    yield from assert_points(colors=NESTED_COLORS | {"bone": 'BLACK'}, picks=NESTED_PICKS | {"bone": None})

    # Disabling "Empty" selection: the empty skips all its instances, neither the plane nor the bones can be picked.
    yield from _show_object_reset(space, disable={"select_empty"})
    yield from assert_points(colors=NESTED_COLORS, picks={"plane": None, "bone": None})

    # Disabling "Mesh" & "Armature" selection: the plane & bones still pick the empty.
    yield from _show_object_reset(space, disable={"select_mesh", "select_armature"})
    yield from assert_points(colors=NESTED_COLORS, picks=NESTED_PICKS)

    # Re-enabling everything restores the initial state.
    yield from _show_object_reset(space)
    yield from assert_points(colors=NESTED_COLORS, picks=NESTED_PICKS)
