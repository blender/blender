# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from typing import List, Dict, Any


class Extension:
    """Container for extensions. Allows to specify requiredness"""
    extension = True  # class method used to check Extension class at traversal (after reloading script, isinstance is not working)

    def __init__(self, name: str, extension: Dict[str, Any], required: bool = True):
        self.name = name
        self.extension = extension
        self.required = required


class ChildOfRootExtension(Extension):
    """Container object for extensions that should be appended to the root extensions"""

    def __init__(self, path: List[str], name: str, extension: Dict[str, Any], required: bool = True):
        """
        Wrap a local extension entity into an object that will later be inserted into a root extension and converted
        to a reference.
        :param path: The path of the extension object in the root extension. E.g. ['lights'] for
        KHR_lights_punctual. Must be a path to a list in the extensions dict.
        :param extension: The data that should be placed into the extension list
        """
        self.path = path
        super().__init__(name, extension, required)
