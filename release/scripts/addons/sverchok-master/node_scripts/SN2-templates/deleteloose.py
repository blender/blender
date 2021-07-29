from itertools import chain

class DeleteLooseVerts(SvScriptSimpleFunction):
    inputs = [("v", "verts"), ("s", "pol")]
    outputs = [("v", "verts"), ("s", "pol")]
    
    @staticmethod
    def function(*args, **kwargs):
        ve, pe = args       
        indx = set(chain.from_iterable(pe))
        v_index = sorted(set(chain.from_iterable(pe)))
        v_out = [ve[i] for i in v_index]
        mapping = dict(((j, i) for i, j in enumerate(v_index)))
        p_out = [tuple(map(mapping.get, p)) for p in pe]
        return v_out, p_out
        
