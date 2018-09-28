#pragma once

#include <memory>
#include <list>
#include <functional>

#include "generic_holder.h"

class base_observable
{
public:
    virtual ~base_observable() = default;
};
using base_observable_ptr = std::shared_ptr<base_observable>;

class base_observable_callback_handle
{
public:
    virtual ~base_observable_callback_handle() = default;
};
using base_observable_callback_handle_ptr = std::unique_ptr<base_observable_callback_handle>;
using observable_callback_handle_holder = generic_holder<base_observable_callback_handle_ptr>;

// listener list
template <typename T>
class observable_callback_list
{
    using handler_t = typename std::list<T>::iterator;
public:
    struct handle : public base_observable_callback_handle
    {
        observable_callback_list *arr;
        handler_t handler;
        base_observable_ptr observable;
        
        handle() : arr(nullptr) {}
        handle(base_observable_ptr observable, observable_callback_list *arr, handler_t handler) : observable(observable), arr(arr), handler(handler) {}
        handle(handle &&other) : observable(std::move(other.observable)), arr(other.arr), handler(other.handler)
        {
            other.arr = nullptr;
            other.observable.reset();
        }
        handle &operator = (handle &&other)
        {
            release();
            observable = std::move(other.observable);
            arr = other.arr;
            handler = other.handler;
            other.arr = nullptr;
            other.observable.reset();
            return *this;
        }
        handle(const handle &) = delete;
        handle &operator =(const handle &) = delete;
        
        ~handle()
        {
            release();
        }
        
        void release()
        {
            if (arr != nullptr)
            {
                arr->remove(handler);
                arr = nullptr;
            }
            observable.reset();
        }

        std::unique_ptr<handle> to_ptr()
        {
            std::unique_ptr<handle> ret(new handle(std::move(*this)));
            return ret;
        }
    };
    
    handle add(base_observable_ptr observable, T item)
    {
        _list.push_front(item);
        return handle(observable, this, _list.begin());
    }
    
    const std::list<T> &get()
    {
        return _list;
    }
    
    void clear_nullptr()
    {
        auto it = _list.begin();
        auto next = it;
        while (it != _list.end())
        {
            next = std::next(it);
            if (*it == nullptr)
            {
                _list.erase(it);
            }
            it = next;
        }
    }
    
    int _in_fire_count = 0;
    bool _unregistered_during_fire = false;
    
private:
    
    void remove(handler_t handle)
    {
        if (_in_fire_count == 0)
        {
            _list.erase(handle);
        }
        else
        {
            *handle = nullptr;
            _unregistered_during_fire = true;
        }
    }
    
    std::list<T> _list;
};

template <typename T>
class observable : public base_observable, public std::enable_shared_from_this<observable<T>>
{
    using data_cb = std::function<void(const T &)>;
    using void_cb = std::function<void()>;

    T data;
    mutable observable_callback_list<data_cb> callbacks;

    observable_callback_handle_holder callback_holder;
    
    void_cb finally_cb; // for cleanup
    
    DISALLOW_COPY_AND_ASSIGN(observable);
    
    void call_finally()
    {
        if (finally_cb != nullptr)
        {
            finally_cb();
        }
        finally_cb = nullptr;
    }

public:
    using ResultType = T;
    using CallbackHandle = typename observable_callback_list<data_cb>::handle;

    observable() = default;
    observable(T data) : data(std::move(data)) {}
    ~observable()
    {
        call_finally();
    }

    CallbackHandle observe(data_cb cb, bool call_with_initial_value = false)
    {
        if (call_with_initial_value)
        {
            cb(get());
        }
        return callbacks.add(this->shared_from_this(), cb);
    }
    
    void finally(void_cb cb)
    {
        // strictly for cleanup only
        // reserved to be used by observable owner only
        mr_assert(finally_cb == nullptr);
        
        finally_cb = cb;
    }

    void push(T new_data)
    {
        data = std::move(new_data);
        
        callbacks._in_fire_count++;
        auto it = callbacks.get().begin();
        auto next = it;
        while (it != callbacks.get().end())
        {
            next = std::next(it);
            if (*it != nullptr)
            {
                try
                {
                    (*it)(data);
                }
                catch (const std::bad_function_call &e)
                {
                    assert(false);
                }
            }
            it = next;
        }
        callbacks._in_fire_count--;
        
        if (callbacks._in_fire_count == 0 && callbacks._unregistered_during_fire)
        {
            callbacks.clear_nullptr();
            callbacks._unregistered_during_fire = false;
        }
    }

    const T &get()
    {
        return data;
    }

    void hold_handle(base_observable_callback_handle_ptr ptr)
    {
        callback_holder.hold(std::move(ptr));
    }
    
    void clear_all_held_handles()
    {
        callback_holder.clear();
    }

    int observer_count()
    {
        return callbacks.get().size();
    }
    
    std::weak_ptr<observable> get_weak()
    {
        return std::weak_ptr<observable>(this->shared_from_this());
    }

    static std::shared_ptr<observable> create()
    {
        return std::shared_ptr<observable>(new observable());
    }

    static std::shared_ptr<observable> create(T data)
    {
        return std::shared_ptr<observable>(new observable(std::move(data)));
    }
};

template <typename T>
using observable_ptr = std::shared_ptr<observable<T>>;
template <typename T>
using observable_weak_ptr = std::weak_ptr<observable<T>>;


// monad implementation
#include "monad.h"

template <typename T>
struct monad<observable_ptr<T>>
{
    using ElemType = T;
    template <typename U> using OtherType = observable_ptr<U>;
    using M = observable_ptr<T>;
    static const bool has_monad = true;
    template <typename Func>
    static auto fmap(M from, Func f)
    {
        using From = T;
        using To = decltype(f(std::declval<T>()));
        auto ret_observable = observable<To>::create(f(from->get()));
        observable_weak_ptr<To> weak_ret_observable(ret_observable);

        ret_observable->hold_handle(from->observe([weak_ret_observable, f](const From &data) mutable {
            auto o = weak_ret_observable.lock();
            if (o)
            {
                o->push(f(data));
            }
        }).to_ptr());

        return ret_observable;
    }

    static M join(observable_ptr<M> o)
    {
        auto ret_observable = observable<T>::create(o->get()->get());
        observable_weak_ptr<T> weak_ret_observable(ret_observable);
        std::shared_ptr<observable_callback_handle_holder> inner_handle_holder = std::make_shared<observable_callback_handle_holder>();
        ret_observable->hold_handle(o->observe([inner_handle_holder, weak_ret_observable](const observable_ptr<T> &inner_o) mutable {
            inner_handle_holder->clear();
            auto ret_observable = weak_ret_observable.lock();
            if (ret_observable) {
                inner_handle_holder->hold(inner_o->observe([weak_ret_observable](const T &data) {
                    auto ret_observable = weak_ret_observable.lock();
                    if (ret_observable)
                    {
                        ret_observable->push(data);
                    }
                }, true).to_ptr());
            }
        }, true).to_ptr());
        return ret_observable;
    }

    static M wrap(T obj)
    {
        auto ret_observable = observable<T>::create(std::move(obj));
        return ret_observable;
    }

    template <typename Func>
    static auto bind(M p, Func f)
    {
        using RetType = decltype(f(std::declval<T>()));
        return monad<RetType>::join(fmap(p, [f](auto m) mutable { return f(std::forward<decltype(m)>(m)); }));
    }
};
