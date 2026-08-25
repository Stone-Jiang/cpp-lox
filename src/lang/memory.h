#pragma once

#include <cstddef>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

class VM;
void prepareAllocation(VM* owner, size_t oldSize, size_t newSize);
void trackAllocation(VM* owner, size_t oldSize, size_t newSize);

template <typename T>
class Vector
{
    friend class VM;
private:
    static constexpr size_t VECTOR_MAX = std::numeric_limits<size_t>::max() / sizeof(T);
    T* arr = nullptr;
    size_t cap = 0, sz = 0;
    VM* owner = nullptr;

    void reportCapacityChange(size_t oldCap, size_t newCap)
    {
        if(owner != nullptr)
            trackAllocation(owner, oldCap * sizeof(T), newCap * sizeof(T));
    }

    size_t nextCapacity() const
    {
        if(cap == VECTOR_MAX)
            throw std::length_error("vector too big");
        if(cap == 0)
            return 1;
        return cap > VECTOR_MAX / 2 ? VECTOR_MAX : cap * 2;
    }

public:
    void clear()
    {
        for(size_t i=0; i<sz; i++)
            arr[i].~T();
        sz = 0;
    }

    void expand(size_t newcap)
    {
        reserve(newcap);
    }

    Vector() = default;
    explicit Vector(VM* vm): owner(vm) {}

    ~Vector()
    {
        const size_t oldCap = cap;
        clear();
        ::operator delete(arr);
        arr = nullptr;
        cap = 0;
        reportCapacityChange(oldCap, 0);
    }

    Vector(const Vector& other): owner(other.owner)
    {
        if(other.sz>0)
        {
            prepareAllocation(owner, 0, sizeof(T) * other.sz);
            void* raw = ::operator new(sizeof(T)*other.sz);
            arr = static_cast<T*>(raw);
            cap = other.sz;

            try
            {
                for(; sz<other.sz; ++sz)
                    new (&arr[sz]) T(other.arr[sz]);
            }
            catch(...)
            {
                clear();
                ::operator delete(arr);
                arr = nullptr;
                cap = 0;
                throw;
            }

            reportCapacityChange(0, cap);
        }
    }

    Vector(Vector&& other) noexcept: arr(other.arr), cap(other.cap), sz(other.sz), owner(other.owner)
    {
        other.arr = nullptr;
        other.sz = 0;
        other.cap = 0;
        other.owner = nullptr;
    }

    Vector& operator=(const Vector& other)
    {
        if(this == &other)
            return *this;

        Vector temp(other);
        swap(temp);
        return *this;
    }

    Vector& operator=(Vector&& other)
    {
        if(this==&other)
            return *this;

        Vector temp(std::move(other));
        swap(temp);
        return *this;
    }

    void push_back(const T& val)
    {
        if(sz == cap)
            expand(nextCapacity());

        new (&arr[sz]) T(val);
        sz++;
    }

    void push_back(T&& val)
    {
        if(sz == cap)
            expand(nextCapacity());

        new (&arr[sz]) T(std::move(val));
        sz++;
    }

    void pop_back()
    {
        if(sz==0)
            throw std::out_of_range("vector is empty");
        sz--;
        arr[sz].~T();
    }

    size_t size() const
    {
        return sz;
    }

    bool empty() const
    {
        return sz==0;
    }

    size_t capacity() const
    {
        return cap;
    }

    void resize(size_t count)
    {
        if(count == sz)
            return;

        if(count > sz)
        {
            if(count > cap)
                reserve(count);

            while(sz < count)
            {
                new (&arr[sz]) T();
                ++sz;
            }
            return;
        }

        while(sz > count)
            pop_back();
    }

    void shrink_to_fit()
    {
        if(sz == 0)
        {
            const size_t oldCap = cap;
            clear();
            ::operator delete(arr);
            arr = nullptr;
            cap = 0;
            reportCapacityChange(oldCap, 0);
            return;
        }

        if(sz == cap)
            return;

        T* newdata = static_cast<T*>(::operator new(sizeof(T) * sz));
        size_t i = 0;

        try
        {
            for(; i<sz; ++i)
                new (&newdata[i]) T(std::move_if_noexcept(arr[i]));
        }
        catch(...)
        {
            for(size_t j = 0; j < i; ++j)
                newdata[j].~T();
            ::operator delete(newdata);
            throw;
        }

        for(size_t i = 0; i < sz; ++i)
            arr[i].~T();

        const size_t oldCap = cap;
        ::operator delete(arr);
        arr = newdata;
        cap = sz;
        reportCapacityChange(oldCap, cap);
    }

    T& get(size_t index)
    {
        if(index >= sz)
            throw std::out_of_range("index out of bounds");
        return arr[index];
    }

    const T& get(size_t index) const
    {
        if(index >= sz)
            throw std::out_of_range("index out of bounds");
        return arr[index];
    }

    T& at(size_t index)
    {
        return get(index);
    }

    const T& at(size_t index) const
    {
        return get(index);
    }

    T& operator[](size_t index)
    {
        return arr[index];
    }

    const T& operator[](size_t index) const
    {
        return arr[index];
    }

    T& front()
    {
        return get(0);
    }

    const T& front() const
    {
        return get(0);
    }

    T& back()
    {
        return get(sz-1);
    }

    const T& back() const
    {
        return get(sz-1);
    }

    T* data()
    {
        return arr;
    }

    const T* data() const
    {
        return arr;
    }

    void print(std::ostream& os = std::cout) const
    {
        os<<"[";
        for(size_t i=0; i<sz; i++)
        {
            os<<arr[i];
            if(i+1 < sz)
                os<<", ";
        }
        os<<"]";
    }

    friend std::ostream& operator<<(std::ostream& os, const Vector& vec)
    {
        vec.print(os);
        return os;
    }

    void swap(Vector& other)
    {
        const size_t oldCap = cap;
        const size_t otherOldCap = other.cap;

        std::swap(arr, other.arr);
        std::swap(sz, other.sz);
        std::swap(cap, other.cap);

        if(owner != other.owner)
        {
            reportCapacityChange(oldCap, cap);
            other.reportCapacityChange(otherOldCap, other.cap);
        }
    }

    void reserve(size_t newcap);

    void insert(int pos, const T& val)
    {
        if(pos < 0 || static_cast<size_t>(pos) > sz)
            throw std::out_of_range("index out of bounds");
        if(sz==cap)
            expand(nextCapacity());

        for(size_t i=sz; i>static_cast<size_t>(pos); --i)
        {
            new (&arr[i]) T(std::move(arr[i-1]));
            arr[i-1].~T();
        }

        new (&arr[pos]) T(val);
        sz++;
    }

    void erase(int pos)
    {
        if(pos < 0 || static_cast<size_t>(pos) >= sz)
            throw std::out_of_range("index out of bounds");

        const size_t index = static_cast<size_t>(pos);
        for(size_t i=index; i+1<sz; ++i)
            arr[i] = std::move(arr[i+1]);

        --sz;
        arr[sz].~T();
    }

    template <bool IsConst>
    class BasicIterator
    {
        template <bool>
        friend class BasicIterator;

    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<IsConst, const T*, T*>;
        using reference = std::conditional_t<IsConst, const T&, T&>;
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;
    
    private:
        pointer ptr = nullptr;

    public:
        BasicIterator() = default;
        explicit BasicIterator(pointer p): ptr(p) {}

        template <bool OtherConst>
        requires (IsConst || !OtherConst)
        BasicIterator(const BasicIterator<OtherConst>& other): ptr(other.ptr) {}

        reference operator*() const
        {
            return *ptr;
        }

        pointer operator->() const
        {
            return ptr;
        }

        BasicIterator& operator++()
        {
            ptr++;
            return *this;
        }

        BasicIterator& operator--()
        {
            ptr--;
            return *this;
        }

        BasicIterator operator++(int)
        {
            BasicIterator temp = *this;
            ++(*this);
            return temp;
        }

        BasicIterator operator--(int)
        {
            BasicIterator temp = *this;
            --(*this);
            return temp;
        }

        BasicIterator& operator+=(difference_type n)
        {
            ptr += n;
            return *this;
        }

        BasicIterator& operator-=(difference_type n)
        {
            ptr -= n;
            return *this;
        }

        BasicIterator operator+(difference_type n) const
        {
            BasicIterator temp = *this;
            temp += n;
            return temp;
        }

        BasicIterator operator-(difference_type n) const
        {
            BasicIterator temp = *this;
            temp -= n;
            return temp;
        }

        difference_type operator-(const BasicIterator& other) const
        {
            return ptr - other.ptr;
        }

        reference operator[](difference_type n) const
        {
            return ptr[n];
        }

        friend BasicIterator operator+(difference_type n, BasicIterator iterator)
        {
            iterator += n;
            return iterator;
        }

        bool operator==(const BasicIterator& other) const {return ptr == other.ptr;}
        bool operator!=(const BasicIterator& other) const {return ptr != other.ptr;}
        bool operator<(const BasicIterator& other) const {return ptr < other.ptr;}
        bool operator>(const BasicIterator& other) const {return ptr > other.ptr;}
        bool operator<=(const BasicIterator& other) const {return ptr <= other.ptr;}
        bool operator>=(const BasicIterator& other) const {return ptr >= other.ptr;}
    };

    using Iterator = BasicIterator<false>;
    using ConstIterator = BasicIterator<true>;

    Iterator begin()
    {
        return Iterator(arr);
    }

    Iterator end()
    {
        return Iterator(arr == nullptr ? nullptr : arr+sz);
    }

    ConstIterator begin() const
    {
        return ConstIterator(arr);
    }

    ConstIterator end() const
    {
        return ConstIterator(arr == nullptr ? nullptr : arr+sz);
    }

    ConstIterator cbegin() const
    {
        return begin();
    }

    ConstIterator cend() const
    {
        return end();
    }
};

template <typename T>
void Vector<T>::reserve(size_t newcap)
{
    if(newcap > VECTOR_MAX)
        throw std::length_error("vector too big");
    if(newcap <= cap)
        return;

    prepareAllocation(owner, cap * sizeof(T), newcap * sizeof(T));
    void* raw = ::operator new(sizeof(T) * newcap);
    T* newdata = static_cast<T*>(raw);
    size_t i = 0;
    try
    {
        for(; i<sz; ++i)
            new (&newdata[i]) T(std::move_if_noexcept(arr[i]));
    }
    catch(...)
    {
        for(size_t j = 0; j < i; ++j)
            newdata[j].~T();
        ::operator delete(newdata);
        throw;
    }

    for(size_t i = 0; i < sz; ++i)
        arr[i].~T();

    const size_t oldCap = cap;
    ::operator delete(arr);
    arr = newdata;
    cap = newcap;
    reportCapacityChange(oldCap, cap);
}
