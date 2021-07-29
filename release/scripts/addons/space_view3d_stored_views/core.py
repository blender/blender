# gpl authors: nfloyd, Francesco Siddi


import logging
module_logger = logging.getLogger(__name__)

import hashlib
import bpy


# Utility function get preferences setting for exporters
def get_preferences():
    # replace the key if the add-on name changes
    addon = bpy.context.user_preferences.addons[__package__]
    show_warn = (addon.preferences.show_exporters if addon else False)

    return show_warn


class StoredView():
    def __init__(self, mode, index=None):
        self.logger = logging.getLogger('%s.StoredView' % __name__)
        self.scene = bpy.context.scene
        self.view3d = bpy.context.space_data
        self.index = index
        self.data_store = DataStore(mode=mode)

    def save(self):
        if self.index == -1:
            stored_view, self.index = self.data_store.create()
        else:
            stored_view = self.data_store.get(self.index)
        self.from_v3d(stored_view)
        self.logger.debug('index: %s name: %s' % (self.data_store.current_index, stored_view.name))

    def set(self):
        stored_view = self.data_store.get(self.index)
        self.update_v3d(stored_view)
        self.logger.debug('index: %s name: %s' % (self.data_store.current_index, stored_view.name))

    def from_v3d(self, stored_view):
        raise NotImplementedError("Subclass must implement abstract method")

    def update_v3d(self, stored_view):
        raise NotImplementedError("Subclass must implement abstract method")

    @staticmethod
    def is_modified(context, stored_view):
        raise NotImplementedError("Subclass must implement abstract method")


class POV(StoredView):
    def __init__(self, index=None):
        super().__init__(mode='POV', index=index)
        self.logger = logging.getLogger('%s.POV' % __name__)

    def from_v3d(self, stored_view):
        view3d = self.view3d
        region3d = view3d.region_3d

        stored_view.distance = region3d.view_distance
        stored_view.location = region3d.view_location
        stored_view.rotation = region3d.view_rotation
        stored_view.perspective_matrix_md5 = POV._get_perspective_matrix_md5(region3d)
        stored_view.perspective = region3d.view_perspective
        stored_view.lens = view3d.lens
        stored_view.clip_start = view3d.clip_start
        stored_view.clip_end = view3d.clip_end

        if region3d.view_perspective == 'CAMERA':
            stored_view.camera_type = view3d.camera.type  # type : 'CAMERA' or 'MESH'
            stored_view.camera_name = view3d.camera.name  # store string instead of object
        if view3d.lock_object is not None:
            stored_view.lock_object_name = view3d.lock_object.name  # idem
        else:
            stored_view.lock_object_name = ""
        stored_view.lock_cursor = view3d.lock_cursor
        stored_view.cursor_location = view3d.cursor_location

    def update_v3d(self, stored_view):
        view3d = self.view3d
        region3d = view3d.region_3d
        region3d.view_distance = stored_view.distance
        region3d.view_location = stored_view.location
        region3d.view_rotation = stored_view.rotation
        region3d.view_perspective = stored_view.perspective
        view3d.lens = stored_view.lens
        view3d.clip_start = stored_view.clip_start
        view3d.clip_end = stored_view.clip_end
        view3d.lock_cursor = stored_view.lock_cursor
        if stored_view.lock_cursor is True:
            # update cursor only if view is locked to cursor
            view3d.cursor_location = stored_view.cursor_location

        if stored_view.perspective == "CAMERA":

            lock_obj = self._get_object(stored_view.lock_object_name)
            if lock_obj:
                view3d.lock_object = lock_obj
            else:
                cam = self._get_object(stored_view.camera_name)
                if cam:
                    view3d.camera = cam

    @staticmethod
    def _get_object(name, pointer=None):
        return bpy.data.objects.get(name)

    @staticmethod
    def is_modified(context, stored_view):
        # TODO: check for others param, currently only perspective
        # and perspective_matrix are checked
        POV.logger = logging.getLogger('%s.POV' % __name__)
        view3d = context.space_data
        region3d = view3d.region_3d
        if region3d.view_perspective != stored_view.perspective:
            POV.logger.debug('view_perspective')
            return True

        md5 = POV._get_perspective_matrix_md5(region3d)
        if (md5 != stored_view.perspective_matrix_md5 and
          region3d.view_perspective != "CAMERA"):
            POV.logger.debug('perspective_matrix')
            return True

        return False

    @staticmethod
    def _get_perspective_matrix_md5(region3d):
        md5 = hashlib.md5(str(region3d.perspective_matrix).encode('utf-8')).hexdigest()
        return md5


class Layers(StoredView):
    def __init__(self, index=None):
        super().__init__(mode='LAYERS', index=index)
        self.logger = logging.getLogger('%s.Layers' % __name__)

    def from_v3d(self, stored_view):
        view3d = self.view3d
        stored_view.view_layers = view3d.layers
        stored_view.scene_layers = self.scene.layers
        stored_view.lock_camera_and_layers = view3d.lock_camera_and_layers

    def update_v3d(self, stored_view):
        view3d = self.view3d
        view3d.lock_camera_and_layers = stored_view.lock_camera_and_layers
        if stored_view.lock_camera_and_layers is True:
            self.scene.layers = stored_view.scene_layers
        else:
            view3d.layers = stored_view.view_layers

    @staticmethod
    def is_modified(context, stored_view):
        Layers.logger = logging.getLogger('%s.Layers' % __name__)
        if stored_view.lock_camera_and_layers != context.space_data.lock_camera_and_layers:
            Layers.logger.debug('lock_camera_and_layers')
            return True
        if stored_view.lock_camera_and_layers is True:
            for i in range(20):
                if stored_view.scene_layers[i] != context.scene.layers[i]:
                    Layers.logger.debug('scene_layers[%s]' % (i, ))
                    return True
        else:
            for i in range(20):
                if stored_view.view_layers[i] != context.space_data.view3d.layers[i]:
                    return True
        return False


class Display(StoredView):
    def __init__(self, index=None):
        super().__init__(mode='DISPLAY', index=index)
        self.logger = logging.getLogger('%s.Display' % __name__)

    def from_v3d(self, stored_view):
        view3d = self.view3d
        stored_view.viewport_shade = view3d.viewport_shade
        stored_view.show_only_render = view3d.show_only_render
        stored_view.show_outline_selected = view3d.show_outline_selected
        stored_view.show_all_objects_origin = view3d.show_all_objects_origin
        stored_view.show_relationship_lines = view3d.show_relationship_lines
        stored_view.show_floor = view3d.show_floor
        stored_view.show_axis_x = view3d.show_axis_x
        stored_view.show_axis_y = view3d.show_axis_y
        stored_view.show_axis_z = view3d.show_axis_z
        stored_view.grid_lines = view3d.grid_lines
        stored_view.grid_scale = view3d.grid_scale
        stored_view.grid_subdivisions = view3d.grid_subdivisions
        stored_view.material_mode = self.scene.game_settings.material_mode
        stored_view.show_textured_solid = view3d.show_textured_solid

    def update_v3d(self, stored_view):
        view3d = self.view3d
        view3d.viewport_shade = stored_view.viewport_shade
        view3d.show_only_render = stored_view.show_only_render
        view3d.show_outline_selected = stored_view.show_outline_selected
        view3d.show_all_objects_origin = stored_view.show_all_objects_origin
        view3d.show_relationship_lines = stored_view.show_relationship_lines
        view3d.show_floor = stored_view.show_floor
        view3d.show_axis_x = stored_view.show_axis_x
        view3d.show_axis_y = stored_view.show_axis_y
        view3d.show_axis_z = stored_view.show_axis_z
        view3d.grid_lines = stored_view.grid_lines
        view3d.grid_scale = stored_view.grid_scale
        view3d.grid_subdivisions = stored_view.grid_subdivisions
        self.scene.game_settings.material_mode = stored_view.material_mode
        view3d.show_textured_solid = stored_view.show_textured_solid

    @staticmethod
    def is_modified(context, stored_view):
        Display.logger = logging.getLogger('%s.Display' % __name__)
        view3d = context.space_data
        excludes = ["material_mode", "quad_view", "lock_rotation", "show_sync_view", "use_box_clip", "name"]
        for k, v in stored_view.items():
            if k not in excludes:
                if getattr(view3d, k) != getattr(stored_view, k):
                    return True

        if stored_view.material_mode != context.scene.game_settings.material_mode:
            Display.logger.debug('material_mode')
            return True


class View(StoredView):
    def __init__(self, index=None):
        super().__init__(mode='VIEW', index=index)
        self.logger = logging.getLogger('%s.View' % __name__)
        self.pov = POV()
        self.layers = Layers()
        self.display = Display()

    def from_v3d(self, stored_view):
        self.pov.from_v3d(stored_view.pov)
        self.layers.from_v3d(stored_view.layers)
        self.display.from_v3d(stored_view.display)

    def update_v3d(self, stored_view):
        self.pov.update_v3d(stored_view.pov)
        self.layers.update_v3d(stored_view.layers)
        self.display.update_v3d(stored_view.display)

    @staticmethod
    def is_modified(context, stored_view):
        if POV.is_modified(context, stored_view.pov) or \
           Layers.is_modified(context, stored_view.layers) or \
           Display.is_modified(context, stored_view.display):
            return True
        return False


class DataStore():
    def __init__(self, scene=None, mode=None):
        if scene is None:
            scene = bpy.context.scene
        stored_views = scene.stored_views
        self.mode = mode

        if mode is None:
            self.mode = stored_views.mode

        if self.mode == 'VIEW':
            self.list = stored_views.view_list
            self.current_index = stored_views.current_indices[0]
        elif self.mode == 'POV':
            self.list = stored_views.pov_list
            self.current_index = stored_views.current_indices[1]
        elif self.mode == 'LAYERS':
            self.list = stored_views.layers_list
            self.current_index = stored_views.current_indices[2]
        elif self.mode == 'DISPLAY':
            self.list = stored_views.display_list
            self.current_index = stored_views.current_indices[3]

    def create(self):
        item = self.list.add()
        item.name = self._generate_name()
        index = len(self.list) - 1
        self._set_current_index(index)
        return item, index

    def get(self, index):
        self._set_current_index(index)
        return self.list[index]

    def delete(self, index):
        if self.current_index > index:
            self._set_current_index(self.current_index - 1)
        elif self.current_index == index:
            self._set_current_index(-1)

        self.list.remove(index)

    def _set_current_index(self, index):
        self.current_index = index
        mode = self.mode
        stored_views = bpy.context.scene.stored_views
        if mode == 'VIEW':
            stored_views.current_indices[0] = index
        elif mode == 'POV':
            stored_views.current_indices[1] = index
        elif mode == 'LAYERS':
            stored_views.current_indices[2] = index
        elif mode == 'DISPLAY':
            stored_views.current_indices[3] = index

    def _generate_name(self):
        default_name = str(self.mode)
        names = []
        for i in self.list:
            i_name = i.name
            if i_name.startswith(default_name):
                names.append(i_name)
        names.sort()
        try:
            l_name = names[-1]
            post_fix = l_name.rpartition('.')[2]
            if post_fix.isnumeric():
                post_fix = str(int(post_fix) + 1).zfill(3)
            else:
                if post_fix == default_name:
                    post_fix = "001"
            return default_name + "." + post_fix
        except:
            return default_name

    @staticmethod
    def sanitize_data(scene):

        def check_objects_references(mode, list):
            to_remove = []
            for i, list_item in enumerate(list.items()):
                key, item = list_item
                if mode == 'POV' or mode == 'VIEWS':
                    if mode == 'VIEWS':
                        item = item.pov

                    if item.perspective == "CAMERA":

                        camera = bpy.data.objects.get(item.camera_name)
                        if camera is None:
                            try:  # pick a default camera TODO: ask to pick?
                                camera = bpy.data.cameras[0]
                                item.camera_name = camera.name
                            except:  # couldn't find a camera in the scene
                                pass

                        obj = bpy.data.objects.get(item.lock_object_name)
                        if obj is None and camera is None:
                            to_remove.append(i)

            for i in reversed(to_remove):
                list.remove(i)

        modes = ['POV', 'VIEW', 'DISPLAY', 'LAYERS']
        for mode in modes:
            data = DataStore(scene=scene, mode=mode)
            check_objects_references(mode, data.list)


def stored_view_factory(mode, *args, **kwargs):
    if mode == 'POV':
        return POV(*args, **kwargs)
    elif mode == 'LAYERS':
        return Layers(*args, **kwargs)
    elif mode == 'DISPLAY':
        return Display(*args, **kwargs)
    elif mode == 'VIEW':
        return View(*args, **kwargs)
