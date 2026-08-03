#include "table.h"
#include "value.h"

Table::Table()
{
    m.max_load_factor(MAX_LOAD_FACTOR);
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
    bool flag = (m.find(key)==m.end());
    m[key] = val;
    return flag;
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