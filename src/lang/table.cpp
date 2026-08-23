#include "table.h"
#include "object.h"

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
    trackAllocation(owner, slotStorageBytes(accountedSlots), 0);
}

size_t Table::slotStorageBytes(size_t slots)
{
    constexpr size_t slotSize = sizeof(ObjString*) + sizeof(Value) + sizeof(size_t);
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

std::optional<Value> Table::find(ObjString* key) const
{
    auto it = m.find(key);
    if(it==m.end())
        return std::nullopt;
    return std::make_optional<>(it->second);
}

bool Table::get(ObjString* key, Value& value)
{
    auto it = m.find(key);
    if(it==m.end())
        return false;
    value = it->second;
    return true;
}

bool Table::set(ObjString* key, Value val)
{
    auto it = m.find(key);
    if(it != m.end())
    {
        it->second = val;
        return false;
    }

    ensureCapacity(m.size() + 1);

    m.emplace(key, val);
    return true;
}

bool Table::del(ObjString* key)
{
    auto it = m.find(key);
    if(it == m.end())
        return false;

    m.erase(it);
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

MemberTable::MemberTable(VM* vm): owner(vm)
{
    #ifndef BETTER_HASH_TABLE
    m.max_load_factor(MAX_LOAD_FACTOR_F);
    #endif
}

MemberTable::~MemberTable()
{
    trackAllocation(owner, slotStorageBytes(accountedSlots), 0);
}

size_t MemberTable::slotStorageBytes(size_t slots)
{
    constexpr size_t slotSize = sizeof(ObjString*) + sizeof(ClassMember) + sizeof(size_t);
    const size_t max = std::numeric_limits<size_t>::max();
    return slots > max / slotSize ? max : slots * slotSize;
}

size_t MemberTable::slotsForEntries(size_t entries)
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

void MemberTable::ensureCapacity(size_t entries)
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

std::optional<ClassMember> MemberTable::find(ObjString* key) const
{
    auto it = m.find(key);
    if(it == m.end())
        return std::nullopt;
    return std::make_optional<>(it->second);
}

bool MemberTable::get(ObjString* key, ClassMember& value)
{
    auto it = m.find(key);
    if(it == m.end())
        return false;
    value = it->second;
    return true;
}

bool MemberTable::set(ObjString* key, const ClassMember& val)
{
    auto it = m.find(key);
    if(it != m.end())
    {
        it->second = val;
        return false;
    }

    ensureCapacity(m.size() + 1);
    m.emplace(key, val);
    return true;
}

bool MemberTable::del(ObjString* key)
{
    auto it = m.find(key);
    if(it == m.end())
        return false;

    m.erase(it);
    return true;
}

size_t MemberTable::size() const
{
    return m.size();
}

void MemberTable::reserve(size_t cap)
{
    ensureCapacity(cap);
}

size_t InternHash::operator()(const ObjString* value) const noexcept
{
    return value->hash;
}

size_t InternHash::operator()(std::string_view value) const noexcept
{
    return std::hash<std::string_view>{}(value);
}

bool InternEqual::operator()(const ObjString* left, const ObjString* right) const noexcept
{
    return left == right || left->str() == right->str();
}

bool InternEqual::operator()(const ObjString* left, std::string_view right) const noexcept
{
    return left->str() == right;
}

bool InternEqual::operator()(std::string_view left, const ObjString* right) const noexcept
{
    return left == right->str();
}

StringPool::StringPool(VM* vm): owner(vm)
{
    #ifndef BETTER_HASH_TABLE
    m.max_load_factor(MAX_LOAD_FACTOR_F);
    #endif
}

StringPool::~StringPool()
{
    trackAllocation(owner, slotStorageBytes(accountedSlots), 0);
}

size_t StringPool::slotStorageBytes(size_t slots)
{
    constexpr size_t slotSize = sizeof(ObjString*) + sizeof(size_t);
    const size_t max = std::numeric_limits<size_t>::max();
    return slots > max / slotSize ? max : slots * slotSize;
}

size_t StringPool::slotsForEntries(size_t entries)
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

void StringPool::ensureCapacity(size_t entries)
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

ObjString* StringPool::find(std::string_view chars) const
{
    auto it = m.find(chars);
    return it == m.end() ? nullptr : *it;
}

void StringPool::insert(ObjString* string)
{
    ensureCapacity(m.size() + 1);
    m.emplace(string);
}

size_t StringPool::size() const
{
    return m.size();
}
