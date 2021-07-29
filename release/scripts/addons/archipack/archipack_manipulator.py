# -*- coding:utf-8 -*-

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

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
#
# ----------------------------------------------------------
import bpy
from math import atan2, pi
from mathutils import Vector, Matrix
from mathutils.geometry import intersect_line_plane, intersect_point_line, intersect_line_sphere
from bpy_extras import view3d_utils
from bpy.types import PropertyGroup, Operator
from bpy.props import FloatVectorProperty, StringProperty, CollectionProperty, BoolProperty
from bpy.app.handlers import persistent
from .archipack_snap import snap_point
from .archipack_keymaps import Keymaps
from .archipack_gl import (
    GlLine, GlArc, GlText,
    GlPolyline, GlPolygon,
    TriHandle, SquareHandle, EditableText,
    FeedbackPanel, GlCursorArea
)


# NOTE:
# Snap aware manipulators use a dirty hack :
# draw() as a callback to update values in realtime
# as transform.translate in use to allow snap
# does catch all events.
# This however has a wanted side effect:
# the manipulator take precedence over allready running
# ones, and prevent select mode to start.
#
# TODO:
# Other manipulators should use same technique to take
# precedence over allready running ones when active
#
# NOTE:
# Select mode does suffer from this stack effect:
# the last running wins. The point is left mouse select mode
# requiring left drag to be RUNNING_MODAL to prevent real
# objects select and move during manipulators selection.
#
# TODO:
# First run a separate modal dedicated to select mode.
# Selecting in whole manips stack when required
# (manips[key].manipulable.manip_stack)
# Must investigate for a way to handle unselect after drag done.

"""
    @TODO:
    Last modal running wins.
    Manipulateurs without snap and thus not running own modal,
    may loose events events caught by select mode of last
    manipulable enabled
"""

# Arrow sizes (world units)
arrow_size = 0.05
# Handle area size (pixels)
handle_size = 10


# a global manipulator stack reference
# prevent Blender "ACCESS_VIOLATION" crashes
# use a dict to prevent collisions
# between many objects being in manipulate mode
# use object names as loose keys
# NOTE : use app.drivers to reset before file load
manips = {}


class ArchipackActiveManip:
    """
        Store manipulated object
        - object_name: manipulated object name
        - stack: array of Manipulators instances
        - manipulable: Manipulable instance
    """
    def __init__(self, object_name):
        self.object_name = object_name
        # manipulators stack for object
        self.stack = []
        # reference to object manipulable instance
        self.manipulable = None

    @property
    def dirty(self):
        """
            Check for manipulable validity
            to disable modal when required
        """
        return (
            self.manipulable is None or
            bpy.data.objects.find(self.object_name) < 0
            )

    def exit(self):
        """
            Exit manipulation mode
            - exit from all running manipulators
            - empty manipulators stack
            - set manipulable.manipulate_mode to False
            - remove reference to manipulable
        """
        for m in self.stack:
            if m is not None:
                m.exit()
        if self.manipulable is not None:
            self.manipulable.manipulate_mode = False
            self.manipulable = None
        self.object_name = ""
        self.stack.clear()


def remove_manipulable(key):
    """
        disable and remove a manipulable from stack
    """
    global manips
    # print("remove_manipulable key:%s" % (key))
    if key in manips.keys():
        manips[key].exit()
        manips.pop(key)


def check_stack(key):
    """
        check for stack item validity
        use in modal to destroy invalid modals
        return true when invalid / not found
        false when valid
    """
    global manips
    if key not in manips.keys():
        # print("check_stack : key not found %s" % (key))
        return True
    elif manips[key].dirty:
        # print("check_stack : key.dirty %s" % (key))
        remove_manipulable(key)
        return True

    return False


def empty_stack():
    # print("empty_stack()")
    """
        kill every manipulators in stack
        and cleanup stack
    """
    global manips
    for key in manips.keys():
        manips[key].exit()
    manips.clear()


def add_manipulable(key, manipulable):
    """
        add a ArchipackActiveManip into the stack
        if not allready present
        setup reference to manipulable
        return manipulators stack
    """
    global manips
    if key not in manips.keys():
        # print("add_manipulable() key:%s not found create new" % (key))
        manips[key] = ArchipackActiveManip(key)

    manips[key].manipulable = manipulable
    return manips[key].stack


# ------------------------------------------------------------------
# Define Manipulators
# ------------------------------------------------------------------


class Manipulator():
    """
        Manipulator base class to derive other
        handle keyboard and modal events
        provide convenient funcs including getter and setter for datablock values
        store reference of base object, datablock and manipulator
    """
    keyboard_ascii = {
            ".", ",", "-", "+", "1", "2", "3",
            "4", "5", "6", "7", "8", "9", "0",
            "c", "m", "d", "k", "h", "a",
            " ", "/", "*", "'", "\""
            # "="
            }
    keyboard_type = {
            'BACK_SPACE', 'DEL',
            'LEFT_ARROW', 'RIGHT_ARROW'
            }

    def __init__(self, context, o, datablock, manipulator, snap_callback=None):
        """
            o : object to manipulate
            datablock : object data to manipulate
            manipulator: object archipack_manipulator datablock
            snap_callback: on snap enabled manipulators, will be called when drag occurs
        """
        self.keymap = Keymaps(context)
        self.feedback = FeedbackPanel()
        self.active = False
        self.selectable = False
        self.selected = False
        # active text input value for manipulator
        self.keyboard_input_active = False
        self.label_value = 0
        # unit for keyboard input value
        self.value_type = 'LENGTH'
        self.pts_mode = 'SIZE'
        self.o = o
        self.datablock = datablock
        self.manipulator = manipulator
        self.snap_callback = snap_callback
        self.origin = Vector((0, 0, 1))
        self.mouse_pos = Vector((0, 0))
        self.length_entered = ""
        self.line_pos = 0
        args = (self, context)
        self._handle = bpy.types.SpaceView3D.draw_handler_add(self.draw_callback, args, 'WINDOW', 'POST_PIXEL')

    @classmethod
    def poll(cls, context):
        """
            Allow manipulator enable/disable
            in given context
            handles will not show
        """
        return True

    def exit(self):
        """
            Modal exit, DONT EVEN TRY TO OVERRIDE
        """
        if self._handle is not None:
            bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
            self._handle = None
        else:
            print("Manipulator.exit() handle not found %s" % (type(self).__name__))

    # Mouse event handlers, MUST be overriden
    def mouse_press(self, context, event):
        """
            Manipulators must implement
            mouse press event handler
            return True to callback manipulable_manipulate
        """
        raise NotImplementedError

    def mouse_release(self, context, event):
        """
            Manipulators must implement
            mouse mouse_release event handler
            return False to callback manipulable_release
        """
        raise NotImplementedError

    def mouse_move(self, context, event):
        """
            Manipulators must implement
            mouse move event handler
            return True to callback manipulable_manipulate
        """
        raise NotImplementedError

    # Keyboard event handlers, MAY be overriden
    def keyboard_done(self, context, event, value):
        """
            Manipulators may implement
            keyboard value validated event handler
            value: changed by keyboard
            return True to callback manipulable_manipulate
        """
        return False

    def keyboard_editing(self, context, event, value):
        """
            Manipulators may implement
            keyboard value changed event handler
            value: string changed by keyboard
            allow realtime update of label
            return False to show edited value on window header
            return True when feedback show right on screen
        """
        self.label_value = value
        return True

    def keyboard_cancel(self, context, event):
        """
            Manipulators may implement
            keyboard entry cancelled
        """
        return

    def cancel(self, context, event):
        """
            Manipulators may implement
            cancelled event (ESC RIGHTCLICK)
        """
        self.active = False
        return

    def undo(self, context, event):
        """
            Manipulators may implement
            undo event (CTRL+Z)
        """
        return False

    # Internal, do not override unless you realy
    # realy realy deeply know what you are doing
    def keyboard_eval(self, context, event):
        """
            evaluate keyboard entry while typing
            do not override this one
        """
        c = event.ascii
        if c:
            if c == ",":
                c = "."
            self.length_entered = self.length_entered[:self.line_pos] + c + self.length_entered[self.line_pos:]
            self.line_pos += 1

        if self.length_entered:
            if event.type == 'BACK_SPACE':
                self.length_entered = self.length_entered[:self.line_pos - 1] + self.length_entered[self.line_pos:]
                self.line_pos -= 1

            elif event.type == 'DEL':
                self.length_entered = self.length_entered[:self.line_pos] + self.length_entered[self.line_pos + 1:]

            elif event.type == 'LEFT_ARROW':
                self.line_pos = (self.line_pos - 1) % (len(self.length_entered) + 1)

            elif event.type == 'RIGHT_ARROW':
                self.line_pos = (self.line_pos + 1) % (len(self.length_entered) + 1)

        try:
            value = bpy.utils.units.to_value(context.scene.unit_settings.system, self.value_type, self.length_entered)
            draw_on_header = self.keyboard_editing(context, event, value)
        except:  # ValueError:
            draw_on_header = True
            pass

        if draw_on_header:
            a = ""
            if self.length_entered:
                pos = self.line_pos
                a = self.length_entered[:pos] + '|' + self.length_entered[pos:]
            context.area.header_text_set("%s" % (a))

        # modal mode: do not let event bubble up
        return True

    def modal(self, context, event):
        """
            Modal handler
            handle mouse, and keyboard events
            enable and disable feedback
        """
        # print("Manipulator modal:%s %s" % (event.value, event.type))

        if event.type == 'MOUSEMOVE':
            return self.mouse_move(context, event)

        elif event.value == 'PRESS':

            if event.type == 'LEFTMOUSE':
                active = self.mouse_press(context, event)
                if active:
                    self.feedback.enable()
                return active

            elif self.keymap.check(event, self.keymap.undo):
                if self.keyboard_input_active:
                    self.keyboard_input_active = False
                    self.keyboard_cancel(context, event)
                self.feedback.disable()
                # prevent undo CRASH
                return True

            elif self.keyboard_input_active and (
                    event.ascii in self.keyboard_ascii or
                    event.type in self.keyboard_type
                    ):
                # get keyboard input
                return self.keyboard_eval(context, event)

            elif event.type in {'ESC', 'RIGHTMOUSE'}:
                self.feedback.disable()
                if self.keyboard_input_active:
                    # allow keyboard exit without setting value
                    self.length_entered = ""
                    self.line_pos = 0
                    self.keyboard_input_active = False
                    self.keyboard_cancel(context, event)
                    return True
                elif self.active:
                    self.cancel(context, event)
                    return True
                return False

        elif event.value == 'RELEASE':

            if event.type == 'LEFTMOUSE':
                if not self.keyboard_input_active:
                    self.feedback.disable()
                return self.mouse_release(context, event)

            elif self.keyboard_input_active and event.type in {'RET', 'NUMPAD_ENTER'}:
                # validate keyboard input
                if self.length_entered != "":
                    try:
                        value = bpy.utils.units.to_value(
                            context.scene.unit_settings.system,
                            self.value_type, self.length_entered)
                        self.length_entered = ""
                        ret = self.keyboard_done(context, event, value)
                    except:  # ValueError:
                        ret = False
                        self.keyboard_cancel(context, event)
                        pass
                    context.area.header_text_set()
                    self.keyboard_input_active = False
                    self.feedback.disable()
                    return ret

        return False

    def mouse_position(self, event):
        """
            store mouse position in a 2d Vector
        """
        self.mouse_pos.x, self.mouse_pos.y = event.mouse_region_x, event.mouse_region_y

    def get_pos3d(self, context):
        """
            convert mouse pos to 3d point over plane defined by origin and normal
            pt is in world space
        """
        region = context.region
        rv3d = context.region_data
        rM = context.active_object.matrix_world.to_3x3()
        view_vector_mouse = view3d_utils.region_2d_to_vector_3d(region, rv3d, self.mouse_pos)
        ray_origin_mouse = view3d_utils.region_2d_to_origin_3d(region, rv3d, self.mouse_pos)
        pt = intersect_line_plane(ray_origin_mouse, ray_origin_mouse + view_vector_mouse,
            self.origin, rM * self.manipulator.normal, False)
        # fix issue with parallel plane
        if pt is None:
            pt = intersect_line_plane(ray_origin_mouse, ray_origin_mouse + view_vector_mouse,
                self.origin, view_vector_mouse, False)
        return pt

    def get_value(self, data, attr, index=-1):
        """
            Datablock value getter with index support
        """
        try:
            if index > -1:
                return getattr(data, attr)[index]
            else:
                return getattr(data, attr)
        except:
            print("get_value of %s %s failed" % (data, attr))
            return 0

    def set_value(self, context, data, attr, value, index=-1):
        """
            Datablock value setter with index support
        """
        try:
            if self.get_value(data, attr, index) != value:
                # switch context so unselected object may be manipulable too
                old = context.active_object
                state = self.o.select
                self.o.select = True
                context.scene.objects.active = self.o
                if index > -1:
                    getattr(data, attr)[index] = value
                else:
                    setattr(data, attr, value)
                self.o.select = state
                old.select = True
                context.scene.objects.active = old
        except:
            pass

    def preTranslate(self, tM, vec):
        """
            return a preTranslated Matrix
            tM Matrix source
            vec Vector translation
        """
        return tM * Matrix([
        [1, 0, 0, vec.x],
        [0, 1, 0, vec.y],
        [0, 0, 1, vec.z],
        [0, 0, 0, 1]])

    def _move(self, o, axis, value):
        if axis == 'x':
            vec = Vector((value, 0, 0))
        elif axis == 'y':
            vec = Vector((0, value, 0))
        else:
            vec = Vector((0, 0, value))
        o.matrix_world = self.preTranslate(o.matrix_world, vec)

    def move_linked(self, context, axis, value):
        """
            Move an object along local axis
            takes care of linked too, fix issue #8
        """
        old = context.active_object
        bpy.ops.object.select_all(action='DESELECT')
        self.o.select = True
        context.scene.objects.active = self.o
        bpy.ops.object.select_linked(type='OBDATA')
        for o in context.selected_objects:
            if o != self.o:
                self._move(o, axis, value)
        bpy.ops.object.select_all(action='DESELECT')
        old.select = True
        context.scene.objects.active = old

    def move(self, context, axis, value):
        """
            Move an object along local axis
        """
        self._move(self.o, axis, value)


# OUT OF ORDER
class SnapPointManipulator(Manipulator):
    """
        np_station based snap manipulator
        dosent update anything by itself.
        NOTE : currently out of order
        and disabled in __init__
    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):

        raise NotImplementedError

        self.handle = SquareHandle(handle_size, 1.2 * arrow_size, draggable=True)
        Manipulator.__init__(self, context, o, datablock, manipulator, snap_callback)

    def check_hover(self):
        self.handle.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        if self.handle.hover:
            self.handle.hover = False
            self.handle.active = True
            self.o.select = True
            # takeloc = self.o.matrix_world * self.manipulator.p0
            # print("Invoke sp_point_move %s" % (takeloc))
            # @TODO:
            # implement and add draw and callbacks
            # snap_point(takeloc, draw, callback)
            return True
        return False

    def mouse_release(self, context, event):
        self.check_hover()
        self.handle.active = False
        # False to callback manipulable_release
        return False

    def update(self, context, event):
        # NOTE:
        # dosent set anything internally
        return

    def mouse_move(self, context, event):
        """

        """
        self.mouse_position(event)
        if self.handle.active:
            # self.handle.active = np_snap.is_running
            # self.update(context)
            # True here to callback manipulable_manipulate
            return True
        else:
            self.check_hover()
        return False

    def draw_callback(self, _self, context, render=False):
        left, right, side, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.handle.set_pos(context, left, Vector((1, 0, 0)), normal=normal)
        self.handle.draw(context, render)


# Generic snap tool for line based archipack objects (fence, wall, maybe stair too)
gl_pts3d = []


class WallSnapManipulator(Manipulator):
    """
        np_station snap inspired manipulator
        Use prop1_name as string part index
        Use prop2_name as string identifier height property for placeholders

        Misnamed as it work for all line based archipack's
        primitives, currently wall and fences,
        but may also work with stairs (sharing same data structure)
    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        self.placeholder_area = GlPolygon((0.5, 0, 0, 0.2))
        self.placeholder_line = GlPolyline((0.5, 0, 0, 0.8))
        self.placeholder_line.closed = True
        self.label = GlText()
        self.line = GlLine()
        self.handle = SquareHandle(handle_size, 1.2 * arrow_size, draggable=True, selectable=True)
        Manipulator.__init__(self, context, o, datablock, manipulator, snap_callback)
        self.selectable = True

    def select(self, cursor_area):
        self.selected = self.selected or cursor_area.in_area(self.handle.pos_2d)
        self.handle.selected = self.selected

    def deselect(self, cursor_area):
        self.selected = not cursor_area.in_area(self.handle.pos_2d)
        self.handle.selected = self.selected

    def check_hover(self):
        self.handle.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        global gl_pts3d
        global manips
        if self.handle.hover:
            self.active = True
            self.handle.active = True
            gl_pts3d = []
            idx = int(self.manipulator.prop1_name)

            # get selected manipulators idx
            selection = []
            for m in manips[self.o.name].stack:
                if m is not None and m.selected:
                    selection.append(int(m.manipulator.prop1_name))

            # store all points of wall
            for i, part in enumerate(self.datablock.parts):
                p0, p1, side, normal = part.manipulators[2].get_pts(self.o.matrix_world)
                # if selected p0 will move and require placeholder
                gl_pts3d.append((p0, p1, i in selection or i == idx))

            self.feedback.instructions(context, "Move / Snap", "Drag to move, use keyboard to input values", [
                ('CTRL', 'Snap'),
                ('X Y', 'Constraint to axis (toggle Global Local None)'),
                ('SHIFT+Z', 'Constraint to xy plane'),
                ('MMBTN', 'Constraint to axis'),
                ('RIGHTCLICK or ESC', 'exit without change')
                ])
            self.feedback.enable()
            self.handle.hover = False
            self.o.select = True
            takeloc, right, side, dz = self.manipulator.get_pts(self.o.matrix_world)
            dx = (right - takeloc).normalized()
            dy = dz.cross(dx)
            takemat = Matrix([
                [dx.x, dy.x, dz.x, takeloc.x],
                [dx.y, dy.y, dz.y, takeloc.y],
                [dx.z, dy.z, dz.z, takeloc.z],
                [0, 0, 0, 1]
            ])
            snap_point(takemat=takemat, draw=self.sp_draw, callback=self.sp_callback,
                constraint_axis=(True, True, False))
            # this prevent other selected to run
            return True

        return False

    def mouse_release(self, context, event):
        self.check_hover()
        self.handle.active = False
        self.active = False
        self.feedback.disable()
        # False to callback manipulable_release
        return False

    def sp_callback(self, context, event, state, sp):
        """
            np station callback on moving, place, or cancel
        """
        global gl_pts3d

        if state == 'SUCCESS':

            self.o.select = True
            # apply changes to wall
            d = self.datablock
            d.auto_update = False

            g = d.get_generator()

            # rotation relative to object
            rM = self.o.matrix_world.inverted().to_3x3()
            delta = (rM * sp.delta).to_2d()
            # x_axis = (rM * Vector((1, 0, 0))).to_2d()

            # update generator
            idx = 0
            for p0, p1, selected in gl_pts3d:

                if selected:

                    # new location in object space
                    pt = g.segs[idx].lerp(0) + delta

                    # move last point of segment before current
                    if idx > 0:
                        g.segs[idx - 1].p1 = pt

                    # move first point of current segment
                    g.segs[idx].p0 = pt

                idx += 1

            # update properties from generator
            idx = 0
            for p0, p1, selected in gl_pts3d:

                if selected:

                    # adjust segment before current
                    if idx > 0:
                        w = g.segs[idx - 1]
                        part = d.parts[idx - 1]

                        if idx > 1:
                            part.a0 = w.delta_angle(g.segs[idx - 2])
                        else:
                            part.a0 = w.straight(1, 0).angle

                        if "C_" in part.type:
                            part.radius = w.r
                        else:
                            part.length = w.length

                    # adjust current segment
                    w = g.segs[idx]
                    part = d.parts[idx]

                    if idx > 0:
                        part.a0 = w.delta_angle(g.segs[idx - 1])
                    else:
                        part.a0 = w.straight(1, 0).angle
                        # move object when point 0
                        self.o.location += sp.delta
                        self.o.matrix_world.translation += sp.delta

                    if "C_" in part.type:
                        part.radius = w.r
                    else:
                        part.length = w.length

                    # adjust next one
                    if idx + 1 < d.n_parts:
                        d.parts[idx + 1].a0 = g.segs[idx + 1].delta_angle(w)

                idx += 1

            self.mouse_release(context, event)
            d.auto_update = True

        if state == 'CANCEL':
            self.mouse_release(context, event)

        return

    def sp_draw(self, sp, context):
        # draw wall placeholders

        global gl_pts3d

        if self.o is None:
            return

        z = self.get_value(self.datablock, self.manipulator.prop2_name)

        placeholders = []
        for p0, p1, selected in gl_pts3d:
            pt = p0.copy()
            if selected:
                # when selected, p0 is moving
                # last one p1 should move too
                # last one require a placeholder too
                pt += sp.delta
                if len(placeholders) > 0:
                    placeholders[-1][1] = pt
                    placeholders[-1][2] = True
            placeholders.append([pt, p1, selected])

        # first selected and closed -> should move last p1 too
        if gl_pts3d[0][2] and self.datablock.closed:
            placeholders[-1][1] = placeholders[0][0].copy()
            placeholders[-1][2] = True

        # last one not visible when not closed
        if not self.datablock.closed:
            placeholders[-1][2] = False

        for p0, p1, selected in placeholders:
            if selected:
                self.placeholder_area.set_pos([p0, p1, Vector((p1.x, p1.y, p1.z + z)), Vector((p0.x, p0.y, p0.z + z))])
                self.placeholder_line.set_pos([p0, p1, Vector((p1.x, p1.y, p1.z + z)), Vector((p0.x, p0.y, p0.z + z))])
                self.placeholder_area.draw(context, render=False)
                self.placeholder_line.draw(context, render=False)

        p0, p1, side, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.line.p = p0
        self.line.v = sp.delta
        self.label.set_pos(context, self.line.length, self.line.lerp(0.5), self.line.v, normal=Vector((0, 0, 1)))
        self.line.draw(context, render=False)
        self.label.draw(context, render=False)

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.handle.active:
            # False here to pass_through
            # print("i'm able to pick up mouse move event while transform running")
            return False
        else:
            self.check_hover()
        return False

    def draw_callback(self, _self, context, render=False):
        left, right, side, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.handle.set_pos(context, left, (left - right).normalized(), normal=normal)
        self.handle.draw(context, render)
        self.feedback.draw(context, render)


class CounterManipulator(Manipulator):
    """
        increase or decrease an integer step by step
        right on click to prevent misuse
    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        self.handle_left = TriHandle(handle_size, arrow_size, draggable=True)
        self.handle_right = TriHandle(handle_size, arrow_size, draggable=True)
        self.line_0 = GlLine()
        self.label = GlText()
        self.label.unit_mode = 'NONE'
        self.label.precision = 0
        Manipulator.__init__(self, context, o, datablock, manipulator, snap_callback)

    def check_hover(self):
        self.handle_right.check_hover(self.mouse_pos)
        self.handle_left.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        if self.handle_right.hover:
            value = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.set_value(context, self.datablock, self.manipulator.prop1_name, value + 1)
            self.handle_right.active = True
            return True
        if self.handle_left.hover:
            value = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.set_value(context, self.datablock, self.manipulator.prop1_name, value - 1)
            self.handle_left.active = True
            return True
        return False

    def mouse_release(self, context, event):
        self.check_hover()
        self.handle_right.active = False
        self.handle_left.active = False
        return False

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.handle_right.active:
            return True
        if self.handle_left.active:
            return True
        else:
            self.check_hover()
        return False

    def draw_callback(self, _self, context, render=False):
        """
            draw on screen feedback using gl.
        """
        # won't render counter
        if render:
            return
        left, right, side, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.origin = left
        self.line_0.p = left
        self.line_0.v = right - left
        self.line_0.z_axis = normal
        self.label.z_axis = normal
        value = self.get_value(self.datablock, self.manipulator.prop1_name)
        self.handle_left.set_pos(context, self.line_0.p, -self.line_0.v, normal=normal)
        self.handle_right.set_pos(context, self.line_0.lerp(1), self.line_0.v, normal=normal)
        self.label.set_pos(context, value, self.line_0.lerp(0.5), self.line_0.v, normal=normal)
        self.label.draw(context, render)
        self.handle_left.draw(context, render)
        self.handle_right.draw(context, render)


class DumbStringManipulator(Manipulator):
    """
        not a real manipulator, but allow to show a string
    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        self.label = GlText(colour=(0, 0, 0, 1))
        self.label.unit_mode = 'NONE'
        self.label.label = manipulator.prop1_name
        Manipulator.__init__(self, context, o, datablock, manipulator, snap_callback)

    def check_hover(self):
        return False

    def mouse_press(self, context, event):
        return False

    def mouse_release(self, context, event):
        return False

    def mouse_move(self, context, event):
        return False

    def draw_callback(self, _self, context, render=False):
        """
            draw on screen feedback using gl.
        """
        # won't render string
        if render:
            return
        left, right, side, normal = self.manipulator.get_pts(self.o.matrix_world)
        pos = left + 0.5 * (right - left)
        self.label.set_pos(context, None, pos, pos, normal=normal)
        self.label.draw(context, render)


class SizeManipulator(Manipulator):

    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        self.handle_left = TriHandle(handle_size, arrow_size)
        self.handle_right = TriHandle(handle_size, arrow_size, draggable=True)
        self.line_0 = GlLine()
        self.line_1 = GlLine()
        self.line_2 = GlLine()
        self.label = EditableText(handle_size, arrow_size, draggable=True)
        # self.label.label = 'S '
        Manipulator.__init__(self, context, o, datablock, manipulator, snap_callback)

    def check_hover(self):
        self.handle_right.check_hover(self.mouse_pos)
        self.label.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        global gl_pts3d
        if self.handle_right.hover:
            self.active = True
            self.original_size = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.original_location = self.o.matrix_world.translation.copy()
            self.feedback.instructions(context, "Size", "Drag or Keyboard to modify size", [
                ('CTRL', 'Snap'),
                ('SHIFT', 'Round'),
                ('RIGHTCLICK or ESC', 'cancel')
                ])
            left, right, side, dz = self.manipulator.get_pts(self.o.matrix_world)
            dx = (right - left).normalized()
            dy = dz.cross(dx)
            takemat = Matrix([
                [dx.x, dy.x, dz.x, right.x],
                [dx.y, dy.y, dz.y, right.y],
                [dx.z, dy.z, dz.z, right.z],
                [0, 0, 0, 1]
            ])
            gl_pts3d = [left, right]
            snap_point(takemat=takemat,
                draw=self.sp_draw,
                callback=self.sp_callback,
                constraint_axis=(True, False, False))
            self.handle_right.active = True
            return True
        if self.label.hover:
            self.feedback.instructions(context, "Size", "Use keyboard to modify size",
                [('ENTER', 'Validate'), ('RIGHTCLICK or ESC', 'cancel')])
            self.label.active = True
            self.keyboard_input_active = True
            return True
        return False

    def mouse_release(self, context, event):
        self.active = False
        self.check_hover()
        self.handle_right.active = False
        if not self.keyboard_input_active:
            self.feedback.disable()
        return False

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.active:
            self.update(context, event)
            return True
        else:
            self.check_hover()
        return False

    def cancel(self, context, event):
        if self.active:
            self.mouse_release(context, event)
            self.set_value(context, self.datablock, self.manipulator.prop1_name, self.original_size)

    def keyboard_done(self, context, event, value):
        self.set_value(context, self.datablock, self.manipulator.prop1_name, value)
        self.label.active = False
        return True

    def keyboard_cancel(self, context, event):
        self.label.active = False
        return False

    def update(self, context, event):
        # 0  1  2
        # |_____|
        #
        pt = self.get_pos3d(context)
        pt, t = intersect_point_line(pt, self.line_0.p, self.line_2.p)
        length = (self.line_0.p - pt).length
        if event.alt:
            length = round(length, 1)
        self.set_value(context, self.datablock, self.manipulator.prop1_name, length)

    def draw_callback(self, _self, context, render=False):
        """
            draw on screen feedback using gl.
        """
        left, right, side, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.origin = left
        self.line_1.p = left
        self.line_1.v = right - left
        self.line_0.z_axis = normal
        self.line_1.z_axis = normal
        self.line_2.z_axis = normal
        self.label.z_axis = normal
        self.line_0 = self.line_1.sized_normal(0, side.x * 1.1)
        self.line_2 = self.line_1.sized_normal(1, side.x * 1.1)
        self.line_1.offset(side.x * 1.0)
        self.handle_left.set_pos(context, self.line_1.p, -self.line_1.v, normal=normal)
        self.handle_right.set_pos(context, self.line_1.lerp(1), self.line_1.v, normal=normal)
        if not self.keyboard_input_active:
            self.label_value = self.line_1.length
        self.label.set_pos(context, self.label_value, self.line_1.lerp(0.5), self.line_1.v, normal=normal)
        self.line_0.draw(context, render)
        self.line_1.draw(context, render)
        self.line_2.draw(context, render)
        self.handle_left.draw(context, render)
        self.handle_right.draw(context, render)
        self.label.draw(context, render)
        self.feedback.draw(context, render)

    def sp_draw(self, sp, context):
        global gl_pts3d
        if self.o is None:
            return
        p0 = gl_pts3d[0].copy()
        p1 = gl_pts3d[1].copy()
        p1 += sp.delta
        self.sp_update(context, p0, p1)
        return

    def sp_callback(self, context, event, state, sp):

        if state == 'SUCCESS':
            self.sp_draw(sp, context)
            self.mouse_release(context, event)

        if state == 'CANCEL':
            p0 = gl_pts3d[0].copy()
            p1 = gl_pts3d[1].copy()
            self.sp_update(context, p0, p1)
            self.mouse_release(context, event)

    def sp_update(self, context, p0, p1):
        length = (p0 - p1).length
        self.set_value(context, self.datablock, self.manipulator.prop1_name, length)


class SizeLocationManipulator(SizeManipulator):
    """
        Handle resizing by any of the boundaries
        of objects with centered pivots
        so when size change, object should move of the
        half of the change in the direction of change.

        Also take care of moving linked objects too
        Changing size is not necessary as link does
        allredy handle this and childs panels are
        updated by base object.
    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        SizeManipulator.__init__(self, context, o, datablock, manipulator, handle_size, snap_callback)
        self.handle_left.draggable = True

    def check_hover(self):
        self.handle_right.check_hover(self.mouse_pos)
        self.handle_left.check_hover(self.mouse_pos)
        self.label.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        if self.handle_right.hover:
            self.active = True
            self.original_location = self.o.matrix_world.translation.copy()
            self.original_size = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.feedback.instructions(context, "Size", "Drag to modify size", [
                ('ALT', 'Round value'), ('RIGHTCLICK or ESC', 'cancel')
                ])
            self.handle_right.active = True
            return True
        if self.handle_left.hover:
            self.active = True
            self.original_location = self.o.matrix_world.translation.copy()
            self.original_size = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.feedback.instructions(context, "Size", "Drag to modify size", [
                ('ALT', 'Round value'), ('RIGHTCLICK or ESC', 'cancel')
                ])
            self.handle_left.active = True
            return True
        if self.label.hover:
            self.feedback.instructions(context, "Size", "Use keyboard to modify size",
                [('ENTER', 'Validate'), ('RIGHTCLICK or ESC', 'cancel')])
            self.label.active = True
            self.keyboard_input_active = True
            return True
        return False

    def mouse_release(self, context, event):
        self.active = False
        self.check_hover()
        self.handle_right.active = False
        self.handle_left.active = False
        if not self.keyboard_input_active:
            self.feedback.disable()
        return False

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.handle_right.active or self.handle_left.active:
            self.update(context, event)
            return True
        else:
            self.check_hover()
        return False

    def keyboard_done(self, context, event, value):
        self.set_value(context, self.datablock, self.manipulator.prop1_name, value)
        # self.move_linked(context, self.manipulator.prop2_name, dl)
        self.label.active = False
        self.feedback.disable()
        return True

    def cancel(self, context, event):
        if self.active:
            self.mouse_release(context, event)
            # must move back to original location
            itM = self.o.matrix_world.inverted()
            dl = self.get_value(itM * self.original_location, self.manipulator.prop2_name)

            self.move(context, self.manipulator.prop2_name, dl)
            self.set_value(context, self.datablock, self.manipulator.prop1_name, self.original_size)
            self.move_linked(context, self.manipulator.prop2_name, dl)

    def update(self, context, event):
        # 0  1  2
        # |_____|
        #
        pt = self.get_pos3d(context)
        pt, t = intersect_point_line(pt, self.line_0.p, self.line_2.p)

        len_0 = (pt - self.line_0.p).length
        len_1 = (pt - self.line_2.p).length

        length = max(len_0, len_1)

        if event.alt:
            length = round(length, 1)

        dl = length - self.line_1.length

        if len_0 > len_1:
            dl = 0.5 * dl
        else:
            dl = -0.5 * dl

        self.move(context, self.manipulator.prop2_name, dl)
        self.set_value(context, self.datablock, self.manipulator.prop1_name, length)
        self.move_linked(context, self.manipulator.prop2_name, dl)


class SnapSizeLocationManipulator(SizeLocationManipulator):
    """
        Snap aware extension of SizeLocationManipulator
        Handle resizing by any of the boundaries
        of objects with centered pivots
        so when size change, object should move of the
        half of the change in the direction of change.

        Also take care of moving linked objects too
        Changing size is not necessary as link does
        allredy handle this and childs panels are
        updated by base object.


    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        SizeLocationManipulator.__init__(self, context, o, datablock, manipulator, handle_size, snap_callback)

    def mouse_press(self, context, event):
        global gl_pts3d
        if self.handle_right.hover:
            self.active = True
            self.original_size = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.original_location = self.o.matrix_world.translation.copy()
            self.feedback.instructions(context, "Size", "Drag or Keyboard to modify size", [
                ('CTRL', 'Snap'),
                ('SHIFT', 'Round'),
                ('RIGHTCLICK or ESC', 'cancel')
                ])
            left, right, side, dz = self.manipulator.get_pts(self.o.matrix_world)
            dx = (right - left).normalized()
            dy = dz.cross(dx)
            takemat = Matrix([
                [dx.x, dy.x, dz.x, right.x],
                [dx.y, dy.y, dz.y, right.y],
                [dx.z, dy.z, dz.z, right.z],
                [0, 0, 0, 1]
            ])
            gl_pts3d = [left, right]
            snap_point(takemat=takemat,
            draw=self.sp_draw,
            callback=self.sp_callback,
            constraint_axis=(True, False, False))

            self.handle_right.active = True
            return True

        if self.handle_left.hover:
            self.active = True
            self.original_size = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.original_location = self.o.matrix_world.translation.copy()
            self.feedback.instructions(context, "Size", "Drag or Keyboard to modify size", [
                ('CTRL', 'Snap'),
                ('SHIFT', 'Round'),
                ('RIGHTCLICK or ESC', 'cancel')
                ])
            left, right, side, dz = self.manipulator.get_pts(self.o.matrix_world)
            dx = (left - right).normalized()
            dy = dz.cross(dx)
            takemat = Matrix([
                [dx.x, dy.x, dz.x, left.x],
                [dx.y, dy.y, dz.y, left.y],
                [dx.z, dy.z, dz.z, left.z],
                [0, 0, 0, 1]
            ])
            gl_pts3d = [left, right]
            snap_point(takemat=takemat,
            draw=self.sp_draw,
            callback=self.sp_callback,
            constraint_axis=(True, False, False))
            self.handle_left.active = True
            return True

        if self.label.hover:
            self.feedback.instructions(context, "Size", "Use keyboard to modify size",
                [('ENTER', 'Validate'), ('RIGHTCLICK or ESC', 'cancel')])
            self.label.active = True
            self.keyboard_input_active = True
            return True

        return False

    def sp_draw(self, sp, context):
        global gl_pts3d
        if self.o is None:
            return
        p0 = gl_pts3d[0].copy()
        p1 = gl_pts3d[1].copy()
        if self.handle_right.active:
            p1 += sp.delta
        else:
            p0 += sp.delta
        self.sp_update(context, p0, p1)

        # snapping child objects may require base object update
        # eg manipulating windows requiring wall update
        if self.snap_callback is not None:
            snap_helper = context.active_object
            self.snap_callback(context, o=self.o, manipulator=self)
            context.scene.objects.active = snap_helper

        return

    def sp_callback(self, context, event, state, sp):

        if state == 'SUCCESS':
            self.sp_draw(sp, context)
            self.mouse_release(context, event)

        if state == 'CANCEL':
            p0 = gl_pts3d[0].copy()
            p1 = gl_pts3d[1].copy()
            self.sp_update(context, p0, p1)
            self.mouse_release(context, event)

    def sp_update(self, context, p0, p1):
        l0 = self.get_value(self.datablock, self.manipulator.prop1_name)
        length = (p0 - p1).length
        dp = length - l0
        if self.handle_left.active:
            dp = -dp
        dl = 0.5 * dp
        self.move(context, self.manipulator.prop2_name, dl)
        self.set_value(context, self.datablock, self.manipulator.prop1_name, length)
        self.move_linked(context, self.manipulator.prop2_name, dl)


class DeltaLocationManipulator(SizeManipulator):
    """
        Move a child window or door in wall segment
        not limited to this by the way
    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        SizeManipulator.__init__(self, context, o, datablock, manipulator, handle_size, snap_callback)
        self.label.label = ''
        self.feedback.instructions(context, "Move", "Drag to move", [
            ('CTRL', 'Snap'),
            ('SHIFT', 'Round value'),
            ('RIGHTCLICK or ESC', 'cancel')
            ])

    def check_hover(self):
        self.handle_right.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        global gl_pts3d
        if self.handle_right.hover:
            self.original_location = self.o.matrix_world.translation.copy()
            self.active = True
            self.feedback.enable()
            self.handle_right.active = True

            left, right, side, dz = self.manipulator.get_pts(self.o.matrix_world)
            dp = (right - left)
            dx = dp.normalized()
            dy = dz.cross(dx)
            p0 = left + 0.5 * dp
            takemat = Matrix([
                [dx.x, dy.x, dz.x, p0.x],
                [dx.y, dy.y, dz.y, p0.y],
                [dx.z, dy.z, dz.z, p0.z],
                [0, 0, 0, 1]
            ])
            gl_pts3d = [p0]
            snap_point(takemat=takemat,
                draw=self.sp_draw,
                callback=self.sp_callback,
                constraint_axis=(
                    self.manipulator.prop1_name == 'x',
                    self.manipulator.prop1_name == 'y',
                    self.manipulator.prop1_name == 'z'))
            return True
        return False

    def mouse_release(self, context, event):
        self.check_hover()
        self.feedback.disable()
        self.active = False
        self.handle_right.active = False
        return False

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.handle_right.active:
            # self.update(context, event)
            return True
        else:
            self.check_hover()
        return False

    def sp_draw(self, sp, context):
        global gl_pts3d
        if self.o is None:
            return
        p0 = gl_pts3d[0].copy()
        p1 = p0 + sp.delta
        itM = self.o.matrix_world.inverted()
        dl = self.get_value(itM * p1, self.manipulator.prop1_name)
        self.move(context, self.manipulator.prop1_name, dl)

        # snapping child objects may require base object update
        # eg manipulating windows requiring wall update
        if self.snap_callback is not None:
            snap_helper = context.active_object
            self.snap_callback(context, o=self.o, manipulator=self)
            context.scene.objects.active = snap_helper

        return

    def sp_callback(self, context, event, state, sp):

        if state == 'SUCCESS':
            self.sp_draw(sp, context)
            self.mouse_release(context, event)

        if state == 'CANCEL':
            self.cancel(context, event)

    def cancel(self, context, event):
        if self.active:
            self.mouse_release(context, event)
            # must move back to original location
            itM = self.o.matrix_world.inverted()
            dl = self.get_value(itM * self.original_location, self.manipulator.prop1_name)
            self.move(context, self.manipulator.prop1_name, dl)

    def draw_callback(self, _self, context, render=False):
        """
            draw on screen feedback using gl.
        """
        left, right, side, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.origin = left
        self.line_1.p = left
        self.line_1.v = right - left
        self.line_1.z_axis = normal
        self.handle_left.set_pos(context, self.line_1.lerp(0.5), -self.line_1.v, normal=normal)
        self.handle_right.set_pos(context, self.line_1.lerp(0.5), self.line_1.v, normal=normal)
        self.handle_left.draw(context, render)
        self.handle_right.draw(context, render)
        self.feedback.draw(context)


class DumbSizeManipulator(SizeManipulator):
    """
        Show a size while not being editable
    """
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        SizeManipulator.__init__(self, context, o, datablock, manipulator, handle_size, snap_callback)
        self.handle_right.draggable = False
        self.label.draggable = False
        self.label.colour_inactive = (0, 0, 0, 1)
        # self.label.label = 'Dumb '

    def mouse_move(self, context, event):
        return False


class AngleManipulator(Manipulator):
    """
        NOTE:
            There is a default shortcut to +5 and -5 on angles with left/right arrows

        Manipulate angle between segments
        bound to [-pi, pi]
    """

    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        # Angle
        self.handle_right = TriHandle(handle_size, arrow_size, draggable=True)
        self.handle_center = SquareHandle(handle_size, arrow_size)
        self.arc = GlArc()
        self.line_0 = GlLine()
        self.line_1 = GlLine()
        self.label_a = EditableText(handle_size, arrow_size, draggable=True)
        self.label_a.unit_type = 'ANGLE'
        Manipulator.__init__(self, context, o, datablock, manipulator, snap_callback)
        self.pts_mode = 'RADIUS'

    def check_hover(self):
        self.handle_right.check_hover(self.mouse_pos)
        self.label_a.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        if self.handle_right.hover:
            self.active = True
            self.original_angle = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.feedback.instructions(context, "Angle", "Drag to modify angle", [
                ('SHIFT', 'Round value'),
                ('RIGHTCLICK or ESC', 'cancel')
                ])
            self.handle_right.active = True
            return True
        if self.label_a.hover:
            self.feedback.instructions(context, "Angle (degree)", "Use keyboard to modify angle",
                [('ENTER', 'validate'),
                ('RIGHTCLICK or ESC', 'cancel')])
            self.value_type = 'ROTATION'
            self.label_a.active = True
            self.label_value = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.keyboard_input_active = True
            return True
        return False

    def mouse_release(self, context, event):
        self.check_hover()
        self.handle_right.active = False
        self.active = False
        return False

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.active:
            # print("AngleManipulator.mouse_move")
            self.update(context, event)
            return True
        else:
            self.check_hover()
        return False

    def keyboard_done(self, context, event, value):
        self.set_value(context, self.datablock, self.manipulator.prop1_name, value)
        self.label_a.active = False
        return True

    def keyboard_cancel(self, context, event):
        self.label_a.active = False
        return False

    def cancel(self, context, event):
        if self.active:
            self.mouse_release(context, event)
            self.set_value(context, self.datablock, self.manipulator.prop1_name, self.original_angle)

    def update(self, context, event):
        pt = self.get_pos3d(context)
        c = self.arc.c
        v = 2 * self.arc.r * (pt - c).normalized()
        v0 = c - v
        v1 = c + v
        p0, p1 = intersect_line_sphere(v0, v1, c, self.arc.r)
        if p0 is not None and p1 is not None:

            if (p1 - pt).length < (p0 - pt).length:
                p0, p1 = p1, p0

            v = p0 - self.arc.c
            da = atan2(v.y, v.x) - self.line_0.angle
            if da > pi:
                da -= 2 * pi
            if da < -pi:
                da += 2 * pi
            # from there pi > da > -pi
            # print("a:%.4f da:%.4f a0:%.4f" % (atan2(v.y, v.x), da, self.line_0.angle))
            if da > pi:
                da = pi
            if da < -pi:
                da = -pi
            if event.shift:
                da = round(da / pi * 180, 0) / 180 * pi
            self.set_value(context, self.datablock, self.manipulator.prop1_name, da)

    def draw_callback(self, _self, context, render=False):
        c, left, right, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.line_0.z_axis = normal
        self.line_1.z_axis = normal
        self.arc.z_axis = normal
        self.label_a.z_axis = normal
        self.origin = c
        self.line_0.p = c
        self.line_1.p = c
        self.arc.c = c
        self.line_0.v = left
        self.line_0.v = -self.line_0.cross.normalized()
        self.line_1.v = right
        self.line_1.v = self.line_1.cross.normalized()
        self.arc.a0 = self.line_0.angle
        self.arc.da = self.get_value(self.datablock, self.manipulator.prop1_name)
        self.arc.r = 1.0
        self.handle_right.set_pos(context, self.line_1.lerp(1),
                                  self.line_1.sized_normal(1, -1 if self.arc.da > 0 else 1).v)
        self.handle_center.set_pos(context, self.arc.c, -self.line_0.v)
        label_value = self.arc.da
        if self.keyboard_input_active:
            label_value = self.label_value
        self.label_a.set_pos(context, label_value, self.arc.lerp(0.5), -self.line_0.v)
        self.arc.draw(context, render)
        self.line_0.draw(context, render)
        self.line_1.draw(context, render)
        self.handle_right.draw(context, render)
        self.handle_center.draw(context, render)
        self.label_a.draw(context, render)
        self.feedback.draw(context, render)


class DumbAngleManipulator(AngleManipulator):
    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        AngleManipulator.__init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None)
        self.handle_right.draggable = False
        self.label_a.draggable = False

    def draw_callback(self, _self, context, render=False):
        c, left, right, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.line_0.z_axis = normal
        self.line_1.z_axis = normal
        self.arc.z_axis = normal
        self.label_a.z_axis = normal
        self.origin = c
        self.line_0.p = c
        self.line_1.p = c
        self.arc.c = c
        self.line_0.v = left
        self.line_0.v = -self.line_0.cross.normalized()
        self.line_1.v = right
        self.line_1.v = self.line_1.cross.normalized()

        # prevent ValueError in angle_signed
        if self.line_0.length == 0 or self.line_1.length == 0:
            return

        self.arc.a0 = self.line_0.angle
        self.arc.da = self.line_1.v.to_2d().angle_signed(self.line_0.v.to_2d())
        self.arc.r = 1.0
        self.handle_right.set_pos(context, self.line_1.lerp(1),
                                  self.line_1.sized_normal(1, -1 if self.arc.da > 0 else 1).v)
        self.handle_center.set_pos(context, self.arc.c, -self.line_0.v)
        label_value = self.arc.da
        self.label_a.set_pos(context, label_value, self.arc.lerp(0.5), -self.line_0.v)
        self.arc.draw(context, render)
        self.line_0.draw(context, render)
        self.line_1.draw(context, render)
        self.handle_right.draw(context, render)
        self.handle_center.draw(context, render)
        self.label_a.draw(context, render)
        self.feedback.draw(context, render)


class ArcAngleManipulator(Manipulator):
    """
        Manipulate angle of an arc
        when angle < 0 the arc center is on the left part of the circle
        when angle > 0 the arc center is on the right part of the circle
        bound to [-pi, pi]
    """

    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):

        # Fixed
        self.handle_left = SquareHandle(handle_size, arrow_size)
        # Angle
        self.handle_right = TriHandle(handle_size, arrow_size, draggable=True)
        self.handle_center = SquareHandle(handle_size, arrow_size)
        self.arc = GlArc()
        self.line_0 = GlLine()
        self.line_1 = GlLine()
        self.label_a = EditableText(handle_size, arrow_size, draggable=True)
        self.label_r = EditableText(handle_size, arrow_size, draggable=False)
        self.label_a.unit_type = 'ANGLE'
        Manipulator.__init__(self, context, o, datablock, manipulator, snap_callback)
        self.pts_mode = 'RADIUS'

    def check_hover(self):
        self.handle_right.check_hover(self.mouse_pos)
        self.label_a.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        if self.handle_right.hover:
            self.active = True
            self.original_angle = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.feedback.instructions(context, "Angle (degree)", "Drag to modify angle", [
                ('SHIFT', 'Round value'),
                ('RIGHTCLICK or ESC', 'cancel')
                ])
            self.handle_right.active = True
            return True
        if self.label_a.hover:
            self.feedback.instructions(context, "Angle (degree)", "Use keyboard to modify angle",
                [('ENTER', 'validate'),
                ('RIGHTCLICK or ESC', 'cancel')])
            self.value_type = 'ROTATION'
            self.label_value = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.label_a.active = True
            self.keyboard_input_active = True
            return True
        if self.label_r.hover:
            self.feedback.instructions(context, "Radius", "Use keyboard to modify radius",
                [('ENTER', 'validate'),
                ('RIGHTCLICK or ESC', 'cancel')])
            self.value_type = 'LENGTH'
            self.label_r.active = True
            self.keyboard_input_active = True
            return True
        return False

    def mouse_release(self, context, event):
        self.check_hover()
        self.handle_right.active = False
        self.active = False
        return False

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.handle_right.active:
            self.update(context, event)
            return True
        else:
            self.check_hover()
        return False

    def keyboard_done(self, context, event, value):
        self.set_value(context, self.datablock, self.manipulator.prop1_name, value)
        self.label_a.active = False
        self.label_r.active = False
        return True

    def keyboard_cancel(self, context, event):
        self.label_a.active = False
        self.label_r.active = False
        return False

    def cancel(self, context, event):
        if self.active:
            self.mouse_release(context, event)
            self.set_value(context, self.datablock, self.manipulator.prop1_name, self.original_angle)

    def update(self, context, event):

        pt = self.get_pos3d(context)
        c = self.arc.c

        v = 2 * self.arc.r * (pt - c).normalized()
        v0 = c - v
        v1 = c + v
        p0, p1 = intersect_line_sphere(v0, v1, c, self.arc.r)

        if p0 is not None and p1 is not None:
            # find nearest mouse intersection point
            if (p1 - pt).length < (p0 - pt).length:
                p0, p1 = p1, p0

            v = p0 - self.arc.c

            s = self.arc.tangeant(0, 1)
            res, d, t = s.point_sur_segment(pt)
            if d > 0:
                # right side
                a = self.arc.sized_normal(0, self.arc.r).angle
            else:
                a = self.arc.sized_normal(0, -self.arc.r).angle

            da = atan2(v.y, v.x) - a

            # bottom side +- pi
            if t < 0:
                # right
                if d > 0:
                    da = pi
                else:
                    da = -pi
            # top side bound to +- pi
            else:
                if da > pi:
                    da -= 2 * pi
                if da < -pi:
                    da += 2 * pi

            if event.shift:
                da = round(da / pi * 180, 0) / 180 * pi
            self.set_value(context, self.datablock, self.manipulator.prop1_name, da)

    def draw_callback(self, _self, context, render=False):
        # center : 3d points
        # left   : 3d vector pt-c
        # right  : 3d vector pt-c
        c, left, right, normal = self.manipulator.get_pts(self.o.matrix_world)
        self.line_0.z_axis = normal
        self.line_1.z_axis = normal
        self.arc.z_axis = normal
        self.label_a.z_axis = normal
        self.label_r.z_axis = normal
        self.origin = c
        self.line_0.p = c
        self.line_1.p = c
        self.arc.c = c
        self.line_0.v = left
        self.line_1.v = right
        self.arc.a0 = self.line_0.angle
        self.arc.da = self.get_value(self.datablock, self.manipulator.prop1_name)
        self.arc.r = left.length
        self.handle_left.set_pos(context, self.line_0.lerp(1), self.line_0.v)
        self.handle_right.set_pos(context, self.line_1.lerp(1),
            self.line_1.sized_normal(1, -1 if self.arc.da > 0 else 1).v)
        self.handle_center.set_pos(context, self.arc.c, -self.line_0.v)
        label_a_value = self.arc.da
        label_r_value = self.arc.r
        if self.keyboard_input_active:
            if self.value_type == 'LENGTH':
                label_r_value = self.label_value
            else:
                label_a_value = self.label_value
        self.label_a.set_pos(context, label_a_value, self.arc.lerp(0.5), -self.line_0.v)
        self.label_r.set_pos(context, label_r_value, self.line_0.lerp(0.5), self.line_0.v)
        self.arc.draw(context, render)
        self.line_0.draw(context, render)
        self.line_1.draw(context, render)
        self.handle_left.draw(context, render)
        self.handle_right.draw(context, render)
        self.handle_center.draw(context, render)
        self.label_r.draw(context, render)
        self.label_a.draw(context, render)
        self.feedback.draw(context, render)


class ArcAngleRadiusManipulator(ArcAngleManipulator):
    """
        Manipulate angle and radius of an arc
        when angle < 0 the arc center is on the left part of the circle
        when angle > 0 the arc center is on the right part of the circle
        bound to [-pi, pi]
    """

    def __init__(self, context, o, datablock, manipulator, handle_size, snap_callback=None):
        ArcAngleManipulator.__init__(self, context, o, datablock, manipulator, handle_size, snap_callback)
        self.handle_center = TriHandle(handle_size, arrow_size, draggable=True)
        self.label_r.draggable = True

    def check_hover(self):
        self.handle_right.check_hover(self.mouse_pos)
        self.handle_center.check_hover(self.mouse_pos)
        self.label_a.check_hover(self.mouse_pos)
        self.label_r.check_hover(self.mouse_pos)

    def mouse_press(self, context, event):
        if self.handle_right.hover:
            self.active = True
            self.original_angle = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.feedback.instructions(context, "Angle (degree)", "Drag to modify angle", [
                ('SHIFT', 'Round value'),
                ('RIGHTCLICK or ESC', 'cancel')
                ])
            self.handle_right.active = True
            return True
        if self.handle_center.hover:
            self.active = True
            self.original_radius = self.get_value(self.datablock, self.manipulator.prop2_name)
            self.feedback.instructions(context, "Radius", "Drag to modify radius", [
                ('SHIFT', 'Round value'),
                ('RIGHTCLICK or ESC', 'cancel')
                ])
            self.handle_center.active = True
            return True
        if self.label_a.hover:
            self.feedback.instructions(context, "Angle (degree)", "Use keyboard to modify angle",
                [('ENTER', 'validate'),
                ('RIGHTCLICK or ESC', 'cancel')])
            self.value_type = 'ROTATION'
            self.label_value = self.get_value(self.datablock, self.manipulator.prop1_name)
            self.label_a.active = True
            self.keyboard_input_active = True
            return True
        if self.label_r.hover:
            self.feedback.instructions(context, "Radius", "Use keyboard to modify radius",
                [('ENTER', 'validate'),
                ('RIGHTCLICK or ESC', 'cancel')])
            self.value_type = 'LENGTH'
            self.label_r.active = True
            self.keyboard_input_active = True
            return True
        return False

    def mouse_release(self, context, event):
        self.check_hover()
        self.active = False
        self.handle_right.active = False
        self.handle_center.active = False
        return False

    def mouse_move(self, context, event):
        self.mouse_position(event)
        if self.handle_right.active:
            self.update(context, event)
            return True
        elif self.handle_center.active:
            self.update_radius(context, event)
            return True
        else:
            self.check_hover()
        return False

    def keyboard_done(self, context, event, value):
        if self.value_type == 'LENGTH':
            self.set_value(context, self.datablock, self.manipulator.prop2_name, value)
            self.label_r.active = False
        else:
            self.set_value(context, self.datablock, self.manipulator.prop1_name, value)
            self.label_a.active = False
        return True

    def update_radius(self, context, event):
        pt = self.get_pos3d(context)
        c = self.arc.c
        left = self.line_0.lerp(1)
        p, t = intersect_point_line(pt, c, left)
        radius = (left - p).length
        if event.alt:
            radius = round(radius, 1)
        self.set_value(context, self.datablock, self.manipulator.prop2_name, radius)

    def cancel(self, context, event):
        if self.handle_right.active:
            self.mouse_release(context, event)
            self.set_value(context, self.datablock, self.manipulator.prop1_name, self.original_angle)
        if self.handle_center.active:
            self.mouse_release(context, event)
            self.set_value(context, self.datablock, self.manipulator.prop2_name, self.original_radius)


# ------------------------------------------------------------------
# Define a single Manipulator Properties to store on object
# ------------------------------------------------------------------


# Allow registering manipulators classes
manipulators_class_lookup = {}


def register_manipulator(type_key, manipulator_class):
    if type_key in manipulators_class_lookup.keys():
        raise RuntimeError("Manipulator of type {} allready exists, unable to override".format(type_key))
    manipulators_class_lookup[type_key] = manipulator_class


class archipack_manipulator(PropertyGroup):
    """
        A property group to add to manipulable objects
        type_key: type of manipulator
        prop1_name = the property name of object to modify
        prop2_name = another property name of object to modify (eg: angle and radius)
        p0, p1, p2 3d Vectors as base points to represent manipulators on screen
        normal Vector normal of plane on with draw manipulator
    """
    type_key = StringProperty(default='SIZE')

    # How 3d points are stored in manipulators ?
    # SIZE = 2 absolute positionned and a scaling vector
    # RADIUS = 1 absolute positionned (center) and 2 relatives (sides)
    # POLYGON = 2 absolute positionned and a relative vector (for rect polygons)

    pts_mode = StringProperty(default='SIZE')
    prop1_name = StringProperty()
    prop2_name = StringProperty()
    p0 = FloatVectorProperty(subtype='XYZ')
    p1 = FloatVectorProperty(subtype='XYZ')
    p2 = FloatVectorProperty(subtype='XYZ')
    # allow orientation of manipulators by default on xy plane,
    # but may be used to constrain heights on local object space
    normal = FloatVectorProperty(subtype='XYZ', default=(0, 0, 1))

    def set_pts(self, pts, normal=None):
        """
            set 3d location of gl points (in object space)
            pts: array of 3 vectors 3d
            normal: optionnal vector 3d default to Z axis
        """
        pts = [Vector(p) for p in pts]
        self.p0, self.p1, self.p2 = pts
        if normal is not None:
            self.normal = Vector(normal)

    def get_pts(self, tM):
        """
            convert points from local to world absolute
            to draw them at the right place
            tM : object's world matrix
        """
        rM = tM.to_3x3()
        if self.pts_mode in ['SIZE', 'POLYGON']:
            return tM * self.p0, tM * self.p1, self.p2, rM * self.normal
        else:
            return tM * self.p0, rM * self.p1, rM * self.p2, rM * self.normal

    def get_prefs(self, context):
        global __name__
        global arrow_size
        global handle_size
        try:
            # retrieve addon name from imports
            addon_name = __name__.split('.')[0]
            prefs = context.user_preferences.addons[addon_name].preferences
            arrow_size = prefs.arrow_size
            handle_size = prefs.handle_size
        except:
            pass

    def setup(self, context, o, datablock, snap_callback=None):
        """
            Factory return a manipulator object or None
            o:         object
            datablock: datablock to modify
            snap_callback: function call y
        """

        self.get_prefs(context)

        global manipulators_class_lookup

        if self.type_key not in manipulators_class_lookup.keys() or \
                not manipulators_class_lookup[self.type_key].poll(context):
            # RuntimeError is overkill but may be enabled for debug purposes
            # Silentely ignore allow skipping manipulators if / when deps as not meet
            # manip stack will simply be filled with None objects
            # raise RuntimeError("Manipulator of type {} not found".format(self.type_key))
            return None

        m = manipulators_class_lookup[self.type_key](context, o, datablock, self, handle_size, snap_callback)
        # points storage model as described upside
        self.pts_mode = m.pts_mode
        return m


# ------------------------------------------------------------------
# Define Manipulable to make a PropertyGroup manipulable
# ------------------------------------------------------------------


class ARCHIPACK_OT_manipulate(Operator):
    bl_idname = "archipack.manipulate"
    bl_label = "Manipulate"
    bl_description = "Manipulate"
    bl_options = {'REGISTER', 'UNDO'}

    object_name = StringProperty(default="")

    @classmethod
    def poll(self, context):
        return context.active_object is not None

    def exit_selectmode(self, context, key):
        """
            Hide select area on exit
        """
        global manips
        if key in manips.keys():
            if manips[key].manipulable is not None:
                manips[key].manipulable.manipulable_exit_selectmode(context)

    def modal(self, context, event):
        global manips
        # Exit on stack change
        # handle multiple object stack
        # use object_name property to find manupulated object in stack
        # select and make object active
        # and exit when not found
        if context.area is not None:
            context.area.tag_redraw()
        key = self.object_name
        if check_stack(key):
            self.exit_selectmode(context, key)
            remove_manipulable(key)
            # print("modal exit by check_stack(%s)" % (key))
            return {'FINISHED'}

        res = manips[key].manipulable.manipulable_modal(context, event)

        if 'FINISHED' in res:
            self.exit_selectmode(context, key)
            remove_manipulable(key)
            # print("modal exit by {FINISHED}")

        return res

    def invoke(self, context, event):
        if context.space_data is not None and context.space_data.type == 'VIEW_3D':
            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "Active space must be a View3d")
            return {'CANCELLED'}


class ARCHIPACK_OT_disable_manipulate(Operator):
    bl_idname = "archipack.disable_manipulate"
    bl_label = "Disable Manipulate"
    bl_description = "Disable any active manipulator"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        return True

    def execute(self, context):
        empty_stack()
        return {'FINISHED'}


class Manipulable():
    """
        A class extending PropertyGroup to setup gl manipulators
        Beware : prevent crash calling manipulable_disable()
                 before changing manipulated data structure
    """
    manipulators = CollectionProperty(
            type=archipack_manipulator,
            # options={'SKIP_SAVE'},
            # options={'HIDDEN'},
            description="store 3d points to draw gl manipulators"
            )
    manipulable_refresh = BoolProperty(
            default=False,
            options={'SKIP_SAVE'},
            description="Flag enable to rebuild manipulators when data model change"
            )
    manipulate_mode = BoolProperty(
            default=False,
            options={'SKIP_SAVE'},
            description="Flag manipulation state so we are able to toggle"
            )
    select_mode = BoolProperty(
            default=False,
            options={'SKIP_SAVE'},
            description="Flag select state so we are able to toggle"
            )
    manipulable_selectable = BoolProperty(
            default=False,
            options={'SKIP_SAVE'},
            description="Flag make manipulators selectable"
            )
    keymap = None

    # selectable manipulators
    manipulable_area = GlCursorArea()
    manipulable_start_point = Vector((0, 0))
    manipulable_end_point = Vector((0, 0))
    manipulable_draw_handler = None

    def setup_manipulators(self):
        """
            Must implement manipulators creation
            TODO: call from update and manipulable_setup
        """
        raise NotImplementedError

    def manipulable_draw_callback(self, _self, context):
        self.manipulable_area.draw(context)

    def manipulable_disable(self, context):
        """
            disable gl draw handlers
        """
        o = context.active_object
        if o is not None:
            self.manipulable_exit_selectmode(context)
            remove_manipulable(o.name)
            self.manip_stack = add_manipulable(o.name, self)

        self.manipulate_mode = False
        self.select_mode = False

    def manipulable_exit_selectmode(self, context):
        self.manipulable_area.disable()
        self.select_mode = False
        # remove select draw handler
        if self.manipulable_draw_handler is not None:
            bpy.types.SpaceView3D.draw_handler_remove(
                self.manipulable_draw_handler,
                'WINDOW')
        self.manipulable_draw_handler = None

    def manipulable_setup(self, context):
        """
            TODO: Implement the setup part as per parent object basis
        """
        self.manipulable_disable(context)
        o = context.active_object
        self.setup_manipulators()
        for m in self.manipulators:
            self.manip_stack.append(m.setup(context, o, self))

    def _manipulable_invoke(self, context):

        object_name = context.active_object.name

        # store a reference to self for operators
        add_manipulable(object_name, self)

        # copy context so manipulator always use
        # invoke time context
        ctx = context.copy()

        # take care of context switching
        # when call from outside of 3d view
        if context.space_data is not None and context.space_data.type != 'VIEW_3D':
            for window in bpy.context.window_manager.windows:
                screen = window.screen
                for area in screen.areas:
                    if area.type == 'VIEW_3D':
                        ctx['area'] = area
                        for region in area.regions:
                            if region.type == 'WINDOW':
                                ctx['region'] = region
                        break
        if ctx is not None:
            bpy.ops.archipack.manipulate(ctx, 'INVOKE_DEFAULT', object_name=object_name)

    def manipulable_invoke(self, context):
        """
            call this in operator invoke()
            NB:
            if override dont forget to call:
                _manipulable_invoke(context)

        """
        # print("manipulable_invoke self.manipulate_mode:%s" % (self.manipulate_mode))

        if self.manipulate_mode:
            self.manipulable_disable(context)
            return False
        # else:
        #    bpy.ops.archipack.disable_manipulate('INVOKE_DEFAULT')

        # self.manip_stack = []
        # kills other's manipulators
        # self.manipulate_mode = True
        self.manipulable_setup(context)
        self.manipulate_mode = True

        self._manipulable_invoke(context)

        return True

    def manipulable_modal(self, context, event):
        """
            call in operator modal()
            should not be overriden
            as it provide all needed
            functionnality out of the box
        """
        # setup again when manipulators type change
        if self.manipulable_refresh:
            # print("manipulable_refresh")
            self.manipulable_refresh = False
            self.manipulable_setup(context)
            self.manipulate_mode = True

        if context.area is None:
            self.manipulable_disable(context)
            return {'FINISHED'}

        context.area.tag_redraw()

        if self.keymap is None:
            self.keymap = Keymaps(context)

        if self.keymap.check(event, self.keymap.undo):
            # user feedback on undo by disabling manipulators
            self.manipulable_disable(context)
            return {'FINISHED'}

        # clean up manipulator on delete
        if self.keymap.check(event, self.keymap.delete):  # {'X'}:
            # @TODO:
            # for doors and windows, seek and destroy holes object if any
            # a dedicated delete method into those objects may be an option ?
            # A type check is required any way we choose
            #
            # Time for a generic archipack's datablock getter / filter into utils
            #
            # May also be implemented into nearly hidden "reference point"
            # to delete / duplicate / link duplicate / unlink of
            # a complete set of wall, doors and windows at once
            self.manipulable_disable(context)

            if bpy.ops.object.delete.poll():
                bpy.ops.object.delete('INVOKE_DEFAULT', use_global=False)

            return {'FINISHED'}

        """
        # handle keyborad for select mode
        if self.select_mode:
            if event.type in {'A'} and event.value == 'RELEASE':
                return {'RUNNING_MODAL'}
        """

        for manipulator in self.manip_stack:
            # manipulator should return false on left mouse release
            # so proper release handler is called
            # and return true to call manipulate when required
            # print("manipulator:%s" % manipulator)
            if manipulator is not None and manipulator.modal(context, event):
                self.manipulable_manipulate(context, event, manipulator)
                return {'RUNNING_MODAL'}

        # print("Manipulable %s %s" % (event.type, event.value))

        # Manipulators are not active so check for selection
        if event.type == 'LEFTMOUSE':

            # either we are starting select mode
            # user press on area not over maniuplator
            # Prevent 3 mouse emultation to select when alt pressed
            if self.manipulable_selectable and event.value == 'PRESS' and not event.alt:
                self.select_mode = True
                self.manipulable_area.enable()
                self.manipulable_start_point = Vector((event.mouse_region_x, event.mouse_region_y))
                self.manipulable_area.set_location(
                    context,
                    self.manipulable_start_point,
                    self.manipulable_start_point)
                # add a select draw handler
                args = (self, context)
                self.manipulable_draw_handler = bpy.types.SpaceView3D.draw_handler_add(
                    self.manipulable_draw_callback,
                    args,
                    'WINDOW',
                    'POST_PIXEL')
                # don't keep focus
                # as this prevent click over ui
                # return {'RUNNING_MODAL'}

            elif event.value == 'RELEASE':
                if self.select_mode:
                    # confirm selection

                    self.manipulable_exit_selectmode(context)

                    # keep focus
                    # return {'RUNNING_MODAL'}

                else:
                    # allow manipulator action on release
                    for manipulator in self.manip_stack:
                        if manipulator is not None and manipulator.selectable:
                            manipulator.selected = False
                    self.manipulable_release(context)

        elif self.select_mode and event.type == 'MOUSEMOVE' and event.value == 'PRESS':
            # update select area size
            self.manipulable_end_point = Vector((event.mouse_region_x, event.mouse_region_y))
            self.manipulable_area.set_location(
                context,
                self.manipulable_start_point,
                self.manipulable_end_point)
            if event.shift:
                # deselect
                for i, manipulator in enumerate(self.manip_stack):
                    if manipulator is not None and manipulator.selectable:
                        manipulator.deselect(self.manipulable_area)
            else:
                # select / more
                for i, manipulator in enumerate(self.manip_stack):
                    if manipulator is not None and manipulator.selectable:
                        manipulator.select(self.manipulable_area)
            # keep focus to prevent left select mouse to actually move object
            return {'RUNNING_MODAL'}

        # event.alt here to prevent 3 button mouse emulation exit while zooming
        if event.type in {'RIGHTMOUSE', 'ESC'} and event.value == 'PRESS' and not event.alt:
            self.manipulable_disable(context)
            self.manipulable_exit(context)
            return {'FINISHED'}

        return {'PASS_THROUGH'}

    # Callbacks
    def manipulable_release(self, context):
        """
            Override with action to do on mouse release
            eg: big update
        """
        return

    def manipulable_exit(self, context):
        """
            Override with action to do when modal exit
        """
        return

    def manipulable_manipulate(self, context, event, manipulator):
        """
            Override with action to do when a handle is active (pressed and mousemove)
        """
        return


@persistent
def cleanup(dummy=None):
    empty_stack()


def register():
    # Register default manipulators
    global manips
    global manipulators_class_lookup
    manipulators_class_lookup = {}
    manips = {}
    register_manipulator('SIZE', SizeManipulator)
    register_manipulator('SIZE_LOC', SizeLocationManipulator)
    register_manipulator('ANGLE', AngleManipulator)
    register_manipulator('DUMB_ANGLE', DumbAngleManipulator)
    register_manipulator('ARC_ANGLE_RADIUS', ArcAngleRadiusManipulator)
    register_manipulator('COUNTER', CounterManipulator)
    register_manipulator('DUMB_SIZE', DumbSizeManipulator)
    register_manipulator('DELTA_LOC', DeltaLocationManipulator)
    register_manipulator('DUMB_STRING', DumbStringManipulator)

    # snap aware size loc
    register_manipulator('SNAP_SIZE_LOC', SnapSizeLocationManipulator)
    # register_manipulator('SNAP_POINT', SnapPointManipulator)
    # wall's line based object snap
    register_manipulator('WALL_SNAP', WallSnapManipulator)
    bpy.utils.register_class(ARCHIPACK_OT_manipulate)
    bpy.utils.register_class(ARCHIPACK_OT_disable_manipulate)
    bpy.utils.register_class(archipack_manipulator)
    bpy.app.handlers.load_pre.append(cleanup)


def unregister():
    global manips
    global manipulators_class_lookup
    empty_stack()
    del manips
    manipulators_class_lookup.clear()
    del manipulators_class_lookup
    bpy.utils.unregister_class(ARCHIPACK_OT_manipulate)
    bpy.utils.unregister_class(ARCHIPACK_OT_disable_manipulate)
    bpy.utils.unregister_class(archipack_manipulator)
    bpy.app.handlers.load_pre.remove(cleanup)
