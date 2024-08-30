# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

def export_user_extensions(hook_name, export_settings, *args):
    if args and hasattr(args[0], "extensions"):
        if args[0].extensions is None:
            args[0].extensions = {}

    for extension in export_settings['gltf_user_extensions']:
        hook = getattr(extension, hook_name, None)
        if hook is not None:
            try:
                hook(*args, export_settings)
            except Exception as e:
                export_settings['log'].error("Extension hook " + hook_name + " fails on " + extension)
                export_settings['log'].error(str(e))
