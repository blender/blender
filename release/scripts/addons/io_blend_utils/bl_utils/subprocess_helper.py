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

"""
Defines an operator mix-in to use for non-blocking command line access.
"""


class SubprocessHelper:
    """
    Mix-in class for operators to run commands in a non-blocking way.

    This uses a modal operator to manage an external process.

    Subclass must define:
        ``command``:
            List of arguments to pass to subprocess.Popen
            report_interval: Time in seconds between updating reports.

        ``process_pre()``:
            Callback that runs before the process executes.

        ``process_post(returncode)``:
            Callback that runs when the process has ende.
            returncode is -1 if the process was terminated.

    Subclass may define:
        ``environment``:
            Dict of environment variables exposed to the subprocess.
            Contrary to the subprocess.Popen(env=...) parameter, this
            dict is and not used to replace the existing environment
            entirely, but is just used to update it.
    """

    environ = {}
    command = ()

    @staticmethod
    def _non_blocking_readlines(f, chunk=64):
        """
        Iterate over lines, yielding b'' when nothings left
        or when new data is not yet available.
        """
        import os

        from .pipe_non_blocking import (
                pipe_non_blocking_set,
                pipe_non_blocking_is_error_blocking,
                PortableBlockingIOError,
                )

        fd = f.fileno()
        pipe_non_blocking_set(fd)

        blocks = []

        while True:
            try:
                data = os.read(fd, chunk)
                if not data:
                    # case were reading finishes with no trailing newline
                    yield b''.join(blocks)
                    blocks.clear()
            except PortableBlockingIOError as ex:
                if not pipe_non_blocking_is_error_blocking(ex):
                    raise ex

                yield b''
                continue

            while True:
                n = data.find(b'\n')
                if n == -1:
                    break

                yield b''.join(blocks) + data[:n + 1]
                data = data[n + 1:]
                blocks.clear()
            blocks.append(data)

    def _report_output(self):
        stdout_line_iter, stderr_line_iter = self._buffer_iter
        for line_iter, report_type in (
                (stdout_line_iter, {'INFO'}),
                (stderr_line_iter, {'WARNING'})
                ):
            while True:
                line = next(line_iter).rstrip()  # rstrip all, to include \r on windows
                if not line:
                    break
                self.report(report_type, line.decode(encoding='utf-8', errors='surrogateescape'))

    def _wm_enter(self, context):
        wm = context.window_manager
        window = context.window

        self._timer = wm.event_timer_add(self.report_interval, window)
        window.cursor_set('WAIT')

    def _wm_exit(self, context):
        wm = context.window_manager
        window = context.window

        wm.event_timer_remove(self._timer)
        window.cursor_set('DEFAULT')

    def process_pre(self):
        pass

    def process_post(self, returncode):
        pass

    def modal(self, context, event):
        wm = context.window_manager
        p = self._process

        if event.type == 'ESC':
            self.cancel(context)
            self.report({'INFO'}, "Operation aborted by user")
            return {'CANCELLED'}

        elif event.type == 'TIMER':
            if p.poll() is not None:
                self._report_output()
                self._wm_exit(context)
                self.process_post(p.returncode)
                return {'FINISHED'}

            self._report_output()

        return {'PASS_THROUGH'}

    def execute(self, context):
        import subprocess
        import os
        import copy

        self.process_pre()

        env = copy.deepcopy(os.environ)
        env.update(self.environ)

        try:
            p = subprocess.Popen(
                    self.command,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    env=env,
                    )
        except FileNotFoundError as ex:
            # Command not found
            self.report({'ERROR'}, str(ex))
            return {'CANCELLED'}

        self._process = p
        self._buffer_iter = (
                iter(self._non_blocking_readlines(p.stdout)),
                iter(self._non_blocking_readlines(p.stderr)),
                )

        wm = context.window_manager
        wm.modal_handler_add(self)

        self._wm_enter(context)

        return {'RUNNING_MODAL'}

    def cancel(self, context):
        self._wm_exit(context)
        self._process.kill()
        self.process_post(-1)

