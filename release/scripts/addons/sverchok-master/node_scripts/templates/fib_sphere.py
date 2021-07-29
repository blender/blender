import math, random

def fibonacci_sphere(samples, rseed):
    # http://stackoverflow.com/a/26127012/1243487
    rnd = 1.
    random.seed(rseed)
    rnd = random.random() * samples

    points = []
    offset = 2./samples
    increment = math.pi * (3. - math.sqrt(5.));

    for i in range(samples):
        y = ((i * offset) - 1) + (offset / 2);
        r = math.sqrt(1 - pow(y,2))

        phi = ((i + rnd) % samples) * increment

        x = math.cos(phi) * r
        z = math.sin(phi) * r

        points.append([x,y,z])

    return points

def sv_main(samples=400, rseed=4):
    verts_out = []

    in_sockets = [
        ['s', 'samples', samples],
        ['s', 'rseed', rseed]
       ]

    out_sockets = [['v', 'verts', [verts_out]]]
    
    p = fibonacci_sphere(samples, rseed)
    verts_out.extend(p)

    return in_sockets, out_sockets
