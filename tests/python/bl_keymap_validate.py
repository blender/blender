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

# <pep8 compliant>

# ./blender.bin --background -noaudio --factory-startup --python tests/python/bl_keymap_validate.py
#

"""
This catches the following kinds of issues:

- Arguments that don't make sense, e.g.
  - 'repeat' for non keyboard events.
  - 'any' argument with other modifiers also enabled.
  - Duplicate property entries.
- Unknown/unused dictionary keys.
- Unused keymaps (keymaps which are defined but not used anywhere).
- Event values that don't make sense for the event type, e.g.
  An escape key could have the value "NORTH" instead of "PRESS".

This works by taking the keymap data (before it's loaded into Blender),
then comparing it with that same keymap after exporting and imporing.

NOTE:

   Append ' -- --relaxed' not to fail on small issues,
   this is so minor inconsistencies don't break the tests.
"""

import types
import typing

import contextlib

import bpy

# Useful for diffing the output to see what changed in context.
# this writes keymaps into the current directory with `.orig.py` & `.rewrite.py` extensions.
WRITE_OUTPUT_DIR = None  # "/tmp", defaults to the systems temp directory.


# -----------------------------------------------------------------------------
# Generic Utilities

@contextlib.contextmanager
def temp_fn_argument_extractor(
        mod: types.ModuleType,
        mod_attr: str,
) -> typing.Iterator[typing.List[typing.Tuple[list, dict]]]:
    """
    Temporarily intercept a function, so it's arguments can be extracted.
    The context manager gives us a list where each item is a tuple of
    arguments & keywords, stored each time the function was called.
    """
    args_collected = []
    real_fn = getattr(mod, mod_attr)

    def wrap_fn(*args, **kw):
        args_collected.append((args, kw))
        return real_fn(*args, **kw)
    setattr(mod, mod_attr, wrap_fn)
    try:
        yield args_collected
    finally:
        setattr(mod, mod_attr, real_fn)


def round_float_32(f: float) -> float:
    from struct import pack, unpack
    return unpack("f", pack("f", f))[0]


def report_humanly_readable_difference(a: typing.Any, b: typing.Any) -> typing.Optional[str]:
    """
    Compare strings, return None whrn they match,
    otherwise a humanly readable difference message.
    """
    import unittest
    cls = unittest.TestCase()
    try:
        cls.assertEqual(a, b)
    except AssertionError as ex:
        return str(ex)
    return None


# -----------------------------------------------------------------------------
# Keymap Utilities.

def keyconfig_preset_scan() -> typing.List[str]:
    """
    Return all bundled presets (keymaps), not user presets.
    """
    import os
    from bpy import context
    # This assumes running with `--factory-settings`.
    default_keyconfig = context.window_manager.keyconfigs.active.name
    default_preset_filepath = bpy.utils.preset_find(default_keyconfig, "keyconfig")
    dirname, filename = os.path.split(default_preset_filepath)
    return [
        os.path.join(dirname, f)
        for f in sorted(os.listdir(dirname))
        if f.lower().endswith(".py")
        if not f.startswith((".", "_"))
    ]


def keymap_item_property_clean(value: typing.Any) -> typing.Any:
    """
    Recursive property sanitize.

    - Make all floats 32bit (since internally they are).
    - Sort all properties (since the order isn't important).
    """
    if type(value) is float:
        # Once the value is loaded back from Blender, it will be 32bit.
        return round_float_32(value)
    if type(value) is list:
        return sorted(
            # Convert to `dict` to de-duplicate.
            dict([(k, keymap_item_property_clean(v)) for k, v in value]).items(),
            key=lambda item: item[0],
        )
    return value


def keymap_data_clean(keyconfig_data: typing.List, *, relaxed: bool) -> None:
    """
    Order & sanitize keymap data so the result
    from the hand written Python script is comparable with data exported & imported.
    """
    keyconfig_data.sort(key=lambda a: a[0])
    for _, _, km_items_data in keyconfig_data:
        items = km_items_data["items"]
        for i, (item_op, item_event, item_prop) in enumerate(items):
            if relaxed:
                # Prevent "alt": False from raising an error.
                defaults = {"alt": False, "ctrl": False, "shift": False, "any": False}
                defaults.update(item_event)
                item_event.update(defaults)

            if item_prop:
                properties = item_prop.get("properties")
                if properties:
                    properties[:] = keymap_item_property_clean(properties)
                else:
                    item_prop.pop("properties")

            # Needed so: `{"properties": ()}` matches `None` as there is no meaningful difference.
            # Wruting `None` makes the most sense when explicitly written, however generated properties
            # might be empty and it's not worth adding checks in the generation logic to use `None`
            # just to satisfy this check.
            if not item_prop:
                items[i] = item_op, item_event, None


def keyconfig_activate_and_extract_data(filepath: str, *, relaxed: bool) -> typing.List:
    """
    Activate the key-map by filepath,
    return the key-config data (cleaned for comparison).
    """
    import bl_keymap_utils.io
    with temp_fn_argument_extractor(bl_keymap_utils.io, "keyconfig_init_from_data") as args_collected:
        bpy.ops.preferences.keyconfig_activate(filepath=filepath)
        # If called multiple times, something strange is happening.
        assert(len(args_collected) == 1)
        args, _kw = args_collected[0]
        keyconfig_data = args[1]
        keymap_data_clean(keyconfig_data, relaxed=relaxed)
        return keyconfig_data


def main() -> None:
    import os
    import sys
    import pprint
    import tempfile

    argv = (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])

    # Use `argparse` for full arg parsing, for now this is enough.
    relaxed = "--relaxed" not in argv

    has_error = False

    presets = keyconfig_preset_scan()
    for filepath in presets:
        name_only = os.path.splitext(os.path.basename(filepath))[0]

        print("KeyMap Validate:", name_only, end=" ... ")

        data_orig = keyconfig_activate_and_extract_data(filepath, relaxed=relaxed)

        with tempfile.TemporaryDirectory() as dir_temp:
            filepath_temp = os.path.join(dir_temp, name_only + ".test.py")
            bpy.ops.preferences.keyconfig_export(filepath=filepath_temp, all=True)
            data_reimport = keyconfig_activate_and_extract_data(filepath_temp, relaxed=relaxed)

        # Comparing a pretty printed string tends to give more useful
        # text output compared to the data-structure. Both will work.
        if (cmp_message := report_humanly_readable_difference(
                pprint.pformat(data_orig, indent=0, width=120),
                pprint.pformat(data_reimport, indent=0, width=120),
        )):
            print("FAILED!")
            sys.stdout.write((
                "Keymap %s has inconsistency on re-importing:\n"
                "  %r"
            ) % (filepath, cmp_message))
            has_error = True
        else:
            print("OK!")

        if WRITE_OUTPUT_DIR:
            name_only_temp = os.path.join(WRITE_OUTPUT_DIR, name_only)
            print("Writing data to:", name_only_temp + ".*.py")
            with open(name_only_temp + ".orig.py", 'w') as fh:
                fh.write(pprint.pformat(data_orig, indent=0, width=120))
            with open(name_only_temp + ".rewrite.py", 'w') as fh:
                fh.write(pprint.pformat(data_reimport, indent=0, width=120))
    if has_error:
        sys.exit(1)


if __name__ == "__main__":
    main()
