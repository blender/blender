from math import sin, cos, radians, pi, sqrt

def sv_main(n=480, scale=1.0, a=1.0, b=1.0, m=3.0, n1=4.5, n2=10.0, n3=10.0):

    in_sockets = [
        ['s', 'Number of points',  n],
        ['s', 'Scale Factor',  scale],
        ['s', 'a',  a],
        ['s', 'b',  b],
        ['s', 'm',  m],
        ['s', 'n1',  n1],
        ['s', 'n2',  n2],
        ['s', 'n3',  n3]
    ]

    step = (2 * pi) / n
    r = 1.0
    phi = 0

    Verts = []
    verts_new = Verts.append

    for i in range(0, n):
        #theta = i * radians(137.5)
        #r = c * sqrt(i)
        c = pow(abs(cos((m*phi) / 4)/a), n2)
        s = pow(abs(sin((m*phi) / 4)/b), n3)
        r = pow((c + s), (1/n1))

        # polar to cartesian
        posx = scale * r * sin(phi)
        posy = scale * r * cos(phi)
                
        verts_new((posx, posy, 0.0))
        phi += step

    out_sockets = [
        ['v', 'Verts', [Verts]]
    ]

    return in_sockets, out_sockets
