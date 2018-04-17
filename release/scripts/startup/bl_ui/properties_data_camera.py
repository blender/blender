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
from bpy.types import Panel, Menu
from rna_prop_ui import PropertyPanel


class CameraButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.camera and (engine in cls.COMPAT_ENGINES)


class CAMERA_MT_presets(Menu):
    bl_label = "Camera Presets"
    preset_subdir = "camera"
    preset_operator = "script.execute_preset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}
    draw = Menu.draw_preset


class SAFE_AREAS_MT_presets(Menu):
    bl_label = "Camera Presets"
    preset_subdir = "safe_areas"
    preset_operator = "script.execute_preset"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}
    draw = Menu.draw_preset


class DATA_PT_context_camera(CameraButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        cam = context.camera
        space = context.space_data

        split = layout.split(percentage=0.65)
        if ob:
            split.template_ID(ob, "data")
            split.separator()
        elif cam:
            split.template_ID(space, "pin_id")
            split.separator()


class DATA_PT_lens(CameraButtonsPanel, Panel):
    bl_label = "Lens"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        layout.row().prop(cam, "type", expand=True)

        split = layout.split()

        col = split.column()
        if cam.type == 'PERSP':
            row = col.row()
            if cam.lens_unit == 'MILLIMETERS':
                row.prop(cam, "lens")
            elif cam.lens_unit == 'FOV':
                row.prop(cam, "angle")
            row.prop(cam, "lens_unit", text="")

        elif cam.type == 'ORTHO':
            col.prop(cam, "ortho_scale")

        elif cam.type == 'PANO':
            engine = context.engine
            if engine == 'CYCLES':
                ccam = cam.cycles
                col.prop(ccam, "panorama_type", text="Type")
                if ccam.panorama_type == 'FISHEYE_EQUIDISTANT':
                    col.prop(ccam, "fisheye_fov")
                elif ccam.panorama_type == 'FISHEYE_EQUISOLID':
                    row = layout.row()
                    row.prop(ccam, "fisheye_lens", text="Lens")
                    row.prop(ccam, "fisheye_fov")
                elif ccam.panorama_type == 'EQUIRECTANGULAR':
                    row = layout.row()
                    sub = row.column(align=True)
                    sub.prop(ccam, "latitude_min")
                    sub.prop(ccam, "latitude_max")
                    sub = row.column(align=True)
                    sub.prop(ccam, "longitude_min")
                    sub.prop(ccam, "longitude_max")
            elif engine in {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}:
                row = col.row()
                if cam.lens_unit == 'MILLIMETERS':
                    row.prop(cam, "lens")
                elif cam.lens_unit == 'FOV':
                    row.prop(cam, "angle")
                row.prop(cam, "lens_unit", text="")

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Shift:")
        col.prop(cam, "shift_x", text="X")
        col.prop(cam, "shift_y", text="Y")

        col = split.column(align=True)
        col.label(text="Clipping:")
        col.prop(cam, "clip_start", text="Start")
        col.prop(cam, "clip_end", text="End")


class DATA_PT_camera_stereoscopy(CameraButtonsPanel, Panel):
    bl_label = "Stereoscopy"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        render = context.scene.render
        return (super().poll(context) and render.use_multiview and
                render.views_format == 'STEREO_3D')

    def draw(self, context):
        layout = self.layout
        render = context.scene.render
        st = context.camera.stereo
        cam = context.camera

        is_spherical_stereo = cam.type != 'ORTHO' and render.use_spherical_stereo
        use_spherical_stereo = is_spherical_stereo and st.use_spherical_stereo

        col = layout.column()
        col.row().prop(st, "convergence_mode", expand=True)

        sub = col.column()
        sub.active = st.convergence_mode != 'PARALLEL'
        sub.prop(st, "convergence_distance")

        col.prop(st, "interocular_distance")

        if is_spherical_stereo:
            col.separator()
            row = col.row()
            row.prop(st, "use_spherical_stereo")
            sub = row.row()
            sub.active = st.use_spherical_stereo
            sub.prop(st, "use_pole_merge")
            row = col.row(align=True)
            row.active = st.use_pole_merge
            row.prop(st, "pole_merge_angle_from")
            row.prop(st, "pole_merge_angle_to")

        col.label(text="Pivot:")
        row = col.row()
        row.active = not use_spherical_stereo
        row.prop(st, "pivot", expand=True)


class DATA_PT_camera(CameraButtonsPanel, Panel):
    bl_label = "Camera"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        row = layout.row(align=True)

        row.menu("CAMERA_MT_presets", text=bpy.types.CAMERA_MT_presets.bl_label)
        row.operator("camera.preset_add", text="", icon='ZOOMIN')
        row.operator("camera.preset_add", text="", icon='ZOOMOUT').remove_active = True

        layout.label(text="Sensor:")

        split = layout.split()

        col = split.column(align=True)
        if cam.sensor_fit == 'AUTO':
            col.prop(cam, "sensor_width", text="Size")
        else:
            sub = col.column(align=True)
            sub.active = cam.sensor_fit == 'HORIZONTAL'
            sub.prop(cam, "sensor_width", text="Width")
            sub = col.column(align=True)
            sub.active = cam.sensor_fit == 'VERTICAL'
            sub.prop(cam, "sensor_height", text="Height")

        col = split.column(align=True)
        col.prop(cam, "sensor_fit", text="")


class DATA_PT_camera_dof(CameraButtonsPanel, Panel):
    bl_label = "Depth of Field"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera
        dof_options = cam.gpu_dof

        split = layout.split()

        col = split.column()
        col.label(text="Focus:")
        col.prop(cam, "dof_object", text="")
        sub = col.column()
        sub.active = (cam.dof_object is None)
        sub.prop(cam, "dof_distance", text="Distance")

        if context.engine == 'BLENDER_EEVEE':
            col = split.column(align=True)
            col.label("Aperture:")
            engine = context.engine
            sub = col.column(align=True)
            sub.prop(dof_options, "fstop")
            sub.prop(dof_options, "blades")
            sub.prop(dof_options, "rotation")
            sub.prop(dof_options, "ratio")
        else:
            hq_support = dof_options.is_hq_supported
            col = split.column(align=True)
            col.label("Viewport:")
            sub = col.column()
            sub.active = hq_support
            sub.prop(dof_options, "use_high_quality")
            col.prop(dof_options, "fstop")
            if dof_options.use_high_quality and hq_support:
                col.prop(dof_options, "blades")


class DATA_PT_camera_background_image(CameraButtonsPanel, Panel):
    bl_label = "Background Images"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw_header(self, context):
        cam = context.camera

        self.layout.prop(cam, "show_background_images", text="")

    def draw(self, context):
        layout = self.layout

        cam = context.camera
        use_multiview = context.scene.render.use_multiview

        col = layout.column()
        col.operator("view3d.background_image_add", text="Add Image")

        for i, bg in enumerate(cam.background_images):
            layout.active = cam.show_background_images
            box = layout.box()
            row = box.row(align=True)
            row.prop(bg, "show_expanded", text="", emboss=False)
            if bg.source == 'IMAGE' and bg.image:
                row.prop(bg.image, "name", text="", emboss=False)
            elif bg.source == 'MOVIE_CLIP' and bg.clip:
                row.prop(bg.clip, "name", text="", emboss=False)
            else:
                row.label(text="Not Set")

            if bg.show_background_image:
                row.prop(bg, "show_background_image", text="", emboss=False, icon='RESTRICT_VIEW_OFF')
            else:
                row.prop(bg, "show_background_image", text="", emboss=False, icon='RESTRICT_VIEW_ON')

            row.operator("view3d.background_image_remove", text="", emboss=False, icon='X').index = i

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

                        if use_multiview and bg.view_axis in {'CAMERA', 'ALL'}:
                            box.prop(bg.image, "use_multiview")

                            column = box.column()
                            column.active = bg.image.use_multiview

                            column.label(text="Views Format:")
                            column.row().prop(bg.image, "views_format", expand=True)

                            sub = column.box()
                            sub.active = bg.image.views_format == 'STEREO_3D'
                            sub.template_image_stereo_3d(bg.image.stereo_3d_format)

                elif bg.source == 'MOVIE_CLIP':
                    box.prop(bg, "use_camera_clip")

                    column = box.column()
                    column.active = not bg.use_camera_clip
                    column.template_ID(bg, "clip", open="clip.open")

                    if bg.clip:
                        column.template_movieclip(bg, "clip", compact=True)

                    if bg.use_camera_clip or bg.clip:
                        has_bg = True

                    column = box.column()
                    column.active = has_bg
                    column.prop(bg.clip_user, "proxy_render_size", text="")
                    column.prop(bg.clip_user, "use_render_undistorted")

                if has_bg:
                    col = box.column()
                    col.prop(bg, "alpha", slider=True)
                    col.row().prop(bg, "draw_depth", expand=True)

                    col.row().prop(bg, "frame_method", expand=True)

                    box = col.box()
                    row = box.row()
                    row.prop(bg, "offset")

                    row = box.row()
                    row.prop(bg, "use_flip_x")
                    row.prop(bg, "use_flip_y")

                    row = box.row()
                    row.prop(bg, "rotation")
                    row.prop(bg, "scale")


class DATA_PT_camera_display(CameraButtonsPanel, Panel):
    bl_label = "Display"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        split = layout.split()

        col = split.column()
        col.prop(cam, "show_limits", text="Limits")
        col.prop(cam, "show_mist", text="Mist")

        col.prop(cam, "show_sensor", text="Sensor")
        col.prop(cam, "show_name", text="Name")

        col = split.column()
        col.prop_menu_enum(cam, "show_guide")
        col.separator()
        col.prop(cam, "draw_size", text="Size")
        col.separator()
        col.prop(cam, "show_passepartout", text="Passepartout")
        sub = col.column()
        sub.active = cam.show_passepartout
        sub.prop(cam, "passepartout_alpha", text="Alpha", slider=True)


class DATA_PT_camera_safe_areas(CameraButtonsPanel, Panel):
    bl_label = "Safe Areas"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw_header(self, context):
        cam = context.camera

        self.layout.prop(cam, "show_safe_areas", text="")

    def draw(self, context):
        layout = self.layout
        safe_data = context.scene.safe_areas
        camera = context.camera

        draw_display_safe_settings(layout, safe_data, camera)


class DATA_PT_custom_props_camera(CameraButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_CLAY', 'BLENDER_EEVEE'}
    _context_path = "object.data"
    _property_type = bpy.types.Camera


def draw_display_safe_settings(layout, safe_data, settings):
    show_safe_areas = settings.show_safe_areas
    show_safe_center = settings.show_safe_center

    split = layout.split()

    col = split.column()
    row = col.row(align=True)
    row.menu("SAFE_AREAS_MT_presets", text=bpy.types.SAFE_AREAS_MT_presets.bl_label)
    row.operator("safe_areas.preset_add", text="", icon='ZOOMIN')
    row.operator("safe_areas.preset_add", text="", icon='ZOOMOUT').remove_active = True

    col = split.column()
    col.prop(settings, "show_safe_center", text="Center-Cut Safe Areas")

    split = layout.split()
    col = split.column()
    col.active = show_safe_areas
    col.prop(safe_data, "title", slider=True)
    col.prop(safe_data, "action", slider=True)

    col = split.column()
    col.active = show_safe_areas and show_safe_center
    col.prop(safe_data, "title_center", slider=True)
    col.prop(safe_data, "action_center", slider=True)


classes = (
    CAMERA_MT_presets,
    SAFE_AREAS_MT_presets,
    DATA_PT_context_camera,
    DATA_PT_lens,
    DATA_PT_camera,
    DATA_PT_camera_stereoscopy,
    DATA_PT_camera_dof,
    DATA_PT_camera_display,
    DATA_PT_camera_background_image,
    DATA_PT_camera_safe_areas,
    DATA_PT_custom_props_camera,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
