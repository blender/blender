# <pep8-80 compliant>

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

__author__ = "Nutti <nutti.metro@gmail.com>"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"

from enum import IntEnum
import math

import bpy
import bgl
import mathutils
import bmesh

from . import muv_common


MAX_VALUE = 100000.0


class MUV_UVBBCmd():
    """
    Custom class: Base class of command
    """

    def __init__(self):
        self.op = 'NONE'        # operation

    def to_matrix(self):
        # mat = I
        mat = mathutils.Matrix()
        mat.identity()
        return mat


class MUV_UVBBTranslationCmd(MUV_UVBBCmd):
    """
    Custom class: Translation operation
    """

    def __init__(self, ix, iy):
        super().__init__()
        self.op = 'TRANSLATION'
        self.__x = ix       # current x
        self.__y = iy       # current y
        self.__ix = ix      # initial x
        self.__iy = iy      # initial y

    def to_matrix(self):
        # mat = Mt
        dx = self.__x - self.__ix
        dy = self.__y - self.__iy
        return mathutils.Matrix.Translation((dx, dy, 0))

    def set(self, x, y):
        self.__x = x
        self.__y = y


class MUV_UVBBRotationCmd(MUV_UVBBCmd):
    """
    Custom class: Rotation operation
    """

    def __init__(self, ix, iy, cx, cy):
        super().__init__()
        self.op = 'ROTATION'
        self.__x = ix       # current x
        self.__y = iy       # current y
        self.__cx = cx      # center of rotation x
        self.__cy = cy      # center of rotation y
        dx = self.__x - self.__cx
        dy = self.__y - self.__cy
        self.__iangle = math.atan2(dy, dx)      # initial rotation angle

    def to_matrix(self):
        # mat = Mt * Mr * Mt^-1
        dx = self.__x - self.__cx
        dy = self.__y - self.__cy
        angle = math.atan2(dy, dx) - self.__iangle
        mti = mathutils.Matrix.Translation((-self.__cx, -self.__cy, 0.0))
        mr = mathutils.Matrix.Rotation(angle, 4, 'Z')
        mt = mathutils.Matrix.Translation((self.__cx, self.__cy, 0.0))
        return mt * mr * mti

    def set(self, x, y):
        self.__x = x
        self.__y = y


class MUV_UVBBScalingCmd(MUV_UVBBCmd):
    """
    Custom class: Scaling operation
    """

    def __init__(self, ix, iy, ox, oy, dir_x, dir_y, mat):
        super().__init__()
        self.op = 'SCALING'
        self.__ix = ix          # initial x
        self.__iy = iy          # initial y
        self.__x = ix           # current x
        self.__y = iy           # current y
        self.__ox = ox          # origin of scaling x
        self.__oy = oy          # origin of scaling y
        self.__dir_x = dir_x    # direction of scaling x
        self.__dir_y = dir_y    # direction of scaling y
        self.__mat = mat
        # initial origin of scaling = M(to original transform) * (ox, oy)
        iov = mat * mathutils.Vector((ox, oy, 0.0))
        self.__iox = iov.x      # initial origin of scaling X
        self.__ioy = iov.y      # initial origin of scaling y

    def to_matrix(self):
        """
        mat = M(to original transform)^-1 * Mt(to origin) * Ms *
              Mt(to origin)^-1 * M(to original transform)
        """
        m = self.__mat
        mi = self.__mat.inverted()
        mtoi = mathutils.Matrix.Translation((-self.__iox, -self.__ioy, 0.0))
        mto = mathutils.Matrix.Translation((self.__iox, self.__ioy, 0.0))
        # every point must be transformed to origin
        t = m * mathutils.Vector((self.__ix, self.__iy, 0.0))
        tix, tiy = t.x, t.y
        t = m * mathutils.Vector((self.__ox, self.__oy, 0.0))
        tox, toy = t.x, t.y
        t = m * mathutils.Vector((self.__x, self.__y, 0.0))
        tx, ty = t.x, t.y
        ms = mathutils.Matrix()
        ms.identity()
        if self.__dir_x == 1:
            ms[0][0] = (tx - tox) * self.__dir_x / (tix - tox)
        if self.__dir_y == 1:
            ms[1][1] = (ty - toy) * self.__dir_y / (tiy - toy)
        return mi * mto * ms * mtoi * m

    def set(self, x, y):
        self.__x = x
        self.__y = y


class MUV_UVBBUniformScalingCmd(MUV_UVBBCmd):
    """
    Custom class: Uniform Scaling operation
    """

    def __init__(self, ix, iy, ox, oy, mat):
        super().__init__()
        self.op = 'SCALING'
        self.__ix = ix          # initial x
        self.__iy = iy          # initial y
        self.__x = ix           # current x
        self.__y = iy           # current y
        self.__ox = ox          # origin of scaling x
        self.__oy = oy          # origin of scaling y
        self.__mat = mat
        # initial origin of scaling = M(to original transform) * (ox, oy)
        iov = mat * mathutils.Vector((ox, oy, 0.0))
        self.__iox = iov.x      # initial origin of scaling x
        self.__ioy = iov.y      # initial origin of scaling y
        self.__dir_x = 1
        self.__dir_y = 1

    def to_matrix(self):
        """
        mat = M(to original transform)^-1 * Mt(to origin) * Ms *
              Mt(to origin)^-1 * M(to original transform)
        """
        m = self.__mat
        mi = self.__mat.inverted()
        mtoi = mathutils.Matrix.Translation((-self.__iox, -self.__ioy, 0.0))
        mto = mathutils.Matrix.Translation((self.__iox, self.__ioy, 0.0))
        # every point must be transformed to origin
        t = m * mathutils.Vector((self.__ix, self.__iy, 0.0))
        tix, tiy = t.x, t.y
        t = m * mathutils.Vector((self.__ox, self.__oy, 0.0))
        tox, toy = t.x, t.y
        t = m * mathutils.Vector((self.__x, self.__y, 0.0))
        tx, ty = t.x, t.y
        ms = mathutils.Matrix()
        ms.identity()
        tir = math.sqrt((tix - tox) * (tix - tox) + (tiy - toy) * (tiy - toy))
        tr = math.sqrt((tx - tox) * (tx - tox) + (ty - toy) * (ty - toy))

        sr = tr / tir

        if ((tx - tox) * (tix - tox)) > 0:
            self.__dir_x = 1
        else:
            self.__dir_x = -1
        if ((ty - toy) * (tiy - toy)) > 0:
            self.__dir_y = 1
        else:
            self.__dir_y = -1

        ms[0][0] = sr * self.__dir_x
        ms[1][1] = sr * self.__dir_y

        return mi * mto * ms * mtoi * m

    def set(self, x, y):
        self.__x = x
        self.__y = y


class MUV_UVBBCmdExecuter():
    """
    Custom class: manage command history and execute command
    """

    def __init__(self):
        self.__cmd_list = []        # history
        self.__cmd_list_redo = []   # redo list

    def execute(self, begin=0, end=-1):
        """
        create matrix from history
        """
        mat = mathutils.Matrix()
        mat.identity()
        for i, cmd in enumerate(self.__cmd_list):
            if begin <= i and (end == -1 or i <= end):
                mat = cmd.to_matrix() * mat
        return mat

    def undo_size(self):
        """
        get history size
        """
        return len(self.__cmd_list)

    def top(self):
        """
        get top of history
        """
        if len(self.__cmd_list) <= 0:
            return None
        return self.__cmd_list[-1]

    def append(self, cmd):
        """
        append command
        """
        self.__cmd_list.append(cmd)
        self.__cmd_list_redo = []

    def undo(self):
        """
        undo command
        """
        if len(self.__cmd_list) <= 0:
            return
        self.__cmd_list_redo.append(self.__cmd_list.pop())

    def redo(self):
        """
        redo command
        """
        if len(self.__cmd_list_redo) <= 0:
            return
        self.__cmd_list.append(self.__cmd_list_redo.pop())

    def pop(self):
        if len(self.__cmd_list) <= 0:
            return None
        return self.__cmd_list.pop()

    def push(self, cmd):
        self.__cmd_list.append(cmd)


class MUV_UVBBRenderer(bpy.types.Operator):
    """
    Operation class: Render UV bounding box
    """

    bl_idname = "uv.muv_uvbb_renderer"
    bl_label = "UV Bounding Box Renderer"
    bl_description = "Bounding Box Renderer about UV in Image Editor"

    __handle = None

    @staticmethod
    def handle_add(obj, context):
        if MUV_UVBBRenderer.__handle is None:
            sie = bpy.types.SpaceImageEditor
            MUV_UVBBRenderer.__handle = sie.draw_handler_add(
                MUV_UVBBRenderer.draw_bb,
                (obj, context), "WINDOW", "POST_PIXEL")

    @staticmethod
    def handle_remove():
        if MUV_UVBBRenderer.__handle is not None:
            sie = bpy.types.SpaceImageEditor
            sie.draw_handler_remove(
                MUV_UVBBRenderer.__handle, "WINDOW")
            MUV_UVBBRenderer.__handle = None

    @staticmethod
    def __draw_ctrl_point(context, pos):
        """
        Draw control point
        """
        prefs = context.user_preferences.addons["uv_magic_uv"].preferences
        cp_size = prefs.uvbb_cp_size
        offset = cp_size / 2
        verts = [
            [pos.x - offset, pos.y - offset],
            [pos.x - offset, pos.y + offset],
            [pos.x + offset, pos.y + offset],
            [pos.x + offset, pos.y - offset]
        ]
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glBegin(bgl.GL_QUADS)
        bgl.glColor4f(1.0, 1.0, 1.0, 1.0)
        for (x, y) in verts:
            bgl.glVertex2f(x, y)
        bgl.glEnd()

    @staticmethod
    def draw_bb(_, context):
        """
        Draw bounding box
        """
        props = context.scene.muv_props.uvbb
        for cp in props.ctrl_points:
            MUV_UVBBRenderer.__draw_ctrl_point(
                context, mathutils.Vector(
                    context.region.view2d.view_to_region(cp.x, cp.y)))


class MUV_UVBBState(IntEnum):
    """
    Enum: State definition used by MUV_UVBBStateMgr
    """
    NONE = 0
    TRANSLATING = 1
    SCALING_1 = 2
    SCALING_2 = 3
    SCALING_3 = 4
    SCALING_4 = 5
    SCALING_5 = 6
    SCALING_6 = 7
    SCALING_7 = 8
    SCALING_8 = 9
    ROTATING = 10
    UNIFORM_SCALING_1 = 11
    UNIFORM_SCALING_2 = 12
    UNIFORM_SCALING_3 = 13
    UNIFORM_SCALING_4 = 14


class MUV_UVBBStateBase():
    """
    Custom class: Base class of state
    """

    def __init__(self):
        pass

    def update(self, context, event, ctrl_points, mouse_view):
        raise NotImplementedError


class MUV_UVBBStateNone(MUV_UVBBStateBase):
    """
    Custom class:
    No state
    Wait for event from mouse
    """

    def __init__(self, cmd_exec):
        super().__init__()
        self.__cmd_exec = cmd_exec

    def update(self, context, event, ctrl_points, mouse_view):
        """
        Update state
        """
        prefs = context.user_preferences.addons["uv_magic_uv"].preferences
        cp_react_size = prefs.uvbb_cp_react_size
        is_uscaling = context.scene.muv_uvbb_uniform_scaling
        if event.type == 'LEFTMOUSE':
            if event.value == 'PRESS':
                x, y = context.region.view2d.view_to_region(
                    mouse_view.x, mouse_view.y)
                for i, p in enumerate(ctrl_points):
                    px, py = context.region.view2d.view_to_region(p.x, p.y)
                    in_cp_x = (px + cp_react_size > x and
                               px - cp_react_size < x)
                    in_cp_y = (py + cp_react_size > y and
                               py - cp_react_size < y)
                    if in_cp_x and in_cp_y:
                        if is_uscaling:
                            arr = [1, 3, 6, 8]
                            if i in arr:
                                return (
                                    MUV_UVBBState.UNIFORM_SCALING_1 +
                                    arr.index(i)
                                )
                        else:
                            return MUV_UVBBState.TRANSLATING + i

        return MUV_UVBBState.NONE


class MUV_UVBBStateTranslating(MUV_UVBBStateBase):
    """
    Custom class: Translating state
    """

    def __init__(self, cmd_exec, ctrl_points):
        super().__init__()
        self.__cmd_exec = cmd_exec
        ix, iy = ctrl_points[0].x, ctrl_points[0].y
        self.__cmd_exec.append(MUV_UVBBTranslationCmd(ix, iy))

    def update(self, context, event, ctrl_points, mouse_view):
        if event.type == 'LEFTMOUSE':
            if event.value == 'RELEASE':
                return MUV_UVBBState.NONE
        if event.type == 'MOUSEMOVE':
            x, y = mouse_view.x, mouse_view.y
            self.__cmd_exec.top().set(x, y)
        return MUV_UVBBState.TRANSLATING


class MUV_UVBBStateScaling(MUV_UVBBStateBase):
    """
    Custom class: Scaling state
    """

    def __init__(self, cmd_exec, state, ctrl_points):
        super().__init__()
        self.__state = state
        self.__cmd_exec = cmd_exec
        dir_x_list = [1, 1, 1, 0, 0, 1, 1, 1]
        dir_y_list = [1, 0, 1, 1, 1, 1, 0, 1]
        idx = state - 2
        ix, iy = ctrl_points[idx + 1].x, ctrl_points[idx + 1].y
        ox, oy = ctrl_points[8 - idx].x, ctrl_points[8 - idx].y
        dir_x, dir_y = dir_x_list[idx], dir_y_list[idx]
        mat = self.__cmd_exec.execute(end=self.__cmd_exec.undo_size())
        self.__cmd_exec.append(
            MUV_UVBBScalingCmd(ix, iy, ox, oy, dir_x, dir_y, mat.inverted()))

    def update(self, context, event, ctrl_points, mouse_view):
        if event.type == 'LEFTMOUSE':
            if event.value == 'RELEASE':
                return MUV_UVBBState.NONE
        if event.type == 'MOUSEMOVE':
            x, y = mouse_view.x, mouse_view.y
            self.__cmd_exec.top().set(x, y)
        return self.__state


class MUV_UVBBStateUniformScaling(MUV_UVBBStateBase):
    """
    Custom class: Uniform Scaling state
    """

    def __init__(self, cmd_exec, state, ctrl_points):
        super().__init__()
        self.__state = state
        self.__cmd_exec = cmd_exec
        icp_idx = [1, 3, 6, 8]
        ocp_idx = [8, 6, 3, 1]
        idx = state - MUV_UVBBState.UNIFORM_SCALING_1
        ix, iy = ctrl_points[icp_idx[idx]].x, ctrl_points[icp_idx[idx]].y
        ox, oy = ctrl_points[ocp_idx[idx]].x, ctrl_points[ocp_idx[idx]].y
        mat = self.__cmd_exec.execute(end=self.__cmd_exec.undo_size())
        self.__cmd_exec.append(MUV_UVBBUniformScalingCmd(
            ix, iy, ox, oy, mat.inverted()))

    def update(self, context, event, ctrl_points, mouse_view):
        if event.type == 'LEFTMOUSE':
            if event.value == 'RELEASE':
                return MUV_UVBBState.NONE
        if event.type == 'MOUSEMOVE':
            x, y = mouse_view.x, mouse_view.y
            self.__cmd_exec.top().set(x, y)

        return self.__state


class MUV_UVBBStateRotating(MUV_UVBBStateBase):
    """
    Custom class: Rotating state
    """

    def __init__(self, cmd_exec, ctrl_points):
        super().__init__()
        self.__cmd_exec = cmd_exec
        ix, iy = ctrl_points[9].x, ctrl_points[9].y
        ox, oy = ctrl_points[0].x, ctrl_points[0].y
        self.__cmd_exec.append(MUV_UVBBRotationCmd(ix, iy, ox, oy))

    def update(self, context, event, ctrl_points, mouse_view):
        if event.type == 'LEFTMOUSE':
            if event.value == 'RELEASE':
                return MUV_UVBBState.NONE
        if event.type == 'MOUSEMOVE':
            x, y = mouse_view.x, mouse_view.y
            self.__cmd_exec.top().set(x, y)
        return MUV_UVBBState.ROTATING


class MUV_UVBBStateMgr():
    """
    Custom class: Manage state about this feature
    """

    def __init__(self, cmd_exec):
        self.__cmd_exec = cmd_exec          # command executer
        self.__state = MUV_UVBBState.NONE   # current state
        self.__state_obj = MUV_UVBBStateNone(self.__cmd_exec)

    def __update_state(self, next_state, ctrl_points):
        """
        Update state
        """

        if next_state == self.__state:
            return
        obj = None
        if next_state == MUV_UVBBState.TRANSLATING:
            obj = MUV_UVBBStateTranslating(self.__cmd_exec, ctrl_points)
        elif MUV_UVBBState.SCALING_1 <= next_state <= MUV_UVBBState.SCALING_8:
            obj = MUV_UVBBStateScaling(
                self.__cmd_exec, next_state, ctrl_points)
        elif next_state == MUV_UVBBState.ROTATING:
            obj = MUV_UVBBStateRotating(self.__cmd_exec, ctrl_points)
        elif next_state == MUV_UVBBState.NONE:
            obj = MUV_UVBBStateNone(self.__cmd_exec)
        elif (MUV_UVBBState.UNIFORM_SCALING_1 <= next_state <=
              MUV_UVBBState.UNIFORM_SCALING_4):
            obj = MUV_UVBBStateUniformScaling(
                self.__cmd_exec, next_state, ctrl_points)

        if obj is not None:
            self.__state_obj = obj

        self.__state = next_state

    def update(self, context, ctrl_points, event):
        mouse_region = mathutils.Vector((
            event.mouse_region_x, event.mouse_region_y))
        mouse_view = mathutils.Vector((context.region.view2d.region_to_view(
            mouse_region.x, mouse_region.y)))
        next_state = self.__state_obj.update(
            context, event, ctrl_points, mouse_view)
        self.__update_state(next_state, ctrl_points)


class MUV_UVBBUpdater(bpy.types.Operator):
    """
    Operation class: Update state and handle event by modal function
    """

    bl_idname = "uv.muv_uvbb_updater"
    bl_label = "UV Bounding Box Updater"
    bl_description = "Update UV Bounding Box"
    bl_options = {'REGISTER', 'UNDO'}

    def __init__(self):
        self.__timer = None
        self.__cmd_exec = MUV_UVBBCmdExecuter()         # Command executer
        self.__state_mgr = MUV_UVBBStateMgr(self.__cmd_exec)    # State Manager

    def __handle_add(self, context):
        if self.__timer is None:
            self.__timer = context.window_manager.event_timer_add(
                0.1, context.window)
            context.window_manager.modal_handler_add(self)
        MUV_UVBBRenderer.handle_add(self, context)

    def __handle_remove(self, context):
        MUV_UVBBRenderer.handle_remove()
        if self.__timer is not None:
            context.window_manager.event_timer_remove(self.__timer)
            self.__timer = None

    def __get_uv_info(self, context):
        """
        Get UV coordinate
        """
        obj = context.active_object
        uv_info = []
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()
        if not bm.loops.layers.uv:
            return None
        uv_layer = bm.loops.layers.uv.verify()
        for f in bm.faces:
            if f.select:
                for i, l in enumerate(f.loops):
                    uv_info.append((f.index, i, l[uv_layer].uv.copy()))
        if len(uv_info) == 0:
            return None
        return uv_info

    def __get_ctrl_point(self, uv_info_ini):
        """
        Get control point
        """
        left = MAX_VALUE
        right = -MAX_VALUE
        top = -MAX_VALUE
        bottom = MAX_VALUE

        for info in uv_info_ini:
            uv = info[2]
            if uv.x < left:
                left = uv.x
            if uv.x > right:
                right = uv.x
            if uv.y < bottom:
                bottom = uv.y
            if uv.y > top:
                top = uv.y

        points = [
            mathutils.Vector((
                (left + right) * 0.5, (top + bottom) * 0.5, 0.0
            )),
            mathutils.Vector((left, top, 0.0)),
            mathutils.Vector((left, (top + bottom) * 0.5, 0.0)),
            mathutils.Vector((left, bottom, 0.0)),
            mathutils.Vector(((left + right) * 0.5, top, 0.0)),
            mathutils.Vector(((left + right) * 0.5, bottom, 0.0)),
            mathutils.Vector((right, top, 0.0)),
            mathutils.Vector((right, (top + bottom) * 0.5, 0.0)),
            mathutils.Vector((right, bottom, 0.0)),
            mathutils.Vector(((left + right) * 0.5, top + 0.03, 0.0))
        ]

        return points

    def __update_uvs(self, context, uv_info_ini, trans_mat):
        """
        Update UV coordinate
        """
        obj = context.active_object
        bm = bmesh.from_edit_mesh(obj.data)
        if muv_common.check_version(2, 73, 0) >= 0:
            bm.faces.ensure_lookup_table()
        if not bm.loops.layers.uv:
            return
        uv_layer = bm.loops.layers.uv.verify()
        for info in uv_info_ini:
            fidx = info[0]
            lidx = info[1]
            uv = info[2]
            v = mathutils.Vector((uv.x, uv.y, 0.0))
            av = trans_mat * v
            bm.faces[fidx].loops[lidx][uv_layer].uv = mathutils.Vector(
                (av.x, av.y))

    def __update_ctrl_point(self, ctrl_points_ini, trans_mat):
        """
        Update control point
        """
        return [trans_mat * cp for cp in ctrl_points_ini]

    def modal(self, context, event):
        props = context.scene.muv_props.uvbb
        muv_common.redraw_all_areas()
        if props.running is False:
            self.__handle_remove(context)
            return {'FINISHED'}
        if event.type == 'TIMER':
            trans_mat = self.__cmd_exec.execute()
            self.__update_uvs(context, props.uv_info_ini, trans_mat)
            props.ctrl_points = self.__update_ctrl_point(
                props.ctrl_points_ini, trans_mat)

        self.__state_mgr.update(context, props.ctrl_points, event)

        return {'PASS_THROUGH'}

    def execute(self, context):
        props = context.scene.muv_props.uvbb

        if props.running is True:
            props.running = False
            return {'FINISHED'}

        props.uv_info_ini = self.__get_uv_info(context)
        if props.uv_info_ini is None:
            return {'CANCELLED'}
        props.ctrl_points_ini = self.__get_ctrl_point(props.uv_info_ini)
        trans_mat = self.__cmd_exec.execute()
        # Update is needed in order to display control point
        self.__update_uvs(context, props.uv_info_ini, trans_mat)
        props.ctrl_points = self.__update_ctrl_point(
            props.ctrl_points_ini, trans_mat)
        self.__handle_add(context)
        props.running = True

        return {'RUNNING_MODAL'}


class IMAGE_PT_MUV_UVBB(bpy.types.Panel):
    """
    Panel class: UV Bounding Box Menu on Property Panel on UV/ImageEditor
    """

    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "UV Bounding Box"
    bl_context = 'mesh_edit'

    @classmethod
    def poll(cls, context):
        prefs = context.user_preferences.addons["uv_magic_uv"].preferences
        return prefs.enable_uvbb

    def draw_header(self, _):
        layout = self.layout
        layout.label(text="", icon='IMAGE_COL')

    def draw(self, context):
        sc = context.scene
        props = sc.muv_props.uvbb
        layout = self.layout
        if props.running is False:
            layout.operator(
                MUV_UVBBUpdater.bl_idname, text="Display UV Bounding Box",
                icon='PLAY')
        else:
            layout.operator(
                MUV_UVBBUpdater.bl_idname, text="Hide UV Bounding Box",
                icon='PAUSE')
        layout.prop(sc, "muv_uvbb_uniform_scaling", text="Uniform Scaling")
