#
# Copyright 2011, Blender Foundation.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

import bpy
from bpy.props import *

from cycles import enums

class CyclesRenderSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Scene.cycles = PointerProperty(type=cls, name="Cycles Render Settings", description="Cycles Render Settings")

        cls.device = EnumProperty(name="Device", description="Device to use for rendering",
            items=enums.devices, default="CPU")

        cls.shading_system = EnumProperty(name="Shading System", description="Shading system to use for rendering",
            items=enums.shading_systems, default="GPU_COMPATIBLE")

        cls.passes = IntProperty(name="Passes", description="Number of passes to render",
            default=10, min=1, max=2147483647)
        cls.preview_passes = IntProperty(name="Preview Passes", description="Number of passes to render in the viewport, unlimited if 0",
            default=0, min=0, max=2147483647)
        cls.preview_pause = BoolProperty(name="Pause Preview", description="Pause all viewport preview renders",
            default=False)

        cls.min_bounces = IntProperty(name="Min Bounces", description="Minimum number of bounces",
            default=3, min=0, max=1024)
        cls.max_bounces = IntProperty(name="Max Bounces", description="Maximum number of bounces",
            default=8, min=0, max=1024)
        cls.no_caustics = BoolProperty(name="No Caustics", description="Leave out caustics, resulting in a darker image with less noise",
            default=False)
        cls.blur_caustics = FloatProperty(name="Blur Caustics", description="Blur caustics to reduce noise",
            default=0.0, min=0.0, max=1.0)

        cls.exposure = FloatProperty(name="Exposure", description="Image brightness scale",
            default=1.0, min=0.0, max=10.0)
        cls.transparent = BoolProperty(name="Transparent", description="World background is transparent",
            default=False)

        cls.filter_type = EnumProperty(name="Filter Type", description="Pixel filter type",
            items=enums.filter_types, default="GAUSSIAN")
        cls.filter_width = FloatProperty(name="Filter Width", description="Pixel filter width",
            default=1.5, min=0.01, max=10.0)

        cls.debug_tile_size = IntProperty(name="Tile Size", description="",
            default=1024, min=1, max=4096)
        cls.debug_min_size = IntProperty(name="Min Size", description="",
            default=64, min=1, max=4096)
        cls.debug_reset_timeout = FloatProperty(name="Reset timeout", description="",
            default=0.1, min=0.01, max=10.0)
        cls.debug_cancel_timeout = FloatProperty(name="Cancel timeout", description="",
            default=0.1, min=0.01, max=10.0)
        cls.debug_text_timeout = FloatProperty(name="Text timeout", description="",
            default=1.0, min=0.01, max=10.0)

        cls.debug_bvh_type = EnumProperty(name="BVH Type", description="Choose between faster updates, or faster render",
            items=enums.bvh_types, default="DYNAMIC_BVH")
        cls.debug_use_spatial_splits = BoolProperty(name="Use Spatial Splits", description="Use BVH spatial splits: longer builder time, faster render",
            default=False)

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles

class CyclesCameraSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Camera.cycles = PointerProperty(type=cls, name="Cycles Camera Settings", description="Cycles Camera Settings")

        cls.lens_radius = FloatProperty(name="Lens radius", description="Lens radius for depth of field",
            default=0.0, min=0.0, max=10.0)
    
    @classmethod
    def unregister(cls):
        del bpy.types.Camera.cycles

class CyclesMaterialSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Material.cycles = PointerProperty(type=cls, name="Cycles Material Settings", description="Cycles Material Settings")

    @classmethod
    def unregister(cls):
        del bpy.types.Material.cycles

class CyclesWorldSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.World.cycles = PointerProperty(type=cls, name="Cycles World Settings", description="Cycles World Settings")

    @classmethod
    def unregister(cls):
        del bpy.types.World.cycles

class CyclesMeshSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Mesh.cycles = PointerProperty(type=cls, name="Cycles Mesh Settings", description="Cycles Mesh Settings")
        bpy.types.Curve.cycles = PointerProperty(type=cls, name="Cycles Mesh Settings", description="Cycles Mesh Settings")
        bpy.types.MetaBall.cycles = PointerProperty(type=cls, name="Cycles Mesh Settings", description="Cycles Mesh Settings")

        cls.displacement_method = EnumProperty(name="Displacement Method", description="Method to use for the displacement",
            items=enums.displacement_methods, default="BUMP")
        cls.use_subdivision = BoolProperty(name="Use Subdivision", description="Subdivide mesh for rendering",
            default=False)
        cls.dicing_rate = FloatProperty(name="Dicing Rate", description="", default=1.0, min=0.001, max=1000.0)

    @classmethod
    def unregister(cls):
        del bpy.types.Mesh.cycles

def register():
    bpy.utils.register_class(CyclesRenderSettings)
    bpy.utils.register_class(CyclesCameraSettings)
    bpy.utils.register_class(CyclesMaterialSettings)
    bpy.utils.register_class(CyclesWorldSettings)
    bpy.utils.register_class(CyclesMeshSettings)
    
def unregister():
    bpy.utils.unregister_class(CyclesRenderSettings)
    bpy.utils.unregister_class(CyclesCameraSettings)
    bpy.utils.unregister_class(CyclesMaterialSettings)
    bpy.utils.unregister_class(CyclesWorldSettings)
    bpy.utils.unregister_class(CyclesMeshSettings)

