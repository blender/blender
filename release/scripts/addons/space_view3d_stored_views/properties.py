# gpl authors: nfloyd, Francesco Siddi

from bpy.types import PropertyGroup
from bpy.props import (
        BoolProperty,
        BoolVectorProperty,
        CollectionProperty,
        FloatProperty,
        FloatVectorProperty,
        EnumProperty,
        IntProperty,
        IntVectorProperty,
        PointerProperty,
        StringProperty,
        )


class POVData(PropertyGroup):
    distance = FloatProperty()
    location = FloatVectorProperty(
            subtype='TRANSLATION'
            )
    rotation = FloatVectorProperty(
            subtype='QUATERNION',
            size=4
            )
    name = StringProperty()
    perspective = EnumProperty(
            items=[('PERSP', '', ''),
                   ('ORTHO', '', ''),
                   ('CAMERA', '', '')]
            )
    lens = FloatProperty()
    clip_start = FloatProperty()
    clip_end = FloatProperty()
    lock_cursor = BoolProperty()
    cursor_location = FloatVectorProperty()
    perspective_matrix_md5 = StringProperty()
    camera_name = StringProperty()
    camera_type = StringProperty()
    lock_object_name = StringProperty()


class LayersData(PropertyGroup):
    view_layers = BoolVectorProperty(size=20)
    scene_layers = BoolVectorProperty(size=20)
    lock_camera_and_layers = BoolProperty()
    name = StringProperty()


class DisplayData(PropertyGroup):
    name = StringProperty()
    viewport_shade = EnumProperty(
            items=[('BOUNDBOX', 'BOUNDBOX', 'BOUNDBOX'),
                   ('WIREFRAME', 'WIREFRAME', 'WIREFRAME'),
                   ('SOLID', 'SOLID', 'SOLID'),
                   ('TEXTURED', 'TEXTURED', 'TEXTURED'),
                   ('MATERIAL', 'MATERIAL', 'MATERIAL'),
                   ('RENDERED', 'RENDERED', 'RENDERED')]
            )
    show_only_render = BoolProperty()
    show_outline_selected = BoolProperty()
    show_all_objects_origin = BoolProperty()
    show_relationship_lines = BoolProperty()
    show_floor = BoolProperty()
    show_axis_x = BoolProperty()
    show_axis_y = BoolProperty()
    show_axis_z = BoolProperty()
    grid_lines = IntProperty()
    grid_scale = FloatProperty()
    grid_subdivisions = IntProperty()
    material_mode = StringProperty()
    show_textured_solid = BoolProperty()
    quad_view = BoolProperty()
    lock_rotation = BoolProperty()
    show_sync_view = BoolProperty()
    use_box_clip = BoolProperty()


class ViewData(PropertyGroup):
    pov = PointerProperty(
            type=POVData
            )
    layers = PointerProperty(
            type=LayersData
            )
    display = PointerProperty(
            type=DisplayData
            )
    name = StringProperty()


class StoredViewsData(PropertyGroup):
    pov_list = CollectionProperty(
            type=POVData
            )
    layers_list = CollectionProperty(
            type=LayersData
            )
    display_list = CollectionProperty(
            type=DisplayData
            )
    view_list = CollectionProperty(
            type=ViewData
            )
    mode = EnumProperty(
            name="Mode",
            items=[('VIEW', "View", "3D View settings"),
                   ('POV', "POV", "POV settings"),
                   ('LAYERS', "Layers", "Layers settings"),
                   ('DISPLAY', "Display", "Display settings")],
            default='VIEW'
            )
    current_indices = IntVectorProperty(
            size=4,
            default=[-1, -1, -1, -1]
            )
    view_modified = BoolProperty(
            default=False
            )
