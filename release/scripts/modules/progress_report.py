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

import time


class ProgressReport:
    """
    A basic 'progress report' using either simple prints in console, or WindowManager's 'progress' API.

    This object can be used as a context manager.

    It supports multiple levels of 'substeps' - you shall always enter at least one substep (because level 0
    has only one single step, representing the whole 'area' of the progress stuff).

    You should give the expected number of substeps each time you enter a new one (you may then step more or less then
    given number, but this will give incoherent progression).

    Leaving a substep automatically steps by one the parent level.

        with ProgressReport() as progress:  # Not giving a WindowManager here will default to console printing.
            progress.enter_substeps(10)
            for i in range(10):
                progress.enter_substeps(100)
                for j in range(100):
                    progress.step()
                progress.leave_substeps()  # No need to step here, this implicitly does it.
            progress.leave_substeps("Finished!")  # You may pass some message too.
    """
    __slots__ = ('wm', 'running', 'steps', 'curr_step', 'start_time')

    def __init__(self, wm=None):
        self_wm = getattr(self, 'wm', None)
        if self_wm:
            self.finalize()
        self.running = False

        self.wm = wm
        self.steps = [100000]
        self.curr_step = [0]

    initialize = __init__

    def __enter__(self):
        self.start_time = [time.time()]
        if self.wm:
            self.wm.progress_begin(0, self.steps[0])
            self.update()
            self.running = True
        return self

    def __exit__(self, exc_type=None, exc_value=None, traceback=None):
        self.running = False
        if self.wm:
            self.wm.progress_end()
            self.wm = None
        print("\n")
        self.steps = [100000]
        self.curr_step = [0]
        self.start_time = [time.time()]

    def start(self):
        self.__enter__()

    def finalize(self):
        self.__exit__()

    def update(self, msg=""):
        steps = sum(s * cs for (s, cs) in zip(self.steps, self.curr_step))
        steps_percent = steps / self.steps[0] * 100.0
        tm = time.time()
        loc_tm = tm - self.start_time[-1]
        tm -= self.start_time[0]
        if self.wm and self.running:
            self.wm.progress_update(steps)
        if msg:
            prefix = "  " * (len(self.steps) - 1)
            print(prefix + "(%8.4f sec | %8.4f sec) %s\nProgress: %6.2f%%\r" %
                  (tm, loc_tm, msg, steps_percent), end='')
        else:
            print("Progress: %6.2f%%\r" % (steps_percent,), end='')

    def enter_substeps(self, nbr, msg=""):
        if msg:
            self.update(msg)
        self.steps.append(self.steps[-1] / max(nbr, 1))
        self.curr_step.append(0)
        self.start_time.append(time.time())

    def step(self, msg="", nbr=1):
        self.curr_step[-1] += nbr
        self.update(msg)

    def leave_substeps(self, msg=""):
        if (msg):
            self.update(msg)
        assert(len(self.steps) > 1)
        del self.steps[-1]
        del self.curr_step[-1]
        del self.start_time[-1]
        self.step()


class ProgressReportSubstep:
    """
    A sub-step context manager for ProgressReport.

    It can be used to generate other sub-step contexts too, and can act as a (limited) proxy of its real ProgressReport.

    Its exit method always ensure ProgressReport is back on 'level' it was before entering this context.
    This means it is especially useful to ensure a coherent behavior around code that could return/continue/break
    from many places, without having to bother to explicitly leave substep in each and every possible place!

        with ProgressReport() as progress:  # Not giving a WindowManager here will default to console printing.
            with ProgressReportSubstep(progress, 10, final_msg="Finished!") as subprogress1:
                for i in range(10):
                    with ProgressReportSubstep(subprogress1, 100) as subprogress2:
                        for j in range(100):
                            subprogress2.step()
    """
    __slots__ = ('progress', 'nbr', 'msg', 'final_msg', 'level')

    def __init__(self, progress, nbr, msg="", final_msg=""):
        # Allows to generate a subprogress context handler from another one.
        progress = getattr(progress, 'progress', progress)

        self.progress = progress
        self.nbr = nbr
        self.msg = msg
        self.final_msg = final_msg

    def __enter__(self):
        self.level = len(self.progress.steps)
        self.progress.enter_substeps(self.nbr, self.msg)
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        assert(len(self.progress.steps) > self.level)
        while len(self.progress.steps) > self.level + 1:
            self.progress.leave_substeps()
        self.progress.leave_substeps(self.final_msg)

    def enter_substeps(self, nbr, msg=""):
        self.progress.enter_substeps(nbr, msg)

    def step(self, msg="", nbr=1):
        self.progress.step(msg, nbr)

    def leave_substeps(self, msg=""):
        self.progress.leave_substeps(msg)
