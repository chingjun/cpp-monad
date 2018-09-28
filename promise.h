#pragma once

#include <functional>
#include <deque>
#include <vector>
#include <memory>
#include <utility>

#include "maybe.h"
#include "generic_holder.h"

class base_promise
{
public:
    virtual ~base_promise() = default;
};
using base_promise_ptr = std::shared_ptr<base_promise>;

using promise_holder = generic_holder<base_promise_ptr>;

template <typename T>
class promise : public base_promise, public std::enable_shared_from_this<promise<T>>
{
    using resolve_cb = std::function<void(const T &)>;
    using void_cb = std::function<void()>;
    
    maybe<T> data;
    
    mutable resolve_cb then_cb; // optimization when only one `then`
    mutable std::deque<resolve_cb> other_then_cbs;
    
    void_cb finally_cb; // optimization when only one `finally_cb`
    std::deque<void_cb> other_finally_cbs;
    
    promise_holder holder;
    
    promise() = default;
    promise(T data) : data(std::move(data)) {}
    DISALLOW_COPY_AND_ASSIGN(promise);
    
    void call_finally()
    {
        // call in reverse order
        for (auto &cb : other_finally_cbs)
        {
            cb();
        }
        other_finally_cbs.clear();
        
        if (finally_cb != nullptr)
        {
            finally_cb();
        }
        finally_cb = nullptr;
    }

public:
    using ResultType = T;
    
    ~promise()
    {
        call_finally();
    }
    
    void then(resolve_cb cb) const
    {
        if (data.has_data())
        {
            cb(result());
        }
        else if (then_cb == nullptr)
        {
            then_cb = std::move(cb);
        }
        else
        {
            other_then_cbs.emplace_back(std::move(cb));
        }
    }
    
    void finally(void_cb cb)
    {
        // strictly for cleanup only
        // reserved to be used by promise owner only
        mr_assert(!data.has_data());
        
        if (finally_cb == nullptr)
        {
            finally_cb = cb;
        }
        else
        {
            other_finally_cbs.emplace_front(std::move(cb));
        }
    }
    
    template <typename... P>
    void resolve(P... params)
    {
        mr_assert(!data.has_data());
        data.initialize(std::forward<P>(params)...);
        
        if (then_cb != nullptr)
        {
            then_cb(result());
        }
        for (auto &cb : other_then_cbs)
        {
            cb(result());
        }
        
        call_finally();
        
        then_cb = nullptr;
        other_then_cbs.clear();
    }
    
    void hold_promise(base_promise_ptr promise)
    {
        holder.hold(promise);
    }

    T &result() { return data.get(); }
    const T &result() const { return data.get(); }
    bool is_finished() const { return data.has_data(); }
    
    std::weak_ptr<promise> get_weak()
    {
        return std::weak_ptr<promise>(this->shared_from_this());
    }
    
    static std::shared_ptr<promise> create()
    {
        return std::shared_ptr<promise>(new promise());
    }

    static std::shared_ptr<promise> create(T data)
    {
        return std::shared_ptr<promise>(new promise(std::move(data)));
    }
};

template <typename T>
using promise_ptr = std::shared_ptr<promise<T>>;
template <typename T>
using promise_weak_ptr = std::weak_ptr<promise<T>>;


// monad implementation
#include "monad.h"

template <typename T>
struct monad<promise_ptr<T>>
{
    using ElemType = T;
    template <typename U> using OtherType = promise_ptr<U>;
    using M = promise_ptr<T>;
    static const bool has_monad = true;
    template <typename Func>
    static auto fmap(M from, Func f)
    {
        using From = T;
        using To = decltype(f(std::declval<T>()));
        auto ret_promise = promise<To>::create();
        promise_weak_ptr<To> weak_ret_promise(ret_promise);

        from->then([weak_ret_promise, f](const From &data) mutable {
            auto p = weak_ret_promise.lock();
            if (p)
            {
                p->resolve(f(data));
            }
        });
        ret_promise->hold_promise(from);

        return ret_promise;
    }

    static M join(promise_ptr<M> p)
    {
        auto ret_promise = promise<T>::create();
        promise_weak_ptr<T> weak_ret_promise(ret_promise);
        p->then([weak_ret_promise](const promise_ptr<T> &inner_p) {
            auto ret_promise = weak_ret_promise.lock();
            if (ret_promise)
            {
                inner_p->then([weak_ret_promise](const T &data) {
                    auto ret_promise = weak_ret_promise.lock();
                    if (ret_promise)
                    {
                        ret_promise->resolve(data);
                    }
                });
                ret_promise->hold_promise(inner_p);
            }
        });
        ret_promise->hold_promise(p);
        return ret_promise;
    }

    static M wrap(T obj)
    {
        return promise<T>::create(std::move(obj));
    }

    template <typename Func>
    static auto bind(M p, Func f)
    {
        using RetType = decltype(f(std::declval<T>()));
        return monad<RetType>::join(fmap(p, [f](auto m) mutable { return f(std::forward<decltype(m)>(m)); }));
    }
};


// special monad for promise<result>
#include "result.h"

template <typename T, typename E>
struct monad<promise_ptr<result<T, E>>>
{
    using ElemType = T;
    template <typename U> using OtherType = promise_ptr<result<U, E>>;
    using M = promise_ptr<result<T, E>>;
    using Result = result<T, E>;

    static const bool has_monad = true;
    template <typename Func>
    static auto fmap(M from, Func f)
    {
        using From = T;
        using To = decltype(f(std::declval<T>()));
        using RetType = result<To, E>;
        auto ret_promise = promise<RetType>::create();
        promise_weak_ptr<RetType> weak_ret_promise(ret_promise);

        from->then([weak_ret_promise, f](const Result &data) mutable {
            auto p = weak_ret_promise.lock();
            if (p)
            {
                if (data.is_ok())
                {
                    p->resolve(RetType(Ok, f(data.ok())));
                }
                else
                {
                    mr_assert(data.is_error());
                    p->resolve(RetType(Error, data.error()));
                }
            }
        });
        ret_promise->hold_promise(from);

        return ret_promise;
    }

    static M join(promise_ptr<result<M, E>> p)
    {
        auto ret_promise = promise<Result>::create();
        promise_weak_ptr<Result> weak_ret_promise(ret_promise);
        p->then([weak_ret_promise](const result<promise_ptr<Result>, E> &inner_p) {
            auto ret_promise = weak_ret_promise.lock();
            if (ret_promise)
            {
                if (inner_p.is_ok())
                {
                    inner_p.ok()->then([weak_ret_promise](const Result &data) {
                        auto ret_promise = weak_ret_promise.lock();
                        if (ret_promise)
                        {
                            if (data.is_ok())
                            {
                                ret_promise->resolve(Result(Ok, data.ok()));
                            }
                            else
                            {
                                mr_assert(data.is_error());
                                ret_promise->resolve(Result(Error, data.error()));
                            }
                        }
                    });
                    ret_promise->hold_promise(inner_p.ok());
                }
                else
                {
                    mr_assert(inner_p.is_error());
                    ret_promise->resolve(Result(Error, inner_p.error()));
                }
            }
        });
        ret_promise->hold_promise(p);
        return ret_promise;
    }

    static M wrap(T obj)
    {
        return promise<Result>::create(Result(Ok, std::move(obj)));
    }

    template <typename Func>
    static auto bind(M p, Func f)
    {
        using RetType = decltype(f(std::declval<T>()));
        return monad<RetType>::join(fmap(p, [f](auto m) mutable { return f(std::forward<decltype(m)>(m)); }));
    }
};
