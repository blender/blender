# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This file does not run anything, its methods are accessed for tests by ``run_blender_setup.py``.
"""


def _test_window(windows_exclude=None):
    import bpy
    wm = bpy.data.window_managers[0]
    if windows_exclude is None:
        return wm.windows[0]
    for window in wm.windows:
        if window not in windows_exclude:
            return window
    return None


def _test_vars(window):
    import unittest
    from modules.easy_keys import EventGenerate
    return (
        EventGenerate(window),
        unittest.TestCase(),
    )


def _call_by_name(e, text: str):
    yield e.f3()
    yield e.text(text)
    yield e.ret()


def _call_menu(e, text: str):
    yield e.f3()
    yield e.text_unicode(text.replace(" -> ", " \u25b8 "))
    yield e.ret()


def sanity_check_general():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("ga")  # General > Animation
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Animation")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gc")  # General > Compositing
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Compositing")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gg")  # General > Geometry Nodes
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Geometry Nodes")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gl")  # General > Layout
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Layout")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gm")  # General > Modeling
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Modeling")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gr")  # General > Rendering
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Rendering")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gscr")  # General > Scripting
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Scripting")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gscu")  # General > Sculpting
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Sculpting")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gsh")  # General > Shading
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Shading")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gt")  # General > Texture Paint
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Texture Paint")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("gu")  # General > UV Editing
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "UV Editing")


def sanity_check_2d_animation():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.text(" 2d")  # 2D Animation > 2D Animation
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "2D Animation")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("fu")  # 2D Animation > 2D Full Canvas
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "2D Full Canvas")


def sanity_check_sculpting():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("sscu")  # Sculpting > Sculpting
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Sculpting")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("ssh")  # Sculpting > Shading
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Shading")


def sanity_check_storyboarding():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("sv")  # Storyboarding > Video Editing
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Video Editing")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("st")  # Storyboarding > Storyboarding
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Storyboarding")


def sanity_check_vfx():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("vc")  # VFX > Compositing
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Compositing")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("vma")  # VFX > Masking
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Masking")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("vmo")  # VFX > Motion Tracking
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Motion Tracking")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("vr")  # VFX > Rendering
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Rendering")


def sanity_check_video_editing():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("vir")  # Video Editing > Rendering
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Rendering")

    yield from _call_by_name(e, "Add Workspace")
    yield e.text("vv")  # Video Editing > Video Editing
    yield e.ret()
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Video Editing")
