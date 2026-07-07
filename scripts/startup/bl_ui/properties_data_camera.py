# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel
from bpy.app.translations import contexts as i18n_contexts
from rna_prop_ui import PropertyPanel
from bl_ui.utils import PresetPanel
from bl_ui.space_properties import PropertiesAnimationMixin


class CameraButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.camera and (engine in cls.COMPAT_ENGINES)


class CAMERA_PT_presets(PresetPanel, Panel):
    bl_label = "Camera Presets"
    preset_subdir = "camera"
    preset_operator = "script.execute_preset"
    preset_add_operator = "camera.preset_add"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }


class CAMERA_PT_safe_areas_presets(PresetPanel, Panel):
    bl_label = "Camera Presets"
    preset_subdir = "safe_areas"
    preset_operator = "script.execute_preset"
    preset_add_operator = "camera.safe_areas_preset_add"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }


class DATA_PT_context_camera(CameraButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout

        ob = context.object
        cam = context.camera
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif cam:
            layout.template_ID(space, "pin_id")


class DATA_PT_lens(CameraButtonsPanel, Panel):
    bl_label = "Lens"
    bl_translation_context = i18n_contexts.id_camera
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera

        layout.prop(cam, "type")

        col = layout.column()
        col.separator()

        if cam.type == 'PERSP':
            if cam.lens_unit == 'MILLIMETERS':
                col.prop(cam, "lens")
            elif cam.lens_unit == 'FOV':
                col.prop(cam, "angle")
            col.prop(cam, "lens_unit")

        elif cam.type == 'ORTHO':
            col.prop(cam, "ortho_scale")

        elif cam.type == 'PANO':
            engine = context.engine
            if engine == 'CYCLES':
                col.prop(cam, "panorama_type")
                if cam.panorama_type == 'FISHEYE_EQUIDISTANT':
                    col.prop(cam, "fisheye_fov")
                elif cam.panorama_type == 'FISHEYE_EQUISOLID':
                    col.prop(cam, "fisheye_lens", text="Lens")
                    col.prop(cam, "fisheye_fov")
                elif cam.panorama_type == 'EQUIRECTANGULAR':
                    sub = col.column(align=True)
                    sub.prop(cam, "latitude_min", text="Latitude Min")
                    sub.prop(cam, "latitude_max", text="Max")
                    sub = col.column(align=True)
                    sub.prop(cam, "longitude_min", text="Longitude Min")
                    sub.prop(cam, "longitude_max", text="Max")
                elif cam.panorama_type == 'FISHEYE_LENS_POLYNOMIAL':
                    col.prop(cam, "fisheye_fov")
                    col.prop(cam, "fisheye_polynomial_k0", text="K0")
                    col.prop(cam, "fisheye_polynomial_k1", text="K1")
                    col.prop(cam, "fisheye_polynomial_k2", text="K2")
                    col.prop(cam, "fisheye_polynomial_k3", text="K3")
                    col.prop(cam, "fisheye_polynomial_k4", text="K4")
                elif cam.panorama_type == 'CENTRAL_CYLINDRICAL':
                    sub = col.column(align=True)
                    sub.prop(cam, "central_cylindrical_range_v_min", text="Height Min")
                    sub.prop(cam, "central_cylindrical_range_v_max", text="Max")
                    sub = col.column(align=True)
                    sub.prop(cam, "central_cylindrical_range_u_min", text="Longitude Min")
                    sub.prop(cam, "central_cylindrical_range_u_max", text="Max")
                    sub = col.column(align=True)
                    sub.prop(cam, "central_cylindrical_radius", text="Cylinder Radius")

            elif engine in {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}:
                if cam.lens_unit == 'MILLIMETERS':
                    col.prop(cam, "lens")
                elif cam.lens_unit == 'FOV':
                    col.prop(cam, "angle")
                col.prop(cam, "lens_unit")

        elif cam.type == 'CUSTOM':
            engine = context.engine
            if engine == 'CYCLES':
                sub = col.row()
                sub.prop(cam, "custom_mode", text=" ", expand=True)

                sub = col.row(align=True)
                if cam.custom_mode == 'EXTERNAL':
                    sub.prop(cam, "custom_filepath", text=" ")
                else:
                    sub.prop(cam, "custom_shader", text=" ")
                sub.operator("object.camera_custom_update", icon='FILE_REFRESH', text="")

        col = layout.column()
        col.separator()

        sub = col.column(align=True)
        sub.prop(cam, "shift_x", text="Shift X")
        sub.prop(cam, "shift_y", text="Y")

        col.separator()
        sub = col.column(align=True)
        sub.prop(cam, "clip_start", text="Clip Start")
        sub.prop(cam, "clip_end", text="End", text_ctxt=i18n_contexts.id_camera)


class DATA_PT_camera_stereoscopy(CameraButtonsPanel, Panel):
    bl_label = "Stereoscopy"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        render = context.scene.render
        return (super().poll(context) and render.use_multiview and render.views_format == 'STEREO_3D')

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        render = context.scene.render
        st = context.camera.stereo
        cam = context.camera

        is_spherical_stereo = cam.type != 'ORTHO' and render.use_spherical_stereo
        use_spherical_stereo = is_spherical_stereo and st.use_spherical_stereo

        layout.prop(st, "convergence_mode")

        col = layout.column()
        sub = col.column()
        sub.active = st.convergence_mode != 'PARALLEL'
        sub.prop(st, "convergence_distance")

        col.prop(st, "interocular_distance")

        if is_spherical_stereo:
            col.separator()
            col.prop(st, "use_spherical_stereo")
            sub = col.column()
            sub.active = st.use_spherical_stereo
            sub.prop(st, "use_pole_merge")

            sub = col.column(align=True)
            sub.active = st.use_pole_merge
            sub.prop(st, "pole_merge_angle_from", text="Pole Merge Angle Start")
            sub.prop(st, "pole_merge_angle_to", text="End")

        col = layout.column()
        col.active = not use_spherical_stereo
        col.separator()
        col.prop(st, "pivot")


class DATA_PT_camera(CameraButtonsPanel, Panel):
    bl_label = "Camera"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header_preset(self, _context):
        CAMERA_PT_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        layout.use_property_split = True

        col = layout.column()
        col.prop(cam, "sensor_fit")

        if cam.sensor_fit == 'AUTO':
            col.prop(cam, "sensor_width", text="Size")
        else:
            sub = col.column(align=True)
            sub.active = cam.sensor_fit == 'HORIZONTAL'
            sub.prop(cam, "sensor_width", text="Width")

            sub = col.column(align=True)
            sub.active = cam.sensor_fit == 'VERTICAL'
            sub.prop(cam, "sensor_height", text="Height")


class DATA_PT_camera_dof(CameraButtonsPanel, Panel):
    bl_label = "Depth of Field"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        cam = context.camera
        dof = cam.dof
        self.layout.prop(dof, "use_dof", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera
        dof = cam.dof
        layout.active = dof.use_dof

        col = layout.column()
        col.prop(dof, "focus_object", text="Focus on Object")
        if dof.focus_object and dof.focus_object.type == 'ARMATURE':
            col.prop_search(dof, "focus_subtarget", dof.focus_object.data, "bones", text="Focus on Bone")

        sub = col.column()
        sub.active = (dof.focus_object is None)
        row = sub.row(align=True)
        row.prop(dof, "focus_distance", text="Focus Distance")
        row.operator(
            "ui.eyedropper_depth",
            icon='EYEDROPPER',
            text="",
        ).prop_data_path = "scene.camera.data.dof.focus_distance"


class DATA_PT_camera_dof_aperture(CameraButtonsPanel, Panel):
    bl_label = "Aperture"
    bl_parent_id = "DATA_PT_camera_dof"
    COMPAT_ENGINES = {
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera
        dof = cam.dof
        layout.active = dof.use_dof

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(dof, "aperture_fstop")

        col = flow.column()
        col.prop(dof, "aperture_blades")
        col.prop(dof, "aperture_rotation")
        col.prop(dof, "aperture_ratio")


class DATA_PT_camera_background_image(CameraButtonsPanel, Panel):
    bl_label = "Background Images"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        cam = context.camera

        self.layout.prop(cam, "show_background_images", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        cam = context.camera
        use_multiview = context.scene.render.use_multiview

        col = layout.column()
        col.operator("view3d.camera_background_image_add", text="Add Image")

        for i, bg in enumerate(cam.background_images):
            layout.active = cam.show_background_images
            box = layout.box()
            row = box.row(align=True)
            row.prop(bg, "show_expanded", text="", emboss=False)
            if bg.source == 'IMAGE' and bg.image:
                row.prop(bg.image, "name", text="", emboss=False)
            elif bg.source == 'MOVIE_CLIP' and bg.clip:
                row.prop(bg.clip, "name", text="", emboss=False)
            elif bg.source and bg.use_camera_clip:
                row.label(text="Active Clip")
            else:
                row.label(text="Not Set")

            row.prop(
                bg,
                "show_background_image",
                text="",
                emboss=False,
                icon='RESTRICT_VIEW_OFF' if bg.show_background_image else 'RESTRICT_VIEW_ON',
            )

            row.operator("view3d.camera_background_image_remove", text="", emboss=False, icon='X').index = i

            if bg.show_expanded:
                row = box.row()
                row.prop(bg, "source", expand=True)

                has_bg = False
                if bg.source == 'IMAGE':
                    row = box.row()
                    row.template_ID(bg, "image", open="image.open")
                    if bg.image is not None:
                        box.template_image(bg, "image", bg.image_user, compact=True)
                        has_bg = True

                        if use_multiview:
                            box.prop(bg.image, "use_multiview")

                            column = box.column()
                            column.active = bg.image.use_multiview

                            column.label(text="Views Format:")
                            column.row().prop(bg.image, "views_format", expand=True)

                            sub = column.box()
                            sub.active = bg.image.views_format == 'STEREO_3D'
                            sub.template_image_stereo_3d(bg.image.stereo_3d_format)

                elif bg.source == 'MOVIE_CLIP':
                    box.prop(bg, "use_camera_clip", text="Active Clip")

                    column = box.column()
                    column.active = not bg.use_camera_clip
                    column.template_ID(bg, "clip", open="clip.open")

                    if bg.clip:
                        column.template_movieclip(bg, "clip", compact=True)

                    if bg.use_camera_clip or bg.clip:
                        has_bg = True

                    column = box.column()
                    column.active = has_bg
                    column.prop(bg.clip_user, "use_render_undistorted")
                    column.prop(bg.clip_user, "proxy_render_size")

                if has_bg:
                    col = box.column()
                    if bg.image is not None:
                        col.prop(bg.image, "use_view_as_render")
                    col.prop(bg, "alpha")
                    col.row().prop(bg, "display_depth", expand=True)

                    col.row().prop(bg, "frame_method", expand=True)

                    row = box.row()
                    row.prop(bg, "offset")

                    col = box.column()
                    col.prop(bg, "rotation")
                    col.prop(bg, "scale")

                    col = box.column(heading="Flip", heading_ctxt=i18n_contexts.id_image)
                    col.prop(bg, "use_flip_x", text="X")
                    col.prop(bg, "use_flip_y", text="Y")


class DATA_PT_camera_display(CameraButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera

        col = layout.column(align=True)

        col.prop(cam, "display_size", text="Size")

        col = layout.column(heading="Show")
        col.prop(cam, "show_limits", text="Limits")
        col.prop(cam, "show_mist", text="Mist")
        col.prop(cam, "show_sensor", text="Sensor")
        col.prop(cam, "show_name", text="Name")

        col = layout.column(align=False, heading="Passepartout")
        col.use_property_decorate = False
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.prop(cam, "show_passepartout", text="")
        sub = sub.row(align=True)
        sub.active = cam.show_passepartout
        sub.prop(cam, "passepartout_alpha", text="")
        row.prop_decorator(cam, "passepartout_alpha")


class DATA_PT_camera_display_composition_guides(CameraButtonsPanel, Panel):
    bl_label = "Composition Guides"
    bl_parent_id = "DATA_PT_camera_display"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        cam = context.camera

        layout.prop(cam, "show_composition_thirds")

        col = layout.column(heading="Center", align=True)
        col.prop(cam, "show_composition_center")
        col.prop(cam, "show_composition_center_diagonal", text="Diagonal")

        col = layout.column(heading="Golden", align=True)
        col.prop(cam, "show_composition_golden", text="Ratio")
        col.prop(cam, "show_composition_golden_tria_a", text="Triangle A")
        col.prop(cam, "show_composition_golden_tria_b", text="Triangle B")

        col = layout.column(heading="Harmony", align=True)
        col.prop(cam, "show_composition_harmony_tri_a", text="Triangle A")
        col.prop(cam, "show_composition_harmony_tri_b", text="Triangle B")

        col = layout.column()
        col.prop(cam, "composition_guide_color", text="Color")


class DATA_PT_camera_safe_areas(CameraButtonsPanel, Panel):
    bl_label = "Safe Areas"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        cam = context.camera

        self.layout.prop(cam, "show_safe_areas", text="")

    def draw_header_preset(self, _context):
        CAMERA_PT_safe_areas_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        safe_data = context.scene.safe_areas
        camera = context.camera

        layout.use_property_split = True

        layout.active = camera.show_safe_areas

        col = layout.column()

        sub = col.column()
        sub.prop(safe_data, "title", slider=True)
        sub.prop(safe_data, "action", slider=True)


class DATA_PT_camera_safe_areas_center_cut(CameraButtonsPanel, Panel):
    bl_label = "Center-Cut Safe Areas"
    bl_parent_id = "DATA_PT_camera_safe_areas"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        cam = context.camera

        layout = self.layout
        layout.active = cam.show_safe_areas
        layout.prop(cam, "show_safe_center", text="")

    def draw(self, context):
        layout = self.layout
        safe_data = context.scene.safe_areas
        camera = context.camera

        layout.use_property_split = True

        layout.active = camera.show_safe_areas and camera.show_safe_center

        col = layout.column()
        col.prop(safe_data, "title_center", slider=True)
        col.prop(safe_data, "action_center", slider=True)


class DATA_PT_camera_animation(CameraButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }
    _animated_id_context_property = "camera"


class DATA_PT_custom_props_camera(CameraButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }
    _context_path = "object.data"
    _property_type = bpy.types.Camera


classes = (
    CAMERA_PT_presets,
    CAMERA_PT_safe_areas_presets,
    DATA_PT_context_camera,
    DATA_PT_lens,
    DATA_PT_camera_dof,
    DATA_PT_camera_dof_aperture,
    DATA_PT_camera,
    DATA_PT_camera_stereoscopy,
    DATA_PT_camera_safe_areas,
    DATA_PT_camera_safe_areas_center_cut,
    DATA_PT_camera_background_image,
    DATA_PT_camera_display,
    DATA_PT_camera_display_composition_guides,
    DATA_PT_camera_animation,
    DATA_PT_custom_props_camera,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
