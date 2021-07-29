def sv_main(index_set=[[]],stream=[[]]): 
    in_sockets = [
        ['s', 'Set',  index_set],
        ['s', 'Indexes', stream ],
    ]
    masks = []
    #print(index_set)
    if index_set[0]:# check that we have input
        for i_set,st in zip(index_set[0],stream[0]):
            checks = set(i_set)
            res = [s in checks for s in st]
            masks.append(res)
            
    out_sockets = [
        ['s', 'Mask', masks],
    ]
    return in_sockets, out_sockets
