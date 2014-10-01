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
from bpy.types import Menu, Panel, UIList


# Render properties

class RenderFreestyleButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        with_freestyle = bpy.app.build_options.freestyle
        return scene and with_freestyle and(scene.render.engine in cls.COMPAT_ENGINES)


class RENDER_PT_freestyle(RenderFreestyleButtonsPanel, Panel):
    bl_label = "Freestyle"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'CYCLES'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_freestyle", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.active = rd.use_freestyle

        row = layout.row()
        row.label(text="Line Thickness:")
        row.prop(rd, "line_thickness_mode", expand=True)

        if (rd.line_thickness_mode == 'ABSOLUTE'):
            layout.prop(rd, "line_thickness")


# Render layer properties

class RenderLayerFreestyleButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render_layer"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        rd = context.scene.render
        with_freestyle = bpy.app.build_options.freestyle

        return (scene and with_freestyle and rd.use_freestyle
            and rd.layers.active and(scene.render.engine in cls.COMPAT_ENGINES))


class RenderLayerFreestyleEditorButtonsPanel(RenderLayerFreestyleButtonsPanel):
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        rl = context.scene.render.layers.active
        return rl and rl.freestyle_settings.mode == 'EDITOR'


class RENDERLAYER_UL_linesets(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        lineset = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(lineset, "name", text="", emboss=False, icon_value=icon)
            layout.prop(lineset, "show_render", text="", index=index)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label("", icon_value=icon)


class RENDER_MT_lineset_specials(Menu):
    bl_label = "Lineset Specials"

    def draw(self, context):
        layout = self.layout
        layout.operator("scene.freestyle_lineset_copy", icon='COPYDOWN')
        layout.operator("scene.freestyle_lineset_paste", icon='PASTEDOWN')


class RENDERLAYER_PT_freestyle(RenderLayerFreestyleButtonsPanel, Panel):
    bl_label = "Freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'CYCLES'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings

        layout.active = rl.use_freestyle

        layout.prop(freestyle, "mode", text="Control mode")
        layout.label(text="Edge Detection Options:")

        split = layout.split()

        col = split.column()
        col.prop(freestyle, "crease_angle")
        col.prop(freestyle, "use_culling")
        col.prop(freestyle, "use_advanced_options")

        col = split.column()
        col.prop(freestyle, "use_smoothness")
        if freestyle.mode == 'SCRIPT':
            col.prop(freestyle, "use_material_boundaries")

        # Advanced options are hidden by default to warn new users
        if freestyle.use_advanced_options:
            if freestyle.mode == 'SCRIPT':
                row = layout.row()
                row.prop(freestyle, "use_ridges_and_valleys")
                row.prop(freestyle, "use_suggestive_contours")
            row = layout.row()
            row.prop(freestyle, "sphere_radius")
            row.prop(freestyle, "kr_derivative_epsilon")

        if freestyle.mode == 'SCRIPT':
            row = layout.row()
            row.label("Style modules:")
            row.operator("scene.freestyle_module_add", text="Add")
            for i, module in enumerate(freestyle.modules):
                box = layout.box()
                box.context_pointer_set("freestyle_module", module)
                row = box.row(align=True)
                row.prop(module, "use", text="")
                row.prop(module, "script", text="")
                row.operator("scene.freestyle_module_open", icon='FILESEL', text="")
                row.operator("scene.freestyle_module_remove", icon='X', text="")
                row.operator("scene.freestyle_module_move", icon='TRIA_UP', text="").direction = 'UP'
                row.operator("scene.freestyle_module_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class RENDERLAYER_PT_freestyle_lineset(RenderLayerFreestyleEditorButtonsPanel, Panel):
    bl_label = "Freestyle Line Set"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'CYCLES'}

    def draw_edge_type_buttons(self, box, lineset, edge_type):
        # property names
        select_edge_type = "select_" + edge_type
        exclude_edge_type = "exclude_" + edge_type
        # draw edge type buttons
        row = box.row(align=True)
        row.prop(lineset, select_edge_type)
        sub = row.column(align=True)
        sub.prop(lineset, exclude_edge_type, text="")
        sub.active = getattr(lineset, select_edge_type)

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings
        lineset = freestyle.linesets.active

        layout.active = rl.use_freestyle

        row = layout.row()
        rows = 4 if lineset else 2
        row.template_list("RENDERLAYER_UL_linesets", "", freestyle, "linesets", freestyle.linesets, "active_index", rows=rows)

        sub = row.column(align=True)
        sub.operator("scene.freestyle_lineset_add", icon='ZOOMIN', text="")
        sub.operator("scene.freestyle_lineset_remove", icon='ZOOMOUT', text="")
        sub.menu("RENDER_MT_lineset_specials", icon='DOWNARROW_HLT', text="")
        if lineset:
            sub.separator()
            sub.separator()
            sub.operator("scene.freestyle_lineset_move", icon='TRIA_UP', text="").direction = 'UP'
            sub.operator("scene.freestyle_lineset_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

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


class RENDERLAYER_PT_freestyle_linestyle(RenderLayerFreestyleEditorButtonsPanel, Panel):
    bl_label = "Freestyle Line Style"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'CYCLES'}

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
                row.prop(modifier, "material_attribute", text="")
                sub = row.column()
                sub.prop(modifier, "use_ramp")
                if modifier.material_attribute in {'LINE', 'DIFF', 'SPEC'}:
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
                box.prop(modifier, "material_attribute", text="")
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
                box.prop(modifier, "material_attribute", text="")
                self.draw_modifier_curve_common(box, modifier, False, True)

            elif modifier.type == 'CALLIGRAPHY':
                box.prop(modifier, "orientation")
                row = box.row(align=True)
                row.prop(modifier, "thickness_min")
                row.prop(modifier, "thickness_max")

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
                col.prop(modifier, "use_pure_random")

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
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        lineset = rl.freestyle_settings.linesets.active

        layout.active = rl.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        layout.template_ID(lineset, "linestyle", new="scene.freestyle_linestyle_new")
        if linestyle is None:
            return
        row = layout.row(align=True)
        row.prop(linestyle, "panel", expand=True)
        if linestyle.panel == 'STROKES':
            ## Chaining
            layout.prop(linestyle, "use_chaining", text="Chaining:")
            split = layout.split(align=True)
            split.active = linestyle.use_chaining
            # First column
            col = split.column()
            col.active = linestyle.use_chaining
            col.prop(linestyle, "chaining", text="")
            if linestyle.chaining == 'SKETCHY':
                col.prop(linestyle, "rounds")
            # Second column
            col = split.column()
            col.prop(linestyle, "use_same_object")

            ## Splitting
            layout.label(text="Splitting:")
            split = layout.split(align=True)
            # First column
            col = split.column()
            row = col.row(align=True)
            row.prop(linestyle, "use_angle_min", text="")
            sub = row.row()
            sub.active = linestyle.use_angle_min
            sub.prop(linestyle, "angle_min")
            row = col.row(align=True)
            row.prop(linestyle, "use_angle_max", text="")
            sub = row.row()
            sub.active = linestyle.use_angle_max
            sub.prop(linestyle, "angle_max")
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
            sub = row.row(align=True)
            sub.active = linestyle.use_split_pattern
            sub.prop(linestyle, "split_dash1", text="D1")
            sub.prop(linestyle, "split_gap1", text="G1")
            sub.prop(linestyle, "split_dash2", text="D2")
            sub.prop(linestyle, "split_gap2", text="G2")
            sub.prop(linestyle, "split_dash3", text="D3")
            sub.prop(linestyle, "split_gap3", text="G3")

            ## Sorting
            layout.prop(linestyle, "use_sorting", text="Sorting:")
            col = layout.column()
            col.active = linestyle.use_sorting
            row = col.row(align=True)
            row.prop(linestyle, "sort_key", text="")
            sub = row.row()
            sub.active = linestyle.sort_key in {'DISTANCE_FROM_CAMERA',
                                                'PROJECTED_X',
                                                'PROJECTED_Y'}
            sub.prop(linestyle, "integration_type", text="")
            row = col.row(align=True)
            row.prop(linestyle, "sort_order", expand=True)

            ## Selection
            layout.label(text="Selection:")
            split = layout.split(align=True)
            # First column
            col = split.column()
            row = col.row(align=True)
            row.prop(linestyle, "use_length_min", text="")
            sub = row.row()
            sub.active = linestyle.use_length_min
            sub.prop(linestyle, "length_min")
            row = col.row(align=True)
            row.prop(linestyle, "use_length_max", text="")
            sub = row.row()
            sub.active = linestyle.use_length_max
            sub.prop(linestyle, "length_max")
            # Second column
            col = split.column()
            row = col.row(align=True)
            row.prop(linestyle, "use_chain_count", text="")
            sub = row.row()
            sub.active = linestyle.use_chain_count
            sub.prop(linestyle, "chain_count")

            ## Caps
            layout.label(text="Caps:")
            row = layout.row(align=True)
            row.prop(linestyle, "caps", expand=True)

            ## Dashed lines
            layout.prop(linestyle, "use_dashed_line", text="Dashed Line:")
            row = layout.row(align=True)
            row.active = linestyle.use_dashed_line
            row.prop(linestyle, "dash1", text="D1")
            row.prop(linestyle, "gap1", text="G1")
            row.prop(linestyle, "dash2", text="D2")
            row.prop(linestyle, "gap2", text="G2")
            row.prop(linestyle, "dash3", text="D3")
            row.prop(linestyle, "gap3", text="G3")

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
            subcol = col.column()
            subcol.active = linestyle.chaining == 'PLAIN' and linestyle.use_same_object
            row = subcol.row()
            row.prop(linestyle, "thickness_position", expand=True)
            row = subcol.row()
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

        elif linestyle.panel == 'TEXTURE':
            layout.separator()

            row = layout.row()
            if rd.use_shading_nodes:
                row.prop(linestyle, "use_nodes")
            else:
                row.prop(linestyle, "use_texture")
            row.prop(linestyle, "texture_spacing", text="Spacing Along Stroke")

            row = layout.row()
            op = row.operator("wm.properties_context_change",
                         text="Go to Linestyle Textures Properties",
                         icon='TEXTURE')
            op.context = 'TEXTURE'

        elif linestyle.panel == 'MISC':
            pass


# Material properties

class MaterialFreestyleButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        material = context.material
        with_freestyle = bpy.app.build_options.freestyle
        return with_freestyle and material and scene and scene.render.use_freestyle and \
            (scene.render.engine in cls.COMPAT_ENGINES)


class MATERIAL_PT_freestyle_line(MaterialFreestyleButtonsPanel, Panel):
    bl_label = "Freestyle Line"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'CYCLES'}

    def draw(self, context):
        layout = self.layout

        mat = context.material

        row = layout.row()
        row.prop(mat, "line_color", text="")
        row.prop(mat, "line_priority", text="Priority")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
