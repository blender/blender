import numpy as np
import time

from mathutils import Vector, Matrix    

from sverchok.data_structure import Matrix_listing

def closest_np2(xyz1, xyz2):
    x2 = np.subtract.outer(xyz2[:,0], xyz1[:,0])
    y2 = np.subtract.outer(xyz2[:,1], xyz1[:,1])
    z2 = np.subtract.outer(xyz2[:,2], xyz1[:,2])
    d2 = np.sum((x2**2, y2**2, z2**2), axis=0)
    ci = d2.argmin(axis=1)
    column_i = range(d2.shape[0])
    dout = np.sqrt(d2[column_i, ci])
    v = np.vstack((x2[column_i, ci], y2[column_i, ci], z2[column_i, ci]))    
    return dout, ci, v.T

class SCA:

    def __init__(self, d=0.3, NBP=2000, KILLDIST=5, INFLUENCE=15, endpoints=[], 
                 TROPISM=0.0, max_time=1.0, startpoints=[]):
        self.killdistance = KILLDIST
        self.branchlength = d
        self.maxiterations = NBP
        self.tropism = np.array(TROPISM)
        self.influence = INFLUENCE if INFLUENCE > 0 else 1e16
        self.max_time = max_time
        
        if len(startpoints) > 0:            
            self.bpalln = np.array(startpoints)
        else:
            self.bpalln = np.array([[0, 0, 0]])      
        
        self.bpp = [None] * self.bpalln.shape[0]
        self.bpc = [0] * self.bpalln.shape[0]
        self.bpg = [0] * self.bpalln.shape[0]
            
        self.epn = np.array(endpoints)        
        d, ci, v = closest_np2(self.bpalln, self.epn)
        self.epbn = ci
        self.epdn = d
        self.epvn = v / self.epdn.reshape((-1, 1))
        self.epbn[self.epdn >= self.influence] = -2
        
        
    def addBranchPoint(self, bpn, pi, generation):
        self.bpalln = np.append(self.bpalln, [bpn], axis=0)

        self.bpp.append(pi)
        self.bpc.append(0)
        #self.bpg.append(generation + 1)
        self.bpg = np.append(self.bpg, [generation+1])
        self.bpc[pi] += 1  
        bi = self.bpalln.shape[0] - 1
        v = self.epn - bpn
        d2 = (v**2).sum(axis=1)
        index = (self.epbn != -1) & (d2 < self.epdn**2) & (d2 > self.killdistance**2)
         
        d  = np.sqrt(d2[index])
        self.epvn[index] = v[index, :] / d.reshape((-1,1))
        self.epdn[index] = d
        
        index2 = (index & (d2 < self.influence**2))               
        self.epbn[index2] = bi
        
        index3 = (index & (d2 >= self.influence**2))                
        self.epbn[index3] = -2
        
        index4 = (self.epbn != -1) & (d2 < self.epdn**2) & (d2 <= self.killdistance**2)                
        self.epbn[index4] = -1    
               
        if self.bpc[pi] > 1:  # a branch point with two children will not grow any new branches ...
            index_e = (self.epbn == pi)
            index_b = (np.array(self.bpc) <= 1)
            d, c, v = closest_np2(self.bpalln[index_b], self.epn[index_e])
            # this turns c indixes into self.bpalln[index_b] into u indices into self.bpalln
            #needs better variable names
            t = np.arange(self.bpalln.shape[0])[index_b]
            u = t[c]
            # set points not within influence distance to -2 so they will be
            # ignored in growBranches
            u[d >= self.influence] = -2

            self.epdn[index_e] = d
            self.epbn[index_e] = u            
            self.epvn[index_e] = v / d.reshape((-1,1))    
                         
    def growBranches(self, generation):
        index = self.epbn >= 0        
        epbn = self.epbn[index]
        bis = np.unique(epbn)
        
        v_sums = np.empty((bis.shape[0], 3))
        for col in range(3):
            v_sums[:, col] = np.bincount(epbn, weights=self.epvn[index, col])[bis]
#        d2 = (v_sums**2).sum(axis=1)
#        d = np.sqrt(d2) /self.branchlength
#        vd = v_sums / d.reshape((-1, 1))

        n_hat = v_sums/(((v_sums**2).sum(axis=1))**0.5).reshape((-1,1))
        n_tilde = (n_hat + self.tropism)
        n_tilde = n_tilde/(((n_tilde**2).sum(axis=1))**0.5).reshape((-1,1))
       
        newbps = self.bpalln[bis] + n_tilde * self.branchlength
        newbpps = bis        
                
        for newbp, newbpp in zip(newbps, newbpps):
            self.addBranchPoint(newbp, newbpp, generation)
                        
    def iterate(self):
        t0 = time.time()
        for i in range(self.maxiterations):
            nbp = self.bpalln.shape[0]    
            self.growBranches(i)
            if self.bpalln.shape[0] == nbp:
                return
            if (time.time() - t0) > self.max_time:
                print('SCA timed out')
                return
        return

    def bp_verts_edges_n(self):
        """
        returns branchpoints verts as a list of positions
        and edges as index to connect the branch points
        and leaves matrices 
        """
        verts = []
        edges = []
        ends = []
        ends_inds = []
        for i, b in enumerate(self.bpalln):
            bp_parent = self.bpp[i]
            verts.append(list(b))
            if bp_parent != None:
                edges.append((bp_parent, i))
            if self.bpc[i] == 0:
                ends.append(True)
                ends_inds.append(i)
            else:
                ends.append(False)  
        process = ends_inds
        # branch radii
        br = [int(t) for t in ends]
        finished = []
        while len(process) > 0:
            process.sort()
            i = process.pop()
            finished.append(i)
            p = self.bpp[i]
            if p != None:
                br[p] = br[p] + br[i]
                if p not in process:
                    if p not in finished:
                        process.insert(0, p)    
                        
        mats= []
        for edge in edges:           
            if ends[edge[1]]:
                #calculate leaf directions
                #end will always be edge[1]
                v0 = Vector(verts[edge[0]])
                v1 = Vector(verts[edge[1]])
                dir1 = (v1 - v0).normalized()
                dir2 = (dir1.cross(Vector((0.0, 0.0, 1.0)))).normalized()               
                dir3 = -(dir1.cross(dir2)).normalized() 
                m = Matrix.Identity(4)
                m[0][0:3] = dir1
                m[1][0:3] = dir2
                m[2][0:3] = dir3
                m[3][0:3] = v1
                m.transpose()
                mats.append(m)

        mats_out =  Matrix_listing(mats)
     
        return verts, edges, ends, br, mats_out
        
def sv_main(npoints=100 , dist=0.05, min_dist=0.05, max_dist=2.0, tip_radius=0.01, trop=[], verts_in=[], verts_start=[]):

    in_sockets = [
        ['s', 'maximum branches', npoints],
        ['s', 'branch length', dist],
        ['s', 'minimum distance', min_dist],
        ['s', 'maximum distance', max_dist],
        ['s', 'tip radius', tip_radius], 
        ['v', 'tropism', trop],
        ['v', 'End Vertices',  verts_in],
        ['v', 'Start Vertices', verts_start]
    ]
    verts_out = []
    edges_out = []
    rad_out = []
    ends_out = []
    mats_out = []
    if not verts_start:
        verts_start = [[]]
    if not trop:
        trop = [0., 0., 0.]    
        
    if verts_in :
        sca = SCA(NBP = npoints,
                  d=dist,
                  KILLDIST=min_dist,
                  INFLUENCE=max_dist, 
                  TROPISM=trop[0],
                  endpoints=verts_in[0],
                  startpoints = verts_start[0])

        sca.iterate()
        verts_out, edges_out, ends_out, br, mats_out = sca.bp_verts_edges_n()
        rad_out = [tip_radius*b**0.5 for b in br]
        
    out_sockets = [
        ['v', 'Vertices', [verts_out]],
        ['s', 'Edges', [edges_out]],
        ['s', 'Branch radii', [rad_out]],
        ['s', 'Ends mask', [ends_out]],
        ['m', 'Leaf matrices', mats_out],
    ]

    return in_sockets, out_sockets
