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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110- 1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# ----------------------------------------------------------
# Author: Stephen Leger (s-leger)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
# noinspection PyUnresolvedReferences
from bpy.types import Operator, PropertyGroup, Mesh, Panel
from bpy.props import (
    FloatProperty, IntProperty, BoolProperty, BoolVectorProperty,
    CollectionProperty, FloatVectorProperty, EnumProperty, StringProperty
)
from mathutils import Vector, Matrix
from math import tan, sqrt
from .bmesh_utils import BmeshEdit as bmed
from .panel import Panel as WindowPanel
from .archipack_handle import create_handle, window_handle_vertical_01, window_handle_vertical_02
# from .archipack_door_panel import ARCHIPACK_OT_select_parent
from .archipack_manipulator import Manipulable
from .archipack_preset import ArchipackPreset, PresetMenuOperator
from .archipack_gl import FeedbackPanel
from .archipack_object import ArchipackObject, ArchipackCreateTool, ArchpackDrawTool
from .archipack_keymaps import Keymaps


def update(self, context):
    self.update(context)


def update_childs(self, context):
    self.update(context, childs_only=True)


def update_portal(self, context):
    self.update_portal(context)


def set_cols(self, value):
    if self.n_cols != value:
        self.auto_update = False
        self._set_width(value)
        self.auto_update = True
        self.n_cols = value
    return None


def get_cols(self):
    return self.n_cols


class archipack_window_panelrow(PropertyGroup):
    width = FloatVectorProperty(
            name="Width",
            min=0.5,
            max=100.0,
            default=[
                50, 50, 50, 50, 50, 50, 50, 50,
                50, 50, 50, 50, 50, 50, 50, 50,
                50, 50, 50, 50, 50, 50, 50, 50,
                50, 50, 50, 50, 50, 50, 50
            ],
            size=31,
            update=update
            )
    fixed = BoolVectorProperty(
            name="Fixed",
            default=[
                False, False, False, False, False, False, False, False,
                False, False, False, False, False, False, False, False,
                False, False, False, False, False, False, False, False,
                False, False, False, False, False, False, False, False
            ],
            size=32,
            update=update
            )
    cols = IntProperty(
            name="Panels",
            description="number of panels getter and setter, to avoid infinite recursion",
            min=1,
            max=32,
            default=2,
            get=get_cols, set=set_cols
            )
    n_cols = IntProperty(
            name="Panels",
            description="store number of panels, internal use only to avoid infinite recursion",
            min=1,
            max=32,
            default=2,
            update=update
            )
    height = FloatProperty(
            name="Height",
            min=0.1,
            default=1.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            update=update
            )
    auto_update = BoolProperty(
            options={'SKIP_SAVE'},
            name="auto_update",
            description="disable auto update to avoid infinite recursion",
            default=True
            )

    def get_row(self, x, y):
        size = [Vector((x * self.width[w] / 100, y, 0)) for w in range(self.cols - 1)]
        sum_x = sum([s.x for s in size])
        size.append(Vector((x - sum_x, y, 0)))
        origin = []
        pivot = []
        ttl = 0
        xh = x / 2
        n_center = len(size) / 2
        for i, sx in enumerate(size):
            ttl += sx.x
            if i < n_center:
                # pivot left
                origin.append(Vector((ttl - xh - sx.x, 0)))
                pivot.append(1)
            else:
                # pivot right
                origin.append(Vector((ttl - xh, 0)))
                pivot.append(-1)
        return size, origin, pivot

    def _set_width(self, cols):
        width = 100 / cols
        for i in range(cols - 1):
            self.width[i] = width

    def find_datablock_in_selection(self, context):
        """
            find witch selected object this instance belongs to
            provide support for "copy to selected"
        """
        selected = [o for o in context.selected_objects]
        for o in selected:
            props = archipack_window.datablock(o)
            if props:
                for row in props.rows:
                    if row == self:
                        return props
        return None

    def update(self, context):
        if self.auto_update:
            props = self.find_datablock_in_selection(context)
            if props is not None:
                props.update(context, childs_only=False)

    def draw(self, layout, context, last_row):
        # store parent at runtime to trigger update on parent
        row = layout.row()
        row.prop(self, "cols")
        row = layout.row()
        if not last_row:
            row.prop(self, "height")
        for i in range(self.cols - 1):
            row = layout.row()
            row.prop(self, "width", text="col " + str(i + 1), index=i)
            row.prop(self, "fixed", text="fixed", index=i)
        row = layout.row()
        row.label(text="col " + str(self.cols))
        row.prop(self, "fixed", text="fixed", index=(self.cols - 1))


class archipack_window_panel(ArchipackObject, PropertyGroup):
    center = FloatVectorProperty(
            subtype='XYZ'
            )
    origin = FloatVectorProperty(
            subtype='XYZ'
            )
    size = FloatVectorProperty(
            subtype='XYZ'
            )
    radius = FloatVectorProperty(
            subtype='XYZ'
            )
    angle_y = FloatProperty(
            name='angle',
            unit='ROTATION',
            subtype='ANGLE',
            min=-1.5, max=1.5,
            default=0, precision=2,
            description='angle'
            )
    frame_y = FloatProperty(
            name='Depth',
            min=0,
            default=0.06, precision=2,
            unit='LENGTH', subtype='DISTANCE',
            description='frame depth'
            )
    frame_x = FloatProperty(
            name='Width',
            min=0,
            default=0.06, precision=2,
            unit='LENGTH', subtype='DISTANCE',
            description='frame width'
            )
    curve_steps = IntProperty(
            name="curve steps",
            min=1,
            max=128,
            default=1
            )
    shape = EnumProperty(
            name='Shape',
            items=(
                ('RECTANGLE', 'Rectangle', '', 0),
                ('ROUND', 'Top Round', '', 1),
                ('ELLIPSIS', 'Top elliptic', '', 2),
                ('QUADRI', 'Top oblique', '', 3),
                ('CIRCLE', 'Full circle', '', 4)
                ),
            default='RECTANGLE'
            )
    pivot = FloatProperty(
            name='pivot',
            min=-1, max=1,
            default=-1, precision=2,
            description='pivot'
            )
    side_material = IntProperty(
            name="side material",
            min=0,
            max=2,
            default=0
            )
    handle = EnumProperty(
            name='Shape',
            items=(
                ('NONE', 'No handle', '', 0),
                ('INSIDE', 'Inside', '', 1),
                ('BOTH', 'Inside and outside', '', 2)
                ),
            default='NONE'
            )
    handle_model = IntProperty(
            name="handle model",
            default=1,
            min=1,
            max=2
            )
    handle_altitude = FloatProperty(
            name='handle altitude',
            min=0,
            default=0.2, precision=2,
            unit='LENGTH', subtype='DISTANCE',
            description='handle altitude'
            )
    fixed = BoolProperty(
            name="Fixed",
            default=False
            )
    enable_glass = BoolProperty(
            name="Enable glass",
            default=True
            )

    @property
    def window(self):
        verre = 0.005
        chanfer = 0.004
        x0 = 0
        x1 = self.frame_x
        x2 = 0.75 * self.frame_x
        x3 = chanfer
        y0 = -self.frame_y
        y1 = 0
        y2 = -0.5 * self.frame_y
        y3 = -chanfer
        y4 = chanfer - self.frame_y

        if self.fixed:
            # profil carre avec support pour verre
            # p ______       y1
            # / |      y3
            # |       |___
            # x       |___   y2  verre
            # |       |      y4
            #  \______|      y0
            # x0 x3   x1
            #
            x1 = 0.5 * self.frame_x
            y1 = -0.45 * self.frame_y
            y3 = y1 - chanfer
            y4 = chanfer + y0
            y2 = (y0 + y2) / 2

            side_cap_front = -1
            side_cap_back = -1

            if self.enable_glass:
                side_cap_front = 6
                side_cap_back = 7

            return WindowPanel(
                True,  # closed
                [1, 0, 0, 0, 1, 2, 2, 2, 2],  # x index
                [x0, x3, x1],
                [y0, y4, y2, y3, y1, y1, y2 + verre, y2 - verre, y0],
                [0, 0, 1, 1, 1, 1, 0, 0, 0],  # materials
                side_cap_front=side_cap_front,
                side_cap_back=side_cap_back      # cap index
                )
        else:
            # profil avec chanfrein et joint et support pour verre
            # p ____         y1    inside
            # / |_       y3
            # |       |___
            # x       |___   y2  verre
            # |      _|      y4
            #  \____|        y0
            # x0 x3 x2 x1          outside
            if self.side_material == 0:
                materials = [0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0]
            elif self.side_material == 1:
                # rail window exterior
                materials = [0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0]
            else:
                # rail window interior
                materials = [0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0]

            side_cap_front = -1
            side_cap_back = -1

            if self.enable_glass:
                side_cap_front = 8
                side_cap_back = 9

            return WindowPanel(
                True,            # closed shape
                [1, 0, 0, 0, 1, 2, 2, 3, 3, 3, 3, 2, 2],     # x index
                [x0, x3, x2, x1],     # unique x positions
                [y0, y4, y2, y3, y1, y1, y3, y3, y2 + verre, y2 - verre, y4, y4, y0],
                materials,     # materials
                side_cap_front=side_cap_front,
                side_cap_back=side_cap_back      # cap index
                )

    @property
    def verts(self):
        offset = Vector((0, 0, 0))
        return self.window.vertices(self.curve_steps, offset, self.center, self.origin, self.size,
            self.radius, self.angle_y, self.pivot, shape_z=None, path_type=self.shape)

    @property
    def faces(self):
        return self.window.faces(self.curve_steps, path_type=self.shape)

    @property
    def matids(self):
        return self.window.mat(self.curve_steps, 2, 2, path_type=self.shape)

    @property
    def uvs(self):
        return self.window.uv(self.curve_steps, self.center, self.origin, self.size,
            self.radius, self.angle_y, self.pivot, 0, self.frame_x, path_type=self.shape)

    def find_handle(self, o):
        for child in o.children:
            if 'archipack_handle' in child:
                return child
        return None

    def update_handle(self, context, o):
        handle = self.find_handle(o)
        if handle is None:
            m = bpy.data.meshes.new("Handle")
            handle = create_handle(context, o, m)
            # MaterialUtils.add_handle_materials(handle)
        if self.handle_model == 1:
            verts, faces = window_handle_vertical_01(1)
        else:
            verts, faces = window_handle_vertical_02(1)
        handle.location = (self.pivot * (self.size.x - 0.4 * self.frame_x), 0, self.handle_altitude)
        bmed.buildmesh(context, handle, verts, faces)

    def remove_handle(self, context, o):
        handle = self.find_handle(o)
        if handle is not None:
            context.scene.objects.unlink(handle)
            bpy.data.objects.remove(handle, do_unlink=True)

    def update(self, context):

        o = self.find_in_selection(context)

        if o is None:
            return

        if self.handle == 'NONE':
            self.remove_handle(context, o)
        else:
            self.update_handle(context, o)

        bmed.buildmesh(context, o, self.verts, self.faces, self.matids, self.uvs)

        self.restore_context(context)


class archipack_window(ArchipackObject, Manipulable, PropertyGroup):
    x = FloatProperty(
            name='Width',
            min=0.25,
            default=100.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='Width', update=update
            )
    y = FloatProperty(
            name='Depth',
            min=0.1,
            default=0.20, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='Depth', update=update,
            )
    z = FloatProperty(
            name='Height',
            min=0.1,
            default=1.2, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='height', update=update,
            )
    angle_y = FloatProperty(
            name='Angle',
            unit='ROTATION',
            subtype='ANGLE',
            min=-1.5, max=1.5,
            default=0, precision=2,
            description='angle', update=update,
            )
    radius = FloatProperty(
            name='Radius',
            min=0.1,
            default=2.5, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='radius', update=update,
            )
    elipsis_b = FloatProperty(
            name='Ellipsis',
            min=0.1,
            default=0.5, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='ellipsis vertical size', update=update,
            )
    altitude = FloatProperty(
            name='Altitude',
            default=1.0, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='altitude', update=update,
            )
    offset = FloatProperty(
            name='Offset',
            default=0.1, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='offset', update=update,
            )
    frame_y = FloatProperty(
            name='Depth',
            min=0,
            default=0.06, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='frame depth', update=update,
            )
    frame_x = FloatProperty(
            name='Width',
            min=0,
            default=0.06, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='frame width', update=update,
            )
    out_frame = BoolProperty(
            name="Out frame",
            default=False, update=update,
            )
    out_frame_y = FloatProperty(
            name='Side depth',
            min=0.001,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='frame side depth', update=update,
            )
    out_frame_y2 = FloatProperty(
            name='Front depth',
            min=0.001,
            default=0.02, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='frame front depth', update=update,
            )
    out_frame_x = FloatProperty(
            name='Front Width',
            min=0.0,
            default=0.1, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='frame width set to 0 disable front frame', update=update,
            )
    out_frame_offset = FloatProperty(
            name='Offset',
            min=0.0,
            default=0.0, precision=3, step=0.1,
            unit='LENGTH', subtype='DISTANCE',
            description='frame offset', update=update,
            )
    out_tablet_enable = BoolProperty(
            name="Out tablet",
            default=True, update=update,
            )
    out_tablet_x = FloatProperty(
            name='Width',
            min=0.0,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='tablet width', update=update,
            )
    out_tablet_y = FloatProperty(
            name='Depth',
            min=0.001,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='tablet depth', update=update,
            )
    out_tablet_z = FloatProperty(
            name='Height',
            min=0.001,
            default=0.03, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='tablet height', update=update,
            )
    in_tablet_enable = BoolProperty(
            name="In tablet",
            default=True, update=update,
            )
    in_tablet_x = FloatProperty(
            name='Width',
            min=0.0,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='tablet width', update=update,
            )
    in_tablet_y = FloatProperty(
            name='Depth',
            min=0.001,
            default=0.04, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='tablet depth', update=update,
            )
    in_tablet_z = FloatProperty(
            name='Height',
            min=0.001,
            default=0.03, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='tablet height', update=update,
            )
    blind_enable = BoolProperty(
            name="Blind",
            default=False, update=update,
            )
    blind_y = FloatProperty(
            name='Depth',
            min=0.001,
            default=0.002, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='Store depth', update=update,
            )
    blind_z = FloatProperty(
            name='Height',
            min=0.001,
            default=0.03, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='Store height', update=update,
            )
    blind_open = FloatProperty(
            name='Open',
            min=0.0, max=100,
            default=80, precision=1,
            subtype='PERCENTAGE',
            description='Store open', update=update,
            )
    rows = CollectionProperty(type=archipack_window_panelrow)
    n_rows = IntProperty(
            name="Number of rows",
            min=1,
            max=32,
            default=1, update=update,
            )
    curve_steps = IntProperty(
            name="Steps",
            min=6,
            max=128,
            default=16, update=update,
            )
    hole_outside_mat = IntProperty(
            name="Outside",
            min=0,
            max=128,
            default=0, update=update,
            )
    hole_inside_mat = IntProperty(
            name="Inside",
            min=0,
            max=128,
            default=1, update=update,
            )
    window_shape = EnumProperty(
            name='Shape',
            items=(
                ('RECTANGLE', 'Rectangle', '', 0),
                ('ROUND', 'Top Round', '', 1),
                ('ELLIPSIS', 'Top elliptic', '', 2),
                ('QUADRI', 'Top oblique', '', 3),
                ('CIRCLE', 'Full circle', '', 4)
                ),
            default='RECTANGLE', update=update,
            )
    window_type = EnumProperty(
            name='Type',
            items=(
                ('FLAT', 'Flat window', '', 0),
                ('RAIL', 'Rail window', '', 1)
                ),
            default='FLAT', update=update,
            )
    enable_glass = BoolProperty(
            name="Enable glass",
            default=True,
            update=update
            )
    warning = BoolProperty(
            options={'SKIP_SAVE'},
            name="Warning",
            default=False
            )
    handle_enable = BoolProperty(
            name='Handle',
            default=True, update=update_childs,
            )
    handle_altitude = FloatProperty(
            name="Altitude",
            min=0,
            default=1.4, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='handle altitude', update=update_childs,
            )
    hole_margin = FloatProperty(
            name='Hole margin',
            min=0.0,
            default=0.1, precision=2, step=1,
            unit='LENGTH', subtype='DISTANCE',
            description='how much hole surround wall'
            )
    flip = BoolProperty(
            default=False,
            update=update,
            description='flip outside and outside material of hole'
            )
    # layout related
    display_detail = BoolProperty(
            options={'SKIP_SAVE'},
            default=False
            )
    display_panels = BoolProperty(
            options={'SKIP_SAVE'},
            default=True
            )
    display_materials = BoolProperty(
            options={'SKIP_SAVE'},
            default=True
            )

    auto_update = BoolProperty(
            options={'SKIP_SAVE'},
            default=True,
            update=update
            )
    portal = BoolProperty(
            default=False,
            name="Portal",
            description="Generate a portal",
            update=update
            )

    @property
    def shape(self):
        if self.window_type == 'RAIL':
            return 'RECTANGLE'
        else:
            return self.window_shape

    @property
    def window(self):
        # Flat window frame profil
        #  ___        y1
        # |   |__
        # |      |    y2
        # |______|    y0
        #
        x0 = 0
        x1 = -x0 - self.frame_x
        x2 = x0 + 0.5 * self.frame_x
        y0 = 0.5 * self.y - self.offset
        y2 = y0 + 0.5 * self.frame_y

        if self.window_type == 'FLAT':
            y1 = y0 + self.frame_y
            return WindowPanel(
                True,     # closed
                [0, 0, 1, 1, 2, 2],     # x index
                [x1, x0, x2],
                [y0, y1, y1, y2, y2, y0],
                [1, 1, 1, 1, 0, 0]  # material index
                )
        else:
            # Rail window frame profil
            #  ________       y1
            # |      __|      y5
            # |     |__       y4
            # |      __|      y3
            # |     |____
            # |          |    y2
            # |__________|    y0
            # -x1   0 x3 x2
            x2 = x0 + 0.35 * self.frame_y
            x3 = x0 + 0.2 * self.frame_x
            y1 = y0 + 2.55 * self.frame_y
            y3 = y0 + 1.45 * self.frame_y
            y4 = y0 + 1.55 * self.frame_y
            y5 = y0 + 2.45 * self.frame_y

            return WindowPanel(
                True,     # closed
                [0, 0, 2, 2, 1, 1, 2, 2, 1, 1, 3, 3],     # x index
                [x1, x0, x3, x2],
                [y0, y1, y1, y5, y5, y4, y4, y3, y3, y2, y2, y0],
                [1, 1, 1, 3, 1, 3, 3, 3, 0, 3, 0, 0]  # material index
                )

    @property
    def hole(self):
        # profil percement                          ____
        #   _____  y_inside          vertical ___|      x1
        #  |
        #  |__     y0                outside   ___
        #     |___ y_outside                       |____ x1-shape_z     inside
        # -x1 x0
        y0 = 0.5 * self.y - self.offset
        x1 = self.frame_x     # sur-largeur percement interieur
        y_inside = 0.5 * self.y + self.hole_margin     # outside wall

        if self.out_frame is False:
            x0 = 0
        else:
            x0 = -min(self.frame_x - 0.001, self.out_frame_y + self.out_frame_offset)

        outside_mat = self.hole_outside_mat
        inside_mat = self.hole_inside_mat
        # if self.flip:
        #    outside_mat, inside_mat = inside_mat, outside_mat

        y_outside = -y_inside           # inside wall

        return WindowPanel(
            False,     # closed
            [1, 1, 0, 0],     # x index
            [-x1, x0],
            [y_outside, y0, y0, y_inside],
            [outside_mat, outside_mat, inside_mat],     # material index
            side_cap_front=3,     # cap index
            side_cap_back=0
            )

    @property
    def frame(self):
        # profil cadre
        #     ___     y0
        #  __|   |
        # |      |    y2
        # |______|    y1
        # x1 x2  x0
        y2 = -0.5 * self.y
        y0 = 0.5 * self.y - self.offset
        y1 = y2 - self.out_frame_y2
        x0 = 0   # -min(self.frame_x - 0.001, self.out_frame_offset)
        x1 = x0 - self.out_frame_x
        x2 = x0 - self.out_frame_y
        # y = depth
        # x = width
        if self.out_frame_x <= self.out_frame_y:
            if self.out_frame_x == 0:
                pts_y = [y2, y0, y0, y2]
            else:
                pts_y = [y1, y0, y0, y1]
            return WindowPanel(
                True,     # closed profil
                [0, 0, 1, 1],     # x index
                [x2, x0],
                pts_y,
                [0, 0, 0, 0],     # material index
                closed_path=bool(self.shape == 'CIRCLE')  # closed path
                )
        else:
            return WindowPanel(
                True,     # closed profil
                [0, 0, 1, 1, 2, 2],     # x index
                [x1, x2, x0],
                [y1, y2, y2, y0, y0, y1],
                [0, 0, 0, 0, 0, 0],     # material index
                closed_path=bool(self.shape == 'CIRCLE')   # closed path
                )

    @property
    def out_tablet(self):
        # profil tablette
        #  __  y0
        # |  | y2
        # | / y3
        # |_| y1
        # x0 x2 x1
        y0 = 0.001 + 0.5 * self.y - self.offset
        y1 = -0.5 * self.y - self.out_tablet_y
        y2 = y0 - 0.01
        y3 = y2 - 0.04
        x2 = 0
        x0 = x2 - self.out_tablet_z
        x1 = 0.3 * self.frame_x
        # y = depth
        # x = width1
        return WindowPanel(
            True,     # closed profil
            [1, 1, 2, 2, 0, 0],     # x index
            [x0, x2, x1],
            [y1, y3, y2, y0, y0, y1],
            [4, 3, 3, 4, 4, 4],     # material index
            closed_path=False           # closed path
            )

    @property
    def in_tablet(self):
        # profil tablette
        #  __  y0
        # |  |
        # |  |
        # |__| y1
        # x0  x1
        y0 = 0.5 * self.y + self.frame_y - self.offset
        y1 = 0.5 * self.y + self.in_tablet_y
        if self.window_type == 'RAIL':
            y0 += 1.55 * self.frame_y
            y1 += 1.55 * self.frame_y
        x0 = -self.frame_x
        x1 = min(x0 + self.in_tablet_z, x0 + self.frame_x - 0.001)
        # y = depth
        # x = width1
        return WindowPanel(
            True,     # closed profil
            [0, 0, 1, 1],     # x index
            [x0, x1],
            [y1, y0, y0, y1],
            [1, 1, 1, 1],     # material index
            closed_path=False           # closed path
            )

    @property
    def blind(self):
        # profil blind
        #  y0
        #  | / | / | /
        #  y1
        # xn x1 x0
        dx = self.z / self.blind_z
        nx = int(self.z / dx)
        y0 = -0.5 * self.offset
        # -0.5 * self.y + 0.5 * (0.5 * self.y - self.offset)
        # 0.5 * (-0.5 * self.y-0.5 * self.offset)
        y1 = y0 + self.blind_y
        nx = int(self.z / self.blind_z)
        dx = self.z / nx
        open = 1.0 - self.blind_open / 100
        return WindowPanel(
            False,                      # profil closed
            [int((i + (i % 2)) / 2) for i in range(2 * nx)],     # x index
            [self.z - (dx * i * open) for i in range(nx + 1)],     # x
            [[y1, y0][i % 2] for i in range(2 * nx)],     #
            [5 for i in range(2 * nx - 1)],     # material index
            closed_path=False                           #
            )

    @property
    def verts(self):
        center, origin, size, radius = self.get_radius()
        is_not_circle = self.shape != 'CIRCLE'
        offset = Vector((0, self.altitude, 0))
        verts = self.window.vertices(self.curve_steps, offset, center, origin,
            size, radius, self.angle_y, 0, shape_z=None, path_type=self.shape)
        if self.out_frame:
            verts += self.frame.vertices(self.curve_steps, offset, center, origin,
                size, radius, self.angle_y, 0, shape_z=None, path_type=self.shape)
        if is_not_circle and self.out_tablet_enable:
            verts += self.out_tablet.vertices(self.curve_steps, offset, center, origin,
                Vector((size.x + 2 * self.out_tablet_x, size.y, size.z)),
                radius, self.angle_y, 0, shape_z=None, path_type='HORIZONTAL')
        if is_not_circle and self.in_tablet_enable:
            verts += self.in_tablet.vertices(self.curve_steps, offset, center, origin,
                Vector((size.x + 2 * (self.frame_x + self.in_tablet_x), size.y, size.z)),
                radius, self.angle_y, 0, shape_z=None, path_type='HORIZONTAL')
        if is_not_circle and self.blind_enable:
            verts += self.blind.vertices(self.curve_steps, offset, center, origin,
                Vector((-size.x, 0, 0)), radius, 0, 0, shape_z=None, path_type='HORIZONTAL')
        return verts

    @property
    def faces(self):
        window = self.window
        faces = window.faces(self.curve_steps, path_type=self.shape)
        verts_offset = window.n_verts(self.curve_steps, path_type=self.shape)
        is_not_circle = self.shape != 'CIRCLE'
        if self.out_frame:
            frame = self.frame
            faces += frame.faces(self.curve_steps, path_type=self.shape, offset=verts_offset)
            verts_offset += frame.n_verts(self.curve_steps, path_type=self.shape)
        if is_not_circle and self.out_tablet_enable:
            tablet = self.out_tablet
            faces += tablet.faces(self.curve_steps, path_type='HORIZONTAL', offset=verts_offset)
            verts_offset += tablet.n_verts(self.curve_steps, path_type='HORIZONTAL')
        if is_not_circle and self.in_tablet_enable:
            tablet = self.in_tablet
            faces += tablet.faces(self.curve_steps, path_type='HORIZONTAL', offset=verts_offset)
            verts_offset += tablet.n_verts(self.curve_steps, path_type='HORIZONTAL')
        if is_not_circle and self.blind_enable:
            blind = self.blind
            faces += blind.faces(self.curve_steps, path_type='HORIZONTAL', offset=verts_offset)
            verts_offset += blind.n_verts(self.curve_steps, path_type='HORIZONTAL')

        return faces

    @property
    def matids(self):
        mat = self.window.mat(self.curve_steps, 2, 2, path_type=self.shape)
        is_not_circle = self.shape != 'CIRCLE'
        if self.out_frame:
            mat += self.frame.mat(self.curve_steps, 0, 0, path_type=self.shape)
        if is_not_circle and self.out_tablet_enable:
            mat += self.out_tablet.mat(self.curve_steps, 0, 0, path_type='HORIZONTAL')
        if is_not_circle and self.in_tablet_enable:
            mat += self.in_tablet.mat(self.curve_steps, 0, 0, path_type='HORIZONTAL')
        if is_not_circle and self.blind_enable:
            mat += self.blind.mat(self.curve_steps, 0, 0, path_type='HORIZONTAL')
        return mat

    @property
    def uvs(self):
        center, origin, size, radius = self.get_radius()
        uvs = self.window.uv(self.curve_steps, center, origin, size, radius,
            self.angle_y, 0, 0, self.frame_x, path_type=self.shape)
        is_not_circle = self.shape != 'CIRCLE'
        if self.out_frame:
            uvs += self.frame.uv(self.curve_steps, center, origin, size, radius,
                self.angle_y, 0, 0, self.frame_x, path_type=self.shape)
        if is_not_circle and self.out_tablet_enable:
            uvs += self.out_tablet.uv(self.curve_steps, center, origin, size, radius,
                self.angle_y, 0, 0, self.frame_x, path_type='HORIZONTAL')
        if is_not_circle and self.in_tablet_enable:
            uvs += self.in_tablet.uv(self.curve_steps, center, origin, size, radius,
                self.angle_y, 0, 0, self.frame_x, path_type='HORIZONTAL')
        if is_not_circle and self.blind_enable:
            uvs += self.blind.uv(self.curve_steps, center, origin, size, radius,
                self.angle_y, 0, 0, self.frame_x, path_type='HORIZONTAL')
        return uvs

    def find_portal(self, o):
        for child in o.children:
            if child.type == 'LAMP':
                return child
        return None

    def update_portal(self, context, o):

        lamp = self.find_portal(o)
        if self.portal:
            if lamp is None:
                bpy.ops.object.lamp_add(type='AREA')
                lamp = context.active_object
                lamp.name = "Portal"
                lamp.parent = o

            d = lamp.data
            d.cycles.is_portal = True
            d.shape = 'RECTANGLE'
            d.size = self.x
            d.size_y = self.z

            tM = Matrix([
                [1, 0, 0, 0],
                [0, 0, -1, -0.5 * self.y],
                [0, 1, 0, 0.5 * self.z + self.altitude],
                [0, 0, 0, 1]
            ])
            lamp.matrix_world = o.matrix_world * tM

        elif lamp is not None:
            d = lamp.data
            context.scene.objects.unlink(lamp)
            bpy.data.objects.remove(lamp)
            bpy.data.lamps.remove(d)

        context.scene.objects.active = o

    def setup_manipulators(self):
        if len(self.manipulators) == 4:
            return
        s = self.manipulators.add()
        s.prop1_name = "x"
        s.prop2_name = "x"
        s.type_key = "SNAP_SIZE_LOC"
        s = self.manipulators.add()
        s.prop1_name = "y"
        s.prop2_name = "y"
        s.type_key = "SNAP_SIZE_LOC"
        s = self.manipulators.add()
        s.prop1_name = "z"
        s.normal = Vector((0, 1, 0))
        s = self.manipulators.add()
        s.prop1_name = "altitude"
        s.normal = Vector((0, 1, 0))

    def remove_childs(self, context, o, to_remove):
        for child in o.children:
            if to_remove < 1:
                return
            if archipack_window_panel.filter(child):
                to_remove -= 1
                self.remove_handle(context, child)
                context.scene.objects.unlink(child)
                bpy.data.objects.remove(child, do_unlink=True)

    def remove_handle(self, context, o):
        handle = self.find_handle(o)
        if handle is not None:
            context.scene.objects.unlink(handle)
            bpy.data.objects.remove(handle, do_unlink=True)

    def update_rows(self, context, o):
        # remove rows
        for i in range(len(self.rows), self.n_rows, -1):
            self.rows.remove(i - 1)

        # add rows
        for i in range(len(self.rows), self.n_rows):
            self.rows.add()

        # wanted childs
        if self.shape == 'CIRCLE':
            w_childs = 1
        elif self.window_type == 'RAIL':
            w_childs = self.rows[0].cols
        else:
            w_childs = sum([row.cols for row in self.rows])

        # real childs
        childs = self.get_childs_panels(context, o)
        n_childs = len(childs)

        # remove child
        if n_childs > w_childs:
            self.remove_childs(context, o, n_childs - w_childs)

    def get_childs_panels(self, context, o):
        return [child for child in o.children if archipack_window_panel.filter(child)]

    def adjust_size_and_origin(self, size, origin, pivot, materials):
        if len(size) > 1:
            size[0].x += 0.5 * self.frame_x
            size[-1].x += 0.5 * self.frame_x
        for i in range(1, len(size) - 1):
            size[i].x += 0.5 * self.frame_x
            origin[i].x += -0.25 * self.frame_x * pivot[i]
        for i, o in enumerate(origin):
            o.y = (1 - (i % 2)) * self.frame_y
        for i, o in enumerate(origin):
            materials[i] = (1 - (i % 2)) + 1

    def find_handle(self, o):
        for handle in o.children:
            if 'archipack_handle' in handle:
                return handle
        return None

    def _synch_portal(self, context, o, linked, childs):
        # update portal
        dl = archipack_window.datablock(linked)
        dl.update_portal(context, linked)

    def _synch_childs(self, context, o, linked, childs):
        """
            sub synch childs nodes of linked object
        """

        # remove childs not found on source
        l_childs = self.get_childs_panels(context, linked)
        c_names = [c.data.name for c in childs]
        for c in l_childs:
            try:
                id = c_names.index(c.data.name)
            except:
                self.remove_handle(context, c)
                context.scene.objects.unlink(c)
                bpy.data.objects.remove(c, do_unlink=True)

        # children ordering may not be the same, so get the right l_childs order
        l_childs = self.get_childs_panels(context, linked)
        l_names = [c.data.name for c in l_childs]
        order = []
        for c in childs:
            try:
                id = l_names.index(c.data.name)
            except:
                id = -1
            order.append(id)

        # add missing childs and update other ones
        for i, child in enumerate(childs):
            if order[i] < 0:
                p = bpy.data.objects.new("Panel", child.data)
                context.scene.objects.link(p)
                p.lock_location[0] = True
                p.lock_location[1] = True
                p.lock_location[2] = True
                p.lock_rotation[1] = True
                p.lock_scale[0] = True
                p.lock_scale[1] = True
                p.lock_scale[2] = True
                p.parent = linked
                p.matrix_world = linked.matrix_world.copy()
                m = p.archipack_material.add()
                m.category = 'window'
                m.material = o.archipack_material[0].material
            else:
                p = l_childs[order[i]]

            # update handle
            handle = self.find_handle(child)
            h = self.find_handle(p)
            if handle is not None:
                if h is None:
                    h = create_handle(context, p, handle.data)
                h.location = handle.location.copy()
            elif h is not None:
                context.scene.objects.unlink(h)
                bpy.data.objects.remove(h, do_unlink=True)

            p.location = child.location.copy()

        # restore context
        context.scene.objects.active = o

    def _synch_hole(self, context, linked, hole):
        l_hole = self.find_hole(linked)
        if l_hole is None:
            l_hole = bpy.data.objects.new("hole", hole.data)
            l_hole['archipack_hole'] = True
            context.scene.objects.link(l_hole)
            for mat in hole.data.materials:
                l_hole.data.materials.append(mat)
            l_hole.parent = linked
            l_hole.matrix_world = linked.matrix_world.copy()
            l_hole.location = hole.location.copy()
        else:
            l_hole.data = hole.data

    def synch_childs(self, context, o):
        """
            synch childs nodes of linked objects
        """
        bpy.ops.object.select_all(action='DESELECT')
        o.select = True
        context.scene.objects.active = o
        childs = self.get_childs_panels(context, o)
        hole = self.find_hole(o)
        bpy.ops.object.select_linked(type='OBDATA')
        for linked in context.selected_objects:
            if linked != o:
                self._synch_portal(context, o, linked, childs)
                self._synch_childs(context, o, linked, childs)
                if hole is not None:
                    self._synch_hole(context, linked, hole)

    def update_childs(self, context, o):
        """
            pass params to childrens
        """
        self.update_rows(context, o)
        childs = self.get_childs_panels(context, o)
        n_childs = len(childs)
        child_n = 0
        row_n = 0
        location_y = 0.5 * self.y - self.offset + 0.5 * self.frame_y
        center, origin, size, radius = self.get_radius()
        offset = Vector((0, 0))
        handle = 'NONE'
        if self.shape != 'CIRCLE':
            if self.handle_enable:
                if self.z > 1.8:
                    handle = 'BOTH'
                else:
                    handle = 'INSIDE'
            is_circle = False
        else:
            is_circle = True

        if self.window_type == 'RAIL':
            handle_model = 2
        else:
            handle_model = 1

        for row in self.rows:
            row_n += 1
            if row_n < self.n_rows and not is_circle and self.window_type != 'RAIL':
                z = row.height
                shape = 'RECTANGLE'
            else:
                z = max(2 * self.frame_x + 0.001, self.z - offset.y)
                shape = self.shape

            self.warning = bool(z > self.z - offset.y)
            if self.warning:
                break
            size, origin, pivot = row.get_row(self.x, z)
            # side materials
            materials = [0 for i in range(row.cols)]

            handle_altitude = min(
                max(4 * self.frame_x, self.handle_altitude - offset.y - self.altitude),
                z - 4 * self.frame_x
                )

            if self.window_type == 'RAIL':
                self.adjust_size_and_origin(size, origin, pivot, materials)

            for panel in range(row.cols):
                child_n += 1

                if row.fixed[panel]:
                    enable_handle = 'NONE'
                else:
                    enable_handle = handle

                if child_n > n_childs:
                    bpy.ops.archipack.window_panel(
                        center=center,
                        origin=Vector((origin[panel].x, offset.y, 0)),
                        size=size[panel],
                        radius=radius,
                        pivot=pivot[panel],
                        shape=shape,
                        fixed=row.fixed[panel],
                        handle=enable_handle,
                        handle_model=handle_model,
                        handle_altitude=handle_altitude,
                        curve_steps=self.curve_steps,
                        side_material=materials[panel],
                        frame_x=self.frame_x,
                        frame_y=self.frame_y,
                        angle_y=self.angle_y,
                        enable_glass=self.enable_glass,
                        material=o.archipack_material[0].material
                    )
                    child = context.active_object
                    # parenting at 0, 0, 0 before set object matrix_world
                    # so location remains local from frame
                    child.parent = o
                    child.matrix_world = o.matrix_world.copy()

                else:
                    child = childs[child_n - 1]
                    child.select = True
                    context.scene.objects.active = child
                    props = archipack_window_panel.datablock(child)
                    if props is not None:
                        props.origin = Vector((origin[panel].x, offset.y, 0))
                        props.center = center
                        props.radius = radius
                        props.size = size[panel]
                        props.pivot = pivot[panel]
                        props.shape = shape
                        props.fixed = row.fixed[panel]
                        props.handle = enable_handle
                        props.handle_model = handle_model
                        props.handle_altitude = handle_altitude
                        props.side_material = materials[panel]
                        props.curve_steps = self.curve_steps
                        props.frame_x = self.frame_x
                        props.frame_y = self.frame_y
                        props.angle_y = self.angle_y
                        props.enable_glass = self.enable_glass
                        props.update(context)
                # location y + frame width. frame depends on choosen profile (fixed or not)
                # update linked childs location too
                child.location = Vector((origin[panel].x, origin[panel].y + location_y + self.frame_y,
                    self.altitude + offset.y))

                if not row.fixed[panel]:
                    handle = 'NONE'

                # only one single panel allowed for circle
                if is_circle:
                    return

            # only one single row allowed for rail window
            if self.window_type == 'RAIL':
                return
            offset.y += row.height

    def _get_tri_radius(self):
        return Vector((0, self.y, 0)), Vector((0, 0, 0)), \
            Vector((self.x, self.z, 0)), Vector((self.x, 0, 0))

    def _get_quad_radius(self):
        fx_z = self.z / self.x
        center_y = min(self.x / (self.x - self.frame_x) * self.z - self.frame_x * (1 + sqrt(1 + fx_z * fx_z)),
            abs(tan(self.angle_y) * (self.x)))
        if self.angle_y < 0:
            center_x = 0.5 * self.x
        else:
            center_x = -0.5 * self.x
        return Vector((center_x, center_y, 0)), Vector((0, 0, 0)), \
            Vector((self.x, self.z, 0)), Vector((self.x, 0, 0))

    def _get_round_radius(self):
        """
            bound radius to available space
            return center, origin, size, radius
        """
        x = 0.5 * self.x - self.frame_x
        # minimum space available
        y = self.z - sum([row.height for row in self.rows[:self.n_rows - 1]]) - 2 * self.frame_x
        y = min(y, x)
        # minimum radius inside
        r = y + x * (x - (y * y / x)) / (2 * y)
        radius = max(self.radius, 0.001 + self.frame_x + r)
        return Vector((0, self.z - radius, 0)), Vector((0, 0, 0)), \
            Vector((self.x, self.z, 0)), Vector((radius, 0, 0))

    def _get_circle_radius(self):
        """
            return center, origin, size, radius
        """
        return Vector((0, 0.5 * self.x, 0)), Vector((0, 0, 0)), \
            Vector((self.x, self.z, 0)), Vector((0.5 * self.x, 0, 0))

    def _get_ellipsis_radius(self):
        """
            return center, origin, size, radius
        """
        y = self.z - sum([row.height for row in self.rows[:self.n_rows - 1]])
        radius_b = max(0, 0.001 - 2 * self.frame_x + min(y, self.elipsis_b))
        return Vector((0, self.z - radius_b, 0)), Vector((0, 0, 0)), \
            Vector((self.x, self.z, 0)), Vector((self.x / 2, radius_b, 0))

    def get_radius(self):
        """
            return center, origin, size, radius
        """
        if self.shape == 'ROUND':
            return self._get_round_radius()
        elif self.shape == 'ELLIPSIS':
            return self._get_ellipsis_radius()
        elif self.shape == 'CIRCLE':
            return self._get_circle_radius()
        elif self.shape == 'QUADRI':
            return self._get_quad_radius()
        elif self.shape in ['TRIANGLE', 'PENTAGON']:
            return self._get_tri_radius()
        else:
            return Vector((0, 0, 0)), Vector((0, 0, 0)), \
                Vector((self.x, self.z, 0)), Vector((0, 0, 0))

    def update(self, context, childs_only=False):
        # support for "copy to selected"
        o = self.find_in_selection(context, self.auto_update)

        if o is None:
            return

        self.setup_manipulators()

        if childs_only is False:
            bmed.buildmesh(context, o, self.verts, self.faces, self.matids, self.uvs)

        self.update_portal(context, o)
        self.update_childs(context, o)

        # update hole
        if childs_only is False and self.find_hole(o) is not None:
            self.interactive_hole(context, o)

        # support for instances childs, update at object level
        self.synch_childs(context, o)

        # store 3d points for gl manipulators
        x, y = 0.5 * self.x, 0.5 * self.y
        self.manipulators[0].set_pts([(-x, -y, 0), (x, -y, 0), (1, 0, 0)])
        self.manipulators[1].set_pts([(-x, -y, 0), (-x, y, 0), (-1, 0, 0)])
        self.manipulators[2].set_pts([(x, -y, self.altitude), (x, -y, self.altitude + self.z), (-1, 0, 0)])
        self.manipulators[3].set_pts([(x, -y, 0), (x, -y, self.altitude), (-1, 0, 0)])

        # restore context
        self.restore_context(context)

    def find_hole(self, o):
        for child in o.children:
            if 'archipack_hole' in child:
                return child
        return None

    def interactive_hole(self, context, o):
        hole_obj = self.find_hole(o)

        if hole_obj is None:
            m = bpy.data.meshes.new("hole")
            hole_obj = bpy.data.objects.new("hole", m)
            context.scene.objects.link(hole_obj)
            hole_obj['archipack_hole'] = True
            hole_obj.parent = o
            hole_obj.matrix_world = o.matrix_world.copy()

        """
        hole_obj.data.materials.clear()

        for mat in o.data.materials:
            hole_obj.data.materials.append(mat)
            # MaterialUtils.add_wall2_materials(hole_obj)
        """

        hole = self.hole
        center, origin, size, radius = self.get_radius()

        if self.out_frame is False:
            x0 = 0
        else:
            x0 = min(self.frame_x - 0.001, self.out_frame_y + self.out_frame_offset)

        if self.out_tablet_enable:
            x0 -= min(self.frame_x - 0.001, self.out_tablet_z)
        shape_z = [0, x0]

        verts = hole.vertices(self.curve_steps, Vector((0, self.altitude, 0)), center, origin, size, radius,
            self.angle_y, 0, shape_z=shape_z, path_type=self.shape)

        faces = hole.faces(self.curve_steps, path_type=self.shape)

        matids = hole.mat(self.curve_steps, 2, 2, path_type=self.shape)

        uvs = hole.uv(self.curve_steps, center, origin, size, radius,
            self.angle_y, 0, 0, self.frame_x, path_type=self.shape)

        bmed.buildmesh(context, hole_obj, verts, faces, matids=matids, uvs=uvs)
        return hole_obj

    def robust_hole(self, context, tM):
        hole = self.hole
        center, origin, size, radius = self.get_radius()

        if self.out_frame is False:
            x0 = 0
        else:
            x0 = min(self.frame_x - 0.001, self.out_frame_y + self.out_frame_offset)

        if self.out_tablet_enable:
            x0 -= min(self.frame_x - 0.001, self.out_tablet_z)
        shape_z = [0, x0]

        m = bpy.data.meshes.new("hole")
        o = bpy.data.objects.new("hole", m)
        o['archipack_robusthole'] = True
        context.scene.objects.link(o)
        verts = hole.vertices(self.curve_steps, Vector((0, self.altitude, 0)), center, origin, size, radius,
            self.angle_y, 0, shape_z=shape_z, path_type=self.shape)

        verts = [tM * Vector(v) for v in verts]

        faces = hole.faces(self.curve_steps, path_type=self.shape)

        matids = hole.mat(self.curve_steps, 2, 2, path_type=self.shape)

        uvs = hole.uv(self.curve_steps, center, origin, size, radius,
            self.angle_y, 0, 0, self.frame_x, path_type=self.shape)

        bmed.buildmesh(context, o, verts, faces, matids=matids, uvs=uvs)
        # MaterialUtils.add_wall2_materials(o)
        o.select = True
        context.scene.objects.active = o
        return o


class ARCHIPACK_PT_window(Panel):
    bl_idname = "ARCHIPACK_PT_window"
    bl_label = "Window"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    # bl_context = 'object'
    bl_category = 'ArchiPack'

    # layout related
    display_detail = BoolProperty(
        default=False
    )
    display_panels = BoolProperty(
        default=True
    )

    @classmethod
    def poll(cls, context):
        return archipack_window.filter(context.active_object)

    def draw(self, context):
        o = context.active_object
        prop = archipack_window.datablock(o)
        if prop is None:
            return
        layout = self.layout
        layout.operator('archipack.window_manipulate', icon='HAND')
        row = layout.row(align=True)
        row.operator('archipack.window', text="Refresh", icon='FILE_REFRESH').mode = 'REFRESH'
        if o.data.users > 1:
            row.operator('archipack.window', text="Make unique", icon='UNLINKED').mode = 'UNIQUE'
        row.operator('archipack.window', text="Delete", icon='ERROR').mode = 'DELETE'
        box = layout.box()
        # box.label(text="Styles")
        row = box.row(align=True)
        row.operator("archipack.window_preset_menu", text=bpy.types.ARCHIPACK_OT_window_preset_menu.bl_label)
        row.operator("archipack.window_preset", text="", icon='ZOOMIN')
        row.operator("archipack.window_preset", text="", icon='ZOOMOUT').remove_active = True
        box = layout.box()
        box.prop(prop, 'window_type')
        box.prop(prop, 'x')
        box.prop(prop, 'y')
        if prop.window_shape != 'CIRCLE':
            box.prop(prop, 'z')
            if prop.warning:
                box.label(text="Insufficient height", icon='ERROR')
        box.prop(prop, 'altitude')
        box.prop(prop, 'offset')

        if prop.window_type == 'FLAT':
            box = layout.box()
            box.prop(prop, 'window_shape')
            if prop.window_shape in ['ROUND', 'CIRCLE', 'ELLIPSIS']:
                box.prop(prop, 'curve_steps')
            if prop.window_shape in ['ROUND']:
                box.prop(prop, 'radius')
            elif prop.window_shape == 'ELLIPSIS':
                box.prop(prop, 'elipsis_b')
            elif prop.window_shape == 'QUADRI':
                box.prop(prop, 'angle_y')

        row = layout.row(align=True)
        if prop.display_detail:
            row.prop(prop, "display_detail", icon="TRIA_DOWN", icon_only=True, text="Components", emboss=False)
        else:
            row.prop(prop, "display_detail", icon="TRIA_RIGHT", icon_only=True, text="Components", emboss=False)

        if prop.display_detail:
            box = layout.box()
            box.prop(prop, 'enable_glass')
            box = layout.box()
            box.label("Frame")
            box.prop(prop, 'frame_x')
            box.prop(prop, 'frame_y')
            if prop.window_shape != 'CIRCLE':
                box = layout.box()
                row = box.row(align=True)
                row.prop(prop, 'handle_enable')
                if prop.handle_enable:
                    box.prop(prop, 'handle_altitude')
            box = layout.box()
            row = box.row(align=True)
            row.prop(prop, 'out_frame')
            if prop.out_frame:
                box.prop(prop, 'out_frame_x')
                box.prop(prop, 'out_frame_y2')
                box.prop(prop, 'out_frame_y')
                box.prop(prop, 'out_frame_offset')
            if prop.window_shape != 'CIRCLE':
                box = layout.box()
                row = box.row(align=True)
                row.prop(prop, 'out_tablet_enable')
                if prop.out_tablet_enable:
                    box.prop(prop, 'out_tablet_x')
                    box.prop(prop, 'out_tablet_y')
                    box.prop(prop, 'out_tablet_z')
                box = layout.box()
                row = box.row(align=True)
                row.prop(prop, 'in_tablet_enable')
                if prop.in_tablet_enable:
                    box.prop(prop, 'in_tablet_x')
                    box.prop(prop, 'in_tablet_y')
                    box.prop(prop, 'in_tablet_z')
                box = layout.box()
                row = box.row(align=True)
                row.prop(prop, 'blind_enable')
                if prop.blind_enable:
                    box.prop(prop, 'blind_open')
                    box.prop(prop, 'blind_y')
                    box.prop(prop, 'blind_z')
        if prop.window_shape != 'CIRCLE':
            row = layout.row()
            if prop.display_panels:
                row.prop(prop, "display_panels", icon="TRIA_DOWN", icon_only=True, text="Rows", emboss=False)
            else:
                row.prop(prop, "display_panels", icon="TRIA_RIGHT", icon_only=True, text="Rows", emboss=False)

            if prop.display_panels:
                if prop.window_type != 'RAIL':
                    row = layout.row()
                    row.prop(prop, 'n_rows')
                    last_row = prop.n_rows - 1
                    for i, row in enumerate(prop.rows):
                        box = layout.box()
                        box.label(text="Row " + str(i + 1))
                        row.draw(box, context, i == last_row)
                else:
                    box = layout.box()
                    row = prop.rows[0]
                    row.draw(box, context, True)

        row = layout.row(align=True)
        if prop.display_materials:
            row.prop(prop, "display_materials", icon="TRIA_DOWN", icon_only=True, text="Materials", emboss=False)
        else:
            row.prop(prop, "display_materials", icon="TRIA_RIGHT", icon_only=True, text="Materials", emboss=False)
        if prop.display_materials:
            box = layout.box()
            box.label("Hole")
            box.prop(prop, 'hole_inside_mat')
            box.prop(prop, 'hole_outside_mat')

        layout.prop(prop, 'portal', icon="LAMP_AREA")


class ARCHIPACK_PT_window_panel(Panel):
    bl_idname = "ARCHIPACK_PT_window_panel"
    bl_label = "Window panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'ArchiPack'

    @classmethod
    def poll(cls, context):
        return archipack_window_panel.filter(context.active_object)

    def draw(self, context):
        layout = self.layout
        layout.operator("archipack.select_parent")


# ------------------------------------------------------------------
# Define operator class to create object
# ------------------------------------------------------------------


class ARCHIPACK_OT_window(ArchipackCreateTool, Operator):
    bl_idname = "archipack.window"
    bl_label = "Window"
    bl_description = "Window"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}
    x = FloatProperty(
        name='width',
        min=0.1, max=10000,
        default=2.0, precision=2,
        description='Width'
        )
    y = FloatProperty(
        name='depth',
        min=0.1, max=10000,
        default=0.20, precision=2,
        description='Depth'
        )
    z = FloatProperty(
        name='height',
        min=0.1, max=10000,
        default=1.2, precision=2,
        description='height'
        )
    altitude = FloatProperty(
        name='altitude',
        min=0.0, max=10000,
        default=1.0, precision=2,
        description='altitude'
        )
    mode = EnumProperty(
        items=(
        ('CREATE', 'Create', '', 0),
        ('DELETE', 'Delete', '', 1),
        ('REFRESH', 'Refresh', '', 2),
        ('UNIQUE', 'Make unique', '', 3),
        ),
        default='CREATE'
        )
    # auto_manipulate = BoolProperty(default=True)

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.label("Use Properties panel (N) to define parms", icon='INFO')

    def create(self, context):
        m = bpy.data.meshes.new("Window")
        o = bpy.data.objects.new("Window", m)
        d = m.archipack_window.add()
        d.x = self.x
        d.y = self.y
        d.z = self.z
        d.altitude = self.altitude
        context.scene.objects.link(o)
        o.select = True
        context.scene.objects.active = o
        self.add_material(o)
        self.load_preset(d)
        # select frame
        o.select = True
        context.scene.objects.active = o
        return o

    def delete(self, context):
        o = context.active_object
        if archipack_window.filter(o):
            bpy.ops.archipack.disable_manipulate()
            for child in o.children:
                if child.type == 'LAMP':
                    d = child.data
                    context.scene.objects.unlink(child)
                    bpy.data.objects.remove(child)
                    bpy.data.lamps.remove(d)
                elif 'archipack_hole' in child:
                    context.scene.objects.unlink(child)
                    bpy.data.objects.remove(child, do_unlink=True)
                elif child.data is not None and 'archipack_window_panel' in child.data:
                    for handle in child.children:
                        if 'archipack_handle' in handle:
                            context.scene.objects.unlink(handle)
                            bpy.data.objects.remove(handle, do_unlink=True)
                    context.scene.objects.unlink(child)
                    bpy.data.objects.remove(child, do_unlink=True)
            context.scene.objects.unlink(o)
            bpy.data.objects.remove(o, do_unlink=True)

    def update(self, context):
        o = context.active_object
        d = archipack_window.datablock(o)
        if d is not None:
            d.update(context)
            bpy.ops.object.select_linked(type='OBDATA')
            for linked in context.selected_objects:
                if linked != o:
                    archipack_window.datablock(linked).update(context)

        bpy.ops.object.select_all(action="DESELECT")
        o.select = True
        context.scene.objects.active = o

    def unique(self, context):
        act = context.active_object
        sel = [o for o in context.selected_objects]
        bpy.ops.object.select_all(action="DESELECT")
        for o in sel:
            if archipack_window.filter(o):
                o.select = True
                for child in o.children:
                    if 'archipack_hole' in child or (
                            child.data is not None and
                            'archipack_window_panel' in child.data):
                        child.hide_select = False
                        child.select = True
        if len(context.selected_objects) > 0:
            bpy.ops.object.make_single_user(type='SELECTED_OBJECTS', object=True,
                obdata=True, material=False, texture=False, animation=False)
            for child in context.selected_objects:
                if 'archipack_hole' in child:
                    child.hide_select = True
        bpy.ops.object.select_all(action="DESELECT")
        context.scene.objects.active = act
        for o in sel:
            o.select = True

    # -----------------------------------------------------
    # Execute
    # -----------------------------------------------------
    def execute(self, context):
        if context.mode == "OBJECT":
            if self.mode == 'CREATE':
                bpy.ops.object.select_all(action="DESELECT")
                o = self.create(context)
                o.location = bpy.context.scene.cursor_location
                o.select = True
                context.scene.objects.active = o
                self.manipulate()
            elif self.mode == 'DELETE':
                self.delete(context)
            elif self.mode == 'REFRESH':
                self.update(context)
            elif self.mode == 'UNIQUE':
                self.unique(context)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_window_draw(ArchpackDrawTool, Operator):
    bl_idname = "archipack.window_draw"
    bl_label = "Draw Windows"
    bl_description = "Draw Windows over walls"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}

    filepath = StringProperty(default="")
    feedback = None
    stack = []
    object_name = ""

    @classmethod
    def poll(cls, context):
        return True

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.label("Use Properties panel (N) to define parms", icon='INFO')

    def draw_callback(self, _self, context):
        self.feedback.draw(context)

    def add_object(self, context, event):

        o = context.active_object
        bpy.ops.object.select_all(action="DESELECT")

        if archipack_window.filter(o):

            o.select = True
            context.scene.objects.active = o

            if event.shift:
                bpy.ops.archipack.window(mode="UNIQUE")

            # instance subs
            new_w = o.copy()
            new_w.data = o.data
            context.scene.objects.link(new_w)
            for child in o.children:
                if "archipack_hole" not in child:
                    new_c = child.copy()
                    new_c.data = child.data
                    new_c.parent = new_w
                    context.scene.objects.link(new_c)
                    # dup handle if any
                    for c in child.children:
                        new_h = c.copy()
                        new_h.data = c.data
                        new_h.parent = new_c
                        context.scene.objects.link(new_h)

            o = new_w
            o.select = True
            context.scene.objects.active = o

        else:
            bpy.ops.archipack.window(auto_manipulate=False, filepath=self.filepath)
            o = context.active_object

        self.object_name = o.name

        bpy.ops.archipack.generate_hole('INVOKE_DEFAULT')
        o.select = True
        context.scene.objects.active = o

    def modal(self, context, event):

        context.area.tag_redraw()
        o = context.scene.objects.get(self.object_name)

        if o is None:
            return {'FINISHED'}

        d = archipack_window.datablock(o)
        hole = None
        if d is not None:
            hole = d.find_hole(o)

        # hide hole from raycast
        if hole is not None:
            o.hide = True
            hole.hide = True

        res, tM, wall, y = self.mouse_hover_wall(context, event)

        if hole is not None:
            o.hide = False
            hole.hide = False

        if res and d is not None:
            o.matrix_world = tM
            if d.y != wall.data.archipack_wall2[0].width:
                d.y = wall.data.archipack_wall2[0].width

        if event.value == 'PRESS':

            if event.type in {'C'}:
                bpy.ops.archipack.window(mode='DELETE')
                self.feedback.disable()
                bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
                bpy.ops.archipack.window_preset_menu('INVOKE_DEFAULT', preset_operator="archipack.window_draw")
                return {'FINISHED'}

            if event.type in {'LEFTMOUSE', 'RET', 'NUMPAD_ENTER', 'SPACE'}:
                if wall is not None:
                    context.scene.objects.active = wall
                    wall.select = True
                    if bpy.ops.archipack.single_boolean.poll():
                        bpy.ops.archipack.single_boolean()
                    wall.select = False
                    # o must be a window here
                    if d is not None:
                        context.scene.objects.active = o
                        self.stack.append(o)
                        self.add_object(context, event)
                        context.active_object.matrix_world = tM
                    return {'RUNNING_MODAL'}
            # prevent selection of other object
            if event.type in {'RIGHTMOUSE'}:
                return {'RUNNING_MODAL'}

        if self.keymap.check(event, self.keymap.undo) or (
                event.type in {'BACK_SPACE'} and event.value == 'RELEASE'
                ):
            if len(self.stack) > 0:
                last = self.stack.pop()
                context.scene.objects.active = last
                bpy.ops.archipack.window(mode="DELETE")
                context.scene.objects.active = o
            return {'RUNNING_MODAL'}

        if event.value == 'RELEASE':

            if event.type in {'ESC', 'RIGHTMOUSE'}:
                bpy.ops.archipack.window(mode='DELETE')
                self.feedback.disable()
                bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
                return {'FINISHED'}

        return {'PASS_THROUGH'}

    def invoke(self, context, event):

        if context.mode == "OBJECT":
            o = None
            self.stack = []
            self.keymap = Keymaps(context)
            # exit manipulate_mode if any
            bpy.ops.archipack.disable_manipulate()
            # invoke with shift pressed will use current object as basis for linked copy
            if self.filepath == '' and archipack_window.filter(context.active_object):
                o = context.active_object
            context.scene.objects.active = None
            bpy.ops.object.select_all(action="DESELECT")
            if o is not None:
                o.select = True
                context.scene.objects.active = o
            self.add_object(context, event)
            self.feedback = FeedbackPanel()
            self.feedback.instructions(context, "Draw a window", "Click & Drag over a wall", [
                ('LEFTCLICK, RET, SPACE, ENTER', 'Create a window'),
                ('BACKSPACE, CTRL+Z', 'undo last'),
                ('C', 'Choose another window'),
                ('SHIFT', 'Make independant copy'),
                ('RIGHTCLICK or ESC', 'exit')
                ])
            self.feedback.enable()
            args = (self, context)

            self._handle = bpy.types.SpaceView3D.draw_handler_add(self.draw_callback, args, 'WINDOW', 'POST_PIXEL')
            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}


class ARCHIPACK_OT_window_portals(Operator):
    bl_idname = "archipack.window_portals"
    bl_label = "Portals"
    bl_description = "Create portal for each window"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        return True

    def invoke(self, context, event):
        for o in context.scene.objects:
            d = archipack_window.datablock(o)
            if d is not None:
                d.update_portal(context)

        return {'FINISHED'}


# ------------------------------------------------------------------
# Define operator class to create object
# ------------------------------------------------------------------


class ARCHIPACK_OT_window_panel(Operator):
    bl_idname = "archipack.window_panel"
    bl_label = "Window panel"
    bl_description = "Window panel"
    bl_category = 'Archipack'
    bl_options = {'REGISTER', 'UNDO'}
    center = FloatVectorProperty(
            subtype='XYZ'
            )
    origin = FloatVectorProperty(
            subtype='XYZ'
            )
    size = FloatVectorProperty(
            subtype='XYZ'
            )
    radius = FloatVectorProperty(
            subtype='XYZ'
            )
    angle_y = FloatProperty(
            name='angle',
            unit='ROTATION',
            subtype='ANGLE',
            min=-1.5, max=1.5,
            default=0, precision=2,
            description='angle'
            )
    frame_y = FloatProperty(
            name='Depth',
            min=0, max=100,
            default=0.06, precision=2,
            description='frame depth'
            )
    frame_x = FloatProperty(
            name='Width',
            min=0, max=100,
            default=0.06, precision=2,
            description='frame width'
            )
    curve_steps = IntProperty(
            name="curve steps",
            min=1,
            max=128,
            default=16
            )
    shape = EnumProperty(
            name='Shape',
            items=(
                ('RECTANGLE', 'Rectangle', '', 0),
                ('ROUND', 'Top Round', '', 1),
                ('ELLIPSIS', 'Top Elliptic', '', 2),
                ('QUADRI', 'Top oblique', '', 3),
                ('CIRCLE', 'Full circle', '', 4)
                ),
            default='RECTANGLE'
            )
    pivot = FloatProperty(
            name='pivot',
            min=-1, max=1,
            default=-1, precision=2,
            description='pivot'
            )
    side_material = IntProperty(
            name="side material",
            min=0,
            max=2,
            default=0
            )
    handle = EnumProperty(
            name='Handle',
            items=(
                ('NONE', 'No handle', '', 0),
                ('INSIDE', 'Inside', '', 1),
                ('BOTH', 'Inside and outside', '', 2)
                ),
            default='NONE'
            )
    handle_model = IntProperty(
            name="handle model",
            default=1,
            min=1,
            max=2
            )
    handle_altitude = FloatProperty(
            name='handle altitude',
            min=0, max=1000,
            default=0.2, precision=2,
            description='handle altitude'
            )
    fixed = BoolProperty(
            name="Fixed",
            default=False
            )
    material = StringProperty(
            name="material",
            default=""
            )
    enable_glass = BoolProperty(
            name="Enable glass",
            default=True
            )

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.label("Use Properties panel (N) to define parms", icon='INFO')

    def create(self, context):
        m = bpy.data.meshes.new("Window Panel")
        o = bpy.data.objects.new("Window Panel", m)
        d = m.archipack_window_panel.add()
        d.center = self.center
        d.origin = self.origin
        d.size = self.size
        d.radius = self.radius
        d.frame_y = self.frame_y
        d.frame_x = self.frame_x
        d.curve_steps = self.curve_steps
        d.shape = self.shape
        d.fixed = self.fixed
        d.pivot = self.pivot
        d.angle_y = self.angle_y
        d.side_material = self.side_material
        d.handle = self.handle
        d.handle_model = self.handle_model
        d.handle_altitude = self.handle_altitude
        d.enable_glass = self.enable_glass
        context.scene.objects.link(o)
        o.select = True
        context.scene.objects.active = o
        m = o.archipack_material.add()
        m.category = "window"
        m.material = self.material
        o.lock_location[0] = True
        o.lock_location[1] = True
        o.lock_location[2] = True
        o.lock_rotation[1] = True
        o.lock_scale[0] = True
        o.lock_scale[1] = True
        o.lock_scale[2] = True
        d.update(context)
        return o

    def execute(self, context):
        if context.mode == "OBJECT":
            o = self.create(context)
            o.select = True
            context.scene.objects.active = o
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Archipack: Option only valid in Object mode")
            return {'CANCELLED'}

# ------------------------------------------------------------------
# Define operator class to manipulate object
# ------------------------------------------------------------------


class ARCHIPACK_OT_window_manipulate(Operator):
    bl_idname = "archipack.window_manipulate"
    bl_label = "Manipulate"
    bl_description = "Manipulate"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        return archipack_window.filter(context.active_object)

    def invoke(self, context, event):
        d = archipack_window.datablock(context.active_object)
        d.manipulable_invoke(context)
        return {'FINISHED'}

# ------------------------------------------------------------------
# Define operator class to load / save presets
# ------------------------------------------------------------------


class ARCHIPACK_OT_window_preset_menu(PresetMenuOperator, Operator):
    bl_description = "Show Window Presets"
    bl_idname = "archipack.window_preset_menu"
    bl_label = "Window Presets"
    preset_subdir = "archipack_window"


class ARCHIPACK_OT_window_preset(ArchipackPreset, Operator):
    """Add a Window Preset"""
    bl_idname = "archipack.window_preset"
    bl_label = "Add Window Preset"
    preset_menu = "ARCHIPACK_OT_window_preset_menu"

    @property
    def blacklist(self):
        # 'x', 'y', 'z', 'altitude', 'window_shape'
        return ['manipulators']


def register():
    bpy.utils.register_class(archipack_window_panelrow)
    bpy.utils.register_class(archipack_window_panel)
    Mesh.archipack_window_panel = CollectionProperty(type=archipack_window_panel)
    bpy.utils.register_class(ARCHIPACK_PT_window_panel)
    bpy.utils.register_class(ARCHIPACK_OT_window_panel)
    bpy.utils.register_class(archipack_window)
    Mesh.archipack_window = CollectionProperty(type=archipack_window)
    bpy.utils.register_class(ARCHIPACK_OT_window_preset_menu)
    bpy.utils.register_class(ARCHIPACK_PT_window)
    bpy.utils.register_class(ARCHIPACK_OT_window)
    bpy.utils.register_class(ARCHIPACK_OT_window_preset)
    bpy.utils.register_class(ARCHIPACK_OT_window_draw)
    bpy.utils.register_class(ARCHIPACK_OT_window_manipulate)
    bpy.utils.register_class(ARCHIPACK_OT_window_portals)


def unregister():
    bpy.utils.unregister_class(archipack_window_panelrow)
    bpy.utils.unregister_class(archipack_window_panel)
    bpy.utils.unregister_class(ARCHIPACK_PT_window_panel)
    del Mesh.archipack_window_panel
    bpy.utils.unregister_class(ARCHIPACK_OT_window_panel)
    bpy.utils.unregister_class(archipack_window)
    del Mesh.archipack_window
    bpy.utils.unregister_class(ARCHIPACK_OT_window_preset_menu)
    bpy.utils.unregister_class(ARCHIPACK_PT_window)
    bpy.utils.unregister_class(ARCHIPACK_OT_window)
    bpy.utils.unregister_class(ARCHIPACK_OT_window_preset)
    bpy.utils.unregister_class(ARCHIPACK_OT_window_draw)
    bpy.utils.unregister_class(ARCHIPACK_OT_window_manipulate)
    bpy.utils.unregister_class(ARCHIPACK_OT_window_portals)
