"""
in   verts  v   .=[]   n=0
in   faces  s   .=[]   n=0
in   scale  s   .=0.0  n=0
in   itera  s   .=1    n=0
in   multi  s   .=-1.0   n=0
out  overts   v
out  ofaces   s
"""

from mathutils import Vector as V
from mathutils.geometry import normal as nm
import numpy as np

overts = verts
ofaces = faces
itera = min(max(itera,1),4)
multi = min(max(multi,-1.0),1.0)

def iteration(vers,facs,scal):
    overts = []
    ofaces = []
    for ov, of in zip(vers, facs):
        lv = len(ov)
        overts_ = ov
        ofaces_ = []
        fcs = []
        for f in of:
            vrts = [ov[i] for i in f]
            norm = nm(V(ov[f[0]]),V(ov[f[1]]),V(ov[f[2]]))
            nv  = np.array(vrts)
            vrt  = (nv.sum(axis=0)/len(f))+np.array(norm*scal)
            fcs = [[i,k,lv] for i,k in zip(f,f[-1:]+f[:-1])]
            overts_.append(vrt.tolist())
            ofaces_.extend(fcs)
            lv += 1
        overts.append(overts_)
        ofaces.append(ofaces_)
    return overts, ofaces, scale*multi

for i in range(itera):
    overts,ofaces,scale = iteration(overts,ofaces,scale)