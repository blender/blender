# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from urllib.parse import unquote, quote
from os.path import normpath
from os import sep


def uri_to_path(uri):
    uri = uri.replace('\\', '/')  # Some files come with \\ as dir separator
    uri = unquote(uri)
    return normpath(uri)


def path_to_uri(path):
    path = normpath(path)
    path = path.replace(sep, '/')
    return quote(path)
