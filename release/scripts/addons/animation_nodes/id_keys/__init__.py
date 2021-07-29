'''
The ID Key system allows to store custom data inside of ID objects (Objects, ...).
Therefor it builds upon the normal ID properties Blender provides.

The used ID properties always have one of the following structures:
    AN*Data_Type*Subproperty_Name*Property_Name
    AN*Data_Type*Property_Name

The total length of this string is limited to 63 characters.

Data_Type, Subproperty_Name and Property_Name must not be empty nor contain '*'.
'''

from . data_types import keyDataTypeItems
from . data_types import dataTypeByIdentifier as IDKeyTypes
from . existing_keys import getAllIDKeys, updateIdKeysList, IDKey, findsIDKeys
