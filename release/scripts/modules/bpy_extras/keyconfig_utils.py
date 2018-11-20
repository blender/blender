# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>


# -----------------------------------------------------------------------------
# Add-on helpers to properly (un)register their own keymaps.

def addon_keymap_register(keymap_data):
    """
    Register a set of keymaps for addons using a list of keymaps.

    See 'blender_defaults.py' for examples of the format this takes.
    """
    import bpy
    wm = bpy.context.window_manager

    from bl_keymap_utils.io import keymap_init_from_data

    kconf = wm.keyconfigs.addon
    if not kconf:
        return  # happens in background mode...
    for km_name, km_args, km_content in keymap_data:
        km_space_type = km_args["space_type"]
        km_region_type = km_args["region_type"]
        km_modal = km_args.get("modal", False)
        kmap = next(iter(
            k for k in kconf.keymaps
            if k.name == km_name and
            k.region_type == km_region_type and
            k.space_type == km_space_type and
            k.is_modal == km_modal
        ), None)
        if kmap is None:
            kmap = kconf.keymaps.new(km_name, **km_args)
        keymap_init_from_data(kmap, km_content["items"], is_modal=km_modal)

def addon_keymap_unregister(keymap_data):
    """
    Unregister a set of keymaps for addons.
    """
    # NOTE: We must also clean up user keyconfig, else, if user has customized one of add-on's shortcut, this
    #       customization remains in memory, and comes back when re-enabling the addon, causing a segfault... :/
    import bpy
    wm = bpy.context.window_manager

    kconfs = wm.keyconfigs
    for kconf in (kconfs.user, kconfs.addon):
        for km_name, km_args, km_content in keymap_data:
            km_space_type = km_args["space_type"]
            km_region_type = km_args["region_type"]
            km_modal = km_args.get("modal", False)
            kmaps = (
                k for k in kconf.keymaps
                if k.name == km_name and
                k.region_type == km_region_type and
                k.space_type == km_space_type and
                k.is_modal == km_modal
            )
            for kmap in kmaps:
                for kmi_idname, _, _ in km_content["items"]:
                    for kmi in kmap.keymap_items:
                        if kmi.idname == kmi_idname:
                            kmap.keymap_items.remove(kmi)
            # NOTE: We won't remove addons keymaps themselves, other addons might also use them!


# -----------------------------------------------------------------------------
# Utility Functions

def keyconfig_test(kc):

    def testEntry(kc, entry, src=None, parent=None):
        result = False

        idname, spaceid, regionid, children = entry

        km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)

        if km:
            km = km.active()
            is_modal = km.is_modal

            if src:
                for item in km.keymap_items:
                    if src.compare(item):
                        print("===========")
                        print(parent.name)
                        print(_kmistr(src, is_modal).strip())
                        print(km.name)
                        print(_kmistr(item, is_modal).strip())
                        result = True

                for child in children:
                    if testEntry(kc, child, src, parent):
                        result = True
            else:
                for i in range(len(km.keymap_items)):
                    src = km.keymap_items[i]

                    for child in children:
                        if testEntry(kc, child, src, km):
                            result = True

                    for j in range(len(km.keymap_items) - i - 1):
                        item = km.keymap_items[j + i + 1]
                        if src.compare(item):
                            print("===========")
                            print(km.name)
                            print(_kmistr(src, is_modal).strip())
                            print(_kmistr(item, is_modal).strip())
                            result = True

                for child in children:
                    if testEntry(kc, child):
                        result = True

        return result

    # -------------------------------------------------------------------------
    # Function body

    from bl_keymap_utils import keymap_hierarchy
    result = False
    for entry in keymap_hierarchy.generate():
        if testEntry(kc, entry):
            result = True
    return result
