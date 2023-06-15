# SPDX-FileCopyrightText: 2009-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bpy_types_Operator_bl_property__doc__ = (
    """
The name of a property to use as this operators primary property.
Currently this is only used to select the default property when
expanding an operator into a menu.
:type: string
""")


def main():
    import bpy
    from bpy.types import Operator

    def dummy_func(test):
        pass

    kw_dummy = dict(fget=dummy_func, fset=dummy_func, fdel=dummy_func)

    # bpy registration handles this,
    # but its only checked for and not existing in the base class.
    Operator.bl_property = property(doc=bpy_types_Operator_bl_property__doc__, **kw_dummy)


if __name__ == "__main__":
    main()
