# cmake script, to be called on its own with 3 defined args
#
# - FILE_FROM
# - FILE_TO
# - VAR_NAME

# not highly optimal, may replace with generated C program like makesdna
file(READ ${FILE_FROM} file_from_string HEX)
string(LENGTH ${file_from_string} _max_index)
math(EXPR size_on_disk ${_max_index}/2)

file(REMOVE ${FILE_TO})

file(APPEND ${FILE_TO} "int  ${VAR_NAME}_size = ${size_on_disk};\n")
file(APPEND ${FILE_TO} "char ${VAR_NAME}[] = {")

set(_index 0)

while(NOT _index EQUAL _max_index)
    string(SUBSTRING "${file_from_string}" ${_index} 2 _pair)
    file(APPEND ${FILE_TO} "0x${_pair},")
    math(EXPR _index ${_index}+2)
endwhile()
# null terminator not essential but good if we want plane strings encoded
file(APPEND ${FILE_TO} "0x00};\n")
