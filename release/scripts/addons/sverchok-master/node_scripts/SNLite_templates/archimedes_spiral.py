"""
in arc         s  d=1.0   n=2
in separation  s  d=3.0   n=2
in num_points  s  d=300   n=2
in scale       s  d=0.13  n=2
out verts      v
"""

import math

def spiral_points(scale, arc=1, separation=1):
    """
    lifted directly from: 
    http://stackoverflow.com/a/27528612/1243487 
    by user: liborm

    generate points on an Archimedes' spiral
    with `arc` giving the length of arc between two points
    and `separation` giving the distance between consecutive 
    turnings
    - approximate arc length with circle arc at given distance
    - use a spiral equation r = b * phi
    """
    def p2c(r, phi):
        """polar to cartesian
        """
        return (scale * r * math.cos(phi), scale * r * math.sin(phi), 0)

    # yield a point at origin
    # not using the origin may give better results
    yield (0, 0, 0) 

    # initialize the next point in the required distance
    r = arc
    b = separation / (2 * math.pi)
    # find the first phi to satisfy distance of `arc` to the second point
    phi = float(r) / b
    while True:
        yield p2c(r, phi)
        # advance the variables
        # calculate phi that will give desired arc length at current radius
        # (approximating with circle)
        phi += float(arc) / r
        r = b * phi


point_gen = spiral_points(scale, arc, separation)

verts = [[]]
point = verts[0].append
for i in range(num_points):
    point(next(point_gen))
