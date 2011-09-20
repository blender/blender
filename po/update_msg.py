# $Id$
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
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Write out messages.txt from blender

# Execite:
#   blender --background --python po/update_msg.py

import os

CURRENT_DIR = os.path.dirname(__file__)
SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.join(CURRENT_DIR, "..")))

FILE_NAME_MESSAGES = os.path.join(CURRENT_DIR, "messages.txt")


def dump_messages():
    import bpy

    # -------------------------------------------------------------------------
    # Function definitions

    def _putMessage(messages, msg):
        if len(msg):
            messages[msg] = True

    def _walkProperties(properties, messages):
        import bpy
        for prop in properties:
            _putMessage(messages, prop.name)
            _putMessage(messages, prop.description)

            if isinstance(prop, bpy.types.EnumProperty):
                for item in prop.enum_items:
                    _putMessage(messages, item.name)
                    _putMessage(messages, item.description)

    def _walkRNA(bl_rna, messages):
        if bl_rna.name and bl_rna.name != bl_rna.identifier:
            _putMessage(messages, bl_rna.name)

        if bl_rna.description:
            _putMessage(messages, bl_rna.description)

        _walkProperties(bl_rna.properties, messages)

    def _walkClass(cls, messages):
        _walkRNA(cls.bl_rna, messages)

    def _walk_keymap_hierarchy(hier, messages):
        for lvl in hier:
            _putMessage(messages, lvl[0])

            if lvl[3]:
                _walk_keymap_hierarchy(lvl[3], messages)

    # -------------------------------------------------------------------------
    # Dump Messages

    messages = {}

    for cls in type(bpy.context).__base__.__subclasses__():
        _walkClass(cls, messages)

    for cls in bpy.types.Space.__subclasses__():
        _walkClass(cls, messages)

    for cls in bpy.types.Operator.__subclasses__():
        _walkClass(cls, messages)

    from bl_ui.space_userpref_keymap import KM_HIERARCHY

    _walk_keymap_hierarchy(KM_HIERARCHY, messages)

    message_file = open(FILE_NAME_MESSAGES, 'w')
    message_file.writelines("\n".join(messages))
    message_file.close()
    print("Written %d messages to: %r" % (len(messages), FILE_NAME_MESSAGES))

    # XXX. what is this supposed to do, we wrote the file already???
    _walkClass(bpy.types.SpaceDopeSheetEditor, messages)

    return {'FINISHED'}


def main():

    try:
        import bpy
    except ImportError:
        print("This script must run from inside blender")
        return

    dump_messages()


if __name__ == "__main__":
    print("\n\n *** Running %r *** \n" % __file__)
    main()
