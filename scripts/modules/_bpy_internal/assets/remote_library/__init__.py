# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations


# The function below is (un)registered from scripts/addons_core/bl_pkg/__init__.py:
def asset_listing_main(args: list[str]) -> int:
    """Run the `blender -c asset_listing` CLI command.

    This is late-importing the cli module, so that it (and its
    dependencies) are only imported when actually used.
    """
    import traceback
    from . import cli

    try:
        cli.main(args)
    except SystemExit as ex:
        if isinstance(ex.code, int):
            return ex.code
        return 2
    except BaseException:
        traceback.print_exc()
        return 1
    return 0
