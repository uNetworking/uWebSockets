
//  Copyright 2015-2020 Denis Blank <denis.blank at outlook dot com>
//     Distributed under the Boost Software License, Version 1.0
//       (See accompanying file LICENSE_1_0.txt or copy at
//             http://www.boost.org/LICENSE_1_0.txt)

#ifndef FU2_INCLUDED_FUNCTION2_HPP_
#define FU2_INCLUDED_FUNCTION2_HPP_

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

// Defines:
// - FU2_HAS_DISABLED_EXCEPTIONS
#if defined(FU2_WITH_DISABLED_EXCEPTIONS) ||                                   \
    defined(FU2_MACRO_DISABLE_EXCEPTIONS)
#define FU2_HAS_DISABLED_EXCEPTIONS
#else // FU2_WITH_DISABLED_EXCEPTIONS
#if defined(_MSC_VER)
#if !defined(_HAS_EXCEPTIONS) || (_HAS_EXCEPTIONS == 0)
#define FU2_HAS_DISABLED_EXCEPTIONS
#endif
#elif defined(__clang__)
#if !(__EXCEPTIONS && __has_feature(cxx_exceptions))
#define FU2_HAS_DISABLED_EXCEPTIONS
#endif
#elif defined(__GNUC__)
#if !__EXCEPTIONS
#define FU2_HAS_DISABLED_EXCEPTIONS
#endif
#endif
#endif // FU2_WITH_DISABLED_EXCEPTIONS
// - FU2_HAS_NO_FUNCTIONAL_HEADER
#if !defined(FU2_WITH_NO_FUNCTIONAL_HEADER) &&                                 \
    !defined(FU2_NO_FUNCTIONAL_HEADER) &&                                      \
    !defined(FU2_HAS_DISABLED_EXCEPTIONS)
#include <functional>
#else
#define FU2_HAS_NO_FUNCTIONAL_HEADER
#endif
// - FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE
#if defined(FU2_WITH_CXX17_NOEXCEPT_FUNCTION_TYPE)
#define FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE
#else // FU2_WITH_CXX17_NOEXCEPT_FUNCTION_TYPE
#if defined(_MSC_VER)
#if defined(_HAS_CXX17) && _HAS_CXX17
#define FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE
#endif
#elif defined(__cpp_noexcept_function_type)
#define FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE
#elif defined(__cplusplus) && (__cplusplus >= 201703L)
#define FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE
#endif
#endif // FU2_WITH_CXX17_NOEXCEPT_FUNCTION_TYPE

// - FU2_HAS_NO_EMPTY_PROPAGATION
#if defined(FU2_WITH_NO_EMPTY_PROPAGATION)
#define FU2_HAS_NO_EMPTY_PROPAGATION
#endif // FU2_WITH_NO_EMPTY_PROPAGATION

#if !defined(FU2_HAS_DISABLED_EXCEPTIONS)
#include <exception>
#endif

/// Hint for the compiler that this point should be unreachable
#if defined(_MSC_VER)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_UNREACHABLE_INTRINSIC() __assume(false)
#elif defined(__GNUC__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_UNREACHABLE_INTRINSIC() __builtin_unreachable()
#elif defined(__has_builtin) && __has_builtin(__builtin_unreachable)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_UNREACHABLE_INTRINSIC() __builtin_unreachable()
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_UNREACHABLE_INTRINSIC() abort()
#endif

/// Causes the application to exit abnormally
#if defined(_MSC_VER)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_TRAP() __debugbreak()
#elif defined(__GNUC__)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_TRAP() __builtin_trap()
#elif defined(__has_builtin) && __has_builtin(__builtin_trap)
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_TRAP() __builtin_trap()
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_TRAP() *(volatile int*)0x11 = 0
#endif

#ifndef NDEBUG
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_UNREACHABLE() ::fu2::detail::unreachable_debug()
#else
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define FU2_DETAIL_UNREACHABLE() FU2_DETAIL_UNREACHABLE_INTRINSIC()
#endif

namespace fu2 {
inline namespace abi_400 {
namespace detail {
template <typename Config, typename Property>
class function;

template <typename...>
struct identity {};

// Equivalent to C++17's std::void_t which targets a bug in GCC,
// that prevents correct SFINAE behavior.
// See http://stackoverflow.com/questions/35753920 for details.
template <typename...>
struct deduce_to_void : std::common_type<void> {};

template <typename... T>
using void_t = typename deduce_to_void<T...>::type;

template <typename T>
using unrefcv_t = std::remove_cv_t<std::remove_reference_t<T>>;

// Copy enabler helper class
template <bool /*Copyable*/>
struct copyable {};
template <>
struct copyable<false> {
  copyable() = default;
  ~copyable() = default;
  copyable(copyable const&) = delete;
  copyable(copyable&&) = default;
  copyable& operator=(copyable const&) = delete;
  copyable& operator=(copyable&&) = default;
};

/// Configuration trait to configure the function_base class.
template <bool Owning, bool Copyable, typename Capacity>
struct config {
  // Is true if the function is owning.
  static constexpr auto const is_owning = Owning;

  // Is true if the function is copyable.
  static constexpr auto const is_copyable = Copyable;

  // The internal capacity of the function
  // used in small functor optimization.
  // The object shall expose the real capacity through Capacity::capacity
  // and the intended alignment through Capacity::alignment.
  using capacity = Capacity;
};

/// A config which isn't compatible to other configs
template <bool Throws, bool HasStrongExceptGuarantee, typename... Args>
struct property {
  // Is true when the function throws an exception on empty invocation.
  static constexpr auto const is_throwing = Throws;

  // Is true when the function throws an exception on empty invocation.
  static constexpr auto const is_strong_exception_guaranteed =
      HasStrongExceptGuarantee;
};

#ifndef NDEBUG
[[noreturn]] inline void unreachable_debug() {
  FU2_DETAIL_TRAP();
  std::abort();
}
#endif

/// Provides utilities for invocing callable objects
namespace invocation {
/// Invokes the given callable object with the given arguments
template <typename Callable, typename... Args>
constexpr auto invoke(Callable&& callable, Args&&... args) noexcept(
    noexcept(std::forward<Callable>(callable)(std::forward<Args>(args)...)))
    -> decltype(std::forward<Callable>(callable)(std::forward<Args>(args)...)) {

  return std::forward<Callable>(callable)(std::forward<Args>(args)...);
}
/// Invokes the given member function pointer by reference
template <typename T, typename Type, typename Self, typename... Args>
constexpr auto invoke(Type T::*member, Self&& self, Args&&... args) noexcept(
    noexcept((std::forward<Self>(self).*member)(std::forward<Args>(args)...)))
    -> decltype((std::forward<Self>(self).*
                 member)(std::forward<Args>(args)...)) {
  return (std::forward<Self>(self).*member)(std::forward<Args>(args)...);
}
/// Invokes the given member function pointer by pointer
template <typename T, typename Type, typename Self, typename... Args>
constexpr auto invoke(Type T::*member, Self&& self, Args&&... args) noexcept(
    noexcept((std::forward<Self>(self)->*member)(std::forward<Args>(args)...)))
    -> decltype(
        (std::forward<Self>(self)->*member)(std::forward<Args>(args)...)) {
  return (std::forward<Self>(self)->*member)(std::forward<Args>(args)...);
}
/// Invokes the given pointer to a scalar member by reference
template <typename T, typename Type, typename Self>
constexpr auto
invoke(Type T::*member,
       Self&& self) noexcept(noexcept(std::forward<Self>(self).*member))
    -> decltype(std::forward<Self>(self).*member) {
  return (std::forward<Self>(self).*member);
}
/// Invokes the given pointer to a scalar member by pointer
template <typename T, typename Type, typename Self>
constexpr auto
invoke(Type T::*member,
       Self&& self) noexcept(noexcept(std::forward<Self>(self)->*member))
    -> decltype(std::forward<Self>(self)->*member) {
  return std::forward<Self>(self)->*member;
}

/// Deduces to a true type if the callable object can be invoked with
/// the given arguments.
/// We don't use invoke here because MSVC can't evaluate the nested expression
/// SFINAE here.
template <typename T, typename Args, typename = void>
struct can_invoke : std::false_type {};
template <typename T, typename... Args>
struct can_invoke<T, identity<Args...>,
                  decltype((void)std::declval<T>()(std::declval<Args>()...))>
    : std::true_type {};
template <typename Pointer, typename T, typename... Args>
struct can_invoke<Pointer, identity<T&, Args...>,
                  decltype((void)((std::declval<T&>().*std::declval<Pointer>())(
                      std::declval<Args>()...)))> : std::true_type {};
template <typename Pointer, typename T, typename... Args>
struct can_invoke<Pointer, identity<T&&, Args...>,
                  decltype(
                      (void)((std::declval<T&&>().*std::declval<Pointer>())(
                          std::declval<Args>()...)))> : std::true_type {};
template <typename Pointer, typename T, typename... Args>
struct can_invoke<Pointer, identity<T*, Args...>,
                  decltype(
                      (void)((std::declval<T*>()->*std::declval<Pointer>())(
                          std::declval<Args>()...)))> : std::true_type {};
template <typename Pointer, typename T>
struct can_invoke<Pointer, identity<T&>,
                  decltype((void)(std::declval<T&>().*std::declval<Pointer>()))>
    : std::true_type {};
template <typename Pointer, typename T>
struct can_invoke<Pointer, identity<T&&>,
                  decltype(
                      (void)(std::declval<T&&>().*std::declval<Pointer>()))>
    : std::true_type {};
template <typename Pointer, typename T>
struct can_invoke<Pointer, identity<T*>,
                  decltype(
                      (void)(std::declval<T*>()->*std::declval<Pointer>()))>
    : std::true_type {};

template <bool RequiresNoexcept, typename T, typename Args>
struct is_noexcept_correct : std::true_type {};
template <typename T, typename... Args>
struct is_noexcept_correct<true, T, identity<Args...>>
    : std::integral_constant<bool, noexcept(invoke(std::declval<T>(),
                                                   std::declval<Args>()...))> {
};
} // end namespace invocation

namespace overloading {
template <typename... Args>
struct overload_impl;
template <typename Current, typename Next, typename... Rest>
struct overload_impl<Current, Next, Rest...> : Current,
                                               overload_impl<Next, Rest...> {
  explicit overload_impl(Current current, Next next, Rest... rest)
      : Current(std::move(current)), overload_impl<Next, Rest...>(
                                         std::move(next), std::move(rest)...) {
  }

  using Current::operator();
  using overload_impl<Next, Rest...>::operator();
};
template <typename Current>
struct overload_impl<Current> : Current {
  explicit overload_impl(Current current) : Current(std::move(current)) {
  }

  using Current::operator();
};

template <typename... T>
constexpr auto overload(T&&... callables) {
  return overload_impl<std::decay_t<T>...>{std::forward<T>(callables)...};
}
} // namespace overloading

/// Declares the namespace which provides the functionality to work with a
/// type-erased object.
namespace type_erasure {
/// Specialization to work with addresses of callable objects
template <typename T, typename = void>
struct address_taker {
  template <typename O>
  static void* take(O&& obj) {
    return std::addressof(obj);
  }
  static T& restore(void* ptr) {
    return *static_cast<T*>(ptr);
  }
  static T const& restore(void const* ptr) {
    return *static_cast<T const*>(ptr);
  }
  static T volatile& restore(void volatile* ptr) {
    return *static_cast<T volatile*>(ptr);
  }
  static T const volatile& restore(void const volatile* ptr) {
    return *static_cast<T const volatile*>(ptr);
  }
};
/// Specialization to work with addresses of raw function pointers
template <typename T>
struct address_taker<T, std::enable_if_t<std::is_pointer<T>::value>> {
  template <typename O>
  static void* take(O&& obj) {
    return reinterpret_cast<void*>(obj);
  }
  template <typename O>
  static T restore(O ptr) {
    return reinterpret_cast<T>(const_cast<void*>(ptr));
  }
};

template <typename Box>
struct box_factory;
/// Store the allocator inside the box
template <bool IsCopyable, typename T, typename Allocator>
struct box : private Allocator {
  friend box_factory<box>;

  T value_;

  explicit box(T value, Allocator allocator)
      : Allocator(std::move(allocator)), value_(std::move(value)) {
  }

  box(box&&) = default;
  box(box const&) = default;
  box& operator=(box&&) = default;
  box& operator=(box const&) = default;
  ~box() = default;
};
template <typename T, typename Allocator>
struct box<false, T, Allocator> : private Allocator {
  friend box_factory<box>;

  T value_;

  explicit box(T value, Allocator allocator)
      : Allocator(std::move(allocator)), value_(std::move(value)) {
  }

  box(box&&) = default;
  box(box const&) = delete;
  box& operator=(box&&) = default;
  box& operator=(box const&) = delete;
  ~box() = default;
};

template <bool IsCopyable, typename T, typename Allocator>
struct box_factory<box<IsCopyable, T, Allocator>> {
  using real_allocator =
      typename std::allocator_traits<std::decay_t<Allocator>>::
          template rebind_alloc<box<IsCopyable, T, Allocator>>;

  /// Allocates space through the boxed allocator
  static box<IsCopyable, T, Allocator>*
  box_allocate(box<IsCopyable, T, Allocator> const* me) {
    real_allocator allocator(*static_cast<Allocator const*>(me));

    return static_cast<box<IsCopyable, T, Allocator>*>(
        std::allocator_traits<real_allocator>::allocate(allocator, 1U));
  }

  /// Destroys the box through the given allocator
  static void box_deallocate(box<IsCopyable, T, Allocator>* me) {
    real_allocator allocator(*static_cast<Allocator const*>(me));

    me->~box();
    std::allocator_traits<real_allocator>::deallocate(allocator, me, 1U);
  }
};

/// Creates a box containing the given value and allocator
template <bool IsCopyable, typename T, typename Allocator>
auto make_box(std::integral_constant<bool, IsCopyable>, T&& value,
              Allocator&& allocator) {
  return box<IsCopyable, std::decay_t<T>, std::decay_t<Allocator>>(
      std::forward<T>(value), std::forward<Allocator>(allocator));
}

template <typename T>
struct is_box : std::false_type {};
template <bool IsCopyable, typename T, typename Allocator>
struct is_box<box<IsCopyable, T, Allocator>> : std::true_type {};

/// Provides access to the pointer to a heal allocated erased object
/// as well to the inplace storage.
union data_accessor {
  data_accessor() = default;
  explicit constexpr data_accessor(std::nullptr_t) noexcept : ptr_(nullptr) {
  }
  explicit constexpr data_accessor(void* ptr) noexcept : ptr_(ptr) {
  }

  /// The pointer we use if the object is on the heap
  void* ptr_;
  /// The first field of the inplace storage
  std::size_t inplace_storage_;
};

/// See opcode::op_fetch_empty
constexpr void write_empty(data_accessor* accessor, bool empty) noexcept {
  accessor->inplace_storage_ = std::size_t(empty);
}

template <typename From, typename To>
using transfer_const_t =
    std::conditional_t<std::is_const<std::remove_pointer_t<From>>::value,
                       std::add_const_t<To>, To>;
template <typename From, typename To>
using transfer_volatile_t =
    std::conditional_t<std::is_volatile<std::remove_pointer_t<From>>::value,
                       std::add_volatile_t<To>, To>;

/// The retriever when the object is allocated inplace
template <typename T, typename Accessor>
constexpr auto retrieve(std::true_type /*is_inplace*/, Accessor from,
                        std::size_t from_capacity) {
  using type = transfer_const_t<Accessor, transfer_volatile_t<Accessor, void>>*;

  /// Process the command by using the data inside the internal capacity
  auto storage = &(from->inplace_storage_);
  auto inplace = const_cast<void*>(static_cast<type>(storage));
  return type(std::align(alignof(T), sizeof(T), inplace, from_capacity));
}

/// The retriever which is used when the object is allocated
/// through the allocator
template <typename T, typename Accessor>
constexpr auto retrieve(std::false_type /*is_inplace*/, Accessor from,
                        std::size_t /*from_capacity*/) {

  return from->ptr_;
}

namespace invocation_table {
#if !defined(FU2_HAS_DISABLED_EXCEPTIONS)
#if defined(FU2_HAS_NO_FUNCTIONAL_HEADER)
struct bad_function_call : std::exception {
  bad_function_call() noexcept {
  }

  char const* what() const noexcept override {
    return "bad function call";
  }
};
#else
using std::bad_function_call;
#endif
#endif

#ifdef FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE
#define FU2_DETAIL_EXPAND_QUALIFIERS_NOEXCEPT(F)                               \
  F(, , noexcept, , &)                                                         \
  F(const, , noexcept, , &)                                                    \
  F(, volatile, noexcept, , &)                                                 \
  F(const, volatile, noexcept, , &)                                            \
  F(, , noexcept, &, &)                                                        \
  F(const, , noexcept, &, &)                                                   \
  F(, volatile, noexcept, &, &)                                                \
  F(const, volatile, noexcept, &, &)                                           \
  F(, , noexcept, &&, &&)                                                      \
  F(const, , noexcept, &&, &&)                                                 \
  F(, volatile, noexcept, &&, &&)                                              \
  F(const, volatile, noexcept, &&, &&)
#define FU2_DETAIL_EXPAND_CV_NOEXCEPT(F)                                       \
  F(, , noexcept)                                                              \
  F(const, , noexcept)                                                         \
  F(, volatile, noexcept)                                                      \
  F(const, volatile, noexcept)
#else // FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE
#define FU2_DETAIL_EXPAND_QUALIFIERS_NOEXCEPT(F)
#define FU2_DETAIL_EXPAND_CV_NOEXCEPT(F)
#endif // FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE

#define FU2_DETAIL_EXPAND_QUALIFIERS(F)                                        \
  F(, , , , &)                                                                 \
  F(const, , , , &)                                                            \
  F(, volatile, , , &)                                                         \
  F(const, volatile, , , &)                                                    \
  F(, , , &, &)                                                                \
  F(const, , , &, &)                                                           \
  F(, volatile, , &, &)                                                        \
  F(const, volatile, , &, &)                                                   \
  F(, , , &&, &&)                                                              \
  F(const, , , &&, &&)                                                         \
  F(, volatile, , &&, &&)                                                      \
  F(const, volatile, , &&, &&)                                                 \
  FU2_DETAIL_EXPAND_QUALIFIERS_NOEXCEPT(F)
#define FU2_DETAIL_EXPAND_CV(F)                                                \
  F(, , )                                                                      \
  F(const, , )                                                                 \
  F(, volatile, )                                                              \
  F(const, volatile, )                                                         \
  FU2_DETAIL_EXPAND_CV_NOEXCEPT(F)

/// If the function is qualified as noexcept, the call will never throw
template <bool IsNoexcept>
[[noreturn]] void throw_or_abortnoexcept(
    std::integral_constant<bool, IsNoexcept> /*is_throwing*/) noexcept {
  std::abort();
}
/// Calls std::abort on empty function calls
[[noreturn]] inline void
throw_or_abort(std::false_type /*is_throwing*/) noexcept {
  std::abort();
}
/// Throws bad_function_call on empty funciton calls
[[noreturn]] inline void throw_or_abort(std::true_type /*is_throwing*/) {
#ifdef FU2_HAS_DISABLED_EXCEPTIONS
  throw_or_abort(std::false_type{});
#else
  throw bad_function_call{};
#endif
}

template <typename T>
struct function_trait;

using is_noexcept_ = std::false_type;
using is_noexcept_noexcept = std::true_type;

#define FU2_DEFINE_FUNCTION_TRAIT(CONST, VOLATILE, NOEXCEPT, OVL_REF, REF)     \
  template <typename Ret, typename... Args>                                    \
  struct function_trait<Ret(Args...) CONST VOLATILE OVL_REF NOEXCEPT> {        \
    using pointer_type = Ret (*)(data_accessor CONST VOLATILE*,                \
                                 std::size_t capacity, Args...);               \
    template <typename T, bool IsInplace>                                      \
    struct internal_invoker {                                                  \
      static Ret invoke(data_accessor CONST VOLATILE* data,                    \
                        std::size_t capacity, Args... args) NOEXCEPT {         \
        auto obj = retrieve<T>(std::integral_constant<bool, IsInplace>{},      \
                               data, capacity);                                \
        auto box = static_cast<T CONST VOLATILE*>(obj);                        \
        return invocation::invoke(                                             \
            static_cast<std::decay_t<decltype(box->value_)> CONST VOLATILE     \
                            REF>(box->value_),                                 \
            std::forward<Args>(args)...);                                      \
      }                                                                        \
    };                                                                         \
                                                                               \
    template <typename T>                                                      \
    struct view_invoker {                                                      \
      static Ret invoke(data_accessor CONST VOLATILE* data, std::size_t,       \
                        Args... args) NOEXCEPT {                               \
                                                                               \
        auto ptr = static_cast<void CONST VOLATILE*>(data->ptr_);              \
        return invocation::invoke(address_taker<T>::restore(ptr),              \
                                  std::forward<Args>(args)...);                \
      }                                                                        \
    };                                                                         \
                                                                               \
    template <typename T>                                                      \
    using callable = T CONST VOLATILE REF;                                     \
                                                                               \
    using arguments = identity<Args...>;                                       \
                                                                               \
    using is_noexcept = is_noexcept_##NOEXCEPT;                                \
                                                                               \
    template <bool Throws>                                                     \
    struct empty_invoker {                                                     \
      static Ret invoke(data_accessor CONST VOLATILE* /*data*/,                \
                        std::size_t /*capacity*/, Args... /*args*/) NOEXCEPT { \
        throw_or_abort##NOEXCEPT(std::integral_constant<bool, Throws>{});      \
      }                                                                        \
    };                                                                         \
  };

FU2_DETAIL_EXPAND_QUALIFIERS(FU2_DEFINE_FUNCTION_TRAIT)
#undef FU2_DEFINE_FUNCTION_TRAIT

/// Deduces to the function pointer to the given signature
template <typename Signature>
using function_pointer_of = typename function_trait<Signature>::pointer_type;

template <typename... Args>
struct invoke_table;

/// We optimize the vtable_t in case there is a single function overload
template <typename First>
struct invoke_table<First> {
  using type = function_pointer_of<First>;

  /// Return the function pointer itself
  template <std::size_t Index>
  static constexpr auto fetch(type pointer) noexcept {
    static_assert(Index == 0U, "The index should be 0 here!");
    return pointer;
  }

  /// Returns the thunk of an single overloaded callable
  template <typename T, bool IsInplace>
  static constexpr type get_invocation_table_of() noexcept {
    return &function_trait<First>::template internal_invoker<T,
                                                             IsInplace>::invoke;
  }
  /// Returns the thunk of an single overloaded callable
  template <typename T>
  static constexpr type get_invocation_view_table_of() noexcept {
    return &function_trait<First>::template view_invoker<T>::invoke;
  }
  /// Returns the thunk of an empty single overloaded callable
  template <bool IsThrowing>
  static constexpr type get_empty_invocation_table() noexcept {
    return &function_trait<First>::template empty_invoker<IsThrowing>::invoke;
  }
};
/// We generate a table in case of multiple function overloads
template <typename First, typename Second, typename... Args>
struct invoke_table<First, Second, Args...> {
  using type =
      std::tuple<function_pointer_of<First>, function_pointer_of<Second>,
                 function_pointer_of<Args>...> const*;

  /// Return the function pointer at the particular index
  template <std::size_t Index>
  static constexpr auto fetch(type table) noexcept {
    return std::get<Index>(*table);
  }

  /// The invocation vtable for a present object
  template <typename T, bool IsInplace>
  struct invocation_vtable : public std::tuple<function_pointer_of<First>,
                                               function_pointer_of<Second>,
                                               function_pointer_of<Args>...> {
    constexpr invocation_vtable() noexcept
        : std::tuple<function_pointer_of<First>, function_pointer_of<Second>,
                     function_pointer_of<Args>...>(std::make_tuple(
              &function_trait<First>::template internal_invoker<
                  T, IsInplace>::invoke,
              &function_trait<Second>::template internal_invoker<
                  T, IsInplace>::invoke,
              &function_trait<Args>::template internal_invoker<
                  T, IsInplace>::invoke...)) {
    }
  };

  /// Returns the thunk of an multi overloaded callable
  template <typename T, bool IsInplace>
  static type get_invocation_table_of() noexcept {
    static invocation_vtable<T, IsInplace> const table;
    return &table;
  }

  /// The invocation vtable for a present object
  template <typename T>
  struct invocation_view_vtable
      : public std::tuple<function_pointer_of<First>,
                          function_pointer_of<Second>,
                          function_pointer_of<Args>...> {
    constexpr invocation_view_vtable() noexcept
        : std::tuple<function_pointer_of<First>, function_pointer_of<Second>,
                     function_pointer_of<Args>...>(std::make_tuple(
              &function_trait<First>::template view_invoker<T>::invoke,
              &function_trait<Second>::template view_invoker<T>::invoke,
              &function_trait<Args>::template view_invoker<T>::invoke...)) {
    }
  };

  /// Returns the thunk of an multi overloaded callable
  template <typename T>
  static type get_invocation_view_table_of() noexcept {
    static invocation_view_vtable<T> const table;
    return &table;
  }

  /// The invocation table for an empty wrapper
  template <bool IsThrowing>
  struct empty_vtable : public std::tuple<function_pointer_of<First>,
                                          function_pointer_of<Second>,
                                          function_pointer_of<Args>...> {
    constexpr empty_vtable() noexcept
        : std::tuple<function_pointer_of<First>, function_pointer_of<Second>,
                     function_pointer_of<Args>...>(
              std::make_tuple(&function_trait<First>::template empty_invoker<
                                  IsThrowing>::invoke,
                              &function_trait<Second>::template empty_invoker<
                                  IsThrowing>::invoke,
                              &function_trait<Args>::template empty_invoker<
                                  IsThrowing>::invoke...)) {
    }
  };

  /// Returns the thunk of an multi single overloaded callable
  template <bool IsThrowing>
  static type get_empty_invocation_table() noexcept {
    static empty_vtable<IsThrowing> const table;
    return &table;
  }
};

template <std::size_t Index, typename Function, typename... Signatures>
class operator_impl;

#define FU2_DEFINE_FUNCTION_TRAIT(CONST, VOLATILE, NOEXCEPT, OVL_REF, REF)     \
  template <std::size_t Index, typename Function, typename Ret,                \
            typename... Args, typename Next, typename... Signatures>           \
  class operator_impl<Index, Function,                                         \
                      Ret(Args...) CONST VOLATILE OVL_REF NOEXCEPT, Next,      \
                      Signatures...>                                           \
      : operator_impl<Index + 1, Function, Next, Signatures...> {              \
                                                                               \
    template <std::size_t, typename, typename...>                              \
    friend class operator_impl;                                                \
                                                                               \
  protected:                                                                   \
    operator_impl() = default;                                                 \
    ~operator_impl() = default;                                                \
    operator_impl(operator_impl const&) = default;                             \
    operator_impl(operator_impl&&) = default;                                  \
    operator_impl& operator=(operator_impl const&) = default;                  \
    operator_impl& operator=(operator_impl&&) = default;                       \
                                                                               \
    using operator_impl<Index + 1, Function, Next, Signatures...>::operator(); \
                                                                               \
    Ret operator()(Args... args) CONST VOLATILE OVL_REF NOEXCEPT {             \
      auto parent = static_cast<Function CONST VOLATILE*>(this);               \
      using erasure_t = std::decay_t<decltype(parent->erasure_)>;              \
                                                                               \
      /* `std::decay_t<decltype(parent->erasure_)>` is a workaround for a   */ \
      /* compiler regression of MSVC 16.3.1, see #29 for details.           */ \
      return std::decay_t<decltype(parent->erasure_)>::template invoke<Index>( \
          static_cast<erasure_t CONST VOLATILE REF>(parent->erasure_),         \
          std::forward<Args>(args)...);                                        \
    }                                                                          \
  };                                                                           \
  template <std::size_t Index, typename Config, typename Property,             \
            typename Ret, typename... Args>                                    \
  class operator_impl<Index, function<Config, Property>,                       \
                      Ret(Args...) CONST VOLATILE OVL_REF NOEXCEPT>            \
      : copyable<!Config::is_owning || Config::is_copyable> {                  \
                                                                               \
    template <std::size_t, typename, typename...>                              \
    friend class operator_impl;                                                \
                                                                               \
  protected:                                                                   \
    operator_impl() = default;                                                 \
    ~operator_impl() = default;                                                \
    operator_impl(operator_impl const&) = default;                             \
    operator_impl(operator_impl&&) = default;                                  \
    operator_impl& operator=(operator_impl const&) = default;                  \
    operator_impl& operator=(operator_impl&&) = default;                       \
                                                                               \
    Ret operator()(Args... args) CONST VOLATILE OVL_REF NOEXCEPT {             \
      auto parent =                                                            \
          static_cast<function<Config, Property> CONST VOLATILE*>(this);       \
      using erasure_t = std::decay_t<decltype(parent->erasure_)>;              \
                                                                               \
      /* `std::decay_t<decltype(parent->erasure_)>` is a workaround for a   */ \
      /* compiler regression of MSVC 16.3.1, see #29 for details.           */ \
      return std::decay_t<decltype(parent->erasure_)>::template invoke<Index>( \
          static_cast<erasure_t CONST VOLATILE REF>(parent->erasure_),         \
          std::forward<Args>(args)...);                                        \
    }                                                                          \
  };

FU2_DETAIL_EXPAND_QUALIFIERS(FU2_DEFINE_FUNCTION_TRAIT)
#undef FU2_DEFINE_FUNCTION_TRAIT
} // namespace invocation_table

namespace tables {
/// Identifies the action which is dispatched on the erased object
enum class opcode {
  op_move,         //< Move the object and set the vtable
  op_copy,         //< Copy the object and set the vtable
  op_destroy,      //< Destroy the object and reset the vtable
  op_weak_destroy, //< Destroy the object without resetting the vtable
  op_fetch_empty,  //< Stores true or false into the to storage
                   //< to indicate emptiness
};

/// Abstraction for a vtable together with a command table
/// TODO Add optimization for a single formal argument
/// TODO Add optimization to merge both tables if the function is size
/// optimized
template <typename Property>
class vtable;
template <bool IsThrowing, bool HasStrongExceptGuarantee,
          typename... FormalArgs>
class vtable<property<IsThrowing, HasStrongExceptGuarantee, FormalArgs...>> {
  using command_function_t = void (*)(vtable* /*this*/, opcode /*op*/,
                                      data_accessor* /*from*/,
                                      std::size_t /*from_capacity*/,
                                      data_accessor* /*to*/,
                                      std::size_t /*to_capacity*/);

  using invoke_table_t = invocation_table::invoke_table<FormalArgs...>;

  command_function_t cmd_;
  typename invoke_table_t::type vtable_;

  template <typename T>
  struct trait {
    static_assert(is_box<T>::value,
                  "The trait must be specialized with a box!");

    /// The command table
    template <bool IsInplace>
    static void process_cmd(vtable* to_table, opcode op, data_accessor* from,
                            std::size_t from_capacity, data_accessor* to,
                            std::size_t to_capacity) {

      switch (op) {
        case opcode::op_move: {
          /// Retrieve the pointer to the object
          auto box = static_cast<T*>(retrieve<T>(
              std::integral_constant<bool, IsInplace>{}, from, from_capacity));
          assert(box && "The object must not be over aligned or null!");

          if (!IsInplace) {
            // Just swap both pointers if we allocated on the heap
            to->ptr_ = from->ptr_;

#ifndef NDEBUG
            // We don't need to null the pointer since we know that
            // we don't own the data anymore through the vtable
            // which is set to empty.
            from->ptr_ = nullptr;
#endif

            to_table->template set_allocated<T>();

          }
          // The object is allocated inplace
          else {
            construct(std::true_type{}, std::move(*box), to_table, to,
                      to_capacity);
            box->~T();
          }
          return;
        }
        case opcode::op_copy: {
          auto box = static_cast<T const*>(retrieve<T>(
              std::integral_constant<bool, IsInplace>{}, from, from_capacity));
          assert(box && "The object must not be over aligned or null!");

          assert(std::is_copy_constructible<T>::value &&
                 "The box is required to be copyable here!");

          // Try to allocate the object inplace
          construct(std::is_copy_constructible<T>{}, *box, to_table, to,
                    to_capacity);
          return;
        }
        case opcode::op_destroy:
        case opcode::op_weak_destroy: {

          assert(!to && !to_capacity && "Arg overflow!");
          auto box = static_cast<T*>(retrieve<T>(
              std::integral_constant<bool, IsInplace>{}, from, from_capacity));

          if (IsInplace) {
            box->~T();
          } else {
            box_factory<T>::box_deallocate(box);
          }

          if (op == opcode::op_destroy) {
            to_table->set_empty();
          }
          return;
        }
        case opcode::op_fetch_empty: {
          write_empty(to, false);
          return;
        }
      }

      FU2_DETAIL_UNREACHABLE();
    }

    template <typename Box>
    static void
    construct(std::true_type /*apply*/, Box&& box, vtable* to_table,
              data_accessor* to,
              std::size_t to_capacity) noexcept(HasStrongExceptGuarantee) {
      // Try to allocate the object inplace
      void* storage = retrieve<T>(std::true_type{}, to, to_capacity);
      if (storage) {
        to_table->template set_inplace<T>();
      } else {
        // Allocate the object through the allocator
        to->ptr_ = storage =
            box_factory<std::decay_t<Box>>::box_allocate(std::addressof(box));
        to_table->template set_allocated<T>();
      }
      new (storage) T(std::forward<Box>(box));
    }

    template <typename Box>
    static void
    construct(std::false_type /*apply*/, Box&& /*box*/, vtable* /*to_table*/,
              data_accessor* /*to*/,
              std::size_t /*to_capacity*/) noexcept(HasStrongExceptGuarantee) {
    }
  };

  /// The command table
  static void empty_cmd(vtable* to_table, opcode op, data_accessor* /*from*/,
                        std::size_t /*from_capacity*/, data_accessor* to,
                        std::size_t /*to_capacity*/) {

    switch (op) {
      case opcode::op_move:
      case opcode::op_copy: {
        to_table->set_empty();
        break;
      }
      case opcode::op_destroy:
      case opcode::op_weak_destroy: {
        // Do nothing
        break;
      }
      case opcode::op_fetch_empty: {
        write_empty(to, true);
        break;
      }
      default: {
        FU2_DETAIL_UNREACHABLE();
      }
    }
  }

public:
  vtable() noexcept = default;

  /// Initialize an object at the given position
  template <typename T>
  static void init(vtable& table, T&& object, data_accessor* to,
                   std::size_t to_capacity) {

    trait<std::decay_t<T>>::construct(std::true_type{}, std::forward<T>(object),
                                      &table, to, to_capacity);
  }

  /// Moves the object at the given position
  void move(vtable& to_table, data_accessor* from, std::size_t from_capacity,
            data_accessor* to,
            std::size_t to_capacity) noexcept(HasStrongExceptGuarantee) {
    cmd_(&to_table, opcode::op_move, from, from_capacity, to, to_capacity);
    set_empty();
  }

  /// Destroys the object at the given position
  void copy(vtable& to_table, data_accessor const* from,
            std::size_t from_capacity, data_accessor* to,
            std::size_t to_capacity) const {
    cmd_(&to_table, opcode::op_copy, const_cast<data_accessor*>(from),
         from_capacity, to, to_capacity);
  }

  /// Destroys the object at the given position
  void destroy(data_accessor* from,
               std::size_t from_capacity) noexcept(HasStrongExceptGuarantee) {
    cmd_(this, opcode::op_destroy, from, from_capacity, nullptr, 0U);
  }

  /// Destroys the object at the given position without invalidating the
  /// vtable
  void
  weak_destroy(data_accessor* from,
               std::size_t from_capacity) noexcept(HasStrongExceptGuarantee) {
    cmd_(this, opcode::op_weak_destroy, from, from_capacity, nullptr, 0U);
  }

  /// Returns true when the vtable doesn't hold any erased object
  bool empty() const noexcept {
    data_accessor data;
    cmd_(nullptr, opcode::op_fetch_empty, nullptr, 0U, &data, 0U);
    return bool(data.inplace_storage_);
  }

  /// Invoke the function at the given index
  template <std::size_t Index, typename... Args>
  constexpr decltype(auto) invoke(Args&&... args) const {
    auto thunk = invoke_table_t::template fetch<Index>(vtable_);
    return thunk(std::forward<Args>(args)...);
  }
  /// Invoke the function at the given index
  template <std::size_t Index, typename... Args>
  constexpr decltype(auto) invoke(Args&&... args) const volatile {
    auto thunk = invoke_table_t::template fetch<Index>(vtable_);
    return thunk(std::forward<Args>(args)...);
  }

  template <typename T>
  void set_inplace() noexcept {
    using type = std::decay_t<T>;
    vtable_ = invoke_table_t::template get_invocation_table_of<type, true>();
    cmd_ = &trait<type>::template process_cmd<true>;
  }

  template <typename T>
  void set_allocated() noexcept {
    using type = std::decay_t<T>;
    vtable_ = invoke_table_t::template get_invocation_table_of<type, false>();
    cmd_ = &trait<type>::template process_cmd<false>;
  }

  void set_empty() noexcept {
    vtable_ = invoke_table_t::template get_empty_invocation_table<IsThrowing>();
    cmd_ = &empty_cmd;
  }
};
} // namespace tables

/// A union which makes the pointer to the heap object share the
/// same space with the internal capacity.
/// The storage type is distinguished by multiple versions of the
/// control and vtable.
template <typename Capacity, typename = void>
struct internal_capacity {
  /// We extend the union through a technique similar to the tail object hack
  typedef union {
    /// Tag to access the structure in a type-safe way
    data_accessor accessor_;
    /// The internal capacity we use to allocate in-place
    std::aligned_storage_t<Capacity::capacity, Capacity::alignment> capacity_;
  } type;
};
template <typename Capacity>
struct internal_capacity<
    Capacity, std::enable_if_t<(Capacity::capacity < sizeof(void*))>> {
  typedef struct {
    /// Tag to access the structure in a type-safe way
    data_accessor accessor_;
  } type;
};

template <typename Capacity>
class internal_capacity_holder {
  // Tag to access the structure in a type-safe way
  typename internal_capacity<Capacity>::type storage_;

public:
  constexpr internal_capacity_holder() = default;

  constexpr data_accessor* opaque_ptr() noexcept {
    return &storage_.accessor_;
  }
  constexpr data_accessor const* opaque_ptr() const noexcept {
    return &storage_.accessor_;
  }
  constexpr data_accessor volatile* opaque_ptr() volatile noexcept {
    return &storage_.accessor_;
  }
  constexpr data_accessor const volatile* opaque_ptr() const volatile noexcept {
    return &storage_.accessor_;
  }

  static constexpr std::size_t capacity() noexcept {
    return sizeof(storage_);
  }
};

/// An owning erasure
template <bool IsOwning /* = true*/, typename Config, typename Property>
class erasure : internal_capacity_holder<typename Config::capacity> {
  template <bool, typename, typename>
  friend class erasure;
  template <std::size_t, typename, typename...>
  friend class operator_impl;

  using vtable_t = tables::vtable<Property>;

  vtable_t vtable_;

public:
  /// Returns the capacity of this erasure
  static constexpr std::size_t capacity() noexcept {
    return internal_capacity_holder<typename Config::capacity>::capacity();
  }

  constexpr erasure() noexcept {
    vtable_.set_empty();
  }

  constexpr erasure(std::nullptr_t) noexcept {
    vtable_.set_empty();
  }

  constexpr erasure(erasure&& right) noexcept(
      Property::is_strong_exception_guaranteed) {
    right.vtable_.move(vtable_, right.opaque_ptr(), right.capacity(),
                       this->opaque_ptr(), capacity());
  }

  constexpr erasure(erasure const& right) {
    right.vtable_.copy(vtable_, right.opaque_ptr(), right.capacity(),
                       this->opaque_ptr(), capacity());
  }

  template <typename OtherConfig>
  constexpr erasure(erasure<true, OtherConfig, Property> right) noexcept(
      Property::is_strong_exception_guaranteed) {
    right.vtable_.move(vtable_, right.opaque_ptr(), right.capacity(),
                       this->opaque_ptr(), capacity());
  }

  template <typename T, typename Allocator = std::allocator<std::decay_t<T>>>
  constexpr erasure(std::false_type /*use_bool_op*/, T&& callable,
                    Allocator&& allocator = Allocator{}) {
    vtable_t::init(vtable_,
                   type_erasure::make_box(
                       std::integral_constant<bool, Config::is_copyable>{},
                       std::forward<T>(callable),
                       std::forward<Allocator>(allocator)),
                   this->opaque_ptr(), capacity());
  }
  template <typename T, typename Allocator = std::allocator<std::decay_t<T>>>
  constexpr erasure(std::true_type /*use_bool_op*/, T&& callable,
                    Allocator&& allocator = Allocator{}) {
    if (bool(callable)) {
      vtable_t::init(vtable_,
                     type_erasure::make_box(
                         std::integral_constant<bool, Config::is_copyable>{},
                         std::forward<T>(callable),
                         std::forward<Allocator>(allocator)),
                     this->opaque_ptr(), capacity());
    } else {
      vtable_.set_empty();
    }
  }

  ~erasure() {
    vtable_.weak_destroy(this->opaque_ptr(), capacity());
  }

  constexpr erasure&
  operator=(std::nullptr_t) noexcept(Property::is_strong_exception_guaranteed) {
    vtable_.destroy(this->opaque_ptr(), capacity());
    return *this;
  }

  constexpr erasure& operator=(erasure&& right) noexcept(
      Property::is_strong_exception_guaranteed) {
    vtable_.weak_destroy(this->opaque_ptr(), capacity());
    right.vtable_.move(vtable_, right.opaque_ptr(), right.capacity(),
                       this->opaque_ptr(), capacity());
    return *this;
  }

  constexpr erasure& operator=(erasure const& right) {
    vtable_.weak_destroy(this->opaque_ptr(), capacity());
    right.vtable_.copy(vtable_, right.opaque_ptr(), right.capacity(),
                       this->opaque_ptr(), capacity());
    return *this;
  }

  template <typename OtherConfig>
  constexpr erasure&
  operator=(erasure<true, OtherConfig, Property> right) noexcept(
      Property::is_strong_exception_guaranteed) {
    vtable_.weak_destroy(this->opaque_ptr(), capacity());
    right.vtable_.move(vtable_, right.opaque_ptr(), right.capacity(),
                       this->opaque_ptr(), capacity());
    return *this;
  }

  template <typename T, typename Allocator = std::allocator<std::decay_t<T>>>
  void assign(std::false_type /*use_bool_op*/, T&& callable,
              Allocator&& allocator = {}) {
    vtable_.weak_destroy(this->opaque_ptr(), capacity());
    vtable_t::init(vtable_,
                   type_erasure::make_box(
                       std::integral_constant<bool, Config::is_copyable>{},
                       std::forward<T>(callable),
                       std::forward<Allocator>(allocator)),
                   this->opaque_ptr(), capacity());
  }

  template <typename T, typename Allocator = std::allocator<std::decay_t<T>>>
  void assign(std::true_type /*use_bool_op*/, T&& callable,
              Allocator&& allocator = {}) {
    if (bool(callable)) {
      assign(std::false_type{}, std::forward<T>(callable),
             std::forward<Allocator>(allocator));
    } else {
      operator=(nullptr);
    }
  }

  /// Returns true when the erasure doesn't hold any erased object
  constexpr bool empty() const noexcept {
    return vtable_.empty();
  }

  /// Invoke the function of the erasure at the given index
  ///
  /// We define this out of class to be able to forward the qualified
  /// erasure correctly.
  template <std::size_t Index, typename Erasure, typename... Args>
  static constexpr decltype(auto) invoke(Erasure&& erasure, Args&&... args) {
    auto const capacity = erasure.capacity();
    return erasure.vtable_.template invoke<Index>(
        std::forward<Erasure>(erasure).opaque_ptr(), capacity,
        std::forward<Args>(args)...);
  }
};

// A non owning erasure
template </*bool IsOwning = false, */ typename Config, bool IsThrowing,
          bool HasStrongExceptGuarantee, typename... Args>
class erasure<false, Config,
              property<IsThrowing, HasStrongExceptGuarantee, Args...>> {
  template <bool, typename, typename>
  friend class erasure;
  template <std::size_t, typename, typename...>
  friend class operator_impl;

  using property_t = property<IsThrowing, HasStrongExceptGuarantee, Args...>;

  using invoke_table_t = invocation_table::invoke_table<Args...>;
  typename invoke_table_t::type invoke_table_;

  /// The internal pointer to the non owned object
  data_accessor view_;

public:
  // NOLINTNEXTLINE(cppcoreguidlines-pro-type-member-init)
  constexpr erasure() noexcept
      : invoke_table_(
            invoke_table_t::template get_empty_invocation_table<IsThrowing>()),
        view_(nullptr) {
  }

  // NOLINTNEXTLINE(cppcoreguidlines-pro-type-member-init)
  constexpr erasure(std::nullptr_t) noexcept
      : invoke_table_(
            invoke_table_t::template get_empty_invocation_table<IsThrowing>()),
        view_(nullptr) {
  }

  // NOLINTNEXTLINE(cppcoreguidlines-pro-type-member-init)
  constexpr erasure(erasure&& right) noexcept
      : invoke_table_(right.invoke_table_), view_(right.view_) {
  }

  constexpr erasure(erasure const& /*right*/) = default;

  template <typename OtherConfig>
  // NOLINTNEXTLINE(cppcoreguidlines-pro-type-member-init)
  constexpr erasure(erasure<false, OtherConfig, property_t> right) noexcept
      : invoke_table_(right.invoke_table_), view_(right.view_) {
  }

  template <typename T>
  // NOLINTNEXTLINE(cppcoreguidlines-pro-type-member-init)
  constexpr erasure(std::false_type /*use_bool_op*/, T&& object)
      : invoke_table_(invoke_table_t::template get_invocation_view_table_of<
                      std::decay_t<T>>()),
        view_(address_taker<std::decay_t<T>>::take(std::forward<T>(object))) {
  }
  template <typename T>
  // NOLINTNEXTLINE(cppcoreguidlines-pro-type-member-init)
  constexpr erasure(std::true_type use_bool_op, T&& object) {
    this->assign(use_bool_op, std::forward<T>(object));
  }

  ~erasure() = default;

  constexpr erasure&
  operator=(std::nullptr_t) noexcept(HasStrongExceptGuarantee) {
    invoke_table_ =
        invoke_table_t::template get_empty_invocation_table<IsThrowing>();
    view_.ptr_ = nullptr;
    return *this;
  }

  constexpr erasure& operator=(erasure&& right) noexcept {
    invoke_table_ = right.invoke_table_;
    view_ = right.view_;
    right = nullptr;
    return *this;
  }

  constexpr erasure& operator=(erasure const& /*right*/) = default;

  template <typename OtherConfig>
  constexpr erasure&
  operator=(erasure<true, OtherConfig, property_t> right) noexcept {
    invoke_table_ = right.invoke_table_;
    view_ = right.view_;
    return *this;
  }

  template <typename T>
  constexpr void assign(std::false_type /*use_bool_op*/, T&& callable) {
    invoke_table_ = invoke_table_t::template get_invocation_view_table_of<
        std::decay_t<T>>();
    view_.ptr_ =
        address_taker<std::decay_t<T>>::take(std::forward<T>(callable));
  }
  template <typename T>
  constexpr void assign(std::true_type /*use_bool_op*/, T&& callable) {
    if (bool(callable)) {
      assign(std::false_type{}, std::forward<T>(callable));
    } else {
      operator=(nullptr);
    }
  }

  /// Returns true when the erasure doesn't hold any erased object
  constexpr bool empty() const noexcept {
    return view_.ptr_ == nullptr;
  }

  template <std::size_t Index, typename Erasure, typename... T>
  static constexpr decltype(auto) invoke(Erasure&& erasure, T&&... args) {
    auto thunk = invoke_table_t::template fetch<Index>(erasure.invoke_table_);
    return thunk(&(erasure.view_), 0UL, std::forward<T>(args)...);
  }
};
} // namespace type_erasure

/// Deduces to a true_type if the type T provides the given signature and the
/// signature is noexcept correct callable.
template <typename T, typename Signature,
          typename Trait =
              type_erasure::invocation_table::function_trait<Signature>>
struct accepts_one
    : std::integral_constant<
          bool, invocation::can_invoke<typename Trait::template callable<T>,
                                       typename Trait::arguments>::value &&
                    invocation::is_noexcept_correct<
                        Trait::is_noexcept::value,
                        typename Trait::template callable<T>,
                        typename Trait::arguments>::value> {};

/// Deduces to a true_type if the type T provides all signatures
template <typename T, typename Signatures, typename = void>
struct accepts_all : std::false_type {};
template <typename T, typename... Signatures>
struct accepts_all<
    T, identity<Signatures...>,
    void_t<std::enable_if_t<accepts_one<T, Signatures>::value>...>>
    : std::true_type {};

/// Deduces to a true_type if the type T is implementing operator bool()
/// or if the type is convertible to bool directly, this also implements an
/// optimizations for function references `void(&)()` which are can never
/// be null and for such a conversion to bool would never return false.
#if defined(FU2_HAS_NO_EMPTY_PROPAGATION)
template <typename T>
struct use_bool_op : std::false_type {};
#else
template <typename T, typename = void>
struct has_bool_op : std::false_type {};
template <typename T>
struct has_bool_op<T, void_t<decltype(bool(std::declval<T>()))>>
    : std::true_type {
#ifndef NDEBUG
  static_assert(!std::is_pointer<T>::value,
                "Missing deduction for function pointer!");
#endif
};

template <typename T>
struct use_bool_op : has_bool_op<T> {};

#define FU2_DEFINE_USE_OP_TRAIT(CONST, VOLATILE, NOEXCEPT)                     \
  template <typename Ret, typename... Args>                                    \
  struct use_bool_op<Ret (*CONST VOLATILE)(Args...) NOEXCEPT>                  \
      : std::true_type {};

FU2_DETAIL_EXPAND_CV(FU2_DEFINE_USE_OP_TRAIT)
#undef FU2_DEFINE_USE_OP_TRAIT

template <typename Ret, typename... Args>
struct use_bool_op<Ret(Args...)> : std::false_type {};

#if defined(FU2_HAS_CXX17_NOEXCEPT_FUNCTION_TYPE)
template <typename Ret, typename... Args>
struct use_bool_op<Ret(Args...) noexcept> : std::false_type {};
#endif
#endif // FU2_HAS_NO_EMPTY_PROPAGATION

template <typename Config, typename T>
struct assert_wrong_copy_assign {
  static_assert(!Config::is_owning || !Config::is_copyable ||
                    std::is_copy_constructible<std::decay_t<T>>::value,
                "Can't wrap a non copyable object into a unique function!");

  using type = void;
};

template <bool IsStrongExceptGuaranteed, typename T>
struct assert_no_strong_except_guarantee {
  static_assert(
      !IsStrongExceptGuaranteed ||
          (std::is_nothrow_move_constructible<T>::value &&
           std::is_nothrow_destructible<T>::value),
      "Can't wrap a object an object that has no strong exception guarantees "
      "if this is required by the wrapper!");

  using type = void;
};

/// SFINAES out if the given callable is not copyable correct to the left one.
template <typename LeftConfig, typename RightConfig>
using enable_if_copyable_correct_t =
    std::enable_if_t<(!LeftConfig::is_copyable || RightConfig::is_copyable)>;

template <typename LeftConfig, typename RightConfig>
using is_owning_correct =
    std::integral_constant<bool,
                           (LeftConfig::is_owning == RightConfig::is_owning)>;

/// SFINAES out if the given function2 is not owning correct to this one
template <typename LeftConfig, typename RightConfig>
using enable_if_owning_correct_t =
    std::enable_if_t<is_owning_correct<LeftConfig, RightConfig>::value>;

template <typename Config, bool IsThrowing, bool HasStrongExceptGuarantee,
          typename... Args>
class function<Config, property<IsThrowing, HasStrongExceptGuarantee, Args...>>
    : type_erasure::invocation_table::operator_impl<
          0U,
          function<Config,
                   property<IsThrowing, HasStrongExceptGuarantee, Args...>>,
          Args...> {

  template <typename, typename>
  friend class function;

  template <std::size_t, typename, typename...>
  friend class type_erasure::invocation_table::operator_impl;

  using property_t = property<IsThrowing, HasStrongExceptGuarantee, Args...>;
  using erasure_t =
      type_erasure::erasure<Config::is_owning, Config, property_t>;

  template <typename T>
  using enable_if_can_accept_all_t =
      std::enable_if_t<accepts_all<std::decay_t<T>, identity<Args...>>::value>;

  template <typename Function, typename = void>
  struct is_convertible_to_this : std::false_type {};
  template <typename RightConfig>
  struct is_convertible_to_this<
      function<RightConfig, property_t>,
      void_t<enable_if_copyable_correct_t<Config, RightConfig>,
             enable_if_owning_correct_t<Config, RightConfig>>>
      : std::true_type {};

  template <typename T>
  using enable_if_not_convertible_to_this =
      std::enable_if_t<!is_convertible_to_this<std::decay_t<T>>::value>;

  template <typename T>
  using enable_if_owning_t =
      std::enable_if_t<std::is_same<T, T>::value && Config::is_owning>;

  template <typename T>
  using assert_wrong_copy_assign_t =
      typename assert_wrong_copy_assign<Config, std::decay_t<T>>::type;

  template <typename T>
  using assert_no_strong_except_guarantee_t =
      typename assert_no_strong_except_guarantee<HasStrongExceptGuarantee,
                                                 std::decay_t<T>>::type;

  erasure_t erasure_;

public:
  /// Default constructor which empty constructs the function
  function() = default;
  ~function() = default;

  explicit constexpr function(function const& /*right*/) = default;
  explicit constexpr function(function&& /*right*/) = default;

  /// Copy construction from another copyable function
  template <typename RightConfig,
            std::enable_if_t<RightConfig::is_copyable>* = nullptr,
            enable_if_copyable_correct_t<Config, RightConfig>* = nullptr,
            enable_if_owning_correct_t<Config, RightConfig>* = nullptr>
  constexpr function(function<RightConfig, property_t> const& right)
      : erasure_(right.erasure_) {
  }

  /// Move construction from another function
  template <typename RightConfig,
            enable_if_copyable_correct_t<Config, RightConfig>* = nullptr,
            enable_if_owning_correct_t<Config, RightConfig>* = nullptr>
  constexpr function(function<RightConfig, property_t>&& right)
      : erasure_(std::move(right.erasure_)) {
  }

  /// Construction from a callable object which overloads the `()` operator
  template <typename T, //
            enable_if_not_convertible_to_this<T>* = nullptr,
            enable_if_can_accept_all_t<T>* = nullptr,
            assert_wrong_copy_assign_t<T>* = nullptr,
            assert_no_strong_except_guarantee_t<T>* = nullptr>
  constexpr function(T&& callable)
      : erasure_(use_bool_op<unrefcv_t<T>>{}, std::forward<T>(callable)) {
  }
  template <typename T, typename Allocator, //
            enable_if_not_convertible_to_this<T>* = nullptr,
            enable_if_can_accept_all_t<T>* = nullptr,
            enable_if_owning_t<T>* = nullptr,
            assert_wrong_copy_assign_t<T>* = nullptr,
            assert_no_strong_except_guarantee_t<T>* = nullptr>
  constexpr function(T&& callable, Allocator&& allocator)
      : erasure_(use_bool_op<unrefcv_t<T>>{}, std::forward<T>(callable),
                 std::forward<Allocator>(allocator)) {
  }

  /// Empty constructs the function
  constexpr function(std::nullptr_t np) : erasure_(np) {
  }

  function& operator=(function const& /*right*/) = default;
  function& operator=(function&& /*right*/) = default;

  /// Copy assigning from another copyable function
  template <typename RightConfig,
            std::enable_if_t<RightConfig::is_copyable>* = nullptr,
            enable_if_copyable_correct_t<Config, RightConfig>* = nullptr,
            enable_if_owning_correct_t<Config, RightConfig>* = nullptr>
  function& operator=(function<RightConfig, property_t> const& right) {
    erasure_ = right.erasure_;
    return *this;
  }

  /// Move assigning from another function
  template <typename RightConfig,
            enable_if_copyable_correct_t<Config, RightConfig>* = nullptr,
            enable_if_owning_correct_t<Config, RightConfig>* = nullptr>
  function& operator=(function<RightConfig, property_t>&& right) {
    erasure_ = std::move(right.erasure_);
    return *this;
  }

  /// Move assigning from a callable object
  template <typename T, // ...
            enable_if_not_convertible_to_this<T>* = nullptr,
            enable_if_can_accept_all_t<T>* = nullptr,
            assert_wrong_copy_assign_t<T>* = nullptr,
            assert_no_strong_except_guarantee_t<T>* = nullptr>
  function& operator=(T&& callable) {
    erasure_.assign(use_bool_op<unrefcv_t<T>>{}, std::forward<T>(callable));
    return *this;
  }

  /// Clears the function
  function& operator=(std::nullptr_t np) {
    erasure_ = np;
    return *this;
  }

  /// Returns true when the function is empty
  bool empty() const noexcept {
    return erasure_.empty();
  }

  /// Returns true when the function isn't empty
  explicit operator bool() const noexcept {
    return !empty();
  }

  /// Assigns a new target with an optional allocator
  template <typename T, typename Allocator = std::allocator<std::decay_t<T>>,
            enable_if_not_convertible_to_this<T>* = nullptr,
            enable_if_can_accept_all_t<T>* = nullptr,
            assert_wrong_copy_assign_t<T>* = nullptr,
            assert_no_strong_except_guarantee_t<T>* = nullptr>
  void assign(T&& callable, Allocator&& allocator = Allocator{}) {
    erasure_.assign(use_bool_op<unrefcv_t<T>>{}, std::forward<T>(callable),
                    std::forward<Allocator>(allocator));
  }

  /// Swaps this function with the given function
  void swap(function& other) noexcept(HasStrongExceptGuarantee) {
    if (&other == this) {
      return;
    }

    function cache = std::move(other);
    other = std::move(*this);
    *this = std::move(cache);
  }

  /// Swaps the left function with the right one
  friend void swap(function& left,
                   function& right) noexcept(HasStrongExceptGuarantee) {
    left.swap(right);
  }

  /// Calls the wrapped callable object
  using type_erasure::invocation_table::operator_impl<
      0U, function<Config, property_t>, Args...>::operator();
};

template <typename Config, typename Property>
bool operator==(function<Config, Property> const& f, std::nullptr_t) {
  return !bool(f);
}

template <typename Config, typename Property>
bool operator!=(function<Config, Property> const& f, std::nullptr_t) {
  return bool(f);
}

template <typename Config, typename Property>
bool operator==(std::nullptr_t, function<Config, Property> const& f) {
  return !bool(f);
}

template <typename Config, typename Property>
bool operator!=(std::nullptr_t, function<Config, Property> const& f) {
  return bool(f);
}

// Default intended object size of the function
using object_size = std::integral_constant<std::size_t, 32U>;
} // namespace detail
} // namespace abi_400

/// Can be passed to function_base as template argument which causes
/// the internal small buffer to be sized according to the given size,
/// and aligned with the given alignment.
template <std::size_t Capacity,
          std::size_t Alignment = alignof(std::max_align_t)>
struct capacity_fixed {
  static constexpr std::size_t capacity = Capacity;
  static constexpr std::size_t alignment = Alignment;
};

/// Default capacity for small functor optimization
struct capacity_default
    : capacity_fixed<detail::object_size::value - (2 * sizeof(void*))> {};

/// Can be passed to function_base as template argument which causes
/// the internal small buffer to be removed from the callable wrapper.
/// The owning function_base will then allocate memory for every object
/// it applies a type erasure on.
struct capacity_none : capacity_fixed<0UL> {};

/// Can be passed to function_base as template argument which causes
/// the internal small buffer to be sized such that it can hold
/// the given object without allocating memory for an applied type erasure.
template <typename T>
struct capacity_can_hold {
  static constexpr std::size_t capacity = sizeof(T);
  static constexpr std::size_t alignment = alignof(T);
};

/// An adaptable function wrapper base for arbitrary functional types.
///
/// \tparam IsOwning Is true when the type erasure shall be owning the object.
///
/// \tparam IsCopyable Defines whether the function is copyable or not
///
/// \tparam Capacity Defines the internal capacity of the function
///                  for small functor optimization.
///                  The size of the whole function object will be the capacity
///                  plus the size of two pointers. If the capacity is zero,
///                  the size will increase through one additional pointer
///                  so the whole object has the size of 3 * sizeof(void*).
///                  The type which is passed to the Capacity template parameter
///                  shall provide a capacity and alignment member which
///                  looks like the following example:
/// ```cpp
/// struct my_capacity {
///   static constexpr std::size_t capacity = sizeof(my_type);
///   static constexpr std::size_t alignment = alignof(my_type);
/// };
/// ```
///
/// \tparam IsThrowing Defines whether the function throws an exception on
///                    empty function call, `std::abort` is called otherwise.
///
/// \tparam HasStrongExceptGuarantee Defines whether all objects satisfy the
///                                  strong exception guarantees,
///                                  which means the function type will satisfy
///                                  the strong exception guarantees too.
///
/// \tparam Signatures Defines the signature of the callable wrapper
///
template <bool IsOwning, bool IsCopyable, typename Capacity, bool IsThrowing,
          bool HasStrongExceptGuarantee, typename... Signatures>
using function_base = detail::function<
    detail::config<IsOwning, IsCopyable, Capacity>,
    detail::property<IsThrowing, HasStrongExceptGuarantee, Signatures...>>;

/// An owning copyable function wrapper for arbitrary callable types.
template <typename... Signatures>
using function = function_base<true, true, capacity_default, //
                               true, false, Signatures...>;

/// An owning non copyable function wrapper for arbitrary callable types.
template <typename... Signatures>
using unique_function = function_base<true, false, capacity_default, //
                                      true, false, Signatures...>;

/// A non owning copyable function wrapper for arbitrary callable types.
template <typename... Signatures>
using function_view = function_base<false, true, capacity_default, //
                                    true, false, Signatures...>;

#if !defined(FU2_HAS_DISABLED_EXCEPTIONS)
/// Exception type that is thrown when invoking empty function objects
/// and exception support isn't disabled.
///
/// Exception support is enabled if
/// the template parameter 'Throwing' is set to true (default).
///
/// This type will default to std::bad_function_call if the
/// functional header is used, otherwise the library provides its own type.
///
/// You may disable the inclusion of the functional header
/// through defining `FU2_WITH_NO_FUNCTIONAL_HEADER`.
///
using detail::type_erasure::invocation_table::bad_function_call;
#endif

/// Returns a callable object, which unifies all callable objects
/// that were passed to this function.
///
///   ```cpp
///   auto overloaded = fu2::overload([](std::true_type) { return true; },
///                                   [](std::false_type) { return false; });
///   ```
///
/// \param  callables A pack of callable objects with arbitrary signatures.
///
/// \returns          A callable object which exposes the
///
template <typename... T>
constexpr auto overload(T&&... callables) {
  return detail::overloading::overload(std::forward<T>(callables)...);
}
} // namespace fu2

#undef FU2_DETAIL_EXPAND_QUALIFIERS
#undef FU2_DETAIL_EXPAND_QUALIFIERS_NOEXCEPT
#undef FU2_DETAIL_EXPAND_CV
#undef FU2_DETAIL_EXPAND_CV_NOEXCEPT
#undef FU2_DETAIL_UNREACHABLE_INTRINSIC
#undef FU2_DETAIL_UNREACHABLE_INTRINSIC
#undef FU2_DETAIL_TRAP

#endif // FU2_INCLUDED_FUNCTION2_HPP_
