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

# <pep8-80 compliant>
import sys
import bpy

language_id = "python"

# store our own __main__ module, not 100% needed
# but python expects this in some places
_BPY_MAIN_OWN = True


def add_scrollback(text, text_type):
    for l in text.split("\n"):
        bpy.ops.console.scrollback_append(text=l.replace("\t", "    "),
                                          type=text_type)


def replace_help(namespace):
    def _help(*args):
        # because of how the console works. we need our own help() pager func.
        # replace the bold function because it adds crazy chars
        import pydoc
        pydoc.getpager = lambda: pydoc.plainpager
        pydoc.Helper.getline = lambda self, prompt: None
        pydoc.TextDoc.use_bold = lambda self, text: text

        pydoc.help(*args)

    namespace["help"] = _help


def get_console(console_id):
    """
    helper function for console operators
    currently each text data block gets its own
    console - code.InteractiveConsole()
    ...which is stored in this function.

    console_id can be any hashable type
    """
    from code import InteractiveConsole

    consoles = getattr(get_console, "consoles", None)
    hash_next = hash(bpy.context.window_manager)

    if consoles is None:
        consoles = get_console.consoles = {}
        get_console.consoles_namespace_hash = hash_next
    else:
        # check if clearing the namespace is needed to avoid a memory leak.
        # the window manager is normally loaded with new blend files
        # so this is a reasonable way to deal with namespace clearing.
        # bpy.data hashing is reset by undo so can't be used.
        hash_prev = getattr(get_console, "consoles_namespace_hash", 0)

        if hash_prev != hash_next:
            get_console.consoles_namespace_hash = hash_next
            consoles.clear()

    console_data = consoles.get(console_id)

    if console_data:
        console, stdout, stderr = console_data

        # XXX, bug in python 3.1.2, 3.2 ? (worked in 3.1.1)
        # seems there is no way to clear StringIO objects for writing, have to
        # make new ones each time.
        import io
        stdout = io.StringIO()
        stderr = io.StringIO()
    else:
        if _BPY_MAIN_OWN:
            import types
            bpy_main_mod = types.ModuleType("__main__")
            namespace = bpy_main_mod.__dict__
        else:
            namespace = {}

        namespace["__builtins__"] = sys.modules["builtins"]
        namespace["bpy"] = bpy

        # weak! - but highly convenient
        namespace["C"] = bpy.context
        namespace["D"] = bpy.data

        replace_help(namespace)

        console = InteractiveConsole(locals=namespace,
                                     filename="<blender_console>")

        console.push("from mathutils import *")
        console.push("from math import *")

        if _BPY_MAIN_OWN:
            console._bpy_main_mod = bpy_main_mod

        import io
        stdout = io.StringIO()
        stderr = io.StringIO()

        consoles[console_id] = console, stdout, stderr

    return console, stdout, stderr


# Both prompts must be the same length
PROMPT = '>>> '
PROMPT_MULTI = '... '


def execute(context, is_interactive):
    sc = context.space_data

    try:
        line_object = sc.history[-1]
    except:
        return {'CANCELLED'}

    console, stdout, stderr = get_console(hash(context.region))

    if _BPY_MAIN_OWN:
        main_mod_back = sys.modules["__main__"]
        sys.modules["__main__"] = console._bpy_main_mod

    # redirect output
    from contextlib import (
        redirect_stdout,
        redirect_stderr,
    )

    # not included with Python
    class redirect_stdin(redirect_stdout.__base__):
        _stream = "stdin"

    # don't allow the stdin to be used, can lock blender.
    with redirect_stdout(stdout), \
            redirect_stderr(stderr), \
            redirect_stdin(None):

        # in case exception happens
        line = ""  # in case of encoding error
        is_multiline = False

        try:
            line = line_object.body

            # run the console, "\n" executes a multi line statement
            line_exec = line if line.strip() else "\n"

            is_multiline = console.push(line_exec)
        except:
            # unlikely, but this can happen with unicode errors for example.
            import traceback
            stderr.write(traceback.format_exc())

    if _BPY_MAIN_OWN:
        sys.modules["__main__"] = main_mod_back

    stdout.seek(0)
    stderr.seek(0)

    output = stdout.read()
    output_err = stderr.read()

    # cleanup
    sys.last_traceback = None

    # So we can reuse, clear all data
    stdout.truncate(0)
    stderr.truncate(0)

    # special exception. its possible the command loaded a new user interface
    if hash(sc) != hash(context.space_data):
        return {'FINISHED'}

    bpy.ops.console.scrollback_append(text=sc.prompt + line, type='INPUT')

    if is_multiline:
        sc.prompt = PROMPT_MULTI
        if is_interactive:
            indent = line[:len(line) - len(line.lstrip())]
            if line.rstrip().endswith(":"):
                indent += "    "
        else:
            indent = ""
    else:
        sc.prompt = PROMPT
        indent = ""

    # insert a new blank line
    bpy.ops.console.history_append(text=indent, current_character=0,
                                   remove_duplicates=True)
    sc.history[-1].current_character = len(indent)

    # Insert the output into the editor
    # not quite correct because the order might have changed,
    # but ok 99% of the time.
    if output:
        add_scrollback(output, 'OUTPUT')
    if output_err:
        add_scrollback(output_err, 'ERROR')

    # execute any hooks
    for func, args in execute.hooks:
        func(*args)

    return {'FINISHED'}


execute.hooks = []


def autocomplete(context):
    _readline_bypass()

    from console import intellisense

    sc = context.space_data

    console = get_console(hash(context.region))[0]

    if not console:
        return {'CANCELLED'}

    # don't allow the stdin to be used, can lock blender.
    # note: unlikely stdin would be used for autocomplete. but its possible.
    stdin_backup = sys.stdin
    sys.stdin = None

    scrollback = ""
    scrollback_error = ""

    if _BPY_MAIN_OWN:
        main_mod_back = sys.modules["__main__"]
        sys.modules["__main__"] = console._bpy_main_mod

    try:
        current_line = sc.history[-1]
        line = current_line.body

        # This function isn't aware of the text editor or being an operator
        # just does the autocomplete then copy its results back
        result = intellisense.expand(
            line=line,
            cursor=current_line.current_character,
            namespace=console.locals,
            private=bpy.app.debug_python)

        line_new = result[0]
        current_line.body, current_line.current_character, scrollback = result
        del result

        # update selection. setting body should really do this!
        ofs = len(line_new) - len(line)
        sc.select_start += ofs
        sc.select_end += ofs
    except:
        # unlikely, but this can happen with unicode errors for example.
        # or if the api attribute access its self causes an error.
        import traceback
        scrollback_error = traceback.format_exc()

    if _BPY_MAIN_OWN:
        sys.modules["__main__"] = main_mod_back

    # Separate autocomplete output by command prompts
    if scrollback != '':
        bpy.ops.console.scrollback_append(text=sc.prompt + current_line.body,
                                          type='INPUT')

    # Now we need to copy back the line from blender back into the
    # text editor. This will change when we don't use the text editor
    # anymore
    if scrollback:
        add_scrollback(scrollback, 'INFO')

    if scrollback_error:
        add_scrollback(scrollback_error, 'ERROR')

    # restore the stdin
    sys.stdin = stdin_backup

    context.area.tag_redraw()

    return {'FINISHED'}


def copy_as_script(context):
    sc = context.space_data
    lines = [
        "import bpy",
        "from bpy import data as D",
        "from bpy import context as C",
        "from mathutils import *",
        "from math import *",
        "",
    ]

    for line in sc.scrollback:
        text = line.body
        type = line.type

        if type == 'INFO':  # ignore autocomp.
            continue
        if type == 'INPUT':
            if text.startswith(PROMPT):
                text = text[len(PROMPT):]
            elif text.startswith(PROMPT_MULTI):
                text = text[len(PROMPT_MULTI):]
        elif type == 'OUTPUT':
            text = "#~ " + text
        elif type == 'ERROR':
            text = "#! " + text

        lines.append(text)

    context.window_manager.clipboard = "\n".join(lines)

    return {'FINISHED'}


def banner(context):
    sc = context.space_data
    version_string = sys.version.strip().replace('\n', ' ')

    add_scrollback("PYTHON INTERACTIVE CONSOLE %s" % version_string, 'OUTPUT')
    add_scrollback("", 'OUTPUT')
    add_scrollback("Command History:     Up/Down Arrow", 'OUTPUT')
    add_scrollback("Cursor:              Left/Right Home/End", 'OUTPUT')
    add_scrollback("Remove:              Backspace/Delete", 'OUTPUT')
    add_scrollback("Execute:             Enter", 'OUTPUT')
    add_scrollback("Autocomplete:        Ctrl-Space", 'OUTPUT')
    add_scrollback("Zoom:                Ctrl +/-, Ctrl-Wheel", 'OUTPUT')
    add_scrollback("Builtin Modules:     bpy, bpy.data, bpy.ops, "
                   "bpy.props, bpy.types, bpy.context, bpy.utils, "
                   "bgl, blf, mathutils",
                   'OUTPUT')
    add_scrollback("Convenience Imports: from mathutils import *; "
                   "from math import *", 'OUTPUT')
    add_scrollback("Convenience Variables: C = bpy.context, D = bpy.data",
                   'OUTPUT')
    add_scrollback("", 'OUTPUT')
    sc.prompt = PROMPT

    return {'FINISHED'}


# workaround for readline crashing, see: T43491
def _readline_bypass():
    if "rlcompleter" in sys.modules or "readline" in sys.modules:
        return

    # prevent 'rlcompleter' from loading the real 'readline' module.
    sys.modules["readline"] = None
    import rlcompleter
    del sys.modules["readline"]
