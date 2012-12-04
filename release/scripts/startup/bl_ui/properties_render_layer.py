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
        layout.active = rl.use_freestyle
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
            sub.active = freestyle.use_advanced_options
            if freestyle.mode == 'SCRIPT':
                sub.prop(freestyle, "use_ridges_and_valleys")
            sub.prop(freestyle, "sphere_radius")
            sub = split.column()
            sub.active = freestyle.use_advanced_options
            if freestyle.mode == 'SCRIPT':
                sub.prop(freestyle, "use_suggestive_contours")
            sub.prop(freestyle, "kr_derivative_epsilon")

        if freestyle.mode == 'SCRIPT':
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
        return rd.use_freestyle and rl and rl.freestyle_settings.mode == 'EDITOR'

    def draw_edge_type_buttons(self, box, lineset, edge_type):
        # property names
        select_edge_type = "select_" + edge_type
        exclude_edge_type = "exclude_" + edge_type
        # draw edge type buttons
        row = box.row(align=True)
        row.prop(lineset, select_edge_type)
        sub = row.column()
        sub.prop(lineset, exclude_edge_type, text="")
        sub.active = getattr(lineset, select_edge_type)

    def draw(self, context):
        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings
        lineset = freestyle.linesets.active

        layout = self.layout
        layout.active = rl.use_freestyle

        col = layout.column()
        row = col.row()
        rows = 5 if lineset else 2
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

            col.prop(lineset, "name")

            col = layout.column()
            col.label(text="Selection By:")
            row = col.row(align=True)
            row.prop(lineset, "select_by_visibility", text="Visibility", toggle=True)
            row.prop(lineset, "select_by_edge_types", text="Edge Types", toggle=True)
            row.prop(lineset, "select_by_face_marks", text="Face Marks", toggle=True)
            row.prop(lineset, "select_by_group", text="Group", toggle=True)
            row.prop(lineset, "select_by_image_border", text="Image Border", toggle=True)

            if lineset.select_by_visibility:
                col.label(text="Visibility:")
                row = col.row(align=True)
                row.prop(lineset, "visibility", expand=True)
                if lineset.visibility == 'RANGE':
                    row = col.row(align=True)
                    row.prop(lineset, "qi_start")
                    row.prop(lineset, "qi_end")

            if lineset.select_by_edge_types:
                col.label(text="Edge Types:")
                row = col.row()
                row.prop(lineset, "edge_type_negation", expand=True)
                row.prop(lineset, "edge_type_combination", expand=True)

                split = col.split()
                sub = split.column()
                self.draw_edge_type_buttons(sub, lineset, "silhouette")
                self.draw_edge_type_buttons(sub, lineset, "border")
                self.draw_edge_type_buttons(sub, lineset, "contour")
                self.draw_edge_type_buttons(sub, lineset, "suggestive_contour")
                self.draw_edge_type_buttons(sub, lineset, "ridge_valley")
                sub = split.column()
                self.draw_edge_type_buttons(sub, lineset, "crease")
                self.draw_edge_type_buttons(sub, lineset, "edge_mark")
                self.draw_edge_type_buttons(sub, lineset, "external_contour")
                self.draw_edge_type_buttons(sub, lineset, "material_boundary")

            if lineset.select_by_face_marks:
                col.label(text="Face Marks:")
                row = col.row()
                row.prop(lineset, "face_mark_negation", expand=True)
                row.prop(lineset, "face_mark_condition", expand=True)

            if lineset.select_by_group:
                col.label(text="Group:")
                row = col.row()
                row.prop(lineset, "group", text="")
                row.prop(lineset, "group_negation", expand=True)


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
        return rd.use_freestyle and rl and rl.freestyle_settings.mode == 'EDITOR'

    def draw_modifier_box_header(self, box, modifier):
        row = box.row()
        row.context_pointer_set("modifier", modifier)
        if modifier.expanded:
            icon = 'TRIA_DOWN'
        else:
            icon = 'TRIA_RIGHT'
        row.prop(modifier, "expanded", text="", icon=icon, emboss=False)
        # TODO: Use icons rather than text label, would save some room!
        row.label(text=modifier.rna_type.name)
        row.prop(modifier, "name", text="")
        if modifier.use:
            icon = 'RESTRICT_RENDER_OFF'
        else:
            icon = 'RESTRICT_RENDER_ON'
        row.prop(modifier, "use", text="", icon=icon)
        sub = row.row(align=True)
        sub.operator("scene.freestyle_modifier_copy", icon='NONE', text="Copy")
        sub.operator("scene.freestyle_modifier_move", icon='TRIA_UP', text="").direction = 'UP'
        sub.operator("scene.freestyle_modifier_move", icon='TRIA_DOWN', text="").direction = 'DOWN'
        sub.operator("scene.freestyle_modifier_remove", icon='X', text="")

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
        if modifier.mapping == 'CURVE':
            sub.active = False
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

            if modifier.type == 'ALONG_STROKE':
                self.draw_modifier_color_ramp_common(box, modifier, False)

            elif modifier.type == 'DISTANCE_FROM_OBJECT':
                box.prop(modifier, "target")
                self.draw_modifier_color_ramp_common(box, modifier, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'COLOR'
                prop.name = modifier.name

            elif modifier.type == 'DISTANCE_FROM_CAMERA':
                self.draw_modifier_color_ramp_common(box, modifier, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'COLOR'
                prop.name = modifier.name

            elif modifier.type == 'MATERIAL':
                row = box.row()
                row.prop(modifier, "material_attr", text="")
                sub = row.column()
                sub.prop(modifier, "use_ramp")
                if modifier.material_attr in {'DIFF', 'SPEC'}:
                    sub.active = True
                    show_ramp = modifier.use_ramp
                else:
                    sub.active = False
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

            if modifier.type == 'ALONG_STROKE':
                self.draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'DISTANCE_FROM_OBJECT':
                box.prop(modifier, "target")
                self.draw_modifier_curve_common(box, modifier, True, False)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'ALPHA'
                prop.name = modifier.name

            elif modifier.type == 'DISTANCE_FROM_CAMERA':
                self.draw_modifier_curve_common(box, modifier, True, False)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'ALPHA'
                prop.name = modifier.name

            elif modifier.type == 'MATERIAL':
                box.prop(modifier, "material_attr", text="")
                self.draw_modifier_curve_common(box, modifier, False, False)

    def draw_thickness_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            self.draw_modifier_common(box, modifier)

            if modifier.type == 'ALONG_STROKE':
                self.draw_modifier_curve_common(box, modifier, False, True)

            elif modifier.type == 'DISTANCE_FROM_OBJECT':
                box.prop(modifier, "target")
                self.draw_modifier_curve_common(box, modifier, True, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'THICKNESS'
                prop.name = modifier.name

            elif modifier.type == 'DISTANCE_FROM_CAMERA':
                self.draw_modifier_curve_common(box, modifier, True, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'THICKNESS'
                prop.name = modifier.name

            elif modifier.type == 'MATERIAL':
                box.prop(modifier, "material_attr", text="")
                self.draw_modifier_curve_common(box, modifier, False, True)

            elif modifier.type == 'CALLIGRAPHY':
                box.prop(modifier, "orientation")
                row = box.row(align=True)
                row.prop(modifier, "min_thickness")
                row.prop(modifier, "max_thickness")

    def draw_geometry_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()

            if modifier.type == 'SAMPLING':
                box.prop(modifier, "sampling")

            elif modifier.type == 'BEZIER_CURVE':
                box.prop(modifier, "error")

            elif modifier.type == 'SINUS_DISPLACEMENT':
                split = box.split()
                col = split.column()
                col.prop(modifier, "wavelength")
                col.prop(modifier, "amplitude")
                col = split.column()
                col.prop(modifier, "phase")

            elif modifier.type == 'SPATIAL_NOISE':
                split = box.split()
                col = split.column()
                col.prop(modifier, "amplitude")
                col.prop(modifier, "scale")
                col.prop(modifier, "octaves")
                col = split.column()
                col.prop(modifier, "smooth")
                col.prop(modifier, "pure_random")

            elif modifier.type == 'PERLIN_NOISE_1D':
                split = box.split()
                col = split.column()
                col.prop(modifier, "frequency")
                col.prop(modifier, "amplitude")
                col.prop(modifier, "seed")
                col = split.column()
                col.prop(modifier, "octaves")
                col.prop(modifier, "angle")

            elif modifier.type == 'PERLIN_NOISE_2D':
                split = box.split()
                col = split.column()
                col.prop(modifier, "frequency")
                col.prop(modifier, "amplitude")
                col.prop(modifier, "seed")
                col = split.column()
                col.prop(modifier, "octaves")
                col.prop(modifier, "angle")

            elif modifier.type == 'BACKBONE_STRETCHER':
                box.prop(modifier, "backbone_length")

            elif modifier.type == 'TIP_REMOVER':
                box.prop(modifier, "tip_length")

            elif modifier.type == 'POLYGONIZATION':
                box.prop(modifier, "error")

            elif modifier.type == 'GUIDING_LINES':
                box.prop(modifier, "offset")

            elif modifier.type == 'BLUEPRINT':
                row = box.row()
                row.prop(modifier, "shape", expand=True)
                box.prop(modifier, "rounds")
                row = box.row()
                if modifier.shape in {'CIRCLES', 'ELLIPSES'}:
                    row.prop(modifier, "random_radius")
                    row.prop(modifier, "random_center")
                elif modifier.shape == 'SQUARES':
                    row.prop(modifier, "backbone_length")
                    row.prop(modifier, "random_backbone")

            elif modifier.type == '2D_OFFSET':
                row = box.row(align=True)
                row.prop(modifier, "start")
                row.prop(modifier, "end")
                row = box.row(align=True)
                row.prop(modifier, "x")
                row.prop(modifier, "y")

            elif modifier.type == '2D_TRANSFORM':
                box.prop(modifier, "pivot")
                if modifier.pivot == 'PARAM':
                    box.prop(modifier, "pivot_u")
                elif modifier.pivot == 'ABSOLUTE':
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
        layout.active = rl.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        layout.template_ID(lineset, "linestyle", new="scene.freestyle_linestyle_new")
        row = layout.row(align=True)
        row.prop(linestyle, "panel", expand=True)
        if linestyle.panel == 'STROKES':
            ## Chaining
            layout.label(text="Chaining:")
            split = layout.split(align=True)
            # First column
            col = split.column()
            col.prop(linestyle, "use_chaining", text="Enable Chaining")
            sub = col.row()
            sub.active = linestyle.use_chaining
            sub.prop(linestyle, "same_object")
            # Second column
            col = split.column()
            col.active = linestyle.use_chaining
            col.prop(linestyle, "chaining", text="")
            if linestyle.chaining == 'SKETCHY':
                col.prop(linestyle, "rounds")

            ## Splitting
            layout.label(text="Splitting:")
            split = layout.split(align=True)
            # First column
            col = split.column()
            row = col.row(align=True)
            row.prop(linestyle, "use_min_angle", text="")
            sub = row.row()
            sub.active = linestyle.use_min_angle
            sub.prop(linestyle, "min_angle")
            row = col.row(align=True)
            row.prop(linestyle, "use_max_angle", text="")
            sub = row.row()
            sub.active = linestyle.use_max_angle
            sub.prop(linestyle, "max_angle")
            # Second column
            col = split.column()
            row = col.row(align=True)
            row.prop(linestyle, "use_split_length", text="")
            sub = row.row()
            sub.active = linestyle.use_split_length
            sub.prop(linestyle, "split_length", text="2D Length")
            row = col.row(align=True)
            row.prop(linestyle, "material_boundary")
            # End of columns
            row = layout.row(align=True)
            row.prop(linestyle, "use_split_pattern", text="")
            sub = row.row()
            sub.active = linestyle.use_split_pattern
            sub.prop(linestyle, "split_dash1", text="D1")
            sub.prop(linestyle, "split_gap1", text="G1")
            sub.prop(linestyle, "split_dash2", text="D2")
            sub.prop(linestyle, "split_gap2", text="G2")
            sub.prop(linestyle, "split_dash3", text="D3")
            sub.prop(linestyle, "split_gap3", text="G3")

            ## Selection
            layout.label(text="Selection:")
            split = layout.split(align=True)
            # First column
            col = split.column()
            row = col.row(align=True)
            row.prop(linestyle, "use_min_length", text="")
            sub = row.row()
            sub.active = linestyle.use_min_length
            sub.prop(linestyle, "min_length")
            # Second column
            col = split.column()
            row = col.row(align=True)
            row.prop(linestyle, "use_max_length", text="")
            sub = row.row()
            sub.active = linestyle.use_max_length
            sub.prop(linestyle, "max_length")

            ## Caps
            layout.label(text="Caps:")
            row = layout.row(align=True)
            row.prop(linestyle, "caps", expand=True)

            ## Dashed lines
            layout.label(text="Dashed Line:")
            row = layout.row(align=True)
            row.prop(linestyle, "use_dashed_line", text="")
            sub = row.row()
            sub.active = linestyle.use_dashed_line
            sub.prop(linestyle, "dash1", text="D1")
            sub.prop(linestyle, "gap1", text="G1")
            sub.prop(linestyle, "dash2", text="D2")
            sub.prop(linestyle, "gap2", text="G2")
            sub.prop(linestyle, "dash3", text="D3")
            sub.prop(linestyle, "gap3", text="G3")

        elif linestyle.panel == 'COLOR':
            col = layout.column()
            row = col.row()
            row.label(text="Base Color:")
            row.prop(linestyle, "color", text="")
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_color_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.color_modifiers:
                self.draw_color_modifier(context, modifier)

        elif linestyle.panel == 'ALPHA':
            col = layout.column()
            row = col.row()
            row.label(text="Base Transparency:")
            row.prop(linestyle, "alpha")
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_alpha_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.alpha_modifiers:
                self.draw_alpha_modifier(context, modifier)

        elif linestyle.panel == 'THICKNESS':
            col = layout.column()
            row = col.row()
            row.label(text="Base Thickness:")
            row.prop(linestyle, "thickness")
            row = col.row()
            row.prop(linestyle, "thickness_position", expand=True)
            row = col.row()
            row.prop(linestyle, "thickness_ratio")
            row.active = (linestyle.thickness_position == 'RELATIVE')
            col = layout.column()
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_thickness_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.thickness_modifiers:
                self.draw_thickness_modifier(context, modifier)

        elif linestyle.panel == 'GEOMETRY':
            col = layout.column()
            col.label(text="Modifiers:")
            col.operator_menu_enum("scene.freestyle_geometry_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.geometry_modifiers:
                self.draw_geometry_modifier(context, modifier)

        elif linestyle.panel == 'MISC':
            pass


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
