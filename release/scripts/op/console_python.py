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
import sys
import bpy

language_id = 'python'


def add_scrollback(text, text_type):
    for l in text.split('\n'):
        bpy.ops.console.scrollback_append(text=l.replace('\t', '    '),
            type=text_type)


def get_console(console_id):
    '''
    helper function for console operators
    currently each text datablock gets its own
    console - bpython_code.InteractiveConsole()
    ...which is stored in this function.

    console_id can be any hashable type
    '''
    from code import InteractiveConsole

    try:
        consoles = get_console.consoles
    except:
        consoles = get_console.consoles = {}

    # clear all dead consoles, use text names as IDs
    # TODO, find a way to clear IDs
    '''
    for console_id in list(consoles.keys()):
        if console_id not in bpy.data.texts:
            del consoles[id]
    '''

    try:
        console, stdout, stderr = consoles[console_id]
    except:
        namespace = {'__builtins__': __builtins__, 'bpy': bpy}
        console = InteractiveConsole(namespace)

        import io
        stdout = io.StringIO()
        stderr = io.StringIO()

        consoles[console_id] = console, stdout, stderr

    return console, stdout, stderr


# Both prompts must be the same length
PROMPT = '>>> '
PROMPT_MULTI = '... '


def execute(context):
    sc = context.space_data

    try:
        line = sc.history[-1].line
    except:
        return {'CANCELLED'}

    if sc.console_type != 'PYTHON':
        return {'CANCELLED'}

    console, stdout, stderr = get_console(hash(context.region))

    # Hack, useful but must add some other way to access
    #if "C" not in console.locals:
    console.locals["C"] = context

    # redirect output
    sys.stdout = stdout
    sys.stderr = stderr

    # run the console
    if not line.strip():
        line_exec = '\n'  # executes a multiline statement
    else:
        line_exec = line

    is_multiline = console.push(line_exec)

    stdout.seek(0)
    stderr.seek(0)

    output = stdout.read()
    output_err = stderr.read()

    # cleanup
    sys.stdout = sys.__stdout__
    sys.stderr = sys.__stderr__
    sys.last_traceback = None

    # So we can reuse, clear all data
    stdout.truncate(0)
    stderr.truncate(0)

    bpy.ops.console.scrollback_append(text=sc.prompt + line, type='INPUT')

    if is_multiline:
        sc.prompt = PROMPT_MULTI
    else:
        sc.prompt = PROMPT

    # insert a new blank line
    bpy.ops.console.history_append(text="", current_character=0,
        remove_duplicates=True)

    # Insert the output into the editor
    # not quite correct because the order might have changed,
    # but ok 99% of the time.
    if output:
        add_scrollback(output, 'OUTPUT')
    if output_err:
        add_scrollback(output_err, 'ERROR')

    return {'FINISHED'}


def autocomplete(context):
    from console import intellisense

    sc = context.space_data

    console = get_console(hash(context.region))[0]

    current_line = sc.history[-1]
    line = current_line.line

    if not console:
        return {'CANCELLED'}

    if sc.console_type != 'PYTHON':
        return {'CANCELLED'}

    # This function isnt aware of the text editor or being an operator
    # just does the autocomp then copy its results back
    current_line.line, current_line.current_character, scrollback = \
        intellisense.expand(
            line=current_line.line,
            cursor=current_line.current_character,
            namespace=console.locals,
            private=bpy.app.debug)

    # Separate automplete output by command prompts
    if scrollback != '':
        bpy.ops.console.scrollback_append(text=sc.prompt + current_line.line, type='INPUT')

    # Now we need to copy back the line from blender back into the
    # text editor. This will change when we dont use the text editor
    # anymore
    if scrollback:
        add_scrollback(scrollback, 'INFO')

    context.area.tag_redraw()

    return {'FINISHED'}


def banner(context):
    sc = context.space_data
    version_string = sys.version.strip().replace('\n', ' ')

    add_scrollback(" * Python Interactive Console %s *" % version_string, 'OUTPUT')
    add_scrollback("Command History:  Up/Down Arrow", 'OUTPUT')
    add_scrollback("Cursor:           Left/Right Home/End", 'OUTPUT')
    add_scrollback("Remove:           Backspace/Delete", 'OUTPUT')
    add_scrollback("Execute:          Enter", 'OUTPUT')
    add_scrollback("Autocomplete:     Ctrl+Space", 'OUTPUT')
    add_scrollback("Ctrl +/-  Wheel:  Zoom", 'OUTPUT')
    add_scrollback("Builtin Modules: bpy, bpy.data, bpy.ops, bpy.props, bpy.types, bpy.context, bgl, blf, Mathutils, Geometry", 'OUTPUT')
    add_scrollback("", 'OUTPUT')
    add_scrollback("  WARNING!!! Blender 2.5 API is subject to change, see API reference for more info.", 'ERROR')
    add_scrollback("", 'OUTPUT')
    sc.prompt = PROMPT

    # Add context into the namespace for quick access
    console = get_console(hash(context.region))[0]
    console.locals["C"] = bpy.context

    return {'FINISHED'}


def register():
    pass


def unregister():
    pass


if __name__ == "__main__":
    register()
