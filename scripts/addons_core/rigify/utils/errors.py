# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later


class MetarigError(Exception):
    """ Exception raised for errors.
    """
    def __init__(self, message):
        self.message = message

    def __str__(self):
        return repr(self.message)


class RaiseErrorMixin(object):
    base_bone: str

    def raise_error(self, message: str, *args, **kwargs):
        from .naming import strip_org

        message = message.format(*args, **kwargs)

        if hasattr(self, 'base_bone'):
            message = "Bone '%s': %s" % (strip_org(self.base_bone), message)

        raise MetarigError("RIGIFY ERROR: " + message)
