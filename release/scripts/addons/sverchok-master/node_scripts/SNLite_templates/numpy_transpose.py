"""
in   datain    s   .=[]   n=0
in   level     s   .=1    n=0
out  dataout   s
"""

import numpy as np
from numpy import array as ar

Data = ar(datain)
print(Data.shape)
shape = np.roll(ar([i for i in range(Data.ndim-1)]), level)
shape = np.append(shape,ar([Data.ndim-1])).tolist()
Data = np.transpose(Data, shape)

dataout = Data.tolist()