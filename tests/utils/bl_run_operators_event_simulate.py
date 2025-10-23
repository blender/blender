# SPDX-FileCopyrightText: 2021-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

r"""
Overview
========

This is a utility to generate events from the command line,
so reproducible test cases can be written without having to create a custom script each time.

The key differentiating feature for this utility as is that it's able to control modal operators.

Possible use cases for this script include:

- Creating reproducible user interactions for the purpose of benchmarking and profiling.

  Note that cursor-motion actions report the update time between events
  which can be helpful when measuring optimizations.

- As a convenient way to replay interactive actions that reproduce a bug.

- For writing tests (although some extra functionality may be necessary in this case).


Actions
=======

You will notice most of the functionality is supported using the actions command line argument,
this is a kind of mini-language to drive Blender.

While the current set of commands is fairly limited more can be added as needed.

To see a list of actions as well as their arguments run:

    ./blender.bin --python tests/utils/bl_run_operators_event_simulate.py -- --help


Examples
========

Rotate in edit-mode examples:

    ./blender.bin \
        --factory-startup \
        --enable-event-simulate \
        --python tests/utils/bl_run_operators_event_simulate.py \
        -- \
        --actions \
        'area_maximize(ui_type="VIEW_3D")' \
        'operator("object.mode_set", mode="EDIT")' \
        'operator("mesh.select_all", action="SELECT")' \
        'operator("mesh.subdivide", number_cuts=5)' \
        'operator("transform.rotate")' \
        'cursor_motion(path="CIRCLE", radius=300, steps=100, repeat=2)'

Sculpt stroke:

    ./blender.bin \
        --factory-startup \
        --enable-event-simulate \
        --python tests/utils/bl_run_operators_event_simulate.py \
        -- \
        --actions \
        'area_maximize(ui_type="VIEW_3D")' \
        'event(type="FIVE", value="TAP", ctrl=True)' \
        'menu("Visual Geometry to Mesh")' \
        'menu("Frame Selected")' \
        'menu("Toggle Sculpt Mode")' \
        'event(type="WHEELDOWNMOUSE", value="TAP", repeat=2)' \
        'event(type="LEFTMOUSE", value="PRESS")' \
        'cursor_motion(path="CIRCLE", radius=300, steps=100, repeat=5)' \
        'event(type="LEFTMOUSE", value="RELEASE")'


Implementation
==============

While most of the operations listed above can be executed in Python directly,
either the event loop won't be handled between actions (the case for typical Python script),
or the context for executing the actions is not properly set (the case for timers).

This utility executes actions as if the user initiated them from a key shortcut.
"""
__all__ = (
    "main",
)

import os
import sys
import argparse
from argparse import ArgumentTypeError

import bpy


# -----------------------------------------------------------------------------
# Constants

EVENT_TYPES = tuple(bpy.types.Event.bl_rna.properties["type"].enum_items.keys())
EVENT_VALUES = tuple(bpy.types.Event.bl_rna.properties["value"].enum_items.keys())
# `TAP` is just convenience for (`PRESS`, `RELEASE`).
EVENT_VALUES_EXTRA = EVENT_VALUES + ('TAP',)


# -----------------------------------------------------------------------------
# Globals

# Assign a global since this script is not going to be loading new files (which would free the window).
win = bpy.context.window_manager.windows[0]


# -----------------------------------------------------------------------------
# Utilities

def find_main_area(ui_type=None):
    """
    Find the largest area from the current screen.
    """
    area_best = None
    size_best = -1
    for area in win.screen.areas:
        if ui_type is not None:
            if ui_type != area.ui_type:
                continue

        size = area.width * area.height
        if size > size_best:
            size_best = size
            area_best = area
    return area_best


def gen_events_type_text(text):
    """
    Generate events to type in ``text``.
    """
    for ch in text:
        kw_extra = {}
        # The event type in this case is ignored as only the unicode value is used for text input.
        type = 'SPACE'
        if ch == '\t':
            type = 'TAB'
        elif ch == '\n':
            type = 'RET'
        else:
            kw_extra["unicode"] = ch

        yield dict(type=type, value='PRESS', **kw_extra)
        kw_extra.pop("unicode", None)
        yield dict(type=type, value='RELEASE', **kw_extra)


def repr_action(name, args, kwargs):
    return "%s(%s)" % (
        name,
        ", ".join(
            [repr(value) for value in args] +
            [("%s=%r" % (key, value)) for key, value in kwargs.items()]
        )
    )


# -----------------------------------------------------------------------------
# Simulate Events

def mouse_location_get():
    return (
        run_event_simulate.last_event["x"],
        run_event_simulate.last_event["y"],
    )


def run_event_simulate(*, event_iter, exit_fn):
    """
    Pass events from event_iter into Blender.
    """
    last_event = run_event_simulate.last_event

    def event_step():
        win = bpy.context.window_manager.windows[0]

        val = next(event_step.run_events, Ellipsis)
        if val is Ellipsis:
            bpy.app.use_event_simulate = False
            print("Finished simulation")
            exit_fn()
            return None

        # Run event simulation.
        for attr in ("x", "y"):
            if attr in val:
                last_event[attr] = val[attr]
            else:
                val[attr] = last_event[attr]

        # Fake event value, since press, release is so common.
        if val.get("value") == 'TAP':
            del val["value"]
            win.event_simulate(**val, value='PRESS')
            # Needed if new files are loaded.
            # win = bpy.context.window_manager.windows[0]
            win.event_simulate(**val, value='RELEASE')
        else:
            # print("val", val)
            win.event_simulate(**val)
        return 0.0

    event_step.run_events = iter(event_iter)

    bpy.app.timers.register(event_step, first_interval=0.0, persistent=True)


run_event_simulate.last_event = dict(
    x=win.width // 2,
    y=win.height // 2,
)


# -----------------------------------------------------------------------------
# Action Implementations

# Static methods from this class are automatically exposed as actions and included in the help text.
class action_handlers:

    @staticmethod
    def area_maximize(*, ui_type=None, only_validate=False):
        """
        ui_type:
          Select the area type (typically 'VIEW_3D').
          Note that this area type needs to exist in the current screen.
        """
        if not ((ui_type is None) or (isinstance(ui_type, str))):
            raise ArgumentTypeError("'type' argument %r not None or a string type")

        if only_validate:
            return

        area = find_main_area(ui_type=ui_type)
        if area is None:
            raise ArgumentTypeError("Area with ui_type=%r not found" % ui_type)

        x = area.x + (area.width // 2)
        y = area.y + (area.height // 2)

        yield dict(type='MOUSEMOVE', value='NOTHING', x=x, y=y)
        yield dict(type='SPACE', value='TAP', ctrl=True, alt=True)

        x = win.width // 2
        y = win.height // 2

        yield dict(type='MOUSEMOVE', value='NOTHING', x=x, y=y)

    @staticmethod
    def menu(text, *, only_validate=False):
        """
        text: Menu item to search for and execute.
        """
        if not isinstance(text, str):
            raise ArgumentTypeError("'text' argument not a string")

        if only_validate:
            return

        yield dict(type='F3', value='TAP')
        yield from gen_events_type_text(text)
        yield dict(type='RET', value='TAP')

    @staticmethod
    def event(*, type, value, ctrl=False, alt=False, shift=False, hyper=False, repeat=1, only_validate=False):
        """
        type: The event, typically key, e.g. 'ESC', 'RET', 'SPACE', 'A'.
        value: The event type, valid values include: 'PRESS', 'RELEASE', 'TAP'.
        ctrl: Control modifier.
        alt: Alt modifier.
        shift: Shift modifier.
        hyper: Hyper modifier.
        """
        valid_items = EVENT_VALUES_EXTRA
        if value not in valid_items:
            raise ArgumentTypeError("'value' argument %r not in %r" % (value, valid_items))
        valid_items = EVENT_TYPES
        if type not in valid_items:
            raise ArgumentTypeError("'type' argument %r not in %r" % (value, valid_items))
        valid_items = range(1, sys.maxsize)
        if repeat not in valid_items:
            raise ArgumentTypeError("'repeat' argument %r not in %r" % (repeat, valid_items))
        del valid_items

        if only_validate:
            return

        for _ in range(repeat):
            yield dict(type=type, ctrl=ctrl, alt=alt, shift=shift, hyper=hyper, value=value)

    @staticmethod
    def cursor_motion(*, path, steps, radius=100, repeat=1, only_validate=False):
        """
        path: The path type to use in ('CIRCLE').
        steps: The number of events to generate.
        radius: The radius in pixels.
        repeat: Number of times to repeat the cursor rotation.
        """

        import time
        from math import sin, cos, pi

        valid_items = range(1, sys.maxsize)
        if steps not in valid_items:
            raise ArgumentTypeError("'steps' argument %r not in %r" % (steps, valid_items))

        valid_items = range(1, sys.maxsize)
        if radius not in valid_items:
            raise ArgumentTypeError("'radius' argument %r not in %r" % (steps, valid_items))

        valid_items = ('CIRCLE',)
        if path not in valid_items:
            raise ArgumentTypeError("'path' argument %r not in %r" % (path, valid_items))

        valid_items = range(1, sys.maxsize)
        if repeat not in valid_items:
            raise ArgumentTypeError("'repeat' argument %r not in %r" % (repeat, valid_items))
        del valid_items

        if only_validate:
            return

        x_init, y_init = mouse_location_get()

        y_init_ofs = y_init + radius

        yield dict(type='MOUSEMOVE', value='NOTHING', x=x_init, y=y_init_ofs)

        print("\n" "Times for: %s" % os.path.basename(bpy.data.filepath))

        t = time.time()
        step_total = 0

        if path == 'CIRCLE':
            for _ in range(repeat):
                for i in range(1, steps + 1):
                    phi = (i / steps) * 2.0 * pi
                    x_ofs = -radius * sin(phi)
                    y_ofs = +radius * cos(phi)
                    step_total += 1
                    yield dict(
                        type='MOUSEMOVE',
                        value='NOTHING',
                        x=int(x_init + x_ofs),
                        y=int(y_init + y_ofs),
                    )

        delta = time.time() - t
        delta_step = delta / step_total
        print(
            "Average:",
            ("%.6f FPS" % (1 / delta_step)).rjust(10),
        )

        yield dict(type='MOUSEMOVE', value='NOTHING', x=x_init, y=y_init)

    @staticmethod
    def operator(idname, *, only_validate=False, **kw):
        """
        idname: The operator identifier (positional argument only).
        kw: Passed to the operator.
        """

        # Create a temporary key binding to call the operator.
        wm = bpy.context.window_manager
        keyconf = wm.keyconfigs.user

        keymap_id = "Screen"
        key_to_map = 'F24'

        if only_validate:
            op_mod, op_submod = idname.partition(".")[0::2]
            op = getattr(getattr(bpy.ops, op_mod), op_submod)
            try:
                # The poll result doesn't matter we only want to know if the operator exists or not.
                op.poll()
            except AttributeError:
                raise ArgumentTypeError("Operator %r does not exist" % (idname))

            keymap = keyconf.keymaps[keymap_id]
            kmi = keymap.keymap_items.new(idname=idname, type=key_to_map, value='PRESS')
            kmi.idname = idname
            props = kmi.properties
            for key, value in kw.items():
                if not hasattr(props, key):
                    raise ArgumentTypeError("Operator %r does not have a %r property" % (idname, key))

                try:
                    setattr(props, key, value)
                except Exception as ex:
                    raise ArgumentTypeError("Operator %r assign %r property with error %s" % (idname, key, str(ex)))

            keymap.keymap_items.remove(kmi)
            return

        keymap = keyconf.keymaps[keymap_id]
        kmi = keymap.keymap_items.new(idname=idname, type=key_to_map, value='PRESS')
        kmi.idname = idname
        props = kmi.properties
        for key, value in kw.items():
            setattr(props, key, value)

        yield dict(type=key_to_map, value='TAP')

        keymap = keyconf.keymaps[keymap_id]
        kmi = keymap.keymap_items[-1]
        keymap.keymap_items.remove(kmi)


ACTION_DIR = tuple([
    key for key in sorted(action_handlers.__dict__.keys())
    if not key.startswith("_")
])


def handle_action(op, args, kwargs, only_validate=False):
    fn = getattr(action_handlers, op, None)
    if fn is None:
        raise ArgumentTypeError("Action %r is not found in %r" % (op, ACTION_DIR))
    yield from fn(*args, **kwargs, only_validate=only_validate)


# -----------------------------------------------------------------------------
# Argument Parsing


class BlenderAction(argparse.Action):
    """
    This class is used to extract positional & keyword arguments from
    a string, validate them, and return the (action, positional_args, keyword_args).

    All of this happens during argument parsing so any errors in the actions
    show useful error messages instead of failing to execute part way through.
    """

    @staticmethod
    def _parse_value(value, index):
        """
        Convert:
           "value(1, 2, a=1, b='', c=None)"
        To:
           ("value", (1, 2), {"a": 1, "b": "", "c": None})
        """
        split = value.find("(")
        if split == -1:
            op = value
            args = None
            kwargs = None
        else:
            op = value[:split]
            namespace = {op: lambda *args, **kwargs: (args, kwargs)}
            expr = value
            try:
                args, kwargs = eval(expr, namespace, namespace)
            except Exception as ex:
                raise ArgumentTypeError("Unable to parse \"%s\" at index %d, error: %s" % (expr, index, str(ex)))

        # Creating a list is necessary since this is a generator.
        try:
            dummy_result = list(handle_action(op, args, kwargs, only_validate=True))
        except ArgumentTypeError as ex:
            raise ArgumentTypeError("Invalid 'action' arguments \"%s\" at index %d, %s" % (value, index, str(ex)))
        # Validation should never yield any events.
        assert not dummy_result

        return (op, args, kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        setattr(
            namespace,
            self.dest, [
                self._parse_value(value, index)
                for index, value in enumerate(values)
            ],
        )


def argparse_create():
    import inspect
    import textwrap

    # When --help or no args are given, print this help
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "--keep-open",
        dest="keep_open",
        default=False,
        action="store_true",
        help=(
            "Keep the window open instead of exiting once event simulation is complete.\n"
            "This can be useful to inspect the state of the file once the simulation is complete."
        ),
        required=False,
    )

    parser.add_argument(
        "--time-actions",
        dest="time_actions",
        default=False,
        action="store_true",
        help=(
            "Display the time each action takes\n"
            "(useful for measuring delay between key-presses)."
        ),
        required=False,
    )

    # Collect doc-strings from static methods in `actions`.
    actions_docstring = []
    for action_key in ACTION_DIR:
        action = getattr(action_handlers, action_key)
        args = str(inspect.signature(action))
        args = "(" + args[1:].removeprefix("*, ")
        args = args.replace(", *, ", ", ")  # Needed in case the are positional arguments.
        args = args.replace(", only_validate=False", "")

        actions_docstring.append("- %s%s\n" % (action_key, args))
        docs = textwrap.dedent((action.__doc__ or "").lstrip("\n").rstrip()) + "\n\n"

        actions_docstring.append(textwrap.indent(docs, "  "))

    parser.add_argument(
        "--actions",
        dest="actions",
        metavar='ACTIONS', type=str,
        help=(
            "\n" "Arguments must use one of the following prefix:\n"
            "\n" + "".join(actions_docstring)
        ),
        nargs='+',
        required=True,
        action=BlenderAction,
    )

    return parser


# -----------------------------------------------------------------------------
# Default Startup


def setup_default_preferences(prefs):
    """
    Set preferences useful for automation.
    """
    prefs.view.show_splash = False
    prefs.view.smooth_view = 0
    prefs.view.use_save_prompt = False
    prefs.view.show_developer_ui = True
    prefs.filepaths.use_auto_save_temporary_files = False


# -----------------------------------------------------------------------------
# Main Function


def main_event_iter(*, action_list, time_actions):
    """
    Yield all events from action handlers.
    """
    area = find_main_area()

    x_init = area.x + (area.width // 2)
    y_init = area.y + (area.height // 2)

    yield dict(type='MOUSEMOVE', value='NOTHING', x=x_init, y=y_init)

    if time_actions:
        import time
        t_prev = time.time()

    for (op, args, kwargs) in action_list:
        yield from handle_action(op, args, kwargs)

        if time_actions:
            t = time.time()
            print("%.4f: %s" % ((t - t_prev), repr_action(op, args, kwargs)))
            t_prev = t


def main():
    from sys import argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []

    try:
        args = argparse_create().parse_args(argv)
    except ArgumentTypeError as ex:
        print(ex)
        sys.exit(1)

    setup_default_preferences(bpy.context.preferences)

    def exit_fn():
        if not args.keep_open:
            sys.exit(0)
        else:
            bpy.app.use_event_simulate = False

    run_event_simulate(
        event_iter=main_event_iter(action_list=args.actions, time_actions=args.time_actions),
        exit_fn=exit_fn,
    )


if __name__ == "__main__":
    main()
