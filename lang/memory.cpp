#include "memory.h"

template <typename T>
void Vector<T>::clear()
{
    for(int i=0; i<sz; i++)
        data[i].~T();
    sz = 0;
}

template <typename T>
void Vector<T>::resize(int newcap)
{
    void* raw = ::operator new(sizeof(T)*newcap);
    T* newdata = static_cast<T*>(raw);
    for(int i=0; i<sz; i++)
    {
        new (&newdata[i]) T(move(data[i]));
        data[i].~T();
    }

    ::operator delete(data);
    data = newdata;
    cap = newcap;    
}

template <typename T>
Vector<T>& Vector<T>::operator=(const Vector& other)
{
    if(this == &other)
        return *this;

    clear();
    if(cap<other.sz)
    {
        ::operator delete(data);
        void* raw = ::operator new(sizeof(T)*other.sz);
        data = static_cast<T*>(raw);
        cap = other.sz;
    }

    for(int i=0; i<other.sz; i++)
        new (&data[i]) T(other.data[i]);

    size = other.sz;
    return *this;
}

template <typename T>
Vector<T>& Vector<T>::operator=(Vector&& other)
{
    if(this==&other)
        return *this;

    clear();
    ::operator delete(data);
    data = other.data;
    sz = other.sz;
    cap = other.cap;

    other.data = nullptr;
    other.sz = 0;
    other.cap = 0;

    return *this;
}

template <typename T>
void Vector<T>::push_back(const T& val)
{
    if(sz == cap)
    {
        int newcap = (cap==0)? 1: cap*2;
        resize(newcap);
    }

    new (&data[sz]) T(val);
    sz++;
}

template <typename T>
void Vector<T>::push_back(T&& val)
{
    if(sz == cap)
    {
        int newcap = (cap==0)? 1: cap*2;
        resize(newcap);
    }

    new (&data[sz]) T(move(val));
    sz++;
}

template <typename T>
void Vector<T>::pop_back()
{
    if(sz==0)
        throw std::out_of_range("vector is empty");
    sz--;
    data[sz].~T();
}

template <typename T>
int Vector<T>::size() const
{
    return sz;
}

template <typename T>
bool Vector<T>::empty() const
{
    return sz==0;
}

template <typename T>
int Vector<T>::capacity() const
{
    return cap;
}

template <typename T>
T& Vector<T>::get(int index)
{
    if(index<0 || index>=sz)
        throw std::out_of_range("index out of bounds");
    return data[index];
}

template <typename T>
const T& Vector<T>::get(int index) const
{
    if(index<0 || index>=sz)
        throw std::out_of_range("index out of bounds");
    return data[index];
}

template <typename T>
T& Vector<T>::operator[](int index)
{
    return data[index];
}

template <typename T>
const T& Vector<T>::operator[](int index) const
{
    return data[index];
}

template <typename T>
void Vector<T>::swap(Vector<T>& other) noexcept
{
    std::swap(data, other.data);
    std::swap(sz, other.sz);
    std::swap(cap, other.cap);
}

template <typename T>
void Vector<T>::reserve(int newcap)
{
    if(newcap <= cap)
        return;
    
    void* raw = ::operator new(sizeof(T)*newcap);
    T* newdata = static_cast<T*>(raw);
    int i = 0;
    try
    {
        for(; i<sz; i++)
            new (&newdata[i]) T(move_if_noexcept(data[i]));
    } catch(...)
    { //retreat
        for(int j=0; j<i; j++)
            newdata[j].~T();
        ::operator delete(newdata);
        throw;
    }

    for(int i=0; i<sz; i++)
        data[i].~T();

    ::operator delete(data);
    data = newdata;
    cap = newcap;
}

template <typename T>
void Vector<T>::insert(int pos, const T& val)
{
    if(pos<0 || pos>sz)
        throw std::out_of_range("index out of bounds");
    if(sz==cap)
    {
        int newcap = (cap==0)? 1: cap*2;
        resize(newcap);
    }

    for(int i=sz; i>pos; i--)
    {
        new (&data[i]) T(move(data[i-1]));
        data[i-1].~T();
    }

    new (&data[pos]) T(val);
    sz++;
}

template <typename T>
void Vector<T>::erase(int pos)
{
    if(pos<0 || pos>=sz)
        throw std::out_of_range("index out of bounds");
    data[pos].~T();

    for(int i=pos; i<sz-1; i++)
        data[i] = move(data[i+1]);
    
    data[sz-1].~T();
    sz--;
}