// Nonius - C++ benchmarking tool
//
// Written in 2014 by Martinho Fernandes <martinho.fernandes@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>

// User-facing param

#ifndef NONIUS_PARAM_HPP
#define NONIUS_PARAM_HPP

#include <nonius/detail/meta.h++>
#include <nonius/detail/noexcept.h++>
#include <boost/lexical_cast.hpp>
#include <unordered_map>

namespace nonius {

struct param_bad_operation : std::exception {
    char const* what() const NONIUS_NOEXCEPT override {
        return "operation not supported for this parameter type";
    }
};

struct param_spec_base {
    virtual std::string plus(std::string const&, std::string const&) const = 0;
    virtual std::string times(std::string const&, std::string const&) const = 0;
    virtual bool check(std::string const&) const = 0;
    virtual std::string default_value() const = 0;
};

namespace detail {

struct plus_fn {
    template <typename T>
    auto operator() (T x, T y) -> decltype(x + y) { return x + y; }
};

struct times_fn {
    template <typename T>
    auto operator() (T x, T y) -> decltype(x * y) { return x * y; }
};

} // namespace detail

// Users may partially specialize this to define custom types.  They
// can more simply define isomorphic << and >>
template <typename T>
struct param_spec : param_spec_base {
    T value;

    param_spec(T v) : value(std::move(v)) {}

    std::string default_value() const override {
        return boost::lexical_cast<std::string>(value);
    }

    std::string plus(std::string const& x, std::string const& y) const override {
        return operate(detail::plus_fn{}, x, y);
    }

    std::string times(std::string const& x, std::string const& y) const override {
        return operate(detail::times_fn{}, x, y);
    }

    bool check(std::string const& x) const {
        try {
            boost::lexical_cast<T>(x);
            return true;
        } catch (boost::bad_lexical_cast const&) {
            return false;
        }
    }

    template <typename Op, typename ...Args>
    std::string operate(Op&& op, Args&& ...xs) const {
        return boost::lexical_cast<std::string>(
            do_operate(std::forward<Op>(op),
                       boost::lexical_cast<T>(std::forward<Args>(xs))...));
    }

    template <typename Op, typename ...Args>
    auto do_operate(Op&& op, Args&& ...xs) const
        -> typename std::enable_if<detail::is_callable<Op&&(Args&&...)>::value, T>::type {
        return std::forward<Op>(op)(std::forward<Args>(xs)...);
    }

    template <typename Op, typename ...Args>
    auto do_operate(Op&&, Args&&...) const
        -> typename std::enable_if<!detail::is_callable<Op&&(Args&&...)>::value, T>::type {
        throw param_bad_operation{};
    }
};

namespace detail {

struct param_registry {
    using spec_map = std::unordered_map<
        std::string,
        std::reference_wrapper<const param_spec_base>>;

    spec_map specs;

    param_map defaults() {
        auto r = param_map{};
        for (auto&& s : specs)
            r[s.first] = s.second.get().default_value();
        return r;
    }
};

inline param_registry& global_param_registry() {
    static param_registry instance;
    return instance;
}

template <typename T>
struct param_declaration {
    param_spec<T> spec;

    param_declaration(std::string p, T v, param_registry& registry = global_param_registry())
        : spec{std::move(v)}
    {
        registry.specs.emplace(std::move(p), spec);
    }
};

} /* namespace detail */
} /* namespace nonius */

#define NONIUS_PARAM(name, default_value)                               \
    namespace {                                                         \
    static auto NONIUS_DETAIL_UNIQUE_NAME(param_declaration) =          \
        ::nonius::detail::param_declaration<decltype(default_value)>{ name, default_value }; \
    }                                                                   \
    //

#endif // NONIUS_PARAM_HPP
