# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.

Covered test cases:
  - VIEW_3D ("Monkey") → Add mesh primitive
  - PROPERTIES ("Subdivision Surface") → Add modifier
  - SEQUENCE_EDITOR ("Adjustment Layer") → Add strip
  - SEQUENCE_EDITOR ("Color Balance") → Add strip modifier
  - NODE_EDITOR / Shader ("Math") → Add shader node
  - NODE_EDITOR / Compositor ("RGB Curves") → Add compositor node
  - NODE_EDITOR / Geometry ("Transform Geometry") → Add geometry node
  - NODE_EDITOR / Swap ("Math") → Swap active node
  - Grease Pencil ("Layer 2") → Switch active layer
"""
import modules.ui_test_utils as ui


def _setup(area_type):
    import bpy
    bpy.ops.wm.read_homefile(use_empty=True)
    e, t, w = ui.test_window()
    a = max(w.screen.areas, key=lambda area: area.width * area.height)
    a.type = area_type
    return e, t, w, a


def _invoke(e, t, w, a, menu):
    import bpy
    e.cursor_position_set(*ui.get_area_center(a), move=True)
    region = next(r for r in a.regions if r.type == 'WINDOW')
    with bpy.context.temp_override(window=w, area=a, region=region):
        t.assertTrue(bpy.ops.wm.search_single_menu.poll(), f"Poll failed for '{menu}'")
        bpy.ops.wm.search_single_menu('INVOKE_DEFAULT', menu_idname=menu)


def _search(e, t, w, a, menu, search_text):
    _invoke(e, t, w, a, menu)
    yield e.text(search_text)
    yield e.ret()


def view3d_add():
    import bpy
    e, t, w, a = _setup('VIEW_3D')
    yield

    count_before = len(bpy.data.objects)
    _invoke(e, t, w, a, "VIEW3D_MT_add")
    yield e.text("Monkey")
    yield e.ret()
    yield e.ret()

    t.assertEqual(len(bpy.data.objects), count_before + 1)


def modifier_add():
    import bpy
    e, t, w, a = _setup('PROPERTIES')
    yield

    bpy.ops.mesh.primitive_cube_add()
    cube = bpy.context.view_layer.objects.active
    yield

    a.spaces.active.context = 'MODIFIER'

    _invoke(e, t, w, a, "OBJECT_MT_modifier_add")
    yield e.text("Subdivision Surface")
    yield e.ret()

    t.assertTrue(any(m.type == 'SUBSURF' for m in cube.modifiers))


def sequencer_add():
    import bpy
    e, t, w, a = _setup('SEQUENCE_EDITOR')
    yield

    bpy.context.workspace.sequencer_scene = bpy.context.scene
    yield

    count_before = len(bpy.context.scene.sequence_editor.strips)
    _invoke(e, t, w, a, "SEQUENCER_MT_add")
    yield e.text("Adjustment Layer")
    yield e.ret()
    yield e.ret()

    t.assertEqual(len(bpy.context.scene.sequence_editor.strips), count_before + 1)


def sequencer_modifier_add():
    import bpy
    e, t, w, a = _setup('SEQUENCE_EDITOR')
    yield

    bpy.context.workspace.sequencer_scene = bpy.context.scene
    with bpy.context.temp_override(window=w, area=a):
        bpy.ops.sequencer.scene_strip_add_new()
    strip = bpy.context.scene.sequence_editor.active_strip
    t.assertIsNotNone(strip)
    count_before = len(strip.modifiers)

    _invoke(e, t, w, a, "SEQUENCER_MT_modifier_add")
    yield e.text("Color Balance")
    yield e.ret()

    t.assertEqual(len(strip.modifiers), count_before + 1)


def shader_node_add():
    import bpy
    e, t, w, a = _setup('NODE_EDITOR')
    yield

    mat = bpy.data.materials.new("TestMat")
    bpy.ops.mesh.primitive_cube_add()
    bpy.context.active_object.data.materials.append(mat)
    a.spaces.active.tree_type = 'ShaderNodeTree'
    yield

    count_before = len(mat.node_tree.nodes)
    _invoke(e, t, w, a, "NODE_MT_add")
    yield e.text("Math")
    yield e.ret()
    yield e.ret()

    t.assertEqual(len(mat.node_tree.nodes), count_before + 1)
    t.assertTrue(any(n.type == 'MATH' for n in mat.node_tree.nodes))


def compositor_node_add():
    import bpy
    e, t, w, a = _setup('NODE_EDITOR')
    yield

    tree = bpy.data.node_groups.new("TestCompositor", 'CompositorNodeTree')
    bpy.context.scene.compositing_node_group = tree
    a.spaces.active.tree_type = 'CompositorNodeTree'
    yield

    count_before = len(tree.nodes)
    _invoke(e, t, w, a, "NODE_MT_add")
    yield e.text("RGB Curves")
    yield e.ret()
    yield e.ret()

    t.assertEqual(len(tree.nodes), count_before + 1)
    t.assertTrue(any(n.type == 'CURVE_RGB' for n in tree.nodes))


def geometry_node_add():
    import bpy
    e, t, w, a = _setup('NODE_EDITOR')
    yield

    bpy.ops.mesh.primitive_cube_add()
    t.assertTrue(bpy.ops.node.new_geometry_nodes_modifier.poll(), "Geometry Nodes modifier poll failed")
    bpy.ops.node.new_geometry_nodes_modifier()
    a.spaces.active.tree_type = 'GeometryNodeTree'
    node_tree = bpy.context.active_object.modifiers[-1].node_group
    a.spaces.active.node_tree = node_tree
    yield

    count_before = len(node_tree.nodes)
    _invoke(e, t, w, a, "NODE_MT_add")
    yield e.text("Transform Geometry")
    yield e.ret()
    yield e.ret()

    t.assertEqual(len(node_tree.nodes), count_before + 1)
    t.assertTrue(any(n.bl_idname == 'GeometryNodeTransform' for n in node_tree.nodes))


def node_swap_search():
    import bpy
    e, t, w, a = _setup('NODE_EDITOR')
    yield

    mat = bpy.data.materials.new("TestSwapMat")
    bpy.ops.mesh.primitive_cube_add()
    bpy.context.active_object.data.materials.append(mat)
    a.spaces.active.tree_type = 'ShaderNodeTree'
    node_tree = mat.node_tree
    bsdf = node_tree.nodes.get("Principled BSDF")
    if bsdf is None:
        bsdf = node_tree.nodes.new("ShaderNodeBsdfPrincipled")
    node_tree.nodes.active = bsdf
    yield

    yield from _search(e, t, w, a, "NODE_MT_swap", "Math")

    t.assertEqual(node_tree.nodes.active.type, 'MATH')


def grease_pencil_layer_search():
    import bpy
    e, t, w, a = _setup('PROPERTIES')
    yield

    gp = bpy.data.grease_pencils.new("TestGP")
    gp_obj = bpy.data.objects.new("TestGPObj", gp)
    bpy.context.collection.objects.link(gp_obj)
    bpy.context.view_layer.objects.active = gp_obj
    gp.layers.new(name="Layer 1")
    gp.layers.new(name="Layer 2")
    yield

    yield from _search(e, t, w, a, "GREASE_PENCIL_MT_move_to_layer_SEARCH", "Layer 2")

    t.assertEqual(gp.layers.active.name, "Layer 2")
