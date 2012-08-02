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


class RENDER_MT_presets(Menu):
    bl_label = "Render Presets"
    preset_subdir = "render"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class RENDER_MT_ffmpeg_presets(Menu):
    bl_label = "FFMPEG Presets"
    preset_subdir = "ffmpeg"
    preset_operator = "script.python_file_run"
    draw = Menu.draw_preset


class RENDER_MT_framerate_presets(Menu):
    bl_label = "Frame Rate Presets"
    preset_subdir = "framerate"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return context.scene and (rd.engine in cls.COMPAT_ENGINES)


class RENDER_PT_render(RenderButtonsPanel, Panel):
    bl_label = "Render"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        row = layout.row()
        row.operator("render.render", text="Image", icon='RENDER_STILL')
        row.operator("render.render", text="Animation", icon='RENDER_ANIMATION').animation = True

        layout.prop(rd, "display_mode", text="Display")


class RENDER_PT_layers(RenderButtonsPanel, Panel):
    bl_label = "Layers"
    bl_options = {'DEFAULT_CLOSED'}
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

        split = layout.split()

        col = split.column()
        col.prop(scene, "layers", text="Scene")
        col.label(text="")
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
        col.prop(rl, "use_freestyle")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.label(text="Passes:")
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
        col.label()
        col.prop(rl, "use_pass_diffuse")
        row = col.row()
        row.prop(rl, "use_pass_specular")
        row.prop(rl, "exclude_specular", text="")
        row = col.row()
        row.prop(rl, "use_pass_shadow")
        row.prop(rl, "exclude_shadow", text="")
        row = col.row()
        row.prop(rl, "use_pass_emit")
        row.prop(rl, "exclude_emit", text="")
        row = col.row()
        row.prop(rl, "use_pass_ambient_occlusion")
        row.prop(rl, "exclude_ambient_occlusion", text="")
        row = col.row()
        row.prop(rl, "use_pass_environment")
        row.prop(rl, "exclude_environment", text="")
        row = col.row()
        row.prop(rl, "use_pass_indirect")
        row.prop(rl, "exclude_indirect", text="")
        row = col.row()
        row.prop(rl, "use_pass_reflection")
        row.prop(rl, "exclude_reflection", text="")
        row = col.row()
        row.prop(rl, "use_pass_refraction")
        row.prop(rl, "exclude_refraction", text="")


class RENDER_MT_lineset_specials(Menu):
    bl_label = "Lineset Specials"

    def draw(self, context):
        layout = self.layout
        layout.operator("scene.freestyle_lineset_copy", icon='COPYDOWN')
        layout.operator("scene.freestyle_lineset_paste", icon='PASTEDOWN')


class RENDER_PT_freestyle(RenderButtonsPanel, Panel):
    bl_label = "Freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        if rd.engine not in cls.COMPAT_ENGINES:
            return False
        rl = rd.layers.active
        return rl and rl.use_freestyle

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings

        split = layout.split()

        col = split.column()
        col.prop(freestyle, "raycasting_algorithm", text="Raycasting Algorithm")
        col.prop(freestyle, "mode", text="Control Mode")

        col.label(text="Edge Detection Options:")
        col.prop(freestyle, "use_smoothness")
        col.prop(freestyle, "crease_angle")
        if freestyle.mode == "SCRIPT":
            col.prop(freestyle, "use_material_boundaries")
            col.prop(freestyle, "use_ridges_and_valleys")
            col.prop(freestyle, "use_suggestive_contours")
        col.prop(freestyle, "use_advanced_options")
        if freestyle.use_advanced_options:
            col.prop(freestyle, "sphere_radius")
            col.prop(freestyle, "kr_derivative_epsilon")

        if freestyle.mode == "EDITOR":

            lineset = freestyle.linesets.active

            col.label(text="Line Sets:")
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

        else: # freestyle.mode == "SCRIPT"

            col.separator()
            col.operator("scene.freestyle_module_add")

            for i, module in enumerate(freestyle.modules):
                box = layout.box()
                box.context_pointer_set("freestyle_module", module)
                row = box.row(align=True)
                row.prop(module, "use", text="")
                row.prop(module, "module_path", text="")
                row.operator("scene.freestyle_module_remove", icon='X', text="")
                row.operator("scene.freestyle_module_move", icon='TRIA_UP', text="").direction = 'UP'
                row.operator("scene.freestyle_module_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class RENDER_PT_freestyle_lineset(RenderButtonsPanel, Panel):
    bl_label = "Freestyle: Line Set"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        if rd.engine not in cls.COMPAT_ENGINES:
            return False
        rl = rd.layers.active
        if rl and rl.use_freestyle:
            freestyle = rl.freestyle_settings
            return freestyle.mode == "EDITOR" and freestyle.linesets.active
        return False

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
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings
        lineset = freestyle.linesets.active

        split = layout.split()

        col = split.column()
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


class RENDER_PT_freestyle_linestyle(RenderButtonsPanel, Panel):
    bl_label = "Freestyle: Line Style"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        if rd.engine not in cls.COMPAT_ENGINES:
            return False
        rl = rd.layers.active
        if rl and rl.use_freestyle:
            freestyle = rl.freestyle_settings
            return freestyle.mode == "EDITOR" and freestyle.linesets.active
        return False

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
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        lineset = rl.freestyle_settings.linesets.active
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
            col = layout.column()
            col.label(text="Splitting:")
            row = col.row()
            row.prop(linestyle, "material_boundary")
            row = col.row()
            sub = row.column()
            sub.prop(linestyle, "use_min_angle", text="Min 2D Angle")
            subsub = sub.split()
            subsub.prop(linestyle, "min_angle", text="")
            subsub.enabled = linestyle.use_min_angle
            sub = row.column()
            sub.prop(linestyle, "use_max_angle", text="Max 2D Angle")
            subsub = sub.split()
            subsub.prop(linestyle, "max_angle", text="")
            subsub.enabled = linestyle.use_max_angle
            col.prop(linestyle, "use_split_length", text="2D Length")
            row = col.row()
            row.prop(linestyle, "split_length", text="")
            row.enabled = linestyle.use_split_length
            # Selection
            col = layout.column()
            col.label(text="Selection:")
            sub = col.row()
            subcol = sub.column()
            subcol.prop(linestyle, "use_min_length", text="Min 2D Length")
            subsub = subcol.split()
            subsub.prop(linestyle, "min_length", text="")
            subsub.enabled = linestyle.use_min_length
            subcol = sub.column()
            subcol.prop(linestyle, "use_max_length", text="Max 2D Length")
            subsub = subcol.split()
            subsub.prop(linestyle, "max_length", text="")
            subsub.enabled = linestyle.use_max_length
            # Caps
            col = layout.column()
            col.label(text="Caps:")
            row = col.row(align=True)
            row.prop(linestyle, "caps", expand=True)
            col = layout.column()
            col.prop(linestyle, "use_dashed_line")
            split = col.split()
            split.enabled = linestyle.use_dashed_line
            sub = split.column()
            sub.label(text="Dash")
            sub.prop(linestyle, "dash1", text="")
            sub = split.column()
            sub.label(text="Gap")
            sub.prop(linestyle, "gap1", text="")
            sub = split.column()
            sub.label(text="Dash")
            sub.prop(linestyle, "dash2", text="")
            sub = split.column()
            sub.label(text="Gap")
            sub.prop(linestyle, "gap2", text="")
            sub = split.column()
            sub.label(text="Dash")
            sub.prop(linestyle, "dash3", text="")
            sub = split.column()
            sub.label(text="Gap")
            sub.prop(linestyle, "gap3", text="")
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


class RENDER_PT_dimensions(RenderButtonsPanel, Panel):
    bl_label = "Dimensions"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render

        row = layout.row(align=True)
        row.menu("RENDER_MT_presets", text=bpy.types.RENDER_MT_presets.bl_label)
        row.operator("render.preset_add", text="", icon='ZOOMIN')
        row.operator("render.preset_add", text="", icon='ZOOMOUT').remove_active = True

        split = layout.split()

        col = split.column()
        sub = col.column(align=True)
        sub.label(text="Resolution:")
        sub.prop(rd, "resolution_x", text="X")
        sub.prop(rd, "resolution_y", text="Y")
        sub.prop(rd, "resolution_percentage", text="")

        sub.label(text="Aspect Ratio:")
        sub.prop(rd, "pixel_aspect_x", text="X")
        sub.prop(rd, "pixel_aspect_y", text="Y")

        row = col.row()
        row.prop(rd, "use_border", text="Border")
        sub = row.row()
        sub.active = rd.use_border
        sub.prop(rd, "use_crop_to_border", text="Crop")

        col = split.column()
        sub = col.column(align=True)
        sub.label(text="Frame Range:")
        sub.prop(scene, "frame_start")
        sub.prop(scene, "frame_end")
        sub.prop(scene, "frame_step")

        sub.label(text="Frame Rate:")
        if rd.fps_base == 1:
            fps_rate = round(rd.fps / rd.fps_base)
        else:
            fps_rate = round(rd.fps / rd.fps_base, 2)

        # TODO: Change the following to iterate over existing presets
        custom_framerate = (fps_rate not in {23.98, 24, 25, 29.97, 30, 50, 59.94, 60})

        if custom_framerate == True:
            fps_label_text = "Custom (" + str(fps_rate) + " fps)"
        else:
            fps_label_text = str(fps_rate) + " fps"

        sub.menu("RENDER_MT_framerate_presets", text=fps_label_text)

        if custom_framerate or (bpy.types.RENDER_MT_framerate_presets.bl_label == "Custom"):
            sub.prop(rd, "fps")
            sub.prop(rd, "fps_base", text="/")
        subrow = sub.row(align=True)
        subrow.label(text="Time Remapping:")
        subrow = sub.row(align=True)
        subrow.prop(rd, "frame_map_old", text="Old")
        subrow.prop(rd, "frame_map_new", text="New")


class RENDER_PT_antialiasing(RenderButtonsPanel, Panel):
    bl_label = "Anti-Aliasing"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_antialiasing", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        layout.active = rd.use_antialiasing

        split = layout.split()

        col = split.column()
        col.row().prop(rd, "antialiasing_samples", expand=True)
        sub = col.row()
        sub.enabled = not rd.use_border
        sub.prop(rd, "use_full_sample")

        col = split.column()
        col.prop(rd, "pixel_filter_type", text="")
        col.prop(rd, "filter_size", text="Size")


class RENDER_PT_motion_blur(RenderButtonsPanel, Panel):
    bl_label = "Sampled Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return not rd.use_full_sample and (rd.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        layout.active = rd.use_motion_blur

        row = layout.row()
        row.prop(rd, "motion_blur_samples")
        row.prop(rd, "motion_blur_shutter")


class RENDER_PT_shading(RenderButtonsPanel, Panel):
    bl_label = "Shading"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_textures", text="Textures")
        col.prop(rd, "use_shadows", text="Shadows")
        col.prop(rd, "use_sss", text="Subsurface Scattering")
        col.prop(rd, "use_envmaps", text="Environment Map")

        col = split.column()
        col.prop(rd, "use_raytrace", text="Ray Tracing")
        col.prop(rd, "use_color_management")
        sub = col.row()
        sub.active = rd.use_color_management == True
        sub.prop(rd, "use_color_unpremultiply")
        col.prop(rd, "alpha_mode", text="Alpha")


class RENDER_PT_performance(RenderButtonsPanel, Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.label(text="Threads:")
        col.row().prop(rd, "threads_mode", expand=True)
        sub = col.column()
        sub.enabled = rd.threads_mode == 'FIXED'
        sub.prop(rd, "threads")
        sub = col.column(align=True)
        sub.label(text="Tiles:")
        sub.prop(rd, "parts_x", text="X")
        sub.prop(rd, "parts_y", text="Y")

        col = split.column()
        col.label(text="Memory:")
        sub = col.column()
        sub.enabled = not (rd.use_border or rd.use_full_sample)
        sub.prop(rd, "use_save_buffers")
        sub = col.column()
        sub.active = rd.use_compositing
        sub.prop(rd, "use_free_image_textures")
        sub.prop(rd, "use_free_unused_nodes")
        sub = col.column()
        sub.active = rd.use_raytrace
        sub.label(text="Acceleration structure:")
        sub.prop(rd, "raytrace_method", text="")
        if rd.raytrace_method == 'OCTREE':
            sub.prop(rd, "octree_resolution", text="Resolution")
        else:
            sub.prop(rd, "use_instances", text="Instances")
        sub.prop(rd, "use_local_coords", text="Local Coordinates")


class RENDER_PT_post_processing(RenderButtonsPanel, Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        split.prop(rd, "dither_intensity", text="Dither", slider=True)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_fields", text="Fields")
        sub = col.column()
        sub.active = rd.use_fields
        sub.row().prop(rd, "field_order", expand=True)
        sub.prop(rd, "use_fields_still", text="Still")

        col = split.column()
        col.prop(rd, "use_edge_enhance")
        sub = col.column()
        sub.active = rd.use_edge_enhance
        sub.prop(rd, "edge_threshold", text="Threshold", slider=True)
        sub.prop(rd, "edge_color", text="")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_freestyle", text="Freestyle")
        sub = col.column()
        sub.label(text="Line Thickness:")
        sub.active = rd.use_freestyle
        sub.row().prop(rd, "line_thickness_mode", expand=True)
        subrow = sub.row()
        subrow.active = (rd.line_thickness_mode == "ABSOLUTE")
        subrow.prop(rd, "unit_line_thickness")


class RENDER_PT_stamp(RenderButtonsPanel, Panel):
    bl_label = "Stamp"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_stamp", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.active = rd.use_stamp

        layout.prop(rd, "stamp_font_size", text="Font Size")

        row = layout.row()
        row.column().prop(rd, "stamp_foreground", slider=True)
        row.column().prop(rd, "stamp_background", slider=True)

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_stamp_time", text="Time")
        col.prop(rd, "use_stamp_date", text="Date")
        col.prop(rd, "use_stamp_render_time", text="RenderTime")
        col.prop(rd, "use_stamp_frame", text="Frame")
        col.prop(rd, "use_stamp_scene", text="Scene")

        col = split.column()
        col.prop(rd, "use_stamp_camera", text="Camera")
        col.prop(rd, "use_stamp_lens", text="Lens")
        col.prop(rd, "use_stamp_filename", text="Filename")
        col.prop(rd, "use_stamp_marker", text="Marker")
        col.prop(rd, "use_stamp_sequencer_strip", text="Seq. Strip")

        row = layout.split(percentage=0.2)
        row.prop(rd, "use_stamp_note", text="Note")
        sub = row.row()
        sub.active = rd.use_stamp_note
        sub.prop(rd, "stamp_note_text", text="")


class RENDER_PT_output(RenderButtonsPanel, Panel):
    bl_label = "Output"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        image_settings = rd.image_settings
        file_format = image_settings.file_format

        layout.prop(rd, "filepath", text="")

        flow = layout.column_flow()
        flow.prop(rd, "use_overwrite")
        flow.prop(rd, "use_placeholder")
        flow.prop(rd, "use_file_extension")

        layout.template_image_settings(image_settings)

        if file_format == 'QUICKTIME_CARBON':
            layout.operator("scene.render_data_set_quicktime_codec")

        elif file_format == 'QUICKTIME_QTKIT':
            quicktime = rd.quicktime

            split = layout.split()
            col = split.column()
            col.prop(quicktime, "codec_type", text="Video Codec")
            col.prop(quicktime, "codec_spatial_quality", text="Quality")

            # Audio
            col.prop(quicktime, "audiocodec_type", text="Audio Codec")
            if quicktime.audiocodec_type != 'No audio':
                split = layout.split()
                if quicktime.audiocodec_type == 'LPCM':
                    split.prop(quicktime, "audio_bitdepth", text="")

                split.prop(quicktime, "audio_samplerate", text="")

                split = layout.split()
                col = split.column()
                if quicktime.audiocodec_type == 'AAC':
                    col.prop(quicktime, "audio_bitrate")

                subsplit = split.split()
                col = subsplit.column()

                if quicktime.audiocodec_type == 'AAC':
                    col.prop(quicktime, "audio_codec_isvbr")

                col = subsplit.column()
                col.prop(quicktime, "audio_resampling_hq")


class RENDER_PT_encoding(RenderButtonsPanel, Panel):
    bl_label = "Encoding"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.image_settings.file_format in {'FFMPEG', 'XVID', 'H264', 'THEORA'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        ffmpeg = rd.ffmpeg

        layout.menu("RENDER_MT_ffmpeg_presets", text="Presets")

        split = layout.split()
        split.prop(rd.ffmpeg, "format")
        if ffmpeg.format in {'AVI', 'QUICKTIME', 'MKV', 'OGG'}:
            split.prop(ffmpeg, "codec")
        elif rd.ffmpeg.format == 'H264':
            split.prop(ffmpeg, "use_lossless_output")
        else:
            split.label()

        row = layout.row()
        row.prop(ffmpeg, "video_bitrate")
        row.prop(ffmpeg, "gopsize")

        split = layout.split()

        col = split.column()
        col.label(text="Rate:")
        col.prop(ffmpeg, "minrate", text="Minimum")
        col.prop(ffmpeg, "maxrate", text="Maximum")
        col.prop(ffmpeg, "buffersize", text="Buffer")

        col = split.column()
        col.prop(ffmpeg, "use_autosplit")
        col.label(text="Mux:")
        col.prop(ffmpeg, "muxrate", text="Rate")
        col.prop(ffmpeg, "packetsize", text="Packet Size")

        layout.separator()

        # Audio:
        if ffmpeg.format != 'MP3':
            layout.prop(ffmpeg, "audio_codec", text="Audio Codec")

        row = layout.row()
        row.prop(ffmpeg, "audio_bitrate")
        row.prop(ffmpeg, "audio_volume", slider=True)


class RENDER_PT_bake(RenderButtonsPanel, Panel):
    bl_label = "Bake"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.operator("object.bake_image", icon='RENDER_STILL')

        layout.prop(rd, "bake_type")

        multires_bake = False
        if rd.bake_type in ['NORMALS', 'DISPLACEMENT']:
            layout.prop(rd, "use_bake_multires")
            multires_bake = rd.use_bake_multires

        if not multires_bake:
            if rd.bake_type == 'NORMALS':
                layout.prop(rd, "bake_normal_space")
            elif rd.bake_type in {'DISPLACEMENT', 'AO'}:
                layout.prop(rd, "use_bake_normalize")

            # col.prop(rd, "bake_aa_mode")
            # col.prop(rd, "use_bake_antialiasing")

            layout.separator()

            split = layout.split()

            col = split.column()
            col.prop(rd, "use_bake_clear")
            col.prop(rd, "bake_margin")
            col.prop(rd, "bake_quad_split", text="Split")

            col = split.column()
            col.prop(rd, "use_bake_selected_to_active")
            sub = col.column()
            sub.active = rd.use_bake_selected_to_active
            sub.prop(rd, "bake_distance")
            sub.prop(rd, "bake_bias")
        else:
            if rd.bake_type == 'DISPLACEMENT':
                layout.prop(rd, "use_bake_lores_mesh")

            layout.prop(rd, "use_bake_clear")
            layout.prop(rd, "bake_margin")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
