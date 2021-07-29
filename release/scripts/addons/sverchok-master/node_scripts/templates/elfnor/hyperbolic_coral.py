from math import pi, cos, sin
import random

#get code right then use list comprehensions etc to tidy up
#its pretty gruesome for now

def sv_main(p1=2, p2 =3, n1 = 4, nrings=5, radius=1.0, spacing=1.0):

    in_sockets = [['s', 'p1', p1],
                  ['s', 'p2', p2],
                  ['s', 'n points', n1],
                  ['s', 'n rings', nrings],
                  ['s', 'r spacing', radius],
                  ['s', 't spacing', spacing]]
    
    edges_out = []
    verts_out = [] 
    faces_out = []

    pat = [p1, p2]

    verts_out.append([0.0,0.0,0.0])
    nverts = n1
    vID = 1
    rings_list = [[0]]
    #vertices and ring edges
    for ring in range(1,nrings+1):        
        ring_radius =  ring * radius
        plane_spacing= (2 * pi * ring_radius)/nverts
        zh = 0
        if spacing > plane_spacing:
            zh = (spacing**2 - plane_spacing**2)**0.5
         
        theta = (2*pi)/nverts
        #make edge saw tooth 
        izh = [(i, (1 - abs(i % 4 - 1))*zh ) for i in range(nverts)] 
        verts = [ [ring_radius*cos(i*theta), ring_radius*sin(i*theta), zi] for i, zi in izh ] 
        verts_out.extend(verts)

        rings_list.append(list(range(vID, vID+nverts))) 
        vID = vID + nverts   
        #ring edges
        edges = [(v, v + 1) for v in rings_list[ring][:-1] ]
        edges_out.extend(edges)
        edges_out.append((rings_list[ring][-1], rings_list[ring][0]))
        
        # radial edges and faces
        if ring == 1:
           edges = [(r, 0) for  r in rings_list[ring] ]
           edges_out.extend(edges)
           faces = [(r, r + 1, 0) for r in rings_list[ring][:-1] ]
           faces_out.extend(faces)
           faces_out.append( (rings_list[ring][-1], rings_list[ring][0], 0) )
        else:
            vc = []
            for i,v in enumerate(rings_list[ring-1]):
                vc.extend([v]*pat[i%2])
            edges = list(zip(rings_list[ring], vc))
            edges_out.extend(edges)
            
            for i, e in enumerate(edges[:-1]):
                face = [ e[1], e[0], edges[i+1][0] ]
                if edges[i+1][1] != edges[i][1]:
                    face.append(edges[i+1][1] )
                faces_out.append(face) 
                
            face = [ edges[-1][1], edges[-1][0], edges[0][0] ]
            if edges[0][1] != edges[-1][1]:
               face.append(edges[0][1] )
            faces_out.append(face)
   
        nverts = int(nverts * (p1 + p2)/2)
        
    out_sockets = [
        ['v', 'Verts',  verts_out],
        ['s', 'Edges', edges_out], 
        ['s', 'Faces', faces_out], 
    ]

    return in_sockets, out_sockets
