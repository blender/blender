#  ***** BEGIN GPL LICENSE BLOCK *****
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
#  along with this program; if not, see <http://www.gnu.org/licenses/>
#  and write to the Free Software Foundation, Inc., 51 Franklin Street,
#  Fifth Floor, Boston, MA  02110-1301, USA..
#
#  The Original Code is Copyright (C) 2013-2014 by Gorodetskiy Nikita  ###
#  All rights reserved.
#
#  Contact:      sverchok-b3d@ya.ru    ###
#  Information:  http://nikitron.cc.ua/sverchok_en.html   ###
#
#  The Original Code is: all of this file.
#
#  Contributor(s):
#     Nedovizin Alexander (aka Cfyzzz)
#     Gorodetskiy Nikita (aka Nikitron)
#     Linus Yng (aka Ly29)
#     Agustin Jimenez (aka AgustinJB)
#     Dealga McArdle (aka zeffii)
#     Konstantin Vorobiew (aka Kosvor)
#     Ilya Portnov (aka portnov)
#     Eleanor Howick (aka elfnor)
#     Walter Perdan (aka kalwalt)
#     Marius Giurgi (aka DolphinDream)
#
#  ***** END GPL LICENSE BLOCK *****
#
# -*- coding: utf-8 -*-

bl_info = {
    "name": "Sverchok",
    "author": (
        "sverchok-b3d@ya.ru, "
        "Cfyzzz, Nikitron, Ly29, "
        "AgustinJB, Zeffii, Kosvor, "
        "Portnov, Elfnor, kalwalt, DolphinDream"
    ),
    "version": (0, 5, 9, 6),
    "blender": (2, 7, 8),
    "location": "Nodes > CustomNodesTree > Add user nodes",
    "description": "Parametric node-based geometry programming",
    "warning": "",
    "wiki_url": "http://nikitron.cc.ua/sverch/html/main.html",
    "tracker_url": "http://www.blenderartists.org/forum/showthread.php?272679",
    "category": "Node"
}


import sys
import importlib

# pylint: disable=E0602
# pylint: disable=C0413
# pylint: disable=C0412

# make sverchok the root module name, (if sverchok dir not named exactly "sverchok") 
if __name__ != "sverchok":
    sys.modules["sverchok"] = sys.modules[__name__]

from sverchok.core import sv_registration_utils, init_architecture, make_node_list
from sverchok.core import reload_event, handle_reload_event
from sverchok.utils import utils_modules
from sverchok.ui import ui_modules


imported_modules = init_architecture(__name__, utils_modules, ui_modules)
node_list = make_node_list(nodes)

if "bpy" in locals():
    reload_event = True
    node_list = handle_reload_event(nodes, imported_modules, old_nodes) 


import bpy
import sverchok


def register():
    sv_registration_utils.register_all(imported_modules + node_list)
    sverchok.core.init_bookkeeping(__name__)

    menu.register()
    if reload_event:
        data_structure.RELOAD_EVENT = True
        menu.reload_menu()


def unregister():
    sverchok.utils.clear_node_classes()
    sv_registration_utils.unregister_all(imported_modules + node_list)

# EOF
