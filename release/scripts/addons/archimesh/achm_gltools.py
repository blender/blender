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
# support routines for OpenGL
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
# noinspection PyUnresolvedReferences
import bgl
# noinspection PyUnresolvedReferences
import blf
from math import fabs, sqrt, sin, cos
# noinspection PyUnresolvedReferences
from mathutils import Vector
# noinspection PyUnresolvedReferences
from bpy_extras import view3d_utils
from .achm_room_maker import get_wall_points


# -------------------------------------------------------------
# Handle all draw routines (OpenGL main entry point)
#
# -------------------------------------------------------------
def draw_main(context):
    region = context.region
    rv3d = context.space_data.region_3d
    scene = context.scene

    rgb = scene.archimesh_text_color
    rgbw = scene.archimesh_walltext_color
    fsize = scene.archimesh_font_size
    wfsize = scene.archimesh_wfont_size
    space = scene.archimesh_hint_space
    measure = scene.archimesh_gl_measure
    dspname = scene.archimesh_gl_name
    # Get visible layers
    layers = []
    for x in range(0, 20):
        if context.scene.layers[x] is True:
            layers.extend([x])

    bgl.glEnable(bgl.GL_BLEND)
    # Display selected or all
    if scene.archimesh_gl_ghost is False:
        objlist = context.selected_objects
    else:
        objlist = context.scene.objects
    # ---------------------------------------
    # Generate all OpenGL calls
    # ---------------------------------------
    for myobj in objlist:
        if myobj.hide is False:
            # verify visible layer
            for x in range(0, 20):
                if myobj.layers[x] is True:
                    if x in layers:
                        # -----------------------------------------------------
                        # Rooms
                        # -----------------------------------------------------
                        if 'RoomGenerator' in myobj:
                            op = myobj.RoomGenerator[0]
                            draw_room_data(myobj, op, region, rv3d, rgb, rgbw, fsize, wfsize, space, measure, dspname)

                        # -----------------------------------------------------
                        # Doors
                        # -----------------------------------------------------
                        if 'DoorObjectGenerator' in myobj:
                            op = myobj.DoorObjectGenerator[0]
                            draw_door_data(myobj, op, region, rv3d, rgb, fsize, space, measure)

                        # -----------------------------------------------------
                        # Window (Rail)
                        # -----------------------------------------------------
                        if 'WindowObjectGenerator' in myobj:
                            op = myobj.WindowObjectGenerator[0]
                            draw_window_rail_data(myobj, op, region, rv3d, rgb, fsize, space, measure)

                        # -----------------------------------------------------
                        # Window (Panel)
                        # -----------------------------------------------------
                        if 'WindowPanelGenerator' in myobj:
                            op = myobj.WindowPanelGenerator[0]
                            draw_window_panel_data(myobj, op, region, rv3d, rgb, fsize, space, measure)
                        break   # avoid unnecessary loops
    # -----------------------
    # restore opengl defaults
    # -----------------------
    bgl.glLineWidth(1)
    bgl.glDisable(bgl.GL_BLEND)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)


# -------------------------------------------------------------
# Create OpenGL text
#
# right: Align to right
# -------------------------------------------------------------
def draw_text(x_pos, y_pos, display_text, rgb, fsize, right=False):
    gap = 12
    font_id = 0
    blf.size(font_id, fsize, 72)

    text_width, text_height = blf.dimensions(font_id, display_text)
    if right is True:
        newx = x_pos - text_width - gap
    else:
        newx = x_pos
    blf.position(font_id, newx, y_pos, 0)
    bgl.glColor4f(rgb[0], rgb[1], rgb[2], rgb[3])
    blf.draw(font_id, display_text)
    return


# -------------------------------------------------------------
# Draw an OpenGL line
#
# -------------------------------------------------------------
def draw_line(v1, v2):
    # noinspection PyBroadException
    try:
        if v1 is not None and v2 is not None:
            bgl.glBegin(bgl.GL_LINES)
            bgl.glVertex2f(*v1)
            bgl.glVertex2f(*v2)
            bgl.glEnd()
    except:
        pass


# -------------------------------------------------------------
# Draw room information
#
# rgb: Color
# fsize: Font size
# -------------------------------------------------------------
def draw_room_data(myobj, op, region, rv3d, rgb, rgbw, fsize, wfsize, space, measure, dspname):

    verts, activefaces, activenormals = get_wall_points(myobj)

    # --------------------------
    # Get line points and draw
    # --------------------------
    for face in activefaces:
        a1 = None
        b1 = None
        a2 = None
        b2 = None
        # Bottom
        for e in face:
            if verts[e][2] == 0:
                if a1 is None:
                    a1 = e
                else:
                    b1 = e
        # Top
        for e in face:
            if verts[e][2] != 0:
                if round(verts[a1][0], 5) == round(verts[e][0], 5) and round(verts[a1][1], 5) == round(verts[e][1], 5):
                    a2 = e
                else:
                    b2 = e
        # Points
        # a1_p = get_point((verts[a1][0], verts[a1][1], verts[a1][2]), myobj)  # bottom
        a2_p = get_point((verts[a2][0], verts[a2][1], verts[a2][2] + space), myobj)  # top
        a2_s1 = get_point((verts[a2][0], verts[a2][1], verts[a2][2]), myobj)  # vertical line
        a2_s2 = get_point((verts[a2][0], verts[a2][1], verts[a2][2] + space + fsize / 200), myobj)  # vertical line

        # b1_p = get_point((verts[b1][0], verts[b1][1], verts[b1][2]), myobj)  # bottom
        b2_p = get_point((verts[b2][0], verts[b2][1], verts[b2][2] + space), myobj)  # top
        b2_s1 = get_point((verts[b2][0], verts[b2][1], verts[b2][2]), myobj)  # vertical line
        b2_s2 = get_point((verts[b2][0], verts[b2][1], verts[b2][2] + space + fsize / 200), myobj)  # vertical line

        # converting to screen coordinates
        screen_point_a = view3d_utils.location_3d_to_region_2d(region, rv3d, a2_p)
        screen_point_b = view3d_utils.location_3d_to_region_2d(region, rv3d, b2_p)
        screen_point_a1 = view3d_utils.location_3d_to_region_2d(region, rv3d, a2_s1)
        screen_point_b1 = view3d_utils.location_3d_to_region_2d(region, rv3d, b2_s1)
        screen_point_a2 = view3d_utils.location_3d_to_region_2d(region, rv3d, a2_s2)
        screen_point_b2 = view3d_utils.location_3d_to_region_2d(region, rv3d, b2_s2)

        # colour + line setup
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glLineWidth(1)
        bgl.glColor4f(rgb[0], rgb[1], rgb[2], rgb[3])

        # --------------------------------
        # Measures
        # --------------------------------
        if measure is True:
            # Draw text
            dist = distance(a2_p, b2_p)
            txtpoint3d = interpolate3d(a2_p, b2_p, fabs(dist / 2))
            # add a gap
            gap3d = (txtpoint3d[0], txtpoint3d[1], txtpoint3d[2] + 0.05)

            txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)

            draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

            # Draw horizontal line
            draw_line(screen_point_a, screen_point_b)
            # Draw vertical line 1 (upper vertical)
            draw_line(screen_point_a1, screen_point_a2)
            # Draw vertical line 2 (upper vertical)
            draw_line(screen_point_b1, screen_point_b2)
            # Draw vertical line 1
            draw_line(screen_point_a, screen_point_a1)
            # Draw vertical line 2
            draw_line(screen_point_b, screen_point_b1)

        # --------------------------------
        # Wall Number
        # --------------------------------
        if dspname is True:
            for i in range(0, op.wall_num):
                ap = get_point((op.walls[i].glpoint_a[0], op.walls[i].glpoint_a[1], op.walls[i].glpoint_a[2]), myobj)
                bp = get_point((op.walls[i].glpoint_b[0], op.walls[i].glpoint_b[1], op.walls[i].glpoint_b[2]), myobj)

                dist = distance(ap, bp)
                txtpoint3d = interpolate3d(ap, bp, fabs(dist / 2))
                # add a gap
                gap3d = (txtpoint3d[0], txtpoint3d[1], txtpoint3d[2])  # + op.room_height / 2)
                txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
                txt = "Wall: "
                if op.walls[i].a is True:
                    txt = "Advance: "
                if op.walls[i].curved is True:
                    txt = "Curved: "

                draw_text(txtpoint2d[0], txtpoint2d[1], txt + str(i + 1), rgbw, wfsize)

    return


# -------------------------------------------------------------
# Draw door information
#
# rgb: Color
# fsize: Font size
# -------------------------------------------------------------
def draw_door_data(myobj, op, region, rv3d, rgb, fsize, space, measure):

    # Points
    a_p1 = get_point(op.glpoint_a, myobj)
    a_p2 = get_point((op.glpoint_a[0] - space, op.glpoint_a[1], op.glpoint_a[2]), myobj)
    a_p3 = get_point((op.glpoint_a[0] - space - fsize / 200, op.glpoint_a[1], op.glpoint_a[2]), myobj)

    t_p1 = get_point(op.glpoint_b, myobj)
    t_p2 = get_point((op.glpoint_b[0] - space, op.glpoint_b[1], op.glpoint_b[2]), myobj)
    t_p3 = get_point((op.glpoint_b[0] - space - fsize / 200, op.glpoint_b[1], op.glpoint_b[2]), myobj)

    b_p1 = get_point(op.glpoint_b, myobj)
    b_p2 = get_point((op.glpoint_b[0], op.glpoint_b[1], op.glpoint_b[2] + space), myobj)
    b_p3 = get_point((op.glpoint_b[0], op.glpoint_b[1], op.glpoint_b[2] + space + fsize / 200), myobj)

    c_p1 = get_point(op.glpoint_c, myobj)
    c_p2 = get_point((op.glpoint_c[0], op.glpoint_c[1], op.glpoint_c[2] + space), myobj)
    c_p3 = get_point((op.glpoint_c[0], op.glpoint_c[1], op.glpoint_c[2] + space + fsize / 200), myobj)

    d_p1 = get_point(op.glpoint_d, myobj)
    d_p2 = get_point((op.glpoint_d[0], op.glpoint_d[1], op.glpoint_b[2] + space + fsize / 300), myobj)
    d_p3 = get_point((op.glpoint_d[0], op.glpoint_d[1], op.glpoint_d[2] - fsize / 250), myobj)

    e_p1 = get_point(op.glpoint_e, myobj)
    e_p2 = get_point((op.glpoint_e[0], op.glpoint_e[1], op.glpoint_b[2] + space + fsize / 300), myobj)
    e_p3 = get_point((op.glpoint_e[0], op.glpoint_e[1], op.glpoint_e[2] - fsize / 250), myobj)

    # converting to screen coordinates
    screen_point_ap1 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p1)
    screen_point_bp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p1)
    screen_point_cp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p1)
    screen_point_tp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p1)

    screen_point_ap2 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p2)
    screen_point_bp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p2)
    screen_point_cp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p2)
    screen_point_tp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p2)

    screen_point_ap3 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p3)
    screen_point_bp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p3)
    screen_point_cp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p3)
    screen_point_tp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p3)

    screen_point_dp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, d_p1)
    screen_point_dp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, d_p2)
    screen_point_dp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, d_p3)

    screen_point_ep1 = view3d_utils.location_3d_to_region_2d(region, rv3d, e_p1)
    screen_point_ep2 = view3d_utils.location_3d_to_region_2d(region, rv3d, e_p2)
    screen_point_ep3 = view3d_utils.location_3d_to_region_2d(region, rv3d, e_p3)

    # colour + line setup
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glLineWidth(1)
    bgl.glColor4f(rgb[0], rgb[1], rgb[2], rgb[3])

    # --------------------------------
    # Measures
    # --------------------------------
    if measure is True:
        # Vertical
        dist = distance(a_p1, t_p1)
        txtpoint3d = interpolate3d(a_p1, t_p1, fabs(dist / 2))
        gap3d = (a_p2[0], txtpoint3d[1], txtpoint3d[2])
        txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
        draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize, True)

        draw_line(screen_point_ap2, screen_point_tp2)
        draw_line(screen_point_ap3, screen_point_ap1)
        draw_line(screen_point_tp3, screen_point_tp1)

        # Horizontal
        dist = distance(b_p1, c_p1)
        txtpoint3d = interpolate3d(b_p1, c_p1, fabs(dist / 2))
        gap3d = (txtpoint3d[0], txtpoint3d[1], b_p2[2] + 0.02)
        txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
        draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

        draw_line(screen_point_bp2, screen_point_cp2)
        draw_line(screen_point_bp3, screen_point_bp1)
        draw_line(screen_point_cp3, screen_point_cp1)

        # Door size
        dist = distance(d_p1, e_p1)
        txtpoint3d = interpolate3d(d_p1, e_p1, fabs(dist / 2))
        gap3d = (txtpoint3d[0], txtpoint3d[1], txtpoint3d[2] + 0.02)
        txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
        draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

        draw_line(screen_point_dp1, screen_point_ep1)
        draw_line(screen_point_dp2, screen_point_dp3)
        draw_line(screen_point_ep2, screen_point_ep3)
    return


# -------------------------------------------------------------
# Draw window rail information
#
# rgb: Color
# fsize: Font size
# -------------------------------------------------------------
def draw_window_rail_data(myobj, op, region, rv3d, rgb, fsize, space, measure):

    # Points
    a_p1 = get_point(op.glpoint_a, myobj)
    a_p2 = get_point((op.glpoint_a[0] - space, op.glpoint_a[1], op.glpoint_a[2]), myobj)
    a_p3 = get_point((op.glpoint_a[0] - space - fsize / 200, op.glpoint_a[1], op.glpoint_a[2]), myobj)

    t_p1 = get_point(op.glpoint_b, myobj)
    t_p2 = get_point((op.glpoint_b[0] - space, op.glpoint_b[1], op.glpoint_b[2]), myobj)
    t_p3 = get_point((op.glpoint_b[0] - space - fsize / 200, op.glpoint_b[1], op.glpoint_b[2]), myobj)

    b_p1 = get_point(op.glpoint_b, myobj)
    b_p2 = get_point((op.glpoint_b[0], op.glpoint_b[1], op.glpoint_b[2] + space), myobj)
    b_p3 = get_point((op.glpoint_b[0], op.glpoint_b[1], op.glpoint_b[2] + space + fsize / 200), myobj)

    c_p1 = get_point(op.glpoint_c, myobj)
    c_p2 = get_point((op.glpoint_c[0], op.glpoint_c[1], op.glpoint_c[2] + space), myobj)
    c_p3 = get_point((op.glpoint_c[0], op.glpoint_c[1], op.glpoint_c[2] + space + fsize / 200), myobj)

    # converting to screen coordinates
    screen_point_ap1 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p1)
    screen_point_bp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p1)
    screen_point_cp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p1)
    screen_point_tp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p1)

    screen_point_ap2 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p2)
    screen_point_bp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p2)
    screen_point_cp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p2)
    screen_point_tp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p2)

    screen_point_ap3 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p3)
    screen_point_bp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p3)
    screen_point_cp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p3)
    screen_point_tp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p3)

    # colour + line setup
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glLineWidth(1)
    bgl.glColor4f(rgb[0], rgb[1], rgb[2], rgb[3])

    # --------------------------------
    # Measures
    # --------------------------------
    if measure is True:
        # Vertical
        dist = distance(a_p1, t_p1)
        txtpoint3d = interpolate3d(a_p1, t_p1, fabs(dist / 2))
        gap3d = (a_p2[0], txtpoint3d[1], txtpoint3d[2])
        txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
        draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize, True)

        draw_line(screen_point_ap2, screen_point_tp2)
        draw_line(screen_point_ap3, screen_point_ap1)
        draw_line(screen_point_tp3, screen_point_tp1)

        # Horizontal
        dist = distance(b_p1, c_p1)
        txtpoint3d = interpolate3d(b_p1, c_p1, fabs(dist / 2))
        gap3d = (txtpoint3d[0], txtpoint3d[1], b_p2[2] + 0.02)
        txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
        draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

        draw_line(screen_point_bp2, screen_point_cp2)
        draw_line(screen_point_bp3, screen_point_bp1)
        draw_line(screen_point_cp3, screen_point_cp1)

    return


# -------------------------------------------------------------
# Draw window panel information
#
# rgb: Color
# fsize: Font size
# -------------------------------------------------------------
def draw_window_panel_data(myobj, op, region, rv3d, rgb, fsize, space, measure):

    # Points
    a_p1 = get_point(op.glpoint_a, myobj)
    a_p2 = get_point((op.glpoint_a[0] - space, op.glpoint_a[1], op.glpoint_a[2]), myobj)
    a_p3 = get_point((op.glpoint_a[0] - space - fsize / 200, op.glpoint_a[1], op.glpoint_a[2]), myobj)

    f_p1 = get_point((op.glpoint_c[0], op.glpoint_c[1], op.glpoint_a[2]), myobj)
    f_p2 = get_point((op.glpoint_c[0] + space, op.glpoint_c[1], op.glpoint_a[2]), myobj)
    f_p3 = get_point((op.glpoint_c[0] + space + fsize / 200, op.glpoint_c[1], op.glpoint_a[2]), myobj)

    t_p1 = get_point(op.glpoint_b, myobj)
    t_p2 = get_point((op.glpoint_b[0] - space, op.glpoint_b[1], op.glpoint_b[2]), myobj)
    t_p3 = get_point((op.glpoint_b[0] - space - fsize / 200, op.glpoint_b[1], op.glpoint_b[2]), myobj)

    b_p1 = get_point(op.glpoint_b, myobj)
    b_p2 = get_point((op.glpoint_b[0], op.glpoint_b[1], op.glpoint_b[2] + space), myobj)
    b_p3 = get_point((op.glpoint_b[0], op.glpoint_b[1], op.glpoint_b[2] + space + fsize / 200), myobj)

    c_p1 = get_point(op.glpoint_c, myobj)
    c_p2 = get_point((op.glpoint_c[0], op.glpoint_c[1], op.glpoint_c[2] + space), myobj)
    c_p3 = get_point((op.glpoint_c[0], op.glpoint_c[1], op.glpoint_c[2] + space + fsize / 200), myobj)

    d_p1 = get_point(op.glpoint_c, myobj)
    d_p2 = get_point((op.glpoint_c[0] + space, op.glpoint_c[1], op.glpoint_c[2]), myobj)
    d_p3 = get_point((op.glpoint_c[0] + space + fsize / 200, op.glpoint_c[1], op.glpoint_c[2]), myobj)

    g_p2 = get_point((op.glpoint_d[0], op.glpoint_d[1], 0), myobj)
    g_p3 = get_point((op.glpoint_d[0], op.glpoint_d[1], op.glpoint_d[2]), myobj)
    g_p4 = get_point((op.glpoint_d[0], op.glpoint_d[1], op.glpoint_d[2] + space), myobj)
    g_p5 = get_point((op.glpoint_d[0], op.glpoint_d[1], op.glpoint_d[2] + space + fsize / 200), myobj)

    h_p1 = get_point((op.glpoint_a[0], op.glpoint_a[1], op.glpoint_a[2] - space), myobj)
    h_p2 = get_point((op.glpoint_a[0], op.glpoint_a[1], op.glpoint_a[2] - space - fsize / 200), myobj)

    h_p3 = get_point((op.glpoint_c[0], op.glpoint_a[1], op.glpoint_a[2]), myobj)
    h_p4 = get_point((op.glpoint_c[0], op.glpoint_a[1], op.glpoint_a[2] - space), myobj)
    h_p5 = get_point((op.glpoint_c[0], op.glpoint_a[1], op.glpoint_a[2] - space - fsize / 200), myobj)

    # converting to screen coordinates
    screen_point_ap1 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p1)
    screen_point_bp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p1)
    screen_point_cp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p1)
    screen_point_tp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p1)

    screen_point_ap2 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p2)
    screen_point_bp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p2)
    screen_point_cp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p2)
    screen_point_tp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p2)

    screen_point_ap3 = view3d_utils.location_3d_to_region_2d(region, rv3d, a_p3)
    screen_point_bp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, b_p3)
    screen_point_cp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, c_p3)
    screen_point_tp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, t_p3)

    screen_point_dp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, d_p1)
    screen_point_dp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, d_p2)
    screen_point_dp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, d_p3)

    screen_point_fp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, f_p1)
    screen_point_fp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, f_p2)
    screen_point_fp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, f_p3)

    screen_point_gp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, g_p2)
    screen_point_gp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, g_p3)
    screen_point_gp4 = view3d_utils.location_3d_to_region_2d(region, rv3d, g_p4)
    screen_point_gp5 = view3d_utils.location_3d_to_region_2d(region, rv3d, g_p5)
    # colour + line setup
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glLineWidth(1)
    bgl.glColor4f(rgb[0], rgb[1], rgb[2], rgb[3])

    # --------------------------------
    # Measures
    # --------------------------------
    if measure is True:
        # Vertical (right)
        dist = distance(a_p1, t_p1)
        txtpoint3d = interpolate3d(a_p1, t_p1, fabs(dist / 2))
        gap3d = (a_p2[0], txtpoint3d[1], txtpoint3d[2])
        txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
        draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize, True)

        draw_line(screen_point_ap2, screen_point_tp2)
        draw_line(screen_point_ap3, screen_point_ap1)
        draw_line(screen_point_tp3, screen_point_tp1)

        # Vertical (Left)
        dist = distance(f_p1, d_p1)
        txtpoint3d = interpolate3d(f_p1, d_p1, fabs(dist / 2))
        gap3d = (f_p2[0], txtpoint3d[1], txtpoint3d[2])
        txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
        draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

        draw_line(screen_point_fp2, screen_point_dp2)
        draw_line(screen_point_fp1, screen_point_fp3)
        draw_line(screen_point_dp1, screen_point_dp3)

        # Horizontal (not triangle nor arch)
        if op.UST != "4" and op.UST != "2":
            dist = distance(b_p1, c_p1)
            txtpoint3d = interpolate3d(b_p2, c_p2, fabs(dist / 2))
            gap3d = (txtpoint3d[0], txtpoint3d[1], txtpoint3d[2] + 0.05)
            txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
            draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

            draw_line(screen_point_bp2, screen_point_cp2)
            draw_line(screen_point_bp3, screen_point_bp1)
            draw_line(screen_point_cp3, screen_point_cp1)
        else:
            dist = distance(b_p1, g_p3)
            txtpoint3d = interpolate3d(b_p2, g_p4, fabs(dist / 2))
            gap3d = (txtpoint3d[0], txtpoint3d[1], txtpoint3d[2] + 0.05)
            txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
            draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize, True)

            dist = distance(g_p3, c_p1)
            txtpoint3d = interpolate3d(g_p4, c_p2, fabs(dist / 2))
            gap3d = (txtpoint3d[0], txtpoint3d[1], txtpoint3d[2] + 0.05)
            txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
            draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

            draw_line(screen_point_bp2, screen_point_gp4)
            draw_line(screen_point_gp4, screen_point_cp2)
            draw_line(screen_point_bp3, screen_point_bp1)
            draw_line(screen_point_cp3, screen_point_cp1)
            draw_line(screen_point_gp3, screen_point_gp5)

        # Only for Triangle or arch
        if op.UST == "2" or op.UST == "4":
            dist = distance(g_p2, g_p3)
            txtpoint3d = interpolate3d(g_p2, g_p3, fabs(dist / 2))
            gap3d = (txtpoint3d[0] + 0.05, txtpoint3d[1], txtpoint3d[2])
            txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
            draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

            draw_line(screen_point_gp2, screen_point_gp3)

        # Only for Triangle and Inclines or arch
        if op.UST == "3" or op.UST == "4" or op.UST == "2":
            screen_point_hp1 = view3d_utils.location_3d_to_region_2d(region, rv3d, h_p1)
            screen_point_hp2 = view3d_utils.location_3d_to_region_2d(region, rv3d, h_p2)
            screen_point_hp3 = view3d_utils.location_3d_to_region_2d(region, rv3d, h_p3)
            screen_point_hp4 = view3d_utils.location_3d_to_region_2d(region, rv3d, h_p4)
            screen_point_hp5 = view3d_utils.location_3d_to_region_2d(region, rv3d, h_p5)

            dist = distance(h_p1, h_p3)
            txtpoint3d = interpolate3d(h_p1, h_p3, fabs(dist / 2))
            gap3d = (txtpoint3d[0], txtpoint3d[1], txtpoint3d[2] - space - 0.05)
            txtpoint2d = view3d_utils.location_3d_to_region_2d(region, rv3d, gap3d)
            draw_text(txtpoint2d[0], txtpoint2d[1], "%6.2f" % dist, rgb, fsize)

            draw_line(screen_point_ap1, screen_point_hp2)
            draw_line(screen_point_hp3, screen_point_hp5)
            draw_line(screen_point_hp1, screen_point_hp4)

    return


# --------------------------------------------------------------------
# Distance between 2 points in 3D space
# v1: first point
# v2: second point
# return: distance
# --------------------------------------------------------------------
def distance(v1, v2):
    return sqrt((v2[0] - v1[0]) ** 2 + (v2[1] - v1[1]) ** 2 + (v2[2] - v1[2]) ** 2)


# --------------------------------------------------------------------
# Interpolate 2 points in 3D space
# v1: first point
# v2: second point
# d1: distance
# return: interpolate point
# --------------------------------------------------------------------
def interpolate3d(v1, v2, d1):
    # calculate vector
    v = (v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2])
    # calculate distance between points
    d0 = distance(v1, v2)
    # calculate interpolate factor (distance from origin / distance total)
    # if d1 > d0, the point is projected in 3D space
    if d0 > 0:
        x = d1 / d0
    else:
        x = d1

    final = (v1[0] + (v[0] * x), v1[1] + (v[1] * x), v1[2] + (v[2] * x))
    return final


# --------------------------------------------------------------------
# Get point rotated and relative to parent
# v1: point
# mainobject
# --------------------------------------------------------------------
def get_point(v1, mainobject):

    # Using World Matrix
    vt = Vector((v1[0], v1[1], v1[2], 1))
    m4 = mainobject.matrix_world
    vt2 = m4 * vt
    v2 = [vt2[0], vt2[1], vt2[2]]

    return v2


# --------------------------------------------------------------------
# rotate point EULER X
# v1: point
# rad: Angles of rotation in Radians
# --------------------------------------------------------------------
def rotate_x(v1, rot):
    v2 = [0, 0, 0]

    radx = rot[0]

    # X axis
    v2[0] = v1[0]
    v2[1] = v1[1] * cos(radx) - v1[2] * sin(radx)
    v2[2] = v1[1] * sin(radx) + v1[2] * cos(radx)

    return v2


# --------------------------------------------------------------------
# rotate point EULER Y
# v1: point
# rad: Angles of rotation in Radians
# --------------------------------------------------------------------
def rotate_y(v1, rot):
    v2 = [0, 0, 0]

    rady = rot[1]

    # Y axis
    v2[0] = v1[0] * cos(rady) + v1[2] * sin(rady)
    v2[1] = v1[1]
    v2[2] = v1[2] * cos(rady) - v1[0] * sin(rady)

    return v2


# --------------------------------------------------------------------
# rotate point EULER Z
# v1: point
# rad: Angles of rotation in Radians
# --------------------------------------------------------------------
def rotate_z(v1, rot):
    v2 = [0, 0, 0]

    radz = rot[2]

    # Z axis
    v2[0] = v1[0] * cos(radz) - v1[1] * sin(radz)
    v2[1] = v1[0] * sin(radz) + v1[1] * cos(radz)
    v2[2] = v1[2]

    return v2
