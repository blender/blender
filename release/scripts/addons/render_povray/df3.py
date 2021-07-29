################################################################################
#
# df3.py
#
# Copyright (C) 2005 Mike Kost <contact@povray.tashcorp.net>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#
################################################################################
#
# Creation functions
#     __init__(x=1, y=1, z=1) : default constructor
#     clone(indf3) : make this df3 look like indf3
#
# Info functions
#     sizeX(): returns X dimention
#     sizeY(): returns Y dimention
#     sizeZ(): returns Z dimention
#
# Scalar functions
#     mult():
#     add():
#     max(): returns highest voxel value in df3
#     min(): returns lowest voxel value in df3
#
# Vector functions
#
# Import/Export functions
#     exportDF3():
#     importDF3():
#
################################################################################

import struct
import os
import stat
import array
import sys

# -+-+-+- Start df3 Class -+-+-+-

class df3:
    __version__ = '0.2'

    __arraytype__ = 'f'

    __struct4byte__  = '>I'
    __struct2byte__  = '>H'
    __struct2byte3__ = '>HHH'
    __struct1byte__  = '>B'
    __array4byte__   = 'I'
    __array2byte__   = 'H'
    __array1byte__   = 'B'

    def __init__(self, x=1, y=1, z=1):
        self.maxX = x
        self.maxY = y
        self.maxZ = z
        self.voxel = self.__create__(x, y, z)

    def clone(self, indf3):
        self.voxel = array.array(self.__arraytype__)
        for i in range(indf3.sizeX()*indf3.sizeY()*indf3.sizeZ()):
            self.voxel[i] = indf3.voxel[i]
        return self

    #### Info Functions

    def sizeX(self):
        return self.maxX

    def sizeY(self):
        return self.maxY

    def sizeZ(self):
        return self.maxZ

    def size(self):
        tmp = []
        tmp.append(self.sizeX())
        tmp.append(self.sizeY())
        tmp.append(self.sizeZ())
        return tmp

    #### Voxel Access Functions

    def get(self, x, y, z):
        return self.voxel[self.__voxa__(x,y,z)]

    def getB(self, x, y, z):
        if (x > self.sizeX() or x < 0): return 0
        if (y > self.sizeX() or y < 0): return 0
        if (z > self.sizeX() or z < 0): return 0

        return self.voxel[self.__voxa__(x,y,z)]

    def set(self, x, y, z, val):
        self.voxel[self.__voxa__(x,y,z)] = val

    def setB(self, x, y, z, val):
        if (x > self.sizeX() or x < 0): return
        if (y > self.sizeX() or y < 0): return
        if (z > self.sizeX() or z < 0): return

        self.voxel[self.__voxa__(x,y,z)] = val

    #### Scalar Functions

    def mult(self, val):
        for i in range(self.sizeX()*self.sizeY()*self.sizeZ()):
            self.voxel[i] = self.voxel[i] * val

        return self

    def add(self, val):
        for i in range(self.sizeX()*self.sizeY()*self.sizeZ()):
            self.voxel[i] = self.voxel[i] + val

        return self

    def max(self):
        tmp = self.voxel[0]

        for i in range(self.sizeX()*self.sizeY()*self.sizeZ()):
            if (self.voxel[i] > tmp):
                tmp = self.voxel[i]

        return tmp

    def min(self):
        tmp = self.voxel[0]

        for i in range(self.sizeX()*self.sizeY()*self.sizeZ()):
            if (self.voxel[i] < tmp):
                tmp = self.voxel[i]

        return tmp

    #### Vector Functions

    def compare(self, indf3):
        if (self.__samesize__(indf3) == 0): return 0

        if (self.voxel == indf3.voxel):
            return 1

        return 0

    def multV(self, indf3):
        if (self.__samesize__(indf3) == 0):
            print("Cannot multiply voxels - not same size")
            return

        for i in range(self.sizeX()*self.sizeY()*self.sizeZ()):
            self.voxel[i] = self.voxel[i]*indf3.voxel[i]

        return self

    def addV(self, indf3):
        if (self.__samesize__(indf3) == 0):
            print("Cannot add voxels - not same size")
            return

        for i in range(self.sizeX()*self.sizeY()*self.sizeZ()):
            self.voxel[i] = self.voxel[i]+indf3.voxel[i]

        return self

    def convolveV(self, filt):
        fx = filt.sizeX()
        fy = filt.sizeY()
        fz = filt.sizeZ()
        if (fx % 2 != 1):
            print("Incompatible filter - must be odd number of X")
            return self
        if (fy % 2 != 1):
            print("Incompatible filter - must be odd number of Y")
            return self
        if (fz % 2 != 1):
            print("Incompatible filter - must be odd number of Z")
            return self

        fdx = (fx-1)/2
        fdy = (fy-1)/2
        fdz = (fz-1)/2
        flen = fx*fy*fz

        newV = self.__create__(self.sizeX(), self.sizeY(), self.sizeZ());

        for x in range(self.sizeX()):
            for y in range(self.sizeY()):
                for z in range(self.sizeZ()):
                    rip = self.__rip__(x-fdx, x+fdx, y-fdy, y+fdy, z-fdz, z+fdz)
                    tmp = 0.0
                    for i in range(flen):
                        tmp += rip[i]*filt.voxel[i]
                    newV[self.__voxa__(x,y,z)] = tmp

        self.voxel = newV

        return self

    #### Import/Export Functions

    def exportDF3(self, file, depth=8, rescale=1):
        x = self.sizeX()
        y = self.sizeY()
        z = self.sizeZ()

        try:
            f = open(file, 'wb');
        except:
            print("Could not open " + file + " for write");
            return

        f.write(struct.pack(self.__struct2byte3__, x, y, z));

        tmp = self.__toInteger__(pow(2,depth)-1, rescale)

        if (depth > 16): # 32-bit
            for i in range( x*y*z ):
                f.write(struct.pack(self.__struct4byte__, tmp[i]))
        elif (depth > 8): # 16-bit
            for i in range( x*y*z ):
                f.write(struct.pack(self.__struct2byte__, tmp[i]))
        else:
            for i in range( x*y*z ):
                f.write(struct.pack(self.__struct1byte__, tmp[i]))

    def importDF3(self, file, scale=1):
        try:
            f = open(file, 'rb');
            size = os.stat(file)[stat.ST_SIZE]

        except:
            print("Could not open " + file + " for read");
            return []

        (x, y, z) = struct.unpack(self.__struct2byte3__, f.read(6) )

        self.voxel = self.__create__(x, y, z)
        self.maxX = x
        self.maxY = y
        self.maxZ = z

        size = size-6
        if (size == x*y*z):     format = 8
        elif (size == 2*x*y*z): format = 16
        elif (size == 4*x*y*z): format = 32

        if (format == 32):
            for i in range(x*y*z):
                self.voxel[i] = float(struct.unpack(self.__struct4byte__, f.read(4) )[0])
        elif (format == 16):
            for i in range(x*y*z):
                self.voxel[i] = float(struct.unpack(self.__struct2byte__, f.read(2) )[0])
        elif (format == 8):
            for i in range(x*y*z):
                self.voxel[i] = float(struct.unpack(self.__struct1byte__, f.read(1) )[0])

        return self

    #### Local classes not intended for user use

    def __rip__(self, minX, maxX, minY, maxY, minZ, maxZ):
        sizeX = maxX-minX+1
        sizeY = maxY-minY+1
        sizeZ = maxZ-minZ+1

        tmpV = self.__create__(sizeX, sizeY, sizeZ)

        for x in range(sizeX):
            for y in range(sizeY):
                for z in range(sizeZ):
                    # Check X
                    if ((minX + x) < 0):
                        tmpV[(z*sizeZ+y)*sizeY+x] = 0.0
                    elif ((minX + x) > self.sizeX()-1):
                        tmpV[(z*sizeZ+y)*sizeY+x] = 0.0
                    # Check Y
                    elif ((minY + y) < 0):
                        tmpV[(z*sizeZ+y)*sizeY+x] = 0.0
                    elif ((minY + y) > self.sizeY()-1):
                        tmpV[(z*sizeZ+y)*sizeY+x] = 0.0
                    # Check Z
                    elif ((minZ + z) < 0):
                        tmpV[(z*sizeZ+y)*sizeY+x] = 0.0
                    elif ((minZ + z) > self.sizeZ()-1):
                        tmpV[(z*sizeZ+y)*sizeY+x] = 0.0
                    else:
                        tmpV[(z*sizeZ+y)*sizeY+x] = self.get(minX+x,minY+y,minZ+z)

        return tmpV

    def __samesize__(self, indf3):
        if (self.sizeX() != indf3.sizeX()): return 0
        if (self.sizeY() != indf3.sizeY()): return 0
        if (self.sizeZ() != indf3.sizeZ()): return 0
        return 1

    def __voxa__(self, x, y, z):
        return ((z*self.sizeY()+y)*self.sizeX()+x)

    def __create__(self, x, y, z, atype='0', init=1):
        if (atype == '0'):
            tmp = self.__arraytype__
        else:
            tmp = atype

        if (init == 1):
            if tmp in ('f','d'):
                voxel = array.array(tmp, [0.0 for i in range(x*y*z)])
            else:
                voxel = array.array(tmp, [0 for i in range(x*y*z)])
        else:
            voxel = array.array(tmp)

        return voxel

    def __toInteger__(self, scale, rescale=1):
        if (scale < pow(2,8)): # 8-bit
            tmp = self.__create__(self.sizeX(), self.sizeY(), self.sizeZ(), self.__array1byte__)
        elif (scale < pow(2,16)): # 16-bit
            tmp = self.__create__(self.sizeX(), self.sizeY(), self.sizeZ(), self.__array2byte__)
        else: # 32-bit
            tmp = self.__create__(self.sizeX(), self.sizeY(), self.sizeZ(), self.__array4byte__)

        maxVal = self.max()

        print(scale)

        for i in range(self.sizeX()*self.sizeY()*self.sizeZ()):
            if (rescale == 1):
                tmp[i] = max(0,int(round(scale*self.voxel[i]/maxVal)))
            else:
                tmp[i] = max(0,min(scale,int(round(self.voxel[i]))))

        return tmp

# -=-=-=- End df3 Class -=-=-=-
##########DEFAULT EXAMPLES
# if __name__ == '__main__':
    # localX = 80
    # localY = 90
    # localZ = 100
    ## Generate an output
    # temp = df3(localX, localY, localZ)

    # for i in range(localX):
        # for j in range(localY):
            # for k in range(localZ):
                # if (i >= (localX/2)):
                    # temp.set(i, j, k, 1.0)

    # temp.exportDF3('temp.df3', 16)
###############################################################################
    ## Import
    # temp2 = df3().importDF3('temp.df3')
    # temp2.mult(1/temp2.max())

    ## Compare
    # print(temp2.size())

    # if (temp.compare(temp2) == 0): print("DF3's Do Not Match")

###############################################################################
# ChangeLog
# ---------
# 08/09/05: 0.20 released
#    + Change internal representation to floating point
#    + Rewrite import/export for speed
#    + Convert from 3-d list structure to Array class for data storage
#    + Add element access, scalar, and vector functions
# 07/13/05: 0.10 released
###############################################################################

