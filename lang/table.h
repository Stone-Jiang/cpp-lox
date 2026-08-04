#pragma once
#include "value.h"

#ifdef BETTER_HASH_TABLE
#include "robinhood.h"
#endif

class Table
{
    friend class VM;
private:
    #ifdef BETTER_HASH_TABLE
    robin_hood::unordered_flat_map<string, Value, robin_hood::hash<string>, std::equal_to<string>, MAX_LOAD_FACTOR_I> m;
    #else
    std::unordered_map<string, Value> m;
    #endif
public:
    Table();
    ~Table() = default;   

    std::optional<Value> find(const string& key) const;
    bool get(const string& key, Value& value);
    bool set(const string& key, Value val);
    bool del(const string& key);
    size_t size() const;
};
