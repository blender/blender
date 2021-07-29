"""
in   datain    s   .=[]   n=0
in   level     s   .=1    n=2
out  dataout   s
"""

from numpy import array as ar

Data = ar(datain)
if level < Data.ndim and level > 0:
    Data = Data.swapaxes(level,level-1)
    print('swap axes ',Data)
dataout = Data.tolist()
