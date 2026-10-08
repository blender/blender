# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from collections import namedtuple
from bpy.types import ShaderNodeMath, ShaderNodeMix


#################
# rl_outputs:
# list of outputs of Input Render Layer
# with attributes determining if pass is used,
# and MultiLayer EXR outputs names and corresponding render engines
#
# rl_outputs entry = (render_pass, rl_output_name, exr_output_name, in_eevee, in_cycles)
RL_entry = namedtuple('RL_Entry', ['render_pass', 'output_name', 'exr_output_name', 'in_eevee', 'in_cycles'])
rl_outputs = (
    RL_entry('use_pass_ambient_occlusion', 'Ambient Occlusion', 'Ambient Occlusion', True, True),
    RL_entry('use_pass_combined', 'Image', 'Combined', True, True),
    RL_entry('use_pass_diffuse_color', 'Diffuse Color', 'Diffuse Color', False, True),
    RL_entry('use_pass_diffuse_direct', 'Diffuse Direct', 'Diffuse Direct', False, True),
    RL_entry('use_pass_diffuse_indirect', 'Diffuse Indirect', 'Diffuse Indirect', False, True),
    RL_entry('use_pass_emit', 'Emission', 'Emission', False, True),
    RL_entry('use_pass_environment', 'Environment', 'Environment', False, False),
    RL_entry('use_pass_glossy_color', 'Glossy Color', 'Glossy Color', False, True),
    RL_entry('use_pass_glossy_direct', 'Glossy Direct', 'Glossy Direct', False, True),
    RL_entry('use_pass_glossy_indirect', 'Glossy Indirect', 'Glossy Indirect', False, True),
    RL_entry('use_pass_indirect', 'Indirect', 'Indirect', False, False),
    RL_entry('use_pass_material_index', 'Material Index', 'Material Index', False, True),
    RL_entry('use_pass_mist', 'Mist', 'Mist', True, True),
    RL_entry('use_pass_normal', 'Normal', 'Normal', True, True),
    RL_entry('use_pass_object_index', 'Object Index', 'Object Index', False, True),
    RL_entry('use_pass_shadow', 'Shadow', 'Shadow', False, True),
    RL_entry('use_pass_subsurface_color', 'Subsurface Color', 'Subsurface Color', True, True),
    RL_entry('use_pass_subsurface_direct', 'Subsurface Direct', 'Subsurface Direct', True, True),
    RL_entry('use_pass_subsurface_indirect', 'Subsurface Indirect', 'Subsurface Indirect', False, True),
    RL_entry('use_pass_transmission_color', 'Transmission Color', 'Transmission Color', False, True),
    RL_entry('use_pass_transmission_direct', 'Transmission Direct', 'Transmission Direct', False, True),
    RL_entry('use_pass_transmission_indirect', 'Transmission Indirect', 'Transmission Indirect', False, True),
    RL_entry('use_pass_uv', 'UV', 'UV', True, True),
    RL_entry('use_pass_vector', 'Speed', 'Vector', False, True),
    RL_entry('use_pass_z', 'Z', 'Depth', True, True),
)

# list of blend types of "Mix" nodes in a form that can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
blend_types = [(enum.identifier, enum.name, enum.description)
               for enum in ShaderNodeMix.bl_rna.properties['blend_type'].enum_items_static]

# list of operations of "Math" nodes in a form that can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
operations = [(enum.identifier, enum.name, enum.description)
              for enum in ShaderNodeMath.bl_rna.properties['operation'].enum_items_static]

# Operations used by the geometry boolean node and join geometry node
geo_combine_operations = [
    ('JOIN', 'Join Geometry', 'Join Geometry Mode'),
    ('INSTANCES', 'Geometry to Instance', 'Geometry to Instance Mode'),
    ('INTERSECT', 'Intersect', 'Intersect Mode'),
    ('UNION', 'Union', 'Union Mode'),
    ('DIFFERENCE', 'Difference', 'Difference Mode'),
]

# in NWBatchChangeNodes additional types/operations. Can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
navs = [
    ('CURRENT', 'Current', 'Leave at current state'),
    ('NEXT', 'Next', 'Next blend type/operation'),
    ('PREV', 'Prev', 'Previous blend type/operation'),
]

draw_color_sets = {
    "red_white": (
        (1.0, 1.0, 1.0, 0.7),
        (1.0, 0.0, 0.0, 0.7),
        (0.8, 0.2, 0.2, 1.0)
    ),
    "green": (
        (0.0, 0.0, 0.0, 1.0),
        (0.38, 0.77, 0.38, 1.0),
        (0.38, 0.77, 0.38, 1.0)
    ),
    "yellow": (
        (0.0, 0.0, 0.0, 1.0),
        (0.77, 0.77, 0.16, 1.0),
        (0.77, 0.77, 0.16, 1.0)
    ),
    "purple": (
        (0.0, 0.0, 0.0, 1.0),
        (0.38, 0.38, 0.77, 1.0),
        (0.38, 0.38, 0.77, 1.0)
    ),
    "grey": (
        (0.0, 0.0, 0.0, 1.0),
        (0.63, 0.63, 0.63, 1.0),
        (0.63, 0.63, 0.63, 1.0)
    ),
    "black": (
        (1.0, 1.0, 1.0, 0.7),
        (0.0, 0.0, 0.0, 0.7),
        (0.2, 0.2, 0.2, 1.0)
    )
}


def get_texture_node_types():
    return [
        "ShaderNodeTexBrick",
        "ShaderNodeTexChecker",
        "ShaderNodeTexEnvironment",
        "ShaderNodeTexGabor",
        "ShaderNodeTexGradient",
        "ShaderNodeTexIES",
        "ShaderNodeTexImage",
        "ShaderNodeTexMagic",
        "ShaderNodeTexMusgrave",
        "ShaderNodeTexNoise",
        "ShaderNodeTexSky",
        "ShaderNodeTexVoronoi",
        "ShaderNodeTexWave",
        "ShaderNodeTexWhiteNoise"
    ]


def nice_hotkey_name(punc):
    # convert the ugly string name into the actual character
    from bpy.types import KeyMapItem
    nice_name = {
        enum_item.identifier:
        enum_item.name
        for enum_item in KeyMapItem.bl_rna.properties['type'].enum_items
    }
    try:
        return nice_name[punc]
    except KeyError:
        return punc.replace("_", " ").title()
