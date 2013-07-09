 # ***** BEGIN GPL LICENSE BLOCK *****
 #
 # This program is free software; you can redistribute it and/or
 # modify it under the terms of the GNU General Public License
 # as published by the Free Software Foundation; either version 2
 # of the License, or (at your option) any later version.
 #
 # This program is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 # GNU General Public License for more details.
 #
 # You should have received a copy of the GNU General Public License
 # along with this program; if not, write to the Free Software Foundation,
 # Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 #
 # Contributor(s): Campbell Barton
 #
 # #**** END GPL LICENSE BLOCK #****

# <pep8 compliant>

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
