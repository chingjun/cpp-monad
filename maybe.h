#pragma once

template <typename T>
class maybe
{
    char buffer[sizeof(T)];
    bool _has_data;
    
    void destroy()
    {
        if (has_data())
        {
            get().~T();
        }
        _has_data = false;
    }
public:
    maybe() : _has_data(false) {}
    template <typename... P>
    maybe(P... params) : _has_data(true)
    {
        new (buffer) T(std::forward<P>(params)...);
    }
    maybe(const maybe &other)
    {
        _has_data = other.has_data();
        if (other.has_data())
        {
            new (buffer) T(other.get());
        }
    }
    maybe(maybe &&other)
    {
        _has_data = other.has_data();
        if (other.has_data())
        {
            new (buffer) T(std::move(other.get()));
        }
    }
    maybe & operator =(const maybe &other)
    {
        if (other.has_data())
        {
            if (!has_data())
            {
                new (buffer) T(other.get());
            }
            else
            {
                get() = other.get();
            }
        }
        else
        {
            destroy();
        }
        _has_data = other.has_data();
        
        return *this;
    }
    maybe & operator =(maybe &&other)
    {
        if (other.has_data())
        {
            if (!has_data())
            {
                new (buffer) T(std::move(other.get()));
            }
            else
            {
                get() = std::move(other.get());
            }
        }
        else
        {
            destroy();
        }
        _has_data = other.has_data();
        
        return *this;
    }
    ~maybe()
    {
        destroy();
    }

    template <typename... P>
    maybe &initialize(P... params)
    {
        destroy();
        _has_data = true;
        new (buffer) T(std::forward<P>(params)...);

        return *this;
    }
    
    template <typename P>
    maybe &assign(P val)
    {
        if (has_data())
        {
            get() = std::forward<P>(val);
        }
        else
        {
            new (buffer) T(std::forward<P>(val));
            _has_data = true;
        }
        
        return *this;
    }
    
    T &get() { mr_assert(has_data()); return *(T *) buffer; }
    const T &get() const { mr_assert(has_data()); return *(T *) buffer; }
    bool has_data() const { return _has_data; }
};


// monad implementation
#include "monad.h"

template <typename T>
struct monad<maybe<T>>
{
    using ElemType = T;
    template <typename U> using OtherType = maybe<U>;
    using M = maybe<T>;
    static const bool has_monad = true;
    template <typename Func>
    static auto fmap(const M &from, Func f)
    {
        using From = T;
        using To = decltype(f(std::declval<T>()));
        using RetType = maybe<To>;
        RetType ret;
        if (from.has_data())
        {
            ret = f(from.get());
        }
        return ret;
    }

    static M join(maybe<M> v)
    {
        M ret;
        if (v.has_data())
        {
            ret = v.get();
        }
        return ret;
    }

    static M wrap(T e)
    {
        return e;
    }

    template <typename Func>
    static auto bind(M p, Func f)
    {
        using RetType = decltype(f(std::declval<T>()));
        return monad<RetType>::join(fmap(p, [f](auto m) mutable { return f(std::forward<decltype(m)>(m)); }));
    }
};
