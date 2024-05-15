# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
High level API for managing an extension local site-packages and wheels.

NOTE: this is a standalone module.
"""

__all__ = (
    "sync",
)


import os
import sys

from .wheel_manager import WheelSource

from typing import (
    List,
)


def sync(
        *,
        local_dir: str,
        wheel_list: List[WheelSource],
) -> None:
    from . import wheel_manager
    local_dir_site_packages = os.path.join(
        local_dir,
        "lib",
        "python{:d}.{:d}".format(sys.version_info.major, sys.version_info.minor),
        "site-packages",
    )

    wheel_manager.apply_action(
        local_dir=local_dir,
        local_dir_site_packages=local_dir_site_packages,
        wheel_list=wheel_list,
    )
    if os.path.exists(local_dir_site_packages):
        if local_dir_site_packages not in sys.path:
            sys.path.append(local_dir_site_packages)
