# SPDX-FileCopyrightText: 2021-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "property_definition_from_data_path",
    "decompose_data_path",
)


class _TokenizeDataPath:
    """
    Class to split up tokens of a data-path.

    Note that almost all access generates new objects with additional paths,
    with the exception of iteration which is the intended way to access the resulting data."""
    __slots__ = (
        "data_path",
    )

    def __init__(self, attrs):
        self.data_path = attrs

    def __getattr__(self, attr):
        return _TokenizeDataPath(self.data_path + ((".{:s}".format(attr)),))

    def __getitem__(self, key):
        return _TokenizeDataPath(self.data_path + (("[{!r}]".format(key)),))

    def __call__(self, *args, **kw):
        value_str = ", ".join([
            val for val in (
                ", ".join(repr(value) for value in args),
                ", ".join(["{:s}={!r}".format(key, value) for key, value in kw.items()]),
            ) if val])
        return _TokenizeDataPath(self.data_path + ('({:s})'.format(value_str), ))

    def __iter__(self):
        return iter(self.data_path)


def decompose_data_path(data_path):
    """
    Return the components of a data path split into a list.
    """
    ns = {"base": _TokenizeDataPath(())}
    return list(eval("base" + data_path, ns, ns))


def property_definition_from_data_path(base, data_path):
    """
    Return an RNA property definition from an object and a data path.

    In Blender this is often used with ``context`` as the base and a
    path that it references, for example ``.space_data.lock_camera``.
    """
    data = decompose_data_path(data_path)
    while data and (not data[-1].startswith(".")):
        data.pop()

    if (not data) or (not data[-1].startswith(".")) or (len(data) < 2):
        return None

    data_path_head = "".join(data[:-1])
    data_path_tail = data[-1]

    value_head = eval("base" + data_path_head)
    value_head_rna = getattr(value_head, "bl_rna", None)
    if value_head_rna is None:
        return None

    value_tail = value_head.bl_rna.properties.get(data_path_tail[1:])
    if not value_tail:
        return None

    return value_tail
