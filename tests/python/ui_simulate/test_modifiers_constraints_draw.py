# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.

This test:
1. Adds all available modifiers to the objects dynamically.
2. Switches the Properties editor to the Modifiers tab for each object type and redraws.
3. Adds all available constraints to a mesh object, using a bone for bone-only constraints.
4. Switches the Properties editor to the Constraints tab and redraws the panels.
"""

import modules.ui_test_utils as ui


_BONE_ONLY_CONSTRAINTS = {
    "IK",
    "SPLINE_IK",
}


def _activate_object(obj):
    """Make an object active and selected."""
    import bpy

    if bpy.context.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def modifiers_constraints_draw():
    import bpy

    # Setup editor area
    bpy.ops.wm.read_homefile(use_empty=True)
    e, t, window = ui.test_window()
    area = ui.largest_area(window.screen)
    area.type = 'PROPERTIES'
    yield

    space = area.spaces.active
    t.assertIsInstance(space, bpy.types.SpaceProperties, "Area did not switch to Properties editor")

    # Create objects of different types
    objects = {}

    # Mesh
    bpy.ops.mesh.primitive_cube_add()
    objects['MESH'] = bpy.context.active_object

    # Curve
    bpy.ops.curve.primitive_bezier_curve_add()
    objects['CURVE'] = bpy.context.active_object

    # Grease Pencil
    bpy.ops.object.grease_pencil_add()
    objects['GPENCIL'] = bpy.context.active_object

    # Lattice
    lattice_data = bpy.data.lattices.new("Lattice")
    lattice_obj = bpy.data.objects.new("LatticeObj", lattice_data)
    bpy.context.scene.collection.objects.link(lattice_obj)
    objects['LATTICE'] = lattice_obj

    # Volume
    volume_data = bpy.data.volumes.new("Volume")
    volume_obj = bpy.data.objects.new("VolumeObj", volume_data)
    bpy.context.scene.collection.objects.link(volume_obj)
    objects['VOLUME'] = volume_obj

    # Armature
    bpy.ops.object.armature_add()
    armature_obj = bpy.context.active_object
    objects['ARMATURE'] = armature_obj

    bone = armature_obj.pose.bones[0]

    # Ensure the Properties editor updates to object-related contexts.
    mesh_obj = objects['MESH']
    _activate_object(mesh_obj)
    yield

    # Step 1: Add all modifiers to objects dynamically
    modifier_types = [
        item.identifier
        for item in bpy.types.Modifier.bl_rna.properties['type'].enum_items
    ]
    unsupported_modifiers = []

    for mod_type in modifier_types:
        supported = False
        for obj_type, obj in objects.items():
            # Armatures don't support modifiers.
            if obj_type == 'ARMATURE':
                continue
            try:
                obj.modifiers.new(name=mod_type, type=mod_type)
                supported = True
            except (RuntimeError, TypeError):
                pass
        if not supported:
            unsupported_modifiers.append(mod_type)

    t.assertFalse(
        unsupported_modifiers,
        (
            f"{len(unsupported_modifiers)} modifier(s) could not be added "
            + ", ".join(unsupported_modifiers)
        )
    )

    # Step 2: Draw all modifiers
    _activate_object(mesh_obj)
    yield
    space.context = 'MODIFIER'
    yield

    # Draw modifier stack for each object type
    for obj_type, obj in objects.items():
        if obj_type == 'ARMATURE':
            continue
        _activate_object(obj)
        yield
        area.tag_redraw()
        yield

    # Step 3: Add all constraints
    constraint_types = [item.identifier for item in bpy.types.Constraint.bl_rna.properties['type'].enum_items]
    unsupported_constraints = []
    for const_type in constraint_types:
        if const_type in _BONE_ONLY_CONSTRAINTS:
            try:
                bone.constraints.new(type=const_type)
            except (RuntimeError, TypeError):
                unsupported_constraints.append(const_type)
            continue

        try:
            mesh_obj.constraints.new(type=const_type)
        except (RuntimeError, TypeError):
            try:
                bone.constraints.new(type=const_type)
            except (RuntimeError, TypeError):
                unsupported_constraints.append(const_type)

    t.assertFalse(
        unsupported_constraints,
        (
            f"{len(unsupported_constraints)} constraint(s) could not be added: "
            + ", ".join(unsupported_constraints)
        )
    )

    # Step 4: Draw all modifier and constraint panels.
    # The test passes if the redraw completes without Python exceptions.
    # Any draw() errors will be reported by the ui_simulate event loop.
    _activate_object(mesh_obj)
    yield
    space.context = 'CONSTRAINT'
    yield

    # Draw object constraints (on mesh object)
    area.tag_redraw()
    yield

    # Draw bone constraints
    _activate_object(armature_obj)
    bpy.ops.object.mode_set(mode='POSE')
    yield
    space.context = 'BONE_CONSTRAINT'
    yield
    area.tag_redraw()
    yield
