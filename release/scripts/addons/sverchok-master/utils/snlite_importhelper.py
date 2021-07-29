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
import ast

from sverchok.utils.snlite_utils import get_valid_node


TRIPPLE_QUOTES = '"""'
UNPARSABLE = None, None, None, None

sock_dict = {
    'v': 'VerticesSocket', 's': 'StringsSocket', 'm': 'MatrixSocket', 'o': 'SvObjectSocket'
}


def set_autocolor(node, use_me, color_me):
    node.use_custom_color = use_me
    node.color = color_me


def processed(str_in):
    _, b = str_in.split('=')
    return ast.literal_eval(b)


def parse_socket_line(line):
    lsp = line.strip().split()
    if not len(lsp) in {3, 5}:
        print(line, 'is malformed')
        return UNPARSABLE
    else:
        socket_type = sock_dict.get(lsp[2])
        socket_name = lsp[1]
        if not socket_type:
            return UNPARSABLE
        elif len(lsp) == 3:
            return socket_type, socket_name, None, None
        else:
            default = processed(lsp[3])
            nested = processed(lsp[4])
            return socket_type, socket_name, default, nested

def parse_ui_line(L):
    """ 
    :    the bl_idname is needed only if it isn't ShaderNodeRGBCurve
    :    expects the following format, comma separated

    ui = material_name, node_name
    ui = material_name, node_name, bl_idname

    something like:
    ui = MyMaterial, RGB Curves

    """
    l = L[4:].strip()
    items = [sl.strip() for sl in l.split(',')]
    if len(items) == 2:
        return dict(mat_name=items[0], node_name=items[1], bl_idname='ShaderNodeRGBCurve')
    elif len(items) == 3:
        return dict(mat_name=items[0], node_name=items[1], bl_idname=items[2])


def parse_sockets(node):

    if hasattr(node, 'inject_params'):
        node.inject_params = False

    snlite_info = {
        'inputs': [], 'outputs': [], 
        'snlite_ui': [], 'includes': {},
        'custom_enum': [], 'callbacks': {}
    }

    quotes = 0
    for line in node.script_str.split('\n'):
        L = line.strip()

        if L.startswith(TRIPPLE_QUOTES):
            quotes += 1
            if quotes == 2:
                break

        elif L.startswith('in ') or L.startswith('out '):
            socket_dir = L.split(' ')[0] + 'puts'
            snlite_info[socket_dir].append(parse_socket_line(L))

        elif L.startswith('ui = '):
            ui_dict = parse_ui_line(L)
            if isinstance(ui_dict, dict):
                snlite_info['snlite_ui'].append(ui_dict)

        elif L.startswith('inject'):
            if hasattr(node, 'inject_params'):
                node.inject_params = True

        elif L.startswith('enum ='):
            snlite_info['custom_enum'] = L[6:].strip().split(' ')

        elif L.startswith('include <') and L.endswith('>'):
            filename = L[9:-1]
            file = bpy.data.texts.get(filename)
            if file:
                snlite_info['includes'][filename] = file.as_string()

        elif L in {'fh', 'filehandler'}:
            snlite_info['display_file_handler'] = True

        # elif L.startswith('cb '):
        #     cb_name = L[3:].strip()
        #     snlite_info['callbacks'].append(cb_name)

    return snlite_info


def are_matched(sock_, socket_description):
    return (sock_.bl_idname, sock_.name) == socket_description[:2]


def get_rgb_curve(material_name, node_name):
    '''
    ShaderNodeRGBCurve support only
    '''
    m = bpy.data.materials.get(material_name)
    node = m.node_tree.nodes.get(node_name)

    out_list = []
    for curve in node.mapping.curves:
        points = curve.points
        out_list.append([(p.handle_type, p.location[:]) for p in points])
    
    x = 'ShaderNodeRGBCurve'
    return dict(mat_name=material_name, node_name=node_name, bl_idname=x, data=out_list)


def set_rgb_curve(data_dict):
    '''
    ShaderNodeRGBCurve support only
    '''

    mat_name = data_dict['mat_name']
    node_name = data_dict['node_name']
    bl_id = data_dict['bl_idname']

    node = get_valid_node(mat_name, node_name, bl_id)

    node.mapping.initialize()
    data = data_dict['data']
    for idx, curve in enumerate(node.mapping.curves):

        # add extra points, if needed
        extra = len(data[idx]) - len(curve.points)
        _ = [curve.points.new(0.5, 0.5) for _ in range(extra)]

        # set points to correspond with stored collection
        for pidx, (handle_type, location) in enumerate(data[idx]):
            curve.points[pidx].handle_type = handle_type
            curve.points[pidx].location = location
    node.mapping.update()
