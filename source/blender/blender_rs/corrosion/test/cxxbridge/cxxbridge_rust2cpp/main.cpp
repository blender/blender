#include <cxxbridge-cpp/foo/mod.h>
#include <cxxbridge-cpp/lib.h>
#include <vector>

int main(int argc, char **argv)
{
    std::vector<uint64_t> input = { 4, 5, 6};
    rust::Slice<const ::std::uint64_t> slice{input.data(), input.size()};
    lib::print(slice);
    foo::print(slice);
}
