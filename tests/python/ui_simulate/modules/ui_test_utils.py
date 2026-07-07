# SPDX-FileCopyrightText: 2019-2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "call_menu",
    "call_operator",
    "cursor_motion_data_x",
    "cursor_motion_data_y",
    "cursor_motion_data_xy",
    "cursor_motion_data_circle",
    "get_area_center",
    "get_area_center_from_spacetype",
    "get_window_area_by_type",
    "get_window_size_in_pixels",
    "idle_until",
    "keep_open",
    "test_window",
)


def test_window(window_index=0):
    """
    Get window with associated event simulator and tests.
    """
    import unittest
    from .easy_keys import EventGenerate
    import bpy

    window = bpy.data.window_managers[0].windows[window_index]

    return (
        EventGenerate(window),
        unittest.TestCase(),
        window,
    )


def call_operator(e, text: str):
    """
    Call operator by name.
    """
    yield e.f3()
    yield e.text(text)
    yield e.ret()


def call_menu(e, text: str):
    """
    Call operator through menu.
    """
    yield e.f3()
    yield e.text_unicode(text.replace(" -> ", " \u25b8 "))
    yield e.ret()


def get_window_area_by_type(window, space_type):
    """
    Get first area of the specified space type in a window.
    """
    for area in window.screen.areas:
        if area.type == space_type:
            return area


def get_area_center(area):
    """
    Get coordinate in the center of an area, e.g. for placing the cursor.
    """
    return (
        area.x + area.width // 2,
        area.y + area.height // 2,
    )


def get_area_center_from_spacetype(window, space_type):
    """
    Get coordinate in the center of the first area with the given spacetype.
    """
    area = get_window_area_by_type(window, space_type)
    if area is None:
        raise Exception("Space Type {!r} not found".format(space_type))
    return get_area_center(area)


def get_window_size_in_pixels(window):
    """
    Get window size that can be used for positioning a cursor.
    """
    import sys
    size = window.width, window.height
    # macOS window size is a multiple of the pixel_size.
    if sys.platform == "darwin":
        from bpy import context
        # The value is always rounded to an int, so converting to an int is safe here.
        pixel_size = int(context.preferences.system.pixel_size)
        size = size[0] * pixel_size, size[1] * pixel_size
    return size


def idle_until(until, idle=1 / 60, timeout=1.0):
    """
    Idle while the internal event loop runs until a specified condition is true.

    This should be used sparingly as it may represent some other failure
    condition inside Blender. Currently, it is used to:
    - Test completion and cancellation of operators that use jobs.
    - Multi window undo tests that need separate view layer (see #148903).

    Note: In practice, the timeout value of 1.0 seconds should be more than enough
    for all cases. In testing with a fixed, constant delay, the tests succeeded
    with a timeout of 1/6th of a second.
    :param until: lambda to check the condition of after each sleep
    :param idle: how long to idle between checks of the `until` lambda.
        Defaults to 60Hz due to common refresh rates.
    :param timeout: the max time in seconds that this busy wait will execute.
    :return:
    """
    import datetime
    import time
    start_time = time.time()
    current_time = time.time()
    while current_time - start_time < timeout and not until():
        yield datetime.timedelta(seconds=idle)
        current_time = time.time()


def keep_open():
    """
    Only for development, handy so we can quickly keep the window open while testing.
    """
    import bpy
    bpy.app.use_event_simulate = False


def cursor_motion_data_x(window, margin=0.2):
    """
    Generate a range of (x,y) positions in screen space, centered vertically in the window
    from left to right.

    :param margin: Percentage of left and right window space to leave unused
    """
    size = get_window_size_in_pixels(window)
    return [
        (x, size[1] // 2) for x in
        range(int(size[0] * margin), int(size[0] * (1.0 - margin)), 80)
    ]


def cursor_motion_data_y(window, margin=0.2):
    """
    Generate a range of (x,y) positions in screen space, centered horizontally in the window
    from bottom to top.

    :param margin: Percentage of top and bottom window space to leave unused
    """
    size = get_window_size_in_pixels(window)
    return [
        (size[0] // 2, y) for y in
        range(int(size[1] * margin), int(size[1] * (1.0 - margin)), 80)
    ]


def cursor_motion_data_xy(window, margin=0.2):
    """
    Generate a range of (x,y) positions in screen space from bottom left to top right

    :param margin: Percentage of window space to leave unused
    """
    size = get_window_size_in_pixels(window)
    return [
        (p, p) for p in
        range(int(size[0] * margin), int(size[0] * (1.0 - margin)), 80)
    ]


def cursor_motion_data_circle(center, radius):
    """
    Generate a range of (x,y) positions in screen space as a circle.
    :param center: The center of the circle
    :param radius: The radius of the circle
    """
    import sys
    from math import sin, cos, pi
    if sys.platform == "darwin":
        from bpy import context
        # The value is always rounded to an int, so converting to an int is safe here.
        radius = radius * int(context.preferences.system.pixel_size)

    steps = 20
    angles = [(i / steps) * 2.0 * pi for i in range(steps)]
    angles.append(0.0)

    return [
        (int(center[0] + -radius * sin(phi)), int(center[1] + radius * cos(phi))) for phi in angles
    ]
