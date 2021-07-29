# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 3
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

# By Daniel Monteiro Basso, April-November 2011.

# This script was developed with financial support from the Foundation for
# Science and Technology of Portugal, under the grant SFRH/BD/66452/2009.

# Complete rewrite, but based on the original importer for Blender
# 2.39, developed by Jean-Baptiste PERIN (jb_perin(at)yahoo.fr), which was
# based on the MATLAB C3D loader from Alan Morris, Toronto, October 1998
# and Jaap Harlaar, Amsterdam, april 2002


import struct
try:
    from numpy import array as vec  # would be nice to have NumPy in Blender
except:
    from mathutils import Vector as vec


class Marker:
    position = (0., 0., 0.)
    confidence = -1.


class Parameter:
    def __init__(self, infile):
        (nameLength, self.paramIdx) = struct.unpack('bb', infile.read(2))
        if not nameLength:
            self.name = ''
            return
        nameLength = abs(nameLength)  # negative flags something
        if nameLength > 64:
            raise ValueError
        self.name = infile.read(nameLength).decode('ascii')
        (offset, b) = struct.unpack('hb', infile.read(3))
        if self.paramIdx > 0:
            self.isGroup = False
            self.data = infile.read(offset - 3)
        else:
            self.isGroup = True
            self.paramIdx *= -1
            self.description = infile.read(b)
            self.params = {}

    def collect(self, infile):
        while True:
            p = Parameter(infile)
            if not p.name or p.isGroup:
                return p
            self.params[p.name] = p

    def decode(self):
        # for now only decode labels
        l, c = struct.unpack('BB', self.data[1:3])
        return [self.data[3 + i:3 + i + l].strip().decode('ascii')
                for i in range(0, l * c, l)]


class MarkerSet:
    def __init__(self, fileName, scale=1., stripPrefix=True, onlyHeader=False):
        self.fileName = fileName
        if fileName.endswith('.csv'):
            with open(fileName, 'rt') as infile:
                self.readCSV(infile)
            return
        if onlyHeader:
            self.infile = open(fileName, 'rb')
            self.readHeader(self.infile, scale)
            self.identifyMarkerPrefix(stripPrefix)
            self.infile.seek(512 * (self.dataBlock - 1))
            self.frames = []
            return
        with open(fileName, 'rb') as infile:
            self.readHeader(infile, scale)
            self.identifyMarkerPrefix(stripPrefix)
            self.readFrameData(infile)

    def readCSV(self, infile):
        import csv
        csvr = csv.reader(infile)
        header = next(csvr)
        if 0 != len(header) % 3:
            raise Exception('Incorrect data format in CSV file')
        self.markerLabels = [label[:-2] for label in header[::3]]
        self.frames = []
        for framerow in csvr:
            newFrame = []
            for c in range(0, len(framerow), 3):
                m = Marker()
                try:
                    m.position = vec([float(v) for v in framerow[c:c + 3]])
                    m.confidence = 1.
                except:
                    pass
                newFrame.append(m)
            self.frames.append(newFrame)
        self.startFrame = 0
        self.endFrame = len(self.frames) - 1
        self.scale = 1.

    def writeCSV(self, fileName, applyScale=True, mfilter=[]):
        import csv
        with open(fileName, 'w') as fo:
            o = csv.writer(fo)
            appxyz = lambda m: [m + a for a in ('_X', '_Y', '_Z')]
            explabels = (appxyz(m) for m in self.markerLabels
                         if not mfilter or m in mfilter)
            o.writerow(sum(explabels, []))
            fmt = lambda m: tuple('{0:.4f}'.format(
                a * (self.scale if applyScale else 1.))
                for a in m.position)
            nan = ('NaN', 'NaN', 'NaN')
            if mfilter:
                mfilter = [self.markerLabels.index(m)
                            for m in self.markerLabels if m in mfilter]
            for f in self.frames:
                F = f
                if mfilter:
                    F = [m for i, m in enumerate(f) if i in mfilter]
                expmarkers = (m.confidence < 0 and nan or fmt(m) for m in F)
                o.writerow(sum(expmarkers, ()))

    def identifyMarkerPrefix(self, stripPrefix):
        prefix = self.markerLabels[0]
        for ml in self.markerLabels[1:]:
            if len(ml) < len(prefix):
                prefix = prefix[:len(ml)]
            if not prefix:
                break
            for i in range(len(prefix)):
                if prefix[i] != ml[i]:
                    prefix = prefix[:i]
                    break
        self.prefix = prefix
        if stripPrefix:
            p = len(self.prefix)
            self.markerLabels = [ml[p:] for ml in self.markerLabels]

    def readHeader(self, infile, scale):
        (self.firstParameterBlock, key, self.markerCount, bogus,
         self.startFrame, self.endFrame,
         bogus) = struct.unpack('BBhhhhh', infile.read(12))
        if key != 80:
            raise Exception('Not a C3D file.')
        self.readParameters(infile)
        infile.seek(12)
        td = infile.read(12)
        if self.procType == 2:
            td = td[2:4] + td[:2] + td[4:8] + td[10:] + td[8:10]
        (self.scale, self.dataBlock, bogus,
         self.frameRate) = struct.unpack('fhhf', td)
        self.scale *= scale
        if self.scale < 0:
            if self.procType == 2:
                self.readMarker = self.readFloatMarkerInvOrd
            else:
                self.readMarker = self.readFloatMarker
            self.scale *= -1
        else:
            self.readMarker = self.readShortMarker

    def readParameters(self, infile):
        infile.seek(512 * (self.firstParameterBlock - 1))
        (ig, ig, pointIdx,
         self.procType) = struct.unpack('BBBB', infile.read(4))
        self.procType -= 83
        if self.procType not in {1, 2}:
            # 1(INTEL-PC); 2(DEC-VAX); 3(MIPS-SUN/SGI)
            print('Warning: importer was not tested for files from '
                  'architectures other than Intel-PC and DEC-VAX')
            print('Type: {0}'.format(self.procType))
        self.paramGroups = {}
        g = Parameter(infile)
        self.paramGroups[g.name] = g
        while(True):
            g = g.collect(infile)
            if not g.name:
                break
            self.paramGroups[g.name] = g
        cand_mlabel = None
        for pg in self.paramGroups:
            #print("group: " + pg)
            #for p in self.paramGroups[pg].params:
            #    print("   * " + p)
            if 'LABELS' in self.paramGroups[pg].params:
                cand_mlabel = self.paramGroups[pg].params['LABELS'].decode()
                if len(cand_mlabel) == self.markerCount:
                    break
                cand_mlabel = None
        # pg should be 'POINT', but let's be liberal and accept any group
        # as long as the LABELS parameter has the same number of markers
        if cand_mlabel is None:
            self.markerLabels = ["m{}".format(idx)
                                 for idx in range(self.markerCount)]
        else:
            self.markerLabels = cand_mlabel
        repeats = {}
        for i, m in enumerate(self.markerLabels):
            if m in repeats:
                self.markerLabels[i] = '{}.{}'.format(m, repeats[m])
                repeats[m] += 1
            else:
                repeats[m] = 1

    def readMarker(self, infile):
        pass  # ...

    def readFloatMarker(self, infile):
        m = Marker()
        x, y, z, m.confidence = struct.unpack('ffff', infile.read(16))
        m.position = (x * self.scale, y * self.scale, z * self.scale)
        return m

    def readFloatMarkerInvOrd(self, infile):
        m = Marker()
        inv = lambda f: f[2:] + f[:2]
        i = lambda: inv(infile.read(4))
        x, y, z, m.confidence = struct.unpack('ffff', i() + i() + i() + i())
        m.position = (x * self.scale, y * self.scale, z * self.scale)
        return m

    def readShortMarker(self, infile):
        m = Marker()
        x, y, z, m.confidence = struct.unpack('hhhh', infile.read(8))
        m.position = (x * self.scale, y * self.scale, z * self.scale)
        return m

    def readFrameData(self, infile):
        infile.seek(512 * (self.dataBlock - 1))
        self.frames = []
        for f in range(self.startFrame, self.endFrame + 1):
            frame = [self.readMarker(infile) for m in range(self.markerCount)]
            self.frames.append(frame)

    def readNextFrameData(self):
        if len(self.frames) < (self.endFrame - self.startFrame + 1):
            frame = [self.readMarker(self.infile)
                for m in range(self.markerCount)]
            self.frames.append(frame)
        return self.frames[-1]

    def getFramesByMarker(self, marker):
        if type(marker) == int:
            idx = marker
        else:
            idx = self.markerLabels.index(marker)
        fcnt = self.endFrame - self.startFrame + 1
        return [self.frames[f][idx] for f in range(fcnt)]

    def getMarker(self, marker, frame):
        idx = self.markerLabels.index(marker)
        return self.frames[frame - self.startFrame][idx]


def read(filename, *a, **kw):
    return MarkerSet(filename, *a, **kw)

# -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-


if __name__ == '__main__':
    import os
    import sys

    sys.argv.pop(0)
    if not sys.argv:
        print("Convert C3D to CSV.\n"
              "Please specify at least one C3D input file.")
        raise SystemExit
    while sys.argv:
        fname = sys.argv.pop(0)
        markerset = read(fname)
        print("frameRate={0.frameRate}\t"
              "scale={0.scale:.2f}\t"
              "markers={0.markerCount}\t"
              "startFrame={0.startFrame}\t"
              "endFrame={0.endFrame}".format(markerset))
        markerset.writeCSV(fname.lower().replace(".c3d", ".csv"))
