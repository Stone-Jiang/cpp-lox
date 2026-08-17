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

    VM* owner = nullptr;
    size_t accountedSlots = 0;
    size_t keyBytes = 0;

    static size_t entryKeyBytes(const string& key);
    static size_t slotStorageBytes(size_t slots);
    static size_t slotsForEntries(size_t entries);
    void ensureCapacity(size_t entries);
    void releaseKeyBytes(size_t bytes);
public:
    explicit Table(VM* owner = nullptr);
    ~Table();
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    std::optional<Value> find(const string& key) const;
    bool get(const string& key, Value& value);
    bool set(const string& key, Value val);
    bool del(const string& key);
    size_t size() const;
    void reserve(size_t cap);
};
