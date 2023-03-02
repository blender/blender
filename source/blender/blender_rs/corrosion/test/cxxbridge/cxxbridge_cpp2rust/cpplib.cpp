#include <iostream>
#include "cpplib.h"
#include "cxxbridge-cpp/lib.h"

// todo: the cxx cli does not seem to generate this binding.
//std::ostream &operator<<(std::ostream &, const rust::Str &);

RsImage read_image(rust::Str path) {
    //std::cout << path << std::endl;
    Rgba c = { 1.0, 2.0, 3.0, 4.0};
    RsImage v = { 1, 1, c};
    return v;
}
void write_image(::rust::Str path, ::RsImage const & image) {

}
