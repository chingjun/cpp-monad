#include <vector>

#include "monad.h"

template <typename T>
struct monad<std::vector<T>>
{
    using ElemType = T;
    template <typename U> using OtherType = std::vector<U>;
    using M = std::vector<T>;
    static const bool has_monad = true;
    template <typename Func>
    static auto fmap(const M &from, Func f)
    {
        using From = T;
        using To = decltype(f(std::declval<T>()));
        auto ret = std::vector<To>();
        ret.reserve(from.size());

        for (auto &elem : from)
        {
            ret.push_back(f(elem));
        }

        return ret;
    }

    static M join(std::vector<M> v)
    {
        M ret;
        for (auto &row : v)
        {
            for (auto &elem : row)
            {
                ret.push_back(elem);
            }
        }
        return ret;
    }

    static M wrap(T e)
    {
        M ret;
        ret.emplace_back(std::move(e));
        return ret;
    }

    template <typename Func>
    static auto bind(M p, Func f)
    {
        using RetType = decltype(f(std::declval<T>()));
        return monad<RetType>::join(fmap(p, [f](auto m) mutable { return f(std::forward<decltype(m)>(m)); }));
    }
};

