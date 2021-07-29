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


bl_info = {
    "name": "Light Field Tools",
    "author": "Aurel Wildfellner",
    "description": "Tools to create a light field camera and projector",
    "version": (0, 3, 1),
    "blender": (2, 64, 0),
    "location": "View3D > Tool Shelf > Light Field Tools",
    "url": "http://www.jku.at/cg/",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Render/Light_Field_Tools",
    "category": "Render"
}


if "bpy" in locals():
    import importlib
    importlib.reload(light_field_tools)
else:
    from . import light_field_tools


import bpy
from bpy.types import (
        AddonPreferences,
        PropertyGroup,
        )
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntProperty,
        StringProperty,
        PointerProperty,
        )


# global properties for the script, mainly for UI
class LightFieldPropertyGroup(PropertyGroup):
    angle = FloatProperty(
            name="Angle",
            # 40 degrees
            default=0.69813170079,
            min=0,
            # 172 degrees
            max=3.001966313430247,
            precision=2,
            subtype='ANGLE',
            description="Field of view of camera and angle of beam for spotlights"
            )
    row_length = IntProperty(
            name="Row Length",
            default=1,
            min=1,
            description="The number of cameras/lights in one row"
            )
    create_handler = BoolProperty(
            name="Handler",
            default=True,
            description="Creates an empty object, to which the cameras and spotlights are parented to"
            )
    do_camera = BoolProperty(
            name="Create Camera",
            default=True,
            description="A light field camera is created"
            )
    animate_camera = BoolProperty(
            name="Animate Camera",
            default=True,
            description="Animates a single camera, so not multiple cameras get created"
            )
    do_projection = BoolProperty(
            name="Create Projector",
            default=False,
            description="A light field projector is created"
            )
    texture_path = StringProperty(
            name="Texture Path",
            description="From this path textures for the spotlights will be loaded",
            subtype='DIR_PATH'
            )
    light_intensity = FloatProperty(
            name="Light Intensity",
            default=2,
            min=0,
            precision=3,
            description="Total intensity of all lamps"
            )
    # blending of the spotlights
    spot_blend = FloatProperty(
            name="Blend",
            default=0,
            min=0,
            max=1,
            precision=3,
            description="Blending of the spotlights"
            )
    # spacing in pixels on the focal plane
    spacing = IntProperty(
            name="Spacing",
            default=10,
            min=0,
            description="The spacing in pixels between two cameras on the focal plane"
            )


# Add-ons Preferences Update Panel

# Define Panel classes for updating
panels = (
        light_field_tools.VIEW3D_OT_lightfield_tools,
        )


def update_panel(self, context):
    message = "Light Field Tools: Updating Panel locations has failed"
    try:
        for panel in panels:
            if "bl_rna" in panel.__dict__:
                bpy.utils.unregister_class(panel)

        for panel in panels:
            panel.bl_category = context.user_preferences.addons[__name__].preferences.category
            bpy.utils.register_class(panel)

    except Exception as e:
        print("\n[{}]\n{}\n\nError:\n{}".format(__name__, message, e))
        pass


class LFTPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    category = StringProperty(
            name="Tab Category",
            description="Choose a name for the category of the panel",
            default="Tools",
            update=update_panel
            )

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        col = row.column()
        col.label(text="Tab Category:")
        col.prop(self, "category", text="")


def register():
    # register properties
    # bpy.utils.register_class(LightFieldPropertyGroup)
    bpy.utils.register_module(__name__)
    bpy.types.Scene.lightfield = PointerProperty(type=LightFieldPropertyGroup)


def unregister():
    bpy.utils.unregister_module(__name__)
    del bpy.types.Scene.lightfield


if __name__ == "__main__":
    register()
