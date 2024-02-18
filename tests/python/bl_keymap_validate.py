# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ./blender.bin --background --factory-startup --python tests/python/bl_keymap_validate.py
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
- Identical key-map items.

This works by taking the keymap data (before it's loaded into Blender),
then comparing it with that same keymap after exporting and importing.

NOTE:

   Append ' -- --relaxed' not to fail on small issues,
   this is so minor inconsistencies don't break the tests.
"""

import types
from typing import (
    Any,
    Dict,
    Generator,
    List,
    Optional,
    Sequence,
    Tuple,
)

KeyConfigData = List[Tuple[str, Tuple[Any], Dict[str, Any]]]

import contextlib

import bpy  # type: ignore

# Useful for diffing the output to see what changed in context.
# this writes keymaps into the current directory with `.orig.py` & `.rewrite.py` extensions.
WRITE_OUTPUT_DIR = ""  # "/tmp", defaults to the systems temp directory.

# For each preset, test all of these options.
# The key is the preset name, containing a sequence of (attribute, value) pairs to test.
#
# NOTE(@campbellbarton): only add these for preferences which impact multiple keys as exposing all preferences
# this way would create too many combinations making the tests take too long to complete.
PRESET_PREFS = {
    "Blender": (
        (("select_mouse", 'LEFT'), ("use_alt_tool", False)),
        (("select_mouse", 'LEFT'), ("use_alt_tool", True)),
        (("select_mouse", 'RIGHT'), ("rmb_action", 'TWEAK')),
        (("select_mouse", 'RIGHT'), ("rmb_action", 'FALLBACK_TOOL')),
    ),
}

# Don't report duplicates for these presets.
ALLOW_DUPLICATES = {
    # This key-map manipulates the default key-map, making it difficult to avoid duplicates entirely.
    "Industry_Compatible"
}

# -----------------------------------------------------------------------------
# Generic Utilities


@contextlib.contextmanager
def temp_fn_argument_extractor(
        mod: types.ModuleType,
        mod_attr: str,
) -> Generator[List[Tuple[Tuple[Tuple[Any], ...], Dict[str, Dict[str, Any]]]], None, None]:
    """
    Temporarily intercept a function, so its arguments can be extracted.
    The context manager gives us a list where each item is a tuple of
    arguments & keywords, stored each time the function was called.
    """
    args_collected = []
    real_fn = getattr(mod, mod_attr)

    def wrap_fn(*args: Tuple[Any], **kw: Dict[str, Any]) -> Any:
        args_collected.append((args, kw))
        return real_fn(*args, **kw)
    setattr(mod, mod_attr, wrap_fn)
    try:
        yield args_collected
    finally:
        setattr(mod, mod_attr, real_fn)


def round_float_32(f: float) -> float:
    from struct import pack, unpack
    return unpack("f", pack("f", f))[0]  # type: ignore


def report_humanly_readable_difference(a: Any, b: Any) -> Optional[str]:
    """
    Compare strings, return None when they match,
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

def keyconfig_preset_scan() -> List[str]:
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


def keymap_item_property_clean(value: Any) -> Any:
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
            # Ignore type checking, these are strings which we know can be sorted.
            key=lambda item: item[0],  # type: ignore
        )
    return value


def keymap_data_clean(keyconfig_data: KeyConfigData, *, relaxed: bool) -> None:
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
            # Writing `None` makes the most sense when explicitly written, however generated properties
            # might be empty and it's not worth adding checks in the generation logic to use `None`
            # just to satisfy this check.
            if not item_prop:
                items[i] = item_op, item_event, None


def keyconfig_config_as_filename_component(values: Sequence[Tuple[str, Any]]) -> str:
    """
    Takes a configuration, eg:

        [("select_mouse", 'LEFT'), ("rmb_action", 'TWEAK')]

    And returns a filename compatible path:
    """
    from urllib.parse import quote
    if not values:
        return ""

    return "(" + quote(
        ".".join([
            "-".join((str(key), str(val)))
            for key, val in values
        ]),
        # Needed so forward slashes aren't included in the resulting name.
        safe="",
    ) + ")"


def keyconfig_activate_and_extract_data(
        filepath: str,
        *,
        relaxed: bool,
        config: Sequence[Tuple[str, Any]],
) -> KeyConfigData:
    """
    Activate the key-map by filepath,
    return the key-config data (cleaned for comparison).
    """
    import bl_keymap_utils.io  # type: ignore

    if config:
        bpy.ops.preferences.keyconfig_activate(filepath=filepath)
        km_prefs = bpy.context.window_manager.keyconfigs.active.preferences
        for attr, value in config:
            setattr(km_prefs, attr, value)

    with temp_fn_argument_extractor(bl_keymap_utils.io, "keyconfig_init_from_data") as args_collected:
        bpy.ops.preferences.keyconfig_activate(filepath=filepath)

        # If called multiple times, something strange is happening.
        assert len(args_collected) == 1
        args, _kw = args_collected[0]
        # Ignore the type check as `temp_fn_argument_extractor` is a generic function
        # which doesn't contain type information of the function being wrapped.
        keyconfig_data: KeyConfigData = args[1]  # type: ignore
        keymap_data_clean(keyconfig_data, relaxed=relaxed)
        return keyconfig_data


def keyconfig_report_duplicates(keyconfig_data: KeyConfigData) -> str:
    """
    Return true if any of the key-maps have duplicate items.

    Duplicate items are reported so they can be resolved.
    """
    error_text = []
    for km_idname, km_args, km_items_data in keyconfig_data:
        items = tuple(km_items_data["items"])
        unique: Dict[str, List[int]] = {}
        for i, (item_op, item_event, item_prop) in enumerate(items):
            # Ensure stable order as `repr` will use order of definition.
            item_event = {key: item_event[key] for key in sorted(item_event.keys())}
            if item_prop is not None:
                item_prop = {key: item_prop[key] for key in sorted(item_prop.keys())}
            item_repr = repr((item_op, item_event, item_prop))
            unique.setdefault(item_repr, []).append(i)
        for key, value in unique.items():
            if len(value) > 1:
                error_text.append("\"%s\" %r indices %r for item %r" % (km_idname, km_args, value, key))
    return "\n".join(error_text)


def main() -> None:
    import os
    import sys
    import pprint
    import tempfile

    argv = (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])

    # Use `argparse` for full argument parsing, for now this is enough.
    relaxed = "--relaxed" in argv

    # NOTE(@ideasman42): Disable add-on items as they may cause differences in the key-map.
    import addon_utils
    addon_utils.disable_all()

    has_error = False

    presets = keyconfig_preset_scan()
    for filepath in presets:
        name_only = os.path.splitext(os.path.basename(filepath))[0]
        for config in PRESET_PREFS.get(name_only, ((),)):
            name_only_with_config = name_only + keyconfig_config_as_filename_component(config)
            print("KeyMap Validate:", name_only_with_config, end=" ... ")
            data_orig = keyconfig_activate_and_extract_data(
                filepath,
                relaxed=relaxed,
                config=config,
            )

            with tempfile.TemporaryDirectory() as dir_temp:
                filepath_temp = os.path.join(
                    dir_temp,
                    name_only_with_config + ".test" + ".py",
                )

                bpy.ops.preferences.keyconfig_export(filepath=filepath_temp, all=True)
                data_reimport = keyconfig_activate_and_extract_data(
                    filepath_temp,
                    relaxed=relaxed,
                    # No configuration supported when loading exported key-maps.
                    config=(),
                )

            # Comparing a pretty printed string tends to give more useful
            # text output compared to the data-structure. Both will work.
            if (cmp_message := report_humanly_readable_difference(
                    pprint.pformat(data_orig, indent=0, width=120),
                    pprint.pformat(data_reimport, indent=0, width=120),
            )):
                error_text_consistency = "Keymap %s has inconsistency on re-importing." % cmp_message
            else:
                error_text_consistency = ""

            # Perform an additional sanity check:
            # That there are no identical key-map items.
            if name_only not in ALLOW_DUPLICATES:
                error_text_duplicates = keyconfig_report_duplicates(data_orig)
            else:
                error_text_duplicates = ""

            if error_text_consistency or error_text_duplicates:
                print("FAILED!")
                print("%r has errors!" % filepath)
                if error_text_consistency:
                    print(error_text_consistency)
                if error_text_duplicates:
                    print(error_text_duplicates)
                has_error = True
            else:
                print("OK!")

            if WRITE_OUTPUT_DIR:
                os.makedirs(WRITE_OUTPUT_DIR, exist_ok=True)
                name_only_temp = os.path.join(WRITE_OUTPUT_DIR, name_only_with_config)
                print("Writing data to:", name_only_temp + ".*.py")
                with open(name_only_temp + ".orig.py", 'w') as fh:
                    fh.write(pprint.pformat(data_orig, indent=0, width=120))
                with open(name_only_temp + ".rewrite.py", 'w') as fh:
                    fh.write(pprint.pformat(data_reimport, indent=0, width=120))
    if has_error:
        sys.exit(1)


if __name__ == "__main__":
    main()
