"""
in vers         v  d=[]    n=0
in pols         s  d=[]    n=0
in mats         m  d=[]    n=0
out versout      v
out polsout      s
"""

import mathutils

def doit(vers,pols,mats):
    # 1. размножаем матрицы
    # 2. присваиваем матрицы
    # 3. группируем объекты
    
    """making duplis along matrices with individual objects consistency"""
    
    out = []
    outpols = []
    oa = out.append
    op = outpols.append
    #print([(i,k) for i,k in zip(vers,pols)])
    for (v,p) in zip(vers,pols):
        out_ = []
        oa_ = out_.extend
        outpols_ = []
        op_ = outpols_.extend
        o = 0
        plen = len(v)
        for m in mats:
            oa_([(mathutils.Matrix(m)*mathutils.Vector(i))[:] for i in v])
            op_([[k+plen*o for k in i] for i in p])
            o += 1
        oa(out_)
        op(outpols_)
    return out, outpols
            

versout, polsout = doit(vers,pols,mats)
