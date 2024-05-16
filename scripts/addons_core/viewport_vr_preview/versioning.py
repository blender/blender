# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Update Blender version this action map was written in:
#
# When the version is ``(0, 0, 0)``, the action map being loaded didn't contain any versioning information.
# This will older than ``(3, 0, 0)``.

def actionconfig_update(actionconfig_data, actionconfig_version):
    from bpy.app import version_file as blender_version
    if actionconfig_version >= blender_version:
        return actionconfig_data

# Version the action map.
##    import copy
##    has_copy = False
##
# if actionconfig_version <= (3, 0, 0):
# Only copy once.
# if not has_copy:
##            actionconfig_data = copy.deepcopy(actionconfig_data)
##            has_copy = True
##
# for (am_name, am_content) in actionconfig_data:
# Apply action map updates.
##
##            am_items = am_content["items"]
##
# for (ami_name, ami_args, ami_data, ami_content) in am_items
# Apply action map item updates.
##
##                ami_bindings = ami_content["bindings"]
##
# for (amb_name, amb_args) in ami_bindings:
# Apply action map binding updates.

    return actionconfig_data
