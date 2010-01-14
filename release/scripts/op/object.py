# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
from bpy.props import *


class SelectPattern(bpy.types.Operator):
    '''Select object matching a naming pattern.'''
    bl_idname = "object.select_pattern"
    bl_label = "Select Pattern"
    bl_register = True
    bl_undo = True

    pattern = StringProperty(name="Pattern", description="Name filter using '*' and '?' wildcard chars", maxlen=32, default="*")
    case_sensitive = BoolProperty(name="Case Sensitive", description="Do a case sensitive compare", default=False)
    extend = BoolProperty(name="Extend", description="Extend the existing selection", default=True)

    def execute(self, context):

        import fnmatch

        if self.properties.case_sensitive:
            pattern_match = fnmatch.fnmatchcase
        else:
            pattern_match = lambda a, b: fnmatch.fnmatchcase(a.upper(), b.upper())

        obj = context.object
        if obj and obj.mode == 'POSE':
            items = obj.data.bones
        elif obj and obj.type == 'ARMATURE' and obj.mode == 'EDIT':
            items = obj.data.edit_bones
        else:
            items = context.visible_objects

        # Can be pose bones or objects
        for item in items:
            if pattern_match(item.name, self.properties.pattern):
                item.selected = True
            elif not self.properties.extend:
                item.selected = False

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        # return wm.invoke_props_popup(self, event)
        wm.invoke_props_popup(self, event)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        layout = self.layout
        props = self.properties

        layout.prop(props, "pattern")
        row = layout.row()
        row.prop(props, "case_sensitive")
        row.prop(props, "extend")


class SubdivisionSet(bpy.types.Operator):
    '''Sets a Subdivision Surface Level (1-5)'''

    bl_idname = "object.subdivision_set"
    bl_label = "Subdivision Set"
    bl_register = True
    bl_undo = True

    level = IntProperty(name="Level",
            default=1, min=-100, max=100, soft_min=-6, soft_max=6)

    relative = BoolProperty(name="Relative", description="Apply the subsurf level as an offset relative to the current level", default=False)

    def poll(self, context):
        obs = context.selected_editable_objects
        return (obs is not None)

    def execute(self, context):
        level = self.properties.level
        relative = self.properties.relative

        if relative and level == 0:
            return {'CANCELLED'} # nothing to do

        def set_object_subd(obj):
            for mod in obj.modifiers:
                if mod.type == 'MULTIRES':
                    if level <= mod.total_levels:
                        if obj.mode == 'SCULPT':
                            if relative:
                                mod.sculpt_levels += level
                            else:
                                if mod.sculpt_levels != level:
                                    mod.sculpt_levels = level
                        elif obj.mode == 'OBJECT':
                            if relative:
                                mod.levels += level
                            else:
                                if mod.levels != level:
                                    mod.levels = level
                    return
                elif mod.type == 'SUBSURF':
                    if relative:
                        mod.levels += level
                    else:
                        if mod.levels != level:
                            mod.levels = level

                    return

            # adda new modifier
            mod = obj.modifiers.new("Subsurf", 'SUBSURF')
            mod.levels = level

        for obj in context.selected_editable_objects:
            set_object_subd(obj)

        return {'FINISHED'}


class Retopo(bpy.types.Operator):
    '''TODO - doc'''

    bl_idname = "object.retopology"
    bl_label = "Retopology from Grease Pencil"
    bl_register = True
    bl_undo = True

    def execute(self, context):
        import retopo
        reload(retopo)
        retopo.main()
        return {'FINISHED'}


class ShapeTransfer(bpy.types.Operator):
    '''Copy the active objects current shape to other selected objects with the same number of verts'''

    bl_idname = "object.shape_key_transfer"
    bl_label = "Transfer Shape Key"
    bl_register = True
    bl_undo = True

    mode = EnumProperty(items=(
                        ('OFFSET', "Offset", "Apply the relative positional offset"),
                        ('RELATIVE_FACE', "Relative Face", "Calculate the geometricly relative position (using faces)."),
                        ('RELATIVE_EDGE', "Relative Edge", "Calculate the geometricly relative position (using edges).")),
                name="Transformation Mode",
                description="Method to apply relative shape positions to the new shape",
                default='OFFSET')

    use_clamp = BoolProperty(name="Clamp Offset",
                description="Clamp the transformation to the distance each vertex moves in the original shape.",
                default=False)

    def _main(self, ob_act, objects, mode='OFFSET', use_clamp=False):
        def me_nos(verts):
            return [v.normal.copy() for v in verts]

        def me_cos(verts):
            return [v.co.copy() for v in verts]

        def ob_add_shape(ob, name):
            me = ob.data
            key = ob.add_shape_key(from_mix=False)
            if len(me.shape_keys.keys) == 1:
                key = ob.add_shape_key(from_mix=False) # we need a rest
            key.name = name
            ob.active_shape_key_index = len(me.shape_keys.keys) - 1
            ob.shape_key_lock = True

        from Geometry import BarycentricTransform
        from Mathutils import Vector

        if use_clamp and mode == 'OFFSET':
            use_clamp = False

        me = ob_act.data
        orig_key_name = ob_act.active_shape_key.name

        orig_shape_coords = me_cos(ob_act.active_shape_key.data)

        orig_normals = me_nos(me.verts)
        orig_coords = me_cos(me.verts)

        for ob_other in objects:
            me_other = ob_other.data
            if len(me_other.verts) != len(me.verts):
                self.report({'WARNING'}, "Skipping '%s', vertex count differs" % ob_other.name)
                continue

            target_normals = me_nos(me_other.verts)
            target_coords = me_cos(me_other.verts)

            ob_add_shape(ob_other, orig_key_name)

            # editing the final coords, only list that stores wrapped coords
            target_shape_coords = [v.co for v in ob_other.active_shape_key.data]

            median_coords = [[] for i in range(len(me.verts))]

            # Method 1, edge
            if mode == 'OFFSET':
                for i, vert_cos in enumerate(median_coords):
                    vert_cos.append(target_coords[i] + (orig_shape_coords[i] - orig_coords[i]))

            elif mode == 'RELATIVE_FACE':
                for face in me.faces:
                    i1, i2, i3, i4 = face.verts_raw
                    if i4 != 0:
                        pt = BarycentricTransform(orig_shape_coords[i1],
                            orig_coords[i4], orig_coords[i1], orig_coords[i2],
                            target_coords[i4], target_coords[i1], target_coords[i2])
                        median_coords[i1].append(pt)

                        pt = BarycentricTransform(orig_shape_coords[i2],
                            orig_coords[i1], orig_coords[i2], orig_coords[i3],
                            target_coords[i1], target_coords[i2], target_coords[i3])
                        median_coords[i2].append(pt)

                        pt = BarycentricTransform(orig_shape_coords[i3],
                            orig_coords[i2], orig_coords[i3], orig_coords[i4],
                            target_coords[i2], target_coords[i3], target_coords[i4])
                        median_coords[i3].append(pt)

                        pt = BarycentricTransform(orig_shape_coords[i4],
                            orig_coords[i3], orig_coords[i4], orig_coords[i1],
                            target_coords[i3], target_coords[i4], target_coords[i1])
                        median_coords[i4].append(pt)

                    else:
                        pt = BarycentricTransform(orig_shape_coords[i1],
                            orig_coords[i3], orig_coords[i1], orig_coords[i2],
                            target_coords[i3], target_coords[i1], target_coords[i2])
                        median_coords[i1].append(pt)

                        pt = BarycentricTransform(orig_shape_coords[i2],
                            orig_coords[i1], orig_coords[i2], orig_coords[i3],
                            target_coords[i1], target_coords[i2], target_coords[i3])
                        median_coords[i2].append(pt)

                        pt = BarycentricTransform(orig_shape_coords[i3],
                            orig_coords[i2], orig_coords[i3], orig_coords[i1],
                            target_coords[i2], target_coords[i3], target_coords[i1])
                        median_coords[i3].append(pt)

            elif mode == 'RELATIVE_EDGE':
                for ed in me.edges:
                    i1, i2 = ed.verts
                    v1, v2 = orig_coords[i1], orig_coords[i2]
                    edge_length = (v1 - v2).length
                    n1loc = v1 + orig_normals[i1] * edge_length
                    n2loc = v2 + orig_normals[i2] * edge_length


                    # now get the target nloc's
                    v1_to, v2_to = target_coords[i1], target_coords[i2]
                    edlen_to = (v1_to - v2_to).length
                    n1loc_to = v1_to + target_normals[i1] * edlen_to
                    n2loc_to = v2_to + target_normals[i2] * edlen_to

                    pt = BarycentricTransform(orig_shape_coords[i1],
                        v2, v1, n1loc,
                        v2_to, v1_to, n1loc_to)
                    median_coords[i1].append(pt)

                    pt = BarycentricTransform(orig_shape_coords[i2],
                        v1, v2, n2loc,
                        v1_to, v2_to, n2loc_to)
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

    def poll(self, context):
        obj = context.active_object
        return (obj and obj.mode != 'EDIT')

    def execute(self, context):
        C = bpy.context
        ob_act = C.active_object
        if ob_act.active_shape_key is None:
            self.report({'ERROR'}, "Active object has no shape key")
            return {'CANCELLED'}
        objects = [ob for ob in C.selected_editable_objects if ob != ob_act]
        return self._main(ob_act, objects, self.properties.mode, self.properties.use_clamp)


class JoinUVs(bpy.types.Operator):
    '''Copy UV Layout to objects with matching geometry'''
    bl_idname = "object.join_uvs"
    bl_label = "Join as UVs"

    def poll(self, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def _main(self, context):
        import array
        obj = context.active_object
        mesh = obj.data

        is_editmode = (obj.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        len_faces = len(mesh.faces)
        
        uv_array = array.array('f', [0.0] * 8) * len_faces # seems to be the fastest way to create an array
        mesh.active_uv_texture.data.foreach_get("uv_raw", uv_array)

        objects = context.selected_editable_objects[:]
        
        for obj_other in objects:
            if obj_other.type == 'MESH':
                obj_other.data.tag = False

        for obj_other in objects:
            if obj_other != obj and obj_other.type == 'MESH':
                mesh_other = obj_other.data
                if mesh_other != mesh:
                    if mesh_other.tag == False:
                        mesh_other.tag = True
                        
                        if len(mesh_other.faces) != len_faces:
                            self.report({'WARNING'}, "Object: %s, Mesh: '%s' has %d faces, expected %d\n" % (obj_other.name, mesh_other.name, len(mesh_other.faces), len_faces))
                        else:
                            uv_other = mesh_other.active_uv_texture
                            if not uv_other:
                                mesh_other.uv_texture_add() # should return the texture it adds
                                uv_other = mesh_other.active_uv_texture
                            
                            # finally do the copy
                            uv_other.data.foreach_set("uv_raw", uv_array)

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

    def execute(self, context):
        self._main(context)
        return {'FINISHED'}

if __name__ == "__main__":
    bpy.ops.uv.simple_operator()


bpy.types.register(SelectPattern)
bpy.types.register(SubdivisionSet)
bpy.types.register(Retopo)
bpy.types.register(ShapeTransfer)
bpy.types.register(JoinUVs)
