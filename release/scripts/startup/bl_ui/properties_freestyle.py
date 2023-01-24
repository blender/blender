# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Menu, Panel, UIList


# Render properties
class RenderFreestyleButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        with_freestyle = bpy.app.build_options.freestyle
        return scene and with_freestyle and (context.engine in cls.COMPAT_ENGINES)


class RENDER_PT_freestyle(RenderFreestyleButtonsPanel, Panel):
    bl_label = "Freestyle"
    bl_options = {'DEFAULT_CLOSED'}
    bl_order = 10
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_freestyle", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        rd = context.scene.render
        layout.active = rd.use_freestyle
        layout.row().prop(rd, "line_thickness_mode", expand=True, text="Line Thickness Mode")
        if rd.line_thickness_mode == 'ABSOLUTE':
            layout.prop(rd, "line_thickness")


# Render layer properties


class ViewLayerFreestyleButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "view_layer"
    bl_order = 10
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        rd = scene.render
        with_freestyle = bpy.app.build_options.freestyle

        return (
            scene
            and with_freestyle
            and rd.use_freestyle
            and (context.engine in cls.COMPAT_ENGINES)
        )


class ViewLayerFreestyleEditorButtonsPanel(ViewLayerFreestyleButtonsPanel):
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        view_layer = context.view_layer
        return (
            view_layer
            and view_layer.use_freestyle
            and view_layer.freestyle_settings.mode == 'EDITOR'
        )


class ViewLayerFreestyleLineStyle(ViewLayerFreestyleEditorButtonsPanel):
    # Freestyle Linestyle Panels
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active
        if lineset is None:
            return False
        linestyle = lineset.linestyle
        if linestyle is None:
            return False

        return True


class ViewLayerFreestyleLinestyleStrokesSubPanel(ViewLayerFreestyleLineStyle):
    # Freestyle Linestyle Strokes sub panels
    bl_parent_id = "VIEWLAYER_PT_freestyle_linestyle_strokes"


class VIEWLAYER_UL_linesets(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, index):
        lineset = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(lineset, "name", text="", emboss=False, icon_value=icon)
            layout.prop(lineset, "show_render", text="", index=index)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class RENDER_MT_lineset_context_menu(Menu):
    bl_label = "Lineset Specials"

    def draw(self, _context):
        layout = self.layout
        layout.operator("scene.freestyle_lineset_copy", icon='COPYDOWN')
        layout.operator("scene.freestyle_lineset_paste", icon='PASTEDOWN')


class VIEWLAYER_PT_freestyle(ViewLayerFreestyleButtonsPanel, Panel):
    bl_label = "Freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw_header(self, context):
        view_layer = context.view_layer
        rd = context.scene.render

        layout = self.layout

        layout.active = rd.use_freestyle
        layout.prop(view_layer, "use_freestyle", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings

        layout.active = view_layer.use_freestyle

        col = layout.column(align=True)
        col.prop(freestyle, "mode", text="Control Mode")
        col.prop(freestyle, "use_view_map_cache", text="View Map Cache")
        col.prop(freestyle, "as_render_pass", text="As Render Pass")


class VIEWLAYER_PT_freestyle_edge_detection(ViewLayerFreestyleButtonsPanel, Panel):
    bl_label = "Edge Detection"
    bl_parent_id = "VIEWLAYER_PT_freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings

        layout.active = view_layer.use_freestyle

        col = layout.column()
        col.prop(freestyle, "crease_angle")
        col.prop(freestyle, "use_culling")
        col.prop(freestyle, "use_smoothness")

        if freestyle.mode == 'SCRIPT':
            col.prop(freestyle, "use_material_boundaries")

        if freestyle.mode == 'SCRIPT':
            col.prop(freestyle, "use_ridges_and_valleys")
            col.prop(freestyle, "use_suggestive_contours")
        col.prop(freestyle, "sphere_radius")
        col.prop(freestyle, "kr_derivative_epsilon")


class VIEWLAYER_PT_freestyle_style_modules(ViewLayerFreestyleButtonsPanel, Panel):
    bl_label = "Style Modules"
    bl_parent_id = "VIEWLAYER_PT_freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    @classmethod
    def poll(cls, context):
        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        return freestyle.mode == 'SCRIPT'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings

        layout.active = view_layer.use_freestyle

        col = layout.column()
        col.use_property_split = False
        row = col.row()
        row.operator("scene.freestyle_module_add", text="Add")
        for module in freestyle.modules:
            box = col.box()
            box.context_pointer_set("freestyle_module", module)
            row = box.row(align=True)
            row.prop(module, "use", text="")
            row.prop(module, "script", text="")
            row.operator("scene.freestyle_module_open", icon='FILEBROWSER', text="")
            row.operator("scene.freestyle_module_remove", icon='X', text="")
            row.operator("scene.freestyle_module_move", icon='TRIA_UP', text="").direction = 'UP'
            row.operator("scene.freestyle_module_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class VIEWLAYER_PT_freestyle_lineset(ViewLayerFreestyleEditorButtonsPanel, Panel):
    bl_label = "Freestyle Line Set"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

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

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        row = layout.row()

        is_sortable = len(freestyle.linesets) > 1
        rows = 3
        if is_sortable:
            rows = 5

        row.template_list(
            "VIEWLAYER_UL_linesets",
            "",
            freestyle,
            "linesets",
            freestyle.linesets,
            "active_index",
            rows=rows,
        )

        col = row.column(align=True)
        col.operator("scene.freestyle_lineset_add", icon='ADD', text="")
        col.operator("scene.freestyle_lineset_remove", icon='REMOVE', text="")

        col.separator()

        col.menu("RENDER_MT_lineset_context_menu", icon='DOWNARROW_HLT', text="")

        if is_sortable:
            col.separator()
            col.operator("scene.freestyle_lineset_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("scene.freestyle_lineset_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        if lineset:
            layout.template_ID(lineset, "linestyle", new="scene.freestyle_linestyle_new")
            layout.separator()
            col = layout.column(heading="Select by")
            col.use_property_split = True
            col.use_property_decorate = False
            col.prop(lineset, "select_by_image_border", text="Image Border")


# Freestyle Lineset Sub Panels
class VIEWLAYER_PT_freestyle_lineset_visibilty(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Visibility"
    bl_parent_id = "VIEWLAYER_PT_freestyle_lineset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.prop(lineset, "select_by_visibility", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.active = lineset.select_by_visibility
        col = layout.column(align=True, heading="Type")
        col.prop(lineset, "visibility", text="Type", expand=False)

        if lineset.visibility == 'RANGE':
            col = layout.column(align=True)
            col.use_property_split = True
            col.prop(lineset, "qi_start")
            col.prop(lineset, "qi_end")


class VIEWLAYER_PT_freestyle_lineset_edgetype(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Edge Type"
    bl_parent_id = "VIEWLAYER_PT_freestyle_lineset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.prop(lineset, "select_by_edge_types", text="")

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
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.active = lineset.select_by_edge_types
        layout.row().prop(lineset, "edge_type_negation", expand=True, text="Negation")
        layout.row().prop(lineset, "edge_type_combination", expand=True, text="Combination")

        col = layout.column(heading="Type")
        self.draw_edge_type_buttons(col, lineset, "silhouette")
        self.draw_edge_type_buttons(col, lineset, "crease")
        self.draw_edge_type_buttons(col, lineset, "border")
        self.draw_edge_type_buttons(col, lineset, "edge_mark")
        self.draw_edge_type_buttons(col, lineset, "contour")
        self.draw_edge_type_buttons(col, lineset, "external_contour")
        self.draw_edge_type_buttons(col, lineset, "material_boundary")
        self.draw_edge_type_buttons(col, lineset, "suggestive_contour")
        self.draw_edge_type_buttons(col, lineset, "ridge_valley")


class VIEWLAYER_PT_freestyle_lineset_facemarks(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Face Marks"
    bl_parent_id = "VIEWLAYER_PT_freestyle_lineset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.prop(lineset, "select_by_face_marks", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.active = lineset.select_by_face_marks
        layout.row().prop(lineset, "face_mark_negation", expand=True, text="Negation")
        layout.row().prop(lineset, "face_mark_condition", expand=True, text="Condition")


class VIEWLAYER_PT_freestyle_lineset_collection(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Collection"
    bl_parent_id = "VIEWLAYER_PT_freestyle_lineset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.prop(lineset, "select_by_collection", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        layout.active = lineset.select_by_collection
        layout.row().prop(lineset, "collection", text="Line Set Collection")
        layout.row().prop(lineset, "collection_negation", expand=True, text="Negation")


# Linestyle Modifier Drawing code
def draw_modifier_box_header(box, modifier):
    row = box.row()
    row.use_property_split = False
    row.context_pointer_set("modifier", modifier)
    if modifier.expanded:
        icon = 'TRIA_DOWN'
    else:
        icon = 'TRIA_RIGHT'
    row.prop(modifier, "expanded", text="", icon=icon, emboss=False)

    sub = row.row(align=True)
    sub.prop(modifier, "name", text="")
    if modifier.use:
        icon = 'RESTRICT_RENDER_OFF'
    else:
        icon = 'RESTRICT_RENDER_ON'
    sub.prop(modifier, "use", text="", icon=icon)
    sub.operator("scene.freestyle_modifier_copy", icon='DUPLICATE', text="")

    sub = row.row(align=True)
    sub.operator("scene.freestyle_modifier_move", icon='TRIA_UP', text="").direction = 'UP'
    sub.operator("scene.freestyle_modifier_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

    row.operator("scene.freestyle_modifier_remove", icon='X', text="", emboss=False)


def draw_modifier_box_error(box, _modifier, message):
    row = box.row()
    row.label(text=message, icon='ERROR')


def draw_modifier_common(box, modifier):
    col = box.column()
    col.prop(modifier, "blend", text="Blend Mode")
    col.prop(modifier, "influence")


def draw_modifier_color_ramp_common(box, modifier, has_range):
    box.template_color_ramp(modifier, "color_ramp", expand=True)
    if has_range:
        col = box.column(align=True)
        col.prop(modifier, "range_min", text="Range Min")
        col.prop(modifier, "range_max", text="Max")


def draw_modifier_curve_common(box, modifier, has_range, has_value):
    row = box.row()
    row.prop(modifier, "mapping", text="Mapping")
    if modifier.mapping == 'LINEAR':
        box.prop(modifier, "invert")
    if has_range:
        col = box.column(align=True)
        col.prop(modifier, "range_min", text="Range Min")
        col.prop(modifier, "range_max", text="Max")
    if has_value:
        col = box.column(align=True)
        col.prop(modifier, "value_min", text="Value Min")
        col.prop(modifier, "value_max", text="Max")
    if modifier.mapping == 'CURVE':
        box.template_curve_mapping(modifier, "curve")


class VIEWLAYER_PT_freestyle_linestyle_strokes(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Freestyle Strokes"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active

        layout.active = view_layer.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        if linestyle is None:
            return

        row = layout.row(align=True)
        row.alignment = 'LEFT'
        row.label(text=lineset.name, icon='LINE_DATA')
        row.label(text="", icon='RIGHTARROW')
        row.label(text=linestyle.name)

        col = layout.column(align=True)
        col.prop(linestyle, "caps", expand=False)


class VIEWLAYER_PT_freestyle_linestyle_strokes_chaining(ViewLayerFreestyleLinestyleStrokesSubPanel, Panel):
    bl_label = "Chaining"

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        linestyle = lineset.linestyle
        layout.prop(linestyle, "use_chaining", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active
        linestyle = lineset.linestyle

        layout.active = linestyle.use_chaining
        layout.row().prop(linestyle, "chaining", expand=True, text="Method")
        if linestyle.chaining == 'SKETCHY':
            layout.prop(linestyle, "rounds")
        layout.prop(linestyle, "use_same_object")


class VIEWLAYER_PT_freestyle_linestyle_strokes_splitting(ViewLayerFreestyleLinestyleStrokesSubPanel, Panel):
    bl_label = "Splitting"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active
        linestyle = lineset.linestyle

        row = layout.row(align=False, heading="Min 2D Angle")
        row.prop(linestyle, "use_angle_min", text="")
        sub = row.row()
        sub.active = linestyle.use_angle_min
        sub.prop(linestyle, "angle_min", text="")

        row = layout.row(align=False, heading="Max 2D Angle")
        row.prop(linestyle, "use_angle_max", text="")
        sub = row.row()
        sub.active = linestyle.use_angle_max
        sub.prop(linestyle, "angle_max", text="")

        row = layout.row(align=False, heading="2D Length")
        row.prop(linestyle, "use_split_length", text="")
        sub = row.row()
        sub.active = linestyle.use_split_length
        sub.prop(linestyle, "split_length", text="")

        layout.prop(linestyle, "material_boundary")


class VIEWLAYER_PT_freestyle_linestyle_strokes_splitting_pattern(ViewLayerFreestyleLinestyleStrokesSubPanel, Panel):
    bl_label = "Split Pattern"
    bl_parent_id = "VIEWLAYER_PT_freestyle_linestyle_strokes_splitting"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        linestyle = lineset.linestyle
        layout.prop(linestyle, "use_split_pattern", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active
        linestyle = lineset.linestyle

        layout.active = linestyle.use_split_pattern

        col = layout.column(align=True)
        col.prop(linestyle, "split_dash1", text="Dash 1")
        col.prop(linestyle, "split_dash2", text="2")
        col.prop(linestyle, "split_dash3", text="3")
        col = layout.column(align=True)
        col.prop(linestyle, "split_gap1", text="Gap 1")
        col.prop(linestyle, "split_gap2", text="2")
        col.prop(linestyle, "split_gap3", text="3")


class VIEWLAYER_PT_freestyle_linestyle_strokes_sorting(ViewLayerFreestyleLinestyleStrokesSubPanel, Panel):
    bl_label = "Sorting"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        linestyle = lineset.linestyle
        layout.prop(linestyle, "use_sorting", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        linestyle = lineset.linestyle

        layout.active = linestyle.use_sorting

        layout.prop(linestyle, "sort_key")

        row = layout.row()
        row.active = linestyle.sort_key in {
            'DISTANCE_FROM_CAMERA',
            'PROJECTED_X',
            'PROJECTED_Y',
        }
        row.prop(linestyle, "integration_type")
        layout.row().prop(linestyle, "sort_order", expand=True, text="Sort Order")


class VIEWLAYER_PT_freestyle_linestyle_strokes_selection(ViewLayerFreestyleLinestyleStrokesSubPanel, Panel):
    bl_label = "Selection"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active
        linestyle = lineset.linestyle

        row = layout.row(align=False, heading="Min 2D Length")
        row.prop(linestyle, "use_length_min", text="")
        sub = row.row()
        sub.active = linestyle.use_length_min
        sub.prop(linestyle, "length_min", text="")

        row = layout.row(align=False, heading="Max 2D Length")
        row.prop(linestyle, "use_length_max", text="")
        sub = row.row()
        sub.active = linestyle.use_length_max
        sub.prop(linestyle, "length_max", text="")

        row = layout.row(align=False, heading="Chain Count")
        row.prop(linestyle, "use_chain_count", text="")
        sub = row.row()
        sub.active = linestyle.use_chain_count
        sub.prop(linestyle, "chain_count", text="")


class VIEWLAYER_PT_freestyle_linestyle_strokes_dashedline(ViewLayerFreestyleLinestyleStrokesSubPanel, Panel):
    bl_label = "Dashed Line"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        layout = self.layout

        view_layer = context.view_layer
        freestyle = view_layer.freestyle_settings
        lineset = freestyle.linesets.active

        linestyle = lineset.linestyle
        layout.prop(linestyle, "use_dashed_line", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active
        linestyle = lineset.linestyle

        layout.active = linestyle.use_dashed_line

        col = layout.column(align=True)
        col.prop(linestyle, "dash1", text="Dash 1")
        col.prop(linestyle, "dash2", text="2")
        col.prop(linestyle, "dash3", text="3")
        col = layout.column(align=True)
        col.prop(linestyle, "gap1", text="Gap 1")
        col.prop(linestyle, "gap2", text="2")
        col.prop(linestyle, "gap3", text="3")


class VIEWLAYER_PT_freestyle_linestyle_color(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Freestyle Color"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_color_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            draw_modifier_common(box, modifier)

            if modifier.type == 'ALONG_STROKE':
                draw_modifier_color_ramp_common(box, modifier, False)

            elif modifier.type == 'DISTANCE_FROM_OBJECT':
                box.prop(modifier, "target")
                draw_modifier_color_ramp_common(box, modifier, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'COLOR'
                prop.name = modifier.name

            elif modifier.type == 'DISTANCE_FROM_CAMERA':
                draw_modifier_color_ramp_common(box, modifier, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'COLOR'
                prop.name = modifier.name

            elif modifier.type == 'MATERIAL':
                row = box.row()
                row.prop(modifier, "material_attribute",
                         text="Material Attribute")
                sub = box.column()
                sub.prop(modifier, "use_ramp")
                if modifier.material_attribute in {'LINE', 'DIFF', 'SPEC'}:
                    sub.active = True
                    show_ramp = modifier.use_ramp
                else:
                    sub.active = False
                    show_ramp = True
                if show_ramp:
                    draw_modifier_color_ramp_common(box, modifier, False)

            elif modifier.type == 'TANGENT':
                draw_modifier_color_ramp_common(box, modifier, False)

            elif modifier.type == 'NOISE':
                subcol = box.column(align=True)
                subcol.prop(modifier, "amplitude")
                subcol.prop(modifier, "period")
                subcol.prop(modifier, "seed")
                draw_modifier_color_ramp_common(box, modifier, False)

            elif modifier.type == 'CREASE_ANGLE':
                subcol = box.column(align=True)
                subcol.prop(modifier, "angle_min", text="Angle Min")
                subcol.prop(modifier, "angle_max", text="Max")
                draw_modifier_color_ramp_common(box, modifier, False)

            elif modifier.type == 'CURVATURE_3D':
                subcol = box.column(align=True)
                subcol.prop(modifier, "curvature_min", text="Curvature Min")
                subcol.prop(modifier, "curvature_max", text="Max")

                draw_modifier_color_ramp_common(box, modifier, False)

                freestyle = context.view_layer.freestyle_settings
                if not freestyle.use_smoothness:
                    message = "Enable Face Smoothness to use this modifier"
                    draw_modifier_box_error(col.box(), modifier, message)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active

        layout.active = view_layer.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        if linestyle is None:
            return

        row = layout.row(align=True)
        row.alignment = 'LEFT'
        row.label(text=lineset.name, icon='LINE_DATA')
        row.label(text="", icon='RIGHTARROW')
        row.label(text=linestyle.name)

        col = layout.column()
        row = col.row()
        row.prop(linestyle, "color", text="Base Color")
        col.operator_menu_enum("scene.freestyle_color_modifier_add", "type", text="Add Modifier")
        for modifier in linestyle.color_modifiers:
            self.draw_color_modifier(context, modifier)


class VIEWLAYER_PT_freestyle_linestyle_alpha(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Freestyle Alpha"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_alpha_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            draw_modifier_common(box, modifier)

            if modifier.type == 'ALONG_STROKE':
                draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'DISTANCE_FROM_OBJECT':
                box.prop(modifier, "target")
                draw_modifier_curve_common(box, modifier, True, False)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'ALPHA'
                prop.name = modifier.name

            elif modifier.type == 'DISTANCE_FROM_CAMERA':
                draw_modifier_curve_common(box, modifier, True, False)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'ALPHA'
                prop.name = modifier.name

            elif modifier.type == 'MATERIAL':
                box.prop(modifier, "material_attribute", text="Material Attribute")
                draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'TANGENT':
                draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'NOISE':
                col = box.column(align=True)
                col.prop(modifier, "amplitude")
                col.prop(modifier, "period")
                col.prop(modifier, "seed")
                draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'CREASE_ANGLE':
                col = box.column(align=True)
                col.prop(modifier, "angle_min", text="Angle Min")
                col.prop(modifier, "angle_max", text="Max")
                draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'CURVATURE_3D':
                draw_modifier_curve_common(box, modifier, False, False)

                subcol = box.column(align=True)
                subcol.prop(modifier, "curvature_min", text="Curvature Min")
                subcol.prop(modifier, "curvature_max", text="Max")

                freestyle = context.view_layer.freestyle_settings
                if not freestyle.use_smoothness:
                    message = "Enable Face Smoothness to use this modifier"
                    draw_modifier_box_error(col.box(), modifier, message)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active

        layout.active = view_layer.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        if linestyle is None:
            return

        row = layout.row(align=True)
        row.alignment = 'LEFT'
        row.label(text=lineset.name, icon='LINE_DATA')
        row.label(text="", icon='RIGHTARROW')
        row.label(text=linestyle.name)

        col = layout.column()
        row = col.row()
        row.prop(linestyle, "alpha", text="Base Transparency")
        col.operator_menu_enum("scene.freestyle_alpha_modifier_add", "type", text="Add Modifier")
        for modifier in linestyle.alpha_modifiers:
            self.draw_alpha_modifier(context, modifier)


class VIEWLAYER_PT_freestyle_linestyle_thickness(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Freestyle Thickness"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_thickness_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            draw_modifier_common(box, modifier)

            if modifier.type == 'ALONG_STROKE':
                draw_modifier_curve_common(box, modifier, False, True)

            elif modifier.type == 'DISTANCE_FROM_OBJECT':
                box.prop(modifier, "target")
                draw_modifier_curve_common(box, modifier, True, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'THICKNESS'
                prop.name = modifier.name

            elif modifier.type == 'DISTANCE_FROM_CAMERA':
                draw_modifier_curve_common(box, modifier, True, True)
                prop = box.operator("scene.freestyle_fill_range_by_selection")
                prop.type = 'THICKNESS'
                prop.name = modifier.name

            elif modifier.type == 'MATERIAL':
                box.prop(modifier, "material_attribute", text="Material Attribute")
                draw_modifier_curve_common(box, modifier, False, True)

            elif modifier.type == 'CALLIGRAPHY':
                box.prop(modifier, "orientation")
                subcol = box.column(align=True)
                subcol.prop(modifier, "thickness_min", text="Thickness Min")
                subcol.prop(modifier, "thickness_max", text="Max")

            elif modifier.type == 'TANGENT':
                self.mapping = 'CURVE'
                subcol = box.column(align=True)
                subcol.prop(modifier, "thickness_min", text="Thickness Min")
                subcol.prop(modifier, "thickness_max", text="Max")

                draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'NOISE':
                col = box.column(align=True)
                col.prop(modifier, "amplitude")
                col.prop(modifier, "period")

                col = box.column(align=True)
                col.prop(modifier, "seed")
                col.prop(modifier, "use_asymmetric")

            elif modifier.type == 'CREASE_ANGLE':
                col = box.column(align=True)
                col.prop(modifier, "thickness_min", text="Thickness Min")
                col.prop(modifier, "thickness_max", text="Max")

                col = box.column(align=True)
                col.prop(modifier, "angle_min", text="Angle Min")
                col.prop(modifier, "angle_max", text="Max")

                draw_modifier_curve_common(box, modifier, False, False)

            elif modifier.type == 'CURVATURE_3D':
                subcol = box.column(align=True)
                subcol.prop(modifier, "thickness_min", text="Thickness Min")
                subcol.prop(modifier, "thickness_max", text="Max")

                subcol = box.column(align=True)
                subcol.prop(modifier, "curvature_min")
                subcol.prop(modifier, "curvature_max")

                draw_modifier_curve_common(box, modifier, False, False)

                freestyle = context.view_layer.freestyle_settings
                if not freestyle.use_smoothness:
                    message = "Enable Face Smoothness to use this modifier"
                    draw_modifier_box_error(col.box(), modifier, message)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active

        layout.active = view_layer.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        if linestyle is None:
            return

        row = layout.row(align=True)
        row.alignment = 'LEFT'
        row.label(text=lineset.name, icon='LINE_DATA')
        row.label(text="", icon='RIGHTARROW')
        row.label(text=linestyle.name)

        col = layout.column()
        row = col.row()
        row.prop(linestyle, "thickness", text="Base Thickness")
        subcol = col.column()
        subcol.active = linestyle.chaining == 'PLAIN' and linestyle.use_same_object
        row = subcol.row()
        row.prop(linestyle, "thickness_position", expand=False)

        if linestyle.thickness_position == 'RELATIVE':
            row = subcol.row()
            row.prop(linestyle, "thickness_ratio")

        col = layout.column()
        col.operator_menu_enum("scene.freestyle_thickness_modifier_add", "type", text="Add Modifier")
        for modifier in linestyle.thickness_modifiers:
            self.draw_thickness_modifier(context, modifier)


class VIEWLAYER_PT_freestyle_linestyle_geometry(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Freestyle Geometry"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_geometry_modifier(self, _context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()

            if modifier.type == 'SAMPLING':
                box.prop(modifier, "sampling")

            elif modifier.type == 'BEZIER_CURVE':
                box.prop(modifier, "error")

            elif modifier.type == 'SINUS_DISPLACEMENT':
                col = box.column(align=True)
                col.prop(modifier, "wavelength")
                col.prop(modifier, "amplitude")

                col = box.column(align=True)
                col.prop(modifier, "phase")

            elif modifier.type == 'SPATIAL_NOISE':
                col = box.column(align=True)
                col.prop(modifier, "amplitude")
                col.prop(modifier, "scale")
                col.prop(modifier, "octaves")

                col = box.column(align=True)
                col.prop(modifier, "smooth")
                col.prop(modifier, "use_pure_random")

            elif modifier.type == 'PERLIN_NOISE_1D':
                col = box.column(align=True)
                col.prop(modifier, "frequency")
                col.prop(modifier, "amplitude")
                col.prop(modifier, "seed")

                col = box.column(align=True)
                col.prop(modifier, "octaves")
                col.prop(modifier, "angle")

            elif modifier.type == 'PERLIN_NOISE_2D':
                col = box.column(align=True)
                col.prop(modifier, "frequency")
                col.prop(modifier, "amplitude")
                col.prop(modifier, "seed")

                col = box.column(align=True)
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
                subcol = box.column(align=True)
                if modifier.shape in {'CIRCLES', 'ELLIPSES'}:
                    subcol.prop(modifier, "random_radius", text="Random Radius")
                    subcol.prop(modifier, "random_center", text="Center")
                elif modifier.shape == 'SQUARES':
                    subcol.prop(modifier, "backbone_length", text="Backbone Length")
                    subcol.prop(modifier, "random_backbone", text="Randomness")

            elif modifier.type == '2D_OFFSET':
                subcol = box.column(align=True)
                subcol.prop(modifier, "start")
                subcol.prop(modifier, "end")

                subcol = box.column(align=True)
                subcol.prop(modifier, "x")
                subcol.prop(modifier, "y")

            elif modifier.type == '2D_TRANSFORM':
                box.prop(modifier, "pivot")
                if modifier.pivot == 'PARAM':
                    box.prop(modifier, "pivot_u")
                elif modifier.pivot == 'ABSOLUTE':
                    subcol = box.column(align=True)
                    subcol.prop(modifier, "pivot_x", text="Pivot X")
                    subcol.prop(modifier, "pivot_y", text="Y")
                subcol = box.column(align=True)
                subcol.prop(modifier, "scale_x", text="Scale X")
                subcol.prop(modifier, "scale_y", text="Y")
                box.prop(modifier, "angle")

            elif modifier.type == 'SIMPLIFICATION':
                box.prop(modifier, "tolerance")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active

        layout.active = view_layer.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        if linestyle is None:
            return

        row = layout.row(align=True)
        row.alignment = 'LEFT'
        row.label(text=lineset.name, icon='LINE_DATA')
        row.label(text="", icon='RIGHTARROW')
        row.label(text=linestyle.name)

        col = layout.column()
        col.operator_menu_enum("scene.freestyle_geometry_modifier_add", "type", text="Add Modifier")
        for modifier in linestyle.geometry_modifiers:
            self.draw_geometry_modifier(context, modifier)


class VIEWLAYER_PT_freestyle_linestyle_texture(ViewLayerFreestyleLineStyle, Panel):
    bl_label = "Freestyle Texture"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active

        layout.active = view_layer.use_freestyle

        if lineset is None:
            return
        linestyle = lineset.linestyle

        if linestyle is None:
            return

        row = layout.row(align=True)
        row.alignment = 'LEFT'
        row.label(text=lineset.name, icon='LINE_DATA')
        row.label(text="", icon='RIGHTARROW')
        row.label(text=linestyle.name)

        layout.prop(linestyle, "use_nodes")
        layout.prop(linestyle, "texture_spacing", text="Spacing Along Stroke")

        row = layout.row()
        props = row.operator(
            "wm.properties_context_change",
            text="Go to Linestyle Textures Properties",
            icon='TEXTURE',
        )
        props.context = 'TEXTURE'


# Material properties
class MaterialFreestyleButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        material = context.material
        with_freestyle = bpy.app.build_options.freestyle
        return (
            with_freestyle
            and material
            and scene
            and scene.render.use_freestyle
            and (context.engine in cls.COMPAT_ENGINES)
        )


class MATERIAL_PT_freestyle_line(MaterialFreestyleButtonsPanel, Panel):
    bl_label = "Freestyle Line"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH', 'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        mat = context.material

        col = layout.column()
        col.prop(mat, "line_color")
        col.prop(mat, "line_priority", text="Priority")


classes = (
    RENDER_PT_freestyle,
    VIEWLAYER_UL_linesets,
    RENDER_MT_lineset_context_menu,
    VIEWLAYER_PT_freestyle,
    VIEWLAYER_PT_freestyle_edge_detection,
    VIEWLAYER_PT_freestyle_style_modules,
    VIEWLAYER_PT_freestyle_lineset,
    VIEWLAYER_PT_freestyle_lineset_visibilty,
    VIEWLAYER_PT_freestyle_lineset_edgetype,
    VIEWLAYER_PT_freestyle_lineset_facemarks,
    VIEWLAYER_PT_freestyle_lineset_collection,
    VIEWLAYER_PT_freestyle_linestyle_strokes,
    VIEWLAYER_PT_freestyle_linestyle_strokes_chaining,
    VIEWLAYER_PT_freestyle_linestyle_strokes_splitting,
    VIEWLAYER_PT_freestyle_linestyle_strokes_splitting_pattern,
    VIEWLAYER_PT_freestyle_linestyle_strokes_sorting,
    VIEWLAYER_PT_freestyle_linestyle_strokes_selection,
    VIEWLAYER_PT_freestyle_linestyle_strokes_dashedline,
    VIEWLAYER_PT_freestyle_linestyle_color,
    VIEWLAYER_PT_freestyle_linestyle_alpha,
    VIEWLAYER_PT_freestyle_linestyle_thickness,
    VIEWLAYER_PT_freestyle_linestyle_geometry,
    VIEWLAYER_PT_freestyle_linestyle_texture,
    MATERIAL_PT_freestyle_line,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)
