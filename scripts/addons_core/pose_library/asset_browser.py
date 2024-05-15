# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""Functions for finding and working with Asset Browsers."""

from typing import Iterable, Optional, Tuple

import bpy
from bpy_extras import asset_utils


if "functions" not in locals():
    from . import functions
else:
    import importlib

    functions = importlib.reload(functions)


def biggest_asset_browser_area(screen: bpy.types.Screen) -> Optional[bpy.types.Area]:
    """Return the asset browser Area that's largest on screen.

    :param screen: context.window.screen

    :return: the Area, or None if no Asset Browser area exists.
    """

    def area_sorting_key(area: bpy.types.Area) -> Tuple[bool, int]:
        """Return area size in pixels."""
        return area.width * area.height

    areas = list(suitable_areas(screen))
    if not areas:
        return None

    return max(areas, key=area_sorting_key)


def suitable_areas(screen: bpy.types.Screen) -> Iterable[bpy.types.Area]:
    """Generator, yield Asset Browser areas."""

    for area in screen.areas:
        space_data = area.spaces[0]
        if not asset_utils.SpaceAssetInfo.is_asset_browser(space_data):
            continue
        yield area


def area_from_context(context: bpy.types.Context) -> Optional[bpy.types.Area]:
    """Return an Asset Browser suitable for the given category.

    Prefers the current Asset Browser if available, otherwise the biggest.
    """

    space_data = context.space_data
    if asset_utils.SpaceAssetInfo.is_asset_browser(space_data):
        return context.area

    # Try the current screen first.
    browser_area = biggest_asset_browser_area(context.screen)
    if browser_area:
        return browser_area

    for win in context.window_manager.windows:
        if win.screen == context.screen:
            continue
        browser_area = biggest_asset_browser_area(win.screen)
        if browser_area:
            return browser_area

    return None


def activate_asset(asset: bpy.types.Action, asset_browser: bpy.types.Area, *, deferred: bool) -> None:
    """Select & focus the asset in the browser."""

    space_data = asset_browser.spaces[0]
    assert asset_utils.SpaceAssetInfo.is_asset_browser(space_data)
    space_data.activate_asset_by_id(asset, deferred=deferred)


def active_catalog_id(asset_browser: bpy.types.Area) -> str:
    """Return the ID of the catalog shown in the asset browser."""
    return params(asset_browser).catalog_id


def params(asset_browser: bpy.types.Area) -> bpy.types.FileAssetSelectParams:
    """Return the asset browser parameters given its Area."""
    space_data = asset_browser.spaces[0]
    assert asset_utils.SpaceAssetInfo.is_asset_browser(space_data)
    return space_data.params


def tag_redraw(screen: bpy.types.Screen) -> None:
    """Tag all asset browsers for redrawing."""

    for area in suitable_areas(screen):
        area.tag_redraw()
