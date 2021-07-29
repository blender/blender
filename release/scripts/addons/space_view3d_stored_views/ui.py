# gpl authors: nfloyd, Francesco Siddi

import logging
module_logger = logging.getLogger(__name__)

import bpy
import blf
from . import core
from bpy.types import (
        Operator,
        Panel,
        )

"""
  If view name display is enabled,
  it will check periodically if the view has been modified
  since last set.
  get_preferences_timer() is the time in seconds between these checks.
  It can be increased, if the view become sluggish
  It is set in the add-on preferences
"""


# Utility function get_preferences_timer for update of 3d view draw
def get_preferences_timer():
    # replace the key if the add-on name changes
    # TODO: expose refresh rate to ui???
    addon = bpy.context.user_preferences.addons[__package__]
    timer_update = (addon.preferences.view_3d_update_rate if addon else False)

    return timer_update


def init_draw(context=None):
    if context is None:
        context = bpy.context

    if "stored_views_osd" not in context.window_manager:
        context.window_manager["stored_views_osd"] = False

    if not context.window_manager["stored_views_osd"]:
        context.window_manager["stored_views_osd"] = True
        bpy.ops.stored_views.draw()


def _draw_callback_px(self, context):
    if context.area and context.area.type == 'VIEW_3D':
        r_width = text_location = context.region.width
        r_height = context.region.height
        font_id = 0  # TODO: need to find out how best to get font_id

        blf.size(font_id, 11, context.user_preferences.system.dpi)
        text_size = blf.dimensions(0, self.view_name)

        # compute the text location
        text_location = 0
        overlap = context.user_preferences.system.use_region_overlap
        if overlap:
            for region in context.area.regions:
                if region.type == "UI":
                    text_location = r_width - region.width

        text_x = text_location - text_size[0] - 10
        text_y = r_height - text_size[1] - 8
        blf.position(font_id, text_x, text_y, 0)
        blf.draw(font_id, self.view_name)


class VIEW3D_stored_views_draw(Operator):
    bl_idname = "stored_views.draw"
    bl_label = "Show current"
    bl_description = "Toggle the display current view name in the view 3D"

    _handle = None
    _timer = None

    @staticmethod
    def handle_add(self, context):
        VIEW3D_stored_views_draw._handle = bpy.types.SpaceView3D.draw_handler_add(
            _draw_callback_px, (self, context), 'WINDOW', 'POST_PIXEL')
        VIEW3D_stored_views_draw._timer = \
            context.window_manager.event_timer_add(get_preferences_timer(), context.window)

    @staticmethod
    def handle_remove(context):
        if VIEW3D_stored_views_draw._handle is not None:
            bpy.types.SpaceView3D.draw_handler_remove(VIEW3D_stored_views_draw._handle, 'WINDOW')
        if VIEW3D_stored_views_draw._timer is not None:
            context.window_manager.event_timer_remove(VIEW3D_stored_views_draw._timer)
        VIEW3D_stored_views_draw._handle = None
        VIEW3D_stored_views_draw._timer = None

    @classmethod
    def poll(cls, context):
        # return context.mode == 'OBJECT'
        return True

    def modal(self, context, event):
        if context.area:
            context.area.tag_redraw()

        if not context.area or context.area.type != "VIEW_3D":
            return {"PASS_THROUGH"}

        data = core.DataStore()
        stored_views = context.scene.stored_views

        if len(data.list) > 0 and \
           data.current_index >= 0 and \
           not stored_views.view_modified:

            if not stored_views.view_modified:
                sv = data.list[data.current_index]
                self.view_name = sv.name
                if event.type == 'TIMER':
                    is_modified = False
                    if data.mode == 'VIEW':
                        is_modified = core.View.is_modified(context, sv)
                    elif data.mode == 'POV':
                        is_modified = core.POV.is_modified(context, sv)
                    elif data.mode == 'LAYERS':
                        is_modified = core.Layers.is_modified(context, sv)
                    elif data.mode == 'DISPLAY':
                        is_modified = core.Display.is_modified(context, sv)
                    if is_modified:
                        module_logger.debug(
                                'view modified - index: %s name: %s' % (data.current_index, sv.name)
                                )
                        self.view_name = ""
                        stored_views.view_modified = is_modified

                return {"PASS_THROUGH"}
        else:
            module_logger.debug('exit')
            context.window_manager["stored_views_osd"] = False
            VIEW3D_stored_views_draw.handle_remove(context)

            return {'FINISHED'}

    def execute(self, context):
        if context.area.type == "VIEW_3D":
            self.view_name = ""
            VIEW3D_stored_views_draw.handle_add(self, context)
            context.window_manager.modal_handler_add(self)

            return {"RUNNING_MODAL"}
        else:
            self.report({"WARNING"}, "View3D not found. Operation Cancelled")

            return {"CANCELLED"}


class VIEW3D_PT_properties_stored_views(Panel):
    bl_label = "Stored Views"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"

    def draw(self, context):
        self.logger = logging.getLogger('%s Properties panel' % __name__)
        layout = self.layout

        if bpy.ops.view3d.stored_views_initialize.poll():
            layout.operator("view3d.stored_views_initialize")
            return

        stored_views = context.scene.stored_views

        # UI : mode
        col = layout.column(align=True)
        col.prop_enum(stored_views, "mode", 'VIEW')
        row = layout.row(align=True)
        row.operator("view3d.camera_to_view", text="Camera To view")
        row.operator("stored_views.newcamera")

        row = col.row(align=True)
        row.prop_enum(stored_views, "mode", 'POV')
        row.prop_enum(stored_views, "mode", 'LAYERS')
        row.prop_enum(stored_views, "mode", 'DISPLAY')

        # UI : operators
        row = layout.row()
        row.operator("stored_views.save").index = -1

        # IO Operators
        if core.get_preferences():
            row = layout.row(align=True)
            row.operator("stored_views.import_from_scene", text="Import from Scene")
            row.operator("stored_views.import", text="", icon="IMPORT")
            row.operator("stored_views.export", text="", icon="EXPORT")

        data_store = core.DataStore()
        list = data_store.list
        # UI : items list
        if len(list) > 0:
            row = layout.row()
            box = row.box()
            # items list
            mode = stored_views.mode
            for i in range(len(list)):
                # associated icon
                icon_string = "MESH_CUBE"  # default icon
                # TODO: icons for view
                if mode == 'POV':
                    persp = list[i].perspective
                    if persp == 'PERSP':
                        icon_string = "MESH_CUBE"
                    elif persp == 'ORTHO':
                        icon_string = "MESH_PLANE"
                    elif persp == 'CAMERA':
                        if list[i].camera_type != 'CAMERA':
                            icon_string = 'OBJECT_DATAMODE'
                        else:
                            icon_string = "OUTLINER_DATA_CAMERA"
                if mode == 'LAYERS':
                    if list[i].lock_camera_and_layers is True:
                        icon_string = 'SCENE_DATA'
                    else:
                        icon_string = 'RENDERLAYERS'
                if mode == 'DISPLAY':
                    shade = list[i].viewport_shade
                    if shade == 'TEXTURED':
                        icon_string = 'TEXTURE_SHADED'
                    if shade == 'MATERIAL':
                        icon_string = 'MATERIAL_DATA'
                    elif shade == 'SOLID':
                        icon_string = 'SOLID'
                    elif shade == 'WIREFRAME':
                        icon_string = "WIRE"
                    elif shade == 'BOUNDBOX':
                        icon_string = 'BBOX'
                    elif shade == 'RENDERED':
                        icon_string = 'MATERIAL'
                # stored view row
                subrow = box.row(align=True)
                # current view indicator
                if data_store.current_index == i and context.scene.stored_views.view_modified is False:
                    subrow.label(text="", icon='SMALL_TRI_RIGHT_VEC')
                subrow.operator("stored_views.set",
                                text="", icon=icon_string).index = i
                subrow.prop(list[i], "name", text="")
                subrow.operator("stored_views.save",
                                text="", icon="REC").index = i
                subrow.operator("stored_views.delete",
                                text="", icon="PANEL_CLOSE").index = i

        layout = self.layout
        scene = context.scene
        layout.label("Camera Selector")
        cameras = sorted([o for o in scene.objects if o.type == 'CAMERA'],
                         key=lambda o: o.name)

        if len(cameras) > 0:
            for camera in cameras:
                row = layout.row(align=True)
                row.context_pointer_set("active_object", camera)
                row.operator("cameraselector.set_scene_camera",
                                   text=camera.name, icon='OUTLINER_DATA_CAMERA')
                row.operator("cameraselector.preview_scene_camera",
                                   text='', icon='RESTRICT_VIEW_OFF')
                row.operator("cameraselector.add_camera_marker",
                                   text='', icon='MARKER')
        else:
            layout.label("No cameras in this scene")
