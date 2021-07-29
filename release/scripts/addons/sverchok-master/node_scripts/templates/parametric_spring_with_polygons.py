# imports and aliases
from math import sin, cos, pi, atan2, radians
from mathutils import Euler, Vector


def sv_main(n_verts=12, prad=1.2, crad=0.5, n_iter=4, n_turns=7, height=6):

    # in boilerplate, could be less verbose
    in_sockets = [
        ['s', "num_verts",  n_verts],
        ['s', "profile_radius", prad],
        ['s', "coil_radius", crad],
        ['s', "num_iterations_per_turn", n_iter],
        ['s', "num_turns", n_turns],
        ['s', "height", height]
    ]

    

    def make_coil():

        # variables
        amp = prad
        th = height / n_turns
        ipt = n_iter
        radius = crad
        diameter = radius * 2
        section_angle = 360.0 / n_verts
        rad_slice = 2.0 * pi / ipt
        total_segments = (ipt * n_turns) + 1
        z_jump = height / total_segments

        x_rotation = atan2(th / 2, diameter)

        n = n_verts
        Verts = []
        for segment in range(total_segments):
            rad_angle = rad_slice * segment

            for i in range(n):

                # create the vector
                this_angle = section_angle * i
                x_float = amp * sin(radians(this_angle)) + radius
                z_float = amp * cos(radians(this_angle))
                v1 = Vector((x_float, 0.0, z_float))

                # rotate it
                some_euler = Euler((-x_rotation, 0.0, -rad_angle), 'XYZ')
                v1.rotate(some_euler)

                # add extra z height per segment
                v1 += Vector((0, 0, (segment * z_jump)))

                # append it
                Verts.append(v1.to_tuple())

        Faces = []
        for t in range(total_segments - 1):
            for i in range(n - 1):
                p0 = i + (n * t)
                p1 = i + (n * t) + 1
                p2 = i + (n * t + n) + 1
                p3 = i + (n * t + n)
                Faces.append([p0, p1, p2, p3])
            p0 = n * t
            p1 = n * t + n
            p2 = n * t + (2 * n) - 1
            p3 = n * t + n - 1
            Faces.append([p0, p1, p2, p3])

        return Verts, Faces

    verts, Faces = make_coil()

    out_sockets = [
        ['v', 'Vecs', verts],
        ['s', 'Edges', [Faces]]
    ]

    return in_sockets, out_sockets
