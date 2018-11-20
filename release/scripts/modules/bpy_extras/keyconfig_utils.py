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

# Example of keymaps_description:
keymaps_description_doc = """
keymaps_description is a tuple (((keymap_description), (tuple of keymap_item_descriptions))).
keymap_description is a tuple (name, space_type, region_type, is_modal).
keymap_item_description is a tuple ({kw_args_for_keymap_new}, (tuple of properties)).
kw_args_for_keymap_new is a mapping which keywords match parameters of keymap.new() function.
tuple of properties is a tuple of pairs (prop_name, prop_value) (properties being those of called operator).

Example:

KEYMAPS = (
    # First, keymap identifiers (last bool is True for modal km).
    (('Sequencer', 'SEQUENCE_EDITOR', 'WINDOW', False), (
    # Then a tuple of keymap items, defined by a dict of kwargs for the km new func, and a tuple of tuples (name, val)
    # for ops properties, if needing non-default values.
        ({"idname": export_strips.SEQExportStrip.bl_idname, "type": 'P', "value": 'PRESS', "shift": True, "ctrl": True},
         ()),
    )),
)
"""


def addon_keymap_register(wm, keymaps_description):
    """
    Register a set of keymaps for addons.

    """ + keymaps_description_doc
    kconf = wm.keyconfigs.addon
    if not kconf:
        return  # happens in background mode...
    for km_info, km_items in keymaps_description:
        km_name, km_sptype, km_regtype, km_ismodal = km_info
        kmap = [k for k in kconf.keymaps
                if k.name == km_name and k.region_type == km_regtype and
                k.space_type == km_sptype and k.is_modal == km_ismodal]
        if kmap:
            kmap = kmap[0]
        else:
            kmap = kconf.keymaps.new(km_name, region_type=km_regtype, space_type=km_sptype, modal=km_ismodal)
        for kmi_kwargs, props in km_items:
            kmi = kmap.keymap_items.new(**kmi_kwargs)
            kmi.active = True
            for prop, val in props:
                setattr(kmi.properties, prop, val)


def addon_keymap_unregister(wm, keymaps_description):
    """
    Unregister a set of keymaps for addons.

    """ + keymaps_description_doc
    # NOTE: We must also clean up user keyconfig, else, if user has customized one of add-on's shortcut, this
    #       customization remains in memory, and comes back when re-enabling the addon, causing a segfault... :/
    kconfs = wm.keyconfigs
    for kconf in (kconfs.user, kconfs.addon):
        for km_info, km_items in keymaps_description:
            km_name, km_sptype, km_regtype, km_ismodal = km_info
            kmaps = (k for k in kconf.keymaps
                     if k.name == km_name and k.region_type == km_regtype and
                     k.space_type == km_sptype and k.is_modal == km_ismodal)
            for kmap in kmaps:
                for kmi_kwargs, props in km_items:
                    idname = kmi_kwargs["idname"]
                    for kmi in kmap.keymap_items:
                        if kmi.idname == idname:
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
