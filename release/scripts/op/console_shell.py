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
import os
import bpy

language_id = 'shell'


def add_scrollback(text, text_type):
    for l in text.split('\n'):
        bpy.ops.console.scrollback_append(text=l.replace('\t', '    '),
            type=text_type)


def shell_run(text):
    import subprocess
    val, output = subprocess.getstatusoutput(text)

    if not val:
        style = 'OUTPUT'
    else:
        style = 'ERROR'

    add_scrollback(output, style)

PROMPT = '$ '


def execute(context):
    sc = context.space_data

    try:
        line = sc.history[-1].line
    except:
        return {'CANCELLED'}

    bpy.ops.console.scrollback_append(text=sc.prompt + line, type='INPUT')

    shell_run(line)

    # insert a new blank line
    bpy.ops.console.history_append(text="", current_character=0,
        remove_duplicates=True)

    sc.prompt = os.getcwd() + PROMPT
    return {'FINISHED'}


def autocomplete(context):
    # sc = context.space_data
    # TODO
    return {'CANCELLED'}


def banner(context):
    sc = context.space_data

    shell_run("bash --version")
    sc.prompt = os.getcwd() + PROMPT

    return {'FINISHED'}
