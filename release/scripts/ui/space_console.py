import sys
import bpy


class CONSOLE_HT_header(bpy.types.Header):
    __space_type__ = 'CONSOLE'

    def draw(self, context):
        sc = context.space_data
        # text = sc.text
        layout = self.layout

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)

            if sc.console_type == 'REPORT':
                sub.itemM("CONSOLE_MT_report")
            else:
                sub.itemM("CONSOLE_MT_console")

        layout.itemS()
        layout.itemR(sc, "console_type", expand=True)

        if sc.console_type == 'REPORT':
            row = layout.row(align=True)
            row.itemR(sc, "show_report_debug", text="Debug")
            row.itemR(sc, "show_report_info", text="Info")
            row.itemR(sc, "show_report_operator", text="Operators")
            row.itemR(sc, "show_report_warn", text="Warnings")
            row.itemR(sc, "show_report_error", text="Errors")

            row = layout.row()
            row.enabled = sc.show_report_operator
            row.itemO("console.report_replay")
        else:
            row = layout.row(align=True)
            row.itemO("console.autocomplete", text="Autocomplete")


class CONSOLE_MT_console(bpy.types.Menu):
    __label__ = "Console"

    def draw(self, context):
        layout = self.layout
        layout.column()
        layout.itemO("console.clear")
        layout.itemO("console.copy")
        layout.itemO("console.paste")


class CONSOLE_MT_report(bpy.types.Menu):
    __label__ = "Report"

    def draw(self, context):
        layout = self.layout
        layout.column()
        layout.itemO("console.select_all_toggle")
        layout.itemO("console.select_border")
        layout.itemO("console.report_delete")
        layout.itemO("console.report_copy")


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


class CONSOLE_OT_exec(bpy.types.Operator):
    '''Execute the current console line as a python expression.'''
    __idname__ = "console.execute"
    __label__ = "Console Execute"
    __register__ = False

    # Both prompts must be the same length
    PROMPT = '>>> '
    PROMPT_MULTI = '... '

    # is this working???
    '''
    def poll(self, context):
        return (context.space_data.type == 'PYTHON')
    '''
    # its not :|

    def execute(self, context):
        sc = context.space_data

        try:
            line = sc.history[-1].line
        except:
            return ('CANCELLED',)

        if sc.console_type != 'PYTHON':
            return ('CANCELLED',)

        console, stdout, stderr = get_console(hash(context.region))

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
            sc.prompt = self.PROMPT_MULTI
        else:
            sc.prompt = self.PROMPT

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

        return ('FINISHED',)


class CONSOLE_OT_autocomplete(bpy.types.Operator):
    '''Evaluate the namespace up until the cursor and give a list of
    options or complete the name if there is only one.'''
    __idname__ = "console.autocomplete"
    __label__ = "Console Autocomplete"
    __register__ = False

    def poll(self, context):
        return context.space_data.console_type == 'PYTHON'

    def execute(self, context):
        from console import intellisense

        sc = context.space_data

        console = get_console(hash(context.region))[0]

        current_line = sc.history[-1]
        line = current_line.line

        if not console:
            return ('CANCELLED',)

        if sc.console_type != 'PYTHON':
            return ('CANCELLED',)

        # This function isnt aware of the text editor or being an operator
        # just does the autocomp then copy its results back
        current_line.line, current_line.current_character, scrollback = \
            intellisense.expand(
                line=current_line.line,
                cursor=current_line.current_character,
                namespace=console.locals,
                private='-d' in sys.argv)

        # Now we need to copy back the line from blender back into the
        # text editor. This will change when we dont use the text editor
        # anymore
        if scrollback:
            add_scrollback(scrollback, 'INFO')

        context.area.tag_redraw()

        return ('FINISHED',)


bpy.types.register(CONSOLE_HT_header)
bpy.types.register(CONSOLE_MT_console)
bpy.types.register(CONSOLE_MT_report)

bpy.ops.add(CONSOLE_OT_exec)
bpy.ops.add(CONSOLE_OT_autocomplete)
