#ifndef _LIBRW_OPTIONAL_HPP
#define _LIBRW_OPTIONAL_HPP

#if defined(__cpp_lib_optional)
#include <optional>
namespace rwopt {
    template <class T>
    using optional = std::optional<T>;
}
#elif defined(__cpp_lib_experimental_optional)
#include <experimental/optional.hpp>
namespace rwopt {
    template <class T>
    using optional = std::experimental::optional<T>;
}
#else
#include <boost/optional.hpp>
namespace rwopt {
    template <class T>
    using optional = boost::optional<T>;
}
#endif

#endif // OPTIONAL_HPP
