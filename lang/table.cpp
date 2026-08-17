#include "table.h"
#include "value.h"

namespace
{
size_t saturatingAdd(size_t left, size_t right)
{
    const size_t max = std::numeric_limits<size_t>::max();
    return right > max - left ? max : left + right;
}
}

Table::Table(VM* vm): owner(vm)
{
    #ifndef BETTER_HASH_TABLE
    m.max_load_factor(MAX_LOAD_FACTOR_F);
    #endif
}

Table::~Table()
{
    const size_t storage = saturatingAdd(slotStorageBytes(accountedSlots), keyBytes);
    trackAllocation(owner, storage, 0);
}

size_t Table::entryKeyBytes(const string& key)
{
    return saturatingAdd(key.size(), 1);
}

size_t Table::slotStorageBytes(size_t slots)
{
    constexpr size_t slotSize = sizeof(string) + sizeof(Value) + sizeof(size_t);
    const size_t max = std::numeric_limits<size_t>::max();
    return slots > max / slotSize ? max : slots * slotSize;
}

size_t Table::slotsForEntries(size_t entries)
{
    if(entries == 0)
        return 0;

    const size_t max = std::numeric_limits<size_t>::max();
    const size_t numerator = entries > (max - (MAX_LOAD_FACTOR_I - 1)) / 100
        ? max
        : entries * 100 + (MAX_LOAD_FACTOR_I - 1);
    const size_t required = numerator / MAX_LOAD_FACTOR_I;

    size_t slots = 8;
    while(slots < required)
    {
        if(slots > max / 2)
            return max;
        slots *= 2;
    }
    return slots;
}

void Table::ensureCapacity(size_t entries)
{
    const size_t newSlots = slotsForEntries(entries);
    if(newSlots <= accountedSlots)
        return;

    const size_t oldBytes = slotStorageBytes(accountedSlots);
    const size_t newBytes = slotStorageBytes(newSlots);
    prepareAllocation(owner, oldBytes, newBytes);
    m.reserve(entries);
    trackAllocation(owner, oldBytes, newBytes);
    accountedSlots = newSlots;
}

void Table::releaseKeyBytes(size_t bytes)
{
    const size_t newBytes = bytes > keyBytes ? 0 : keyBytes - bytes;
    trackAllocation(owner, keyBytes, newBytes);
    keyBytes = newBytes;
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
    auto it = m.find(key);
    if(it != m.end())
    {
        it->second = val;
        return false;
    }

    ensureCapacity(m.size() + 1);

    const size_t bytes = entryKeyBytes(key);
    const size_t newKeyBytes = saturatingAdd(keyBytes, bytes);
    prepareAllocation(owner, keyBytes, newKeyBytes);
    m.emplace(key, val);
    trackAllocation(owner, keyBytes, newKeyBytes);
    keyBytes = newKeyBytes;
    return true;
}

bool Table::del(const string& key)
{
    auto it = m.find(key);
    if(it == m.end())
        return false;

    const size_t bytes = entryKeyBytes(it->first);
    m.erase(it);
    releaseKeyBytes(bytes);
    return true;
}

size_t Table::size() const
{
    return m.size();
}

void Table::reserve(size_t cap)
{
    ensureCapacity(cap);
}
