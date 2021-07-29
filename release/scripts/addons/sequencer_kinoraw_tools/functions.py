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
import os.path
import operator
import subprocess
import random
from bpy.props import (
        IntProperty,
        FloatProperty,
        EnumProperty,
        BoolProperty,
        )

imb_ext_image = [
    # IMG
    ".png", ".tga", ".bmp", ".jpg", ".jpeg", ".sgi", ".rgb",
    ".rgba", ".tif", ".tiff", ".tx", ".jp2", ".hdr", ".dds",
    ".dpx", ".cin", ".exr", ".rw2",
    # IMG QT
    ".gif", ".psd", ".pct", ".pict", ".pntg", ".qtif"
    ]
imb_ext_audio = [
    ".wav", ".ogg", ".oga", ".mp3", ".mp2", ".ac3", ".aac",
    ".flac", ".wma", ".eac3", ".aif", ".aiff", ".m4a"
    ]
imb_ext_movie = [
    ".avi", ".flc", ".mov", ".movie", ".mp4", ".m4v", ".m2v",
    ".m2t", ".m2ts", ".mts", ".mv", ".avs", ".wmv", ".ogv", ".ogg",
    ".dv", ".mpeg", ".mpg", ".mpg2", ".vob", ".mkv", ".flv",
    ".divx", ".xvid", ".mxf"
    ]
movieextdict = [
    ("1", ".avi", ""),
    ("2", ".flc", ""), ("3", ".mov", ""),
    ("4", ".movie", ""), ("5", ".mp4", ""),
    ("6", ".m4v", ""), ("7", ".m2v", ""),
    ("8", ".m2t", ""), ("9", ".m2ts", ""),
    ("10", ".mts", ""), ("11", ".mv", ""),
    ("12", ".avs", ""), ("13", ".wmv", ""),
    ("14", ".ogv", ""), ("15", ".dv", ""),
    ("16", ".mpeg", ""), ("17", ".mpg", ""),
    ("18", ".mpg2", ""), ("19", ".vob", ""),
    ("20", ".mkv", ""), ("21", ".flv", ""),
    ("22", ".divx", ""), ("23", ".xvid", ""),
    ("24", ".mxf", "")
    ]


# Functions
def error_handlers(self, op_name, errors, reports="ERROR"):
    if self and reports:
        self.report({'INFO'},
                    reports + ": some operations could not be performed "
                    "(See Console for more info)")

    print("\n[Kinoraw Tools]\nOperator: {}\nWarning: {}\n".format(op_name, errors))


def initSceneProperties(context):
    # initSceneProperties is ONLY for varaibles that should
    # be keeped with the blend file. Any other addon preferences
    # should go to the addon preferences operator in __init__
    try:
        if context.scene.kr_scn_init is True:
            return False
    except AttributeError:
        pass

    scn = context.scene

    # jump to cut
    bpy.types.Scene.kr_auto_markers = BoolProperty(
            name="kr_auto_markers",
            description="Activate Auto markers",
            default=False
            )
    scn.kr_auto_markers = False

    bpy.types.Scene.kr_in_marker = IntProperty(
            name="In",
            description="In frame position",
            min=-30000, max=30000,
            default=1
            )
    scn.kr_in_marker = 1

    bpy.types.Scene.kr_out_marker = IntProperty(
            name="Out",
            description="Out frame position",
            min=scn.kr_in_marker, max=30000,
            default=75
            )
    scn.kr_out_marker = 75

    # sequencer extra actions
    bpy.types.Scene.kr_default_fade_duration = IntProperty(
            name="Duration",
            description="Number of frames to fade",
            min=1, max=250,
            default=scn.render.fps
            )
    scn.kr_default_fade_duration = scn.render.fps

    bpy.types.Scene.kr_default_fade_amount = FloatProperty(
            name="Amount",
            description="Maximum value of fade",
            min=0.0,
            max=100.0,
            default=1.0
            )
    scn.kr_default_fade_amount = 1.0

    # recursive loader
    bpy.types.Scene.kr_recursive = BoolProperty(
            name="Recursive",
            description="Load in recursive folders",
            default=False
            )
    scn.kr_recursive = False

    bpy.types.Scene.kr_recursive_select_by_extension = BoolProperty(
            name="Recursive ext",
            description="Load only clips with selected extension",
            default=False
            )
    scn.kr_recursive_select_by_extension = False

    bpy.types.Scene.kr_default_ext = EnumProperty(
            items=movieextdict,
            name="ext enum",
            default="3"
            )
    scn.kr_default_ext = "3"

    bpy.types.Scene.kr_scn_init = BoolProperty(
            name="Init",
            default=False
            )
    scn.kr_scn_init = True

    return True


def get_selected_strips(context):
    "return a list of selected strips"
    strips = []
    for i in context.scene.sequence_editor.sequences_all:
        if i.select is True:
            strips.append(i)
    return strips


def create_folder(path):
    if not os.path.isdir(bpy.path.abspath(path)):
        folder = bpy.path.abspath(path)
        command = "mkdir " + folder
        subprocess.call(command, shell=True)


def add_marker(context, text, frame):
    scn = context.scene
    markers = scn.timeline_markers
    mark = markers.new(name=text)
    mark.frame = frame


def act_strip(context):
    try:
        return context.scene.sequence_editor.active_strip
    except AttributeError:
        return None


def detect_strip_type(filepath):
    extension = os.path.splitext(filepath)[1]
    extension = extension.lower()
    if extension in imb_ext_image:
        type = 'IMAGE'
    elif extension in imb_ext_movie:
        type = 'MOVIE'
    elif extension in imb_ext_audio:
        type = 'SOUND'
    else:
        type = None

    return type


# recursive load functions
def getpathfrombrowser(context):
    '''
    returns path from filebrowser
    '''
    for a in context.window.screen.areas:
        if a.type == 'FILE_BROWSER':
            params = a.spaces[0].params
            break
    try:
        params
    except UnboundLocalError:
        return {'CANCELLED'}

    path = params.directory
    return path


def getfilepathfrombrowser(context):
    """
    returns path and file from filebrowser
    """
    for a in context.window.screen.areas:
        if a.type == 'FILE_BROWSER':
            params = a.spaces[0].params
            break
    try:
        params
    except UnboundLocalError:
        return {'CANCELLED'}

    if params.filename == '':
        return {'CANCELLED'}

    path = params.directory
    filename = params.filename
    return path, filename


def setpathinbrowser(context, path, file):
    '''
    set path and file in the filebrowser
    '''
    for a in context.window.screen.areas:
        if a.type == 'FILE_BROWSER':
            params = a.spaces[0].params
            break
    try:
        params
    except UnboundLocalError:

        return {'CANCELLED'}

    params.directory = path
    params.filename = file
    return path, params


def sortlist(filelist):
    '''
    given a list of tuplas (path, filename) returns a list sorted by filename
    '''
    filelist_sorted = sorted(filelist, key=operator.itemgetter(1))
    return filelist_sorted


def onefolder(context, recursive_select_by_extension, ext):
    '''
    returns a list of MOVIE type files from folder selected in file browser
    '''
    filelist = []
    path, filename = getfilepathfrombrowser(context)

    for i in movieextdict:
        if i[0] == ext:
            extension = i[1].rpartition(".")[2]
            break

    if detect_strip_type(path + filename) == 'MOVIE':
        if recursive_select_by_extension is True:
            # filtering by extension...
            for file in os.listdir(path):
                if file.rpartition(".")[2].lower() == extension:
                    filelist.append((path, file))
        else:
            # looking for all known extensions
            for file in os.listdir(path):
                for i in movieextdict:
                    if file.rpartition(".")[2].lower() == i[1].rpartition(".")[2]:
                        filelist.append((path, file))
    return (filelist)


def recursive(context, recursive_select_by_extension, ext):
    '''
    returns a list of MOVIE type files recursively from file browser
    '''
    filelist = []
    path = getpathfrombrowser(context)

    for i in movieextdict:
        if i[0] == ext:
            extension = i[1].rpartition(".")[2]
            break

    for root, dirs, files in os.walk(path):
        for file in files:
            if recursive_select_by_extension is True:
                # filtering by extension...
                if file.rpartition(".")[2].lower() == extension:
                    filelist.append((root, file))
            else:
                # looking for all known extensions
                for i in movieextdict:
                    if file.rpartition(".")[2].lower() == i[1].rpartition(".")[2]:
                        filelist.append((root, file))
    return filelist


# jump to cut functions
def triminout(strip, sin, sout):

    """trim the strip to in and out, and returns
    true if the strip is outside given in and out"""

    start = strip.frame_start + strip.frame_offset_start - strip.frame_still_start
    end = start + strip.frame_final_duration

    remove = False
    if end < sin:
        remove = True
    if start > sout:
        remove = True

    if end > sin:
        if start < sin:
            strip.select_right_handle = False
            strip.select_left_handle = True
            bpy.ops.sequencer.snap(frame=sin)
            strip.select_left_handle = False
    if start < sout:
        if end > sout:
            strip.select_left_handle = False
            strip.select_right_handle = True
            bpy.ops.sequencer.snap(frame=sout)
            strip.select_right_handle = False

    return remove


# random editor functions

def randompartition(lst, n, rand):
    division = len(lst) / float(n)
    lista = []
    for i in range(n):
        lista.append(division)

    var = 0
    for i in range(n - 1):
        lista[i] += random.randint(-int(rand * division), int(rand * division))
        var += lista[i]

    if lista[n - 1] != len(lst) - var:
        lista[n - 1] = len(lst) - var

    random.shuffle(lista)
    division = len(lst) / float(n)
    count = 0
    newlist = []
    for i in range(n):
        # print(lst[count : int(lista[i]-1)+count])
        newlist.append([lst[count: int(lista[i] - 1) + count]])
        count += int(lista[i])

    return newlist


def randomframe(strip):
    # random frame between a and b
    a = strip.frame_start
    b = strip.frame_final_duration
    rand = a + int(random.random() * b)

    return rand


# ???
def get_matching_markers(scene, name=None):
    '''
    return a list of markers with same name
    from the scene, or all markers if name is None
    '''
    selected_markers = []
    markers = scene.timeline_markers
    for mark in markers:
        # print(mark.name, name)
        if mark.name == name or name is None:
            selected_markers.append(mark.frame)

    return selected_markers


def generate_subsets_list(number_of_subsets):
    # generate marker subsets list
    subset_list = []
    subset_names = ['A', 'B', 'C', 'D', 'E', 'F']

    for subset in range(number_of_subsets):
        subset_list.append(subset_names[subset])
    return subset_list


def get_marker_dict(scene, number_of_subsets):
    """
    return a dict where:
        keys = subset names
        values = list of markers
    """

    subset_list = generate_subsets_list(number_of_subsets)
    # generate dict with a list for each subset
    marker_dict = {}

    for subset in subset_list:
        lists = get_matching_markers(scene, subset)
        marker_dict[subset] = lists
    return marker_dict


def get_cut_dict(scene, number_of_subsets):
    """
    return a dict where:
        keys = markers in the scene + start and end
        values = duration in frames from key marker to next marker
    """
    # generate cut_list

    lists = get_matching_markers(scene)
    lists.append(scene.frame_start)
    lists.append(scene.frame_end)
    lists.sort()
    cut_dict = {}

    for i, j in enumerate(lists):
        try:
            cut_dict[j] = lists[i + 1] - j
        except IndexError:
            continue
    return cut_dict
