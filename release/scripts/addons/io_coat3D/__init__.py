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
    "name": "3D-Coat Applink",
    "author": "Kalle-Samuli Riihikoski (haikalle)",
    "version": (3, 5, 22),
    "blender": (2, 59, 0),
    "location": "Scene > 3D-Coat Applink",
    "description": "Transfer data between 3D-Coat/Blender",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/3dcoat_applink",
    "category": "Import-Export",
}


if "bpy" in locals():
    import importlib
    importlib.reload(coat)
    importlib.reload(tex)
else:
    from . import coat
    from . import tex

import bpy
from bpy.types import PropertyGroup
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatVectorProperty,
        StringProperty,
        PointerProperty,
        )


def register():
    bpy.coat3D = dict()
    bpy.coat3D['active_coat'] = ''
    bpy.coat3D['status'] = 0
    bpy.coat3D['kuva'] = 1

    class ObjectCoat3D(PropertyGroup):
        objpath = StringProperty(
            name="Object_Path"
            )
        applink_name = StringProperty(
            name="Object_Applink_name"
            )
        coatpath = StringProperty(
            name="Coat_Path"
            )
        objectdir = StringProperty(
            name="ObjectPath",
            subtype="FILE_PATH"
            )
        objecttime = StringProperty(
            name="ObjectTime",
            subtype="FILE_PATH"
            )
        texturefolder = StringProperty(
            name="Texture folder:",
            subtype="DIR_PATH"
            )
        path3b = StringProperty(
            name="3B Path",
            subtype="FILE_PATH"
            )
        export_on = BoolProperty(
            name="Export_On",
            description="Add Modifiers and export",
            default=False
            )
        dime = FloatVectorProperty(
            name="dime",
            description="Dimension"
            )
        loc = FloatVectorProperty(
            name="Location",
            description="Location"
            )
        rot = FloatVectorProperty(
            name="Rotation",
            description="Rotation",
            subtype='EULER'
            )
        sca = FloatVectorProperty(
            name="Scale",
            description="Scale"
            )

    class SceneCoat3D(PropertyGroup):
        defaultfolder = StringProperty(
            name="FilePath",
            subtype="DIR_PATH",
            )
        cursor_loc = FloatVectorProperty(
            name="Cursor_loc",
            description="location"
            )
        exchangedir = StringProperty(
            name="FilePath",
            subtype="DIR_PATH"
            )
        exchangefolder = StringProperty(
            name="FilePath",
            subtype="DIR_PATH"
            )
        wasactive = StringProperty(
            name="Pass active object",
            )
        import_box = BoolProperty(
            name="Import window",
            description="Allows to skip import dialog",
            default=True
            )
        exchange_found = BoolProperty(
            name="Exchange Found",
            description="Alert if Exchange folder is not found",
            default=True
            )
        export_box = BoolProperty(
            name="Export window",
            description="Allows to skip export dialog",
            default=True
            )
        export_color = BoolProperty(
            name="Export color",
            description="Export color texture",
            default=True
            )
        export_spec = BoolProperty(
            name="Export specular",
            description="Export specular texture",
            default=True
            )
        export_normal = BoolProperty(
            name="Export Normal",
            description="Export normal texture",
            default=True
            )
        export_disp = BoolProperty(
            name="Export Displacement",
            description="Export displacement texture",
            default=True
            )
        export_position = BoolProperty(
            name="Export Source Position",
            description="Export source position",
            default=True
            )
        export_zero_layer = BoolProperty(
            name="Export from Layer 0",
            description="Export mesh from Layer 0",
            default=True
            )
        export_coarse = BoolProperty(
            name="Export Coarse",
            description="Export Coarse",
            default=True
            )
        exportfile = BoolProperty(
            name="No Import File",
            description="Add Modifiers and export",
            default=False
            )
        importmod = BoolProperty(
            name="Remove Modifiers",
            description="Import and add modifiers",
            default=False
            )
        exportmod = BoolProperty(
            name="Modifiers",
            description="Export modifiers",
            default=False
            )
        export_pos = BoolProperty(
            name="Remember Position",
            description="Remember position",
            default=True
            )
        importtextures = BoolProperty(
            name="Bring Textures",
            description="Import Textures",
            default=True
            )
        importlevel = BoolProperty(
            name="Multires. Level",
            description="Bring Specific Multires Level",
            default=False
            )
        exportover = BoolProperty(
            name="Export Obj",
            description="Import Textures",
            default=False
            )
        importmesh = BoolProperty(
            name="Mesh",
            description="Import Mesh",
            default=True
            )

        # copy location
        cursor = FloatVectorProperty(
            name="Cursor",
            description="Location",
            subtype="XYZ",
            default=(0.0, 0.0, 0.0)
            )
        loca = FloatVectorProperty(
            name="location",
            description="Location",
            subtype="XYZ",
            default=(0.0, 0.0, 0.0)
            )
        rota = FloatVectorProperty(
            name="location",
            description="Location",
            subtype="EULER",
            default=(0.0, 0.0, 0.0)
            )
        scal = FloatVectorProperty(
            name="location",
            description="Location",
            subtype="XYZ",
            default=(0.0, 0.0, 0.0)
            )
        dime = FloatVectorProperty(
            name="dimension",
            description="Dimension",
            subtype="XYZ",
            default=(0.0, 0.0, 0.0)
            )
        type = EnumProperty(
            name="Export Type",
            description="Different Export Types",
            items=(("ppp", "Per-Pixel Painting", ""),
                   ("mv", "Microvertex Painting", ""),
                   ("ptex", "Ptex Painting", ""),
                   ("uv", "UV-Mapping", ""),
                   ("ref", "Reference Mesh", ""),
                   ("retopo", "Retopo mesh as new layer", ""),
                   ("vox", "Mesh As Voxel Object", ""),
                   ("alpha", "Mesh As New Pen Alpha", ""),
                   ("prim", "Mesh As Voxel Primitive", ""),
                   ("curv", "Mesh As a Curve Profile", ""),
                   ("autopo", "Mesh For Auto-retopology", ""),
                   ),
            default="ppp"
            )

    bpy.utils.register_module(__name__)

    bpy.types.Object.coat3D = PointerProperty(
        name="Applink Variables",
        type=ObjectCoat3D,
        description="Applink variables"
        )
    bpy.types.Scene.coat3D = PointerProperty(
        name="Applink Variables",
        type=SceneCoat3D,
        description="Applink variables"
        )


def unregister():
    import bpy

    del bpy.types.Object.coat3D
    del bpy.types.Scene.coat3D
    del bpy.coat3D

    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
