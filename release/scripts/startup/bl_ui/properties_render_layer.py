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
from bpy.types import Menu, Panel


class RenderLayerButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render_layer"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.render.engine in cls.COMPAT_ENGINES)


class RENDERLAYER_PT_layers(RenderLayerButtonsPanel, Panel):
    bl_label = "Layers"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render

        row = layout.row()
        row.template_list(rd, "layers", rd.layers, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("scene.render_layer_add", icon='ZOOMIN', text="")
        col.operator("scene.render_layer_remove", icon='ZOOMOUT', text="")

        row = layout.row()
        rl = rd.layers.active
        if rl:
            row.prop(rl, "name")
        row.prop(rd, "use_single_layer", text="", icon_only=True)


class RENDERLAYER_PT_layer_options(RenderLayerButtonsPanel, Panel):
    bl_label = "Layer"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        rl = rd.layers.active

        split = layout.split()

        col = split.column()
        col.prop(scene, "layers", text="Scene")
#        col.label(text="")
        col.prop(rl, "light_override", text="Light")
        col.prop(rl, "material_override", text="Material")

        col = split.column()
        col.prop(rl, "layers", text="Layer")
        col.label(text="Mask Layers:")
        col.prop(rl, "layers_zmask", text="")

        layout.separator()
        layout.label(text="Include:")

        split = layout.split()

        col = split.column()
        col.prop(rl, "use_zmask")
        row = col.row()
        row.prop(rl, "invert_zmask", text="Negate")
        row.active = rl.use_zmask
        col.prop(rl, "use_all_z")

        col = split.column()
        col.prop(rl, "use_solid")
        col.prop(rl, "use_halo")
        col.prop(rl, "use_ztransp")
        col.prop(rl, "use_sky")

        col = split.column()
        col.prop(rl, "use_edge_enhance")
        col.prop(rl, "use_strand")
        row = col.row()
        row.prop(rl, "use_freestyle")
        row.active = rd.use_freestyle


class RENDERLAYER_PT_layer_passes(RenderLayerButtonsPanel, Panel):
    bl_label = "Render Passes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_pass_type_buttons(self, box, rl, pass_type):
        # property names
        use_pass_type = "use_pass_" + pass_type
        exclude_pass_type = "exclude_" + pass_type
        # draw pass type buttons
        row = box.row()
        row.prop(rl, use_pass_type)
        row.prop(rl, exclude_pass_type, text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        rl = rd.layers.active

        split = layout.split()

        col = split.column()
        col.prop(rl, "use_pass_combined")
        col.prop(rl, "use_pass_z")
        col.prop(rl, "use_pass_vector")
        col.prop(rl, "use_pass_normal")
        col.prop(rl, "use_pass_uv")
        col.prop(rl, "use_pass_mist")
        col.prop(rl, "use_pass_object_index")
        col.prop(rl, "use_pass_material_index")
        col.prop(rl, "use_pass_color")

        col = split.column()
        col.prop(rl, "use_pass_diffuse")
        self.draw_pass_type_buttons(col, rl, "specular")
        self.draw_pass_type_buttons(col, rl, "shadow")
        self.draw_pass_type_buttons(col, rl, "emit")
        self.draw_pass_type_buttons(col, rl, "ambient_occlusion")
        self.draw_pass_type_buttons(col, rl, "environment")
        self.draw_pass_type_buttons(col, rl, "indirect")
        self.draw_pass_type_buttons(col, rl, "reflection")
        self.draw_pass_type_buttons(col, rl, "refraction")


class RENDER_MT_lineset_specials(Menu):
    bl_label = "Lineset Specials"

    def draw(self, context):
        layout = self.layout
        layout.operator("scene.freestyle_lineset_copy", icon='COPYDOWN')
        layout.operator("scene.freestyle_lineset_paste", icon='PASTEDOWN')


class RENDERLAYER_PT_freestyle(RenderLayerButtonsPanel, Panel):
    bl_label = "Freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        if not (scene and (scene.render.engine in cls.COMPAT_ENGINES)):
            return False
        rd = scene.render
        rl = rd.layers.active
        return rd.use_freestyle and rl

    def draw(self, context):
        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings

        layout = self.layout
        layout.enabled = rl.use_freestyle
        layout.prop(freestyle, "mode", text="Control mode")

        col = layout.column()
        col.label(text="Edge Detection Options:")
        split = col.split()
        sub = split.column()
        sub.prop(freestyle, "crease_angle")
        sub.prop(freestyle, "use_culling")
        sub = split.column()
        sub.prop(freestyle, "use_smoothness")
        sub.prop(freestyle, "use_material_boundaries")
        col.prop(freestyle, "use_advanced_options")
        # Advanced options are hidden by default to warn new users
        if freestyle.use_advanced_options:
            split = col.split()
            sub = split.column()
            sub.enabled = freestyle.use_advanced_options
            if freestyle.mode == "SCRIPT":
                sub.prop(freestyle, "use_ridges_and_valleys")
            sub.prop(freestyle, "sphere_radius")
            sub = split.column()
            sub.enabled = freestyle.use_advanced_options
            if freestyle.mode == "SCRIPT":
                sub.prop(freestyle, "use_suggestive_contours")
            sub.prop(freestyle, "kr_derivative_epsilon")

        if freestyle.mode == "SCRIPT":
            split = layout.split()
            split.label("Style modules:")
            split.operator("scene.freestyle_module_add", text="Add")
            for i, module in enumerate(freestyle.modules):
                box = layout.box()
                box.context_pointer_set("freestyle_module", module)
                row = box.row(align=True)
                row.prop(module, "use", text="")
                row.prop(module, "module_path", text="")
                row.operator("scene.freestyle_module_remove", icon='X', text="")
                row.operator("scene.freestyle_module_move", icon='TRIA_UP', text="").direction = 'UP'
                row.operator("scene.freestyle_module_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class RENDERLAYER_PT_freestyle_lineset(RenderLayerButtonsPanel, Panel):
    bl_label = "Freestyle Line Set"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        if not (scene and (scene.render.engine in cls.COMPAT_ENGINES)):
            return False
        rd = scene.render
        rl = rd.layers.active
        return rd.use_freestyle and rl and rl.freestyle_settings.mode == "EDITOR"

    def draw_edge_type_buttons(self, box, lineset, edge_type):
        # property names
        select_edge_type = "select_" + edge_type
        exclude_edge_type = "exclude_" + edge_type
        # draw edge type buttons
        row = box.row(align=True)
        row.prop(lineset, select_edge_type)
        sub = row.column()
        sub.prop(lineset, exclude_edge_type, text="")
        sub.enabled = getattr(lineset, select_edge_type)

    def draw(self, context):
        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings
        lineset = freestyle.linesets.active

        layout = self.layout
        layout.enabled = rl.use_freestyle

        split = layout.split()

        col = split.column()
        row = col.row()
        rows = 2
        if lineset:
            rows = 5
        row.template_list(freestyle, "linesets", freestyle.linesets, "active_index", rows=rows)

        sub = row.column()
        subsub = sub.column(align=True)
        subsub.operator("scene.freestyle_lineset_add", icon='ZOOMIN', text="")
        subsub.operator("scene.freestyle_lineset_remove", icon='ZOOMOUT', text="")
        subsub.menu("RENDER_MT_lineset_specials", icon='DOWNARROW_HLT', text="")
        if lineset:
            sub.separator()
            subsub = sub.column(align=True)
            subsub.operator("scene.freestyle_lineset_move", icon='TRIA_UP', text="").direction = 'UP'
            subsub.operator("scene.freestyle_lineset_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            #col = split.column()
            col.prop(lineset, "name")

            col.prop(lineset, "select_by_visibility")
            if lineset.select_by_visibility:
                sub = col.row(align=True)
                sub.prop(lineset, "visibility", expand=True)
                if lineset.visibility == "RANGE":
                    sub = col.row(align=True)
                    sub.prop(lineset, "qi_start")
                    sub.prop(lineset, "qi_end")
                col.separator() # XXX

            col.prop(lineset, "select_by_edge_types")
            if lineset.select_by_edge_types:
                row = col.row()
                row.prop(lineset, "edge_type_negation", expand=True)
                row = col.row()
                row.prop(lineset, "edge_type_combination", expand=True)

                row = col.row()
                sub = row.column()
                self.draw_edge_type_buttons(sub, lineset, "silhouette")
                self.draw_edge_type_buttons(sub, lineset, "border")
                self.draw_edge_type_buttons(sub, lineset, "contour")
                self.draw_edge_type_buttons(sub, lineset, "suggestive_contour")
                self.draw_edge_type_buttons(sub, lineset, "ridge_valley")
                sub = row.column()
                self.draw_edge_type_buttons(sub, lineset, "crease")
                self.draw_edge_type_buttons(sub, lineset, "edge_mark")
                self.draw_edge_type_buttons(sub, lineset, "external_contour")
                self.draw_edge_type_buttons(sub, lineset, "material_boundary")
                col.separator() # XXX

            col.prop(lineset, "select_by_face_marks")
            if lineset.select_by_face_marks:
                row = col.row()
                row.prop(lineset, "face_mark_negation", expand=True)
                row = col.row()
                row.prop(lineset, "face_mark_condition", expand=True)
                col.separator() # XXX

            col.prop(lineset, "select_by_group")
            if lineset.select_by_group:
                col.prop(lineset, "group")
                row = col.row()
                row.prop(lineset, "group_negation", expand=True)
                col.separator() # XXX

            col.prop(lineset, "select_by_image_border")


class RENDERLAYER_PT_freestyle_linestyle(RenderLayerButtonsPanel, Panel):
    bl_label = "Freestyle Line Style"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        if not (scene and (scene.render.engine in cls.COMPAT_ENGINES)):
            return False
        rd = scene.render
        rl = rd.layers.active
        return rd.use_freestyle and rl and rl.freestyle_settings.mode == "EDITOR"

    def draw_modifier_box_header(self, box, modifier):
        row = box.row()
        row.context_pointer_set("modifier", modifier)
        if modifier.expanded:
            icon = "TRIA_DOWN"
        else:
            icon = "TRIA_RIGHT"
        row.prop(modifier, "expanded", text="", icon=icon, emboss=False)
        row.label(text=modifier.rna_type.name)
        row.prop(modifier, "name", text="")
        row.prop(modifier, "use", text="")
        row.operator("scene.freestyle_modifier_copy", icon='NONE', text="Copy")
        sub = row.row(align=True)
        sub.operator("scene.freestyle_modifier_move", icon='TRIA_UP', text="").direction = 'UP'
        sub.operator("scene.freestyle_modifier_move", icon='TRIA_DOWN', text="").direction = 'DOWN'
        row.operator("scene.freestyle_modifier_remove", icon='X', text="")

    def draw_modifier_common(self, box, modifier):
        row = box.row()
        row.prop(modifier, "blend", text="")
        row.prop(modifier, "influence")

    def draw_modifier_color_ramp_common(self, box, modifier, has_range):
        box.template_color_ramp(modifier, "color_ramp", expand=True)
        if has_range:
            row = box.row(align=True)
            row.prop(modifier, "range_min")
            row.prop(modifier, "range_max")

    def draw_modifier_curve_common(self, box, modifier, has_range, has_value):
        row = box.row()
        row.prop(modifier, "mapping", text="")
        sub = row.column()
        sub.prop(modifier, "invert")
        if modifier.mapping == "CURVE":
            sub.enabled = False
            box.template_curve_mapping(modifier, "curve")
        if has_range:
            row = box.row(align=True)
            row.prop(modifier, "range_min")
            row.prop(modifier, "range_max")
        if has_value:
            row = box.row(align=True)
            row.prop(modifier, "value_min")
            row.prop(modifier, "value_max")

    def draw_color_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            self.draw_modifier_common(box, modifier)

            if modifier.type == "ALONG_STROKE":
                self.draw_modifier_color_ramp_common(box, modifier, False)

            elif modifier.type == "DISTANCE_FROM_OBJECT":
                box.prop(modifier, "target")
                self.draw_modifier_color_ramp_common(box, modifier, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'COLOR'
                prop.name = modifier.name

            elif modifier.type == "DISTANCE_FROM_CAMERA":
                self.draw_modifier_color_ramp_common(box, modifier, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'COLOR'
                prop.name = modifier.name

            elif modifier.type == "MATERIAL":
                row = box.row()
                row.prop(modifier, "material_attr", text="")
                sub = row.column()
                sub.prop(modifier, "use_ramp")
                if modifier.material_attr in ["DIFF", "SPEC"]:
                    sub.enabled = True
                    show_ramp = modifier.use_ramp
                else:
                    sub.enabled = False
                    show_ramp = True
                if show_ramp:
                    self.draw_modifier_color_ramp_common(box, modifier, False)

    def draw_alpha_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            self.draw_modifier_common(box, modifier)

            if modifier.type == "ALONG_STROKE":
                self.draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == "DISTANCE_FROM_OBJECT":
                box.prop(modifier, "target")
                self.draw_modifier_curve_common(box, modifier, True, False)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'ALPHA'
                prop.name = modifier.name

            elif modifier.type == "DISTANCE_FROM_CAMERA":
                self.draw_modifier_curve_common(box, modifier, True, False)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'ALPHA'
                prop.name = modifier.name

            elif modifier.type == "MATERIAL":
                box.prop(modifier, "material_attr", text="")
                self.draw_modifier_curve_common(box, modifier, False, False)

    def draw_thickness_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            self.draw_modifier_common(box, modifier)

            if modifier.type == "ALONG_STROKE":
                self.draw_modifier_curve_common(box, modifier, False, True)

            elif modifier.type == "DISTANCE_FROM_OBJECT":
                box.prop(modifier, "target")
                self.draw_modifier_curve_common(box, modifier, True, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'THICKNESS'
                prop.name = modifier.name

            elif modifier.type == "DISTANCE_FROM_CAMERA":
                self.draw_modifier_curve_common(box, modifier, True, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'THICKNESS'
                prop.name = modifier.name

            elif modifier.type == "MATERIAL":
                box.prop(modifier, "material_attr", text="")
                self.draw_modifier_curve_common(box, modifier, False, True)

            elif modifier.type == "CALLIGRAPHY":
                col = box.column()
                col.prop(modifier, "orientation")
                col.prop(modifier, "min_thickness")
                col.prop(modifier, "max_thickness")

    def draw_geometry_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()

            if modifier.type == "SAMPLING":
                box.prop(modifier, "sampling")

            elif modifier.type == "BEZIER_CURVE":
                box.prop(modifier, "error")

            elif modifier.type == "SINUS_DISPLACEMENT":
                box.prop(modifier, "wavelength")
                box.prop(modifier, "amplitude")
                box.prop(modifier, "phase")

            elif modifier.type == "SPATIAL_NOISE":
                box.prop(modifier, "amplitude")
                box.prop(modifier, "scale")
                box.prop(modifier, "octaves")
                sub = box.row()
                sub.prop(modifier, "smooth")
                sub.prop(modifier, "pure_random")

            elif modifier.type == "PERLIN_NOISE_1D":
                box.prop(modifier, "frequency")
                box.prop(modifier, "amplitude")
                box.prop(modifier, "octaves")
                box.prop(modifier, "angle")
                box.prop(modifier, "seed")

            elif modifier.type == "PERLIN_NOISE_2D":
                box.prop(modifier, "frequency")
                box.prop(modifier, "amplitude")
                box.prop(modifier, "octaves")
                box.prop(modifier, "angle")
                box.prop(modifier, "seed")

            elif modifier.type == "BACKBONE_STRETCHER":
                box.prop(modifier, "backbone_length")

            elif modifier.type == "TIP_REMOVER":
                box.prop(modifier, "tip_length")

            elif modifier.type == "POLYGONIZATION":
                box.prop(modifier, "error")

            elif modifier.type == "GUIDING_LINES":
                box.prop(modifier, "offset")

            elif modifier.type == "BLUEPRINT":
                row = box.row()
                row.prop(modifier, "shape", expand=True)
                box.prop(modifier, "rounds")
                if modifier.shape in ["CIRCLES", "ELLIPSES"]:
                    box.prop(modifier, "random_radius")
                    box.prop(modifier, "random_center")
                elif modifier.shape == "SQUARES":
                    box.prop(modifier, "backbone_length")
                    box.prop(modifier, "random_backbone")

            elif modifier.type == "2D_OFFSET":
                row = box.row(align=True)
                row.prop(modifier, "start")
                row.prop(modifier, "end")
                row = box.row(align=True)
                row.prop(modifier, "x")
                row.prop(modifier, "y")

            elif modifier.type == "2D_TRANSFORM":
                box.prop(modifier, "pivot")
                if modifier.pivot == "PARAM":
                    box.prop(modifier, "pivot_u")
                elif modifier.pivot == "ABSOLUTE":
                    row = box.row(align=True)
                    row.prop(modifier, "pivot_x")
                    row.prop(modifier, "pivot_y")
                row = box.row(align=True)
                row.prop(modifier, "scale_x")
                row.prop(modifier, "scale_y")
                box.prop(modifier, "angle")

    def draw(self, context):
        rd = context.scene.render
        rl = rd.layers.active
        lineset = rl.freestyle_settings.linesets.active

        layout = self.layout
        layout.enabled = rl.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        layout.template_ID(lineset, "linestyle", new="scene.freestyle_linestyle_new")
        row = layout.row(align=True)
        row.prop(linestyle, "panel", expand=True)
        if linestyle.panel == "STROKES":
            # Chaining
            col = layout.column()
            col.prop(linestyle, "use_chaining", text="Chaining:")
            sub = col.column()
            sub.enabled = linestyle.use_chaining
            sub.prop(linestyle, "chaining", text="")
            if linestyle.chaining == "PLAIN":
                sub.prop(linestyle, "same_object")
            elif linestyle.chaining == "SKETCHY":
                subsub = sub.row()
                subsub.prop(linestyle, "same_object")
                subsub.prop(linestyle, "rounds")
            # Splitting
            layout.label(text="Splitting:")
            row = layout.row()
            col = row.column()
            sub = col.row(align=True)
            sub.prop(linestyle, "use_min_angle", text="")
            subsub = sub.row()
            subsub.enabled = linestyle.use_min_angle
            subsub.prop(linestyle, "min_angle")
            sub = col.row(align=True)
            sub.prop(linestyle, "use_max_angle", text="")
            subsub = sub.row()
            subsub.enabled = linestyle.use_max_angle
            subsub.prop(linestyle, "max_angle")
            col.prop(linestyle, "use_split_pattern", text="Split Pattern")
            col = row.column()
            sub = col.row(align=True)
            sub.prop(linestyle, "use_split_length", text="")
            subsub = sub.row()
            subsub.enabled = linestyle.use_split_length
            subsub.prop(linestyle, "split_length", text="2D Length")
            col.prop(linestyle, "material_boundary")
            row = layout.row(align=True)
            row.enabled = linestyle.use_split_pattern
            row.prop(linestyle, "split_dash1")
            row.prop(linestyle, "split_gap1")
            row.prop(linestyle, "split_dash2")
            row.prop(linestyle, "split_gap2")
            row.prop(linestyle, "split_dash3")
            row.prop(linestyle, "split_gap3")

            # Selection
            layout.label(text="Selection:")
            row = layout.row()
            col = row.column()
            sub = col.row(align=True)
            sub.prop(linestyle, "use_min_length", text="")
            subsub = sub.row()
            subsub.enabled = linestyle.use_min_length
            subsub.prop(linestyle, "min_length")
            col = row.column()
            sub = col.row(align=True)
            sub.prop(linestyle, "use_max_length", text="")
            subsub = sub.row()
            subsub.enabled = linestyle.use_max_length
            subsub.prop(linestyle, "max_length")
            # Caps
            layout.label(text="Caps:")

            row = layout.row(align=True)
            row.prop(linestyle, "caps", expand=True)

            layout.prop(linestyle, "use_dashed_line")
            row = layout.row(align=True)
            row.enabled = linestyle.use_dashed_line
            row.prop(linestyle, "dash1")
            row.prop(linestyle, "gap1")
            row.prop(linestyle, "dash2")
            row.prop(linestyle, "gap2")
            row.prop(linestyle, "dash3")
            row.prop(linestyle, "gap3")

        elif linestyle.panel == "COLOR":
            col = layout.column()
            col.label(text="Base Color:")
            col.prop(linestyle, "color", text="")
            col = layout.column()
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_color_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.color_modifiers:
                self.draw_color_modifier(context, modifier)
        elif linestyle.panel == "ALPHA":
            col = layout.column()
            col.label(text="Base Transparency:")
            col.prop(linestyle, "alpha")
            col = layout.column()
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_alpha_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.alpha_modifiers:
                self.draw_alpha_modifier(context, modifier)
        elif linestyle.panel == "THICKNESS":
            col = layout.column()
            col.label(text="Base Thickness:")
            col.prop(linestyle, "thickness")
            col = layout.column()
            row = col.row()
            row.prop(linestyle, "thickness_position", expand=True)
            row = col.row()
            row.prop(linestyle, "thickness_ratio")
            row.enabled = linestyle.thickness_position == "RELATIVE"
            col = layout.column()
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_thickness_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.thickness_modifiers:
                self.draw_thickness_modifier(context, modifier)
        elif linestyle.panel == "GEOMETRY":
            col = layout.column()
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_geometry_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.geometry_modifiers:
                self.draw_geometry_modifier(context, modifier)
        elif linestyle.panel == "MISC":
            pass


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
