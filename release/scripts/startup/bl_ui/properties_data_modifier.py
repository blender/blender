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
from bpy.types import Panel
from bpy.app.translations import pgettext_iface as iface_


class ModifierButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "modifier"
    bl_options = {'HIDE_HEADER'}


class DATA_PT_modifiers(ModifierButtonsPanel, Panel):
    bl_label = "Modifiers"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.type != 'GPENCIL'

    def draw(self, context):
        layout = self.layout

        ob = context.object

        layout.operator_menu_enum("object.modifier_add", "type")

        for md in ob.modifiers:
            box = layout.template_modifier(md)
            if box:
                # match enum type to our functions, avoids a lookup table.
                getattr(self, md.type)(box, ob, md)

    # the mt.type enum is (ab)used for a lookup on function names
    # ...to avoid lengthy if statements
    # so each type must have a function here.

    def ARMATURE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        col.prop(md, "use_deform_preserve_volume")

        col = split.column()
        col.label(text="Bind To:")
        col.prop(md, "use_vertex_groups", text="Vertex Groups")
        col.prop(md, "use_bone_envelopes", text="Bone Envelopes")

        layout.separator()

        split = layout.split()

        row = split.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = row.row(align=True)
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

        split.prop(md, "use_multi_modifier")

    def ARRAY(self, layout, ob, md):
        layout.prop(md, "fit_type")

        if md.fit_type == 'FIXED_COUNT':
            layout.prop(md, "count")
        elif md.fit_type == 'FIT_LENGTH':
            layout.prop(md, "fit_length")
        elif md.fit_type == 'FIT_CURVE':
            layout.prop(md, "curve")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(md, "use_constant_offset")
        sub = col.column()
        sub.active = md.use_constant_offset
        sub.prop(md, "constant_offset_displace", text="")

        col.separator()

        col.prop(md, "use_merge_vertices", text="Merge")
        sub = col.column()
        sub.active = md.use_merge_vertices
        sub.prop(md, "use_merge_vertices_cap", text="First Last")
        sub.prop(md, "merge_threshold", text="Distance")

        col = split.column()
        col.prop(md, "use_relative_offset")
        sub = col.column()
        sub.active = md.use_relative_offset
        sub.prop(md, "relative_offset_displace", text="")

        col.separator()

        col.prop(md, "use_object_offset")
        sub = col.column()
        sub.active = md.use_object_offset
        sub.prop(md, "offset_object", text="")

        row = layout.row()
        split = row.split()
        col = split.column()
        col.label(text="UVs:")
        sub = col.column(align=True)
        sub.prop(md, "offset_u")
        sub.prop(md, "offset_v")
        layout.separator()

        layout.prop(md, "start_cap")
        layout.prop(md, "end_cap")

    def BEVEL(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.prop(md, "width")
        col.prop(md, "segments")
        col.prop(md, "profile")
        col.prop(md, "material")

        col = split.column()
        col.prop(md, "use_only_vertices")
        col.prop(md, "use_clamp_overlap")
        col.prop(md, "loop_slide")
        col.prop(md, "mark_seam")
        col.prop(md, "mark_sharp")

        layout.label(text="Limit Method:")
        layout.row().prop(md, "limit_method", expand=True)
        if md.limit_method == 'ANGLE':
            layout.prop(md, "angle_limit")
        elif md.limit_method == 'VGROUP':
            layout.label(text="Vertex Group:")
            layout.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        layout.label(text="Width Method:")
        layout.row().prop(md, "offset_type", expand=True)

        layout.label(text="Normal Mode")
        layout.row().prop(md, "hnmode", expand=True)
        layout.prop(md, "hn_strength")
        layout.prop(md, "set_wn_strength")


    def BOOLEAN(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Operation:")
        col.prop(md, "operation", text="")

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")

        layout.prop(md, "double_threshold")

        if bpy.app.debug:
            layout.prop(md, "debug_options")

    def BUILD(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.prop(md, "frame_start")
        col.prop(md, "frame_duration")
        col.prop(md, "use_reverse")

        col = split.column()
        col.prop(md, "use_random_order")
        sub = col.column()
        sub.active = md.use_random_order
        sub.prop(md, "seed")

    def MESH_CACHE(self, layout, ob, md):
        layout.prop(md, "cache_format")
        layout.prop(md, "filepath")

        if md.cache_format == 'ABC':
            layout.prop(md, "sub_object")

        layout.label(text="Evaluation:")
        layout.prop(md, "factor", slider=True)
        layout.prop(md, "deform_mode")
        layout.prop(md, "interpolation")

        layout.label(text="Time Mapping:")

        row = layout.row()
        row.prop(md, "time_mode", expand=True)
        row = layout.row()
        row.prop(md, "play_mode", expand=True)
        if md.play_mode == 'SCENE':
            layout.prop(md, "frame_start")
            layout.prop(md, "frame_scale")
        else:
            time_mode = md.time_mode
            if time_mode == 'FRAME':
                layout.prop(md, "eval_frame")
            elif time_mode == 'TIME':
                layout.prop(md, "eval_time")
            elif time_mode == 'FACTOR':
                layout.prop(md, "eval_factor")

        layout.label(text="Axis Mapping:")
        split = layout.split(percentage=0.5, align=True)
        split.alert = (md.forward_axis[-1] == md.up_axis[-1])
        split.label("Forward/Up Axis:")
        split.prop(md, "forward_axis", text="")
        split.prop(md, "up_axis", text="")
        split = layout.split(percentage=0.5)
        split.label(text="Flip Axis:")
        row = split.row()
        row.prop(md, "flip_axis")

    def MESH_SEQUENCE_CACHE(self, layout, ob, md):
        layout.label(text="Cache File Properties:")
        box = layout.box()
        box.template_cache_file(md, "cache_file")

        cache_file = md.cache_file

        layout.label(text="Modifier Properties:")
        box = layout.box()

        if cache_file is not None:
            box.prop_search(md, "object_path", cache_file, "object_paths")

        if ob.type == 'MESH':
            box.row().prop(md, "read_data")

    def CAST(self, layout, ob, md):
        split = layout.split(percentage=0.25)

        split.label(text="Cast Type:")
        split.prop(md, "cast_type", text="")

        split = layout.split(percentage=0.25)

        col = split.column()
        col.prop(md, "use_x")
        col.prop(md, "use_y")
        col.prop(md, "use_z")

        col = split.column()
        col.prop(md, "factor")
        col.prop(md, "radius")
        col.prop(md, "size")
        col.prop(md, "use_radius_as_size")

        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        col = split.column()
        col.label(text="Control Object:")
        col.prop(md, "object", text="")
        if md.object:
            col.prop(md, "use_transform")

    def CLOTH(self, layout, ob, md):
        layout.label(text="Settings are inside the Physics tab")

    def COLLISION(self, layout, ob, md):
        layout.label(text="Settings are inside the Physics tab")

    def CURVE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        layout.label(text="Deformation Axis:")
        layout.row().prop(md, "deform_axis", expand=True)

    def DECIMATE(self, layout, ob, md):
        decimate_type = md.decimate_type

        row = layout.row()
        row.prop(md, "decimate_type", expand=True)

        if decimate_type == 'COLLAPSE':
            has_vgroup = bool(md.vertex_group)
            layout.prop(md, "ratio")

            split = layout.split()

            col = split.column()
            row = col.row(align=True)
            row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
            row.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

            layout_info = col

            col = split.column()
            row = col.row()
            row.active = has_vgroup
            row.prop(md, "vertex_group_factor")

            col.prop(md, "use_collapse_triangulate")
            row = col.split(percentage=0.75)
            row.prop(md, "use_symmetry")
            row.prop(md, "symmetry_axis", text="")

        elif decimate_type == 'UNSUBDIV':
            layout.prop(md, "iterations")
            layout_info = layout
        else:  # decimate_type == 'DISSOLVE':
            layout.prop(md, "angle_limit")
            layout.prop(md, "use_dissolve_boundaries")
            layout.label("Delimit:")
            row = layout.row()
            row.prop(md, "delimit")
            layout_info = layout

        layout_info.label(
            text=iface_("Face Count: {:,}".format(md.face_count)),
            translate=False,
        )

    def DISPLACE(self, layout, ob, md):
        has_texture = (md.texture is not None)

        col = layout.column(align=True)
        col.label(text="Texture:")
        col.template_ID(md, "texture", new="texture.new")

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Direction:")
        col.prop(md, "direction", text="")
        if md.direction in {'X', 'Y', 'Z', 'RGB_TO_XYZ'}:
            col.label(text="Space:")
            col.prop(md, "space", text="")
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        col = split.column(align=True)
        col.active = has_texture
        col.label(text="Texture Coordinates:")
        col.prop(md, "texture_coords", text="")
        if md.texture_coords == 'OBJECT':
            col.label(text="Object:")
            col.prop(md, "texture_coords_object", text="")
        elif md.texture_coords == 'UV' and ob.type == 'MESH':
            col.label(text="UV Map:")
            col.prop_search(md, "uv_layer", ob.data, "uv_layers", text="")

        layout.separator()

        row = layout.row()
        row.prop(md, "mid_level")
        row.prop(md, "strength")

    def DYNAMIC_PAINT(self, layout, ob, md):
        layout.label(text="Settings are inside the Physics tab")

    def EDGE_SPLIT(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.prop(md, "use_edge_angle", text="Edge Angle")
        sub = col.column()
        sub.active = md.use_edge_angle
        sub.prop(md, "split_angle")

        split.prop(md, "use_edge_sharp", text="Sharp Edges")

    def EXPLODE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = col.column()
        sub.active = bool(md.vertex_group)
        sub.prop(md, "protect")
        col.label(text="Particle UV")
        col.prop_search(md, "particle_uv", ob.data, "uv_layers", text="")

        col = split.column()
        col.prop(md, "use_edge_cut")
        col.prop(md, "show_unborn")
        col.prop(md, "show_alive")
        col.prop(md, "show_dead")
        col.prop(md, "use_size")

        layout.operator("object.explode_refresh", text="Refresh")

    def FLUID_SIMULATION(self, layout, ob, md):
        layout.label(text="Settings are inside the Physics tab")

    def HOOK(self, layout, ob, md):
        use_falloff = (md.falloff_type != 'NONE')
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        if md.object and md.object.type == 'ARMATURE':
            col.label(text="Bone:")
            col.prop_search(md, "subtarget", md.object.data, "bones", text="")
        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        layout.separator()

        row = layout.row(align=True)
        if use_falloff:
            row.prop(md, "falloff_radius")
        row.prop(md, "strength", slider=True)
        layout.prop(md, "falloff_type")

        col = layout.column()
        if use_falloff:
            if md.falloff_type == 'CURVE':
                col.template_curve_mapping(md, "falloff_curve")

        split = layout.split()

        col = split.column()
        col.prop(md, "use_falloff_uniform")

        if ob.mode == 'EDIT':
            row = col.row(align=True)
            row.operator("object.hook_reset", text="Reset")
            row.operator("object.hook_recenter", text="Recenter")

            row = layout.row(align=True)
            row.operator("object.hook_select", text="Select")
            row.operator("object.hook_assign", text="Assign")

    def LAPLACIANDEFORM(self, layout, ob, md):
        is_bind = md.is_bind

        layout.prop(md, "iterations")

        row = layout.row()
        row.active = not is_bind
        row.label(text="Anchors Vertex Group:")

        row = layout.row()
        row.enabled = not is_bind
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        layout.separator()

        row = layout.row()
        row.enabled = bool(md.vertex_group)
        row.operator("object.laplaciandeform_bind", text="Unbind" if is_bind else "Bind")

    def LAPLACIANSMOOTH(self, layout, ob, md):
        layout.prop(md, "iterations")

        split = layout.split(percentage=0.25)

        col = split.column()
        col.label(text="Axis:")
        col.prop(md, "use_x")
        col.prop(md, "use_y")
        col.prop(md, "use_z")

        col = split.column()
        col.label(text="Lambda:")
        col.prop(md, "lambda_factor", text="Factor")
        col.prop(md, "lambda_border", text="Border")

        col.separator()
        col.prop(md, "use_volume_preserve")
        col.prop(md, "use_normalized")

        layout.label(text="Vertex Group:")
        layout.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

    def LATTICE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        layout.separator()
        layout.prop(md, "strength", slider=True)

    def MASK(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Mode:")
        col.prop(md, "mode", text="")

        col = split.column()
        if md.mode == 'ARMATURE':
            col.label(text="Armature:")
            row = col.row(align=True)
            row.prop(md, "armature", text="")
            sub = row.row(align=True)
            sub.active = (md.armature is not None)
            sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
        elif md.mode == 'VERTEX_GROUP':
            col.label(text="Vertex Group:")
            row = col.row(align=True)
            row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
            sub = row.row(align=True)
            sub.active = bool(md.vertex_group)
            sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

    def MESH_DEFORM(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.enabled = not md.is_bound
        col.label(text="Object:")
        col.prop(md, "object", text="")

        col = split.column()
        col.label(text="Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = row.row(align=True)
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

        layout.separator()
        row = layout.row()
        row.enabled = not md.is_bound
        row.prop(md, "precision")
        row.prop(md, "use_dynamic_bind")

        layout.separator()
        if md.is_bound:
            layout.operator("object.meshdeform_bind", text="Unbind")
        else:
            layout.operator("object.meshdeform_bind", text="Bind")

    def MIRROR(self, layout, ob, md):
        split = layout.split(percentage=0.25)

        col = split.column()
        col.label(text="Axis:")
        col.prop(md, "use_x")
        col.prop(md, "use_y")
        col.prop(md, "use_z")

        col = split.column()
        col.label(text="Options:")
        col.prop(md, "use_mirror_merge", text="Merge")
        col.prop(md, "use_clip", text="Clipping")
        col.prop(md, "use_mirror_vertex_groups", text="Vertex Groups")

        col = split.column()
        col.label(text="Textures:")
        col.prop(md, "use_mirror_u", text="Flip U")
        col.prop(md, "use_mirror_v", text="Flip V")

        col = layout.column(align=True)

        if md.use_mirror_u:
            col.prop(md, "mirror_offset_u")

        if md.use_mirror_v:
            col.prop(md, "mirror_offset_v")

        col = layout.column(align=True)
        col.prop(md, "offset_u")
        col.prop(md, "offset_v")

        col = layout.column()

        if md.use_mirror_merge is True:
            col.prop(md, "merge_threshold")
        col.label(text="Mirror Object:")
        col.prop(md, "mirror_object", text="")

    def MULTIRES(self, layout, ob, md):
        layout.row().prop(md, "subdivision_type", expand=True)

        split = layout.split()
        col = split.column()
        col.prop(md, "levels", text="Preview")
        col.prop(md, "sculpt_levels", text="Sculpt")
        col.prop(md, "render_levels", text="Render")
        if hasattr(md, "quality"):
            col.prop(md, "quality")

        col = split.column()

        col.enabled = ob.mode != 'EDIT'
        col.operator("object.multires_subdivide", text="Subdivide")
        col.operator("object.multires_higher_levels_delete", text="Delete Higher")
        col.operator("object.multires_reshape", text="Reshape")
        col.operator("object.multires_base_apply", text="Apply Base")
        col.prop(md, "uv_smooth", text="")
        col.prop(md, "show_only_control_edges")

        layout.separator()

        col = layout.column()
        row = col.row()
        if md.is_external:
            row.operator("object.multires_external_pack", text="Pack External")
            row.label()
            row = col.row()
            row.prop(md, "filepath", text="")
        else:
            row.operator("object.multires_external_save", text="Save External...")
            row.label()

    def OCEAN(self, layout, ob, md):
        if not bpy.app.build_options.mod_oceansim:
            layout.label("Built without OceanSim modifier")
            return

        layout.prop(md, "geometry_mode")

        if md.geometry_mode == 'GENERATE':
            row = layout.row()
            row.prop(md, "repeat_x")
            row.prop(md, "repeat_y")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(md, "time")
        col.prop(md, "depth")
        col.prop(md, "random_seed")

        col = split.column()
        col.prop(md, "resolution")
        col.prop(md, "size")
        col.prop(md, "spatial_size")

        layout.label("Waves:")

        split = layout.split()

        col = split.column()
        col.prop(md, "choppiness")
        col.prop(md, "wave_scale", text="Scale")
        col.prop(md, "wave_scale_min")
        col.prop(md, "wind_velocity")

        col = split.column()
        col.prop(md, "wave_alignment", text="Alignment")
        sub = col.column()
        sub.active = (md.wave_alignment > 0.0)
        sub.prop(md, "wave_direction", text="Direction")
        sub.prop(md, "damping")

        layout.separator()

        layout.prop(md, "use_normals")

        split = layout.split()

        col = split.column()
        col.prop(md, "use_foam")
        sub = col.row()
        sub.active = md.use_foam
        sub.prop(md, "foam_coverage", text="Coverage")

        col = split.column()
        col.active = md.use_foam
        col.label("Foam Data Layer Name:")
        col.prop(md, "foam_layer_name", text="")

        layout.separator()

        if md.is_cached:
            layout.operator("object.ocean_bake", text="Free Bake").free = True
        else:
            layout.operator("object.ocean_bake").free = False

        split = layout.split()
        split.enabled = not md.is_cached

        col = split.column(align=True)
        col.prop(md, "frame_start", text="Start")
        col.prop(md, "frame_end", text="End")

        col = split.column(align=True)
        col.label(text="Cache path:")
        col.prop(md, "filepath", text="")

        split = layout.split()
        split.enabled = not md.is_cached

        col = split.column()
        col.active = md.use_foam
        col.prop(md, "bake_foam_fade")

        col = split.column()

    def PARTICLE_INSTANCE(self, layout, ob, md):
        layout.prop(md, "object")
        if md.object:
            layout.prop_search(md, "particle_system", md.object, "particle_systems", text="Particle System")
        else:
            layout.prop(md, "particle_system_index", text="Particle System")

        split = layout.split()
        col = split.column()
        col.label(text="Create From:")
        layout.prop(md, "space", text="")
        col.prop(md, "use_normal")
        col.prop(md, "use_children")
        col.prop(md, "use_size")

        col = split.column()
        col.label(text="Show Particles When:")
        col.prop(md, "show_alive")
        col.prop(md, "show_unborn")
        col.prop(md, "show_dead")

        row = layout.row(align=True)
        row.prop(md, "particle_amount", text="Amount")
        row.prop(md, "particle_offset", text="Offset")

        row = layout.row(align=True)
        row.prop(md, "axis", expand=True)

        layout.separator()

        layout.prop(md, "use_path", text="Create Along Paths")

        col = layout.column()
        col.active = md.use_path
        col.prop(md, "use_preserve_shape")

        row = col.row(align=True)
        row.prop(md, "position", slider=True)
        row.prop(md, "random_position", text="Random", slider=True)
        row = col.row(align=True)
        row.prop(md, "rotation", slider=True)
        row.prop(md, "random_rotation", text="Random", slider=True)

        layout.separator()

        col = layout.column()
        col.prop_search(md, "index_layer_name", ob.data, "vertex_colors", text="Index Layer")
        col.prop_search(md, "value_layer_name", ob.data, "vertex_colors", text="Value Layer")

    def PARTICLE_SYSTEM(self, layout, ob, md):
        layout.label(text="Settings can be found inside the Particle context")

    def SCREW(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.prop(md, "axis")
        col.prop(md, "object", text="AxisOb")
        col.prop(md, "angle")
        col.prop(md, "steps")
        col.prop(md, "render_steps")
        col.prop(md, "use_smooth_shade")
        col.prop(md, "use_merge_vertices")
        sub = col.column()
        sub.active = md.use_merge_vertices
        sub.prop(md, "merge_threshold")

        col = split.column()
        row = col.row()
        row.active = (md.object is None or md.use_object_screw_offset is False)
        row.prop(md, "screw_offset")
        row = col.row()
        row.active = (md.object is not None)
        row.prop(md, "use_object_screw_offset")
        col.prop(md, "use_normal_calculate")
        col.prop(md, "use_normal_flip")
        col.prop(md, "iterations")
        col.prop(md, "use_stretch_u")
        col.prop(md, "use_stretch_v")

    def SHRINKWRAP(self, layout, ob, md):
        split = layout.split()
        col = split.column()
        col.label(text="Target:")
        col.prop(md, "target", text="")
        col = split.column()
        col.label(text="Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

        split = layout.split()

        col = split.column()
        col.prop(md, "offset")

        col = split.column()
        col.label(text="Mode:")
        col.prop(md, "wrap_method", text="")

        if md.wrap_method == 'PROJECT':
            split = layout.split()
            col = split.column()
            col.prop(md, "subsurf_levels")
            col = split.column()

            col.prop(md, "project_limit", text="Limit")
            split = layout.split(percentage=0.25)

            col = split.column()
            col.label(text="Axis:")
            col.prop(md, "use_project_x")
            col.prop(md, "use_project_y")
            col.prop(md, "use_project_z")

            col = split.column()
            col.label(text="Direction:")
            col.prop(md, "use_negative_direction")
            col.prop(md, "use_positive_direction")

            col = split.column()
            col.label(text="Cull Faces:")
            col.prop(md, "cull_face", expand=True)

            layout.prop(md, "auxiliary_target")

        elif md.wrap_method == 'NEAREST_SURFACEPOINT':
            layout.prop(md, "use_keep_above_surface")

    def SIMPLE_DEFORM(self, layout, ob, md):

        layout.row().prop(md, "deform_method", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

        split = layout.split()

        col = split.column()
        col.label(text="Axis, Origin:")
        col.prop(md, "origin", text="")

        col.prop(md, "deform_axis")

        if md.deform_method in {'TAPER', 'STRETCH', 'TWIST'}:
            row = col.row(align=True)
            row.label(text="Lock:")
            deform_axis = md.deform_axis
            if deform_axis != 'X':
                row.prop(md, "lock_x")
            if deform_axis != 'Y':
                row.prop(md, "lock_y")
            if deform_axis != 'Z':
                row.prop(md, "lock_z")

        col = split.column()
        col.label(text="Deform:")
        if md.deform_method in {'TAPER', 'STRETCH'}:
            col.prop(md, "factor")
        else:
            col.prop(md, "angle")
        col.prop(md, "limits", slider=True)

    def SMOKE(self, layout, ob, md):
        layout.label(text="Settings are inside the Physics tab")

    def SMOOTH(self, layout, ob, md):
        split = layout.split(percentage=0.25)

        col = split.column()
        col.label(text="Axis:")
        col.prop(md, "use_x")
        col.prop(md, "use_y")
        col.prop(md, "use_z")

        col = split.column()
        col.prop(md, "factor")
        col.prop(md, "iterations")
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

    def SOFT_BODY(self, layout, ob, md):
        layout.label(text="Settings are inside the Physics tab")

    def SOLIDIFY(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.prop(md, "thickness")
        col.prop(md, "thickness_clamp")

        col.separator()

        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = row.row(align=True)
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

        sub = col.row()
        sub.active = bool(md.vertex_group)
        sub.prop(md, "thickness_vertex_group", text="Factor")

        col.label(text="Crease:")
        col.prop(md, "edge_crease_inner", text="Inner")
        col.prop(md, "edge_crease_outer", text="Outer")
        col.prop(md, "edge_crease_rim", text="Rim")

        col = split.column()

        col.prop(md, "offset")
        col.prop(md, "use_flip_normals")

        col.prop(md, "use_even_offset")
        col.prop(md, "use_quality_normals")
        col.prop(md, "use_rim")
        col_rim = col.column()
        col_rim.active = md.use_rim
        col_rim.prop(md, "use_rim_only")

        col.separator()

        col.label(text="Material Index Offset:")

        sub = col.column()
        row = sub.split(align=True, percentage=0.4)
        row.prop(md, "material_offset", text="")
        row = row.row(align=True)
        row.active = md.use_rim
        row.prop(md, "material_offset_rim", text="Rim")

    def SUBSURF(self, layout, ob, md):
        from bpy import context
        layout.row().prop(md, "subdivision_type", expand=True)

        split = layout.split()
        col = split.column()

        scene = context.scene
        engine = context.engine
        show_adaptive_options = (
            engine == 'CYCLES' and md == ob.modifiers[-1] and
            scene.cycles.feature_set == 'EXPERIMENTAL'
        )
        if show_adaptive_options:
            col.label(text="View:")
            col.prop(md, "levels", text="Levels")
            col.label(text="Render:")
            col.prop(ob.cycles, "use_adaptive_subdivision", text="Adaptive")
            if ob.cycles.use_adaptive_subdivision:
                col.prop(ob.cycles, "dicing_rate")
            else:
                col.prop(md, "render_levels", text="Levels")
        else:
            col.label(text="Subdivisions:")
            col.prop(md, "levels", text="View")
            col.prop(md, "render_levels", text="Render")
            if hasattr(md, "quality"):
                col.prop(md, "quality")

        col = split.column()
        col.label(text="Options:")

        sub = col.column()
        sub.active = (not show_adaptive_options) or (not ob.cycles.use_adaptive_subdivision)
        sub.prop(md, "uv_smooth", text="")

        col.prop(md, "show_only_control_edges")

        if show_adaptive_options and ob.cycles.use_adaptive_subdivision:
            col = layout.column(align=True)
            col.scale_y = 0.6
            col.separator()
            col.label("Final Dicing Rate:")
            col.separator()

            render = max(scene.cycles.dicing_rate * ob.cycles.dicing_rate, 0.1)
            preview = max(scene.cycles.preview_dicing_rate * ob.cycles.dicing_rate, 0.1)
            col.label(f"Render {render:10.2f} px, Preview {preview:10.2f} px")

    def SURFACE(self, layout, ob, md):
        layout.label(text="Settings are inside the Physics tab")

    def SURFACE_DEFORM(self, layout, ob, md):
        col = layout.column()
        col.active = not md.is_bound

        col.prop(md, "target")
        col.prop(md, "falloff")

        layout.separator()

        col = layout.column()

        if md.is_bound:
            col.operator("object.surfacedeform_bind", text="Unbind")
        else:
            col.active = md.target is not None
            col.operator("object.surfacedeform_bind", text="Bind")

    def UV_PROJECT(self, layout, ob, md):
        split = layout.split()
        col = split.column()
        col.prop_search(md, "uv_layer", ob.data, "uv_layers")
        col.separator()

        col.prop(md, "projector_count", text="Projectors")
        for proj in md.projectors:
            col.prop(proj, "object", text="")

        col = split.column()
        sub = col.column(align=True)
        sub.prop(md, "aspect_x", text="Aspect X")
        sub.prop(md, "aspect_y", text="Aspect Y")

        sub = col.column(align=True)
        sub.prop(md, "scale_x", text="Scale X")
        sub.prop(md, "scale_y", text="Scale Y")

    def WARP(self, layout, ob, md):
        use_falloff = (md.falloff_type != 'NONE')
        split = layout.split()

        col = split.column()
        col.label(text="From:")
        col.prop(md, "object_from", text="")

        col.prop(md, "use_volume_preserve")

        col = split.column()
        col.label(text="To:")
        col.prop(md, "object_to", text="")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        col = layout.column()

        row = col.row(align=True)
        row.prop(md, "strength")
        if use_falloff:
            row.prop(md, "falloff_radius")

        col.prop(md, "falloff_type")
        if use_falloff:
            if md.falloff_type == 'CURVE':
                col.template_curve_mapping(md, "falloff_curve")

        # 2 new columns
        split = layout.split()
        col = split.column()
        col.label(text="Texture:")
        col.template_ID(md, "texture", new="texture.new")

        col = split.column()
        col.label(text="Texture Coordinates:")
        col.prop(md, "texture_coords", text="")

        if md.texture_coords == 'OBJECT':
            layout.prop(md, "texture_coords_object", text="Object")
        elif md.texture_coords == 'UV' and ob.type == 'MESH':
            layout.prop_search(md, "uv_layer", ob.data, "uv_layers")

    def WAVE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Motion:")
        col.prop(md, "use_x")
        col.prop(md, "use_y")
        col.prop(md, "use_cyclic")

        col = split.column()
        col.prop(md, "use_normal")
        sub = col.column()
        sub.active = md.use_normal
        sub.prop(md, "use_normal_x", text="X")
        sub.prop(md, "use_normal_y", text="Y")
        sub.prop(md, "use_normal_z", text="Z")

        split = layout.split()

        col = split.column()
        col.label(text="Time:")
        sub = col.column(align=True)
        sub.prop(md, "time_offset", text="Offset")
        sub.prop(md, "lifetime", text="Life")
        col.prop(md, "damping_time", text="Damping")

        col = split.column()
        col.label(text="Position:")
        sub = col.column(align=True)
        sub.prop(md, "start_position_x", text="X")
        sub.prop(md, "start_position_y", text="Y")
        col.prop(md, "falloff_radius", text="Falloff")

        layout.separator()

        layout.prop(md, "start_position_object")
        layout.prop_search(md, "vertex_group", ob, "vertex_groups")
        split = layout.split(percentage=0.33)
        col = split.column()
        col.label(text="Texture")
        col = split.column()
        col.template_ID(md, "texture", new="texture.new")
        layout.prop(md, "texture_coords")
        if md.texture_coords == 'UV' and ob.type == 'MESH':
            layout.prop_search(md, "uv_layer", ob.data, "uv_layers")
        elif md.texture_coords == 'OBJECT':
            layout.prop(md, "texture_coords_object")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(md, "speed", slider=True)
        col.prop(md, "height", slider=True)

        col = split.column()
        col.prop(md, "width", slider=True)
        col.prop(md, "narrowness", slider=True)

    def REMESH(self, layout, ob, md):
        if not bpy.app.build_options.mod_remesh:
            layout.label("Built without Remesh modifier")
            return

        layout.prop(md, "mode")

        row = layout.row()
        row.prop(md, "octree_depth")
        row.prop(md, "scale")

        if md.mode == 'SHARP':
            layout.prop(md, "sharpness")

        layout.prop(md, "use_smooth_shade")
        layout.prop(md, "use_remove_disconnected")
        row = layout.row()
        row.active = md.use_remove_disconnected
        row.prop(md, "threshold")

    @staticmethod
    def vertex_weight_mask(layout, ob, md):
        layout.label(text="Influence/Mask Options:")

        split = layout.split(percentage=0.4)
        split.label(text="Global Influence:")
        split.prop(md, "mask_constant", text="")

        if not md.mask_texture:
            split = layout.split(percentage=0.4)
            split.label(text="Vertex Group Mask:")
            split.prop_search(md, "mask_vertex_group", ob, "vertex_groups", text="")

        if not md.mask_vertex_group:
            split = layout.split(percentage=0.4)
            split.label(text="Texture Mask:")
            split.template_ID(md, "mask_texture", new="texture.new")
            if md.mask_texture:
                split = layout.split()

                col = split.column()
                col.label(text="Texture Coordinates:")
                col.prop(md, "mask_tex_mapping", text="")

                col = split.column()
                col.label(text="Use Channel:")
                col.prop(md, "mask_tex_use_channel", text="")

                if md.mask_tex_mapping == 'OBJECT':
                    layout.prop(md, "mask_tex_map_object", text="Object")
                elif md.mask_tex_mapping == 'UV' and ob.type == 'MESH':
                    layout.prop_search(md, "mask_tex_uv_layer", ob.data, "uv_layers")

    def VERTEX_WEIGHT_EDIT(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        col.label(text="Default Weight:")
        col.prop(md, "default_weight", text="")

        col = split.column()
        col.prop(md, "use_add")
        sub = col.column()
        sub.active = md.use_add
        sub.prop(md, "add_threshold")

        col = col.column()
        col.prop(md, "use_remove")
        sub = col.column()
        sub.active = md.use_remove
        sub.prop(md, "remove_threshold")

        layout.separator()

        layout.prop(md, "falloff_type")
        if md.falloff_type == 'CURVE':
            layout.template_curve_mapping(md, "map_curve")

        # Common mask options
        layout.separator()
        self.vertex_weight_mask(layout, ob, md)

    def VERTEX_WEIGHT_MIX(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group A:")
        col.prop_search(md, "vertex_group_a", ob, "vertex_groups", text="")
        col.label(text="Default Weight A:")
        col.prop(md, "default_weight_a", text="")

        col.label(text="Mix Mode:")
        col.prop(md, "mix_mode", text="")

        col = split.column()
        col.label(text="Vertex Group B:")
        col.prop_search(md, "vertex_group_b", ob, "vertex_groups", text="")
        col.label(text="Default Weight B:")
        col.prop(md, "default_weight_b", text="")

        col.label(text="Mix Set:")
        col.prop(md, "mix_set", text="")

        # Common mask options
        layout.separator()
        self.vertex_weight_mask(layout, ob, md)

    def VERTEX_WEIGHT_PROXIMITY(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        col = split.column()
        col.label(text="Target Object:")
        col.prop(md, "target", text="")

        split = layout.split()

        col = split.column()
        col.label(text="Distance:")
        col.prop(md, "proximity_mode", text="")
        if md.proximity_mode == 'GEOMETRY':
            col.row().prop(md, "proximity_geometry")

        col = split.column()
        col.label()
        col.prop(md, "min_dist")
        col.prop(md, "max_dist")

        layout.separator()
        layout.prop(md, "falloff_type")

        # Common mask options
        layout.separator()
        self.vertex_weight_mask(layout, ob, md)

    def SKIN(self, layout, ob, md):
        row = layout.row()
        row.operator("object.skin_armature_create", text="Create Armature")
        row.operator("mesh.customdata_skin_add")

        layout.separator()

        row = layout.row(align=True)
        row.prop(md, "branch_smoothing")
        row.prop(md, "use_smooth_shade")

        split = layout.split()

        col = split.column()
        col.label(text="Selected Vertices:")
        sub = col.column(align=True)
        sub.operator("object.skin_loose_mark_clear", text="Mark Loose").action = 'MARK'
        sub.operator("object.skin_loose_mark_clear", text="Clear Loose").action = 'CLEAR'

        sub = col.column()
        sub.operator("object.skin_root_mark", text="Mark Root")
        sub.operator("object.skin_radii_equalize", text="Equalize Radii")

        col = split.column()
        col.label(text="Symmetry Axes:")
        col.prop(md, "use_x_symmetry")
        col.prop(md, "use_y_symmetry")
        col.prop(md, "use_z_symmetry")

    def TRIANGULATE(self, layout, ob, md):
        row = layout.row()

        col = row.column()
        col.label(text="Quad Method:")
        col.prop(md, "quad_method", text="")
        col = row.column()
        col.label(text="Ngon Method:")
        col.prop(md, "ngon_method", text="")

    def UV_WARP(self, layout, ob, md):
        split = layout.split()
        col = split.column()
        col.prop(md, "center")

        col = split.column()
        col.label(text="UV Axis:")
        col.prop(md, "axis_u", text="")
        col.prop(md, "axis_v", text="")

        split = layout.split()
        col = split.column()
        col.label(text="From:")
        col.prop(md, "object_from", text="")

        col = split.column()
        col.label(text="To:")
        col.prop(md, "object_to", text="")

        split = layout.split()
        col = split.column()
        obj = md.object_from
        if obj and obj.type == 'ARMATURE':
            col.label(text="Bone:")
            col.prop_search(md, "bone_from", obj.data, "bones", text="")

        col = split.column()
        obj = md.object_to
        if obj and obj.type == 'ARMATURE':
            col.label(text="Bone:")
            col.prop_search(md, "bone_to", obj.data, "bones", text="")

        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(md, "vertex_group", ob, "vertex_groups", text="")

        col = split.column()
        col.label(text="UV Map:")
        col.prop_search(md, "uv_layer", ob.data, "uv_layers", text="")

    def WIREFRAME(self, layout, ob, md):
        has_vgroup = bool(md.vertex_group)

        split = layout.split()

        col = split.column()
        col.prop(md, "thickness", text="Thickness")

        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = row.row(align=True)
        sub.active = has_vgroup
        sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
        row = col.row(align=True)
        row.active = has_vgroup
        row.prop(md, "thickness_vertex_group", text="Factor")

        col.prop(md, "use_crease", text="Crease Edges")
        row = col.row()
        row.active = md.use_crease
        row.prop(md, "crease_weight", text="Crease Weight")

        col = split.column()

        col.prop(md, "offset")
        col.prop(md, "use_even_offset", text="Even Thickness")
        col.prop(md, "use_relative_offset", text="Relative Thickness")
        col.prop(md, "use_boundary", text="Boundary")
        col.prop(md, "use_replace", text="Replace Original")

        col.prop(md, "material_offset", text="Material Offset")

    def DATA_TRANSFER(self, layout, ob, md):
        row = layout.row(align=True)
        row.prop(md, "object")
        sub = row.row(align=True)
        sub.active = bool(md.object)
        sub.prop(md, "use_object_transform", text="", icon='GROUP')

        layout.separator()

        split = layout.split(0.333)
        split.prop(md, "use_vert_data")
        use_vert = md.use_vert_data
        row = split.row()
        row.active = use_vert
        row.prop(md, "vert_mapping", text="")
        if use_vert:
            col = layout.column(align=True)
            split = col.split(0.333, align=True)
            sub = split.column(align=True)
            sub.prop(md, "data_types_verts")
            sub = split.column(align=True)
            row = sub.row(align=True)
            row.prop(md, "layers_vgroup_select_src", text="")
            row.label(icon='RIGHTARROW')
            row.prop(md, "layers_vgroup_select_dst", text="")
            row = sub.row(align=True)
            row.label("", icon='NONE')

        layout.separator()

        split = layout.split(0.333)
        split.prop(md, "use_edge_data")
        use_edge = md.use_edge_data
        row = split.row()
        row.active = use_edge
        row.prop(md, "edge_mapping", text="")
        if use_edge:
            col = layout.column(align=True)
            split = col.split(0.333, align=True)
            sub = split.column(align=True)
            sub.prop(md, "data_types_edges")

        layout.separator()

        split = layout.split(0.333)
        split.prop(md, "use_loop_data")
        use_loop = md.use_loop_data
        row = split.row()
        row.active = use_loop
        row.prop(md, "loop_mapping", text="")
        if use_loop:
            col = layout.column(align=True)
            split = col.split(0.333, align=True)
            sub = split.column(align=True)
            sub.prop(md, "data_types_loops")
            sub = split.column(align=True)
            row = sub.row(align=True)
            row.label("", icon='NONE')
            row = sub.row(align=True)
            row.prop(md, "layers_vcol_select_src", text="")
            row.label(icon='RIGHTARROW')
            row.prop(md, "layers_vcol_select_dst", text="")
            row = sub.row(align=True)
            row.prop(md, "layers_uv_select_src", text="")
            row.label(icon='RIGHTARROW')
            row.prop(md, "layers_uv_select_dst", text="")
            col.prop(md, "islands_precision")

        layout.separator()

        split = layout.split(0.333)
        split.prop(md, "use_poly_data")
        use_poly = md.use_poly_data
        row = split.row()
        row.active = use_poly
        row.prop(md, "poly_mapping", text="")
        if use_poly:
            col = layout.column(align=True)
            split = col.split(0.333, align=True)
            sub = split.column(align=True)
            sub.prop(md, "data_types_polys")

        layout.separator()

        split = layout.split()
        col = split.column()
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.active = md.use_max_distance
        sub.prop(md, "max_distance")
        row.prop(md, "use_max_distance", text="", icon='STYLUS_PRESSURE')

        col = split.column()
        col.prop(md, "ray_radius")

        layout.separator()

        split = layout.split()
        col = split.column()
        col.prop(md, "mix_mode")
        col.prop(md, "mix_factor")

        col = split.column()
        row = col.row()
        row.active = bool(md.object)
        row.operator("object.datalayout_transfer", text="Generate Data Layers")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = row.row(align=True)
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

    def NORMAL_EDIT(self, layout, ob, md):
        has_vgroup = bool(md.vertex_group)
        do_polynors_fix = not md.no_polynors_fix
        needs_object_offset = (((md.mode == 'RADIAL') and not md.target) or
                               ((md.mode == 'DIRECTIONAL') and md.use_direction_parallel))

        row = layout.row()
        row.prop(md, "mode", expand=True)

        split = layout.split()

        col = split.column()
        col.prop(md, "target", text="")
        sub = col.column(align=True)
        sub.active = needs_object_offset
        sub.prop(md, "offset")
        row = col.row(align=True)

        col = split.column()
        row = col.row()
        row.active = (md.mode == 'DIRECTIONAL')
        row.prop(md, "use_direction_parallel")

        subcol = col.column(align=True)
        subcol.label("Mix Mode:")
        subcol.prop(md, "mix_mode", text="")
        subcol.prop(md, "mix_factor")
        row = subcol.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = row.row(align=True)
        sub.active = has_vgroup
        sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
        row = subcol.row(align=True)
        row.prop(md, "mix_limit")
        row.prop(md, "no_polynors_fix", text="", icon='UNLOCKED' if do_polynors_fix else 'LOCKED')

    def CORRECTIVE_SMOOTH(self, layout, ob, md):
        is_bind = md.is_bind

        layout.prop(md, "factor", text="Factor")
        layout.prop(md, "iterations")

        row = layout.row()
        row.prop(md, "smooth_type")

        split = layout.split()

        col = split.column()
        col.label(text="Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

        col = split.column()
        col.prop(md, "use_only_smooth")
        col.prop(md, "use_pin_boundary")

        layout.prop(md, "rest_source")
        if md.rest_source == 'BIND':
            layout.operator("object.correctivesmooth_bind", text="Unbind" if is_bind else "Bind")

    def WEIGHTED_NORMAL(self, layout, ob, md):
        layout.label("Weighting Mode:")
        split = layout.split(align=True)
        col = split.column(align=True)
        col.prop(md, "mode", text="")
        col.prop(md, "weight", text="Weight")
        col.prop(md, "keep_sharp")

        col = split.column(align=True)
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.active = bool(md.vertex_group)
        row.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')
        col.prop(md, "thresh", text="Threshold")
        col.prop(md, "face_influence")


class DATA_PT_gpencil_modifiers(ModifierButtonsPanel, Panel):
    bl_label = "Modifiers"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.type == 'GPENCIL'

    def draw(self, context):
        layout = self.layout

        ob = context.object

        layout.operator_menu_enum("object.gpencil_modifier_add", "type")

        for md in ob.grease_pencil_modifiers:
            box = layout.template_greasepencil_modifier(md)
            if box:
                # match enum type to our functions, avoids a lookup table.
                getattr(self, md.type)(box, ob, md)

    # the mt.type enum is (ab)used for a lookup on function names
    # ...to avoid lengthy if statements
    # so each type must have a function here.

    def GP_NOISE(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        row = col.row(align=True)
        row.prop(md, "factor")
        row.prop(md, "random", text="", icon="TIME", toggle=True)
        row = col.row()
        row.enabled = md.random
        row.prop(md, "step")
        col.prop(md, "full_stroke")
        col.prop(md, "move_extreme")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        col.label("Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex", text="", icon="ARROW_LEFTRIGHT")

        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        row = layout.row(align=True)
        row.label("Affect:")
        row = layout.row(align=True)
        row.prop(md, "affect_position", text="Position", icon='MESH_DATA', toggle=True)
        row.prop(md, "affect_strength", text="Strength", icon='COLOR', toggle=True)
        row.prop(md, "affect_thickness", text="Thickness", icon='LINE_DATA', toggle=True)
        row.prop(md, "affect_uv", text="UV", icon='MOD_UVPROJECT', toggle=True)

    def GP_SMOOTH(self, layout, ob, md):
        gpd = ob.data
        row = layout.row(align=False)
        row.prop(md, "factor")
        row.prop(md, "step")

        split = layout.split()
        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        col = split.column()
        col.label("Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex", text="", icon="ARROW_LEFTRIGHT")

        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        row = layout.row(align=True)
        row.label("Affect:")
        row = layout.row(align=True)
        row.prop(md, "affect_position", text="Position", icon='MESH_DATA', toggle=True)
        row.prop(md, "affect_strength", text="Strength", icon='COLOR', toggle=True)
        row.prop(md, "affect_thickness", text="Thickness", icon='LINE_DATA', toggle=True)
        row.prop(md, "affect_uv", text="UV", icon='MOD_UVPROJECT', toggle=True)

    def GP_SUBDIV(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        row = col.row(align=True)
        row.prop(md, "level")
        row.prop(md, "simple", text="", icon="PARTICLE_POINT")
        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

    def GP_SIMPLIFY(self, layout, ob, md):
        gpd = ob.data

        row = layout.row()
        row.prop(md, "mode")

        split = layout.split()

        col = split.column()
        col.label("Settings:")
        row = col.row(align=True)
        row.enabled = md.mode == 'FIXED'
        row.prop(md, "step")

        row = col.row(align=True)
        row.enabled = not md.mode == 'FIXED'
        row.prop(md, "factor")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')

        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

    def GP_THICK(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        row = col.row(align=True)
        row.prop(md, "thickness")
        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        col.prop(md, "normalize_thickness")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        col.label("Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex", text="", icon="ARROW_LEFTRIGHT")

        if not md.normalize_thickness:
            split = layout.split()
            col = split.column()
            col.prop(md, "use_custom_curve")

            if md.use_custom_curve:
                col.template_curve_mapping(md, "curve")

    def GP_TINT(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        col.prop(md, "color")
        col.prop(md, "factor")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")
        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        row = layout.row()
        row.prop(md, "create_materials")
        row.prop(md, "modify_color")
        

    def GP_COLOR(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        col.label("Color:")
        col.prop(md, "hue", text="H")
        col.prop(md, "saturation", text="S")
        col.prop(md, "value", text="V")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")
        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        row = layout.row()
        row.prop(md, "create_materials")
        row.prop(md, "modify_color")

    def GP_OPACITY(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        col.label("Opacity:")
        col.prop(md, "factor")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        col.label("Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex", text="", icon="ARROW_LEFTRIGHT")

        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")
        
        row = layout.row()
        row.prop(md, "create_materials")
        row.prop(md, "modify_color")

    def GP_INSTANCE(self, layout, ob, md):
        gpd = ob.data

        col = layout.column()
        col.prop(md, "count")

        split = layout.split()
        col = split.column()
        col.label("Offset:")
        col.prop(md, "offset", text="")

        col = split.column()
        col.label("Shift:")
        col.prop(md, "shift", text="")
        row = col.row(align=True)
        row.prop(md, "lock_axis", expand=True)

        split = layout.split()
        col = split.column()
        col.label("Rotation:")
        col.prop(md, "rotation", text="")
        col.separator()
        row = col.row(align=True)
        row.prop(md, "random_rot", text="", icon="TIME", toggle=True)
        row.prop(md, "rot_factor", text="")

        col = split.column()
        col.label("Scale:")
        col.prop(md, "scale", text="")
        col.separator()
        row = col.row(align=True)
        row.prop(md, "random_scale", text="", icon="TIME", toggle=True)
        row.prop(md, "scale_factor", text="")

        split = layout.split()
        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")
        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

    def GP_BUILD(self, layout, ob, md):
        gpd = ob.data

        split = layout.split()

        col = split.column()
        col.prop(md, "mode")
        if md.mode == 'CONCURRENT':
            col.prop(md, "concurrent_time_alignment")
        else:
            col.separator()  # For spacing
            col.separator()
        col.separator()

        col.prop(md, "transition")
        sub = col.column(align=True)
        sub.prop(md, "start_delay")
        sub.prop(md, "length")

        col = split.column(align=True)
        col.prop(md, "use_restrict_frame_range")
        sub = col.column(align=True)
        sub.active = md.use_restrict_frame_range
        sub.prop(md, "frame_start", text="Start")
        sub.prop(md, "frame_end", text="End")
        col.separator()

        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

    def GP_LATTICE(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        col.label("Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex", text="", icon="ARROW_LEFTRIGHT")

        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        layout.separator()
        layout.prop(md, "strength", slider=True)

    def GP_MIRROR(self, layout, ob, md):
        gpd = ob.data

        row = layout.row(align=True)
        row.prop(md, "x_axis")
        row.prop(md, "y_axis")
        row.prop(md, "z_axis")

        # GPXX: Not implemented yet
        # layout.separator()
        # layout.prop(md, "clip")

        layout.label("Layer:")
        row = layout.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        row = layout.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        layout.label(text="Object:")
        layout.prop(md, "object", text="")

    def GP_HOOK(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        if md.object and md.object.type == 'ARMATURE':
            col.label(text="Bone:")
            col.prop_search(md, "subtarget", md.object.data, "bones", text="")

        col = split.column()
        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        col.label("Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex", text="", icon="ARROW_LEFTRIGHT")

        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")

        use_falloff = (md.falloff_type != 'NONE')
        split = layout.split()

        layout.separator()

        row = layout.row(align=True)
        if use_falloff:
            row.prop(md, "falloff_radius")
        row.prop(md, "strength", slider=True)
        layout.prop(md, "falloff_type")

        col = layout.column()
        if use_falloff:
            if md.falloff_type == 'CURVE':
                col.template_curve_mapping(md, "falloff_curve")

        split = layout.split()

        col = split.column()
        col.prop(md, "use_falloff_uniform")

    def GP_OFFSET(self, layout, ob, md):
        gpd = ob.data
        split = layout.split()

        col = split.column()
        col.prop(md, "location")
        col.prop(md, "scale")

        col = split.column()
        col.prop(md, "rotation")

        col.label("Layer:")
        row = col.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon="ARROW_LEFTRIGHT")

        col.label("Vertex Group:")
        row = col.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        row.prop(md, "invert_vertex", text="", icon="ARROW_LEFTRIGHT")

        row = col.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_pass", text="", icon="ARROW_LEFTRIGHT")


classes = (
    DATA_PT_modifiers,
    DATA_PT_gpencil_modifiers,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
