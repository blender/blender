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
    yield e.g()     # General
    yield e.a()     # Animation
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Animation")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.c()     # Compositing
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Compositing")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.g()     # Geometry Nodes
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Geometry Nodes")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.l()     # Layout
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Layout")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.m()     # Modeling
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Modeling")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.r()     # Rendering
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Rendering")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.s()     # Scripting
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Scripting")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.p()     # Sculpting
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Sculpting")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.h()     # Shading
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Shading")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.t()     # Texture Paint
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Texture Paint")

    yield from _call_by_name(e, "Add Workspace")
    yield e.g()     # General
    yield e.u()     # UV Editing
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "UV Editing")


def sanity_check_2d_animation():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.d()     # 2D Animation
    yield e.d()     # 2D Animation
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "2D Animation")

    yield from _call_by_name(e, "Add Workspace")
    yield e.d()     # 2D Animation
    yield e.f()     # 2D Full Canvas
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "2D Full Canvas")


def sanity_check_sculpting():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.s()     # Sculpting
    yield e.s()     # Sculpting
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Sculpting")

    yield from _call_by_name(e, "Add Workspace")
    yield e.s()     # Sculpting
    yield e.h()     # Shading
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Shading")


def sanity_check_storyboarding():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.t()     # Storyboarding
    yield e.d()     # 2D Full Canvas
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "2D Full Canvas")

    yield from _call_by_name(e, "Add Workspace")
    yield e.t()     # Storyboarding
    yield e.s()     # Storyboarding
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Storyboarding")


def sanity_check_vfx():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.v()     # VFX
    yield e.c()     # Compositing
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Compositing")

    yield from _call_by_name(e, "Add Workspace")
    yield e.v()     # VFX
    yield e.m()     # Masking
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Masking")

    yield from _call_by_name(e, "Add Workspace")
    yield e.v()     # VFX
    yield e.t()     # Motion Tracking
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Motion Tracking")

    yield from _call_by_name(e, "Add Workspace")
    yield e.v()     # VFX
    yield e.r()     # Rendering
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Rendering")


def sanity_check_video_editing():
    e, t = _test_vars(window := _test_window())

    yield from _call_by_name(e, "Add Workspace")
    yield e.e()     # Video Editing
    yield e.r()     # Rendering
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Rendering")

    yield from _call_by_name(e, "Add Workspace")
    yield e.e()     # Video Editing
    yield e.v()     # Video Editing
    t.assertEqual(window.workspace.name_full.split(".", 1)[0], "Video Editing")
