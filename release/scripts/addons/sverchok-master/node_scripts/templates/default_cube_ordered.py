def sv_main(size=1.0,divxy=1,divz=1):
    
    # in boilerplate - make your own sockets
    in_sockets = [
        ['s', 'Size(float)',  size],
        ['s', 'DivisionsXY(int)', divxy],
        ['s', 'DivisionsZ(int)', divz],
    ]
    
    if divxy<1: divxy=1
    divx = divy = divxy
    if divz<1: 
        divz=1
    if divxy>1 and divz==1: 
        divz=2
    
    criteria = ((divx+1)*(divy+1))
    
    # def to make rows and indeces
    def generate_row(size,dx,dy,z,cup,criteria):
        step_x = size/(dx)
        step_y = size/(dy)
        row = []
        i = 0
        for y in range(dy+1): 
            for x in range(dx+1):
                if cup:
                    row.append([step_x*x,step_y*y,z])
                elif x in [0,dx] and y not in [0,dy]:
                    row.append([step_x*x,step_y*y,z])
                elif x not in [0,dx] and y in [0,dy]:
                    row.append([step_x*x,step_y*y,z])
                elif x in [0,dx] and y in [0,dy]:
                    row.append([step_x*x,step_y*y,z])
                i += 1
        return row
    
    # make vertices + indeces of them
    col = []
    step_z = size/(divz)
    for i in range(divz+1):
        if i == 0 or i == divz:
            cup=True
        else:
            cup=False
        gen = generate_row(size,divx,divy,step_z*i,cup,criteria)
        col.extend(gen)
        
    pol = []
    
    # def to make horisontal planes
    def generate_plane(divx,divy,divz,bottom,height):
        pol=[]
        for y in range(divy):
            for x in range(divx):
                if not bottom:
                    pol.append([
                        x + (y+1)*(divx+1),
                        (x+1) + (y+1)*(divx+1),
                        (x+1) + y*(divx+1),
                        x + y*(divx+1),
                    ])
                else:
                    pol.append([
                        x + y*(divx+1)+height,
                        (x+1) + y*(divx+1)+height,
                        (x+1) + (y+1)*(divx+1)+height,
                        x + (y+1)*(divx+1)+height,
                    ])
        return pol
    
    # to make floor
    pol.extend(generate_plane(divx,divy,divz,False,0))
    # celling
    coef = (divz-1)*(divx-1)*2 \
        + (divz-1)*(divy+1)*2 \
        + criteria
    pol.extend(generate_plane(divx,divy,divz,True,coef))
    
    
    # walls x AB
    # wall x A
    polWal_A=[]
    coef_A = (divx-1)*2 + (divy+1)*2 # xy with z>0 row to skip
    # wall x B
    polWal_B=[]
    coef_B = (divx+1)*divy # z==0 skip to other side
    for r in range(divx):
        polWal_A.append([   r+1,
            r+criteria+1,
            r+criteria,
            r   ])
        polWal_B.append([   r+coef_B,
            r+coef_A+coef_B,
            r+coef_A+coef_B+1,
            r+coef_B+1  ])
        for c in range(divz):
            if c:
                polWal_A.append([   r+criteria+(c-1)*coef_A+1,
                    r+criteria+c*coef_A+1,
                    r+criteria+c*coef_A,
                    r+criteria+(c-1)*coef_A    ])
                if c == divz-1:
                    polWal_B.append([   r+c*coef_A+coef_B,
                        r+c*coef_A+coef_B+criteria,
                        r+c*coef_A+coef_B+1+criteria,
                        r+c*coef_A+coef_B+1    ])
                else:
                    polWal_B.append([   r+c*coef_A+coef_B,
                        r+(c+1)*coef_A+coef_B,
                        r+(c+1)*coef_A+coef_B+1,
                        r+c*coef_A+coef_B+1    ])
                        
    # walls y CD
    # there is error!!! some faces overlap
    # if divx == divy there is no error...
    def generate_wall_CD(ran,left):
        #print(ran)
        polWal=[]
        for i in range((divy+1)*divz):
            if (i+1)%(divx+1): # maybe this condition wrong
                if left:
                    polWal.append([   ran[i],
                        ran[i+divy+1],
                        ran[i+divy+2],
                        ran[i+1]   ])
                else:
                    polWal.append([   ran[i+1],
                        ran[i+divy+2],
                        ran[i+divy+1],
                        ran[i]   ])
        #print(polWal)
        return polWal
    # wall y C
    ran_C = []
    ran_C.extend([a for a in range(0,(divy)*(divx+1)+1,divx+1)])
    for z in range(1, divz):
        ran_C.append(criteria+(z-1)*coef_A)
        ran_C.extend([a for a in range( \
                criteria+(z-1)*coef_A+divx+1, \
                criteria+coef_A*z-divx, \
                2)])
    ran_C.extend([a for a in range( \
            criteria+(divz-1)*coef_A, \
            criteria*2+(divz-1)*coef_A-divx, \
            divx+1)])
    
    polWal_C = generate_wall_CD(ran_C,True)
    
    # wall y D
    ran_D = []
    ran_D.extend([a for a in range(divx,(divy+1)*(divx+1),divx+1)])
    for z in range(1, divz):
        ran_D.extend([a for a in range( \
                criteria+(z-1)*coef_A+divx, \
                criteria+coef_A*z-divx, \
                2)])
        ran_D.append(criteria+coef_A*z-1)
    ran_D.extend([a for a in range( \
            criteria+(divz-1)*coef_A+divx, \
            criteria*2+(divz-1)*coef_A+1, \
            divx+1)])
    #print(ran_D)
    polWal_D = generate_wall_CD(ran_D,False)
            
    pol.extend(polWal_A)
    pol.extend(polWal_B)
    pol.extend(polWal_C)
    pol.extend(polWal_D)
    out=[col]
    # simple cube if divisions are 1
    if divx ==1 and divy ==1 and divz ==1:
        out=[[[-1.0, -1.0, -1.0], [-1.0, 1.0, -1.0], [1.0, 1.0, -1.0], [1.0, -1.0, -1.0], [-1.0, -1.0, 1.0], [-1.0, 1.0, 1.0], [1.0, 1.0, 1.0], [1.0, -1.0, 1.0]]]
        edg=[[[0, 4], [4, 5], [5, 1], [1, 0], [5, 6], [6, 2], [2, 1], [6, 7], [7, 3], [3, 2], [7, 4], [0, 3]]]
        pol=[[4, 5, 1, 0], [5, 6, 2, 1], [6, 7, 3, 2], [7, 4, 0, 3], [0, 1, 2, 3], [7, 6, 5, 4]]
    
    
    # out boilerplate - set your own sockets packet
    out_sockets = [
        ['v', 'Ver', out],
        ['s', 'Pol', [pol]],
    ]

    return in_sockets, out_sockets
