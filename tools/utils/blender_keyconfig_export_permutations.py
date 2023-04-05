#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later


r"""
This script exports permutations of key-maps with different settings modified.

Useful for checking changes intended for one configuration don't impact others accidentally.

./blender.bin -b --factory-startup \
    --python tools/utils/blender_keyconfig_export_permutations.py -- \
    --preset=Blender \
    --output-dir=./output \
    --keymap-prefs=select_mouse:rmb_action

/blender.bin -b --factory-startup \
    --python tools/utils/blender_keyconfig_export_permutations.py -- \
    --preset=Blender_27x \
    --output-dir=output \
    --keymap-prefs="select_mouse"

The preferences setting: ``select_mouse:rmb_action`` expands into:

config = [
    ("select_mouse", ('LEFT', 'RIGHT')),
    ("rmb_action", ('TWEAK', 'FALLBACK_TOOL')),
]
"""

import os
import sys


def argparse_create():
    import argparse

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "--preset",
        dest="preset",
        default="Blender",
        metavar='PRESET', type=str,
        help="The name of the preset to export",
        required=False,
    )

    parser.add_argument(
        "--output-dir",
        dest="output_dir",
        default=".",
        metavar='OUTPUT_DIR', type=str,
        help="The directory to output to.",
        required=False,
    )

    parser.add_argument(
        "--keymap-prefs",
        dest="keymap_prefs",
        default="select_mouse:rmb_action",
        metavar='KEYMAP_PREFS', type=str,
        help=(
            "Colon separated list of attributes to generate key-map configuration permutations. "
            "WARNING: as all combinations are tested, their number increases exponentially!"
        ),
        required=False,
    )

    return parser


def permutations_from_attrs_impl(config, permutation, index):
    index_next = index + 1
    attr, values = config[index]
    for val in values:
        permutation[index] = (attr, val)
        if index_next == len(config):
            yield tuple(permutation)
        else:
            # Keep walking down the list of permutations.
            yield from permutations_from_attrs_impl(config, permutation, index_next)
    # Not necessary, just ensure stale values aren't used.
    permutation[index] = None


def permutations_from_attrs(config):
    """
    Take a list of attributes and possible values:

        config = [
            ("select_mouse", ('LEFT', 'RIGHT')),
            ("rmb_action", ('TWEAK', 'FALLBACK_TOOL')),
        ]

    Yielding all permutations:

        [("select_mouse", 'LEFT'), ("rmb_action", 'TWEAK')],
        [("select_mouse", 'LEFT'), ("rmb_action", 'FALLBACK_TOOL')],
        ... etc ...
    """
    if not config:
        return ()
    permutation = [None] * len(config)
    result = list(permutations_from_attrs_impl(config, permutation, 0))
    assert permutation == ([None] * len(config))
    return result


def permutation_as_filename(preset, values):
    """
    Takes a configuration, eg:

        [("select_mouse", 'LEFT'), ("rmb_action", 'TWEAK')]

    And returns a filename compatible path:
    """
    from urllib.parse import quote
    if not values:
        return quote(preset)

    return quote(
        preset + "_" + ".".join([
            "-".join((str(key), str(val)))
            for key, val in values
        ]),
        # Needed so forward slashes aren't included in the resulting name.
        safe="",
    )


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    try:
        import bpy
    except ImportError:
        # Run outside of Blender, just show "--help".
        bpy = None
        args.insert(0, "--help")

    args = argparse_create().parse_args(args)
    if bpy is None:
        return

    from bpy import context

    preset = args.preset
    output_dir = args.output_dir

    os.makedirs(output_dir, exist_ok=True)

    # Needed for background mode.
    preset_filepath = bpy.utils.preset_find(preset, preset_path="keyconfig")
    bpy.ops.preferences.keyconfig_activate(filepath=preset_filepath)

    # Key-map preferences..
    km_prefs = context.window_manager.keyconfigs.active.preferences
    config = []
    # Use RNA introspection:
    if args.keymap_prefs:
        for attr in args.keymap_prefs.split(":"):
            if not hasattr(km_prefs, attr):
                print(f"KeyMap preferences does not have attribute: {attr:s}")
                sys.exit(1)

            prop_def = km_prefs.rna_type.properties.get(attr)
            match prop_def.type:
                case 'ENUM':
                    value = tuple(val.identifier for val in prop_def.enum_items)
                case 'BOOLEAN':
                    value = (True, False)
                case _ as prop_def_type:
                    raise Exception(f"Unhandled attribute type {prop_def_type:s}")
            config.append((attr, value))
    config = tuple(config)

    for attr_permutation in (permutations_from_attrs(config) or ((),)):

        # Reload and set.
        if attr_permutation is not None:
            km_prefs = context.window_manager.keyconfigs.active.preferences
            for attr, value in attr_permutation:
                setattr(km_prefs, attr, value)
        # Re-activate after setting preferences, tsk, ideally this shouldn't be necessary.
        bpy.ops.preferences.keyconfig_activate(filepath=preset_filepath)

        filepath = os.path.join(output_dir, permutation_as_filename(preset, attr_permutation) + ".py")

        print("Writing:", filepath)
        bpy.ops.preferences.keyconfig_export(filepath=filepath, all=True)

    sys.exit()


if __name__ == "__main__":
    main()
