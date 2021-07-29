from math import sin, cos, radians, pi
from mathutils import Vector, Euler
# creates flower, based on SN 1 script
# modified for SN2 as a test


class Flower2(SvScriptSimpleGenerator):
    
    inputs  = [
        ['s', 'Nm Petals',  8],
        ['s', 'herts per Petal',  20],
        ['s', 'Profile Radius', 1.3],
        ['s', 'Amp',  1.0],
    ]
    
    outputs = [("v", "Verts", "make_verts"),
                ("s", "Edges", "make_edges")]
    @staticmethod
    def make_verts(n_petals=8, vp_petal=20, profile_radius=1.3, amp=1.0):
        # variables
        z_float = 0.0
        n_verts = n_petals * vp_petal
        section_angle = 360.0 / n_verts
        position = (2 * (pi / (n_verts / n_petals)))

        # consumables
        Verts = []

        # makes vertex coordinates
        for i in range(n_verts):
            # difference is a function of the position on the circumference
            difference = amp * cos(i * position)
            arm = profile_radius + difference
            ampline = Vector((arm, 0.0, 0.0))

            rad_angle = radians(section_angle * i)
            myEuler = Euler((0.0, 0.0, rad_angle), 'XYZ')

            # changes the vector in place, successive calls are accumulative
            # we reset at the start of the loop.
            ampline.rotate(myEuler)
            x_float = ampline.x
            y_float = ampline.y
            Verts.append((x_float, y_float, z_float))
        return Verts
    
    @staticmethod    
    def make_edges(n_petals=8, vp_petal=20, profile_radius=1.3, amp=1.0):
        # makes edge keys, ensure cyclic
        n_verts = n_petals * vp_petal
        Edges = [[i, i + 1] for i in range(n_verts - 1)]
        Edges.append([n_verts-1, 0])
        return Edges
