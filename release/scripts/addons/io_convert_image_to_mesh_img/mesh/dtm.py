# This file is a part of the HiRISE DTM Importer for Blender
#
# Copyright (C) 2017 Arizona Board of Regents on behalf of the Planetary Image
# Research Laboratory, Lunar and Planetary Laboratory at the University of
# Arizona.
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Objects for importing HiRISE DTMs."""

import numpy as np

from .. import pvl


class DTM:
    """
    HiRISE Digital Terrain Model

    This class imports a HiRISE DTM from a Planetary Data Systems (PDS)
    compliant .IMG file.

    Parameters
    ----------
    path : str
    terrain_resolution : float, optional
        Controls the resolution the DTM is read at. This should be a float
        in the range [0.01, 1.0] (and will be constrained to this range). A
        value of 1.0 will result in the DTM being read at full resolution. A
        value of 0.01 will result in the DTM being read at 1/100th resolution.
        Default is 1.0 (no downsampling).

    Todo
    ----
    * Use GDAL for importing the DTM if it is installed for this Python
      environment. If/when I have the time to do this, it probably
      warrants breaking out separate importer classes. The benefits of
      doing this are pretty substantial, though:

        + More reliable (doesn't rely on my PVL parser for finding the
          valid values in the DTM, for locating the starting position of
          the elevation data in the .IMG file)

        + Other, better, downsampling algorithms are already built in.

        + Would make this much better at general PDS DTM importing,
          currently some of the import code is specific to HiRISE DTMs.

    """

    # Special constants in our data:
    #     NULL : No data at this point.
    #     LRS  : Low Representation Saturation
    #     LIS  : Low Instrument Saturation
    #     HRS  : High Representation Saturation
    #     HIS  : High Insturment Saturation
    SPECIAL_VALUES = {
        "NULL": np.fromstring(b'\xFF\x7F\xFF\xFB', dtype='>f4')[0],
        "LRS": np.fromstring(b'\xFF\x7F\xFF\xFC', dtype='>f4')[0],
        "LIS": np.fromstring(b'\xFF\x7F\xFF\xFD', dtype='>f4')[0],
        "HRS": np.fromstring(b'\xFF\x7F\xFF\xFE', dtype='>f4')[0],
        "HIS": np.fromstring(b'\xFF\x7F\xFF\xFF', dtype='>f4')[0]
    }

    def __init__(self, path, terrain_resolution=1.0):
        self.path = path
        self.terrain_resolution = terrain_resolution
        self.label = self._read_label()
        self.data = self._read_data()

    def _read_label(self):
        """Returns a dict-like representation of a PVL label"""
        return pvl.load(self.path)

    def _read_data(self):
        """
        Reads elevation data from a PDS .IMG file.

        Notes
        -----
        * Uses nearest-neighbor to downsample data.

        Todo
        ----
        * Add other downsampling algorithms.

        """
        h, w = self.image_resolution
        max_samples = int(w - w % self.bin_size)

        data = np.zeros(self.shape)
        with open(self.path, 'rb') as f:
            # Seek to the first byte of data
            start_byte = self._get_data_start()
            f.seek(start_byte)
            # Iterate over each row of the data
            for r in range(data.shape[0]):
                # Each iteration, seek to the right location before
                # reading a row. We determine this location as the
                # first byte of data PLUS a offset which we calculate as the
                # product of:
                #
                #     4, the number of bytes in a single record
                #     r, the current row index
                #     w, the number of records in a row of the DTM
                #     bin_size, the number of records in a bin
                #
                # This is where we account for skipping over rows.
                offset = int(4 * r * w * self.bin_size)
                f.seek(start_byte + offset)
                # Read a row
                row = np.fromfile(f, dtype=np.float32, count=max_samples)
                # This is where we account for skipping over columns.
                data[r] = row[::self.bin_size]

        data = self._process_invalid_data(data)
        return data

    def _get_data_start(self):
        """Gets the start position of the DTM data block"""
        label_length = self.label['RECORD_BYTES']
        num_labels = self.label.get('LABEL_RECORDS', 1)
        return int(label_length * num_labels)

    def _process_invalid_data(self, data):
        """Sets any 'NULL' elevation values to np.NaN"""
        invalid_data_mask = (data <= self.SPECIAL_VALUES['NULL'])
        data[invalid_data_mask] = np.NaN
        return data

    @property
    def map_size(self):
        """Geographic size of the bounding box around the DTM"""
        scale = self.map_scale * self.unit_scale
        w = self.image_resolution[0] * scale
        h = self.image_resolution[1] * scale
        return (w, h)

    @property
    def mesh_scale(self):
        """Geographic spacing between mesh vertices"""
        return self.bin_size * self.map_scale * self.unit_scale

    @property
    def map_info(self):
        """Map Projection metadata"""
        return self.label['IMAGE_MAP_PROJECTION']

    @property
    def map_scale(self):
        """Geographic spacing between DTM posts"""
        map_scale = self.map_info.get('MAP_SCALE', None)
        return getattr(map_scale, 'value', 1.0)

    @property
    def map_units(self):
        """Geographic unit for spacing between DTM posts"""
        map_scale = self.map_info.get('MAP_SCALE', None)
        return getattr(map_scale, 'units', None)

    @property
    def unit_scale(self):
        """
        The function that creates a Blender mesh from this object will assume
        that the height values passed into it are in meters --- this
        property is a multiplier to convert DTM-units to meters.
        """
        scaling_factors = {
            'KM/PIXEL': 1000,
            'METERS/PIXEL': 1
        }
        return scaling_factors.get(self.map_units, 1.0)

    @property
    def terrain_resolution(self):
        """Vertex spacing, meters"""
        return self._terrain_resolution

    @terrain_resolution.setter
    def terrain_resolution(self, t):
        self._terrain_resolution = np.clip(t, 0.01, 1.0)

    @property
    def bin_size(self):
        """The width of the (square) downsampling bin"""
        return int(np.ceil(1 / self.terrain_resolution))

    @property
    def image_stats(self):
        """Image statistics from the original DTM label"""
        return self.label['IMAGE']

    @property
    def image_resolution(self):
        """(Line, Sample) resolution of the original DTM"""
        return (self.image_stats['LINES'], self.image_stats['LINE_SAMPLES'])

    @property
    def size(self):
        """Number of posts in our reduced DTM"""
        return self.shape[0] * self.shape[1]

    @property
    def shape(self):
        """Shape of our reduced DTM"""
        num_rows = self.image_resolution[0] // self.bin_size
        num_cols = self.image_resolution[1] // self.bin_size
        return (num_rows, num_cols)
