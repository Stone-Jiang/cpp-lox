#pragma once
#include "value.h"

class Table
{
    friend class VM;
private:
    std::unordered_map<string, Value> m;
public:
    Table();
    ~Table() = default;   

    std::optional<Value> find(const string& key) const;
    bool get(const string& key, Value& value);
    bool set(const string& key, Value val);
    bool del(const string& key);
    size_t size() const;
};
