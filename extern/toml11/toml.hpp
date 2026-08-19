#ifndef TOML11_VERSION_HPP
#define TOML11_VERSION_HPP

#define TOML11_VERSION_MAJOR 4
#define TOML11_VERSION_MINOR 4
#define TOML11_VERSION_PATCH 0

#ifndef __cplusplus
#    error "__cplusplus is not defined"
#endif

/*
 * Defines a name for an inline namespace that includes the current version
 * number. This becomes necessary if multiple software packages use toml11 as an
 * internal build-time dependency, since multiple packages will in general not
 * use the same version of toml11. An inline namespace with a version number
 * ensures that the symbols emitted by compiling toml11 into downstream
 * applications will be distinguished by the specific used version of toml11,
 * making it possible to link multiple packages that internally use toml11 in
 * different versions.
 */
#define TOML11_CONCAT(a, b, c, d, e, f) a##b##c##d##e##f

#define TOML11_GENERATE_INLINE_VERSION_NAMESPACE(major, minor, patch)          \
    TOML11_CONCAT(toml11_, major, _, minor, _, patch)

#define TOML11_INLINE_VERSION_NAMESPACE                                        \
    TOML11_GENERATE_INLINE_VERSION_NAMESPACE(                                  \
        TOML11_VERSION_MAJOR, TOML11_VERSION_MINOR, TOML11_VERSION_PATCH)

// Since MSVC does not define `__cplusplus` correctly unless you pass
// `/Zc:__cplusplus` when compiling, the workaround macros are added.
//
// The value of `__cplusplus` macro is defined in the C++ standard spec, but
// MSVC ignores the value, maybe because of backward compatibility. Instead,
// MSVC defines _MSVC_LANG that has the same value as __cplusplus defined in
// the C++ standard. So we check if _MSVC_LANG is defined before using `__cplusplus`.
//
// FYI: https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=msvc-170
//      https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170
//

#if defined(_MSVC_LANG) && defined(_MSC_VER) && 190024210 <= _MSC_FULL_VER
#  define TOML11_CPLUSPLUS_STANDARD_VERSION _MSVC_LANG
#else
#  define TOML11_CPLUSPLUS_STANDARD_VERSION __cplusplus
#endif

#if TOML11_CPLUSPLUS_STANDARD_VERSION < 201103L
#    error "toml11 requires C++11 or later."
#endif

#if ! defined(__has_include)
#  define __has_include(x) 0
#endif

#if ! defined(__has_cpp_attribute)
#  define __has_cpp_attribute(x) 0
#endif

#if ! defined(__has_builtin)
#  define __has_builtin(x) 0
#endif

// hard to remember

#ifndef TOML11_CXX14_VALUE
#define TOML11_CXX14_VALUE 201402L
#endif//TOML11_CXX14_VALUE

#ifndef TOML11_CXX17_VALUE
#define TOML11_CXX17_VALUE 201703L
#endif//TOML11_CXX17_VALUE

#ifndef TOML11_CXX20_VALUE
#define TOML11_CXX20_VALUE 202002L
#endif//TOML11_CXX20_VALUE

#if defined(__cpp_char8_t)
#  if __cpp_char8_t >= 201811L
#    define TOML11_HAS_CHAR8_T 1
#  endif
#endif

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if __has_include(<string_view>)
#    define TOML11_HAS_STRING_VIEW 1
#  endif
#endif

#ifndef TOML11_DISABLE_STD_FILESYSTEM
#  if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#    if __has_include(<filesystem>)
#      define TOML11_HAS_FILESYSTEM 1
#    endif
#  endif
#endif

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if __has_include(<optional>)
#    define TOML11_HAS_OPTIONAL 1
#  endif
#endif

#if defined(TOML11_COMPILE_SOURCES)
#  define TOML11_INLINE
#else
#  define TOML11_INLINE inline
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

inline const char* license_notice() noexcept
{
    return R"(The MIT License (MIT)

Copyright (c) 2017-now Toru Niina

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.)";
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_VERSION_HPP
#ifndef TOML11_SPEC_HPP
#define TOML11_SPEC_HPP

#include <array>
#include <functional>
#include <ostream>
#include <sstream>
#include <utility>

#include <cstdint>


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

struct semantic_version
{
    constexpr semantic_version(std::uint32_t mjr, std::uint32_t mnr, std::uint32_t p) noexcept
        : major{mjr}, minor{mnr}, patch{p}
    {}

    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;
};

constexpr inline semantic_version
make_semver(std::uint32_t mjr, std::uint32_t mnr, std::uint32_t p) noexcept
{
    return semantic_version(mjr, mnr, p);
}

constexpr inline bool
operator==(const semantic_version& lhs, const semantic_version& rhs) noexcept
{
    return lhs.major == rhs.major &&
           lhs.minor == rhs.minor &&
           lhs.patch == rhs.patch;
}
constexpr inline bool
operator!=(const semantic_version& lhs, const semantic_version& rhs) noexcept
{
    return !(lhs == rhs);
}
constexpr inline bool
operator<(const semantic_version& lhs, const semantic_version& rhs) noexcept
{
    return lhs.major < rhs.major ||
           (lhs.major == rhs.major && lhs.minor < rhs.minor) ||
           (lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch < rhs.patch);
}
constexpr inline bool
operator>(const semantic_version& lhs, const semantic_version& rhs) noexcept
{
    return rhs < lhs;
}
constexpr inline bool
operator<=(const semantic_version& lhs, const semantic_version& rhs) noexcept
{
    return !(lhs > rhs);
}
constexpr inline bool
operator>=(const semantic_version& lhs, const semantic_version& rhs) noexcept
{
    return !(lhs < rhs);
}

inline std::ostream& operator<<(std::ostream& os, const semantic_version& v)
{
    os << v.major << '.' << v.minor << '.' << v.patch;
    return os;
}

inline std::string to_string(const semantic_version& v)
{
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

struct spec
{
    constexpr static spec default_version() noexcept
    {
        return spec::v(1, 0, 0);
    }

    constexpr static spec v(std::uint32_t mjr, std::uint32_t mnr, std::uint32_t p) noexcept
    {
        return spec(make_semver(mjr, mnr, p));
    }

    constexpr explicit spec(const semantic_version& semver) noexcept
        : version{semver},
          v1_1_0_allow_newlines_in_inline_tables      {semantic_version{1, 1, 0} <= semver},
          v1_1_0_allow_trailing_comma_in_inline_tables{semantic_version{1, 1, 0} <= semver},
          v1_1_0_add_escape_sequence_e                {semantic_version{1, 1, 0} <= semver},
          v1_1_0_add_escape_sequence_x                {semantic_version{1, 1, 0} <= semver},
          v1_1_0_make_seconds_optional                {semantic_version{1, 1, 0} <= semver},
          ext_allow_control_characters_in_comments{false},
          ext_allow_non_english_in_bare_keys{false},
          ext_hex_float {false},
          ext_num_suffix{false},
          ext_null_value{false}
    {}

    semantic_version version; // toml version

    // diff from v1.0.0 -> v1.1.0
    bool v1_1_0_allow_newlines_in_inline_tables;
    bool v1_1_0_allow_trailing_comma_in_inline_tables;
    bool v1_1_0_add_escape_sequence_e;
    bool v1_1_0_add_escape_sequence_x;
    bool v1_1_0_make_seconds_optional;

    // discussed in toml-lang, but currently not in it
    bool ext_allow_control_characters_in_comments;
    bool ext_allow_non_english_in_bare_keys;

    // library extensions
    bool ext_hex_float;  // allow hex float (in C++ style)
    bool ext_num_suffix; // allow number suffix (in C++ style)
    bool ext_null_value; // allow `null` as a value
};

namespace detail
{
inline std::pair<const semantic_version&, std::array<bool, 10>>
to_tuple(const spec& s) noexcept
{
    return std::make_pair(std::cref(s.version), std::array<bool, 10>{{
            s.v1_1_0_allow_newlines_in_inline_tables,
            s.v1_1_0_allow_trailing_comma_in_inline_tables,
            s.v1_1_0_add_escape_sequence_e,
            s.v1_1_0_add_escape_sequence_x,
            s.v1_1_0_make_seconds_optional,
            s.ext_allow_control_characters_in_comments,
            s.ext_allow_non_english_in_bare_keys,
            s.ext_hex_float,
            s.ext_num_suffix,
            s.ext_null_value
        }});
}
} // detail

inline bool operator==(const spec& lhs, const spec& rhs) noexcept
{
    return detail::to_tuple(lhs) == detail::to_tuple(rhs);
}
inline bool operator!=(const spec& lhs, const spec& rhs) noexcept
{
    return detail::to_tuple(lhs) != detail::to_tuple(rhs);
}
inline bool operator< (const spec& lhs, const spec& rhs) noexcept
{
    return detail::to_tuple(lhs) <  detail::to_tuple(rhs);
}
inline bool operator<=(const spec& lhs, const spec& rhs) noexcept
{
    return detail::to_tuple(lhs) <= detail::to_tuple(rhs);
}
inline bool operator> (const spec& lhs, const spec& rhs) noexcept
{
    return detail::to_tuple(lhs) >  detail::to_tuple(rhs);
}
inline bool operator>=(const spec& lhs, const spec& rhs) noexcept
{
    return detail::to_tuple(lhs) >= detail::to_tuple(rhs);
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_SPEC_HPP
#ifndef TOML11_ORDERED_MAP_HPP
#define TOML11_ORDERED_MAP_HPP

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

namespace detail
{
template<typename Cmp>
struct ordered_map_ebo_container
{
    Cmp cmp_; // empty base optimization for empty Cmp type
};
} // detail

template<typename Key, typename Val, typename Cmp = std::equal_to<Key>,
         typename Allocator = std::allocator<std::pair<Key, Val>>>
class ordered_map : detail::ordered_map_ebo_container<Cmp>
{
  public:
    using key_type    = Key;
    using mapped_type = Val;
    using value_type  = std::pair<Key, Val>;

    using key_compare    = Cmp;
    using allocator_type = Allocator;

    using container_type  = std::vector<value_type, Allocator>;
    using reference       = typename container_type::reference;
    using pointer         = typename container_type::pointer;
    using const_reference = typename container_type::const_reference;
    using const_pointer   = typename container_type::const_pointer;
    using iterator        = typename container_type::iterator;
    using const_iterator  = typename container_type::const_iterator;
    using size_type       = typename container_type::size_type;
    using difference_type = typename container_type::difference_type;

  private:

    using ebo_base = detail::ordered_map_ebo_container<Cmp>;

  public:

    ordered_map() = default;
    ~ordered_map() = default;
    ordered_map(const ordered_map&) = default;
    ordered_map(ordered_map&&)      = default;
    ordered_map& operator=(const ordered_map&) = default;
    ordered_map& operator=(ordered_map&&)      = default;

    ordered_map(const ordered_map& other, const Allocator& alloc)
        : container_(other.container_, alloc)
    {}
    ordered_map(ordered_map&& other, const Allocator& alloc)
        : container_(std::move(other.container_), alloc)
    {}

    explicit ordered_map(const Cmp& cmp, const Allocator& alloc = Allocator())
        : ebo_base{cmp}, container_(alloc)
    {}
    explicit ordered_map(const Allocator& alloc)
        : container_(alloc)
    {}

    template<typename InputIterator>
    ordered_map(InputIterator first, InputIterator last, const Cmp& cmp = Cmp(), const Allocator& alloc = Allocator())
        : ebo_base{cmp}, container_(first, last, alloc)
    {}
    template<typename InputIterator>
    ordered_map(InputIterator first, InputIterator last, const Allocator& alloc)
        : container_(first, last, alloc)
    {}

    ordered_map(std::initializer_list<value_type> v, const Cmp& cmp = Cmp(), const Allocator& alloc = Allocator())
        : ebo_base{cmp}, container_(std::move(v), alloc)
    {}
    ordered_map(std::initializer_list<value_type> v, const Allocator& alloc)
        : container_(std::move(v), alloc)
    {}
    ordered_map& operator=(std::initializer_list<value_type> v)
    {
        this->container_ = std::move(v);
        return *this;
    }

    iterator       begin()        noexcept {return container_.begin();}
    iterator       end()          noexcept {return container_.end();}
    const_iterator begin()  const noexcept {return container_.begin();}
    const_iterator end()    const noexcept {return container_.end();}
    const_iterator cbegin() const noexcept {return container_.cbegin();}
    const_iterator cend()   const noexcept {return container_.cend();}

    bool        empty()    const noexcept {return container_.empty();}
    std::size_t size()     const noexcept {return container_.size();}
    std::size_t max_size() const noexcept {return container_.max_size();}

    void clear() {container_.clear();}

    void push_back(const value_type& v)
    {
        if(this->contains(v.first))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.push_back(v);
    }
    void push_back(value_type&& v)
    {
        if(this->contains(v.first))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.push_back(std::move(v));
    }
    void emplace_back(key_type k, mapped_type v)
    {
        if(this->contains(k))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.emplace_back(std::move(k), std::move(v));
    }
    void pop_back()  {container_.pop_back();}

    void insert(value_type kv)
    {
        if(this->contains(kv.first))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.push_back(std::move(kv));
    }
    void emplace(key_type k, mapped_type v)
    {
        if(this->contains(k))
        {
            throw std::out_of_range("ordered_map: value already exists");
        }
        container_.emplace_back(std::move(k), std::move(v));
    }

    std::size_t count(const key_type& key) const
    {
        if(this->find(key) != this->end())
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    bool contains(const key_type& key) const
    {
        return this->find(key) != this->end();
    }
    iterator find(const key_type& key) noexcept
    {
        return std::find_if(this->begin(), this->end(),
            [&key, this](const value_type& v) {return this->cmp_(v.first, key);});
    }
    const_iterator find(const key_type& key) const noexcept
    {
        return std::find_if(this->begin(), this->end(),
            [&key, this](const value_type& v) {return this->cmp_(v.first, key);});
    }

    mapped_type&       at(const key_type& k)
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            throw std::out_of_range("ordered_map: no such element");
        }
        return iter->second;
    }
    mapped_type const& at(const key_type& k) const
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            throw std::out_of_range("ordered_map: no such element");
        }
        return iter->second;
    }

    iterator erase(iterator pos)
    {
        return container_.erase(pos);
    }

    iterator erase(const_iterator pos)
    {
        return container_.erase(pos);
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        return container_.erase(first, last);
    }

    size_type erase(const key_type& key)
    {
        auto it = this->find(key);
        if (it != this->end())
        {
            container_.erase(it);
            return 1;
        }
        return 0;
    }

    mapped_type& operator[](const key_type& k)
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            this->container_.emplace_back(k, mapped_type{});
            return this->container_.back().second;
        }
        return iter->second;
    }

    mapped_type const& operator[](const key_type& k) const
    {
        const auto iter = this->find(k);
        if(iter == this->end())
        {
            throw std::out_of_range("ordered_map: no such element");
        }
        return iter->second;
    }

    key_compare key_comp() const {return this->cmp_;}

    void swap(ordered_map& other)
    {
        container_.swap(other.container_);
    }

  private:

    container_type container_;
};

template<typename K, typename V, typename C, typename A>
bool operator==(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}
template<typename K, typename V, typename C, typename A>
bool operator!=(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return !(lhs == rhs);
}
template<typename K, typename V, typename C, typename A>
bool operator<(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
template<typename K, typename V, typename C, typename A>
bool operator>(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return rhs < lhs;
}
template<typename K, typename V, typename C, typename A>
bool operator<=(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return !(lhs > rhs);
}
template<typename K, typename V, typename C, typename A>
bool operator>=(const ordered_map<K,V,C,A>& lhs, const ordered_map<K,V,C,A>& rhs)
{
    return !(lhs < rhs);
}

template<typename K, typename V, typename C, typename A>
void swap(ordered_map<K,V,C,A>& lhs, ordered_map<K,V,C,A>& rhs)
{
    lhs.swap(rhs);
    return;
}


} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_ORDERED_MAP_HPP
#ifndef TOML11_INTO_HPP
#define TOML11_INTO_HPP

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

template<typename T>
struct into;
// {
//     static toml::value into_toml(const T& user_defined_type)
//     {
//         // User-defined conversions ...
//     }
// };

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_INTO_HPP
#ifndef TOML11_FROM_HPP
#define TOML11_FROM_HPP

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

template<typename T>
struct from;
// {
//     static T from_toml(const toml::value& v)
//     {
//         // User-defined conversions ...
//     }
// };

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_FROM_HPP
#ifndef TOML11_FORMAT_HPP
#define TOML11_FORMAT_HPP

#ifndef TOML11_FORMAT_FWD_HPP
#define TOML11_FORMAT_FWD_HPP

#include <iosfwd>
#include <string>
#include <utility>

#include <cstddef>
#include <cstdint>


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// toml types with serialization info

enum class indent_char : std::uint8_t
{
    space, // use space
    tab,   // use tab
    none   // no indent
};

std::ostream& operator<<(std::ostream& os, const indent_char& c);
std::string to_string(const indent_char c);

// ----------------------------------------------------------------------------
// boolean

struct boolean_format_info
{
    // nothing, for now
};

inline bool operator==(const boolean_format_info&, const boolean_format_info&) noexcept
{
    return true;
}
inline bool operator!=(const boolean_format_info&, const boolean_format_info&) noexcept
{
    return false;
}

// ----------------------------------------------------------------------------
// integer

enum class integer_format : std::uint8_t
{
    dec = 0,
    bin = 1,
    oct = 2,
    hex = 3,
};

std::ostream& operator<<(std::ostream& os, const integer_format f);
std::string to_string(const integer_format);

struct integer_format_info
{
    integer_format fmt = integer_format::dec;
    bool        uppercase = true; // hex with uppercase
    std::size_t width     = 0;    // minimal width (may exceed)
    std::size_t spacer    = 0;    // position of `_` (if 0, no spacer)
    std::string suffix    = "";   // _suffix (library extension)
};

bool operator==(const integer_format_info&, const integer_format_info&) noexcept;
bool operator!=(const integer_format_info&, const integer_format_info&) noexcept;

// ----------------------------------------------------------------------------
// floating

enum class floating_format : std::uint8_t
{
    defaultfloat = 0,
    fixed        = 1, // does not include exponential part
    scientific   = 2, // always include exponential part
    hex          = 3  // hexfloat extension
};

std::ostream& operator<<(std::ostream& os, const floating_format f);
std::string to_string(const floating_format);

struct floating_format_info
{
    floating_format fmt = floating_format::defaultfloat;
    std::size_t prec  = 0;        // precision (if 0, use the default)
    std::string suffix = "";      // 1.0e+2_suffix (library extension)
};

bool operator==(const floating_format_info&, const floating_format_info&) noexcept;
bool operator!=(const floating_format_info&, const floating_format_info&) noexcept;

// ----------------------------------------------------------------------------
// string

enum class string_format : std::uint8_t
{
    basic             = 0,
    literal           = 1,
    multiline_basic   = 2,
    multiline_literal = 3
};

std::ostream& operator<<(std::ostream& os, const string_format f);
std::string to_string(const string_format);

struct string_format_info
{
    string_format fmt = string_format::basic;
    bool start_with_newline    = false;
};

bool operator==(const string_format_info&, const string_format_info&) noexcept;
bool operator!=(const string_format_info&, const string_format_info&) noexcept;

// ----------------------------------------------------------------------------
// datetime

enum class datetime_delimiter_kind : std::uint8_t
{
    upper_T = 0,
    lower_t = 1,
    space   = 2,
};
std::ostream& operator<<(std::ostream& os, const datetime_delimiter_kind d);
std::string to_string(const datetime_delimiter_kind);

struct offset_datetime_format_info
{
    datetime_delimiter_kind delimiter = datetime_delimiter_kind::upper_T;
    bool has_seconds = true;
    std::size_t subsecond_precision = 6; // [us]
};

bool operator==(const offset_datetime_format_info&, const offset_datetime_format_info&) noexcept;
bool operator!=(const offset_datetime_format_info&, const offset_datetime_format_info&) noexcept;

struct local_datetime_format_info
{
    datetime_delimiter_kind delimiter = datetime_delimiter_kind::upper_T;
    bool has_seconds = true;
    std::size_t subsecond_precision = 6; // [us]
};

bool operator==(const local_datetime_format_info&, const local_datetime_format_info&) noexcept;
bool operator!=(const local_datetime_format_info&, const local_datetime_format_info&) noexcept;

struct local_date_format_info
{
    // nothing, for now
};

bool operator==(const local_date_format_info&, const local_date_format_info&) noexcept;
bool operator!=(const local_date_format_info&, const local_date_format_info&) noexcept;

struct local_time_format_info
{
    bool has_seconds = true;
    std::size_t subsecond_precision = 6; // [us]
};

bool operator==(const local_time_format_info&, const local_time_format_info&) noexcept;
bool operator!=(const local_time_format_info&, const local_time_format_info&) noexcept;

// ----------------------------------------------------------------------------
// array

enum class array_format : std::uint8_t
{
    default_format  = 0,
    oneline         = 1,
    multiline       = 2,
    array_of_tables = 3 // [[format.in.this.way]]
};

std::ostream& operator<<(std::ostream& os, const array_format f);
std::string to_string(const array_format);

struct array_format_info
{
    array_format fmt            = array_format::default_format;
    indent_char  indent_type    = indent_char::space;
    std::int32_t body_indent    = 4; // indent in case of multiline
    std::int32_t closing_indent = 0; // indent of `]`
};

bool operator==(const array_format_info&, const array_format_info&) noexcept;
bool operator!=(const array_format_info&, const array_format_info&) noexcept;

// ----------------------------------------------------------------------------
// table

enum class table_format : std::uint8_t
{
    multiline         = 0, // [foo] \n bar = "baz"
    oneline           = 1, // foo = {bar = "baz"}
    dotted            = 2, // foo.bar = "baz"
    multiline_oneline = 3, // foo = { \n bar = "baz" \n }
    implicit          = 4  // [x] defined by [x.y.z]. skip in serializer.
};

std::ostream& operator<<(std::ostream& os, const table_format f);
std::string to_string(const table_format);

struct table_format_info
{
    table_format fmt = table_format::multiline;
    indent_char  indent_type    = indent_char::space;
    std::int32_t body_indent    = 0; // indent of values
    std::int32_t name_indent    = 0; // indent of [table]
    std::int32_t closing_indent = 0; // in case of {inline-table}
};

bool operator==(const table_format_info&, const table_format_info&) noexcept;
bool operator!=(const table_format_info&, const table_format_info&) noexcept;

// ----------------------------------------------------------------------------
// wrapper

namespace detail
{
template<typename T, typename F>
struct value_with_format
{
    using value_type  = T;
    using format_type = F;

    value_with_format()  = default;
    ~value_with_format() = default;
    value_with_format(const value_with_format&) = default;
    value_with_format(value_with_format&&)      = default;
    value_with_format& operator=(const value_with_format&) = default;
    value_with_format& operator=(value_with_format&&)      = default;

    value_with_format(value_type v, format_type f)
        : value{std::move(v)}, format{std::move(f)}
    {}

    template<typename U>
    value_with_format(value_with_format<U, format_type> other)
        : value{std::move(other.value)}, format{std::move(other.format)}
    {}

    value_type  value;
    format_type format;
};
} // detail

} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_FORMAT_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_FORMAT_IMPL_HPP
#define TOML11_FORMAT_IMPL_HPP


#include <ostream>
#include <sstream>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// toml types with serialization info

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const indent_char& c)
{
    switch(c)
    {
        case indent_char::space:         {os << "space"        ; break;}
        case indent_char::tab:           {os << "tab"          ; break;}
        case indent_char::none:          {os << "none"         ; break;}
        default:
        {
            os << "unknown indent char: " << static_cast<std::uint8_t>(c);
        }
    }
    return os;
}

TOML11_INLINE std::string to_string(const indent_char c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

// ----------------------------------------------------------------------------
// boolean

// ----------------------------------------------------------------------------
// integer

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const integer_format f)
{
    switch(f)
    {
        case integer_format::dec: {os << "dec"; break;}
        case integer_format::bin: {os << "bin"; break;}
        case integer_format::oct: {os << "oct"; break;}
        case integer_format::hex: {os << "hex"; break;}
        default:
        {
            os << "unknown integer_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const integer_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}


TOML11_INLINE bool operator==(const integer_format_info& lhs, const integer_format_info& rhs) noexcept
{
    return lhs.fmt       == rhs.fmt    &&
           lhs.uppercase == rhs.uppercase &&
           lhs.width     == rhs.width  &&
           lhs.spacer    == rhs.spacer &&
           lhs.suffix    == rhs.suffix ;
}
TOML11_INLINE bool operator!=(const integer_format_info& lhs, const integer_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// floating

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const floating_format f)
{
    switch(f)
    {
        case floating_format::defaultfloat: {os << "defaultfloat"; break;}
        case floating_format::fixed       : {os << "fixed"       ; break;}
        case floating_format::scientific  : {os << "scientific"  ; break;}
        case floating_format::hex         : {os << "hex"         ; break;}
        default:
        {
            os << "unknown floating_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const floating_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const floating_format_info& lhs, const floating_format_info& rhs) noexcept
{
    return lhs.fmt          == rhs.fmt          &&
           lhs.prec         == rhs.prec         &&
           lhs.suffix       == rhs.suffix       ;
}
TOML11_INLINE bool operator!=(const floating_format_info& lhs, const floating_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// string

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const string_format f)
{
    switch(f)
    {
        case string_format::basic            : {os << "basic"            ; break;}
        case string_format::literal          : {os << "literal"          ; break;}
        case string_format::multiline_basic  : {os << "multiline_basic"  ; break;}
        case string_format::multiline_literal: {os << "multiline_literal"; break;}
        default:
        {
            os << "unknown string_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const string_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const string_format_info& lhs, const string_format_info& rhs) noexcept
{
    return lhs.fmt                == rhs.fmt                &&
           lhs.start_with_newline == rhs.start_with_newline ;
}
TOML11_INLINE bool operator!=(const string_format_info& lhs, const string_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}
// ----------------------------------------------------------------------------
// datetime

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const datetime_delimiter_kind d)
{
    switch(d)
    {
        case datetime_delimiter_kind::upper_T: { os << "upper_T, "; break; }
        case datetime_delimiter_kind::lower_t: { os << "lower_t, "; break; }
        case datetime_delimiter_kind::space:   { os << "space, ";   break; }
        default:
        {
            os << "unknown datetime delimiter: " << static_cast<std::uint8_t>(d);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const datetime_delimiter_kind c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const offset_datetime_format_info& lhs, const offset_datetime_format_info& rhs) noexcept
{
    return lhs.delimiter           == rhs.delimiter   &&
           lhs.has_seconds         == rhs.has_seconds &&
           lhs.subsecond_precision == rhs.subsecond_precision ;
}
TOML11_INLINE bool operator!=(const offset_datetime_format_info& lhs, const offset_datetime_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

TOML11_INLINE bool operator==(const local_datetime_format_info& lhs, const local_datetime_format_info& rhs) noexcept
{
    return lhs.delimiter           == rhs.delimiter   &&
           lhs.has_seconds         == rhs.has_seconds &&
           lhs.subsecond_precision == rhs.subsecond_precision ;
}
TOML11_INLINE bool operator!=(const local_datetime_format_info& lhs, const local_datetime_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

TOML11_INLINE bool operator==(const local_date_format_info&, const local_date_format_info&) noexcept
{
    return true;
}
TOML11_INLINE bool operator!=(const local_date_format_info& lhs, const local_date_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

TOML11_INLINE bool operator==(const local_time_format_info& lhs, const local_time_format_info& rhs) noexcept
{
    return lhs.has_seconds         == rhs.has_seconds         &&
           lhs.subsecond_precision == rhs.subsecond_precision ;
}
TOML11_INLINE bool operator!=(const local_time_format_info& lhs, const local_time_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// array

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const array_format f)
{
    switch(f)
    {
        case array_format::default_format : {os << "default_format" ; break;}
        case array_format::oneline        : {os << "oneline"        ; break;}
        case array_format::multiline      : {os << "multiline"      ; break;}
        case array_format::array_of_tables: {os << "array_of_tables"; break;}
        default:
        {
            os << "unknown array_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const array_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const array_format_info& lhs, const array_format_info& rhs) noexcept
{
    return lhs.fmt            == rhs.fmt            &&
           lhs.indent_type    == rhs.indent_type    &&
           lhs.body_indent    == rhs.body_indent    &&
           lhs.closing_indent == rhs.closing_indent ;
}
TOML11_INLINE bool operator!=(const array_format_info& lhs, const array_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// table

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const table_format f)
{
    switch(f)
    {
        case table_format::multiline        : {os << "multiline"        ; break;}
        case table_format::oneline          : {os << "oneline"          ; break;}
        case table_format::dotted           : {os << "dotted"           ; break;}
        case table_format::multiline_oneline: {os << "multiline_oneline"; break;}
        case table_format::implicit         : {os << "implicit"         ; break;}
        default:
        {
            os << "unknown table_format: " << static_cast<std::uint8_t>(f);
            break;
        }
    }
    return os;
}
TOML11_INLINE std::string to_string(const table_format c)
{
    std::ostringstream oss;
    oss << c;
    return oss.str();
}

TOML11_INLINE bool operator==(const table_format_info& lhs, const table_format_info& rhs) noexcept
{
    return lhs.fmt                == rhs.fmt              &&
           lhs.indent_type        == rhs.indent_type      &&
           lhs.body_indent        == rhs.body_indent      &&
           lhs.name_indent        == rhs.name_indent      &&
           lhs.closing_indent     == rhs.closing_indent   ;
}
TOML11_INLINE bool operator!=(const table_format_info& lhs, const table_format_info& rhs) noexcept
{
    return !(lhs == rhs);
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_FORMAT_IMPL_HPP
#endif

#endif// TOML11_FORMAT_HPP
#ifndef TOML11_EXCEPTION_HPP
#define TOML11_EXCEPTION_HPP

#include <exception>


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

struct exception : public std::exception
{
  public:
    virtual ~exception() noexcept override = default;
    virtual const char* what() const noexcept override {return "";}
};

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOMl11_EXCEPTION_HPP
#ifndef TOML11_DATETIME_HPP
#define TOML11_DATETIME_HPP

#ifndef TOML11_DATETIME_FWD_HPP
#define TOML11_DATETIME_FWD_HPP

#include <chrono>
#include <iosfwd>
#include <string>

#include <cstdint>
#include <cstdlib>
#include <ctime>


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

enum class month_t : std::uint8_t
{
    Jan =  0,
    Feb =  1,
    Mar =  2,
    Apr =  3,
    May =  4,
    Jun =  5,
    Jul =  6,
    Aug =  7,
    Sep =  8,
    Oct =  9,
    Nov = 10,
    Dec = 11
};

// ----------------------------------------------------------------------------

struct local_date
{
    std::int16_t year{0};   // A.D. (like, 2018)
    std::uint8_t month{0};  // [0, 11]
    std::uint8_t day{0};    // [1, 31]

    local_date(int y, month_t m, int d)
        : year {static_cast<std::int16_t>(y)},
          month{static_cast<std::uint8_t>(m)},
          day  {static_cast<std::uint8_t>(d)}
    {}

    explicit local_date(const std::tm& t)
        : year {static_cast<std::int16_t>(t.tm_year + 1900)},
          month{static_cast<std::uint8_t>(t.tm_mon)},
          day  {static_cast<std::uint8_t>(t.tm_mday)}
    {}

    explicit local_date(const std::chrono::system_clock::time_point& tp);
    explicit local_date(const std::time_t t);

    operator std::chrono::system_clock::time_point() const;
    operator std::time_t() const;

    local_date() = default;
    ~local_date() = default;
    local_date(local_date const&) = default;
    local_date(local_date&&)      = default;
    local_date& operator=(local_date const&) = default;
    local_date& operator=(local_date&&)      = default;
};
bool operator==(const local_date& lhs, const local_date& rhs);
bool operator!=(const local_date& lhs, const local_date& rhs);
bool operator< (const local_date& lhs, const local_date& rhs);
bool operator<=(const local_date& lhs, const local_date& rhs);
bool operator> (const local_date& lhs, const local_date& rhs);
bool operator>=(const local_date& lhs, const local_date& rhs);

std::ostream& operator<<(std::ostream& os, const local_date& date);
std::string to_string(const local_date& date);

// -----------------------------------------------------------------------------

struct local_time
{
    std::uint8_t  hour{0};        // [0, 23]
    std::uint8_t  minute{0};      // [0, 59]
    std::uint8_t  second{0};      // [0, 60]
    std::uint16_t millisecond{0}; // [0, 999]
    std::uint16_t microsecond{0}; // [0, 999]
    std::uint16_t nanosecond{0};  // [0, 999]

    local_time(int h, int m, int s,
               int ms = 0, int us = 0, int ns = 0)
        : hour  {static_cast<std::uint8_t>(h)},
          minute{static_cast<std::uint8_t>(m)},
          second{static_cast<std::uint8_t>(s)},
          millisecond{static_cast<std::uint16_t>(ms)},
          microsecond{static_cast<std::uint16_t>(us)},
          nanosecond {static_cast<std::uint16_t>(ns)}
    {}

    explicit local_time(const std::tm& t)
        : hour  {static_cast<std::uint8_t>(t.tm_hour)},
          minute{static_cast<std::uint8_t>(t.tm_min )},
          second{static_cast<std::uint8_t>(t.tm_sec )},
          millisecond{0}, microsecond{0}, nanosecond{0}
    {}

    template<typename Rep, typename Period>
    explicit local_time(const std::chrono::duration<Rep, Period>& t)
    {
        const auto h = std::chrono::duration_cast<std::chrono::hours>(t);
        this->hour = static_cast<std::uint8_t>(h.count());
        const auto t2 = t - h;
        const auto m = std::chrono::duration_cast<std::chrono::minutes>(t2);
        this->minute = static_cast<std::uint8_t>(m.count());
        const auto t3 = t2 - m;
        const auto s = std::chrono::duration_cast<std::chrono::seconds>(t3);
        this->second = static_cast<std::uint8_t>(s.count());
        const auto t4 = t3 - s;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t4);
        this->millisecond = static_cast<std::uint16_t>(ms.count());
        const auto t5 = t4 - ms;
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t5);
        this->microsecond = static_cast<std::uint16_t>(us.count());
        const auto t6 = t5 - us;
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t6);
        this->nanosecond = static_cast<std::uint16_t>(ns.count());
    }

    operator std::chrono::nanoseconds() const;

    local_time() = default;
    ~local_time() = default;
    local_time(local_time const&) = default;
    local_time(local_time&&)      = default;
    local_time& operator=(local_time const&) = default;
    local_time& operator=(local_time&&)      = default;
};

bool operator==(const local_time& lhs, const local_time& rhs);
bool operator!=(const local_time& lhs, const local_time& rhs);
bool operator< (const local_time& lhs, const local_time& rhs);
bool operator<=(const local_time& lhs, const local_time& rhs);
bool operator> (const local_time& lhs, const local_time& rhs);
bool operator>=(const local_time& lhs, const local_time& rhs);

std::ostream& operator<<(std::ostream& os, const local_time& time);
std::string to_string(const local_time& time);

// ----------------------------------------------------------------------------

struct time_offset
{
    std::int8_t hour{0};   // [-12, 12]
    std::int8_t minute{0}; // [-59, 59]

    time_offset(int h, int m)
        : hour  {static_cast<std::int8_t>(h)},
          minute{static_cast<std::int8_t>(m)}
    {}

    operator std::chrono::minutes() const;

    time_offset() = default;
    ~time_offset() = default;
    time_offset(time_offset const&) = default;
    time_offset(time_offset&&)      = default;
    time_offset& operator=(time_offset const&) = default;
    time_offset& operator=(time_offset&&)      = default;
};

bool operator==(const time_offset& lhs, const time_offset& rhs);
bool operator!=(const time_offset& lhs, const time_offset& rhs);
bool operator< (const time_offset& lhs, const time_offset& rhs);
bool operator<=(const time_offset& lhs, const time_offset& rhs);
bool operator> (const time_offset& lhs, const time_offset& rhs);
bool operator>=(const time_offset& lhs, const time_offset& rhs);

std::ostream& operator<<(std::ostream& os, const time_offset& offset);

std::string to_string(const time_offset& offset);

// -----------------------------------------------------------------------------

struct local_datetime
{
    local_date date{};
    local_time time{};

    local_datetime(local_date d, local_time t): date{d}, time{t} {}

    explicit local_datetime(const std::tm& t): date{t}, time{t}{}

    explicit local_datetime(const std::chrono::system_clock::time_point& tp);
    explicit local_datetime(const std::time_t t);

    operator std::chrono::system_clock::time_point() const;
    operator std::time_t() const;

    local_datetime() = default;
    ~local_datetime() = default;
    local_datetime(local_datetime const&) = default;
    local_datetime(local_datetime&&)      = default;
    local_datetime& operator=(local_datetime const&) = default;
    local_datetime& operator=(local_datetime&&)      = default;
};

bool operator==(const local_datetime& lhs, const local_datetime& rhs);
bool operator!=(const local_datetime& lhs, const local_datetime& rhs);
bool operator< (const local_datetime& lhs, const local_datetime& rhs);
bool operator<=(const local_datetime& lhs, const local_datetime& rhs);
bool operator> (const local_datetime& lhs, const local_datetime& rhs);
bool operator>=(const local_datetime& lhs, const local_datetime& rhs);

std::ostream& operator<<(std::ostream& os, const local_datetime& dt);

std::string to_string(const local_datetime& dt);

// -----------------------------------------------------------------------------

struct offset_datetime
{
    local_date  date{};
    local_time  time{};
    time_offset offset{};

    offset_datetime(local_date d, local_time t, time_offset o)
        : date{d}, time{t}, offset{o}
    {}
    offset_datetime(const local_datetime& dt, time_offset o)
        : date{dt.date}, time{dt.time}, offset{o}
    {}
    // use the current local timezone offset
    explicit offset_datetime(const local_datetime& ld);
    explicit offset_datetime(const std::chrono::system_clock::time_point& tp);
    explicit offset_datetime(const std::time_t& t);
    explicit offset_datetime(const std::tm& t);

    operator std::chrono::system_clock::time_point() const;

    operator std::time_t() const;

    offset_datetime() = default;
    ~offset_datetime() = default;
    offset_datetime(offset_datetime const&) = default;
    offset_datetime(offset_datetime&&)      = default;
    offset_datetime& operator=(offset_datetime const&) = default;
    offset_datetime& operator=(offset_datetime&&)      = default;

  private:

    static time_offset get_local_offset(const std::time_t* tp);
};

bool operator==(const offset_datetime& lhs, const offset_datetime& rhs);
bool operator!=(const offset_datetime& lhs, const offset_datetime& rhs);
bool operator< (const offset_datetime& lhs, const offset_datetime& rhs);
bool operator<=(const offset_datetime& lhs, const offset_datetime& rhs);
bool operator> (const offset_datetime& lhs, const offset_datetime& rhs);
bool operator>=(const offset_datetime& lhs, const offset_datetime& rhs);

std::ostream& operator<<(std::ostream& os, const offset_datetime& dt);

std::string to_string(const offset_datetime& dt);

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_DATETIME_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_DATETIME_IMPL_HPP
#define TOML11_DATETIME_IMPL_HPP


#include <array>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <tuple>

#include <cstdlib>
#include <ctime>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// To avoid non-threadsafe std::localtime. In C11 (not C++11!), localtime_s is
// provided in the absolutely same purpose, but C++11 is actually not compatible
// with C11. We need to dispatch the function depending on the OS.
namespace detail
{
// TODO: find more sophisticated way to handle this
#if defined(_MSC_VER)
TOML11_INLINE std::tm localtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::localtime_s(&dst, src);
    if (result) { throw std::runtime_error("localtime_s failed."); }
    return dst;
}
TOML11_INLINE std::tm gmtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::gmtime_s(&dst, src);
    if (result) { throw std::runtime_error("gmtime_s failed."); }
    return dst;
}
#elif (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 1) || defined(_XOPEN_SOURCE) || defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_POSIX_SOURCE)
TOML11_INLINE std::tm localtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::localtime_r(src, &dst);
    if (!result) { throw std::runtime_error("localtime_r failed."); }
    return dst;
}
TOML11_INLINE std::tm gmtime_s(const std::time_t* src)
{
    std::tm dst;
    const auto result = ::gmtime_r(src, &dst);
    if (!result) { throw std::runtime_error("gmtime_r failed."); }
    return dst;
}
#else // fallback. not threadsafe
TOML11_INLINE std::tm localtime_s(const std::time_t* src)
{
    const auto result = std::localtime(src);
    if (!result) { throw std::runtime_error("localtime failed."); }
    return *result;
}
TOML11_INLINE std::tm gmtime_s(const std::time_t* src)
{
    const auto result = std::gmtime(src);
    if (!result) { throw std::runtime_error("gmtime failed."); }
    return *result;
}
#endif
} // detail

// ----------------------------------------------------------------------------

TOML11_INLINE local_date::local_date(const std::chrono::system_clock::time_point& tp)
{
    const auto t    = std::chrono::system_clock::to_time_t(tp);
    const auto time = detail::localtime_s(&t);
    *this = local_date(time);
}

TOML11_INLINE local_date::local_date(const std::time_t t)
    : local_date{std::chrono::system_clock::from_time_t(t)}
{}

TOML11_INLINE local_date::operator std::chrono::system_clock::time_point() const
{
    // std::mktime returns date as local time zone. no conversion needed
    std::tm t;
    t.tm_sec   = 0;
    t.tm_min   = 0;
    t.tm_hour  = 0;
    t.tm_mday  = static_cast<int>(this->day);
    t.tm_mon   = static_cast<int>(this->month);
    t.tm_year  = static_cast<int>(this->year) - 1900;
    t.tm_wday  = 0; // the value will be ignored
    t.tm_yday  = 0; // the value will be ignored
    t.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(std::mktime(&t));
}

TOML11_INLINE local_date::operator std::time_t() const
{
    return std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(*this));
}

TOML11_INLINE bool operator==(const local_date& lhs, const local_date& rhs)
{
    return std::make_tuple(lhs.year, lhs.month, lhs.day) ==
           std::make_tuple(rhs.year, rhs.month, rhs.day);
}
TOML11_INLINE bool operator!=(const local_date& lhs, const local_date& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const local_date& lhs, const local_date& rhs)
{
    return std::make_tuple(lhs.year, lhs.month, lhs.day) <
           std::make_tuple(rhs.year, rhs.month, rhs.day);
}
TOML11_INLINE bool operator<=(const local_date& lhs, const local_date& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const local_date& lhs, const local_date& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const local_date& lhs, const local_date& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const local_date& date)
{
    os << std::setfill('0') << std::setw(4) << static_cast<int>(date.year )     << '-';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(date.month) + 1 << '-';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(date.day  )    ;
    return os;
}

TOML11_INLINE std::string to_string(const local_date& date)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << date;
    return oss.str();
}

// -----------------------------------------------------------------------------

TOML11_INLINE local_time::operator std::chrono::nanoseconds() const
{
    return std::chrono::nanoseconds (this->nanosecond)  +
           std::chrono::microseconds(this->microsecond) +
           std::chrono::milliseconds(this->millisecond) +
           std::chrono::seconds(this->second) +
           std::chrono::minutes(this->minute) +
           std::chrono::hours(this->hour);
}

TOML11_INLINE bool operator==(const local_time& lhs, const local_time& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute, lhs.second, lhs.millisecond, lhs.microsecond, lhs.nanosecond) ==
           std::make_tuple(rhs.hour, rhs.minute, rhs.second, rhs.millisecond, rhs.microsecond, rhs.nanosecond);
}
TOML11_INLINE bool operator!=(const local_time& lhs, const local_time& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const local_time& lhs, const local_time& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute, lhs.second, lhs.millisecond, lhs.microsecond, lhs.nanosecond) <
           std::make_tuple(rhs.hour, rhs.minute, rhs.second, rhs.millisecond, rhs.microsecond, rhs.nanosecond);
}
TOML11_INLINE bool operator<=(const local_time& lhs, const local_time& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const local_time& lhs, const local_time& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const local_time& lhs, const local_time& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const local_time& time)
{
    os << std::setfill('0') << std::setw(2) << static_cast<int>(time.hour  ) << ':';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(time.minute) << ':';
    os << std::setfill('0') << std::setw(2) << static_cast<int>(time.second);
    if(time.millisecond != 0 || time.microsecond != 0 || time.nanosecond != 0)
    {
        os << '.';
        os << std::setfill('0') << std::setw(3) << static_cast<int>(time.millisecond);
        if(time.microsecond != 0 || time.nanosecond != 0)
        {
            os << std::setfill('0') << std::setw(3) << static_cast<int>(time.microsecond);
            if(time.nanosecond != 0)
            {
                os << std::setfill('0') << std::setw(3) << static_cast<int>(time.nanosecond);
            }
        }
    }
    return os;
}

TOML11_INLINE std::string to_string(const local_time& time)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << time;
    return oss.str();
}

// ----------------------------------------------------------------------------

TOML11_INLINE time_offset::operator std::chrono::minutes() const
{
    return std::chrono::minutes(this->minute) +
           std::chrono::hours(this->hour);
}

TOML11_INLINE bool operator==(const time_offset& lhs, const time_offset& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute) ==
           std::make_tuple(rhs.hour, rhs.minute);
}
TOML11_INLINE bool operator!=(const time_offset& lhs, const time_offset& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const time_offset& lhs, const time_offset& rhs)
{
    return std::make_tuple(lhs.hour, lhs.minute) <
           std::make_tuple(rhs.hour, rhs.minute);
}
TOML11_INLINE bool operator<=(const time_offset& lhs, const time_offset& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const time_offset& lhs, const time_offset& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const time_offset& lhs, const time_offset& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const time_offset& offset)
{
    if(offset.hour == 0 && offset.minute == 0)
    {
        os << 'Z';
        return os;
    }
    int minute = static_cast<int>(offset.hour) * 60 + offset.minute;
    if(minute < 0){os << '-'; minute = std::abs(minute);} else {os << '+';}
    os << std::setfill('0') << std::setw(2) << minute / 60 << ':';
    os << std::setfill('0') << std::setw(2) << minute % 60;
    return os;
}

TOML11_INLINE std::string to_string(const time_offset& offset)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << offset;
    return oss.str();
}

// -----------------------------------------------------------------------------

TOML11_INLINE local_datetime::local_datetime(const std::chrono::system_clock::time_point& tp)
{
    const auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm ltime = detail::localtime_s(&t);

    this->date = local_date(ltime);
    this->time = local_time(ltime);

    // std::tm lacks subsecond information, so diff between tp and tm
    // can be used to get millisecond & microsecond information.
    const auto t_diff = tp -
        std::chrono::system_clock::from_time_t(std::mktime(&ltime));
    this->time.millisecond = static_cast<std::uint16_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(t_diff).count());
    this->time.microsecond = static_cast<std::uint16_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(t_diff).count());
    this->time.nanosecond = static_cast<std::uint16_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds >(t_diff).count());
}

TOML11_INLINE local_datetime::local_datetime(const std::time_t t)
    : local_datetime{std::chrono::system_clock::from_time_t(t)}
{}

TOML11_INLINE local_datetime::operator std::chrono::system_clock::time_point() const
{
    using internal_duration =
        typename std::chrono::system_clock::time_point::duration;

    // Normally DST begins at A.M. 3 or 4. If we re-use conversion operator
    // of local_date and local_time independently, the conversion fails if
    // it is the day when DST begins or ends. Since local_date considers the
    // time is 00:00 A.M. and local_time does not consider DST because it
    // does not have any date information. We need to consider both date and
    // time information at the same time to convert it correctly.

    std::tm t;
    t.tm_sec   = static_cast<int>(this->time.second);
    t.tm_min   = static_cast<int>(this->time.minute);
    t.tm_hour  = static_cast<int>(this->time.hour);
    t.tm_mday  = static_cast<int>(this->date.day);
    t.tm_mon   = static_cast<int>(this->date.month);
    t.tm_year  = static_cast<int>(this->date.year) - 1900;
    t.tm_wday  = 0; // the value will be ignored
    t.tm_yday  = 0; // the value will be ignored
    t.tm_isdst = -1;

    // std::mktime returns date as local time zone. no conversion needed
    auto dt = std::chrono::system_clock::from_time_t(std::mktime(&t));
    dt += std::chrono::duration_cast<internal_duration>(
            std::chrono::milliseconds(this->time.millisecond) +
            std::chrono::microseconds(this->time.microsecond) +
            std::chrono::nanoseconds (this->time.nanosecond));
    return dt;
}

TOML11_INLINE local_datetime::operator std::time_t() const
{
    return std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(*this));
}

TOML11_INLINE bool operator==(const local_datetime& lhs, const local_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time) ==
           std::make_tuple(rhs.date, rhs.time);
}
TOML11_INLINE bool operator!=(const local_datetime& lhs, const local_datetime& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const local_datetime& lhs, const local_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time) <
           std::make_tuple(rhs.date, rhs.time);
}
TOML11_INLINE bool operator<=(const local_datetime& lhs, const local_datetime& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const local_datetime& lhs, const local_datetime& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const local_datetime& lhs, const local_datetime& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const local_datetime& dt)
{
    os << dt.date << 'T' << dt.time;
    return os;
}

TOML11_INLINE std::string to_string(const local_datetime& dt)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << dt;
    return oss.str();
}

// -----------------------------------------------------------------------------


TOML11_INLINE offset_datetime::offset_datetime(const local_datetime& ld)
    : date{ld.date}, time{ld.time}, offset{get_local_offset(nullptr)}
      // use the current local timezone offset
{}
TOML11_INLINE offset_datetime::offset_datetime(const std::chrono::system_clock::time_point& tp)
    : offset{0, 0} // use gmtime
{
    const auto timet = std::chrono::system_clock::to_time_t(tp);
    const auto tm    = detail::gmtime_s(&timet);
    this->date = local_date(tm);
    this->time = local_time(tm);
}
TOML11_INLINE offset_datetime::offset_datetime(const std::time_t& t)
    : offset{0, 0} // use gmtime
{
    const auto tm    = detail::gmtime_s(&t);
    this->date = local_date(tm);
    this->time = local_time(tm);
}
TOML11_INLINE offset_datetime::offset_datetime(const std::tm& t)
    : offset{0, 0} // assume gmtime
{
    this->date = local_date(t);
    this->time = local_time(t);
}

TOML11_INLINE offset_datetime::operator std::chrono::system_clock::time_point() const
{
    // get date-time
    using internal_duration =
        typename std::chrono::system_clock::time_point::duration;

    // first, convert it to local date-time information in the same way as
    // local_datetime does. later we will use time_t to adjust time offset.
    std::tm t;
    t.tm_sec   = static_cast<int>(this->time.second);
    t.tm_min   = static_cast<int>(this->time.minute);
    t.tm_hour  = static_cast<int>(this->time.hour);
    t.tm_mday  = static_cast<int>(this->date.day);
    t.tm_mon   = static_cast<int>(this->date.month);
    t.tm_year  = static_cast<int>(this->date.year) - 1900;
    t.tm_wday  = 0; // the value will be ignored
    t.tm_yday  = 0; // the value will be ignored
    t.tm_isdst = -1;
    const std::time_t tp_loc = std::mktime(std::addressof(t));

    auto tp = std::chrono::system_clock::from_time_t(tp_loc);
    tp += std::chrono::duration_cast<internal_duration>(
            std::chrono::milliseconds(this->time.millisecond) +
            std::chrono::microseconds(this->time.microsecond) +
            std::chrono::nanoseconds (this->time.nanosecond));

    // Since mktime uses local time zone, it should be corrected.
    // `12:00:00+09:00` means `03:00:00Z`. So mktime returns `03:00:00Z` if
    // we are in `+09:00` timezone. To represent `12:00:00Z` there, we need
    // to add `+09:00` to `03:00:00Z`.
    //    Here, it uses the time_t converted from date-time info to handle
    // daylight saving time.
    const auto ofs = get_local_offset(std::addressof(tp_loc));
    tp += std::chrono::hours  (ofs.hour);
    tp += std::chrono::minutes(ofs.minute);

    // We got `12:00:00Z` by correcting local timezone applied by mktime.
    // Then we will apply the offset. Let's say `12:00:00-08:00` is given.
    // And now, we have `12:00:00Z`. `12:00:00-08:00` means `20:00:00Z`.
    // So we need to subtract the offset.
    tp -= std::chrono::minutes(this->offset);
    return tp;
}

TOML11_INLINE offset_datetime::operator std::time_t() const
{
    return std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::time_point(*this));
}

TOML11_INLINE time_offset offset_datetime::get_local_offset(const std::time_t* tp)
{
    // get local timezone with the same date-time information as mktime
    const auto t = detail::localtime_s(tp);

    std::array<char, 6> buf;
    const auto result = std::strftime(buf.data(), 6, "%z", &t); // +hhmm\0
    if(result != 5)
    {
        throw std::runtime_error("toml::offset_datetime: cannot obtain "
                                 "timezone information of current env");
    }
    const int ofs = std::atoi(buf.data());
    const int ofs_h = ofs / 100;
    const int ofs_m = ofs - (ofs_h * 100);
    return time_offset(ofs_h, ofs_m);
}

TOML11_INLINE bool operator==(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time, lhs.offset) ==
           std::make_tuple(rhs.date, rhs.time, rhs.offset);
}
TOML11_INLINE bool operator!=(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return !(lhs == rhs);
}
TOML11_INLINE bool operator< (const offset_datetime& lhs, const offset_datetime& rhs)
{
    return std::make_tuple(lhs.date, lhs.time, lhs.offset) <
           std::make_tuple(rhs.date, rhs.time, rhs.offset);
}
TOML11_INLINE bool operator<=(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
TOML11_INLINE bool operator> (const offset_datetime& lhs, const offset_datetime& rhs)
{
    return !(lhs <= rhs);
}
TOML11_INLINE bool operator>=(const offset_datetime& lhs, const offset_datetime& rhs)
{
    return !(lhs < rhs);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const offset_datetime& dt)
{
    os << dt.date << 'T' << dt.time << dt.offset;
    return os;
}

TOML11_INLINE std::string to_string(const offset_datetime& dt)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << dt;
    return oss.str();
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_DATETIME_IMPL_HPP
#endif

#endif // TOML11_DATETIME_HPP
#ifndef TOML11_COMPAT_HPP
#define TOML11_COMPAT_HPP


#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>

#include <cassert>

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if __has_include(<bit>)
#    include <bit>
#  endif
#endif

#include <cstring>

// ----------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if __has_cpp_attribute(deprecated)
#    define TOML11_HAS_ATTR_DEPRECATED 1
#  endif
#endif

#if defined(TOML11_HAS_ATTR_DEPRECATED)
#  define TOML11_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(__GNUC__)
#  define TOML11_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#  define TOML11_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#  define TOML11_DEPRECATED(msg)
#endif

// ----------------------------------------------------------------------------

#if defined(__cpp_if_constexpr)
#  if __cpp_if_constexpr >= 201606L
#    define TOML11_HAS_CONSTEXPR_IF 1
#  endif
#endif

#if defined(TOML11_HAS_CONSTEXPR_IF)
#  define TOML11_CONSTEXPR_IF if constexpr
#else
#  define TOML11_CONSTEXPR_IF if
#endif

// ----------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_make_unique)
#    if __cpp_lib_make_unique >= 201304L
#      define TOML11_HAS_STD_MAKE_UNIQUE 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{

#if defined(TOML11_HAS_STD_MAKE_UNIQUE)

using std::make_unique;

#else

template<typename T, typename ... Ts>
std::unique_ptr<T> make_unique(Ts&& ... args)
{
    return std::unique_ptr<T>(new T(std::forward<Ts>(args)...));
}

#endif // TOML11_HAS_STD_MAKE_UNIQUE

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_make_reverse_iterator)
#    if __cpp_lib_make_reverse_iterator >= 201402L
#      define TOML11_HAS_STD_MAKE_REVERSE_ITERATOR 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
# if defined(TOML11_HAS_STD_MAKE_REVERSE_ITERATOR)

using std::make_reverse_iterator;

#else

template<typename Iterator>
std::reverse_iterator<Iterator> make_reverse_iterator(Iterator iter)
{
    return std::reverse_iterator<Iterator>(iter);
}

#endif // TOML11_HAS_STD_MAKE_REVERSE_ITERATOR

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if defined(__cpp_lib_clamp)
#    if __cpp_lib_clamp >= 201603L
#      define TOML11_HAS_STD_CLAMP 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_CLAMP)

using std::clamp;

#else

template<typename T>
T clamp(const T& x, const T& low, const T& high) noexcept
{
    assert(low <= high);
    return (std::min)((std::max)(x, low), high);
}

#endif // TOML11_HAS_STD_CLAMP

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if defined(__cpp_lib_bit_cast)
#    if __cpp_lib_bit_cast >= 201806L
#      define TOML11_HAS_STD_BIT_CAST 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_BIT_CAST)

using std::bit_cast;

#else

template<typename U, typename T>
U bit_cast(const T& x) noexcept
{
    static_assert(sizeof(T) == sizeof(U), "");
    static_assert(std::is_default_constructible<T>::value, "");

    U z;
    std::memcpy(reinterpret_cast<char*>(std::addressof(z)),
                reinterpret_cast<const char*>(std::addressof(x)),
                sizeof(T));

    return z;
}

#endif // TOML11_HAS_STD_BIT_CAST

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------
// C++20 remove_cvref_t

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX20_VALUE
#  if defined(__cpp_lib_remove_cvref)
#    if __cpp_lib_remove_cvref >= 201711L
#      define TOML11_HAS_STD_REMOVE_CVREF 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_REMOVE_CVREF)

using std::remove_cvref;
using std::remove_cvref_t;

#else

template<typename T>
struct remove_cvref
{
    using type = typename std::remove_cv<
        typename std::remove_reference<T>::type>::type;
};

template<typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

#endif // TOML11_HAS_STD_REMOVE_CVREF

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------
// C++17 and/or/not

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if defined(__cpp_lib_logical_traits)
#    if __cpp_lib_logical_traits >= 201510L
#      define TOML11_HAS_STD_CONJUNCTION 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_CONJUNCTION)

using std::conjunction;
using std::disjunction;
using std::negation;

#else

template<typename ...> struct conjunction : std::true_type{};
template<typename T>   struct conjunction<T> : T{};
template<typename T, typename ... Ts>
struct conjunction<T, Ts...> :
    std::conditional<static_cast<bool>(T::value), conjunction<Ts...>, T>::type
{};

template<typename ...> struct disjunction : std::false_type{};
template<typename T>   struct disjunction<T> : T {};
template<typename T, typename ... Ts>
struct disjunction<T, Ts...> :
    std::conditional<static_cast<bool>(T::value), T, disjunction<Ts...>>::type
{};

template<typename T>
struct negation : std::integral_constant<bool, !static_cast<bool>(T::value)>{};

#endif // TOML11_HAS_STD_CONJUNCTION

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------
// C++14 index_sequence

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_integer_sequence)
#    if __cpp_lib_integer_sequence >= 201304L
#      define TOML11_HAS_STD_INTEGER_SEQUENCE 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_INTEGER_SEQUENCE)

using std::index_sequence;
using std::make_index_sequence;

#else

template<std::size_t ... Ns> struct index_sequence{};

template<bool B, std::size_t N, typename T>
struct double_index_sequence;

template<std::size_t N, std::size_t ... Is>
struct double_index_sequence<true, N, index_sequence<Is...>>
{
    using type = index_sequence<Is..., (Is+N)..., N*2>;
};
template<std::size_t N, std::size_t ... Is>
struct double_index_sequence<false, N, index_sequence<Is...>>
{
    using type = index_sequence<Is..., (Is+N)...>;
};

template<std::size_t N>
struct index_sequence_maker
{
    using type = typename double_index_sequence<
            N % 2 == 1, N/2, typename index_sequence_maker<N/2>::type
        >::type;
};
template<>
struct index_sequence_maker<0>
{
    using type = index_sequence<>;
};

template<std::size_t N>
using make_index_sequence = typename index_sequence_maker<N>::type;

#endif // TOML11_HAS_STD_INTEGER_SEQUENCE

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------
// C++14 enable_if_t

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#  if defined(__cpp_lib_transformation_trait_aliases)
#    if __cpp_lib_transformation_trait_aliases >= 201304L
#      define TOML11_HAS_STD_ENABLE_IF_T 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_ENABLE_IF_T)

using std::enable_if_t;

#else

template<bool B, typename T>
using enable_if_t = typename std::enable_if<B, T>::type;

#endif // TOML11_HAS_STD_ENABLE_IF_T

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------
// return_type_of_t

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if defined(__cpp_lib_is_invocable)
#    if __cpp_lib_is_invocable >= 201703
#      define TOML11_HAS_STD_INVOKE_RESULT 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_INVOKE_RESULT)

template<typename F, typename ... Args>
using return_type_of_t = std::invoke_result_t<F, Args...>;

#else

// result_of is deprecated after C++17
template<typename F, typename ... Args>
using return_type_of_t = typename std::result_of<F(Args...)>::type;

#endif // TOML11_HAS_STD_INVOKE_RESULT

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ---------------------------------------------------------------------------
// C++17 void_t

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if defined(__cpp_lib_void_t)
#    if __cpp_lib_void_t >= 201411L
#      define TOML11_HAS_STD_VOID_T 1
#    endif
#  endif
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
#if defined(TOML11_HAS_STD_VOID_T)

using std::void_t;

#else

template<typename ...>
using void_t = void;

#endif // TOML11_HAS_STD_VOID_T

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

// ----------------------------------------------------------------------------
// (subset of) source_location

#if ! defined(TOML11_DISABLE_SOURCE_LOCATION) && TOML11_CPLUSPLUS_STANDARD_VERSION >= 202002L
#  if __has_include(<source_location>)
#    define TOML11_HAS_STD_SOURCE_LOCATION
#  endif // has_include
#endif // c++20

#if ! defined(TOML11_DISABLE_SOURCE_LOCATION) && ! defined(TOML11_HAS_STD_SOURCE_LOCATION)
#  if defined(__GNUC__) && ! defined(__clang__)
#    if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX14_VALUE
#      if __has_include(<experimental/source_location>)
#        define TOML11_HAS_EXPERIMENTAL_SOURCE_LOCATION
#      endif
#    endif
#  endif // GNU g++
#endif // not TOML11_HAS_STD_SOURCE_LOCATION

#if ! defined(TOML11_DISABLE_SOURCE_LOCATION) && ! defined(TOML11_HAS_STD_SOURCE_LOCATION) && ! defined(TOML11_HAS_EXPERIMENTAL_SOURCE_LOCATION)
#  if defined(__GNUC__) && ! defined(__clang__)
#    if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#      define TOML11_HAS_BUILTIN_FILE_LINE 1
#      define TOML11_BUILTIN_LINE_TYPE int
#    endif
#  elif defined(__clang__) // clang 9.0.0 implements builtin_FILE/LINE
#    if __has_builtin(__builtin_FILE) && __has_builtin(__builtin_LINE)
#      define TOML11_HAS_BUILTIN_FILE_LINE 1
#      define TOML11_BUILTIN_LINE_TYPE unsigned int
#    endif
#  elif defined(_MSVC_LANG) && defined(_MSC_VER)
#    if _MSC_VER > 1926
#      define TOML11_HAS_BUILTIN_FILE_LINE 1
#      define TOML11_BUILTIN_LINE_TYPE int
#    endif
#  endif
#endif

#if defined(TOML11_HAS_STD_SOURCE_LOCATION)
#include <source_location>
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
using source_location = std::source_location;

inline std::string to_string(const source_location& loc)
{
    const char* fname = loc.file_name();
    if(fname)
    {
        return std::string(" at line ") + std::to_string(loc.line()) +
               std::string(" in file ") + std::string(fname);
    }
    else
    {
        return std::string(" at line ") + std::to_string(loc.line()) +
               std::string(" in unknown file");
    }
}

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#elif defined(TOML11_HAS_EXPERIMENTAL_SOURCE_LOCATION)
#include <experimental/source_location>
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
using source_location = std::experimental::source_location;

inline std::string to_string(const source_location& loc)
{
    const char* fname = loc.file_name();
    if(fname)
    {
        return std::string(" at line ") + std::to_string(loc.line()) +
               std::string(" in file ") + std::string(fname);
    }
    else
    {
        return std::string(" at line ") + std::to_string(loc.line()) +
               std::string(" in unknown file");
    }
}

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#elif defined(TOML11_HAS_BUILTIN_FILE_LINE)
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
struct source_location
{
    using line_type = TOML11_BUILTIN_LINE_TYPE;
    static source_location current(const line_type line = __builtin_LINE(),
                                   const char*     file = __builtin_FILE())
    {
        return source_location(line, file);
    }

    source_location(const line_type line, const char* file)
        : line_(line), file_name_(file)
    {}

    line_type   line()      const noexcept {return line_;}
    const char* file_name() const noexcept {return file_name_;}

  private:

    line_type   line_;
    const char* file_name_;
};

inline std::string to_string(const source_location& loc)
{
    const char* fname = loc.file_name();
    if(fname)
    {
        return std::string(" at line ") + std::to_string(loc.line()) +
               std::string(" in file ") + std::string(fname);
    }
    else
    {
        return std::string(" at line ") + std::to_string(loc.line()) +
               std::string(" in unknown file");
    }
}

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#else // no builtin
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
struct source_location
{
    static source_location current() { return source_location{}; }
};

inline std::string to_string(const source_location&)
{
    return std::string("");
}
} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_HAS_STD_SOURCE_LOCATION

// ----------------------------------------------------------------------------
// (subset of) optional

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if __has_include(<optional>)
#    include <optional>
#  endif // has_include(optional)
#endif // C++17

#if TOML11_CPLUSPLUS_STANDARD_VERSION >= TOML11_CXX17_VALUE
#  if defined(__cpp_lib_optional)
#    if __cpp_lib_optional >= 201606L
#      define TOML11_HAS_STD_OPTIONAL 1
#    endif
#  endif
#endif

#if defined(TOML11_HAS_STD_OPTIONAL)

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{
using std::optional;

inline std::nullopt_t make_nullopt() {return std::nullopt;}

template<typename charT, typename traitsT>
std::basic_ostream<charT, traitsT>&
operator<<(std::basic_ostream<charT, traitsT>& os, const std::nullopt_t&)
{
    os << "nullopt";
    return os;
}

} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

#else // TOML11_HAS_STD_OPTIONAL

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace cxx
{

struct nullopt_t{};
inline nullopt_t make_nullopt() {return nullopt_t{};}

inline bool operator==(const nullopt_t&, const nullopt_t&) noexcept {return true;}
inline bool operator!=(const nullopt_t&, const nullopt_t&) noexcept {return false;}
inline bool operator< (const nullopt_t&, const nullopt_t&) noexcept {return false;}
inline bool operator<=(const nullopt_t&, const nullopt_t&) noexcept {return true;}
inline bool operator> (const nullopt_t&, const nullopt_t&) noexcept {return false;}
inline bool operator>=(const nullopt_t&, const nullopt_t&) noexcept {return true;}

template<typename charT, typename traitsT>
std::basic_ostream<charT, traitsT>&
operator<<(std::basic_ostream<charT, traitsT>& os, const nullopt_t&)
{
    os << "nullopt";
    return os;
}

template<typename T>
class optional
{
  public:

    using value_type = T;

  public:

    optional() noexcept : has_value_(false), null_('\0') {}
    optional(nullopt_t) noexcept : has_value_(false), null_('\0') {}

    optional(const T& x): has_value_(true), value_(x) {}
    optional(T&& x): has_value_(true), value_(std::move(x)) {}

    template<typename U, enable_if_t<std::is_constructible<T, U>::value, std::nullptr_t> = nullptr>
    explicit optional(U&& x): has_value_(true), value_(std::forward<U>(x)) {}

    optional(const optional& rhs): has_value_(rhs.has_value_)
    {
        if(rhs.has_value_)
        {
            this->assigner(rhs.value_);
        }
    }
    optional(optional&& rhs): has_value_(rhs.has_value_)
    {
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
    }

    optional& operator=(const optional& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(rhs.value_);
        }
        return *this;
    }
    optional& operator=(optional&& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
        return *this;
    }

    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    explicit optional(const optional<U>& rhs): has_value_(rhs.has_value_), null_('\0')
    {
        if(rhs.has_value_)
        {
            this->assigner(rhs.value_);
        }
    }
    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    explicit optional(optional<U>&& rhs): has_value_(rhs.has_value_), null_('\0')
    {
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
    }

    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    optional& operator=(const optional<U>& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(rhs.value_);
        }
        return *this;
    }

    template<typename U, enable_if_t<conjunction<
        negation<std::is_same<T, U>>, std::is_constructible<T, U>
        >::value, std::nullptr_t> = nullptr>
    optional& operator=(optional<U>&& rhs)
    {
        if(this == std::addressof(rhs)) {return *this;}

        this->cleanup();
        this->has_value_ = rhs.has_value_;
        if(this->has_value_)
        {
            this->assigner(std::move(rhs.value_));
        }
        return *this;
    }
    ~optional() noexcept
    {
        this->cleanup();
    }

    explicit operator bool() const noexcept
    {
        return has_value_;
    }

    bool has_value() const noexcept {return has_value_;}

    value_type const& value(source_location loc = source_location::current()) const
    {
        if( ! this->has_value_)
        {
            throw std::runtime_error("optional::value(): bad_unwrap" + to_string(loc));
        }
        return this->value_;
    }
    value_type& value(source_location loc = source_location::current())
    {
        if( ! this->has_value_)
        {
            throw std::runtime_error("optional::value(): bad_unwrap" + to_string(loc));
        }
        return this->value_;
    }

    value_type const& value_or(const value_type& opt) const
    {
        if(this->has_value_) {return this->value_;} else {return opt;}
    }
    value_type& value_or(value_type& opt)
    {
        if(this->has_value_) {return this->value_;} else {return opt;}
    }

  private:

    void cleanup() noexcept
    {
        if(this->has_value_)
        {
            value_.~T();
        }
    }

    template<typename U>
    void assigner(U&& x)
    {
        const auto tmp = ::new(std::addressof(this->value_)) value_type(std::forward<U>(x));
        assert(tmp == std::addressof(this->value_));
        (void)tmp;
    }

  private:

    bool has_value_;
    union
    {
        char null_;
        T    value_;
    };
};
} // cxx
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_HAS_STD_OPTIONAL

#endif // TOML11_COMPAT_HPP
#ifndef TOML11_VALUE_T_HPP
#define TOML11_VALUE_T_HPP

#ifndef TOML11_VALUE_T_FWD_HPP
#define TOML11_VALUE_T_FWD_HPP


#include <iosfwd>
#include <string>
#include <type_traits>

#include <cstdint>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// forward decl
template<typename TypeConfig>
class basic_value;

// ----------------------------------------------------------------------------
// enum representing toml types

enum class value_t : std::uint8_t
{
    empty           =  0,
    boolean         =  1,
    integer         =  2,
    floating        =  3,
    string          =  4,
    offset_datetime =  5,
    local_datetime  =  6,
    local_date      =  7,
    local_time      =  8,
    array           =  9,
    table           = 10
};

std::ostream& operator<<(std::ostream& os, value_t t);
std::string to_string(value_t t);


// ----------------------------------------------------------------------------
// meta functions for internal use

namespace detail
{

template<value_t V>
using value_t_constant = std::integral_constant<value_t, V>;

template<typename T, typename Value>
struct type_to_enum : value_t_constant<value_t::empty> {};

template<typename V> struct type_to_enum<typename V::boolean_type        , V> : value_t_constant<value_t::boolean        > {};
template<typename V> struct type_to_enum<typename V::integer_type        , V> : value_t_constant<value_t::integer        > {};
template<typename V> struct type_to_enum<typename V::floating_type       , V> : value_t_constant<value_t::floating       > {};
template<typename V> struct type_to_enum<typename V::string_type         , V> : value_t_constant<value_t::string         > {};
template<typename V> struct type_to_enum<typename V::offset_datetime_type, V> : value_t_constant<value_t::offset_datetime> {};
template<typename V> struct type_to_enum<typename V::local_datetime_type , V> : value_t_constant<value_t::local_datetime > {};
template<typename V> struct type_to_enum<typename V::local_date_type     , V> : value_t_constant<value_t::local_date     > {};
template<typename V> struct type_to_enum<typename V::local_time_type     , V> : value_t_constant<value_t::local_time     > {};
template<typename V> struct type_to_enum<typename V::array_type          , V> : value_t_constant<value_t::array          > {};
template<typename V> struct type_to_enum<typename V::table_type          , V> : value_t_constant<value_t::table          > {};

template<value_t V, typename Value>
struct enum_to_type { using type = void; };

template<typename V> struct enum_to_type<value_t::boolean        , V> { using type = typename V::boolean_type        ; };
template<typename V> struct enum_to_type<value_t::integer        , V> { using type = typename V::integer_type        ; };
template<typename V> struct enum_to_type<value_t::floating       , V> { using type = typename V::floating_type       ; };
template<typename V> struct enum_to_type<value_t::string         , V> { using type = typename V::string_type         ; };
template<typename V> struct enum_to_type<value_t::offset_datetime, V> { using type = typename V::offset_datetime_type; };
template<typename V> struct enum_to_type<value_t::local_datetime , V> { using type = typename V::local_datetime_type ; };
template<typename V> struct enum_to_type<value_t::local_date     , V> { using type = typename V::local_date_type     ; };
template<typename V> struct enum_to_type<value_t::local_time     , V> { using type = typename V::local_time_type     ; };
template<typename V> struct enum_to_type<value_t::array          , V> { using type = typename V::array_type          ; };
template<typename V> struct enum_to_type<value_t::table          , V> { using type = typename V::table_type          ; };

template<value_t V, typename Value>
using enum_to_type_t = typename enum_to_type<V, Value>::type;

template<value_t V>
struct enum_to_fmt_type { using type = void; };

template<> struct enum_to_fmt_type<value_t::boolean        > { using type = boolean_format_info        ; };
template<> struct enum_to_fmt_type<value_t::integer        > { using type = integer_format_info        ; };
template<> struct enum_to_fmt_type<value_t::floating       > { using type = floating_format_info       ; };
template<> struct enum_to_fmt_type<value_t::string         > { using type = string_format_info         ; };
template<> struct enum_to_fmt_type<value_t::offset_datetime> { using type = offset_datetime_format_info; };
template<> struct enum_to_fmt_type<value_t::local_datetime > { using type = local_datetime_format_info ; };
template<> struct enum_to_fmt_type<value_t::local_date     > { using type = local_date_format_info     ; };
template<> struct enum_to_fmt_type<value_t::local_time     > { using type = local_time_format_info     ; };
template<> struct enum_to_fmt_type<value_t::array          > { using type = array_format_info          ; };
template<> struct enum_to_fmt_type<value_t::table          > { using type = table_format_info          ; };

template<value_t V>
using enum_to_fmt_type_t = typename enum_to_fmt_type<V>::type;

template<typename T, typename Value>
struct is_exact_toml_type0 : cxx::disjunction<
    std::is_same<T, typename Value::boolean_type        >,
    std::is_same<T, typename Value::integer_type        >,
    std::is_same<T, typename Value::floating_type       >,
    std::is_same<T, typename Value::string_type         >,
    std::is_same<T, typename Value::offset_datetime_type>,
    std::is_same<T, typename Value::local_datetime_type >,
    std::is_same<T, typename Value::local_date_type     >,
    std::is_same<T, typename Value::local_time_type     >,
    std::is_same<T, typename Value::array_type          >,
    std::is_same<T, typename Value::table_type          >
    >{};
template<typename T, typename V> struct is_exact_toml_type: is_exact_toml_type0<cxx::remove_cvref_t<T>, V> {};
template<typename T, typename V> struct is_not_toml_type : cxx::negation<is_exact_toml_type<T, V>> {};

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_VALUE_T_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_VALUE_T_IMPL_HPP
#define TOML11_VALUE_T_IMPL_HPP


#include <ostream>
#include <sstream>
#include <string>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

TOML11_INLINE std::ostream& operator<<(std::ostream& os, value_t t)
{
    switch(t)
    {
        case value_t::boolean         : os << "boolean";         return os;
        case value_t::integer         : os << "integer";         return os;
        case value_t::floating        : os << "floating";        return os;
        case value_t::string          : os << "string";          return os;
        case value_t::offset_datetime : os << "offset_datetime"; return os;
        case value_t::local_datetime  : os << "local_datetime";  return os;
        case value_t::local_date      : os << "local_date";      return os;
        case value_t::local_time      : os << "local_time";      return os;
        case value_t::array           : os << "array";           return os;
        case value_t::table           : os << "table";           return os;
        case value_t::empty           : os << "empty";           return os;
        default                       : os << "unknown";         return os;
    }
}

TOML11_INLINE std::string to_string(value_t t)
{
    std::ostringstream oss;
    oss << t;
    return oss.str();
}

} //TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_VALUE_T_IMPL_HPP
#endif

#endif // TOML11_VALUE_T_HPP
#ifndef TOML11_TRAITS_HPP
#define TOML11_TRAITS_HPP


#include <array>
#include <chrono>
#include <forward_list>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>

#if defined(TOML11_HAS_STRING_VIEW)
#include <string_view>
#endif

#if defined(TOML11_HAS_OPTIONAL)
#include <optional>
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
template<typename TypeConcig>
class basic_value;

namespace detail
{
// ---------------------------------------------------------------------------
// check whether type T is a kind of container/map class

struct has_iterator_impl
{
    template<typename T> static std::true_type  check(typename T::iterator*);
    template<typename T> static std::false_type check(...);
};
struct has_value_type_impl
{
    template<typename T> static std::true_type  check(typename T::value_type*);
    template<typename T> static std::false_type check(...);
};
struct has_key_type_impl
{
    template<typename T> static std::true_type  check(typename T::key_type*);
    template<typename T> static std::false_type check(...);
};
struct has_mapped_type_impl
{
    template<typename T> static std::true_type  check(typename T::mapped_type*);
    template<typename T> static std::false_type check(...);
};
struct has_reserve_method_impl
{
    template<typename T> static std::false_type check(...);
    template<typename T> static std::true_type  check(
        decltype(std::declval<T>().reserve(std::declval<std::size_t>()))*);
};
struct has_push_back_method_impl
{
    template<typename T> static std::false_type check(...);
    template<typename T> static std::true_type  check(
        decltype(std::declval<T>().push_back(std::declval<typename T::value_type>()))*);
};
struct is_comparable_impl
{
    template<typename T> static std::false_type check(...);
    template<typename T> static std::true_type  check(
        decltype(std::declval<T>() < std::declval<T>())*);
};

struct has_from_toml_method_impl
{
    template<typename T, typename TC>
    static std::true_type  check(
        decltype(std::declval<T>().from_toml(std::declval<::toml::basic_value<TC>>()))*);

    template<typename T, typename TC>
    static std::false_type check(...);
};
struct has_into_toml_method_impl
{
    template<typename T>
    static std::true_type  check(decltype(std::declval<T>().into_toml())*);
    template<typename T>
    static std::false_type check(...);
};

struct has_template_into_toml_method_impl
{
    template<typename T, typename TypeConfig>
    static std::true_type  check(decltype(std::declval<T>().template into_toml<TypeConfig>())*);
    template<typename T, typename TypeConfig>
    static std::false_type check(...);
};

struct has_specialized_from_impl
{
    template<typename T>
    static std::false_type check(...);
    template<typename T, std::size_t S = sizeof(::toml::from<T>)>
    static std::true_type check(::toml::from<T>*);
};
struct has_specialized_into_impl
{
    template<typename T>
    static std::false_type check(...);
    template<typename T, std::size_t S = sizeof(::toml::into<T>)>
    static std::true_type check(::toml::into<T>*);
};


/// Intel C++ compiler can not use decltype in parent class declaration, here
/// is a hack to work around it. https://stackoverflow.com/a/23953090/4692076
#ifdef __INTEL_COMPILER
#define decltype(...) std::enable_if<true, decltype(__VA_ARGS__)>::type
#endif

template<typename T>
struct has_iterator: decltype(has_iterator_impl::check<T>(nullptr)){};
template<typename T>
struct has_value_type: decltype(has_value_type_impl::check<T>(nullptr)){};
template<typename T>
struct has_key_type: decltype(has_key_type_impl::check<T>(nullptr)){};
template<typename T>
struct has_mapped_type: decltype(has_mapped_type_impl::check<T>(nullptr)){};
template<typename T>
struct has_reserve_method: decltype(has_reserve_method_impl::check<T>(nullptr)){};
template<typename T>
struct has_push_back_method: decltype(has_push_back_method_impl::check<T>(nullptr)){};
template<typename T>
struct is_comparable: decltype(is_comparable_impl::check<T>(nullptr)){};

template<typename T, typename TC>
struct has_from_toml_method: decltype(has_from_toml_method_impl::check<T, TC>(nullptr)){};

template<typename T>
struct has_into_toml_method: decltype(has_into_toml_method_impl::check<T>(nullptr)){};

template<typename T, typename TypeConfig>
struct has_template_into_toml_method: decltype(has_template_into_toml_method_impl::check<T, TypeConfig>(nullptr)){};

template<typename T>
struct has_specialized_from: decltype(has_specialized_from_impl::check<T>(nullptr)){};
template<typename T>
struct has_specialized_into: decltype(has_specialized_into_impl::check<T>(nullptr)){};

#ifdef __INTEL_COMPILER
#undef decltype
#endif

// ---------------------------------------------------------------------------
// type checkers

template<typename T> struct is_std_pair_impl : std::false_type{};
template<typename T1, typename T2>
struct is_std_pair_impl<std::pair<T1, T2>> : std::true_type{};
template<typename T>
using is_std_pair = is_std_pair_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_std_tuple_impl : std::false_type{};
template<typename ... Ts>
struct is_std_tuple_impl<std::tuple<Ts...>> : std::true_type{};
template<typename T>
using is_std_tuple = is_std_tuple_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_unordered_set_impl : std::false_type {};
template<typename T>
struct is_unordered_set_impl<std::unordered_set<T>> : std::true_type {};
template<typename T>
using is_unordered_set = is_unordered_set_impl<cxx::remove_cvref_t<T>>;

#if defined(TOML11_HAS_OPTIONAL)
template<typename T> struct is_std_optional_impl : std::false_type{};
template<typename T>
struct is_std_optional_impl<std::optional<T>> : std::true_type{};
template<typename T>
using is_std_optional = is_std_optional_impl<cxx::remove_cvref_t<T>>;
#else
template<typename T> struct is_std_optional : std::false_type{};
#endif // > C++17

template<typename T> struct is_std_array_impl : std::false_type{};
template<typename T, std::size_t N>
struct is_std_array_impl<std::array<T, N>> : std::true_type{};
template<typename T>
using is_std_array = is_std_array_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_std_forward_list_impl : std::false_type{};
template<typename T>
struct is_std_forward_list_impl<std::forward_list<T>> : std::true_type{};
template<typename T>
using is_std_forward_list = is_std_forward_list_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_std_basic_string_impl : std::false_type{};
template<typename C, typename T, typename A>
struct is_std_basic_string_impl<std::basic_string<C, T, A>> : std::true_type{};
template<typename T>
using is_std_basic_string = is_std_basic_string_impl<cxx::remove_cvref_t<T>>;

template<typename T> struct is_1byte_std_basic_string_impl : std::false_type{};
template<typename C, typename T, typename A>
struct is_1byte_std_basic_string_impl<std::basic_string<C, T, A>>
    : std::integral_constant<bool, sizeof(C) == sizeof(char)> {};
template<typename T>
using is_1byte_std_basic_string = is_std_basic_string_impl<cxx::remove_cvref_t<T>>;

#if defined(TOML11_HAS_STRING_VIEW)
template<typename T> struct is_std_basic_string_view_impl : std::false_type{};
template<typename C, typename T>
struct is_std_basic_string_view_impl<std::basic_string_view<C, T>> : std::true_type{};
template<typename T>
using is_std_basic_string_view = is_std_basic_string_view_impl<cxx::remove_cvref_t<T>>;

template<typename V, typename S>
struct is_string_view_of : std::false_type {};
template<typename C, typename T>
struct is_string_view_of<std::basic_string_view<C, T>, std::basic_string<C, T>> : std::true_type {};
#else
template<typename T>
struct is_std_basic_string_view : std::false_type {};
template<typename V, typename S>
struct is_string_view_of : std::false_type {};
#endif

template<typename T> struct is_chrono_duration_impl: std::false_type{};
template<typename Rep, typename Period>
struct is_chrono_duration_impl<std::chrono::duration<Rep, Period>>: std::true_type{};
template<typename T>
using is_chrono_duration = is_chrono_duration_impl<cxx::remove_cvref_t<T>>;

template<typename T>
struct is_map_impl : cxx::conjunction< // map satisfies all the following conditions
    has_iterator<T>,         // has T::iterator
    has_value_type<T>,       // has T::value_type
    has_key_type<T>,         // has T::key_type
    has_mapped_type<T>       // has T::mapped_type
    >{};
template<typename T>
using is_map = is_map_impl<cxx::remove_cvref_t<T>>;

template<typename T>
struct is_container_impl : cxx::conjunction<
    cxx::negation<is_map<T>>,                         // not a map
    cxx::negation<std::is_same<T, std::string>>,      // not a std::string
#ifdef TOML11_HAS_STRING_VIEW
    cxx::negation<std::is_same<T, std::string_view>>, // not a std::string_view
#endif
    has_iterator<T>,                             // has T::iterator
    has_value_type<T>                            // has T::value_type
    >{};
template<typename T>
using is_container = is_container_impl<cxx::remove_cvref_t<T>>;

template<typename T>
struct is_basic_value_impl: std::false_type{};
template<typename TC>
struct is_basic_value_impl<::toml::basic_value<TC>>: std::true_type{};
template<typename T>
using is_basic_value = is_basic_value_impl<cxx::remove_cvref_t<T>>;

}// detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_TRAITS_HPP
#ifndef TOML11_STORAGE_HPP
#define TOML11_STORAGE_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

// It owns a pointer to T. It does deep-copy when copied.
// This struct is introduced to implement a recursive type.
//
// `toml::value` contains `std::vector<toml::value>` to represent a toml array.
// But, in the definition of `toml::value`, `toml::value` is still incomplete.
// `std::vector` of an incomplete type is not allowed in C++11 (it is allowed
// after C++17). To avoid this, we need to use a pointer to `toml::value`, like
// `std::vector<std::unique_ptr<toml::value>>`. Although `std::unique_ptr` is
// noncopyable, we want to make `toml::value` copyable. `storage` is introduced
// to resolve those problems.
template<typename T>
struct storage
{
    using value_type = T;

    explicit storage(value_type v): ptr_(cxx::make_unique<T>(std::move(v))) {}
    ~storage() = default;

    storage(const storage& rhs): ptr_(cxx::make_unique<T>(*rhs.ptr_)) {}
    storage& operator=(const storage& rhs)
    {
        this->ptr_ = cxx::make_unique<T>(*rhs.ptr_);
        return *this;
    }

    storage(storage&&) = default;
    storage& operator=(storage&&) = default;

    bool is_ok() const noexcept {return static_cast<bool>(ptr_);}

    value_type& get() const noexcept {return *ptr_;}

  private:
    std::unique_ptr<value_type> ptr_;
};

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_STORAGE_HPP
#ifndef TOML11_RESULT_HPP
#define TOML11_RESULT_HPP


#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include <cassert>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

struct bad_result_access final : public ::toml::exception
{
  public:
    explicit bad_result_access(std::string what_arg)
        : what_(std::move(what_arg))
    {}
    ~bad_result_access() noexcept override = default;
    const char* what() const noexcept override {return what_.c_str();}

  private:
    std::string what_;
};

// -----------------------------------------------------------------------------

template<typename T>
struct success
{
    static_assert( ! std::is_void<T>::value, "");

    using value_type = T;

    explicit success(value_type v)
        noexcept(std::is_nothrow_move_constructible<value_type>::value)
        : value(std::move(v))
    {}

    template<typename U, cxx::enable_if_t<
        std::is_convertible<cxx::remove_cvref_t<U>, T>::value,
        std::nullptr_t> = nullptr>
    explicit success(U&& v): value(std::forward<U>(v)) {}

    template<typename U>
    explicit success(success<U> v): value(std::move(v.value)) {}

    ~success() = default;
    success(const success&) = default;
    success(success&&)      = default;
    success& operator=(const success&) = default;
    success& operator=(success&&)      = default;

    value_type&       get()       noexcept {return value;}
    value_type const& get() const noexcept {return value;}

  private:

    value_type value;
};

template<typename T>
struct success<std::reference_wrapper<T>>
{
    static_assert( ! std::is_void<T>::value, "");

    using value_type = T;

    explicit success(std::reference_wrapper<value_type> v) noexcept
        : value(std::move(v))
    {}

    ~success() = default;
    success(const success&) = default;
    success(success&&)      = default;
    success& operator=(const success&) = default;
    success& operator=(success&&)      = default;

    value_type&       get()       noexcept {return value.get();}
    value_type const& get() const noexcept {return value.get();}

  private:

    std::reference_wrapper<value_type> value;
};

template<typename T>
success<typename std::decay<T>::type> ok(T&& v)
{
    return success<typename std::decay<T>::type>(std::forward<T>(v));
}
template<std::size_t N>
success<std::string> ok(const char (&literal)[N])
{
    return success<std::string>(std::string(literal));
}

// -----------------------------------------------------------------------------

template<typename T>
struct failure
{
    using value_type = T;

    explicit failure(value_type v)
        noexcept(std::is_nothrow_move_constructible<value_type>::value)
        : value(std::move(v))
    {}

    template<typename U, cxx::enable_if_t<
        std::is_convertible<cxx::remove_cvref_t<U>, T>::value,
        std::nullptr_t> = nullptr>
    explicit failure(U&& v): value(std::forward<U>(v)) {}

    template<typename U>
    explicit failure(failure<U> v): value(std::move(v.value)) {}

    ~failure() = default;
    failure(const failure&) = default;
    failure(failure&&)      = default;
    failure& operator=(const failure&) = default;
    failure& operator=(failure&&)      = default;

    value_type&       get()       noexcept {return value;}
    value_type const& get() const noexcept {return value;}

  private:

    value_type value;
};

template<typename T>
struct failure<std::reference_wrapper<T>>
{
    using value_type = T;

    explicit failure(std::reference_wrapper<value_type> v) noexcept
        : value(std::move(v))
    {}

    ~failure() = default;
    failure(const failure&) = default;
    failure(failure&&)      = default;
    failure& operator=(const failure&) = default;
    failure& operator=(failure&&)      = default;

    value_type&       get()       noexcept {return value.get();}
    value_type const& get() const noexcept {return value.get();}

  private:

    std::reference_wrapper<value_type> value;
};

template<typename T>
failure<typename std::decay<T>::type> err(T&& v)
{
    return failure<typename std::decay<T>::type>(std::forward<T>(v));
}

template<std::size_t N>
failure<std::string> err(const char (&literal)[N])
{
    return failure<std::string>(std::string(literal));
}

/* ============================================================================
 *                  _ _
 *  _ _ ___ ____  _| | |_
 * | '_/ -_|_-< || | |  _|
 * |_| \___/__/\_,_|_|\__|
 */

template<typename T, typename E>
struct result
{
    using success_type = success<T>;
    using failure_type = failure<E>;
    using value_type = typename success_type::value_type;
    using error_type = typename failure_type::value_type;

    result(success_type s): is_ok_(true),  succ_(std::move(s)) {}
    result(failure_type f): is_ok_(false), fail_(std::move(f)) {}

    template<typename U, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, value_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, value_type>
        >::value, std::nullptr_t> = nullptr>
    result(success<U> s): is_ok_(true),  succ_(std::move(s.value)) {}

    template<typename U, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, error_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, error_type>
        >::value, std::nullptr_t> = nullptr>
    result(failure<U> f): is_ok_(false), fail_(std::move(f.value)) {}

    result& operator=(success_type s)
    {
        this->cleanup();
        this->is_ok_ = true;
        auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(s));
        assert(tmp == std::addressof(this->succ_));
        (void)tmp;
        return *this;
    }
    result& operator=(failure_type f)
    {
        this->cleanup();
        this->is_ok_ = false;
        auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(f));
        assert(tmp == std::addressof(this->fail_));
        (void)tmp;
        return *this;
    }

    template<typename U>
    result& operator=(success<U> s)
    {
        this->cleanup();
        this->is_ok_ = true;
        auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(s.value));
        assert(tmp == std::addressof(this->succ_));
        (void)tmp;
        return *this;
    }
    template<typename U>
    result& operator=(failure<U> f)
    {
        this->cleanup();
        this->is_ok_ = false;
        auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(f.value));
        assert(tmp == std::addressof(this->fail_));
        (void)tmp;
        return *this;
    }

    ~result() noexcept {this->cleanup();}

    result(const result& other): is_ok_(other.is_ok())
    {
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(other.succ_);
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(other.fail_);
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
    }
    result(result&& other): is_ok_(other.is_ok())
    {
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.succ_));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.fail_));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
    }

    result& operator=(const result& other)
    {
        this->cleanup();
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(other.succ_);
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(other.fail_);
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
        is_ok_ = other.is_ok();
        return *this;
    }
    result& operator=(result&& other)
    {
        this->cleanup();
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.succ_));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.fail_));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
        is_ok_ = other.is_ok();
        return *this;
    }

    template<typename U, typename F, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, value_type>>,
            cxx::negation<std::is_same<cxx::remove_cvref_t<F>, error_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, value_type>,
            std::is_convertible<cxx::remove_cvref_t<F>, error_type>
        >::value, std::nullptr_t> = nullptr>
    result(result<U, F> other): is_ok_(other.is_ok())
    {
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.as_ok()));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.as_err()));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
    }

    template<typename U, typename F, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<U>, value_type>>,
            cxx::negation<std::is_same<cxx::remove_cvref_t<F>, error_type>>,
            std::is_convertible<cxx::remove_cvref_t<U>, value_type>,
            std::is_convertible<cxx::remove_cvref_t<F>, error_type>
        >::value, std::nullptr_t> = nullptr>
    result& operator=(result<U, F> other)
    {
        this->cleanup();
        if(other.is_ok())
        {
            auto tmp = ::new(std::addressof(this->succ_)) success_type(std::move(other.as_ok()));
            assert(tmp == std::addressof(this->succ_));
            (void)tmp;
        }
        else
        {
            auto tmp = ::new(std::addressof(this->fail_)) failure_type(std::move(other.as_err()));
            assert(tmp == std::addressof(this->fail_));
            (void)tmp;
        }
        is_ok_ = other.is_ok();
        return *this;
    }

    bool is_ok()  const noexcept {return is_ok_;}
    bool is_err() const noexcept {return !is_ok_;}

    explicit operator bool() const noexcept {return is_ok_;}

    value_type& unwrap(cxx::source_location loc = cxx::source_location::current())
    {
        if(this->is_err())
        {
            throw bad_result_access("toml::result: bad unwrap" + cxx::to_string(loc));
        }
        return this->succ_.get();
    }
    value_type const& unwrap(cxx::source_location loc = cxx::source_location::current()) const
    {
        if(this->is_err())
        {
            throw bad_result_access("toml::result: bad unwrap" + cxx::to_string(loc));
        }
        return this->succ_.get();
    }

    value_type& unwrap_or(value_type& opt) noexcept
    {
        if(this->is_err()) {return opt;}
        return this->succ_.get();
    }
    value_type const& unwrap_or(value_type const& opt) const noexcept
    {
        if(this->is_err()) {return opt;}
        return this->succ_.get();
    }

    error_type& unwrap_err(cxx::source_location loc = cxx::source_location::current())
    {
        if(this->is_ok())
        {
            throw bad_result_access("toml::result: bad unwrap_err" + cxx::to_string(loc));
        }
        return this->fail_.get();
    }
    error_type const& unwrap_err(cxx::source_location loc = cxx::source_location::current()) const
    {
        if(this->is_ok())
        {
            throw bad_result_access("toml::result: bad unwrap_err" + cxx::to_string(loc));
        }
        return this->fail_.get();
    }

    value_type& as_ok() noexcept
    {
        assert(this->is_ok());
        return this->succ_.get();
    }
    value_type const& as_ok() const noexcept
    {
        assert(this->is_ok());
        return this->succ_.get();
    }

    error_type& as_err() noexcept
    {
        assert(this->is_err());
        return this->fail_.get();
    }
    error_type const& as_err() const noexcept
    {
        assert(this->is_err());
        return this->fail_.get();
    }

  private:

    void cleanup() noexcept
    {
#if defined(__GNUC__) && ! defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#endif

        if(this->is_ok_) {this->succ_.~success_type();}
        else             {this->fail_.~failure_type();}

#if defined(__GNUC__) && ! defined(__clang__)
#pragma GCC diagnostic pop
#endif
        return;
    }

  private:

    bool      is_ok_;
    union
    {
        success_type succ_;
        failure_type fail_;
    };
};

// ----------------------------------------------------------------------------

namespace detail
{
struct none_t {};
inline bool operator==(const none_t&, const none_t&) noexcept {return true;}
inline bool operator!=(const none_t&, const none_t&) noexcept {return false;}
inline bool operator< (const none_t&, const none_t&) noexcept {return false;}
inline bool operator<=(const none_t&, const none_t&) noexcept {return true;}
inline bool operator> (const none_t&, const none_t&) noexcept {return false;}
inline bool operator>=(const none_t&, const none_t&) noexcept {return true;}
inline std::ostream& operator<<(std::ostream& os, const none_t&)
{
    os << "none";
    return os;
}
} // detail

inline success<detail::none_t> ok() noexcept
{
    return success<detail::none_t>(detail::none_t{});
}
inline failure<detail::none_t> err() noexcept
{
    return failure<detail::none_t>(detail::none_t{});
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_RESULT_HPP
#ifndef TOML11_UTILITY_HPP
#define TOML11_UTILITY_HPP


#include <array>
#include <sstream>

#include <cassert>
#include <cctype>
#include <cstring>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

// to output character in an error message.
inline std::string show_char(const int c)
{
    using char_type = unsigned char;
    if(std::isgraph(c))
    {
        return std::string(1, static_cast<char>(c));
    }
    else
    {
        std::array<char, 5> buf;
        buf.fill('\0');
        const auto r = std::snprintf(buf.data(), buf.size(), "0x%02x", static_cast<unsigned int>(c & 0xFF));
        assert(r == static_cast<int>(buf.size()) - 1);
        (void) r; // Unused variable warning
        auto in_hex = std::string(buf.data());
        switch(c)
        {
            case char_type('\0'):   {in_hex += "(NUL)";             break;}
            case char_type(' ') :   {in_hex += "(SPACE)";           break;}
            case char_type('\n'):   {in_hex += "(LINE FEED)";       break;}
            case char_type('\r'):   {in_hex += "(CARRIAGE RETURN)"; break;}
            case char_type('\t'):   {in_hex += "(TAB)";             break;}
            case char_type('\v'):   {in_hex += "(VERTICAL TAB)";    break;}
            case char_type('\f'):   {in_hex += "(FORM FEED)";       break;}
            case char_type('\x1B'): {in_hex += "(ESCAPE)";          break;}
            default: break;
        }
        return in_hex;
    }
}

// ---------------------------------------------------------------------------

template<typename Container>
void try_reserve_impl(Container& container, std::size_t N, std::true_type)
{
    container.reserve(N);
    return;
}
template<typename Container>
void try_reserve_impl(Container&, std::size_t, std::false_type) noexcept
{
    return;
}

template<typename Container>
void try_reserve(Container& container, std::size_t N)
{
    try_reserve_impl(container, N, has_reserve_method<Container>{});
    return;
}

// ---------------------------------------------------------------------------

template<typename T>
result<T, none_t> from_string(const std::string& str)
{
    T v;
    std::istringstream iss(str);
    iss >> v;
    if(iss.fail())
    {
        return err();
    }
    return ok(v);
}

// ---------------------------------------------------------------------------

// helper function to avoid std::string(0, 'c') or std::string(iter, iter)
template<typename Iterator>
std::string make_string(Iterator first, Iterator last)
{
    if(first == last) {return "";}
    return std::string(first, last);
}
inline std::string make_string(std::size_t len, char c)
{
    if(len == 0) {return "";}
    return std::string(len, c);
}

// ---------------------------------------------------------------------------

template<typename Char,  typename Traits, typename Alloc,
         typename Char2, typename Traits2, typename Alloc2>
struct string_conv_impl
{
    static_assert(sizeof(Char)  == sizeof(char), "");
    static_assert(sizeof(Char2) == sizeof(char), "");

    static std::basic_string<Char, Traits, Alloc> invoke(std::basic_string<Char2, Traits2, Alloc2> s)
    {
        std::basic_string<Char, Traits, Alloc> retval;
        std::transform(s.begin(), s.end(), std::back_inserter(retval),
            [](const Char2 c) {return static_cast<Char>(c);});
        return retval;
    }
    template<std::size_t N>
    static std::basic_string<Char, Traits, Alloc> invoke(const Char2 (&s)[N])
    {
        std::basic_string<Char, Traits, Alloc> retval;
        // "string literal" has null-char at the end. to skip it, we use prev.
        std::transform(std::begin(s), std::prev(std::end(s)), std::back_inserter(retval),
            [](const Char2 c) {return static_cast<Char>(c);});
        return retval;
    }
};

template<typename Char,  typename Traits, typename Alloc>
struct string_conv_impl<Char, Traits, Alloc, Char, Traits, Alloc>
{
    static_assert(sizeof(Char) == sizeof(char), "");

    static std::basic_string<Char, Traits, Alloc> invoke(std::basic_string<Char, Traits, Alloc> s)
    {
        return s;
    }
    template<std::size_t N>
    static std::basic_string<Char, Traits, Alloc> invoke(const Char (&s)[N])
    {
        return std::basic_string<Char, Traits, Alloc>(s);
    }
};

template<typename S, typename Char2, typename Traits2, typename Alloc2>
cxx::enable_if_t<is_std_basic_string<S>::value, S>
string_conv(std::basic_string<Char2, Traits2, Alloc2> s)
{
    using C = typename S::value_type;
    using T = typename S::traits_type;
    using A = typename S::allocator_type;
    return string_conv_impl<C, T, A, Char2, Traits2, Alloc2>::invoke(std::move(s));
}
template<typename S, std::size_t N>
cxx::enable_if_t<is_std_basic_string<S>::value, S>
string_conv(const char (&s)[N])
{
    using C = typename S::value_type;
    using T = typename S::traits_type;
    using A = typename S::allocator_type;
    using C2 = char;
    using T2 = std::char_traits<C2>;
    using A2 = std::allocator<C2>;

    return string_conv_impl<C, T, A, C2, T2, A2>::template invoke<N>(s);
}

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_UTILITY_HPP
#ifndef TOML11_LOCATION_HPP
#define TOML11_LOCATION_HPP

#ifndef TOML11_LOCATION_FWD_HPP
#define TOML11_LOCATION_FWD_HPP


#include <memory>
#include <string>
#include <vector>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

class region; // fwd decl

//
// To represent where we are reading in the parse functions.
// Since it "points" somewhere in the input stream, the length is always 1.
//
class location
{
  public:

    using char_type       = unsigned char; // must be unsigned
    using container_type  = std::vector<char_type>;
    using difference_type = typename container_type::difference_type; // to suppress sign-conversion warning
    using source_ptr      = std::shared_ptr<const container_type>;

  public:

    location(source_ptr src, std::string src_name)
        : source_(std::move(src)), source_name_(std::move(src_name)),
          location_(0), line_number_(1), column_number_(1)
    {}

    location(const location&) = default;
    location(location&&)      = default;
    location& operator=(const location&) = default;
    location& operator=(location&&)      = default;
    ~location() = default;

    void advance(std::size_t n = 1) noexcept;
    void retrace() noexcept;

    bool is_ok() const noexcept { return static_cast<bool>(this->source_); }

    bool eof() const noexcept;
    char_type current() const;

    char_type peek();

    std::size_t get_location() const noexcept
    {
        return this->location_;
    }

    std::size_t line_number() const noexcept
    {
        return this->line_number_;
    }
    std::size_t column_number() const noexcept
    {
        return this->column_number_;
    }
    std::string get_line() const;

    source_ptr const&  source()      const noexcept {return this->source_;}
    std::string const& source_name() const noexcept {return this->source_name_;}

  private:

    void advance_impl(const std::size_t n);
    void retrace_impl();
    std::size_t calc_column_number() const noexcept;

  private:

    friend region;

  private:

    source_ptr  source_;
    std::string source_name_;
    std::size_t location_; // std::vector<>::difference_type is signed
    std::size_t line_number_;
    std::size_t column_number_;
};

bool operator==(const location& lhs, const location& rhs) noexcept;
bool operator!=(const location& lhs, const location& rhs);

location prev(const location& loc);
location next(const location& loc);
location make_temporary_location(const std::string& str) noexcept;

template<typename F>
result<location, none_t>
find_if(const location& first, const location& last, const F& func) noexcept
{
    if(first.source() != last.source())             { return err(); }
    if(first.get_location() >= last.get_location()) { return err(); }

    auto loc = first;
    while(loc.get_location() != last.get_location())
    {
        if(func(loc.current()))
        {
            return ok(loc);
        }
        loc.advance();
    }
    return err();
}

template<typename F>
result<location, none_t>
rfind_if(location first, const location& last, const F& func)
{
    if(first.source() != last.source())             { return err(); }
    if(first.get_location() >= last.get_location()) { return err(); }

    auto loc = last;
    while(loc.get_location() != first.get_location())
    {
        if(func(loc.current()))
        {
            return ok(loc);
        }
        loc.retrace();
    }
    if(func(first.current()))
    {
        return ok(first);
    }
    return err();
}

result<location, none_t> find(const location& first, const location& last,
                              const location::char_type val);
result<location, none_t> rfind(const location& first, const location& last,
                               const location::char_type val);

std::size_t count(const location& first, const location& last,
                  const location::char_type& c);

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_LOCATION_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_LOCATION_IMPL_HPP
#define TOML11_LOCATION_IMPL_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

TOML11_INLINE void location::advance(std::size_t n) noexcept
{
    assert(this->is_ok());
    if(this->location_ + n < this->source_->size())
    {
        this->advance_impl(n);
    }
    else
    {
        this->advance_impl(this->source_->size() - this->location_);

        assert(this->location_ == this->source_->size());
    }
}
TOML11_INLINE void location::retrace(/*restricted to n=1*/) noexcept
{
    assert(this->is_ok());
    if(this->location_ == 0)
    {
        this->location_ = 0;
        this->line_number_ = 1;
        this->column_number_ = 1;
    }
    else
    {
        this->retrace_impl();
    }
}

TOML11_INLINE bool location::eof() const noexcept
{
    assert(this->is_ok());
    return this->location_ >= this->source_->size();
}
TOML11_INLINE location::char_type location::current() const
{
    assert(this->is_ok());
    if(this->eof()) {return '\0';}

    assert(this->location_ < this->source_->size());
    return this->source_->at(this->location_);
}

TOML11_INLINE location::char_type location::peek()
{
    assert(this->is_ok());
    if(this->location_ >= this->source_->size())
    {
        return '\0';
    }
    else
    {
        return this->source_->at(this->location_ + 1);
    }
}

TOML11_INLINE std::string location::get_line() const
{
    assert(this->is_ok());
    const auto iter = std::next(this->source_->cbegin(), static_cast<difference_type>(this->location_));
    const auto riter = cxx::make_reverse_iterator(iter);

    const auto prev = std::find(riter, this->source_->crend(), char_type('\n'));
    const auto next = std::find(iter,  this->source_->cend(),  char_type('\n'));

    return make_string(std::next(prev.base()), next);
}

TOML11_INLINE std::size_t location::calc_column_number() const noexcept
{
    assert(this->is_ok());
    const auto iter  = std::next(this->source_->cbegin(), static_cast<difference_type>(this->location_));
    const auto riter = cxx::make_reverse_iterator(iter);
    const auto prev  = std::find(riter, this->source_->crend(), char_type('\n'));

    assert(prev.base() <= iter);
    return static_cast<std::size_t>(std::distance(prev.base(), iter) + 1); // 1-origin
}

TOML11_INLINE void location::advance_impl(const std::size_t n)
{
    assert(this->is_ok());
    assert(this->location_ + n <= this->source_->size());

    auto iter = this->source_->cbegin();
    std::advance(iter, static_cast<difference_type>(this->location_));

    for(std::size_t i=0; i<n; ++i)
    {
        const auto c = *iter;
        if(c == char_type('\n'))
        {
            this->line_number_  += 1;
            this->column_number_ = 1;
        }
        else
        {
            this->column_number_ += 1;
        }
        iter++;
    }
    this->location_ += n;
    return;
}
TOML11_INLINE void location::retrace_impl(/*n == 1*/)
{
    assert(this->is_ok());
    assert(this->location_ != 0);

    this->location_ -= 1;

    auto iter = this->source_->cbegin();
    std::advance(iter, static_cast<difference_type>(this->location_));
    if(*iter == '\n')
    {
        this->line_number_ -= 1;
        this->column_number_ = this->calc_column_number();
    }
    return;
}

TOML11_INLINE bool operator==(const location& lhs, const location& rhs) noexcept
{
    if( ! lhs.is_ok() || ! rhs.is_ok())
    {
        return (!lhs.is_ok()) && (!rhs.is_ok());
    }
    return lhs.source()       == rhs.source()      &&
           lhs.source_name()  == rhs.source_name() &&
           lhs.get_location() == rhs.get_location();
}
TOML11_INLINE bool operator!=(const location& lhs, const location& rhs)
{
    return !(lhs == rhs);
}

TOML11_INLINE location prev(const location& loc)
{
    location p(loc);
    p.retrace();
    return p;
}
TOML11_INLINE location next(const location& loc)
{
    location p(loc);
    p.advance(1);
    return p;
}

TOML11_INLINE location make_temporary_location(const std::string& str) noexcept
{
    location::container_type cont(str.size());
    std::transform(str.begin(), str.end(), cont.begin(),
        [](const std::string::value_type& c) {
            return cxx::bit_cast<location::char_type>(c);
        });
    return location(std::make_shared<const location::container_type>(
            std::move(cont)), "internal temporary");
}

TOML11_INLINE result<location, none_t>
find(const location& first, const location& last, const location::char_type val)
{
    return find_if(first, last, [val](const location::char_type c) {
            return c == val;
        });
}
TOML11_INLINE result<location, none_t>
rfind(const location& first, const location& last, const location::char_type val)
{
    return rfind_if(first, last, [val](const location::char_type c) {
            return c == val;
        });
}

TOML11_INLINE std::size_t
count(const location& first, const location& last, const location::char_type& c)
{
    if(first.source() != last.source())             { return 0; }
    if(first.get_location() >= last.get_location()) { return 0; }

    auto loc = first;
    std::size_t num = 0;
    while(loc.get_location() != last.get_location())
    {
        if(loc.current() == c)
        {
            num += 1;
        }
        loc.advance();
    }
    return num;
}

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_LOCATION_HPP
#endif

#endif // TOML11_LOCATION_HPP
#ifndef TOML11_REGION_HPP
#define TOML11_REGION_HPP

#ifndef TOML11_REGION_FWD_HPP
#define TOML11_REGION_FWD_HPP


#include <string>
#include <vector>

#include <cassert>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

//
// To represent where is a toml::value defined, or where does an error occur.
// Stored in toml::value. source_location will be constructed based on this.
//
class region
{
  public:

    using char_type       = location::char_type;
    using container_type  = location::container_type;
    using difference_type = location::difference_type;
    using source_ptr      = location::source_ptr;

    using iterator       = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

  public:

    // a value that is constructed manually does not have input stream info
    region()
        : source_(nullptr), source_name_(""), length_(0),
          first_(0), first_line_(0), first_column_(0), last_(0), last_line_(0),
          last_column_(0)
    {}

    // a value defined in [first, last).
    // Those source must be the same. Instread, `region` does not make sense.
    region(const location& first, const location& last);

    // shorthand of [loc, loc+1)
    explicit region(const location& loc);

    ~region() = default;
    region(const region&) = default;
    region(region&&)      = default;
    region& operator=(const region&) = default;
    region& operator=(region&&)      = default;

    bool is_ok() const noexcept { return static_cast<bool>(this->source_); }

    operator bool() const noexcept { return this->is_ok(); }

    std::size_t length() const noexcept {return this->length_;}

    std::size_t first_line_number() const noexcept
    {
        return this->first_line_;
    }
    std::size_t first_column_number() const noexcept
    {
        return this->first_column_;
    }
    std::size_t last_line_number() const noexcept
    {
        return this->last_line_;
    }
    std::size_t last_column_number() const noexcept
    {
        return this->last_column_;
    }

    char_type at(std::size_t i) const;

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    std::string as_string() const;
    std::vector<std::pair<std::string, std::size_t>> as_lines() const;

    source_ptr const&  source()      const noexcept {return this->source_;}
    std::string const& source_name() const noexcept {return this->source_name_;}

  private:

    std::pair<std::string, std::size_t>
    take_line(const_iterator begin, const_iterator end) const;

  private:

    source_ptr  source_;
    std::string source_name_;
    std::size_t length_;
    std::size_t first_;
    std::size_t first_line_;
    std::size_t first_column_;
    std::size_t last_;
    std::size_t last_line_;
    std::size_t last_column_;
};

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_REGION_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_REGION_IMPL_HPP
#define TOML11_REGION_IMPL_HPP


#include <algorithm>
#include <iterator>
#include <string>
#include <sstream>
#include <vector>
#include <cassert>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

// a value defined in [first, last).
// Those source must be the same. Instread, `region` does not make sense.
TOML11_INLINE region::region(const location& first, const location& last)
    : source_(first.source()), source_name_(first.source_name()),
      length_(last.get_location() - first.get_location()),
      first_(first.get_location()),
      first_line_(first.line_number()),
      first_column_(first.column_number()),
      last_(last.get_location()),
      last_line_(last.line_number()),
      last_column_(last.column_number())
{
    assert(first.source()      == last.source());
    assert(first.source_name() == last.source_name());
}

    // shorthand of [loc, loc+1)
TOML11_INLINE region::region(const location& loc)
    : source_(loc.source()), source_name_(loc.source_name()), length_(0),
      first_line_(0), first_column_(0), last_line_(0), last_column_(0)
{
    // if the file ends with LF, the resulting region points no char.
    if(loc.eof())
    {
        if(loc.get_location() == 0)
        {
            this->length_       = 0;
            this->first_        = 0;
            this->first_line_   = 0;
            this->first_column_ = 0;
            this->last_         = 0;
            this->last_line_    = 0;
            this->last_column_  = 0;
        }
        else
        {
            const auto first = prev(loc);
            this->first_        = first.get_location();
            this->first_line_   = first.line_number();
            this->first_column_ = first.column_number();
            this->last_         = loc.get_location();
            this->last_line_    = loc.line_number();
            this->last_column_  = loc.column_number();
            this->length_       = 1;
        }
    }
    else
    {
        this->first_        = loc.get_location();
        this->first_line_   = loc.line_number();
        this->first_column_ = loc.column_number();
        this->last_         = loc.get_location() + 1;
        this->last_line_    = loc.line_number();
        this->last_column_  = loc.column_number() + 1;
        this->length_       = 1;
    }
}

TOML11_INLINE region::char_type region::at(std::size_t i) const
{
    if(this->last_ <= this->first_ + i)
    {
        throw std::out_of_range("range::at: index " + std::to_string(i) +
                " exceeds length " + std::to_string(this->length_));
    }
    const auto iter = std::next(this->source_->cbegin(),
            static_cast<difference_type>(this->first_ + i));
    return *iter;
}

TOML11_INLINE region::const_iterator region::begin() const noexcept
{
    return std::next(this->source_->cbegin(),
            static_cast<difference_type>(this->first_));
}
TOML11_INLINE region::const_iterator region::end() const noexcept
{
    return std::next(this->source_->cbegin(),
            static_cast<difference_type>(this->last_));
}
TOML11_INLINE region::const_iterator region::cbegin() const noexcept
{
    return std::next(this->source_->cbegin(),
            static_cast<difference_type>(this->first_));
}
TOML11_INLINE region::const_iterator region::cend() const noexcept
{
    return std::next(this->source_->cbegin(),
            static_cast<difference_type>(this->last_));
}

TOML11_INLINE std::string region::as_string() const
{
    if(this->is_ok())
    {
        const auto begin = std::next(this->source_->cbegin(), static_cast<difference_type>(this->first_));
        const auto end   = std::next(this->source_->cbegin(), static_cast<difference_type>(this->last_ ));
        return ::toml::detail::make_string(begin, end);
    }
    else
    {
        return std::string("");
    }
}

TOML11_INLINE std::pair<std::string, std::size_t>
region::take_line(const_iterator begin, const_iterator end) const
{
    // To omit long line, we cap region by before/after 30 chars
    const auto dist_before = std::distance(source_->cbegin(), begin);
    const auto dist_after  = std::distance(end, source_->cend());

    const const_iterator capped_begin = (dist_before <= 30) ? source_->cbegin() : std::prev(begin, 30);
    const const_iterator capped_end   = (dist_after  <= 30) ? source_->cend()   : std::next(end,   30);

    const auto lf = char_type('\n');
    const auto lf_before = std::find(cxx::make_reverse_iterator(begin),
                                     cxx::make_reverse_iterator(capped_begin), lf);
    const auto lf_after  = std::find(end, capped_end, lf);

    auto offset = static_cast<std::size_t>(std::distance(lf_before.base(), begin));

    std::string retval = make_string(lf_before.base(), lf_after);

    if(lf_before.base() != source_->cbegin() && *lf_before != lf)
    {
        retval = "... " + retval;
        offset += 4;
    }

    if(lf_after != source_->cend() && *lf_after != lf)
    {
        retval = retval + " ...";
    }

    return std::make_pair(retval, offset);
}

TOML11_INLINE std::vector<std::pair<std::string, std::size_t>> region::as_lines() const
{
    assert(this->is_ok());
    if(this->length_ == 0)
    {
        return std::vector<std::pair<std::string, std::size_t>>{
            std::make_pair("", std::size_t(0))
        };
    }

    // Consider the following toml file
    // ```
    // array = [
    //   1, 2, 3,
    // ] # comment
    // ```
    // and the region represnets
    // ```
    //         [
    //   1, 2, 3,
    // ]
    // ```
    // but we want to show the following.
    // ```
    // array = [
    //   1, 2, 3,
    // ] # comment
    // ```
    // So we need to find LFs before `begin` and after `end`.
    //
    // But, if region ends with LF, it should not include the next line.
    // ```
    // a = 42
    //     ^^^- with the last LF
    // ```
    // So we start from `end-1` when looking for LF.

    const auto begin_idx = static_cast<difference_type>(this->first_);
    const auto end_idx   = static_cast<difference_type>(this->last_) - 1;

    // length_ != 0, so begin < end. then begin <= end-1
    assert(begin_idx <= end_idx);

    const auto begin = std::next(this->source_->cbegin(), begin_idx);
    const auto end   = std::next(this->source_->cbegin(), end_idx);

    assert(this->first_line_number() <= this->last_line_number());

    if(this->first_line_number() == this->last_line_number())
    {
        return std::vector<std::pair<std::string, std::size_t>>{
            this->take_line(begin, end)
        };
    }

    // we have multiple lines. `begin` and `end` points different lines.
    // that means that there is at least one `LF` between `begin` and `end`.

    const auto after_begin = std::distance(begin, this->source_->cend());
    const auto before_end  = std::distance(this->source_->cbegin(), end);

    const_iterator capped_file_end   = this->source_->cend();
    const_iterator capped_file_begin = this->source_->cbegin();
    if(60 < after_begin) {capped_file_end   = std::next(begin, 50);}
    if(60 < before_end)  {capped_file_begin = std::prev(end,   50);}

    const auto lf = char_type('\n');
    const auto first_line_end  = std::find(begin, capped_file_end, lf);
    const auto last_line_begin = std::find(capped_file_begin, end, lf);

    const auto first_line = this->take_line(begin, first_line_end);
    const auto last_line  = this->take_line(last_line_begin, end);

    if(this->first_line_number() + 1 == this->last_line_number())
    {
        return std::vector<std::pair<std::string, std::size_t>>{
            first_line, last_line
        };
    }
    else
    {
        return std::vector<std::pair<std::string, std::size_t>>{
            first_line, std::make_pair("...", 0), last_line
        };
    }
}

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_REGION_IMPL_HPP
#endif

#endif // TOML11_REGION_HPP
#ifndef TOML11_SCANNER_HPP
#define TOML11_SCANNER_HPP

#ifndef TOML11_SCANNER_FWD_HPP
#define TOML11_SCANNER_FWD_HPP


#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdio>
#include <cctype>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

class scanner_base
{
  public:
    virtual ~scanner_base() = default;
    virtual region scan(location& loc) const = 0;
    virtual scanner_base* clone() const = 0;

    // returns expected character or set of characters or literal.
    // to show the error location, it changes loc (in `sequence`, especially).
    virtual std::string expected_chars(location& loc) const = 0;
    virtual std::string name() const = 0;
};

// make `scanner*` copyable
struct scanner_storage
{
    template<typename Scanner, cxx::enable_if_t<
        std::is_base_of<scanner_base, cxx::remove_cvref_t<Scanner>>::value,
        std::nullptr_t> = nullptr>
    explicit scanner_storage(Scanner&& s)
        : scanner_(cxx::make_unique<cxx::remove_cvref_t<Scanner>>(std::forward<Scanner>(s)))
    {}
    ~scanner_storage() = default;

    scanner_storage(const scanner_storage& other);
    scanner_storage& operator=(const scanner_storage& other);
    scanner_storage(scanner_storage&&) = default;
    scanner_storage& operator=(scanner_storage&&) = default;

    bool is_ok() const noexcept {return static_cast<bool>(scanner_);}

    region scan(location& loc) const;

    std::string expected_chars(location& loc) const;

    scanner_base& get() const noexcept;

    std::string name() const;

  private:

    std::unique_ptr<scanner_base> scanner_;
};

// ----------------------------------------------------------------------------

class character final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit character(const char_type c) noexcept
        : value_(c)
    {}
    ~character() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    char_type value_;
};

// ----------------------------------------------------------------------------

class character_either final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    template<std::size_t N>
    explicit character_either(const char (&cs)[N]) noexcept
        : value_(cs), size_(N-1) // remove null character at the end
    {}
    ~character_either() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    const char* value_;
    std::size_t size_;
};

// ----------------------------------------------------------------------------

class character_in_range final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit character_in_range(const char_type from, const char_type to) noexcept
        : from_(from), to_(to)
    {}
    ~character_in_range() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    char_type from_;
    char_type to_;
};

// ----------------------------------------------------------------------------

class literal final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    template<std::size_t N>
    explicit literal(const char (&cs)[N]) noexcept
        : value_(cs), size_(N-1) // remove null character at the end
    {}
    ~literal() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    const char* value_;
    std::size_t size_;
};

// ----------------------------------------------------------------------------

class sequence final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename ... Ts>
    explicit sequence(Ts&& ... args)
    {
        push_back_all(std::forward<Ts>(args)...);
    }
    sequence(const sequence&)            = default;
    sequence(sequence&&)                 = default;
    sequence& operator=(const sequence&) = default;
    sequence& operator=(sequence&&)      = default;
    ~sequence() override                 = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:

    void push_back_all()
    {
        return;
    }
    template<typename T, typename ... Ts>
    void push_back_all(T&& head, Ts&& ... args)
    {
        others_.emplace_back(std::forward<T>(head));
        push_back_all(std::forward<Ts>(args)...);
        return;
    }

  private:
    std::vector<scanner_storage> others_;
};

// ----------------------------------------------------------------------------

class either final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename ... Ts>
    explicit either(Ts&& ... args)
    {
        push_back_all(std::forward<Ts>(args)...);
    }
    either(const either&)            = default;
    either(either&&)                 = default;
    either& operator=(const either&) = default;
    either& operator=(either&&)      = default;
    ~either() override               = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:

    void push_back_all()
    {
        return;
    }
    template<typename T, typename ... Ts>
    void push_back_all(T&& head, Ts&& ... args)
    {
        others_.emplace_back(std::forward<T>(head));
        push_back_all(std::forward<Ts>(args)...);
        return;
    }

  private:
    std::vector<scanner_storage> others_;
};

// ----------------------------------------------------------------------------

class repeat_exact final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename Scanner>
    repeat_exact(const std::size_t length, Scanner&& other)
        : length_(length), other_(std::forward<Scanner>(other))
    {}
    repeat_exact(const repeat_exact&)            = default;
    repeat_exact(repeat_exact&&)                 = default;
    repeat_exact& operator=(const repeat_exact&) = default;
    repeat_exact& operator=(repeat_exact&&)      = default;
    ~repeat_exact() override                     = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    std::size_t length_;
    scanner_storage other_;
};

// ----------------------------------------------------------------------------

class repeat_at_least final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename Scanner>
    repeat_at_least(const std::size_t length, Scanner&& s)
        : length_(length), other_(std::forward<Scanner>(s))
    {}
    repeat_at_least(const repeat_at_least&)            = default;
    repeat_at_least(repeat_at_least&&)                 = default;
    repeat_at_least& operator=(const repeat_at_least&) = default;
    repeat_at_least& operator=(repeat_at_least&&)      = default;
    ~repeat_at_least() override                        = default;

    region scan(location& loc) const override;

    std::string expected_chars(location& loc) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    std::size_t length_;
    scanner_storage other_;
};

// ----------------------------------------------------------------------------

class maybe final: public scanner_base
{
  public:
    using char_type = location::char_type;

  public:

    template<typename Scanner>
    explicit maybe(Scanner&& s)
        : other_(std::forward<Scanner>(s))
    {}
    maybe(const maybe&)            = default;
    maybe(maybe&&)                 = default;
    maybe& operator=(const maybe&) = default;
    maybe& operator=(maybe&&)      = default;
    ~maybe() override              = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override;

    scanner_base* clone() const override;

    std::string name() const override;

  private:
    scanner_storage other_;
};

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_SCANNER_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_SCANNER_IMPL_HPP
#define TOML11_SCANNER_IMPL_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

TOML11_INLINE scanner_storage::scanner_storage(const scanner_storage& other)
    : scanner_(nullptr)
{
    if(other.is_ok())
    {
        scanner_.reset(other.get().clone());
    }
}
TOML11_INLINE scanner_storage& scanner_storage::operator=(const scanner_storage& other)
{
    if(this == std::addressof(other)) {return *this;}
    if(other.is_ok())
    {
        scanner_.reset(other.get().clone());
    }
    return *this;
}

TOML11_INLINE region scanner_storage::scan(location& loc) const
{
    assert(this->is_ok());
    return this->scanner_->scan(loc);
}

TOML11_INLINE std::string scanner_storage::expected_chars(location& loc) const
{
    assert(this->is_ok());
    return this->scanner_->expected_chars(loc);
}

TOML11_INLINE scanner_base& scanner_storage::get() const noexcept
{
    assert(this->is_ok());
    return *scanner_;
}

TOML11_INLINE std::string scanner_storage::name() const
{
    assert(this->is_ok());
    return this->scanner_->name();
}

// ----------------------------------------------------------------------------

TOML11_INLINE region character::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    if(loc.current() == this->value_)
    {
        const auto first = loc;
        loc.advance(1);
        return region(first, loc);
    }
    return region{};
}

TOML11_INLINE std::string character::expected_chars(location&) const
{
    return show_char(value_);
}

TOML11_INLINE scanner_base* character::clone() const
{
    return new character(*this);
}

TOML11_INLINE std::string character::name() const
{
    return "character{" + show_char(value_) + "}";
}

// ----------------------------------------------------------------------------

TOML11_INLINE region character_either::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    for(std::size_t i=0; i<this->size_; ++i)
    {
        const auto c = char_type(this->value_[i]);
        if(loc.current() == c)
        {
            const auto first = loc;
            loc.advance(1);
            return region(first, loc);
        }
    }
    return region{};
}

TOML11_INLINE std::string character_either::expected_chars(location&) const
{
    assert( this->value_ );
    assert( this->size_ != 0 );

    std::string expected;
    if(this->size_ == 1)
    {
        expected += show_char(char_type(value_[0]));
    }
    else if(this->size_ == 2)
    {
        expected += show_char(char_type(value_[0])) + " or " +
                    show_char(char_type(value_[1]));
    }
    else
    {
        for(std::size_t i=0; i<this->size_; ++i)
        {
            if(i != 0)
            {
                expected += ", ";
            }
            if(i + 1 == this->size_)
            {
                expected += "or ";
            }
            expected += show_char(char_type(value_[i]));
        }
    }
    return expected;
}

TOML11_INLINE scanner_base* character_either::clone() const
{
    return new character_either(*this);
}

TOML11_INLINE std::string character_either::name() const
{
    std::string n("character_either{");
    for(std::size_t i=0; i<this->size_; ++i)
    {
        const auto c = char_type(this->value_[i]);
        n += show_char(c);
        n += ", ";
    }
    if(this->size_ != 0)
    {
        n.pop_back();
        n.pop_back();
    }
    n += "}";
    return n;
}

// ----------------------------------------------------------------------------
// character_in_range

TOML11_INLINE region character_in_range::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    const auto curr = loc.current();
    if(this->from_ <= curr && curr <= this->to_)
    {
        const auto first = loc;
        loc.advance(1);
        return region(first, loc);
    }
    return region{};
}

TOML11_INLINE std::string character_in_range::expected_chars(location&) const
{
    std::string expected("from `");
    expected += show_char(from_);
    expected += "` to `";
    expected += show_char(to_);
    expected += "`";
    return expected;
}

TOML11_INLINE scanner_base* character_in_range::clone() const
{
    return new character_in_range(*this);
}

TOML11_INLINE std::string character_in_range::name() const
{
    return "character_in_range{" + show_char(from_) + "," + show_char(to_) + "}";
}

// ----------------------------------------------------------------------------
// literal

TOML11_INLINE region literal::scan(location& loc) const
{
    const auto first = loc;
    for(std::size_t i=0; i<size_; ++i)
    {
        if(loc.eof() || char_type(value_[i]) != loc.current())
        {
            loc = first;
            return region{};
        }
        loc.advance(1);
    }
    return region(first, loc);
}

TOML11_INLINE std::string literal::expected_chars(location&) const
{
    return std::string(value_);
}

TOML11_INLINE scanner_base* literal::clone() const
{
    return new literal(*this);
}

TOML11_INLINE std::string literal::name() const
{
    return std::string("literal{") + std::string(value_, size_) + "}";
}

// ----------------------------------------------------------------------------
// sequence

TOML11_INLINE region sequence::scan(location& loc) const
{
    const auto first = loc;
    for(const auto& other : others_)
    {
        const auto reg = other.scan(loc);
        if( ! reg.is_ok())
        {
            loc = first;
            return region{};
        }
    }
    return region(first, loc);
}

TOML11_INLINE std::string sequence::expected_chars(location& loc) const
{
    const auto first = loc;
    for(const auto& other : others_)
    {
        const auto reg = other.scan(loc);
        if( ! reg.is_ok())
        {
            return other.expected_chars(loc);
        }
    }
    assert(false);
    return ""; // XXX
}

TOML11_INLINE scanner_base* sequence::clone() const
{
    return new sequence(*this);
}

TOML11_INLINE std::string sequence::name() const
{
    std::string n("sequence{");
    for(const auto& other : others_)
    {
        n += other.name();
        n += ", ";
    }
    if( ! this->others_.empty())
    {
        n.pop_back();
        n.pop_back();
    }
    n += "}";
    return n;
}

// ----------------------------------------------------------------------------
// either

TOML11_INLINE region either::scan(location& loc) const
{
    for(const auto& other : others_)
    {
        const auto reg = other.scan(loc);
        if(reg.is_ok())
        {
            return reg;
        }
    }
    return region{};
}

TOML11_INLINE std::string either::expected_chars(location& loc) const
{
    assert( ! others_.empty());

    std::string expected = others_.at(0).expected_chars(loc);
    if(others_.size() == 2)
    {
        expected += " or ";
        expected += others_.at(1).expected_chars(loc);
    }
    else
    {
        for(std::size_t i=1; i<others_.size(); ++i)
        {
            expected += ", ";
            if(i + 1 == others_.size())
            {
                expected += "or ";
            }
            expected += others_.at(i).expected_chars(loc);
        }
    }
    return expected;
}

TOML11_INLINE scanner_base* either::clone() const
{
    return new either(*this);
}

TOML11_INLINE std::string either::name() const
{
    std::string n("either{");
    for(const auto& other : others_)
    {
        n += other.name();
        n += ", ";
    }
    if( ! this->others_.empty())
    {
        n.pop_back();
        n.pop_back();
    }
    n += "}";
    return n;
}

// ----------------------------------------------------------------------------
// repeat_exact

TOML11_INLINE region repeat_exact::scan(location& loc) const
{
    const auto first = loc;
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            loc = first;
            return region{};
        }
    }
    return region(first, loc);
}

TOML11_INLINE std::string repeat_exact::expected_chars(location& loc) const
{
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            return other_.expected_chars(loc);
        }
    }
    assert(false);
    return "";
}

TOML11_INLINE scanner_base* repeat_exact::clone() const
{
    return new repeat_exact(*this);
}

TOML11_INLINE std::string repeat_exact::name() const
{
    return "repeat_exact{" + std::to_string(length_) + ", " + other_.name() + "}";
}

// ----------------------------------------------------------------------------
// repeat_at_least

TOML11_INLINE region repeat_at_least::scan(location& loc) const
{
    const auto first = loc;
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            loc = first;
            return region{};
        }
    }
    while( ! loc.eof())
    {
        const auto checkpoint = loc;
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            loc = checkpoint;
            return region(first, loc);
        }
    }
    return region(first, loc);
}

TOML11_INLINE std::string repeat_at_least::expected_chars(location& loc) const
{
    for(std::size_t i=0; i<length_; ++i)
    {
        const auto reg = other_.scan(loc);
        if( ! reg.is_ok())
        {
            return other_.expected_chars(loc);
        }
    }
    assert(false);
    return "";
}

TOML11_INLINE scanner_base* repeat_at_least::clone() const
{
    return new repeat_at_least(*this);
}

TOML11_INLINE std::string repeat_at_least::name() const
{
    return "repeat_at_least{" + std::to_string(length_) + ", " + other_.name() + "}";
}

// ----------------------------------------------------------------------------
// maybe

TOML11_INLINE region maybe::scan(location& loc) const
{
    const auto first = loc;
    const auto reg = other_.scan(loc);
    if( ! reg.is_ok())
    {
        loc = first;
    }
    return region(first, loc);
}

TOML11_INLINE std::string maybe::expected_chars(location&) const
{
    return "";
}

TOML11_INLINE scanner_base* maybe::clone() const
{
    return new maybe(*this);
}

TOML11_INLINE std::string maybe::name() const
{
    return "maybe{" + other_.name() + "}";
}

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_SCANNER_IMPL_HPP
#endif

#endif // TOML11_SCANNER_HPP
#ifndef TOML11_SYNTAX_HPP
#define TOML11_SYNTAX_HPP

#ifndef TOML11_SYNTAX_FWD_HPP
#define TOML11_SYNTAX_FWD_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{
namespace syntax
{

using char_type = location::char_type;

// ===========================================================================
// UTF-8

// avoid redundant representation and out-of-unicode sequence

character_in_range const& utf8_1byte (const spec&);
sequence           const& utf8_2bytes(const spec&);
sequence           const& utf8_3bytes(const spec&);
sequence           const& utf8_4bytes(const spec&);

class non_ascii final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit non_ascii(const spec& s) noexcept
        : utf8_2B_(utf8_2bytes(s)),
          utf8_3B_(utf8_3bytes(s)),
          utf8_4B_(utf8_4bytes(s))
    {}
    ~non_ascii() override = default;

    region scan(location& loc) const override
    {
        {
            const auto reg = utf8_2B_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        {
            const auto reg = utf8_3B_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        {
            const auto reg = utf8_4B_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        return region{};
    }

    std::string expected_chars(location&) const override
    {
        return "non-ascii utf-8 bytes";
    }

    scanner_base* clone() const override
    {
        return new non_ascii(*this);
    }

    std::string name() const override
    {
        return "non_ascii";
    }

  private:
    sequence utf8_2B_;
    sequence utf8_3B_;
    sequence utf8_4B_;
};

// ===========================================================================
// Whitespace

character_either const& wschar(const spec&);

repeat_at_least const& ws(const spec& s);

// ===========================================================================
// Newline

either const& newline(const spec&);

// ===========================================================================
// Comments

either const& allowed_comment_char(const spec& s);

// XXX Note that it does not take newline
sequence const& comment(const spec& s);

// ===========================================================================
// Boolean

either const& boolean(const spec&);

// ===========================================================================
// Integer

class digit final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit digit(const spec&) noexcept
      : scanner_(char_type('0'), char_type('9'))
    {}


    ~digit() override = default;

    region scan(location& loc) const override
    {
        return scanner_.scan(loc);
    }

    std::string expected_chars(location&) const override
    {
        return "digit [0-9]";
    }

    scanner_base* clone() const override
    {
        return new digit(*this);
    }

    std::string name() const override
    {
        return "digit";
    }

  private:

    character_in_range scanner_;
};

class alpha final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit alpha(const spec&) noexcept
      : lowercase_(char_type('a'), char_type('z')),
        uppercase_(char_type('A'), char_type('Z'))
    {}
    ~alpha() override = default;

    region scan(location& loc) const override
    {
        {
            const auto reg = lowercase_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        {
            const auto reg = uppercase_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        return region{};
    }

    std::string expected_chars(location&) const override
    {
        return "alpha [a-zA-Z]";
    }

    scanner_base* clone() const override
    {
        return new alpha(*this);
    }

    std::string name() const override
    {
        return "alpha";
    }

  private:

    character_in_range lowercase_;
    character_in_range uppercase_;
};

class hexdig final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit hexdig(const spec& s) noexcept
      : digit_(s),
        lowercase_(char_type('a'), char_type('f')),
        uppercase_(char_type('A'), char_type('F'))
    {}
    ~hexdig() override = default;

    region scan(location& loc) const override
    {
        {
            const auto reg = digit_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        {
            const auto reg = lowercase_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        {
            const auto reg = uppercase_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        return region{};
    }

    std::string expected_chars(location&) const override
    {
        return "hex [0-9a-fA-F]";
    }

    scanner_base* clone() const override
    {
        return new hexdig(*this);
    }

    std::string name() const override
    {
        return "hexdig";
    }

  private:

    digit              digit_;
    character_in_range lowercase_;
    character_in_range uppercase_;
};

sequence const& num_suffix(const spec& s);

sequence const& dec_int(const spec& s);
sequence const& hex_int(const spec& s);
sequence const& oct_int(const spec&);
sequence const& bin_int(const spec&);
either   const& integer(const spec& s);

// ===========================================================================
// Floating

sequence const& zero_prefixable_int(const spec& s);
sequence const& fractional_part(const spec& s);
sequence const& exponent_part(const spec& s);
sequence const& hex_floating(const spec& s);
either   const& floating(const spec& s);

// ===========================================================================
// Datetime

sequence const& local_date(const spec& s);
sequence const& local_time(const spec& s);
either   const& time_offset(const spec& s);
sequence const& full_time(const spec& s);
character_either const& time_delim(const spec&);
sequence const& local_datetime(const spec& s);
sequence const& offset_datetime(const spec& s);

// ===========================================================================
// String

sequence const& escaped_x2(const spec& s);
sequence const& escaped_u4(const spec& s);
sequence const& escaped_U8(const spec& s);

sequence const& escaped     (const spec& s);
either   const& basic_char  (const spec& s);
sequence const& basic_string(const spec& s);

// ---------------------------------------------------------------------------
// multiline string

sequence const& escaped_newline(const spec& s);
sequence const& ml_basic_string(const spec& s);

// ---------------------------------------------------------------------------
// literal string

either   const& literal_char(const spec& s);
sequence const& literal_string(const spec& s);
sequence const& ml_literal_string(const spec& s);
either   const& string(const spec& s);

// ===========================================================================
// Keys

// to keep `expected_chars` simple
class non_ascii_key_char final : public scanner_base
{
  public:

    using char_type = location::char_type;

  private:

    using in_range = character_in_range; // make definition short

  public:

    explicit non_ascii_key_char(const spec& s) noexcept;
    ~non_ascii_key_char() override = default;

    region scan(location& loc) const override;

    std::string expected_chars(location&) const override
    {
        return "bare key non-ASCII script";
    }

    scanner_base* clone() const override
    {
        return new non_ascii_key_char(*this);
    }

    std::string name() const override
    {
        return "non-ASCII bare key";
    }

  private:

    std::uint32_t read_utf8(location& loc) const;
};


repeat_at_least const& unquoted_key(const spec& s);
either   const& quoted_key(const spec& s);
either   const& simple_key(const spec& s);
sequence const& dot_sep(const spec& s);
sequence const& dotted_key(const spec& s);

class key final : public scanner_base
{
  public:

    using char_type = location::char_type;

  public:

    explicit key(const spec& s) noexcept
        : dotted_(dotted_key(s)),
          simple_(simple_key(s))
    {}
    ~key() override = default;

    region scan(location& loc) const override
    {
        {
            const auto reg = dotted_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        {
            const auto reg = simple_.scan(loc);
            if(reg.is_ok()) {return reg;}
        }
        return region{};
    }

    std::string expected_chars(location&) const override
    {
        return "basic key([a-zA-Z0-9_-]) or quoted key(\" or ')";
    }

    scanner_base* clone() const override
    {
        return new key(*this);
    }

    std::string name() const override
    {
        return "key";
    }

  private:

    sequence dotted_;
    either   simple_;
};

sequence const& keyval_sep(const spec& s);

// ===========================================================================
// Table key

sequence const& std_table(const spec& s);

sequence const& array_table(const spec& s);

// ===========================================================================
// extension: null

literal const& null_value(const spec&);

} // namespace syntax
} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_SYNTAX_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_SYNTAX_IMPL_HPP
#define TOML11_SYNTAX_IMPL_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{
namespace syntax
{

using char_type = location::char_type;

template<typename F>
struct syntax_cache
{
    using value_type = cxx::return_type_of_t<F, const spec&>;
    static_assert(std::is_base_of<scanner_base, value_type>::value, "");

    explicit syntax_cache(F f)
        : func_(std::move(f)), cache_(cxx::make_nullopt())
    {}

    value_type const& at(const spec& s)
    {
        if( ! this->cache_.has_value() || this->cache_.value().first != s)
        {
            this->cache_ = std::make_pair(s, func_(s));
        }
        return this->cache_.value().second;
    }

  private:
    F func_;
    cxx::optional<std::pair<spec, value_type>> cache_;
};

template<typename F>
syntax_cache<cxx::remove_cvref_t<F>> make_cache(F&& f)
{
    return syntax_cache<cxx::remove_cvref_t<F>>(std::forward<F>(f));
}

// ===========================================================================
// UTF-8

// avoid redundant representation and out-of-unicode sequence

TOML11_INLINE character_in_range const& utf8_1byte(const spec&)
{
    static thread_local character_in_range cache(0x00, 0x7F);
    return cache;
}

TOML11_INLINE sequence const& utf8_2bytes(const spec&)
{
    static thread_local sequence cache(
            character_in_range(0xC2, 0xDF),
            character_in_range(0x80, 0xBF));
    return cache;
}

TOML11_INLINE sequence const& utf8_3bytes(const spec&)
{
    static thread_local sequence cache(/*1~2 bytes = */either(
        sequence(character         (0xE0),       character_in_range(0xA0, 0xBF)),
        sequence(character_in_range(0xE1, 0xEC), character_in_range(0x80, 0xBF)),
        sequence(character         (0xED),       character_in_range(0x80, 0x9F)),
        sequence(character_in_range(0xEE, 0xEF), character_in_range(0x80, 0xBF))
    ), /*3rd byte = */ character_in_range(0x80, 0xBF));

    return cache;
}

TOML11_INLINE sequence const& utf8_4bytes(const spec&)
{
    static thread_local sequence cache(/*1~2 bytes = */either(
        sequence(character         (0xF0),       character_in_range(0x90, 0xBF)),
        sequence(character_in_range(0xF1, 0xF3), character_in_range(0x80, 0xBF)),
        sequence(character         (0xF4),       character_in_range(0x80, 0x8F))
    ), character_in_range(0x80, 0xBF), character_in_range(0x80, 0xBF));

    return cache;
}

// ===========================================================================
// Whitespace

TOML11_INLINE character_either const& wschar(const spec&)
{
    static thread_local character_either cache(" \t");
    return cache;
}

TOML11_INLINE repeat_at_least const& ws(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s){
        return repeat_at_least(0, wschar(s));
    });
    return cache.at(sp);
}

// ===========================================================================
// Newline

TOML11_INLINE either const& newline(const spec&)
{
    static thread_local either cache(character(char_type('\n')), literal("\r\n"));
    return cache;
}

// ===========================================================================
// Comments

TOML11_INLINE either const& allowed_comment_char(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s){
            if(s.ext_allow_control_characters_in_comments)
            {
                return either(
                    character_in_range(0x01, 0x09),
                    character_in_range(0x0E, 0x7F),
                    non_ascii(s)
                );
            }
            else
            {
                return either(
                    character(0x09),
                    character_in_range(0x20, 0x7E),
                    non_ascii(s)
                );
            }
        });
    return cache.at(sp);
}

// XXX Note that it does not take newline
TOML11_INLINE sequence const& comment(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s){
        return sequence(character(char_type('#')),
                    repeat_at_least(0, allowed_comment_char(s)));
    });
    return cache.at(sp);
}

// ===========================================================================
// Boolean

TOML11_INLINE either const& boolean(const spec&)
{
    static thread_local either cache(literal("true"), literal("false"));
    return cache;
}

// ===========================================================================
// Integer

// non-digit-graph = ([a-zA-Z]|unicode mb char)
// graph           = ([a-zA-Z0-9]|unicode mb char)
// suffix          = _ non-digit-graph (graph | _graph)
TOML11_INLINE sequence const& num_suffix(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        const auto non_digit_graph = [&s]() {
            return either(
                alpha(s),
                non_ascii(s)
            );
        };
        const auto graph = [&s]() {
            return either(
                alpha(s),
                digit(s),
                non_ascii(s)
            );
        };

        return sequence(
                character(char_type('_')),
                non_digit_graph(),
                repeat_at_least(0,
                    either(
                        sequence(character(char_type('_')), graph()),
                        graph()
                    )
                )
            );
        });
    return cache.at(sp);
}

TOML11_INLINE sequence const& dec_int(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        const auto digit19 = []() {
            return character_in_range(char_type('1'), char_type('9'));
        };
        return sequence(
                maybe(character_either("+-")),
                either(
                    sequence(
                        digit19(),
                        repeat_at_least(1,
                            either(
                                digit(s),
                                sequence(character(char_type('_')), digit(s))
                            )
                        )
                    ),
                    digit(s)
                )
            );
        });
    return cache.at(sp);
}

TOML11_INLINE sequence const& hex_int(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                literal("0x"),
                hexdig(s),
                repeat_at_least(0,
                    either(
                        hexdig(s),
                        sequence(character(char_type('_')), hexdig(s))
                    )
                )
            );
        });
    return cache.at(sp);
}

TOML11_INLINE sequence const& oct_int(const spec& s)
{
    static thread_local auto cache = make_cache([](const spec&) {
        const auto digit07 = []() {
            return character_in_range(char_type('0'), char_type('7'));
        };
        return sequence(
                literal("0o"),
                digit07(),
                repeat_at_least(0,
                    either(
                        digit07(),
                        sequence(character(char_type('_')), digit07())
                    )
                )
            );
        });
    return cache.at(s);
}

TOML11_INLINE sequence const& bin_int(const spec& s)
{
    static thread_local auto cache = make_cache([](const spec&) {
        const auto digit01 = []() {
            return character_either("01");
        };
        return sequence(
                literal("0b"),
                digit01(),
                repeat_at_least(0,
                    either(
                        digit01(),
                        sequence(character(char_type('_')), digit01())
                    )
                )
            );
        });
    return cache.at(s);
}

TOML11_INLINE either const& integer(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return either(
                hex_int(s),
                oct_int(s),
                bin_int(s),
                dec_int(s)
            );
        });
    return cache.at(sp);
}


// ===========================================================================
// Floating

TOML11_INLINE sequence const& zero_prefixable_int(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                digit(s),
                repeat_at_least(0,
                    either(
                        digit(s),
                        sequence(character('_'), digit(s))
                    )
                )
            );
        });
    return cache.at(sp);
}

TOML11_INLINE sequence const& fractional_part(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                character('.'),
                zero_prefixable_int(s)
            );
        });
    return cache.at(sp);
}

TOML11_INLINE sequence const& exponent_part(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                character_either("eE"),
                maybe(character_either("+-")),
                zero_prefixable_int(s)
            );
        });
    return cache.at(sp);
}

TOML11_INLINE sequence const& hex_floating(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        // C99 hexfloat (%a)
        // [+-]? 0x ( [0-9a-fA-F]*\.[0-9a-fA-F]+ | [0-9a-fA-F]+\.? ) [pP] [+-]? [0-9]+

        // - 0x(int).(frac)p[+-](int)
        // - 0x(int).p[+-](int)
        // - 0x.(frac)p[+-](int)
        // - 0x(int)p[+-](int)

        return sequence(
                maybe(character_either("+-")),
                character('0'),
                character_either("xX"),
                either(
                    sequence(
                        repeat_at_least(0, hexdig(s)),
                        character('.'),
                        repeat_at_least(1, hexdig(s))
                    ),
                    sequence(
                        repeat_at_least(1, hexdig(s)),
                        maybe(character('.'))
                    )
                ),
                character_either("pP"),
                maybe(character_either("+-")),
                repeat_at_least(1, character_in_range('0', '9'))
            );
        });
    return cache.at(sp);
}

TOML11_INLINE either const& floating(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return either(
                sequence(
                    dec_int(s),
                    either(
                        exponent_part(s),
                        sequence(fractional_part(s), maybe(exponent_part(s)))
                    )
                ),
                sequence(
                    maybe(character_either("+-")),
                    either(literal("inf"), literal("nan"))
                )
            );
        });
    return cache.at(sp);
}

// ===========================================================================
// Datetime

TOML11_INLINE sequence const& local_date(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                repeat_exact(4, digit(s)),
                character('-'),
                repeat_exact(2, digit(s)),
                character('-'),
                repeat_exact(2, digit(s))
            );
        });
    return cache.at(sp);
}
TOML11_INLINE sequence const& local_time(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        if(s.v1_1_0_make_seconds_optional)
        {
            return sequence(
                repeat_exact(2, digit(s)),
                character(':'),
                repeat_exact(2, digit(s)),
                maybe(sequence(
                    character(':'),
                    repeat_exact(2, digit(s)),
                    maybe(sequence(character('.'), repeat_at_least(1, digit(s))))
                )));
        }
        else
        {
            return sequence(
                repeat_exact(2, digit(s)),
                character(':'),
                repeat_exact(2, digit(s)),
                character(':'),
                repeat_exact(2, digit(s)),
                maybe(sequence(character('.'), repeat_at_least(1, digit(s))))
            );
        }
        });
    return cache.at(sp);
}
TOML11_INLINE either const& time_offset(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return either(
                character_either("zZ"),
                sequence(character_either("+-"),
                         repeat_exact(2, digit(s)),
                         character(':'),
                         repeat_exact(2, digit(s))
                 )
            );
        });
    return cache.at(sp);
}
TOML11_INLINE sequence const& full_time(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
            return sequence(local_time(s), time_offset(s));
        });
    return cache.at(sp);
}
TOML11_INLINE character_either const& time_delim(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec&) {
            return character_either("Tt ");
        });
    return cache.at(sp);
}
TOML11_INLINE sequence const& local_datetime(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
            return sequence(local_date(s), time_delim(s), local_time(s));
        });
    return cache.at(sp);
}
TOML11_INLINE sequence const& offset_datetime(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
            return sequence(local_date(s), time_delim(s), full_time(s));
        });
    return cache.at(sp);
}

// ===========================================================================
// String

TOML11_INLINE sequence const& escaped_x2(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
            return sequence(character('x'), repeat_exact(2, hexdig(s)));
        });
    return cache.at(sp);
}
TOML11_INLINE sequence const& escaped_u4(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
            return sequence(character('u'), repeat_exact(4, hexdig(s)));
        });
    return cache.at(sp);
}
TOML11_INLINE sequence const& escaped_U8(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
            return sequence(character('U'), repeat_exact(8, hexdig(s)));
        });
    return cache.at(sp);
}

TOML11_INLINE sequence const& escaped(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        const auto escape_char = [&s] {
            if(s.v1_1_0_add_escape_sequence_e)
            {
                return character_either("\"\\bfnrte");
            }
            else
            {
                return character_either("\"\\bfnrt");
            }
        };

        const auto escape_seq = [&s, &escape_char] {
            if(s.v1_1_0_add_escape_sequence_x)
            {
                return either(
                    escape_char(),
                    escaped_u4(s),
                    escaped_U8(s),
                    escaped_x2(s)
                );
            }
            else
            {
                return either(
                    escape_char(),
                    escaped_u4(s),
                    escaped_U8(s)
                );
            }
        };

        return sequence(character('\\'), escape_seq());
    });
    return cache.at(sp);
}

TOML11_INLINE either const& basic_char(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        const auto basic_unescaped = [&s]() {
            return either(
                    wschar(s),
                    character(0x21),                // 22 is "
                    character_in_range(0x23, 0x5B), // 5C is backslash
                    character_in_range(0x5D, 0x7E), // 7F is DEL
                    non_ascii(s)
                );
        };
        return either(basic_unescaped(), escaped(s));
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& basic_string(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                character('"'),
                repeat_at_least(0, basic_char(s)),
                character('"')
            );
    });
    return cache.at(sp);
}

// ---------------------------------------------------------------------------
// multiline string

TOML11_INLINE sequence const& escaped_newline(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                character('\\'), ws(s), newline(s),
                repeat_at_least(0, either(wschar(s), newline(s)))
            );
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& ml_basic_string(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        const auto mlb_content = [&s]() {
            return either(basic_char(s), newline(s), escaped_newline(s));
        };
        const auto mlb_quotes = []() {
            return either(literal("\"\""), character('\"'));
        };

        return sequence(
                literal("\"\"\""),
                maybe(newline(s)),
                repeat_at_least(0, mlb_content()),
                repeat_at_least(0,
                    sequence(
                        mlb_quotes(),
                        repeat_at_least(1, mlb_content())
                    )
                ),
                // XXX """ and mlb_quotes are intentionally reordered to avoid
                //     unexpected match of mlb_quotes
                literal("\"\"\""),
                maybe(mlb_quotes())
            );
    });
    return cache.at(sp);
}

// ---------------------------------------------------------------------------
// literal string

TOML11_INLINE either const& literal_char(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return either(
                character         (0x09),
                character_in_range(0x20, 0x26),
                character_in_range(0x28, 0x7E),
                non_ascii(s)
            );
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& literal_string(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
                character('\''),
                repeat_at_least(0, literal_char(s)),
                character('\'')
            );
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& ml_literal_string(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        const auto mll_quotes = []() {
            return either(literal("''"), character('\''));
        };
        const auto mll_content = [&s]() {
            return either(literal_char(s), newline(s));
        };

        return sequence(
                literal("'''"),
                maybe(newline(s)),
                repeat_at_least(0, mll_content()),
                repeat_at_least(0, sequence(
                        mll_quotes(),
                        repeat_at_least(1, mll_content())
                    )
                ),
                literal("'''"),
                maybe(mll_quotes())
                // XXX ''' and mll_quotes are intentionally reordered to avoid
                //     unexpected match of mll_quotes
            );
    });
    return cache.at(sp);
}

TOML11_INLINE either const& string(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return either(
                ml_basic_string(s),
                ml_literal_string(s),
                basic_string(s),
                literal_string(s)
            );
    });
    return cache.at(sp);
}

// ===========================================================================
// Keys

// to keep `expected_chars` simple
TOML11_INLINE non_ascii_key_char::non_ascii_key_char(const spec& s) noexcept
{
    assert(s.ext_allow_non_english_in_bare_keys);
    (void)s; // for NDEBUG
}

TOML11_INLINE std::uint32_t non_ascii_key_char::read_utf8(location& loc) const
{
    // U+0000   ... U+0079  ; 0xxx_xxxx
    // U+0080   ... U+07FF  ; 110y_yyyx 10xx_xxxx;
    // U+0800   ... U+FFFF  ; 1110_yyyy 10yx_xxxx 10xx_xxxx
    // U+010000 ... U+10FFFF; 1111_0yyy 10yy_xxxx 10xx_xxxx 10xx_xxxx

    const unsigned char b1 = loc.current(); loc.advance(1);
    if(b1 < 0x80)
    {
        return static_cast<std::uint32_t>(b1);
    }
    else if((b1 >> 5) == 6) // 0b110 == 6
    {
        const auto b2 = loc.current(); loc.advance(1);

        const std::uint32_t c1 = b1 & ((1 << 5) - 1);
        const std::uint32_t c2 = b2 & ((1 << 6) - 1);
        const std::uint32_t codep = (c1 << 6) + c2;

        if(codep < 0x80)
        {
            return 0xFFFFFFFF;
        }
        return codep;
    }
    else if((b1 >> 4) == 14) // 0b1110 == 14
    {
        const auto b2 = loc.current(); loc.advance(1); if(loc.eof()) {return 0xFFFFFFFF;}
        const auto b3 = loc.current(); loc.advance(1);

        const std::uint32_t c1 = b1 & ((1 << 4) - 1);
        const std::uint32_t c2 = b2 & ((1 << 6) - 1);
        const std::uint32_t c3 = b3 & ((1 << 6) - 1);

        const std::uint32_t codep = (c1 << 12) + (c2 << 6) + c3;
        if(codep < 0x800)
        {
            return 0xFFFFFFFF;
        }
        return codep;
    }
    else if((b1 >> 3) == 30) // 0b11110 == 30
    {
        const auto b2 = loc.current(); loc.advance(1); if(loc.eof()) {return 0xFFFFFFFF;}
        const auto b3 = loc.current(); loc.advance(1); if(loc.eof()) {return 0xFFFFFFFF;}
        const auto b4 = loc.current(); loc.advance(1);

        const std::uint32_t c1 = b1 & ((1 << 3) - 1);
        const std::uint32_t c2 = b2 & ((1 << 6) - 1);
        const std::uint32_t c3 = b3 & ((1 << 6) - 1);
        const std::uint32_t c4 = b4 & ((1 << 6) - 1);
        const std::uint32_t codep = (c1 << 18) + (c2 << 12) + (c3 << 6) + c4;

        if(codep < 0x10000)
        {
            return 0xFFFFFFFF;
        }
        return codep;
    }
    else // not a Unicode codepoint in UTF-8
    {
        return 0xFFFFFFFF;
    }
}

TOML11_INLINE region non_ascii_key_char::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    const auto first = loc;

    const auto cp = read_utf8(loc);

    if(cp == 0xFFFFFFFF)
    {
        return region{};
    }

    // ALPHA / DIGIT / %x2D / %x5F    ; a-z A-Z 0-9 - _
    // / %xB2 / %xB3 / %xB9 / %xBC-BE ; superscript digits, fractions
    // / %xC0-D6 / %xD8-F6 / %xF8-37D ; non-symbol chars in Latin block
    // / %x37F-1FFF                   ; exclude GREEK QUESTION MARK, which is basically a semi-colon
    // / %x200C-200D / %x203F-2040    ; from General Punctuation Block, include the two tie symbols and ZWNJ, ZWJ
    // / %x2070-218F / %x2460-24FF    ; include super-/subscripts, letterlike/numberlike forms, enclosed alphanumerics
    // / %x2C00-2FEF / %x3001-D7FF    ; skip arrows, math, box drawing etc, skip 2FF0-3000 ideographic up/down markers and spaces
    // / %xF900-FDCF / %xFDF0-FFFD    ; skip D800-DFFF surrogate block, E000-F8FF Private Use area, FDD0-FDEF intended for process-internal use (unicode)
    // / %x10000-EFFFF                ; all chars outside BMP range, excluding Private Use planes (F0000-10FFFF)

    if(cp == 0xB2 || cp == 0xB3 || cp == 0xB9 || (0xBC <= cp && cp <= 0xBE) ||
       (0xC0    <= cp && cp <= 0xD6  ) || (0xD8 <= cp && cp <= 0xF6) || (0xF8 <= cp && cp <= 0x37D) ||
       (0x37F   <= cp && cp <= 0x1FFF) ||
       (0x200C  <= cp && cp <= 0x200D) || (0x203F <= cp && cp <= 0x2040) ||
       (0x2070  <= cp && cp <= 0x218F) || (0x2460 <= cp && cp <= 0x24FF) ||
       (0x2C00  <= cp && cp <= 0x2FEF) || (0x3001 <= cp && cp <= 0xD7FF) ||
       (0xF900  <= cp && cp <= 0xFDCF) || (0xFDF0 <= cp && cp <= 0xFFFD) ||
       (0x10000 <= cp && cp <= 0xEFFFF) )
    {
        return region(first, loc);
    }
    loc = first;
    return region{};
}

TOML11_INLINE repeat_at_least const& unquoted_key(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        const auto keychar = [&s] {
            if(s.ext_allow_non_english_in_bare_keys)
            {
                return either(alpha(s), digit(s), character{0x2D}, character{0x5F},
                              non_ascii_key_char(s));
            }
            else
            {
                return either(alpha(s), digit(s), character{0x2D}, character{0x5F});
            }
        };
        return repeat_at_least(1, keychar());
    });
    return cache.at(sp);
}

TOML11_INLINE either const& quoted_key(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
    return either(basic_string(s), literal_string(s));
    });
    return cache.at(sp);
}

TOML11_INLINE either const& simple_key(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return either(unquoted_key(s), quoted_key(s));
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& dot_sep(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(ws(s), character('.'), ws(s));
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& dotted_key(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(
            simple_key(s),
            repeat_at_least(1, sequence(dot_sep(s), simple_key(s)))
        );
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& keyval_sep(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(ws(s), character('='), ws(s));
    });
    return cache.at(sp);
}

// ===========================================================================
// Table key

TOML11_INLINE sequence const& std_table(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(character('['), ws(s), key(s), ws(s), character(']'));
    });
    return cache.at(sp);
}

TOML11_INLINE sequence const& array_table(const spec& sp)
{
    static thread_local auto cache = make_cache([](const spec& s) {
        return sequence(literal("[["), ws(s), key(s), ws(s), literal("]]"));
    });
    return cache.at(sp);
}

// ===========================================================================
// extension: null

TOML11_INLINE literal const& null_value(const spec&)
{
    static thread_local literal cache("null");
    return cache;
}

} // namespace syntax
} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_SYNTAX_IMPL_HPP
#endif

#endif// TOML11_SYNTAX_HPP
#ifndef TOML11_COMMENTS_HPP
#define TOML11_COMMENTS_HPP

#ifndef TOML11_COMMENTS_FWD_HPP
#define TOML11_COMMENTS_FWD_HPP

// to use __has_builtin

#include <exception>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <ostream>

// This file provides mainly two classes, `preserve_comments` and `discard_comments`.
// Those two are a container that have the same interface as `std::vector<std::string>`
// but bahaves in the opposite way. `preserve_comments` is just the same as
// `std::vector<std::string>` and each `std::string` corresponds to a comment line.
// Conversely, `discard_comments` discards all the strings and ignores everything
// assigned in it. `discard_comments` is always empty and you will encounter an
// error whenever you access to the element.
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
class discard_comments; // forward decl

class preserve_comments
{
  public:
    // `container_type` is not provided in discard_comments.
    // do not use this inner-type in a generic code.
    using container_type         = std::vector<std::string>;

    using size_type              = container_type::size_type;
    using difference_type        = container_type::difference_type;
    using value_type             = container_type::value_type;
    using reference              = container_type::reference;
    using const_reference        = container_type::const_reference;
    using pointer                = container_type::pointer;
    using const_pointer          = container_type::const_pointer;
    using iterator               = container_type::iterator;
    using const_iterator         = container_type::const_iterator;
    using reverse_iterator       = container_type::reverse_iterator;
    using const_reverse_iterator = container_type::const_reverse_iterator;

  public:

    preserve_comments()  = default;
    ~preserve_comments() = default;
    preserve_comments(preserve_comments const&) = default;
    preserve_comments(preserve_comments &&)     = default;
    preserve_comments& operator=(preserve_comments const&) = default;
    preserve_comments& operator=(preserve_comments &&)     = default;

    explicit preserve_comments(const std::vector<std::string>& c): comments(c){}
    explicit preserve_comments(std::vector<std::string>&& c)
        : comments(std::move(c))
    {}
    preserve_comments& operator=(const std::vector<std::string>& c)
    {
        comments = c;
        return *this;
    }
    preserve_comments& operator=(std::vector<std::string>&& c)
    {
        comments = std::move(c);
        return *this;
    }

    explicit preserve_comments(const discard_comments&) {}

    explicit preserve_comments(size_type n): comments(n) {}
    preserve_comments(size_type n, const std::string& x): comments(n, x) {}
    preserve_comments(std::initializer_list<std::string> x): comments(x) {}
    template<typename InputIterator>
    preserve_comments(InputIterator first, InputIterator last)
        : comments(first, last)
    {}

    template<typename InputIterator>
    void assign(InputIterator first, InputIterator last) {comments.assign(first, last);}
    void assign(std::initializer_list<std::string> ini)  {comments.assign(ini);}
    void assign(size_type n, const std::string& val)     {comments.assign(n, val);}

    // Related to the issue #97.
    //
    // `std::vector::insert` and `std::vector::erase` in the STL implementation
    // included in GCC 4.8.5 takes `std::vector::iterator` instead of
    // `std::vector::const_iterator`. It causes compilation error in GCC 4.8.5.
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__) && !defined(__clang__)
#  if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) <= 40805
#    define TOML11_WORKAROUND_GCC_4_8_X_STANDARD_LIBRARY_IMPLEMENTATION
#  endif
#endif

#ifdef TOML11_WORKAROUND_GCC_4_8_X_STANDARD_LIBRARY_IMPLEMENTATION
    iterator insert(iterator p, const std::string& x)
    {
        return comments.insert(p, x);
    }
    iterator insert(iterator p, std::string&&      x)
    {
        return comments.insert(p, std::move(x));
    }
    void insert(iterator p, size_type n, const std::string& x)
    {
        return comments.insert(p, n, x);
    }
    template<typename InputIterator>
    void insert(iterator p, InputIterator first, InputIterator last)
    {
        return comments.insert(p, first, last);
    }
    void insert(iterator p, std::initializer_list<std::string> ini)
    {
        return comments.insert(p, ini);
    }

    template<typename ... Ts>
    iterator emplace(iterator p, Ts&& ... args)
    {
        return comments.emplace(p, std::forward<Ts>(args)...);
    }

    iterator erase(iterator pos) {return comments.erase(pos);}
    iterator erase(iterator first, iterator last)
    {
        return comments.erase(first, last);
    }
#else
    iterator insert(const_iterator p, const std::string& x)
    {
        return comments.insert(p, x);
    }
    iterator insert(const_iterator p, std::string&&      x)
    {
        return comments.insert(p, std::move(x));
    }
    iterator insert(const_iterator p, size_type n, const std::string& x)
    {
        return comments.insert(p, n, x);
    }
    template<typename InputIterator>
    iterator insert(const_iterator p, InputIterator first, InputIterator last)
    {
        return comments.insert(p, first, last);
    }
    iterator insert(const_iterator p, std::initializer_list<std::string> ini)
    {
        return comments.insert(p, ini);
    }

    template<typename ... Ts>
    iterator emplace(const_iterator p, Ts&& ... args)
    {
        return comments.emplace(p, std::forward<Ts>(args)...);
    }

    iterator erase(const_iterator pos) {return comments.erase(pos);}
    iterator erase(const_iterator first, const_iterator last)
    {
        return comments.erase(first, last);
    }
#endif

    void swap(preserve_comments& other) {comments.swap(other.comments);}

    void push_back(const std::string& v) {comments.push_back(v);}
    void push_back(std::string&&      v) {comments.push_back(std::move(v));}
    void pop_back()                      {comments.pop_back();}

    template<typename ... Ts>
    void emplace_back(Ts&& ... args) {comments.emplace_back(std::forward<Ts>(args)...);}

    void clear() {comments.clear();}

    size_type size()     const noexcept {return comments.size();}
    size_type max_size() const noexcept {return comments.max_size();}
    size_type capacity() const noexcept {return comments.capacity();}
    bool      empty()    const noexcept {return comments.empty();}

    void reserve(size_type n)                      {comments.reserve(n);}
    void resize(size_type n)                       {comments.resize(n);}
    void resize(size_type n, const std::string& c) {comments.resize(n, c);}
    void shrink_to_fit()                           {comments.shrink_to_fit();}

    reference       operator[](const size_type n)       noexcept {return comments[n];}
    const_reference operator[](const size_type n) const noexcept {return comments[n];}
    reference       at(const size_type n)       {return comments.at(n);}
    const_reference at(const size_type n) const {return comments.at(n);}
    reference       front()       noexcept {return comments.front();}
    const_reference front() const noexcept {return comments.front();}
    reference       back()        noexcept {return comments.back();}
    const_reference back()  const noexcept {return comments.back();}

    pointer         data()        noexcept {return comments.data();}
    const_pointer   data()  const noexcept {return comments.data();}

    iterator       begin()        noexcept {return comments.begin();}
    iterator       end()          noexcept {return comments.end();}
    const_iterator begin()  const noexcept {return comments.begin();}
    const_iterator end()    const noexcept {return comments.end();}
    const_iterator cbegin() const noexcept {return comments.cbegin();}
    const_iterator cend()   const noexcept {return comments.cend();}

    reverse_iterator       rbegin()        noexcept {return comments.rbegin();}
    reverse_iterator       rend()          noexcept {return comments.rend();}
    const_reverse_iterator rbegin()  const noexcept {return comments.rbegin();}
    const_reverse_iterator rend()    const noexcept {return comments.rend();}
    const_reverse_iterator crbegin() const noexcept {return comments.crbegin();}
    const_reverse_iterator crend()   const noexcept {return comments.crend();}

    friend bool operator==(const preserve_comments&, const preserve_comments&);
    friend bool operator!=(const preserve_comments&, const preserve_comments&);
    friend bool operator< (const preserve_comments&, const preserve_comments&);
    friend bool operator<=(const preserve_comments&, const preserve_comments&);
    friend bool operator> (const preserve_comments&, const preserve_comments&);
    friend bool operator>=(const preserve_comments&, const preserve_comments&);

    friend void swap(preserve_comments&, std::vector<std::string>&);
    friend void swap(std::vector<std::string>&, preserve_comments&);

  private:

    container_type comments;
};

bool operator==(const preserve_comments& lhs, const preserve_comments& rhs);
bool operator!=(const preserve_comments& lhs, const preserve_comments& rhs);
bool operator< (const preserve_comments& lhs, const preserve_comments& rhs);
bool operator<=(const preserve_comments& lhs, const preserve_comments& rhs);
bool operator> (const preserve_comments& lhs, const preserve_comments& rhs);
bool operator>=(const preserve_comments& lhs, const preserve_comments& rhs);

void swap(preserve_comments& lhs, preserve_comments& rhs);
void swap(preserve_comments& lhs, std::vector<std::string>& rhs);
void swap(std::vector<std::string>& lhs, preserve_comments& rhs);

std::ostream& operator<<(std::ostream& os, const preserve_comments& com);

namespace detail
{

// To provide the same interface with `preserve_comments`, `discard_comments`
// should have an iterator. But it does not contain anything, so we need to
// add an iterator that points nothing.
//
// It always points null, so DO NOT unwrap this iterator. It always crashes
// your program.
template<typename T, bool is_const>
struct empty_iterator
{
    using value_type        = T;
    using reference_type    = typename std::conditional<is_const, T const&, T&>::type;
    using pointer_type      = typename std::conditional<is_const, T const*, T*>::type;
    using difference_type   = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    empty_iterator()  = default;
    ~empty_iterator() = default;
    empty_iterator(empty_iterator const&) = default;
    empty_iterator(empty_iterator &&)     = default;
    empty_iterator& operator=(empty_iterator const&) = default;
    empty_iterator& operator=(empty_iterator &&)     = default;

    // DO NOT call these operators.
    reference_type operator*()  const noexcept {std::terminate();}
    pointer_type   operator->() const noexcept {return nullptr;}
    reference_type operator[](difference_type) const noexcept {return this->operator*();}

    // These operators do nothing.
    empty_iterator& operator++()    noexcept {return *this;}
    empty_iterator  operator++(int) noexcept {return *this;}
    empty_iterator& operator--()    noexcept {return *this;}
    empty_iterator  operator--(int) noexcept {return *this;}

    empty_iterator& operator+=(difference_type) noexcept {return *this;}
    empty_iterator& operator-=(difference_type) noexcept {return *this;}

    empty_iterator  operator+(difference_type) const noexcept {return *this;}
    empty_iterator  operator-(difference_type) const noexcept {return *this;}
};

template<typename T, bool C>
bool operator==(const empty_iterator<T, C>&, const empty_iterator<T, C>&) noexcept {return true;}
template<typename T, bool C>
bool operator!=(const empty_iterator<T, C>&, const empty_iterator<T, C>&) noexcept {return false;}
template<typename T, bool C>
bool operator< (const empty_iterator<T, C>&, const empty_iterator<T, C>&) noexcept {return false;}
template<typename T, bool C>
bool operator<=(const empty_iterator<T, C>&, const empty_iterator<T, C>&) noexcept {return true;}
template<typename T, bool C>
bool operator> (const empty_iterator<T, C>&, const empty_iterator<T, C>&) noexcept {return false;}
template<typename T, bool C>
bool operator>=(const empty_iterator<T, C>&, const empty_iterator<T, C>&) noexcept {return true;}

template<typename T, bool C>
typename empty_iterator<T, C>::difference_type
operator-(const empty_iterator<T, C>&, const empty_iterator<T, C>&) noexcept {return 0;}

template<typename T, bool C>
empty_iterator<T, C>
operator+(typename empty_iterator<T, C>::difference_type, const empty_iterator<T, C>& rhs) noexcept {return rhs;}
template<typename T, bool C>
empty_iterator<T, C>
operator+(const empty_iterator<T, C>& lhs, typename empty_iterator<T, C>::difference_type) noexcept {return lhs;}

} // detail

// The default comment type. It discards all the comments. It requires only one
// byte to contain, so the memory footprint is smaller than preserve_comments.
//
// It just ignores `push_back`, `insert`, `erase`, and any other modifications.
// IT always returns size() == 0, the iterator taken by `begin()` is always the
// same as that of `end()`, and accessing through `operator[]` or iterators
// always causes a segmentation fault. DO NOT access to the element of this.
//
// Why this is chose as the default type is because the last version (2.x.y)
// does not contain any comments in a value. To minimize the impact on the
// efficiency, this is chosen as a default.
//
// To reduce the memory footprint, later we can try empty base optimization (EBO).
class discard_comments
{
  public:
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using value_type             = std::string;
    using reference              = std::string&;
    using const_reference        = std::string const&;
    using pointer                = std::string*;
    using const_pointer          = std::string const*;
    using iterator               = detail::empty_iterator<std::string, false>;
    using const_iterator         = detail::empty_iterator<std::string, true>;
    using reverse_iterator       = detail::empty_iterator<std::string, false>;
    using const_reverse_iterator = detail::empty_iterator<std::string, true>;

  public:
    discard_comments() = default;
    ~discard_comments() = default;
    discard_comments(discard_comments const&) = default;
    discard_comments(discard_comments &&)     = default;
    discard_comments& operator=(discard_comments const&) = default;
    discard_comments& operator=(discard_comments &&)     = default;

    explicit discard_comments(const std::vector<std::string>&) noexcept {}
    explicit discard_comments(std::vector<std::string>&&)      noexcept {}
    discard_comments& operator=(const std::vector<std::string>&) noexcept {return *this;}
    discard_comments& operator=(std::vector<std::string>&&)      noexcept {return *this;}

    explicit discard_comments(const preserve_comments&)        noexcept {}

    explicit discard_comments(size_type) noexcept {}
    discard_comments(size_type, const std::string&) noexcept {}
    discard_comments(std::initializer_list<std::string>) noexcept {}
    template<typename InputIterator>
    discard_comments(InputIterator, InputIterator) noexcept {}

    template<typename InputIterator>
    void assign(InputIterator, InputIterator)       noexcept {}
    void assign(std::initializer_list<std::string>) noexcept {}
    void assign(size_type, const std::string&)      noexcept {}

    iterator insert(const_iterator, const std::string&)                 {return iterator{};}
    iterator insert(const_iterator, std::string&&)                      {return iterator{};}
    iterator insert(const_iterator, size_type, const std::string&)      {return iterator{};}
    template<typename InputIterator>
    iterator insert(const_iterator, InputIterator, InputIterator)       {return iterator{};}
    iterator insert(const_iterator, std::initializer_list<std::string>) {return iterator{};}

    template<typename ... Ts>
    iterator emplace(const_iterator, Ts&& ...)     {return iterator{};}
    iterator erase(const_iterator)                 {return iterator{};}
    iterator erase(const_iterator, const_iterator) {return iterator{};}

    void swap(discard_comments&) {return;}

    void push_back(const std::string&) {return;}
    void push_back(std::string&&     ) {return;}
    void pop_back()                    {return;}

    template<typename ... Ts>
    void emplace_back(Ts&& ...) {return;}

    void clear() {return;}

    size_type size()     const noexcept {return 0;}
    size_type max_size() const noexcept {return 0;}
    size_type capacity() const noexcept {return 0;}
    bool      empty()    const noexcept {return true;}

    void reserve(size_type)                    {return;}
    void resize(size_type)                     {return;}
    void resize(size_type, const std::string&) {return;}
    void shrink_to_fit()                       {return;}

    // DO NOT access to the element of this container. This container is always
    // empty, so accessing through operator[], front/back, data causes address
    // error.

    reference       operator[](const size_type)       noexcept {never_call("toml::discard_comment::operator[]");}
    const_reference operator[](const size_type) const noexcept {never_call("toml::discard_comment::operator[]");}
    reference       at(const size_type)       {throw std::out_of_range("toml::discard_comment is always empty.");}
    const_reference at(const size_type) const {throw std::out_of_range("toml::discard_comment is always empty.");}
    reference       front()       noexcept {never_call("toml::discard_comment::front");}
    const_reference front() const noexcept {never_call("toml::discard_comment::front");}
    reference       back()        noexcept {never_call("toml::discard_comment::back");}
    const_reference back()  const noexcept {never_call("toml::discard_comment::back");}

    pointer         data()        noexcept {return nullptr;}
    const_pointer   data()  const noexcept {return nullptr;}

    iterator       begin()        noexcept {return iterator{};}
    iterator       end()          noexcept {return iterator{};}
    const_iterator begin()  const noexcept {return const_iterator{};}
    const_iterator end()    const noexcept {return const_iterator{};}
    const_iterator cbegin() const noexcept {return const_iterator{};}
    const_iterator cend()   const noexcept {return const_iterator{};}

    reverse_iterator       rbegin()        noexcept {return iterator{};}
    reverse_iterator       rend()          noexcept {return iterator{};}
    const_reverse_iterator rbegin()  const noexcept {return const_iterator{};}
    const_reverse_iterator rend()    const noexcept {return const_iterator{};}
    const_reverse_iterator crbegin() const noexcept {return const_iterator{};}
    const_reverse_iterator crend()   const noexcept {return const_iterator{};}

  private:

    [[noreturn]] static void never_call(const char *const this_function)
    {
#if __has_builtin(__builtin_unreachable)
        __builtin_unreachable();
#endif
        throw std::logic_error{this_function};
    }
};

inline bool operator==(const discard_comments&, const discard_comments&) noexcept {return true;}
inline bool operator!=(const discard_comments&, const discard_comments&) noexcept {return false;}
inline bool operator< (const discard_comments&, const discard_comments&) noexcept {return false;}
inline bool operator<=(const discard_comments&, const discard_comments&) noexcept {return true;}
inline bool operator> (const discard_comments&, const discard_comments&) noexcept {return false;}
inline bool operator>=(const discard_comments&, const discard_comments&) noexcept {return true;}

inline void swap(const discard_comments&, const discard_comments&) noexcept {return;}

inline std::ostream& operator<<(std::ostream& os, const discard_comments&) {return os;}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml11
#endif // TOML11_COMMENTS_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_COMMENTS_IMPL_HPP
#define TOML11_COMMENTS_IMPL_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

TOML11_INLINE bool operator==(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments == rhs.comments;}
TOML11_INLINE bool operator!=(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments != rhs.comments;}
TOML11_INLINE bool operator< (const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments <  rhs.comments;}
TOML11_INLINE bool operator<=(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments <= rhs.comments;}
TOML11_INLINE bool operator> (const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments >  rhs.comments;}
TOML11_INLINE bool operator>=(const preserve_comments& lhs, const preserve_comments& rhs) {return lhs.comments >= rhs.comments;}

TOML11_INLINE void swap(preserve_comments& lhs, preserve_comments& rhs)
{
    lhs.swap(rhs);
    return;
}
TOML11_INLINE void swap(preserve_comments& lhs, std::vector<std::string>& rhs)
{
    lhs.comments.swap(rhs);
    return;
}
TOML11_INLINE void swap(std::vector<std::string>& lhs, preserve_comments& rhs)
{
    lhs.swap(rhs.comments);
    return;
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const preserve_comments& com)
{
    for(const auto& c : com)
    {
        if(c.front() != '#')
        {
            os << '#';
        }
        os << c << '\n';
    }
    return os;
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml11
#endif // TOML11_COMMENTS_IMPL_HPP
#endif

#endif // TOML11_COMMENTS_HPP
#ifndef TOML11_COLOR_HPP
#define TOML11_COLOR_HPP

#ifndef TOML11_COLOR_FWD_HPP
#define TOML11_COLOR_FWD_HPP

#include <iosfwd>


#ifdef TOML11_COLORIZE_ERROR_MESSAGE
#define TOML11_ERROR_MESSAGE_COLORIZED true
#else
#define TOML11_ERROR_MESSAGE_COLORIZED false
#endif

#ifdef TOML11_USE_THREAD_LOCAL_COLORIZATION
#define TOML11_THREAD_LOCAL_COLORIZATION thread_local
#else
#define TOML11_THREAD_LOCAL_COLORIZATION
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace color
{
// put ANSI escape sequence to ostream
inline namespace ansi
{
namespace detail
{

// Control color mode globally
class color_mode
{
  public:

    void enable() noexcept
    {
        should_color_ = true;
    }
    void disable() noexcept
    {
        should_color_ = false;
    }
    bool should_color() const noexcept
    {
        return should_color_;
    }

  private:

    bool should_color_ = TOML11_ERROR_MESSAGE_COLORIZED;
};

inline color_mode& color_status() noexcept
{
    static TOML11_THREAD_LOCAL_COLORIZATION color_mode status;
    return status;
}

} // detail

std::ostream& reset  (std::ostream& os);
std::ostream& bold   (std::ostream& os);
std::ostream& grey   (std::ostream& os);
std::ostream& gray   (std::ostream& os);
std::ostream& red    (std::ostream& os);
std::ostream& green  (std::ostream& os);
std::ostream& yellow (std::ostream& os);
std::ostream& blue   (std::ostream& os);
std::ostream& magenta(std::ostream& os);
std::ostream& cyan   (std::ostream& os);
std::ostream& white  (std::ostream& os);

} // ansi

inline void enable()
{
    return detail::color_status().enable();
}
inline void disable()
{
    return detail::color_status().disable();
}
inline bool should_color()
{
    return detail::color_status().should_color();
}

} // color
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_COLOR_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_COLOR_IMPL_HPP
#define TOML11_COLOR_IMPL_HPP


#include <ostream>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace color
{
// put ANSI escape sequence to ostream
inline namespace ansi
{

TOML11_INLINE std::ostream& reset(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[00m";}
    return os;
}
TOML11_INLINE std::ostream& bold(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[01m";}
    return os;
}
TOML11_INLINE std::ostream& grey(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[30m";}
    return os;
}
TOML11_INLINE std::ostream& gray(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[30m";}
    return os;
}
TOML11_INLINE std::ostream& red(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[31m";}
    return os;
}
TOML11_INLINE std::ostream& green(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[32m";}
    return os;
}
TOML11_INLINE std::ostream& yellow(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[33m";}
    return os;
}
TOML11_INLINE std::ostream& blue(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[34m";}
    return os;
}
TOML11_INLINE std::ostream& magenta(std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[35m";}
    return os;
}
TOML11_INLINE std::ostream& cyan   (std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[36m";}
    return os;
}
TOML11_INLINE std::ostream& white  (std::ostream& os)
{
    if(detail::color_status().should_color()) {os << "\033[37m";}
    return os;
}

} // ansi
} // color
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_COLOR_IMPL_HPP
#endif

#endif // TOML11_COLOR_HPP
#ifndef TOML11_SOURCE_LOCATION_HPP
#define TOML11_SOURCE_LOCATION_HPP

#ifndef TOML11_SOURCE_LOCATION_FWD_HPP
#define TOML11_SOURCE_LOCATION_FWD_HPP


#include <sstream>
#include <string>
#include <vector>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

//
// A struct to contain location in a toml file.
//
// To reduce memory consumption, it omits unrelated parts of long lines. like:
//
// 1. one long line, short region
// ```
//    |
//  1 | ... "foo", "bar", baz, "qux", "foobar", ...
//    |                   ^-- unknown value
// ```
// 2. long region
// ```
//    |
//  1 | array = [ "foo", ... "bar" ]
//    |         ^^^^^^^^^^^^^^^^^^^^- in this array
// ```
// 3. many lines
//     |
//   1 | array = [ "foo",
//     |         ^^^^^^^^
//     | ...
//     | ^^^
//     |
//  10 | , "bar"]
//     | ^^^^^^^^- in this array
// ```
//
struct source_location
{
  public:

    explicit source_location(const detail::region& r);
    ~source_location() = default;
    source_location(source_location const&) = default;
    source_location(source_location &&)     = default;
    source_location& operator=(source_location const&) = default;
    source_location& operator=(source_location &&)     = default;

    bool is_ok() const noexcept {return this->is_ok_;}
    std::size_t length() const noexcept {return this->length_;}

    std::size_t first_line_number()   const noexcept {return this->first_line_;}
    std::size_t first_column_number() const noexcept {return this->first_column_;}
    std::size_t last_line_number()    const noexcept {return this->last_line_;}
    std::size_t last_column_number()  const noexcept {return this->last_column_;}

    std::string const& file_name()  const noexcept {return this->file_name_;}

    std::size_t num_lines() const noexcept {return this->line_str_.size();}

    std::string const& first_line() const;
    std::string const& last_line() const;

    std::vector<std::string> const& lines() const noexcept {return line_str_;}

    // for internal use
    std::size_t first_column_offset() const noexcept {return this->first_offset_;}
    std::size_t last_column_offset()  const noexcept {return this->last_offset_;}

  private:

    bool        is_ok_;
    std::size_t first_line_;
    std::size_t first_column_; // column num in the actual file
    std::size_t first_offset_; // column num in the shown line
    std::size_t last_line_;
    std::size_t last_column_;  // column num in the actual file
    std::size_t last_offset_;  // column num in the shown line
    std::size_t length_;
    std::string file_name_;
    std::vector<std::string> line_str_;
};

namespace detail
{

std::size_t integer_width_base10(std::size_t i) noexcept;

inline std::size_t line_width() noexcept {return 0;}

template<typename ... Ts>
std::size_t line_width(const source_location& loc, const std::string& /*msg*/,
        const Ts& ... tail) noexcept
{
    return (std::max)(
            integer_width_base10(loc.last_line_number()), line_width(tail...));
}

std::ostringstream&
format_filename(std::ostringstream& oss, const source_location& loc);

std::ostringstream&
format_empty_line(std::ostringstream& oss, const std::size_t lnw);

std::ostringstream& format_line(std::ostringstream& oss,
    const std::size_t lnw, const std::size_t linenum, const std::string& line);

std::ostringstream& format_underline(std::ostringstream& oss,
        const std::size_t lnw, const std::size_t col, const std::size_t len,
        const std::string& msg);

std::string format_location_impl(const std::size_t lnw,
    const std::string& prev_fname,
    const source_location& loc, const std::string& msg);

inline std::string format_location_rec(const std::size_t, const std::string&)
{
    return "";
}

template<typename ... Ts>
std::string format_location_rec(const std::size_t lnw,
        const std::string& prev_fname,
        const source_location& loc, const std::string& msg,
        const Ts& ... tail)
{
    return format_location_impl(lnw, prev_fname, loc, msg) +
           format_location_rec(lnw, loc.file_name(), tail...);
}

} // namespace detail

// format a location info without title
template<typename ... Ts>
std::string format_location(
        const source_location& loc, const std::string& msg, const Ts& ... tail)
{
    const auto lnw = detail::line_width(loc, msg, tail...);

    const std::string f(""); // at the 1st iteration, no prev_filename is given
    return detail::format_location_rec(lnw, f, loc, msg, tail...);
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_SOURCE_LOCATION_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_SOURCE_LOCATION_IMPL_HPP
#define TOML11_SOURCE_LOCATION_IMPL_HPP



#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <cctype>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

TOML11_INLINE source_location::source_location(const detail::region& r)
    : is_ok_(false),
      first_line_(1),
      first_column_(1),
      first_offset_(1),
      last_line_(1),
      last_column_(1),
      last_offset_(1),
      length_(0),
      file_name_("unknown file")
{
    if(r.is_ok())
    {
        this->is_ok_        = true;
        this->file_name_    = r.source_name();
        this->first_line_   = r.first_line_number();
        this->first_column_ = r.first_column_number();
        this->last_line_    = r.last_line_number();
        this->last_column_  = r.last_column_number();
        this->length_       = r.length();

        const auto lines = r.as_lines();
        assert( ! lines.empty());

        for(const auto& l : lines)
        {
            this->line_str_.push_back(l.first);
        }

        this->first_offset_ = lines.at(             0).second + 1; // to 1-origin
        this->last_offset_  = lines.at(lines.size()-1).second + 1;
    }
}

TOML11_INLINE std::string const& source_location::first_line() const
{
    if(this->line_str_.size() == 0)
    {
        throw std::out_of_range("toml::source_location::first_line: `lines` is empty");
    }
    return this->line_str_.front();
}
TOML11_INLINE std::string const& source_location::last_line() const
{
    if(this->line_str_.size() == 0)
    {
        throw std::out_of_range("toml::source_location::first_line: `lines` is empty");
    }
    return this->line_str_.back();
}

namespace detail
{

TOML11_INLINE std::size_t integer_width_base10(std::size_t i) noexcept
{
    std::size_t width = 0;
    while(i != 0)
    {
        i /= 10;
        width += 1;
    }
    return width;
}

TOML11_INLINE std::ostringstream&
format_filename(std::ostringstream& oss, const source_location& loc)
{
    // --> example.toml
    oss << color::bold << color::blue << " --> " << color::reset
        << color::bold << loc.file_name() << '\n' << color::reset;
    return oss;
}

TOML11_INLINE std::ostringstream& format_empty_line(std::ostringstream& oss,
        const std::size_t lnw)
{
    //    |
    oss << detail::make_string(lnw + 1, ' ')
        << color::bold << color::blue << " |\n"  << color::reset;
    return oss;
}

TOML11_INLINE std::ostringstream& format_line(std::ostringstream& oss,
    const std::size_t lnw, const std::size_t linenum, const std::string& line)
{
    // 10 | key = "value"
    oss << ' ' << color::bold << color::blue
        << std::setw(static_cast<int>(lnw))
        << std::right << linenum << " | "  << color::reset;
    for(const char c : line)
    {
        if(std::isgraph(c) || c == ' ')
        {
            oss << c;
        }
        else
        {
            oss << show_char(c);
        }
    }
    oss << '\n';
    return oss;
}
TOML11_INLINE std::ostringstream& format_underline(std::ostringstream& oss,
        const std::size_t lnw, const std::size_t col, const std::size_t len,
        const std::string& msg)
{
    //    |       ^^^^^^^-- this part
    oss << make_string(lnw + 1, ' ')
        << color::bold << color::blue << " | " << color::reset;

    // in case col is 0, so we don't create a string with size_t max length
    const std::size_t sanitized_col = col == 0 ? 0 : col - 1 /*1-origin*/;
    oss << make_string(sanitized_col, ' ')
        << color::bold << color::red
        << make_string(len, '^') << "-- "
        << color::reset << msg << '\n';

    return oss;
}

TOML11_INLINE std::string format_location_impl(const std::size_t lnw,
    const std::string& prev_fname,
    const source_location& loc, const std::string& msg)
{
    std::ostringstream oss;

    if(loc.file_name() != prev_fname)
    {
        format_filename(oss, loc);
        if( ! loc.lines().empty())
        {
            format_empty_line(oss, lnw);
        }
    }

    if(loc.lines().size() == 1)
    {
        // when column points LF, it exceeds the size of the first line.
        std::size_t underline_limit = 1;
        if(loc.first_line().size() < loc.first_column_offset())
        {
            underline_limit = 1;
        }
        else
        {
            underline_limit = loc.first_line().size() - loc.first_column_offset() + 1;
        }
        const auto underline_len = (std::min)(underline_limit, loc.length());

        format_line(oss, lnw, loc.first_line_number(), loc.first_line());
        format_underline(oss, lnw, loc.first_column_offset(), underline_len, msg);
    }
    else if(loc.lines().size() == 2)
    {
        const auto first_underline_len =
            loc.first_line().size() - loc.first_column_offset() + 1;
        format_line(oss, lnw, loc.first_line_number(), loc.first_line());
        format_underline(oss, lnw, loc.first_column_offset(),
                first_underline_len, "");

        format_line(oss, lnw, loc.last_line_number(), loc.last_line());
        format_underline(oss, lnw, 1, loc.last_column_offset(), msg);
    }
    else if(loc.lines().size() > 2)
    {
        const auto first_underline_len =
            loc.first_line().size() - loc.first_column_offset() + 1;
        format_line(oss, lnw, loc.first_line_number(), loc.first_line());
        format_underline(oss, lnw, loc.first_column_offset(),
                first_underline_len, "and");

        if(loc.lines().size() == 3)
        {
            format_line(oss, lnw, loc.first_line_number()+1, loc.lines().at(1));
            format_underline(oss, lnw, 1, loc.lines().at(1).size(), "and");
        }
        else
        {
            format_line(oss, lnw, loc.first_line_number()+1, " ...");
            format_empty_line(oss, lnw);
        }
        format_line(oss, lnw, loc.last_line_number(), loc.last_line());
        format_underline(oss, lnw, 1, loc.last_column_offset(), msg);
    }
    // if loc is empty, do nothing.
    return oss.str();
}

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_SOURCE_LOCATION_IMPL_HPP
#endif

#endif // TOML11_SOURCE_LOCATION_HPP
#ifndef TOML11_ERROR_INFO_HPP
#define TOML11_ERROR_INFO_HPP

#ifndef TOML11_ERROR_INFO_FWD_HPP
#define TOML11_ERROR_INFO_FWD_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// error info returned from parser.
struct error_info
{
    error_info(std::string t, source_location l, std::string m, std::string s = "")
        : title_(std::move(t)), locations_{std::make_pair(std::move(l), std::move(m))},
          suffix_(std::move(s))
    {}

    error_info(std::string t, std::vector<std::pair<source_location, std::string>> l,
               std::string s = "")
        : title_(std::move(t)), locations_(std::move(l)), suffix_(std::move(s))
    {}

    std::string const& title() const noexcept {return title_;}
    std::string &      title()       noexcept {return title_;}

    std::vector<std::pair<source_location, std::string>> const&
    locations() const noexcept {return locations_;}

    void add_locations(source_location loc, std::string msg) noexcept
    {
        locations_.emplace_back(std::move(loc), std::move(msg));
    }

    std::string const& suffix() const noexcept {return suffix_;}
    std::string &      suffix()       noexcept {return suffix_;}

  private:

    std::string title_;
    std::vector<std::pair<source_location, std::string>> locations_;
    std::string suffix_; // hint or something like that
};

// forward decl
template<typename TypeConfig>
class basic_value;

namespace detail
{
inline error_info make_error_info_rec(error_info e)
{
    return e;
}
inline error_info make_error_info_rec(error_info e, std::string s)
{
    e.suffix() = s;
    return e;
}

template<typename TC, typename ... Ts>
error_info make_error_info_rec(error_info e,
        const basic_value<TC>& v, std::string msg, Ts&& ... tail);

template<typename ... Ts>
error_info make_error_info_rec(error_info e,
        source_location loc, std::string msg, Ts&& ... tail)
{
    e.add_locations(std::move(loc), std::move(msg));
    return make_error_info_rec(std::move(e), std::forward<Ts>(tail)...);
}

} // detail

template<typename ... Ts>
error_info make_error_info(
    std::string title, source_location loc, std::string msg, Ts&& ... tail)
{
    error_info ei(std::move(title), std::move(loc), std::move(msg));
    return detail::make_error_info_rec(ei, std::forward<Ts>(tail) ... );
}

std::string format_error(const std::string& errkind, const error_info& err);
std::string format_error(const error_info& err);

// for custom error message
template<typename ... Ts>
std::string format_error(std::string title,
        source_location loc, std::string msg, Ts&& ... tail)
{
    return format_error("", make_error_info(std::move(title),
                std::move(loc), std::move(msg), std::forward<Ts>(tail)...));
}

std::ostream& operator<<(std::ostream& os, const error_info& e);

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_ERROR_INFO_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_ERROR_INFO_IMPL_HPP
#define TOML11_ERROR_INFO_IMPL_HPP


#include <sstream>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

TOML11_INLINE std::string format_error(const std::string& errkind, const error_info& err)
{
    std::string errmsg;
    if( ! errkind.empty())
    {
        errmsg = errkind;
        errmsg += ' ';
    }
    errmsg += err.title();
    errmsg += '\n';

    const auto lnw = [&err]() {
        std::size_t width = 0;
        for(const auto& l : err.locations())
        {
            width = (std::max)(detail::integer_width_base10(l.first.last_line_number()), width);
        }
        return width;
    }();

    bool first = true;
    std::string prev_fname;
    for(const auto& lm : err.locations())
    {
        if( ! first)
        {
            std::ostringstream oss;
            oss << detail::make_string(lnw + 1, ' ')
                << color::bold << color::blue << " |" << color::reset
                << color::bold << " ...\n" << color::reset;
            oss << detail::make_string(lnw + 1, ' ')
                << color::bold << color::blue << " |\n" << color::reset;
            errmsg += oss.str();
        }

        const auto& l = lm.first;
        const auto& m = lm.second;

        errmsg += detail::format_location_impl(lnw, prev_fname, l, m);

        prev_fname = l.file_name();
        first = false;
    }

    errmsg += err.suffix();

    return errmsg;
}

TOML11_INLINE std::string format_error(const error_info& err)
{
    std::ostringstream oss;
    oss << color::red << color::bold << "[error]" << color::reset;
    return format_error(oss.str(), err);
}

TOML11_INLINE std::ostream& operator<<(std::ostream& os, const error_info& e)
{
    os << format_error(e);
    return os;
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_ERROR_INFO_IMPL_HPP
#endif

#endif // TOML11_ERROR_INFO_HPP
#ifndef TOML11_VALUE_HPP
#define TOML11_VALUE_HPP


#ifdef TOML11_HAS_STRING_VIEW
#include <string_view>
#endif

#ifdef TOML11_ENABLE_ACCESS_CHECK
#include <atomic>
#endif

#include <cassert>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
template<typename TypeConfig>
class basic_value;

struct type_error final : public ::toml::exception
{
  public:
    type_error(std::string what_arg, source_location loc)
        : what_(std::move(what_arg)), loc_(std::move(loc))
    {}
    ~type_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}

    source_location const& location() const noexcept {return loc_;}

  private:
    std::string what_;
    source_location loc_;
};

// only for internal use
namespace detail
{
template<typename TC>
error_info make_type_error(const basic_value<TC>&, const std::string&, const value_t);

template<typename TC>
error_info make_not_found_error(const basic_value<TC>&, const std::string&, const typename basic_value<TC>::key_type&);

template<typename TC>
void change_region_of_value(basic_value<TC>&, const basic_value<TC>&);

template<typename TC, value_t V>
struct getter;

#ifdef TOML11_ENABLE_ACCESS_CHECK
template<typename TC>
void unset_access_flag(basic_value<TC>&);
#endif
} // detail

template<typename TypeConfig>
class basic_value
{
  public:

    using config_type          = TypeConfig;
    using key_type             = typename config_type::string_type;
    using value_type           = basic_value<config_type>;
    using boolean_type         = typename config_type::boolean_type;
    using integer_type         = typename config_type::integer_type;
    using floating_type        = typename config_type::floating_type;
    using string_type          = typename config_type::string_type;
    using local_time_type      = ::toml::local_time;
    using local_date_type      = ::toml::local_date;
    using local_datetime_type  = ::toml::local_datetime;
    using offset_datetime_type = ::toml::offset_datetime;
    using array_type           = typename config_type::template array_type<value_type>;
    using table_type           = typename config_type::template table_type<key_type, value_type>;
    using comment_type         = typename config_type::comment_type;
    using char_type            = typename string_type::value_type;

  private:

    using region_type = detail::region;

  public:

    basic_value() noexcept
        : type_(value_t::empty), empty_('\0'), region_{}, comments_{}
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    ~basic_value() noexcept {this->cleanup();}

    // copy/move constructor/assigner ===================================== {{{

    basic_value(const basic_value& v)
        : type_(v.type_), region_(v.region_), comments_(v.comments_)
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{v.accessed()}
#endif
    {
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , v.boolean_        ); break;
            case value_t::integer        : assigner(integer_        , v.integer_        ); break;
            case value_t::floating       : assigner(floating_       , v.floating_       ); break;
            case value_t::string         : assigner(string_         , v.string_         ); break;
            case value_t::offset_datetime: assigner(offset_datetime_, v.offset_datetime_); break;
            case value_t::local_datetime : assigner(local_datetime_ , v.local_datetime_ ); break;
            case value_t::local_date     : assigner(local_date_     , v.local_date_     ); break;
            case value_t::local_time     : assigner(local_time_     , v.local_time_     ); break;
            case value_t::array          : assigner(array_          , v.array_          ); break;
            case value_t::table          : assigner(table_          , v.table_          ); break;
            default                      : assigner(empty_          , '\0'              ); break;
        }
    }
    basic_value(basic_value&& v)
        : type_(v.type()), region_(std::move(v.region_)),
          comments_(std::move(v.comments_))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{v.accessed()}
#endif
    {
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , std::move(v.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(v.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(v.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(v.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(v.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(v.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(v.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(v.local_time_     )); break;
            case value_t::array          : assigner(array_          , std::move(v.array_          )); break;
            case value_t::table          : assigner(table_          , std::move(v.table_          )); break;
            default                      : assigner(empty_          , '\0'                         ); break;
        }
    }

    basic_value& operator=(const basic_value& v)
    {
        if(this == std::addressof(v)) {return *this;}

        this->cleanup();
        this->type_     = v.type_;
        this->region_   = v.region_;
        this->comments_ = v.comments_;
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = v.accessed();
#endif
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , v.boolean_        ); break;
            case value_t::integer        : assigner(integer_        , v.integer_        ); break;
            case value_t::floating       : assigner(floating_       , v.floating_       ); break;
            case value_t::string         : assigner(string_         , v.string_         ); break;
            case value_t::offset_datetime: assigner(offset_datetime_, v.offset_datetime_); break;
            case value_t::local_datetime : assigner(local_datetime_ , v.local_datetime_ ); break;
            case value_t::local_date     : assigner(local_date_     , v.local_date_     ); break;
            case value_t::local_time     : assigner(local_time_     , v.local_time_     ); break;
            case value_t::array          : assigner(array_          , v.array_          ); break;
            case value_t::table          : assigner(table_          , v.table_          ); break;
            default                      : assigner(empty_          , '\0'              ); break;
        }
        return *this;
    }
    basic_value& operator=(basic_value&& v)
    {
        if(this == std::addressof(v)) {return *this;}

        this->cleanup();
        this->type_     = v.type_;
        this->region_   = std::move(v.region_);
        this->comments_ = std::move(v.comments_);
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = v.accessed();
#endif
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , std::move(v.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(v.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(v.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(v.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(v.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(v.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(v.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(v.local_time_     )); break;
            case value_t::array          : assigner(array_          , std::move(v.array_          )); break;
            case value_t::table          : assigner(table_          , std::move(v.table_          )); break;
            default                      : assigner(empty_          , '\0'                         ); break;
        }
        return *this;
    }
    // }}}

    // constructor to overwrite commnets ================================== {{{

    basic_value(basic_value v, std::vector<std::string> com)
        : type_(v.type()), region_(std::move(v.region_)),
          comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{v.accessed()}
#endif
    {
        switch(this->type_)
        {
            case value_t::boolean        : assigner(boolean_        , std::move(v.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(v.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(v.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(v.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(v.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(v.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(v.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(v.local_time_     )); break;
            case value_t::array          : assigner(array_          , std::move(v.array_          )); break;
            case value_t::table          : assigner(table_          , std::move(v.table_          )); break;
            default                      : assigner(empty_          , '\0'                         ); break;
        }
    }
    // }}}

    // conversion between different basic_values ========================== {{{

    template<typename TI>
    basic_value(basic_value<TI> other)
        : type_(other.type_),
          region_(std::move(other.region_)),
          comments_(std::move(other.comments_))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{other.accessed()}
#endif
    {
        switch(other.type_)
        {
            // use auto-convert in constructor
            case value_t::boolean        : assigner(boolean_        , std::move(other.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(other.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(other.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(other.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(other.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(other.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(other.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(other.local_time_     )); break;

            // may have different container type
            case value_t::array          :
            {
                array_type tmp(
                    std::make_move_iterator(other.array_.value.get().begin()),
                    std::make_move_iterator(other.array_.value.get().end()));
                assigner(array_, array_storage(
                        detail::storage<array_type>(std::move(tmp)),
                        other.array_.format
                    ));
                break;
            }
            case value_t::table          :
            {
                table_type tmp(
                    std::make_move_iterator(other.table_.value.get().begin()),
                    std::make_move_iterator(other.table_.value.get().end()));
                assigner(table_, table_storage(
                        detail::storage<table_type>(std::move(tmp)),
                        other.table_.format
                    ));
                break;
            }
            default: break;
        }
    }

    template<typename TI>
    basic_value(basic_value<TI> other, std::vector<std::string> com)
        : type_(other.type_),
          region_(std::move(other.region_)),
          comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{other.accessed()}
#endif
    {
        switch(other.type_)
        {
            // use auto-convert in constructor
            case value_t::boolean        : assigner(boolean_        , std::move(other.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(other.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(other.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(other.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(other.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(other.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(other.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(other.local_time_     )); break;

            // may have different container type
            case value_t::array          :
            {
                array_type tmp(
                    std::make_move_iterator(other.array_.value.get().begin()),
                    std::make_move_iterator(other.array_.value.get().end()));
                assigner(array_, array_storage(
                        detail::storage<array_type>(std::move(tmp)),
                        other.array_.format
                    ));
                break;
            }
            case value_t::table          :
            {
                table_type tmp(
                    std::make_move_iterator(other.table_.value.get().begin()),
                    std::make_move_iterator(other.table_.value.get().end()));
                assigner(table_, table_storage(
                        detail::storage<table_type>(std::move(tmp)),
                        other.table_.format
                    ));
                break;
            }
            default: break;
        }
    }
    template<typename TI>
    basic_value& operator=(basic_value<TI> other)
    {
        this->cleanup();
        this->region_ = other.region_;
        this->comments_    = comment_type(other.comments_);
        this->type_        = other.type_;
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = other.accessed();
#endif

        switch(other.type_)
        {
            // use auto-convert in constructor
            case value_t::boolean        : assigner(boolean_        , std::move(other.boolean_        )); break;
            case value_t::integer        : assigner(integer_        , std::move(other.integer_        )); break;
            case value_t::floating       : assigner(floating_       , std::move(other.floating_       )); break;
            case value_t::string         : assigner(string_         , std::move(other.string_         )); break;
            case value_t::offset_datetime: assigner(offset_datetime_, std::move(other.offset_datetime_)); break;
            case value_t::local_datetime : assigner(local_datetime_ , std::move(other.local_datetime_ )); break;
            case value_t::local_date     : assigner(local_date_     , std::move(other.local_date_     )); break;
            case value_t::local_time     : assigner(local_time_     , std::move(other.local_time_     )); break;

            // may have different container type
            case value_t::array          :
            {
                array_type tmp(
                    std::make_move_iterator(other.array_.value.get().begin()),
                    std::make_move_iterator(other.array_.value.get().end()));
                assigner(array_, array_storage(
                        detail::storage<array_type>(std::move(tmp)),
                        other.array_.format
                    ));
                break;
            }
            case value_t::table          :
            {
                table_type tmp(
                    std::make_move_iterator(other.table_.value.get().begin()),
                    std::make_move_iterator(other.table_.value.get().end()));
                assigner(table_, table_storage(
                        detail::storage<table_type>(std::move(tmp)),
                        other.table_.format
                    ));
                break;
            }
            default: break;
        }
        return *this;
    }
    // }}}

    // constructor (boolean) ============================================== {{{

    basic_value(boolean_type x)
        : basic_value(x, boolean_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(boolean_type x, boolean_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(boolean_type x, std::vector<std::string> com)
        : basic_value(x, boolean_format_info{}, std::move(com), region_type{})
    {}
    basic_value(boolean_type x, boolean_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(boolean_type x, boolean_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::boolean), boolean_(boolean_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(boolean_type x)
    {
        boolean_format_info fmt;
        if(this->is_boolean())
        {
            fmt = this->as_boolean_fmt();
        }
        this->cleanup();
        this->type_   = value_t::boolean;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->boolean_, boolean_storage(x, fmt));
        return *this;
    }

    // }}}

    // constructor (integer) ============================================== {{{

    basic_value(integer_type x)
        : basic_value(std::move(x), integer_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(integer_type x, integer_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(integer_type x, std::vector<std::string> com)
        : basic_value(std::move(x), integer_format_info{}, std::move(com), region_type{})
    {}
    basic_value(integer_type x, integer_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(integer_type x, integer_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::integer), integer_(integer_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(integer_type x)
    {
        integer_format_info fmt;
        if(this->is_integer())
        {
            fmt = this->as_integer_fmt();
        }
        this->cleanup();
        this->type_   = value_t::integer;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->integer_, integer_storage(std::move(x), std::move(fmt)));
        return *this;
    }

  private:

    template<typename T>
    using enable_if_integer_like_t = cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, boolean_type>>,
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, integer_type>>,
            std::is_integral<cxx::remove_cvref_t<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(std::move(x), integer_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, integer_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(std::move(x), integer_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, integer_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value(T x, integer_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::integer), integer_(integer_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    template<typename T, enable_if_integer_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        integer_format_info fmt;
        if(this->is_integer())
        {
            fmt = this->as_integer_fmt();
        }
        this->cleanup();
        this->type_   = value_t::integer;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->integer_, integer_storage(x, std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (floating) ============================================= {{{

    basic_value(floating_type x)
        : basic_value(std::move(x), floating_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(floating_type x, floating_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(floating_type x, std::vector<std::string> com)
        : basic_value(std::move(x), floating_format_info{}, std::move(com), region_type{})
    {}
    basic_value(floating_type x, floating_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(floating_type x, floating_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::floating), floating_(floating_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(floating_type x)
    {
        floating_format_info fmt;
        if(this->is_floating())
        {
            fmt = this->as_floating_fmt();
        }
        this->cleanup();
        this->type_   = value_t::floating;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->floating_, floating_storage(std::move(x), std::move(fmt)));
        return *this;
    }

  private:

    template<typename T>
    using enable_if_floating_like_t = cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, floating_type>>,
            std::is_floating_point<cxx::remove_cvref_t<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(x, floating_format_info{}, std::vector<std::string>{}, region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, floating_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(x, floating_format_info{}, std::move(com), region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, floating_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value(T x, floating_format_info fmt, std::vector<std::string> com, region_type reg)
        : type_(value_t::floating), floating_(floating_storage(x, std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}

    template<typename T, enable_if_floating_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        floating_format_info fmt;
        if(this->is_floating())
        {
            fmt = this->as_floating_fmt();
        }
        this->cleanup();
        this->type_   = value_t::floating;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->floating_, floating_storage(x, std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (string) =============================================== {{{

    basic_value(string_type x)
        : basic_value(std::move(x), string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_type x, string_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_type x, std::vector<std::string> com)
        : basic_value(std::move(x), string_format_info{}, std::move(com), region_type{})
    {}
    basic_value(string_type x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(string_type x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string), string_(string_storage(std::move(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(string_type x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->string_, string_storage(x, std::move(fmt)));
        return *this;
    }

    // "string literal"

    basic_value(const typename string_type::value_type* x)
        : basic_value(x, string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(const typename string_type::value_type* x, string_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(const typename string_type::value_type* x, std::vector<std::string> com)
        : basic_value(x, string_format_info{}, std::move(com), region_type{})
    {}
    basic_value(const typename string_type::value_type* x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(const typename string_type::value_type* x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string), string_(string_storage(string_type(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(const typename string_type::value_type* x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->string_, string_storage(string_type(x), std::move(fmt)));
        return *this;
    }

#if defined(TOML11_HAS_STRING_VIEW)
    using string_view_type = std::basic_string_view<
        typename string_type::value_type, typename string_type::traits_type>;

    basic_value(string_view_type x)
        : basic_value(x, string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_view_type x, string_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(string_view_type x, std::vector<std::string> com)
        : basic_value(x, string_format_info{}, std::move(com), region_type{})
    {}
    basic_value(string_view_type x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}
    basic_value(string_view_type x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string), string_(string_storage(string_type(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(string_view_type x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->string_, string_storage(string_type(x), std::move(fmt)));
        return *this;
    }

#endif // TOML11_HAS_STRING_VIEW

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x)
        : basic_value(x, string_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, string_format_info fmt)
        : basic_value(x, std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, std::vector<std::string> com)
        : basic_value(x, string_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, string_format_info fmt, std::vector<std::string> com)
        : basic_value(x, std::move(fmt), std::move(com), region_type{})
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& x, string_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::string),
          string_(string_storage(detail::string_conv<string_type>(x), std::move(fmt))),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<cxx::remove_cvref_t<T>, string_type>>,
            detail::is_1byte_std_basic_string<T>
        >::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& x)
    {
        string_format_info fmt;
        if(this->is_string())
        {
            fmt = this->as_string_fmt();
        }
        this->cleanup();
        this->type_   = value_t::string;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->string_, string_storage(detail::string_conv<string_type>(x), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (local_date) =========================================== {{{

    basic_value(local_date_type x)
        : basic_value(x, local_date_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_date_type x, local_date_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_date_type x, std::vector<std::string> com)
        : basic_value(x, local_date_format_info{}, std::move(com), region_type{})
    {}
    basic_value(local_date_type x, local_date_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(local_date_type x, local_date_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::local_date), local_date_(local_date_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(local_date_type x)
    {
        local_date_format_info fmt;
        if(this->is_local_date())
        {
            fmt = this->as_local_date_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_date;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->local_date_, local_date_storage(x, fmt));
        return *this;
    }

    // }}}

    // constructor (local_time) =========================================== {{{

    basic_value(local_time_type x)
        : basic_value(x, local_time_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_time_type x, local_time_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_time_type x, std::vector<std::string> com)
        : basic_value(x, local_time_format_info{}, std::move(com), region_type{})
    {}
    basic_value(local_time_type x, local_time_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(local_time_type x, local_time_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::local_time), local_time_(local_time_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(local_time_type x)
    {
        local_time_format_info fmt;
        if(this->is_local_time())
        {
            fmt = this->as_local_time_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_time;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->local_time_, local_time_storage(x, fmt));
        return *this;
    }

    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x)
        : basic_value(local_time_type(x), local_time_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x, local_time_format_info fmt)
        : basic_value(local_time_type(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x, std::vector<std::string> com)
        : basic_value(local_time_type(x), local_time_format_info{}, std::move(com), region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x, local_time_format_info fmt, std::vector<std::string> com)
        : basic_value(local_time_type(x), std::move(fmt), std::move(com), region_type{})
    {}
    template<typename Rep, typename Period>
    basic_value(const std::chrono::duration<Rep, Period>& x,
                local_time_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : basic_value(local_time_type(x), std::move(fmt), std::move(com), std::move(reg))
    {}
    template<typename Rep, typename Period>
    basic_value& operator=(const std::chrono::duration<Rep, Period>& x)
    {
        local_time_format_info fmt;
        if(this->is_local_time())
        {
            fmt = this->as_local_time_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_time;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->local_time_, local_time_storage(local_time_type(x), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (local_datetime) =========================================== {{{

    basic_value(local_datetime_type x)
        : basic_value(x, local_datetime_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_datetime_type x, local_datetime_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(local_datetime_type x, std::vector<std::string> com)
        : basic_value(x, local_datetime_format_info{}, std::move(com), region_type{})
    {}
    basic_value(local_datetime_type x, local_datetime_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(local_datetime_type x, local_datetime_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::local_datetime), local_datetime_(local_datetime_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(local_datetime_type x)
    {
        local_datetime_format_info fmt;
        if(this->is_local_datetime())
        {
            fmt = this->as_local_datetime_fmt();
        }
        this->cleanup();
        this->type_   = value_t::local_datetime;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->local_datetime_, local_datetime_storage(x, fmt));
        return *this;
    }

    // }}}

    // constructor (offset_datetime) =========================================== {{{

    basic_value(offset_datetime_type x)
        : basic_value(x, offset_datetime_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(offset_datetime_type x, offset_datetime_format_info fmt)
        : basic_value(x, fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(offset_datetime_type x, std::vector<std::string> com)
        : basic_value(x, offset_datetime_format_info{}, std::move(com), region_type{})
    {}
    basic_value(offset_datetime_type x, offset_datetime_format_info fmt, std::vector<std::string> com)
        : basic_value(x, fmt, std::move(com), region_type{})
    {}
    basic_value(offset_datetime_type x, offset_datetime_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::offset_datetime), offset_datetime_(offset_datetime_storage(x, fmt)),
          region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(offset_datetime_type x)
    {
        offset_datetime_format_info fmt;
        if(this->is_offset_datetime())
        {
            fmt = this->as_offset_datetime_fmt();
        }
        this->cleanup();
        this->type_   = value_t::offset_datetime;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->offset_datetime_, offset_datetime_storage(x, fmt));
        return *this;
    }

    // system_clock::time_point

    basic_value(std::chrono::system_clock::time_point x)
        : basic_value(offset_datetime_type(x), offset_datetime_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, offset_datetime_format_info fmt)
        : basic_value(offset_datetime_type(x), fmt, std::vector<std::string>{}, region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, std::vector<std::string> com)
        : basic_value(offset_datetime_type(x), offset_datetime_format_info{}, std::move(com), region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, offset_datetime_format_info fmt, std::vector<std::string> com)
        : basic_value(offset_datetime_type(x), fmt, std::move(com), region_type{})
    {}
    basic_value(std::chrono::system_clock::time_point x, offset_datetime_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : basic_value(offset_datetime_type(x), std::move(fmt), std::move(com), std::move(reg))
    {}
    basic_value& operator=(std::chrono::system_clock::time_point x)
    {
        offset_datetime_format_info fmt;
        if(this->is_offset_datetime())
        {
            fmt = this->as_offset_datetime_fmt();
        }
        this->cleanup();
        this->type_   = value_t::offset_datetime;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->offset_datetime_, offset_datetime_storage(offset_datetime_type(x), fmt));
        return *this;
    }

    // }}}

    // constructor (array) ================================================ {{{

    basic_value(array_type x)
        : basic_value(std::move(x), array_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(array_type x, array_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(array_type x, std::vector<std::string> com)
        : basic_value(std::move(x), array_format_info{}, std::move(com), region_type{})
    {}
    basic_value(array_type x, array_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    basic_value(array_type x, array_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::array), array_(array_storage(
              detail::storage<array_type>(std::move(x)), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(array_type x)
    {
        array_format_info fmt;
        if(this->is_array())
        {
            fmt = this->as_array_fmt();
        }
        this->cleanup();
        this->type_   = value_t::array;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->array_, array_storage(
                    detail::storage<array_type>(std::move(x)), std::move(fmt)));
        return *this;
    }

  private:

    template<typename T>
    using enable_if_array_like_t = cxx::enable_if_t<cxx::conjunction<
            detail::is_container<T>,
            cxx::negation<std::is_same<T, array_type>>,
            cxx::negation<detail::is_std_basic_string<T>>,
#if defined(TOML11_HAS_STRING_VIEW)
            cxx::negation<detail::is_std_basic_string_view<T>>,
#endif
            cxx::negation<detail::has_from_toml_method<T, config_type>>,
            cxx::negation<detail::has_specialized_from<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(std::move(x), array_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, array_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(std::move(x), array_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, array_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value(T x, array_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::array), array_(array_storage(
              detail::storage<array_type>(array_type(
                      std::make_move_iterator(x.begin()),
                      std::make_move_iterator(x.end()))
              ), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    template<typename T, enable_if_array_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        array_format_info fmt;
        if(this->is_array())
        {
            fmt = this->as_array_fmt();
        }
        this->cleanup();
        this->type_   = value_t::array;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        array_type a(std::make_move_iterator(x.begin()),
                     std::make_move_iterator(x.end()));
        assigner(this->array_, array_storage(
                    detail::storage<array_type>(std::move(a)), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (table) ================================================ {{{

    basic_value(table_type x)
        : basic_value(std::move(x), table_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    basic_value(table_type x, table_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    basic_value(table_type x, std::vector<std::string> com)
        : basic_value(std::move(x), table_format_info{}, std::move(com), region_type{})
    {}
    basic_value(table_type x, table_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    basic_value(table_type x, table_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::table), table_(table_storage(
                detail::storage<table_type>(std::move(x)), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    basic_value& operator=(table_type x)
    {
        table_format_info fmt;
        if(this->is_table())
        {
            fmt = this->as_table_fmt();
        }
        this->cleanup();
        this->type_   = value_t::table;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        assigner(this->table_, table_storage(
            detail::storage<table_type>(std::move(x)), std::move(fmt)));
        return *this;
    }

    // table-like

  private:

    template<typename T>
    using enable_if_table_like_t = cxx::enable_if_t<cxx::conjunction<
            cxx::negation<std::is_same<T, table_type>>,
            detail::is_map<T>,
            cxx::negation<detail::has_from_toml_method<T, config_type>>,
            cxx::negation<detail::has_specialized_from<T>>
        >::value, std::nullptr_t>;

  public:

    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x)
        : basic_value(std::move(x), table_format_info{}, std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, table_format_info fmt)
        : basic_value(std::move(x), std::move(fmt), std::vector<std::string>{}, region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, std::vector<std::string> com)
        : basic_value(std::move(x), table_format_info{}, std::move(com), region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, table_format_info fmt, std::vector<std::string> com)
        : basic_value(std::move(x), fmt, std::move(com), region_type{})
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value(T x, table_format_info fmt,
                std::vector<std::string> com, region_type reg)
        : type_(value_t::table), table_(table_storage(
              detail::storage<table_type>(table_type(
                      std::make_move_iterator(x.begin()),
                      std::make_move_iterator(x.end())
              )), std::move(fmt)
          )), region_(std::move(reg)), comments_(std::move(com))
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}
    template<typename T, enable_if_table_like_t<T> = nullptr>
    basic_value& operator=(T x)
    {
        table_format_info fmt;
        if(this->is_table())
        {
            fmt = this->as_table_fmt();
        }
        this->cleanup();
        this->type_   = value_t::table;
        this->region_ = region_type{};
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        table_type t(std::make_move_iterator(x.begin()),
                     std::make_move_iterator(x.end()));
        assigner(this->table_, table_storage(
            detail::storage<table_type>(std::move(t)), std::move(fmt)));
        return *this;
    }

    // }}}

    // constructor (user_defined) ========================================= {{{

    template<typename T, cxx::enable_if_t<
        detail::has_specialized_into<T>::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud)
        : basic_value(
            into<cxx::remove_cvref_t<T>>::template into_toml<config_type>(ud))
    {}
    template<typename T, cxx::enable_if_t<
        detail::has_specialized_into<T>::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud, std::vector<std::string> com)
        : basic_value(
            into<cxx::remove_cvref_t<T>>::template into_toml<config_type>(ud),
            std::move(com))
    {}
    template<typename T, cxx::enable_if_t<
        detail::has_specialized_into<T>::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& ud)
    {
        *this = into<cxx::remove_cvref_t<T>>::template into_toml<config_type>(ud);
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        return *this;
    }

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_into_toml_method<T>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud): basic_value(ud.into_toml()) {}

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_into_toml_method<T>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud, std::vector<std::string> com)
        : basic_value(ud.into_toml(), std::move(com))
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_into_toml_method<T>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& ud)
    {
        *this = ud.into_toml();
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        return *this;
    }

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_template_into_toml_method<T, TypeConfig>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud): basic_value(ud.template into_toml<TypeConfig>()) {}

    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_template_into_toml_method<T, TypeConfig>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value(const T& ud, std::vector<std::string> com)
        : basic_value(ud.template into_toml<TypeConfig>(), std::move(com))
    {}
    template<typename T, cxx::enable_if_t<cxx::conjunction<
            detail::has_template_into_toml_method<T, TypeConfig>,
            cxx::negation<detail::has_specialized_into<T>>
        >::value, std::nullptr_t> = nullptr>
    basic_value& operator=(const T& ud)
    {
        *this = ud.template into_toml<TypeConfig>();
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        return *this;
    }
    // }}}

    // empty value with region info ======================================= {{{

    // mainly for `null` extension
    basic_value(detail::none_t, region_type reg) noexcept
        : type_(value_t::empty), empty_('\0'), region_(std::move(reg)), comments_{}
#ifdef TOML11_ENABLE_ACCESS_CHECK
        , accessed_{false}
#endif
    {}

    // }}}

    // type checking ====================================================== {{{

    template<typename T, cxx::enable_if_t<
        detail::is_exact_toml_type<cxx::remove_cvref_t<T>, value_type>::value,
        std::nullptr_t> = nullptr>
    bool is() const noexcept
    {
        return this->is(detail::type_to_enum<T, value_type>::value);
    }
    bool is(value_t t) const noexcept
    {
        this->set_accessed();
        return t == this->type_;
    }

    bool is_empty()           const noexcept {return this->is(value_t::empty          );}
    bool is_boolean()         const noexcept {return this->is(value_t::boolean        );}
    bool is_integer()         const noexcept {return this->is(value_t::integer        );}
    bool is_floating()        const noexcept {return this->is(value_t::floating       );}
    bool is_string()          const noexcept {return this->is(value_t::string         );}
    bool is_offset_datetime() const noexcept {return this->is(value_t::offset_datetime);}
    bool is_local_datetime()  const noexcept {return this->is(value_t::local_datetime );}
    bool is_local_date()      const noexcept {return this->is(value_t::local_date     );}
    bool is_local_time()      const noexcept {return this->is(value_t::local_time     );}
    bool is_array()           const noexcept {return this->is(value_t::array          );}
    bool is_table()           const noexcept {return this->is(value_t::table          );}

    bool is_array_of_tables() const noexcept
    {
        this->set_accessed();
        if( ! this->is_array()) {return false;}
        const auto& a = this->as_array(std::nothrow); // already checked.

        // when you define [[array.of.tables]], at least one empty table will be
        // assigned. In case of array of inline tables, `array_of_tables = []`,
        // there is no reason to consider this as an array of *tables*.
        // So empty array is not an array-of-tables.
        if(a.empty()) {return false;}

        // since toml v1.0.0 allows array of heterogeneous types, we need to
        // check all the elements. if any of the elements is not a table, it
        // is a heterogeneous array and cannot be expressed by `[[aot]]` form.
        for(const auto& e : a)
        {
            if( ! e.is_table()) {return false;}
        }
        return true;
    }

    value_t type() const noexcept
    {
        this->set_accessed();
        return type_;
    }

    // }}}

    // as_xxx (noexcept) version ========================================== {{{

    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>> const&
    as(const std::nothrow_t&) const noexcept
    {
        this->set_accessed();
        return detail::getter<config_type, T>::get_nothrow(*this);
    }
    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>>&
    as(const std::nothrow_t&) noexcept
    {
        this->set_accessed();
        return detail::getter<config_type, T>::get_nothrow(*this);
    }

    boolean_type         const& as_boolean        (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->boolean_.value;}
    integer_type         const& as_integer        (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->integer_.value;}
    floating_type        const& as_floating       (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->floating_.value;}
    string_type          const& as_string         (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->string_.value;}
    offset_datetime_type const& as_offset_datetime(const std::nothrow_t&) const noexcept {this->set_accessed(); return this->offset_datetime_.value;}
    local_datetime_type  const& as_local_datetime (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->local_datetime_.value;}
    local_date_type      const& as_local_date     (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->local_date_.value;}
    local_time_type      const& as_local_time     (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->local_time_.value;}
    array_type           const& as_array          (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->array_.value.get();}
    table_type           const& as_table          (const std::nothrow_t&) const noexcept {this->set_accessed(); return this->table_.value.get();}

    boolean_type        & as_boolean        (const std::nothrow_t&) noexcept {this->set_accessed(); return this->boolean_.value;}
    integer_type        & as_integer        (const std::nothrow_t&) noexcept {this->set_accessed(); return this->integer_.value;}
    floating_type       & as_floating       (const std::nothrow_t&) noexcept {this->set_accessed(); return this->floating_.value;}
    string_type         & as_string         (const std::nothrow_t&) noexcept {this->set_accessed(); return this->string_.value;}
    offset_datetime_type& as_offset_datetime(const std::nothrow_t&) noexcept {this->set_accessed(); return this->offset_datetime_.value;}
    local_datetime_type & as_local_datetime (const std::nothrow_t&) noexcept {this->set_accessed(); return this->local_datetime_.value;}
    local_date_type     & as_local_date     (const std::nothrow_t&) noexcept {this->set_accessed(); return this->local_date_.value;}
    local_time_type     & as_local_time     (const std::nothrow_t&) noexcept {this->set_accessed(); return this->local_time_.value;}
    array_type          & as_array          (const std::nothrow_t&) noexcept {this->set_accessed(); return this->array_.value.get();}
    table_type          & as_table          (const std::nothrow_t&) noexcept {this->set_accessed(); return this->table_.value.get();}

    // }}}

    // as_xxx (throw) ===================================================== {{{

    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>> const& as() const
    {
        return detail::getter<config_type, T>::get(*this);
    }
    template<value_t T>
    detail::enum_to_type_t<T, basic_value<config_type>>& as()
    {
        return detail::getter<config_type, T>::get(*this);
    }

    boolean_type const& as_boolean() const
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean()", value_t::boolean);
        }
        this->set_accessed();
        return this->boolean_.value;
    }
    integer_type const& as_integer() const
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer()", value_t::integer);
        }
        this->set_accessed();
        return this->integer_.value;
    }
    floating_type const& as_floating() const
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating()", value_t::floating);
        }
        this->set_accessed();
        return this->floating_.value;
    }
    string_type const& as_string() const
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string()", value_t::string);
        }
        this->set_accessed();
        return this->string_.value;
    }
    offset_datetime_type const& as_offset_datetime() const
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime()", value_t::offset_datetime);
        }
        this->set_accessed();
        return this->offset_datetime_.value;
    }
    local_datetime_type const& as_local_datetime() const
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime()", value_t::local_datetime);
        }
        this->set_accessed();
        return this->local_datetime_.value;
    }
    local_date_type const& as_local_date() const
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date()", value_t::local_date);
        }
        this->set_accessed();
        return this->local_date_.value;
    }
    local_time_type const& as_local_time() const
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time()", value_t::local_time);
        }
        this->set_accessed();
        return this->local_time_.value;
    }
    array_type const& as_array() const
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array()", value_t::array);
        }
        this->set_accessed();
        return this->array_.value.get();
    }
    table_type const& as_table() const
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table()", value_t::table);
        }
        this->set_accessed();
        return this->table_.value.get();
    }

    // ------------------------------------------------------------------------
    // nonconst reference

    boolean_type& as_boolean()
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean()", value_t::boolean);
        }
        this->set_accessed();
        return this->boolean_.value;
    }
    integer_type& as_integer()
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer()", value_t::integer);
        }
        this->set_accessed();
        return this->integer_.value;
    }
    floating_type& as_floating()
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating()", value_t::floating);
        }
        this->set_accessed();
        return this->floating_.value;
    }
    string_type& as_string()
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string()", value_t::string);
        }
        this->set_accessed();
        return this->string_.value;
    }
    offset_datetime_type& as_offset_datetime()
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime()", value_t::offset_datetime);
        }
        this->set_accessed();
        return this->offset_datetime_.value;
    }
    local_datetime_type& as_local_datetime()
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime()", value_t::local_datetime);
        }
        this->set_accessed();
        return this->local_datetime_.value;
    }
    local_date_type& as_local_date()
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date()", value_t::local_date);
        }
        this->set_accessed();
        return this->local_date_.value;
    }
    local_time_type& as_local_time()
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time()", value_t::local_time);
        }
        this->set_accessed();
        return this->local_time_.value;
    }
    array_type& as_array()
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array()", value_t::array);
        }
        this->set_accessed();
        return this->array_.value.get();
    }
    table_type& as_table()
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table()", value_t::table);
        }
        this->set_accessed();
        return this->table_.value.get();
    }

    // }}}

    // format accessors (noexcept) ======================================== {{{

    template<value_t T>
    detail::enum_to_fmt_type_t<T> const&
    as_fmt(const std::nothrow_t&) const noexcept
    {
        return detail::getter<config_type, T>::get_fmt_nothrow(*this);
    }
    template<value_t T>
    detail::enum_to_fmt_type_t<T>&
    as_fmt(const std::nothrow_t&) noexcept
    {
        return detail::getter<config_type, T>::get_fmt_nothrow(*this);
    }

    boolean_format_info        & as_boolean_fmt        (const std::nothrow_t&) noexcept {return this->boolean_.format;}
    integer_format_info        & as_integer_fmt        (const std::nothrow_t&) noexcept {return this->integer_.format;}
    floating_format_info       & as_floating_fmt       (const std::nothrow_t&) noexcept {return this->floating_.format;}
    string_format_info         & as_string_fmt         (const std::nothrow_t&) noexcept {return this->string_.format;}
    offset_datetime_format_info& as_offset_datetime_fmt(const std::nothrow_t&) noexcept {return this->offset_datetime_.format;}
    local_datetime_format_info & as_local_datetime_fmt (const std::nothrow_t&) noexcept {return this->local_datetime_.format;}
    local_date_format_info     & as_local_date_fmt     (const std::nothrow_t&) noexcept {return this->local_date_.format;}
    local_time_format_info     & as_local_time_fmt     (const std::nothrow_t&) noexcept {return this->local_time_.format;}
    array_format_info          & as_array_fmt          (const std::nothrow_t&) noexcept {return this->array_.format;}
    table_format_info          & as_table_fmt          (const std::nothrow_t&) noexcept {return this->table_.format;}

    boolean_format_info         const& as_boolean_fmt        (const std::nothrow_t&) const noexcept {return this->boolean_.format;}
    integer_format_info         const& as_integer_fmt        (const std::nothrow_t&) const noexcept {return this->integer_.format;}
    floating_format_info        const& as_floating_fmt       (const std::nothrow_t&) const noexcept {return this->floating_.format;}
    string_format_info          const& as_string_fmt         (const std::nothrow_t&) const noexcept {return this->string_.format;}
    offset_datetime_format_info const& as_offset_datetime_fmt(const std::nothrow_t&) const noexcept {return this->offset_datetime_.format;}
    local_datetime_format_info  const& as_local_datetime_fmt (const std::nothrow_t&) const noexcept {return this->local_datetime_.format;}
    local_date_format_info      const& as_local_date_fmt     (const std::nothrow_t&) const noexcept {return this->local_date_.format;}
    local_time_format_info      const& as_local_time_fmt     (const std::nothrow_t&) const noexcept {return this->local_time_.format;}
    array_format_info           const& as_array_fmt          (const std::nothrow_t&) const noexcept {return this->array_.format;}
    table_format_info           const& as_table_fmt          (const std::nothrow_t&) const noexcept {return this->table_.format;}

    // }}}

    // format accessors (throw) =========================================== {{{

    template<value_t T>
    detail::enum_to_fmt_type_t<T> const& as_fmt() const
    {
        return detail::getter<config_type, T>::get_fmt(*this);
    }
    template<value_t T>
    detail::enum_to_fmt_type_t<T>& as_fmt()
    {
        return detail::getter<config_type, T>::get_fmt(*this);
    }

    boolean_format_info const& as_boolean_fmt() const
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean_fmt()", value_t::boolean);
        }
        return this->boolean_.format;
    }
    integer_format_info const& as_integer_fmt() const
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer_fmt()", value_t::integer);
        }
        return this->integer_.format;
    }
    floating_format_info const& as_floating_fmt() const
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating_fmt()", value_t::floating);
        }
        return this->floating_.format;
    }
    string_format_info const& as_string_fmt() const
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string_fmt()", value_t::string);
        }
        return this->string_.format;
    }
    offset_datetime_format_info const& as_offset_datetime_fmt() const
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime_fmt()", value_t::offset_datetime);
        }
        return this->offset_datetime_.format;
    }
    local_datetime_format_info const& as_local_datetime_fmt() const
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime_fmt()", value_t::local_datetime);
        }
        return this->local_datetime_.format;
    }
    local_date_format_info const& as_local_date_fmt() const
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date_fmt()", value_t::local_date);
        }
        return this->local_date_.format;
    }
    local_time_format_info const& as_local_time_fmt() const
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time_fmt()", value_t::local_time);
        }
        return this->local_time_.format;
    }
    array_format_info const& as_array_fmt() const
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array_fmt()", value_t::array);
        }
        return this->array_.format;
    }
    table_format_info const& as_table_fmt() const
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table_fmt()", value_t::table);
        }
        return this->table_.format;
    }

    // ------------------------------------------------------------------------
    // nonconst reference

    boolean_format_info& as_boolean_fmt()
    {
        if(this->type_ != value_t::boolean)
        {
            this->throw_bad_cast("toml::value::as_boolean_fmt()", value_t::boolean);
        }
        return this->boolean_.format;
    }
    integer_format_info& as_integer_fmt()
    {
        if(this->type_ != value_t::integer)
        {
            this->throw_bad_cast("toml::value::as_integer_fmt()", value_t::integer);
        }
        return this->integer_.format;
    }
    floating_format_info& as_floating_fmt()
    {
        if(this->type_ != value_t::floating)
        {
            this->throw_bad_cast("toml::value::as_floating_fmt()", value_t::floating);
        }
        return this->floating_.format;
    }
    string_format_info& as_string_fmt()
    {
        if(this->type_ != value_t::string)
        {
            this->throw_bad_cast("toml::value::as_string_fmt()", value_t::string);
        }
        return this->string_.format;
    }
    offset_datetime_format_info& as_offset_datetime_fmt()
    {
        if(this->type_ != value_t::offset_datetime)
        {
            this->throw_bad_cast("toml::value::as_offset_datetime_fmt()", value_t::offset_datetime);
        }
        return this->offset_datetime_.format;
    }
    local_datetime_format_info& as_local_datetime_fmt()
    {
        if(this->type_ != value_t::local_datetime)
        {
            this->throw_bad_cast("toml::value::as_local_datetime_fmt()", value_t::local_datetime);
        }
        return this->local_datetime_.format;
    }
    local_date_format_info& as_local_date_fmt()
    {
        if(this->type_ != value_t::local_date)
        {
            this->throw_bad_cast("toml::value::as_local_date_fmt()", value_t::local_date);
        }
        return this->local_date_.format;
    }
    local_time_format_info& as_local_time_fmt()
    {
        if(this->type_ != value_t::local_time)
        {
            this->throw_bad_cast("toml::value::as_local_time_fmt()", value_t::local_time);
        }
        return this->local_time_.format;
    }
    array_format_info& as_array_fmt()
    {
        if(this->type_ != value_t::array)
        {
            this->throw_bad_cast("toml::value::as_array_fmt()", value_t::array);
        }
        return this->array_.format;
    }
    table_format_info& as_table_fmt()
    {
        if(this->type_ != value_t::table)
        {
            this->throw_bad_cast("toml::value::as_table_fmt()", value_t::table);
        }
        return this->table_.format;
    }
    // }}}

    // table accessors ==================================================== {{{

    value_type& at(const key_type& k)
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::at(key_type)", value_t::table);
        }
        auto& table = this->as_table(std::nothrow);
        const auto found = table.find(k);
        if(found == table.end())
        {
            this->throw_key_not_found_error("toml::value::at", k);
        }
        assert(found->first == k);
        return found->second;
    }
    value_type const& at(const key_type& k) const
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::at(key_type)", value_t::table);
        }
        const auto& table = this->as_table(std::nothrow);
        const auto found = table.find(k);
        if(found == table.end())
        {
            this->throw_key_not_found_error("toml::value::at", k);
        }
        assert(found->first == k);
        return found->second;
    }
    value_type& operator[](const key_type& k)
    {
        if(this->is_empty())
        {
            (*this) = table_type{};
        }
        else if( ! this->is_table()) // initialized, but not a table
        {
            this->throw_bad_cast("toml::value::operator[](key_type)", value_t::table);
        }
        return (this->as_table(std::nothrow))[k];
    }
    std::size_t count(const key_type& k) const
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::count(key_type)", value_t::table);
        }
        return this->as_table(std::nothrow).count(k);
    }
    bool contains(const key_type& k) const
    {
        if(!this->is_table())
        {
            this->throw_bad_cast("toml::value::contains(key_type)", value_t::table);
        }
        const auto& table = this->as_table(std::nothrow);
        return table.find(k) != table.end();
    }
    // }}}

    // array accessors ==================================================== {{{

    value_type& at(const std::size_t idx)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::at(idx)", value_t::array);
        }
        auto& ar = this->as_array(std::nothrow);

        if(ar.size() <= idx)
        {
            std::ostringstream oss;
            oss << "actual length (" << ar.size()
                << ") is shorter than the specified index (" << idx << ").";
            throw std::out_of_range(format_error(
                "toml::value::at(idx): no element corresponding to the index",
                this->location(), oss.str()
                ));
        }
        return ar.at(idx);
    }
    value_type const& at(const std::size_t idx) const
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::at(idx)", value_t::array);
        }
        const auto& ar = this->as_array(std::nothrow);

        if(ar.size() <= idx)
        {
            std::ostringstream oss;
            oss << "actual length (" << ar.size()
                << ") is shorter than the specified index (" << idx << ").";

            throw std::out_of_range(format_error(
                "toml::value::at(idx): no element corresponding to the index",
                this->location(), oss.str()
                ));
        }
        return ar.at(idx);
    }

    value_type&       operator[](const std::size_t idx) noexcept
    {
        // no check...
        return this->as_array(std::nothrow)[idx];
    }
    value_type const& operator[](const std::size_t idx) const noexcept
    {
        // no check...
        return this->as_array(std::nothrow)[idx];
    }

    void push_back(const value_type& x)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::push_back(idx)", value_t::array);
        }
        this->as_array(std::nothrow).push_back(x);
        return;
    }
    void push_back(value_type&& x)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::push_back(idx)", value_t::array);
        }
        this->as_array(std::nothrow).push_back(std::move(x));
        return;
    }

    template<typename ... Ts>
    value_type& emplace_back(Ts&& ... args)
    {
        if(!this->is_array())
        {
            this->throw_bad_cast("toml::value::emplace_back(idx)", value_t::array);
        }
        auto& ar = this->as_array(std::nothrow);
        ar.emplace_back(std::forward<Ts>(args) ...);
        return ar.back();
    }

    std::size_t size() const
    {
        switch(this->type_)
        {
            case value_t::array:
            {
                return this->as_array(std::nothrow).size();
            }
            case value_t::table:
            {
                return this->as_table(std::nothrow).size();
            }
            case value_t::string:
            {
                return this->as_string(std::nothrow).size();
            }
            default:
            {
                throw type_error(format_error(
                    "toml::value::size(): bad_cast to container types",
                    this->location(),
                    "the actual type is " + to_string(this->type_)
                    ), this->location());
            }
        }
    }

    // }}}

    source_location location() const
    {
        return source_location(this->region_);
    }

    comment_type const& comments() const noexcept {return this->comments_;}
    comment_type&       comments()       noexcept {return this->comments_;}

#ifdef TOML11_ENABLE_ACCESS_CHECK
    bool accessed() const {return this->accessed_.load();}
#endif

  private:

    // private helper functions =========================================== {{{

    void set_accessed() const noexcept
    {
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_.store(true);
#endif
        return;
    }

    void cleanup() noexcept
    {
        switch(this->type_)
        {
            case value_t::boolean         : { boolean_        .~boolean_storage         (); break; }
            case value_t::integer         : { integer_        .~integer_storage         (); break; }
            case value_t::floating        : { floating_       .~floating_storage        (); break; }
            case value_t::string          : { string_         .~string_storage          (); break; }
            case value_t::offset_datetime : { offset_datetime_.~offset_datetime_storage (); break; }
            case value_t::local_datetime  : { local_datetime_ .~local_datetime_storage  (); break; }
            case value_t::local_date      : { local_date_     .~local_date_storage      (); break; }
            case value_t::local_time      : { local_time_     .~local_time_storage      (); break; }
            case value_t::array           : { array_          .~array_storage           (); break; }
            case value_t::table           : { table_          .~table_storage           (); break; }
            default                       : { break; }
        }
#ifdef TOML11_ENABLE_ACCESS_CHECK
        this->accessed_ = false;
#endif
        this->type_ = value_t::empty;
        return;
    }

    template<typename T, typename U>
    static void assigner(T& dst, U&& v)
    {
        const auto tmp = ::new(std::addressof(dst)) T(std::forward<U>(v));
        assert(tmp == std::addressof(dst));
        (void)tmp;
    }

    [[noreturn]]
    void throw_bad_cast(const std::string& funcname, const value_t ty) const
    {
        throw type_error(format_error(detail::make_type_error(*this, funcname, ty)),
                         this->location());
    }

    [[noreturn]]
    void throw_key_not_found_error(const std::string& funcname, const key_type& key) const
    {
        throw std::out_of_range(format_error(
                    detail::make_not_found_error(*this, funcname, key)));
    }

    template<typename TC>
    friend void detail::change_region_of_value(basic_value<TC>&, const basic_value<TC>&);

    template<typename TC>
    friend class basic_value;


#ifdef TOML11_ENABLE_ACCESS_CHECK
    template<typename TC>
    friend void detail::unset_access_flag(basic_value<TC>&);
#endif

    // }}}

  private:

    using boolean_storage         = detail::value_with_format<boolean_type,                boolean_format_info        >;
    using integer_storage         = detail::value_with_format<integer_type,                integer_format_info        >;
    using floating_storage        = detail::value_with_format<floating_type,               floating_format_info       >;
    using string_storage          = detail::value_with_format<string_type,                 string_format_info         >;
    using offset_datetime_storage = detail::value_with_format<offset_datetime_type,        offset_datetime_format_info>;
    using local_datetime_storage  = detail::value_with_format<local_datetime_type,         local_datetime_format_info >;
    using local_date_storage      = detail::value_with_format<local_date_type,             local_date_format_info     >;
    using local_time_storage      = detail::value_with_format<local_time_type,             local_time_format_info     >;
    using array_storage           = detail::value_with_format<detail::storage<array_type>, array_format_info          >;
    using table_storage           = detail::value_with_format<detail::storage<table_type>, table_format_info          >;

  private:

    value_t type_;
    union
    {
        char                    empty_; // the smallest type
        boolean_storage         boolean_;
        integer_storage         integer_;
        floating_storage        floating_;
        string_storage          string_;
        offset_datetime_storage offset_datetime_;
        local_datetime_storage  local_datetime_;
        local_date_storage      local_date_;
        local_time_storage      local_time_;
        array_storage           array_;
        table_storage           table_;
    };
    region_type  region_;
    comment_type comments_;

#ifdef TOML11_ENABLE_ACCESS_CHECK
    mutable std::atomic<bool> accessed_;
#endif
};

template<typename TC>
bool operator==(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    if(lhs.type()     != rhs.type())     {return false;}
    if(lhs.comments() != rhs.comments()) {return false;}

    switch(lhs.type())
    {
        case value_t::boolean  :
        {
            return lhs.as_boolean() == rhs.as_boolean();
        }
        case value_t::integer  :
        {
            return lhs.as_integer() == rhs.as_integer();
        }
        case value_t::floating :
        {
            return lhs.as_floating() == rhs.as_floating();
        }
        case value_t::string   :
        {
            return lhs.as_string() == rhs.as_string();
        }
        case value_t::offset_datetime:
        {
            return lhs.as_offset_datetime() == rhs.as_offset_datetime();
        }
        case value_t::local_datetime:
        {
            return lhs.as_local_datetime() == rhs.as_local_datetime();
        }
        case value_t::local_date:
        {
            return lhs.as_local_date() == rhs.as_local_date();
        }
        case value_t::local_time:
        {
            return lhs.as_local_time() == rhs.as_local_time();
        }
        case value_t::array    :
        {
            return lhs.as_array() == rhs.as_array();
        }
        case value_t::table    :
        {
            return lhs.as_table() == rhs.as_table();
        }
        case value_t::empty    : {return true; }
        default:                 {return false;}
    }
}

template<typename TC>
bool operator!=(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return !(lhs == rhs);
}

template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator<(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    if(lhs.type() != rhs.type())
    {
        return (lhs.type() < rhs.type());
    }
    switch(lhs.type())
    {
        case value_t::boolean  :
        {
            return lhs.as_boolean() <  rhs.as_boolean() ||
                  (lhs.as_boolean() == rhs.as_boolean() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::integer  :
        {
            return lhs.as_integer() <  rhs.as_integer() ||
                  (lhs.as_integer() == rhs.as_integer() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::floating :
        {
            return lhs.as_floating() <  rhs.as_floating() ||
                  (lhs.as_floating() == rhs.as_floating() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::string   :
        {
            return lhs.as_string() <  rhs.as_string() ||
                  (lhs.as_string() == rhs.as_string() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::offset_datetime:
        {
            return lhs.as_offset_datetime() <  rhs.as_offset_datetime() ||
                  (lhs.as_offset_datetime() == rhs.as_offset_datetime() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::local_datetime:
        {
            return lhs.as_local_datetime() <  rhs.as_local_datetime() ||
                  (lhs.as_local_datetime() == rhs.as_local_datetime() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::local_date:
        {
            return lhs.as_local_date() <  rhs.as_local_date() ||
                  (lhs.as_local_date() == rhs.as_local_date() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::local_time:
        {
            return lhs.as_local_time() <  rhs.as_local_time() ||
                  (lhs.as_local_time() == rhs.as_local_time() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::array    :
        {
            return lhs.as_array() <  rhs.as_array() ||
                  (lhs.as_array() == rhs.as_array() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::table    :
        {
            return lhs.as_table() <  rhs.as_table() ||
                  (lhs.as_table() == rhs.as_table() &&
                   lhs.comments() < rhs.comments());
        }
        case value_t::empty    :
        {
            return lhs.comments() < rhs.comments();
        }
        default:
        {
            return lhs.comments() < rhs.comments();
        }
    }
}

template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator<=(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return (lhs < rhs) || (lhs == rhs);
}
template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator>(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return !(lhs <= rhs);
}
template<typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_comparable<typename basic_value<TC>::array_type>,
    detail::is_comparable<typename basic_value<TC>::table_type>
    >::value, bool>
operator>=(const basic_value<TC>& lhs, const basic_value<TC>& rhs)
{
    return !(lhs < rhs);
}

// error_info helper
namespace detail
{
template<typename TC, typename ... Ts>
error_info make_error_info_rec(error_info e,
        const basic_value<TC>& v, std::string msg, Ts&& ... tail)
{
    return make_error_info_rec(std::move(e), v.location(), std::move(msg), std::forward<Ts>(tail)...);
}
} // detail

template<typename TC, typename ... Ts>
error_info make_error_info(
        std::string title, const basic_value<TC>& v, std::string msg, Ts&& ... tail)
{
    return make_error_info(std::move(title),
            v.location(), std::move(msg), std::forward<Ts>(tail)...);
}
template<typename TC, typename ... Ts>
std::string format_error(std::string title,
        const basic_value<TC>& v, std::string msg, Ts&& ... tail)
{
    return format_error(std::move(title),
            v.location(), std::move(msg), std::forward<Ts>(tail)...);
}

namespace detail
{

template<typename TC>
error_info make_type_error(const basic_value<TC>& v, const std::string& fname, const value_t ty)
{
    return make_error_info(fname + ": bad_cast to " + to_string(ty),
        v.location(), "the actual type is " + to_string(v.type()));
}
template<typename TC>
error_info make_not_found_error(const basic_value<TC>& v, const std::string& fname, const typename basic_value<TC>::key_type& key)
{
    const auto loc = v.location();
    const std::string title = fname + ": key \"" + string_conv<std::string>(key) + "\" not found";

    std::vector<std::pair<source_location, std::string>> locs;
    if( ! loc.is_ok())
    {
        return error_info(title, locs);
    }

    if(loc.first_line_number() == 1 && loc.first_column_number() == 1 && loc.length() == 1)
    {
        // The top-level table has its region at the 0th character of the file.
        // That means that, in the case when a key is not found in the top-level
        // table, the error message points to the first character. If the file has
        // the first table at the first line, the error message would be like this.
        // ```console
        // [error] key "a" not found
        //  --> example.toml
        //    |
        //  1 | [table]
        //    | ^------ in this table
        // ```
        // It actually points to the top-level table at the first character, not
        // `[table]`. But it is too confusing. To avoid the confusion, the error
        // message should explicitly say "key not found in the top-level table".
        locs.emplace_back(v.location(), "at the top-level table");
    }
    else
    {
        locs.emplace_back(v.location(), "in this table");
    }
    return error_info(title, locs);
}

#define TOML11_DETAIL_GENERATE_COMPTIME_GETTER(ty)                              \
    template<typename TC>                                                       \
    struct getter<TC, value_t::ty>                                              \
    {                                                                           \
        using value_type = basic_value<TC>;                                     \
        using result_type = enum_to_type_t<value_t::ty, value_type>;            \
        using format_type = enum_to_fmt_type_t<value_t::ty>;                    \
                                                                                \
        static result_type&       get(value_type& v)                            \
        {                                                                       \
            return v.as_ ## ty();                                               \
        }                                                                       \
        static result_type const& get(const value_type& v)                      \
        {                                                                       \
            return v.as_ ## ty();                                               \
        }                                                                       \
                                                                                \
        static result_type&       get_nothrow(value_type& v) noexcept           \
        {                                                                       \
            return v.as_ ## ty(std::nothrow);                                   \
        }                                                                       \
        static result_type const& get_nothrow(const value_type& v) noexcept     \
        {                                                                       \
            return v.as_ ## ty(std::nothrow);                                   \
        }                                                                       \
                                                                                \
        static format_type&       get_fmt(value_type& v)                        \
        {                                                                       \
            return v.as_ ## ty ## _fmt();                                       \
        }                                                                       \
        static format_type const& get_fmt(const value_type& v)                  \
        {                                                                       \
            return v.as_ ## ty ## _fmt();                                       \
        }                                                                       \
                                                                                \
        static format_type&       get_fmt_nothrow(value_type& v) noexcept       \
        {                                                                       \
            return v.as_ ## ty ## _fmt(std::nothrow);                           \
        }                                                                       \
        static format_type const& get_fmt_nothrow(const value_type& v) noexcept \
        {                                                                       \
            return v.as_ ## ty ## _fmt(std::nothrow);                           \
        }                                                                       \
    };

TOML11_DETAIL_GENERATE_COMPTIME_GETTER(boolean        )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(integer        )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(floating       )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(string         )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(offset_datetime)
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(local_datetime )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(local_date     )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(local_time     )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(array          )
TOML11_DETAIL_GENERATE_COMPTIME_GETTER(table          )

#undef TOML11_DETAIL_GENERATE_COMPTIME_GETTER

template<typename TC>
void change_region_of_value(basic_value<TC>& dst, const basic_value<TC>& src)
{
    dst.region_ = std::move(src.region_);
    return;
}

#ifdef TOML11_ENABLE_ACCESS_CHECK
template<typename TC>
void unset_access_flag(basic_value<TC>& v)
{
    v.accessed_.store(false);
}

template<typename TC>
void unset_access_flag_recursively(basic_value<TC>& v)
{
    switch(v.type())
    {
        case value_t::empty            : { return unset_access_flag(v); }
        case value_t::boolean          : { return unset_access_flag(v); }
        case value_t::integer          : { return unset_access_flag(v); }
        case value_t::floating         : { return unset_access_flag(v); }
        case value_t::string           : { return unset_access_flag(v); }
        case value_t::offset_datetime  : { return unset_access_flag(v); }
        case value_t::local_datetime   : { return unset_access_flag(v); }
        case value_t::local_date       : { return unset_access_flag(v); }
        case value_t::local_time       : { return unset_access_flag(v); }
        case value_t::array:
        {
            for(auto& elem : v.as_array())
            {
                unset_access_flag_recursively(elem);
            }
            return unset_access_flag(v);
        }
        case value_t::table:
        {
            for(auto& kv : v.as_table())
            {
                unset_access_flag_recursively(kv.second);
            }
            return unset_access_flag(v);
        }
        default: { return unset_access_flag(v); }
    }
}
#endif

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml
#endif // TOML11_VALUE_HPP
#ifndef TOML11_VISIT_HPP
#define TOML11_VISIT_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

namespace detail
{

template<typename F, typename ... Ts>
using visit_result_t = decltype(std::declval<F>()(std::declval<Ts>().as_boolean() ...));

template<typename F, typename T>
struct front_binder
{
    template<typename ... Args>
    auto operator()(Args&& ... args) -> decltype(std::declval<F>()(std::declval<T>(), std::forward<Args>(args)...))
    {
        return func(std::move(front), std::forward<Args>(args)...);
    }
    F func;
    T front;
};

template<typename F, typename T>
front_binder<cxx::remove_cvref_t<F>, cxx::remove_cvref_t<T>>
bind_front(F&& f, T&& t)
{
    return front_binder<cxx::remove_cvref_t<F>, cxx::remove_cvref_t<T>>{
        std::forward<F>(f), std::forward<T>(t)
    };
}

template<typename Visitor, typename TC, typename ... Args>
visit_result_t<Visitor, const basic_value<TC>&, Args...>
visit_impl(Visitor&& visitor, const basic_value<TC>& v, Args&& ... args);

template<typename Visitor, typename TC, typename ... Args>
visit_result_t<Visitor, basic_value<TC>&, Args...>
visit_impl(Visitor&& visitor, basic_value<TC>& v, Args&& ... args);

template<typename Visitor, typename TC, typename ... Args>
visit_result_t<Visitor, basic_value<TC>, Args...>
visit_impl(Visitor&& visitor, basic_value<TC>&& v, Args&& ... args);


template<typename Visitor>
visit_result_t<Visitor> visit_impl(Visitor&& visitor)
{
    return visitor();
}

template<typename Visitor, typename TC, typename ... Args>
visit_result_t<Visitor, basic_value<TC>&, Args...>
visit_impl(Visitor&& visitor, basic_value<TC>& v, Args&& ... args)
{
    switch(v.type())
    {
        case value_t::boolean        : {return visit_impl(bind_front(visitor, std::ref(v.as_boolean        ())), std::forward<Args>(args)...);}
        case value_t::integer        : {return visit_impl(bind_front(visitor, std::ref(v.as_integer        ())), std::forward<Args>(args)...);}
        case value_t::floating       : {return visit_impl(bind_front(visitor, std::ref(v.as_floating       ())), std::forward<Args>(args)...);}
        case value_t::string         : {return visit_impl(bind_front(visitor, std::ref(v.as_string         ())), std::forward<Args>(args)...);}
        case value_t::offset_datetime: {return visit_impl(bind_front(visitor, std::ref(v.as_offset_datetime())), std::forward<Args>(args)...);}
        case value_t::local_datetime : {return visit_impl(bind_front(visitor, std::ref(v.as_local_datetime ())), std::forward<Args>(args)...);}
        case value_t::local_date     : {return visit_impl(bind_front(visitor, std::ref(v.as_local_date     ())), std::forward<Args>(args)...);}
        case value_t::local_time     : {return visit_impl(bind_front(visitor, std::ref(v.as_local_time     ())), std::forward<Args>(args)...);}
        case value_t::array          : {return visit_impl(bind_front(visitor, std::ref(v.as_array          ())), std::forward<Args>(args)...);}
        case value_t::table          : {return visit_impl(bind_front(visitor, std::ref(v.as_table          ())), std::forward<Args>(args)...);}
        case value_t::empty          : break;
        default: break;
    }
    throw type_error(format_error("[error] toml::visit: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
}

template<typename Visitor, typename TC, typename ... Args>
visit_result_t<Visitor, const basic_value<TC>&, Args...>
visit_impl(Visitor&& visitor, const basic_value<TC>& v, Args&& ... args)
{
    switch(v.type())
    {
        case value_t::boolean        : {return visit_impl(bind_front(visitor, std::cref(v.as_boolean        ())), std::forward<Args>(args)...);}
        case value_t::integer        : {return visit_impl(bind_front(visitor, std::cref(v.as_integer        ())), std::forward<Args>(args)...);}
        case value_t::floating       : {return visit_impl(bind_front(visitor, std::cref(v.as_floating       ())), std::forward<Args>(args)...);}
        case value_t::string         : {return visit_impl(bind_front(visitor, std::cref(v.as_string         ())), std::forward<Args>(args)...);}
        case value_t::offset_datetime: {return visit_impl(bind_front(visitor, std::cref(v.as_offset_datetime())), std::forward<Args>(args)...);}
        case value_t::local_datetime : {return visit_impl(bind_front(visitor, std::cref(v.as_local_datetime ())), std::forward<Args>(args)...);}
        case value_t::local_date     : {return visit_impl(bind_front(visitor, std::cref(v.as_local_date     ())), std::forward<Args>(args)...);}
        case value_t::local_time     : {return visit_impl(bind_front(visitor, std::cref(v.as_local_time     ())), std::forward<Args>(args)...);}
        case value_t::array          : {return visit_impl(bind_front(visitor, std::cref(v.as_array          ())), std::forward<Args>(args)...);}
        case value_t::table          : {return visit_impl(bind_front(visitor, std::cref(v.as_table          ())), std::forward<Args>(args)...);}
        case value_t::empty          : break;
        default: break;
    }
    throw type_error(format_error("[error] toml::visit: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
}

template<typename Visitor, typename TC, typename ... Args>
visit_result_t<Visitor, basic_value<TC>, Args...>
visit_impl(Visitor&& visitor, basic_value<TC>&& v, Args&& ... args)
{
    switch(v.type())
    {
        case value_t::boolean        : {return visit_impl(bind_front(visitor, std::move(v.as_boolean        ())), std::forward<Args>(args)...);}
        case value_t::integer        : {return visit_impl(bind_front(visitor, std::move(v.as_integer        ())), std::forward<Args>(args)...);}
        case value_t::floating       : {return visit_impl(bind_front(visitor, std::move(v.as_floating       ())), std::forward<Args>(args)...);}
        case value_t::string         : {return visit_impl(bind_front(visitor, std::move(v.as_string         ())), std::forward<Args>(args)...);}
        case value_t::offset_datetime: {return visit_impl(bind_front(visitor, std::move(v.as_offset_datetime())), std::forward<Args>(args)...);}
        case value_t::local_datetime : {return visit_impl(bind_front(visitor, std::move(v.as_local_datetime ())), std::forward<Args>(args)...);}
        case value_t::local_date     : {return visit_impl(bind_front(visitor, std::move(v.as_local_date     ())), std::forward<Args>(args)...);}
        case value_t::local_time     : {return visit_impl(bind_front(visitor, std::move(v.as_local_time     ())), std::forward<Args>(args)...);}
        case value_t::array          : {return visit_impl(bind_front(visitor, std::move(v.as_array          ())), std::forward<Args>(args)...);}
        case value_t::table          : {return visit_impl(bind_front(visitor, std::move(v.as_table          ())), std::forward<Args>(args)...);}
        case value_t::empty          : break;
        default: break;
    }
    throw type_error(format_error("[error] toml::visit: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
}

} // detail

template<typename Visitor, typename ... Args>
detail::visit_result_t<Visitor, Args...>
visit(Visitor&& visitor, Args&& ... args)
{
    return detail::visit_impl(std::forward<Visitor>(visitor), std::forward<Args>(args)...);
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_VISIT_HPP
#ifndef TOML11_TYPES_HPP
#define TOML11_TYPES_HPP


#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <cstdint>
#include <cstdio>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// forward decl
template<typename TypeConfig>
class basic_value;

// when you use a special integer type as toml::value::integer_type, parse must
// be able to read it. So, type_config has static member functions that read the
// integer_type as {dec, hex, oct, bin}-integer. But, in most cases, operator<<
// is enough. To make config easy, we provide the default read functions.
//
// Before this functions is called, syntax is checked and prefix(`0x` etc) and
// spacer(`_`) are removed.

template<typename T>
result<T, error_info>
read_dec_int(const std::string& str, const source_location src)
{
    constexpr auto max_digits = std::numeric_limits<T>::digits;
    assert( ! str.empty());

    T val{0};
    std::istringstream iss(str);
    iss >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_dec_integer: "
            "too large integer: current max digits = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_hex_int(const std::string& str, const source_location src)
{
    constexpr auto max_digits = std::numeric_limits<T>::digits;
    assert( ! str.empty());

    T val{0};
    std::istringstream iss(str);
    iss >> std::hex >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_hex_integer: "
            "too large integer: current max value = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_oct_int(const std::string& str, const source_location src)
{
    constexpr auto max_digits = std::numeric_limits<T>::digits;
    assert( ! str.empty());

    T val{0};
    std::istringstream iss(str);
    iss >> std::oct >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_oct_integer: "
            "too large integer: current max value = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_bin_int(const std::string& str, const source_location src)
{
    constexpr auto is_bounded =  std::numeric_limits<T>::is_bounded;
    constexpr auto max_digits =  std::numeric_limits<T>::digits;
    const auto max_value  = (std::numeric_limits<T>::max)();

    T val{0};
    T base{1};
    for(auto i = str.rbegin(); i != str.rend(); ++i)
    {
        const auto c = *i;
        if(c == '1')
        {
            val += base;
            // prevent `base` from overflow
            if(is_bounded && max_value / 2 < base && std::next(i) != str.rend())
            {
                base = 0;
            }
            else
            {
                base *= 2;
            }
        }
        else
        {
            assert(c == '0');

            if(is_bounded && max_value / 2 < base && std::next(i) != str.rend())
            {
                base = 0;
            }
            else
            {
                base *= 2;
            }
        }
    }
    if(base == 0)
    {
        return err(make_error_info("toml::parse_bin_integer: "
            "too large integer: current max value = 2^" + std::to_string(max_digits),
            std::move(src), "must be < 2^" + std::to_string(max_digits)));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_int(const std::string& str, const source_location src, const std::uint8_t base)
{
    assert(base == 10 || base == 16 || base == 8 || base == 2);
    switch(base)
    {
        case  2: { return read_bin_int<T>(str, src); }
        case  8: { return read_oct_int<T>(str, src); }
        case 16: { return read_hex_int<T>(str, src); }
        default:
        {
            assert(base == 10);
            return read_dec_int<T>(str, src);
        }
    }
}

inline result<float, error_info>
read_hex_float(const std::string& str, const source_location src, float val)
{
#if defined(_MSC_VER) && ! defined(__clang__)
    const auto res = ::sscanf_s(str.c_str(), "%a", std::addressof(val));
#else
    const auto res = std::sscanf(str.c_str(), "%a", std::addressof(val));
#endif
    if(res != 1)
    {
        return err(make_error_info("toml::parse_floating: "
            "failed to read hexadecimal floating point value ",
            std::move(src), "here"));
    }
    return ok(val);
}
inline result<double, error_info>
read_hex_float(const std::string& str, const source_location src, double val)
{
#if defined(_MSC_VER) && ! defined(__clang__)
    const auto res = ::sscanf_s(str.c_str(), "%la", std::addressof(val));
#else
    const auto res = std::sscanf(str.c_str(), "%la", std::addressof(val));
#endif
    if(res != 1)
    {
        return err(make_error_info("toml::parse_floating: "
            "failed to read hexadecimal floating point value ",
            std::move(src), "here"));
    }
    return ok(val);
}
template<typename T>
cxx::enable_if_t<cxx::conjunction<
    cxx::negation<std::is_same<cxx::remove_cvref_t<T>, double>>,
    cxx::negation<std::is_same<cxx::remove_cvref_t<T>, float>>
    >::value, result<T, error_info>>
read_hex_float(const std::string&, const source_location src, T)
{
    return err(make_error_info("toml::parse_floating: failed to read "
        "floating point value because of unknown type in type_config",
        std::move(src), "here"));
}

template<typename T>
result<T, error_info>
read_dec_float(const std::string& str, const source_location src)
{
    T val;
    std::istringstream iss(str);
    iss >> val;
    if(iss.fail())
    {
        return err(make_error_info("toml::parse_floating: "
            "failed to read floating point value from stream",
            std::move(src), "here"));
    }
    return ok(val);
}

template<typename T>
result<T, error_info>
read_float(const std::string& str, const source_location src, const bool is_hex)
{
    if(is_hex)
    {
        return read_hex_float(str, src, T{});
    }
    else
    {
        return read_dec_float<T>(str, src);
    }
}

struct type_config
{
    using comment_type  = preserve_comments;

    using boolean_type  = bool;
    using integer_type  = std::int64_t;
    using floating_type = double;
    using string_type   = std::string;

    template<typename T>
    using array_type = std::vector<T>;
    template<typename K, typename T>
    using table_type = std::unordered_map<K, T>;

    static result<integer_type, error_info>
    parse_int(const std::string& str, const source_location src, const std::uint8_t base)
    {
        return read_int<integer_type>(str, src, base);
    }
    static result<floating_type, error_info>
    parse_float(const std::string& str, const source_location src, const bool is_hex)
    {
        return read_float<floating_type>(str, src, is_hex);
    }
};

using value = basic_value<type_config>;
using table = typename value::table_type;
using array = typename value::array_type;

struct ordered_type_config
{
    using comment_type  = preserve_comments;

    using boolean_type  = bool;
    using integer_type  = std::int64_t;
    using floating_type = double;
    using string_type   = std::string;

    template<typename T>
    using array_type = std::vector<T>;
    template<typename K, typename T>
    using table_type = ordered_map<K, T>;

    static result<integer_type, error_info>
    parse_int(const std::string& str, const source_location src, const std::uint8_t base)
    {
        return read_int<integer_type>(str, src, base);
    }
    static result<floating_type, error_info>
    parse_float(const std::string& str, const source_location src, const bool is_hex)
    {
        return read_float<floating_type>(str, src, is_hex);
    }
};

using ordered_value = basic_value<ordered_type_config>;
using ordered_table = typename ordered_value::table_type;
using ordered_array = typename ordered_value::array_type;

// ----------------------------------------------------------------------------
// meta functions for internal use

namespace detail
{

// ----------------------------------------------------------------------------
// check if type T has all the needed member types

template<typename T, typename U = void>
struct has_comment_type: std::false_type{};
template<typename T>
struct has_comment_type<T, cxx::void_t<typename T::comment_type>>: std::true_type{};

template<typename T, typename U = void>
struct has_integer_type: std::false_type{};
template<typename T>
struct has_integer_type<T, cxx::void_t<typename T::integer_type>>: std::true_type{};

template<typename T, typename U = void>
struct has_floating_type: std::false_type{};
template<typename T>
struct has_floating_type<T, cxx::void_t<typename T::floating_type>>: std::true_type{};

template<typename T, typename U = void>
struct has_string_type: std::false_type{};
template<typename T>
struct has_string_type<T, cxx::void_t<typename T::string_type>>: std::true_type{};

template<typename T, typename U = void>
struct has_array_type: std::false_type{};
template<typename T>
struct has_array_type<T, cxx::void_t<typename T::template array_type<int>>>: std::true_type{};

template<typename T, typename U = void>
struct has_table_type: std::false_type{};
template<typename T>
struct has_table_type<T, cxx::void_t<typename T::template table_type<int, int>>>: std::true_type{};

template<typename T, typename U = void>
struct has_parse_int: std::false_type{};
template<typename T>
struct has_parse_int<T, cxx::void_t<decltype(std::declval<T>().parse_int(
        std::declval<std::string const&>(),
        std::declval<::toml::source_location const&>(),
        std::declval<std::uint8_t>()
    ))>>: std::true_type{};

template<typename T, typename U = void>
struct has_parse_float: std::false_type{};
template<typename T>
struct has_parse_float<T, cxx::void_t<decltype(std::declval<T>().parse_float(
        std::declval<std::string const&>(),
        std::declval<::toml::source_location const&>(),
        std::declval<bool>()
    ))>>: std::true_type{};

template<typename T>
using is_type_config = cxx::conjunction<
    has_comment_type<T>,
    has_integer_type<T>,
    has_floating_type<T>,
    has_string_type<T>,
    has_array_type<T>,
    has_table_type<T>,
    has_parse_int<T>,
    has_parse_float<T>
    >;

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
extern template class basic_value<type_config>;
extern template class basic_value<ordered_type_config>;
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_TYPES_HPP
#ifndef TOML11_SERIALIZER_HPP
#define TOML11_SERIALIZER_HPP


#include <iomanip>
#include <iterator>
#include <sstream>

#include <cmath>
#include <cstdio>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

struct serialization_error final : public ::toml::exception
{
  public:
    explicit serialization_error(std::string what_arg, source_location loc)
        : what_(std::move(what_arg)), loc_(std::move(loc))
    {}
    ~serialization_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}
    source_location const& location() const noexcept {return loc_;}

  private:
    std::string what_;
    source_location loc_;
};

namespace detail
{
template<typename TC>
class serializer
{
  public:

    using value_type           = basic_value<TC>;

    using key_type             = typename value_type::key_type            ;
    using comment_type         = typename value_type::comment_type        ;
    using boolean_type         = typename value_type::boolean_type        ;
    using integer_type         = typename value_type::integer_type        ;
    using floating_type        = typename value_type::floating_type       ;
    using string_type          = typename value_type::string_type         ;
    using local_time_type      = typename value_type::local_time_type     ;
    using local_date_type      = typename value_type::local_date_type     ;
    using local_datetime_type  = typename value_type::local_datetime_type ;
    using offset_datetime_type = typename value_type::offset_datetime_type;
    using array_type           = typename value_type::array_type          ;
    using table_type           = typename value_type::table_type          ;

    using char_type            = typename string_type::value_type;

  public:

    explicit serializer(const spec& sp)
        : spec_(sp), force_inline_(false), current_indent_(0)
    {}

    string_type operator()(const std::vector<key_type>& ks, const value_type& v)
    {
        for(const auto& k : ks)
        {
            this->keys_.push_back(k);
        }
        return (*this)(v);
    }

    string_type operator()(const key_type& k, const value_type& v)
    {
        this->keys_.push_back(k);
        return (*this)(v);
    }

    string_type operator()(const value_type& v)
    {
        switch(v.type())
        {
            case value_t::boolean        : {return (*this)(v.as_boolean        (), v.as_boolean_fmt        (), v.location());}
            case value_t::integer        : {return (*this)(v.as_integer        (), v.as_integer_fmt        (), v.location());}
            case value_t::floating       : {return (*this)(v.as_floating       (), v.as_floating_fmt       (), v.location());}
            case value_t::string         : {return (*this)(v.as_string         (), v.as_string_fmt         (), v.location());}
            case value_t::offset_datetime: {return (*this)(v.as_offset_datetime(), v.as_offset_datetime_fmt(), v.location());}
            case value_t::local_datetime : {return (*this)(v.as_local_datetime (), v.as_local_datetime_fmt (), v.location());}
            case value_t::local_date     : {return (*this)(v.as_local_date     (), v.as_local_date_fmt     (), v.location());}
            case value_t::local_time     : {return (*this)(v.as_local_time     (), v.as_local_time_fmt     (), v.location());}
            case value_t::array          :
            {
                return (*this)(v.as_array(), v.as_array_fmt(), v.comments(), v.location());
            }
            case value_t::table          :
            {
                string_type retval;
                if(this->keys_.empty()) // it might be the root table. emit comments here.
                {
                    retval += format_comments(v.comments(), v.as_table_fmt().indent_type);
                }
                if( ! retval.empty()) // we have comment.
                {
                    retval += char_type('\n');
                }

                retval += (*this)(v.as_table(), v.as_table_fmt(), v.comments(), v.location());
                return retval;
            }
            case value_t::empty:
            {
                if(this->spec_.ext_null_value)
                {
                    return string_conv<string_type>("null");
                }
                break;
            }
            default:
            {
                break;
            }
        }
        throw serialization_error(format_error(
            "[error] toml::serializer: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
    }

  private:

    string_type operator()(const boolean_type& b, const boolean_format_info&, const source_location&) // {{{
    {
        if(b)
        {
            return string_conv<string_type>("true");
        }
        else
        {
            return string_conv<string_type>("false");
        }
    } // }}}

    string_type operator()(const integer_type i, const integer_format_info& fmt, const source_location& loc) // {{{
    {
        std::ostringstream oss;
        this->set_locale(oss);

        const auto insert_spacer = [&fmt](std::string s) -> std::string {
            if(fmt.spacer == 0) {return s;}

            std::string sign;
            if( ! s.empty() && (s.at(0) == '+' || s.at(0) == '-'))
            {
                sign += s.at(0);
                s.erase(s.begin());
            }

            std::string spaced;
            std::size_t counter = 0;
            for(auto iter = s.rbegin(); iter != s.rend(); ++iter)
            {
                if(counter != 0 && counter % fmt.spacer == 0)
                {
                    spaced += '_';
                }
                spaced += *iter;
                counter += 1;
            }
            if(!spaced.empty() && spaced.back() == '_') {spaced.pop_back();}

            s.clear();
            std::copy(spaced.rbegin(), spaced.rend(), std::back_inserter(s));
            return sign + s;
        };

        std::string retval;
        if(fmt.fmt == integer_format::dec)
        {
            oss << std::setw(static_cast<int>(fmt.width)) << std::dec << i;
            retval = insert_spacer(oss.str());

            if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
            {
                retval += '_';
                retval += fmt.suffix;
            }
        }
        else
        {
            if(i < 0)
            {
                throw serialization_error(format_error("binary, octal, hexadecimal "
                    "integer does not allow negative value", loc, "here"), loc);
            }
            switch(fmt.fmt)
            {
                case integer_format::hex:
                {
                    oss << std::noshowbase
                        << std::setw(static_cast<int>(fmt.width))
                        << std::setfill('0')
                        << std::hex;
                    if(fmt.uppercase)
                    {
                        oss << std::uppercase;
                    }
                    else
                    {
                        oss << std::nouppercase;
                    }
                    oss << i;
                    retval = std::string("0x") + insert_spacer(oss.str());
                    break;
                }
                case integer_format::oct:
                {
                    oss << std::setw(static_cast<int>(fmt.width)) << std::setfill('0') << std::oct << i;
                    retval = std::string("0o") + insert_spacer(oss.str());
                    break;
                }
                case integer_format::bin:
                {
                    integer_type x{i};
                    std::string tmp;
                    std::size_t bits(0);
                    while(x != 0)
                    {
                        if(fmt.spacer != 0)
                        {
                            if(bits != 0 && (bits % fmt.spacer) == 0) {tmp += '_';}
                        }
                        if(x % 2 == 1) { tmp += '1'; } else { tmp += '0'; }
                        x >>= 1;
                        bits += 1;
                    }
                    for(; bits < fmt.width; ++bits)
                    {
                        if(fmt.spacer != 0)
                        {
                            if(bits != 0 && (bits % fmt.spacer) == 0) {tmp += '_';}
                        }
                        tmp += '0';
                    }
                    for(auto iter = tmp.rbegin(); iter != tmp.rend(); ++iter)
                    {
                        oss << *iter;
                    }
                    retval = std::string("0b") + oss.str();
                    break;
                }
                default:
                {
                    throw serialization_error(format_error(
                        "none of dec, hex, oct, bin: " + to_string(fmt.fmt),
                        loc, "here"), loc);
                }
            }
        }
        return string_conv<string_type>(retval);
    } // }}}

    string_type operator()(const floating_type f, const floating_format_info& fmt, const source_location&) // {{{
    {
        using std::isnan;
        using std::isinf;
        using std::signbit;

        std::ostringstream oss;
        this->set_locale(oss);

        if(isnan(f))
        {
            if(signbit(f))
            {
                oss << '-';
            }
            oss << "nan";
            if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
            {
                oss << '_';
                oss << fmt.suffix;
            }
            return string_conv<string_type>(oss.str());
        }

        if(isinf(f))
        {
            if(signbit(f))
            {
                oss << '-';
            }
            oss << "inf";
            if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
            {
                oss << '_';
                oss << fmt.suffix;
            }
            return string_conv<string_type>(oss.str());
        }

        switch(fmt.fmt)
        {
            case floating_format::defaultfloat:
            {
                if(fmt.prec != 0)
                {
                    oss << std::setprecision(static_cast<int>(fmt.prec));
                }
                oss << f;
                // since defaultfloat may omit point, we need to add it
                std::string s = oss.str();
                if (s.find('.') == std::string::npos &&
                    s.find('e') == std::string::npos &&
                    s.find('E') == std::string::npos )
                {
                    s += ".0";
                }
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    s += '_';
                    s += fmt.suffix;
                }
                return string_conv<string_type>(s);
            }
            case floating_format::fixed:
            {
                if(fmt.prec != 0)
                {
                    oss << std::setprecision(static_cast<int>(fmt.prec));
                }
                oss << std::fixed << f;
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    oss << '_' << fmt.suffix;
                }
                return string_conv<string_type>(oss.str());
            }
            case floating_format::scientific:
            {
                if(fmt.prec != 0)
                {
                    oss << std::setprecision(static_cast<int>(fmt.prec));
                }
                oss << std::scientific << f;
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    oss << '_' << fmt.suffix;
                }
                return string_conv<string_type>(oss.str());
            }
            case floating_format::hex:
            {
                if(this->spec_.ext_hex_float)
                {
                    oss << std::hexfloat << f;
                    // suffix is only for decimal numbers.
                    return string_conv<string_type>(oss.str());
                }
                else // no hex allowed. output with max precision.
                {
                    oss << std::setprecision(std::numeric_limits<floating_type>::max_digits10)
                        << std::scientific << f;
                    // suffix is only for decimal numbers.
                    return string_conv<string_type>(oss.str());
                }
            }
            default:
            {
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    oss << '_' << fmt.suffix;
                }
                return string_conv<string_type>(oss.str());
            }
        }
    } // }}}

    string_type operator()(string_type s, const string_format_info& fmt, const source_location& loc) // {{{
    {
        string_type retval;
        switch(fmt.fmt)
        {
            case string_format::basic:
            {
                retval += char_type('"');
                retval += this->escape_basic_string(s);
                retval += char_type('"');
                return retval;
            }
            case string_format::literal:
            {
                if(std::find(s.begin(), s.end(), char_type('\n')) != s.end())
                {
                    throw serialization_error(format_error("toml::serializer: "
                        "(non-multiline) literal string cannot have a newline",
                        loc, "here"), loc);
                }
                retval += char_type('\'');
                retval += s;
                retval += char_type('\'');
                return retval;
            }
            case string_format::multiline_basic:
            {
                retval += string_conv<string_type>("\"\"\"");
                if(fmt.start_with_newline)
                {
                    retval += char_type('\n');
                }

                retval += this->escape_ml_basic_string(s);

                retval += string_conv<string_type>("\"\"\"");
                return retval;
            }
            case string_format::multiline_literal:
            {
                retval += string_conv<string_type>("'''");
                if(fmt.start_with_newline)
                {
                    retval += char_type('\n');
                }
                retval += s;
                retval += string_conv<string_type>("'''");
                return retval;
            }
            default:
            {
                throw serialization_error(format_error(
                    "[error] toml::serializer::operator()(string): "
                    "invalid string_format value", loc, "here"), loc);
            }
        }
    } // }}}

    string_type operator()(const local_date_type& d, const local_date_format_info&, const source_location&) // {{{
    {
        std::ostringstream oss;
        oss << d;
        return string_conv<string_type>(oss.str());
    } // }}}

    string_type operator()(const local_time_type& t, const local_time_format_info& fmt, const source_location&) // {{{
    {
        return this->format_local_time(t, fmt.has_seconds, fmt.subsecond_precision);
    } // }}}

    string_type operator()(const local_datetime_type& dt, const local_datetime_format_info& fmt, const source_location&) // {{{
    {
        std::ostringstream oss;
        oss << dt.date;
        switch(fmt.delimiter)
        {
            case datetime_delimiter_kind::upper_T: { oss << 'T'; break; }
            case datetime_delimiter_kind::lower_t: { oss << 't'; break; }
            case datetime_delimiter_kind::space:   { oss << ' '; break; }
            default:                               { oss << 'T'; break; }
        }
        return string_conv<string_type>(oss.str()) +
            this->format_local_time(dt.time, fmt.has_seconds, fmt.subsecond_precision);
    } // }}}

    string_type operator()(const offset_datetime_type& odt, const offset_datetime_format_info& fmt, const source_location&) // {{{
    {
        std::ostringstream oss;
        oss << odt.date;
        switch(fmt.delimiter)
        {
            case datetime_delimiter_kind::upper_T: { oss << 'T'; break; }
            case datetime_delimiter_kind::lower_t: { oss << 't'; break; }
            case datetime_delimiter_kind::space:   { oss << ' '; break; }
            default:                               { oss << 'T'; break; }
        }
        oss << string_conv<std::string>(this->format_local_time(odt.time, fmt.has_seconds, fmt.subsecond_precision));
        oss << odt.offset;
        return string_conv<string_type>(oss.str());
    } // }}}

    string_type operator()(const array_type& a, const array_format_info& fmt, const comment_type& com, const source_location& loc) // {{{
    {
        array_format f = fmt.fmt;
        if(fmt.fmt == array_format::default_format)
        {
            // [[in.this.form]], you cannot add a comment to the array itself
            // (but you can add a comment to each table).
            // To keep comments, we need to avoid multiline array-of-tables
            // if array itself has a comment.
            if( ! this->keys_.empty() &&
                ! a.empty() &&
                com.empty() &&
                std::all_of(a.begin(), a.end(), [](const value_type& e) {return e.is_table();}))
            {
                f = array_format::array_of_tables;
            }
            else
            {
                f = array_format::oneline;

                // check if it becomes long
                std::size_t approx_len = 0;
                for(const auto& e : a)
                {
                    // have a comment. cannot be inlined
                    if( ! e.comments().empty())
                    {
                        f = array_format::multiline;
                        break;
                    }
                    // possibly long types ...
                    if(e.is_array() || e.is_table() || e.is_offset_datetime() || e.is_local_datetime())
                    {
                        f = array_format::multiline;
                        break;
                    }
                    else if(e.is_boolean())
                    {
                        approx_len += (*this)(e.as_boolean(), e.as_boolean_fmt(), e.location()).size();
                    }
                    else if(e.is_integer())
                    {
                        approx_len += (*this)(e.as_integer(), e.as_integer_fmt(), e.location()).size();
                    }
                    else if(e.is_floating())
                    {
                        approx_len += (*this)(e.as_floating(), e.as_floating_fmt(), e.location()).size();
                    }
                    else if(e.is_string())
                    {
                        if(e.as_string_fmt().fmt == string_format::multiline_basic ||
                           e.as_string_fmt().fmt == string_format::multiline_literal)
                        {
                            f = array_format::multiline;
                            break;
                        }
                        approx_len += 2 + (*this)(e.as_string(), e.as_string_fmt(), e.location()).size();
                    }
                    else if(e.is_local_date())
                    {
                        approx_len += 10; // 1234-56-78
                    }
                    else if(e.is_local_time())
                    {
                        approx_len += 15; // 12:34:56.789012
                    }

                    if(approx_len > 60) // key, ` = `, `[...]` < 80
                    {
                        f = array_format::multiline;
                        break;
                    }
                    approx_len += 2; // `, `
                }
            }
        }
        if(this->force_inline_ && f == array_format::array_of_tables)
        {
            f = array_format::multiline;
        }
        if(a.empty() && f == array_format::array_of_tables)
        {
            f = array_format::oneline;
        }

        // --------------------------------------------------------------------

        if(f == array_format::array_of_tables)
        {
            if(this->keys_.empty())
            {
                throw serialization_error("array of table must have its key. "
                        "use format(key, v)", loc);
            }
            string_type retval;
            for(const auto& e : a)
            {
                assert(e.is_table());

                this->current_indent_ += e.as_table_fmt().name_indent;
                retval += this->format_comments(e.comments(), e.as_table_fmt().indent_type);
                retval += this->format_indent(e.as_table_fmt().indent_type);
                this->current_indent_ -= e.as_table_fmt().name_indent;

                retval += string_conv<string_type>("[[");
                retval += this->format_keys(this->keys_).value();
                retval += string_conv<string_type>("]]\n");

                retval += this->format_ml_table(e.as_table(), e.as_table_fmt());
            }
            return retval;
        }
        else if(f == array_format::oneline)
        {
            // ignore comments. we cannot emit comments
            string_type retval;
            retval += char_type('[');
            for(const auto& e : a)
            {
                this->force_inline_ = true;
                retval += (*this)(e);
                retval += string_conv<string_type>(", ");
            }
            if( ! a.empty())
            {
                retval.pop_back(); // ` `
                retval.pop_back(); // `,`
            }
            retval += char_type(']');
            this->force_inline_ = false;
            return retval;
        }
        else
        {
            assert(f == array_format::multiline);

            string_type retval;
            retval += string_conv<string_type>("[\n");

            for(const auto& e : a)
            {
                this->current_indent_ += fmt.body_indent;
                retval += this->format_comments(e.comments(), fmt.indent_type);
                retval += this->format_indent(fmt.indent_type);
                this->current_indent_ -= fmt.body_indent;

                this->force_inline_ = true;
                retval += (*this)(e);
                retval += string_conv<string_type>(",\n");
            }
            this->force_inline_ = false;

            this->current_indent_ += fmt.closing_indent;
            retval += this->format_indent(fmt.indent_type);
            this->current_indent_ -= fmt.closing_indent;

            retval += char_type(']');
            return retval;
        }
    } // }}}

    string_type operator()(const table_type& t, const table_format_info& fmt, const comment_type& com, const source_location& loc) // {{{
    {
        if(this->force_inline_)
        {
            if(fmt.fmt == table_format::multiline_oneline)
            {
                return this->format_ml_inline_table(t, fmt);
            }
            else
            {
                return this->format_inline_table(t, fmt);
            }
        }
        else
        {
            if(fmt.fmt == table_format::multiline)
            {
                string_type retval;
                // comment is emitted inside format_ml_table
                if(auto k = this->format_keys(this->keys_))
                {
                    this->current_indent_ += fmt.name_indent;
                    retval += this->format_comments(com, fmt.indent_type);
                    retval += this->format_indent(fmt.indent_type);
                    this->current_indent_ -= fmt.name_indent;
                    retval += char_type('[');
                    retval += k.value();
                    retval += string_conv<string_type>("]\n");
                }
                // otherwise, its the root.

                retval += this->format_ml_table(t, fmt);
                return retval;
            }
            else if(fmt.fmt == table_format::oneline)
            {
                return this->format_inline_table(t, fmt);
            }
            else if(fmt.fmt == table_format::multiline_oneline)
            {
                return this->format_ml_inline_table(t, fmt);
            }
            else if(fmt.fmt == table_format::dotted)
            {
                std::vector<string_type> keys;
                if(this->keys_.empty())
                {
                    throw serialization_error(format_error("toml::serializer: "
                        "dotted table must have its key. use format(key, v)",
                        loc, "here"), loc);
                }
                keys.push_back(this->keys_.back());

                const auto retval = this->format_dotted_table(t, fmt, loc, keys);
                keys.pop_back();
                return retval;
            }
            else
            {
                assert(fmt.fmt == table_format::implicit);

                string_type retval;
                for(const auto& kv : t)
                {
                    const auto& k = kv.first;
                    const auto& v = kv.second;

                    if( ! v.is_table() && ! v.is_array_of_tables())
                    {
                        throw serialization_error(format_error("toml::serializer: "
                            "an implicit table cannot have non-table value.",
                            v.location(), "here"), v.location());
                    }
                    if(v.is_table())
                    {
                        if(v.as_table_fmt().fmt != table_format::multiline &&
                           v.as_table_fmt().fmt != table_format::implicit)
                        {
                            throw serialization_error(format_error("toml::serializer: "
                                "an implicit table cannot have non-multiline table",
                                v.location(), "here"), v.location());
                        }
                    }
                    else
                    {
                        assert(v.is_array());
                        for(const auto& e : v.as_array())
                        {
                            if(e.as_table_fmt().fmt != table_format::multiline &&
                               v.as_table_fmt().fmt != table_format::implicit)
                            {
                                throw serialization_error(format_error("toml::serializer: "
                                    "an implicit table cannot have non-multiline table",
                                    e.location(), "here"), e.location());
                            }
                        }
                    }

                    keys_.push_back(k);
                    retval += (*this)(v);
                    keys_.pop_back();
                }
                return retval;
            }
        }
    } // }}}

  private:

    string_type escape_basic_string(const string_type& s) const // {{{
    {
        string_type retval;
        for(const char_type c : s)
        {
            switch(c)
            {
                case char_type('\\'): {retval += string_conv<string_type>("\\\\"); break;}
                case char_type('\"'): {retval += string_conv<string_type>("\\\""); break;}
                case char_type('\b'): {retval += string_conv<string_type>("\\b" ); break;}
                case char_type('\t'): {retval += string_conv<string_type>("\\t" ); break;}
                case char_type('\f'): {retval += string_conv<string_type>("\\f" ); break;}
                case char_type('\n'): {retval += string_conv<string_type>("\\n" ); break;}
                case char_type('\r'): {retval += string_conv<string_type>("\\r" ); break;}
                default  :
                {
                    if(c == char_type(0x1B) && spec_.v1_1_0_add_escape_sequence_e)
                    {
                        retval += string_conv<string_type>("\\e");
                    }
                    else if((char_type(0x00) <= c && c <= char_type(0x08)) ||
                            (char_type(0x0A) <= c && c <= char_type(0x1F)) ||
                             c == char_type(0x7F))
                    {
                        if(spec_.v1_1_0_add_escape_sequence_x)
                        {
                            retval += string_conv<string_type>("\\x");
                        }
                        else
                        {
                            retval += string_conv<string_type>("\\u00");
                        }
                        const auto c1 = c / 16;
                        const auto c2 = c % 16;
                        retval += static_cast<char_type>('0' + c1);
                        if(c2 < 10)
                        {
                            retval += static_cast<char_type>('0' + c2);
                        }
                        else // 10 <= c2
                        {
                            retval += static_cast<char_type>('A' + (c2 - 10));
                        }
                    }
                    else
                    {
                        retval += c;
                    }
                }
            }
        }
        return retval;
    } // }}}

    string_type escape_ml_basic_string(const string_type& s) // {{{
    {
        string_type retval;
        for(const char_type c : s)
        {
            switch(c)
            {
                case char_type('\\'): {retval += string_conv<string_type>("\\\\"); break;}
                case char_type('\b'): {retval += string_conv<string_type>("\\b" ); break;}
                case char_type('\t'): {retval += string_conv<string_type>("\\t" ); break;}
                case char_type('\f'): {retval += string_conv<string_type>("\\f" ); break;}
                case char_type('\n'): {retval += string_conv<string_type>("\n"  ); break;}
                case char_type('\r'): {retval += string_conv<string_type>("\\r" ); break;}
                default  :
                {
                    if(c == char_type(0x1B) && spec_.v1_1_0_add_escape_sequence_e)
                    {
                        retval += string_conv<string_type>("\\e");
                    }
                    else if((char_type(0x00) <= c && c <= char_type(0x08)) ||
                            (char_type(0x0A) <= c && c <= char_type(0x1F)) ||
                            c == char_type(0x7F))
                    {
                        if(spec_.v1_1_0_add_escape_sequence_x)
                        {
                            retval += string_conv<string_type>("\\x");
                        }
                        else
                        {
                            retval += string_conv<string_type>("\\u00");
                        }
                        const auto c1 = c / 16;
                        const auto c2 = c % 16;
                        retval += static_cast<char_type>('0' + c1);
                        if(c2 < 10)
                        {
                            retval += static_cast<char_type>('0' + c2);
                        }
                        else // 10 <= c2
                        {
                            retval += static_cast<char_type>('A' + (c2 - 10));
                        }
                    }
                    else
                    {
                        retval += c;
                    }
                }
            }
        }
        // Only 1 or 2 consecutive `"`s are allowed in multiline basic string.
        // 3 consecutive `"`s are considered as a closing delimiter.
        // We need to check if there are 3 or more consecutive `"`s and insert
        // backslash to break them down into several short `"`s like the `str6`
        // in the following example.
        // ```toml
        // str4 = """Here are two quotation marks: "". Simple enough."""
        // # str5 = """Here are three quotation marks: """."""  # INVALID
        // str5 = """Here are three quotation marks: ""\"."""
        // str6 = """Here are fifteen quotation marks: ""\"""\"""\"""\"""\"."""
        // ```
        auto found_3_quotes = retval.find(string_conv<string_type>("\"\"\""));
        while(found_3_quotes != string_type::npos)
        {
            retval.replace(found_3_quotes, 3, string_conv<string_type>("\"\"\\\""));
            found_3_quotes = retval.find(string_conv<string_type>("\"\"\""));
        }
        return retval;
    } // }}}

    string_type format_local_time(const local_time_type& t, const bool has_seconds, const std::size_t subsec_prec) // {{{
    {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << static_cast<int>(t.hour);
        oss << ':';
        oss << std::setfill('0') << std::setw(2) << static_cast<int>(t.minute);
        if(has_seconds)
        {
            oss << ':';
            oss << std::setfill('0') << std::setw(2) << static_cast<int>(t.second);
            if(subsec_prec != 0)
            {
                std::ostringstream subsec;
                subsec << std::setfill('0') << std::setw(3) << static_cast<int>(t.millisecond);
                subsec << std::setfill('0') << std::setw(3) << static_cast<int>(t.microsecond);
                subsec << std::setfill('0') << std::setw(3) << static_cast<int>(t.nanosecond);
                std::string subsec_str = subsec.str();
                oss << '.' << subsec_str.substr(0, subsec_prec);
            }
        }
        return string_conv<string_type>(oss.str());
    } // }}}

    string_type format_ml_table(const table_type& t, const table_format_info& fmt) // {{{
    {
        const auto format_later = [](const value_type& v) -> bool {

            const bool is_ml_table = v.is_table() &&
                v.as_table_fmt().fmt != table_format::oneline           &&
                v.as_table_fmt().fmt != table_format::multiline_oneline &&
                v.as_table_fmt().fmt != table_format::dotted ;

            const bool is_ml_array_table = v.is_array_of_tables() &&
                v.as_array_fmt().fmt != array_format::oneline &&
                v.as_array_fmt().fmt != array_format::multiline;

            return is_ml_table || is_ml_array_table;
        };

        string_type retval;
        this->current_indent_ += fmt.body_indent;
        for(const auto& kv : t)
        {
            const auto& key = kv.first;
            const auto& val = kv.second;
            if(format_later(val))
            {
                continue;
            }
            this->keys_.push_back(key);

            retval += format_comments(val.comments(), fmt.indent_type);
            retval += format_indent(fmt.indent_type);
            if(val.is_table() && val.as_table_fmt().fmt == table_format::dotted)
            {
                retval += (*this)(val);
            }
            else
            {
                retval += format_key(key);
                retval += string_conv<string_type>(" = ");
                retval += (*this)(val);
                retval += char_type('\n');
            }
            this->keys_.pop_back();
        }
        this->current_indent_ -= fmt.body_indent;

        if( ! retval.empty())
        {
            retval += char_type('\n'); // for readability, add empty line between tables
        }
        for(const auto& kv : t)
        {
            if( ! format_later(kv.second))
            {
                continue;
            }
            // must be a [multiline.table] or [[multiline.array.of.tables]].
            // comments will be generated inside it.
            this->keys_.push_back(kv.first);
            retval += (*this)(kv.second);
            this->keys_.pop_back();
        }
        return retval;
    } // }}}

    string_type format_inline_table(const table_type& t, const table_format_info&) // {{{
    {
        // comments are ignored because we cannot write without newline
        string_type retval;
        retval += char_type('{');
        for(const auto& kv : t)
        {
            this->force_inline_ = true;
            retval += this->format_key(kv.first);
            retval += string_conv<string_type>(" = ");
            retval += (*this)(kv.second);
            retval += string_conv<string_type>(", ");
        }
        if( ! t.empty())
        {
            retval.pop_back(); // ' '
            retval.pop_back(); // ','
        }
        retval += char_type('}');
        this->force_inline_ = false;
        return retval;
    } // }}}

    string_type format_ml_inline_table(const table_type& t, const table_format_info& fmt) // {{{
    {
        string_type retval;
        retval += string_conv<string_type>("{\n");
        this->current_indent_ += fmt.body_indent;
        for(const auto& kv : t)
        {
            this->force_inline_ = true;
            retval += format_comments(kv.second.comments(), fmt.indent_type);
            retval += format_indent(fmt.indent_type);
            retval += kv.first;
            retval += string_conv<string_type>(" = ");

            this->force_inline_ = true;
            retval += (*this)(kv.second);

            retval += string_conv<string_type>(",\n");
        }
        if( ! t.empty())
        {
            retval.pop_back(); // '\n'
            retval.pop_back(); // ','
        }
        this->current_indent_ -= fmt.body_indent;
        this->force_inline_ = false;

        this->current_indent_ += fmt.closing_indent;
        retval += format_indent(fmt.indent_type);
        this->current_indent_ -= fmt.closing_indent;

        retval += char_type('}');
        return retval;
    } // }}}

    string_type format_dotted_table(const table_type& t, const table_format_info& fmt, // {{{
            const source_location&, std::vector<string_type>& keys)
    {
        // lets say we have: `{"a": {"b": {"c": {"d": "foo", "e": "bar"} } }`
        // and `a` and `b` are `dotted`.
        //
        // - in case if `c` is `oneline`:
        // ```toml
        // a.b.c = {d = "foo", e = "bar"}
        // ```
        //
        // - in case if and `c` is `dotted`:
        // ```toml
        // a.b.c.d = "foo"
        // a.b.c.e = "bar"
        // ```

        string_type retval;

        for(const auto& kv : t)
        {
            const auto& key = kv.first;
            const auto& val = kv.second;

            keys.push_back(key);

            // format recursive dotted table?
            if (val.is_table() &&
                val.as_table_fmt().fmt != table_format::oneline &&
                val.as_table_fmt().fmt != table_format::multiline_oneline)
            {
                retval += this->format_dotted_table(val.as_table(), val.as_table_fmt(), val.location(), keys);
            }
            else // non-table or inline tables. format normally
            {
                retval += format_comments(val.comments(), fmt.indent_type);
                retval += format_indent(fmt.indent_type);
                retval += format_keys(keys).value();
                retval += string_conv<string_type>(" = ");
                this->force_inline_ = true; // sub-table must be inlined
                retval += (*this)(val);
                retval += char_type('\n');
                this->force_inline_ = false;
            }
            keys.pop_back();
        }
        return retval;
    } // }}}

    string_type format_key(const key_type& key) // {{{
    {
        if(key.empty())
        {
            return string_conv<string_type>("\"\"");
        }

        // check the key can be a bare (unquoted) key
        auto loc = detail::make_temporary_location(string_conv<std::string>(key));
        auto reg = detail::syntax::unquoted_key(this->spec_).scan(loc);
        if(reg.is_ok() && loc.eof())
        {
            return key;
        }

        //if it includes special characters, then format it in a "quoted" key.
        string_type formatted = string_conv<string_type>("\"");
        for(const char_type c : key)
        {
            switch(c)
            {
                case char_type('\\'): {formatted += string_conv<string_type>("\\\\"); break;}
                case char_type('\"'): {formatted += string_conv<string_type>("\\\""); break;}
                case char_type('\b'): {formatted += string_conv<string_type>("\\b" ); break;}
                case char_type('\t'): {formatted += string_conv<string_type>("\\t" ); break;}
                case char_type('\f'): {formatted += string_conv<string_type>("\\f" ); break;}
                case char_type('\n'): {formatted += string_conv<string_type>("\\n" ); break;}
                case char_type('\r'): {formatted += string_conv<string_type>("\\r" ); break;}
                default  :
                {
                    // ASCII ctrl char
                    if( (char_type(0x00) <= c && c <= char_type(0x08)) ||
                        (char_type(0x0A) <= c && c <= char_type(0x1F)) ||
                        c == char_type(0x7F))
                    {
                        if(spec_.v1_1_0_add_escape_sequence_x)
                        {
                            formatted += string_conv<string_type>("\\x");
                        }
                        else
                        {
                            formatted += string_conv<string_type>("\\u00");
                        }
                        const auto c1 = c / 16;
                        const auto c2 = c % 16;
                        formatted += static_cast<char_type>('0' + c1);
                        if(c2 < 10)
                        {
                            formatted += static_cast<char_type>('0' + c2);
                        }
                        else // 10 <= c2
                        {
                            formatted += static_cast<char_type>('A' + (c2 - 10));
                        }
                    }
                    else
                    {
                        formatted += c;
                    }
                    break;
                }
            }
        }
        formatted += string_conv<string_type>("\"");
        return formatted;
    } // }}}
    cxx::optional<string_type> format_keys(const std::vector<key_type>& keys) // {{{
    {
        if(keys.empty())
        {
            return cxx::make_nullopt();
        }

        string_type formatted;
        for(const auto& ky : keys)
        {
            formatted += format_key(ky);
            formatted += char_type('.');
        }
        formatted.pop_back(); // remove the last dot '.'
        return formatted;
    } // }}}

    string_type format_comments(const discard_comments&, const indent_char) const // {{{
    {
        return string_conv<string_type>("");
    } // }}}
    string_type format_comments(const preserve_comments& comments, const indent_char indent_type) const // {{{
    {
        string_type retval;
        for(const auto& c : comments)
        {
            if(c.empty()) {continue;}
            retval += format_indent(indent_type);
            if(c.front() != '#') {retval += char_type('#');}
            retval += string_conv<string_type>(c);
            if(c.back() != '\n') {retval += char_type('\n');}
        }
        return retval;
    } // }}}

    string_type format_indent(const indent_char indent_type) const // {{{
    {
        const auto indent = static_cast<std::size_t>((std::max)(0, this->current_indent_));
        if(indent_type == indent_char::space)
        {
            return string_conv<string_type>(make_string(indent, ' '));
        }
        else if(indent_type == indent_char::tab)
        {
            return string_conv<string_type>(make_string(indent, '\t'));
        }
        else
        {
            return string_type{};
        }
    } // }}}

    std::locale set_locale(std::ostream& os) const
    {
        return os.imbue(std::locale::classic());
    }

  private:

    spec spec_;
    bool force_inline_; // table inside an array without fmt specification
    std::int32_t current_indent_;
    std::vector<key_type> keys_;
};
} // detail

template<typename TC>
typename basic_value<TC>::string_type
format(const basic_value<TC>& v, const spec s = spec::default_version())
{
    detail::serializer<TC> ser(s);
    return ser(v);
}
template<typename TC>
typename basic_value<TC>::string_type
format(const typename basic_value<TC>::key_type& k,
       const basic_value<TC>& v,
       const spec s = spec::default_version())
{
    detail::serializer<TC> ser(s);
    return ser(k, v);
}
template<typename TC>
typename basic_value<TC>::string_type
format(const std::vector<typename basic_value<TC>::key_type>& ks,
       const basic_value<TC>& v,
       const spec s = spec::default_version())
{
    detail::serializer<TC> ser(s);
    return ser(ks, v);
}

template<typename TC>
std::ostream& operator<<(std::ostream& os, const basic_value<TC>& v)
{
    os << format(v);
    return os;
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
struct type_config;
struct ordered_type_config;

extern template typename basic_value<type_config>::string_type
format<type_config>(const basic_value<type_config>&, const spec);

extern template typename basic_value<type_config>::string_type
format<type_config>(const typename basic_value<type_config>::key_type& k,
                    const basic_value<type_config>& v, const spec);

extern template typename basic_value<type_config>::string_type
format<type_config>(const std::vector<typename basic_value<type_config>::key_type>& ks,
                    const basic_value<type_config>& v, const spec s);

extern template typename basic_value<type_config>::string_type
format<ordered_type_config>(const basic_value<ordered_type_config>&, const spec);

extern template typename basic_value<type_config>::string_type
format<ordered_type_config>(const typename basic_value<ordered_type_config>::key_type& k,
                            const basic_value<ordered_type_config>& v, const spec);

extern template typename basic_value<type_config>::string_type
format<ordered_type_config>(const std::vector<typename basic_value<ordered_type_config>::key_type>& ks,
                            const basic_value<ordered_type_config>& v, const spec s);

namespace detail
{
extern template class serializer<::toml::type_config>;
extern template class serializer<::toml::ordered_type_config>;
} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_COMPILE_SOURCES


#endif // TOML11_SERIALIZER_HPP
#ifndef TOML11_GET_HPP
#define TOML11_GET_HPP

#include <algorithm>


#if defined(TOML11_HAS_STRING_VIEW)
#include <string_view>
#endif // string_view

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// ============================================================================
// T is toml::value; identity transformation.

template<typename T, typename TC>
cxx::enable_if_t<std::is_same<T, basic_value<TC>>::value, T>&
get(basic_value<TC>& v)
{
    return v;
}

template<typename T, typename TC>
cxx::enable_if_t<std::is_same<T, basic_value<TC>>::value, T> const&
get(const basic_value<TC>& v)
{
    return v;
}

template<typename T, typename TC>
cxx::enable_if_t<std::is_same<T, basic_value<TC>>::value, T>
get(basic_value<TC>&& v)
{
    return basic_value<TC>(std::move(v));
}

// ============================================================================
// exact toml::* type

template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value, T> &
get(basic_value<TC>& v)
{
    constexpr auto ty = detail::type_to_enum<T, basic_value<TC>>::value;
    return detail::getter<TC, ty>::get(v);
}

template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value, T> const&
get(const basic_value<TC>& v)
{
    constexpr auto ty = detail::type_to_enum<T, basic_value<TC>>::value;
    return detail::getter<TC, ty>::get(v);
}

template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value, T>
get(basic_value<TC>&& v)
{
    constexpr auto ty = detail::type_to_enum<T, basic_value<TC>>::value;
    return detail::getter<TC, ty>::get(std::move(v));
}

// ============================================================================
// T is toml::basic_value<U>

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_basic_value<T>,
    cxx::negation<std::is_same<T, basic_value<TC>>>
    >::value, T>
get(basic_value<TC> v)
{
    return T(std::move(v));
}

// ============================================================================
// integer convertible from toml::value::integer_type

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_integral<T>,
    cxx::negation<std::is_same<T, bool>>,
    detail::is_not_toml_type<T, basic_value<TC>>,
    cxx::negation<detail::has_from_toml_method<T, TC>>,
    cxx::negation<detail::has_specialized_from<T>>
    >::value, T>
get(const basic_value<TC>& v)
{
    return static_cast<T>(v.as_integer());
}

// ============================================================================
// floating point convertible from toml::value::floating_type

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_floating_point<T>,
    detail::is_not_toml_type<T, basic_value<TC>>,
    cxx::negation<detail::has_from_toml_method<T, TC>>,
    cxx::negation<detail::has_specialized_from<T>>
    >::value, T>
get(const basic_value<TC>& v)
{
    return static_cast<T>(v.as_floating());
}

// ============================================================================
// std::string with different char/trait/allocator

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_not_toml_type<T, basic_value<TC>>,
    detail::is_1byte_std_basic_string<T>
    >::value, T>
get(const basic_value<TC>& v)
{
    return detail::string_conv<cxx::remove_cvref_t<T>>(v.as_string());
}

// ============================================================================
// std::string_view

#if defined(TOML11_HAS_STRING_VIEW)

template<typename T, typename TC>
cxx::enable_if_t<detail::is_string_view_of<T, typename basic_value<TC>::string_type>::value, T>
get(const basic_value<TC>& v)
{
    return T(v.as_string());
}

#endif // string_view

// ============================================================================
// std::chrono::duration from toml::local_time

template<typename T, typename TC>
cxx::enable_if_t<detail::is_chrono_duration<T>::value, T>
get(const basic_value<TC>& v)
{
    return std::chrono::duration_cast<T>(
            std::chrono::nanoseconds(v.as_local_time()));
}

// ============================================================================
// std::chrono::system_clock::time_point from toml::datetime variants

template<typename T, typename TC>
cxx::enable_if_t<
    std::is_same<std::chrono::system_clock::time_point, T>::value, T>
get(const basic_value<TC>& v)
{
    switch(v.type())
    {
        case value_t::local_date:
        {
            return std::chrono::system_clock::time_point(v.as_local_date());
        }
        case value_t::local_datetime:
        {
            return std::chrono::system_clock::time_point(v.as_local_datetime());
        }
        case value_t::offset_datetime:
        {
            return std::chrono::system_clock::time_point(v.as_offset_datetime());
        }
        default:
        {
            const auto loc = v.location();
            throw type_error(format_error("toml::get: "
                "bad_cast to std::chrono::system_clock::time_point", loc,
                "the actual type is " + to_string(v.type())), loc);
        }
    }
}

// ============================================================================
// forward declaration to use this recursively. ignore this and go ahead.

// array-like (w/ push_back)
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_container<T>,                            // T is a container
    detail::has_push_back_method<T>,                    // .push_back() works
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::array
    cxx::negation<detail::is_std_basic_string<T>>,      // but not std::basic_string<CharT>
#if defined(TOML11_HAS_STRING_VIEW)
    cxx::negation<detail::is_std_basic_string_view<T>>,      // but not std::basic_string_view<CharT>
#endif
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>&);

// std::array
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_array<T>::value, T>
get(const basic_value<TC>&);

// std::forward_list
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_forward_list<T>::value, T>
get(const basic_value<TC>&);

// std::pair<T1, T2>
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_pair<T>::value, T>
get(const basic_value<TC>&);

// std::tuple<T1, T2, ...>
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_tuple<T>::value, T>
get(const basic_value<TC>&);

// std::map<key, value> (key is convertible from toml::value::key_type)
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                  // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::table
    std::is_convertible<typename basic_value<TC>::key_type,
                        typename T::key_type>,          // keys are convertible
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v);

// std::map<key, value> (key is not convertible from toml::value::key_type, but
// is a std::basic_string)
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                  // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::table
    cxx::negation<std::is_convertible<typename basic_value<TC>::key_type,
        typename T::key_type>>,                         // keys are NOT convertible
    detail::is_1byte_std_basic_string<typename T::key_type>, // is std::basic_string
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v);

// toml::from<T>::from_toml(v)
template<typename T, typename TC>
cxx::enable_if_t<detail::has_specialized_from<T>::value, T>
get(const basic_value<TC>&);

// has T.from_toml(v) but no from<T>
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::has_from_toml_method<T, TC>,            // has T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>, // no toml::from<T>
    std::is_default_constructible<T>                // T{} works
    >::value, T>
get(const basic_value<TC>&);

// T(const toml::value&) and T is not toml::basic_value,
// and it does not have `from<T>` nor `from_toml`.
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_constructible<T, const basic_value<TC>&>,   // has T(const basic_value&)
    cxx::negation<detail::is_basic_value<T>>,           // but not basic_value itself
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no .from_toml()
    cxx::negation<detail::has_specialized_from<T>>      // no toml::from<T>
    >::value, T>
get(const basic_value<TC>&);

// ============================================================================
// array-like types; most likely STL container, like std::vector, etc.

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_container<T>,                            // T is a container
    detail::has_push_back_method<T>,                    // .push_back() works
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::array
    cxx::negation<detail::is_std_basic_string<T>>,      // but not std::basic_string<CharT>
#if defined(TOML11_HAS_STRING_VIEW)
    cxx::negation<detail::is_std_basic_string_view<T>>, // but not std::basic_string_view<CharT>
#endif
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v)
{
    using value_type = typename T::value_type;
    const auto& a = v.as_array();

    T container;
    detail::try_reserve(container, a.size()); // if T has .reserve(), call it

    for(const auto& elem : a)
    {
        container.push_back(get<value_type>(elem));
    }
    return container;
}

// ============================================================================
// std::array

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_array<T>::value, T>
get(const basic_value<TC>& v)
{
    using value_type = typename T::value_type;
    const auto& a = v.as_array();

    T container;
    if(a.size() != container.size())
    {
        const auto loc = v.location();
        throw std::out_of_range(format_error("toml::get: while converting to an array: "
            " array size is " + std::to_string(container.size()) +
            " but there are " + std::to_string(a.size()) + " elements in toml array.",
            loc, "here"));
    }
    for(std::size_t i=0; i<a.size(); ++i)
    {
        container.at(i) = ::toml::get<value_type>(a.at(i));
    }
    return container;
}

// ============================================================================
// std::forward_list

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_forward_list<T>::value, T>
get(const basic_value<TC>& v)
{
    using value_type = typename T::value_type;

    T container;
    for(const auto& elem : v.as_array())
    {
        container.push_front(get<value_type>(elem));
    }
    container.reverse();
    return container;
}

// ============================================================================
// std::pair

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_pair<T>::value, T>
get(const basic_value<TC>& v)
{
    using first_type  = typename T::first_type;
    using second_type = typename T::second_type;

    const auto& ar = v.as_array();
    if(ar.size() != 2)
    {
        const auto loc = v.location();
        throw std::out_of_range(format_error("toml::get: while converting std::pair: "
            " but there are " + std::to_string(ar.size()) + " > 2 elements in toml array.",
            loc, "here"));
    }
    return std::make_pair(::toml::get<first_type >(ar.at(0)),
                          ::toml::get<second_type>(ar.at(1)));
}

// ============================================================================
// std::tuple.

namespace detail
{
template<typename T, typename Array, std::size_t ... I>
T get_tuple_impl(const Array& a, cxx::index_sequence<I...>)
{
    return std::make_tuple(
        ::toml::get<typename std::tuple_element<I, T>::type>(a.at(I))...);
}
} // detail

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_tuple<T>::value, T>
get(const basic_value<TC>& v)
{
    const auto& ar = v.as_array();
    if(ar.size() != std::tuple_size<T>::value)
    {
        const auto loc = v.location();
        throw std::out_of_range(format_error("toml::get: while converting std::tuple: "
            " there are " + std::to_string(ar.size()) + " > " +
            std::to_string(std::tuple_size<T>::value) + " elements in toml array.",
            loc, "here"));
    }
    return detail::get_tuple_impl<T>(ar,
            cxx::make_index_sequence<std::tuple_size<T>::value>{});
}

// ============================================================================
// std::unordered_set

template<typename T, typename TC>
cxx::enable_if_t<toml::detail::is_unordered_set<T>::value, T>
get(const basic_value<TC>& v)
{
    using value_type = typename T::value_type;
    const auto& a = v.as_array();

    T container;
    for (const auto& elem : a)
    {
        container.insert(get<value_type>(elem));
    }
    return container;
}


// ============================================================================
// map-like types; most likely STL map, like std::map or std::unordered_map.

// key is convertible from toml::value::key_type
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                  // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,       // but not toml::table
    std::is_convertible<typename basic_value<TC>::key_type,
                        typename T::key_type>,          // keys are convertible
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,     // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v)
{
    using key_type    = typename T::key_type;
    using mapped_type = typename T::mapped_type;
    static_assert(
        std::is_convertible<typename basic_value<TC>::key_type, key_type>::value,
        "toml::get only supports map type of which key_type is "
        "convertible from toml::basic_value::key_type.");

    T m;
    for(const auto& kv : v.as_table())
    {
        m.emplace(key_type(kv.first), get<mapped_type>(kv.second));
    }
    return m;
}

// key is NOT convertible from toml::value::key_type but std::basic_string
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::is_map<T>,                                       // T is map
    detail::is_not_toml_type<T, basic_value<TC>>,            // but not toml::table
    cxx::negation<std::is_convertible<typename basic_value<TC>::key_type,
        typename T::key_type>>,                              // keys are NOT convertible
    detail::is_1byte_std_basic_string<typename T::key_type>, // is std::basic_string
    cxx::negation<detail::has_from_toml_method<T, TC>>,      // no T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>,          // no toml::from<T>
    cxx::negation<std::is_constructible<T, const basic_value<TC>&>>
    >::value, T>
get(const basic_value<TC>& v)
{
    using key_type    = typename T::key_type;
    using mapped_type = typename T::mapped_type;

    T m;
    for(const auto& kv : v.as_table())
    {
        m.emplace(detail::string_conv<key_type>(kv.first), get<mapped_type>(kv.second));
    }
    return m;
}

// ============================================================================
// user-defined, but convertible types.

// toml::from<T>
template<typename T, typename TC>
cxx::enable_if_t<detail::has_specialized_from<T>::value, T>
get(const basic_value<TC>& v)
{
    return ::toml::from<T>::from_toml(v);
}

// has T.from_toml(v) but no from<T>
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    detail::has_from_toml_method<T, TC>,            // has T.from_toml()
    cxx::negation<detail::has_specialized_from<T>>, // no toml::from<T>
    std::is_default_constructible<T>                // T{} works
    >::value, T>
get(const basic_value<TC>& v)
{
    T ud;
    ud.from_toml(v);
    return ud;
}

// T(const toml::value&) and T is not toml::basic_value,
// and it does not have `from<T>` nor `from_toml`.
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    std::is_constructible<T, const basic_value<TC>&>,   // has T(const basic_value&)
    cxx::negation<detail::is_basic_value<T>>,           // but not basic_value itself
    cxx::negation<detail::has_from_toml_method<T, TC>>, // no .from_toml()
    cxx::negation<detail::has_specialized_from<T>>      // no toml::from<T>
    >::value, T>
get(const basic_value<TC>& v)
{
    return T(v);
}

// ============================================================================
// get_or(value, fallback)

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
get_or(const basic_value<TC>& v, const basic_value<TC>&)
{
    return v;
}

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
get_or(basic_value<TC>& v, basic_value<TC>&)
{
    return v;
}

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
get_or(basic_value<TC>&& v, basic_value<TC>&&)
{
    return v;
}

// ----------------------------------------------------------------------------
// specialization for the exact toml types (return type becomes lvalue ref)

template<typename T, typename TC>
cxx::enable_if_t<
    detail::is_exact_toml_type<T, basic_value<TC>>::value, T> const&
get_or(const basic_value<TC>& v, const T& opt) noexcept
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(v);
    }
    catch(...)
    {
        return opt;
    }
}
template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
        cxx::negation<std::is_const<T>>,
        detail::is_exact_toml_type<T, basic_value<TC>>
    >::value, T>&
get_or(basic_value<TC>& v, T& opt) noexcept
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(v);
    }
    catch(...)
    {
        return opt;
    }
}
template<typename T, typename TC>
cxx::enable_if_t<detail::is_exact_toml_type<cxx::remove_cvref_t<T>,
    basic_value<TC>>::value, cxx::remove_cvref_t<T>>
get_or(basic_value<TC>&& v, T&& opt) noexcept
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(std::move(v));
    }
    catch(...)
    {
        return cxx::remove_cvref_t<T>(std::forward<T>(opt));
    }
}

// ----------------------------------------------------------------------------
// specialization for string literal

// template<std::size_t N, typename TC>
// typename basic_value<TC>::string_type
// get_or(const basic_value<TC>& v,
//        const typename basic_value<TC>::string_type::value_type (&opt)[N])
// {
//     try
//     {
//         return v.as_string();
//     }
//     catch(...)
//     {
//         return typename basic_value<TC>::string_type(opt);
//     }
// }
//
// The above only matches to the literal, like `get_or(v, "foo");` but not
// ```cpp
// const auto opt = "foo";
// const auto str = get_or(v, opt);
// ```
// . And the latter causes an error.
// To match to both `"foo"` and `const auto opt = "foo"`, we take a pointer to
// a character here.

template<typename TC>
typename basic_value<TC>::string_type
get_or(const basic_value<TC>& v,
       const typename basic_value<TC>::string_type::value_type* opt)
{
    try
    {
        return v.as_string();
    }
    catch(...)
    {
        return typename basic_value<TC>::string_type(opt);
    }
}

// ----------------------------------------------------------------------------
// others (require type conversion and return type cannot be lvalue reference)

template<typename T, typename TC>
cxx::enable_if_t<cxx::conjunction<
    cxx::negation<detail::is_basic_value<T>>,
    cxx::negation<detail::is_exact_toml_type<T, basic_value<TC>>>,
    cxx::negation<std::is_same<cxx::remove_cvref_t<T>, typename basic_value<TC>::string_type::value_type const*>>
    >::value, cxx::remove_cvref_t<T>>
get_or(const basic_value<TC>& v, T&& opt)
{
    try
    {
        return get<cxx::remove_cvref_t<T>>(v);
    }
    catch(...)
    {
        return cxx::remove_cvref_t<T>(std::forward<T>(opt));
    }
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_GET_HPP
#ifndef TOML11_FIND_HPP
#define TOML11_FIND_HPP

#include <algorithm>


#if defined(TOML11_HAS_STRING_VIEW)
#include <string_view>
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

// ----------------------------------------------------------------------------
// find<T>(value, key);

template<typename T, typename TC>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC> const&>()))>
find(const basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    return ::toml::get<T>(v.at(ky));
}

template<typename T, typename TC>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC>&>()))>
find(basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    return ::toml::get<T>(v.at(ky));
}

template<typename T, typename TC>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC>&&>()))>
find(basic_value<TC>&& v, const typename basic_value<TC>::key_type& ky)
{
    return ::toml::get<T>(std::move(v.at(ky)));
}

// ----------------------------------------------------------------------------
// find<T>(value, idx)

template<typename T, typename TC>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC> const&>()))>
find(const basic_value<TC>& v, const std::size_t idx)
{
    return ::toml::get<T>(v.at(idx));
}
template<typename T, typename TC>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC>&>()))>
find(basic_value<TC>& v, const std::size_t idx)
{
    return ::toml::get<T>(v.at(idx));
}
template<typename T, typename TC>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC>&&>()))>
find(basic_value<TC>&& v, const std::size_t idx)
{
    return ::toml::get<T>(std::move(v.at(idx)));
}

// ----------------------------------------------------------------------------
// find(value, key/idx), w/o conversion

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find(basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    return v.at(ky);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find(basic_value<TC> const& v, const typename basic_value<TC>::key_type& ky)
{
    return v.at(ky);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find(basic_value<TC>&& v, const typename basic_value<TC>::key_type& ky)
{
    return basic_value<TC>(std::move(v.at(ky)));
}

template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find(basic_value<TC>& v, const std::size_t idx)
{
    return v.at(idx);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find(basic_value<TC> const& v, const std::size_t idx)
{
    return v.at(idx);
}
template<typename TC>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find(basic_value<TC>&& v, const std::size_t idx)
{
    return basic_value<TC>(std::move(v.at(idx)));
}

// --------------------------------------------------------------------------
// find<optional<T>>

#if defined(TOML11_HAS_OPTIONAL)
template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_optional<T>::value, T>
find(const basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    if(v.contains(ky))
    {
        return ::toml::get<typename T::value_type>(v.at(ky));
    }
    else
    {
        return std::nullopt;
    }
}

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_optional<T>::value, T>
find(basic_value<TC>& v, const typename basic_value<TC>::key_type& ky)
{
    if(v.contains(ky))
    {
        return ::toml::get<typename T::value_type>(v.at(ky));
    }
    else
    {
        return std::nullopt;
    }
}

template<typename T, typename TC>
cxx::enable_if_t<detail::is_std_optional<T>::value, T>
find(basic_value<TC>&& v, const typename basic_value<TC>::key_type& ky)
{
    if(v.contains(ky))
    {
        return ::toml::get<typename T::value_type>(std::move(v.at(ky)));
    }
    else
    {
        return std::nullopt;
    }
}

template<typename T, typename K, typename TC>
cxx::enable_if_t<detail::is_std_optional<T>::value && std::is_integral<K>::value, T>
find(const basic_value<TC>& v, const K& k)
{
    if(static_cast<std::size_t>(k) < v.size())
    {
        return ::toml::get<typename T::value_type>(v.at(static_cast<std::size_t>(k)));
    }
    else
    {
        return std::nullopt;
    }
}

template<typename T, typename K, typename TC>
cxx::enable_if_t<detail::is_std_optional<T>::value && std::is_integral<K>::value, T>
find(basic_value<TC>& v, const K& k)
{
    if(static_cast<std::size_t>(k) < v.size())
    {
        return ::toml::get<typename T::value_type>(v.at(static_cast<std::size_t>(k)));
    }
    else
    {
        return std::nullopt;
    }
}

template<typename T, typename K, typename TC>
cxx::enable_if_t<detail::is_std_optional<T>::value && std::is_integral<K>::value, T>
find(basic_value<TC>&& v, const K& k)
{
    if(static_cast<std::size_t>(k) < v.size())
    {
        return ::toml::get<typename T::value_type>(std::move(v.at(static_cast<std::size_t>(k))));
    }
    else
    {
        return std::nullopt;
    }
}
#endif // optional

// --------------------------------------------------------------------------
// toml::find(toml::value, toml::key, Ts&& ... keys)

namespace detail
{

// It suppresses warnings by -Wsign-conversion when we pass integer literal
// to toml::find. integer literal `0` is deduced as an int, and will be
// converted to std::size_t. This causes sign-conversion.

template<typename TC>
std::size_t key_cast(const std::size_t& v) noexcept
{
    return v;
}
template<typename TC, typename T>
cxx::enable_if_t<std::is_integral<cxx::remove_cvref_t<T>>::value, std::size_t>
key_cast(const T& v) noexcept
{
    return static_cast<std::size_t>(v);
}

// for string-like (string, string literal, string_view)

template<typename TC>
typename basic_value<TC>::key_type const&
key_cast(const typename basic_value<TC>::key_type& v) noexcept
{
    return v;
}
template<typename TC>
typename basic_value<TC>::key_type
key_cast(const typename basic_value<TC>::key_type::value_type* v)
{
    return typename basic_value<TC>::key_type(v);
}
#if defined(TOML11_HAS_STRING_VIEW)
template<typename TC>
typename basic_value<TC>::key_type
key_cast(const std::string_view v)
{
    return typename basic_value<TC>::key_type(v);
}
#endif // string_view

} // detail

// ----------------------------------------------------------------------------
// find(v, keys...)

template<typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find(const basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find(basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find(basic_value<TC>&& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find(std::move(v.at(detail::key_cast<TC>(k1))), detail::key_cast<TC>(k2), ks...);
}

// ----------------------------------------------------------------------------
// find<T>(v, keys...)

template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<const basic_value<TC>&>()))>
find(const basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find<T>(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC>&>()))>
find(basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find<T>(v.at(detail::key_cast<TC>(k1)), detail::key_cast<TC>(k2), ks...);
}
template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<cxx::negation<detail::is_std_optional<T>>::value,
    decltype(::toml::get<T>(std::declval<basic_value<TC>&&>()))>
find(basic_value<TC>&& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    return find<T>(std::move(v.at(detail::key_cast<TC>(k1))), detail::key_cast<TC>(k2), ks...);
}

#if defined(TOML11_HAS_OPTIONAL)
template<typename T, typename TC, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_std_optional<T>::value, T>
find(const basic_value<TC>& v, const typename basic_value<TC>::key_type& k1, const K2& k2, const Ks& ... ks)
{
    if(v.contains(k1))
    {
        return find<T>(v.at(k1), detail::key_cast<TC>(k2), ks...);
    }
    else
    {
        return std::nullopt;
    }
}
template<typename T, typename TC, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_std_optional<T>::value, T>
find(basic_value<TC>& v, const typename basic_value<TC>::key_type& k1, const K2& k2, const Ks& ... ks)
{
    if(v.contains(k1))
    {
        return find<T>(v.at(k1), detail::key_cast<TC>(k2), ks...);
    }
    else
    {
        return std::nullopt;
    }
}
template<typename T, typename TC, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_std_optional<T>::value, T>
find(basic_value<TC>&& v, const typename basic_value<TC>::key_type& k1, const K2& k2, const Ks& ... ks)
{
    if(v.contains(k1))
    {
        return find<T>(v.at(k1), detail::key_cast<TC>(k2), ks...);
    }
    else
    {
        return std::nullopt;
    }
}

template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_std_optional<T>::value && std::is_integral<K1>::value, T>
find(const basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    if(static_cast<std::size_t>(k1) < v.size())
    {
        return find<T>(v.at(static_cast<std::size_t>(k1)), detail::key_cast<TC>(k2), ks...);
    }
    else
    {
        return std::nullopt;
    }
}
template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_std_optional<T>::value && std::is_integral<K1>::value, T>
find(basic_value<TC>& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    if(static_cast<std::size_t>(k1) < v.size())
    {
        return find<T>(v.at(static_cast<std::size_t>(k1)), detail::key_cast<TC>(k2), ks...);
    }
    else
    {
        return std::nullopt;
    }
}
template<typename T, typename TC, typename K1, typename K2, typename ... Ks>
cxx::enable_if_t<detail::is_std_optional<T>::value && std::is_integral<K1>::value, T>
find(basic_value<TC>&& v, const K1& k1, const K2& k2, const Ks& ... ks)
{
    if(static_cast<std::size_t>(k1) < v.size())
    {
        return find<T>(v.at(static_cast<std::size_t>(k1)), detail::key_cast<TC>(k2), ks...);
    }
    else
    {
        return std::nullopt;
    }
}
#endif // optional

// ===========================================================================
// find_or<T>(value, key, fallback)

// ---------------------------------------------------------------------------
// find_or(v, key, other_v)

template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>&
find_or(basic_value<TC>& v, const K& k, basic_value<TC>& opt) noexcept
{
    try
    {
        return ::toml::find(v, detail::key_cast<TC>(k));
    }
    catch(...)
    {
        return opt;
    }
}
template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>> const&
find_or(const basic_value<TC>& v, const K& k, const basic_value<TC>& opt) noexcept
{
    try
    {
        return ::toml::find(v, detail::key_cast<TC>(k));
    }
    catch(...)
    {
        return opt;
    }
}
template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, basic_value<TC>>
find_or(basic_value<TC>&& v, const K& k, basic_value<TC>&& opt) noexcept
{
    try
    {
        return ::toml::find(v, detail::key_cast<TC>(k));
    }
    catch(...)
    {
        return opt;
    }
}

// ---------------------------------------------------------------------------
// toml types (return type can be a reference)

template<typename T, typename TC, typename K>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value,
    cxx::remove_cvref_t<T> const&>
find_or(const basic_value<TC>& v, const K& k, const T& opt)
{
    try
    {
        return ::toml::get<T>(v.at(detail::key_cast<TC>(k)));
    }
    catch(...)
    {
        return opt;
    }
}

template<typename T, typename TC, typename K>
cxx::enable_if_t<cxx::conjunction<
        cxx::negation<std::is_const<T>>,
        detail::is_exact_toml_type<T, basic_value<TC>>
    >::value, cxx::remove_cvref_t<T>&>
find_or(basic_value<TC>& v, const K& k, T& opt)
{
    try
    {
        return ::toml::get<T>(v.at(detail::key_cast<TC>(k)));
    }
    catch(...)
    {
        return opt;
    }
}

template<typename T, typename TC, typename K>
cxx::enable_if_t<detail::is_exact_toml_type<T, basic_value<TC>>::value,
    cxx::remove_cvref_t<T>>
find_or(basic_value<TC>&& v, const K& k, T opt)
{
    try
    {
        return ::toml::get<T>(std::move(v.at(detail::key_cast<TC>(k))));
    }
    catch(...)
    {
        return T(std::move(opt));
    }
}

// ---------------------------------------------------------------------------
// string literal (deduced as std::string)

// XXX to avoid confusion when T is explicitly specified in find_or<T>(),
//     we restrict the string type as std::string.
template<typename TC, typename K>
cxx::enable_if_t<detail::is_type_config<TC>::value, std::string>
find_or(const basic_value<TC>& v, const K& k, const char* opt)
{
    try
    {
        return ::toml::get<std::string>(v.at(detail::key_cast<TC>(k)));
    }
    catch(...)
    {
        return std::string(opt);
    }
}

// ---------------------------------------------------------------------------
// other types (requires type conversion and return type cannot be a reference)

template<typename T, typename TC, typename K>
cxx::enable_if_t<cxx::conjunction<
        cxx::negation<detail::is_basic_value<cxx::remove_cvref_t<T>>>,
        detail::is_not_toml_type<cxx::remove_cvref_t<T>, basic_value<TC>>,
        cxx::negation<std::is_same<cxx::remove_cvref_t<T>,
            const typename basic_value<TC>::string_type::value_type*>>
    >::value, cxx::remove_cvref_t<T>>
find_or(const basic_value<TC>& v, const K& ky, T opt)
{
    try
    {
        return ::toml::get<cxx::remove_cvref_t<T>>(v.at(detail::key_cast<TC>(ky)));
    }
    catch(...)
    {
        return cxx::remove_cvref_t<T>(std::move(opt));
    }
}

// ----------------------------------------------------------------------------
// recursive

namespace detail
{

template<typename ...Ts>
auto last_one(Ts&&... args)
 -> decltype(std::get<sizeof...(Ts)-1>(std::forward_as_tuple(std::forward<Ts>(args)...)))
{
    return std::get<sizeof...(Ts)-1>(std::forward_as_tuple(std::forward<Ts>(args)...));
}

} // detail

template<typename Value, typename K1, typename K2, typename K3, typename ... Ks>
auto find_or(Value&& v, const K1& k1, const K2& k2, K3&& k3, Ks&& ... keys) noexcept
    -> cxx::enable_if_t<
        detail::is_basic_value<cxx::remove_cvref_t<Value>>::value,
        decltype(find_or(v, k2, std::forward<K3>(k3), std::forward<Ks>(keys)...))
    >
{
    try
    {
        return find_or(v.at(k1), k2, std::forward<K3>(k3), std::forward<Ks>(keys)...);
    }
    catch(...)
    {
        return detail::last_one(k3, keys...);
    }
}

template<typename T, typename TC, typename K1, typename K2, typename K3, typename ... Ks>
T find_or(const basic_value<TC>& v, const K1& k1, const K2& k2, const K3& k3, const Ks& ... keys) noexcept
{
    try
    {
        return find_or<T>(v.at(k1), k2, k3, keys...);
    }
    catch(...)
    {
        return static_cast<T>(detail::last_one(k3, keys...));
    }
}

// ===========================================================================
// find_or_default<T>(value, key)

template<typename T, typename TC, typename K>
cxx::enable_if_t<std::is_default_constructible<T>::value, T>
find_or_default(const basic_value<TC>& v, K&& k) noexcept(std::is_nothrow_default_constructible<T>::value)
{
    try
    {
        return ::toml::get<T>(v.at(detail::key_cast<TC>(std::forward<K>(k))));
    }
    catch(...)
    {
        return T();
    }
}

template<typename T, typename TC, typename K1, typename ... Ks>
cxx::enable_if_t<std::is_default_constructible<T>::value, T>
find_or_default(const basic_value<TC>& v, K1&& k1, Ks&& ... keys) noexcept(std::is_nothrow_default_constructible<T>::value)
{
    try
    {
        return find_or_default<T>(v.at(std::forward<K1>(k1)), std::forward<Ks>(keys)...);
    }
    catch(...)
    {
        return T();
    }
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_FIND_HPP
#ifndef TOML11_CONVERSION_HPP
#define TOML11_CONVERSION_HPP


#if defined(TOML11_HAS_OPTIONAL)

#include <optional>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

template<typename T>
inline constexpr bool is_optional_v = false;

template<typename T>
inline constexpr bool is_optional_v<std::optional<T>> = true;

template<typename T, typename TC>
void find_member_variable_from_value(T& obj, const basic_value<TC>& v, const char* var_name)
{
    if constexpr(is_optional_v<T>)
    {
        if(v.contains(var_name))
        {
            obj = toml::find<typename T::value_type>(v, var_name);
        }
        else
        {
            obj = std::nullopt;
        }
    }
    else
    {
        obj = toml::find<T>(v, var_name);
    }
}

template<typename T, typename TC>
void assign_member_variable_to_value(const T& obj, basic_value<TC>& v, const char* var_name)
{
    if constexpr(is_optional_v<T>)
    {
        if(obj.has_value())
        {
            v[var_name] = obj.value();
        }
    }
    else
    {
        v[var_name] = obj;
    }
}

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

#else

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

template<typename T, typename TC>
void find_member_variable_from_value(T& obj, const basic_value<TC>& v, const char* var_name)
{
    obj = toml::find<T>(v, var_name);
}

template<typename T, typename TC>
void assign_member_variable_to_value(const T& obj, basic_value<TC>& v, const char* var_name)
{
    v[var_name] = obj;
}

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

#endif // optional

// use it in the following way.
// ```cpp
// namespace foo
// {
// struct Foo
// {
//     std::string s;
//     double      d;
//     int         i;
// };
// } // foo
//
// TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(foo::Foo, s, d, i)
// ```
//
// And then you can use `toml::get<foo::Foo>(v)` and `toml::find<foo::Foo>(file, "foo");`
//

#define TOML11_STRINGIZE_AUX(x) #x
#define TOML11_STRINGIZE(x)     TOML11_STRINGIZE_AUX(x)

#define TOML11_CONCATENATE_AUX(x, y) x##y
#define TOML11_CONCATENATE(x, y)     TOML11_CONCATENATE_AUX(x, y)

// ============================================================================
// TOML11_DEFINE_CONVERSION_NON_INTRUSIVE

#ifndef TOML11_WITHOUT_DEFINE_NON_INTRUSIVE

// ----------------------------------------------------------------------------
// TOML11_ARGS_SIZE

#define TOML11_INDEX_RSEQ() \
    32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
    16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0
#define TOML11_ARGS_SIZE_IMPL(\
    ARG1,  ARG2,  ARG3,  ARG4,  ARG5,  ARG6,  ARG7,  ARG8,  ARG9,  ARG10, \
    ARG11, ARG12, ARG13, ARG14, ARG15, ARG16, ARG17, ARG18, ARG19, ARG20, \
    ARG21, ARG22, ARG23, ARG24, ARG25, ARG26, ARG27, ARG28, ARG29, ARG30, \
    ARG31, ARG32, N, ...) N
#define TOML11_ARGS_SIZE_AUX(...) TOML11_ARGS_SIZE_IMPL(__VA_ARGS__)
#define TOML11_ARGS_SIZE(...) TOML11_ARGS_SIZE_AUX(__VA_ARGS__, TOML11_INDEX_RSEQ())

// ----------------------------------------------------------------------------
// TOML11_FOR_EACH_VA_ARGS

#define TOML11_FOR_EACH_VA_ARGS_AUX_1( FUNCTOR, ARG1     ) FUNCTOR(ARG1)
#define TOML11_FOR_EACH_VA_ARGS_AUX_2( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_1( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_3( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_2( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_4( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_3( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_5( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_4( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_6( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_5( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_7( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_6( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_8( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_7( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_9( FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_8( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_10(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_9( FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_11(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_10(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_12(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_11(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_13(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_12(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_14(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_13(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_15(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_14(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_16(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_15(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_17(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_16(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_18(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_17(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_19(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_18(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_20(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_19(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_21(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_20(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_22(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_21(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_23(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_22(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_24(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_23(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_25(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_24(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_26(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_25(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_27(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_26(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_28(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_27(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_29(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_28(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_30(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_29(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_31(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_30(FUNCTOR, __VA_ARGS__)
#define TOML11_FOR_EACH_VA_ARGS_AUX_32(FUNCTOR, ARG1, ...) FUNCTOR(ARG1) TOML11_FOR_EACH_VA_ARGS_AUX_31(FUNCTOR, __VA_ARGS__)

#define TOML11_FOR_EACH_VA_ARGS(FUNCTOR, ...)\
    TOML11_CONCATENATE(TOML11_FOR_EACH_VA_ARGS_AUX_, TOML11_ARGS_SIZE(__VA_ARGS__))(FUNCTOR, __VA_ARGS__)


#define TOML11_FIND_MEMBER_VARIABLE_FROM_VALUE(VAR_NAME)\
    toml::detail::find_member_variable_from_value(obj.VAR_NAME, v, TOML11_STRINGIZE(VAR_NAME));

#define TOML11_ASSIGN_MEMBER_VARIABLE_TO_VALUE(VAR_NAME)\
    toml::detail::assign_member_variable_to_value(obj.VAR_NAME, v, TOML11_STRINGIZE(VAR_NAME));

#define TOML11_DEFINE_CONVERSION_NON_INTRUSIVE(NAME, ...)\
    namespace toml {                                                                     \
    inline namespace TOML11_INLINE_VERSION_NAMESPACE {                                          \
    template<>                                                                           \
    struct from<NAME>                                                                    \
    {                                                                                    \
        template<typename TC>                                                            \
        static NAME from_toml(const basic_value<TC>& v)                                  \
        {                                                                                \
            NAME obj;                                                                    \
            TOML11_FOR_EACH_VA_ARGS(TOML11_FIND_MEMBER_VARIABLE_FROM_VALUE, __VA_ARGS__) \
            return obj;                                                                  \
        }                                                                                \
    };                                                                                   \
    template<>                                                                           \
    struct into<NAME>                                                                    \
    {                                                                                    \
        template<typename TC>                                                            \
        static basic_value<TC> into_toml(const NAME& obj)                                \
        {                                                                                \
            ::toml::basic_value<TC> v = typename ::toml::basic_value<TC>::table_type{};  \
            TOML11_FOR_EACH_VA_ARGS(TOML11_ASSIGN_MEMBER_VARIABLE_TO_VALUE, __VA_ARGS__) \
            return v;                                                                    \
        }                                                                                \
    };                                                                                   \
    } /* TOML11_INLINE_VERSION_NAMESPACE */                                              \
    } /* toml */

#endif// TOML11_WITHOUT_DEFINE_NON_INTRUSIVE

#endif // TOML11_CONVERSION_HPP
#ifndef TOML11_CONTEXT_HPP
#define TOML11_CONTEXT_HPP


#include <vector>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

template<typename TypeConfig>
class context
{
  public:

    explicit context(const spec& toml_spec)
        : toml_spec_(toml_spec), errors_{}
    {}

    bool has_error() const noexcept {return !errors_.empty();}

    std::vector<error_info> const& errors() const noexcept {return errors_;}

    semantic_version&       toml_version()       noexcept {return toml_spec_.version;}
    semantic_version const& toml_version() const noexcept {return toml_spec_.version;}

    spec&       toml_spec()       noexcept {return toml_spec_;}
    spec const& toml_spec() const noexcept {return toml_spec_;}

    void report_error(error_info err)
    {
        this->errors_.push_back(std::move(err));
    }

    error_info pop_last_error()
    {
        assert( ! errors_.empty());
        auto e = std::move(errors_.back());
        errors_.pop_back();
        return e;
    }

  private:

    spec toml_spec_;
    std::vector<error_info> errors_;
};

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
struct type_config;
struct ordered_type_config;
namespace detail
{
extern template class context<::toml::type_config>;
extern template class context<::toml::ordered_type_config>;
} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_CONTEXT_HPP
#ifndef TOML11_SKIP_HPP
#define TOML11_SKIP_HPP


#include <cassert>

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
namespace detail
{

template<typename TC>
bool skip_whitespace(location& loc, const context<TC>& ctx)
{
    return syntax::ws(ctx.toml_spec()).scan(loc).is_ok();
}

template<typename TC>
bool skip_empty_lines(location& loc, const context<TC>& ctx)
{
    return repeat_at_least(1, sequence(
            syntax::ws(ctx.toml_spec()),
            syntax::newline(ctx.toml_spec())
        )).scan(loc).is_ok();
}

// For error recovery.
//
// In case if a comment line contains an invalid character, we need to skip it
// to advance parsing.
template<typename TC>
void skip_comment_block(location& loc, const context<TC>& ctx)
{
    while( ! loc.eof())
    {
        skip_whitespace(loc, ctx);
        if(loc.current() == '#')
        {
            while( ! loc.eof())
            {
                // both CRLF and LF ends with LF.
                if(loc.current() == '\n')
                {
                    loc.advance();
                    break;
                }
            }
        }
        else if(syntax::newline(ctx.toml_spec()).scan(loc).is_ok())
        {
            ; // an empty line. skip this also
        }
        else
        {
            // the next token is neither a comment nor empty line.
            return ;
        }
    }
    return ;
}

template<typename TC>
void skip_empty_or_comment_lines(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    repeat_at_least(0, sequence(
            syntax::ws(spec),
            maybe(syntax::comment(spec)),
            syntax::newline(spec))
        ).scan(loc);
    return ;
}

// For error recovery.
//
// Sometimes we need to skip a value and find a delimiter, like `,`, `]`, or `}`.
// To find delimiter, we need to skip delimiters in a string.
// Since we are skipping invalid value while error recovery, we don't need
// to check the syntax. Here we just skip string-like region until closing quote
// is found.
template<typename TC>
void skip_string_like(location& loc, const context<TC>&)
{
    // if """ is found, skip until the closing """ is found.
    if(literal("\"\"\"").scan(loc).is_ok())
    {
        while( ! loc.eof())
        {
            if(literal("\"\"\"").scan(loc).is_ok())
            {
                return;
            }
            loc.advance();
        }
    }
    else if(literal("'''").scan(loc).is_ok())
    {
        while( ! loc.eof())
        {
            if(literal("'''").scan(loc).is_ok())
            {
                return;
            }
            loc.advance();
        }
    }
    // if " is found, skip until the closing " or newline is found.
    else if(loc.current() == '"')
    {
        while( ! loc.eof())
        {
            loc.advance();
            if(loc.current() == '"' || loc.current() == '\n')
            {
                loc.advance();
                return;
            }
        }
    }
    else if(loc.current() == '\'')
    {
        while( ! loc.eof())
        {
            loc.advance();
            if(loc.current() == '\'' || loc.current() == '\n')
            {
                loc.advance();
                return ;
            }
        }
    }
    return;
}

template<typename TC>
void skip_value(location& loc, const context<TC>& ctx);
template<typename TC>
void skip_array_like(location& loc, const context<TC>& ctx);
template<typename TC>
void skip_inline_table_like(location& loc, const context<TC>& ctx);
template<typename TC>
void skip_key_value_pair(location& loc, const context<TC>& ctx);

template<typename TC>
result<value_t, error_info>
guess_value_type(const location& loc, const context<TC>& ctx);

template<typename TC>
void skip_array_like(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    assert(loc.current() == '[');
    loc.advance();

    while( ! loc.eof())
    {
        if(loc.current() == '\"' || loc.current() == '\'')
        {
            skip_string_like(loc, ctx);
        }
        else if(loc.current() == '#')
        {
            skip_comment_block(loc, ctx);
        }
        else if(loc.current() == '{')
        {
            skip_inline_table_like(loc, ctx);
        }
        else if(loc.current() == '[')
        {
            const auto checkpoint = loc;
            if(syntax::std_table(spec).scan(loc).is_ok() ||
               syntax::array_table(spec).scan(loc).is_ok())
            {
                loc = checkpoint;
                break;
            }
            // if it is not a table-definition, then it is an array.
            skip_array_like(loc, ctx);
        }
        else if(loc.current() == '=')
        {
            // key-value pair cannot be inside the array.
            // guessing the error is "missing closing bracket `]`".
            // find the previous key just before `=`.
            while(loc.get_location() != 0)
            {
                loc.retrace();
                if(loc.current() == '\n')
                {
                    loc.advance();
                    break;
                }
            }
            break;
        }
        else if(loc.current() == ']')
        {
            break; // found closing bracket
        }
        else
        {
            loc.advance();
        }
    }
    return ;
}

template<typename TC>
void skip_inline_table_like(location& loc, const context<TC>& ctx)
{
    assert(loc.current() == '{');
    loc.advance();

    const auto& spec = ctx.toml_spec();

    while( ! loc.eof())
    {
        if(loc.current() == '\n' && ! spec.v1_1_0_allow_newlines_in_inline_tables)
        {
            break; // missing closing `}`.
        }
        else if(loc.current() == '\"' || loc.current() == '\'')
        {
            skip_string_like(loc, ctx);
        }
        else if(loc.current() == '#')
        {
            skip_comment_block(loc, ctx);
            if( ! spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                // comment must end with newline.
                break; // missing closing `}`.
            }
        }
        else if(loc.current() == '[')
        {
            const auto checkpoint = loc;
            if(syntax::std_table(spec).scan(loc).is_ok() ||
               syntax::array_table(spec).scan(loc).is_ok())
            {
                loc = checkpoint;
                break; // missing closing `}`.
            }
            // if it is not a table-definition, then it is an array.
            skip_array_like(loc, ctx);
        }
        else if(loc.current() == '{')
        {
            skip_inline_table_like(loc, ctx);
        }
        else if(loc.current() == '}')
        {
            // closing brace found. guessing the error is inside the table.
            break;
        }
        else
        {
            // skip otherwise.
            loc.advance();
        }
    }
    return ;
}

template<typename TC>
void skip_value(location& loc, const context<TC>& ctx)
{
    value_t ty = guess_value_type(loc, ctx).unwrap_or(value_t::empty);
    if(ty == value_t::string)
    {
        skip_string_like(loc, ctx);
    }
    else if(ty == value_t::array)
    {
        skip_array_like(loc, ctx);
    }
    else if(ty == value_t::table)
    {
        // In case of multiline tables, it may skip key-value pair but not the
        // whole table.
        skip_inline_table_like(loc, ctx);
    }
    else // others are an "in-line" values. skip until the next line
    {
        while( ! loc.eof())
        {
            if(loc.current() == '\n')
            {
                break;
            }
            else if(loc.current() == ',' || loc.current() == ']' || loc.current() == '}')
            {
                break;
            }
            loc.advance();
        }
    }
    return;
}

template<typename TC>
void skip_key_value_pair(location& loc, const context<TC>& ctx)
{
    while( ! loc.eof())
    {
        if(loc.current() == '=')
        {
            skip_whitespace(loc, ctx);
            skip_value(loc, ctx);
            return;
        }
        else if(loc.current() == '\n')
        {
            // newline is found before finding `=`. assuming "missing `=`".
            return;
        }
        loc.advance();
    }
    return ;
}

template<typename TC>
void skip_until_next_table(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    while( ! loc.eof())
    {
        if(loc.current() == '\n')
        {
            loc.advance();
            const auto line_begin = loc;

            skip_whitespace(loc, ctx);
            if(syntax::std_table(spec).scan(loc).is_ok())
            {
                loc = line_begin;
                return ;
            }
            if(syntax::array_table(spec).scan(loc).is_ok())
            {
                loc = line_begin;
                return ;
            }
        }
        loc.advance();
    }
}

} // namespace detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
struct type_config;
struct ordered_type_config;

namespace detail
{
extern template bool skip_whitespace            <type_config>(location& loc, const context<type_config>&);
extern template bool skip_empty_lines           <type_config>(location& loc, const context<type_config>&);
extern template void skip_comment_block         <type_config>(location& loc, const context<type_config>&);
extern template void skip_empty_or_comment_lines<type_config>(location& loc, const context<type_config>&);
extern template void skip_string_like           <type_config>(location& loc, const context<type_config>&);
extern template void skip_array_like            <type_config>(location& loc, const context<type_config>&);
extern template void skip_inline_table_like     <type_config>(location& loc, const context<type_config>&);
extern template void skip_value                 <type_config>(location& loc, const context<type_config>&);
extern template void skip_key_value_pair        <type_config>(location& loc, const context<type_config>&);
extern template void skip_until_next_table      <type_config>(location& loc, const context<type_config>&);

extern template bool skip_whitespace            <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template bool skip_empty_lines           <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_comment_block         <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_empty_or_comment_lines<ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_string_like           <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_array_like            <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_inline_table_like     <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_value                 <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_key_value_pair        <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_until_next_table      <ordered_type_config>(location& loc, const context<ordered_type_config>&);

} // detail
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_SKIP_HPP
#ifndef TOML11_PARSER_HPP
#define TOML11_PARSER_HPP


#include <fstream>
#include <sstream>

#include <cassert>
#include <cmath>

#if defined(TOML11_HAS_FILESYSTEM) && TOML11_HAS_FILESYSTEM
#include <filesystem>
#endif

namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

struct syntax_error final : public ::toml::exception
{
  public:
    syntax_error(std::string what_arg, std::vector<error_info> err)
        : what_(std::move(what_arg)), err_(std::move(err))
    {}
    ~syntax_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}

    std::vector<error_info> const& errors() const noexcept
    {
        return err_;
    }

  private:
    std::string what_;
    std::vector<error_info> err_;
};

struct file_io_error final : public ::toml::exception
{
  public:

    file_io_error(const std::string& msg, const std::string& fname)
        : errno_(cxx::make_nullopt()),
          what_(msg + " \"" + fname + "\"")
    {}
    file_io_error(int errnum, const std::string& msg, const std::string& fname)
        : errno_(errnum),
          what_(msg + " \"" + fname + "\": errno=" + std::to_string(errnum))
    {}
    ~file_io_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}

    bool has_errno() const noexcept {return errno_.has_value();}
    int  get_errno() const noexcept {return errno_.value_or(0);}

  private:

    cxx::optional<int> errno_;
    std::string what_;
};

namespace detail
{

/* ============================================================================
 *    __ ___ _ __  _ __  ___ _ _
 *   / _/ _ \ '  \| '  \/ _ \ ' \
 *   \__\___/_|_|_|_|_|_\___/_||_|
 */

template<typename S>
error_info make_syntax_error(std::string title,
        const S& scanner, location loc, std::string suffix = "")
{
    auto msg = std::string("expected ") + scanner.expected_chars(loc);
    auto src = source_location(region(loc));
    return make_error_info(
        std::move(title), std::move(src), std::move(msg), std::move(suffix));
}


/* ============================================================================
 *                             _
 *  __ ___ _ __  _ __  ___ _ _| |_
 * / _/ _ \ '  \| '  \/ -_) ' \  _|
 * \__\___/_|_|_|_|_|_\___|_||_\__|
 */

template<typename TC>
result<cxx::optional<std::string>, error_info>
parse_comment_line(location& loc, context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    const auto first = loc;

    skip_whitespace(loc, ctx);

    const auto com_reg = syntax::comment(spec).scan(loc);
    if(com_reg.is_ok())
    {
        // once comment started, newline must follow (or reach EOF).
        if( ! loc.eof() && ! syntax::newline(spec).scan(loc).is_ok())
        {
            while( ! loc.eof()) // skip until newline to continue parsing
            {
                loc.advance();
                if(loc.current() == '\n') { /*skip LF*/ loc.advance(); break; }
            }
            return err(make_error_info("toml::parse_comment_line: "
                "newline (LF / CRLF) or EOF is expected",
                source_location(region(loc)), "but got this",
                "Hint: most of the control characters are not allowed in comments"));
        }
        return ok(cxx::optional<std::string>(com_reg.as_string()));
    }
    else
    {
        loc = first; // rollback whitespace to parse indent
        return ok(cxx::optional<std::string>(cxx::make_nullopt()));
    }
}

/* ============================================================================
 *  ___           _
 * | _ ) ___  ___| |___ __ _ _ _
 * | _ \/ _ \/ _ \ / -_) _` | ' \
 * |___/\___/\___/_\___\__,_|_||_|
 */

template<typename TC>
result<basic_value<TC>, error_info>
parse_boolean(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::boolean(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_boolean: "
            "invalid boolean: boolean must be `true` or `false`, in lowercase. "
            "string must be surrounded by `\"`", syntax::boolean(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    const auto str = reg.as_string();
    const auto val = [&str]() {
        if(str == "true")
        {
            return true;
        }
        else
        {
            assert(str == "false");
            return false;
        }
    }();

    // ----------------------------------------------------------------------
    // no format info for boolean
    boolean_format_info fmt;

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

/* ============================================================================
 *  ___     _
 * |_ _|_ _| |_ ___ __ _ ___ _ _
 *  | || ' \  _/ -_) _` / -_) '_|
 * |___|_||_\__\___\__, \___|_|
 *                 |___/
 */

template<typename TC>
result<basic_value<TC>, error_info>
parse_bin_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();
    auto reg = syntax::bin_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_bin_integer: "
            "invalid integer: bin_int must be like: 0b0101, 0b1111_0000",
            syntax::bin_int(spec), loc));
    }

    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt   = integer_format::bin;
    fmt.width = str.size() - 2 - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // skip prefix `0b` and zeros and underscores at the MSB
    str.erase(str.begin(), std::find(std::next(str.begin(), 2), str.end(), '1'));

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    // 0b0000_0000 becomes empty.
    if(str.empty()) { str = "0"; }

    const auto val = TC::parse_int(str, source_location(region(loc)), 2);
    if(val.is_ok())
    {
        return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
    }
    else
    {
        loc = first;
        return err(val.as_err());
    }
}

// ----------------------------------------------------------------------------

template<typename TC>
result<basic_value<TC>, error_info>
parse_oct_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();
    auto reg = syntax::oct_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_oct_integer: "
            "invalid integer: oct_int must be like: 0o775, 0o04_44",
            syntax::oct_int(spec), loc));
    }

    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt   = integer_format::oct;
    fmt.width = str.size() - 2 - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // skip prefix `0o` and zeros and underscores at the MSB
    str.erase(str.begin(), std::find_if(
                std::next(str.begin(), 2), str.end(), [](const char c) {
                    return c != '0' && c != '_';
                }));

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    // 0o0000_0000 becomes empty.
    if(str.empty()) { str = "0"; }

    const auto val = TC::parse_int(str, source_location(region(loc)), 8);
    if(val.is_ok())
    {
        return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
    }
    else
    {
        loc = first;
        return err(val.as_err());
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_hex_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();
    auto reg = syntax::hex_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_hex_integer: "
            "invalid integer: hex_int must be like: 0xC0FFEE, 0xdead_beef",
            syntax::hex_int(spec), loc));
    }

    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt   = integer_format::hex;
    fmt.width = str.size() - 2 - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // skip prefix `0x` and zeros and underscores at the MSB
    str.erase(str.begin(), std::find_if(
                std::next(str.begin(), 2), str.end(), [](const char c) {
                    return c != '0' && c != '_';
                }));

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    // 0x0000_0000 becomes empty.
    if(str.empty()) { str = "0"; }

    // prefix zero and _ is removed. check if it uses upper/lower case.
    // if both upper and lower case letters are found, set upper=true.
    const auto lower_not_found = std::find_if(str.begin(), str.end(),
        [](const char c) { return std::islower(static_cast<int>(c)) != 0; }) == str.end();
    const auto upper_found = std::find_if(str.begin(), str.end(),
        [](const char c) { return std::isupper(static_cast<int>(c)) != 0; }) != str.end();
    fmt.uppercase = lower_not_found || upper_found;

    const auto val = TC::parse_int(str, source_location(region(loc)), 16);
    if(val.is_ok())
    {
        return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
    }
    else
    {
        loc = first;
        return err(val.as_err());
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_dec_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::dec_int(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_dec_integer: "
            "invalid integer: dec_int must be like: 42, 123_456_789",
            syntax::dec_int(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    auto str = reg.as_string();

    integer_format_info fmt;
    fmt.fmt = integer_format::dec;
    fmt.width = str.size() - static_cast<std::size_t>(std::count(str.begin(), str.end(), '_'));

    const auto first_underscore = std::find(str.rbegin(), str.rend(), '_');
    if(first_underscore != str.rend())
    {
        fmt.spacer = static_cast<std::size_t>(std::distance(str.rbegin(), first_underscore));
    }

    // remove all `_` before calling TC::parse_int
    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    auto src = source_location(region(loc));
    const auto val = TC::parse_int(str, src, 10);
    if(val.is_err())
    {
        loc = first;
        return err(val.as_err());
    }

    // ----------------------------------------------------------------------
    // parse suffix (extension)

    if(spec.ext_num_suffix && loc.current() == '_')
    {
        const auto sfx_reg = syntax::num_suffix(spec).scan(loc);
        if( ! sfx_reg.is_ok())
        {
            loc = first;
            return err(make_error_info("toml::parse_dec_integer: "
                "invalid suffix: should be `_ non-digit-graph (graph | _graph)`",
                source_location(region(loc)), "here"));
        }
        auto sfx = sfx_reg.as_string();
        assert( ! sfx.empty() && sfx.front() == '_');
        sfx.erase(sfx.begin()); // remove the first `_`

        fmt.suffix = sfx;
    }

    return ok(basic_value<TC>(val.as_ok(), std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_integer(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    if( ! loc.eof() && (loc.current() == '+' || loc.current() == '-'))
    {
        // skip +/- to diagnose +0xDEADBEEF or -0b0011 (invalid).
        // without this, +0xDEAD_BEEF will be parsed as a decimal int and
        // unexpected "xDEAD_BEEF" will appear after integer "+0".
        loc.advance();
    }

    if( ! loc.eof() && loc.current() == '0')
    {
        loc.advance();
        if(loc.eof())
        {
            // `[+-]?0`. parse as an decimal integer.
            loc = first;
            return parse_dec_integer(loc, ctx);
        }

        const auto prefix = loc.current();
        auto prefix_src = source_location(region(loc));

        loc = first;

        if(prefix == 'b') {return parse_bin_integer(loc, ctx);}
        if(prefix == 'o') {return parse_oct_integer(loc, ctx);}
        if(prefix == 'x') {return parse_hex_integer(loc, ctx);}

        if(std::isdigit(prefix))
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_integer: "
                "leading zero in an decimal integer is not allowed",
                std::move(src), "leading zero"));
        }
    }

    loc = first;
    return parse_dec_integer(loc, ctx);
}

/* ============================================================================
 *  ___ _           _   _
 * | __| |___  __ _| |_(_)_ _  __ _
 * | _|| / _ \/ _` |  _| | ' \/ _` |
 * |_| |_\___/\__,_|\__|_|_||_\__, |
 *                            |___/
 */

template<typename TC>
result<basic_value<TC>, error_info>
parse_floating(location& loc, const context<TC>& ctx)
{
    using floating_type = typename basic_value<TC>::floating_type;

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    // ----------------------------------------------------------------------
    // check syntax
    bool is_hex = false;
    std::string str;
    region reg;
    if(spec.ext_hex_float && literal("0x").scan(loc).is_ok())
    {
        loc = first;
        is_hex = true;

        reg = syntax::hex_floating(spec).scan(loc);
        if( ! reg.is_ok())
        {
            return err(make_syntax_error("toml::parse_floating: "
                "invalid hex floating: float must be like: 0xABCp-3f",
                syntax::floating(spec), loc));
        }
        str = reg.as_string();
    }
    else
    {
        reg = syntax::floating(spec).scan(loc);
        if( ! reg.is_ok())
        {
            return err(make_syntax_error("toml::parse_floating: "
                "invalid floating: float must be like: -3.14159_26535, 6.022e+23, "
                "inf, or nan (lowercase).", syntax::floating(spec), loc));
        }
        str = reg.as_string();
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    floating_format_info fmt;

    if(is_hex)
    {
        fmt.fmt = floating_format::hex;
    }
    else
    {
        // since we already checked that the string conforms the TOML standard.
        if(std::find(str.begin(), str.end(), 'e') != str.end() ||
           std::find(str.begin(), str.end(), 'E') != str.end())
        {
            fmt.fmt = floating_format::scientific; // use exponent part
        }
        else
        {
            fmt.fmt = floating_format::fixed; // do not use exponent part
        }
    }

    str.erase(std::remove(str.begin(), str.end(), '_'), str.end());

    floating_type val{0};

    if(str == "inf" || str == "+inf")
    {
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_infinity)
        {
            val = std::numeric_limits<floating_type>::infinity();
        }
        else
        {
            return err(make_error_info("toml::parse_floating: inf value found"
                " but the current environment does not support inf. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: inf is not supported"));
        }
    }
    else if(str == "-inf")
    {
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_infinity)
        {
            val = -std::numeric_limits<floating_type>::infinity();
        }
        else
        {
            return err(make_error_info("toml::parse_floating: inf value found"
                " but the current environment does not support inf. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: inf is not supported"));
        }
    }
    else if(str == "nan" || str == "+nan")
    {
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_quiet_NaN)
        {
            val = std::numeric_limits<floating_type>::quiet_NaN();
        }
        else TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_signaling_NaN)
        {
            val = std::numeric_limits<floating_type>::signaling_NaN();
        }
        else
        {
            return err(make_error_info("toml::parse_floating: NaN value found"
                " but the current environment does not support NaN. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: NaN is not supported"));
        }
    }
    else if(str == "-nan")
    {
        using std::copysign;
        TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_quiet_NaN)
        {
            val = copysign(std::numeric_limits<floating_type>::quiet_NaN(), floating_type(-1));
        }
        else TOML11_CONSTEXPR_IF(std::numeric_limits<floating_type>::has_signaling_NaN)
        {
            val = copysign(std::numeric_limits<floating_type>::signaling_NaN(), floating_type(-1));
        }
        else
        {
            return err(make_error_info("toml::parse_floating: NaN value found"
                " but the current environment does not support NaN. Please"
                " make sure that the floating-point implementation conforms"
                " IEEE 754/ISO 60559 international standard.",
                source_location(region(loc)),
                "floating_type: NaN is not supported"));
        }
    }
    else
    {
        // set precision
        const auto has_sign = ! str.empty() && (str.front() == '+' || str.front() == '-');
        const auto decpoint = std::find(str.begin(), str.end(), '.');
        const auto exponent = std::find_if(str.begin(), str.end(),
            [](const char c) { return c == 'e' || c == 'E'; });
        if(decpoint != str.end() && exponent != str.end())
        {
            assert(decpoint < exponent);
        }

        if(fmt.fmt == floating_format::scientific)
        {
            // total width
            fmt.prec = static_cast<std::size_t>(std::distance(str.begin(), exponent));
            if(has_sign)
            {
                fmt.prec -= 1;
            }
            if(decpoint != str.end())
            {
                fmt.prec -= 1;
            }
        }
        else if(fmt.fmt == floating_format::hex)
        {
            fmt.prec = std::numeric_limits<floating_type>::max_digits10;
        }
        else
        {
            // width after decimal point
            fmt.prec = static_cast<std::size_t>(std::distance(std::next(decpoint), exponent));
        }

        auto src = source_location(region(loc));
        const auto res = TC::parse_float(str, src, is_hex);
        if(res.is_ok())
        {
            val = res.as_ok();
        }
        else
        {
            return err(res.as_err());
        }
    }

    // ----------------------------------------------------------------------
    // parse suffix (extension)

    if(spec.ext_num_suffix && loc.current() == '_')
    {
        const auto sfx_reg = syntax::num_suffix(spec).scan(loc);
        if( ! sfx_reg.is_ok())
        {
            auto src = source_location(region(loc));
            loc = first;
            return err(make_error_info("toml::parse_floating: "
                "invalid suffix: should be `_ non-digit-graph (graph | _graph)`",
                std::move(src), "here"));
        }
        auto sfx = sfx_reg.as_string();
        assert( ! sfx.empty() && sfx.front() == '_');
        sfx.erase(sfx.begin()); // remove the first `_`

        fmt.suffix = sfx;
    }

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

/* ============================================================================
 *  ___       _       _   _
 * |   \ __ _| |_ ___| |_(_)_ __  ___
 * | |) / _` |  _/ -_)  _| | '  \/ -_)
 * |___/\__,_|\__\___|\__|_|_|_|_\___|
 */

// all the offset_datetime, local_datetime, local_date parses date part.
template<typename TC>
result<std::tuple<local_date, local_date_format_info, region>, error_info>
parse_local_date_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    local_date_format_info fmt;

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::local_date(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_local_date: "
            "invalid date: date must be like: 1234-05-06, yyyy-mm-dd.",
            syntax::local_date(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    const auto str = reg.as_string();

    // 0123456789
    // yyyy-mm-dd
    const auto year_r  = from_string<int>(str.substr(0, 4));
    const auto month_r = from_string<int>(str.substr(5, 2));
    const auto day_r   = from_string<int>(str.substr(8, 2));

    if(year_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_date: "
            "failed to read year `" + str.substr(0, 4) + "`",
            std::move(src), "here"));
    }
    if(month_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_date: "
            "failed to read month `" + str.substr(5, 2) + "`",
            std::move(src), "here"));
    }
    if(day_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_date: "
            "failed to read day `" + str.substr(8, 2) + "`",
            std::move(src), "here"));
    }

    const auto year  = year_r.unwrap();
    const auto month = month_r.unwrap();
    const auto day   = day_r.unwrap();

    {
        // We briefly check whether the input date is valid or not.
        //     Actually, because of the historical reasons, there are several
        // edge cases, such as 1582/10/5-1582/10/14 (only in several countries).
        // But here, we do not care about it.
        // It makes the code complicated and there is only low probability
        // that such a specific date is needed in practice. If someone need to
        // validate date accurately, that means that the one need a specialized
        // library for their purpose in another layer.

        const bool is_leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        const auto max_day = [month, is_leap]() {
            if(month == 2)
            {
                return is_leap ? 29 : 28;
            }
            if(month == 4 || month == 6 || month == 9 || month == 11)
            {
                return 30;
            }
            return 31;
        }();

        if((month < 1 || 12 < month) || (day < 1 || max_day < day))
        {
            auto src = source_location(region(first));
            return err(make_error_info("toml::parse_local_date: invalid date.",
                std::move(src), "month must be 01-12, day must be any of "
                "01-28,29,30,31 depending on the month/year."));
        }
    }

    return ok(std::make_tuple(
            local_date(year, static_cast<month_t>(month - 1), day),
            std::move(fmt), std::move(reg)
        ));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_local_date(location& loc, const context<TC>& ctx)
{
    auto val_fmt_reg = parse_local_date_only(loc, ctx);
    if(val_fmt_reg.is_err())
    {
        return err(val_fmt_reg.unwrap_err());
    }

    auto val = std::move(std::get<0>(val_fmt_reg.unwrap()));
    auto fmt = std::move(std::get<1>(val_fmt_reg.unwrap()));
    auto reg = std::move(std::get<2>(val_fmt_reg.unwrap()));

    return ok(basic_value<TC>(std::move(val), std::move(fmt), {}, std::move(reg)));
}

// all the offset_datetime, local_datetime, local_time parses date part.
template<typename TC>
result<std::tuple<local_time, local_time_format_info, region>, error_info>
parse_local_time_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    local_time_format_info fmt;

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::local_time(spec).scan(loc);
    if( ! reg.is_ok())
    {
        if(spec.v1_1_0_make_seconds_optional)
        {
            return err(make_syntax_error("toml::parse_local_time: "
                "invalid time: time must be HH:MM(:SS.sss) (seconds are optional)",
                syntax::local_time(spec), loc));
        }
        else
        {
            return err(make_syntax_error("toml::parse_local_time: "
                "invalid time: time must be HH:MM:SS(.sss) (subseconds are optional)",
                syntax::local_time(spec), loc));
        }
    }

    // ----------------------------------------------------------------------
    // it matches. gen value
    const auto str = reg.as_string();

    // at least we have HH:MM.
    // 01234
    // HH:MM
    const auto hour_r   = from_string<int>(str.substr(0, 2));
    const auto minute_r = from_string<int>(str.substr(3, 2));

    if(hour_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read hour `" + str.substr(0, 2) + "`",
            std::move(src), "here"));
    }
    if(minute_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read minute `" + str.substr(3, 2) + "`",
            std::move(src), "here"));
    }

    const auto hour   = hour_r.unwrap();
    const auto minute = minute_r.unwrap();

    if((hour < 0 || 24 <= hour) || (minute < 0 || 60 <= minute))
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: invalid time.",
            std::move(src), "hour must be 00-23, minute must be 00-59."));
    }

    // -----------------------------------------------------------------------
    // we have hour and minute.
    // Since toml v1.1.0, second and subsecond part becomes optional.
    // Check the version and return if second does not exist.

    if(str.size() == 5 && spec.v1_1_0_make_seconds_optional)
    {
        fmt.has_seconds = false;
        fmt.subsecond_precision = 0;
        return ok(std::make_tuple(local_time(hour, minute, 0), std::move(fmt), std::move(reg)));
    }
    assert(str.at(5) == ':');

    // we have at least `:SS` part. `.subseconds` are optional.

    // 0         1
    // 012345678901234
    // HH:MM:SS.subsec
    const auto sec_r = from_string<int>(str.substr(6, 2));
    if(sec_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read second `" + str.substr(6, 2) + "`",
            std::move(src), "here"));
    }
    const auto sec = sec_r.unwrap();

    if(sec < 0 || 60 < sec) // :60 is allowed
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: invalid time.",
                    std::move(src), "second must be 00-60."));
    }

    if(str.size() == 8)
    {
        fmt.has_seconds = true;
        fmt.subsecond_precision = 0;
        return ok(std::make_tuple(local_time(hour, minute, sec), std::move(fmt), std::move(reg)));
    }

    assert(str.at(8) == '.');

    auto secfrac = str.substr(9, str.size() - 9);

    fmt.has_seconds = true;
    fmt.subsecond_precision = secfrac.size();

    while(secfrac.size() < 9)
    {
        secfrac += '0';
    }
    assert(9 <= secfrac.size());
    const auto ms_r = from_string<int>(secfrac.substr(0, 3));
    const auto us_r = from_string<int>(secfrac.substr(3, 3));
    const auto ns_r = from_string<int>(secfrac.substr(6, 3));

    if(ms_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read milliseconds `" + secfrac.substr(0, 3) + "`",
            std::move(src), "here"));
    }
    if(us_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read microseconds`" + str.substr(3, 3) + "`",
            std::move(src), "here"));
    }
    if(ns_r.is_err())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_local_time: "
            "failed to read nanoseconds`" + str.substr(6, 3) + "`",
            std::move(src), "here"));
    }
    const auto ms = ms_r.unwrap();
    const auto us = us_r.unwrap();
    const auto ns = ns_r.unwrap();

    return ok(std::make_tuple(local_time(hour, minute, sec, ms, us, ns), std::move(fmt), std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_local_time(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    auto val_fmt_reg = parse_local_time_only(loc, ctx);
    if(val_fmt_reg.is_err())
    {
        return err(val_fmt_reg.unwrap_err());
    }

    auto val = std::move(std::get<0>(val_fmt_reg.unwrap()));
    auto fmt = std::move(std::get<1>(val_fmt_reg.unwrap()));
    auto reg = std::move(std::get<2>(val_fmt_reg.unwrap()));

    return ok(basic_value<TC>(std::move(val), std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_local_datetime(location& loc, const context<TC>& ctx)
{
    using char_type = location::char_type;

    const auto first = loc;

    local_datetime_format_info fmt;

    // ----------------------------------------------------------------------

    auto date_fmt_reg = parse_local_date_only(loc, ctx);
    if(date_fmt_reg.is_err())
    {
        return err(date_fmt_reg.unwrap_err());
    }

    if(loc.current() == char_type('T'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::upper_T;
    }
    else if(loc.current() == char_type('t'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::lower_t;
    }
    else if(loc.current() == char_type(' '))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::space;
    }
    else
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_local_datetime: "
            "expect date-time delimiter `T`, `t` or ` `(space).",
            std::move(src), "here"));
    }

    auto time_fmt_reg = parse_local_time_only(loc, ctx);
    if(time_fmt_reg.is_err())
    {
        return err(time_fmt_reg.unwrap_err());
    }

    fmt.has_seconds         = std::get<1>(time_fmt_reg.unwrap()).has_seconds;
    fmt.subsecond_precision = std::get<1>(time_fmt_reg.unwrap()).subsecond_precision;

    // ----------------------------------------------------------------------

    region reg(first, loc);
    local_datetime val(std::get<0>(date_fmt_reg.unwrap()),
                       std::get<0>(time_fmt_reg.unwrap()));

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_offset_datetime(location& loc, const context<TC>& ctx)
{
    using char_type = location::char_type;

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    offset_datetime_format_info fmt;

    // ----------------------------------------------------------------------
    // date part

    auto date_fmt_reg = parse_local_date_only(loc, ctx);
    if(date_fmt_reg.is_err())
    {
        return err(date_fmt_reg.unwrap_err());
    }

    // ----------------------------------------------------------------------
    // delimiter

    if(loc.current() == char_type('T'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::upper_T;
    }
    else if(loc.current() == char_type('t'))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::lower_t;
    }
    else if(loc.current() == char_type(' '))
    {
        loc.advance();
        fmt.delimiter = datetime_delimiter_kind::space;
    }
    else
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_offset_datetime: "
            "expect date-time delimiter `T` or ` `(space).", std::move(src), "here"
        ));
    }

    // ----------------------------------------------------------------------
    // time part

    auto time_fmt_reg = parse_local_time_only(loc, ctx);
    if(time_fmt_reg.is_err())
    {
        return err(time_fmt_reg.unwrap_err());
    }

    fmt.has_seconds         = std::get<1>(time_fmt_reg.unwrap()).has_seconds;
    fmt.subsecond_precision = std::get<1>(time_fmt_reg.unwrap()).subsecond_precision;

    // ----------------------------------------------------------------------
    // offset part

    const auto ofs_reg = syntax::time_offset(spec).scan(loc);
    if( ! ofs_reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_offset_datetime: "
            "invalid offset: offset must be like: Z, +01:00, or -10:00.",
            syntax::time_offset(spec), loc));
    }

    const auto ofs_str = ofs_reg.as_string();

    time_offset offset(0, 0);

    assert(ofs_str.size() != 0);

    if(ofs_str.at(0) == char_type('+') || ofs_str.at(0) == char_type('-'))
    {
        const auto hour_r   = from_string<int>(ofs_str.substr(1, 2));
        const auto minute_r = from_string<int>(ofs_str.substr(4, 2));
        if(hour_r.is_err())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_offset_datetime: "
                "Failed to read offset hour part", std::move(src), "here"));
        }
        if(minute_r.is_err())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_offset_datetime: "
                "Failed to read offset minute part", std::move(src), "here"));
        }
        const auto hour   = hour_r.unwrap();
        const auto minute = minute_r.unwrap();

        if(ofs_str.at(0) == '+')
        {
            offset = time_offset(hour, minute);
        }
        else
        {
            offset = time_offset(-hour, -minute);
        }
    }
    else
    {
        assert(ofs_str.at(0) == char_type('Z') || ofs_str.at(0) == char_type('z'));
    }

    if (offset.hour   < -24 || 24 < offset.hour ||
        offset.minute < -60 || 60 < offset.minute)
    {
        return err(make_error_info("toml::parse_offset_datetime: "
            "too large offset: |hour| <= 24, |minute| <= 60",
            source_location(region(first, loc)), "here"));
    }


    // ----------------------------------------------------------------------

    region reg(first, loc);
    offset_datetime val(local_datetime(std::get<0>(date_fmt_reg.unwrap()),
                                       std::get<0>(time_fmt_reg.unwrap())),
                                       offset);

    return ok(basic_value<TC>(val, std::move(fmt), {}, std::move(reg)));
}

/* ============================================================================
 *   ___ _       _
 *  / __| |_ _ _(_)_ _  __ _
 *  \__ \  _| '_| | ' \/ _` |
 *  |___/\__|_| |_|_||_\__, |
 *                     |___/
 */

template<typename TC>
result<typename basic_value<TC>::string_type, error_info>
parse_utf8_codepoint(const region& reg)
{
    using string_type = typename basic_value<TC>::string_type;
    using char_type = typename string_type::value_type;

    // assert(reg.as_lines().size() == 1); // XXX heavy check

    const auto str = reg.as_string();
    assert( ! str.empty());
    assert(str.front() == 'u' || str.front() == 'U' || str.front() == 'x');

    std::uint_least32_t codepoint;
    std::istringstream iss(str.substr(1));
    iss >> std::hex >> codepoint;

    const auto to_char = [](const std::uint_least32_t i) noexcept -> char_type {
        const auto uc = static_cast<unsigned char>(i & 0xFF);
        return cxx::bit_cast<char_type>(uc);
    };

    string_type character;
    if(codepoint < 0x80) // U+0000 ... U+0079 ; just an ASCII.
    {
        character += static_cast<char>(codepoint);
    }
    else if(codepoint < 0x800) //U+0080 ... U+07FF
    {
        // 110yyyyx 10xxxxxx; 0x3f == 0b0011'1111
        character += to_char(0xC0|(codepoint >> 6  ));
        character += to_char(0x80|(codepoint & 0x3F));
    }
    else if(codepoint < 0x10000) // U+0800...U+FFFF
    {
        if(0xD800 <= codepoint && codepoint <= 0xDFFF)
        {
            auto src = source_location(reg);
            return err(make_error_info("toml::parse_utf8_codepoint: "
                "[0xD800, 0xDFFF] is not a valid UTF-8",
                std::move(src), "here"));
        }
        assert(codepoint < 0xD800 || 0xDFFF < codepoint);
        // 1110yyyy 10yxxxxx 10xxxxxx
        character += to_char(0xE0| (codepoint >> 12));
        character += to_char(0x80|((codepoint >> 6 ) & 0x3F));
        character += to_char(0x80|((codepoint      ) & 0x3F));
    }
    else if(codepoint < 0x110000) // U+010000 ... U+10FFFF
    {
        // 11110yyy 10yyxxxx 10xxxxxx 10xxxxxx
        character += to_char(0xF0| (codepoint >> 18));
        character += to_char(0x80|((codepoint >> 12) & 0x3F));
        character += to_char(0x80|((codepoint >> 6 ) & 0x3F));
        character += to_char(0x80|((codepoint      ) & 0x3F));
    }
    else // out of UTF-8 region
    {
        auto src = source_location(reg);
        return err(make_error_info("toml::parse_utf8_codepoint: "
            "input codepoint is too large.",
            std::move(src), "must be in range [0x00, 0x10FFFF]"));
    }
    return ok(character);
}

template<typename TC>
result<typename basic_value<TC>::string_type, error_info>
parse_escape_sequence(location& loc, const context<TC>& ctx)
{
    using string_type = typename basic_value<TC>::string_type;
    using char_type = typename string_type::value_type;

    const auto& spec = ctx.toml_spec();

    assert( ! loc.eof());
    assert(loc.current() == '\\');
    loc.advance(); // consume the first backslash

    string_type retval;

    if     (loc.current() == '\\') { retval += char_type('\\'); loc.advance(); }
    else if(loc.current() == '"')  { retval += char_type('\"'); loc.advance(); }
    else if(loc.current() == 'b')  { retval += char_type('\b'); loc.advance(); }
    else if(loc.current() == 'f')  { retval += char_type('\f'); loc.advance(); }
    else if(loc.current() == 'n')  { retval += char_type('\n'); loc.advance(); }
    else if(loc.current() == 'r')  { retval += char_type('\r'); loc.advance(); }
    else if(loc.current() == 't')  { retval += char_type('\t'); loc.advance(); }
    else if(spec.v1_1_0_add_escape_sequence_e && loc.current() == 'e')
    {
        retval += char_type('\x1b');
        loc.advance();
    }
    else if(spec.v1_1_0_add_escape_sequence_x && loc.current() == 'x')
    {
        const auto reg = syntax::escaped_x2(spec).scan(loc);
        if( ! reg.is_ok())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_escape_sequence: "
                   "invalid token found in UTF-8 codepoint \\xhh",
                   std::move(src), "here"));
        }
        const auto utf8 = parse_utf8_codepoint<TC>(reg);
        if(utf8.is_err())
        {
            return err(utf8.as_err());
        }
        retval += utf8.unwrap();
    }
    else if(loc.current() == 'u')
    {
        const auto reg = syntax::escaped_u4(spec).scan(loc);
        if( ! reg.is_ok())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_escape_sequence: "
                   "invalid token found in UTF-8 codepoint \\uhhhh",
                   std::move(src), "here"));
        }
        const auto utf8 = parse_utf8_codepoint<TC>(reg);
        if(utf8.is_err())
        {
            return err(utf8.as_err());
        }
        retval += utf8.unwrap();
    }
    else if(loc.current() == 'U')
    {
        const auto reg = syntax::escaped_U8(spec).scan(loc);
        if( ! reg.is_ok())
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_escape_sequence: "
                   "invalid token found in UTF-8 codepoint \\Uhhhhhhhh",
                   std::move(src), "here"));
        }
        const auto utf8 = parse_utf8_codepoint<TC>(reg);
        if(utf8.is_err())
        {
            return err(utf8.as_err());
        }
        retval += utf8.unwrap();
    }
    else
    {
        auto src = source_location(region(loc));
        std::string escape_seqs = "allowed escape seqs: \\\\, \\\", \\b, \\f, \\n, \\r, \\t";
        if(spec.v1_1_0_add_escape_sequence_e)
        {
            escape_seqs += ", \\e";
        }
        if(spec.v1_1_0_add_escape_sequence_x)
        {
            escape_seqs += ", \\xhh";
        }
        escape_seqs += ", \\uhhhh, or \\Uhhhhhhhh";

        return err(make_error_info("toml::parse_escape_sequence: "
               "unknown escape sequence.", std::move(src), escape_seqs));
    }
    return ok(retval);
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_ml_basic_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    string_format_info fmt;
    fmt.fmt = string_format::multiline_basic;

    auto reg = syntax::ml_basic_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_ml_basic_string: "
            "invalid string format",
            syntax::ml_basic_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    // we already checked that it starts with """ and ends with """.
    assert(str.substr(0, 3) == "\"\"\"");
    str.erase(0, 3);

    assert(str.size() >= 3);
    assert(str.substr(str.size()-3, 3) == "\"\"\"");
    str.erase(str.size()-3, 3);

    // the first newline just after """ is trimmed
    if(str.size() >= 1 && str.at(0) == '\n')
    {
        str.erase(0, 1);
        fmt.start_with_newline = true;
    }
    else if(str.size() >= 2 && str.at(0) == '\r' && str.at(1) == '\n')
    {
        str.erase(0, 2);
        fmt.start_with_newline = true;
    }

    using string_type = typename basic_value<TC>::string_type;
    string_type val;
    {
        auto iter = str.cbegin();
        while(iter != str.cend())
        {
            if(*iter == '\\') // remove whitespaces around escaped-newline
            {
                // we assume that the string is not too long to copy
                auto loc2 = make_temporary_location(make_string(iter, str.cend()));
                if(syntax::escaped_newline(spec).scan(loc2).is_ok())
                {
                    std::advance(iter, loc2.get_location()); // skip escaped newline and indent
                    // now iter points non-WS char
                    assert(iter == str.end() || (*iter != ' ' && *iter != '\t'));
                }
                else // normal escape seq.
                {
                    auto esc = parse_escape_sequence(loc2, ctx);

                    // syntax does not check its value. the unicode codepoint may be
                    // invalid, e.g. out-of-bound, [0xD800, 0xDFFF]
                    if(esc.is_err())
                    {
                        return err(esc.unwrap_err());
                    }

                    val += esc.unwrap();
                    std::advance(iter, loc2.get_location());
                }
            }
            else // we already checked the syntax. we don't need to check it again.
            {
                val += static_cast<typename string_type::value_type>(*iter);
                ++iter;
            }
        }
    }

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, std::move(reg)
        ));
}

template<typename TC>
result<std::pair<typename basic_value<TC>::string_type, region>, error_info>
parse_basic_string_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::basic_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_basic_string: "
            "invalid string format",
            syntax::basic_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    assert(str.back() == '\"');
    str.pop_back();
    assert(str.at(0) == '\"');
    str.erase(0, 1);

    using string_type = typename basic_value<TC>::string_type;
    using char_type   = typename string_type::value_type;
    string_type val;

    {
        auto iter = str.begin();
        while(iter != str.end())
        {
            if(*iter == '\\')
            {
                auto loc2 = make_temporary_location(make_string(iter, str.end()));

                auto esc = parse_escape_sequence(loc2, ctx);

                // syntax does not check its value. the unicode codepoint may be
                // invalid, e.g. out-of-bound, [0xD800, 0xDFFF]
                if(esc.is_err())
                {
                    return err(esc.unwrap_err());
                }

                val += esc.unwrap();
                std::advance(iter, loc2.get_location());
            }
            else
            {
                val += char_type(*iter); // we already checked the syntax.
                ++iter;
            }
        }
    }
    return ok(std::make_pair(val, reg));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_basic_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    string_format_info fmt;
    fmt.fmt = string_format::basic;

    auto val_res = parse_basic_string_only(loc, ctx);
    if(val_res.is_err())
    {
        return err(std::move(val_res.unwrap_err()));
    }
    auto val = std::move(val_res.unwrap().first );
    auto reg = std::move(val_res.unwrap().second);

    return ok(basic_value<TC>(std::move(val), std::move(fmt), {}, std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_ml_literal_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    string_format_info fmt;
    fmt.fmt = string_format::multiline_literal;

    auto reg = syntax::ml_literal_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_ml_literal_string: "
            "invalid string format",
            syntax::ml_literal_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    assert(str.substr(0, 3) == "'''");
    assert(str.substr(str.size()-3, 3) == "'''");
    str.erase(0, 3);
    str.erase(str.size()-3, 3);

    // the first newline just after """ is trimmed
    if(str.size() >= 1 && str.at(0) == '\n')
    {
        str.erase(0, 1);
        fmt.start_with_newline = true;
    }
    else if(str.size() >= 2 && str.at(0) == '\r' && str.at(1) == '\n')
    {
        str.erase(0, 2);
        fmt.start_with_newline = true;
    }

    using string_type = typename basic_value<TC>::string_type;
    string_type val(str.begin(), str.end());

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, std::move(reg)
        ));
}

template<typename TC>
result<std::pair<typename basic_value<TC>::string_type, region>, error_info>
parse_literal_string_only(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::literal_string(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_literal_string: "
            "invalid string format",
            syntax::literal_string(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    auto str = reg.as_string();

    assert(str.back() == '\'');
    str.pop_back();
    assert(str.at(0) == '\'');
    str.erase(0, 1);

    using string_type = typename basic_value<TC>::string_type;
    string_type val(str.begin(), str.end());

    return ok(std::make_pair(std::move(val), std::move(reg)));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_literal_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    string_format_info fmt;
    fmt.fmt = string_format::literal;

    auto val_res = parse_literal_string_only(loc, ctx);
    if(val_res.is_err())
    {
        return err(std::move(val_res.unwrap_err()));
    }
    auto val = std::move(val_res.unwrap().first );
    auto reg = std::move(val_res.unwrap().second);

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, std::move(reg)
        ));
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_string(location& loc, const context<TC>& ctx)
{
    const auto first = loc;

    if( ! loc.eof() && loc.current() == '"')
    {
        if(literal("\"\"\"").scan(loc).is_ok())
        {
            loc = first;
            return parse_ml_basic_string(loc, ctx);
        }
        else
        {
            loc = first;
            return parse_basic_string(loc, ctx);
        }
    }
    else if( ! loc.eof() && loc.current() == '\'')
    {
        if(literal("'''").scan(loc).is_ok())
        {
            loc = first;
            return parse_ml_literal_string(loc, ctx);
        }
        else
        {
            loc = first;
            return parse_literal_string(loc, ctx);
        }
    }
    else
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_string: "
            "not a string", std::move(src), "here"));
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_null(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    if( ! spec.ext_null_value)
    {
        return err(make_error_info("toml::parse_null: "
            "invalid spec: spec.ext_null_value must be true.",
            source_location(region(loc)), "here"));
    }

    // ----------------------------------------------------------------------
    // check syntax
    auto reg = syntax::null_value(spec).scan(loc);
    if( ! reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_null: "
            "invalid null: null must be lowercase. ",
            syntax::null_value(spec), loc));
    }

    // ----------------------------------------------------------------------
    // it matches. gen value

    // ----------------------------------------------------------------------
    // no format info for boolean

    return ok(basic_value<TC>(detail::none_t{}, std::move(reg)));
}

/* ============================================================================
 *   _  __
 *  | |/ /___ _  _
 *  | ' </ -_) || |
 *  |_|\_\___|\_, |
 *            |__/
 */

// non-dotted key.
template<typename TC>
result<typename basic_value<TC>::key_type, error_info>
parse_simple_key(location& loc, const context<TC>& ctx)
{
    using key_type = typename basic_value<TC>::key_type;
    const auto& spec = ctx.toml_spec();

    if(loc.current() == '\"')
    {
        auto str_res = parse_basic_string_only(loc, ctx);
        if(str_res.is_ok())
        {
            return ok(std::move(str_res.unwrap().first));
        }
        else
        {
            return err(std::move(str_res.unwrap_err()));
        }
    }
    else if(loc.current() == '\'')
    {
        auto str_res = parse_literal_string_only(loc, ctx);
        if(str_res.is_ok())
        {
            return ok(std::move(str_res.unwrap().first));
        }
        else
        {
            return err(std::move(str_res.unwrap_err()));
        }
    }

    // bare key.

    if(const auto bare = syntax::unquoted_key(spec).scan(loc))
    {
        return ok(string_conv<key_type>(bare.as_string()));
    }
    else
    {
        std::string postfix;
        if(spec.ext_allow_non_english_in_bare_keys)
        {
            postfix = "Hint: Not all Unicode characters are allowed as bare key.\n";
        }
        else
        {
            postfix = "Hint: non-ASCII scripts are allowed in toml v1.1.0, but not in v1.0.0.\n";
        }
        return err(make_syntax_error("toml::parse_simple_key: "
            "invalid key: key must be \"quoted\", 'quoted-literal', or bare key.",
            syntax::unquoted_key(spec), loc, postfix));
    }
}

// dotted key become vector of keys
template<typename TC>
result<std::pair<std::vector<typename basic_value<TC>::key_type>, region>, error_info>
parse_key(location& loc, const context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    using key_type = typename basic_value<TC>::key_type;
    std::vector<key_type> keys;
    while( ! loc.eof())
    {
        auto key = parse_simple_key(loc, ctx);
        if( ! key.is_ok())
        {
            return err(key.unwrap_err());
        }
        keys.push_back(std::move(key.unwrap()));

        auto reg = syntax::dot_sep(spec).scan(loc);
        if( ! reg.is_ok())
        {
            break;
        }
    }
    if(keys.empty())
    {
        auto src = source_location(region(first));
        return err(make_error_info("toml::parse_key: expected a new key, "
                    "but got nothing", std::move(src), "reached EOF"));
    }

    return ok(std::make_pair(std::move(keys), region(first, loc)));
}

// ============================================================================

// forward-decl to implement parse_array and parse_table
template<typename TC>
result<basic_value<TC>, error_info>
parse_value(location&, context<TC>& ctx);

template<typename TC>
result<std::pair<
        std::pair<std::vector<typename basic_value<TC>::key_type>, region>,
        basic_value<TC>
    >, error_info>
parse_key_value_pair(location& loc, context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto key_res = parse_key(loc, ctx);
    if(key_res.is_err())
    {
        loc = first;
        return err(key_res.unwrap_err());
    }

    if( ! syntax::keyval_sep(spec).scan(loc).is_ok())
    {
        auto e = make_syntax_error("toml::parse_key_value_pair: "
            "invalid key value separator `=`", syntax::keyval_sep(spec), loc);
        loc = first;
        return err(std::move(e));
    }

    auto v_res = parse_value(loc, ctx);
    if(v_res.is_err())
    {
        // loc = first;
        return err(v_res.unwrap_err());
    }
    return ok(std::make_pair(std::move(key_res.unwrap()), std::move(v_res.unwrap())));
}

/* ============================================================================
 *    __ _ _ _ _ _ __ _ _  _
 *   / _` | '_| '_/ _` | || |
 *   \__,_|_| |_| \__,_|\_, |
 *                      |__/
 */

// array(and multiline inline table with `{` and `}`) has the following format.
// `[`
// (ws|newline|comment-line)? (value) (ws|newline|comment-line)? `,`
// (ws|newline|comment-line)? (value) (ws|newline|comment-line)? `,`
// ...
// (ws|newline|comment-line)? (value) (ws|newline|comment-line)? (`,`)?
// (ws|newline|comment-line)? `]`
// it skips (ws|newline|comment-line) and returns the token.
template<typename TC>
struct multiline_spacer
{
    using comment_type = typename TC::comment_type;
    bool         newline_found;
    indent_char  indent_type;
    std::int32_t indent;
    comment_type comments;
};
template<typename T>
std::ostream& operator<<(std::ostream& os, const multiline_spacer<T>& sp)
{
    os << "{newline="    << sp.newline_found << ", ";
    os << "indent_type=" << sp.indent_type << ", ";
    os << "indent="      << sp.indent << ", ";
    os << "comments="    << sp.comments.size() << "}";
    return os;
}

template<typename TC>
cxx::optional<multiline_spacer<TC>>
skip_multiline_spacer(location& loc, context<TC>& ctx, const bool newline_found = false)
{
    const auto& spec = ctx.toml_spec();

    multiline_spacer<TC> spacer;
    spacer.newline_found = newline_found;
    spacer.indent_type   = indent_char::none;
    spacer.indent        = 0;
    spacer.comments.clear();

    bool spacer_found = false;
    while( ! loc.eof())
    {
        if(auto comm = sequence(syntax::comment(spec), syntax::newline(spec)).scan(loc))
        {
            spacer.newline_found = true;
            auto comment = comm.as_string();
            if( ! comment.empty() && comment.back() == '\n')
            {
                comment.pop_back();
                if (!comment.empty() && comment.back() == '\r')
                {
                    comment.pop_back();
                }
            }

            spacer.comments.push_back(std::move(comment));
            spacer.indent_type = indent_char::none;
            spacer.indent = 0;
            spacer_found = true;
        }
        else if(auto nl = syntax::newline(spec).scan(loc))
        {
            spacer.newline_found = true;
            spacer.comments.clear();
            spacer.indent_type = indent_char::none;
            spacer.indent = 0;
            spacer_found = true;
        }
        else if(auto sp = repeat_at_least(1, character(cxx::bit_cast<location::char_type>(' '))).scan(loc))
        {
            spacer.indent_type = indent_char::space;
            spacer.indent      = static_cast<std::int32_t>(sp.length());
            spacer_found = true;
        }
        else if(auto tabs = repeat_at_least(1, character(cxx::bit_cast<location::char_type>('\t'))).scan(loc))
        {
            spacer.indent_type = indent_char::tab;
            spacer.indent      = static_cast<std::int32_t>(tabs.length());
            spacer_found = true;
        }
        else
        {
            break; // done
        }
    }
    if( ! spacer_found)
    {
        return cxx::make_nullopt();
    }
    return spacer;
}

// not an [[array.of.tables]]. It parses ["this", "type"]
template<typename TC>
result<basic_value<TC>, error_info>
parse_array(location& loc, context<TC>& ctx)
{
    const auto num_errors = ctx.errors().size();

    const auto first = loc;

    if(loc.eof() || loc.current() != '[')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_array: "
                "The next token is not an array", std::move(src), "here"));
    }
    loc.advance();

    typename basic_value<TC>::array_type val;

    array_format_info fmt;
    fmt.fmt = array_format::oneline;
    fmt.indent_type = indent_char::none;

    auto spacer = skip_multiline_spacer(loc, ctx);
    if(spacer.has_value() && spacer.value().newline_found)
    {
        fmt.fmt = array_format::multiline;
    }

    bool comma_found = true;
    while( ! loc.eof())
    {
        if(loc.current() == location::char_type(']'))
        {
            if(spacer.has_value() && spacer.value().newline_found &&
                spacer.value().indent_type != indent_char::none)
            {
                fmt.indent_type    = spacer.value().indent_type;
                fmt.closing_indent = spacer.value().indent;
            }
            break;
        }

        if( ! comma_found)
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_array: "
                    "expected value-separator `,` or closing `]`",
                    std::move(src), "here"));
        }

        if(spacer.has_value() && spacer.value().newline_found &&
            spacer.value().indent_type != indent_char::none)
        {
            fmt.indent_type = spacer.value().indent_type;
            fmt.body_indent = spacer.value().indent;
        }

        if(auto elem_res = parse_value(loc, ctx))
        {
            auto elem = std::move(elem_res.unwrap());

            if(spacer.has_value()) // copy previous comments to value
            {
                elem.comments() = std::move(spacer.value().comments);
            }

            // parse spaces between a value and a comma
            // array = [
            //     42    , # the answer
            //       ^^^^
            //     3.14 # pi
            //   , 2.71 ^^^^
            // ^^
            spacer = skip_multiline_spacer(loc, ctx);
            if(spacer.has_value())
            {
                for(std::size_t i=0; i<spacer.value().comments.size(); ++i)
                {
                    elem.comments().push_back(std::move(spacer.value().comments.at(i)));
                }
                if(spacer.value().newline_found)
                {
                    fmt.fmt = array_format::multiline;
                }
            }

            comma_found = character(',').scan(loc).is_ok();

            // parse comment after a comma
            // array = [
            //     42    , # the answer
            //             ^^^^^^^^^^^^
            //     3.14 # pi
            //          ^^^^
            // ]
            auto com_res = parse_comment_line(loc, ctx);
            if(com_res.is_err())
            {
                ctx.report_error(com_res.unwrap_err());
            }

            const bool comment_found = com_res.is_ok() && com_res.unwrap().has_value();
            if(comment_found)
            {
                fmt.fmt = array_format::multiline;
                elem.comments().push_back(com_res.unwrap().value());
            }
            if(comma_found)
            {
                spacer = skip_multiline_spacer(loc, ctx, comment_found);
                if(spacer.has_value() && spacer.value().newline_found)
                {
                    fmt.fmt = array_format::multiline;
                }
            }
            val.push_back(std::move(elem));
        }
        else
        {
            // if err, push error to ctx and try recovery.
            ctx.report_error(std::move(elem_res.unwrap_err()));

            // if it looks like some value, then skip the value.
            // otherwise, it may be a new key-value pair or a new table and
            // the error is "missing closing ]". stop parsing.

            const auto before_skip = loc.get_location();
            skip_value(loc, ctx);
            if(before_skip == loc.get_location()) // cannot skip! break...
            {
                break;
            }
        }
    }

    if(loc.current() != ']')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_array: missing closing bracket `]`",
            std::move(src), "expected `]`, reached EOF"));
    }
    else
    {
        loc.advance();
    }
    // any error reported from this function
    if(num_errors != ctx.errors().size())
    {
        assert(ctx.has_error()); // already reported
        return err(ctx.errors().back());
    }

    return ok(basic_value<TC>(
            std::move(val), std::move(fmt), {}, region(first, loc)
        ));
}

/* ============================================================================
 *   _      _ _            _        _    _
 *  (_)_ _ | (_)_ _  ___  | |_ __ _| |__| |___
 *  | | ' \| | | ' \/ -_) |  _/ _` | '_ \ / -_)
 *  |_|_||_|_|_|_||_\___|  \__\__,_|_.__/_\___|
 */

// ----------------------------------------------------------------------------
// insert_value is the most complicated part of the toml spec.
//
// To parse a toml file correctly, we sometimes need to check an exising value
// is appendable or not.
//
// For example while parsing an inline array of tables,
//
// ```toml
// aot = [
//   {a = "foo"},
//   {a = "bar", b = "baz"},
// ]
// ```
//
// this `aot` is appendable until parser reaches to `]`. After that, it becomes
// non-appendable.
//
// On the other hand, a normal array of tables, such as
//
// ```toml
// [[aot]]
// a = "foo"
//
// [[aot]]
// a = "bar"
// b = "baz"
// ```
// This `[[aot]]` is appendable until the parser reaches to the EOF.
//
//
// It becomes a bit more difficult in case of dotted keys.
// In TOML, it is allowed to append a key-value pair to a table that is
// *implicitly* defined by a subtable definitino.
//
// ```toml
// [x.y.z]
// w = 123
//
// [x]
// a = "foo" # OK. x is defined implicitly by `[x.y.z]`.
// ```
//
// But if the table is defined by a dotted keys, it is not appendable.
//
// ```toml
// [x]
// y.z.w = 123
//
// [x.y]
// # ERROR. x.y is already defined by a dotted table in the previous table.
// ```
//
// Also, reopening a table using dotted keys is invalid.
//
// ```toml
// [x.y.z]
// w = 123
//
// [x]
// y.z.v = 42 # ERROR. [x.y.z] is already defined.
// ```
//
//
// ```toml
// [a]
// b.c = "foo"
// b.d = "bar"
// ```
//
//
// ```toml
// a.b = "foo"
// [a]
// c = "bar" # ERROR
// ```
//
// In summary,
// - a table must be defined only once.
// - assignment to an exising table is possible only when:
//   - defining a subtable [x.y] to an existing table [x].
//   - defining supertable [x] explicitly after [x.y].
//   - adding dotted keys in the same table.

enum class inserting_value_kind : std::uint8_t
{
    std_table,   // insert [standard.table]
    array_table, // insert [[array.of.tables]]
    dotted_keys  // insert a.b.c = "this"
};

template<typename TC>
result<basic_value<TC>*, error_info>
insert_value(const inserting_value_kind kind,
    typename basic_value<TC>::table_type* current_table_ptr,
    const std::vector<typename basic_value<TC>::key_type>& keys, region key_reg,
    basic_value<TC> val)
{
    using value_type = basic_value<TC>;
    using array_type = typename basic_value<TC>::array_type;
    using table_type = typename basic_value<TC>::table_type;

    auto key_loc = source_location(key_reg);

    assert( ! keys.empty());

    // dotted key can insert to dotted key tables defined at the same level.
    // dotted key can NOT reopen a table even if it is implcitly-defined one.
    //
    // [x.y.z] # define x and x.y implicitly.
    // a = 42
    //
    // [x] # reopening implcitly defined table
    // r.s.t = 3.14 # VALID r and r.s are new tables.
    // r.s.u = 2.71 # VALID r and r.s are dotted-key tables. valid.
    //
    // y.z.b = "foo" # INVALID x.y.z are multiline table, not a dotted key.
    // y.c   = "bar" # INVALID x.y is implicit multiline table, not a dotted key.

    // a table cannot reopen dotted-key tables.
    //
    // [t1]
    // t2.t3.v = 0
    // [t1.t2] # INVALID t1.t2 is defined as a dotted-key table.

    for(std::size_t i=0; i<keys.size(); ++i)
    {
        const auto& key = keys.at(i);
        table_type& current_table = *current_table_ptr;

        if(i+1 < keys.size()) // there are more keys. go down recursively...
        {
            const auto found = current_table.find(key);
            if(found == current_table.end()) // not found. add new table
            {
                table_format_info fmt;
                fmt.indent_type = indent_char::none;
                if(kind == inserting_value_kind::dotted_keys)
                {
                    fmt.fmt = table_format::dotted;
                }
                else // table / array of tables
                {
                    fmt.fmt = table_format::implicit;
                }
                current_table.emplace(key, value_type(
                    table_type{}, fmt, std::vector<std::string>{}, key_reg));

                assert(current_table.at(key).is_table());
                current_table_ptr = std::addressof(current_table.at(key).as_table());
            }
            else if (found->second.is_table())
            {
                const auto fmt = found->second.as_table_fmt().fmt;
                if(fmt == table_format::oneline || fmt == table_format::multiline_oneline)
                {
                    // foo = {bar = "baz"} or foo = { \n bar = "baz" \n }
                    return err(make_error_info("toml::insert_value: "
                        "failed to insert a value: inline table is immutable",
                        key_loc, "inserting this",
                        found->second.location(), "to this table"));
                }
                // dotted key cannot reopen a table.
                if(kind ==inserting_value_kind::dotted_keys && fmt != table_format::dotted)
                {
                    return err(make_error_info("toml::insert_value: "
                        "reopening a table using dotted keys",
                        key_loc, "dotted key cannot reopen a table",
                        found->second.location(), "this table is already closed"));
                }
                assert(found->second.is_table());
                current_table_ptr = std::addressof(found->second.as_table());
            }
            else if(found->second.is_array_of_tables())
            {
                // aot = [{this = "type", of = "aot"}] # cannot be reopened
                if(found->second.as_array_fmt().fmt != array_format::array_of_tables)
                {
                    return err(make_error_info("toml::insert_value:"
                        "inline array of tables are immutable",
                        key_loc, "inserting this",
                        found->second.location(), "inline array of tables"));
                }
                // appending to [[aot]]

                if(kind == inserting_value_kind::dotted_keys)
                {
                    // [[array.of.tables]]
                    // [array.of]          # reopening supertable is okay
                    // tables.x = "foo"    # appending `x` to the first table
                    return err(make_error_info("toml::insert_value:"
                        "dotted key cannot reopen an array-of-tables",
                        key_loc, "inserting this",
                        found->second.location(), "to this array-of-tables."));
                }

                // insert_value_by_dotkeys::std_table
                // [[array.of.tables]]
                // [array.of.tables.subtable] # appending to the last aot
                //
                // insert_value_by_dotkeys::array_table
                // [[array.of.tables]]
                // [[array.of.tables.subtable]] # appending to the last aot
                auto& current_array_table = found->second.as_array().back();

                assert(current_array_table.is_table());
                current_table_ptr = std::addressof(current_array_table.as_table());
            }
            else
            {
                return err(make_error_info("toml::insert_value: "
                    "failed to insert a value, value already exists",
                    key_loc, "while inserting this",
                    found->second.location(), "non-table value already exists"));
            }
        }
        else // this is the last key. insert a new value.
        {
            switch(kind)
            {
                case inserting_value_kind::dotted_keys:
                {
                    if(current_table.find(key) != current_table.end())
                    {
                        return err(make_error_info("toml::insert_value: "
                            "failed to insert a value, value already exists",
                            key_loc, "inserting this",
                            current_table.at(key).location(), "but value already exists"));
                    }
                    current_table.emplace(key, std::move(val));
                    return ok(std::addressof(current_table.at(key)));
                }
                case inserting_value_kind::std_table:
                {
                    // defining a new table or reopening supertable
                    auto found = current_table.find(key);
                    if(found == current_table.end()) // define a new aot
                    {
                        current_table.emplace(key, std::move(val));
                        return ok(std::addressof(current_table.at(key)));
                    }
                    else // the table is already defined, reopen it
                    {
                        // assigning a [std.table]. it must be an implicit table.
                        auto& target = found->second;
                        if( ! target.is_table() || // could be an array-of-tables
                            target.as_table_fmt().fmt != table_format::implicit)
                        {
                            return err(make_error_info("toml::insert_value: "
                                "failed to insert a table, table already defined",
                                key_loc, "inserting this",
                                target.location(), "this table is explicitly defined"));
                        }

                        // merge table
                        for(const auto& kv : val.as_table())
                        {
                            if(target.contains(kv.first))
                            {
                                // [x.y.z]
                                // w = "foo"
                                // [x]
                                // y = "bar"
                                return err(make_error_info("toml::insert_value: "
                                    "failed to insert a table, table keys conflict to each other",
                                    key_loc, "inserting this table",
                                    kv.second.location(), "having this value",
                                    target.at(kv.first).location(), "already defined here"));
                            }
                            else
                            {
                                target[kv.first] = kv.second;
                            }
                        }
                        // change implicit -> explicit
                        target.as_table_fmt().fmt = table_format::multiline;
                        // change definition region
                        change_region_of_value(target, val);

                        return ok(std::addressof(current_table.at(key)));
                    }
                }
                case inserting_value_kind::array_table:
                {
                    auto found = current_table.find(key);
                    if(found == current_table.end()) // define a new aot
                    {
                        array_format_info fmt;
                        fmt.fmt = array_format::array_of_tables;
                        fmt.indent_type = indent_char::none;

                        current_table.emplace(key, value_type(
                                array_type{ std::move(val) }, std::move(fmt),
                                std::vector<std::string>{}, std::move(key_reg)
                            ));

                        assert( ! current_table.at(key).as_array().empty());
                        return ok(std::addressof(current_table.at(key).as_array().back()));
                    }
                    else // the array is already defined, append to it
                    {
                        if( ! found->second.is_array_of_tables())
                        {
                            return err(make_error_info("toml::insert_value: "
                                "failed to insert an array of tables, value already exists",
                                key_loc, "while inserting this",
                                found->second.location(), "non-table value already exists"));
                        }
                        if(found->second.as_array_fmt().fmt != array_format::array_of_tables)
                        {
                            return err(make_error_info("toml::insert_value: "
                                "failed to insert a table, inline array of tables is immutable",
                                key_loc, "while inserting this",
                                found->second.location(), "this is inline array-of-tables"));
                        }
                        found->second.as_array().push_back(std::move(val));
                        assert( ! current_table.at(key).as_array().empty());
                        return ok(std::addressof(current_table.at(key).as_array().back()));
                    }
                }
                default: {assert(false);}
            }
        }
    }
    return err(make_error_info("toml::insert_key: no keys found",
                std::move(key_loc), "here"));
}

// ----------------------------------------------------------------------------

template<typename TC>
result<basic_value<TC>, error_info>
parse_inline_table(location& loc, context<TC>& ctx)
{
    using table_type = typename basic_value<TC>::table_type;

    const auto num_errors = ctx.errors().size();

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    if(loc.eof() || loc.current() != '{')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_inline_table: "
            "The next token is not an inline table", std::move(src), "here"));
    }
    loc.advance();

    table_type table;
    table_format_info fmt;
    fmt.fmt = table_format::oneline;
    fmt.indent_type = indent_char::none;

    cxx::optional<multiline_spacer<TC>> spacer(cxx::make_nullopt());

    if(spec.v1_1_0_allow_newlines_in_inline_tables)
    {
        spacer = skip_multiline_spacer(loc, ctx);
        if(spacer.has_value() && spacer.value().newline_found)
        {
            fmt.fmt = table_format::multiline_oneline;
        }
    }
    else
    {
        skip_whitespace(loc, ctx);
    }

    bool still_empty = true;
    bool comma_found = false;
    while( ! loc.eof())
    {
        // closing!
        if(loc.current() == '}')
        {
            if(comma_found && ! spec.v1_1_0_allow_trailing_comma_in_inline_tables)
            {
                auto src = source_location(region(loc));
                return err(make_error_info("toml::parse_inline_table: trailing "
                    "comma is not allowed in TOML-v1.0.0)", std::move(src), "here"));
            }

            if(spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                if(spacer.has_value() && spacer.value().newline_found &&
                    spacer.value().indent_type != indent_char::none)
                {
                    fmt.indent_type    = spacer.value().indent_type;
                    fmt.closing_indent = spacer.value().indent;
                }
            }
            break;
        }

        // if we already found a value and didn't found `,` nor `}`, error.
        if( ! comma_found && ! still_empty)
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_inline_table: "
                    "expected value-separator `,` or closing `}`",
                    std::move(src), "here"));
        }

        // parse indent.
        if(spacer.has_value() && spacer.value().newline_found &&
            spacer.value().indent_type != indent_char::none)
        {
            fmt.indent_type = spacer.value().indent_type;
            fmt.body_indent = spacer.value().indent;
        }

        still_empty = false; // parsing a value...
        if(auto kv_res = parse_key_value_pair<TC>(loc, ctx))
        {
            auto keys    = std::move(kv_res.unwrap().first.first);
            auto key_reg = std::move(kv_res.unwrap().first.second);
            auto val     = std::move(kv_res.unwrap().second);

            auto ins_res = insert_value(inserting_value_kind::dotted_keys,
                std::addressof(table), keys, std::move(key_reg), std::move(val));
            if(ins_res.is_err())
            {
                ctx.report_error(std::move(ins_res.unwrap_err()));
                // we need to skip until the next value (or end of the table)
                // because we don't have valid kv pair.
                while( ! loc.eof())
                {
                    const auto c = loc.current();
                    if(c == ',' || c == '\n' || c == '}')
                    {
                        comma_found = (c == ',');
                        break;
                    }
                    loc.advance();
                }
                continue;
            }

            // if comment line follows immediately(without newline) after `,`, then
            // the comment is for the elem. we need to check if comment follows `,`.
            //
            // (key) = (val) (ws|newline|comment-line)? `,` (ws)? (comment)?

            if(spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                if(spacer.has_value()) // copy previous comments to value
                {
                    for(std::size_t i=0; i<spacer.value().comments.size(); ++i)
                    {
                        ins_res.unwrap()->comments().push_back(spacer.value().comments.at(i));
                    }
                }
                spacer = skip_multiline_spacer(loc, ctx);
                if(spacer.has_value())
                {
                    for(std::size_t i=0; i<spacer.value().comments.size(); ++i)
                    {
                        ins_res.unwrap()->comments().push_back(spacer.value().comments.at(i));
                    }
                    if(spacer.value().newline_found)
                    {
                        fmt.fmt = table_format::multiline_oneline;
                        if(spacer.value().indent_type != indent_char::none)
                        {
                            fmt.indent_type = spacer.value().indent_type;
                            fmt.body_indent = spacer.value().indent;
                        }
                    }
                }
            }
            else
            {
                skip_whitespace(loc, ctx);
            }

            comma_found = character(',').scan(loc).is_ok();

            if(spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                auto com_res = parse_comment_line(loc, ctx);
                if(com_res.is_err())
                {
                    ctx.report_error(com_res.unwrap_err());
                }
                const bool comment_found = com_res.is_ok() && com_res.unwrap().has_value();
                if(comment_found)
                {
                    fmt.fmt = table_format::multiline_oneline;
                    ins_res.unwrap()->comments().push_back(com_res.unwrap().value());
                }
                if(comma_found)
                {
                    spacer = skip_multiline_spacer(loc, ctx, comment_found);
                    if(spacer.has_value() && spacer.value().newline_found)
                    {
                        fmt.fmt = table_format::multiline_oneline;
                    }
                }
            }
            else
            {
                skip_whitespace(loc, ctx);
            }
        }
        else
        {
            ctx.report_error(std::move(kv_res.unwrap_err()));
            while( ! loc.eof())
            {
                if(loc.current() == '}')
                {
                    break;
                }
                if( ! spec.v1_1_0_allow_newlines_in_inline_tables && loc.current() == '\n')
                {
                    break;
                }
                loc.advance();
            }
            break;
        }
    }

    if(loc.current() != '}')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("toml::parse_inline_table: "
            "missing closing bracket `}`",
            std::move(src), "expected `}`, reached line end"));
    }
    else
    {
        loc.advance(); // skip }
    }

    // any error reported from this function
    if(num_errors < ctx.errors().size())
    {
        assert(ctx.has_error()); // already reported
        return err(ctx.pop_last_error());
    }

    basic_value<TC> retval(
        std::move(table), std::move(fmt), {}, region(first, loc));

    return ok(std::move(retval));
}

/* ============================================================================
 *            _
 *  __ ____ _| |_  _ ___
 *  \ V / _` | | || / -_)
 *   \_/\__,_|_|\_,_\___|
 */

template<typename TC>
result<value_t, error_info>
guess_number_type(const location& first, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    location loc = first;

    if(syntax::offset_datetime(spec).scan(loc).is_ok())
    {
        return ok(value_t::offset_datetime);
    }
    loc = first;

    if(syntax::local_datetime(spec).scan(loc).is_ok())
    {
        const auto curr = loc.current();
        // if offset_datetime contains bad offset, it syntax::offset_datetime
        // fails to scan it.
        if(curr == '+' || curr == '-')
        {
            return err(make_syntax_error("bad offset: must be [+-]HH:MM or Z",
                syntax::time_offset(spec), loc, std::string(
                "Hint: valid  : +09:00, -05:30\n"
                "Hint: invalid: +9:00,  -5:30\n")));
        }
        return ok(value_t::local_datetime);
    }
    loc = first;

    if(syntax::local_date(spec).scan(loc).is_ok())
    {
        // bad time may appear after this.

        if( ! loc.eof())
        {
            const auto c = loc.current();
            if(c == 'T' || c == 't')
            {
                loc.advance();

                return err(make_syntax_error("bad time: must be HH:MM:SS.subsec",
                    syntax::local_time(spec), loc, std::string(
                    "Hint: valid  : 1979-05-27T07:32:00, 1979-05-27 07:32:00.999999\n"
                    "Hint: invalid: 1979-05-27T7:32:00, 1979-05-27 17:32\n")));
            }
            if(c == ' ')
            {
                // A space is allowed as a delimiter between local time.
                // But there is a case where bad time follows a space.
                // - invalid: 2019-06-16 7:00:00
                // - valid  : 2019-06-16 07:00:00
                loc.advance();
                if( ! loc.eof() && ('0' <= loc.current() && loc.current() <= '9'))
                {
                    return err(make_syntax_error("bad time: must be HH:MM:SS.subsec",
                        syntax::local_time(spec), loc, std::string(
                        "Hint: valid  : 1979-05-27T07:32:00, 1979-05-27 07:32:00.999999\n"
                        "Hint: invalid: 1979-05-27T7:32:00, 1979-05-27 17:32\n")));
                }
            }
            if('0' <= c && c <= '9')
            {
                return err(make_syntax_error("bad datetime: missing T or space",
                    character_either("Tt "), loc, std::string(
                    "Hint: valid  : 1979-05-27T07:32:00, 1979-05-27 07:32:00.999999\n"
                    "Hint: invalid: 1979-05-27T7:32:00, 1979-05-27 17:32\n")));
            }
        }
        return ok(value_t::local_date);
    }
    loc = first;

    if(syntax::local_time(spec).scan(loc).is_ok())
    {
        return ok(value_t::local_time);
    }
    loc = first;

    if(syntax::floating(spec).scan(loc).is_ok())
    {
        if( ! loc.eof() && loc.current() == '_')
        {
            if(spec.ext_num_suffix && syntax::num_suffix(spec).scan(loc).is_ok())
            {
                return ok(value_t::floating);
            }
            auto src = source_location(region(loc));
            return err(make_error_info(
                "bad float: `_` must be surrounded by digits",
                std::move(src), "invalid underscore",
                "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
                "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n"));
        }
        return ok(value_t::floating);
    }
    loc = first;

    if(spec.ext_hex_float)
    {
        if(syntax::hex_floating(spec).scan(loc).is_ok())
        {
            if( ! loc.eof() && loc.current() == '_')
            {
                if(spec.ext_num_suffix && syntax::num_suffix(spec).scan(loc).is_ok())
                {
                    return ok(value_t::floating);
                }
                auto src = source_location(region(loc));
                return err(make_error_info(
                    "bad float: `_` must be surrounded by digits",
                    std::move(src), "invalid underscore",
                    "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
                    "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n"));
            }
            return ok(value_t::floating);
        }
        loc = first;
    }

    if(auto int_reg = syntax::integer(spec).scan(loc))
    {
        if( ! loc.eof())
        {
            const auto c = loc.current();
            if(c == '_')
            {
                if(spec.ext_num_suffix && syntax::num_suffix(spec).scan(loc).is_ok())
                {
                    return ok(value_t::integer);
                }

                if(int_reg.length() <= 2 && (int_reg.as_string() == "0" ||
                    int_reg.as_string() == "-0" || int_reg.as_string() == "+0"))
                {
                    auto src = source_location(region(loc));
                    return err(make_error_info(
                        "bad integer: leading zero is not allowed in decimal int",
                        std::move(src), "leading zero",
                        "Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                        "Hint: invalid: _42, 1__000, 0123\n"));
                }
                else
                {
                    auto src = source_location(region(loc));
                    return err(make_error_info(
                        "bad integer: `_` must be surrounded by digits",
                        std::move(src), "invalid underscore",
                        "Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                        "Hint: invalid: _42, 1__000, 0123\n"));
                }
            }
            if('0' <= c && c <= '9')
            {
                if(loc.current() == '0')
                {
                    loc.retrace();
                    return err(make_error_info(
                        "bad integer: leading zero",
                        source_location(region(loc)), "leading zero is not allowed",
                        std::string("Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                                    "Hint: invalid: _42, 1__000, 0123\n")
                        ));
                }
                else // invalid digits, especially in oct/bin ints.
                {
                    return err(make_error_info(
                        "bad integer: invalid digit after an integer",
                        source_location(region(loc)), "this digit is not allowed",
                        std::string("Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
                                    "Hint: invalid: _42, 1__000, 0123\n")
                        ));
                }
            }
            if(c == ':' || c == '-')
            {
                auto src = source_location(region(loc));
                return err(make_error_info("bad datetime: invalid format",
                    std::move(src), "here",
                    std::string("Hint: valid  : 1979-05-27T07:32:00-07:00, 1979-05-27 07:32:00.999999Z\n"
                                "Hint: invalid: 1979-05-27T7:32:00-7:00, 1979-05-27 7:32-00:30")
                    ));
            }
            if(c == '.' || c == 'e' || c == 'E')
            {
                auto src = source_location(region(loc));
                return err(make_error_info("bad float: invalid format",
                    std::move(src), "here", std::string(
                    "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
                    "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n")));
            }
        }
        return ok(value_t::integer);
    }
    if( ! loc.eof() && loc.current() == '.')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("bad float: integer part is required before decimal point",
            std::move(src), "missing integer part", std::string(
            "Hint: valid  : +1.0, -2e-2, 3.141_592_653_589, inf, nan\n"
            "Hint: invalid: .0, 1., _1.0, 1.0_, 1_.0, 1.0__0\n")
            ));
    }
    if( ! loc.eof() && loc.current() == '_')
    {
        auto src = source_location(region(loc));
        return err(make_error_info("bad number: `_` must be surrounded by digits",
            std::move(src), "digits required before `_`", std::string(
            "Hint: valid  : -42, 1_000, 1_2_3_4_5, 0xC0FFEE, 0b0010, 0o755\n"
            "Hint: invalid: _42, 1__000, 0123\n")
            ));
    }

    auto src = source_location(region(loc));
    return err(make_error_info("bad format: unknown value appeared",
                std::move(src), "here"));
}

template<typename TC>
result<value_t, error_info>
guess_value_type(const location& loc, const context<TC>& ctx)
{
    const auto& sp = ctx.toml_spec();
    location inner(loc);

    switch(loc.current())
    {
        case '"' : {return ok(value_t::string);  }
        case '\'': {return ok(value_t::string);  }
        case '[' : {return ok(value_t::array);   }
        case '{' : {return ok(value_t::table);   }
        case 't' :
        {
            return ok(value_t::boolean);
        }
        case 'f' :
        {
            return ok(value_t::boolean);
        }
        case 'T' : // invalid boolean.
        {
            return err(make_syntax_error("toml::parse_value: "
                "`true` must be in lowercase. "
                "A string must be surrounded by quotes.",
                syntax::boolean(sp), inner));
        }
        case 'F' :
        {
            return err(make_syntax_error("toml::parse_value: "
                "`false` must be in lowercase. "
                "A string must be surrounded by quotes.",
                syntax::boolean(sp), inner));
        }
        case 'i' : // inf or string without quotes(syntax error).
        {
            if(literal("inf").scan(inner).is_ok())
            {
                return ok(value_t::floating);
            }
            else
            {
                return err(make_syntax_error("toml::parse_value: "
                    "`inf` must be in lowercase. "
                    "A string must be surrounded by quotes.",
                    syntax::floating(sp), inner));
            }
        }
        case 'I' : // Inf or string without quotes(syntax error).
        {
            return err(make_syntax_error("toml::parse_value: "
                "`inf` must be in lowercase. "
                "A string must be surrounded by quotes.",
                syntax::floating(sp), inner));
        }
        case 'n' : // nan or null-extension
        {
            if(sp.ext_null_value)
            {
                if(literal("nan").scan(inner).is_ok())
                {
                    return ok(value_t::floating);
                }
                else if(literal("null").scan(inner).is_ok())
                {
                    return ok(value_t::empty);
                }
                else
                {
                    return err(make_syntax_error("toml::parse_value: "
                        "Both `nan` and `null` must be in lowercase. "
                        "A string must be surrounded by quotes.",
                        syntax::floating(sp), inner));
                }
            }
            else // must be nan.
            {
                if(literal("nan").scan(inner).is_ok())
                {
                    return ok(value_t::floating);
                }
                else
                {
                    return err(make_syntax_error("toml::parse_value: "
                        "`nan` must be in lowercase. "
                        "A string must be surrounded by quotes.",
                        syntax::floating(sp), inner));
                }
            }
        }
        case 'N' : // nan or null-extension
        {
            if(sp.ext_null_value)
            {
                return err(make_syntax_error("toml::parse_value: "
                    "Both `nan` and `null` must be in lowercase. "
                    "A string must be surrounded by quotes.",
                    syntax::floating(sp), inner));
            }
            else
            {
                return err(make_syntax_error("toml::parse_value: "
                    "`nan` must be in lowercase. "
                    "A string must be surrounded by quotes.",
                    syntax::floating(sp), inner));
            }
        }
        default  :
        {
            return guess_number_type(loc, ctx);
        }
    }
}

template<typename TC>
result<basic_value<TC>, error_info>
parse_value(location& loc, context<TC>& ctx)
{
    const auto ty_res = guess_value_type(loc, ctx);
    if(ty_res.is_err())
    {
        return err(ty_res.unwrap_err());
    }

    switch(ty_res.unwrap())
    {
        case value_t::empty:
        {
            if(ctx.toml_spec().ext_null_value)
            {
                return parse_null(loc, ctx);
            }
            else
            {
                auto src = source_location(region(loc));
                return err(make_error_info("toml::parse_value: unknown value appeared",
                            std::move(src), "here"));
            }
        }
        case value_t::boolean        : {return parse_boolean        (loc, ctx);}
        case value_t::integer        : {return parse_integer        (loc, ctx);}
        case value_t::floating       : {return parse_floating       (loc, ctx);}
        case value_t::string         : {return parse_string         (loc, ctx);}
        case value_t::offset_datetime: {return parse_offset_datetime(loc, ctx);}
        case value_t::local_datetime : {return parse_local_datetime (loc, ctx);}
        case value_t::local_date     : {return parse_local_date     (loc, ctx);}
        case value_t::local_time     : {return parse_local_time     (loc, ctx);}
        case value_t::array          : {return parse_array          (loc, ctx);}
        case value_t::table          : {return parse_inline_table   (loc, ctx);}
        default:
        {
            auto src = source_location(region(loc));
            return err(make_error_info("toml::parse_value: unknown value appeared",
                        std::move(src), "here"));
        }
    }
}

/* ============================================================================
 *  _____     _    _
 * |_   _|_ _| |__| |___
 *   | |/ _` | '_ \ / -_)
 *   |_|\__,_|_.__/_\___|
 */

template<typename TC>
result<std::pair<std::vector<typename basic_value<TC>::key_type>, region>, error_info>
parse_table_key(location& loc, context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::std_table(spec).scan(loc);
    if(!reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_table_key: invalid table key",
            syntax::std_table(spec), loc));
    }

    loc = first;
    loc.advance(); // skip [
    skip_whitespace(loc, ctx);

    auto keys_res = parse_key(loc, ctx);
    if(keys_res.is_err())
    {
        return err(std::move(keys_res.unwrap_err()));
    }

    skip_whitespace(loc, ctx);
    loc.advance(); // ]

    return ok(std::make_pair(std::move(keys_res.unwrap().first), std::move(reg)));
}

template<typename TC>
result<std::pair<std::vector<typename basic_value<TC>::key_type>, region>, error_info>
parse_array_table_key(location& loc, context<TC>& ctx)
{
    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    auto reg = syntax::array_table(spec).scan(loc);
    if(!reg.is_ok())
    {
        return err(make_syntax_error("toml::parse_array_table_key: invalid array-of-tables key",
            syntax::array_table(spec), loc));
    }

    loc = first;
    loc.advance(); // [
    loc.advance(); // [
    skip_whitespace(loc, ctx);

    auto keys_res = parse_key(loc, ctx);
    if(keys_res.is_err())
    {
        return err(std::move(keys_res.unwrap_err()));
    }

    skip_whitespace(loc, ctx);
    loc.advance(); // ]
    loc.advance(); // ]

    return ok(std::make_pair(std::move(keys_res.unwrap().first), std::move(reg)));
}

// called after reading [table.keys] and comments around it.
// Since table may already contain a subtable ([x.y.z] can be defined before [x]),
// the table that is being parsed is passed as an argument.
template<typename TC>
result<none_t, error_info>
parse_table(location& loc, context<TC>& ctx, basic_value<TC>& table)
{
    assert(table.is_table());

    const auto num_errors = ctx.errors().size();
    const auto& spec = ctx.toml_spec();

    // clear indent info
    table.as_table_fmt().indent_type = indent_char::none;

    bool newline_found = true;
    while( ! loc.eof())
    {
        const auto start = loc;

        auto sp = skip_multiline_spacer(loc, ctx, newline_found);

        // if reached to EOF, the table ends here. return.
        if(loc.eof())
        {
            break;
        }
        // if next table is comming, return.
        if(sequence(syntax::ws(spec), character('[')).scan(loc).is_ok())
        {
            loc = start;
            break;
        }
        // otherwise, it should be a key-value pair.
        newline_found = newline_found || (sp.has_value() && sp.value().newline_found);
        if( ! newline_found)
        {
            return err(make_error_info("toml::parse_table: "
                "newline (LF / CRLF) or EOF is expected",
                source_location(region(loc)), "here"));
        }
        if(sp.has_value() && sp.value().indent_type != indent_char::none)
        {
            table.as_table_fmt().indent_type = sp.value().indent_type;
            table.as_table_fmt().body_indent = sp.value().indent;
        }

        newline_found = false; // reset
        if(auto kv_res = parse_key_value_pair(loc, ctx))
        {
            auto keys    = std::move(kv_res.unwrap().first.first);
            auto key_reg = std::move(kv_res.unwrap().first.second);
            auto val     = std::move(kv_res.unwrap().second);

            if(sp.has_value())
            {
                for(const auto& com : sp.value().comments)
                {
                    val.comments().push_back(com);
                }
            }

            if(auto com_res = parse_comment_line(loc, ctx))
            {
                if(auto com_opt = com_res.unwrap())
                {
                    val.comments().push_back(com_opt.value());
                    newline_found = true; // comment includes newline at the end
                }
            }
            else
            {
                ctx.report_error(std::move(com_res.unwrap_err()));
            }

            auto ins_res = insert_value(inserting_value_kind::dotted_keys,
                    std::addressof(table.as_table()),
                    keys, std::move(key_reg), std::move(val));
            if(ins_res.is_err())
            {
                ctx.report_error(std::move(ins_res.unwrap_err()));
            }
        }
        else
        {
            ctx.report_error(std::move(kv_res.unwrap_err()));
            skip_key_value_pair(loc, ctx);
        }
    }

    if(num_errors < ctx.errors().size())
    {
        assert(ctx.has_error()); // already reported
        return err(ctx.pop_last_error());
    }
    return ok();
}

template<typename TC>
result<basic_value<TC>, std::vector<error_info>>
parse_file(location& loc, context<TC>& ctx)
{
    using value_type = basic_value<TC>;
    using table_type = typename value_type::table_type;

    const auto first = loc;
    const auto& spec = ctx.toml_spec();

    if(loc.eof())
    {
        return ok(value_type(table_type(), table_format_info{}, {}, region(loc)));
    }

    value_type root(table_type(), table_format_info{}, {}, region(loc));
    root.as_table_fmt().fmt = table_format::multiline;
    root.as_table_fmt().indent_type = indent_char::none;

    // parse top comment.
    //
    // ```toml
    // # this is a comment for the top-level table.
    //
    // key = "the first value"
    // ```
    //
    // ```toml
    // # this is a comment for "the first value".
    // key = "the first value"
    // ```
    while( ! loc.eof())
    {
        if(auto com_res = parse_comment_line(loc, ctx))
        {
            if(auto com_opt = com_res.unwrap())
            {
                root.comments().push_back(std::move(com_opt.value()));
            }
            else // no comment found.
            {
                // if it is not an empty line, clear the root comment.
                if( ! sequence(syntax::ws(spec), syntax::newline(spec)).scan(loc).is_ok())
                {
                    loc = first;
                    root.comments().clear();
                }
                break;
            }
        }
        else
        {
            ctx.report_error(std::move(com_res.unwrap_err()));
            skip_comment_block(loc, ctx);
        }
    }

    // parse root table
    {
        const auto res = parse_table(loc, ctx, root);
        if(res.is_err())
        {
            ctx.report_error(std::move(res.unwrap_err()));
            skip_until_next_table(loc, ctx);
        }
    }

    // parse tables

    while( ! loc.eof())
    {
        auto sp = skip_multiline_spacer(loc, ctx, /*newline_found=*/true);

        if(auto key_res = parse_array_table_key(loc, ctx))
        {
            auto key = std::move(std::get<0>(key_res.unwrap()));
            auto reg = std::move(std::get<1>(key_res.unwrap()));

            std::vector<std::string> com;
            if(sp.has_value())
            {
                for(std::size_t i=0; i<sp.value().comments.size(); ++i)
                {
                    com.push_back(std::move(sp.value().comments.at(i)));
                }
            }

            // [table.def] must be followed by one of
            // - a comment line
            // - whitespace + newline
            // - EOF
            if(auto com_res = parse_comment_line(loc, ctx))
            {
                if(auto com_opt = com_res.unwrap())
                {
                    com.push_back(com_opt.value());
                }
                else // if there is no comment, ws+newline must exist (or EOF)
                {
                    skip_whitespace(loc, ctx);
                    if( ! loc.eof() && ! syntax::newline(ctx.toml_spec()).scan(loc).is_ok())
                    {
                        ctx.report_error(make_syntax_error("toml::parse_file: "
                            "newline (or EOF) expected",
                            syntax::newline(ctx.toml_spec()), loc));
                        skip_until_next_table(loc, ctx);
                        continue;
                    }
                }
            }
            else // comment syntax error (rare)
            {
                ctx.report_error(com_res.unwrap_err());
                skip_until_next_table(loc, ctx);
                continue;
            }

            table_format_info fmt;
            fmt.fmt = table_format::multiline;
            fmt.indent_type = indent_char::none;
            auto tab = value_type(table_type{}, std::move(fmt), std::move(com), reg);

            auto inserted = insert_value(inserting_value_kind::array_table,
                std::addressof(root.as_table()),
                key, std::move(reg), std::move(tab));

            if(inserted.is_err())
            {
                ctx.report_error(inserted.unwrap_err());

                // check errors in the table
                auto tmp = basic_value<TC>(table_type());
                auto res = parse_table(loc, ctx, tmp);
                if(res.is_err())
                {
                    ctx.report_error(res.unwrap_err());
                    skip_until_next_table(loc, ctx);
                }
                continue;
            }

            auto tab_ptr = inserted.unwrap();
            assert(tab_ptr);

            const auto tab_res = parse_table(loc, ctx, *tab_ptr);
            if(tab_res.is_err())
            {
                ctx.report_error(tab_res.unwrap_err());
                skip_until_next_table(loc, ctx);
            }

            // parse_table first clears `indent_type`.
            // to keep header indent info, we must store it later.
            if(sp.has_value() && sp.value().indent_type != indent_char::none)
            {
                tab_ptr->as_table_fmt().indent_type = sp.value().indent_type;
                tab_ptr->as_table_fmt().name_indent = sp.value().indent;
            }
            continue;
        }
        if(auto key_res = parse_table_key(loc, ctx))
        {
            auto key = std::move(std::get<0>(key_res.unwrap()));
            auto reg = std::move(std::get<1>(key_res.unwrap()));

            std::vector<std::string> com;
            if(sp.has_value())
            {
                for(std::size_t i=0; i<sp.value().comments.size(); ++i)
                {
                    com.push_back(std::move(sp.value().comments.at(i)));
                }
            }

            // [table.def] must be followed by one of
            // - a comment line
            // - whitespace + newline
            // - EOF
            if(auto com_res = parse_comment_line(loc, ctx))
            {
                if(auto com_opt = com_res.unwrap())
                {
                    com.push_back(com_opt.value());
                }
                else // if there is no comment, ws+newline must exist (or EOF)
                {
                    skip_whitespace(loc, ctx);
                    if( ! loc.eof() && ! syntax::newline(ctx.toml_spec()).scan(loc).is_ok())
                    {
                        ctx.report_error(make_syntax_error("toml::parse_file: "
                            "newline (or EOF) expected",
                            syntax::newline(ctx.toml_spec()), loc));
                        skip_until_next_table(loc, ctx);
                        continue;
                    }
                }
            }
            else // comment syntax error (rare)
            {
                ctx.report_error(com_res.unwrap_err());
                skip_until_next_table(loc, ctx);
                continue;
            }

            table_format_info fmt;
            fmt.fmt = table_format::multiline;
            fmt.indent_type = indent_char::none;
            auto tab = value_type(table_type{}, std::move(fmt), std::move(com), reg);

            auto inserted = insert_value(inserting_value_kind::std_table,
                std::addressof(root.as_table()),
                key, std::move(reg), std::move(tab));

            if(inserted.is_err())
            {
                ctx.report_error(inserted.unwrap_err());

                // check errors in the table
                auto tmp = basic_value<TC>(table_type());
                auto res = parse_table(loc, ctx, tmp);
                if(res.is_err())
                {
                    ctx.report_error(res.unwrap_err());
                    skip_until_next_table(loc, ctx);
                }
                continue;
            }

            auto tab_ptr = inserted.unwrap();
            assert(tab_ptr);

            const auto tab_res = parse_table(loc, ctx, *tab_ptr);
            if(tab_res.is_err())
            {
                ctx.report_error(tab_res.unwrap_err());
                skip_until_next_table(loc, ctx);
            }
            if(sp.has_value() && sp.value().indent_type != indent_char::none)
            {
                tab_ptr->as_table_fmt().indent_type = sp.value().indent_type;
                tab_ptr->as_table_fmt().name_indent = sp.value().indent;
            }
            continue;
        }

        // does not match array_table nor std_table. report an error.
        const auto keytop = loc;
        const auto maybe_array_of_tables = literal("[[").scan(loc).is_ok();
        loc = keytop;

        if(maybe_array_of_tables)
        {
            ctx.report_error(make_syntax_error("toml::parse_file: invalid array-table key",
                syntax::array_table(spec), loc));
        }
        else
        {
            ctx.report_error(make_syntax_error("toml::parse_file: invalid table key",
                syntax::std_table(spec), loc));
        }
        skip_until_next_table(loc, ctx);
    }

    if( ! ctx.errors().empty())
    {
        return err(std::move(ctx.errors()));
    }

#ifdef TOML11_ENABLE_ACCESS_CHECK
    detail::unset_access_flag_recursively(root);
#endif

    return ok(std::move(root));
}

template<typename TC>
result<basic_value<TC>, std::vector<error_info>>
parse_impl(std::vector<location::char_type> cs, std::string fname, const spec& s)
{
    using value_type = basic_value<TC>;
    using table_type = typename value_type::table_type;

    // an empty file is a valid toml file.
    if(cs.empty())
    {
        auto src = std::make_shared<std::vector<location::char_type>>(std::move(cs));
        location loc(std::move(src), std::move(fname));
        return ok(value_type(table_type(), table_format_info{}, std::vector<std::string>{}, region(loc)));
    }

    // to simplify parser, add newline at the end if there is no LF.
    // But, if it has raw CR, the file is invalid (in TOML, CR is not a valid
    // newline char). if it ends with CR, do not add LF and report it.
    if(cs.back() != '\n' && cs.back() != '\r')
    {
        cs.push_back('\n');
    }

    auto src = std::make_shared<std::vector<location::char_type>>(std::move(cs));

    location loc(std::move(src), std::move(fname));

    // skip BOM if found
    if(loc.source()->size() >= 3)
    {
        auto first = loc;

        const auto c0 = loc.current(); loc.advance();
        const auto c1 = loc.current(); loc.advance();
        const auto c2 = loc.current(); loc.advance();

        const auto bom_found = (c0 == 0xEF) && (c1 == 0xBB) && (c2 == 0xBF);
        if( ! bom_found)
        {
            loc = first;
        }
    }

    context<TC> ctx(s);

    return parse_file(loc, ctx);
}

} // detail

// -----------------------------------------------------------------------------
// parse(byte array)

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(std::vector<unsigned char> content, std::string filename,
          spec s = spec::default_version())
{
    return detail::parse_impl<TC>(std::move(content), std::move(filename), std::move(s));
}
template<typename TC = type_config>
basic_value<TC>
parse(std::vector<unsigned char> content, std::string filename,
      spec s = spec::default_version())
{
    auto res = try_parse<TC>(std::move(content), std::move(filename), std::move(s));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

// -----------------------------------------------------------------------------
// parse(istream)

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(std::istream& is, std::string fname = "unknown file", spec s = spec::default_version())
{
    const auto beg = is.tellg();
    is.seekg(0, std::ios::end);
    const auto end = is.tellg();
    const auto fsize = end - beg;
    is.seekg(beg);

    // read whole file as a sequence of char
    assert(fsize >= 0);
    std::vector<detail::location::char_type> letters(static_cast<std::size_t>(fsize), '\0');
    is.read(reinterpret_cast<char*>(letters.data()), static_cast<std::streamsize>(fsize));

    return detail::parse_impl<TC>(std::move(letters), std::move(fname), std::move(s));
}

template<typename TC = type_config>
basic_value<TC> parse(std::istream& is, std::string fname = "unknown file", spec s = spec::default_version())
{
    auto res = try_parse<TC>(is, std::move(fname), std::move(s));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

// -----------------------------------------------------------------------------
// parse(filename)

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(std::string fname, spec s = spec::default_version())
{
    std::ifstream ifs(fname, std::ios_base::binary);
    if(!ifs.good())
    {
        std::vector<error_info> e;
        e.push_back(error_info("toml::parse: Error opening file \"" + fname + "\"", {}));
        return err(std::move(e));
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return try_parse<TC>(ifs, std::move(fname), std::move(s));
}

template<typename TC = type_config>
basic_value<TC> parse(std::string fname, spec s = spec::default_version())
{
    std::ifstream ifs(fname, std::ios_base::binary);
    if(!ifs.good())
    {
        throw file_io_error("toml::parse: error opening file", fname);
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return parse<TC>(ifs, std::move(fname), std::move(s));
}

template<typename TC = type_config, std::size_t N>
result<basic_value<TC>, std::vector<error_info>>
try_parse(const char (&fname)[N], spec s = spec::default_version())
{
    return try_parse<TC>(std::string(fname), std::move(s));
}

template<typename TC = type_config, std::size_t N>
basic_value<TC> parse(const char (&fname)[N], spec s = spec::default_version())
{
    return parse<TC>(std::string(fname), std::move(s));
}

// ----------------------------------------------------------------------------
// parse_str

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse_str(std::string content, spec s = spec::default_version(),
              cxx::source_location loc = cxx::source_location::current())
{
    std::istringstream iss(std::move(content));
    std::string name("internal string" + cxx::to_string(loc));
    return try_parse<TC>(iss, std::move(name), std::move(s));
}

template<typename TC = type_config>
basic_value<TC> parse_str(std::string content, spec s = spec::default_version(),
        cxx::source_location loc = cxx::source_location::current())
{
    auto res = try_parse_str<TC>(std::move(content), std::move(s), std::move(loc));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

// ----------------------------------------------------------------------------
// filesystem

#if defined(TOML11_HAS_FILESYSTEM)

template<typename TC = type_config, typename FSPATH>
cxx::enable_if_t<std::is_same<FSPATH, std::filesystem::path>::value,
    result<basic_value<TC>, std::vector<error_info>>>
try_parse(const FSPATH& fpath, spec s = spec::default_version())
{
    std::ifstream ifs(fpath, std::ios_base::binary);
    if(!ifs.good())
    {
        std::vector<error_info> e;
        e.push_back(error_info("toml::parse: Error opening file \"" + fpath.string() + "\"", {}));
        return err(std::move(e));
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return try_parse<TC>(ifs, fpath.string(), std::move(s));
}

template<typename TC = type_config, typename FSPATH>
cxx::enable_if_t<std::is_same<FSPATH, std::filesystem::path>::value,
    basic_value<TC>>
parse(const FSPATH& fpath, spec s = spec::default_version())
{
    std::ifstream ifs(fpath, std::ios_base::binary);
    if(!ifs.good())
    {
        throw file_io_error("toml::parse: error opening file", fpath.string());
    }
    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    return parse<TC>(ifs, fpath.string(), std::move(s));
}
#endif

// -----------------------------------------------------------------------------
// FILE*

template<typename TC = type_config>
result<basic_value<TC>, std::vector<error_info>>
try_parse(FILE* fp, std::string filename, spec s = spec::default_version())
{
    const long beg = std::ftell(fp);
    if (beg == -1L)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to access: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});
    }

    const int res_seekend = std::fseek(fp, 0, SEEK_END);
    if (res_seekend != 0)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to seek: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});
    }

    const long end = std::ftell(fp);
    if (end == -1L)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to access: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});
    }

    const auto fsize = end - beg;

    const auto res_seekbeg = std::fseek(fp, beg, SEEK_SET);
    if (res_seekbeg != 0)
    {
        return err(std::vector<error_info>{error_info(
            std::string("Failed to seek: \"") + filename +
            "\", errno = " + std::to_string(errno), {}
        )});

    }

    // read whole file as a sequence of char
    assert(fsize >= 0);
    std::vector<detail::location::char_type> letters(static_cast<std::size_t>(fsize));
    const auto actual = std::fread(letters.data(), sizeof(char), static_cast<std::size_t>(fsize), fp);
    if(actual != static_cast<std::size_t>(fsize))
    {
        return err(std::vector<error_info>{error_info(
            std::string("File size changed: \"") + filename +
            std::string("\" make sure that FILE* is in binary mode "
                        "to avoid LF <-> CRLF conversion"), {}
        )});
    }

    return detail::parse_impl<TC>(std::move(letters), std::move(filename), std::move(s));
}

template<typename TC = type_config>
basic_value<TC>
parse(FILE* fp, std::string filename, spec s = spec::default_version())
{
    const long beg = std::ftell(fp);
    if (beg == -1L)
    {
        throw file_io_error(errno, "Failed to access", filename);
    }

    const int res_seekend = std::fseek(fp, 0, SEEK_END);
    if (res_seekend != 0)
    {
        throw file_io_error(errno, "Failed to seek", filename);
    }

    const long end = std::ftell(fp);
    if (end == -1L)
    {
        throw file_io_error(errno, "Failed to access", filename);
    }

    const auto fsize = end - beg;

    const auto res_seekbeg = std::fseek(fp, beg, SEEK_SET);
    if (res_seekbeg != 0)
    {
        throw file_io_error(errno, "Failed to seek", filename);
    }

    // read whole file as a sequence of char
    assert(fsize >= 0);
    std::vector<detail::location::char_type> letters(static_cast<std::size_t>(fsize));
    const auto actual = std::fread(letters.data(), sizeof(char), static_cast<std::size_t>(fsize), fp);
    if(actual != static_cast<std::size_t>(fsize))
    {
        throw file_io_error(errno, "File size changed; make sure that "
            "FILE* is in binary mode to avoid LF <-> CRLF conversion", filename);
    }

    auto res = detail::parse_impl<TC>(std::move(letters), std::move(filename), std::move(s));
    if(res.is_ok())
    {
        return res.unwrap();
    }
    else
    {
        std::string msg;
        for(const auto& err : res.unwrap_err())
        {
            msg += format_error(err);
        }
        throw syntax_error(std::move(msg), std::move(res.unwrap_err()));
    }
}

} // TOML11_INLINE_VERSION_NAMESPACE
} // namespace toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{
struct type_config;
struct ordered_type_config;

extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(std::vector<unsigned char>, std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(std::istream&, std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse<type_config>(FILE*, std::string, spec);
extern template result<basic_value<type_config>, std::vector<error_info>> try_parse_str<type_config>(std::string, spec, cxx::source_location);

extern template basic_value<type_config> parse<type_config>(std::vector<unsigned char>, std::string, spec);
extern template basic_value<type_config> parse<type_config>(std::istream&, std::string, spec);
extern template basic_value<type_config> parse<type_config>(std::string, spec);
extern template basic_value<type_config> parse<type_config>(FILE*, std::string, spec);
extern template basic_value<type_config> parse_str<type_config>(std::string, spec, cxx::source_location);

extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(std::vector<unsigned char>, std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(std::istream&, std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse<ordered_type_config>(FILE*, std::string, spec);
extern template result<basic_value<ordered_type_config>, std::vector<error_info>> try_parse_str<ordered_type_config>(std::string, spec, cxx::source_location);

extern template basic_value<ordered_type_config> parse<ordered_type_config>(std::vector<unsigned char>, std::string, spec);
extern template basic_value<ordered_type_config> parse<ordered_type_config>(std::istream&, std::string, spec);
extern template basic_value<ordered_type_config> parse<ordered_type_config>(std::string, spec);
extern template basic_value<ordered_type_config> parse<ordered_type_config>(FILE*, std::string, spec);
extern template basic_value<ordered_type_config> parse_str<ordered_type_config>(std::string, spec, cxx::source_location);

#if defined(TOML11_HAS_FILESYSTEM)
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, result<basic_value<type_config>,         std::vector<error_info>>> try_parse<type_config,         std::filesystem::path>(const std::filesystem::path&, spec);
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, result<basic_value<ordered_type_config>, std::vector<error_info>>> try_parse<ordered_type_config, std::filesystem::path>(const std::filesystem::path&, spec);
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, basic_value<type_config>                                         > parse    <type_config,         std::filesystem::path>(const std::filesystem::path&, spec);
extern template cxx::enable_if_t<std::is_same<std::filesystem::path, std::filesystem::path>::value, basic_value<ordered_type_config>                                 > parse    <ordered_type_config, std::filesystem::path>(const std::filesystem::path&, spec);
#endif // filesystem

} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_PARSER_HPP
#ifndef TOML11_LITERAL_HPP
#define TOML11_LITERAL_HPP

#ifndef TOML11_LITERAL_FWD_HPP
#define TOML11_LITERAL_FWD_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

namespace detail
{
// implementation
::toml::value literal_internal_impl(location loc);
} // detail

inline namespace literals
{
inline namespace toml_literals
{

::toml::value operator""_toml(const char* str, std::size_t len);

#if defined(TOML11_HAS_CHAR8_T)
// value of u8"" literal has been changed from char to char8_t and char8_t is
// NOT compatible to char
::toml::value operator"" _toml(const char8_t* str, std::size_t len);
#endif

} // toml_literals
} // literals
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_LITERAL_FWD_HPP

#if ! defined(TOML11_COMPILE_SOURCES)
#ifndef TOML11_LITERAL_IMPL_HPP
#define TOML11_LITERAL_IMPL_HPP


namespace toml
{
inline namespace TOML11_INLINE_VERSION_NAMESPACE
{

namespace detail
{
// implementation
TOML11_INLINE ::toml::value literal_internal_impl(location loc)
{
    const auto s = ::toml::spec::default_version();
    context<type_config> ctx(s);

    const auto front = loc;

    // ------------------------------------------------------------------------
    // check if it is a raw value.

    // skip empty lines and comment lines
    auto sp = skip_multiline_spacer(loc, ctx);
    if(loc.eof())
    {
        ::toml::value val;
        if(sp.has_value())
        {
            for(std::size_t i=0; i<sp.value().comments.size(); ++i)
            {
                val.comments().push_back(std::move(sp.value().comments.at(i)));
            }
        }
        return val;
    }

    // to distinguish arrays and tables, first check it is a table or not.
    //
    // "[1,2,3]"_toml;   // json: [1, 2, 3]
    // "[table]"_toml;   // json: {"table": {}}
    // "[[1,2,3]]"_toml; // json: [[1, 2, 3]]
    // "[[table]]"_toml; // json: {"table": [{}]}
    //
    // "[[1]]"_toml;     // json: {"1": [{}]}
    // "1 = [{}]"_toml;  // json: {"1": [{}]}
    // "[[1,]]"_toml;    // json: [[1]]
    // "[[1],]"_toml;    // json: [[1]]
    const auto val_start = loc;

    const bool is_table_key = syntax::std_table(s).scan(loc).is_ok();
    loc = val_start;
    const bool is_aots_key  = syntax::array_table(s).scan(loc).is_ok();
    loc = val_start;

    // If it is neither a table-key or a array-of-table-key, it may be a value.
    if(!is_table_key && !is_aots_key)
    {
        auto data = parse_value(loc, ctx);
        if(data.is_ok())
        {
            auto val = std::move(data.unwrap());
            if(sp.has_value())
            {
                for(std::size_t i=0; i<sp.value().comments.size(); ++i)
                {
                    val.comments().push_back(std::move(sp.value().comments.at(i)));
                }
            }
            auto com_res = parse_comment_line(loc, ctx);
            if(com_res.is_ok() && com_res.unwrap().has_value())
            {
                val.comments().push_back(com_res.unwrap().value());
            }
            return val;
        }
    }

    // -------------------------------------------------------------------------
    // Note that still it can be a table, because the literal might be something
    // like the following.
    // ```cpp
    // // c++11 raw-string literal
    // const auto val = R"(
    //   key = "value"
    //   int = 42
    // )"_toml;
    // ```
    // It is a valid toml file.
    // It should be parsed as if we parse a file with this content.

    loc = front;
    auto data = parse_file(loc, ctx);
    if(data.is_ok())
    {
        return data.unwrap();
    }
    else // not a value && not a file. error.
    {
        std::string msg;
        for(const auto& err : data.unwrap_err())
        {
            msg += format_error(err);
        }
        throw ::toml::syntax_error(std::move(msg), std::move(data.unwrap_err()));
    }
}

} // detail

inline namespace literals
{
inline namespace toml_literals
{

TOML11_INLINE ::toml::value
operator""_toml(const char* str, std::size_t len)
{
    if(len == 0)
    {
        return ::toml::value{};
    }

    ::toml::detail::location::container_type c(len);
    std::copy(reinterpret_cast<const ::toml::detail::location::char_type*>(str),
              reinterpret_cast<const ::toml::detail::location::char_type*>(str + len),
              c.begin());
    if( ! c.empty() && c.back())
    {
        c.push_back('\n'); // to make it easy to parse comment, we add newline
    }

    return literal_internal_impl(::toml::detail::location(
        std::make_shared<const toml::detail::location::container_type>(std::move(c)),
        "TOML literal encoded in a C++ code"));
}

#if defined(__cpp_char8_t)
#  if __cpp_char8_t >= 201811L
#    define TOML11_HAS_CHAR8_T 1
#  endif
#endif

#if defined(TOML11_HAS_CHAR8_T)
// value of u8"" literal has been changed from char to char8_t and char8_t is
// NOT compatible to char
TOML11_INLINE ::toml::value
operator"" _toml(const char8_t* str, std::size_t len)
{
    if(len == 0)
    {
        return ::toml::value{};
    }

    ::toml::detail::location::container_type c(len);
    std::copy(reinterpret_cast<const ::toml::detail::location::char_type*>(str),
              reinterpret_cast<const ::toml::detail::location::char_type*>(str + len),
              c.begin());
    if( ! c.empty() && c.back())
    {
        c.push_back('\n'); // to make it easy to parse comment, we add newline
    }

    return literal_internal_impl(::toml::detail::location(
        std::make_shared<const toml::detail::location::container_type>(std::move(c)),
        "TOML literal encoded in a C++ code"));
}
#endif

} // toml_literals
} // literals
} // TOML11_INLINE_VERSION_NAMESPACE
} // toml
#endif // TOML11_LITERAL_IMPL_HPP
#endif

#endif // TOML11_LITERAL_HPP
#ifndef TOML11_TOML_HPP
#define TOML11_TOML_HPP

// The MIT License (MIT)
//
// Copyright (c) 2017-now Toru Niina
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// IWYU pragma: begin_exports
// IWYU pragma: end_exports

#endif// TOML11_TOML_HPP
