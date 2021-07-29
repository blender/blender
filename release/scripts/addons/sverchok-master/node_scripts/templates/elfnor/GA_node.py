from sverchok.data_structure import Matrix_listing, fullList

import LSystem_blender as LSystem
import GA_xml

import imp

imp.reload(LSystem)

def flat(*args):
    for x in args:
        if hasattr(x, '__iter__'):
            for y in flat(*x):
                yield y
        else:
            yield x

def sv_main(rseed=21, max_mats=5000, vars = [], verts=[]):

    in_sockets = [['s', 'rseed', rseed],
                  ['s', 'max matrices', max_mats], 
                  ['s', 'variables', vars],    
                  ['v', 'Vertices', verts]]
       
    imp.reload(GA_xml)
       
    tree = GA_xml.Library["Default"]
    
    nvars = tree.count('{') #this may be too large because of repeats

    
    #flatten vars
    if vars: 
        vars = list(flat(vars))
        fullList(vars, nvars)
    else: 
        vars = [0]*nvars
           
    tree = tree.format(*vars)
            
    lsys = LSystem.LSystem(tree, max_mats)
    shapes = lsys.evaluate(seed = rseed)
       
    names = [shape[0] for shape in shapes if shape] 
    #convert names to integer list
    iddict = {k:v for v,k in enumerate(set(names))}   
       
    mat_sublist = []
    mat_list  = []     
    mask = [] 
    mask_sub = []               
    for i, shape in enumerate(shapes):
        if shape:
            mat_sublist.append(shape[1])
            mask_sub.append(iddict[shape[0]])
        else: 
            if len(mat_sublist) > 0:
                mat_list.append(Matrix_listing(mat_sublist))
                mask.append(mask_sub)
            mat_sublist = []
            mask_sub = []

    edges_out = []
    verts_out = [] 
    faces_out = []             
    if verts:
        #make tubes                   
        for sublist in mat_list:
            v, e, f = lsys.make_tube(sublist, verts)
            if v:
                verts_out.append(v)
                edges_out.append(e)
                faces_out.append(f)
 
    mat_out =  mat_list
     
    out_sockets = [
        ['m', 'matrices', mat_out],
        ['s', 'mask', mask],
        ['v', 'Vertices', verts_out],
        ['s', 'Edges', edges_out],
        ['s', 'Faces', faces_out]]
    
    return in_sockets, out_sockets