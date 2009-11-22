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

narrowui = 180


class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "modifier"


class DATA_PT_modifiers(DataButtonsPanel):
    bl_label = "Modifiers"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.item_menu_enumO("object.modifier_add", "type")
        if wide_ui:
            row.itemL()

        for md in ob.modifiers:
            box = layout.template_modifier(md)
            if box:
                # match enum type to our functions, avoids a lookup table.
                getattr(self, md.type)(box, ob, md, wide_ui)

    # the mt.type enum is (ab)used for a lookup on function names
    # ...to avoid lengthy if statements
    # so each type must have a function here.

    def ARMATURE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Object:")
        col.itemR(md, "object", text="")

        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group::")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")
        sub = col.column()
        sub.active = md.vertex_group
        sub.itemR(md, "invert")

        split = layout.split()

        col = split.column()
        col.itemL(text="Bind To:")
        col.itemR(md, "use_vertex_groups", text="Vertex Groups")
        col.itemR(md, "use_bone_envelopes", text="Bone Envelopes")

        if wide_ui:
            col = split.column()
        col.itemL(text="Deformation:")
        col.itemR(md, "quaternion")
        col.itemR(md, "multi_modifier")

    def ARRAY(self, layout, ob, md, wide_ui):
        if wide_ui:
            layout.itemR(md, "fit_type")
        else:
            layout.itemR(md, "fit_type", text="")


        if md.fit_type == 'FIXED_COUNT':
            layout.itemR(md, "count")
        elif md.fit_type == 'FIT_LENGTH':
            layout.itemR(md, "length")
        elif md.fit_type == 'FIT_CURVE':
            layout.itemR(md, "curve")

        layout.itemS()

        split = layout.split()

        col = split.column()
        col.itemR(md, "constant_offset")
        sub = col.column()
        sub.active = md.constant_offset
        sub.itemR(md, "constant_offset_displacement", text="")

        col.itemS()

        col.itemR(md, "merge_adjacent_vertices", text="Merge")
        sub = col.column()
        sub.active = md.merge_adjacent_vertices
        sub.itemR(md, "merge_end_vertices", text="First Last")
        sub.itemR(md, "merge_distance", text="Distance")

        if wide_ui:
            col = split.column()
        col.itemR(md, "relative_offset")
        sub = col.column()
        sub.active = md.relative_offset
        sub.itemR(md, "relative_offset_displacement", text="")

        col.itemS()

        col.itemR(md, "add_offset_object")
        sub = col.column()
        sub.active = md.add_offset_object
        sub.itemR(md, "offset_object", text="")

        layout.itemS()

        col = layout.column()
        col.itemR(md, "start_cap")
        col.itemR(md, "end_cap")

    def BEVEL(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemR(md, "width")

        if wide_ui:
            col = split.column()
        col.itemR(md, "only_vertices")

        layout.itemL(text="Limit Method:")
        layout.row().itemR(md, "limit_method", expand=True)
        if md.limit_method == 'ANGLE':
            layout.itemR(md, "angle")
        elif md.limit_method == 'WEIGHT':
            layout.row().itemR(md, "edge_weight_method", expand=True)

    def BOOLEAN(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Operation:")
        col.itemR(md, "operation", text="")

        if wide_ui:
            col = split.column()
        col.itemL(text="Object:")
        col.itemR(md, "object", text="")

    def BUILD(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemR(md, "start")
        col.itemR(md, "length")

        if wide_ui:
            col = split.column()
        col.itemR(md, "randomize")
        sub = col.column()
        sub.active = md.randomize
        sub.itemR(md, "seed")

    def CAST(self, layout, ob, md, wide_ui):
        split = layout.split(percentage=0.25)

        if wide_ui:
            split.itemL(text="Cast Type:")
            split.itemR(md, "cast_type", text="")
        else:
            layout.itemR(md, "cast_type", text="")

        split = layout.split(percentage=0.25)

        col = split.column()
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "z")

        col = split.column()
        col.itemR(md, "factor")
        col.itemR(md, "radius")
        col.itemR(md, "size")
        col.itemR(md, "from_radius")

        split = layout.split()

        col = split.column()
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")
        if wide_ui:
            col = split.column()
        col.itemL(text="Control Object:")
        col.itemR(md, "object", text="")
        if md.object:
            col.itemR(md, "use_transform")

    def CLOTH(self, layout, ob, md, wide_ui):
        layout.itemL(text="See Cloth panel.")

    def COLLISION(self, layout, ob, md, wide_ui):
        layout.itemL(text="See Collision panel.")

    def CURVE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Object:")
        col.itemR(md, "object", text="")
        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")
        layout.itemL(text="Deformation Axis:")
        layout.row().itemR(md, "deform_axis", expand=True)

    def DECIMATE(self, layout, ob, md, wide_ui):
        layout.itemR(md, "ratio")
        layout.itemR(md, "face_count")

    def DISPLACE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Texture:")
        col.itemR(md, "texture", text="")
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

        if wide_ui:
            col = split.column()
        col.itemL(text="Direction:")
        col.itemR(md, "direction", text="")
        col.itemL(text="Texture Coordinates:")
        col.itemR(md, "texture_coordinates", text="")
        if md.texture_coordinates == 'OBJECT':
            layout.itemR(md, "texture_coordinate_object", text="Object")
        elif md.texture_coordinates == 'UV' and ob.type == 'MESH':
            layout.item_pointerR(md, "uv_layer", ob.data, "uv_textures")

        layout.itemS()

        split = layout.split()

        col = split.column()
        col.itemR(md, "midlevel")

        if wide_ui:
            col = split.column()
        col.itemR(md, "strength")

    def EDGE_SPLIT(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemR(md, "use_edge_angle", text="Edge Angle")
        sub = col.column()
        sub.active = md.use_edge_angle
        sub.itemR(md, "split_angle")

        if wide_ui:
            col = split.column()
        col.itemR(md, "use_sharp", text="Sharp Edges")

    def EXPLODE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Vertex group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")
        sub = col.column()
        sub.active = md.vertex_group
        sub.itemR(md, "protect")

        if wide_ui:
            col = split.column()
        col.itemR(md, "split_edges")
        col.itemR(md, "unborn")
        col.itemR(md, "alive")
        col.itemR(md, "dead")

        layout.itemO("object.explode_refresh", text="Refresh")

    def FLUID_SIMULATION(self, layout, ob, md, wide_ui):
        layout.itemL(text="See Fluid panel.")

    def HOOK(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Object:")
        col.itemR(md, "object", text="")
        if md.object and md.object.type == 'ARMATURE':
            col.itemL(text="Bone:")
            col.item_pointerR(md, "subtarget", md.object.data, "bones", text="")
        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

        layout.itemS()

        split = layout.split()

        col = split.column()
        col.itemR(md, "falloff")
        col.itemR(md, "force", slider=True)
        if wide_ui:
            col = split.column()
        else:
            col.itemS()
        col.itemO("object.hook_reset", text="Reset")
        col.itemO("object.hook_recenter", text="Recenter")

        if ob.mode == 'EDIT':
            layout.itemS()
            row = layout.row()
            row.itemO("object.hook_select", text="Select")
            row.itemO("object.hook_assign", text="Assign")

    def LATTICE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Object:")
        col.itemR(md, "object", text="")

        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

    def MASK(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Mode:")
        col.itemR(md, "mode", text="")
        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group:")
        if md.mode == 'ARMATURE':
            col.itemR(md, "armature", text="")
        elif md.mode == 'VERTEX_GROUP':
            col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

        sub = col.column()
        sub.active = md.vertex_group
        sub.itemR(md, "invert")

    def MESH_DEFORM(self, layout, ob, md, wide_ui):
        split = layout.split()
        col = split.column()
        col.itemL(text="Object:")
        col.itemR(md, "object", text="")
        if md.object and md.object.type == 'ARMATURE':
            col.itemL(text="Bone:")
            col.item_pointerR(md, "subtarget", md.object.data, "bones", text="")
        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

        sub = col.column()
        sub.active = md.vertex_group
        sub.itemR(md, "invert")

        layout.itemS()

        if md.is_bound:
            layout.itemO("object.meshdeform_bind", text="Unbind")
        else:
            layout.itemO("object.meshdeform_bind", text="Bind")
            split = layout.split()

            col = split.column()
            col.itemR(md, "precision")

            if wide_ui:
                col = split.column()
            col.itemR(md, "dynamic")

    def MIRROR(self, layout, ob, md, wide_ui):
        layout.itemR(md, "merge_limit")
        if wide_ui:
            split = layout.split(percentage=0.25)
        else:
            split = layout.split(percentage=0.4)

        col = split.column()
        col.itemL(text="Axis:")
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "z")

        if wide_ui:
            col = split.column()
        else:
            subsplit = layout.split()
            col = subsplit.column()
        col.itemL(text="Options:")
        col.itemR(md, "clip", text="Clipping")
        col.itemR(md, "mirror_vertex_groups", text="Vertex Groups")

        col = split.column()
        col.itemL(text="Textures:")
        col.itemR(md, "mirror_u", text="U")
        col.itemR(md, "mirror_v", text="V")

        col = layout.column()
        col.itemL(text="Mirror Object:")
        col.itemR(md, "mirror_object", text="")

    def MULTIRES(self, layout, ob, md, wide_ui):
        if wide_ui:
            layout.row().itemR(md, "subdivision_type", expand=True)
        else:
            layout.row().itemR(md, "subdivision_type", text="")
        layout.itemR(md, "level")

        split = layout.split()

        col = split.column()
        col.itemO("object.multires_subdivide", text="Subdivide")

        if wide_ui:
            col = split.column()
        col.itemO("object.multires_higher_levels_delete", text="Delete Higher")

    def PARTICLE_INSTANCE(self, layout, ob, md, wide_ui):
        layout.itemR(md, "object")
        layout.itemR(md, "particle_system_number", text="Particle System")

        split = layout.split()
        col = split.column()
        col.itemL(text="Create From:")
        col.itemR(md, "normal")
        col.itemR(md, "children")
        col.itemR(md, "size")

        if wide_ui:
            col = split.column()
        col.itemL(text="Show Particles When:")
        col.itemR(md, "alive")
        col.itemR(md, "unborn")
        col.itemR(md, "dead")

        layout.itemS()

        layout.itemR(md, "path", text="Create Along Paths")

        split = layout.split()
        split.active = md.path
        col = split.column()
        col.row().itemR(md, "axis", expand=True)
        col.itemR(md, "keep_shape")

        if wide_ui:
            col = split.column()
        col.itemR(md, "position", slider=True)
        col.itemR(md, "random_position", text="Random", slider=True)

    def PARTICLE_SYSTEM(self, layout, ob, md, wide_ui):
        layout.itemL(text="See Particle panel.")

    def SHRINKWRAP(self, layout, ob, md, wide_ui):
        split = layout.split()
        col = split.column()
        col.itemL(text="Target:")
        col.itemR(md, "target", text="")
        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

        split = layout.split()

        col = split.column()
        col.itemR(md, "offset")
        col.itemR(md, "subsurf_levels")

        if wide_ui:
            col = split.column()
            col.itemL(text="Mode:")
        col.itemR(md, "mode", text="")

        if wide_ui:
            split = layout.split(percentage=0.25)
        else:
            split = layout.split(percentage=0.35)
        col = split.column()

        if md.mode == 'PROJECT':
            col.itemL(text="Axis:")
            col.itemR(md, "x")
            col.itemR(md, "y")
            col.itemR(md, "z")

            col = split.column()
            col.itemL(text="Direction:")
            col.itemR(md, "negative")
            col.itemR(md, "positive")

            if wide_ui:
                col = split.column()
            else:
                subsplit = layout.split()
                col = subsplit.column()
            col.itemL(text="Cull Faces:")
            col.itemR(md, "cull_front_faces", text="Front")
            col.itemR(md, "cull_back_faces", text="Back")

            layout.itemL(text="Auxiliary Target:")
            layout.itemR(md, "auxiliary_target", text="")

        elif md.mode == 'NEAREST_SURFACEPOINT':
            layout.itemR(md, "keep_above_surface")

    def SIMPLE_DEFORM(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Mode:")
        col.itemR(md, "mode", text="")

        if wide_ui:
            col = split.column()
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

        split = layout.split()

        col = split.column()
        col.itemL(text="Origin:")
        col.itemR(md, "origin", text="")
        sub = col.column()
        sub.active = md.origin
        sub.itemR(md, "relative")

        if wide_ui:
            col = split.column()
        col.itemL(text="Deform:")
        col.itemR(md, "factor")
        col.itemR(md, "limits", slider=True)
        if md.mode in ('TAPER', 'STRETCH'):
            col.itemR(md, "lock_x_axis")
            col.itemR(md, "lock_y_axis")

    def SMOKE(self, layout, ob, md, wide_ui):
        layout.itemL(text="See Smoke panel.")

    def SMOOTH(self, layout, ob, md, wide_ui):
        split = layout.split(percentage=0.25)

        col = split.column()
        col.itemL(text="Axis:")
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "z")

        col = split.column()
        col.itemR(md, "factor")
        col.itemR(md, "repeat")
        col.itemL(text="Vertex Group:")
        col.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")

    def SOFT_BODY(self, layout, ob, md, wide_ui):
        layout.itemL(text="See Soft Body panel.")

    def SUBSURF(self, layout, ob, md, wide_ui):
        if wide_ui:
            layout.row().itemR(md, "subdivision_type", expand=True)
        else:
            layout.row().itemR(md, "subdivision_type", text="")

        split = layout.split()
        col = split.column()
        col.itemL(text="Subdivisions:")
        col.itemR(md, "levels", text="View")
        col.itemR(md, "render_levels", text="Render")

        if wide_ui:
            col = split.column()
        col.itemL(text="Options:")
        col.itemR(md, "optimal_draw", text="Optimal Display")
        col.itemR(md, "subsurf_uv")

    def SURFACE(self, layout, ob, md, wide_ui):
        layout.itemL(text="See Fields panel.")

    def UV_PROJECT(self, layout, ob, md, wide_ui):
        if ob.type == 'MESH':
            split = layout.split()
            col = split.column()
            col.itemL(text="UV Layer:")
            col.item_pointerR(md, "uv_layer", ob.data, "uv_textures", text="")

            if wide_ui:
                col = split.column()
            col.itemL(text="Image:")
            col.itemR(md, "image", text="")

            split = layout.split()
            col = split.column()
            col.itemR(md, "override_image")
            col.itemR(md, "num_projectors", text="Projectors")
            for proj in md.projectors:
                col.itemR(proj, "object", text="")

            if wide_ui:
                col = split.column()
            sub = col.column(align=True)
            sub.itemL(text="Aspect Ratio:")
            sub.itemR(md, "horizontal_aspect_ratio", text="Horizontal")
            sub.itemR(md, "vertical_aspect_ratio", text="Vertical")

    def WAVE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemL(text="Motion:")
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "cyclic")

        if wide_ui:
            col = split.column()
        col.itemR(md, "normals")
        sub = col.column()
        sub.active = md.normals
        sub.itemR(md, "x_normal", text="X")
        sub.itemR(md, "y_normal", text="Y")
        sub.itemR(md, "z_normal", text="Z")

        split = layout.split()

        col = split.column()
        col.itemL(text="Time:")
        sub = col.column(align=True)
        sub.itemR(md, "time_offset", text="Offset")
        sub.itemR(md, "lifetime", text="Life")
        col.itemR(md, "damping_time", text="Damping")

        if wide_ui:
            col = split.column()
        col.itemL(text="Position:")
        sub = col.column(align=True)
        sub.itemR(md, "start_position_x", text="X")
        sub.itemR(md, "start_position_y", text="Y")
        col.itemR(md, "falloff_radius", text="Falloff")

        layout.itemS()

        layout.itemR(md, "start_position_object")
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        layout.itemR(md, "texture")
        layout.itemR(md, "texture_coordinates")
        if md.texture_coordinates == 'MAP_UV' and ob.type == 'MESH':
            layout.item_pointerR(md, "uv_layer", ob.data, "uv_textures")
        elif md.texture_coordinates == 'OBJECT':
            layout.itemR(md, "texture_coordinates_object")

        layout.itemS()

        split = layout.split()

        col = split.column()
        col.itemR(md, "speed", slider=True)
        col.itemR(md, "height", slider=True)

        if wide_ui:
            col = split.column()
        col.itemR(md, "width", slider=True)
        col.itemR(md, "narrowness", slider=True)

bpy.types.register(DATA_PT_modifiers)
