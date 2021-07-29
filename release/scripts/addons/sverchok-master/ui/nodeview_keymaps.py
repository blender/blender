# -*- coding: utf-8 -*-
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

import bpy

nodeview_keymaps = []

def add_keymap():
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc:
        make_monad = 'node.sv_monad_from_selected'

        km = kc.keymaps.new(name='Node Editor', space_type='NODE_EDITOR')

        # ctrl+G        | make group from selected with periphery links
        kmi = km.keymap_items.new(make_monad, 'G', 'PRESS', ctrl=True)
        nodeview_keymaps.append((km, kmi))

        # ctrl+shift+G  | make group from selected without periphery links
        kmi = km.keymap_items.new(make_monad, 'G', 'PRESS', ctrl=True, shift=True)
        kmi.properties.use_relinking = False
        nodeview_keymaps.append((km, kmi))

        # TAB           | enter or exit monad depending on selection and edit_tree type
        kmi = km.keymap_items.new('node.sv_monad_enter', 'TAB', 'PRESS')
        nodeview_keymaps.append((km, kmi))

        # alt + G       | expand current monad into original state
        kmi = km.keymap_items.new('node.sv_monad_expand', 'G', 'PRESS', alt=True)
        nodeview_keymaps.append((km, kmi))

        # Shift + A     | show custom menu
        kmi = km.keymap_items.new('wm.call_menu', 'A', 'PRESS', shift=True)
        kmi.properties.name = "NODEVIEW_MT_Dynamic_Menu"
        nodeview_keymaps.append((km, kmi))

        # ctrl + Space  | enter extra search operator
        kmi = km.keymap_items.new('node.sv_extra_search', 'SPACE', 'PRESS', ctrl=True)
        nodeview_keymaps.append((km, kmi))

        # Right Click   | show custom menu
        kmi = km.keymap_items.new('wm.call_menu', 'RIGHTMOUSE', 'CLICK')
        kmi.properties.name = "NODEVIEW_MT_sv_rclick_menu"
        nodeview_keymaps.append((km, kmi))


def remove_keymap():

    for km, kmi in nodeview_keymaps:
        try:
            km.keymap_items.remove(kmi)
        except Exception as e:
            err = repr(e)
            if "cannot be removed from 'Node Editor'" in err:
                print('keymaps for Node Editor already removed by another add-on, sverchok will skip this step in unregister')
                break

    nodeview_keymaps.clear()


def register():
    add_keymap()

def unregister():
    remove_keymap()
