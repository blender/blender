# GPL "author": "Dominic Kröper, (dommetysk)"

import bpy
from math import (
        pi, sin,
        cos, tan,
        )
from bpy.types import Operator
from mathutils import (
        Vector,
        Euler,
        )
from bpy.props import (
        IntProperty,
        FloatProperty,
        BoolProperty,
        )


# mesh/object generating function, returns final object

def addBrilliant(context, s, table_w, crown_h, girdle_t, pavi_d, bezel_f,
                 pavi_f, culet, girdle_real, keep_lga, g_real_smooth):

    # # possible user inputs  ( output 100% = 2 blender units )
    # s                   # no. of girdle facets (steps)  default: 16
    # table_w             # table width                   default: 0.530
    # crown_h             # crown height                  default: 0.162
    # girdle_t            # girdle thickness              default: 0.017
    # pavi_d              # pavillion depth               default: 0.431
    # bezel_f             # bezel factor                  default: 0.250
    # pavi_f              # pavillion factor              default: 0.400
    # culet               # culet size                    default: 0.000
    # girdle_real         # type of girdle flat/real      default: True
    # g_real_smooth       # smooth or flat shading        default: False
    # keep_lga            # when culet > 0, keep lga      default: False

    # variables / shortcuts
    if s % 2:                   # prevent odd number of steps (messes up mesh)
        s = s - 1
    if not girdle_real:
        g_real_smooth = False
    ang = 2 * pi / s                        # angle step size
    Verts = []                              # collect all vertices
    Faces = []                              # collect all faces
    ca = cos(ang)
    ca2 = cos(ang / 2)
    sa4 = sin(ang / 4)
    ta4 = tan(ang / 4)
    ta8 = tan(ang / 8)

    def fa(*vs):                            # shortcut Faces.append
        v = []
        for u in vs:
            v.append(u)
        Faces.append(v)

    def va(vx, vz, iang, sang, n):          # shortcut Verts.append
        for i in range(n):
            v = Vector((vx, 0, vz))
            ai = sang + iang * i
            E_rot = Euler((0, 0, ai), 'XYZ')
            v.rotate(E_rot)
            Verts.append((v.x, v.y, v.z))

    # upper girdle angle
    uga = (1 - bezel_f) * crown_h * 2 / (ca2 -
           (table_w + (1 - table_w) * bezel_f) * ca2 / ca)

    # lower girdle angle
    if keep_lga:
        if pavi_f > 0 and pavi_f < 1:
            lga = (1 - pavi_f) * pavi_d * 2 / (ca2 - pavi_f * ca2 / ca)
        elif pavi_f == 1:
            lga = 0
        else:
            lga = 2 * pavi_d * ca
    else:
        lga = (1 - pavi_f) * pavi_d * 2 / (ca2 -
               (culet + (1 - culet) * pavi_f) * ca2 / ca)

    # append girdle vertices
    va(1, 0, ang, 0, s)
    va(1, 2 * girdle_t, ang, 0, s)

    # append real girdle vertices
    if girdle_real:
        dnu = uga * (1 - ca2)
        dfu = uga * (ta8 + ta4) * sa4
        dnl = lga * (1 - ca2)
        dfl = lga * (ta8 + ta4) * sa4
        if abs(dnu) + abs(dnl) > 2 * girdle_t or dnu < 0 or dnl < 0:
            girdle_real = False
        else:
            va(1, dnl, ang, ang / 2, s)
            va(1, 2 * girdle_t - dnu, ang, ang / 2, s)
            va(1, dfl, ang / 2, ang / 4, 2 * s)
            va(1, 2 * girdle_t - dfu, ang / 2, ang / 4, 2 * s)

    # make girdle faces
    l1 = len(Verts)             # 2*s / 8*s
    for i in range(l1):
        if girdle_real:
            if i < s:
                fa(i, i + s, 2 * i + 6 * s, 2 * i + 4 * s)
                if i == 0:
                    fa(i, s, l1 - 1, 6 * s - 1)
                else:
                    fa(i, i + s, 2 * i + 6 * s - 1, 2 * i + 4 * s - 1)
            elif i > 2 * s - 1 and i < 3 * s:
                fa(i, i + s, 2 * (i + s), 2 * i)
                fa(i, i + s, 2 * (i + s) + 1, 2 * i + 1)
        else:
            if i < s - 1:
                fa(i, i + s, i + s + 1, i + 1)
            elif i == s - 1:
                fa(i, i + s, s, 0)

    # append upper girdle facet vertices
    va((table_w + (1 - table_w) * bezel_f) / ca, (1 - bezel_f) * 2 * crown_h +
       2 * girdle_t, 2 * ang, ang, int(s / 2))

    # make upper girdle facet faces
    l2 = len(Verts)             # 2.5*s / 8.5*s
    for i in range(l2):
        if i > s and i < 2 * s - 1 and i % 2 != 0:
            if girdle_real:
                fa(i, 2 * (i + 2 * s), i + 2 * s, 2 * (i + 2 * s) + 1, i + 1,
                   int(7.5 * s) + int((i - 1) / 2))
                fa(i, 2 * (i + 2 * s) - 1, i + 2 * s - 1, 2 * (i + 2 * s - 1),
                   i - 1, int(7.5 * s) + int((i - 1) / 2))
            else:
                fa(i, i + 1, int((i + 3 * s) / 2))
                fa(i, i - 1, int((i + 3 * s) / 2))
        elif i == s:
            if girdle_real:
                fa(i, l1 - 1, 4 * s - 1, l1 - 2, 2 * i - 1, l2 - 1)
                fa(2 * i - 2, l1 - 4, 4 * s - 2, l1 - 3, 2 * i - 1, l2 - 1)
            else:
                fa(i, 2 * i - 1, l2 - 1)
                fa(2 * i - 1, 2 * i - 2, l2 - 1)

    # append table vertices
    va(table_w, (crown_h + girdle_t) * 2, 2 * ang, 0, int(s / 2))

    # make bezel facet faces and star facet faces
    l3 = len(Verts)             # 3*s / 9*s
    for i in range(l3):
        if i > l2 - 1 and i < l3 - 1:
            fa(i, i + 1, i - int(s / 2))
            fa(i + 1, i - int(s / 2), 2 * (i - l2) + 2 + s, i - int(s / 2) + 1)
        elif i == l3 - 1:
            fa(i, l2, l2 - 1)
            fa(s, l2 - 1, l2, l2 - int(s / 2))

    # make table facet face
    tf = []
    for i in range(l3):
        if i > l2 - 1:
            tf.append(i)
    fa(*tf)

    # append lower girdle facet vertices
    if keep_lga:
        va(pavi_f / ca, (pavi_f - 1) * pavi_d * 2, 2 * ang, ang, int(s / 2))
    else:
        va((pavi_f * (1 - culet) + culet) / ca, (pavi_f - 1) * pavi_d * 2, 2 * ang,
           ang, int(s / 2))

    # make lower girdle facet faces
    l4 = len(Verts)             # 3.5*s / 9.5*s
    for i in range(l4):
        if i > 0 and i < s - 1 and i % 2 == 0:
            if girdle_real:
                fa(i, 2 * (i + 2 * s), i + 2 * s, 2 * (i + 2 * s) + 1, i + 1,
                   int(i / 2) + 9 * s)
                fa(i, 2 * (i + 2 * s) - 1, i + 2 * s - 1, 2 * (i + 2 * s - 1),
                   i - 1, int(i / 2) + 9 * s - 1)
            else:
                fa(i, i + 1, int(i / 2) + l4 - int(s / 2))
                fa(i, i - 1, int(i / 2) + l4 - int(s / 2) - 1)
        elif i == 0:
            if girdle_real:
                fa(0, 4 * s, 2 * s, 4 * s + 1, 1, 9 * s)
                fa(0, 6 * s - 1, 3 * s - 1, 6 * s - 2, s - 1, l4 - 1)
            else:
                fa(0, 1, l4 - int(s / 2))
                fa(0, s - 1, l4 - 1)

    # append culet vertice(s)
    if culet == 0:
        va(0, pavi_d * (-2), 0, 0, 1)
    else:
        if keep_lga:
            va(culet * pavi_f / ca, pavi_d * (-2) + culet * pavi_f * 2 * pavi_d,
               2 * ang, ang, int(s / 2))
        else:
            va(culet / ca, pavi_d * (-2), 2 * ang, ang, int(s / 2))

    # make pavillion facet face
    l5 = len(Verts)             # 4*s / 10*s  //if !culet: 3.5*s+1 / 9.5*s+1
    for i in range(l5):
        if i > 0 and i < s - 1 and i % 2 == 0:
            if culet:
                fa(i, l3 + int(i / 2), l3 + int((s + i) / 2),
                   l3 + int((s + i) / 2) - 1, l3 + int(i / 2) - 1)
            else:
                fa(i, l3 + int(i / 2), l5 - 1, l3 + int(i / 2) - 1)
        elif i == 0:
            if culet:
                fa(i, l3, l4, l5 - 1, l4 - 1)
            else:
                fa(i, l3, l5 - 1, l4 - 1)

    # make culet facet face
    if culet:
        cf = []
        for i in range(l5):
            if i > l4 - 1:
                cf.append(i)
        fa(*cf)

    # bpy variables / shortcuts
    scene = bpy.context.scene

    # deactivate possible active Objects
    bpy.context.scene.objects.active = None

    # create actual mesh and object based on Verts and Faces given
    dmesh = bpy.data.meshes.new("dmesh")
    dmesh.from_pydata(Verts, [], Faces)
    dmesh.update()

    # Create object and link it into scene.
    from bpy_extras import object_utils
    dobj = object_utils.object_data_add(context, dmesh, operator=None, name="dobj").object

    # activate and select object
    scene.objects.active = dobj
    dobj.select = True
    obj = bpy.context.active_object

    # flip all face normals outside
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    sel_mode = bpy.context.tool_settings.mesh_select_mode
    bpy.context.tool_settings.mesh_select_mode = [False, False, True]
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
    for i, face in enumerate(obj.data.polygons):
        face.select = True
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.context.tool_settings.mesh_select_mode = sel_mode
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    # make girdle smooth for complex girdle
    if girdle_real and g_real_smooth:

        bpy.ops.object.mode_set(mode='EDIT', toggle=False)

        bpy.ops.mesh.select_all(action='DESELECT')  # deselect all mesh data
        bpy.ops.object.mode_set(mode='OBJECT')
        pls = []
        dp = obj.data.polygons[:4 * s]              # only consider faces of girdle
        ov = obj.data.vertices

        for i, p in enumerate(dp):
            pls.extend(p.vertices)                  # list all verts of girdle

        for i, e in enumerate(obj.data.edges):      # select egdes to mark sharp
            if e.vertices[0] in pls and e.vertices[1] in pls and abs(
                            ov[e.vertices[0]].co.x - ov[e.vertices[1]].co.x):
                obj.data.edges[i].select = True
                continue
            obj.data.edges[i].select = False

        bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        bpy.ops.mesh.mark_sharp()

        bpy.context.tool_settings.mesh_select_mode = [False, False, True]
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
        bpy.ops.object.select_all(action='DESELECT')
        for i, face in enumerate(obj.data.polygons):
            if i < 4 * s:
                face.select = True
                continue
            face.select = False
        bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        bpy.ops.mesh.faces_shade_smooth()

        bpy.ops.object.modifier_add(type='EDGE_SPLIT')

        bpy.context.tool_settings.mesh_select_mode = sel_mode
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        bpy.ops.object.modifier_apply(apply_as='DATA', modifier="EdgeSplit")

    return dobj


# add new operator for object
class MESH_OT_primitive_brilliant_add(Operator):
    bl_idname = "mesh.primitive_brilliant_add"
    bl_label = "Custom Brilliant"
    bl_description = "Contruct a custom brilliant mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    # set user options
    s = IntProperty(
            name="Segments",
            description="Longitudial segmentation",
            step=1,
            min=6,
            max=128,
            default=16,
            subtype='FACTOR'
            )
    table_w = FloatProperty(
            name="Table width",
            description="Width of table",
            min=0.001,
            max=1.0,
            default=0.53,
            subtype='PERCENTAGE'
            )
    crown_h = FloatProperty(
            name="Crown height",
            description="Heigth of crown",
            min=0.0,
            max=1.0,
            default=0.162,
            subtype='PERCENTAGE'
            )
    girdle_t = FloatProperty(
            name="Girdle height",
            description="Height of girdle",
            min=0.0,
            max=0.5,
            default=0.017,
            subtype='PERCENTAGE'
            )
    girdle_real = BoolProperty(
            name="Real girdle",
            description="More beautiful girdle; has more polygons",
            default=True
            )
    g_real_smooth = BoolProperty(
            name="Smooth girdle",
            description="smooth shading for girdle, only available for real girdle",
            default=False
            )
    pavi_d = FloatProperty(
            name="Pavilion depth",
            description="Height of pavillion",
            min=0.0,
            max=1.0,
            default=0.431,
            subtype='PERCENTAGE'
            )
    bezel_f = FloatProperty(
            name="Upper facet factor",
            description="Determines the form of bezel and upper girdle facets",
            min=0.0,
            max=1.0,
            default=0.250,
            subtype='PERCENTAGE'
            )
    pavi_f = FloatProperty(
            name="Lower facet factor",
            description="Determines the form of pavillion and lower girdle facets",
            min=0.001,
            max=1.0,
            default=0.400,
            subtype='PERCENTAGE'
            )
    culet = FloatProperty(
            name="Culet size",
            description="0: no culet (default)",
            min=0.0,
            max=0.999,
            default=0.0,
            subtype='PERCENTAGE'
            )
    keep_lga = BoolProperty(
            name="Retain lower angle",
            description="If culet > 0, retains angle of pavillion facets",
            default=False
            )

    # call mesh/object generator function with user inputs
    def execute(self, context):
        ob = addBrilliant(context, self.s, self.table_w, self.crown_h,
                          self.girdle_t, self.pavi_d, self.bezel_f,
                          self.pavi_f, self.culet, self.girdle_real,
                          self.keep_lga, self.g_real_smooth
                          )
        return {'FINISHED'}
