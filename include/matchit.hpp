/*
 *  Copyright (c) 2021-2022 Bowen Fu
 *  Distributed Under The Apache-2.0 License
 */

#ifndef MATCHIT_H
#define MATCHIT_H

#ifndef MATCHIT_CORE_H
  #define MATCHIT_CORE_H

  #include <algorithm>
  #include <cstdint>
  #include <tuple>

namespace matchit {
  // NOLINTBEGIN(*-identifier-naming)
  namespace impl {
    template <typename Value, bool byRef>
    class ValueType {
     public:
      using ValueT = Value;
    };

    template <typename Value>
    class ValueType<Value, true> {
     public:
      using ValueT = Value&&;
    };

    template <typename Value, typename... Patterns>
    constexpr auto matchPatterns(Value&& value, const Patterns&... patterns);

    template <typename Value, bool byRef>
    class MatchHelper {
     private:
      using ValueT = typename ValueType<Value, byRef>::ValueT;
      ValueT mValue;
      using ValueRefT = ValueT&&;

     public:
      template <typename V>
      constexpr explicit MatchHelper(V&& value)
        : mValue { std::forward<V>(value) } {}
      template <typename... PatternPair>
      constexpr auto operator()(const PatternPair&... patterns) {
        return matchPatterns(std::forward<ValueRefT>(mValue), patterns...);
      }
    };

    template <typename Value>
    constexpr auto match(Value&& value) {
      return MatchHelper<Value, true> { std::forward<Value>(value) };
    }

    template <typename First, typename... Values>
    constexpr auto match(First&& first, Values&&... values) {
      auto result = std::forward_as_tuple(std::forward<First>(first), std::forward<Values>(values)...);
      return MatchHelper<decltype(result), false> { std::forward<decltype(result)>(result) };
    }
  } // namespace impl

  // export symbols
  using impl::match;

} // namespace matchit
#endif // MATCHIT_CORE_H
#ifndef MATCHIT_EXPRESSION_H
  #define MATCHIT_EXPRESSION_H

  #include <type_traits>

namespace matchit {
  namespace impl {
    template <typename T>
    class Nullary : public T {
     public:
      using T::operator();
    };

    template <typename T>
    constexpr auto nullary(const T& t) {
      return Nullary<T> { t };
    }

    template <typename T>
    class Id;
    template <typename T>
    constexpr auto expr(Id<T>& id) {
      return nullary([&] { return *id; });
    }

    template <typename T>
    constexpr auto expr(const T& v) {
      return nullary([&] { return v; });
    }

    template <typename T>
    constexpr auto toNullary(T&& v) {
      if constexpr (std::is_invocable_v<std::decay_t<T>>) {
        return v;
      } else {
        return expr(v);
      }
    }

    // for constant
    template <typename T>
    class EvalTraits {
     public:
      template <typename... Args>
      constexpr static decltype(auto) evalImpl(const T& v, const Args&...) {
        return v;
      }
    };

    template <typename T>
    class EvalTraits<Nullary<T>> {
     public:
      constexpr static decltype(auto) evalImpl(const Nullary<T>& e) {
        return e();
      }
    };

    // Only allowed in nullary
    template <typename T>
    class EvalTraits<Id<T>> {
     public:
      constexpr static decltype(auto) evalImpl(const Id<T>& id) {
        return *const_cast<Id<T>&>(id);
      }
    };

    template <typename Pred>
    class Meet;

    // Unary is an alias of Meet.
    template <typename T>
    using Unary = Meet<T>;

    template <typename T>
    class EvalTraits<Unary<T>> {
     public:
      template <typename Arg>
      constexpr static decltype(auto) evalImpl(const Unary<T>& e, const Arg& arg) {
        return e(arg);
      }
    };

    class Wildcard;
    template <>
    class EvalTraits<Wildcard> {
     public:
      template <typename Arg>
      constexpr static decltype(auto) evalImpl(const Wildcard&, const Arg& arg) {
        return arg;
      }
    };

    template <typename T, typename... Args>
    constexpr decltype(auto) evaluate_(const T& t, const Args&... args) {
      return EvalTraits<T>::evalImpl(t, args...);
    }

    template <typename T>
    class IsNullaryOrId : public std::false_type {};

    template <typename T>
    class IsNullaryOrId<Id<T>> : public std::true_type {};

    template <typename T>
    class IsNullaryOrId<Nullary<T>> : public std::true_type {};

    template <typename T>
    constexpr auto isNullaryOrIdV = IsNullaryOrId<std::decay_t<T>>::value;

  #define UN_OP_FOR_NULLARY(op)                                             \
    template <typename T, std::enable_if_t<isNullaryOrIdV<T>, bool> = true> \
    constexpr auto operator op(T const& t) {                                \
      return nullary([&] { return op evaluate_(t); });                      \
    }

  #define BIN_OP_FOR_NULLARY(op)                                                                             \
    template <typename T, typename U, std::enable_if_t<isNullaryOrIdV<T> || isNullaryOrIdV<U>, bool> = true> \
    constexpr auto operator op(T const& t, U const& u) {                                                     \
      return nullary([&] { return evaluate_(t) op evaluate_(u); });                                          \
    }

    // ADL will find these operators.
    UN_OP_FOR_NULLARY(!)
    UN_OP_FOR_NULLARY(-)

  #undef UN_OP_FOR_NULLARY

    BIN_OP_FOR_NULLARY(+)
    BIN_OP_FOR_NULLARY(-)
    BIN_OP_FOR_NULLARY(*)
    BIN_OP_FOR_NULLARY(/)
    BIN_OP_FOR_NULLARY(%)
    BIN_OP_FOR_NULLARY(<)
    BIN_OP_FOR_NULLARY(<=)
    BIN_OP_FOR_NULLARY(==)
    BIN_OP_FOR_NULLARY(!=)
    BIN_OP_FOR_NULLARY(>=)
    BIN_OP_FOR_NULLARY(>)
    BIN_OP_FOR_NULLARY(||)
    BIN_OP_FOR_NULLARY(&&)
    BIN_OP_FOR_NULLARY(^)

  #undef BIN_OP_FOR_NULLARY

    // Unary
    template <typename T>
    class IsUnaryOrWildcard : public std::false_type {};

    template <>
    class IsUnaryOrWildcard<Wildcard> : public std::true_type {};

    template <typename T>
    class IsUnaryOrWildcard<Unary<T>> : public std::true_type {};

    template <typename T>
    constexpr auto isUnaryOrWildcardV = IsUnaryOrWildcard<std::decay_t<T>>::value;

    // unary is an alias of meet.
    template <typename T>
    constexpr auto unary(T&& t) {
      return meet(std::forward<T>(t));
    }

  #define UN_OP_FOR_UNARY(op)                                                   \
    template <typename T, std::enable_if_t<isUnaryOrWildcardV<T>, bool> = true> \
    constexpr auto operator op(T const& t) {                                    \
      return unary([&](auto&& arg) constexpr { return op evaluate_(t, arg); }); \
    }

  #define BIN_OP_FOR_UNARY(op)                                                                                       \
    template <typename T, typename U, std::enable_if_t<isUnaryOrWildcardV<T> || isUnaryOrWildcardV<U>, bool> = true> \
    constexpr auto operator op(T const& t, U const& u) {                                                             \
      return unary([&](auto&& arg) constexpr { return evaluate_(t, arg) op evaluate_(u, arg); });                    \
    }

    UN_OP_FOR_UNARY(!)
    UN_OP_FOR_UNARY(-)

  #undef UN_OP_FOR_UNARY

    BIN_OP_FOR_UNARY(+)
    BIN_OP_FOR_UNARY(-)
    BIN_OP_FOR_UNARY(*)
    BIN_OP_FOR_UNARY(/)
    BIN_OP_FOR_UNARY(%)
    BIN_OP_FOR_UNARY(<)
    BIN_OP_FOR_UNARY(<=)
    BIN_OP_FOR_UNARY(==)
    BIN_OP_FOR_UNARY(!=)
    BIN_OP_FOR_UNARY(>=)
    BIN_OP_FOR_UNARY(>)
    BIN_OP_FOR_UNARY(||)
    BIN_OP_FOR_UNARY(&&)
    BIN_OP_FOR_UNARY(^)

  #undef BIN_OP_FOR_UNARY

  } // namespace impl
  using impl::expr;
} // namespace matchit

#endif // MATCHIT_EXPRESSION_H
#ifndef MATCHIT_PATTERNS_H
  #define MATCHIT_PATTERNS_H

  #include <array>
  #include <cassert>
  #include <functional>
  #include <stdexcept>
  #include <variant>

  #if !defined(NO_SCALAR_REFERENCES_USED_IN_PATTERNS)
    #define NO_SCALAR_REFERENCES_USED_IN_PATTERNS 0
  #endif // !defined(NO_SCALAR_REFERENCES_USED_IN_PATTERNS)

namespace matchit {
  namespace impl {
    template <typename I, typename S = I>
    class Subrange {
      I mBegin;
      S mEnd;

     public:
      constexpr Subrange(const I begin, const S end)
        : mBegin { begin }, mEnd { end } {}

      constexpr Subrange(const Subrange& other)
        : mBegin { other.begin() }, mEnd { other.end() } {}

      Subrange& operator=(const Subrange& other) {
        mBegin = other.begin();
        mEnd   = other.end();
        return *this;
      }

      size_t size() const {
        return static_cast<size_t>(std::distance(mBegin, mEnd));
      }
      auto begin() const {
        return mBegin;
      }
      auto end() const {
        return mEnd;
      }
    };

    template <typename I, typename S>
    constexpr auto makeSubrange(I begin, S end) {
      return Subrange<I, S> { begin, end };
    }

    template <typename RangeType>
    class IterUnderlyingType {
     public:
      using beginT = decltype(std::begin(std::declval<RangeType&>()));
      using endT   = decltype(std::end(std::declval<RangeType&>()));
    };

    // force array iterators fallback to pointers.
    template <typename ElemT, size_t size>
    class IterUnderlyingType<std::array<ElemT, size>> {
     public:
      using beginT = decltype(&*std::begin(std::declval<std::array<ElemT, size>&>()));
      using endT   = beginT;
    };

    // force array iterators fallback to pointers.
    template <typename ElemT, size_t size>
    class IterUnderlyingType<const std::array<ElemT, size>> {
     public:
      using beginT = decltype(&*std::begin(std::declval<const std::array<ElemT, size>&>()));
      using endT   = beginT;
    };

    template <typename RangeType>
    using SubrangeT =
      Subrange<typename IterUnderlyingType<RangeType>::beginT, typename IterUnderlyingType<RangeType>::endT>;

    template <typename I, typename S>
    bool operator==(const Subrange<I, S>& lhs, const Subrange<I, S>& rhs) {
      using std::operator==;
      return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    template <typename K1, typename V1, typename K2, typename V2>
    auto operator==(const std::pair<K1, V1>& t, const std::pair<K2, V2>& u) {
      return t.first == u.first && t.second == u.second;
    }

    template <typename T, typename... Ts>
    class WithinTypes {
     public:
      constexpr static auto value = (std::is_same_v<T, Ts> || ...);
    };

    template <typename T, typename Tuple>
    class PrependUnique;

    template <typename T, typename... Ts>
    class PrependUnique<T, std::tuple<Ts...>> {
      constexpr static auto unique = !WithinTypes<T, Ts...>::value;

     public:
      using type = std::conditional_t<unique, std::tuple<T, Ts...>, std::tuple<Ts...>>;
    };

    template <typename T, typename Tuple>
    using PrependUniqueT = typename PrependUnique<T, Tuple>::type;

    template <typename Tuple>
    class Unique;

    template <typename... Ts>
    using UniqueT = typename Unique<std::tuple<Ts...>>::type;

    template <>
    class Unique<std::tuple<>> {
     public:
      using type = std::tuple<>;
    };

    template <typename T, typename... Ts>
    class Unique<std::tuple<T, Ts...>> {
     public:
      using type = PrependUniqueT<T, UniqueT<Ts...>>;
    };

    static_assert(std::is_same_v<std::tuple<int32_t>, UniqueT<int32_t, int32_t>>);
    static_assert(std::is_same_v<std::tuple<std::tuple<>, int32_t>, UniqueT<int32_t, std::tuple<>, int32_t>>);

    using std::get;

    namespace detail {
      template <std::size_t start, class Tuple, std::size_t... I>
      constexpr decltype(auto) subtupleImpl(Tuple&& t, std::index_sequence<I...>) {
        return std::forward_as_tuple(get<start + I>(std::forward<Tuple>(t))...);
      }
    } // namespace detail

    // [start, end)
    template <std::size_t start, std::size_t end, class Tuple>
    constexpr decltype(auto) subtuple(Tuple&& t) {
      constexpr auto tupleSize = std::tuple_size_v<std::remove_reference_t<Tuple>>;
      static_assert(start <= end);
      static_assert(end <= tupleSize);
      return detail::subtupleImpl<start>(std::forward<Tuple>(t), std::make_index_sequence<end - start> {});
    }

    template <std::size_t start, class Tuple>
    constexpr decltype(auto) drop(Tuple&& t) {
      constexpr auto tupleSize = std::tuple_size_v<std::remove_reference_t<Tuple>>;
      static_assert(start <= tupleSize);
      return subtuple<start, tupleSize>(std::forward<Tuple>(t));
    }

    template <std::size_t len, class Tuple>
    constexpr decltype(auto) take(Tuple&& t) {
      constexpr auto tupleSize = std::tuple_size_v<std::remove_reference_t<Tuple>>;
      static_assert(len <= tupleSize);
      return subtuple<0, len>(std::forward<Tuple>(t));
    }

    template <class F, class Tuple>
    constexpr decltype(auto) apply_(F&& f, Tuple&& t) {
      return std::apply(std::forward<F>(f), drop<0>(std::forward<Tuple>(t)));
    }

    // as constexpr
    template <class F, class... Args>
    constexpr std::invoke_result_t<F, Args...>
    invoke_(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>) {
      return std::apply(std::forward<F>(f), std::forward_as_tuple(std::forward<Args>(args)...));
    }

    template <class T>
    struct decayArray {
     private:
      typedef std::remove_reference_t<T> U;

     public:
      using type = std::conditional_t<std::is_array_v<U>, std::remove_extent_t<U>*, T>;
    };

    template <class T>
    using decayArrayT = typename decayArray<T>::type;

    static_assert(std::is_same_v<decayArrayT<int32_t[]>, int32_t*>);
    static_assert(std::is_same_v<decayArrayT<const int32_t[]>, const int32_t*>);
    static_assert(std::is_same_v<decayArrayT<const int32_t&>, const int32_t&>);

    template <typename T>
    struct AddConstToPointer {
      using type =
        std::conditional_t<!std::is_pointer_v<T>, T, std::add_pointer_t<std::add_const_t<std::remove_pointer_t<T>>>>;
    };
    template <typename T>
    using AddConstToPointerT = typename AddConstToPointer<T>::type;

    static_assert(std::is_same_v<AddConstToPointerT<void*>, const void*>);
    static_assert(std::is_same_v<AddConstToPointerT<int32_t>, int32_t>);

    template <typename Pattern>
    using InternalPatternT = std::remove_reference_t<AddConstToPointerT<decayArrayT<Pattern>>>;

    template <typename Pattern>
    class PatternTraits;

    template <typename... PatternPairs>
    class PatternPairsRetType {
     public:
      using RetType = std::common_type_t<typename PatternPairs::RetType...>;
    };

    enum class IdProcess : int32_t { kCANCEL,
                                     kCONFIRM };

    template <typename Pattern>
    constexpr void processId(const Pattern& pattern, int32_t depth, IdProcess idProcess) {
      PatternTraits<Pattern>::processIdImpl(pattern, depth, idProcess);
    }

    template <typename Tuple>
    class Variant;

    template <typename T, typename... Ts>
    class Variant<std::tuple<T, Ts...>> {
     public:
      using type = std::variant<std::monostate, T, Ts...>;
    };

    template <typename... Ts>
    using UniqVariant = typename Variant<UniqueT<Ts...>>::type;

    template <typename... Ts>
    class Context {
      using ElementT   = UniqVariant<Ts...>;
      using ContainerT = std::array<ElementT, sizeof...(Ts)>;
      ContainerT mMemHolder;
      size_t     mSize = 0;

     public:
      template <typename T>
      constexpr void emplace_back(T&& t) {
        mMemHolder[mSize] = std::forward<T>(t);
        ++mSize;
      }
      constexpr auto back() -> ElementT& {
        return mMemHolder[mSize - 1];
      }
    };

    template <>
    class Context<> {};

    template <typename T>
    class ContextTrait;

    template <typename... Ts>
    class ContextTrait<std::tuple<Ts...>> {
     public:
      using ContextT = Context<Ts...>;
    };

    template <typename Value, typename Pattern, typename ConctextT>
    constexpr auto matchPattern(Value&& value, const Pattern& pattern, int32_t depth, ConctextT& context) {
      const auto result  = PatternTraits<Pattern>::matchPatternImpl(std::forward<Value>(value), pattern, depth, context);
      const auto process = result ? IdProcess::kCONFIRM : IdProcess::kCANCEL;
      processId(pattern, depth, process);
      return result;
    }

    template <typename Pattern, typename Func>
    class PatternPair {
     public:
      using RetType  = std::invoke_result_t<Func>;
      using PatternT = Pattern;

      constexpr PatternPair(const Pattern& pattern, const Func& func)
        : mPattern { pattern }, mHandler { func } {}
      template <typename Value, typename ContextT>
      constexpr bool matchValue(Value&& value, ContextT& context) const {
        return matchPattern(std::forward<Value>(value), mPattern, /*depth*/ 0, context);
      }
      constexpr auto execute() const {
        return mHandler();
      }

     private:
      const Pattern                                                         mPattern;
      std::conditional_t<std::is_function_v<Func>, const Func&, const Func> mHandler;
    };

    template <typename Pattern, typename Pred>
    class PostCheck;

    template <typename Pred>
    class When {
     public:
      Pred mPred;
    };

    template <typename Pred>
    constexpr auto when(Pred&& pred) {
      auto p = toNullary(pred);
      return When<decltype(p)> { p };
    }

    template <typename Pattern>
    class PatternHelper {
     public:
      constexpr explicit PatternHelper(const Pattern& pattern)
        : mPattern { pattern } {}
      template <typename Func>
      constexpr auto operator=(Func&& func) {
        auto f = toNullary(func);
        return PatternPair<Pattern, decltype(f)> { mPattern, f };
      }
      template <typename Pred>
      constexpr auto operator|(const When<Pred>& w) {
        return PatternHelper<PostCheck<Pattern, Pred>>(PostCheck(mPattern, w.mPred));
      }

     private:
      const Pattern mPattern;
    };

    template <typename... Patterns>
    class Ds;

    template <typename... Patterns>
    constexpr auto ds(const Patterns&... patterns) -> Ds<Patterns...>;

    template <typename Pattern>
    class OooBinder;

    class PatternPipable {
     public:
      template <typename Pattern>
      // ReSharper disable once CppDFAUnreachableFunctionCall
      constexpr auto operator|(const Pattern& p) const {
        return PatternHelper<Pattern> { p };
      }

      template <typename T>
      constexpr auto operator|(const T* p) const {
        return PatternHelper<const T*> { p };
      }

      template <typename Pattern>
      constexpr auto operator|(const OooBinder<Pattern>& p) const {
        return operator|(ds(p));
      }
    };

    constexpr PatternPipable is {};

    template <typename Pattern>
    class PatternTraits {
     public:
      template <typename Value>
      using AppResultTuple = std::tuple<>;

      constexpr static auto nbIdV = 0;

      template <typename Value, typename ContextT>
      constexpr static auto
      matchPatternImpl(Value&& value, const Pattern& pattern, int32_t /* depth */, ContextT& /*context*/) {
        return pattern == std::forward<Value>(value);
      }
      constexpr static void processIdImpl(const Pattern&, int32_t /*depth*/, IdProcess) {}
    };

    class Wildcard {};

    constexpr Wildcard _;

    template <>
    class PatternTraits<Wildcard> {
      using Pattern = Wildcard;

     public:
      template <typename Value>
      using AppResultTuple = std::tuple<>;

      constexpr static auto nbIdV = 0;

      template <typename Value, typename ContextT>
      constexpr static bool matchPatternImpl(Value&&, const Pattern&, int32_t, ContextT&) {
        return true;
      }
      constexpr static void processIdImpl(const Pattern&, int32_t /*depth*/, IdProcess) {}
    };

    template <typename... Patterns>
    class Or {
     public:
      constexpr explicit Or(const Patterns&... patterns)
        : mPatterns { patterns... } {}
      constexpr const auto& patterns() const {
        return mPatterns;
      }

     private:
      std::tuple<InternalPatternT<Patterns>...> mPatterns;
    };

    template <typename... Patterns>
    constexpr auto or_(const Patterns&... patterns) {
      return Or<Patterns...> { patterns... };
    }

    template <typename... Patterns>
    class PatternTraits<Or<Patterns...>> {
     public:
      template <typename Value>
      using AppResultTuple =
        decltype(std::tuple_cat(typename PatternTraits<Patterns>::template AppResultTuple<Value> {}...));

      constexpr static auto nbIdV = (PatternTraits<Patterns>::nbIdV + ... + 0);

      template <typename Value, typename ContextT>
      constexpr static auto
      matchPatternImpl(Value&& value, const Or<Patterns...>& orPat, int32_t depth, ContextT& context) {
        constexpr auto patSize = sizeof...(Patterns);
        return std::apply(
                 [&value, depth, &context](const auto&... patterns) {
                   return (matchPattern(value, patterns, depth + 1, context) || ...);
                 },
                 take<patSize - 1>(orPat.patterns())
               ) ||
          matchPattern(std::forward<Value>(value), get<patSize - 1>(orPat.patterns()), depth + 1, context);
      }
      constexpr static void processIdImpl(const Or<Patterns...>& orPat, int32_t depth, IdProcess idProcess) {
        return std::apply(
          [depth, idProcess](const Patterns&... patterns) { return (processId(patterns, depth, idProcess), ...); },
          orPat.patterns()
        );
      }
    };

    template <typename Pred>
    class Meet : public Pred {
     public:
      using Pred::operator();
    };

    template <typename Pred>
    constexpr auto meet(const Pred& pred) {
      return Meet<Pred> { pred };
    }

    template <typename Pred>
    class PatternTraits<Meet<Pred>> {
     public:
      template <typename Value>
      using AppResultTuple = std::tuple<>;

      constexpr static auto nbIdV = 0;

      template <typename Value, typename ContextT>
      constexpr static auto matchPatternImpl(Value&& value, const Meet<Pred>& meetPat, int32_t /* depth */, ContextT&) {
        return meetPat(std::forward<Value>(value));
      }
      constexpr static void processIdImpl(const Meet<Pred>&, int32_t /*depth*/, IdProcess) {}
    };

    template <typename Unary, typename Pattern>
    class App {
     public:
      constexpr App(Unary&& unary, const Pattern& pattern)
        : mUnary { std::forward<Unary>(unary) }, mPattern { pattern } {}
      constexpr const auto& unary() const {
        return mUnary;
      }
      constexpr const auto& pattern() const {
        return mPattern;
      }

     private:
      const Unary                     mUnary;
      const InternalPatternT<Pattern> mPattern;
    };

    template <typename Unary, typename Pattern>
    constexpr auto app(Unary&& unary, const Pattern& pattern) {
      return App<Unary, Pattern> { std::forward<Unary>(unary), pattern };
    }

    constexpr auto y = 1;
    static_assert(std::holds_alternative<const int32_t*>(std::variant<std::monostate, const int32_t*> { &y }));

    template <typename Unary, typename Pattern>
    class PatternTraits<App<Unary, Pattern>> {
     public:
      template <typename Value>
      using AppResult = std::invoke_result_t<Unary, Value>;
      // We store value for scalar types in Id and they can not be moved. So to
      // support constexpr.
      template <typename Value>
      using AppResultCurTuple = std::conditional_t<
        std::is_lvalue_reference_v<AppResult<Value>>
  #if NO_SCALAR_REFERENCES_USED_IN_PATTERNS
          || std::is_scalar_v<AppResult<Value>>
  #endif // NO_SCALAR_REFERENCES_USED_IN_PATTERNS
        ,
        std::tuple<>,
        std::tuple<std::decay_t<AppResult<Value>>>>;

      template <typename Value>
      using AppResultTuple = decltype(std::tuple_cat(
        std::declval<AppResultCurTuple<Value>>(),
        std::declval<typename PatternTraits<Pattern>::template AppResultTuple<AppResult<Value>>>()
      ));

      constexpr static auto nbIdV = PatternTraits<Pattern>::nbIdV;

      template <typename Value, typename ContextT>
      constexpr static auto
      matchPatternImpl(Value&& value, const App<Unary, Pattern>& appPat, const int32_t depth, ContextT& context) {
        if constexpr (std::is_same_v<AppResultCurTuple<Value>, std::tuple<>>) {
          return matchPattern(
            std::forward<AppResult<Value>>(invoke_(appPat.unary(), value)), appPat.pattern(), depth + 1, context
          );
        } else {
          context.emplace_back(invoke_(appPat.unary(), value));
          decltype(auto) result = get<std::decay_t<AppResult<Value>>>(context.back());
          return matchPattern(std::forward<AppResult<Value>>(result), appPat.pattern(), depth + 1, context);
        }
      }
      constexpr static void processIdImpl(const App<Unary, Pattern>& appPat, int32_t depth, IdProcess idProcess) {
        return processId(appPat.pattern(), depth, idProcess);
      }
    };

    template <typename... Patterns>
    class And {
     public:
      constexpr explicit And(const Patterns&... patterns)
        : mPatterns { patterns... } {}
      constexpr const auto& patterns() const {
        return mPatterns;
      }

     private:
      std::tuple<InternalPatternT<Patterns>...> mPatterns;
    };

    template <typename... Patterns>
    constexpr auto and_(const Patterns&... patterns) {
      return And<Patterns...> { patterns... };
    }

    template <typename Tuple>
    class NbIdInTuple;

    template <typename... Patterns>
    class NbIdInTuple<std::tuple<Patterns...>> {
     public:
      constexpr static auto nbIdV = (PatternTraits<std::decay_t<Patterns>>::nbIdV + ... + 0);
    };

    template <typename... Patterns>
    class PatternTraits<And<Patterns...>> {
     public:
      template <typename Value>
      using AppResultTuple =
        decltype(std::tuple_cat(std::declval<typename PatternTraits<Patterns>::template AppResultTuple<Value>>()...));

      constexpr static auto nbIdV = (PatternTraits<Patterns>::nbIdV + ... + 0);

      template <typename Value, typename ContextT>
      constexpr static auto
      matchPatternImpl(Value&& value, const And<Patterns...>& andPat, int32_t depth, ContextT& context) {
        constexpr auto patSize    = sizeof...(Patterns);
        const auto     exceptLast = std::apply(
          [&value, depth, &context](const auto&... patterns) {
            return (matchPattern(value, patterns, depth + 1, context) && ...);
          },
          take<patSize - 1>(andPat.patterns())
        );

        // No Id in patterns except the last one.
        if constexpr (NbIdInTuple<std::decay_t<decltype(take<patSize - 1>(andPat.patterns()))>>::nbIdV == 0) {
          return exceptLast &&
            matchPattern(std::forward<Value>(value), get<patSize - 1>(andPat.patterns()), depth + 1, context);
        } else {
          return exceptLast && matchPattern(value, get<patSize - 1>(andPat.patterns()), depth + 1, context);
        }
      }
      constexpr static void processIdImpl(const And<Patterns...>& andPat, int32_t depth, IdProcess idProcess) {
        return std::apply(
          [depth, idProcess](const Patterns&... patterns) { return (processId(patterns, depth, idProcess), ...); },
          andPat.patterns()
        );
      }
    };

    template <typename Pattern>
    class Not {
     public:
      explicit Not(const Pattern& pattern)
        : mPattern { pattern } {}
      const auto& pattern() const {
        return mPattern;
      }

     private:
      InternalPatternT<Pattern> mPattern;
    };

    template <typename Pattern>
    constexpr auto not_(const Pattern& pattern) {
      return Not<Pattern> { pattern };
    }

    template <typename Pattern>
    class PatternTraits<Not<Pattern>> {
     public:
      template <typename Value>
      using AppResultTuple = typename PatternTraits<Pattern>::template AppResultTuple<Value>;

      constexpr static auto nbIdV = PatternTraits<Pattern>::nbIdV;

      template <typename Value, typename ContextT>
      constexpr static auto
      matchPatternImpl(Value&& value, const Not<Pattern>& notPat, const int32_t depth, ContextT& context) {
        return !matchPattern(std::forward<Value>(value), notPat.pattern(), depth + 1, context);
      }
      constexpr static void processIdImpl(const Not<Pattern>& notPat, int32_t depth, IdProcess idProcess) {
        processId(notPat.pattern(), depth, idProcess);
      }
    };

    template <typename Ptr, typename Value, typename = std::void_t<>>
    struct StorePointer : std::false_type {};

    template <typename Type>
    using ValueVariant = std::conditional_t<
      std::is_lvalue_reference_v<Type>,
      UniqVariant<std::remove_reference_t<Type>*>,
      std::conditional_t<
        std::is_rvalue_reference_v<Type>,
        UniqVariant<std::remove_reference_t<Type>, std::remove_reference_t<Type>*>,
        std::conditional_t<
          std::is_abstract_v<std::remove_reference_t<Type>>,
          UniqVariant<std::remove_reference_t<Type>*, const std::remove_reference_t<Type>*>,
          UniqVariant<
            std::remove_reference_t<Type>,
            std::remove_reference_t<Type>*,
            const std::remove_reference_t<Type>*>>>>;

    template <typename Type, typename Value>
    struct StorePointer<
      Type,
      Value,
      std::void_t<decltype(std::declval<ValueVariant<Type>&>() = &std::declval<Value>())>>
      : std::is_reference<Value> // need to double check this condition. to loosen it.
    {};

    static_assert(!StorePointer<char, char>::value);
    static_assert(StorePointer<char, char&>::value);
    static_assert(StorePointer<const char, const char&>::value);
    static_assert(StorePointer<const char, char&>::value);
    static_assert(StorePointer<const std::tuple<int32_t&, int32_t&>, const std::tuple<int32_t&, int32_t&>&>::value);

    template <typename... Ts>
    class Overload : public Ts... {
     public:
      using Ts::operator()...;
    };

    template <typename... Ts>
    constexpr auto overload(Ts&&... ts) {
      return Overload<Ts...> { ts... };
    }

    template <typename Pattern>
    class OooBinder;

    class Ooo;

    template <typename Type>
    class IdTraits {
     public:
      constexpr static auto
  #if defined(__has_feature)
    #if __has_feature(address_sanitizer)
        __attribute__((no_sanitize_address))
    #endif
  #endif
        equal(Type const& lhs, Type const& rhs) {
        return lhs == rhs;
      }
    };

    template <typename Type>
    class IdBlockBase {
      int32_t mDepth;

     protected:
      ValueVariant<Type> mVariant;

     public:
      constexpr IdBlockBase()
        : mDepth {}, mVariant {} {}

      constexpr auto& variant() {
        return mVariant;
      }
      constexpr void reset(const int32_t depth) {
        if (mDepth - depth >= 0) {
          mVariant = {};
          mDepth   = depth;
        }
      }
      constexpr void confirm(const int32_t depth) {
        if (mDepth > depth || mDepth == 0) {
          assert(depth == mDepth - 1 || depth == mDepth || mDepth == 0);
          mDepth = depth;
        }
      }
    };

    constexpr IdBlockBase<int> dummy;

    template <typename Type>
    class IdBlock : public IdBlockBase<Type> {
     public:
      constexpr auto hasValue() const {
        return std::visit(
          overload(
            [](const Type&) { return true; },
            [](const Type*) { return true; },
            // [](Type *)
            // { return true; },
            [](const std::monostate&) { return false; }
          ),
          IdBlockBase<Type>::mVariant
        );
      }
      constexpr decltype(auto) get() const {
        return std::visit(
          overload(
            [](const Type& v) -> const Type& { return v; },
            [](const Type* p) -> const Type& { return *p; },
            [](Type* p) -> const Type& { return *p; },
            [](const std::monostate&) -> const Type& { throw std::logic_error("invalid state!"); }
          ),
          IdBlockBase<Type>::mVariant
        );
      }
    };

    template <typename Type>
    class IdBlock<const Type&> : public IdBlock<Type> {};

    template <typename Type>
    class IdBlock<Type&> : public IdBlockBase<Type&> {
     public:
      constexpr auto hasValue() const {
        return std::visit(
          overload([](Type*) { return true; }, [](const std::monostate&) { return false; }),
          IdBlockBase<Type&>::mVariant
        );
      }

      constexpr decltype(auto) get() {
        return std::visit(
          overload(
            [](Type* v) -> Type& {
              if (v == nullptr) {
                throw std::logic_error("Trying to dereference a nullptr!");
              }
              return *v;
            },
            [](std::monostate&) -> Type& { throw std::logic_error("Invalid state!"); }
          ),
          IdBlockBase<Type&>::mVariant
        );
      }
    };

    template <typename Type>
    class IdBlock<Type&&> : public IdBlockBase<Type&&> {
     public:
      constexpr auto hasValue() const {
        return std::visit(
          overload(
            [](const Type&) { return true; }, [](Type*) { return true; }, [](const std::monostate&) { return false; }
          ),
          IdBlockBase<Type&&>::mVariant
        );
      }

      constexpr decltype(auto) get() {
        return std::visit(
          overload(
            [](Type& v) -> Type& { return v; },
            [](Type* v) -> Type& {
              if (v == nullptr) {
                throw std::logic_error("Trying to dereference a nullptr!");
              }
              return *v;
            },
            [](std::monostate&) -> Type& { throw std::logic_error("Invalid state!"); }
          ),
          IdBlockBase<Type&&>::mVariant
        );
      }
    };

    template <typename Type>
    class IdUtil {
     public:
      template <typename Value>
      constexpr static auto bindValue(ValueVariant<Type>& v, Value&& value, std::false_type /* StorePointer */) {
        // for constexpr
        v = ValueVariant<Type> { std::forward<Value>(value) };
      }
      template <typename Value>
      constexpr static auto bindValue(ValueVariant<Type>& v, Value&& value, std::true_type /* StorePointer */) {
        v = ValueVariant<Type> { &value };
      }
    };

    template <typename Type>
    class Id {
     private:
      using BlockT   = IdBlock<Type>;
      using BlockVT  = std::variant<BlockT, BlockT*>;
      BlockVT mBlock = BlockT {};

      constexpr decltype(auto) internalValue() const {
        return block().get();
      }

     public:
      constexpr Id() = default;

      constexpr Id(const Id& id) {
        mBlock = BlockVT { &id.block() };
      }

      // non-const to inform users not to mark Id as const.
      template <typename Pattern>
      constexpr auto at(Pattern&& pattern) {
        return and_(pattern, *this);
      }

      // non-const to inform users not to mark Id as const.
      constexpr auto at(const Ooo&) {
        return OooBinder<Type> { *this };
      }

      constexpr BlockT& block() const {
        return std::visit(
          overload([](BlockT& v) -> BlockT& { return v; }, [](BlockT* p) -> BlockT& { return *p; }),
          // constexpr does not allow mutable, we use const_cast
          // instead. Never declare Id as const.
          const_cast<BlockVT&>(mBlock)
        );
      }

      template <typename Value>
      constexpr auto matchValue(Value&& v) const {
        if (hasValue()) {
          return IdTraits<std::decay_t<Type>>::equal(internalValue(), v);
        }
        IdUtil<Type>::bindValue(block().variant(), std::forward<Value>(v), StorePointer<Type, Value> {});
        return true;
      }
      constexpr void reset(int32_t depth) const {
        return block().reset(depth);
      }
      constexpr void confirm(int32_t depth) const {
        return block().confirm(depth);
      }
      constexpr bool hasValue() const {
        return block().hasValue();
      }
      // non-const to inform users not to mark Id as const.
      constexpr decltype(auto) get() {
        return block().get();
      }
      // non-const to inform users not to mark Id as const.
      constexpr decltype(auto) operator*() {
        return get();
      }
    };

    template <typename Type>
    class PatternTraits<Id<Type>> {
     public:
      template <typename Value>
      using AppResultTuple = std::tuple<>;

      constexpr static auto nbIdV = true;

      template <typename Value, typename ContextT>
      constexpr static auto matchPatternImpl(Value&& value, const Id<Type>& idPat, int32_t /* depth */, ContextT&) {
        return idPat.matchValue(std::forward<Value>(value));
      }
      constexpr static void processIdImpl(const Id<Type>& idPat, int32_t depth, const IdProcess idProcess) {
        switch (idProcess) {
          case IdProcess::kCANCEL: idPat.reset(depth); break;

          case IdProcess::kCONFIRM: idPat.confirm(depth); break;
        }
      }
    };

    template <typename... Patterns>
    class Ds {
     public:
      constexpr explicit Ds(const Patterns&... patterns)
        : mPatterns { patterns... } {}
      constexpr const auto& patterns() const {
        return mPatterns;
      }

      using Type = std::tuple<InternalPatternT<Patterns>...>;

     private:
      Type mPatterns;
    };

    template <typename... Patterns>
    constexpr auto ds(const Patterns&... patterns) -> Ds<Patterns...> {
      return Ds<Patterns...> { patterns... };
    }

    template <typename T>
    class OooBinder {
      Id<T> mId;

     public:
      explicit OooBinder(const Id<T>& id)
        : mId { id } {}
      decltype(auto) binder() const {
        return mId;
      }
    };

    class Ooo {
     public:
      template <typename T>
      constexpr auto operator()(Id<T> id) const {
        return OooBinder<T> { id };
      }
    };

    constexpr Ooo ooo;

    template <>
    class PatternTraits<Ooo> {
     public:
      template <typename Value>
      using AppResultTuple = std::tuple<>;

      constexpr static auto nbIdV = false;

      template <typename Value, typename ContextT>
      constexpr static auto matchPatternImpl(Value&&, Ooo, int32_t /*depth*/, ContextT&) {
        return true;
      }
      constexpr static void processIdImpl(Ooo, int32_t /*depth*/, IdProcess) {}
    };

    template <typename Pattern>
    class PatternTraits<OooBinder<Pattern>> {
     public:
      template <typename Value>
      using AppResultTuple = typename PatternTraits<Pattern>::template AppResultTuple<Value>;

      constexpr static auto nbIdV = PatternTraits<Pattern>::nbIdV;

      template <typename Value, typename ContextT>
      constexpr static auto
      matchPatternImpl(Value&& value, const OooBinder<Pattern>& oooBinderPat, const int32_t depth, ContextT& context) {
        return matchPattern(std::forward<Value>(value), oooBinderPat.binder(), depth + 1, context);
      }
      constexpr static void processIdImpl(const OooBinder<Pattern>& oooBinderPat, int32_t depth, IdProcess idProcess) {
        processId(oooBinderPat.binder(), depth, idProcess);
      }
    };

    template <typename T>
    class IsOoo : public std::false_type {};

    template <>
    class IsOoo<Ooo> : public std::true_type {};

    template <typename T>
    class IsOooBinder : public std::false_type {};

    template <typename T>
    class IsOooBinder<OooBinder<T>> : public std::true_type {};

    template <typename T>
    constexpr auto isOooBinderV = IsOooBinder<std::decay_t<T>>::value;

    template <typename T>
    constexpr auto isOooOrBinderV = IsOoo<std::decay_t<T>>::value || isOooBinderV<T>;

    template <typename... Patterns>
    constexpr auto nbOooOrBinderV = ((isOooOrBinderV<Patterns> ? 1 : 0) + ... + 0);

    static_assert(nbOooOrBinderV<int32_t&, const Ooo&, const char*, Wildcard, const Ooo> == 2);

    template <typename Tuple, std::size_t... I>
    constexpr size_t findOooIdxImpl(std::index_sequence<I...>) {
      return ((isOooOrBinderV<decltype(get<I>(std::declval<Tuple>()))> ? I : 0) + ...);
    }

    template <typename Tuple>
    constexpr size_t findOooIdx() {
      return findOooIdxImpl<Tuple>(std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>> {});
    }

    static_assert(isOooOrBinderV<Ooo>);
    static_assert(isOooOrBinderV<OooBinder<int32_t>>);
    static_assert(findOooIdx<std::tuple<int32_t, OooBinder<int32_t>, const char*>>() == 1);
    static_assert(findOooIdx<std::tuple<int32_t, Ooo, const char*>>() == 1);

    using std::get;
    template <
      std::size_t valueStartIdx,
      std::size_t patternStartIdx,
      std::size_t... I,
      typename ValueTuple,
      typename PatternTuple,
      typename ContextT>
    constexpr decltype(auto)
    matchPatternMultipleImpl(ValueTuple&& valueTuple, PatternTuple&& patternTuple, const int32_t depth, ContextT& context, std::index_sequence<I...>) {
      const auto func = [&]<typename T>(T&& value, auto&& pattern) {
        return matchPattern(std::forward<T>(value), pattern, depth + 1, context);
      };
      static_cast<void>(func);
      return (
        func(
          get<I + valueStartIdx>(std::forward<ValueTuple>(valueTuple)), std::get<I + patternStartIdx>(patternTuple)
        ) &&
        ...
      );
    }

    template <
      std::size_t valueStartIdx,
      std::size_t patternStartIdx,
      std::size_t size,
      typename ValueTuple,
      typename PatternTuple,
      typename ContextT>
    constexpr decltype(auto)
    matchPatternMultiple(ValueTuple&& valueTuple, PatternTuple&& patternTuple, int32_t depth, ContextT& context) {
      return matchPatternMultipleImpl<valueStartIdx, patternStartIdx>(
        std::forward<ValueTuple>(valueTuple), patternTuple, depth, context, std::make_index_sequence<size> {}
      );
    }

    template <
      std::size_t patternStartIdx,
      std::size_t... I,
      typename RangeBegin,
      typename PatternTuple,
      typename ContextT>
    constexpr decltype(auto)
    matchPatternRangeImpl(RangeBegin&& rangeBegin, PatternTuple&& patternTuple, const int32_t depth, ContextT& context, std::index_sequence<I...>) {
      const auto func = [&]<typename T>(T&& value, auto&& pattern) {
        return matchPattern(std::forward<T>(value), pattern, depth + 1, context);
      };
      static_cast<void>(func);
      // Fix Me, avoid call next from begin every time.
      return (func(*std::next(rangeBegin, static_cast<long>(I)), std::get<I + patternStartIdx>(patternTuple)) && ...);
    }

    template <
      std::size_t patternStartIdx,
      std::size_t size,
      typename ValueRangeBegin,
      typename PatternTuple,
      typename ContextT>
    constexpr decltype(auto) matchPatternRange(
      ValueRangeBegin&& valueRangeBegin,
      PatternTuple&&    patternTuple,
      int32_t           depth,
      ContextT&         context
    ) {
      return matchPatternRangeImpl<patternStartIdx>(
        valueRangeBegin, patternTuple, depth, context, std::make_index_sequence<size> {}
      );
    }

    template <std::size_t start, typename Indices, typename Tuple>
    class IndexedTypes;

    template <typename Tuple, std::size_t start, std::size_t... I>
    class IndexedTypes<start, std::index_sequence<I...>, Tuple> {
     public:
      using type = std::tuple<std::decay_t<decltype(std::get<start + I>(std::declval<Tuple>()))>...>;
    };

    template <std::size_t start, std::size_t end, class Tuple>
    class SubTypes {
      constexpr static auto tupleSize = std::tuple_size_v<std::remove_reference_t<Tuple>>;
      static_assert(start <= end);
      static_assert(end <= tupleSize);

      using Indices = std::make_index_sequence<end - start>;

     public:
      using type = typename IndexedTypes<start, Indices, Tuple>::type;
    };

    template <std::size_t start, std::size_t end, class Tuple>
    using SubTypesT = typename SubTypes<start, end, Tuple>::type;

    static_assert(std::is_same_v<
                  std::tuple<std::nullptr_t>,
                  SubTypesT<3, 4, std::tuple<char, bool, int32_t, std::nullptr_t>>>);
    static_assert(std::is_same_v<std::tuple<char>, SubTypesT<0, 1, std::tuple<char, bool, int32_t, std::nullptr_t>>>);
    static_assert(std::is_same_v<std::tuple<>, SubTypesT<1, 1, std::tuple<char, bool, int32_t, std::nullptr_t>>>);
    static_assert(std::is_same_v<
                  std::tuple<int32_t, std::nullptr_t>,
                  SubTypesT<2, 4, std::tuple<char, bool, int32_t, std::nullptr_t>>>);

    template <typename ValueTuple>
    class IsArray : public std::false_type {};

    template <typename T, size_t s>
    class IsArray<std::array<T, s>> : public std::true_type {};

    template <typename ValueTuple>
    constexpr auto isArrayV = IsArray<std::decay_t<ValueTuple>>::value;

    template <typename Value, typename = std::void_t<>>
    struct IsTupleLike : std::false_type {};

    template <typename Value>
    // ReSharper disable once CppUseTypeTraitAlias
    struct IsTupleLike<Value, std::void_t<decltype(std::tuple_size<Value>::value)>> : std::true_type {};

    template <typename ValueTuple>
    constexpr auto isTupleLikeV = IsTupleLike<std::decay_t<ValueTuple>>::value;

    static_assert(isTupleLikeV<std::pair<int32_t, char>>);
    static_assert(!isTupleLikeV<bool>);

    template <typename Value, typename = std::void_t<>>
    struct IsRange : std::false_type {};

    template <typename Value>
    struct IsRange<
      Value,
      std::void_t<decltype(std::begin(std::declval<Value>())), decltype(std::end(std::declval<Value>()))>>
      : std::true_type {};

    template <typename ValueTuple>
    constexpr auto isRangeV = IsRange<std::decay_t<ValueTuple>>::value;

    static_assert(!isRangeV<std::pair<int32_t, char>>);
    static_assert(isRangeV<const std::array<int32_t, 5>>);

    template <typename... Patterns>
    class PatternTraits<Ds<Patterns...>> {
      constexpr static auto nbOooOrBinder = nbOooOrBinderV<Patterns...>;
      static_assert(nbOooOrBinder == 0 || nbOooOrBinder == 1);

     public:
      template <typename PsTuple, typename VsTuple>
      class PairPV;

      template <typename... Ps, typename... Vs>
      class PairPV<std::tuple<Ps...>, std::tuple<Vs...>> {
       public:
        using type =
          decltype(std::tuple_cat(std::declval<typename PatternTraits<Ps>::template AppResultTuple<Vs>>()...));
      };

      template <std::size_t nbOoos, typename ValueTuple>
      class AppResultForTupleHelper;

      template <typename... Values>
      class AppResultForTupleHelper<0, std::tuple<Values...>> {
       public:
        using type =
          decltype(std::tuple_cat(std::declval<typename PatternTraits<Patterns>::template AppResultTuple<Values>>()...)
          );
      };

      template <typename... Values>
      class AppResultForTupleHelper<1, std::tuple<Values...>> {
        constexpr static auto idxOoo   = findOooIdx<typename Ds<Patterns...>::Type>();
        using Ps0                      = SubTypesT<0, idxOoo, std::tuple<Patterns...>>;
        using Vs0                      = SubTypesT<0, idxOoo, std::tuple<Values...>>;
        constexpr static auto isBinder = isOooBinderV<std::tuple_element_t<idxOoo, std::tuple<Patterns...>>>;
        // <0, ...int32_t> to workaround compile failure for std::tuple<>.
        using ElemT                          = std::tuple_element_t<0, std::tuple<std::remove_reference_t<Values>..., int32_t>>;
        constexpr static int64_t diff        = static_cast<int64_t>(sizeof...(Values) - sizeof...(Patterns));
        constexpr static size_t  clippedDiff = static_cast<size_t>(diff > 0 ? diff : 0);
        using OooResultTuple =
          std::conditional_t<isBinder, std::tuple<SubrangeT<std::array<ElemT, clippedDiff>>>, std::tuple<>>;
        using FirstHalfTuple           = typename PairPV<Ps0, Vs0>::type;
        using Ps1                      = SubTypesT<idxOoo + 1, sizeof...(Patterns), std::tuple<Patterns...>>;
        constexpr static auto vs1Start = static_cast<size_t>(static_cast<int64_t>(idxOoo) + 1 + diff);
        using Vs1                      = SubTypesT<vs1Start, sizeof...(Values), std::tuple<Values...>>;
        using SecondHalfTuple          = typename PairPV<Ps1, Vs1>::type;

       public:
        using type = decltype(std::tuple_cat(
          std::declval<FirstHalfTuple>(),
          std::declval<OooResultTuple>(),
          std::declval<SecondHalfTuple>()
        ));
      };

      template <typename Tuple>
      using AppResultForTuple =
        typename AppResultForTupleHelper<nbOooOrBinder, decltype(drop<0>(std::declval<Tuple>()))>::type;

      template <typename RangeType>
      using RangeTuple = std::conditional_t<nbOooOrBinder == 1, std::tuple<SubrangeT<RangeType>>, std::tuple<>>;

      template <typename RangeType>
      using AppResultForRangeType = decltype(std::tuple_cat(
        std::declval<RangeTuple<RangeType>>(),
        std::declval<
          typename PatternTraits<Patterns>::template AppResultTuple<decltype(*std::begin(std::declval<RangeType>()))>>(
        )...
      ));

      template <typename Value, typename = std::void_t<>>
      class AppResultHelper;

      template <typename Value>
      class AppResultHelper<Value, std::enable_if_t<isTupleLikeV<Value>>> {
       public:
        using type = AppResultForTuple<Value>;
      };

      template <typename RangeType>
      class AppResultHelper<RangeType, std::enable_if_t<!isTupleLikeV<RangeType> && isRangeV<RangeType>>> {
       public:
        using type = AppResultForRangeType<RangeType>;
      };

      template <typename Value>
      using AppResultTuple = typename AppResultHelper<Value>::type;

      constexpr static auto nbIdV = (PatternTraits<Patterns>::nbIdV + ... + 0);

      template <typename ValueTuple, typename ContextT>
      constexpr static auto
      matchPatternImpl(ValueTuple&& valueTuple, const Ds<Patterns...>& dsPat, int32_t depth, ContextT& context)
        -> std::enable_if_t<isTupleLikeV<ValueTuple>, bool> {
        if constexpr (nbOooOrBinder == 0) {
          return std::apply(
            [&valueTuple, depth, &context](const auto&... patterns) {
              return apply_(
                [depth, &context, &patterns...]<typename... T>(T&&... values) constexpr {
                  static_assert(sizeof...(patterns) == sizeof...(values));
                  return (matchPattern(std::forward<T>(values), patterns, depth + 1, context) && ...);
                },
                valueTuple
              );
            },
            dsPat.patterns()
          );
        } else if constexpr (nbOooOrBinder == 1) {
          constexpr auto idxOoo   = findOooIdx<typename Ds<Patterns...>::Type>();
          constexpr auto isBinder = isOooBinderV<std::tuple_element_t<idxOoo, std::tuple<Patterns...>>>;
          constexpr auto isArray  = isArrayV<ValueTuple>;
          auto           result =
            matchPatternMultiple<0, 0, idxOoo>(std::forward<ValueTuple>(valueTuple), dsPat.patterns(), depth, context);
          constexpr auto valLen = std::tuple_size_v<std::decay_t<ValueTuple>>;
          constexpr auto patLen = sizeof...(Patterns);
          if constexpr (isArray) {
            if constexpr (isBinder) {
              const auto rangeSize = static_cast<long>(valLen - (patLen - 1));
              context.emplace_back(makeSubrange(&valueTuple[idxOoo], &valueTuple[idxOoo] + rangeSize));
              using type = decltype(makeSubrange(&valueTuple[idxOoo], &valueTuple[idxOoo] + rangeSize));
              result     = result &&
                matchPattern(std::get<type>(context.back()), std::get<idxOoo>(dsPat.patterns()), depth, context);
            }
          } else {
            static_assert(!isBinder);
          }
          return result &&
            matchPatternMultiple<valLen - patLen + idxOoo + 1, idxOoo + 1, patLen - idxOoo - 1>(
                   std::forward<ValueTuple>(valueTuple), dsPat.patterns(), depth, context
            );
        }

        return;
      }

      template <typename ValueRange, typename ContextT>
      constexpr static auto
      matchPatternImpl(ValueRange&& valueRange, const Ds<Patterns...>& dsPat, int32_t depth, ContextT& context)
        -> std::enable_if_t<!isTupleLikeV<ValueRange> && isRangeV<ValueRange>, bool> {
        static_assert(nbOooOrBinder == 0 || nbOooOrBinder == 1);
        constexpr auto nbPat = sizeof...(Patterns);

        if constexpr (nbOooOrBinder == 0) {
          // size mismatch for dynamic array is not an error;
          if (valueRange.size() != nbPat) {
            return false;
          }
          return matchPatternRange<0, nbPat>(std::begin(valueRange), dsPat.patterns(), depth, context);
        } else if constexpr (nbOooOrBinder == 1) {
          if (valueRange.size() < nbPat - 1) {
            return false;
          }
          constexpr auto idxOoo   = findOooIdx<typename Ds<Patterns...>::Type>();
          constexpr auto isBinder = isOooBinderV<std::tuple_element_t<idxOoo, std::tuple<Patterns...>>>;
          auto           result   = matchPatternRange<0, idxOoo>(std::begin(valueRange), dsPat.patterns(), depth, context);
          const auto     valLen   = valueRange.size();
          constexpr auto patLen   = sizeof...(Patterns);
          const auto     beginOoo = std::next(std::begin(valueRange), idxOoo);
          if constexpr (isBinder) {
            const auto rangeSize = static_cast<long>(valLen - (patLen - 1));
            const auto end       = std::next(beginOoo, rangeSize);
            context.emplace_back(makeSubrange(beginOoo, end));
            using type = decltype(makeSubrange(beginOoo, end));
            result     = result &&
              matchPattern(std::get<type>(context.back()), std::get<idxOoo>(dsPat.patterns()), depth, context);
          }
          const auto beginAfterOoo = std::next(beginOoo, static_cast<long>(valLen - patLen + 1));
          return result &&
            matchPatternRange<idxOoo + 1, patLen - idxOoo - 1>(beginAfterOoo, dsPat.patterns(), depth, context);
        }

        return;
      }

      constexpr static void processIdImpl(const Ds<Patterns...>& dsPat, int32_t depth, IdProcess idProcess) {
        return std::apply(
          [depth, idProcess](auto&&... patterns) { return (processId(patterns, depth, idProcess), ...); },
          dsPat.patterns()
        );
      }
    };

    static_assert(std::is_same_v<
                  PatternTraits<Ds<OooBinder<SubrangeT<const std::array<int32_t, 2>>>>>::AppResultTuple<
                    const std::array<int32_t, 2>>,
                  std::tuple<matchit::impl::Subrange<const int32_t*>>>);

    static_assert(std::is_same_v<
                  PatternTraits<Ds<OooBinder<Subrange<int32_t*>>, matchit::impl::Id<int32_t>>>::AppResultTuple<
                    const std::array<int32_t, 3>>,
                  std::tuple<matchit::impl::Subrange<const int32_t*>>>);

    static_assert(std::is_same_v<
                  PatternTraits<Ds<OooBinder<Subrange<int32_t*>>, matchit::impl::Id<int32_t>>>::AppResultTuple<
                    std::array<int32_t, 3>>,
                  std::tuple<matchit::impl::Subrange<int32_t*>>>);

    template <typename Pattern, typename Pred>
    class PostCheck {
     public:
      constexpr explicit PostCheck(const Pattern& pattern, const Pred& pred)
        : mPattern { pattern }, mPred { pred } {}
      constexpr bool check() const {
        return mPred();
      }
      constexpr const auto& pattern() const {
        return mPattern;
      }

     private:
      const Pattern mPattern;
      const Pred    mPred;
    };

    template <typename Pattern, typename Pred>
    class PatternTraits<PostCheck<Pattern, Pred>> {
     public:
      template <typename Value>
      using AppResultTuple = typename PatternTraits<Pattern>::template AppResultTuple<Value>;

      template <typename Value, typename ContextT>
      constexpr static auto matchPatternImpl(
        Value&&                         value,
        const PostCheck<Pattern, Pred>& postCheck,
        const int32_t                   depth,
        ContextT&                       context
      ) {
        return matchPattern(std::forward<Value>(value), postCheck.pattern(), depth + 1, context) && postCheck.check();
      }
      constexpr static void
      processIdImpl(const PostCheck<Pattern, Pred>& postCheck, int32_t depth, IdProcess idProcess) {
        processId(postCheck.pattern(), depth, idProcess);
      }
    };

    static_assert(std::is_same_v<PatternTraits<Wildcard>::AppResultTuple<int32_t>, std::tuple<>>);
    static_assert(std::is_same_v<PatternTraits<int32_t>::AppResultTuple<int32_t>, std::tuple<>>);
    constexpr auto x = [](auto&& t) { return t; };
    static_assert(std::is_same_v<
                  PatternTraits<App<decltype(x), Wildcard>>::AppResultTuple<std::array<int32_t, 3>>,
                  std::tuple<std::array<int32_t, 3>>>);

    static_assert(PatternTraits<And<App<decltype(x), Wildcard>>>::nbIdV == 0);
    static_assert(PatternTraits<And<App<decltype(x), Id<int32_t>>>>::nbIdV == 1);
    static_assert(PatternTraits<And<Id<int32_t>, Id<float>>>::nbIdV == 2);
    static_assert(PatternTraits<Or<Id<int32_t>, Id<float>>>::nbIdV == 2);
    static_assert(PatternTraits<Or<Wildcard, float>>::nbIdV == 0);

    template <typename Value, typename... PatternPairs>
    constexpr auto matchPatterns(Value&& value, const PatternPairs&... patterns) {
      using RetType   = typename PatternPairsRetType<PatternPairs...>::RetType;
      using TypeTuple = decltype(std::tuple_cat(
        std::declval<typename PatternTraits<typename PatternPairs::PatternT>::template AppResultTuple<Value>>()...
      ));

      // expression, has return value.
      if constexpr (!std::is_same_v<RetType, void>) {
        constexpr auto func = [](const auto& pattern, auto&& val, RetType& result) constexpr -> bool {
          if (auto context = typename ContextTrait<TypeTuple>::ContextT {};
              pattern.matchValue(std::forward<Value>(val), context)) {
            result = pattern.execute();
            processId(pattern, 0, IdProcess::kCANCEL);
            return true;
          }
          return false;
        };
        RetType    result {};
        const bool matched = (func(patterns, value, result) || ...);
        if (!matched) {
          throw std::logic_error { "Error: no patterns got matched!" };
        }
        static_cast<void>(matched);
        return result;
      } else
      // statement, no return value, mismatching all patterns is not an error.
      {
        const auto func = [](const auto& pattern, auto&& val) -> bool {
          if (auto context = typename ContextTrait<TypeTuple>::ContextT {};
              pattern.matchValue(std::forward<Value>(val), context)) {
            pattern.execute();
            processId(pattern, 0, IdProcess::kCANCEL);
            return true;
          }
          return false;
        };
        const bool matched = (func(patterns, value) || ...);
        static_cast<void>(matched);
      }
    }

  } // namespace impl

  // export symbols
  using impl::_;
  using impl::and_;
  using impl::app;
  using impl::ds;
  using impl::Id;
  using impl::is;
  using impl::meet;
  using impl::not_;
  using impl::ooo;
  using impl::or_;
  using impl::Subrange;
  using impl::SubrangeT;
  using impl::when;
} // namespace matchit

#endif // MATCHIT_PATTERNS_H
#ifndef MATCHIT_UTILITY_H
  #define MATCHIT_UTILITY_H

  #include <any>

namespace matchit {
  namespace impl {

    template <typename T>
    constexpr auto cast = [](auto&& input) { return static_cast<T>(input); };

    constexpr auto deref = [](auto&& x) -> decltype(*x)& { return *x; };
    constexpr auto some  = [](const auto pat) { return and_(app(cast<bool>, true), app(deref, pat)); };

    constexpr auto none = app(cast<bool>, false);

    template <typename Value, typename Variant, typename = std::void_t<>>
    struct ViaGetIf : std::false_type {};

    using std::get_if;

    template <typename T, typename Variant>
    struct ViaGetIf<T, Variant, std::void_t<decltype(get_if<T>(std::declval<const Variant*>()))>> : std::true_type {};

    template <typename T, typename Variant>
    constexpr auto viaGetIfV = ViaGetIf<T, Variant>::value;

    static_assert(viaGetIfV<int, std::variant<int, bool>>);

    template <typename T>
    class AsPointer {
      static_assert(!std::is_reference_v<T>);

     public:
      template <typename Variant, std::enable_if_t<viaGetIfV<T, std::decay_t<Variant>>>* = nullptr>
      constexpr auto operator()(Variant&& v) const {
        return get_if<T>(std::addressof(v));
      }

      // template to disable implicit cast to std::any
      template <typename A, std::enable_if_t<std::is_same_v<std::decay_t<A>, std::any>>* = nullptr>
      constexpr auto operator()(A&& a) const {
        return std::any_cast<T>(std::addressof(a));
      }

      // cast to base class
      template <typename D, std::enable_if_t<!viaGetIfV<T, D> && std::is_base_of_v<T, D>>* = nullptr>
      constexpr auto operator()(const D& d) const -> decltype(static_cast<const T*>(std::addressof(d))) {
        return static_cast<const T*>(std::addressof(d));
      }

      // No way to handle rvalue to save copy in this class. Need to define some in another way to handle this.
      // cast to base class
      template <typename D, std::enable_if_t<!viaGetIfV<T, D> && std::is_base_of_v<T, D>>* = nullptr>
      constexpr auto operator()(D& d) const -> decltype(static_cast<T*>(std::addressof(d))) {
        return static_cast<T*>(std::addressof(d));
      }

      // cast to derived class
      template <typename B, std::enable_if_t<!viaGetIfV<T, B> && std::is_base_of_v<B, T>>* = nullptr>
      constexpr auto operator()(const B& b) const -> decltype(dynamic_cast<const T*>(std::addressof(b))) {
        return dynamic_cast<const T*>(std::addressof(b));
      }

      // cast to derived class
      template <typename B, std::enable_if_t<!viaGetIfV<T, B> && std::is_base_of_v<B, T>>* = nullptr>
      constexpr auto operator()(B& b) const -> decltype(dynamic_cast<T*>(std::addressof(b))) {
        return dynamic_cast<T*>(std::addressof(b));
      }

      constexpr auto operator()(const T& b) const {
        return std::addressof(b);
      }

      constexpr auto operator()(T& b) const {
        return std::addressof(b);
      }
    };

    static_assert(std::is_invocable_v<AsPointer<int>, int>);
    static_assert(std::is_invocable_v<AsPointer<std::tuple<int>>, std::tuple<int>>);

    template <typename T>
    constexpr AsPointer<T> asPointer;

    template <typename T>
    constexpr auto as = [](const auto pat) { return app(asPointer<T>, some(pat)); };

    template <typename Value, typename Pattern>
    constexpr auto matched(Value&& v, Pattern&& p) {
      return match(std::forward<Value>(v))(
        is | std::forward<Pattern>(p) = [] { return true; }, is | _ = [] { return false; }
      );
    }

    constexpr auto dsVia = [](auto... members) {
      return [members...](auto... pats) { return and_(app(members, pats)...); };
    };

    template <typename T>
    constexpr auto asDsVia =
      [](auto... members) { return [members...](auto... pats) { return as<T>(and_(app(members, pats)...)); }; };

  } // namespace impl
  using impl::as;
  using impl::asDsVia;
  using impl::dsVia;
  using impl::matched;
  using impl::none;
  using impl::some;
  // NOLINTEND(*-identifier-naming)
} // namespace matchit

#endif // MATCHIT_UTILITY_H

#endif // MATCHIT_H
