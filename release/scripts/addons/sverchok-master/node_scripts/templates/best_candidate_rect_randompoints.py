import numpy as np
import math
import random
from random import random, seed

def poisson_disc_select(img_width, img_hight, r, n_try):
    """
    
    this function is with minor modicifactions a copy of:
    BSD License
    http://nbviewer.ipython.org/github/HyperionAnalytics/
    PyDataNYC2014/blob/master/poisson_disc_sampling.ipynb
    
    
    Select points from Poisson disc
    Input:
    img_width - integer, 1 to n
    img_hight - integer, 1 to n
    r - minimum didtance between two points, float
    n_try - number of randomly sampled points per try, integer, 1 - n
    Output:
    sample_pts_array - floats array, shape[img_width*img_hight, 2]
    """

    r_square = r**2
    A = 3*r_square
    cell_size = r/math.sqrt(2)
    grid_width = int(math.ceil(img_width/cell_size))
    grid_hight = int(math.ceil(img_hight/cell_size))
    grid = [None]*grid_width*grid_hight
    queue = list()
    queue_size = 0
    sample_size = 0
    
    def distance(x, y):
        x_idx = int(x/cell_size)
        y_idx = int(y/cell_size)
        x0 = max(x_idx-2, 0)
        y0 = max(y_idx-2, 0)
        x1 = min(x_idx+3, grid_width)
        y1 = min(y_idx+3, grid_hight)
        
        for w in range(y0, y1):
            p = w*grid_width
            for h in range(x0, x1):
                if grid[p+h]:
                    s = grid[p+h]
                    dx = s[0]-x
                    dy = s[1]-y
                    if dx**2 + dy**2 < r_square:
                        return False
        return True
    
    def set_point(x, y):
        nonlocal queue, grid, queue_size, sample_size
        s = [x, y]
        queue.append(s)
        grid[grid_width*int(y/cell_size) + int(x/cell_size)] = s;
        queue_size += 1
        sample_size += 1
        return s
    
    # Set first data point
    if sample_size == 0:
        x = random()*img_width
        y = random()*img_hight
        set_point(x, y)
        
    while queue_size:
        x_idx = int(random()*queue_size)
        s = queue[x_idx]
        
        # Generate random point in annulus [r, 2r]
        for y_idx in range(0, n_try):
            a = 2*math.pi*random()
            b = math.sqrt(A*random() + r_square)
            
            x = s[0] + b*math.cos(a)
            y = s[1] + b*math.sin(a)
            
            # Set point if farther than r from any other point
            if 0 <= x and x < img_width and 0 <= y and y < img_hight and distance(x, y):
                set_point(x, y)
                
        del queue[x_idx]
        queue_size -= 1
                
    sample_pts = list(filter(None, grid))

    sample_pts_array = np.asfarray(sample_pts)
    N = sample_pts_array.shape[0]
    padded_pts = np.c_[ sample_pts_array, np.zeros(N) ] 
    
    return padded_pts.tolist()



def sv_main(height=4.0, width=3.0, radius=0.23, retries=20, mseed=0):
    verts_out = []

    in_sockets = [
        ['s', 'height', height],
        ['s', 'width', width],
        ['s', 'radius', radius],
        ['s', 'retries', retries],
        ['s', 'seed', mseed]
    ]

    out_sockets = [
        ['v', 'verts', [verts_out]]
    ]

    seed(mseed)
    pts_poisson = poisson_disc_select(height, width, radius, retries)
    verts_out.extend(pts_poisson)


    return in_sockets, out_sockets
