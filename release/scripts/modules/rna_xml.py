# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Contributor(s): Campbell Barton
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

import bpy


def build_property_typemap(skip_classes):

    property_typemap = {}

    for attr in dir(bpy.types):
        cls = getattr(bpy.types, attr)
        if issubclass(cls, skip_classes):
            continue

        properties = cls.bl_rna.properties.keys()
        properties.remove("rna_type")
        property_typemap[attr] = properties

    return property_typemap


def print_ln(data):
    print(data, end="")


def rna2xml(fw=print_ln,
            root_node="",
            root_rna=None,  # must be set
            root_rna_skip=set(),
            ident_val="    ",
            skip_classes=(bpy.types.Operator,
                          bpy.types.Panel,
                          bpy.types.KeyingSet,
                          bpy.types.Header,
                          ),
            pretty_format=True,
            method='DATA'):

    from xml.sax.saxutils import quoteattr
    property_typemap = build_property_typemap(skip_classes)

    def number_to_str(val, val_type):
        if val_type == int:
            return "%d" % val
        elif val_type == float:
            return "%.6g" % val
        elif val_type == bool:
            return "TRUE" if val else "FALSE"
        else:
            raise NotImplemented("this type is not a number %s" % val_type)

    def rna2xml_node(ident, value, parent):
        ident_next = ident + ident_val

        # divide into attrs and nodes.
        node_attrs = []
        nodes_items = []
        nodes_lists = []

        value_type = type(value)

        if issubclass(value_type, skip_classes):
            return

        # XXX, fixme, pointcache has eternal nested pointer to its self.
        if value == parent:
            return

        value_type_name = value_type.__name__
        for prop in property_typemap[value_type_name]:

            subvalue = getattr(value, prop)
            subvalue_type = type(subvalue)

            if subvalue_type in (int, bool, float):
                node_attrs.append("%s=\"%s\"" % (prop, number_to_str(subvalue, subvalue_type)))
            elif subvalue_type is str:
                node_attrs.append("%s=%s" % (prop, quoteattr(subvalue)))
            elif subvalue_type == set:
                node_attrs.append("%s=%s" % (prop, quoteattr("{" + ",".join(list(subvalue)) + "}")))
            elif subvalue is None:
                node_attrs.append("%s=\"NONE\"" % prop)
            elif issubclass(subvalue_type, bpy.types.ID):
                # special case, ID's are always referenced.
                node_attrs.append("%s=%s" % (prop, quoteattr(subvalue_type.__name__ + "::" + subvalue.name)))
            else:
                try:
                    subvalue_ls = list(subvalue)
                except:
                    subvalue_ls = None

                if subvalue_ls is None:
                    nodes_items.append((prop, subvalue, subvalue_type))
                else:
                    # check if the list contains native types
                    subvalue_rna = value.path_resolve(prop, False)
                    if type(subvalue_rna).__name__ == "bpy_prop_array":
                        # check if this is a 0-1 color (rgb, rgba)
                        # in that case write as a hexidecimal
                        prop_rna = value.bl_rna.properties[prop]
                        if (prop_rna.subtype == 'COLOR_GAMMA' and
                                prop_rna.hard_min == 0.0 and
                                prop_rna.hard_max == 1.0 and
                                prop_rna.array_length in {3, 4}):
                            # -----
                            # color
                            array_value = "#" + "".join(("%.2x" % int(v * 255) for v in subvalue_rna))

                        else:
                            # default
                            def str_recursive(s):
                                subsubvalue_type = type(s)
                                if subsubvalue_type in (int, float, bool):
                                    return number_to_str(s, subsubvalue_type)
                                else:
                                    return " ".join([str_recursive(si) for si in s])
                            
                            array_value = " ".join(str_recursive(v) for v in subvalue_rna)

                        node_attrs.append("%s=\"%s\"" % (prop, array_value))
                    else:
                        nodes_lists.append((prop, subvalue_ls, subvalue_type))

        # declare + attributes
        if pretty_format:
            tmp_str = "<%s " % value_type_name
            tmp_ident = "\n" + ident + (" " * len(tmp_str))

            fw("%s%s%s>\n" % (ident, tmp_str, tmp_ident.join(node_attrs)))

            del tmp_str
            del tmp_ident
        else:
            fw("%s<%s %s>\n" % (ident, value_type_name, " ".join(node_attrs)))

        # unique members
        for prop, subvalue, subvalue_type in nodes_items:
            fw("%s<%s>\n" % (ident_next, prop))  # XXX, this is awkward, how best to solve?
            rna2xml_node(ident_next + ident_val, subvalue, value)
            fw("%s</%s>\n" % (ident_next, prop))  # XXX, need to check on this.

        # list members
        for prop, subvalue, subvalue_type in nodes_lists:
            fw("%s<%s>\n" % (ident_next, prop))
            for subvalue_item in subvalue:
                if subvalue_item is not None:
                    rna2xml_node(ident_next + ident_val, subvalue_item, value)
            fw("%s</%s>\n" % (ident_next, prop))

        fw("%s</%s>\n" % (ident, value_type_name))

    # -------------------------------------------------------------------------
    # needs re-workign to be generic

    if root_node:
        fw("<%s>\n" % root_node)

    # bpy.data
    if method == 'DATA':
        for attr in dir(root_rna):

            # exceptions
            if attr.startswith("_"):
                continue
            elif attr in root_rna_skip:
                continue

            value = getattr(root_rna, attr)
            try:
                ls = value[:]
            except:
                ls = None

            if type(ls) == list:
                fw("%s<%s>\n" % (ident_val, attr))
                for blend_id in ls:
                    rna2xml_node(ident_val + ident_val, blend_id, None)
                fw("%s</%s>\n" % (ident_val, attr))
    # any attribute
    elif method == 'ATTR':
        rna2xml_node("", root_rna, None)

    if root_node:
        fw("</%s>\n" % root_node)


def xml2rna(root_xml,
            root_rna=None,  # must be set
            ):

    def rna2xml_node(xml_node, value):
#        print("evaluating:", xml_node.nodeName)

        # ---------------------------------------------------------------------
        # Simple attributes

        for attr in xml_node.attributes.keys():
#            print("  ", attr)
            subvalue = getattr(value, attr, Ellipsis)

            if subvalue is Ellipsis:
                print("%s.%s not found" % (type(value).__name__, attr))
            else:
                value_xml = xml_node.attributes[attr].value

                subvalue_type = type(subvalue)
                tp_name = 'UNKNOWN'
                if subvalue_type == float:
                    value_xml_coerce = float(value_xml)
                    tp_name = 'FLOAT'
                elif subvalue_type == int:
                    value_xml_coerce = int(value_xml)
                    tp_name = 'INT'
                elif subvalue_type == bool:
                    value_xml_coerce = {'TRUE': True, 'FALSE': False}[value_xml]
                    tp_name = 'BOOL'
                elif subvalue_type == str:
                    value_xml_coerce = value_xml
                    tp_name = 'STR'
                elif hasattr(subvalue, "__len__"):
                    if value_xml.startswith("#"):
                        # read hexidecimal value as float array
                        value_xml_split = value_xml[1:]
                        value_xml_coerce = [int(value_xml_split[i:i + 2], 16) / 255  for i in range(0, len(value_xml_split), 2)]
                        del value_xml_split
                    else:
                        value_xml_split = value_xml.split()
                        try:
                            value_xml_coerce = [int(v) for v in value_xml_split]
                        except ValueError:
                            value_xml_coerce = [float(v) for v in value_xml_split]
                        del value_xml_split
                    tp_name = 'ARRAY'

#                print("  %s.%s (%s) --- %s" % (type(value).__name__, attr, tp_name, subvalue_type))
                setattr(value, attr, value_xml_coerce)

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
                                    print("None found %s - %d collection:", (child_xml.nodeName, i))
                                else:
                                    rna2xml_node(child_xml_real, subsubvalue)

                    else:
#                        print(elems)

                        if len(elems) == 1:
                            # sub node named by its type
                            child_xml_real, = elems

                            # print(child_xml_real, subvalue)
                            rna2xml_node(child_xml_real, subvalue)
                        else:
                            # empty is valid too
                            pass

    rna2xml_node(root_xml, root_rna)



# -----------------------------------------------------------------------------
# Utility function used by presets.
# The idea is you can run a preset like a script with a few args.
#
# This roughly matches the operator 'bpy.ops.script.python_file_run'

def xml_file_run(context, filepath, rna_map):

    import rna_xml
    import xml.dom.minidom

    xml_nodes = xml.dom.minidom.parse(filepath)
    bpy_xml = xml_nodes.getElementsByTagName("bpy")[0]

    for rna_path, xml_tag in rna_map:

        # first get xml
        # TODO, error check
        xml_node = bpy_xml.getElementsByTagName(xml_tag)[0]

        # now get 
        rna_path_full = "context." + rna_path
        try:
            value = eval(rna_path_full)
        except:
            import traceback
            traceback.print_exc()
            print("Error: %r could not be found" % rna_path_full)

            value = Ellipsis

        if value is not Ellipsis and value is not None:
            print("Loading XML: %r" % rna_path_full)
            rna_xml.xml2rna(xml_node, root_rna=value)
