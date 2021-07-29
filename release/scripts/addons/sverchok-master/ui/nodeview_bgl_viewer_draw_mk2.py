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
import pprint
import re
from collections import defaultdict

import bpy
import blf
import bgl
from bpy.types import SpaceNodeEditor

from sverchok import node_tree


callback_dict = {}
point_dict = {}


def adjust_list(in_list, x, y):
    return [[old_x + x, old_y + y] for (old_x, old_y) in in_list]


def parse_socket(socket, rounding, element_index, view_by_element, props):

    data = socket.sv_get(deepcopy=False)
    num_data_items = len(data)
    if num_data_items > 0 and view_by_element:
        if element_index < num_data_items:
            data = data[element_index]

    str_width = props.line_width

    # okay, here we should be more clever and extract part of the list
    # to avoid the amount of time it take to format it.
    
    content_str = pprint.pformat(data, width=str_width, depth=props.depth, compact=props.compact)
    content_array = content_str.split('\n')

    if len(content_array) > 20:
        ''' first 10, ellipses, last 10 '''
        ellipses = ['... ... ...']
        head = content_array[0:10]
        tail = content_array[-10:]
        display_text = head + ellipses + tail
    elif len(content_array) == 1:
        ''' split on subunit - case of no newline to split on. '''
        content_array = content_array[0].replace("), (", "),\n (")
        display_text = content_array.split("\n")
    else:
        display_text = content_array

    # http://stackoverflow.com/a/7584567/1243487
    rounded_vals = re.compile(r"\d*\.\d+")

    def mround(match):
        format_string = "{{:.{0}g}}".format(rounding)
        return format_string.format(float(match.group()))

    out = []
    for line in display_text:
        out.append(re.sub(rounded_vals, mround, line) if not "bpy." in line else line)
    return out


## end of util functions


def tag_redraw_all_nodeviews():

    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == 'NODE_EDITOR':
                for region in area.regions:
                    if region.type == 'WINDOW':
                        region.tag_redraw()
   

def callback_enable(*args):
    n_id = args[0]
    global callback_dict
    if n_id in callback_dict:
        return

    handle_pixel = SpaceNodeEditor.draw_handler_add(draw_callback_px, args, 'WINDOW', 'POST_VIEW')
    callback_dict[n_id] = handle_pixel
    tag_redraw_all_nodeviews()


def callback_disable(n_id):
    global callback_dict
    handle_pixel = callback_dict.get(n_id, None)
    if not handle_pixel:
        return
    SpaceNodeEditor.draw_handler_remove(handle_pixel, 'WINDOW')
    del callback_dict[n_id]
    tag_redraw_all_nodeviews()


def callback_disable_all():
    global callback_dict
    temp_list = list(callback_dict.keys())
    for n_id in temp_list:
        if n_id:
            callback_disable(n_id)


def draw_text_data(data):
    lines = data.get('content', 'no data')
    x, y = data.get('location', (120, 120))
    x, y = int(x), int(y)
    color = data.get('color', (0.1, 0.1, 0.1))
    font_id = data.get('font_id', 0)
    scale = data.get('scale', 1.0)
    
    text_height = 15 * scale
    line_height = 14 * scale

    # why does the text look so jagged?  <-- still valid question
    # dpi = bpy.context.user_preferences.system.dpi
    blf.size(font_id, int(text_height), 72)
    bgl.glColor3f(*color)
    ypos = y

    for line in lines:
        blf.position(0, x, ypos, 0)
        blf.draw(font_id, line)
        ypos -= int(line_height * 1.3)


def draw_rect(x=0, y=0, w=30, h=10, color=(0.0, 0.0, 0.0, 1.0)):

    bgl.glColor4f(*color)       
    bgl.glBegin(bgl.GL_POLYGON)

    for coord in [(x, y), (x+w, y), (w+x, y-h), (x, y-h)]:
        bgl.glVertex2f(*coord)
    bgl.glEnd()

def draw_triangle(x=0, y=0, w=30, h=10, color=(1.0, 0.3, 0.3, 1.0)):

    bgl.glColor4f(*color)       
    bgl.glBegin(bgl.GL_TRIANGLES)

    for coord in [(x, y), (x+w, y), (x + (w/2), y-h)]:
        bgl.glVertex2f(*coord)
    bgl.glEnd()


def draw_graphical_data(data):
    lines = data.get('content')
    x, y = data.get('location', (120, 120))
    color = data.get('color', (0.1, 0.1, 0.1))
    font_id = data.get('font_id', 0)
    scale = data.get('scale', 1.0)
    text_height = 15 * scale

    if not lines:
        return

    blf.size(font_id, int(text_height), 72)
    
    def draw_text(color, xpos, ypos, line):
        bgl.glColor3f(*color)
        blf.position(0, xpos, ypos, 0)
        blf.draw(font_id, line)
        return blf.dimensions(font_id, line)

    lineheight = 20 * scale
    num_containers = len(lines)
    for idx, line in enumerate(lines):
        y_pos = y - (idx*lineheight)
        gfx_x = x

        num_items = str(len(line))
        kind_of_item = type(line).__name__

        tx, _ = draw_text(color, gfx_x, y_pos, "{0} of {1} items".format(kind_of_item, num_items))
        gfx_x += (tx + 5)
        
        content_dict = defaultdict(int)
        for item in line:
            content_dict[type(item).__name__] += 1

        tx, _ = draw_text(color, gfx_x, y_pos, str(dict(content_dict)))
        gfx_x += (tx + 5)

        if idx == 19 and num_containers > 20:
            y_pos = y - ((idx+1)*lineheight)
            text_body = "Showing the first 20 of {0} items"
            draw_text(color, x, y_pos, text_body.format(num_containers))
            break



def restore_opengl_defaults():
    bgl.glLineWidth(1)
    bgl.glDisable(bgl.GL_BLEND)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)        


def draw_callback_px(n_id, data):

    space = bpy.context.space_data
  
    ng_view = space.edit_tree
    # ng_view can be None
    if not ng_view:
        return
    ng_name = space.edit_tree.name
    if not (data['tree_name'] == ng_name):
        return
    if not isinstance(ng_view, node_tree.SverchCustomTree):
        return

    if data.get('mode', 'text-based') == 'text-based':
        draw_text_data(data)
    elif data.get('mode') == "graphical":
        draw_graphical_data(data)
        restore_opengl_defaults()
    elif data.get('mode') == 'custom_function':
        drawing_func = data.get('custom_function')
        x, y = data.get('loc', (20, 20))
        args = data.get('args', (None,))
        drawing_func(x, y, args)
        restore_opengl_defaults()

        
        
def unregister():
    callback_disable_all()
