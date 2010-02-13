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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180
narrowmod = 260


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
        compact_mod = context.region.width < narrowmod

        layout.operator_menu_enum("object.modifier_add", "type")

        for md in ob.modifiers:
            box = layout.template_modifier(md, compact=compact_mod)
            if box:
                # match enum type to our functions, avoids a lookup table.
                getattr(self, md.type)(box, ob, md, wide_ui)

    # the mt.type enum is (ab)used for a lookup on function names
    # ...to avoid lengthy if statements
    # so each type must have a function here.

    def ARMATURE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Vertex Group::")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")
        sub = col.column()
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert")

        split = layout.split()

        col = split.column()
        col.label(text="Bind To:")
        col.prop(md, "use_vertex_groups", text="Vertex Groups")
        col.prop(md, "use_bone_envelopes", text="Bone Envelopes")

        if wide_ui:
            col = split.column()
        col.label(text="Deformation:")
        col.prop(md, "quaternion")
        col.prop(md, "multi_modifier")

    def ARRAY(self, layout, ob, md, wide_ui):
        if wide_ui:
            layout.prop(md, "fit_type")
        else:
            layout.prop(md, "fit_type", text="")


        if md.fit_type == 'FIXED_COUNT':
            layout.prop(md, "count")
        elif md.fit_type == 'FIT_LENGTH':
            layout.prop(md, "length")
        elif md.fit_type == 'FIT_CURVE':
            layout.prop(md, "curve")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(md, "constant_offset")
        sub = col.column()
        sub.active = md.constant_offset
        sub.prop(md, "constant_offset_displacement", text="")

        col.separator()

        col.prop(md, "merge_adjacent_vertices", text="Merge")
        sub = col.column()
        sub.active = md.merge_adjacent_vertices
        sub.prop(md, "merge_end_vertices", text="First Last")
        sub.prop(md, "merge_distance", text="Distance")

        if wide_ui:
            col = split.column()
        col.prop(md, "relative_offset")
        sub = col.column()
        sub.active = md.relative_offset
        sub.prop(md, "relative_offset_displacement", text="")

        col.separator()

        col.prop(md, "add_offset_object")
        sub = col.column()
        sub.active = md.add_offset_object
        sub.prop(md, "offset_object", text="")

        layout.separator()

        col = layout.column()
        col.prop(md, "start_cap")
        col.prop(md, "end_cap")

    def BEVEL(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.prop(md, "width")

        if wide_ui:
            col = split.column()
        col.prop(md, "only_vertices")

        layout.label(text="Limit Method:")
        layout.row().prop(md, "limit_method", expand=True)
        if md.limit_method == 'ANGLE':
            layout.prop(md, "angle")
        elif md.limit_method == 'WEIGHT':
            layout.row().prop(md, "edge_weight_method", expand=True)

    def BOOLEAN(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Operation:")
        col.prop(md, "operation", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")

    def BUILD(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.prop(md, "start")
        col.prop(md, "length")

        if wide_ui:
            col = split.column()
        col.prop(md, "randomize")
        sub = col.column()
        sub.active = md.randomize
        sub.prop(md, "seed")

    def CAST(self, layout, ob, md, wide_ui):
        split = layout.split(percentage=0.25)

        if wide_ui:
            split.label(text="Cast Type:")
            split.prop(md, "cast_type", text="")
        else:
            layout.prop(md, "cast_type", text="")

        split = layout.split(percentage=0.25)

        col = split.column()
        col.prop(md, "x")
        col.prop(md, "y")
        col.prop(md, "z")

        col = split.column()
        col.prop(md, "factor")
        col.prop(md, "radius")
        col.prop(md, "size")
        col.prop(md, "from_radius")

        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")
        if wide_ui:
            col = split.column()
        col.label(text="Control Object:")
        col.prop(md, "object", text="")
        if md.object:
            col.prop(md, "use_transform")

    def CLOTH(self, layout, ob, md, wide_ui):
        layout.label(text="See Cloth panel.")

    def COLLISION(self, layout, ob, md, wide_ui):
        layout.label(text="See Collision panel.")

    def CURVE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        if wide_ui:
            col = split.column()
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")
        layout.label(text="Deformation Axis:")
        layout.row().prop(md, "deform_axis", expand=True)

    def DECIMATE(self, layout, ob, md, wide_ui):
        layout.prop(md, "ratio")
        layout.prop(md, "face_count")

    def DISPLACE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Texture:")
        col.prop(md, "texture", text="")
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Direction:")
        col.prop(md, "direction", text="")
        col.label(text="Texture Coordinates:")
        col.prop(md, "texture_coordinates", text="")
        if md.texture_coordinates == 'OBJECT':
            layout.prop(md, "texture_coordinate_object", text="Object")
        elif md.texture_coordinates == 'UV' and ob.type == 'MESH':
            layout.prop_object(md, "uv_layer", ob.data, "uv_textures")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(md, "midlevel")

        if wide_ui:
            col = split.column()
        col.prop(md, "strength")

    def EDGE_SPLIT(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.prop(md, "use_edge_angle", text="Edge Angle")
        sub = col.column()
        sub.active = md.use_edge_angle
        sub.prop(md, "split_angle")

        if wide_ui:
            col = split.column()
        col.prop(md, "use_sharp", text="Sharp Edges")

    def EXPLODE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Vertex group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")
        sub = col.column()
        sub.active = bool(md.vertex_group)
        sub.prop(md, "protect")

        if wide_ui:
            col = split.column()
        col.prop(md, "split_edges")
        col.prop(md, "unborn")
        col.prop(md, "alive")
        col.prop(md, "dead")

        layout.operator("object.explode_refresh", text="Refresh")

    def FLUID_SIMULATION(self, layout, ob, md, wide_ui):
        layout.label(text="See Fluid panel.")

    def HOOK(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        if md.object and md.object.type == 'ARMATURE':
            col.label(text="Bone:")
            col.prop_object(md, "subtarget", md.object.data, "bones", text="")
        if wide_ui:
            col = split.column()
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(md, "falloff")
        col.prop(md, "force", slider=True)
        if wide_ui:
            col = split.column()
        else:
            col.separator()
        col.operator("object.hook_reset", text="Reset")
        col.operator("object.hook_recenter", text="Recenter")

        if ob.mode == 'EDIT':
            layout.separator()
            row = layout.row()
            row.operator("object.hook_select", text="Select")
            row.operator("object.hook_assign", text="Assign")

    def LATTICE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

    def MASK(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Mode:")
        col.prop(md, "mode", text="")
        if wide_ui:
            col = split.column()
        if md.mode == 'ARMATURE':
            col.label(text="Armature:")
            col.prop(md, "armature", text="")
        elif md.mode == 'VERTEX_GROUP':
            col.label(text="Vertex Group:")
            col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

        sub = col.column()
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert")

    def MESH_DEFORM(self, layout, ob, md, wide_ui):
        split = layout.split()
        col = split.column()
        sub = col.column()
        sub.label(text="Object:")
        sub.prop(md, "object", text="")
        sub.prop(md, "mode", text="")
        sub.active = not md.is_bound
        if wide_ui:
            col = split.column()
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

        sub = col.column()
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert")

        layout.separator()

        if md.is_bound:
            layout.operator("object.meshdeform_bind", text="Unbind")
        else:
            layout.operator("object.meshdeform_bind", text="Bind")

            if md.mode == 'VOLUME':
                split = layout.split()

                col = split.column()
                col.prop(md, "precision")

                if wide_ui:
                    col = split.column()
                col.prop(md, "dynamic")

    def MIRROR(self, layout, ob, md, wide_ui):
        layout.prop(md, "merge_limit")
        if wide_ui:
            split = layout.split(percentage=0.25)
        else:
            split = layout.split(percentage=0.4)

        col = split.column()
        col.label(text="Axis:")
        col.prop(md, "x")
        col.prop(md, "y")
        col.prop(md, "z")

        if wide_ui:
            col = split.column()
        else:
            subsplit = layout.split()
            col = subsplit.column()
        col.label(text="Options:")
        col.prop(md, "clip", text="Clipping")
        col.prop(md, "mirror_vertex_groups", text="Vertex Groups")

        col = split.column()
        col.label(text="Textures:")
        col.prop(md, "mirror_u", text="U")
        col.prop(md, "mirror_v", text="V")

        col = layout.column()
        col.label(text="Mirror Object:")
        col.prop(md, "mirror_object", text="")

    def MULTIRES(self, layout, ob, md, wide_ui):
        if wide_ui:
            layout.row().prop(md, "subdivision_type", expand=True)
        else:
            layout.row().prop(md, "subdivision_type", text="")

        split = layout.split()
        col = split.column()
        col.prop(md, "levels", text="Preview")
        col.prop(md, "sculpt_levels", text="Sculpt")
        col.prop(md, "render_levels", text="Render")

        if wide_ui:
            col = split.column()

        col.enabled = ob.mode != 'EDIT'
        col.operator("object.multires_subdivide", text="Subdivide")
        col.operator("object.multires_higher_levels_delete", text="Delete Higher")
        col.operator("object.multires_reshape", text="Reshape")
        col.prop(md, "optimal_display")

        layout.separator()

        col = layout.column()
        row = col.row()
        if md.external:
            row.operator("object.multires_pack_external", text="Pack External")
            row.label()
            row = col.row()
            row.prop(md, "filename", text="")
        else:
            row.operator("object.multires_save_external", text="Save External...")
            row.label()

    def PARTICLE_INSTANCE(self, layout, ob, md, wide_ui):
        layout.prop(md, "object")
        layout.prop(md, "particle_system_number", text="Particle System")

        split = layout.split()
        col = split.column()
        col.label(text="Create From:")
        col.prop(md, "normal")
        col.prop(md, "children")
        col.prop(md, "size")

        if wide_ui:
            col = split.column()
        col.label(text="Show Particles When:")
        col.prop(md, "alive")
        col.prop(md, "unborn")
        col.prop(md, "dead")

        layout.separator()

        layout.prop(md, "path", text="Create Along Paths")

        split = layout.split()
        split.active = md.path
        col = split.column()
        col.row().prop(md, "axis", expand=True)
        col.prop(md, "keep_shape")

        if wide_ui:
            col = split.column()
        col.prop(md, "position", slider=True)
        col.prop(md, "random_position", text="Random", slider=True)

    def PARTICLE_SYSTEM(self, layout, ob, md, wide_ui):
        layout.label(text="See Particle panel.")

    def SHRINKWRAP(self, layout, ob, md, wide_ui):
        split = layout.split()
        col = split.column()
        col.label(text="Target:")
        col.prop(md, "target", text="")
        if wide_ui:
            col = split.column()
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

        split = layout.split()

        col = split.column()
        col.prop(md, "offset")
        col.prop(md, "subsurf_levels")

        if wide_ui:
            col = split.column()
            col.label(text="Mode:")
        col.prop(md, "mode", text="")

        if wide_ui:
            split = layout.split(percentage=0.25)
        else:
            split = layout.split(percentage=0.35)
        col = split.column()

        if md.mode == 'PROJECT':
            col.label(text="Axis:")
            col.prop(md, "x")
            col.prop(md, "y")
            col.prop(md, "z")

            col = split.column()
            col.label(text="Direction:")
            col.prop(md, "negative")
            col.prop(md, "positive")

            if wide_ui:
                col = split.column()
            else:
                subsplit = layout.split()
                col = subsplit.column()
            col.label(text="Cull Faces:")
            col.prop(md, "cull_front_faces", text="Front")
            col.prop(md, "cull_back_faces", text="Back")

            layout.label(text="Auxiliary Target:")
            layout.prop(md, "auxiliary_target", text="")

        elif md.mode == 'NEAREST_SURFACEPOINT':
            layout.prop(md, "keep_above_surface")

    def SIMPLE_DEFORM(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Mode:")
        col.prop(md, "mode", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

        split = layout.split()

        col = split.column()
        col.label(text="Origin:")
        col.prop(md, "origin", text="")
        sub = col.column()
        sub.active = (md.origin != "")
        sub.prop(md, "relative")

        if wide_ui:
            col = split.column()
        col.label(text="Deform:")
        col.prop(md, "factor")
        col.prop(md, "limits", slider=True)
        if md.mode in ('TAPER', 'STRETCH'):
            col.prop(md, "lock_x_axis")
            col.prop(md, "lock_y_axis")

    def SMOKE(self, layout, ob, md, wide_ui):
        layout.label(text="See Smoke panel.")

    def SMOOTH(self, layout, ob, md, wide_ui):
        split = layout.split(percentage=0.25)

        col = split.column()
        col.label(text="Axis:")
        col.prop(md, "x")
        col.prop(md, "y")
        col.prop(md, "z")

        col = split.column()
        col.prop(md, "factor")
        col.prop(md, "repeat")
        col.label(text="Vertex Group:")
        col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

    def SOFT_BODY(self, layout, ob, md, wide_ui):
        layout.label(text="See Soft Body panel.")

    def SOLIDIFY(self, layout, ob, md, wide_ui):
        layout.prop(md, "offset")

        split = layout.split()

        col = split.column()
        col.label(text="Crease:")
        col.prop(md, "edge_crease_inner", text="Inner")
        col.prop(md, "edge_crease_outer", text="Outer")
        col.prop(md, "edge_crease_rim", text="Rim")

        if wide_ui:
            col = split.column()
            col.label()
        col.prop(md, "use_rim")
        col.prop(md, "use_even_offset")
        col.prop(md, "use_quality_normals")

        # col = layout.column()
        # col.label(text="Vertex Group:")
        # col.prop_object(md, "vertex_group", ob, "vertex_groups", text="")

    def SUBSURF(self, layout, ob, md, wide_ui):
        if wide_ui:
            layout.row().prop(md, "subdivision_type", expand=True)
        else:
            layout.row().prop(md, "subdivision_type", text="")

        split = layout.split()
        col = split.column()
        col.label(text="Subdivisions:")
        col.prop(md, "levels", text="View")
        col.prop(md, "render_levels", text="Render")

        if wide_ui:
            col = split.column()
        col.label(text="Options:")
        col.prop(md, "optimal_display")

    def SURFACE(self, layout, ob, md, wide_ui):
        layout.label(text="See Fields panel.")

    def UV_PROJECT(self, layout, ob, md, wide_ui):
        if ob.type == 'MESH':
            split = layout.split()

            col = split.column()
            col.label(text="Image:")
            col.prop(md, "image", text="")

            if wide_ui:
                col = split.column()
            col.label(text="UV Layer:")
            col.prop_object(md, "uv_layer", ob.data, "uv_textures", text="")

            split = layout.split()
            col = split.column()
            col.prop(md, "override_image")
            col.prop(md, "num_projectors", text="Projectors")
            for proj in md.projectors:
                col.prop(proj, "object", text="")

            if wide_ui:
                col = split.column()
            sub = col.column(align=True)
            sub.label(text="Aspect Ratio:")
            sub.prop(md, "horizontal_aspect_ratio", text="Horizontal")
            sub.prop(md, "vertical_aspect_ratio", text="Vertical")

    def WAVE(self, layout, ob, md, wide_ui):
        split = layout.split()

        col = split.column()
        col.label(text="Motion:")
        col.prop(md, "x")
        col.prop(md, "y")
        col.prop(md, "cyclic")

        if wide_ui:
            col = split.column()
        col.prop(md, "normals")
        sub = col.column()
        sub.active = md.normals
        sub.prop(md, "x_normal", text="X")
        sub.prop(md, "y_normal", text="Y")
        sub.prop(md, "z_normal", text="Z")

        split = layout.split()

        col = split.column()
        col.label(text="Time:")
        sub = col.column(align=True)
        sub.prop(md, "time_offset", text="Offset")
        sub.prop(md, "lifetime", text="Life")
        col.prop(md, "damping_time", text="Damping")

        if wide_ui:
            col = split.column()
        col.label(text="Position:")
        sub = col.column(align=True)
        sub.prop(md, "start_position_x", text="X")
        sub.prop(md, "start_position_y", text="Y")
        col.prop(md, "falloff_radius", text="Falloff")

        layout.separator()

        layout.prop(md, "start_position_object")
        layout.prop_object(md, "vertex_group", ob, "vertex_groups")
        layout.prop(md, "texture")
        layout.prop(md, "texture_coordinates")
        if md.texture_coordinates == 'MAP_UV' and ob.type == 'MESH':
            layout.prop_object(md, "uv_layer", ob.data, "uv_textures")
        elif md.texture_coordinates == 'OBJECT':
            layout.prop(md, "texture_coordinates_object")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(md, "speed", slider=True)
        col.prop(md, "height", slider=True)

        if wide_ui:
            col = split.column()
        col.prop(md, "width", slider=True)
        col.prop(md, "narrowness", slider=True)

bpy.types.register(DATA_PT_modifiers)
