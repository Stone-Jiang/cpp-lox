#pragma once

#include "value.h"
#include <unordered_map>

#ifdef BETTER_HASH_TABLE
#include "../utils/robinhood.h"
#endif

struct ValueHash
{
    size_t operator()(const Value& value) const noexcept;
};

struct ValueEqual
{
    bool operator()(const Value& left, const Value& right) const noexcept;
};

enum class HashResult: u8
{
    OK,
    UNHASHABLE,
    NAN_VALUE
};

HashResult validateKey(const Value& key) noexcept;

class VM;

class ValueTable
{
private:
    #ifndef BETTER_HASH_TABLE
    std::unordered_map<Value, Value, ValueHash, ValueEqual> m;
    #else
    robin_hood::unordered_flat_map<Value, Value, ValueHash, ValueEqual, MAX_LOAD_FACTOR_I> m;
    #endif

    VM* owner = nullptr;
    size_t accountedSlots = 0;

    static size_t slotStorageBytes(size_t slots) noexcept;
    static size_t slotsForEntries(size_t entries) noexcept;
    void ensureCapacity(size_t entries);

public:
    explicit ValueTable(VM* owner = nullptr);
    ~ValueTable();

    ValueTable(const ValueTable&) = delete;
    ValueTable& operator=(const ValueTable&) = delete;
    ValueTable(ValueTable&&) = delete;
    ValueTable& operator=(ValueTable&&) = delete;

    bool get(const Value& key, Value& result) const;
    bool contains(const Value& key) const;
    bool set(const Value& key, const Value& val);
    bool remove(const Value& key, Value& removed);

    void clear() noexcept;
    void reserve(size_t count);
    size_t size() const noexcept;
    bool empty() const noexcept;

    template<typename Visitor>
    void foreach(Visitor&& visitor) const
    {
        for(const auto& entry: m)
            visitor(entry.first, entry.second);
    }
};
