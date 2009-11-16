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
        col2 = context.region.width > narrowui

        row = layout.row()
        row.item_menu_enumO("object.modifier_add", "type")
        if col2:
            row.itemL()

        for md in ob.modifiers:
            box = layout.template_modifier(md)
            if box:
                # match enum type to our functions, avoids a lookup table.
                getattr(self, md.type)(box, ob, md, col2)

    # the mt.type enum is (ab)used for a lookup on function names
    # ...to avoid lengthy if statements
    # so each type must have a function here.

    def ARMATURE(self, layout, ob, md, col2):
        if col2:
            layout.itemR(md, "object")
        else:
            layout.itemR(md, "object", text="")

        row = layout.row()
        if col2:
            row.itemL(text="Vertex Group:")
        sub = row.split(percentage=0.7)
        sub.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")
        subsub = sub.row()
        subsub.active = md.vertex_group
        subsub.itemR(md, "invert", text="Inv")

        layout.itemS()

        split = layout.split()

        col = split.column()
        col.itemL(text="Bind To:")
        col.itemR(md, "use_vertex_groups", text="Vertex Groups")
        col.itemR(md, "use_bone_envelopes", text="Bone Envelopes")

        if col2:
            col = split.column()
        col.itemL(text="Deformation:")
        col.itemR(md, "quaternion")
        col.itemR(md, "multi_modifier")

    def ARRAY(self, layout, ob, md, col2):
        if col2:
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

        if col2:
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

    def BEVEL(self, layout, ob, md, col2):
        split = layout.split()
        
        col = split.column()
        col.itemR(md, "width")

        if col2:
            col = split.column()
        col.itemR(md, "only_vertices")

        layout.itemL(text="Limit Method:")
        layout.row().itemR(md, "limit_method", expand=True)
        if md.limit_method == 'ANGLE':
            layout.itemR(md, "angle")
        elif md.limit_method == 'WEIGHT':
            layout.row().itemR(md, "edge_weight_method", expand=True)

    def BOOLEAN(self, layout, ob, md, col2):
        layout.itemR(md, "operation")
        layout.itemR(md, "object")

    def BUILD(self, layout, ob, md, col2):
        split = layout.split()

        col = split.column()
        col.itemR(md, "start")
        col.itemR(md, "length")

        if col2:
            col = split.column()
        col.itemR(md, "randomize")
        sub = col.column()
        sub.active = md.randomize
        sub.itemR(md, "seed")

    def CAST(self, layout, ob, md, col2):
        layout.itemR(md, "cast_type")
        layout.itemR(md, "object")
        if md.object:
            layout.itemR(md, "use_transform")

        split = layout.split()
        
        col = split.column()
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "z")
        
        if col2:
            col = split.column()
        col.itemR(md, "factor")
        col.itemR(md, "radius")
        col.itemR(md, "size")

        layout.itemR(md, "from_radius")

        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")

    def CLOTH(self, layout, ob, md, col2):
        layout.itemL(text="See Cloth panel.")

    def COLLISION(self, layout, ob, md, col2):
        layout.itemL(text="See Collision panel.")

    def CURVE(self, layout, ob, md, col2):
        layout.itemR(md, "object")
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        layout.itemR(md, "deform_axis")

    def DECIMATE(self, layout, ob, md, col2):
        layout.itemR(md, "ratio")
        layout.itemR(md, "face_count")

    def DISPLACE(self, layout, ob, md, col2):
        
        split = layout.split()
        
        col = split.column()
        col.itemR(md, "midlevel")
        
        if col2:
            col = split.column()
        col.itemR(md, "strength")
        
        layout.itemS()
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        layout.itemR(md, "texture")
        layout.itemR(md, "direction")
        layout.itemR(md, "texture_coordinates")
        if md.texture_coordinates == 'OBJECT':
            layout.itemR(md, "texture_coordinate_object", text="Object")
        elif md.texture_coordinates == 'UV' and ob.type == 'MESH':
            layout.item_pointerR(md, "uv_layer", ob.data, "uv_textures")

    def EDGE_SPLIT(self, layout, ob, md, col2):
        split = layout.split()

        col = split.column()
        col.itemR(md, "use_edge_angle", text="Edge Angle")
        sub = col.column()
        sub.active = md.use_edge_angle
        sub.itemR(md, "split_angle")

        if col2:
            col = split.column()
        col.itemR(md, "use_sharp", text="Sharp Edges")

    def EXPLODE(self, layout, ob, md, col2):
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        layout.itemR(md, "protect")

        split = layout.split()

        col = split.column()
        col.itemR(md, "split_edges")
        col.itemR(md, "unborn")
        
        if col2:
            col = split.column()
        col.itemR(md, "alive")
        col.itemR(md, "dead")

        layout.itemO("object.explode_refresh", text="Refresh")

    def FLUID_SIMULATION(self, layout, ob, md, col2):
        layout.itemL(text="See Fluid panel.")

    def HOOK(self, layout, ob, md, col2):
        col = layout.column()
        col.itemR(md, "object")
        if md.object and md.object.type == 'ARMATURE':
            layout.item_pointerR(md, "subtarget", md.object.data, "bones", text="Bone")

        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")

        split = layout.split()

        col = split.column()
        col.itemR(md, "falloff")
        
        if col2:
            col = split.column()
        col.itemR(md, "force", slider=True)

        layout.itemS()

        split = layout.split()

        col = split.column()
        col.itemO("object.hook_reset", text="Reset")
        
        if col2:
            col = split.column()
        col.itemO("object.hook_recenter", text="Recenter")

        if ob.mode == 'EDIT':
            row = layout.row()
            row.itemO("object.hook_select", text="Select")
            row.itemO("object.hook_assign", text="Assign")

    def LATTICE(self, layout, ob, md, col2):
        layout.itemR(md, "object")
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")

    def MASK(self, layout, ob, md, col2):
        layout.itemR(md, "mode")
        if md.mode == 'ARMATURE':
            layout.itemR(md, "armature")
        elif md.mode == 'VERTEX_GROUP':
            layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        layout.itemR(md, "inverse")

    def MESH_DEFORM(self, layout, ob, md, col2):
        layout.itemR(md, "object")
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        layout.itemR(md, "invert")

        layout.itemS()

        if md.is_bound:
            layout.itemO("object.meshdeform_bind", text="Unbind")
        else:
            layout.itemO("object.meshdeform_bind", text="Bind")
            split = layout.split()

            col = split.column()
            col.itemR(md, "precision")
            
            if col2:
                col = split.column()
            col.itemR(md, "dynamic")

    def MIRROR(self, layout, ob, md, col2):
        
        split = layout.split()

        col = split.column()
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "z")

        if col2:
            col = split.column()
        col.itemL(text="Textures:")
        col.itemR(md, "mirror_u", text="U")
        col.itemR(md, "mirror_v", text="V")
        
        layout.itemR(md, "merge_limit")
        
        split = layout.split()
        col = split.column()
        col.itemR(md, "clip")
        if col2:
            col = split.column()
        col.itemR(md, "mirror_vertex_groups", text="Vertex Groups")

        layout.itemR(md, "mirror_object")

    def MULTIRES(self, layout, ob, md, col2):
        if col2:
            layout.row().itemR(md, "subdivision_type", expand=True)
        else:
            layout.row().itemR(md, "subdivision_type", text="")
        layout.itemR(md, "level")
        
        split = layout.split()

        col = split.column()
        col.itemO("object.multires_subdivide", text="Subdivide")
        
        if col2:
            col = split.column()
        col.itemO("object.multires_higher_levels_delete", text="Delete Higher")

    def PARTICLE_INSTANCE(self, layout, ob, md, col2):
        layout.itemR(md, "object")
        layout.itemR(md, "particle_system_number", text="Particle System")
        
        split = layout.split()
        col = split.column()
        col.itemL(text="Create From:")
        col.itemR(md, "normal")
        col.itemR(md, "children")
        col.itemR(md, "size")

        if col2:
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
        
        if col2:
            col = split.column()
        col.itemR(md, "position", slider=True)
        col.itemR(md, "random_position", text="Random", slider=True)

    def PARTICLE_SYSTEM(self, layout, ob, md, col2):
        layout.itemL(text="See Particle panel.")

    def SHRINKWRAP(self, layout, ob, md, col2):
        layout.itemR(md, "target")
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        
        split = layout.split()
        col = split.column()
        col.itemR(md, "offset")
        
        if col2:
            col = split.column()
        col.itemR(md, "subsurf_levels")
        
        if col2:
            layout.itemR(md, "mode")
        else:
            layout.itemR(md, "mode", text="")
        if md.mode == 'PROJECT':
            layout.itemR(md, "auxiliary_target")

            row = layout.row()
            row.itemR(md, "x")
            row.itemR(md, "y")
            row.itemR(md, "z")

            split = layout.split()
            col = split.column()
            col.itemL(text="Direction:")
            col.itemR(md, "negative")
            col.itemR(md, "positive")
            
            col = split.column()
            col.itemL(text="Cull Faces:")
            col.itemR(md, "cull_front_faces", text="Front")
            col.itemR(md, "cull_back_faces", text="Back")
            
        elif md.mode == 'NEAREST_SURFACEPOINT':
            layout.itemR(md, "keep_above_surface")

    def SIMPLE_DEFORM(self, layout, ob, md, col2):
        layout.itemR(md, "mode")
        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
        layout.itemR(md, "origin")
        layout.itemR(md, "relative")
        layout.itemR(md, "factor")
        layout.itemR(md, "limits", slider=True)
        if md.mode in ('TAPER', 'STRETCH'):
            layout.itemR(md, "lock_x_axis")
            layout.itemR(md, "lock_y_axis")

    def SMOKE(self, layout, ob, md, col2):
        layout.itemL(text="See Smoke panel.")

    def SMOOTH(self, layout, ob, md, col2):
        split = layout.split()

        col = split.column()
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "z")

        if col2:
            col = split.column()
        col.itemR(md, "factor")
        col.itemR(md, "repeat")

        layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")

    def SOFT_BODY(self, layout, ob, md, col2):
        layout.itemL(text="See Soft Body panel.")

    def SUBSURF(self, layout, ob, md, col2):
        if col2:
            layout.row().itemR(md, "subdivision_type", expand=True)
        else:
            layout.row().itemR(md, "subdivision_type", text="")

        split = layout.split()
        col = split.column()
        col.itemL(text="Subdivisions:")
        col.itemR(md, "levels", text="View")
        col.itemR(md, "render_levels", text="Render")
        
        if col2:
            col = split.column()
        col.itemL(text="Options:")
        col.itemR(md, "optimal_draw", text="Optimal Display")
        col.itemR(md, "subsurf_uv")

    def SURFACE(self, layout, ob, md, col2):
        layout.itemL(text="See Fields panel.")

    def UV_PROJECT(self, layout, ob, md, col2):
        if ob.type == 'MESH':
            layout.item_pointerR(md, "uv_layer", ob.data, "uv_textures")
            layout.itemR(md, "image")
            

            split = layout.split()
            
            col = split.column()
            col.itemR(md, "override_image")
            col.itemR(md, "num_projectors", text="Projectors")
            for proj in md.projectors:
                col.itemR(proj, "object", text="")
            
            if col2:
                col = split.column()
            sub = col.column(align=True)
            sub.itemL(text="Aspect Ratio:")
            sub.itemR(md, "horizontal_aspect_ratio", text="Horizontal")
            sub.itemR(md, "vertical_aspect_ratio", text="Vertical")

    def WAVE(self, layout, ob, md, col2):
        split = layout.split()

        col = split.column()
        col.itemL(text="Motion:")
        col.itemR(md, "x")
        col.itemR(md, "y")
        col.itemR(md, "cyclic")

        if col2:
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

        if col2:
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
        
        if col2:
            col = split.column()
        col.itemR(md, "width", slider=True)
        col.itemR(md, "narrowness", slider=True)

bpy.types.register(DATA_PT_modifiers)
