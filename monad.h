#pragma once

#include <vector>
#include <tuple>
#include <type_traits>
#include <utility>

template <typename M>
struct monad
{
    static const bool has_monad = false;
};

// operator overloading for bind and fmap
template <typename M, typename Func
    , typename _MustBeMonad = typename std::enable_if<monad<M>::has_monad, void>::type
    >
auto operator >=(M m, Func f)
{
    return monad<M>::bind(m, f);
}

template <typename M, typename Func
    , typename _MustBeMonad = typename std::enable_if<monad<M>::has_monad, void>::type
    >
auto operator >(M m, Func f)
{
    return monad<M>::fmap(m, f);
}

template <typename A, typename M_A, typename V_A, typename M_V_A, typename... Other>
struct monad_sequence_helper
{
    // [a] -> [Monad a] -> int -> Monad [a]
    static M_V_A call(V_A out, std::vector<M_A, Other...> vec, size_t index)
    {
        if (index < vec.size())
        {
            // Monad a >>= (a -> Monad [a]) -> Monad [a]
            return vec[index] >= [out = std::move(out), vec = std::move(vec), index](const auto &a) mutable {
                auto new_out = out; // must make a copy, if it's a list monad
                new_out.push_back(a);
                return call(std::move(new_out), vec, index + 1);
            };
        }
        else
        {
            return monad<M_V_A>::wrap(std::move(out));
        }
    }
};

// [Monad a] -> Monad [a]
template <typename T, typename... Other>
auto monad_sequence(std::vector<T, Other...> t)
{
    using A = typename monad<T>::ElemType;
    using V_A = std::vector<A>; // [a]
    using M_V_A = typename monad<T>::template OtherType<V_A>;
    V_A out;
    return monad_sequence_helper<A, T, V_A, M_V_A, Other...>::call(out, std::move(t), 0);
}

// monad apply
template <typename Func, typename Tuple, size_t... I>
auto monad_apply_func_tuple(Func f, Tuple tuple, std::index_sequence<I...>)
{
    return f(std::forward<typename std::tuple_element<I, Tuple>::type>(std::get<I>(tuple))...);
}

template <int N, typename Func, typename MonadTuple, typename OutTuple>
struct monad_apply_helper
{
    static auto call(MonadTuple ms, Func f, OutTuple out_tuple)
    {
        constexpr int Index = std::tuple_size<MonadTuple>::value - N;
        return std::get<Index>(ms) >= [ms = std::move(ms), f = std::move(f), out_tuple = std::move(out_tuple)](const auto &a) mutable {
            auto new_out_tuple = std::tuple_cat(out_tuple, std::make_tuple(a));
            return monad_apply_helper<N - 1, Func, MonadTuple, decltype(new_out_tuple)>::call(ms, f, std::move(new_out_tuple));
        };
    }
};

template <typename Func, typename MonadTuple, typename OutTuple>
struct monad_apply_helper<0, Func, MonadTuple, OutTuple>
{
    static auto call(MonadTuple ms, Func f, OutTuple out_tuple)
    {
        static_assert(std::tuple_size<OutTuple>::value == std::tuple_size<MonadTuple>::value, "tuple must be of same size");
        constexpr size_t N = std::tuple_size<OutTuple>::value;

        auto result = monad_apply_func_tuple(f, std::move(out_tuple), std::make_index_sequence<N>());

        using FirstMonadType = typename std::tuple_element<0, MonadTuple>::type;
        return monad<typename monad<FirstMonadType>::template OtherType<decltype(result)>>::wrap(result);
    }
};

// (m a1, m a2, ...., m an) -> (a1 -> a2 -> ... -> an -> out) -> m out
template <typename Func, typename... Ms>
auto monad_apply(std::tuple<Ms...> ms, Func f)
{
    return monad_apply_helper<sizeof...(Ms), Func, std::tuple<Ms...>, std::tuple<>>::call(std::move(ms), std::move(f), std::make_tuple());
}

// (m a1, m a2, ...., m an) -> (a1 -> a2 -> ... -> an -> out) -> m out
template <typename Func, typename... Ms>
auto operator >(std::tuple<Ms...> ms, Func f)
{
    return monad_apply(ms, f);
}

// (m a1, m a2, ...., m an) -> (a1 -> a2 -> ... -> an -> m out) -> m out
template <typename Func, typename... Ms>
auto operator >=(std::tuple<Ms...> ms, Func f)
{
    auto ret = monad_apply(ms, f); // type: m m out
    return monad<typename monad<decltype(ret)>::ElemType>::join(ret);
}

