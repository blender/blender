# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

class MutatingArgument:
    """simple wrapper to pass a value by reference"""

    def __init__(self, value):
        self.value = value


def import_user_extensions(hook_name, gltf, *args):
    for extension in gltf.import_user_extensions:
        hook = getattr(extension, hook_name, None)
        if hook is not None:
            try:
                hook(*args, gltf)
            except Exception as e:
                import traceback
                if getattr(extension, 'is_critical', False):
                    gltf.log.error(
                        "Critical extension hook " +
                        hook_name +
                        " fails on " +
                        extension.__module__ +
                        ": " +
                        str(e),
                        popup=True)
                    traceback.print_exc()
                    raise RuntimeError("Import aborted due to critical extension failure") from e
                else:
                    gltf.log.error(hook_name, "fails on", extension)
                    gltf.log.error(str(e))
                    traceback.print_exc()
