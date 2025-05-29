# SPDX-FileCopyrightText: 2012-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "xml_file_run",
    "xml_file_write",
)

import bpy


def build_property_typemap(skip_classes, skip_typemap):

    property_typemap = {}

    for attr in dir(bpy.types):
        # Skip internal methods.
        if attr.startswith("_"):
            continue
        cls = getattr(bpy.types, attr)
        if issubclass(cls, skip_classes):
            continue
        bl_rna = getattr(cls, "bl_rna", None)
        # Needed to skip classes added to the modules `__dict__`.
        if bl_rna is None:
            continue

        # # to support skip-save we can't get all props
        # properties = bl_rna.properties.keys()
        properties = []
        for prop_id, prop in bl_rna.properties.items():
            if not prop.is_skip_save:
                properties.append(prop_id)

        properties.remove("rna_type")
        property_typemap[attr] = properties

    if skip_typemap:
        for cls_name, properties_blacklist in skip_typemap.items():
            properties = property_typemap.get(cls_name)
            if properties is not None:
                for prop_id in properties_blacklist:
                    try:
                        properties.remove(prop_id)
                    except Exception:
                        print("skip_typemap unknown prop_id '{:s}.{:s}'".format(cls_name, prop_id))
            else:
                print("skip_typemap unknown class '{:s}'".format(cls_name))

    return property_typemap


def print_ln(data):
    print(data, end="")


def rna2xml(
        fw=print_ln,
        root_node="",
        root_rna=None,  # must be set
        root_rna_skip=set(),
        root_ident="",
        ident_val="  ",
        skip_classes=(
            bpy.types.Operator,
            bpy.types.Panel,
            bpy.types.KeyingSet,
            bpy.types.Header,
            bpy.types.PropertyGroup,
        ),
        skip_typemap=None,
        pretty_format=True,
        method='DATA',
):
    from xml.sax.saxutils import quoteattr
    property_typemap = build_property_typemap(skip_classes, skip_typemap)

    # don't follow properties of this type, just reference them by name
    # they MUST have a unique 'name' property.
    # 'ID' covers most types
    referenced_classes = (
        bpy.types.ID,
        bpy.types.Bone,
        bpy.types.ActionGroup,
        bpy.types.PoseBone,
        bpy.types.Node,
        bpy.types.Strip,
    )

    def number_to_str(val, val_type):
        if val_type == int:
            return "{:d}".format(val)
        elif val_type == float:
            return "{:.6g}".format(val)
        elif val_type == bool:
            return "TRUE" if val else "FALSE"
        else:
            raise NotImplementedError("this type is not a number {:s}".format(val_type))

    def rna2xml_node(ident, value, parent):
        ident_next = ident + ident_val

        # divide into attrs and nodes.
        node_attrs = []
        nodes_items = []
        nodes_lists = []

        value_type = type(value)

        if issubclass(value_type, skip_classes):
            return

        # XXX, FIXME, point-cache has eternal nested pointer to itself.
        if value == parent:
            return

        value_type_name = value_type.__name__
        for prop in property_typemap[value_type_name]:

            subvalue = getattr(value, prop)
            subvalue_type = type(subvalue)

            if subvalue_type in {int, bool, float}:
                node_attrs.append("{:s}=\"{:s}\"".format(prop, number_to_str(subvalue, subvalue_type)))
            elif subvalue_type is str:
                node_attrs.append("{:s}={:s}".format(prop, quoteattr(subvalue)))
            elif subvalue_type is set:
                node_attrs.append("{:s}={:s}".format(prop, quoteattr("{" + ",".join(list(subvalue)) + "}")))
            elif subvalue is None:
                node_attrs.append("{:s}=\"NONE\"".format(prop))
            elif issubclass(subvalue_type, referenced_classes):
                # special case, ID's are always referenced.
                node_attrs.append("{:s}={:s}".format(prop, quoteattr(subvalue_type.__name__ + "::" + subvalue.name)))
            else:
                try:
                    subvalue_ls = list(subvalue)
                except Exception:
                    subvalue_ls = None

                if subvalue_ls is None:
                    nodes_items.append((prop, subvalue, subvalue_type))
                else:
                    # check if the list contains native types
                    subvalue_rna = value.path_resolve(prop, False)
                    if type(subvalue_rna).__name__ == "bpy_prop_array":
                        # Check if this is a 0-1 color (RGB, RGBA)
                        # in that case write as a hexadecimal.
                        prop_rna = value.bl_rna.properties[prop]
                        if (prop_rna.subtype == 'COLOR_GAMMA' and
                                prop_rna.hard_min == 0.0 and
                                prop_rna.hard_max == 1.0 and
                                prop_rna.array_length in {3, 4}):
                            # -----
                            # color
                            array_value = "#" + "".join(("{:02x}".format(int(v * 255)) for v in subvalue_rna))

                        else:
                            # default
                            def str_recursive(s):
                                subsubvalue_type = type(s)
                                if subsubvalue_type in {int, float, bool}:
                                    return number_to_str(s, subsubvalue_type)
                                else:
                                    return " ".join([str_recursive(si) for si in s])

                            array_value = " ".join(str_recursive(v) for v in subvalue_rna)

                        node_attrs.append("{:s}=\"{:s}\"".format(prop, array_value))
                    else:
                        nodes_lists.append((prop, subvalue_ls, subvalue_type))

        # declare + attributes
        if pretty_format:
            if node_attrs:
                fw("{:s}<{:s}\n".format(ident, value_type_name))
                for node_attr in node_attrs:
                    fw("{:s}{:s}\n".format(ident_next, node_attr))
                fw("{:s}>\n".format(ident_next,))
            else:
                fw("{:s}<{:s}>\n".format(ident, value_type_name))
        else:
            fw("{:s}<{:s} {:s}>\n".format(ident, value_type_name, " ".join(node_attrs)))

        # unique members
        for prop, subvalue, subvalue_type in nodes_items:
            fw("{:s}<{:s}>\n".format(ident_next, prop))  # XXX, this is awkward, how best to solve?
            rna2xml_node(ident_next + ident_val, subvalue, value)
            fw("{:s}</{:s}>\n".format(ident_next, prop))  # XXX, need to check on this.

        # list members
        for prop, subvalue, subvalue_type in nodes_lists:
            fw("{:s}<{:s}>\n".format(ident_next, prop))
            for subvalue_item in subvalue:
                if subvalue_item is not None:
                    rna2xml_node(ident_next + ident_val, subvalue_item, value)
            fw("{:s}</{:s}>\n".format(ident_next, prop))

        fw("{:s}</{:s}>\n".format(ident, value_type_name))

    # -------------------------------------------------------------------------
    # needs re-working to be generic

    if root_node:
        fw("{:s}<{:s}>\n".format(root_ident, root_node))

    # bpy.data
    if method == 'DATA':
        ident = root_ident + ident_val
        for attr in dir(root_rna):

            # exceptions
            if attr.startswith("_"):
                continue
            elif attr in root_rna_skip:
                continue

            value = getattr(root_rna, attr)
            try:
                ls = value[:]
            except Exception:
                ls = None

            if type(ls) == list:
                fw("{:s}<{:s}>\n".format(ident, attr))
                for blend_id in ls:
                    rna2xml_node(ident + ident_val, blend_id, None)
                fw("{:s}</{:s}>\n".format(ident_val, attr))
    # any attribute
    elif method == 'ATTR':
        rna2xml_node(root_ident, root_rna, None)

    if root_node:
        fw("{:s}</{:s}>\n".format(root_ident, root_node))


# NOTE(@ideasman42): regarding `secure_types`.
# This is a safe guard when loading an untrusted XML to prevent any possibility of the XML
# paths "escaping" the intended data types, potentially writing into unexpected settings.
# This is done because the XML itself defines the attributes which are recursed into,
# there is a possibility the XML recurse into data that isn't logically owned by "root",
# out of the theme and into user preferences for example, which could change trust settings
# even executing code.
#
# At the time of writing it seems this is not possible with themes (the main user of this functionality),
# however this could become possible in the future through additional RNA properties and it wouldn't be
# obvious an exploit existed.
#
# In short, it's safest for users of this API to restrict types when loading untrusted XML.

def xml2rna(
        root_xml, *,
        root_rna=None,  # must be set
        secure_types=None,  # `Set[str] | None`
):

    def xml2rna_node(xml_node, value):
        # print("evaluating:", xml_node.nodeName)

        if (secure_types is not None) and (xml_node.nodeName not in secure_types):
            print("Loading the XML with type restrictions, skipping \"{:s}\"".format(xml_node.nodeName))
            return

        # ---------------------------------------------------------------------
        # Simple attributes

        for attr in xml_node.attributes.keys():
            # print("  ", attr)
            subvalue = getattr(value, attr, Ellipsis)

            if subvalue is Ellipsis:
                print("{:s}.{:s} not found".format(type(value).__name__, attr))
            else:
                value_xml = xml_node.attributes[attr].value

                subvalue_type = type(subvalue)
                # tp_name = 'UNKNOWN'
                if subvalue_type == float:
                    value_xml_coerce = float(value_xml)
                    # tp_name = 'FLOAT'
                elif subvalue_type == int:
                    value_xml_coerce = int(value_xml)
                    # tp_name = 'INT'
                elif subvalue_type == bool:
                    value_xml_coerce = {'TRUE': True, 'FALSE': False}[value_xml]
                    # tp_name = 'BOOL'
                elif subvalue_type == str:
                    value_xml_coerce = value_xml
                    # tp_name = 'STR'
                elif hasattr(subvalue, "__len__"):
                    if value_xml.startswith("#"):
                        # read hexadecimal value as float array
                        value_xml_split = value_xml[1:]
                        value_xml_coerce = [int(value_xml_split[i:i + 2], 16) /
                                            255 for i in range(0, len(value_xml_split), 2)]
                        del value_xml_split
                    else:
                        value_xml_split = value_xml.split()
                        try:
                            value_xml_coerce = [int(v) for v in value_xml_split]
                        except ValueError:
                            try:
                                value_xml_coerce = [float(v) for v in value_xml_split]
                            except ValueError:  # bool vector property
                                value_xml_coerce = [{'TRUE': True, 'FALSE': False}[v] for v in value_xml_split]
                        del value_xml_split
                    # tp_name = 'ARRAY'

                    # print("  {:s}.{:s} ({:s}) --- {:s}".format(type(value).__name__, attr, tp_name, subvalue_type))
                try:
                    setattr(value, attr, value_xml_coerce)
                except ValueError:
                    # size mismatch
                    val = getattr(value, attr)
                    if len(val) < len(value_xml_coerce):
                        setattr(value, attr, value_xml_coerce[:len(val)])
                    else:
                        setattr(value, attr, list(value_xml_coerce) + list(val)[len(value_xml_coerce):])

        # ---------------------------------------------------------------------
        # Complex attributes
        for child_xml in xml_node.childNodes:
            if child_xml.nodeType == child_xml.ELEMENT_NODE:
                # print()
                # print(child_xml.nodeName)
                subvalue = getattr(value, child_xml.nodeName, None)
                if subvalue is not None:

                    elems = []
                    for child_xml_real in child_xml.childNodes:
                        if child_xml_real.nodeType == child_xml_real.ELEMENT_NODE:
                            elems.append(child_xml_real)
                    del child_xml_real

                    if hasattr(subvalue, "__len__"):
                        # Collection
                        if len(elems) != len(subvalue):
                            print("Size Mismatch! collection:", child_xml.nodeName)
                        else:
                            for i in range(len(elems)):
                                child_xml_real = elems[i]
                                subsubvalue = subvalue[i]

                                if child_xml_real is None or subsubvalue is None:
                                    print("None found {:s} - {:d} collection:".format(child_xml.nodeName, i))
                                else:
                                    xml2rna_node(child_xml_real, subsubvalue)

                    else:
                        # print(elems)
                        if len(elems) == 1:
                            # sub node named by its type
                            child_xml_real = elems[0]

                            # print(child_xml_real, subvalue)
                            xml2rna_node(child_xml_real, subvalue)
                        else:
                            # empty is valid too
                            pass

    xml2rna_node(root_xml, root_rna)


# -----------------------------------------------------------------------------
# Utility function used by presets.
# The idea is you can run a preset like a script with a few args.
#
# This roughly matches the operator 'bpy.ops.script.python_file_run'


def _get_context_val(context, path):
    try:
        value = context.path_resolve(path)
    except Exception as ex:
        print("Error: {!r}, path {!r} not found".format(ex, path))
        value = Ellipsis

    return value


def xml_file_run(
        context,
        filepath,
        rna_map,
        secure_types=None,  # `set[str] | None`
):
    import xml.dom.minidom

    xml_nodes = xml.dom.minidom.parse(filepath)
    bpy_xml = xml_nodes.getElementsByTagName("bpy")[0]

    for rna_path, xml_tag in rna_map:

        # first get xml
        # TODO, error check
        xml_node = bpy_xml.getElementsByTagName(xml_tag)[0]

        value = _get_context_val(context, rna_path)

        if value is not Ellipsis and value is not None:
            # print("  loading XML: {!r} -> {!r}".format(filepath, rna_path))
            xml2rna(xml_node, root_rna=value, secure_types=secure_types)


def xml_file_write(context, filepath, rna_map, *, skip_typemap=None):
    with open(filepath, "w", encoding="utf-8") as file:
        fw = file.write
        fw("<bpy>\n")

        for rna_path, _xml_tag in rna_map:
            # xml_tag is ignored, we get this from the rna
            value = _get_context_val(context, rna_path)
            rna2xml(
                fw=fw,
                root_rna=value,
                method='ATTR',
                root_ident="  ",
                ident_val="  ",
                skip_typemap=skip_typemap,
            )

        fw("</bpy>\n")
