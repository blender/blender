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

import bpy
import blf
import bgl
from  sverchok import node_tree

from bpy.types import SpaceNodeEditor


callback_dict = {}
point_dict = {}


def adjust_list(in_list, x, y):
    return [[old_x + x, old_y + y] for (old_x, old_y) in in_list]


def parse_socket(socket):

    data = socket.sv_get(deepcopy=False)

    str_width = 60

    # okay, here we should be more clever and extract part of the list
    # to avoid the amount of time it take to format it.
    
    content_str = pprint.pformat(data, width=str_width)
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
        return "{:.5f}".format(float(match.group()))

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

    lines = data.get('content', 'no data')
    x, y = data.get('location', (120, 120))
    color = data.get('color', (0.1, 0.1, 0.1))
    font_id = 0
    text_height = 13

    # why does the text look so jagged?
    blf.size(font_id, text_height, 72)  # should check prefs.dpi
    bgl.glColor3f(*color)
    # x = 30  # region.width
    # y = region.height - 40
    ypos = y

    for line in lines:
        blf.position(0, x, ypos, 0)
        blf.draw(0, line)
        ypos -= (text_height * 1.3)
        
        
def unregister():
    callback_disable_all()
