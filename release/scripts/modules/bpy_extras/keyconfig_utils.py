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


def _km_expand_from_toolsystem(space_type, context_mode):
    def _fn():
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        for cls in ToolSelectPanelHelper.__subclasses__():
            if cls.bl_space_type == space_type:
                return cls.keymap_ui_hierarchy(context_mode)
        raise Exception("keymap not found")
    return _fn


def _km_hierarchy_iter_recursive(items):
    for sub in items:
        if callable(sub):
            yield from sub()
        else:
            yield (*sub[:3], list(_km_hierarchy_iter_recursive(sub[3])))


def km_hierarchy():
    return list(_km_hierarchy_iter_recursive(_km_hierarchy))


# bpy.type.KeyMap: (km.name, km.space_type, km.region_type, [...])

#    ('Script', 'EMPTY', 'WINDOW', []),


# Access via 'km_hierarchy'.
_km_hierarchy = [
    ('Window', 'EMPTY', 'WINDOW', []),  # file save, window change, exit
    ('Screen', 'EMPTY', 'WINDOW', [     # full screen, undo, screenshot
        ('Screen Editing', 'EMPTY', 'WINDOW', []),    # re-sizing, action corners
        ('Header', 'EMPTY', 'WINDOW', []),            # header stuff (per region)
    ]),

    ('View2D', 'EMPTY', 'WINDOW', []),    # view 2d navigation (per region)
    ('View2D Buttons List', 'EMPTY', 'WINDOW', []),  # view 2d with buttons navigation

    ('User Interface', 'EMPTY', 'WINDOW', []),

    ('3D View', 'VIEW_3D', 'WINDOW', [  # view 3d navigation and generic stuff (select, transform)
        ('Object Mode', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'OBJECT'),
        ]),
        ('Mesh', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_MESH'),
        ]),
        ('Curve', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_CURVE'),
        ]),
        ('Armature', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_ARMATURE'),
        ]),
        ('Metaball', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_METABALL'),
        ]),
        ('Lattice', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_LATTICE'),
        ]),
        ('Font', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'EDIT_TEXT'),
        ]),

        ('Pose', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'POSE'),
        ]),

        ('Vertex Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_VERTEX'),
        ]),
        ('Weight Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_WEIGHT'),
        ]),
        ('Weight Paint Vertex Selection', 'EMPTY', 'WINDOW', []),
        ('Face Mask', 'EMPTY', 'WINDOW', []),
        # image and view3d
        ('Image Paint', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PAINT_TEXTURE'),
        ]),
        ('Sculpt', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'SCULPT'),
        ]),

        ('Particle', 'EMPTY', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', 'PARTICLE'),
        ]),

        ('Knife Tool Modal Map', 'EMPTY', 'WINDOW', []),
        ('Custom Normals Modal Map', 'EMPTY', 'WINDOW', []),
        ('Paint Stroke Modal', 'EMPTY', 'WINDOW', []),
        ('Paint Curve', 'EMPTY', 'WINDOW', []),

        ('Object Non-modal', 'EMPTY', 'WINDOW', []),  # mode change

        ('View3D Walk Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Fly Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Rotate Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Move Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Zoom Modal', 'EMPTY', 'WINDOW', []),
        ('View3D Dolly Modal', 'EMPTY', 'WINDOW', []),

        # toolbar and properties
        ('3D View Generic', 'VIEW_3D', 'WINDOW', [
            _km_expand_from_toolsystem('VIEW_3D', None),
        ]),
    ]),

    ('Graph Editor', 'GRAPH_EDITOR', 'WINDOW', [
        ('Graph Editor Generic', 'GRAPH_EDITOR', 'WINDOW', []),
    ]),
    ('Dopesheet', 'DOPESHEET_EDITOR', 'WINDOW', [
        ('Dopesheet Generic', 'DOPESHEET_EDITOR', 'WINDOW', []),
    ]),
    ('NLA Editor', 'NLA_EDITOR', 'WINDOW', [
        ('NLA Channels', 'NLA_EDITOR', 'WINDOW', []),
        ('NLA Generic', 'NLA_EDITOR', 'WINDOW', []),
    ]),
    ('Timeline', 'TIMELINE', 'WINDOW', []),

    ('Image', 'IMAGE_EDITOR', 'WINDOW', [
        ('UV Editor', 'EMPTY', 'WINDOW', []),  # image (reverse order, UVEdit before Image)
        ('Image Paint', 'EMPTY', 'WINDOW', []),  # image and view3d
        ('UV Sculpt', 'EMPTY', 'WINDOW', []),
        ('Image Generic', 'IMAGE_EDITOR', 'WINDOW', [
            _km_expand_from_toolsystem('IMAGE_EDITOR', None),
        ]),
    ]),

    ('Outliner', 'OUTLINER', 'WINDOW', []),

    ('Node Editor', 'NODE_EDITOR', 'WINDOW', [
        ('Node Generic', 'NODE_EDITOR', 'WINDOW', []),
    ]),
    ('Sequencer', 'SEQUENCE_EDITOR', 'WINDOW', [
        ('SequencerCommon', 'SEQUENCE_EDITOR', 'WINDOW', []),
        ('SequencerPreview', 'SEQUENCE_EDITOR', 'WINDOW', []),
    ]),

    ('File Browser', 'FILE_BROWSER', 'WINDOW', [
        ('File Browser Main', 'FILE_BROWSER', 'WINDOW', []),
        ('File Browser Buttons', 'FILE_BROWSER', 'WINDOW', []),
    ]),

    ('Info', 'INFO', 'WINDOW', []),

    ('Property Editor', 'PROPERTIES', 'WINDOW', []),  # align context menu

    ('Text', 'TEXT_EDITOR', 'WINDOW', [
        ('Text Generic', 'TEXT_EDITOR', 'WINDOW', []),
    ]),
    ('Console', 'CONSOLE', 'WINDOW', []),
    ('Clip', 'CLIP_EDITOR', 'WINDOW', [
        ('Clip Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Graph Editor', 'CLIP_EDITOR', 'WINDOW', []),
        ('Clip Dopesheet Editor', 'CLIP_EDITOR', 'WINDOW', []),
    ]),

    ('Grease Pencil', 'EMPTY', 'WINDOW', [  # grease pencil stuff (per region)
        ('Grease Pencil Stroke Edit Mode', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint (Draw brush)', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint (Fill)', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint (Erase)', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Paint Mode', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Sculpt Mode', 'EMPTY', 'WINDOW', []),
        ('Grease Pencil Stroke Weight Mode', 'EMPTY', 'WINDOW', []),
    ]),
    ('Mask Editing', 'EMPTY', 'WINDOW', []),
    ('Frames', 'EMPTY', 'WINDOW', []),    # frame navigation (per region)
    ('Markers', 'EMPTY', 'WINDOW', []),    # markers (per region)
    ('Animation', 'EMPTY', 'WINDOW', []),    # frame change on click, preview range (per region)
    ('Animation Channels', 'EMPTY', 'WINDOW', []),

    ('View3D Gesture Circle', 'EMPTY', 'WINDOW', []),
    ('Gesture Straight Line', 'EMPTY', 'WINDOW', []),
    ('Gesture Zoom Border', 'EMPTY', 'WINDOW', []),
    ('Gesture Box', 'EMPTY', 'WINDOW', []),

    ('Standard Modal Map', 'EMPTY', 'WINDOW', []),
    ('Transform Modal Map', 'EMPTY', 'WINDOW', []),
    ('Eyedropper Modal Map', 'EMPTY', 'WINDOW', []),
    ('Eyedropper ColorBand PointSampling Map', 'EMPTY', 'WINDOW', []),
]


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

    result = False
    for entry in km_hierarchy():
        if testEntry(kc, entry):
            result = True
    return result
