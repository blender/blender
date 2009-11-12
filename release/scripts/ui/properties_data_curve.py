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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy

narrowui = 180

class DataButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    def poll(self, context):
        return (context.object and context.object.type in ('CURVE', 'SURFACE') and context.curve)


class DataButtonsPanelCurve(DataButtonsPanel):
    '''Same as above but for curves only'''

    def poll(self, context):
        return (context.object and context.object.type == 'CURVE' and context.curve)


class DataButtonsPanelActive(DataButtonsPanel):
    '''Same as above but for curves only'''

    def poll(self, context):
        curve = context.curve
        return (curve and curve.active_spline)


class DATA_PT_context_curve(DataButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        space = context.space_data
        col2 = context.region.width > narrowui

        
        if col2:
            split = layout.split(percentage=0.65)

            if ob:
                split.template_ID(ob, "data")
                split.itemS()
            elif curve:
                split.template_ID(space, "pin_id")
                split.itemS()
        else:
            layout.template_ID(ob, "data")

class DATA_PT_shape_curve(DataButtonsPanel):
    bl_label = "Shape"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        space = context.space_data
        col2 = context.region.width > narrowui
        is_surf = (ob.type == 'SURFACE')

        if not is_surf:
            row = layout.row()
            row.itemR(curve, "dimensions", expand=True)

        split = layout.split()

        col = split.column()

        if not is_surf:
            sub = col.column()
            sub.active = (curve.dimensions == '2D')
            sub.itemL(text="Caps:")
            row = sub.row()
            row.itemR(curve, "front")
            row.itemR(curve, "back")

        col.itemL(text="Textures:")
#		col.itemR(curve, "uv_orco")
        col.itemR(curve, "auto_texspace")

        if col2:
            col = split.column()
        col.itemL(text="Resolution:")
        sub = col.column(align=True)
        sub.itemR(curve, "resolution_u", text="Preview U")
        sub.itemR(curve, "render_resolution_u", text="Render U")

        if is_surf:
            sub = col.column(align=True)
            sub.itemR(curve, "resolution_v", text="Preview V")
            sub.itemR(curve, "render_resolution_v", text="Render V")

        # XXX - put somewhere nicer.
        split = layout.split()

        col = split.column()
        col.itemR(curve, "twist_mode", text="Twist")
        
        if col2:
            col = split.column()
        col.itemR(curve, "twist_smooth") # XXX - may not be kept


class DATA_PT_geometry_curve(DataButtonsPanel):
    bl_label = "Geometry"

    def draw(self, context):
        layout = self.layout

        curve = context.curve
        col2 = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.itemL(text="Modification:")
        col.itemR(curve, "width")
        col.itemR(curve, "extrude")
        col.itemL(text="Taper Object:")
        col.itemR(curve, "taper_object", text="")

        if col2:
            col = split.column()
        col.itemL(text="Bevel:")
        col.itemR(curve, "bevel_depth", text="Depth")
        col.itemR(curve, "bevel_resolution", text="Resolution")
        col.itemL(text="Bevel Object:")
        col.itemR(curve, "bevel_object", text="")


class DATA_PT_pathanim(DataButtonsPanelCurve):
    bl_label = "Path Animation"

    def draw_header(self, context):
        curve = context.curve

        self.layout.itemR(curve, "use_path", text="")

    def draw(self, context):
        layout = self.layout

        curve = context.curve
        col2 = context.region.width > narrowui

        layout.active = curve.use_path

        split = layout.split()

        col = split.column()
        col.itemR(curve, "path_length", text="Frames")
        col.itemR(curve, "use_path_follow")

        if col2:
            col = split.column()
        col.itemR(curve, "use_stretch")
        col.itemR(curve, "use_radius")
        col.itemR(curve, "use_time_offset", text="Offset Children")


class DATA_PT_active_spline(DataButtonsPanelActive):
    bl_label = "Active Spline"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        curve = context.curve
        act_spline = curve.active_spline
        is_surf = (ob.type == 'SURFACE')
        is_poly = (act_spline.type == 'POLY')

        split = layout.split()

        if is_poly:
            # These settings are below but its easier to have
            # poly's set aside since they use so few settings
            col = split.column()
            col.itemL(text="Cyclic:")
            col.itemR(act_spline, "smooth")
            col = split.column()
            col.itemR(act_spline, "cyclic_u", text="U")

        else:
            col = split.column()
            col.itemL(text="Cyclic:")
            if act_spline.type == 'NURBS':
                col.itemL(text="Bezier:")
                col.itemL(text="Endpoint:")
                col.itemL(text="Order:")

            col.itemL(text="Resolution:")

            col = split.column()
            col.itemR(act_spline, "cyclic_u", text="U")

            if act_spline.type == 'NURBS':
                sub = col.column()
                # sub.active = (not act_spline.cyclic_u)
                sub.itemR(act_spline, "bezier_u", text="U")
                sub.itemR(act_spline, "endpoint_u", text="U")

                sub = col.column()
                sub.itemR(act_spline, "order_u", text="U")
            col.itemR(act_spline, "resolution_u", text="U")

            if is_surf:
                col = split.column()
                col.itemR(act_spline, "cyclic_v", text="V")

                # its a surface, assume its a nurb.
                sub = col.column()
                sub.active = (not act_spline.cyclic_v)
                sub.itemR(act_spline, "bezier_v", text="V")
                sub.itemR(act_spline, "endpoint_v", text="V")
                sub = col.column()
                sub.itemR(act_spline, "order_v", text="V")
                sub.itemR(act_spline, "resolution_v", text="V")


            if not is_surf:
                split = layout.split()
                col = split.column()
                col.active = (curve.dimensions == '3D')

                col.itemL(text="Interpolation:")
                col.itemR(act_spline, "tilt_interpolation", text="Tilt")
                col.itemR(act_spline, "radius_interpolation", text="Radius")

            split = layout.split()
            col = split.column()
            col.itemR(act_spline, "smooth")

bpy.types.register(DATA_PT_context_curve)
bpy.types.register(DATA_PT_shape_curve)
bpy.types.register(DATA_PT_geometry_curve)
bpy.types.register(DATA_PT_pathanim)
bpy.types.register(DATA_PT_active_spline)
