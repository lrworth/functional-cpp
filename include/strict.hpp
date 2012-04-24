
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

struct strict {

  struct match_error : std::logic_error {
    match_error(const std::string & m)
      : std::logic_error("strict::sum: index out of range: " + m) {}
  };

private:

  // Sum value store, indexed by the type with which the sum is initialised.

  struct header {
    header(int i) : index(i) {}
    int index;
  };

  template <typename T>
  struct container : header {

    template <typename... Args>
    container(int index, Args &&... args)
      : header(index), value(std::forward<Args>(args)...) {}

    T value;

  };

  typedef std::shared_ptr<header> ptr_type;

  template <typename T>
  static T& value(const ptr_type & ptr) {
    return static_cast<container<T>*>(ptr.get())->value;
  }

  // Tag type for function template dispatch.

  template <typename T>
  struct tag {};

  // Construction of the typed value store.

  template <typename T, typename... TZ>
  struct construct {

    template <typename... Args>
    ptr_type operator()(int index, tag<T> t, Args &&... args) const {
      return std::make_shared<container<T>>(index, std::forward<Args>(args)...);
    }

    template <typename U, typename... Args>
    ptr_type operator()(int index, tag<U> t, Args &&... args) const {
      return construct<TZ...>()(index+1, t, std::forward<Args>(args)...);
    }

  };

  // Static check that construction of a sum with particular type is allowed.

  template <typename U, typename... TZ>
  struct check_cons : std::false_type {};

  template <typename T, typename... TZ>
  struct check_cons<T,T,TZ...> : std::true_type {};

  template <typename U, typename T, typename... TZ>
  struct check_cons<U,T,TZ...> : check_cons<U,TZ...> {};

  // Static check that a type is callable with given argument types.

  template <typename Fun, typename... Args>
  class callable {
    template <typename F> static char
      test(decltype(std::declval<F>()(std::declval<Args>()...)) *);
    template <typename F> static int test(...);
  public:
    static const bool value = sizeof(test<Fun>(nullptr)) == sizeof(char);
  };

  // Find the first match-function that accepts the given value type.

  template <typename R, typename T>
  struct eliminator {

    template <typename I, typename Fun, typename... Funs>
    typename std::enable_if<callable<Fun,T&>::value,R>::type
    match(I i, const ptr_type & ptr, Fun && fun, Funs &&... funs) const {
      return fun(value<T>(ptr));
    }

    template <typename I, typename Fun, typename... Funs>
    typename std::enable_if<callable<Fun>::value,R>::type
    match(I i, const ptr_type & ptr, Fun && fun, Funs &&... funs) const {
      return fun();
    }

    template <typename I, typename Fun, typename... Funs>
    typename std::enable_if<!callable<Fun,T&>::value && !callable<Fun>::value,R>::type
    match(I i, const ptr_type & ptr, Fun && fun, Funs &&... funs) const {
      return match(i, ptr, std::forward<Funs>(funs)...);
    }

    template <typename I>
    R match(I i, const ptr_type & ptr) const {
      static_assert(I::value < 0, "incomplete match");
    }

  };

  // Determine the constructed type of a sum value, then dispatch.

  template <typename R, typename... TZ>
  struct select;

  template <typename R>
  struct select<R> {

    template <typename... Funs>
    R operator()(const ptr_type & ptr, int index, Funs &&... funs) const {
      throw match_error(std::to_string(ptr->index) + " " + std::to_string(index));
    }

  };

  template <typename R, typename T, typename... TZ>
  struct select<R,T,TZ...> {

    typedef std::integral_constant<int,0> i;

    template <typename... Funs>
    R operator()(const ptr_type & ptr, int index, Funs &&... funs) const {
      return ptr->index == index
        ? eliminator<R,T>().match(i(), ptr, std::forward<Funs>(funs)...)
	: select<R,TZ...>()(ptr, index+1, std::forward<Funs>(funs)...);
    }

  };

public:

  // The public sum type itself.

  template <typename... TZ>
  class sum {

    ptr_type ptr;
    sum(ptr_type && ptr) : ptr(std::move(ptr)) {}

  public:

    // Construct a sum from an allowed value type.

    template <
      typename U,
      typename std::enable_if<check_cons<U,TZ...>::value>::type* = nullptr
      >
    sum(U && u)
      : ptr(construct<TZ...>()(0, tag<U>(), std::forward<U>(u))) {}

    // Alternative factory function, constructing the value in-place.

    template <typename U, typename... Args>
    static typename std::enable_if<check_cons<U,TZ...>::value, sum>::type
    cons(Args &&... args) {
      return construct<TZ...>()(0, tag<U>(), std::forward<Args>(args)...);
    }

    // Inspect a sum, dispatching to the first matching function.

    template <typename R, typename... Funs>
    R match(Funs &&... funs) const {
      return select<R,TZ...>()(ptr, 0, std::forward<Funs>(funs)...);
    }

    // The usual suspects.

    sum(const sum & s) = default;
    sum(sum && s) = default;

    sum() = delete;

    sum & operator = (const sum &) = default;
    sum & operator = (sum &&) = default;

  };

};
