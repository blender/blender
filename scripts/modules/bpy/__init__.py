# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Give access to blender data and utility functions.
"""

__all__ = (
    "app",
    "context",
    "data",
    "msgbus",
    "ops",
    "path",
    "props",
    "types",
    "utils",
)


# Internal Blender C-API modules.
from _bpy import (
    app,
    context,
    data,
    msgbus,
    props,
    types,
)

# python modules
from . import (
    ops,
    path,
    utils,
)


def main():
    import sys

    # Possibly temp. addons path
    from os.path import join, dirname, exists

    # It's unlikely this directory exists.
    # Keep it so users can bundle their own add-ons with app-templates which share modules.
    # Also keep this for consistency with the other `addons` directories.
    # Check this exists because the bundled scripts should not be manipulated at run-time.
    dirpath = join(dirname(dirname(dirname(__file__))), "addons_core", "modules")
    if exists(dirpath):
        sys.path.append(dirpath)

    # Don't check if this exists as it may be created as part of installing add-ons.
    sys.path.append(join(utils.user_resource('SCRIPTS'), "addons", "modules"))

    # fake module to allow:
    #   from bpy.types import Panel
    sys.modules.update({
        "bpy.app": app,
        "bpy.app.handlers": app.handlers,
        "bpy.app.translations": app.translations,
        "bpy.types": types,
    })

    # Denormals are flushed to zero on all threads, which makes numpy (before 2.4)
    # warn when it computes `finfo` for a type. Remove once numpy 2.4+ is shipped.
    # See #164118 for more info.
    # *DEPRECATED* 2028/01/01 (date is arbitrary) we should have bumped to numpy 2.4+ by now.
    if not app.module:
        import warnings
        warnings.filterwarnings(
            "ignore",
            message="The value of the smallest subnormal for",
            category=UserWarning,
            module=r"numpy\._core\.getlimits",
        )

    # Initializes Python classes.
    # (good place to run a profiler or trace).
    # Postpone loading `extensions` scripts (add-ons & app-templates),
    # until after the key-maps have been initialized.
    utils.load_scripts(extensions=False)


main()

del main
