# not finished yet
# will create circles field in what circles will fill maximum space

from math import sin, cos, radians, pi, sqrt
from mathutils import Vector, Euler
from random import random
'''
x1 = []
y1 = []
z1 = []
r1 = []

x1.append(4)
y1.append(0.2)#* 2)
z1.append(1.8)
r1.append(0.1)

x = 2
y = 0.5
z = 3.4
r = 0.01
 
a = True

rand_list = ["M1","M2","M3","M4","M5"]
color = "M1"


add_sphere(location=(x,y,z))

###################################
# change spheres count - 2000
for i in range(2000):
    for j in range(0,len(x1),1):
        if x >= x1[j] and y >= y1[j] and z >= z1[j]:
            d = math.pow(x - x1[j], 2) + math.pow(y - y1[j], 2) + math.pow(z - z1[j], 2)
        if x < x1[j] and y >= y1[j] and z >= z1[j]:
            d = math.pow(x1[j] - x, 2) + math.pow(y - y1[j], 2) + math.pow(z - z1[j], 2)
        if x >= x1[j] and y < y1[j] and z >= z1[j]:
            d = math.pow(x - x1[j], 2) + math.pow(y1[j] - y, 2) + math.pow(z - z1[j], 2)
        if x >= x1[j] and y >= y1[j] and z < z1[j]:
            d = math.pow(x - x1[j], 2) + math.pow(y - y1[j], 2) + math.pow(z1[j] - z, 2)
        if x < x1[j] and y < y1[j] and z >= z1[j]:
            d = math.pow(x1[j] - x, 2) + math.pow(y1[j] - y, 2) + math.pow(z - z1[j], 2)
        if x >= x1[j] and y < y1[j] and z < z1[j]:
            d = math.pow(x - x1[j], 2) + math.pow(y1[j] - y, 2) + math.pow(z1[j] - z, 2)
        if x < x1[j] and y >= y1[j] and z < z1[j]:
            d = math.pow(x1[j] - x, 2) + math.pow(y - y1[j], 2) + math.pow(z1[j] - z, 2)
        if x < x1[j] and y < y1[j] and z < z1[j]:
            d = math.pow(x1[j] - x, 2) + math.pow(y1[j] - y, 2) + math.pow(z1[j] - z, 2)
        print(d)
        if math.pow(r + r1[j], 2) >= d:
            print("intersect")
            a = False
        else:
            print("not intersect")
    if a:
        if x - r >= 1 and x + r <= 5 and y - r >= 0 and y + r <= 1 \
                      z - r >= 1 and z + r <= 5:
            add_sphere(size=(r), location=(x,y,z))
            x1.append(x)
            y1.append(y)
            z1.append(z)
            r1.append(r)
    a = True
    x = random.random() * 5
    y = random.random() # * 5 
    z = random.random() * 5
    ###################################
    # change sphere radius
    r = random.uniform(0.01, 0.5)
'''

def sv_main(v=[],c=3):

    in_sockets = [
        ['v', 'vertices',  v],
        ['s', 'circle',  c],
    ]

    if v:
        max_ = max([i[0] for i in v])
        mix_ = min([i[0] for i in v])
        may_ = max([i[1] for i in v])
        miy_ = min([i[1] for i in v])
        area = (max_-mix_)*(may_-miy_)
        radiuses = [0.2]
        points = [[0,0,0]]
        badpoints=[]
        for i in range(10):
            newarea = area * pow(i, 1/c)
            rad = sqrt(newarea/pi)
            t=True
            print("defined radius", rad)
            while t:
                randx=random()*(max_-mix_)
                randy=random()*(may_-miy_)
                for u, p in enumerate(points):
                    if (p[0] - randx)**2 + (p[1] - randy)**2 < (radiuses[u]**2)+rad:
                        t=False
                    else:
                        print('bad point')
                        badpoints.append([randx+mix_,randy+miy_,0])
                #print('check circles', rad)
            radiuses.append(rad)
            points.append([randx+mix_,randy+miy_,0])
            print('one circle', str([randx+mix_,randy+miy_,0]))
    else:
        points=[]
        radiuses=[]
        badpoints=[]
    out_sockets = [
        ['v', 'centers', [points]],
        ['s', 'radiuses', [radiuses]],
        ['v', 'bads', [badpoints]],
    ]

    return in_sockets, out_sockets
