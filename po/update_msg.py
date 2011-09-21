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


def dump_messages_rna(messages):
    import bpy
    # -------------------------------------------------------------------------
    # Function definitions

    def walkProperties(properties):
        import bpy
        for prop in properties:
            messages.add(prop.name)
            messages.add(prop.description)

            if isinstance(prop, bpy.types.EnumProperty):
                for item in prop.enum_items:
                    messages.add(item.name)
                    messages.add(item.description)

    def walkRNA(bl_rna):
        if bl_rna.name and bl_rna.name != bl_rna.identifier:
            messages.add(bl_rna.name)

        if bl_rna.description:
            messages.add(bl_rna.description)

        walkProperties(bl_rna.properties)

    def walkClass(cls):
        walkRNA(cls.bl_rna)

    def walk_keymap_hierarchy(hier):
        for lvl in hier:
            messages.add(lvl[0])

            if lvl[3]:
                walk_keymap_hierarchy(lvl[3])

    # -------------------------------------------------------------------------
    # Dump Messages

    for cls in type(bpy.context).__base__.__subclasses__():
        walkClass(cls)

    for cls in bpy.types.Space.__subclasses__():
        walkClass(cls)

    for cls in bpy.types.Operator.__subclasses__():
        walkClass(cls)

    from bl_ui.space_userpref_keymap import KM_HIERARCHY

    walk_keymap_hierarchy(KM_HIERARCHY)
    

    ## XXX. what is this supposed to do, we wrote the file already???
    #_walkClass(bpy.types.SpaceDopeSheetEditor)


def dump_messages():
    messages = {""}

    dump_messages_rna(messages)

    messages.remove("")

    message_file = open(FILE_NAME_MESSAGES, 'w', encoding="utf8")
    message_file.writelines("\n".join(sorted(messages)))
    message_file.close()

    print("Written %d messages to: %r" % (len(messages), FILE_NAME_MESSAGES))

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
