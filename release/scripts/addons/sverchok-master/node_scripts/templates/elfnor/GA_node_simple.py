from sverchok.data_structure import Matrix_listing

import LSystem_blender as LSystem
import GA_xml

import imp

max_mats = 5000

def sv_main(rseed=21):

    in_sockets = [['s', 'rseed', rseed]]
       
    imp.reload(GA_xml)
       
    tree = GA_xml.Library["Default"]
    lsys = LSystem.LSystem(tree, max_mats)
    shapes = lsys.evaluate(seed = rseed)
       
    mats = [shape[1] for shape in shapes if shape] 
    mat_out =  Matrix_listing(mats)
     
    out_sockets = [['m', 'matrices', mat_out]]
    
    return in_sockets, out_sockets