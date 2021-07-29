def sv_main(n=[]):
    
    '''use note node to define toe
    l - left, r - right, t - top,
    bake, convert to curve and
    extrude 0.3
    Start always from l finish with t
    and finish inside out
    use live_curve script 
    to materialize curve to tie
    Nikitron, 2014'''
    
    in_sockets = [
        ['s', 'n', n]]
        
    out = [ 
            (0, 0.2, -5),
            (0, 0.2, -3),
            (0, 0.2, 0),
            (1, 0.2, 1),
            (3, 5, 2),
            (0, 7, 2),
            (-3, 5, 2),
            (-1, 0.5, 1) ]
    front = True
    
    vecs = [(0.8660254037844386, 0.0, -0.5),
            (0,0,1),
            (-0.8660254037844386, 0.0, -0.5)]
    if n:
        for k, i in enumerate(n[0]):
            if i[0] == 'l':
                i = 0
            elif i[0] == 't':
                i = 1
            elif i[0] == 'r':
                i = 2
            v = vecs[i]
            if front:
                front = False
                y1 = -0.015
                y2 = 0.015
            else:
                front = True
                y1 = 0.015
                y2 = -0.015
            out.append([v[0]*k*0.05+v[0],
                        y1*(k+1),
                        v[2]*k*0.05+v[2]])
            out.append([v[0]*k*0.05+v[0],
                        y2*(k+1),
                        v[2]*k*0.05+v[2]])
        out.extend([
                (0, -0.4,-1),
                (0, -0.2,-3),
                (0, 0,-10)])

    out_sockets = [
        ['v', 'out', [out]]]

    return in_sockets, out_sockets


