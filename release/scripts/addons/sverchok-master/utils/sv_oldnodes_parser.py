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

import os
import ast


def get_old_nodes_list(path):
    for fp in os.listdir(path):
        if fp.endswith(".py") and not fp.startswith('__'):
            yield fp

def get_sv_nodeclasses(path, old_node_file):
    collection = []
    file_path = os.path.join(path, old_node_file)
    with open(file_path, errors='replace') as file:
        
        node = ast.parse(file.read())
        classes = [n for n in node.body if isinstance(n, ast.ClassDef)]
        for c in classes:
            for k in c.bases:
                if hasattr(k, 'id') and k.id == 'SverchCustomTreeNode':
                    collection.append([c.name, old_node_file])

    return collection

def get_old_node_bl_idnames(path):
    bl_dict = {}
    for old_node_file in get_old_nodes_list(path):
        items = get_sv_nodeclasses(path, old_node_file)
        for bl_idname, file_name in items:
            bl_dict[bl_idname] = file_name[:-3]
    # print('old nodes dict:', len(bl_dict))
    # print(bl_dict)
    return bl_dict
