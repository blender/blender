# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This script is used to generate parts of `vk_to_string.hh` and `vk_to_string.cc` based on the
vulkan API commands, features and extensions we use.

When to use?

Every time we use a new `vkCmd*` or new extension, we should re-generate to detect updates.
Extensions can alter enum types which we would use.

How to use?

Generate source code that can be copied in `vk_to_string.cc`:
  `python3 vk_to_string.py <path-to-vk-xml>`
Generate source code that can be copied into `vk_to_string.hh`:
  `python3 vk_to_string.py <path-to-vk-xml> --header`

Every vulkan installation contains a `vk.xml` which contains the specification in a machine
readable format. `vk.xml` is also part of the vulkan library in blender libraries.

The generated source code will be printed to the console.
"""
import argparse
import xml.etree.ElementTree as ET


# List of features blender uses. Features can extend enum flags.
FEATURES = [
    "VK_VERSION_1_0",
    "VK_VERSION_1_1",
    "VK_VERSION_1_2",
]
# List of extensions blender uses. These can extend enum flags.
EXTENSIONS = [
    "VK_KHR_swapchain",
]

# List of vkCmd commands blender uses.
COMMANDS_TO_GEN = [
    "vkCmdClearColorImage",
    "vkCmdClearDepthStencilImage",
    "vkCmdClearAttachments",

    "vkCmdCopyImageToBuffer",
    "vkCmdCopyBufferToImage",
    "vkCmdCopyImage",
    "vkCmdCopyBuffer",

    "vkCmdFillBuffer",
    "vkCmdBlitImage",

    "vkCmdBindDescriptorSets",
    "vkCmdPushConstants",
    "vkCmdBindIndexBuffer",
    "vkCmdBindVertexBuffers",
    "vkCmdBindPipeline",

    "vkCmdBeginRenderPass",
    "vkCmdEndRenderPass",
    "vkCmdDraw",
    "vkCmdDrawIndexed",
    "vkCmdDrawIndirect",
    "vkCmdDrawIndexedIndirect",

    "vkCmdDispatch",
    "vkCmdDispatchIndirect",

    "vkCmdPipelineBarrier",
]

DEFAULT_ENUMS_TO_GENERATE = [
    "VkObjectType"
]
ENUMS_TO_IGNORE = [
    "VkStructureType"
]

# A list of struct members to ignore as they aren't supported or not useful.
MEMBERS_TO_IGNORE = [
    "sType", "pNext",
    # Disabled as these are arrays.
    "srcOffsets", "dstOffsets",
    # Disabled as it is an union
    "clearValue",
    # Disabled as we don't use cross queue synchronization
    "srcQueueFamilyIndex", "dstQueueFamilyIndex"
]


### Utils - Formatting ###
def to_lower_snake_case(string):
    result = ""
    for char in string:
        if char.isupper() and len(result) != 0:
            result += "_"
        result += char.lower()
    return result


### Commands ###
def extract_type_names(commands, r_types):
    for command in commands:
        for param in command.findall("param"):
            param_type = param.findtext("type")
            if param_type not in r_types:
                r_types.append(param_type)


### Enumerations ###
def generate_enum_to_string_hpp(enum):
    vk_name = enum.get("name")
    result = ""
    result += f"const char *to_string({vk_name} {to_lower_snake_case(vk_name)});\n"
    return result


def generate_enum_to_string_cpp_case(elem):
    result = ""
    vk_elem_name = elem.get("name")
    result += f"    case {vk_elem_name}:\n"
    result += f"      return STRINGIFY({vk_elem_name});\n\n"
    return result


def generate_enum_to_string_cpp(enum, features, extensions):
    vk_name = enum.get("name")
    vk_name_parameter = to_lower_snake_case(vk_name)
    result = ""
    result += f"const char *to_string(const {vk_name} {vk_name_parameter})\n"
    result += "{\n"
    result += f"  switch ({vk_name_parameter}) {{\n"
    for elem in enum.findall("enum"):
        result += generate_enum_to_string_cpp_case(elem)
    for feature in features:
        enum_extensions = feature.findall(f"require/enum[@extends='{vk_name}']")
        if not enum_extensions:
            continue
        feature_name = feature.get("name")
        result += f"    /* Extensions for {feature_name}. */\n"
        for elem in enum_extensions:
            result += generate_enum_to_string_cpp_case(elem)
    for extension in extensions:
        enum_extensions = extension.findall(f"require/enum[@extends='{vk_name}']")
        if not enum_extensions:
            continue
        extension_name = extension.get("name")
        result += f"    /* Extensions for {extension_name}. */\n"
        for elem in enum_extensions:
            result += generate_enum_to_string_cpp_case(elem)

    result += "    default:\n"
    result += "      break;\n"

    result += "  }\n"
    result += f"  return STRINGIFY_ARG({vk_name_parameter});\n"
    result += "}\n"
    return result


### Bit-flags ###
def generate_bitflag_to_string_hpp(vk_name):
    vk_name_parameter = to_lower_snake_case(vk_name)
    result = ""
    result += f"std::string to_string_{vk_name_parameter}({vk_name} {to_lower_snake_case(vk_name)});\n"
    return result


def generate_bitflag_to_string_cpp_case(vk_parameter_name, elem):
    vk_elem_name = elem.get("name")

    result = ""
    result += f"  if ({vk_parameter_name} & {vk_elem_name}) {{\n"
    result += f"    ss << STRINGIFY({vk_elem_name}) << \", \";\n"
    result += f"  }}\n"
    return result


def generate_bitflag_to_string_cpp(vk_name, enum, features, extensions):
    vk_enum_name = enum.get("name")
    vk_name_parameter = to_lower_snake_case(vk_name)
    result = ""
    result += f"std::string to_string_{vk_name_parameter}(const {vk_name} {vk_name_parameter})\n"
    result += "{\n"
    result += "  std::stringstream ss;\n"
    result += "\n"
    for elem in enum.findall("enum"):
        result += generate_bitflag_to_string_cpp_case(vk_name_parameter, elem)
    for feature in features:
        enum_extensions = feature.findall(f"require/enum[@extends='{vk_enum_name}']")
        if not enum_extensions:
            continue
        feature_name = feature.get("name")
        result += f"  /* Extensions for {feature_name}. */\n"
        for elem in enum_extensions:
            result += generate_bitflag_to_string_cpp_case(vk_name_parameter, elem)
    for extension in extensions:
        enum_extensions = extension.findall(f"require/enum[@extends='{vk_enum_name}']")
        if not enum_extensions:
            continue
        extension_name = extension.get("name")
        result += f"  /* Extensions for {extension_name}. */\n"
        for elem in enum_extensions:
            result += generate_bitflag_to_string_cpp_case(vk_name_parameter, elem)

    result += "\n"
    result += f"  std::string result = ss.str();\n"
    result += f"  if (result.size() >= 2) {{\n"
    result += f"    result.erase(result.size() - 2, 2);\n"
    result += f"  }}\n"
    result += f"  return result;\n"
    result += "}\n"
    return result


### Structs ###
def generate_struct_to_string_hpp(struct):
    vk_name = struct.get("name")
    vk_name_parameter = to_lower_snake_case(vk_name)
    result = ""
    result += f"std::string to_string(const {vk_name} &{vk_name_parameter}, int indentation_level=0);\n"
    return result


def generate_struct_to_string_cpp(struct, flags_to_generate, enums_to_generate, structs_to_generate):
    vk_name = struct.get("name")
    vk_name_parameter = to_lower_snake_case(vk_name)
    header = ""
    header += f"std::string to_string(const {vk_name} &{vk_name_parameter}, int indentation_level)\n"
    header += f"{{\n"
    result = ""
    result += f"  std::stringstream ss;\n"
    pre = ""
    indentation_used = False
    for member in struct.findall("member"):
        member_type = member.findtext("type")
        member_type_parameter = to_lower_snake_case(member_type)
        member_name = member.findtext("name")
        member_name_parameter = to_lower_snake_case(member_name)
        if member_name in MEMBERS_TO_IGNORE:
            continue

        result += f"  ss << \"{pre}{member_name_parameter}=\" << "
        if member_type in flags_to_generate:
            result += f"to_string_{member_type_parameter}({vk_name_parameter}.{member_name})"
        elif member_type in enums_to_generate:
            result += f"to_string({vk_name_parameter}.{member_name})"
        elif member_type in structs_to_generate:
            result += f"\"\\n\";\n"
            result += f"  ss << std::string(indentation_level * 2 + 2, ' ') << to_string({vk_name_parameter}.{member_name}, indentation_level + 1);\n"
            result += f"  ss << std::string(indentation_level * 2, ' ')"
            indentation_used = True
        else:
            result += f"{vk_name_parameter}.{member_name}"
        result += ";\n"
        pre = ", "
    result += f"\n"
    result += f"  return ss.str();\n"
    result += f"}}\n"
    if not indentation_used:
        header += "  UNUSED_VARS(indentation_level);\n"
    return header + result


# Parsing vk.xml
def parse_features(root):
    # Find all features that we use.
    features = []
    for feature_name in FEATURES:
        feature = root.find(f"feature[@name='{feature_name}']")
        assert(feature)
        features.append(feature)
    return features


def parse_extensions(root):
    # Find all extensions that we use.
    extensions = []
    for extension_name in EXTENSIONS:
        extension = root.find(f"extensions/extension[@name='{extension_name}']")
        assert(extension)
        extensions.append(extension)
    return extensions


def parse_all_commands(root):
    commands = []
    for command in root.findall("commands/command"):
        command_name = command.findtext("proto/name")
        if command_name in COMMANDS_TO_GEN:
            commands.append(command)
    return commands


def parse_all_flags(root):
    all_flags = {}
    for flag_type in root.findall("types/type[@category='bitmask']"):
        flag_type_name = flag_type.findtext("name")
        flag_type_bits_name = flag_type.get("requires")
        if flag_type_name and flag_type_bits_name:
            all_flags[flag_type_name] = flag_type_bits_name
    return all_flags


# Extraction of used data types.
def extract_used_types(root, commands, all_flags):
    enums_to_generate = []
    enums_to_generate.extend(DEFAULT_ENUMS_TO_GENERATE)

    flags_to_generate = []
    structs_to_generate = []

    types_undetermined = []
    extract_type_names(commands, types_undetermined)
    while types_undetermined:
        newly_found_types = []
        for type_name in types_undetermined:
            if root.find(f"enums[@name='{type_name}']"):
                if type_name not in enums_to_generate and type_name not in ENUMS_TO_IGNORE:
                    enums_to_generate.append(type_name)
            elif type_name in all_flags and type_name not in flags_to_generate:
                flags_to_generate.append(type_name)
            elif type_name not in structs_to_generate:
                struct = root.find(f"types/type[@category='struct'][@name='{type_name}']")
                if struct:
                    structs_to_generate.append(type_name)
                    for member in struct.findall("member/type"):
                        newly_found_types.append(member.text)

        types_undetermined = newly_found_types

    enums_to_generate.sort()
    flags_to_generate.sort()
    structs_to_generate.sort()

    return (enums_to_generate, flags_to_generate, structs_to_generate)


def generate_to_string(vk_xml, header):
    tree = ET.parse(vk_xml)
    root = tree.getroot()

    commands = parse_all_commands(root)
    features = parse_features(root)
    extensions = parse_extensions(root)
    all_flags = parse_all_flags(root)

    (enums_to_generate, flags_to_generate, structs_to_generate) = extract_used_types(root, commands, all_flags)

    vk_to_string = ""

    if header:
        for enum_to_generate in enums_to_generate:
            for enum in root.findall(f"enums[@name='{enum_to_generate}']"):
                vk_to_string += generate_enum_to_string_hpp(enum)
        for flag_to_generate in flags_to_generate:
            enum_to_generate = all_flags[flag_to_generate]
            for enum in root.findall(f"enums[@name='{enum_to_generate}']"):
                vk_to_string += generate_bitflag_to_string_hpp(flag_to_generate)
        for struct_to_generate in structs_to_generate:
            struct = root.find(f"types/type[@category='struct'][@name='{struct_to_generate}']")
            assert(struct)
            vk_to_string += generate_struct_to_string_hpp(struct)
    else:
        for enum_to_generate in enums_to_generate:
            for enum in root.findall(f"enums[@name='{enum_to_generate}']"):
                vk_to_string += generate_enum_to_string_cpp(enum, features, extensions)
                vk_to_string += "\n"
        for flag_to_generate in flags_to_generate:
            enum_to_generate = all_flags[flag_to_generate]
            for enum in root.findall(f"enums[@name='{enum_to_generate}']"):
                vk_to_string += generate_bitflag_to_string_cpp(flag_to_generate, enum, features, extensions)
                vk_to_string += "\n"

        for struct_to_generate in structs_to_generate:
            struct = root.find(f"types/type[@category='struct'][@name='{struct_to_generate}']")
            assert(struct)
            vk_to_string += generate_struct_to_string_cpp(struct,
                                                          flags_to_generate,
                                                          enums_to_generate,
                                                          structs_to_generate)
            vk_to_string += "\n"

    print(vk_to_string)


if __name__ == "__main__":

    parser = argparse.ArgumentParser(
        prog="vk_to_string.py",
        description="Generator for vk_to_string.cc/hh",
    )
    parser.add_argument("vk_xml", help="path to `vk.xml`")
    parser.add_argument("--header", action='store_true', help="generate parts that belong to `vk_to_string.hh`")
    args = parser.parse_args()
    generate_to_string(**dict(args._get_kwargs()))
