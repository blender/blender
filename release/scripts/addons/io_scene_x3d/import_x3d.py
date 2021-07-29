# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
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

# <pep8 compliant>

DEBUG = False

# This should work without a blender at all
import os
import shlex
import math
from math import sin, cos, pi

texture_cache = {}
material_cache = {}

EPSILON = 0.0000001  # Very crude.


def imageConvertCompat(path):

    if os.sep == '\\':
        return path  # assume win32 has quicktime, dont convert

    if path.lower().endswith('.gif'):
        path_to = path[:-3] + 'png'

        '''
        if exists(path_to):
            return path_to
        '''
        # print('\n'+path+'\n'+path_to+'\n')
        os.system('convert "%s" "%s"' % (path, path_to))  # for now just hope we have image magick

        if os.path.exists(path_to):
            return path_to

    return path

# notes
# transform are relative
# order dosnt matter for loc/size/rot
# right handed rotation
# angles are in radians
# rotation first defines axis then amount in radians


# =============================== VRML Spesific

def vrml_split_fields(value):
    """
    key 0.0 otherkey 1,2,3 opt1 opt1 0.0
        -> [key 0.0], [otherkey 1,2,3], [opt1 opt1 0.0]
    """
    def iskey(k):
        if k[0] != '"' and k[0].isalpha() and k.upper() not in {'TRUE', 'FALSE'}:
            return True
        return False

    field_list = []
    field_context = []

    for v in value:
        if iskey(v):
            if field_context:
                field_context_len = len(field_context)
                if (field_context_len > 2) and (field_context[-2] in {'DEF', 'USE'}):
                    field_context.append(v)
                elif (not iskey(field_context[-1])) or ((field_context_len == 3 and field_context[1] == 'IS')):
                    # this IS a key but the previous value was not a key, ot it was a defined field.
                    field_list.append(field_context)
                    field_context = [v]
                else:
                    # The last item was not a value, multiple keys are needed in some cases.
                    field_context.append(v)
            else:
                # Is empty, just add this on
                field_context.append(v)
        else:
            # Add a value to the list
            field_context.append(v)

    if field_context:
        field_list.append(field_context)

    return field_list


def vrmlFormat(data):
    """
    Keep this as a valid vrml file, but format in a way we can predict.
    """
    # Strip all commends - # not in strings - warning multiline strings are ignored.
    def strip_comment(l):
        #l = ' '.join(l.split())
        l = l.strip()

        if l.startswith('#'):
            return ''

        i = l.find('#')

        if i == -1:
            return l

        # Most cases accounted for! if we have a comment at the end of the line do this...
        #j = l.find('url "')
        j = l.find('"')

        if j == -1:  # simple no strings
            return l[:i].strip()

        q = False
        for i, c in enumerate(l):
            if c == '"':
                q = not q  # invert

            elif c == '#':
                if q is False:
                    return l[:i - 1]

        return l

    data = '\n'.join([strip_comment(l) for l in data.split('\n')])  # remove all whitespace

    EXTRACT_STRINGS = True  # only needed when strings or filesnames containe ,[]{} chars :/

    if EXTRACT_STRINGS:

        # We need this so we can detect URL's
        data = '\n'.join([' '.join(l.split()) for l in data.split('\n')])  # remove all whitespace

        string_ls = []

        #search = 'url "'
        search = '"'

        ok = True
        last_i = 0
        while ok:
            ok = False
            i = data.find(search, last_i)
            if i != -1:

                start = i + len(search)  # first char after end of search
                end = data.find('"', start)
                if end != -1:
                    item = data[start:end]
                    string_ls.append(item)
                    data = data[:start] + data[end:]
                    ok = True  # keep looking

                    last_i = (end - len(item)) + 1
                    # print(last_i, item, '|' + data[last_i] + '|')

    # done with messy extracting strings part

    # Bad, dont take strings into account
    '''
    data = data.replace('#', '\n#')
    data = '\n'.join([ll for l in data.split('\n') for ll in (l.strip(),) if not ll.startswith('#')]) # remove all whitespace
    '''
    data = data.replace('{', '\n{\n')
    data = data.replace('}', '\n}\n')
    data = data.replace('[', '\n[\n')
    data = data.replace(']', '\n]\n')
    data = data.replace(',', ' , ')  # make sure comma's separate

    # We need to write one property (field) per line only, otherwise we fail later to detect correctly new nodes.
    # See T45195 for details.
    data = '\n'.join([' '.join(value) for l in data.split('\n') for value in vrml_split_fields(l.split())])

    if EXTRACT_STRINGS:
        # add strings back in

        search = '"'  # fill in these empty strings

        ok = True
        last_i = 0
        while ok:
            ok = False
            i = data.find(search + '"', last_i)
            # print(i)
            if i != -1:
                start = i + len(search)  # first char after end of search
                item = string_ls.pop(0)
                # print(item)
                data = data[:start] + item + data[start:]

                last_i = start + len(item) + 1

                ok = True

    # More annoying obscure cases where USE or DEF are placed on a newline
    # data = data.replace('\nDEF ', ' DEF ')
    # data = data.replace('\nUSE ', ' USE ')

    data = '\n'.join([' '.join(l.split()) for l in data.split('\n')])  # remove all whitespace

    # Better to parse the file accounting for multiline arrays
    '''
    data = data.replace(',\n', ' , ') # remove line endings with commas
    data = data.replace(']', '\n]\n') # very very annoying - but some comma's are at the end of the list, must run this again.
    '''

    return [l for l in data.split('\n') if l]

NODE_NORMAL = 1  # {}
NODE_ARRAY = 2  # []
NODE_REFERENCE = 3  # USE foobar
# NODE_PROTO = 4 #

lines = []


def getNodePreText(i, words):
    # print(lines[i])
    use_node = False
    while len(words) < 5:

        if i >= len(lines):
            break
            '''
        elif lines[i].startswith('PROTO'):
            return NODE_PROTO, i+1
            '''
        elif lines[i] == '{':
            # words.append(lines[i]) # no need
            # print("OK")
            return NODE_NORMAL, i + 1
        elif lines[i].count('"') % 2 != 0:  # odd number of quotes? - part of a string.
            # print('ISSTRING')
            break
        else:
            new_words = lines[i].split()
            if 'USE' in new_words:
                use_node = True

            words.extend(new_words)
            i += 1

        # Check for USE node - no {
        # USE #id - should always be on the same line.
        if use_node:
            # print('LINE', i, words[:words.index('USE')+2])
            words[:] = words[:words.index('USE') + 2]
            if lines[i] == '{' and lines[i + 1] == '}':
                # USE sometimes has {} after it anyway
                i += 2
            return NODE_REFERENCE, i

    # print("error value!!!", words)
    return 0, -1


def is_nodeline(i, words):

    if not lines[i][0].isalpha():
        return 0, 0

    #if lines[i].startswith('field'):
    #   return 0, 0

    # Is this a prototype??
    if lines[i].startswith('PROTO'):
        words[:] = lines[i].split()
        return NODE_NORMAL, i + 1  # TODO - assumes the next line is a '[\n', skip that
    if lines[i].startswith('EXTERNPROTO'):
        words[:] = lines[i].split()
        return NODE_ARRAY, i + 1  # TODO - assumes the next line is a '[\n', skip that

    '''
    proto_type, new_i = is_protoline(i, words, proto_field_defs)
    if new_i != -1:
        return proto_type, new_i
    '''

    # Simple "var [" type
    if lines[i + 1] == '[':
        if lines[i].count('"') % 2 == 0:
            words[:] = lines[i].split()
            return NODE_ARRAY, i + 2

    node_type, new_i = getNodePreText(i, words)

    if not node_type:
        if DEBUG:
            print("not node_type", lines[i])
        return 0, 0

    # Ok, we have a { after some values
    # Check the values are not fields
    for i, val in enumerate(words):
        if i != 0 and words[i - 1] in {'DEF', 'USE'}:
            # ignore anything after DEF, it is a ID and can contain any chars.
            pass
        elif val[0].isalpha() and val not in {'TRUE', 'FALSE'}:
            pass
        else:
            # There is a number in one of the values, therefor we are not a node.
            return 0, 0

    #if node_type==NODE_REFERENCE:
    #   print(words, "REF_!!!!!!!")
    return node_type, new_i


def is_numline(i):
    """
    Does this line start with a number?
    """

    # Works but too slow.
    '''
    l = lines[i]
    for w in l.split():
        if w==',':
            pass
        else:
            try:
                float(w)
                return True

            except:
                return False

    return False
    '''

    l = lines[i]

    line_start = 0

    if l.startswith(', '):
        line_start += 2

    line_end = len(l) - 1
    line_end_new = l.find(' ', line_start)  # comma's always have a space before them

    if line_end_new != -1:
        line_end = line_end_new

    try:
        float(l[line_start:line_end])  # works for a float or int
        return True
    except:
        return False


class vrmlNode(object):
    __slots__ = ('id',
                 'fields',
                 'proto_node',
                 'proto_field_defs',
                 'proto_fields',
                 'node_type',
                 'parent',
                 'children',
                 'parent',
                 'array_data',
                 'reference',
                 'lineno',
                 'filename',
                 'blendObject',
                 'blendData',
                 'DEF_NAMESPACE',
                 'ROUTE_IPO_NAMESPACE',
                 'PROTO_NAMESPACE',
                 'x3dNode',
                 'parsed')

    def __init__(self, parent, node_type, lineno):
        self.id = None
        self.node_type = node_type
        self.parent = parent
        self.blendObject = None
        self.blendData = None
        self.x3dNode = None  # for x3d import only
        self.parsed = None  # We try to reuse objects in a smart way
        if parent:
            parent.children.append(self)

        self.lineno = lineno

        # This is only set from the root nodes.
        # Having a filename also denotes a root node
        self.filename = None
        self.proto_node = None  # proto field definition eg: "field SFColor seatColor .6 .6 .1"

        # Store in the root node because each inline file needs its own root node and its own namespace
        self.DEF_NAMESPACE = None
        self.ROUTE_IPO_NAMESPACE = None
        '''
        self.FIELD_NAMESPACE = None
        '''

        self.PROTO_NAMESPACE = None

        self.reference = None

        if node_type == NODE_REFERENCE:
            # For references, only the parent and ID are needed
            # the reference its self is assigned on parsing
            return

        self.fields = []  # fields have no order, in some cases rool level values are not unique so dont use a dict

        self.proto_field_defs = []  # proto field definition eg: "field SFColor seatColor .6 .6 .1"
        self.proto_fields = []  # proto field usage "diffuseColor IS seatColor"
        self.children = []
        self.array_data = []  # use for arrays of data - should only be for NODE_ARRAY types

    # Only available from the root node
    '''
    def getFieldDict(self):
        if self.FIELD_NAMESPACE is not None:
            return self.FIELD_NAMESPACE
        else:
            return self.parent.getFieldDict()
    '''
    def getProtoDict(self):
        if self.PROTO_NAMESPACE is not None:
            return self.PROTO_NAMESPACE
        else:
            return self.parent.getProtoDict()

    def getDefDict(self):
        if self.DEF_NAMESPACE is not None:
            return self.DEF_NAMESPACE
        else:
            return self.parent.getDefDict()

    def getRouteIpoDict(self):
        if self.ROUTE_IPO_NAMESPACE is not None:
            return self.ROUTE_IPO_NAMESPACE
        else:
            return self.parent.getRouteIpoDict()

    def setRoot(self, filename):
        self.filename = filename
        # self.FIELD_NAMESPACE =        {}
        self.DEF_NAMESPACE = {}
        self.ROUTE_IPO_NAMESPACE = {}
        self.PROTO_NAMESPACE = {}

    def isRoot(self):
        if self.filename is None:
            return False
        else:
            return True

    def getFilename(self):
        if self.filename:
            return self.filename
        elif self.parent:
            return self.parent.getFilename()
        else:
            return None

    def getRealNode(self):
        if self.reference:
            return self.reference
        else:
            return self

    def getSpec(self):
        self_real = self.getRealNode()
        try:
            return self_real.id[-1]  # its possible this node has no spec
        except:
            return None

    def findSpecRecursive(self, spec):
        self_real = self.getRealNode()
        if spec == self_real.getSpec():
            return self

        for child in self_real.children:
            if child.findSpecRecursive(spec):
                return child

        return None

    def getPrefix(self):
        if self.id:
            return self.id[0]
        return None

    def getSpecialTypeName(self, typename):
        self_real = self.getRealNode()
        try:
            return self_real.id[list(self_real.id).index(typename) + 1]
        except:
            return None

    def getDefName(self):
        return self.getSpecialTypeName('DEF')

    def getProtoName(self):
        return self.getSpecialTypeName('PROTO')

    def getExternprotoName(self):
        return self.getSpecialTypeName('EXTERNPROTO')

    def getChildrenBySpec(self, node_spec):  # spec could be Transform, Shape, Appearance
        self_real = self.getRealNode()
        # using getSpec functions allows us to use the spec of USE children that dont have their spec in their ID
        if type(node_spec) == str:
            return [child for child in self_real.children if child.getSpec() == node_spec]
        else:
            # Check inside a list of optional types
            return [child for child in self_real.children if child.getSpec() in node_spec]

    def getChildrenBySpecCondition(self, cond):  # spec could be Transform, Shape, Appearance
        self_real = self.getRealNode()
        # using getSpec functions allows us to use the spec of USE children that dont have their spec in their ID
        return [child for child in self_real.children if cond(child.getSpec())]

    def getChildBySpec(self, node_spec):  # spec could be Transform, Shape, Appearance
        # Use in cases where there is only ever 1 child of this type
        ls = self.getChildrenBySpec(node_spec)
        if ls:
            return ls[0]
        else:
            return None

    def getChildBySpecCondition(self, cond):  # spec could be Transform, Shape, Appearance
        # Use in cases where there is only ever 1 child of this type
        ls = self.getChildrenBySpecCondition(cond)
        if ls:
            return ls[0]
        else:
            return None

    def getChildrenByName(self, node_name):  # type could be geometry, children, appearance
        self_real = self.getRealNode()
        return [child for child in self_real.children if child.id if child.id[0] == node_name]

    def getChildByName(self, node_name):
        self_real = self.getRealNode()
        for child in self_real.children:
            if child.id and child.id[0] == node_name:  # and child.id[-1]==node_spec:
                return child

    def getSerialized(self, results, ancestry):
        """ Return this node and all its children in a flat list """
        ancestry = ancestry[:]  # always use a copy

        # self_real = self.getRealNode()

        results.append((self, tuple(ancestry)))
        ancestry.append(self)
        for child in self.getRealNode().children:
            if child not in ancestry:
                # We dont want to load proto's, they are only references
                # We could enforce this elsewhere

                # Only add this in a very special case
                # where the parent of this object is not the real parent
                # - In this case we have added the proto as a child to a node instancing it.
                # This is a bit arbitary, but its how Proto's are done with this importer.
                if child.getProtoName() is None and child.getExternprotoName() is None:
                    child.getSerialized(results, ancestry)
                else:

                    if DEBUG:
                        print('getSerialized() is proto:', child.getProtoName(), child.getExternprotoName(), self.getSpec())

                    self_spec = self.getSpec()

                    if child.getProtoName() == self_spec or child.getExternprotoName() == self_spec:
                        #if DEBUG:
                        #    "FoundProto!"
                        child.getSerialized(results, ancestry)

        return results

    def searchNodeTypeID(self, node_spec, results):
        self_real = self.getRealNode()
        # print(self.lineno, self.id)
        if self_real.id and self_real.id[-1] == node_spec:  # use last element, could also be only element
            results.append(self_real)
        for child in self_real.children:
            child.searchNodeTypeID(node_spec, results)
        return results

    def getFieldName(self, field, ancestry, AS_CHILD=False, SPLIT_COMMAS=False):
        self_real = self.getRealNode()  # in case we're an instance

        for f in self_real.fields:
            # print(f)
            if f and f[0] == field:
                # print('\tfound field', f)

                if len(f) >= 3 and f[1] == 'IS':  # eg: 'diffuseColor IS legColor'
                    field_id = f[2]

                    # print("\n\n\n\n\n\nFOND IS!!!")
                    f_proto_lookup = None
                    f_proto_child_lookup = None
                    i = len(ancestry)
                    while i:
                        i -= 1
                        node = ancestry[i]
                        node = node.getRealNode()

                        # proto settings are stored in "self.proto_node"
                        if node.proto_node:
                            # Get the default value from the proto, this can be overwridden by the proto instace
                            # 'field SFColor legColor .8 .4 .7'
                            if AS_CHILD:
                                for child in node.proto_node.children:
                                    #if child.id  and  len(child.id) >= 3  and child.id[2]==field_id:
                                    if child.id and ('point' in child.id or 'points' in child.id):
                                        f_proto_child_lookup = child

                            else:
                                for f_def in node.proto_node.proto_field_defs:
                                    if len(f_def) >= 4:
                                        if f_def[0] == 'field' and f_def[2] == field_id:
                                            f_proto_lookup = f_def[3:]

                        # Node instance, Will be 1 up from the proto-node in the ancestry list. but NOT its parent.
                        # This is the setting as defined by the instance, including this setting is optional,
                        # and will override the default PROTO value
                        # eg: 'legColor 1 0 0'
                        if AS_CHILD:
                            for child in node.children:
                                if child.id and child.id[0] == field_id:
                                    f_proto_child_lookup = child
                        else:
                            for f_def in node.fields:
                                if len(f_def) >= 2:
                                    if f_def[0] == field_id:
                                        if DEBUG:
                                            print("getFieldName(), found proto", f_def)
                                        f_proto_lookup = f_def[1:]

                    if AS_CHILD:
                        if f_proto_child_lookup:
                            if DEBUG:
                                print("getFieldName() - AS_CHILD=True, child found")
                                print(f_proto_child_lookup)
                        return f_proto_child_lookup
                    else:
                        return f_proto_lookup
                else:
                    if AS_CHILD:
                        return None
                    else:
                        # Not using a proto
                        return f[1:]
        # print('\tfield not found', field)

        # See if this is a proto name
        if AS_CHILD:
            for child in self_real.children:
                if child.id and len(child.id) == 1 and child.id[0] == field:
                    return child

        return None

    def getFieldAsInt(self, field, default, ancestry):
        self_real = self.getRealNode()  # in case we're an instance

        f = self_real.getFieldName(field, ancestry)
        if f is None:
            return default
        if ',' in f:
            f = f[:f.index(',')]  # strip after the comma

        if len(f) != 1:
            print('\t"%s" wrong length for int conversion for field "%s"' % (f, field))
            return default

        try:
            return int(f[0])
        except:
            print('\tvalue "%s" could not be used as an int for field "%s"' % (f[0], field))
            return default

    def getFieldAsFloat(self, field, default, ancestry):
        self_real = self.getRealNode()  # in case we're an instance

        f = self_real.getFieldName(field, ancestry)
        if f is None:
            return default
        if ',' in f:
            f = f[:f.index(',')]  # strip after the comma

        if len(f) != 1:
            print('\t"%s" wrong length for float conversion for field "%s"' % (f, field))
            return default

        try:
            return float(f[0])
        except:
            print('\tvalue "%s" could not be used as a float for field "%s"' % (f[0], field))
            return default

    def getFieldAsFloatTuple(self, field, default, ancestry):
        self_real = self.getRealNode()  # in case we're an instance

        f = self_real.getFieldName(field, ancestry)
        if f is None:
            return default
        # if ',' in f: f = f[:f.index(',')] # strip after the comma

        if len(f) < 1:
            print('"%s" wrong length for float tuple conversion for field "%s"' % (f, field))
            return default

        ret = []
        for v in f:
            if v != ',':
                try:
                    ret.append(float(v))
                except:
                    break  # quit of first non float, perhaps its a new field name on the same line? - if so we are going to ignore it :/ TODO
        # print(ret)

        if ret:
            return ret
        if not ret:
            print('\tvalue "%s" could not be used as a float tuple for field "%s"' % (f, field))
            return default

    def getFieldAsBool(self, field, default, ancestry):
        self_real = self.getRealNode()  # in case we're an instance

        f = self_real.getFieldName(field, ancestry)
        if f is None:
            return default
        if ',' in f:
            f = f[:f.index(',')]  # strip after the comma

        if len(f) != 1:
            print('\t"%s" wrong length for bool conversion for field "%s"' % (f, field))
            return default

        if f[0].upper() == '"TRUE"' or f[0].upper() == 'TRUE':
            return True
        elif f[0].upper() == '"FALSE"' or f[0].upper() == 'FALSE':
            return False
        else:
            print('\t"%s" could not be used as a bool for field "%s"' % (f[1], field))
            return default

    def getFieldAsString(self, field, default, ancestry):
        self_real = self.getRealNode()  # in case we're an instance

        f = self_real.getFieldName(field, ancestry)
        if f is None:
            return default
        if len(f) < 1:
            print('\t"%s" wrong length for string conversion for field "%s"' % (f, field))
            return default

        if len(f) > 1:
            # String may contain spaces
            st = ' '.join(f)
        else:
            st = f[0]

        # X3D HACK
        if self.x3dNode:
            return st

        if st[0] == '"' and st[-1] == '"':
            return st[1:-1]
        else:
            print('\tvalue "%s" could not be used as a string for field "%s"' % (f[0], field))
            return default

    def getFieldAsArray(self, field, group, ancestry):
        """
        For this parser arrays are children
        """

        def array_as_number(array_string):
            array_data = []
            try:
                array_data = [int(val, 0) for val in array_string]
            except:
                try:
                    array_data = [float(val) for val in array_string]
                except:
                    print('\tWarning, could not parse array data from field')

            return array_data

        self_real = self.getRealNode()  # in case we're an instance

        child_array = self_real.getFieldName(field, ancestry, True, SPLIT_COMMAS=True)

        #if type(child_array)==list: # happens occasionaly
        #   array_data = child_array

        if child_array is None:
            # For x3d, should work ok with vrml too
            # for x3d arrays are fields, vrml they are nodes, annoying but not tooo bad.
            data_split = self.getFieldName(field, ancestry, SPLIT_COMMAS=True)
            if not data_split:
                return []

            array_data = array_as_number(data_split)

        elif type(child_array) == list:
            # x3d creates these
            array_data = array_as_number(child_array)
        else:
            # print(child_array)
            # Normal vrml
            array_data = child_array.array_data

        # print('array_data', array_data)
        if group == -1 or len(array_data) == 0:
            return array_data

        # We want a flat list
        flat = True
        for item in array_data:
            if type(item) == list:
                flat = False
                break

        # make a flat array
        if flat:
            flat_array = array_data  # we are already flat.
        else:
            flat_array = []

            def extend_flat(ls):
                for item in ls:
                    if type(item) == list:
                        extend_flat(item)
                    else:
                        flat_array.append(item)

            extend_flat(array_data)

        # We requested a flat array
        if group == 0:
            return flat_array

        new_array = []
        sub_array = []

        for item in flat_array:
            sub_array.append(item)
            if len(sub_array) == group:
                new_array.append(sub_array)
                sub_array = []

        if sub_array:
            print('\twarning, array was not aligned to requested grouping', group, 'remaining value', sub_array)

        return new_array

    def getFieldAsStringArray(self, field, ancestry):
        """
        Get a list of strings
        """
        self_real = self.getRealNode()  # in case we're an instance

        child_array = None
        for child in self_real.children:
            if child.id and len(child.id) == 1 and child.id[0] == field:
                child_array = child
                break
        if not child_array:
            return []

        # each string gets its own list, remove ""'s
        try:
            new_array = [f[0][1:-1] for f in child_array.fields]
        except:
            print('\twarning, string array could not be made')
            new_array = []

        return new_array

    def getLevel(self):
        # Ignore self_real
        level = 0
        p = self.parent
        while p:
            level += 1
            p = p.parent
            if not p:
                break

        return level

    def __repr__(self):
        level = self.getLevel()
        ind = '  ' * level
        if self.node_type == NODE_REFERENCE:
            brackets = ''
        elif self.node_type == NODE_NORMAL:
            brackets = '{}'
        else:
            brackets = '[]'

        if brackets:
            text = ind + brackets[0] + '\n'
        else:
            text = ''

        text += ind + 'ID: ' + str(self.id) + ' ' + str(level) + (' lineno %d\n' % self.lineno)

        if self.node_type == NODE_REFERENCE:
            text += ind + "(reference node)\n"
            return text

        if self.proto_node:
            text += ind + 'PROTO NODE...\n'
            text += str(self.proto_node)
            text += ind + 'PROTO NODE_DONE\n'

        text += ind + 'FIELDS:' + str(len(self.fields)) + '\n'

        for i, item in enumerate(self.fields):
            text += ind + 'FIELD:\n'
            text += ind + str(item) + '\n'

        text += ind + 'PROTO_FIELD_DEFS:' + str(len(self.proto_field_defs)) + '\n'

        for i, item in enumerate(self.proto_field_defs):
            text += ind + 'PROTO_FIELD:\n'
            text += ind + str(item) + '\n'

        text += ind + 'ARRAY: ' + str(len(self.array_data)) + ' ' + str(self.array_data) + '\n'
        #text += ind + 'ARRAY: ' + str(len(self.array_data)) + '[...] \n'

        text += ind + 'CHILDREN: ' + str(len(self.children)) + '\n'
        for i, child in enumerate(self.children):
            text += ind + ('CHILD%d:\n' % i)
            text += str(child)

        text += '\n' + ind + brackets[1]

        return text

    def parse(self, i, IS_PROTO_DATA=False):
        new_i = self.__parse(i, IS_PROTO_DATA)

        # print(self.id, self.getFilename())

        # Check if this node was an inline or externproto

        url_ls = []

        if self.node_type == NODE_NORMAL and self.getSpec() == 'Inline':
            ancestry = []  # Warning! - PROTO's using this wont work at all.
            url = self.getFieldAsString('url', None, ancestry)
            if url:
                url_ls = [(url, None)]
            del ancestry

        elif self.getExternprotoName():
            # externproto
            url_ls = []
            for f in self.fields:

                if type(f) == str:
                    f = [f]

                for ff in f:
                    for f_split in ff.split('"'):
                        # print(f_split)
                        # "someextern.vrml#SomeID"
                        if '#' in f_split:

                            f_split, f_split_id = f_split.split('#')  # there should only be 1 # anyway

                            url_ls.append((f_split, f_split_id))
                        else:
                            url_ls.append((f_split, None))

        # Was either an Inline or an EXTERNPROTO
        if url_ls:

            # print(url_ls)

            for url, extern_key in url_ls:
                print(url)
                urls = []
                urls.append(url)
                urls.append(bpy.path.resolve_ncase(urls[-1]))

                urls.append(os.path.join(os.path.dirname(self.getFilename()), url))
                urls.append(bpy.path.resolve_ncase(urls[-1]))

                urls.append(os.path.join(os.path.dirname(self.getFilename()), os.path.basename(url)))
                urls.append(bpy.path.resolve_ncase(urls[-1]))

                try:
                    url = [url for url in urls if os.path.exists(url)][0]
                    url_found = True
                except:
                    url_found = False

                if not url_found:
                    print('\tWarning: Inline URL could not be found:', url)
                else:
                    if url == self.getFilename():
                        print('\tWarning: cant Inline yourself recursively:', url)
                    else:

                        try:
                            data = gzipOpen(url)
                        except:
                            print('\tWarning: cant open the file:', url)
                            data = None

                        if data:
                            # Tricky - inline another VRML
                            print('\tLoading Inline:"%s"...' % url)

                            # Watch it! - backup lines
                            lines_old = lines[:]

                            lines[:] = vrmlFormat(data)

                            lines.insert(0, '{')
                            lines.insert(0, 'root_node____')
                            lines.append('}')
                            '''
                            ff = open('/tmp/test.txt', 'w')
                            ff.writelines([l+'\n' for l in lines])
                            '''

                            child = vrmlNode(self, NODE_NORMAL, -1)
                            child.setRoot(url)  # initialized dicts
                            child.parse(0)

                            # if self.getExternprotoName():
                            if self.getExternprotoName():
                                if not extern_key:  # if none is spesified - use the name
                                    extern_key = self.getSpec()

                                if extern_key:

                                    self.children.remove(child)
                                    child.parent = None

                                    extern_child = child.findSpecRecursive(extern_key)

                                    if extern_child:
                                        self.children.append(extern_child)
                                        extern_child.parent = self

                                        if DEBUG:
                                            print("\tEXTERNPROTO ID found!:", extern_key)
                                    else:
                                        print("\tEXTERNPROTO ID not found!:", extern_key)

                            # Watch it! - restore lines
                            lines[:] = lines_old

        return new_i

    def __parse(self, i, IS_PROTO_DATA=False):
        '''
        print('parsing at', i, end="")
        print(i, self.id, self.lineno)
        '''
        l = lines[i]

        if l == '[':
            # An anonymous list
            self.id = None
            i += 1
        else:
            words = []

            node_type, new_i = is_nodeline(i, words)
            if not node_type:  # fail for parsing new node.
                print("Failed to parse new node")
                raise ValueError

            if self.node_type == NODE_REFERENCE:
                # Only assign the reference and quit
                key = words[words.index('USE') + 1]
                self.id = (words[0],)

                self.reference = self.getDefDict()[key]
                return new_i

            self.id = tuple(words)

            # fill in DEF/USE
            key = self.getDefName()
            if key is not None:
                self.getDefDict()[key] = self

            key = self.getProtoName()
            if not key:
                key = self.getExternprotoName()

            proto_dict = self.getProtoDict()
            if key is not None:
                proto_dict[key] = self

                # Parse the proto nodes fields
                self.proto_node = vrmlNode(self, NODE_ARRAY, new_i)
                new_i = self.proto_node.parse(new_i)

                self.children.remove(self.proto_node)

                # print(self.proto_node)

                new_i += 1  # skip past the {

            else:  # If we're a proto instance, add the proto node as our child.
                spec = self.getSpec()
                try:
                    self.children.append(proto_dict[spec])
                    #pass
                except:
                    pass

                del spec

            del proto_dict, key

            i = new_i

        # print(self.id)
        ok = True
        while ok:
            if i >= len(lines):
                return len(lines) - 1

            l = lines[i]
            # print('\tDEBUG:', i, self.node_type, l)
            if l == '':
                i += 1
                continue

            if l == '}':
                if self.node_type != NODE_NORMAL:  # also ends proto nodes, we may want a type for these too.
                    print('wrong node ending, expected an } ' + str(i) + ' ' + str(self.node_type))
                    if DEBUG:
                        raise ValueError
                ### print("returning", i)
                return i + 1
            if l == ']':
                if self.node_type != NODE_ARRAY:
                    print('wrong node ending, expected a ] ' + str(i) + ' ' + str(self.node_type))
                    if DEBUG:
                        raise ValueError
                ### print("returning", i)
                return i + 1

            node_type, new_i = is_nodeline(i, [])
            if node_type:  # check text\n{
                child = vrmlNode(self, node_type, i)
                i = child.parse(i)

            elif l == '[':  # some files have these anonymous lists
                child = vrmlNode(self, NODE_ARRAY, i)
                i = child.parse(i)

            elif is_numline(i):
                l_split = l.split(',')

                values = None
                # See if each item is a float?

                for num_type in (int, float):
                    try:
                        values = [num_type(v) for v in l_split]
                        break
                    except:
                        pass

                    try:
                        values = [[num_type(v) for v in segment.split()] for segment in l_split]
                        break
                    except:
                        pass

                if values is None:  # dont parse
                    values = l_split

                # This should not extend over multiple lines however it is possible
                # print(self.array_data)
                if values:
                    self.array_data.extend(values)
                i += 1
            else:
                words = l.split()
                if len(words) > 2 and words[1] == 'USE':
                    vrmlNode(self, NODE_REFERENCE, i)
                else:

                    # print("FIELD", i, l)
                    #
                    #words = l.split()
                    ### print('\t\ttag', i)
                    # this is a tag/
                    # print(words, i, l)
                    value = l
                    # print(i)
                    # javastrips can exist as values.
                    quote_count = l.count('"')
                    if quote_count % 2:  # odd number?
                        # print('MULTILINE')
                        while 1:
                            i += 1
                            l = lines[i]
                            quote_count = l.count('"')
                            if quote_count % 2:  # odd number?
                                value += '\n' + l[:l.rfind('"')]
                                break  # assume
                            else:
                                value += '\n' + l

                    # use shlex so we get '"a b" "b v"' --> '"a b"', '"b v"'
                    value_all = shlex.split(value, posix=False)

                    for value in vrml_split_fields(value_all):
                        # Split

                        if value[0] == 'field':
                            # field SFFloat creaseAngle 4
                            self.proto_field_defs.append(value)
                        else:
                            self.fields.append(value)
                i += 1

    # This is a prerequisite for DEF/USE-based material caching
    def canHaveReferences(self):
        return self.node_type == NODE_NORMAL and self.getDefName()

    # This is a prerequisite for raw XML-based material caching. For now, only for X3D
    def desc(self):
        return None


def gzipOpen(path):
    import gzip

    data = None
    try:
        data = gzip.open(path, 'r').read()
    except:
        pass

    if data is None:
        try:
            filehandle = open(path, 'rU', encoding='utf-8', errors='surrogateescape')
            data = filehandle.read()
            filehandle.close()
        except:
            import traceback
            traceback.print_exc()
    else:
        data = data.decode(encoding='utf-8', errors='surrogateescape')

    return data


def vrml_parse(path):
    """
    Sets up the root node and returns it so load_web3d() can deal with the blender side of things.
    Return root (vrmlNode, '') or (None, 'Error String')
    """
    data = gzipOpen(path)

    if data is None:
        return None, 'Failed to open file: ' + path

    # Stripped above
    lines[:] = vrmlFormat(data)

    lines.insert(0, '{')
    lines.insert(0, 'dymmy_node')
    lines.append('}')
    # Use for testing our parsed output, so we can check on line numbers.

    '''
    ff = open('/tmp/test.txt', 'w')
    ff.writelines([l+'\n' for l in lines])
    ff.close()
    '''

    # Now evaluate it
    node_type, new_i = is_nodeline(0, [])
    if not node_type:
        return None, 'Error: VRML file has no starting Node'

    # Trick to make sure we get all root nodes.
    lines.insert(0, '{')
    lines.insert(0, 'root_node____')  # important the name starts with an ascii char
    lines.append('}')

    root = vrmlNode(None, NODE_NORMAL, -1)
    root.setRoot(path)  # we need to set the root so we have a namespace and know the path in case of inlineing

    # Parse recursively
    root.parse(0)

    # This prints a load of text
    if DEBUG:
        print(root)

    return root, ''


# ====================== END VRML

# ====================== X3d Support

# Sane as vrml but replace the parser
class x3dNode(vrmlNode):
    def __init__(self, parent, node_type, x3dNode):
        vrmlNode.__init__(self, parent, node_type, -1)
        self.x3dNode = x3dNode

    def parse(self, IS_PROTO_DATA=False):
        # print(self.x3dNode.tagName)
        self.lineno = self.x3dNode.parse_position[0]

        define = self.x3dNode.getAttributeNode('DEF')
        if define:
            self.getDefDict()[define.value] = self
        else:
            use = self.x3dNode.getAttributeNode('USE')
            if use:
                try:
                    self.reference = self.getDefDict()[use.value]
                    self.node_type = NODE_REFERENCE
                except:
                    print('\tWarning: reference', use.value, 'not found')
                    self.parent.children.remove(self)

                return

        for x3dChildNode in self.x3dNode.childNodes:
            if x3dChildNode.nodeType in {x3dChildNode.TEXT_NODE, x3dChildNode.COMMENT_NODE, x3dChildNode.CDATA_SECTION_NODE}:
                continue

            node_type = NODE_NORMAL
            # print(x3dChildNode, dir(x3dChildNode))
            if x3dChildNode.getAttributeNode('USE'):
                node_type = NODE_REFERENCE

            child = x3dNode(self, node_type, x3dChildNode)
            child.parse()

        # TODO - x3d Inline

    def getSpec(self):
        return self.x3dNode.tagName  # should match vrml spec

    # Used to retain object identifiers from X3D to Blender
    def getDefName(self):
        node_id = self.x3dNode.getAttributeNode('DEF')
        if node_id:
            return node_id.value
        node_id = self.x3dNode.getAttributeNode('USE')
        if node_id:
            return "USE_" + node_id.value
        return None

    # Other funcs operate from vrml, but this means we can wrap XML fields, still use nice utility funcs
    # getFieldAsArray getFieldAsBool etc
    def getFieldName(self, field, ancestry, AS_CHILD=False, SPLIT_COMMAS=False):
        # ancestry and AS_CHILD are ignored, only used for VRML now

        self_real = self.getRealNode()  # in case we're an instance
        field_xml = self.x3dNode.getAttributeNode(field)
        if field_xml:
            value = field_xml.value

            # We may want to edit. for x3d specific stuff
            # Sucks a bit to return the field name in the list but vrml excepts this :/
            if SPLIT_COMMAS:
                value = value.replace(",", " ")
            return value.split()
        else:
            return None

    def canHaveReferences(self):
        return self.x3dNode.getAttributeNode('DEF')

    def desc(self):
        return self.getRealNode().x3dNode.toxml()


def x3d_parse(path):
    """
    Sets up the root node and returns it so load_web3d() can deal with the blender side of things.
    Return root (x3dNode, '') or (None, 'Error String')
    """
    import xml.dom.minidom
    import xml.sax

    '''
    try:    doc = xml.dom.minidom.parse(path)
    except: return None, 'Could not parse this X3D file, XML error'
    '''

    # Could add a try/except here, but a console error is more useful.
    data = gzipOpen(path)

    if data is None:
        return None, 'Failed to open file: ' + path

    # Enable line number reporting in the parser - kinda brittle
    def set_content_handler(dom_handler):
        def startElementNS(name, tagName, attrs):
            orig_start_cb(name, tagName, attrs)
            cur_elem = dom_handler.elementStack[-1]
            cur_elem.parse_position = (parser._parser.CurrentLineNumber, parser._parser.CurrentColumnNumber)

        orig_start_cb = dom_handler.startElementNS
        dom_handler.startElementNS = startElementNS
        orig_set_content_handler(dom_handler)

    parser = xml.sax.make_parser()
    orig_set_content_handler = parser.setContentHandler
    parser.setContentHandler = set_content_handler

    doc = xml.dom.minidom.parseString(data, parser)

    try:
        x3dnode = doc.getElementsByTagName('X3D')[0]
    except:
        return None, 'Not a valid x3d document, cannot import'

    bpy.ops.object.select_all(action='DESELECT')

    root = x3dNode(None, NODE_NORMAL, x3dnode)
    root.setRoot(path)  # so images and Inline's we load have a relative path
    root.parse()

    return root, ''

## f = open('/_Cylinder.wrl', 'r')
# f = open('/fe/wrl/Vrml/EGS/TOUCHSN.WRL', 'r')
# vrml_parse('/fe/wrl/Vrml/EGS/TOUCHSN.WRL')
#vrml_parse('/fe/wrl/Vrml/EGS/SCRIPT.WRL')
'''
import os
files = os.popen('find /fe/wrl -iname "*.wrl"').readlines()
files.sort()
tot = len(files)
for i, f in enumerate(files):
    #if i < 801:
    #   continue

    f = f.strip()
    print(f, i, tot)
    vrml_parse(f)
'''

# NO BLENDER CODE ABOVE THIS LINE.
# -----------------------------------------------------------------------------------
import bpy
from bpy_extras import image_utils
from mathutils import Vector, Matrix, Quaternion

GLOBALS = {'CIRCLE_DETAIL': 16}


def translateRotation(rot):
    """ axis, angle """
    return Matrix.Rotation(rot[3], 4, Vector(rot[:3]))


def translateScale(sca):
    mat = Matrix()  # 4x4 default
    mat[0][0] = sca[0]
    mat[1][1] = sca[1]
    mat[2][2] = sca[2]
    return mat


def translateTransform(node, ancestry):
    cent = node.getFieldAsFloatTuple('center', None, ancestry)  # (0.0, 0.0, 0.0)
    rot = node.getFieldAsFloatTuple('rotation', None, ancestry)  # (0.0, 0.0, 1.0, 0.0)
    sca = node.getFieldAsFloatTuple('scale', None, ancestry)  # (1.0, 1.0, 1.0)
    scaori = node.getFieldAsFloatTuple('scaleOrientation', None, ancestry)  # (0.0, 0.0, 1.0, 0.0)
    tx = node.getFieldAsFloatTuple('translation', None, ancestry)  # (0.0, 0.0, 0.0)

    if cent:
        cent_mat = Matrix.Translation(cent)
        cent_imat = cent_mat.inverted()
    else:
        cent_mat = cent_imat = None

    if rot:
        rot_mat = translateRotation(rot)
    else:
        rot_mat = None

    if sca:
        sca_mat = translateScale(sca)
    else:
        sca_mat = None

    if scaori:
        scaori_mat = translateRotation(scaori)
        scaori_imat = scaori_mat.inverted()
    else:
        scaori_mat = scaori_imat = None

    if tx:
        tx_mat = Matrix.Translation(tx)
    else:
        tx_mat = None

    new_mat = Matrix()

    mats = [tx_mat, cent_mat, rot_mat, scaori_mat, sca_mat, scaori_imat, cent_imat]
    for mtx in mats:
        if mtx:
            new_mat = new_mat * mtx

    return new_mat


def translateTexTransform(node, ancestry):
    cent = node.getFieldAsFloatTuple('center', None, ancestry)  # (0.0, 0.0)
    rot = node.getFieldAsFloat('rotation', None, ancestry)  # 0.0
    sca = node.getFieldAsFloatTuple('scale', None, ancestry)  # (1.0, 1.0)
    tx = node.getFieldAsFloatTuple('translation', None, ancestry)  # (0.0, 0.0)

    if cent:
        # cent is at a corner by default
        cent_mat = Matrix.Translation(Vector(cent).to_3d())
        cent_imat = cent_mat.inverted()
    else:
        cent_mat = cent_imat = None

    if rot:
        rot_mat = Matrix.Rotation(rot, 4, 'Z')  # translateRotation(rot)
    else:
        rot_mat = None

    if sca:
        sca_mat = translateScale((sca[0], sca[1], 0.0))
    else:
        sca_mat = None

    if tx:
        tx_mat = Matrix.Translation(Vector(tx).to_3d())
    else:
        tx_mat = None

    new_mat = Matrix()

    # as specified in VRML97 docs
    mats = [cent_imat, sca_mat, rot_mat, cent_mat, tx_mat]

    for mtx in mats:
        if mtx:
            new_mat = new_mat * mtx

    return new_mat

def getFinalMatrix(node, mtx, ancestry, global_matrix):

    transform_nodes = [node_tx for node_tx in ancestry if node_tx.getSpec() == 'Transform']
    if node.getSpec() == 'Transform':
        transform_nodes.append(node)
    transform_nodes.reverse()

    if mtx is None:
        mtx = Matrix()

    for node_tx in transform_nodes:
        mat = translateTransform(node_tx, ancestry)
        mtx = mat * mtx

    # worldspace matrix
    mtx = global_matrix * mtx

    return mtx


# -----------------------------------------------------------------------------------
# Mesh import utilities

# Assumes that the mesh has tessfaces - doesn't support polygons.
# Also assumes that tessfaces are all triangles.
# Assumes that the sequence of the mesh vertices array matches
# the source file. For indexed meshes, that's almost a given;
# for nonindexed ones, this is a consideration.


def importMesh_ApplyColors(bpymesh, geom, ancestry):
    colors = geom.getChildBySpec(['ColorRGBA', 'Color'])
    if colors:
        if colors.getSpec() == 'ColorRGBA':
            # Array of arrays; no need to flatten
            rgb = [c[:3] for c
                   in colors.getFieldAsArray('color', 4, ancestry)]
        else:
            rgb = colors.getFieldAsArray('color', 3, ancestry)
        tc = bpymesh.tessface_vertex_colors.new()
        tc.data.foreach_set("color1", [i for face
                                       in bpymesh.tessfaces
                                       for i in rgb[face.vertices[0]]])
        tc.data.foreach_set("color2", [i for face
                                       in bpymesh.tessfaces
                                       for i in rgb[face.vertices[1]]])
        tc.data.foreach_set("color3", [i for face
                                       in bpymesh.tessfaces
                                       for i in rgb[face.vertices[2]]])


# Assumes that the vertices have not been rearranged compared to the
# source file order # or in the order assumed by the spec (e. g. in
# Elevation, in rows by x).
# Assumes tessfaces have been set, doesn't support polygons.
def importMesh_ApplyNormals(bpymesh, geom, ancestry):
    normals = geom.getChildBySpec('Normal')
    if not normals:
        return

    per_vertex = geom.getFieldAsBool('normalPerVertex', True, ancestry)
    vectors = normals.getFieldAsArray('vector', 0, ancestry)
    if per_vertex:
        bpymesh.vertices.foreach_set("normal", vectors)
    else:
        bpymesh.tessfaces.foreach_set("normal", vectors)


# Reads the standard Coordinate object - common for all mesh elements
# Feeds the vertices in the mesh.
# Rearranging the vertex order is a bad idea - other elements
# in X3D might rely on it,  if you need to rearrange, please play with
# vertex indices in the tessfaces/polygons instead.
#
# Vertex culling that we have in IndexedFaceSet is an unfortunate exception,
# brought forth by a very specific issue.
def importMesh_ReadVertices(bpymesh, geom, ancestry):
    # We want points here as a flat array, but the caching logic in
    # IndexedFaceSet presumes a 2D one.
    # The case for caching is stronger over there.
    coord = geom.getChildBySpec('Coordinate')
    points = coord.getFieldAsArray('point', 0, ancestry)
    bpymesh.vertices.add(len(points) // 3)
    bpymesh.vertices.foreach_set("co", points)


# Assumes the mesh only contains triangular tessfaces, and the order
# of vertices matches the source file.
# Relies upon texture coordinates in the X3D node; if a coordinate generation
# algorithm for a geometry is in the spec (e. g. for ElevationGrid), it needs
# to be implemeted by the geometry handler.
#
# Texture transform is applied in ProcessObject.
def importMesh_ApplyTextureToTessfaces(bpymesh, geom, ancestry, bpyima):
    if not bpyima:
        return

    tex_coord = geom.getChildBySpec('TextureCoordinate')
    if not tex_coord:
        return

    coord_points = tex_coord.getFieldAsArray('point', 2, ancestry)
    if not coord_points:
        return

    d = bpymesh.tessface_uv_textures.new().data
    for face in d:  # No foreach_set for nonscalars
        face.image = bpyima
    uv = [i for face in bpymesh.tessfaces
          for vno in range(3) for i in coord_points[face.vertices[vno]]]
    d.foreach_set('uv', uv)


# Common steps for all triangle meshes once the geometry has been set:
# normals, vertex colors, and texture.
def importMesh_FinalizeTriangleMesh(bpymesh, geom, ancestry, bpyima):
    importMesh_ApplyNormals(bpymesh, geom, ancestry)
    importMesh_ApplyColors(bpymesh, geom, ancestry)
    importMesh_ApplyTextureToTessfaces(bpymesh, geom, ancestry, bpyima)
    bpymesh.validate()
    bpymesh.update()
    return bpymesh


# Assumes that the mesh is stored as polygons and loops, and the premade array
# of texture coordinates follows the loop array.
# The loops array must be flat.
def importMesh_ApplyTextureToLoops(bpymesh, bpyima, loops):
    d = bpymesh.uv_textures.new().data
    for f in d:
        f.image = bpyima
    bpymesh.uv_layers[0].data.foreach_set('uv', loops)


def flip(r, ccw):
    return r if ccw else r[::-1]

# -----------------------------------------------------------------------------------
# Now specific geometry importers


def importMesh_IndexedTriangleSet(geom, ancestry, bpyima):
    # Ignoring solid
    # colorPerVertex is always true
    ccw = geom.getFieldAsBool('ccw', True, ancestry)

    bpymesh = bpy.data.meshes.new(name="XXX")
    importMesh_ReadVertices(bpymesh, geom, ancestry)

    # Read the faces
    index = geom.getFieldAsArray('index', 0, ancestry)
    n = len(index) // 3
    if not ccw:
        index = [index[3 * i + j] for i in range(n) for j in (1, 0, 2)]
    bpymesh.tessfaces.add(n)
    bpymesh.tessfaces.foreach_set("vertices", index)

    return importMesh_FinalizeTriangleMesh(bpymesh, geom, ancestry, bpyima)


def importMesh_IndexedTriangleStripSet(geom, ancestry, bpyima):
    # Ignoring solid
    # colorPerVertex is always true
    cw = 0 if geom.getFieldAsBool('ccw', True, ancestry) else 1
    bpymesh = bpy.data.meshes.new(name="IndexedTriangleStripSet")
    importMesh_ReadVertices(bpymesh, geom, ancestry)

    # Read the faces
    index = geom.getFieldAsArray('index', 0, ancestry)
    while index[-1] == -1:
        del index[-1]
    ngaps = sum(1 for i in index if i == -1)
    bpymesh.tessfaces.add(len(index) - 2 - 3 * ngaps)

    def triangles():
        i = 0
        odd = cw
        while True:
            yield index[i + odd]
            yield index[i + 1 - odd]
            yield index[i + 2]
            odd = 1 - odd
            i += 1
            if i + 2 >= len(index):
                return
            if index[i + 2] == -1:
                i += 3
                odd = cw
    bpymesh.tessfaces.foreach_set("vertices", [f for f in triangles()])
    return importMesh_FinalizeTriangleMesh(bpymesh, geom, ancestry, bpyima)


def importMesh_IndexedTriangleFanSet(geom, ancestry, bpyima):
    # Ignoring solid
    # colorPerVertex is always true
    cw = 0 if geom.getFieldAsBool('ccw', True, ancestry) else 1
    bpymesh = bpy.data.meshes.new(name="IndexedTriangleFanSet")
    importMesh_ReadVertices(bpymesh, geom, ancestry)

    # Read the faces
    index = geom.getFieldAsArray('index', 0, ancestry)
    while index[-1] == -1:
        del index[-1]
    ngaps = sum(1 for i in index if i == -1)
    bpymesh.tessfaces.add(len(index) - 2 - 3 * ngaps)

    def triangles():
        i = 0
        j = 1
        while True:
            yield index[i]
            yield index[i + j + cw]
            yield index[i + j + 1 - cw]
            j += 1
            if i + j + 1 >= len(index):
                return
            if index[i + j + 1] == -1:
                i = j + 2
                j = 1
    bpymesh.tessfaces.foreach_set("vertices", [f for f in triangles()])
    return importMesh_FinalizeTriangleMesh(bpymesh, geom, ancestry, bpyima)


def importMesh_TriangleSet(geom, ancestry, bpyima):
    # Ignoring solid
    # colorPerVertex is always true
    ccw = geom.getFieldAsBool('ccw', True, ancestry)
    bpymesh = bpy.data.meshes.new(name="TriangleSet")
    importMesh_ReadVertices(bpymesh, geom, ancestry)
    n = len(bpymesh.vertices)
    bpymesh.tessfaces.add(n // 3)
    if ccw:
        fv = [i for i in range(n)]
    else:
        fv = [3 * i + j for i in range(n // 3) for j in (1, 0, 2)]
    bpymesh.tessfaces.foreach_set("vertices", fv)

    return importMesh_FinalizeTriangleMesh(bpymesh, geom, ancestry, bpyima)


def importMesh_TriangleStripSet(geom, ancestry, bpyima):
    # Ignoring solid
    # colorPerVertex is always true
    cw = 0 if geom.getFieldAsBool('ccw', True, ancestry) else 1
    bpymesh = bpy.data.meshes.new(name="TriangleStripSet")
    importMesh_ReadVertices(bpymesh, geom, ancestry)
    counts = geom.getFieldAsArray('stripCount', 0, ancestry)
    bpymesh.tessfaces.add(sum([n - 2 for n in counts]))

    def triangles():
        b = 0
        for i in range(0, len(counts)):
            for j in range(0, counts[i] - 2):
                yield b + j + (j + cw) % 2
                yield b + j + 1 - (j + cw) % 2
                yield b + j + 2
            b += counts[i]
    bpymesh.tessfaces.foreach_set("vertices", [x for x in triangles()])

    return importMesh_FinalizeTriangleMesh(bpymesh, geom, ancestry, bpyima)


def importMesh_TriangleFanSet(geom, ancestry, bpyima):
    # Ignoring solid
    # colorPerVertex is always true
    cw = 0 if geom.getFieldAsBool('ccw', True, ancestry) else 1
    bpymesh = bpy.data.meshes.new(name="TriangleStripSet")
    importMesh_ReadVertices(bpymesh, geom, ancestry)
    counts = geom.getFieldAsArray('fanCount', 0, ancestry)
    bpymesh.tessfaces.add(sum([n - 2 for n in counts]))

    def triangles():
        b = 0
        for i in range(0, len(counts)):
            for j in range(1, counts[i] - 1):
                yield b
                yield b + j + cw
                yield b + j + 1 - cw
            b += counts[i]
    bpymesh.tessfaces.foreach_set("vertices", [x for x in triangles()])
    return importMesh_FinalizeTriangleMesh(bpymesh, geom, ancestry, bpyima)


def importMesh_IndexedFaceSet(geom, ancestry, bpyima):
    # Saw the following structure in X3Ds: the first mesh has a huge set
    # of vertices and a reasonably sized index. The rest of the meshes
    # reference the Coordinate node from the first one, and have their
    # own reasonably sized indices.
    #
    # In Blender, to the best of my knowledge, there's no way to reuse
    # the vertex set between meshes. So we have culling logic instead -
    # for each mesh, only leave vertices that are used for faces.

    ccw = geom.getFieldAsBool('ccw', True, ancestry)
    coord = geom.getChildBySpec('Coordinate')
    if coord.reference:
        points = coord.getRealNode().parsed
        # We need unflattened coord array here, while
        # importMesh_ReadVertices uses flattened. Can't cache both :(
        # TODO: resolve that somehow, so that vertex set can be effectively
        # reused between different mesh types?
    else:
        points = coord.getFieldAsArray('point', 3, ancestry)
        if coord.canHaveReferences():
            coord.parsed = points
    index = geom.getFieldAsArray('coordIndex', 0, ancestry)

    while index and index[-1] == -1:
        del index[-1]

    if len(points) >= 2 * len(index):  # Need to cull
        culled_points = []
        cull = {}  # Maps old vertex indices to new ones
        uncull = []  # Maps new indices to the old ones
        new_index = 0
    else:
        uncull = cull = None

    faces = []
    face = []
    # Generate faces. Cull the vertices if necessary,
    for i in index:
        if i == -1:
            if face:
                faces.append(flip(face, ccw))
            face = []
        else:
            if cull is not None:
                if not(i in cull):
                    culled_points.append(points[i])
                    cull[i] = new_index
                    uncull.append(i)
                    i = new_index
                    new_index += 1
                else:
                    i = cull[i]
            face.append(i)
    if face:
        faces.append(flip(face, ccw))  # The last face

    if cull:
        points = culled_points

    bpymesh = bpy.data.meshes.new(name="IndexedFaceSet")
    bpymesh.from_pydata(points, [], faces)
    # No validation here. It throws off the per-face stuff.

    # Similar treatment for normal and color indices

    def processPerVertexIndex(ind):
        if ind:
            # Deflatten into an array of arrays by face; the latter might
            # need to be flipped
            i = 0
            verts_by_face = []
            for f in faces:
                verts_by_face.append(flip(ind[i:i + len(f)], ccw))
                i += len(f) + 1
            return verts_by_face
        elif uncull:
            return [[uncull[v] for v in f] for f in faces]
        else:
            return faces  # Reuse coordIndex, as per the spec

    # Normals
    normals = geom.getChildBySpec('Normal')
    if normals:
        per_vertex = geom.getFieldAsBool('normalPerVertex', True, ancestry)
        vectors = normals.getFieldAsArray('vector', 3, ancestry)
        normal_index = geom.getFieldAsArray('normalIndex', 0, ancestry)
        if per_vertex:
            co = [co for f in processPerVertexIndex(normal_index)
                  for v in f for co in vectors[v]]
            bpymesh.vertices.foreach_set("normal", co)
        else:
            co = [co for (i, f) in enumerate(faces) for j in f
                  for co in vectors[normal_index[i] if normal_index else i]]
            bpymesh.polygons.foreach_set("normal", co)

    # Apply vertex/face colors
    colors = geom.getChildBySpec(['ColorRGBA', 'Color'])
    if colors:
        if colors.getSpec() == 'ColorRGBA':
            # Array of arrays; no need to flatten
            rgb = [c[:3] for c
                   in colors.getFieldAsArray('color', 4, ancestry)]
        else:
            rgb = colors.getFieldAsArray('color', 3, ancestry)

        color_per_vertex = geom.getFieldAsBool('colorPerVertex',
                                               True, ancestry)
        color_index = geom.getFieldAsArray('colorIndex', 0, ancestry)

        d = bpymesh.vertex_colors.new().data
        if color_per_vertex:
            cco = [cco for f in processPerVertexIndex(color_index)
                   for v in f for cco in rgb[v]]
        elif color_index:  # Color per face with index
            cco = [cco for (i, f) in enumerate(faces) for j in f
                   for cco in rgb[color_index[i]]]
        else:  # Color per face without index
            cco = [cco for (i, f) in enumerate(faces) for j in f
                   for cco in rgb[i]]
        d.foreach_set('color', cco)

    # Texture
    if bpyima:
        tex_coord = geom.getChildBySpec('TextureCoordinate')
        if tex_coord:
            tex_coord_points = tex_coord.getFieldAsArray('point', 2, ancestry)
            tex_index = geom.getFieldAsArray('texCoordIndex', 0, ancestry)
            tex_index = processPerVertexIndex(tex_index)
            loops = [co for f in tex_index
                     for v in f for co in tex_coord_points[v]]
        else:
            x_min = x_max = y_min = y_max = z_min = z_max = None
            for f in faces:
                # Unused vertices don't participate in size; X3DOM does so
                for v in f:
                    (x, y, z) = points[v]
                    if x_min is None or x < x_min:
                        x_min = x
                    if x_max is None or x > x_max:
                        x_max = x
                    if y_min is None or y < y_min:
                        y_min = y
                    if y_max is None or y > y_max:
                        y_max = y
                    if z_min is None or z < z_min:
                        z_min = z
                    if z_max is None or z > z_max:
                        z_max = z

            mins = (x_min, y_min, z_min)
            deltas = (x_max - x_min, y_max - y_min, z_max - z_min)
            axes = [0, 1, 2]
            axes.sort(key=lambda a: (-deltas[a], a))
            # Tuple comparison breaks ties
            (s_axis, t_axis) = axes[0:2]
            s_min = mins[s_axis]
            ds = deltas[s_axis]
            t_min = mins[t_axis]
            dt = deltas[t_axis]

            def generatePointCoords(pt):
                return (pt[s_axis] - s_min) / ds, (pt[t_axis] - t_min) / dt
            loops = [co for f in faces for v in f
                     for co in generatePointCoords(points[v])]

        importMesh_ApplyTextureToLoops(bpymesh, bpyima, loops)

    bpymesh.validate()
    bpymesh.update()
    return bpymesh


def importMesh_ElevationGrid(geom, ancestry, bpyima):
    height = geom.getFieldAsArray('height', 0, ancestry)
    x_dim = geom.getFieldAsInt('xDimension', 0, ancestry)
    x_spacing = geom.getFieldAsFloat('xSpacing', 1, ancestry)
    z_dim = geom.getFieldAsInt('zDimension', 0, ancestry)
    z_spacing = geom.getFieldAsFloat('zSpacing', 1, ancestry)
    ccw = geom.getFieldAsBool('ccw', True, ancestry)

    # The spec assumes a certain ordering of quads; outer loop by z, inner by x
    bpymesh = bpy.data.meshes.new(name="ElevationGrid")
    bpymesh.vertices.add(x_dim * z_dim)
    co = [w for x in range(x_dim) for z in range(z_dim)
          for w in (x * x_spacing, height[x_dim * z + x], z * z_spacing)]
    bpymesh.vertices.foreach_set("co", co)

    bpymesh.tessfaces.add((x_dim - 1) * (z_dim - 1))
    # If the ccw is off, we flip the 2nd and the 4th vertices of each face.
    # For quad tessfaces, it's important that the final vertex index is not 0
    # (Blender treats it as a triangle then).
    # So simply reversing the face is not an option.
    verts = [i for x in range(x_dim - 1) for z in range(z_dim - 1)
             for i in (z * x_dim + x,
                       z * x_dim + x + 1 if ccw else (z + 1) * x_dim + x,
                       (z + 1) * x_dim + x + 1,
                       (z + 1) * x_dim + x if ccw else z * x_dim + x + 1)]
    bpymesh.tessfaces.foreach_set("vertices_raw", verts)

    importMesh_ApplyNormals(bpymesh, geom, ancestry)
    # ApplyColors won't work here; faces are quads, and also per-face
    # coloring should be supported
    colors = geom.getChildBySpec(['ColorRGBA', 'Color'])
    if colors:
        if colors.getSpec() == 'ColorRGBA':
            rgb = [c[:3] for c
                   in colors.getFieldAsArray('color', 4, ancestry)]
            # Array of arrays; no need to flatten
        else:
            rgb = colors.getFieldAsArray('color', 3, ancestry)

        tc = bpymesh.tessface_vertex_colors.new()
        tcd = tc.data
        if geom.getFieldAsBool('colorPerVertex', True, ancestry):
            # Per-vertex coloring
            # Note the 2/4 flip here
            tcd.foreach_set("color1", [c for x in range(x_dim - 1)
                            for z in range(z_dim - 1)
                            for c in rgb[z * x_dim + x]])
            tcd.foreach_set("color2" if ccw else "color4",
                            [c for x in range(x_dim - 1)
                             for z in range(z_dim - 1)
                             for c in rgb[z * x_dim + x + 1]])
            tcd.foreach_set("color3", [c for x in range(x_dim - 1)
                            for z in range(z_dim - 1)
                            for c in rgb[(z + 1) * x_dim + x + 1]])
            tcd.foreach_set("color4" if ccw else "color2",
                            [c for x in range(x_dim - 1)
                             for z in range(z_dim - 1)
                             for c in rgb[(z + 1) * x_dim + x]])
        else:  # Coloring per face
            colors = [c for x in range(x_dim - 1)
                      for z in range(z_dim - 1) for c in rgb[z * (x_dim - 1) + x]]
            tcd.foreach_set("color1", colors)
            tcd.foreach_set("color2", colors)
            tcd.foreach_set("color3", colors)
            tcd.foreach_set("color4", colors)

    # Textures also need special treatment; it's all quads,
    # and there's a builtin algorithm for coordinate generation
    if bpyima:
        tex_coord = geom.getChildBySpec('TextureCoordinate')
        if tex_coord:
            coord_points = tex_coord.getFieldAsArray('point', 2, ancestry)
        else:
            coord_points = [(i / (x_dim - 1), j / (z_dim - 1))
                            for i in range(x_dim)
                            for j in range(z_dim)]

        d = bpymesh.tessface_uv_textures.new().data
        for face in d:  # No foreach_set for nonscalars
            face.image = bpyima
        # Rather than repeat the face/vertex algorithm from above, we read
        # the vertex index back from tessfaces. Might be suboptimal.
        uv = [i for face in bpymesh.tessfaces
              for vno in range(4)
              for i in coord_points[face.vertices[vno]]]
        d.foreach_set('uv_raw', uv)

    bpymesh.validate()
    bpymesh.update()
    return bpymesh


def importMesh_Extrusion(geom, ancestry, bpyima):
    # Interestingly, the spec doesn't allow for vertex/face colors in this
    # element, nor for normals.
    # Since coloring and normals are not supported here, and also large
    # polygons for caps might be required, we shall use from_pydata().

    ccw = geom.getFieldAsBool('ccw', True, ancestry)
    begin_cap = geom.getFieldAsBool('beginCap', True, ancestry)
    end_cap = geom.getFieldAsBool('endCap', True, ancestry)
    cross = geom.getFieldAsArray('crossSection', 2, ancestry)
    if not cross:
        cross = ((1, 1), (1, -1), (-1, -1), (-1, 1), (1, 1))
    spine = geom.getFieldAsArray('spine', 3, ancestry)
    if not spine:
        spine = ((0, 0, 0), (0, 1, 0))
    orient = geom.getFieldAsArray('orientation', 4, ancestry)
    if orient:
        orient = [Quaternion(o[:3], o[3]).to_matrix()
                  if o[3] else None for o in orient]
    scale = geom.getFieldAsArray('scale', 2, ancestry)
    if scale:
        scale = [Matrix(((s[0], 0, 0), (0, 1, 0), (0, 0, s[1])))
                 if s[0] != 1 or s[1] != 1 else None for s in scale]

    # Special treatment for the closed spine and cross section.
    # Let's save some memory by not creating identical but distinct vertices;
    # later we'll introduce conditional logic to link the last vertex with
    # the first one where necessary.
    cross_closed = cross[0] == cross[-1]
    if cross_closed:
        cross = cross[:-1]
    nc = len(cross)
    cross = [Vector((c[0], 0, c[1])) for c in cross]
    ncf = nc if cross_closed else nc - 1
    # Face count along the cross; for closed cross, it's the same as the
    # respective vertex count

    spine_closed = spine[0] == spine[-1]
    if spine_closed:
        spine = spine[:-1]
    ns = len(spine)
    spine = [Vector(s) for s in spine]
    nsf = ns if spine_closed else ns - 1

    # This will be used for fallback, where the current spine point joins
    # two collinear spine segments. No need to recheck the case of the
    # closed spine/last-to-first point juncture; if there's an angle there,
    # it would kick in on the first iteration of the main loop by spine.
    def findFirstAngleNormal():
        for i in range(1, ns - 1):
            spt = spine[i]
            z = (spine[i + 1] - spt).cross(spine[i - 1] - spt)
            if z.length > EPSILON:
                return z
        # All the spines are collinear. Fallback to the rotated source
        # XZ plane.
        # TODO: handle the situation where the first two spine points match
        v = spine[1] - spine[0]
        orig_y = Vector((0, 1, 0))
        orig_z = Vector((0, 0, 1))
        if v.cross(orig_y).length >= EPSILON:
            # Spine at angle with global y - rotate the z accordingly
            orig_z.rotate(orig_y.rotation_difference(v))
        return orig_z

    verts = []
    z = None
    for i, spt in enumerate(spine):
        if (i > 0 and i < ns - 1) or spine_closed:
            snext = spine[(i + 1) % ns]
            sprev = spine[(i - 1 + ns) % ns]
            y = snext - sprev
            vnext = snext - spt
            vprev = sprev - spt
            try_z = vnext.cross(vprev)
            # Might be zero, then all kinds of fallback
            if try_z.length > EPSILON:
                if z is not None and try_z.dot(z) < 0:
                    try_z.negate()
                z = try_z
            elif not z:  # No z, and no previous z.
                # Look ahead, see if there's at least one point where
                # spines are not collinear.
                z = findFirstAngleNormal()
        elif i == 0:  # And non-crossed
            snext = spine[i + 1]
            y = snext - spt
            z = findFirstAngleNormal()
        else:  # last point and not crossed
            sprev = spine[i - 1]
            y = spt - sprev
            # If there's more than one point in the spine, z is already set.
            # One point in the spline is an error anyway.

        x = y.cross(z)
        m = Matrix(((x.x, y.x, z.x), (x.y, y.y, z.y), (x.z, y.z, z.z)))
        # Columns are the unit vectors for the xz plane for the cross-section
        m.normalize()
        if orient:
            mrot = orient[i] if len(orient) > 1 else orient[0]
            if mrot:
                m *= mrot  # Not sure about this. Counterexample???
        if scale:
            mscale = scale[i] if len(scale) > 1 else scale[0]
            if mscale:
                m *= mscale
                # First the cross-section 2-vector is scaled,
                # then applied to the xz plane unit vectors
        for cpt in cross:
            verts.append((spt + m * cpt).to_tuple())
            # Could've done this with a single 4x4 matrix... Oh well

    # The method from_pydata() treats correctly quads with final vertex
    # index being zero.
    # So we just flip the vertices if ccw is off.

    faces = []
    if begin_cap:
        faces.append(flip([x for x in range(nc - 1, -1, -1)], ccw))

    # Order of edges in the face: forward along cross, forward along spine,
    # backward along cross, backward along spine, flipped if now ccw.
    # This order is assumed later in the texture coordinate assignment;
    # please don't change without syncing.

    faces += [flip((
        s * nc + c,
        s * nc + (c + 1) % nc,
        (s + 1) * nc + (c + 1) % nc,
        (s + 1) * nc + c), ccw) for s in range(ns - 1) for c in range(ncf)]

    if spine_closed:
        # The faces between the last and the first spine poins
        b = (ns - 1) * nc
        faces += [flip((
            b + c,
            b + (c + 1) % nc,
            (c + 1) % nc,
            c), ccw) for c in range(ncf)]

    if end_cap:
        faces.append(flip([(ns - 1) * nc + x for x in range(0, nc)], ccw))

    bpymesh = bpy.data.meshes.new(name="Extrusion")
    bpymesh.from_pydata(verts, [], faces)

    # Polygons and loops here, not tessfaces. The way we deal with
    # textures in triangular meshes doesn't apply.
    if bpyima:
        # The structure of the loop array goes: cap, side, cap
        if begin_cap or end_cap:  # Need dimensions
            x_min = x_max = z_min = z_max = None
            for c in cross:
                (x, z) = (c.x, c.z)
                if x_min is None or x < x_min:
                    x_min = x
                if x_max is None or x > x_max:
                    x_max = x
                if z_min is None or z < z_min:
                    z_min = z
                if z_max is None or z > z_max:
                    z_max = z
            dx = x_max - x_min
            dz = z_max - z_min
            cap_scale = dz if dz > dx else dx

        # Takes an index in the cross array, returns scaled
        # texture coords for cap texturing purposes
        def scaledLoopVertex(i):
            c = cross[i]
            return (c.x - x_min) / cap_scale, (c.z - z_min) / cap_scale

        # X3DOM uses raw cap shape, not a scaled one. So we will, too.

        loops = []
        mloops = bpymesh.loops
        if begin_cap:  # vertex indices match the indices in cross
            # Rely on the loops in the mesh; don't repeat the face
            # generation logic here
            loops += [co for i in range(nc)
                      for co in scaledLoopVertex(mloops[i].vertex_index)]

        # Sides
        # Same order of vertices as in face generation
        # We don't rely on the loops in the mesh; instead,
        # we repeat the face generation logic.
        loops += [co for s in range(nsf)
                  for c in range(ncf)
                  for v in flip(((c / ncf, s / nsf),
                                 ((c + 1) / ncf, s / nsf),
                                 ((c + 1) / ncf, (s + 1) / nsf),
                                 (c / ncf, (s + 1) / nsf)), ccw) for co in v]

        if end_cap:
            # Base loop index for end cap
            lb = ncf * nsf * 4 + (nc if begin_cap else 0)
            # Rely on the loops here too.
            loops += [co for i in range(nc) for co
                      in scaledLoopVertex(mloops[lb + i].vertex_index % nc)]
        importMesh_ApplyTextureToLoops(bpymesh, bpyima, loops)

    bpymesh.validate(True)
    bpymesh.update()
    return bpymesh


# -----------------------------------------------------------------------------------
# Line and point sets


def importMesh_LineSet(geom, ancestry, bpyima):
    # TODO: line display properties are ignored
    # Per-vertex color is ignored
    coord = geom.getChildBySpec('Coordinate')
    src_points = coord.getFieldAsArray('point', 3, ancestry)
    # Array of 3; Blender needs arrays of 4
    bpycurve = bpy.data.curves.new("LineSet", 'CURVE')
    bpycurve.dimensions = '3D'
    counts = geom.getFieldAsArray('vertexCount', 0, ancestry)
    b = 0
    for n in counts:
        sp = bpycurve.splines.new('POLY')
        sp.points.add(n - 1)  # points already has one element

        def points():
            for x in src_points[b:b + n]:
                yield x[0]
                yield x[1]
                yield x[2]
                yield 0
        sp.points.foreach_set('co', [x for x in points()])
        b += n
    return bpycurve


def importMesh_IndexedLineSet(geom, ancestry, _):
    # VRML not x3d
    # coord = geom.getChildByName('coord') # 'Coordinate'
    coord = geom.getChildBySpec('Coordinate')  # works for x3d and vrml
    if coord:
        points = coord.getFieldAsArray('point', 3, ancestry)
    else:
        points = []

    if not points:
        print('\tWarning: IndexedLineSet had no points')
        return None

    ils_lines = geom.getFieldAsArray('coordIndex', 0, ancestry)

    lines = []
    line = []

    for il in ils_lines:
        if il == -1:
            lines.append(line)
            line = []
        else:
            line.append(int(il))
    lines.append(line)

    # vcolor = geom.getChildByName('color')
    # blender dosnt have per vertex color

    bpycurve = bpy.data.curves.new('IndexedCurve', 'CURVE')
    bpycurve.dimensions = '3D'

    for line in lines:
        if not line:
            continue
        # co = points[line[0]]  # UNUSED
        nu = bpycurve.splines.new('POLY')
        nu.points.add(len(line) - 1)  # the new nu has 1 point to begin with
        for il, pt in zip(line, nu.points):
            pt.co[0:3] = points[il]

    return bpycurve


def importMesh_PointSet(geom, ancestry, _):
    # VRML not x3d
    coord = geom.getChildBySpec('Coordinate')  # works for x3d and vrml
    if coord:
        points = coord.getFieldAsArray('point', 3, ancestry)
    else:
        points = []

    # vcolor = geom.getChildByName('color')
    # blender dosnt have per vertex color

    bpymesh = bpy.data.meshes.new("PointSet")
    bpymesh.vertices.add(len(points))
    bpymesh.vertices.foreach_set("co", [a for v in points for a in v])

    # No need to validate
    bpymesh.update()
    return bpymesh


# -----------------------------------------------------------------------------------
# Primitives
# SA: they used to use bpy.ops for primitive creation. That was
# unbelievably slow on complex scenes. I rewrote to generate meshes
# by hand.


GLOBALS['CIRCLE_DETAIL'] = 12


def importMesh_Sphere(geom, ancestry, bpyima):
    # solid is ignored.
    # Extra field 'subdivision="n m"' attribute, specifying how many
    # rings and segments to use (X3DOM).
    r = geom.getFieldAsFloat('radius', 0.5, ancestry)
    subdiv = geom.getFieldAsArray('subdivision', 0, ancestry)
    if subdiv:
        if len(subdiv) == 1:
            nr = ns = subdiv[0]
        else:
            (nr, ns) = subdiv
    else:
        nr = ns = GLOBALS['CIRCLE_DETAIL']
        # used as both ring count and segment count
    lau = pi / nr  # Unit angle of latitude (rings) for the given tesselation
    lou = 2 * pi / ns  # Unit angle of longitude (segments)

    bpymesh = bpy.data.meshes.new(name="Sphere")

    bpymesh.vertices.add(ns * (nr - 1) + 2)
    # The non-polar vertices go from x=0, negative z plane counterclockwise -
    # to -x, to +z, to +x, back to -z
    co = [0, r, 0, 0, -r, 0]  # +y and -y poles
    co += [r * coe for ring in range(1, nr) for seg in range(ns)
           for coe in (-sin(lou * seg) * sin(lau * ring),
                       cos(lau * ring),
                       -cos(lou * seg) * sin(lau * ring))]
    bpymesh.vertices.foreach_set('co', co)

    tf = bpymesh.tessfaces
    tf.add(ns * nr)
    vb = 2 + (nr - 2) * ns  # First vertex index for the bottom cap
    fb = (nr - 1) * ns  # First face index for the bottom cap

    # Because of tricky structure, assign texture coordinates along with
    # face creation. Can't easily do foreach_set, 'cause caps are triangles and
    # sides are quads.

    if bpyima:
        tex = bpymesh.tessface_uv_textures.new().data
        for face in tex:  # No foreach_set for nonscalars
            face.image = bpyima

    # Faces go in order: top cap, sides, bottom cap.
    # Sides go by ring then by segment.

    # Caps
    # Top cap face vertices go in order: down right up
    # (starting from +y pole)
    # Bottom cap goes: up left down (starting from -y pole)
    for seg in range(ns):
        tf[seg].vertices = (0, seg + 2, (seg + 1) % ns + 2)
        tf[fb + seg].vertices = (1, vb + (seg + 1) % ns, vb + seg)
        if bpyima:
            tex[seg].uv = (((seg + 0.5) / ns, 1),
                           (seg / ns, 1 - 1 / nr),
                           ((seg + 1) / ns, 1 - 1 / nr))
            tex[fb + seg].uv = (((seg + 0.5) / ns, 0),
                                ((seg + 1) / ns, 1 / nr),
                                (seg / ns, 1 / nr))

    # Sides
    # Side face vertices go in order:  down right up left
    for ring in range(nr - 2):
        tvb = 2 + ring * ns
        # First vertex index for the top edge of the ring
        bvb = tvb + ns
        # First vertex index for the bottom edge of the ring
        rfb = ns * (ring + 1)
        # First face index for the ring
        for seg in range(ns):
            nseg = (seg + 1) % ns
            tf[rfb + seg].vertices_raw = (tvb + seg, bvb + seg, bvb + nseg, tvb + nseg)
            if bpyima:
                tex[rfb + seg].uv_raw = (seg / ns, 1 - (ring + 1) / nr,
                                         seg / ns, 1 - (ring + 2) / nr,
                                         (seg + 1) / ns, 1 - (ring + 2) / nr,
                                         (seg + 1) / ns, 1 - (ring + 1) / nr)

    bpymesh.validate(False)
    bpymesh.update()
    return bpymesh


def importMesh_Cylinder(geom, ancestry, bpyima):
    # solid is ignored
    # no ccw in this element
    # Extra parameter subdivision="n" - how many faces to use
    radius = geom.getFieldAsFloat('radius', 1.0, ancestry)
    height = geom.getFieldAsFloat('height', 2, ancestry)
    bottom = geom.getFieldAsBool('bottom', True, ancestry)
    side = geom.getFieldAsBool('side', True, ancestry)
    top = geom.getFieldAsBool('top', True, ancestry)

    n = geom.getFieldAsInt('subdivision', GLOBALS['CIRCLE_DETAIL'], ancestry)

    nn = n * 2
    yvalues = (height / 2, -height / 2)
    angle = 2 * pi / n

    # The seam is at x=0, z=-r, vertices go ccw -
    # to pos x, to neg z, to neg x, back to neg z
    verts = [(-radius * sin(angle * i), y, -radius * cos(angle * i))
             for i in range(n) for y in yvalues]
    faces = []
    if side:
        # Order of edges in side faces: up, left, down, right.
        # Texture coordinate logic depends on it.
        faces += [(i * 2 + 3, i * 2 + 2, i * 2, i * 2 + 1)
                  for i in range(n - 1)] + [(1, 0, nn - 2, nn - 1)]
    if top:
        faces += [[x for x in range(0, nn, 2)]]
    if bottom:
        faces += [[x for x in range(nn - 1, -1, -2)]]

    bpymesh = bpy.data.meshes.new(name="Cylinder")
    bpymesh.from_pydata(verts, [], faces)
    # Tried constructing the mesh manually from polygons/loops/edges,
    # the difference in performance on Blender 2.74 (Win64) is negligible.

    bpymesh.validate(False)

    # Polygons here, not tessfaces
    # The structure of the loop array goes: cap, side, cap.
    if bpyima:
        loops = []
        if side:
            loops += [co for i in range(n)
                      for co in ((i + 1) / n, 0, (i + 1) / n, 1, i / n, 1, i / n, 0)]

        if top:
            loops += [0.5 + co / 2 for i in range(n)
                      for co in (-sin(angle * i), cos(angle * i))]

        if bottom:
            loops += [0.5 - co / 2 for i in range(n - 1, -1, -1)
                      for co in (sin(angle * i), cos(angle * i))]

        importMesh_ApplyTextureToLoops(bpymesh, bpyima, loops)

    bpymesh.update()
    return bpymesh


def importMesh_Cone(geom, ancestry, bpyima):
    # Solid ignored
    # Extra parameter subdivision="n" - how many faces to use
    n = geom.getFieldAsInt('subdivision', GLOBALS['CIRCLE_DETAIL'], ancestry)
    radius = geom.getFieldAsFloat('bottomRadius', 1.0, ancestry)
    height = geom.getFieldAsFloat('height', 2, ancestry)
    bottom = geom.getFieldAsBool('bottom', True, ancestry)
    side = geom.getFieldAsBool('side', True, ancestry)

    d = height / 2
    angle = 2 * pi / n

    verts = [(0, d, 0)]
    verts += [(-radius * sin(angle * i),
               -d,
               -radius * cos(angle * i)) for i in range(n)]
    faces = []

    # Side face vertices go: up down right
    if side:
        faces += [(1 + (i + 1) % n, 0, 1 + i) for i in range(n)]
    if bottom:
        faces += [[i for i in range(n, 0, -1)]]

    bpymesh = bpy.data.meshes.new(name="Cone")
    bpymesh.from_pydata(verts, [], faces)

    bpymesh.validate(False)
    if bpyima:
        loops = []
        if side:
            loops += [co for i in range(n)
                      for co in ((i + 1) / n, 0, (i + 0.5) / n, 1, i / n, 0)]
        if bottom:
            loops += [0.5 - co / 2 for i in range(n - 1, -1, -1)
                      for co in (sin(angle * i), cos(angle * i))]
        importMesh_ApplyTextureToLoops(bpymesh, bpyima, loops)

    bpymesh.update()
    return bpymesh


def importMesh_Box(geom, ancestry, bpyima):
    # Solid is ignored
    # No ccw in this element
    (dx, dy, dz) = geom.getFieldAsFloatTuple('size', (2.0, 2.0, 2.0), ancestry)
    dx /= 2
    dy /= 2
    dz /= 2

    bpymesh = bpy.data.meshes.new(name="Box")
    bpymesh.vertices.add(8)

    # xz plane at +y, ccw
    co = (dx, dy, dz, -dx, dy, dz, -dx, dy, -dz, dx, dy, -dz,
          # xz plane at -y
          dx, -dy, dz, -dx, -dy, dz, -dx, -dy, -dz, dx, -dy, -dz)
    bpymesh.vertices.foreach_set('co', co)

    bpymesh.tessfaces.add(6)
    bpymesh.tessfaces.foreach_set('vertices_raw', (
        0, 1, 2, 3,   # +y
        4, 0, 3, 7,   # +x
        7, 3, 2, 6,   # -z
        6, 2, 1, 5,   # -x
        5, 1, 0, 4,   # +z
        7, 6, 5, 4))  # -y

    bpymesh.validate(False)
    if bpyima:
        d = bpymesh.tessface_uv_textures.new().data
        for face in d:  # No foreach_set for nonscalars
            face.image = bpyima
        d.foreach_set('uv_raw', (
            1, 0, 0, 0, 0, 1, 1, 1,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            0, 0, 0, 1, 1, 1, 1, 0,
            1, 0, 0, 0, 0, 1, 1, 1))

    bpymesh.update()
    return bpymesh

# -----------------------------------------------------------------------------------
# Utilities for importShape


# Textures are processed elsewhere.
def appearance_CreateMaterial(vrmlname, mat, ancestry, is_vcol):
    # Given an X3D material, creates a Blender material.
    # texture is applied later, in appearance_Create().
    # All values between 0.0 and 1.0, defaults from VRML docs.
    bpymat = bpy.data.materials.new(vrmlname)
    bpymat.ambient = mat.getFieldAsFloat('ambientIntensity', 0.2, ancestry)
    diff_color = mat.getFieldAsFloatTuple('diffuseColor',
                                          [0.8, 0.8, 0.8],
                                          ancestry)
    bpymat.diffuse_color = diff_color

    # NOTE - blender dosnt support emmisive color
    # Store in mirror color and approximate with emit.
    emit = mat.getFieldAsFloatTuple('emissiveColor', [0.0, 0.0, 0.0], ancestry)
    bpymat.mirror_color = emit
    bpymat.emit = (emit[0] + emit[1] + emit[2]) / 3.0

    shininess = mat.getFieldAsFloat('shininess', 0.2, ancestry)
    bpymat.specular_hardness = int(1 + (510 * shininess))
    # 0-1 -> 1-511
    bpymat.specular_color = mat.getFieldAsFloatTuple('specularColor',
                                                     [0.0, 0.0, 0.0], ancestry)
    bpymat.alpha = 1.0 - mat.getFieldAsFloat('transparency', 0.0, ancestry)
    if bpymat.alpha < 0.999:
        bpymat.use_transparency = True
    if is_vcol:
        bpymat.use_vertex_color_paint = True
    return bpymat


def appearance_CreateDefaultMaterial():
    # Just applies the X3D defaults. Used for shapes
    # without explicit material definition
    # (but possibly with a texture).

    bpymat = bpy.data.materials.new("Material")
    bpymat.ambient = 0.2
    bpymat.diffuse_color = [0.8, 0.8, 0.8]
    bpymat.mirror_color = (0, 0, 0)
    bpymat.emit = 0

    bpymat.specular_hardness = 103
    # 0-1 -> 1-511
    bpymat.specular_color = (0, 0, 0)
    bpymat.alpha = 1
    return bpymat


def appearance_LoadImageTextureFile(ima_urls, node):
    bpyima = None
    for f in ima_urls:
        dirname = os.path.dirname(node.getFilename())
        bpyima = image_utils.load_image(f, dirname,
                                        place_holder=False,
                                        recursive=False,
                                        convert_callback=imageConvertCompat)
        if bpyima:
            break

    return bpyima


def appearance_LoadImageTexture(imageTexture, ancestry, node):
    # TODO: cache loaded textures...
    ima_urls = imageTexture.getFieldAsString('url', None, ancestry)

    if ima_urls is None:
        try:
            ima_urls = imageTexture.getFieldAsStringArray('url', ancestry)
            # in some cases we get a list of images.
        except:
            ima_urls = None
    else:
        if '" "' in ima_urls:
            # '"foo" "bar"' --> ['foo', 'bar']
            ima_urls = [w.strip('"') for w in ima_urls.split('" "')]
        else:
            ima_urls = [ima_urls]
    # ima_urls is a list or None

    if ima_urls is None:
        print("\twarning, image with no URL, this is odd")
        return None
    else:
        bpyima = appearance_LoadImageTextureFile(ima_urls, node)

        if not bpyima:
            print("ImportX3D warning: unable to load texture", ima_urls)
        else:
            # KNOWN BUG; PNGs with a transparent color are not perceived
            # as transparent. Need alpha channel.

            bpyima.use_alpha = bpyima.depth in {32, 128}
        return bpyima


def appearance_LoadTexture(tex_node, ancestry, node):
    # Both USE-based caching and desc-based caching
    # Works for bother ImageTextures and PixelTextures

    # USE-based caching
    if tex_node.reference:
        return tex_node.getRealNode().parsed

    # Desc-based caching. It might misfire on multifile models, where the
    # same desc means different things in different files.
    # TODO: move caches to file level.
    desc = tex_node.desc()
    if desc and desc in texture_cache:
        bpyima = texture_cache[desc]
        if tex_node.canHaveReferences():
            tex_node.parsed = bpyima
        return bpyima

    # No cached texture, load it.
    if tex_node.getSpec() == 'ImageTexture':
        bpyima = appearance_LoadImageTexture(tex_node, ancestry, node)
    else:  # PixelTexture
        bpyima = appearance_LoadPixelTexture(tex_node, ancestry)

    if bpyima:  # Loading can still fail
        repeat_s = tex_node.getFieldAsBool('repeatS', True, ancestry)
        bpyima.use_clamp_x = not repeat_s
        repeat_t = tex_node.getFieldAsBool('repeatT', True, ancestry)
        bpyima.use_clamp_y = not repeat_t

        # Update the desc-based cache
        if desc:
            texture_cache[desc] = bpyima

        # Update the USE-based cache
        if tex_node.canHaveReferences():
            tex_node.parsed = bpyima

    return bpyima


def appearance_ExpandCachedMaterial(bpymat):
    if bpymat.texture_slots[0] is not None:
        bpyima = bpymat.texture_slots[0].texture.image
        tex_has_alpha = bpyima.use_alpha
        return (bpymat, bpyima, tex_has_alpha)

    return (bpymat, None, False)


def appearance_MakeDescCacheKey(material, tex_node):
    mat_desc = material.desc() if material else "Default"
    tex_desc = tex_node.desc() if tex_node else "Default"

    if not((tex_node and tex_desc is None) or
           (material and mat_desc is None)):
        # desc not available (in VRML)
        # TODO: serialize VRML nodes!!!
        return (mat_desc, tex_desc)
    elif not tex_node and not material:
        # Even for VRML, we cache the null material
        return ("Default", "Default")
    else:
        return None  # Desc-based caching is off


def appearance_Create(vrmlname, material, tex_node, ancestry, node, is_vcol):
    # Creates a Blender material object from appearance
    bpyima = None
    tex_has_alpha = False

    if material:
        bpymat = appearance_CreateMaterial(vrmlname, material, ancestry, is_vcol)
    else:
        bpymat = appearance_CreateDefaultMaterial()

    if tex_node:  # Texture caching inside there
        bpyima = appearance_LoadTexture(tex_node, ancestry, node)

    if is_vcol:
        bpymat.use_vertex_color_paint = True

    if bpyima:
        tex_has_alpha = bpyima.use_alpha

        texture = bpy.data.textures.new(bpyima.name, 'IMAGE')
        texture.image = bpyima

        mtex = bpymat.texture_slots.add()
        mtex.texture = texture

        mtex.texture_coords = 'UV'
        mtex.use_map_diffuse = True
        mtex.use = True

        if bpyima.use_alpha:
            bpymat.use_transparency = True
            mtex.use_map_alpha = True
            mtex.alpha_factor = 0.0

    return (bpymat, bpyima, tex_has_alpha)


def importShape_LoadAppearance(vrmlname, appr, ancestry, node, is_vcol):
    """
    Material creation takes nontrivial time on large models.
    So we cache them aggressively.
    However, in Blender, texture is a part of material, while in
    X3D it's not. Blender's notion of material corresponds to
    X3D's notion of appearance.

    TextureTransform is not a part of material (at least
    not in the current implementation).

    USE on an Appearance node and USE on a Material node
    call for different approaches.

    Tools generate repeating, idential material definitions.
    Can't rely on USE alone. Repeating texture definitions
    are entirely possible, too.

    Vertex coloring is not a part of appearance, but Blender
    has a material flag for it. However, if a mesh has no vertex
    color layer, setting use_vertex_color_paint to true has no
    effect. So it's fine to reuse the same  material for meshes
    with vertex colors and for ones without.
    It's probably an abuse of Blender of some level.

    So here's the caching structure:
    For USE on apprearance, we store the material object
    in the appearance node.

    For USE on texture, we store the image object in the tex node.

    For USE on material with no texture, we store the material object
    in the material node.

    Also, we store textures by description in texture_cache.

    Also, we store materials by (material desc, texture desc)
    in material_cache.
    """
    # First, check entire-appearance cache
    if appr.reference and appr.getRealNode().parsed:
        return appearance_ExpandCachedMaterial(appr.getRealNode().parsed)

    tex_node = appr.getChildBySpec(('ImageTexture', 'PixelTexture'))
    # Other texture nodes are: MovieTexture, MultiTexture
    material = appr.getChildBySpec('Material')
    # We're ignoring FillProperties, LineProperties, and shaders

    # Check the USE-based material cache for textureless materials
    if material and material.reference and not tex_node and material.getRealNode().parsed:
        return appearance_ExpandCachedMaterial(material.getRealNode().parsed)

    # Now the description-based caching
    cache_key = appearance_MakeDescCacheKey(material, tex_node)

    if cache_key and cache_key in material_cache:
        bpymat = material_cache[cache_key]
        # Still want to make the material available for USE-based reuse
        if appr.canHaveReferences():
            appr.parsed = bpymat
        if material and material.canHaveReferences() and not tex_node:
            material.parsed = bpymat
        return appearance_ExpandCachedMaterial(bpymat)

    # Done checking full-material caches. Texture cache may still kick in.
    # Create the material already
    (bpymat, bpyima, tex_has_alpha) = appearance_Create(vrmlname, material, tex_node, ancestry, node, is_vcol)

    # Update the caches
    if appr.canHaveReferences():
        appr.parsed = bpymat

    if cache_key:
        material_cache[cache_key] = bpymat

    if material and material.canHaveReferences() and not tex_node:
        material.parsed = bpymat

    return (bpymat, bpyima, tex_has_alpha)


def appearance_LoadPixelTexture(pixelTexture, ancestry):
    image = pixelTexture.getFieldAsArray('image', 0, ancestry)
    (w, h, plane_count) = image[0:3]
    has_alpha = plane_count in {2, 4}
    pixels = image[3:]
    if len(pixels) != w * h:
        print("ImportX3D warning: pixel count in PixelTexture is off")

    bpyima = bpy.data.images.new("PixelTexture", w, h, has_alpha, True)
    bpyima.use_alpha = has_alpha

    # Conditional above the loop, for performance
    if plane_count == 3:  # RGB
        bpyima.pixels = [(cco & 0xff) / 255 for pixel in pixels
                         for cco in (pixel >> 16, pixel >> 8, pixel, 255)]
    elif plane_count == 4:  # RGBA
        bpyima.pixels = [(cco & 0xff) / 255 for pixel in pixels
                         for cco
                         in (pixel >> 24, pixel >> 16, pixel >> 8, pixel)]
    elif plane_count == 1:  # Intensity - does Blender even support that?
        bpyima.pixels = [(cco & 0xff) / 255 for pixel in pixels
                         for cco in (pixel, pixel, pixel, 255)]
    elif plane_count == 2:  # Intensity/aplha
        bpyima.pixels = [(cco & 0xff) / 255 for pixel in pixels
                         for cco
                         in (pixel >> 8, pixel >> 8, pixel >> 8, pixel)]
    bpyima.update()
    return bpyima


# Called from importShape to insert a data object (typically a mesh)
# into the scene
def importShape_ProcessObject(
        bpyscene, vrmlname, bpydata, geom, geom_spec, node,
        bpymat, has_alpha, texmtx, ancestry,
        global_matrix):

    vrmlname += "_" + geom_spec
    bpydata.name = vrmlname

    if type(bpydata) == bpy.types.Mesh:
        # solid, as understood by the spec, is always true in Blender
        # solid=false, we don't support it yet.
        creaseAngle = geom.getFieldAsFloat('creaseAngle', None, ancestry)
        if creaseAngle is not None:
            bpydata.auto_smooth_angle = creaseAngle
            bpydata.use_auto_smooth = True

        # Only ever 1 material per shape
        if bpymat:
            bpydata.materials.append(bpymat)

        if bpydata.tessface_uv_textures:
            if has_alpha:  # set the faces alpha flag?
                # transp = Mesh.FaceTranspModes.ALPHA
                for f in bpydata.tessface_uv_textures.active.data:
                    f.blend_type = 'ALPHA'

            if texmtx:
                # Apply texture transform?
                uv_copy = Vector()
                for f in bpydata.tessface_uv_textures.active.data:
                    fuv = f.uv
                    for i, uv in enumerate(fuv):
                        uv_copy.x = uv[0]
                        uv_copy.y = uv[1]

                        fuv[i] = (uv_copy * texmtx)[0:2]
        # Done transforming the texture
        # TODO: check if per-polygon textures are supported here.
    elif type(bpydata) == bpy.types.TextCurve:
        # Text with textures??? Not sure...
        if bpymat:
            bpydata.materials.append(bpymat)

    # Can transform data or object, better the object so we can instance
    # the data
    # bpymesh.transform(getFinalMatrix(node))
    bpyob = node.blendObject = bpy.data.objects.new(vrmlname, bpydata)
    bpyob.matrix_world = getFinalMatrix(node, None, ancestry, global_matrix)
    bpyscene.objects.link(bpyob).select = True

    if DEBUG:
        bpyob["source_line_no"] = geom.lineno


def importText(geom, ancestry, bpyima):
    fmt = geom.getChildBySpec('FontStyle')
    size = fmt.getFieldAsFloat("size", 1, ancestry) if fmt else 1.
    body = geom.getFieldAsString("string", None, ancestry)
    body = [w.strip('"') for w in body.split('" "')]

    bpytext = bpy.data.curves.new(name="Text", type='FONT')
    bpytext.offset_y = - size
    bpytext.body = "\n".join(body)
    bpytext.size = size
    return bpytext


# -----------------------------------------------------------------------------------


geometry_importers = {
    'IndexedFaceSet': importMesh_IndexedFaceSet,
    'IndexedTriangleSet': importMesh_IndexedTriangleSet,
    'IndexedTriangleStripSet': importMesh_IndexedTriangleStripSet,
    'IndexedTriangleFanSet': importMesh_IndexedTriangleFanSet,
    'IndexedLineSet': importMesh_IndexedLineSet,
    'TriangleSet': importMesh_TriangleSet,
    'TriangleStripSet': importMesh_TriangleStripSet,
    'TriangleFanSet': importMesh_TriangleFanSet,
    'LineSet': importMesh_LineSet,
    'ElevationGrid': importMesh_ElevationGrid,
    'Extrusion': importMesh_Extrusion,
    'PointSet': importMesh_PointSet,
    'Sphere': importMesh_Sphere,
    'Box': importMesh_Box,
    'Cylinder': importMesh_Cylinder,
    'Cone': importMesh_Cone,
    'Text': importText,
    }


def importShape(bpyscene, node, ancestry, global_matrix):
    # Under Shape, we can only have Appearance, MetadataXXX and a geometry node
    def isGeometry(spec):
        return spec != "Appearance" and not spec.startswith("Metadata")

    bpyob = node.getRealNode().blendObject

    if bpyob is not None:
        bpyob = node.blendData = node.blendObject = bpyob.copy()
        # Could transform data, but better the object so we can instance the data
        bpyob.matrix_world = getFinalMatrix(node, None, ancestry, global_matrix)
        bpyscene.objects.link(bpyob).select = True
        return

    vrmlname = node.getDefName()
    if not vrmlname:
        vrmlname = 'Shape'

    appr = node.getChildBySpec('Appearance')
    geom = node.getChildBySpecCondition(isGeometry)
    if not geom:
        # Oh well, no geometry node in this shape
        return

    bpymat = None
    bpyima = None
    texmtx = None
    tex_has_alpha = False

    is_vcol = (geom.getChildBySpec(['Color', 'ColorRGBA']) is not None)

    if appr:
        (bpymat, bpyima,
         tex_has_alpha) = importShape_LoadAppearance(vrmlname, appr,
                                                     ancestry, node,
                                                     is_vcol)

        textx = appr.getChildBySpec('TextureTransform')
        if textx:
            texmtx = translateTexTransform(textx, ancestry)

    bpydata = None
    geom_spec = geom.getSpec()

    # ccw is handled by every geometry importer separately; some
    # geometries are easier to flip than others
    geom_fn = geometry_importers.get(geom_spec)
    if geom_fn is not None:
        bpydata = geom_fn(geom, ancestry, bpyima)

        # There are no geometry importers that can legally return
        # no object.  It's either a bpy object, or an exception
        importShape_ProcessObject(
                bpyscene, vrmlname, bpydata, geom, geom_spec,
                node, bpymat, tex_has_alpha, texmtx,
                ancestry, global_matrix)
    else:
        print('\tImportX3D warning: unsupported type "%s"' % geom_spec)


# -----------------------------------------------------------------------------------
# Lighting


def importLamp_PointLight(node, ancestry):
    vrmlname = node.getDefName()
    if not vrmlname:
        vrmlname = 'PointLight'

    # ambientIntensity = node.getFieldAsFloat('ambientIntensity', 0.0, ancestry) # TODO
    # attenuation = node.getFieldAsFloatTuple('attenuation', (1.0, 0.0, 0.0), ancestry) # TODO
    color = node.getFieldAsFloatTuple('color', (1.0, 1.0, 1.0), ancestry)
    intensity = node.getFieldAsFloat('intensity', 1.0, ancestry)  # max is documented to be 1.0 but some files have higher.
    location = node.getFieldAsFloatTuple('location', (0.0, 0.0, 0.0), ancestry)
    # is_on = node.getFieldAsBool('on', True, ancestry) # TODO
    radius = node.getFieldAsFloat('radius', 100.0, ancestry)

    bpylamp = bpy.data.lamps.new(vrmlname, 'POINT')
    bpylamp.energy = intensity
    bpylamp.distance = radius
    bpylamp.color = color

    mtx = Matrix.Translation(Vector(location))

    return bpylamp, mtx


def importLamp_DirectionalLight(node, ancestry):
    vrmlname = node.getDefName()
    if not vrmlname:
        vrmlname = 'DirectLight'

    # ambientIntensity = node.getFieldAsFloat('ambientIntensity', 0.0) # TODO
    color = node.getFieldAsFloatTuple('color', (1.0, 1.0, 1.0), ancestry)
    direction = node.getFieldAsFloatTuple('direction', (0.0, 0.0, -1.0), ancestry)
    intensity = node.getFieldAsFloat('intensity', 1.0, ancestry)  # max is documented to be 1.0 but some files have higher.
    # is_on = node.getFieldAsBool('on', True, ancestry) # TODO

    bpylamp = bpy.data.lamps.new(vrmlname, 'SUN')
    bpylamp.energy = intensity
    bpylamp.color = color

    # lamps have their direction as -z, yup
    mtx = Vector(direction).to_track_quat('-Z', 'Y').to_matrix().to_4x4()

    return bpylamp, mtx

# looks like default values for beamWidth and cutOffAngle were swapped in VRML docs.


def importLamp_SpotLight(node, ancestry):
    vrmlname = node.getDefName()
    if not vrmlname:
        vrmlname = 'SpotLight'

    # ambientIntensity = geom.getFieldAsFloat('ambientIntensity', 0.0, ancestry) # TODO
    # attenuation = geom.getFieldAsFloatTuple('attenuation', (1.0, 0.0, 0.0), ancestry) # TODO
    beamWidth = node.getFieldAsFloat('beamWidth', 1.570796, ancestry)  # max is documented to be 1.0 but some files have higher.
    color = node.getFieldAsFloatTuple('color', (1.0, 1.0, 1.0), ancestry)
    cutOffAngle = node.getFieldAsFloat('cutOffAngle', 0.785398, ancestry) * 2.0  # max is documented to be 1.0 but some files have higher.
    direction = node.getFieldAsFloatTuple('direction', (0.0, 0.0, -1.0), ancestry)
    intensity = node.getFieldAsFloat('intensity', 1.0, ancestry)  # max is documented to be 1.0 but some files have higher.
    location = node.getFieldAsFloatTuple('location', (0.0, 0.0, 0.0), ancestry)
    # is_on = node.getFieldAsBool('on', True, ancestry) # TODO
    radius = node.getFieldAsFloat('radius', 100.0, ancestry)

    bpylamp = bpy.data.lamps.new(vrmlname, 'SPOT')
    bpylamp.energy = intensity
    bpylamp.distance = radius
    bpylamp.color = color
    bpylamp.spot_size = cutOffAngle
    if beamWidth > cutOffAngle:
        bpylamp.spot_blend = 0.0
    else:
        if cutOffAngle == 0.0:  # this should never happen!
            bpylamp.spot_blend = 0.5
        else:
            bpylamp.spot_blend = beamWidth / cutOffAngle

    # Convert

    # lamps have their direction as -z, y==up
    mtx = Matrix.Translation(location) * Vector(direction).to_track_quat('-Z', 'Y').to_matrix().to_4x4()

    return bpylamp, mtx


def importLamp(bpyscene, node, spec, ancestry, global_matrix):
    if spec == 'PointLight':
        bpylamp, mtx = importLamp_PointLight(node, ancestry)
    elif spec == 'DirectionalLight':
        bpylamp, mtx = importLamp_DirectionalLight(node, ancestry)
    elif spec == 'SpotLight':
        bpylamp, mtx = importLamp_SpotLight(node, ancestry)
    else:
        print("Error, not a lamp")
        raise ValueError

    bpyob = node.blendData = node.blendObject = bpy.data.objects.new(bpylamp.name, bpylamp)
    bpyscene.objects.link(bpyob).select = True

    bpyob.matrix_world = getFinalMatrix(node, mtx, ancestry, global_matrix)


# -----------------------------------------------------------------------------------


def importViewpoint(bpyscene, node, ancestry, global_matrix):
    name = node.getDefName()
    if not name:
        name = 'Viewpoint'

    fieldOfView = node.getFieldAsFloat('fieldOfView', 0.785398, ancestry)  # max is documented to be 1.0 but some files have higher.
    # jump = node.getFieldAsBool('jump', True, ancestry)
    orientation = node.getFieldAsFloatTuple('orientation', (0.0, 0.0, 1.0, 0.0), ancestry)
    position = node.getFieldAsFloatTuple('position', (0.0, 0.0, 0.0), ancestry)
    description = node.getFieldAsString('description', '', ancestry)

    bpycam = bpy.data.cameras.new(name)

    bpycam.angle = fieldOfView

    mtx = Matrix.Translation(Vector(position)) * translateRotation(orientation)

    bpyob = node.blendData = node.blendObject = bpy.data.objects.new(name, bpycam)
    bpyscene.objects.link(bpyob).select = True
    bpyob.matrix_world = getFinalMatrix(node, mtx, ancestry, global_matrix)


def importTransform(bpyscene, node, ancestry, global_matrix):
    name = node.getDefName()
    if not name:
        name = 'Transform'

    bpyob = node.blendData = node.blendObject = bpy.data.objects.new(name, None)
    bpyscene.objects.link(bpyob).select = True

    bpyob.matrix_world = getFinalMatrix(node, None, ancestry, global_matrix)

    # so they are not too annoying
    bpyob.empty_draw_type = 'PLAIN_AXES'
    bpyob.empty_draw_size = 0.2


#def importTimeSensor(node):
def action_fcurve_ensure(action, data_path, array_index):
    for fcu in action.fcurves:
        if fcu.data_path == data_path and fcu.array_index == array_index:
            return fcu

    return action.fcurves.new(data_path=data_path, index=array_index)


def translatePositionInterpolator(node, action, ancestry):
    key = node.getFieldAsArray('key', 0, ancestry)
    keyValue = node.getFieldAsArray('keyValue', 3, ancestry)

    loc_x = action_fcurve_ensure(action, "location", 0)
    loc_y = action_fcurve_ensure(action, "location", 1)
    loc_z = action_fcurve_ensure(action, "location", 2)

    for i, time in enumerate(key):
        try:
            x, y, z = keyValue[i]
        except:
            continue

        loc_x.keyframe_points.insert(time, x)
        loc_y.keyframe_points.insert(time, y)
        loc_z.keyframe_points.insert(time, z)

    for fcu in (loc_x, loc_y, loc_z):
        for kf in fcu.keyframe_points:
            kf.interpolation = 'LINEAR'


def translateOrientationInterpolator(node, action, ancestry):
    key = node.getFieldAsArray('key', 0, ancestry)
    keyValue = node.getFieldAsArray('keyValue', 4, ancestry)

    rot_x = action_fcurve_ensure(action, "rotation_euler", 0)
    rot_y = action_fcurve_ensure(action, "rotation_euler", 1)
    rot_z = action_fcurve_ensure(action, "rotation_euler", 2)

    for i, time in enumerate(key):
        try:
            x, y, z, w = keyValue[i]
        except:
            continue

        mtx = translateRotation((x, y, z, w))
        eul = mtx.to_euler()
        rot_x.keyframe_points.insert(time, eul.x)
        rot_y.keyframe_points.insert(time, eul.y)
        rot_z.keyframe_points.insert(time, eul.z)

    for fcu in (rot_x, rot_y, rot_z):
        for kf in fcu.keyframe_points:
            kf.interpolation = 'LINEAR'


# Untested!
def translateScalarInterpolator(node, action, ancestry):
    key = node.getFieldAsArray('key', 0, ancestry)
    keyValue = node.getFieldAsArray('keyValue', 4, ancestry)

    sca_x = action_fcurve_ensure(action, "scale", 0)
    sca_y = action_fcurve_ensure(action, "scale", 1)
    sca_z = action_fcurve_ensure(action, "scale", 2)

    for i, time in enumerate(key):
        try:
            x, y, z = keyValue[i]
        except:
            continue

        sca_x.keyframe_points.new(time, x)
        sca_y.keyframe_points.new(time, y)
        sca_z.keyframe_points.new(time, z)


def translateTimeSensor(node, action, ancestry):
    """
    Apply a time sensor to an action, VRML has many combinations of loop/start/stop/cycle times
    to give different results, for now just do the basics
    """

    # XXX25 TODO
    if 1:
        return

    time_cu = action.addCurve('Time')
    time_cu.interpolation = Blender.IpoCurve.InterpTypes.LINEAR

    cycleInterval = node.getFieldAsFloat('cycleInterval', None, ancestry)

    startTime = node.getFieldAsFloat('startTime', 0.0, ancestry)
    stopTime = node.getFieldAsFloat('stopTime', 250.0, ancestry)

    if cycleInterval is not None:
        stopTime = startTime + cycleInterval

    loop = node.getFieldAsBool('loop', False, ancestry)

    time_cu.append((1 + startTime, 0.0))
    time_cu.append((1 + stopTime, 1.0 / 10.0))  # anoying, the UI uses /10

    if loop:
        time_cu.extend = Blender.IpoCurve.ExtendTypes.CYCLIC  # or - EXTRAP, CYCLIC_EXTRAP, CONST,


def importRoute(node, ancestry):
    """
    Animation route only at the moment
    """

    if not hasattr(node, 'fields'):
        return

    routeIpoDict = node.getRouteIpoDict()

    def getIpo(act_id):
        try:
            action = routeIpoDict[act_id]
        except:
            action = routeIpoDict[act_id] = bpy.data.actions.new('web3d_ipo')
        return action

    # for getting definitions
    defDict = node.getDefDict()
    """
    Handles routing nodes to eachother

ROUTE vpPI.value_changed TO champFly001.set_position
ROUTE vpOI.value_changed TO champFly001.set_orientation
ROUTE vpTs.fraction_changed TO vpPI.set_fraction
ROUTE vpTs.fraction_changed TO vpOI.set_fraction
ROUTE champFly001.bindTime TO vpTs.set_startTime
    """

    #from_id, from_type = node.id[1].split('.')
    #to_id, to_type = node.id[3].split('.')

    #value_changed
    set_position_node = None
    set_orientation_node = None
    time_node = None

    for field in node.fields:
        if field and field[0] == 'ROUTE':
            try:
                from_id, from_type = field[1].split('.')
                to_id, to_type = field[3].split('.')
            except:
                print("Warning, invalid ROUTE", field)
                continue

            if from_type == 'value_changed':
                if to_type == 'set_position':
                    action = getIpo(to_id)
                    set_data_from_node = defDict[from_id]
                    translatePositionInterpolator(set_data_from_node, action, ancestry)

                if to_type in {'set_orientation', 'rotation'}:
                    action = getIpo(to_id)
                    set_data_from_node = defDict[from_id]
                    translateOrientationInterpolator(set_data_from_node, action, ancestry)

                if to_type == 'set_scale':
                    action = getIpo(to_id)
                    set_data_from_node = defDict[from_id]
                    translateScalarInterpolator(set_data_from_node, action, ancestry)

            elif from_type == 'bindTime':
                action = getIpo(from_id)
                time_node = defDict[to_id]
                translateTimeSensor(time_node, action, ancestry)


def load_web3d(
        bpyscene,
        filepath,
        *,
        PREF_FLAT=False,
        PREF_CIRCLE_DIV=16,
        global_matrix=None,
        HELPER_FUNC=None
        ):

    # Used when adding blender primitives
    GLOBALS['CIRCLE_DETAIL'] = PREF_CIRCLE_DIV

    #root_node = vrml_parse('/_Cylinder.wrl')
    if filepath.lower().endswith('.x3d'):
        root_node, msg = x3d_parse(filepath)
    else:
        root_node, msg = vrml_parse(filepath)

    if not root_node:
        print(msg)
        return

    if global_matrix is None:
        global_matrix = Matrix()

    # fill with tuples - (node, [parents-parent, parent])
    all_nodes = root_node.getSerialized([], [])

    for node, ancestry in all_nodes:
        #if 'castle.wrl' not in node.getFilename():
        #   continue

        spec = node.getSpec()
        '''
        prefix = node.getPrefix()
        if prefix=='PROTO':
            pass
        else
        '''
        if HELPER_FUNC and HELPER_FUNC(node, ancestry):
            # Note, include this function so the VRML/X3D importer can be extended
            # by an external script. - gets first pick
            pass
        if spec == 'Shape':
            importShape(bpyscene, node, ancestry, global_matrix)
        elif spec in {'PointLight', 'DirectionalLight', 'SpotLight'}:
            importLamp(bpyscene, node, spec, ancestry, global_matrix)
        elif spec == 'Viewpoint':
            importViewpoint(bpyscene, node, ancestry, global_matrix)
        elif spec == 'Transform':
            # Only use transform nodes when we are not importing a flat object hierarchy
            if PREF_FLAT == False:
                importTransform(bpyscene, node, ancestry, global_matrix)
            '''
        # These are delt with later within importRoute
        elif spec=='PositionInterpolator':
            action = bpy.data.ipos.new('web3d_ipo', 'Object')
            translatePositionInterpolator(node, action)
            '''

    # After we import all nodes, route events - anim paths
    for node, ancestry in all_nodes:
        importRoute(node, ancestry)

    for node, ancestry in all_nodes:
        if node.isRoot():
            # we know that all nodes referenced from will be in
            # routeIpoDict so no need to run node.getDefDict() for every node.
            routeIpoDict = node.getRouteIpoDict()
            defDict = node.getDefDict()

            for key, action in routeIpoDict.items():

                # Assign anim curves
                node = defDict[key]
                if node.blendData is None:  # Add an object if we need one for animation
                    node.blendData = node.blendObject = bpy.data.objects.new('AnimOb', None)  # , name)
                    bpyscene.objects.link(node.blendObject).select = True

                if node.blendData.animation_data is None:
                    node.blendData.animation_data_create()

                node.blendData.animation_data.action = action

    # Add in hierarchy
    if PREF_FLAT is False:
        child_dict = {}
        for node, ancestry in all_nodes:
            if node.blendObject:
                blendObject = None

                # Get the last parent
                i = len(ancestry)
                while i:
                    i -= 1
                    blendObject = ancestry[i].blendObject
                    if blendObject:
                        break

                if blendObject:
                    # Parent Slow, - 1 liner but works
                    # blendObject.makeParent([node.blendObject], 0, 1)

                    # Parent FAST
                    try:
                        child_dict[blendObject].append(node.blendObject)
                    except:
                        child_dict[blendObject] = [node.blendObject]

        # Parent
        for parent, children in child_dict.items():
            for c in children:
                c.parent = parent

        # update deps
        bpyscene.update()
        del child_dict


def load_with_profiler(
        context,
        filepath,
        *,
        global_matrix=None
        ):
    import cProfile
    import pstats
    pro = cProfile.Profile()
    pro.runctx("load_web3d(context.scene, filepath, PREF_FLAT=True, "
               "PREF_CIRCLE_DIV=16, global_matrix=global_matrix)",
               globals(), locals())
    st = pstats.Stats(pro)
    st.sort_stats("time")
    st.print_stats(0.1)
    # st.print_callers(0.1)


def load(context,
         filepath,
         *,
         global_matrix=None
         ):

    # loadWithProfiler(operator, context, filepath, global_matrix)
    load_web3d(context.scene, filepath,
               PREF_FLAT=True,
               PREF_CIRCLE_DIV=16,
               global_matrix=global_matrix,
               )

    return {'FINISHED'}
