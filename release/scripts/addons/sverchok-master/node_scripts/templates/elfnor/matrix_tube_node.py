import mathutils as mu

def make_tube(mats, verts):
        """
        takes a list of vertices and a list of matrices
        the vertices are to be joined in a ring, copied and transformed by the 1st matrix 
        and this ring joined to the previous ring.

        The ring dosen't have to be planar.
        outputs lists of vertices, edges and faces
        """
        edges_out = []
        verts_out = [] 
        faces_out = []
        vID = 0
        #print('len mats', len(mats))
        #print(mats[0])
        nring = len(verts[0])
        #end face
        faces_out.append(list(range(nring)))
        for i,m in enumerate(mats):
            for j,v in enumerate(verts[0]):
                vout = mu.Matrix(m) * mu.Vector(v)
                verts_out.append(vout.to_tuple())
                vID = j + i*nring
                #rings
                if j != 0:
                    edges_out.append([vID, vID - 1])
                else: 
                    edges_out.append([vID, vID + nring-1]) 
                #lines
                if i != 0:
                    edges_out.append([vID, vID - nring]) 
                    #faces
                    if j != 0:
                        faces_out.append([vID, vID - nring, vID - nring - 1, vID-1,])
                    else:
                        faces_out.append([vID, vID - nring,  vID-1, vID + nring-1])
        #end face
        #reversing list fixes face normal direction keeps mesh manifold
        f = list(range(vID, vID-nring, -1))
        faces_out.append(f)
        return verts_out, edges_out, faces_out



def sv_main(mats=[], verts = []):

    in_sockets = [
        ['m', 'Matrices',  mats],
        ['v', 'Vertices', verts]
    ]
    
    edges_out = []
    verts_out = [] 
    faces_out = []

    if verts and mats:
        verts_out, edges_out, faces_out = make_tube(mats, verts)
        
    out_sockets = [['v', 'Vertices', verts_out],
                   ['s', 'Edges', edges_out],
                   ['s', 'Faces', faces_out]]

    return in_sockets, out_sockets