from itertools import chain, repeat, zip_longest


# the class based should be slower but kept until tested
class SvZipExhausted(Exception):
    pass

class SvSentinel:
    def __init__(self, fl, top):
        self.fl = fl
        self.top = top
        self.done = False

    def __next__(self):
        if self.done:
            raise StopIteration
        self.top.counter -= 1
        if not self.top.counter:
            raise SvZipExhausted
        self.done = True
        return self.fl

    def __iter__(self):
        return self

class sv_zip_longest:
    def __init__(self, *args):
        self.counter = len(args) 
        self.iterators = []
        for lst in args:
            fl = lst[-1]
            filler = repeat(fl)
            self.iterators.append(chain(lst, SvSentinel(fl,self), filler))

    def __next__(self):
        try:    
            if self.counter:
                return tuple(map(next, self.iterators))
            else:
                raise StopIteration
        except SvZipExhausted:
            raise StopIteration

    def __iter__(self):
        return self    


def sv_zip_longest2(*args):
    # by zeffi
    longest = max([len(i) for i in args])
    itrs = [iter(sl) for sl in args]
    for i in range(longest):
        yield tuple((next(iterator, args[idx][-1]) for idx, iterator in enumerate(itrs)))
        
        
def recurse_fx(l, f):
    if isinstance(l, (list, tuple)):
        return [recurse_fx(i, f) for i in l]
    else:
        return f(l)

def recurse_fxy(l1, l2, f):
    l1_type = isinstance(l1, (list, tuple))
    l2_type = isinstance(l2, (list, tuple)) 
    if not (l1_type or l2_type):
        return f(l1, l2)            
    elif l1_type and l2_type:
        fl = l2[-1] if len(l1) > len(l2) else l1[-1]
        res = []
        res_append = res.append
        for x, y in zip_longest(l1, l2, fillvalue=fl):
            res_append(recurse_fxy(x, y, f))
        return res
    elif l1_type and not l2_type:
        return [recurse_fxy(x, l2, f) for x in l1]
    else: #not l1_type and l2_type
        return [recurse_fxy(l1, y, f) for y in l2]
