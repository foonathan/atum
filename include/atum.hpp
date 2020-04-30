// Copyright (C) 2020 Jonathan Müller <jonathanmueller.dev@gmail.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at https://www.boost.org/LICENSE_1_0.txt).

#ifndef ATUM_HPP_INCLUDED
#define ATUM_HPP_INCLUDED

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

//=== checking ===//
#ifndef ATUM_CHECK_LIFETIME
#    define ATUM_CHECK_LIFETIME 0
#endif

#if ATUM_CHECK_LIFETIME

#    ifndef ATUM_LIFETIME_ASSERT
#        include <cassert>
#        define ATUM_LIFETIME_ASSERT(Cond) assert(Cond)
#    endif

#    define ATUM_VERSION_NS v0_checked

#else

#    undef ATUM_LIFETIME_ASSERT
#    define ATUM_LIFETIME_ASSERT(Cond)

#    define ATUM_VERSION_NS v0

#endif

//=== constinit ===//
#ifndef ATUM_HAS_CONSTINIT
#    if __cpp_constinit
#        define ATUM_HAS_CONSTINIT 1
#    else
#        define ATUM_HAS_CONSTINIT 0
#    endif
#endif

#if ATUM_HAS_CONSTINIT
#    define ATUM_CONSTINIT constinit
#else
#    define ATUM_CONSTINIT
#endif

//=== storage ===//
namespace atum
{
inline namespace ATUM_VERSION_NS
{
    template <typename T>
    class storage
    {
    public:
        using element_type = T;

        constexpr storage() noexcept : _dummy{} {}

        storage(const storage&) = delete;
        storage& operator=(const storage&) = delete;

        ~storage() noexcept
        {
            /* leak */
        }

        constexpr T& get() noexcept
        {
            return _obj;
        }

        void* memory() noexcept
        {
            return &_obj;
        }

    private:
        union
        {
            char _dummy;
            T    _obj;
        };
    };
} // namespace ATUM_VERSION_NS
} // namespace atum

//=== initializer policy ===//
namespace atum
{
inline namespace ATUM_VERSION_NS
{
    struct init_default
    {
        template <typename T>
        static T* init(void* memory)
        {
            // We're doing default initialization, not value initialization.
            // Globals are zero initialized anyway, so there is no difference.
            return ::new (memory) T;
        }
    };

    template <auto... Args>
    struct init_from
    {
        template <typename T>
        static T* init(void* memory)
        {
            return ::new (memory) T(Args...);
        }
    };

    template <auto... Args>
    struct init_braced
    {
        template <typename T>
        static T* init(void* memory)
        {
            return ::new (memory) T{Args...};
        }
    };

    template <auto FunctionPtr>
    struct init_fn
    {
        template <typename T>
        static T* init(void* memory)
        {
            return ::new (memory) T(FunctionPtr());
        }
    };
} // namespace ATUM_VERSION_NS
} // namespace atum

//=== scoped_initializer===//
namespace atum
{
inline namespace ATUM_VERSION_NS
{
    template <auto&... Inits>
    class scoped_initializer
    {
    public:
        scoped_initializer()
        {
            // Evaluated left-to-right.
            (Inits.initialize(), ...);
        }

        ~scoped_initializer() noexcept
        {
            int dummy;
            // Evaluated right-to-left.
            ((static_cast<std::remove_reference_t<decltype(Inits)>&&>(Inits).destroy(), dummy) = ...
             = 0);
            (void)dummy;
        }

        scoped_initializer(const scoped_initializer&) = delete;
        scoped_initializer& operator=(const scoped_initializer&) = delete;
    };
} // namespace ATUM_VERSION_NS
} // namespace atum

//=== manual_init ===//
namespace atum
{
inline namespace ATUM_VERSION_NS
{
    template <typename T, typename Initializer = init_default>
    class manual_init
    {
    public:
        using element_type = T;

        constexpr manual_init() = default;

        void initialize()
        {
            Initializer::template init<T>(_storage.memory());
#if ATUM_CHECK_LIFETIME
            _initialized = true;
#endif
        }
        void destroy() && noexcept
        {
            _storage.get().~T();
#if ATUM_CHECK_LIFETIME
            _initialized = false;
#endif
        }

        element_type& get() noexcept
        {
            ATUM_LIFETIME_ASSERT(_initialized);
            return _storage.get();
        }

        element_type& operator*() noexcept
        {
            return get();
        }
        element_type* operator->() noexcept
        {
            return &get();
        }

    private:
        storage<T> _storage;
#if ATUM_CHECK_LIFETIME
        bool _initialized = false;
#endif
    };
} // namespace ATUM_VERSION_NS
} // namespace atum

//=== lazy_init ===//
namespace atum
{
inline namespace ATUM_VERSION_NS
{
    template <typename Tag, typename T, typename Initializer = init_default>
    class lazy_init
    {
    public:
        using element_type = T;
        using initializer  = Initializer;

        constexpr lazy_init() = default;

        void initialize()
        {
            // We're creating a function local static once per tag.
            // This static will be lazily and thread safely initialized once.
            // This initialization will also initialize the global.

#if ATUM_CHECK_LIFETIME
            // If lifetime checking is enabled, we remember the this pointer of the object that was
            // initialized. This allows to detect if the same tag type was used multiple types.
            static void* initializer
                = (initializer::template init<element_type>(_storage.memory()), this);
            ATUM_LIFETIME_ASSERT(initializer == this);
#else
            static bool initializer
                = (initializer::template init<element_type>(_storage.memory()), true);
            (void)initializer;
#endif
        }
        void destroy() && noexcept
        {
            // leak
        }

        element_type& get()
        {
            initialize();
            return _storage.get();
        }

        element_type& operator*()
        {
            return get();
        }
        element_type* operator->()
        {
            return &get();
        }

    private:
        storage<T> _storage;
    };
} // namespace ATUM_VERSION_NS
} // namespace atum

//=== nitfy_init ===//
namespace atum
{
inline namespace ATUM_VERSION_NS
{
    template <typename T, typename Initializer = init_default>
    class nifty_init
    {
    public:
        using element_type = T;

        constexpr nifty_init() : _count(0) {}

        constexpr element_type& reference() noexcept
        {
            return _storage.get();
        }

        void initialize()
        {
            if (_count++ == 0)
                // First one to execute, initialize.
                Initializer::template init<T>(_storage.memory());
        }
        void destroy() && noexcept
        {
            if (--_count == 0)
                // Last one to execute, destroy.
                _storage.get().~T();
        }

        element_type& get() noexcept
        {
            ATUM_LIFETIME_ASSERT(_count > 0);
            return _storage.get();
        }

        element_type& operator*() noexcept
        {
            return get();
        }
        element_type* operator->() noexcept
        {
            return &get();
        }

    private:
        storage<T>       _storage;
        std::atomic<int> _count;
    };

    template <auto& Nifty>
    using nifty_counter_for = scoped_initializer<Nifty>;
} // namespace ATUM_VERSION_NS
} // namespace atum

#endif // ATUM_HPP_INCLUDED

