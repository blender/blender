
def sv_main(verts_p=[], edges_p=[], verts_t=[], edges_t=[]):

    in_sockets = [
        ['v', 'verts_p', verts_p],
        ['s', 'edges_p', edges_p],
        ['v', 'verts_t', verts_t],
        ['s', 'edges_t', edges_t]]

    verts_out = []
    faces_out = []

    def out_sockets():
        return [
            ['v', 'verts_out', verts_out],
            ['s', 'faces_out', faces_out]]

    if not all([verts_p, edges_p, verts_t, edges_t]):
        return in_sockets, out_sockets()

    # paradigm change
    verts_out = [[(0,0,0), (1,0,0), (1,1,0)]]
    faces_out = [[[0,1,2]]]

    return in_sockets, out_sockets()
