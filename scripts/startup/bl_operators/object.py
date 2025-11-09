# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import (
    BoolProperty,
    EnumProperty,
    IntProperty,
    StringProperty,
)
from bpy.app.translations import (
    pgettext_rpt as rpt_,
    contexts as i18n_contexts,
)


class SelectPattern(Operator):
    """Select objects matching a naming pattern"""
    bl_idname = "object.select_pattern"
    bl_label = "Select Pattern"
    bl_options = {'REGISTER', 'UNDO'}
    bl_property = "pattern"

    pattern: StringProperty(
        name="Pattern",
        translation_context=i18n_contexts.id_text,
        description="Name filter using '*', '?' and "
        "'[abc]' unix style wildcards",
        maxlen=256,
        default="*",
    )
    case_sensitive: BoolProperty(
        name="Case Sensitive",
        description="Do a case sensitive compare",
        default=False,
    )
    extend: BoolProperty(
        name="Extend",
        description="Extend the existing selection",
        default=True,
    )

    def execute(self, context):

        import fnmatch

        if self.case_sensitive:
            pattern_match = fnmatch.fnmatchcase
        else:
            pattern_match = (
                lambda a, b:
                fnmatch.fnmatchcase(a.upper(), b.upper())
            )
        is_ebone = False
        is_pbone = False
        obj = context.object
        if obj and obj.mode == 'POSE':
            items = obj.pose.bones
            if not self.extend:
                bpy.ops.pose.select_all(action='DESELECT')
            is_pbone = True
        elif obj and obj.type == 'ARMATURE' and obj.mode == 'EDIT':
            items = obj.data.edit_bones
            if not self.extend:
                bpy.ops.armature.select_all(action='DESELECT')
            is_ebone = True
        else:
            items = context.visible_objects
            if not self.extend:
                bpy.ops.object.select_all(action='DESELECT')

        # Can be pose bones, edit bones or objects
        for item in items:
            if pattern_match(item.name, self.pattern):

                # hrmf, perhaps there should be a utility function for this.
                if is_ebone:
                    item.select = True
                    item.select_head = True
                    item.select_tail = True
                    if item.use_connect:
                        item_parent = item.parent
                        if item_parent is not None:
                            item_parent.select_tail = True
                elif is_pbone:
                    item.select = True
                else:
                    item.select_set(True)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_popup(self, event)

    def draw(self, _context):
        layout = self.layout

        layout.prop(self, "pattern")
        row = layout.row()
        row.prop(self, "case_sensitive")
        row.prop(self, "extend")

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (not obj) or (obj.mode == 'OBJECT') or (obj.type == 'ARMATURE')


class SelectCamera(Operator):
    """Select the active camera"""
    bl_idname = "object.select_camera"
    bl_label = "Select Camera"
    bl_options = {'REGISTER', 'UNDO'}

    extend: BoolProperty(
        name="Extend",
        description="Extend the selection",
        default=False,
    )

    def execute(self, context):
        scene = context.scene
        view_layer = context.view_layer
        view = context.space_data
        if view and view.type == 'VIEW_3D' and view.use_local_camera:
            camera = view.camera
        else:
            camera = scene.camera

        if camera is None:
            self.report({'WARNING'}, "No camera found")
        elif camera.name not in scene.objects:
            self.report({'WARNING'}, "Active camera is not in this scene")
        else:
            if not self.extend:
                bpy.ops.object.select_all(action='DESELECT')
            view_layer.objects.active = camera
            # camera.hide = False  # XXX TODO where is this now?
            camera.select_set(True)
            return {'FINISHED'}

        return {'CANCELLED'}


class SelectHierarchy(Operator):
    """Select object relative to the active object's position """ \
        """in the hierarchy"""
    bl_idname = "object.select_hierarchy"
    bl_label = "Select Hierarchy"
    bl_options = {'REGISTER', 'UNDO'}

    direction: EnumProperty(
        items=(
            ('PARENT', "Parent", ""),
            ('CHILD', "Child", ""),
        ),
        name="Direction",
        description="Direction to select in the hierarchy",
        default='PARENT',
    )
    extend: BoolProperty(
        name="Extend",
        description="Extend the existing selection",
        default=False,
    )

    @classmethod
    def poll(cls, context):
        return context.object

    def execute(self, context):
        view_layer = context.view_layer
        select_new = []
        act_new = None

        selected_objects = context.selected_objects
        obj_act = context.object

        if context.object not in selected_objects:
            selected_objects.append(context.object)

        if self.direction == 'PARENT':
            for obj in selected_objects:
                parent = obj.parent

                if parent and parent.visible_get():
                    if obj_act == obj:
                        act_new = parent

                    select_new.append(parent)

        else:
            for obj in selected_objects:
                select_new.extend([child for child in obj.children if child.visible_get()])

            if select_new:
                select_new.sort(key=lambda obj_iter: obj_iter.name)
                act_new = select_new[0]

        # don't edit any object settings above this
        if select_new:
            if not self.extend:
                bpy.ops.object.select_all(action='DESELECT')

            for obj in select_new:
                obj.select_set(True)

            view_layer.objects.active = act_new
            return {'FINISHED'}

        return {'CANCELLED'}


class SubdivisionSet(Operator):
    """Sets a Subdivision Surface level (1 to 5)"""

    bl_idname = "object.subdivision_set"
    bl_label = "Subdivision Set"
    bl_options = {'REGISTER', 'UNDO'}

    level: IntProperty(
        name="Level",
        min=-100, max=100,
        soft_min=-6, soft_max=6,
        default=1,
    )
    relative: BoolProperty(
        name="Relative",
        description="Apply the subdivision surface level as an offset relative to the current level",
        default=False,
    )
    ensure_modifier: BoolProperty(
        name="Ensure Modifier",
        description="Create the corresponding modifier if it does not exist",
        default=True,
        options={'HIDDEN'}
    )

    @classmethod
    def poll(cls, context):
        obs = context.selected_editable_objects
        return (obs is not None)

    def execute(self, context):
        level = self.level
        relative = self.relative
        ensure_modifier = self.ensure_modifier

        if relative and level == 0:
            return {'CANCELLED'}  # nothing to do

        if not ensure_modifier:
            any_object_has_relevant_modifier = False
            for obj in context.selected_editable_objects:
                if obj.mode == 'SCULPT':
                    any_object_has_relevant_modifier |= any(mod.type == 'MULTIRES' for mod in obj.modifiers)
                elif obj.mode == 'OBJECT':
                    any_object_has_relevant_modifier |= any(mod.type == 'SUBSURF' for mod in obj.modifiers)
                if any_object_has_relevant_modifier:
                    break

            if not any_object_has_relevant_modifier:
                mod_name = ""
                if obj.mode == 'SCULPT':
                    mod_name = "Multiresolution"
                else:
                    mod_name = "Subdivision Surface"
                self.report({'WARNING'}, rpt_("No {:s} modifiers found").format(mod_name))
                return {'CANCELLED'}

        if not relative and level < 0:
            self.level = level = 0

        def set_object_subd(obj):
            for mod in obj.modifiers:
                if mod.type == 'MULTIRES':
                    if not relative:
                        if level > mod.total_levels:
                            sub = level - mod.total_levels
                            for _ in range(sub):
                                bpy.ops.object.multires_subdivide(modifier="Multires")

                        if obj.mode == 'SCULPT':
                            if mod.sculpt_levels != level:
                                mod.sculpt_levels = level
                        elif obj.mode == 'OBJECT':
                            if mod.levels != level:
                                mod.levels = level
                        return
                    else:
                        if obj.mode == 'SCULPT':
                            if mod.sculpt_levels + level <= mod.total_levels:
                                mod.sculpt_levels += level
                        elif obj.mode == 'OBJECT':
                            if mod.levels + level <= mod.total_levels:
                                mod.levels += level
                        return

                elif mod.type == 'SUBSURF':
                    if relative:
                        mod.levels += level
                    else:
                        if mod.levels != level:
                            mod.levels = level

                    return

            # add a new modifier
            if ensure_modifier:
                try:
                    if obj.mode == 'SCULPT':
                        mod = obj.modifiers.new("Multires", 'MULTIRES')
                        if level > 0:
                            for _ in range(level):
                                bpy.ops.object.multires_subdivide(modifier="Multires")
                    else:
                        mod = obj.modifiers.new("Subdivision", 'SUBSURF')
                        mod.levels = level
                except Exception:
                    self.report({'WARNING'}, rpt_("Modifiers cannot be added to object: {:s}").format(obj.name))

        for obj in context.selected_editable_objects:
            set_object_subd(obj)

        return {'FINISHED'}


class ShapeTransfer(Operator):
    """Copy the active shape key of another selected object to this one"""

    bl_idname = "object.shape_key_transfer"
    bl_label = "Transfer Shape Key"
    bl_options = {'REGISTER', 'UNDO'}

    mode: EnumProperty(
        items=(
            ('OFFSET',
             "Offset",
             "Apply the relative positional offset",
             ),
            ('RELATIVE_FACE',
             "Relative Face",
             "Calculate relative position (using faces)",
             ),
            ('RELATIVE_EDGE',
             "Relative Edge",
             "Calculate relative position (using edges)",
             ),
        ),
        name="Transformation Mode",
        description="Relative shape positions to the new shape method",
        default='OFFSET',
    )
    use_clamp: BoolProperty(
        name="Clamp Offset",
        description="Clamp the transformation to the distance each vertex moves in the original shape",
        default=False,
    )

    def _main(self, ob_act, objects, mode='OFFSET', use_clamp=False):

        def me_nos(verts):
            return [v.normal.copy() for v in verts]

        def me_cos(verts):
            return [v.co.copy() for v in verts]

        def ob_add_shape(ob, name):
            me = ob.data
            key = ob.shape_key_add(from_mix=False)
            if len(me.shape_keys.key_blocks) == 1:
                key.name = "Basis"
                key = ob.shape_key_add(from_mix=False)  # we need a rest
            key.value = 0.0
            key.name = name
            ob.active_shape_key_index = len(me.shape_keys.key_blocks) - 1
            ob.show_only_shape_key = True

        from mathutils.geometry import barycentric_transform
        from mathutils import Vector

        if use_clamp and mode == 'OFFSET':
            use_clamp = False

        me = ob_act.data
        orig_key_name = ob_act.active_shape_key.name

        orig_shape_coords = me_cos(ob_act.active_shape_key.data)

        orig_normals = me_nos(me.vertices)
        # actual mesh vertex location isn't as reliable as the base shape :S
        # orig_coords = me_cos(me.vertices)
        orig_coords = me_cos(me.shape_keys.key_blocks[0].data)

        for ob_other in objects:
            if ob_other.type != 'MESH':
                self.report(
                    {'WARNING'},
                    rpt_("Skipping '{:s}', not a mesh").format(ob_other.name),
                )
                continue
            me_other = ob_other.data
            if len(me_other.vertices) != len(me.vertices):
                self.report(
                    {'WARNING'},
                    rpt_("Skipping '{:s}', vertex count differs").format(ob_other.name),
                )
                continue

            target_normals = me_nos(me_other.vertices)
            if me_other.shape_keys:
                target_coords = me_cos(me_other.shape_keys.key_blocks[0].data)
            else:
                target_coords = me_cos(me_other.vertices)

            ob_add_shape(ob_other, orig_key_name)

            # editing the final coords, only list that stores wrapped coords
            target_shape_coords = [v.co for v in ob_other.active_shape_key.data]

            median_coords = [[] for i in range(len(me.vertices))]

            # Method 1, edge
            if mode == 'OFFSET':
                for i, vert_cos in enumerate(median_coords):
                    vert_cos.append(
                        target_coords[i] +
                        (orig_shape_coords[i] - orig_coords[i])
                    )

            elif mode == 'RELATIVE_FACE':
                for poly in me.polygons:
                    idxs = poly.vertices[:]
                    v_before = idxs[-2]
                    v = idxs[-1]
                    for v_after in idxs:
                        pt = barycentric_transform(
                            orig_shape_coords[v],
                            orig_coords[v_before],
                            orig_coords[v],
                            orig_coords[v_after],
                            target_coords[v_before],
                            target_coords[v],
                            target_coords[v_after],
                        )
                        median_coords[v].append(pt)
                        v_before = v
                        v = v_after

            elif mode == 'RELATIVE_EDGE':
                for ed in me.edges:
                    i1, i2 = ed.vertices
                    v1, v2 = orig_coords[i1], orig_coords[i2]
                    edge_length = (v1 - v2).length
                    n1loc = v1 + orig_normals[i1] * edge_length
                    n2loc = v2 + orig_normals[i2] * edge_length

                    # now get the target nloc's
                    v1_to, v2_to = target_coords[i1], target_coords[i2]
                    edlen_to = (v1_to - v2_to).length
                    n1loc_to = v1_to + target_normals[i1] * edlen_to
                    n2loc_to = v2_to + target_normals[i2] * edlen_to

                    pt = barycentric_transform(
                        orig_shape_coords[i1],
                        v2, v1, n1loc,
                        v2_to, v1_to, n1loc_to,
                    )
                    median_coords[i1].append(pt)

                    pt = barycentric_transform(
                        orig_shape_coords[i2],
                        v1, v2, n2loc,
                        v1_to, v2_to, n2loc_to,
                    )
                    median_coords[i2].append(pt)

            # apply the offsets to the new shape
            from functools import reduce
            VectorAdd = Vector.__add__

            for i, vert_cos in enumerate(median_coords):
                if vert_cos:
                    co = reduce(VectorAdd, vert_cos) / len(vert_cos)

                    if use_clamp:
                        # clamp to the same movement as the original
                        # breaks copy between different scaled meshes.
                        len_from = (orig_shape_coords[i] - orig_coords[i]).length
                        ofs = co - target_coords[i]
                        ofs.length = len_from
                        co = target_coords[i] + ofs

                    target_shape_coords[i][:] = co

        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.mode != 'EDIT')

    def execute(self, context):
        ob_act = context.active_object
        objects = [
            ob for ob in context.selected_editable_objects
            if ob != ob_act
        ]

        if 1:  # swap from/to, means we can't copy to many at once.
            if len(objects) != 1:
                self.report({'ERROR'}, "Expected one other selected mesh object to copy from")
                return {'CANCELLED'}
            ob_act, objects = objects[0], [ob_act]

        if ob_act.type != 'MESH':
            self.report({'ERROR'}, "Other object is not a mesh")
            return {'CANCELLED'}

        if ob_act.active_shape_key is None:
            self.report({'ERROR'}, "Other object has no shape key")
            return {'CANCELLED'}
        return self._main(ob_act, objects, self.mode, self.use_clamp)


class JoinUVs(Operator):
    """Transfer UV Maps from active to selected objects """ \
        """(needs matching geometry)"""
    bl_idname = "object.join_uvs"
    bl_label = "Transfer UV Maps"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def _main(self, context):
        import array
        obj = context.active_object
        mesh = obj.data

        is_editmode = (obj.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        if not mesh.uv_layers:
            self.report(
                {'WARNING'},
                rpt_("Object: {:s}, Mesh: '{:s}' has no UVs").format(obj.name, mesh.name),
            )
        else:
            nbr_loops = len(mesh.loops)

            # seems to be the fastest way to create an array
            uv_array = array.array("f", [0.0] * 2) * nbr_loops
            mesh.uv_layers.active.data.foreach_get("uv", uv_array)

            objects = context.selected_editable_objects[:]

            for obj_other in objects:
                if obj_other.type == 'MESH':
                    obj_other.data.tag = False

            for obj_other in objects:
                if not (obj_other != obj and obj_other.type == 'MESH'):
                    continue
                mesh_other = obj_other.data
                if mesh_other == mesh:
                    continue
                if mesh_other.tag is True:
                    continue

                mesh_other.tag = True
                if len(mesh_other.loops) != nbr_loops:
                    self.report(
                        {'WARNING'},
                        rpt_(
                            "Object: {:s}, Mesh: '{:s}' has {:d} loops (for {:d} faces), expected {:d}"
                        ).format(
                            obj_other.name,
                            mesh_other.name,
                            len(mesh_other.loops),
                            len(mesh_other.polygons),
                            nbr_loops,
                        ),
                    )
                else:
                    uv_other = mesh_other.uv_layers.active
                    if not uv_other:
                        mesh_other.uv_layers.new()
                        uv_other = mesh_other.uv_layers.active
                        if not uv_other:
                            self.report(
                                {'ERROR'},
                                rpt_(
                                    "Could not add a new UV map to object '{:s}' (Mesh '{:s}')"
                                ).format(
                                    obj_other.name,
                                    mesh_other.name,
                                ),
                            )

                    # finally do the copy
                    uv_other.data.foreach_set("uv", uv_array)
                    mesh_other.update()

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

    def execute(self, context):
        self._main(context)
        return {'FINISHED'}


class MakeDupliFace(Operator):
    """Convert objects into instanced faces"""
    bl_idname = "object.make_dupli_face"
    bl_label = "Make Instance Face"
    bl_options = {'REGISTER', 'UNDO'}

    @staticmethod
    def _main(context):
        from mathutils import Vector
        from collections import defaultdict

        SCALE_FAC = 0.01
        offset = 0.5 * SCALE_FAC
        base_tri = (
            Vector((-offset, -offset, 0.0)),
            Vector((+offset, -offset, 0.0)),
            Vector((+offset, +offset, 0.0)),
            Vector((-offset, +offset, 0.0)),
        )

        def matrix_to_quad(matrix):
            # scale = matrix.median_scale
            trans = matrix.to_translation()
            rot = matrix.to_3x3()  # also contains scale

            return [(rot @ b) + trans for b in base_tri]
        linked = defaultdict(list)
        for obj in context.selected_objects:
            if obj.type == 'MESH':
                linked[obj.data].append(obj)
            elif obj.type == 'EMPTY' and obj.instance_type == 'COLLECTION' and obj.instance_collection:
                linked[obj.instance_collection].append(obj)

        for data, objects in linked.items():
            face_verts = [
                axis for obj in objects
                for v in matrix_to_quad(obj.matrix_world)
                for axis in v
            ]
            nbr_verts = len(face_verts) // 3
            nbr_faces = nbr_verts // 4

            faces = list(range(nbr_verts))

            mesh = bpy.data.meshes.new(data.name + "_dupli")

            mesh.vertices.add(nbr_verts)
            mesh.loops.add(nbr_faces * 4)  # Safer than nbr_verts.
            mesh.polygons.add(nbr_faces)

            mesh.vertices.foreach_set("co", face_verts)
            mesh.loops.foreach_set("vertex_index", faces)
            mesh.polygons.foreach_set("loop_start", range(0, nbr_faces * 4, 4))
            mesh.update()  # generates edge data

            ob_new = bpy.data.objects.new(mesh.name, mesh)
            context.collection.objects.link(ob_new)

            if type(data) is bpy.types.Collection:
                ob_inst = bpy.data.objects.new(data.name, None)
                ob_inst.instance_type = 'COLLECTION'
                ob_inst.instance_collection = data
            else:
                ob_inst = bpy.data.objects.new(data.name, data)
            context.collection.objects.link(ob_inst)

            ob_new.instance_type = 'FACES'
            ob_inst.parent = ob_new
            ob_new.use_instance_faces_scale = True
            ob_new.instance_faces_scale = 1.0 / SCALE_FAC

            ob_inst.select_set(True)
            ob_new.select_set(True)

            for obj in objects:
                for collection in obj.users_collection:
                    collection.objects.unlink(obj)

    def execute(self, context):
        self._main(context)
        return {'FINISHED'}


class IsolateTypeRender(Operator):
    """Hide unselected render objects of same type as active """ \
        """by setting the hide render flag"""
    bl_idname = "object.isolate_type_render"
    bl_label = "Restrict Render Unselected"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob is not None)

    def execute(self, context):
        act_type = context.object.type

        for obj in context.visible_objects:

            if obj.select_get():
                obj.hide_render = False
            else:
                if obj.type == act_type:
                    obj.hide_render = True

        return {'FINISHED'}


class ClearAllRestrictRender(Operator):
    """Reveal all render objects by setting the hide render flag"""
    bl_idname = "object.hide_render_clear_all"
    bl_label = "Clear All Restrict Render"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        for obj in context.scene.objects:
            obj.hide_render = False
        return {'FINISHED'}


class TransformsToDeltas(Operator):
    """Convert normal object transforms to delta transforms, """ \
        """any existing delta transforms will be included as well"""
    bl_idname = "object.transforms_to_deltas"
    bl_label = "Transforms to Deltas"
    bl_options = {'REGISTER', 'UNDO'}

    mode: EnumProperty(
        items=(
            ('ALL', "All Transforms", "Transfer location, rotation, and scale transforms"),
            ('LOC', "Location", "Transfer location transforms only"),
            ('ROT', "Rotation", "Transfer rotation transforms only"),
            ('SCALE', "Scale", "Transfer scale transforms only"),
        ),
        name="Mode",
        description="Which transforms to transfer",
        default='ALL',
    )
    reset_values: BoolProperty(
        name="Reset Values",
        description=("Clear transform values after transferring to deltas"),
        default=True,
    )

    @classmethod
    def poll(cls, context):
        obs = context.selected_editable_objects
        return (obs is not None)

    def execute(self, context):
        for obj in context.selected_editable_objects:
            if self.mode in {'ALL', 'LOC'}:
                self.transfer_location(obj)

            if self.mode in {'ALL', 'ROT'}:
                self.transfer_rotation(obj)

            if self.mode in {'ALL', 'SCALE'}:
                self.transfer_scale(obj)

        return {'FINISHED'}

    def transfer_location(self, obj):
        obj.delta_location += obj.location

        if self.reset_values:
            obj.location.zero()

    def transfer_rotation(self, obj):
        # TODO: add transforms together...
        if obj.rotation_mode == 'QUATERNION':
            delta = obj.delta_rotation_quaternion.copy()
            obj.delta_rotation_quaternion = obj.rotation_quaternion
            obj.delta_rotation_quaternion.rotate(delta)

            if self.reset_values:
                obj.rotation_quaternion.identity()
        elif obj.rotation_mode == 'AXIS_ANGLE':
            pass  # Unsupported
        else:
            delta = obj.delta_rotation_euler.copy()
            obj.delta_rotation_euler = obj.rotation_euler
            obj.delta_rotation_euler.rotate(delta)

            if self.reset_values:
                obj.rotation_euler.zero()

    def transfer_scale(self, obj):
        obj.delta_scale[0] *= obj.scale[0]
        obj.delta_scale[1] *= obj.scale[1]
        obj.delta_scale[2] *= obj.scale[2]

        if self.reset_values:
            obj.scale[:] = (1, 1, 1)


class TransformsToDeltasAnim(Operator):
    """Convert object animation for normal transforms to delta transforms"""
    bl_idname = "object.anim_transforms_to_deltas"
    bl_label = "Animated Transforms to Deltas"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obs = context.selected_editable_objects
        return (obs is not None)

    def execute(self, context):
        from bpy_extras import anim_utils

        # map from standard transform paths to "new" transform paths
        STANDARD_TO_DELTA_PATHS = {
            "location": "delta_location",
            "rotation_euler": "delta_rotation_euler",
            "rotation_quaternion": "delta_rotation_quaternion",
            # "rotation_axis_angle" : "delta_rotation_axis_angle",
            "scale": "delta_scale",
        }
        DELTA_PATHS = STANDARD_TO_DELTA_PATHS.values()

        # try to apply on each selected object
        for obj in context.selected_editable_objects:
            adt = obj.animation_data
            if (adt is None) or (adt.action is None):
                self.report(
                    {'WARNING'},
                    rpt_("No animation data to convert on object: {!r}").format(obj.name),
                )
                continue

            # first pass over F-Curves: ensure that we don't have conflicting
            # transforms already (e.g. if this was applied already) #29110.
            existingFCurves = {}
            channelbag = anim_utils.action_get_channelbag_for_slot(adt.action, adt.action_slot)
            if not channelbag:
                continue
            for fcu in channelbag.fcurves:
                # get "delta" path - i.e. the final paths which may clash
                path = fcu.data_path
                if path in STANDARD_TO_DELTA_PATHS:
                    # to be converted - conflicts may exist...
                    dpath = STANDARD_TO_DELTA_PATHS[path]
                elif path in DELTA_PATHS:
                    # already delta - check for conflicts...
                    dpath = path
                else:
                    # non-transform - ignore
                    continue

                # a delta path like this for the same index shouldn't
                # exist already, otherwise we've got a conflict
                if dpath in existingFCurves:
                    # ensure that this index hasn't occurred before
                    if fcu.array_index in existingFCurves[dpath]:
                        # conflict
                        self.report(
                            {'ERROR'},
                            rpt_(
                                "Object {!r} already has {!r} F-Curve(s). "
                                "Remove these before trying again"
                            ).format(obj.name, dpath))
                        return {'CANCELLED'}
                    else:
                        # no conflict here
                        existingFCurves[dpath] += [fcu.array_index]
                else:
                    # no conflict yet
                    existingFCurves[dpath] = [fcu.array_index]

            # Move the 'standard' to the 'delta' data paths.
            for fcu in channelbag.fcurves:
                standard_path = fcu.data_path
                array_index = fcu.array_index
                try:
                    delta_path = STANDARD_TO_DELTA_PATHS[standard_path]
                except KeyError:
                    # Not a standard transform path.
                    continue

                # Just change the F-Curve's data path. The array index should remain the same.
                fcu.data_path = delta_path

                # Reset the now-no-longer-animated property to its default value.
                default_array = obj.bl_rna.properties[standard_path].default_array
                property_array = getattr(obj, standard_path)
                property_array[array_index] = default_array[array_index]

        # hack: force animsys flush by changing frame, so that deltas get run
        context.scene.frame_set(context.scene.frame_current)

        return {'FINISHED'}


class DupliOffsetFromCursor(Operator):
    """Set offset used for collection instances based on cursor position"""
    bl_idname = "object.instance_offset_from_cursor"
    bl_label = "Set Offset from Cursor"
    bl_options = {'INTERNAL', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        collection = context.collection

        collection.instance_offset = scene.cursor.location

        return {'FINISHED'}


class DupliOffsetToCursor(Operator):
    """Set cursor position to the offset used for collection instances"""
    bl_idname = "object.instance_offset_to_cursor"
    bl_label = "Set Cursor to Offset"
    bl_options = {'INTERNAL', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        collection = context.collection
        scene.cursor.location = collection.instance_offset
        return {'FINISHED'}


class DupliOffsetFromObject(Operator):
    """Set offset used for collection instances based on the active object position"""
    bl_idname = "object.instance_offset_from_object"
    bl_label = "Set Offset from Object"
    bl_options = {'INTERNAL', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None)

    def execute(self, context):
        ob_eval = context.active_object.evaluated_get(context.view_layer.depsgraph)
        world_loc = ob_eval.matrix_world.to_translation()
        collection = context.collection
        collection.instance_offset = world_loc
        return {'FINISHED'}


class OBJECT_OT_assign_property_defaults(Operator):
    """Assign the current values of custom properties as their defaults, """ \
        """for use as part of the rest pose state in NLA track mixing"""
    bl_idname = "object.assign_property_defaults"
    bl_label = "Assign Custom Property Values as Default"
    bl_options = {'UNDO', 'REGISTER'}

    process_data: BoolProperty(name="Process data properties", default=True)
    process_bones: BoolProperty(name="Process bone properties", default=True)

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.is_editable and obj.mode in {'POSE', 'OBJECT'}

    @staticmethod
    def assign_defaults(obj):
        from rna_prop_ui import rna_idprop_ui_prop_default_set

        rna_properties = {prop.identifier for prop in obj.bl_rna.properties if prop.is_runtime}

        for prop, value in obj.items():
            if prop not in rna_properties:
                rna_idprop_ui_prop_default_set(obj, prop, value)

    def execute(self, context):
        obj = context.active_object

        self.assign_defaults(obj)

        if self.process_bones and obj.pose:
            for pbone in obj.pose.bones:
                self.assign_defaults(pbone)

        if self.process_data and obj.data and obj.data.is_editable:
            self.assign_defaults(obj.data)

            if self.process_bones and isinstance(obj.data, bpy.types.Armature):
                for bone in obj.data.bones:
                    self.assign_defaults(bone)

        return {'FINISHED'}


classes = (
    ClearAllRestrictRender,
    DupliOffsetFromCursor,
    DupliOffsetToCursor,
    DupliOffsetFromObject,
    IsolateTypeRender,
    JoinUVs,
    MakeDupliFace,
    SelectCamera,
    SelectHierarchy,
    SelectPattern,
    ShapeTransfer,
    SubdivisionSet,
    TransformsToDeltas,
    TransformsToDeltasAnim,
    OBJECT_OT_assign_property_defaults,
)
