def sv_main(se=[]):

    in_sockets = [
        ['s', 'se', se]]
    out = ''
    if se:
        for s in se[0]:
            out += s[0]
            out += ' '
    out_sockets = [
        ['s', 'out', [[[out]]]]]

    return in_sockets, out_sockets

