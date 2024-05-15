# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

import bpy


class Properties(bpy.types.PropertyGroup):
    type = None

    @classmethod
    def register(cls):
        cls.type.hydra_storm = bpy.props.PointerProperty(
            name="Hydra Storm",
            description="Hydra Storm properties",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del cls.type.hydra_storm


class RenderProperties(bpy.types.PropertyGroup):
    max_lights: bpy.props.IntProperty(
        name="Max Lights",
        description="Limit maximum number of lights",
        default=16, min=0, max=16,
    )
    use_tiny_prim_culling: bpy.props.BoolProperty(
        name="Tiny Prim Culling",
        description="Hide small geometry primitives to improve performance",
        default=False,
    )
    volume_raymarching_step_size: bpy.props.FloatProperty(
        name="Volume Raymarching Step Size",
        description="Step size when raymarching volume",
        default=1.0,
    )
    volume_raymarching_step_size_lighting: bpy.props.FloatProperty(
        name="Volume Raymarching Step Size Lighting",
        description="Step size when raymarching volume for lighting computation",
        default=10.0,
    )
    volume_max_texture_memory_per_field: bpy.props.FloatProperty(
        name="Max Texture Memory Per Field",
        description="Maximum memory for a volume field texture in Mb (unless overridden by field prim)",
        default=128.0,
    )


class SceneProperties(Properties):
    type = bpy.types.Scene

    final: bpy.props.PointerProperty(type=RenderProperties)
    viewport: bpy.props.PointerProperty(type=RenderProperties)


register, unregister = bpy.utils.register_classes_factory((
    RenderProperties,
    SceneProperties,
))
