# atum

Like [Atum](https://en.wikipedia.org/wiki/Atum), the Egyptian god of pre-existence and post-existence, this single-header library takes care of global variable initialization and destruction.
Normally, global initialization between translation units is done in an unspecified order, which makes it problematic to access global variables in the constructors of other global variables.
This is referred to as the [static initialization order fiasco](https://isocpp.org/wiki/faq/ctors#static-init-order) and atum has multiple solutions.

```cpp
#include <atum.hpp>

// A global that is initialized at compile time.
ATUM_CONSTINIT std::mutex mutex;
// A global that is initialized manually.
ATUM_CONSTINIT atum::manual_init<std::string, atum::init_braced<'a', 'b', 'c'>> string;
// A global that is initialized on demand.
ATUM_CONSTINIT atum::lazy_init<struct container, std::vector<int>> container;

int main()
{
    atum::scoped_initializer<string> initializer;
    container->push_back(42);
    std::cout << "string " << *string << '\n';
}
```

## Features

* single-header, C++17 library with minimal standard library dependencies
* forward-compatible macro for C++20's [`constinit` keyword](https://en.cppreference.com/w/cpp/language/constinit)
* manual, lazy, and [nifty counter](https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter) initialization
* optional debug mode that checks global variable lifetime before access

## Documentation

### Installation

Just copy the single header `include/atum.hpp` into your project and enable the C++17 compiler flag.
Alternatively you can use the CMake target `foonathan::atum` either via subdirectory or installation.

Lifetime checking is enabled by defining `ATUM_CHECK_LIFETIME` to `1` (defaults to no checking).
The library will then keep track whether a global has been initialized and error on access of an uninitialized object.
The assertion can be customized by further defining an `ATUM_LIFETIME_ASSERT(Cond)` macro, which defaults to `assert(Cond)`.

### `Init` classes

The classes ending with `_init` all have an interface that looks like this:

```cpp
/// Holds an object of the specified type and will initialize it using the `Initializer`.
template <typename T, typename Initializer = atum::init_default>
class Init
{
public:
    using element_type = T; 

    //=== construction ===//
    /// constexpr default constructor that does nothing.
    constexpr Init();

    // Initialize the object using the `Initializer`.
    // This might be called automatically, or you have to manually call it depending on the semantics.
    void initialize();

    // Destroy the object.
    // This might be called automatically, or you have to manually call it depending on the semantics.
    void destroy() &&;

    //=== access ===//
    element_type& get();

    element_type& operator*();
    element_type* operator->();
};
```

They hold an object of the specified type and provide pointer-like access as well as a `.get()` method.
As they do not initialize anything in their constructors, the `Init` objects themselves are subject to constant initialization and thus always available.

Initialization is done using an `Initializer`, which controls how the object is initialized:

* `atum::init_default`: initialize using the default constructor. This is the default if no initializer is specified.
* `atum::init_from<args...>`: initialize using `T(args...)`. Note that the arguments are passed as template parameters.
* `atum::init_braced<args...>`: initialize using `T{args...}`. Note that the arguments are passed as template parameters.
* `atum::init_fn<Fn>`: initialize using `T(Fn())`, where `Fn` is some function pointer.

The helper class `atum::scoped_initializer<Inits...>` takes some `Init` objects as template parameter and will call `.initialize()` on all of them in its constructor and `.destroy()` in its destructor (in reverse order).
It can be used to initialize `atum::manual_init` variables, but is also valid for all other `Init` classes.
If used with other `Init` classes, it might have no effect, but will definitely ensure that the global is initialized while the `atum::scoped_initializer` object lives.

```cpp
ATUM_CONSTINIT atum::manual_init<std::string> a;
ATUM_CONSTINIT atum::manual_init<std::string> b;
ATUM_CONSTINIT atum::lazy_init<std::string> c;

int main()
{
    atum::scoped_initializer<a, b, c> initializer;
    // During this scope, a, b, c are initialized.
    // c was going to be initialized on access anyway, but the initializer has done it earlier during its constructor.
}
```

### Constant Initialization

```cpp
ATUM_CONSTINIT int i = 42; // constant
ATUM_CONSTINIT std::mutex mutex; // constexpr default constructor
```

The best way of initializing global variables is using constant initialization (performed at compile time) and should be done whenever possible.
Use the `ATUM_CONSTINIT` macro to get a compiler error if constant initialization could not be performed at compile-time (only with C++20 or on supported compilers).

### Manual Initialization

```cpp
template <typename T, typename Initializer = atum::init_default>
class manual_init
{
    // See `Init` for interface.
};
```

Use the `Init` class `atum::manual_init` to declare a global variable that has to be initialized manually.
Calling `.initialize()` will initialize it, and calling `.destroy()` will destroy it.
You have to manually ensure that the globals are initialized before their first use and in the correct order to handle inter-global dependencies.

The recommended way is to perform initialization in the beginning of `main()` and destruction at the end.

```cpp
// Some global logger.
ATUM_CONSTINIT atum::manual_init<Logger> logger;
// Some other global that logs something in its constructor.
ATUM_CONSTINIT atum::manual_init<Global> global;

int main()
{
    // We need to ensure that `logger` is initialized before `global`.
    atum::scoped_initializer<logger, global> initializer;

    // You can now access the variables.
    logger->log(*global);
}
```


### Lazy Initialization

```cpp
template <typename Tag, typename T, typename Initializer = atum::init_default>
class lazy_init
{
    // See `Init` for interface.
};
```

Use the `Init` class `atum::lazy_init` to declare a global variable that is initialized on first use or when `.initialize()` is called (whatever happens first).
The lazy initialization is done in a thread-safe way by leveraging a function local `static`.
The `Tag` argument is some type that just has to be different for each `atum::lazy_init` object.

**Note:** Due to the fact that destruction of globals is done in a LIFO order that might be undesirable, `.destroy()` is a no-op, i.e. a lazily initialized global is never destroyed.
This can create resource leaks.

```cpp
// A logger that is initialized on first use.
// The tag type ensures that we can create multiple global loggers that are distinct objects nonetheless.
ATUM_CONSTINIT atum::lazy_init<struct logger, Logger> logger;

int main()
{
    // First usage will create logger.
    logger->log("Hello!");
}
```

### Nifty Initialization

```cpp
template <typename T, typename Initializer = atum::init_default>
class nifty_init
{
public:
    // Provides compile-time, unchecked access to the stored object.
    constexpr T& reference() const noexcept;

    // See `Init` for remaining interface.
};

template <auto& Nifty>
using nifty_counter_for = scoped_initializer<Nifty>;
```

Use the `Init` class `atum::nifty_init` to declare a global variable that is initialized using [nifty counters](https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter).
It is very similar to `atum::manual_init` but uses a counter to allow calling `.initialize()` and `.destroy()` multiple times:
only the first call to `.initialize()` and last call to `.destroy()` will actually initialize/destroy the object; the others have no effect.

Nifty counters allow initialization that happens automatically before use, just like with `atum::lazy_init`, but also ensure destruction.
It is used by `std::cout`, for example.

The header for the global variable must create three objects:

1. The `atum::nifty_init` object that will be initialized at compile-time.
2. A `static` `atum::nfity_counter_for` object that will have copies for each translation-unit the header is included.
   It ensures the automatic initialization.
3. A reference to the object stored in the init object; users can just access it directly and it will have been initialized.

```cpp
// logger.hpp

// A logger that is initialized using nifty counters.
inline ATUM_CONSTINIT atum::nifty_init<Logger> logger_init;     // The init object.
static atum::nifty_counter_for<logger_init> logger_counter;     // The per-translation-unit counter object.
inline ATUM_CONSTINIT Logger& logger = logger_init.reference(); // The global users are going to access.

// user.cpp
#include "logger.hpp"

int main()
{
    // Just use the reference.
    logger.log("Hello World!");
}
```

**Note:** for nifty initialization to work properly, a couple of things have to be insured:

1. The header of every global `B` that depends on a nifty initialized global `A` must include the header of `A` - even if the `A` is otherwise an implementation detail of `B`.
   Otherwise, users of `B` will not get the `static` nifty counter object that will initialize `A`.
  
   ```cpp
   // bad:
   class Global
   {
   public:
      // constructor uses nifty-initialized Logger
      Global();
   };

   // good:
   #include "logger.hpp"

   class Global
   {
   public:
      // constructor uses nifty-initialized Logger
      Global();
   };
   ```

2. If you're using a nifty initialized global `A` in the constructor of some variable template or `static` data member of a template, it is not guaranteed to work.

   ```cpp
   class Global
   {
   public:
      // constructor uses nifty-initialized Logger
      Global();
   };

   template <typename T>
   struct Template
   {
       static Global global; // dangerous
   };
   ```

   In that case you need to create an `atum::nfity_counter_for` object yourself.

3. Some combination of dynamically linked libraries is probably also problematic.

