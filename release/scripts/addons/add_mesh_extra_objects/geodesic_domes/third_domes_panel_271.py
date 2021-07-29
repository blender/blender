import bpy
import os
from . import vefm_271
from . import forms_271
from . import geodesic_classes_271
from . import add_shape_geodesic

from bpy.types import Operator
from bpy.props import (
        EnumProperty,
        IntProperty,
        FloatProperty,
        StringProperty,
        BoolProperty,
        )
from math import pi
from mathutils import Vector  # used for vertex.vector values


# global #
last_generated_object = None
last_imported_mesh = None
basegeodesic = None
imported_hubmesh_to_use = None
# global end #

# ###### EIND FOR SHAPEKEYS ######


class GenerateGeodesicDome(Operator):
    bl_label = "Modify Geodesic Objects"
    bl_idname = "mesh.generate_geodesic_dome"
    bl_description = "Create Geodesic Object Types"
    bl_options = {'REGISTER', 'UNDO'}

    # PKHG_NEW saving and loading parameters
    save_parameters = BoolProperty(
            name="Save params",
            description="Activation save */tmp/GD_0.GD",
            default=False
            )
    load_parameters = BoolProperty(
            name="Load params",
            description="Read */tmp/GD_0.GD",
            default=False
            )
    gd_help_text_width = IntProperty(
            name="Text Width",
            description="The width above which the text wraps",
            default=60,
            max=180, min=20
            )
    mainpages = EnumProperty(
            name="Menu",
            description="Create Faces, Struts & Hubs",
            items=[("Main", "Main", "Geodesic objects"),
                   ("Faces", "Faces", "Generate Faces"),
                   ("Struts", "Struts", "Generate Struts"),
                   ("Hubs", "Hubs", "Generate Hubs"),
                   ("Help", "Help", "Not implemented"),
                  ],
            default='Main'
            )
    # for Faces
    facetype_menu = EnumProperty(
            name="Faces",
            description="choose a facetype",
            items=[("0", "strip", "strip"),
                   ("1", "open vertical", "vertical"),
                   ("2", "open slanted", "slanted"),
                   ("3", "closed point", "closed point"),
                   ("4", "pillow", "pillow"),
                   ("5", "closed vertical", "closed vertical"),
                   ("6", "stepped", "stepped"),
                   ("7", "spikes", "spikes"),
                   ("8", "boxed", "boxed"),
                   ("9", "diamond", "diamond"),
                   ("10", "bar", "bar"),
                  ],
            default='0'
            )
    facetoggle = BoolProperty(
            name="Activate: Face Object",
            description="Activate Faces for Geodesic object",
            default=False
            )
    face_use_imported_object = BoolProperty(
            name="Use: Imported Object",
            description="Activate faces on your Imported object",
            default=False
            )
    facewidth = FloatProperty(
            name="Face Width",
            min=-1, soft_min=0.001, max=4,
            default=.50
            )
    fwtog = BoolProperty(
            name="Width tweak",
            default=False
            )
    faceheight = FloatProperty(
            name="Face Height",
            min=0.001, max=4,
            default=1
            )
    fhtog = BoolProperty(
            name="Height tweak",
            default=False
            )
    face_detach = BoolProperty(
            name="Detach Faces",
            default=False
            )
    fmeshname = StringProperty(
            name="Face Mesh name",
            default="defaultface"
            )
    geodesic_types = EnumProperty(
            name="Objects",
            description="Choose Geodesic, Grid, Cylinder, Parabola, "
                        "Torus, Sphere, Import your mesh or Superparameters",
            items=[("Geodesic", "Geodesic", "Generate Geodesic"),
                   ("Grid", "Grid", "Generate Grid"),
                   ("Cylinder", "Cylinder", "Generate Cylinder"),
                   ("Parabola", "Parabola", "Generate Parabola"),
                   ("Torus", "Torus", "Generate Torus"),
                   ("Sphere", "Sphere", "Generate Sphere"),
                   ("Import your mesh", "Import your mesh", "Import Your Mesh"),
                  ],
            default='Geodesic'
            )
    import_mesh_name = StringProperty(
            name="Mesh to import",
            description="the name has to be the name of a meshobject",
            default="None"
            )
    base_type = EnumProperty(
            name="Hedron",
            description="Choose between Tetrahedron, Octahedron, Icosahedron ",
            items=[("Tetrahedron", "Tetrahedron", "Generate Tetrahedron"),
                   ("Octahedron", "Octahedron", "Generate Octahedron"),
                   ("Icosahedron", "Icosahedron", "Generate Icosahedron"),
                    ],
            default='Tetrahedron'
            )
    orientation = EnumProperty(
            name="Point^",
            description="Point (Vert), Edge or Face pointing upwards",
            items=[("PointUp", "PointUp", "Point up"),
                   ("EdgeUp", "EdgeUp", "Edge up"),
                   ("FaceUp", "FaceUp", "Face up"),
                   ],
            default='PointUp'
            )
    geodesic_class = EnumProperty(
            name="Class",
            description="Subdivide Basic/Triacon",
            items=[("Class 1", "Class 1", "class one"),
                   ("Class 2", "Class 2", "class two"),
                   ],
            default='Class 1'
            )
    tri_hex_star = EnumProperty(
            name="Shape",
            description="Choose between tri hex star face types",
            items=[("tri", "tri", "tri faces"),
                   ("hex", "hex", "hex faces(by tri)"),
                   ("star", "star", "star faces(by tri)"),
                    ],
            default='tri'
            )
    spherical_flat = EnumProperty(
            name="Round",
            description="Choose between spherical or flat ",
            items=[("spherical", "spherical", "Generate spherical"),
                   ("flat", "flat", "Generate flat"),
                    ],
            default='spherical'
            )
    use_imported_mesh = BoolProperty(
            name="use import",
            description="Use an imported mesh",
            default=False
            )
    # Cylinder
    cyxres = IntProperty(
            name="Resolution x/y",
            min=3, max=32,
            description="Number of faces around x/y",
            default=5
            )
    cyyres = IntProperty(
            name="Resolution z",
            min=3, max=32,
            description="Number of faces in z direction",
            default=5
            )
    cyxsz = FloatProperty(
            name="Scale x/y",
            min=0.01, max=10,
            description="Scale in x/y direction",
            default=1
            )
    cyysz = FloatProperty(
            name="Scale z",
            min=0.01, max=10,
            description="Scale in z direction",
            default=1
            )
    cyxell = FloatProperty(
            name="Stretch x",
            min=0.001, max=4,
            description="Stretch in x direction",
            default=1
            )
    cygap = FloatProperty(
            name="Gap",
            min=-2, max=2,
            description="Shrink in % around radius",
            default=1
            )
    cygphase = FloatProperty(
            name="Phase", min=-4, max=4,
            description="Rotate around pivot x/y",
            default=0
            )
    # Parabola
    paxres = IntProperty(
            name="Resolution x/y",
            min=3, max=32,
            description="Number of faces around x/y",
            default=5
            )
    payres = IntProperty(
            name="Resolution z",
            min=3, max=32,
            description="Number of faces in z direction",
            default=5
            )
    paxsz = FloatProperty(
            name="Scale x/y",
            min=0.001, max=10,
            description="scale in x/y direction",
            default=0.30
            )
    paysz = FloatProperty(
            name="Scale z",
            min=0.001, max=10,
            description="Scale in z direction",
            default=1
            )
    paxell = FloatProperty(
            name="Stretch x",
            min=0.001, max=4,
            description="Stretch in x direction",
            default=1
            )
    pagap = FloatProperty(
            name="Gap",
            min=-2, max=2,
            description="Shrink in % around radius",
            default=1
            )
    pagphase = FloatProperty(
            name="Phase",
            min=-4, max=4,
            description="Rotate around pivot x/y",
            default=0
            )
    # Torus
    ures = IntProperty(
            name="Resolution x/y",
            min=3, max=32,
            description="Number of faces around x/y",
            default=8)
    vres = IntProperty(
            name="Resolution z",
            min=3, max=32,
            description="Number of faces in z direction",
            default=8)
    urad = FloatProperty(
            name="Radius x/y",
            min=0.001, max=10,
            description="Radius in x/y plane",
            default=1
            )
    vrad = FloatProperty(
            name="Radius z",
            min=0.001, max=10,
            description="Radius in z plane",
            default=0.250
            )
    uellipse = FloatProperty(
            name="Stretch x",
            min=0.001, max=10,
            description="Number of faces in z direction",
            default=1
            )
    vellipse = FloatProperty(
            name="Stretch z",
            min=0.001, max=10,
            description="Number of faces in z direction",
            default=1
            )
    upart = FloatProperty(
            name="Gap x/y",
            min=-4, max=4,
            description="Shrink faces around x/y",
            default=1
            )
    vpart = FloatProperty(
            name="Gap z",
            min=-4, max=4,
            description="Shrink faces in z direction",
            default=1
            )
    ugap = FloatProperty(
            name="Phase x/y",
            min=-4, max=4,
            description="Rotate around pivot x/y",
            default=0
            )
    vgap = FloatProperty(
            name="Phase z",
            min=-4, max=4,
            description="Rotate around pivot z",
            default=0
            )
    uphase = FloatProperty(
            name="uphase",
            min=-4, max=4,
            description="Number of faces in z direction",
            default=0
            )
    vphase = FloatProperty(
            name="vphase",
            min=-4, max=4,
            description="Number of faces in z direction",
            default=0
            )
    uexp = FloatProperty(
            name="uexp",
            min=-4, max=4,
            description="Number of faces in z direction",
            default=0
            )
    vexp = FloatProperty(
            name="vexp",
            min=-4, max=4,
            description="Number of faces in z direction",
            default=0
            )
    usuper = FloatProperty(
            name="usuper",
            min=-4, max=4,
            description="First set of superform parameters",
            default=2
            )
    vsuper = FloatProperty(
            name="vsuper",
            min=-4, max=4,
            description="Second set of superform parameters",
            default=2
            )
    utwist = FloatProperty(
            name="Twist x/y",
            min=-4, max=4,
            description="Use with superformular u",
            default=0
             )
    vtwist = FloatProperty(
            name="Twist z",
            min=-4, max=4,
            description="Use with superformular v",
            default=0
            )
    # Sphere
    bures = IntProperty(
            name="Resolution x/y",
            min=3, max=32,
            description="Number of faces around x/y",
            default=8
            )
    bvres = IntProperty(
            name="Resolution z",
            min=3, max=32,
            description="Number of faces in z direction",
            default=8
            )
    burad = FloatProperty(
            name="Radius",
            min=-4, max=4,
            description="overall radius",
            default=1
            )
    bupart = FloatProperty(
            name="Gap x/y",
            min=-4, max=4,
            description="Shrink faces around x/y",
            default=1
            )
    bvpart = FloatProperty(
            name="Gap z",
            min=-4, max=4,
            description="Shrink faces in z direction",
            default=1
            )
    buphase = FloatProperty(
            name="Phase x/y",
            min=-4, max=4,
            description="Rotate around pivot x/y",
            default=0
            )
    bvphase = FloatProperty(
            name="Phase z",
            min=-4, max=4,
            description="Rotate around pivot z",
            default=0
            )
    buellipse = FloatProperty(
            name="Stretch x",
            min=0.001, max=4,
            description="Stretch in the x direction",
            default=1
            )
    bvellipse = FloatProperty(
            name="Stretch z",
            min=0.001, max=4,
            description="Stretch in the z direction",
            default=1
            )
    # Grid
    grxres = IntProperty(
            name="Resolution x",
            min=2, soft_max=10, max=20,
            description="Number of faces in x direction",
            default=5
            )
    gryres = IntProperty(
            name="Resolution z",
            min=2, soft_min=2,
            soft_max=10, max=20,
            description="Number of faces in x direction",
            default=2
            )
    grxsz = FloatProperty(
            name="X size",
            min=1, soft_min=0.01,
            soft_max=5, max=10,
            description="X size",
            default=2.0
            )
    grysz = FloatProperty(
            name="Y size",
            min=1, soft_min=0.01,
            soft_max=5, max=10,
            description="Y size",
            default=1.0
            )

    # PKHG_TODO_??? what means cart
    cart = IntProperty(
            name="cart",
            min=0, max=2,
            default=0
            )
    frequency = IntProperty(
            name="Frequency",
            min=1, max=8,
            description="Subdivide base triangles",
            default=1
            )
    eccentricity = FloatProperty(
            name="Eccentricity",
            min=0.01, max=4,
            description="Scaling in x/y dimension",
            default=1
            )
    squish = FloatProperty(
            name="Squish",
            min=0.01,
            soft_max=4, max=10,
            description="Scaling in z dimension",
            default=1
            )
    radius = FloatProperty(
            name="Radius",
            min=0.01,
            soft_max=4, max=10,
            description="Overall radius",
            default=1
            )
    squareness = FloatProperty(
            name="Square x/y",
            min=0.1, max=5,
            description="Superelipse action in x/y",
            default=2
            )
    squarez = FloatProperty(
            name="Square z",
            min=0.1, soft_max=5, max=10,
            description="Superelipse action in z",
            default=2
            )
    baselevel = IntProperty(
            name="baselevel",
            default=5
            )
    dual = BoolProperty(
            name="Dual",
            description="Faces become verts, "
                        "verts become faces, edges flip",
            default=False
            )
    rotxy = FloatProperty(
            name="Rotate x/y",
            min=-4, max=4,
            description="Rotate superelipse action in x/y",
            default=0
            )
    rotz = FloatProperty(
            name="Rotate z",
            min=-4, max=4,
            description="Rotate superelipse action in z",
            default=0
            )

    # for choice of superformula
    uact = BoolProperty(
            name="Superformula u (x/y)",
            description="Activate superformula u parameters",
            default=False
            )
    vact = BoolProperty(
            name="Superformula v (z)",
            description="Activate superformula v parameters",
            default=False
            )
    um = FloatProperty(
            name="Pinch x/y",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Pinch the mesh on x/y",
            default=3
            )
    un1 = FloatProperty(
            name="Squash x/y",
            min=0, soft_min=0.1,
            soft_max=5, max=20,
            description="Squash the mesh x/y",
            default=1
            )
    un2 = FloatProperty(
            name="Inflate x/y",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Inflate the mesh x/y",
            default=1
            )
    un3 = FloatProperty(
            name="Roundify x/y",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Roundify x/y",
            default=1
            )
    ua = FloatProperty(
            name="Shrink",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Shrink",
            default=1.0
            )
    ub = FloatProperty(
            name="Shrink x/y",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Shrink y/x",
            default=4.0
            )
    vm = FloatProperty(
            name="Scale Z Base",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Scale Z Base",
            default=1
            )
    vn1 = FloatProperty(
            name="Scale lock Top Z",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Scale lock Top Z",
            default=1
            )
    vn2 = FloatProperty(
            name="Inflate Base",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Inflate Base",
            default=1
            )
    vn3 = FloatProperty(
            name="Inflate",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Inflate",
            default=1
            )
    va = FloatProperty(
            name="Scale 1",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Scale 1",
            default=1
            )
    vb = FloatProperty(
            name="Scale 2",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="Scale 2",
            default=1
            )

    uturn = FloatProperty(
            name="x/y Vert cycle",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="x/y Vert cycle",
            default=0
            )
    vturn = FloatProperty(
            name="z Vert cycle",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="z Vert cycle",
            default=0
            )
    utwist = FloatProperty(
            name="x/y Twist cycle",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="x/y Twist cycle",
            default=0
            )
    vtwist = FloatProperty(
            name="z Twist cycle",
            min=0, soft_min=0.1,
            soft_max=5, max=10,
            description="z Twist cycle",
            default=0
            )
    # Strut
    struttype = IntProperty(
            name="Strut type",
            default=0
            )
    struttoggle = BoolProperty(
            name="Use Struts",
            default=False
            )
    strutimporttoggle = BoolProperty(
            name="Strut import toggle",
            default=False
            )
    strutimpmesh = StringProperty(
            name="Strut import mesh",
            default="None"
            )
    strutwidth = FloatProperty(
            name="Strut width",
            min=-10, soft_min=5,
            soft_max=5, max=10,
            default=1
            )
    swtog = BoolProperty(
            name="Width enable",
            default=False
            )
    strutheight = FloatProperty(
            name="Strut height",
            min=-5, soft_min=-1,
            soft_max=5, max=10,
            default=1
            )
    shtog = BoolProperty(
            name="Height tweak",
            default=False
            )
    strutshrink = FloatProperty(
            name="Strut shrink",
            min=0.001, max=4,
            default=1
            )
    sstog = BoolProperty(
            name="Shrink tweak",
            default=False
            )
    stretch = FloatProperty(
            name="Stretch",
            min=-4, max=4,
            default=1.0
            )
    lift = FloatProperty(
            name="Lift",
            min=0.001, max=10,
            default=0
            )
    smeshname = StringProperty(
            name="Strut mesh name",
            default="defaultstrut"
            )
    # Hubs
    hubtype = BoolProperty(
            name="Hub type",
            description="not used",
            default=True
            )
    hubtoggle = BoolProperty(
            name="Use Hubs",
            default=False
            )
    hubimporttoggle = BoolProperty(
            name="New import",
            description="Import a mesh",
            default=False
            )
    hubimpmesh = StringProperty(
            name="Hub mesh import",
            description="Name of mesh to import",
            default="None"
            )
    hubwidth = FloatProperty(
            name="Hub width",
            min=0.01, max=10,
            default=1
            )
    hwtog = BoolProperty(
            name="Width tweak",
            default=False
            )
    hubheight = FloatProperty(
            name="Hub height",
            min=0.01, max=10,
            default=1
            )
    hhtog = BoolProperty(
            name="Height tweak",
            default=False
            )
    hublength = FloatProperty(
            name="Hub length",
            min=0.1, max=10,
            default=1
            )
    hstog = BoolProperty(
            name="Hub s tweak",
            default=False
            )
    hmeshname = StringProperty(
            name="Hub mesh name",
            description="Name of an existing mesh needed!",
            default="None"
            )
    name_list = [
            'facetype_menu', 'facetoggle', 'face_use_imported_object',
            'facewidth', 'fwtog', 'faceheight', 'fhtog',
            'face_detach', 'fmeshname', 'geodesic_types', 'import_mesh_name',
            'base_type', 'orientation', 'geodesic_class', 'tri_hex_star',
            'spherical_flat', 'use_imported_mesh', 'cyxres', 'cyyres',
            'cyxsz', 'cyysz', 'cyxell', 'cygap',
            'cygphase', 'paxres', 'payres', 'paxsz',
            'paysz', 'paxell', 'pagap', 'pagphase',
            'ures', 'vres', 'urad', 'vrad',
            'uellipse', 'vellipse', 'upart', 'vpart',
            'ugap', 'vgap', 'uphase', 'vphase',
            'uexp', 'vexp', 'usuper', 'vsuper',
            'utwist', 'vtwist', 'bures', 'bvres',
            'burad', 'bupart', 'bvpart', 'buphase',
            'bvphase', 'buellipse', 'bvellipse', 'grxres',
            'gryres', 'grxsz', 'grysz',
            'cart', 'frequency', 'eccentricity', 'squish',
            'radius', 'squareness', 'squarez', 'baselevel',
            'dual', 'rotxy', 'rotz',
            'uact', 'vact', 'um', 'un1',
            'un2', 'un3', 'ua', 'ub',
            'vm', 'vn1', 'vn2', 'vn3',
            'va', 'vb', 'uturn', 'vturn',
            'utwist', 'vtwist', 'struttype', 'struttoggle',
            'strutimporttoggle', 'strutimpmesh', 'strutwidth', 'swtog',
            'strutheight', 'shtog', 'strutshrink', 'sstog',
            'stretch', 'lift', 'smeshname', 'hubtype',
            'hubtoggle', 'hubimporttoggle', 'hubimpmesh', 'hubwidth',
            'hwtog', 'hubheight', 'hhtog', 'hublength',
            'hstog', 'hmeshname'
            ]

    def write_params(self, filename):
        file = open(filename, "w", encoding="utf8", newline="\n")
        fw = file.write
        # for Faces!
        for el in self.name_list:
            fw(el + ", ")
            fw(repr(getattr(self, el)))
            fw(", \n")
        file.close()

    def read_file(self, filename):
        file = open(filename, "r", newline="\n")
        result = []
        line = file.readline()
        while(line):
            tmp = line.split(",  ")
            result.append(eval(tmp[1]))
            line = file.readline()
        return result

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        col = layout.column()
        col.prop(self, "mainpages")
        which_mainpages = self.mainpages
        if which_mainpages == 'Main':
            col = layout.column()
            col.prop(self, "geodesic_types")
            tmp = self.geodesic_types
            if tmp == "Geodesic":
                col.label(text="Geodesic Object Types:")
                col.prop(self, "geodesic_class")
                col.prop(self, "base_type")
                col.prop(self, "orientation")
                col.prop(self, "tri_hex_star")
                col.prop(self, "spherical_flat")
                col.label("Geodesic Object Parameters:")
                row = layout.row()
                row.prop(self, "frequency")
                row = layout.row()
                row.prop(self, "radius")
                row = layout.row()
                row.prop(self, "eccentricity")
                row = layout.row()
                row.prop(self, "squish")
                row = layout.row()
                row.prop(self, "squareness")
                row = layout.row()
                row.prop(self, "squarez")
                row = layout.row()
                row.prop(self, "rotxy")
                row = layout.row()
                row.prop(self, "rotz")
                row = layout.row()
                row.prop(self, "dual")
            elif tmp == 'Torus':
                col.label("Torus Parameters")
                row = layout.row()
                row.prop(self, "ures")
                row = layout.row()
                row.prop(self, "vres")
                row = layout.row()
                row.prop(self, "urad")
                row = layout.row()
                row.prop(self, "vrad")
                row = layout.row()
                row.prop(self, "uellipse")
                row = layout.row()
                row.prop(self, "vellipse")
                row = layout.row()
                row.prop(self, "upart")
                row = layout.row()
                row.prop(self, "vpart")
                row = layout.row()
                row.prop(self, "ugap")
                row.prop(self, "vgap")
                row = layout.row()

            elif tmp == 'Sphere':
                col.label("Sphere Parameters")
                row = layout.row()
                row.prop(self, "bures")
                row = layout.row()
                row.prop(self, "bvres")
                row = layout.row()
                row.prop(self, "burad")
                row = layout.row()
                row.prop(self, "bupart")
                row = layout.row()
                row.prop(self, "buphase")
                row = layout.row()
                row.prop(self, "bvpart")
                row = layout.row()
                row.prop(self, "bvphase")
                row = layout.row()
                row.prop(self, "buellipse")
                row = layout.row()
                row.prop(self, "bvellipse")
            elif tmp == 'Parabola':
                col.label("Parabola Parameters")
                row = layout.row()
                row.prop(self, "paxres")
                row = layout.row()
                row.prop(self, "payres")
                row = layout.row()
                row.prop(self, "paxsz")
                row = layout.row()
                row.prop(self, "paysz")
                row = layout.row()
                row.prop(self, "paxell")
                row = layout.row()
                row.prop(self, "pagap")
                row = layout.row()
                row.prop(self, "pagphase")
            elif tmp == 'Cylinder':
                col.label("Cylinder Parameters")
                col.prop(self, "cyxres")
                col.prop(self, "cyyres")
                col.prop(self, "cyxsz")
                col.prop(self, "cyysz")
                col.prop(self, "cyxell")
                col.prop(self, "cygap")
                col.prop(self, "cygphase")
            elif tmp == 'Grid':
                col.label("Grid Parameters")
                row = layout.row()
                row.prop(self, "grxres")
                row = layout.row()
                row.prop(self, "gryres")
                row = layout.row()
                row.prop(self, "grxsz")
                row = layout.row()
                row.prop(self, "grysz")
            elif tmp == 'Import your mesh':
                col.prop(self, "use_imported_mesh")
                col.prop(self, "import_mesh_name")
            # superform parameters only where possible
            row = layout.row()
            row.prop(self, "uact")
            row = layout.row()
            row.prop(self, "vact")
            row = layout.row()
            if not(tmp == 'Import your mesh'):
                if (self.uact is False) and (self.vact is False):
                    row.label(text="No checkbox active", icon="INFO")
                else:
                    row.label("Superform Parameters")
                if self.uact:
                    row = layout.row()
                    row.prop(self, "um")
                    row = layout.row()
                    row.prop(self, "un1")
                    row = layout.row()
                    row.prop(self, "un2")
                    row = layout.row()
                    row.prop(self, "un3")
                    row = layout.row()
                    row.prop(self, "ua")
                    row = layout.row()
                    row.prop(self, "ub")
                    row = layout.row()
                    row.prop(self, "uturn")
                    row = layout.row()
                    row.prop(self, "utwist")
                if self.vact:
                    row = layout.row()
                    row.prop(self, "vm")
                    row = layout.row()
                    row.prop(self, "vn1")
                    row = layout.row()
                    row.prop(self, "vn2")
                    row = layout.row()
                    row.prop(self, "vn3")
                    row = layout.row()
                    row.prop(self, "va")
                    row = layout.row()
                    row.prop(self, "vb")
                    row = layout.row()
                    row.prop(self, "vturn")
                    row = layout.row()
                    row.prop(self, "vtwist")
        # einde superform
        elif which_mainpages == "Hubs":
            row = layout.row()
            row.prop(self, "hubtoggle")
            row = layout.row()
            if self.hubimpmesh == "None":
                row = layout.row()
                row.label("Name of a hub to use")
                row = layout.row()
            row.prop(self, "hubimpmesh")
            row = layout.row()
            if self.hmeshname == "None":
                row = layout.row()
                row.label("Name of mesh to be filled in")
                row = layout.row()
            row.prop(self, "hmeshname")
            row = layout.row()
            row.prop(self, "hwtog")
            if self.hwtog:
                row.prop(self, "hubwidth")
            row = layout.row()
            row.prop(self, "hhtog")
            if self.hhtog:
                row.prop(self, "hubheight")
            row = layout.row()
            row.prop(self, "hublength")
        elif which_mainpages == "Struts":
            row = layout.row()
            row.prop(self, "struttype")
            row.prop(self, "struttoggle")

            row = layout.row()
            row.prop(self, "strutimpmesh")
            row = layout.row()
            row.prop(self, "swtog")
            if self.swtog:
                row.prop(self, "strutwidth")
            row = layout.row()
            row.prop(self, "shtog")
            if self.shtog:
                row.prop(self, "strutheight")
            row = layout.row()
            row.prop(self, "sstog")
            if self.sstog:
                row.prop(self, "strutshrink")
            row = layout.row()
            row.prop(self, "stretch")
            row = layout.row()
            row.prop(self, "lift")
            row = layout.row()
            row.prop(self, "smeshname")
        elif which_mainpages == "Faces":
            row = layout.row()
            row.prop(self, "facetoggle")
            row = layout.row()
            row.prop(self, "face_use_imported_object")
            row = layout.row()
            row.prop(self, "facetype_menu")
            row = layout.row()
            row.prop(self, "fwtog")
            if self.fwtog:
                row.prop(self, "facewidth")
            row = layout.row()
            row.prop(self, "fhtog")
            if self.fhtog:
                row.prop(self, "faceheight")
            row = layout.row()
            row.prop(self, "face_detach")
            row = layout.row()
            row.prop(self, "fmeshname")
            row = layout.row()

        # help menu GUI
        elif which_mainpages == "Help":
            import textwrap

            # a function that allows for multiple labels with text that wraps
            # you can allow the user to set where the text wraps with the
            # text_width parameter
            # other parameters are ui : here you usually pass layout
            # text: is a list with each index representing a line of text

            def multi_label(text, ui, text_width=120):
                for x in range(0, len(text)):
                    el = textwrap.wrap(text[x], width=text_width)

                    for y in range(0, len(el)):
                        ui.label(text=el[y])

            box = layout.box()
            help_text = ["To Use",
                "If normals look inverted:",
                "Once mesh is finished,",
                "You may recalc normals outside.",
                "--------",
                "To use your own mesh with the:",
                "Faces:",
                "Import your mesh in the:",
                "Objects: Geodesic menu.",
                "You must type in the name",
                "Of your custom object first.",
                "--------",
                "To use your own mesh with the: ",
                "Struts/Hubs:",
                "You must type in the name",
                "Of your custom object/s first,"]
            text_width = self.gd_help_text_width
            box.prop(self, "gd_help_text_width", slider=True)
            multi_label(help_text, box, text_width)

    def execute(self, context):
        global last_generated_object, last_imported_mesh, basegeodesic, imported_hubmesh_to_use
        # default superformparam = [3, 10, 10, 10, 1, 1, 4, 10, 10, 10, 1, 1, 0, 0, 0.0, 0.0, 0, 0]]
        superformparam = [self.um, self.un1, self.un2, self.un3, self.ua,
                          self.ub, self.vm, self.vn1, self.vn2, self.vn3,
                          self.va, self.vb, self.uact, self.vact,
                          self.uturn * pi, self.vturn * pi,
                          self.utwist, self.vtwist]
        context.scene.error_message = ""
        if self.mainpages == 'Main':
            if self.geodesic_types == "Geodesic":
                tmp_fs = self.tri_hex_star
                faceshape = 0  # tri!
                if tmp_fs == "hex":
                    faceshape = 1
                elif tmp_fs == "star":
                    faceshape = 2
                tmp_cl = self.geodesic_class
                klass = 0
                if tmp_cl == "Class 2":
                    klass = 1
                shape = 0
                parameters = [self.frequency, self.eccentricity, self.squish,
                            self.radius, self.squareness, self.squarez, 0,
                            shape, self.baselevel, faceshape, self.dual,
                            self.rotxy, self.rotz, klass, superformparam]

                basegeodesic = creategeo(self.base_type, self.orientation, parameters)
                basegeodesic.makegeodesic()
                basegeodesic.connectivity()
                mesh = vefm_271.mesh()
                vefm_271.finalfill(basegeodesic, mesh)  # always! for hexifiy etc. necessarry!!!
                vefm_271.vefm_add_object(mesh)
                last_generated_object = context.active_object
                last_generated_object.location = (0, 0, 0)
                context.scene.objects.active = last_generated_object
            elif self.geodesic_types == 'Grid':
                basegeodesic = forms_271.grid(self.grxres, self.gryres,
                       self.grxsz, self.grysz, 1.0, 1.0, 0, 0, 0,
                                      0, 1.0, 1.0, superformparam)
                vefm_271.vefm_add_object(basegeodesic)
                bpy.data.objects[-1].location = (0, 0, 0)
            elif self.geodesic_types == "Cylinder":
                basegeodesic = forms_271.cylinder(
                                    self.cyxres, self.cyyres,
                                    self.cyxsz, self.cyysz, self.cygap,
                                    1.0, self.cygphase, 0, 0, 0, self.cyxell,
                                    1.0, superformparam
                                    )
                vefm_271.vefm_add_object(basegeodesic)
                bpy.data.objects[-1].location = (0, 0, 0)

            elif self.geodesic_types == "Parabola":
                basegeodesic = forms_271.parabola(
                                    self.paxres, self.payres,
                                    self.paxsz, self.paysz, self.pagap, 1.0, self.pagphase,
                                    0, 0, 0, self.paxell, 1.0, superformparam
                                    )
                vefm_271.vefm_add_object(basegeodesic)
                bpy.data.objects[-1].location = (0, 0, 0)
            elif self.geodesic_types == "Torus":
                basegeodesic = forms_271.torus(
                                    self.ures, self.vres,
                                    self.vrad, self.urad, self.upart, self.vpart,
                                    self.ugap, self.vgap, 0, 0, self.uellipse,
                                    self.vellipse, superformparam
                                    )
                vefm_271.vefm_add_object(basegeodesic)
                bpy.data.objects[-1].location = (0, 0, 0)
            elif self.geodesic_types == "Sphere":
                basegeodesic = forms_271.sphere(
                                    self.bures, self.bvres,
                                    self.burad, 1.0, self.bupart, self.bvpart,
                                    self.buphase, self.bvphase, 0, 0, self.buellipse,
                                    self.bvellipse, superformparam
                                    )

                vefm_271.vefm_add_object(basegeodesic)
                bpy.data.objects[-1].location = (0, 0, 0)

            elif self.geodesic_types == "Import your mesh":
                obj_name = self.import_mesh_name
                if obj_name == "None":
                    message = "Fill in a name \nof an existing mesh\nto be imported"
                    context.scene.error_message = message
                    self.report({"INFO"}, message)
                    print("***INFO*** You have to fill in the name of an existing mesh")
                else:
                    names = [el.name for el in context.scene.objects]
                    if obj_name in names and context.scene.objects[obj_name].type == "MESH":
                        obj = context.scene.objects[obj_name]
                        your_obj = vefm_271.importmesh(obj.name, False)
                        last_imported_mesh = your_obj
                        vefm_271.vefm_add_object(your_obj)
                        last_generated_object = bpy.context.active_object
                        last_generated_object.name = "Imported mesh"
                        bpy.context.active_object.location = (0, 0, 0)
                    else:
                        message = obj_name + " does not exist \nor is not a Mesh"
                        context.scene.error_message = message
                        bpy.ops.object.dialog_operator('INVOKE_DEFAULT')
                        self.report({'ERROR'}, message)
                        print("***ERROR***" + obj_name + " does not exist or is not a Mesh")
        elif self.mainpages == "Hubs":
            hubtype = self.hubtype
            hubtoggle = self.hubtoggle
            hubimporttoggle = self.hubimporttoggle
            hubimpmesh = self.hubimpmesh
            hubwidth = self.hubwidth
            hwtog = self.hwtog
            hubheight = self.hubheight
            hhtog = self.hhtog
            hublength = self.hublength
            hstog = self.hstog
            hmeshname = self.hmeshname

            if not (hmeshname == "None") and not (hubimpmesh == "None") and hubtoggle:
                try:
                    hub_obj = vefm_271.importmesh(hmeshname, 0)

                    hub = vefm_271.hub(
                                    hub_obj, True,
                                    hubwidth, hubheight, hublength,
                                    hwtog, hhtog, hstog, hubimpmesh
                                    )
                    mesh = vefm_271.mesh("test")
                    vefm_271.finalfill(hub, mesh)
                    vefm_271.vefm_add_object(mesh)
                    bpy.data.objects[-1].location = (0, 0, 0)
                except:
                    message = "***ERROR*** \nEither no mesh for hub\nor " + \
                              hmeshname + " available"
                    context.scene.error_message = message
                    bpy.ops.object.dialog_operator('INVOKE_DEFAULT')
                    print(message)
            else:
                message = "***INFO***\nEnable Hubs first"
                context.scene.error_message = message
                print("\n***INFO*** Enable Hubs first")
        elif self.mainpages == "Struts":
            struttype = self.struttype
            struttoggle = self.struttoggle
            strutimporttoggle = self.strutimporttoggle
            strutimpmesh = self.strutimpmesh
            strutwidth = self.strutwidth
            swtog = self.swtog
            strutheight = self.strutheight
            shtog = self.shtog
            strutshrink = self.strutshrink
            sstog = self.sstog
            stretch = self.stretch
            lift = self.lift
            smeshname = self.smeshname
            if not (strutimpmesh == "None") and struttoggle:
                names = [el.name for el in context.scene.objects]
                if strutimpmesh in names and context.scene.objects[strutimpmesh].type == "MESH":
                    strut = vefm_271.strut(
                                        basegeodesic, struttype, strutwidth,
                                        strutheight, stretch, swtog, shtog, swtog,
                                        strutimpmesh, sstog, lift
                                        )
                    strutmesh = vefm_271.mesh()
                    vefm_271.finalfill(strut, strutmesh)
                    vefm_271.vefm_add_object(strutmesh)
                    last_generated_object = context.active_object
                    last_generated_object.name = smeshname
                    last_generated_object.location = (0, 0, 0)
                else:
                    message = "***ERROR***\nStrut object " + strutimpmesh + "\nis not a Mesh"
                    context.scene.error_message = message
                    bpy.ops.object.dialog_operator('INVOKE_DEFAULT')
                    print("***ERROR*** Strut object is not a Mesh")
            else:
                vefm_271.vefm_add_object(basegeodesic)
        elif self.mainpages == "Faces":
            if self.facetoggle:
                faceparams = [[self.face_detach, 0, [[0.5, 0.0]]],  # 0 strip
                            [self.face_detach, 0, [[0.0, 0.5]]],  # 1 vertical
                            [self.face_detach, 0, [[0.5, 0.5]]],  # 2 slanted
                            [self.face_detach, 1, [[0.25, 0.25], [0.5, 0.5]]],  # 3 closed point
                            [self.face_detach, 1, [[0.1, 0.03], [0.33, 0.06], [0.0, 0.1]]],  # 4 pillow
                            [self.face_detach, 2, [[0.0, 0.5]]],  # 5 closed vertical
                            [self.face_detach, 2, [[0.0, 0.25], [0.25, 0.25], [0.25, 0.5]]],  # 6 stepped
                            [self.face_detach, 1, [[0.2, 0.1], [0.4, 0.2], [0.0, 1.0]]],  # 7 spikes
                            [self.face_detach, 3, [[0.25, 0.0], [0.25, 0.5], [0.0, 0.5]]],  # 8 boxed
                            [self.face_detach, 3, [[0.25, 0.5], [0.5, 0.0], [0.25, -0.5]]],  # 9 diamond
                            [self.face_detach, 4, [[0.5, 0.0], [0.5, 0.5], [0.0, 0.5]]], ]  # 10 bar
                facedata = faceparams[int(self.facetype_menu)]
                if not self.face_use_imported_object:
                    faceobject = vefm_271.facetype(
                                            basegeodesic, facedata, self.facewidth,
                                            self.faceheight, self.fwtog
                                            )
                else:
                    if last_imported_mesh:
                        faceobject = vefm_271.facetype(
                                            last_imported_mesh, facedata,
                                            self.facewidth, self.faceheight, self.fwtog
                                            )
                    else:
                        message = "***ERROR***\nNo imported message available\n" + "last geodesic used"
                        context.scene.error_message = message
                        bpy.ops.object.dialog_operator('INVOKE_DEFAULT')
                        print("\n***ERROR*** No imported mesh available \nLast geodesic used!")
                        faceobject = vefm_271.facetype(
                                            basegeodesic, facedata,
                                            self.facewidth, self.faceheight, self.fwtog
                                            )
                facemesh = vefm_271.mesh()
                finalfill(faceobject, facemesh)
                vefm_271.vefm_add_object(facemesh)
                obj = bpy.data.objects[-1]
                obj.name = self.fmeshname
                obj.location = (0, 0, 0)

        # PKHG save or load (nearly) all parameters
        if self.save_parameters:
            self.save_parameters = False
            try:
                scriptpath = bpy.utils.script_paths()[0]
                sep = os.path.sep
                tmpdir = os.path.join(scriptpath, "addons", "add_mesh_extra_objects", "tmp")
                # scriptpath + sep + "addons" + sep + "geodesic_domes" + sep + "tmp"
                if not os.path.isdir(tmpdir):
                    message = "***ERROR***\n" + tmpdir + "\nnot (yet) available"

                filename = tmpdir + sep + "GD_0.GD"
                # self.read_file(filename)
                try:
                    self.write_params(filename)
                    message = "***OK***\nParameters saved in\n" + filename
                    print(message)
                except:
                    message = "***ERRROR***\n" + "Writing " + filename + "\nis not possible"
                # bpy.context.scene.instant_filenames = filenames

            except:
                message = "***ERROR***\n Contakt PKHG, something wrong happened"

            context.scene.error_message = message
            bpy.ops.object.dialog_operator('INVOKE_DEFAULT')

        if self.load_parameters:
            self.load_parameters = False
            try:
                scriptpath = bpy.utils.script_paths()[0]
                sep = os.path.sep
                tmpdir = os.path.join(scriptpath, "addons", "add_mesh_extra_objects", "tmp")
                # PKHG>NEXT comment????
                # scriptpath + sep + "addons" + sep + "geodesic_domes" + sep + "tmp"
                if not os.path.isdir(tmpdir):
                    message = "***ERROR***\n" + tmpdir + "\nis not available"
                    print(message)
                filename = tmpdir + sep + "GD_0.GD"
                # self.read_file(filename)
                try:
                    res = self.read_file(filename)
                    for i, el in enumerate(self.name_list):
                        setattr(self, el, res[i])
                    message = "***OK***\nparameters read from\n" + filename
                    print(message)
                except:
                    message = "***ERRROR***\n" + "Writing " + filename + "\nnot possible"
                    # bpy.context.scene.instant_filenames = filenames
            except:
                message = "***ERROR***\n Contakt PKHG,\nsomething went wrong reading params happened"
            context.scene.error_message = message
            bpy.ops.object.dialog_operator('INVOKE_DEFAULT')

        return {'FINISHED'}

    def invoke(self, context, event):
        global basegeodesic
        bpy.ops.view3d.snap_cursor_to_center()
        tmp = context.scene.geodesic_not_yet_called
        if tmp:
            context.scene.geodesic_not_yet_called = False
        self.execute(context)

        return {'FINISHED'}


def creategeo(polytype, orientation, parameters):
    geo = None
    if polytype == "Tetrahedron":
        if orientation == "PointUp":
            geo = geodesic_classes_271.tetrahedron(parameters)
        elif orientation == "EdgeUp":
            geo = geodesic_classes_271.tetraedge(parameters)
        elif orientation == "FaceUp":
            geo = geodesic_classes_271.tetraface(parameters)
    elif polytype == "Octahedron":
        if orientation == "PointUp":
            geo = geodesic_classes_271.octahedron(parameters)
        elif orientation == "EdgeUp":
            geo = geodesic_classes_271.octaedge(parameters)
        elif orientation == "FaceUp":
            geo = geodesic_classes_271.octaface(parameters)
    elif polytype == "Icosahedron":
        if orientation == "PointUp":
            geo = geodesic_classes_271.icosahedron(parameters)
        elif orientation == "EdgeUp":
            geo = geodesic_classes_271.icoedge(parameters)
        elif orientation == "FaceUp":
            geo = geodesic_classes_271.icoface(parameters)
    return geo


basegeodesic, fmeshname, smeshname, hmeshname, outputmeshname, strutimpmesh, hubimpmesh = [None] * 7


def finalfill(source, target):
    count = 0
    for point in source.verts:
        newvert = vefm_271.vertex(point.vector)
        target.verts.append(newvert)
        point.index = count
        count += 1
    for facey in source.faces:
        row = len(facey.vertices)
        if row >= 5:
            newvert = vefm_271.average(facey.vertices).centroid()
            centre = vefm_271.vertex(newvert.vector)
            target.verts.append(centre)
            for i in range(row):
                if i == row - 1:
                    a = target.verts[facey.vertices[-1].index]
                    b = target.verts[facey.vertices[0].index]
                else:
                    a = target.verts[facey.vertices[i].index]
                    b = target.verts[facey.vertices[i + 1].index]
                c = centre
                f = [a, b, c]
                target.faces.append(f)
        else:
            f = []
            for j in range(len(facey.vertices)):

                a = facey.vertices[j]
                f.append(target.verts[a.index])
            target.faces.append(f)


# for error messages
class DialogOperator(Operator):
    bl_idname = "object.dialog_operator"
    bl_label = "INFO"

    def draw(self, context):
        layout = self.layout
        message = context.scene.error_message
        col = layout.column()
        tmp = message.split("\n")
        for el in tmp:
            col.label(el)

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
