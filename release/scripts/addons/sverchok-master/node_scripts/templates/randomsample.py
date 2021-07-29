import random

# Sample a list
# For random select from a list, useful as input into
# List Item Node, items socket.

def sv_main(l=10,s=0,c=3):
    in_sockets = [
       ['s', 'Length', l],
       ['s', 'Seed', s],
       ['s', 'Count', c],
    ]
    # always set seed when with rand
    random.seed(s)
    items=random.sample(range(l), c)

    out_sockets = [
        ['s', 'Rand', [items]]
    ]

    return in_sockets, out_sockets
