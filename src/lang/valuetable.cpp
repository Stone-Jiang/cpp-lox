#include "valuetable.h"

#include "object.h"

#include <cassert>
#include <cmath>
#include <complex>
#include <functional>
#include <limits>

namespace
{
enum class KeyTag : size_t
{
    NIL_VALUE = 0x13d2a8b7,
    BOOL_VALUE = 0x8f34c291,
    NUMBER_VALUE = 0x50a96e43,
    STRING_VALUE = 0xc7631bd5,
    COMPLEX_VALUE = 0x748c5a19,
    OBJECT_VALUE = 0x2b914fe7
};

size_t combine(size_t tag, size_t payload) noexcept
{
    return tag ^ (payload + static_cast<size_t>(0x9e3779b9) +
        (tag << 6) + (tag >> 2));
}

size_t tagged(KeyTag tag, size_t payload) noexcept
{
    return combine(static_cast<size_t>(tag), payload);
}

size_t hashNumber(double number) noexcept
{
    if(number == 0.0)
        number = 0.0;
    return std::hash<double>{}(number);
}

size_t hashComplex(const std::complex<double>& number) noexcept
{
    size_t hash = tagged(
        KeyTag::COMPLEX_VALUE,
        hashNumber(number.real()));
    return combine(hash, hashNumber(number.imag()));
}

void checkStableKey(const Value& key) noexcept
{
    #ifdef DEBUG_VALUE_TABLE
    assert(validateKey(key) == HashResult::OK &&
        "ValueTable received an unstable key");
    #else
    (void)key;
    #endif
}
}

size_t ValueHash::operator()(const Value& value) const noexcept
{
    if(value.is_nil())
        return tagged(KeyTag::NIL_VALUE, 0);
    if(value.is_bool())
        return tagged(
            KeyTag::BOOL_VALUE,
            std::hash<bool>{}(value.as_bool()));
    if(value.is_number())
        return tagged(
            KeyTag::NUMBER_VALUE,
            hashNumber(value.as_number()));

    const Obj* object = value.as_obj();
    if(object == nullptr)
        return tagged(KeyTag::OBJECT_VALUE, 0);

    if(object->type == ObjType::STRING)
    {
        return tagged(
            KeyTag::STRING_VALUE,
            as<ObjString>(object)->hash);
    }
    if(object->type == ObjType::COMPLEX)
        return hashComplex(as<ObjComplex>(object)->c);

    // Remaining object types currently compare by identity. The collector is
    // non-moving, so their addresses stay stable for their lifetimes.
    const size_t identity = std::hash<const Obj*>{}(object);
    return combine(
        tagged(KeyTag::OBJECT_VALUE, static_cast<size_t>(object->type)),
        identity);
}

bool ValueEqual::operator()(
    const Value& left, const Value& right) const noexcept
{
    return left == right;
}

HashResult validateKey(const Value& key) noexcept
{
    if(key.is_nil() || key.is_bool())
        return HashResult::OK;

    if(key.is_number())
    {
        return std::isnan(key.as_number())
            ? HashResult::NAN_VALUE
            : HashResult::OK;
    }

    if(is<ObjString>(key))
        return HashResult::OK;

    return HashResult::UNHASHABLE;
}

ValueTable::ValueTable(VM* vm): owner(vm)
{
    #ifndef BETTER_HASH_TABLE
    m.max_load_factor(MAX_LOAD_FACTOR_F);
    #endif
}

ValueTable::~ValueTable()
{
    trackAlloc(owner, slotStorageBytes(accountedSlots), 0);
}

size_t ValueTable::slotStorageBytes(size_t slots) noexcept
{
    constexpr size_t slotSize =
        sizeof(Value) + sizeof(Value) + sizeof(size_t);
    const size_t maximum = std::numeric_limits<size_t>::max();
    return slots > maximum / slotSize
        ? maximum
        : slots * slotSize;
}

size_t ValueTable::slotsForEntries(size_t entries) noexcept
{
    if(entries == 0)
        return 0;

    const size_t maximum = std::numeric_limits<size_t>::max();
    const size_t numerator =
        entries > (maximum - (MAX_LOAD_FACTOR_I - 1)) / 100
            ? maximum
            : entries * 100 + (MAX_LOAD_FACTOR_I - 1);
    const size_t required = numerator / MAX_LOAD_FACTOR_I;

    size_t slots = 8;
    while(slots < required)
    {
        if(slots > maximum / 2)
            return maximum;
        slots *= 2;
    }
    return slots;
}

void ValueTable::ensureCapacity(size_t entries)
{
    const size_t newSlots = slotsForEntries(entries);
    if(newSlots <= accountedSlots)
        return;

    const size_t oldBytes = slotStorageBytes(accountedSlots);
    const size_t newBytes = slotStorageBytes(newSlots);
    prepareAlloc(owner, oldBytes, newBytes);
    m.reserve(entries);
    trackAlloc(owner, oldBytes, newBytes);
    accountedSlots = newSlots;
}

bool ValueTable::get(const Value& key, Value& result) const
{
    checkStableKey(key);

    const auto found = m.find(key);
    if(found == m.end())
        return false;

    result = found->second;
    return true;
}

bool ValueTable::contains(const Value& key) const
{
    checkStableKey(key);
    return m.find(key) != m.end();
}

bool ValueTable::set(const Value& key, const Value& value)
{
    checkStableKey(key);

    auto found = m.find(key);
    if(found != m.end())
    {
        found->second = value;
        return false;
    }

    ensureCapacity(m.size() + 1);
    m.emplace(std::move(key), std::move(value));
    return true;
}

bool ValueTable::remove(const Value& key, Value& removed)
{
    checkStableKey(key);

    auto found = m.find(key);
    if(found == m.end())
        return false;

    removed = found->second;
    m.erase(found);
    return true;
}

void ValueTable::clear() noexcept
{
    m.clear();
}

void ValueTable::reserve(size_t count)
{
    ensureCapacity(count);
}

size_t ValueTable::size() const noexcept
{
    return m.size();
}

bool ValueTable::empty() const noexcept
{
    return m.empty();
}
