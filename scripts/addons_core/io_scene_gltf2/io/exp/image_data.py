# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import re


class ImageData:
    """Contains encoded images"""
    # FUTURE_WORK: as a method to allow the node graph to be better supported, we could model some of
    # the node graph elements with numpy functions

    def __init__(self, data: bytes, mime_type: str, name: str):
        self._data = data
        self._mime_type = mime_type
        self._name = name
        self._adjusted_name = None
        self._uri = None

    def __eq__(self, other):
        return self._data == other.data

    def __hash__(self):
        return hash(self._data)

    def adjusted_name(self):
        regex_dot = re.compile(r"\.")
        adjusted_name = re.sub(regex_dot, "_", self.name)
        new_name = "".join([char for char in adjusted_name if char not in r"!#$&'()*+,/:;<>?@[\]^`{|}~"])
        return new_name

    @property
    def data(self):
        return self._data

    @property
    def name(self):
        return self._name

    @property
    def file_extension(self):
        if self._mime_type == "image/jpeg":
            return ".jpg"
        elif self._mime_type == "image/webp":
            return ".webp"
        return ".png"

    @property
    def byte_length(self):
        return len(self._data)

    @property
    def uri(self):
        return self._uri

    @uri.setter
    def uri(self, uri):
        self._uri = uri


    def set_adjusted_name(self, names):
        # Set adjusted name
        name = self.name
        count = 1
        regex = re.compile(r"-\d+$")
        while name + self.file_extension in names:
            regex_found = re.findall(regex, name)
            if regex_found:
                name = re.sub(regex, "-" + str(count), name)
            else:
                name += "-" + str(count)

            count += 1
        # TODO: allow embedding of images (base64)
        self._adjusted_name = name + self.file_extension
        return self._adjusted_name

    @property
    def adjusted_name(self):
        return self._adjusted_name
