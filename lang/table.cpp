#include "table.h"
#include "value.h"

Table::Table()
{
    #ifndef BETTER_HASH_TABLE
    m.max_load_factor(MAX_LOAD_FACTOR_F);
    #endif
}

std::optional<Value> Table::find(const string& key) const
{
    auto it = m.find(key);
    if(it==m.end())
        return std::nullopt;
    return std::make_optional<>(it->second);
}

bool Table::get(const string& key, Value& value)
{
    auto it = m.find(key);
    if(it==m.end())
        return false;
    value = it->second;
    return true;
}

bool Table::set(const string& key, Value val)
{
    auto [it, inserted] = m.insert_or_assign(key, val);
    return inserted;
}

bool Table::del(const string& key)
{
    auto num = m.erase(key);
    return num!=0;
}

size_t Table::size() const
{
    return m.size();
}

void Table::reserve(size_t cap)
{
    m.reserve(cap);
}