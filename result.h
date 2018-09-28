#pragma once

class _Ok {};
class _Error {};
constexpr _Ok Ok{};
constexpr _Error Error{};

template <typename Ok, typename Error>
class result
{
    char buffer[std::max(sizeof(Ok), sizeof(Error))];
    enum class e_which {
        NONE, OK, ERROR
    };
    e_which which;
    
    void destroy()
    {
        if (is_ok())
        {
            this->ok().~Ok();
        }
        else if (is_error())
        {
            this->error().~Error();
        }
        which = e_which::NONE;
    }
public:
    result() : which(e_which::NONE) {}
    template <typename... T>
    result(_Ok, T... params) : which(e_which::OK)
    {
        new (buffer) Ok(std::forward<T>(params)...);
    }
    template <typename... T>
    result(_Error, T... params) : which(e_which::ERROR)
    {
        new (buffer) Error(std::forward<T>(params)...);
    }
    result(const result &other)
    {
        which = other.which;
        if (other.is_ok())
        {
            new (buffer) Ok(other.ok());
        }
        else if (other.is_error())
        {
            new (buffer) Error(other.error());
        }
    }
    result(result &&other)
    {
        which = other.which;
        if (other.is_ok())
        {
            new (buffer) Ok(std::move(other.ok()));
        }
        if (other.is_error())
        {
            new (buffer) Error(std::move(other.error()));
        }
    }
    result & operator =(const result &other)
    {
        if (other.is_ok())
        {
            assign_ok(other.ok());
        }
        else if (other.is_error())
        {
            assign_error(other.error());
        }
        else
        {
            destroy();
        }
        
        return *this;
    }
    result & operator =(result &&other)
    {
        if (other.is_ok())
        {
            assign_ok(std::move(other.ok()));
        }
        else if (other.is_error())
        {
            assign_error(std::move(other.error()));
        }
        else
        {
            destroy();
        }
        
        return *this;
    }
    ~result()
    {
        destroy();
    }
    
    template <typename T>
    Ok &assign_ok(T val)
    {
        if (is_ok())
        {
            ok() = std::forward<T>(val);
        }
        else
        {
            destroy();
            new (buffer) Ok(std::forward<T>(val));
            which = e_which::OK;
        }
        
        return ok();
    }
    
    template <typename T>
    Error &assign_error(T val)
    {
        if (is_error())
        {
            error() = std::forward<T>(val);
        }
        else
        {
            destroy();
            new (buffer) Error(std::forward<T>(val));
            which = e_which::ERROR;
        }
        
        return error();
    }
    
    Ok &ok() { mr_assert(is_ok()); return *(Ok *) buffer; }
    Error &error() { mr_assert(is_error()); return *(Error *) buffer; }
    const Ok &ok() const { mr_assert(is_ok()); return *(Ok *) buffer; }
    const Error &error() const { mr_assert(is_error()); return *(Error *) buffer; }
    bool is_ok() const { return which == e_which::OK; }
    bool is_error() const { return which == e_which::ERROR; }
    bool is_none() const { return which == e_which::NONE; }
};
