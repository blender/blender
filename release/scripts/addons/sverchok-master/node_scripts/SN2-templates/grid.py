import numpy 
import itertools

class GridGen(SvScriptSimpleGenerator):
    inputs = [("s", "Size", 10.0),
              ("s", "Subdivs", 10)]
    outputs = [("v", "verts", "make_verts"),
               ("s", "edges", "make_edges")]
    
    @staticmethod
    def make_verts(size, sub):
        # this is needed
        side = numpy.linspace(-size / 2, size / 2, sub)
        return tuple((x,y,0) for x,y in itertools.product(side, side))
        
    @staticmethod
    def make_edges(size, sub):
        edges = []       
        for i in range(sub):
            for j in range(sub - 1):
                edges.append((sub*i+j, sub*i+j+1))
                edges.append((sub*j+i, sub*j+i+sub))
        return edges
