# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import numpy as np

from ...io.com import gltf2_io
from ...io.com import constants as gltf2_io_constants
from ...io.exp import binary_data as gltf2_io_binary_data
from .cache import cached


@cached
def gather_accessor(buffer_view: gltf2_io_binary_data.BinaryData,
                    component_type: gltf2_io_constants.ComponentType,
                    count,
                    max,
                    min,
                    type: gltf2_io_constants.DataType,
                    export_settings) -> gltf2_io.Accessor:
    return gltf2_io.Accessor(
        buffer_view=buffer_view,
        byte_offset=None,
        component_type=component_type,
        count=count,
        extensions=None,
        extras=None,
        max=list(max) if max is not None else None,
        min=list(min) if min is not None else None,
        name=None,
        normalized=None,
        sparse=None,
        type=type
    )


def array_to_accessor(
        array,
        export_settings,
        component_type,
        data_type,
        include_max_and_min=False,
        sparse_type=None,
        normalized=None,
):

    # Not trying to check if sparse is better
    if sparse_type is None:

        buffer_view = gltf2_io_binary_data.BinaryData(
            array.tobytes(),
            gltf2_io_constants.BufferViewTarget.ARRAY_BUFFER,
        )

        amax = None
        amin = None
        if include_max_and_min:
            amax = np.amax(array, axis=0).tolist()
            amin = np.amin(array, axis=0).tolist()

        return gltf2_io.Accessor(
            buffer_view=buffer_view,
            byte_offset=None,
            component_type=component_type,
            count=len(array),
            extensions=None,
            extras=None,
            max=amax,
            min=amin,
            name=None,
            normalized=normalized,
            sparse=None,
            type=data_type,
        )

    # Trying to check if sparse is better (if user want it)
    buffer_view = None
    sparse = None

    try_sparse = False
    if sparse_type == "SK":
        try_sparse = export_settings['gltf_try_sparse_sk']

    if try_sparse:
        sparse, omit_sparse = __try_sparse_accessor(array)
    else:
        omit_sparse = False
    if not sparse and omit_sparse is False:
        buffer_view = gltf2_io_binary_data.BinaryData(
            array.tobytes(),
            gltf2_io_constants.BufferViewTarget.ARRAY_BUFFER,
        )
    elif omit_sparse is True:
        if sparse_type == "SK" and export_settings['gltf_try_omit_sparse_sk'] is True:
            sparse = None  # sparse will be None, buffer_view too

    amax = None
    amin = None
    if include_max_and_min:
        amax = np.amax(array, axis=0).tolist()
        amin = np.amin(array, axis=0).tolist()

    return gltf2_io.Accessor(
        buffer_view=buffer_view,
        byte_offset=None,
        component_type=component_type,
        count=len(array),
        extensions=None,
        extras=None,
        max=amax,
        min=amin,
        name=None,
        normalized=None,
        sparse=sparse,
        type=data_type,
    )


def __try_sparse_accessor(array):
    """
    Returns an AccessorSparse for array, or None if
    writing a dense accessor would be better.
    Return True if we can omit sparse accessor
    """

    omit_sparse = False

    # Find indices of non-zero elements
    nonzero_indices = np.where(np.any(array, axis=1))[0]

    # For all-zero arrays, omitting sparse entirely is legal but poorly
    # supported, so force nonzero_indices to be nonempty.
    if len(nonzero_indices) == 0:
        omit_sparse = True
        nonzero_indices = np.array([0])

    # How big of indices do we need?
    if nonzero_indices[-1] <= 255:
        indices_type = gltf2_io_constants.ComponentType.UnsignedByte
    elif nonzero_indices[-1] <= 65535:
        indices_type = gltf2_io_constants.ComponentType.UnsignedShort
    else:
        indices_type = gltf2_io_constants.ComponentType.UnsignedInt

    # Cast indices to appropriate type (if needed)
    nonzero_indices = nonzero_indices.astype(
        gltf2_io_constants.ComponentType.to_numpy_dtype(indices_type),
        copy=False,
    )

    # Calculate size if we don't use sparse
    one_elem_size = len(array[:1].tobytes())
    dense_size = len(array) * one_elem_size

    # Calculate approximate size if we do use sparse
    indices_size = (
        len(nonzero_indices[:1].tobytes()) *
        len(nonzero_indices)
    )
    values_size = len(nonzero_indices) * one_elem_size
    json_increase = 170  # sparse makes the JSON about this much bigger
    penalty = 64  # further penalty avoids sparse in marginal cases
    sparse_size = indices_size + values_size + json_increase + penalty

    if sparse_size >= dense_size:
        return None, omit_sparse

    return gltf2_io.AccessorSparse(
        count=len(nonzero_indices),
        extensions=None,
        extras=None,
        indices=gltf2_io.AccessorSparseIndices(
            buffer_view=gltf2_io_binary_data.BinaryData(
                nonzero_indices.tobytes()
            ),
            byte_offset=None,
            component_type=indices_type,
            extensions=None,
            extras=None,
        ),
        values=gltf2_io.AccessorSparseValues(
            buffer_view=gltf2_io_binary_data.BinaryData(
                array[nonzero_indices].tobytes()
            ),
            byte_offset=None,
            extensions=None,
            extras=None,
        ),
    ), omit_sparse
