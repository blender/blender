# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import json
import bpy


class BlenderJSONEncoder(json.JSONEncoder):
    """Blender JSON Encoder."""

    def default(self, obj):
        if isinstance(obj, bpy.types.ID):
            return dict(
                name=obj.name,
                type=obj.__class__.__name__
            )
        return super(BlenderJSONEncoder, self).default(obj)


def is_json_convertible(data):
    """Test, if a data set can be expressed as JSON."""
    try:
        json.dumps(data, cls=BlenderJSONEncoder)
        return True
    except:
        return False
