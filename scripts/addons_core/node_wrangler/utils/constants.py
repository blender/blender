# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from collections import namedtuple


#################
# rl_outputs:
# list of outputs of Input Render Layer
# with attributes determining if pass is used,
# and MultiLayer EXR outputs names and corresponding render engines
#
# rl_outputs entry = (render_pass, rl_output_name, exr_output_name, in_eevee, in_cycles)
RL_entry = namedtuple('RL_Entry', ['render_pass', 'output_name', 'exr_output_name', 'in_eevee', 'in_cycles'])
rl_outputs = (
    RL_entry('use_pass_ambient_occlusion', 'AO', 'AO', True, True),
    RL_entry('use_pass_combined', 'Image', 'Combined', True, True),
    RL_entry('use_pass_diffuse_color', 'Diffuse Color', 'DiffCol', False, True),
    RL_entry('use_pass_diffuse_direct', 'Diffuse Direct', 'DiffDir', False, True),
    RL_entry('use_pass_diffuse_indirect', 'Diffuse Indirect', 'DiffInd', False, True),
    RL_entry('use_pass_emit', 'Emit', 'Emit', False, True),
    RL_entry('use_pass_environment', 'Environment', 'Env', False, False),
    RL_entry('use_pass_glossy_color', 'Glossy Color', 'GlossCol', False, True),
    RL_entry('use_pass_glossy_direct', 'Glossy Direct', 'GlossDir', False, True),
    RL_entry('use_pass_glossy_indirect', 'Glossy Indirect', 'GlossInd', False, True),
    RL_entry('use_pass_indirect', 'Indirect', 'Indirect', False, False),
    RL_entry('use_pass_material_index', 'IndexMA', 'IndexMA', False, True),
    RL_entry('use_pass_mist', 'Mist', 'Mist', True, True),
    RL_entry('use_pass_normal', 'Normal', 'Normal', True, True),
    RL_entry('use_pass_object_index', 'IndexOB', 'IndexOB', False, True),
    RL_entry('use_pass_shadow', 'Shadow', 'Shadow', False, True),
    RL_entry('use_pass_subsurface_color', 'Subsurface Color', 'SubsurfaceCol', True, True),
    RL_entry('use_pass_subsurface_direct', 'Subsurface Direct', 'SubsurfaceDir', True, True),
    RL_entry('use_pass_subsurface_indirect', 'Subsurface Indirect', 'SubsurfaceInd', False, True),
    RL_entry('use_pass_transmission_color', 'Transmission Color', 'TransCol', False, True),
    RL_entry('use_pass_transmission_direct', 'Transmission Direct', 'TransDir', False, True),
    RL_entry('use_pass_transmission_indirect', 'Transmission Indirect', 'TransInd', False, True),
    RL_entry('use_pass_uv', 'UV', 'UV', True, True),
    RL_entry('use_pass_vector', 'Speed', 'Vector', False, True),
    RL_entry('use_pass_z', 'Z', 'Depth', True, True),
)

# list of blend types of "Mix" nodes in a form that can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
blend_types = [
    ('MIX', 'Mix', 'Mix Mode'),
    ('ADD', 'Add', 'Add Mode'),
    ('MULTIPLY', 'Multiply', 'Multiply Mode'),
    ('SUBTRACT', 'Subtract', 'Subtract Mode'),
    ('SCREEN', 'Screen', 'Screen Mode'),
    ('DIVIDE', 'Divide', 'Divide Mode'),
    ('DIFFERENCE', 'Difference', 'Difference Mode'),
    ('DARKEN', 'Darken', 'Darken Mode'),
    ('LIGHTEN', 'Lighten', 'Lighten Mode'),
    ('OVERLAY', 'Overlay', 'Overlay Mode'),
    ('DODGE', 'Dodge', 'Dodge Mode'),
    ('BURN', 'Burn', 'Burn Mode'),
    ('HUE', 'Hue', 'Hue Mode'),
    ('SATURATION', 'Saturation', 'Saturation Mode'),
    ('VALUE', 'Value', 'Value Mode'),
    ('COLOR', 'Color', 'Color Mode'),
    ('SOFT_LIGHT', 'Soft Light', 'Soft Light Mode'),
    ('LINEAR_LIGHT', 'Linear Light', 'Linear Light Mode'),
]

# list of operations of "Math" nodes in a form that can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
operations = [
    ('ADD', 'Add', 'Add Mode'),
    ('SUBTRACT', 'Subtract', 'Subtract Mode'),
    ('MULTIPLY', 'Multiply', 'Multiply Mode'),
    ('DIVIDE', 'Divide', 'Divide Mode'),
    ('MULTIPLY_ADD', 'Multiply Add', 'Multiply Add Mode'),
    ('SINE', 'Sine', 'Sine Mode'),
    ('COSINE', 'Cosine', 'Cosine Mode'),
    ('TANGENT', 'Tangent', 'Tangent Mode'),
    ('ARCSINE', 'Arcsine', 'Arcsine Mode'),
    ('ARCCOSINE', 'Arccosine', 'Arccosine Mode'),
    ('ARCTANGENT', 'Arctangent', 'Arctangent Mode'),
    ('ARCTAN2', 'Arctan2', 'Arctan2 Mode'),
    ('SINH', 'Hyperbolic Sine', 'Hyperbolic Sine Mode'),
    ('COSH', 'Hyperbolic Cosine', 'Hyperbolic Cosine Mode'),
    ('TANH', 'Hyperbolic Tangent', 'Hyperbolic Tangent Mode'),
    ('POWER', 'Power', 'Power Mode'),
    ('LOGARITHM', 'Logarithm', 'Logarithm Mode'),
    ('SQRT', 'Square Root', 'Square Root Mode'),
    ('INVERSE_SQRT', 'Inverse Square Root', 'Inverse Square Root Mode'),
    ('EXPONENT', 'Exponent', 'Exponent Mode'),
    ('MINIMUM', 'Minimum', 'Minimum Mode'),
    ('MAXIMUM', 'Maximum', 'Maximum Mode'),
    ('LESS_THAN', 'Less Than', 'Less Than Mode'),
    ('GREATER_THAN', 'Greater Than', 'Greater Than Mode'),
    ('SIGN', 'Sign', 'Sign Mode'),
    ('COMPARE', 'Compare', 'Compare Mode'),
    ('SMOOTH_MIN', 'Smooth Minimum', 'Smooth Minimum Mode'),
    ('SMOOTH_MAX', 'Smooth Maximum', 'Smooth Maximum Mode'),
    ('FRACT', 'Fraction', 'Fraction Mode'),
    ('MODULO', 'Modulo', 'Modulo Mode'),
    ('SNAP', 'Snap', 'Snap Mode'),
    ('WRAP', 'Wrap', 'Wrap Mode'),
    ('PINGPONG', 'Pingpong', 'Pingpong Mode'),
    ('ABSOLUTE', 'Absolute', 'Absolute Mode'),
    ('ROUND', 'Round', 'Round Mode'),
    ('FLOOR', 'Floor', 'Floor Mode'),
    ('CEIL', 'Ceil', 'Ceil Mode'),
    ('TRUNCATE', 'Truncate', 'Truncate Mode'),
    ('RADIANS', 'To Radians', 'To Radians Mode'),
    ('DEGREES', 'To Degrees', 'To Degrees Mode'),
]

# Operations used by the geometry boolean node and join geometry node
geo_combine_operations = [
    ('JOIN', 'Join Geometry', 'Join Geometry Mode'),
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
        "ShaderNodeTexGradient",
        "ShaderNodeTexIES",
        "ShaderNodeTexImage",
        "ShaderNodeTexMagic",
        "ShaderNodeTexMusgrave",
        "ShaderNodeTexNoise",
        "ShaderNodeTexPointDensity",
        "ShaderNodeTexSky",
        "ShaderNodeTexVoronoi",
        "ShaderNodeTexWave",
        "ShaderNodeTexWhiteNoise"
    ]


def nice_hotkey_name(punc):
    # convert the ugly string name into the actual character
    nice_name = {
        'LEFTMOUSE': "LMB",
        'MIDDLEMOUSE': "MMB",
        'RIGHTMOUSE': "RMB",
        'WHEELUPMOUSE': "Wheel Up",
        'WHEELDOWNMOUSE': "Wheel Down",
        'WHEELINMOUSE': "Wheel In",
        'WHEELOUTMOUSE': "Wheel Out",
        'ZERO': "0",
        'ONE': "1",
        'TWO': "2",
        'THREE': "3",
        'FOUR': "4",
        'FIVE': "5",
        'SIX': "6",
        'SEVEN': "7",
        'EIGHT': "8",
        'NINE': "9",
        'OSKEY': "Super",
        'RET': "Enter",
        'LINE_FEED': "Enter",
        'SEMI_COLON': ";",
        'PERIOD': ".",
        'COMMA': ",",
        'QUOTE': '"',
        'MINUS': "-",
        'SLASH': "/",
        'BACK_SLASH': "\\",
        'EQUAL': "=",
        'NUMPAD_1': "Numpad 1",
        'NUMPAD_2': "Numpad 2",
        'NUMPAD_3': "Numpad 3",
        'NUMPAD_4': "Numpad 4",
        'NUMPAD_5': "Numpad 5",
        'NUMPAD_6': "Numpad 6",
        'NUMPAD_7': "Numpad 7",
        'NUMPAD_8': "Numpad 8",
        'NUMPAD_9': "Numpad 9",
        'NUMPAD_0': "Numpad 0",
        'NUMPAD_PERIOD': "Numpad .",
        'NUMPAD_SLASH': "Numpad /",
        'NUMPAD_ASTERIX': "Numpad *",
        'NUMPAD_MINUS': "Numpad -",
        'NUMPAD_ENTER': "Numpad Enter",
        'NUMPAD_PLUS': "Numpad +",
    }
    try:
        return nice_name[punc]
    except KeyError:
        return punc.replace("_", " ").title()
