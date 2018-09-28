#pragma once

#include <vector>

template <typename T>
class generic_holder
{
    std::vector<T> objects;
public:
    generic_holder() = default;
    generic_holder(generic_holder &&other) = default;
    generic_holder &operator = (generic_holder &&other) = default;

    void hold(T object)
    {
        objects.emplace_back(std::move(object));
    }

    void clear()
    {
        objects.clear();
    }

    ~generic_holder()
    {
        clear();
    }
    
    DISALLOW_COPY_AND_ASSIGN(generic_holder);
};
