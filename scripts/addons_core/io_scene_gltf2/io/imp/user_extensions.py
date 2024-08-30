# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

def import_user_extensions(hook_name, gltf, *args):
    for extension in gltf.import_user_extensions:
        hook = getattr(extension, hook_name, None)
        if hook is not None:
            try:
                hook(*args, gltf)
            except Exception as e:
                gltf.log.error(hook_name, "fails on", extension)
                gltf.log.error(str(e))
