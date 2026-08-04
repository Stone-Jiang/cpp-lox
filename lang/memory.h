#pragma once
#include "commons.h"

template <typename T>
class Vector
{
private:
    T* data;
    int cap, sz;

    void clear();
    void resize(int newcap);

public:
    Vector(): data(nullptr), cap(0), sz(0) {}

    ~Vector()
    {
        clear();
        ::operator delete(data);
    }

    Vector(const Vector& other): data(nullptr), cap(0), sz(0)
    {
        if(other.sz>0)
        {
            void* raw = ::operator new(sizeof(T)*other.sz);
            data = static_cast<T*>(raw);
            cap = other.sz;

            for(int i=0; i<other.sz; i++)
                new (&data[i]) T(other.data[i]);
            sz = other.sz;
        }
    }

    Vector(Vector&& other) noexcept: data(other.data), cap(other.cap), sz(other.sz)
    {
        other.data = nullptr;
        other.sz = 0;
        other.cap = 0;
    }

    Vector& operator=(const Vector& other);
    Vector& operator=(Vector&& other);

    void push_back(const T& val);
    void push_back(T&& val);
    void pop_back();

    int size() const;
    bool empty() const;
    int capacity() const;

    T& get(int index);
    const T& get(int index) const;
    T& operator[](int index);
    const T& operator[](int index) const;

    void print(std::ostream& os = std::cout) const
    {
        os<<"[";
        for(int i=0; i<sz; i++)
        {
            os<<data[i];
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

    void swap(Vector& other) noexcept;
    void reserve(int newcap);
    void insert(int pos, const T& val);
    void erase(int pos);

    class Iterator
    {
    public:
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        using iterator_category = std::random_access_iterator_tag;
    
    private:
        T* ptr;
    public:
        Iterator(T* p = nullptr): ptr(p) {}

        T& operator*() const
        {
            return *ptr;
        }

        T* operator->() const
        {
            return ptr;
        }

        Iterator& operator++()
        {
            ptr++;
            return *this;
        }

        Iterator& operator--()
        {
            ptr--;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        Iterator operator--(int)
        {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        Iterator operator+(int n) const
        {
            return Iterator(ptr+n);
        }

        Iterator operator-(int n) const
        {
            return Iterator(ptr-n);
        }

        ptrdiff_t operator-(const Iterator& other) const
        {
            return ptr - other.ptr;
        }

        bool operator==(const Iterator& other){return ptr == other.ptr;}
        bool operator!=(const Iterator& other){return ptr != other.ptr;}
        bool operator<(const Iterator& other) {return ptr<other.ptr;}
        bool operator>(const Iterator& other) {return ptr>other.ptr;}
        bool operator<=(const Iterator& other) {return ptr<=other.ptr;}
        bool operator>=(const Iterator& other) {return ptr>=other.ptr;}
    };

    Iterator begin() const
    {
        return Iterator(data);
    }

    Iterator end() const
    {
        return Iterator(data+sz);
    }
};