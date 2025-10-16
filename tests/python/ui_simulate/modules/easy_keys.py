# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import datetime
import string
import bpy
event_types = tuple(
    e.identifier.lower()
    for e in bpy.types.Event.bl_rna.properties["type"].enum_items_static
)
del bpy

# We don't normally care about which one.
event_types_alias = {
    "ctrl": "left_ctrl",
    "shift": "left_shift",
    "alt": "left_alt",

    # Collides with Python keywords.
    "delete": "del",
}


# Note, we could add support for other keys using control characters,
# for example: `\xF12` could be used for the F12 key.
#
# Besides this, we could encode symbols into a regular string using our own syntax
# which can mix regular text and key symbols.
#
# At the moment this doesn't seem necessary, no need to add it.
event_types_text = (
    ('ZERO', "0", False),
    ('ONE', "1", False),
    ('TWO', "2", False),
    ('THREE', "3", False),
    ('FOUR', "4", False),
    ('FIVE', "5", False),
    ('SIX', "6", False),
    ('SEVEN', "7", False),
    ('EIGHT', "8", False),
    ('NINE', "9", False),

    ('ONE', "!", True),
    ('TWO', "@", True),
    ('THREE', "#", True),
    ('FOUR', "$", True),
    ('FIVE', "%", True),
    ('SIX', "^", True),
    ('SEVEN', "&", True),
    ('EIGHT', "*", True),
    ('NINE', "(", True),
    ('ZERO', ")", True),

    ('MINUS', "-", False),
    ('MINUS', "_", True),

    ('EQUAL', "=", False),
    ('EQUAL', "+", True),

    ('ACCENT_GRAVE', "`", False),
    ('ACCENT_GRAVE', "~", True),

    ('LEFT_BRACKET', "[", False),
    ('LEFT_BRACKET', "{", True),

    ('RIGHT_BRACKET', "]", False),
    ('RIGHT_BRACKET', "}", True),

    ('SEMI_COLON', ";", False),
    ('SEMI_COLON', ":", True),

    ('PERIOD', ".", False),
    ('PERIOD', ">", True),

    ('COMMA', ",", False),
    ('COMMA', "<", True),

    ('QUOTE', "'", False),
    ('QUOTE', '"', True),

    ('SLASH', "/", False),
    ('SLASH', "?", True),

    ('BACK_SLASH', "\\", False),
    ('BACK_SLASH', "|", True),


    *((ch_upper, ch, False) for (ch_upper, ch) in zip(string.ascii_uppercase, string.ascii_lowercase)),
    *((ch, ch, True) for ch in string.ascii_uppercase),

    ('SPACE', " ", False),
    ('RET', "\n", False),
    ('TAB', "\t", False),
)

event_types_text_from_char = {ch: (ty, is_shift) for (ty, ch, is_shift) in event_types_text}
event_types_text_from_event = {(ty, is_shift): ch for (ty, ch, is_shift) in event_types_text}


class _EventBuilder:
    __slots__ = (
        "_shared_event_gen",
        "_event_type",
        "_parent",
    )

    def __init__(self, event_gen, ty):
        self._shared_event_gen = event_gen
        self._event_type = ty
        self._parent = None

    def __call__(self, count=1):
        assert count >= 0
        for _ in range(count):
            self.tap()
        return self._shared_event_gen

    def _key_press_release(self, do_press=False, do_release=False, unicode_override=None):
        assert (do_press or do_release)
        keys_held = self._shared_event_gen._event_types_held
        build_keys = []
        e = self
        while e is not None:
            build_keys.append(e._event_type.upper())
            e = e._parent
        build_keys.reverse()

        events = [None, None]
        for i, value in enumerate(('PRESS', 'RELEASE')):
            if value == 'RELEASE':
                build_keys.reverse()
            for event_type in build_keys:
                if value == 'PRESS':
                    keys_held.add(event_type)
                else:
                    keys_held.remove(event_type)

                if (not do_press) and value == 'PRESS':
                    continue
                if (not do_release) and value == 'RELEASE':
                    continue

                shift = 'LEFT_SHIFT' in keys_held or 'RIGHT_SHIFT' in keys_held
                ctrl = 'LEFT_CTRL' in keys_held or 'RIGHT_CTRL' in keys_held
                shift = 'LEFT_SHIFT' in keys_held or 'RIGHT_SHIFT' in keys_held
                alt = 'LEFT_ALT' in keys_held or 'RIGHT_ALT' in keys_held
                oskey = 'OSKEY' in keys_held
                hyper = 'HYPER' in keys_held

                unicode = None
                if value == 'PRESS':
                    if ctrl is False and alt is False and oskey is False:
                        if unicode_override is not None:
                            unicode = unicode_override
                        else:
                            unicode = event_types_text_from_event.get((event_type, shift))
                            if unicode is None and shift:
                                # Some keys don't care about shift
                                unicode = event_types_text_from_event.get((event_type, False))

                event = self._shared_event_gen.window.event_simulate(
                    type=event_type,
                    value=value,
                    unicode=unicode,
                    shift=shift,
                    ctrl=ctrl,
                    alt=alt,
                    oskey=oskey,
                    hyper=hyper,
                    x=self._shared_event_gen._mouse_co[0],
                    y=self._shared_event_gen._mouse_co[1],
                )
                events[i] = event
        return tuple(events)

    def tap(self):
        return self._key_press_release(do_press=True, do_release=True)

    def press(self):
        return self._key_press_release(do_press=True)[0]

    def release(self):
        return self._key_press_release(do_release=True)[1]

    def cursor_motion(self, coords):
        coords = list(coords)
        self._shared_event_gen.cursor_position_set(*coords[0], move=True)
        yield

        event = self.press()
        shift = event.shift
        ctrl = event.ctrl
        shift = event.shift
        alt = event.alt
        oskey = event.oskey
        hyper = event.hyper
        yield

        for x, y in coords:
            self._shared_event_gen.window.event_simulate(
                type='MOUSEMOVE',
                value='NOTHING',
                unicode=None,
                shift=shift,
                ctrl=ctrl,
                alt=alt,
                oskey=oskey,
                hyper=hyper,
                x=x,
                y=y
            )
            yield
        self._shared_event_gen.cursor_position_set(x, y, move=False)
        self.release()
        yield

    def __getattr__(self, attr):
        attr = event_types_alias.get(attr, attr)
        if attr in event_types:
            e = _EventBuilder(self._shared_event_gen, attr)
            e._parent = self
            return e
        raise Exception(f"{attr!r} not found in {event_types!r}")


class EventGenerate:
    __slots__ = (
        "window",

        "_mouse_co",
        "_event_types_held",
    )

    def __init__(self, window):
        self.window = window
        self._mouse_co = [0, 0]
        self._event_types_held = set()

        self.cursor_position_set(window.width // 2, window.height // 2)

    def cursor_position_set(self, x, y, move=False):
        self._mouse_co[:] = x, y
        if move:
            self.window.event_simulate(
                type='MOUSEMOVE',
                value='NOTHING',
                x=x,
                y=y,
            )

    def text(self, text):
        """ Type in entire phrases. """
        for ch in text:
            ty, shift = event_types_text_from_char[ch]
            ty = ty.lower()
            if shift:
                eb = getattr(_EventBuilder(self, 'left_shift'), ty)
            else:
                eb = _EventBuilder(self, ty)
            eb.tap()
        return self

    def text_unicode(self, text):
        # Since the only purpose of this key-press is to enter text
        # the key can be almost anything, use a key which isn't likely to be assigned to any other action.
        #
        # If it were possible `EVT_UNKNOWNKEY` would be most correct
        # as dead keys map to this and still enter text.
        ty_dummy = 'F24'
        for ch in text:
            eb = _EventBuilder(self, ty_dummy)
            eb._key_press_release(do_press=True, do_release=True, unicode_override=ch)
        return self

    def __getattr__(self, attr):
        attr = event_types_alias.get(attr, attr)
        if attr in event_types:
            return _EventBuilder(self, attr)
        raise Exception(f"{attr!r} not found in {event_types!r}")

    def __del__(self):
        if self._event_types_held:
            print("'__del__' with keys held:", repr(self._event_types_held))


def run(
        event_iter, *,
        on_error=None,
        on_exit=None,
        on_step_command_pre=None,
        on_step_command_post=None,
):
    import bpy

    TICKS = 4  # 3 works, 4  to be on the safe side.

    def event_step():
        # Run once 'TICKS' is reached.
        if event_step._ticks < TICKS:
            event_step._ticks += 1
            return 0.0
        event_step._ticks = 0

        if on_step_command_pre:
            if event_step.run_events.gi_frame is not None:
                import shlex
                import subprocess
                subprocess.call(
                    shlex.split(
                        on_step_command_pre.replace(
                            "{file}", event_step.run_events.gi_frame.f_code.co_filename,
                        ).replace(
                            "{line}", str(event_step.run_events.gi_frame.f_lineno),
                        )
                    )
                )
        try:
            val = next(event_step.run_events, Ellipsis)
        except Exception:
            import traceback
            traceback.print_exc()
            if on_error is not None:
                on_error()
            if on_exit is not None:
                on_exit()
            return None

        if on_step_command_post:
            if event_step.run_events.gi_frame is not None:
                import shlex
                import subprocess
                subprocess.call(
                    shlex.split(
                        on_step_command_post.replace(
                            "{file}", event_step.run_events.gi_frame.f_code.co_filename,
                        ).replace(
                            "{line}", str(event_step.run_events.gi_frame.f_lineno),
                        )
                    )
                )

        if isinstance(val, EventGenerate) or val is None:
            return 0.0
        elif isinstance(val, datetime.timedelta):
            return val.total_seconds()
        elif val is Ellipsis:
            if on_exit is not None:
                on_exit()
            return None
        else:
            raise Exception(f"{val!r} of type {type(val)!r} not supported")

    event_step.run_events = iter(event_iter)
    event_step._ticks = 0

    bpy.app.timers.register(event_step, first_interval=0.0)


def setup_default_preferences(preferences):
    """ Set preferences useful for automation.
    """
    preferences.view.show_splash = False
    preferences.view.smooth_view = 0
    preferences.view.use_save_prompt = False
    preferences.filepaths.use_auto_save_temporary_files = False
