def sv_main(void=[]):

    in_sockets = [
        ['v', 'void', void],
        
        ]
    
    selout=[]
    seloutactiv=[]
    
    for i in bpy.context.selected_objects:
        selout.append(i)
    
    seloutactiv.append(bpy.context.active_object)
    
    
    
    
    
    
    
    
    out_sockets = [
        ['v', 'selected', [selout] ],
        ['v', 'selected_Active', [seloutactiv] ],
     #   ['v', 'Group_with_Active', [] ]
        
    ]
    
    return in_sockets, out_sockets