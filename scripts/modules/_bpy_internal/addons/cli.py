# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Implementation of blender's command line ``--addons`` argument,
e.g. ``--addons a,b,c`` to enable add-ons.
"""

__all__ = (
    "set_from_cli",
)


def set_from_cli(addons_as_string):
    from addon_utils import (
        check,
        check_extension,
        enable,
        extensions_refresh,
    )
    addon_modules = addons_as_string.split(",")
    addon_modules_extensions = [m for m in addon_modules if check_extension(m)]
    addon_modules_extensions_has_failure = False

    if addon_modules_extensions:
        extensions_refresh(
            ensure_wheels=True,
            addon_modules_pending=addon_modules_extensions,
        )

    for m in addon_modules:
        if check(m)[1] is False:
            if enable(m, persistent=True, refresh_handled=True) is None:
                if check_extension(m):
                    addon_modules_extensions_has_failure = True

    # Re-calculate wheels if any extensions failed to be enabled.
    if addon_modules_extensions_has_failure:
        extensions_refresh(
            ensure_wheels=True,
        )
