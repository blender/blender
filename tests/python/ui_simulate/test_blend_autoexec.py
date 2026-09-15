# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.

Covered test cases, with automatic script execution enabled & disabled:
  - "Dropping a blend-file" both trusted/untrusted cases.
  - "Open" (the file selector) both trusted/untrusted cases.
  - Enabling "Trusted Source" for an excluded path when dropping.
"""

import modules.ui_test_utils as ui

BASE_TRUSTED = "trusted_dir"
BASE_UNTRUSTED = "untrusted_dir"

BLEND_TRUSTED = "trusted_file.blend"
BLEND_UNTRUSTED = "untrusted_file.blend"

# A registered text block, run when the blend-file is loaded with scripts trusted.
TEXT_SCRIPT = "script_maybe_executes.py"
SCENE_SCRIPT_RAN = "Scene [modified by script]"
TEXT_SCRIPT_BODY = (
    "import bpy\n"
    "bpy.data.scenes[0].name = {!r}\n"
).format(SCENE_SCRIPT_RAN)

# Drop events can't be simulated, bind the operator a drop runs to this key instead.
DROP_KEY_WORKAROUND = 'F13'


# -----------------------------------------------------------------------------
# Utilities

def _config_assert_empty():
    """
    Ensure a fresh user configuration (see ``BLENDER_USER_RESOURCES``),
    saving writes the recent files which must not overwrite the user's own.
    """
    import bpy
    import os
    dirpath = bpy.utils.user_resource('CONFIG', create=True)
    if os.listdir(dirpath):
        raise Exception("Expected an empty user configuration in {!r}".format(dirpath))


def _events_after_load():
    """
    Loading a file frees the window, return events for the window that replaced it.
    """
    e, _t, _w = ui.test_window()
    return e


def _cursor_move_to_window_bottom_left(e):
    """
    Move the cursor to the bottom left of the window, inset so it stays inside.
    """
    e.cursor_position_set(1, 1, move=True)


def _temp_dir_context():
    """
    Return a temporary directory for the blend-files,
    entered into the file selector as an absolute path.
    """
    import tempfile
    return tempfile.TemporaryDirectory(prefix="bl_autoexec_")


def _blend_file_new(e):
    """
    Create a new file from the "New File" menu, "General".
    """
    yield e.ctrl.n()
    # `g` for General.
    yield e.g()


def _blend_file_save(e, filepath):
    """
    Save to ``filepath`` from the file selector, entering the path directly.
    """
    # WARNING: hack, the directory field only splits off a file name when the file exists.
    # Ideally the file name would be typed in, there is no way to activate the field.
    # TODO: add a `FILE_OT_edit_filename` operator with a shortcut.
    open(filepath, "wb").close()

    yield e.ctrl.shift.s()
    # Activate the location field.
    yield e.ctrl.l()
    yield e.text_unicode(filepath)
    # Two presses accept the path (changing the directory keeps the field
    # active for auto-completion), a third saves the file.
    for _ in range(3):
        yield e.ret()


def _blend_file_open(e, filepath):
    """
    Open ``filepath`` from the file selector, entering the directory directly.

    The file selector starts at the most recent file (the trusted file, see ``_setup``)
    so "Trusted Source" follows the preference until the path is entered.
    """
    import bpy
    import os

    yield e.ctrl.o()
    # Activate the location field.
    yield e.ctrl.l()
    yield e.text_unicode(os.path.dirname(filepath) + os.sep)
    # Accept the directory (twice, it stays active for auto-completion when it changes),
    # then select the file in the list & open it.
    # A file may already be set so any press can open it,
    # stop once the selector closes since loading frees the window.
    for action in ("ret", "ret", "down_arrow", "ret"):
        if ui.get_window_area_by_type(bpy.context.window, 'FILE_BROWSER') is None:
            break
        yield getattr(e, action)()


def _setup(dirpath_temp, *, use_autoexec):
    """
    Save a blend-file in a trusted & an excluded directory of ``dirpath_temp``.
    """
    import bpy
    import os

    _config_assert_empty()

    e, t, _w = ui.test_window()
    # Dismiss the splash screen, it takes any key press.
    yield e.esc()
    yield from _blend_file_new(e)
    e = _events_after_load()

    prefs = bpy.context.preferences
    prefs.use_preferences_save = False
    prefs.filepaths.use_scripts_auto_execute = use_autoexec
    # Show the file selector in the same window, avoiding window switching.
    prefs.view.filebrowser_display_type = 'SCREEN'

    dirpath_trusted = os.path.join(dirpath_temp, BASE_TRUSTED) + os.sep
    dirpath_untrusted = os.path.join(dirpath_temp, BASE_UNTRUSTED) + os.sep
    os.makedirs(dirpath_trusted)
    os.makedirs(dirpath_untrusted)

    prefs.autoexec_paths.new().path = dirpath_untrusted

    # NOTE: the trusted file is saved last so the file selector starts in a trusted
    # directory, otherwise "Trusted Source" is unset before the path is even entered.
    filepaths = {
        BLEND_UNTRUSTED: dirpath_untrusted + BLEND_UNTRUSTED,
        BLEND_TRUSTED: dirpath_trusted + BLEND_TRUSTED,
    }

    # Saved into both files, running it renames the scene so it can be detected.
    text = bpy.data.texts.new(TEXT_SCRIPT)
    text.write(TEXT_SCRIPT_BODY)
    text.use_module = True

    for filepath in filepaths.values():
        yield from _blend_file_save(e, filepath)

    # Start from an unsaved file so opening a recent file can be detected.
    yield from _blend_file_new(e)
    e = _events_after_load()

    t.assertEqual(bpy.data.filepath, "")
    t.assertEqual(bpy.app.autoexec, use_autoexec, "Auto-execution follows the preference")

    return e, t, filepaths


def _assert_blend_file_opened(t, filename, *, is_trusted, message=None):
    """
    Check ``filename`` is open, that it opened with the expected trust
    & that its registered text block ran (or didn't).
    """
    import bpy
    import os
    t.assertEqual(os.path.basename(bpy.data.filepath), filename)
    t.assertEqual(bpy.app.autoexec, is_trusted, message)
    # Check the file opened untrusted instead of being corrected once loaded,
    # both clear auto-execution so the other checks can't tell them apart.
    props = bpy.context.window_manager.operator_properties_last("wm.open_mainfile")
    t.assertEqual(props.use_scripts, is_trusted, message)
    # The file's registered text block only runs when the file is trusted.
    t.assertEqual(bpy.data.scenes[0].name == SCENE_SCRIPT_RAN, is_trusted, message)
    t.assertEqual(bpy.app.autoexec_fail, not is_trusted, message)


def _blend_file_drop(e, *, filepath):
    """
    Drop ``filepath``, showing the popup which defaults the "Trusted Source" option.

    Dropping can't be simulated, ``wm.drop_blend_file`` is bound to ``DROP_KEY_WORKAROUND``.
    """
    import bpy

    # Locate the cursor predictably so no elements land under it when the popup opens.
    _cursor_move_to_window_bottom_left(e)

    km = bpy.context.window_manager.keyconfigs.addon.keymaps.new(name="Window")
    for kmi in km.keymap_items:
        if kmi.idname == "wm.drop_blend_file":
            break
    else:
        kmi = km.keymap_items.new("wm.drop_blend_file", DROP_KEY_WORKAROUND, 'PRESS')
    kmi.properties.filepath = filepath
    # Cycle the event loop so the key-map synchronizes.
    yield

    yield getattr(e, DROP_KEY_WORKAROUND.lower())()


def _assert_blend_file_dropped(t, filepath, *, is_path_trusted, message=None):
    """
    Check whether the path is excluded, an excluded path shows the option
    disabled as "Trusted Source [Untrusted Path]".

    The popups own "Trusted Source" state isn't checked, it's stored on the
    operator running the popup which isn't reachable from here.
    Opening the file checks it, see ``_assert_blend_file_opened``.
    """
    import bpy
    t.assertEqual(
        bpy.path.is_autoexec(filepath, canonicalize=True, strip_filename=True),
        is_path_trusted, message)


def _drop_ui_trusted_source_toggle(e):
    """
    Toggle the "Trusted Source" button, down twice then return activates it.
    """
    for _ in range(2):
        yield e.down_arrow()
    yield e.ret()


def _drop_ui_open(e):
    """
    Activate "Open" in the popup by its accelerator.
    """
    yield e.o()


def _blend_file_drop_test(filename, *, use_autoexec, is_trusted, message):
    is_path_trusted = filename == BLEND_TRUSTED
    with _temp_dir_context() as dirpath_temp:
        e, t, filepaths = yield from _setup(dirpath_temp, use_autoexec=use_autoexec)
        filepath = filepaths[filename]

        yield from _blend_file_drop(e, filepath=filepath)

        _assert_blend_file_dropped(
            t,
            filepath,
            is_path_trusted=is_path_trusted,
            message=message,
        )

        yield from _drop_ui_open(e)

        _assert_blend_file_opened(
            t,
            filename,
            is_trusted=is_trusted,
            message=message,
        )


# -----------------------------------------------------------------------------
# Tests with Auto-Execution Preference "Enabled"


def pref_enabled_drop_blend_file_untrusted():
    """
    Drop an excluded path & open it from the popup with the preference enabled,
    its scripts must not run.

    Proves the excluded paths are used when opening from the popup.
    """
    yield from _blend_file_drop_test(
        BLEND_UNTRUSTED,
        use_autoexec=True,
        is_trusted=False,
        message="An excluded path must not be trusted in the drop UI",
    )


def pref_enabled_drop_blend_file_trusted():
    """
    Drop a trusted file & open it from the popup with the preference enabled,
    its scripts must run.

    Proves a path which isn't excluded runs scripts without being asked.
    """
    yield from _blend_file_drop_test(
        BLEND_TRUSTED,
        use_autoexec=True,
        is_trusted=True,
        message="A path which isn't excluded must be trusted in the drop UI",
    )


def pref_enabled_open_file_selector_trusted():
    """
    Open a trusted file from the file selector with the preference enabled,
    its scripts must run.

    Proves the file selector trusts a path which isn't excluded,
    without it a selector which never trusts anything would pass the other tests.
    """
    with _temp_dir_context() as dirpath_temp:
        e, t, filepaths = yield from _setup(dirpath_temp, use_autoexec=True)

        yield from _blend_file_open(e, filepaths[BLEND_TRUSTED])

        _assert_blend_file_opened(
            t,
            BLEND_TRUSTED,
            is_trusted=True,
            message="A path which isn't excluded must be trusted in the file selector",
        )


def pref_enabled_open_file_selector_untrusted():
    """
    Open an excluded path from the file selector with the preference enabled,
    its scripts must not run.

    Proves the excluded paths are used when opening from the file selector.
    """
    with _temp_dir_context() as dirpath_temp:
        e, t, filepaths = yield from _setup(dirpath_temp, use_autoexec=True)

        yield from _blend_file_open(e, filepaths[BLEND_UNTRUSTED])

        _assert_blend_file_opened(
            t,
            BLEND_UNTRUSTED,
            is_trusted=False,
            message="An excluded path must not be trusted in the file selector",
        )


# -----------------------------------------------------------------------------
# Tests with Auto-Execution Preference "Disabled"


def pref_disabled_open_file_selector():
    """
    Open a trusted file from the file selector with the preference disabled,
    its scripts must not run.

    Proves the preference alone stops them running,
    a trusted path is used so the excluded paths can't be the cause.
    """
    with _temp_dir_context() as dirpath_temp:
        e, t, filepaths = yield from _setup(dirpath_temp, use_autoexec=False)

        yield from _blend_file_open(e, filepaths[BLEND_TRUSTED])

        _assert_blend_file_opened(
            t,
            BLEND_TRUSTED,
            is_trusted=False,
            message="The preference not to run scripts must be respected in the file selector",
        )


def pref_disabled_drop_blend_file():
    """
    Drop a trusted file & open it from the popup with the preference disabled,
    its scripts must not run.

    Proves the preference alone stops them running,
    a trusted path is used so the excluded paths can't be the cause.
    """
    yield from _blend_file_drop_test(
        BLEND_TRUSTED,
        use_autoexec=False,
        is_trusted=False,
        message="The preference not to run scripts must be respected in the drop UI",
    )


def pref_disabled_drop_blend_file_trusted_source():
    """
    Drop an excluded path with the preference disabled & enable "Trusted Source"
    before opening it, its scripts must run.

    Proves the excluded paths are only used when script execution is enabled.
    """
    with _temp_dir_context() as dirpath_temp:
        e, t, filepaths = yield from _setup(dirpath_temp, use_autoexec=False)
        filepath = filepaths[BLEND_UNTRUSTED]

        yield from _blend_file_drop(e, filepath=filepath)

        _assert_blend_file_dropped(
            t,
            filepath,
            is_path_trusted=False,
            message="An excluded path must not be trusted in the drop UI",
        )

        yield from _drop_ui_trusted_source_toggle(e)

        yield from _drop_ui_open(e)

        _assert_blend_file_opened(
            t,
            BLEND_UNTRUSTED,
            is_trusted=True,
            message="Enabling \"Trusted Source\" from the drop UI must run scripts",
        )
