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
        layout.operator_menu_enum("object.modifier_add", "type")
        layout.template_modifiers()


class DATA_PT_gpencil_modifiers(ModifierButtonsPanel, Panel):
    bl_label = "Modifiers"

    def check_conflicts(self, layout, ob):
        for md in ob.grease_pencil_modifiers:
            if md.type == 'GP_TIME':
                row = layout.row()
                row.label(text="Build and Time Offset modifier not compatible", icon='ERROR')
                break

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

    def gpencil_masking(self, layout, ob, md, use_vertex, use_curve=False):
        gpd = ob.data
        layout.separator()
        layout.label(text="Influence Filters:")

        split = layout.split(factor=0.25)

        col1 = split.column()

        col1.label(text="Layer:")
        col1.label(text="Material:")
        if use_vertex:
            col1.label(text="Vertex Group:")

        col2 = split.column()

        split = col2.split(factor=0.6)
        row = split.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon='ARROW_LEFTRIGHT')

        row = split.row(align=True)
        row.prop(md, "layer_pass", text="Pass")
        row.prop(md, "invert_layer_pass", text="", icon='ARROW_LEFTRIGHT')

        split = col2.split(factor=0.6)

        row = split.row(align=True)

        valid = md.material in (slot.material for slot in ob.material_slots) or md.material is None
        if valid:
            icon = 'SHADING_TEXTURE'
        else:
            icon = 'ERROR'

        row.alert = not valid
        row.prop_search(md, "material", gpd, "materials", text="", icon=icon)
        row.prop(md, "invert_materials", text="", icon='ARROW_LEFTRIGHT')

        row = split.row(align=True)
        row.prop(md, "pass_index", text="Pass")
        row.prop(md, "invert_material_pass", text="", icon='ARROW_LEFTRIGHT')

        if use_vertex:
            row = col2.row(align=True)
            row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
            row.prop(md, "invert_vertex", text="", icon='ARROW_LEFTRIGHT')

        if use_curve:
            col = layout.column()
            col.separator()
            col.prop(md, "use_custom_curve")
            if md.use_custom_curve:
                col.template_curve_mapping(md, "curve")

    def GP_NOISE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        row = col.row(align=True)
        row.prop(md, "factor", text="Position")
        row = col.row(align=True)
        row.prop(md, "factor_strength", text="Strength")
        row = col.row(align=True)
        row.prop(md, "factor_thickness", text="Thickness")
        row = col.row(align=True)
        row.prop(md, "factor_uvs", text="UV")

        col.separator()
        row = col.row(align=True)
        row.prop(md, "random", text="", icon='TIME', toggle=True)

        subrow = row.row(align=True)
        subrow.enabled = md.random
        subrow.prop(md, "step")
        subrow.prop(md, "seed")

        col.separator()
        col.prop(md, "noise_scale")

        self.gpencil_masking(layout, ob, md, True, True)

    def GP_SMOOTH(self, layout, ob, md):
        col = layout.column()
        col.prop(md, "factor")
        col.prop(md, "step", text="Repeat")

        col.label(text="Affect:")
        row = col.row(align=True)
        row.prop(md, "use_edit_position", text="Position", toggle=True)
        row.prop(md, "use_edit_strength", text="Strength", toggle=True)
        row.prop(md, "use_edit_thickness", text="Thickness", toggle=True)
        row.prop(md, "use_edit_uv", text="UV", toggle=True)

        self.gpencil_masking(layout, ob, md, True, True)

    def GP_SUBDIV(self, layout, ob, md):
        layout.row().prop(md, "subdivision_type", expand=True)
        split = layout.split()
        col = split.column()
        row = col.row(align=True)
        row.prop(md, "level", text="Subdivisions")

        self.gpencil_masking(layout, ob, md, False)

    def GP_SIMPLIFY(self, layout, ob, md):
        gpd = ob.data

        row = layout.row()
        row.prop(md, "mode")

        split = layout.split()

        col = split.column()
        col.label(text="Settings:")

        if md.mode == 'FIXED':
            col.prop(md, "step")
        elif md.mode == 'ADAPTIVE':
            col.prop(md, "factor")
        elif md.mode == 'SAMPLE':
            col.prop(md, "length")
        elif md.mode == 'MERGE':
            col.prop(md, "distance")

        self.gpencil_masking(layout, ob, md, False)

    def GP_THICK(self, layout, ob, md):
        col = layout.column()

        col.prop(md, "normalize_thickness")

        if md.normalize_thickness:
            col.prop(md, "thickness")
        else:
            col.prop(md, "thickness_factor")

        self.gpencil_masking(layout, ob, md, True, True)

    def GP_TEXTURE(self, layout, ob, md):
        col = layout.column()

        col.prop(md, "mode")
        if md.mode in {'STROKE', 'STROKE_AND_FILL'}:
            col.label(text="Stroke Texture:")
            col.prop(md, "fit_method")
            col.prop(md, "uv_offset")
            col.prop(md, "uv_scale")

        if md.mode == 'STROKE_AND_FILL':
            col.separator()

        if md.mode in {'FILL', 'STROKE_AND_FILL'}:
            col.label(text="Fill Texture:")
            col.prop(md, "fill_rotation", text="Rotation")
            col.prop(md, "fill_offset", text="Location")
            col.prop(md, "fill_scale", text="Scale")

        self.gpencil_masking(layout, ob, md, True)

    def GP_TINT(self, layout, ob, md):
        layout.row().prop(md, "tint_type", expand=True)

        if md.tint_type == 'UNIFORM':
            col = layout.column()
            col.prop(md, "color")

            col.separator()
            col.prop(md, "factor")

        if md.tint_type == 'GRADIENT':
            col = layout.column()
            col.label(text="Colors:")
            col.template_color_ramp(md, "colors")

            col.separator()

            col.label(text="Object:")
            col.prop(md, "object", text="")

            col.separator()
            row = col.row(align=True)
            row.prop(md, "radius")
            row.prop(md, "factor")

        col.separator()
        col.prop(md, "vertex_mode")

        self.gpencil_masking(layout, ob, md, True, True)

    def GP_TIME(self, layout, ob, md):
        gpd = ob.data

        row = layout.row()
        row.prop(md, "mode", text="Mode")

        row = layout.row()
        if md.mode == 'FIX':
            txt = "Frame"
        else:
            txt = "Frame Offset"
        row.prop(md, "offset", text=txt)

        row = layout.row()
        row.enabled = md.mode != 'FIX'
        row.prop(md, "frame_scale")

        row = layout.row()
        row.separator()

        row = layout.row()
        row.enabled = md.mode != 'FIX'
        row.prop(md, "use_custom_frame_range")

        row = layout.row(align=True)
        row.enabled = md.mode != 'FIX' and md.use_custom_frame_range is True
        row.prop(md, "frame_start")
        row.prop(md, "frame_end")

        row = layout.row()
        row.enabled = md.mode != 'FIX'
        row.prop(md, "use_keep_loop")

        row = layout.row()
        row.label(text="Layer:")
        row = layout.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon='ARROW_LEFTRIGHT')

        row = layout.row(align=True)
        row.prop(md, "layer_pass", text="Pass")
        row.prop(md, "invert_layer_pass", text="", icon='ARROW_LEFTRIGHT')

    def GP_COLOR(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Color:")
        col.prop(md, "hue", text="H", slider=True)
        col.prop(md, "saturation", text="S", slider=True)
        col.prop(md, "value", text="V", slider=True)

        row = layout.row()
        row.prop(md, "modify_color")

        self.gpencil_masking(layout, ob, md, False, True)

    def GP_OPACITY(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.prop(md, "modify_color")

        if md.modify_color == 'HARDNESS':
            col.prop(md, "hardness")
            show = False
        else:
            col.prop(md, "normalize_opacity")
            if md.normalize_opacity is True:
                text="Strength"
            else:
                text="Opacity Factor"

            col.prop(md, "factor", text=text)
            show = True
        self.gpencil_masking(layout, ob, md, show, show)

    def GP_ARRAY(self, layout, ob, md):
        col = layout.column()
        col.prop(md, "count")

        split = layout.split()
        col = split.column()
        col.prop(md, "use_constant_offset", text="Constant Offset")
        subcol = col.column()
        subcol.enabled = md.use_constant_offset
        subcol.prop(md, "constant_offset", text="")

        col.prop(md, "use_object_offset")
        subcol = col.column()
        subcol.enabled = md.use_object_offset
        subcol.prop(md, "offset_object", text="")

        col = split.column()
        col.prop(md, "use_relative_offset", text="Relative Offset")
        subcol = col.column()
        subcol.enabled = md.use_relative_offset
        subcol.prop(md, "relative_offset", text="")

        split = layout.split()
        col = split.column()
        col.label(text="Random Offset:")
        col.prop(md, "random_offset", text="")

        col = split.column()
        col.label(text="Random Rotation:")
        col.prop(md, "random_rotation", text="")

        col = split.column()
        col.label(text="Random Scale:")
        col.prop(md, "random_scale", text="")

        col = layout.column()
        col.prop(md, "seed")
        col.separator()
        col.prop(md, "replace_material", text="Material Override")

        self.gpencil_masking(layout, ob, md, False)

    def GP_BUILD(self, layout, ob, md):
        gpd = ob.data

        split = layout.split()

        col = split.column()
        self.check_conflicts(col, ob)

        col.prop(md, "mode")
        if md.mode == 'CONCURRENT':
            col.prop(md, "concurrent_time_alignment")

        col.separator()
        col.prop(md, "transition")
        sub = col.column(align=True)
        sub.prop(md, "start_delay")
        sub.prop(md, "length")

        col = layout.column(align=True)
        col.prop(md, "use_restrict_frame_range")
        sub = col.column(align=True)
        sub.active = md.use_restrict_frame_range
        sub.prop(md, "frame_start", text="Start")
        sub.prop(md, "frame_end", text="End")

        col.prop(md, "use_percentage")
        sub = col.column(align=True)
        sub.active = md.use_percentage
        sub.prop(md, "percentage_factor")

        layout.label(text="Influence Filters:")

        split = layout.split(factor=0.25)

        col1 = split.column()

        col1.label(text="Layer:")

        col2 = split.column()

        split = col2.split(factor=0.6)
        row = split.row(align=True)
        row.prop_search(md, "layer", gpd, "layers", text="", icon='GREASEPENCIL')
        row.prop(md, "invert_layers", text="", icon='ARROW_LEFTRIGHT')

        row = split.row(align=True)
        row.prop(md, "layer_pass", text="Pass")
        row.prop(md, "invert_layer_pass", text="", icon='ARROW_LEFTRIGHT')

    def GP_LATTICE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")

        layout.prop(md, "strength", slider=True)

        self.gpencil_masking(layout, ob, md, True)

    def GP_MIRROR(self, layout, ob, md):
        row = layout.row(align=True)
        row.prop(md, "x_axis")
        row.prop(md, "y_axis")
        row.prop(md, "z_axis")

        layout.label(text="Mirror Object:")
        layout.prop(md, "object", text="")

        self.gpencil_masking(layout, ob, md, False)

    def GP_HOOK(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        if md.object and md.object.type == 'ARMATURE':
            col.label(text="Bone:")
            col.prop_search(md, "subtarget", md.object.data, "bones", text="")

        use_falloff = (md.falloff_type != 'NONE')

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

        self.gpencil_masking(layout, ob, md, True)

    def GP_OFFSET(self, layout, ob, md):
        split = layout.split()

        split.column().prop(md, "location")
        split.column().prop(md, "rotation")
        split.column().prop(md, "scale")

        self.gpencil_masking(layout, ob, md, True)

    def GP_ARMATURE(self, layout, ob, md):
        split = layout.split()

        col = split.column()
        col.label(text="Object:")
        col.prop(md, "object", text="")
        # col.prop(md, "use_deform_preserve_volume")

        col = split.column()
        col.label(text="Bind To:")
        col.prop(md, "use_vertex_groups", text="Vertex Groups")
        col.prop(md, "use_bone_envelopes", text="Bone Envelopes")

        layout.separator()

        row = layout.row(align=True)
        row.label(text="Vertex Group:")
        row = layout.row(align=True)
        row.prop_search(md, "vertex_group", ob, "vertex_groups", text="")
        sub = row.row(align=True)
        sub.active = bool(md.vertex_group)
        sub.prop(md, "invert_vertex_group", text="", icon='ARROW_LEFTRIGHT')

    def GP_MULTIPLY(self, layout, ob, md):
        col = layout.column()

        col.prop(md, "duplicates")
        subcol = col.column()
        subcol.enabled = md.duplicates > 0
        subcol.prop(md, "distance")
        subcol.prop(md, "offset", slider=True)

        subcol.separator()

        subcol.prop(md, "use_fade")
        if md.use_fade:
            subcol.prop(md, "fading_center")
            subcol.prop(md, "fading_thickness", slider=True)
            subcol.prop(md, "fading_opacity", slider=True)

        self.gpencil_masking(layout, ob, md, False)


classes = (
    DATA_PT_modifiers,
    DATA_PT_gpencil_modifiers,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)
