#pragma once
#include "value.h"

#include <string_view>
#include <unordered_set>

#ifdef BETTER_HASH_TABLE
#include "../utils/robinhood.h"
#endif

struct InternHash
{
    using is_transparent = void;

    size_t operator()(const ObjString* value) const noexcept;
    size_t operator()(std::string_view value) const noexcept;
};

struct InternEqual
{
    using is_transparent = void;

    bool operator()(const ObjString* left, const ObjString* right) const noexcept;
    bool operator()(const ObjString* left, std::string_view right) const noexcept;
    bool operator()(std::string_view left, const ObjString* right) const noexcept;
};

struct ClassMember
{
    Value value;
    bool isStatic = false;
};

class Table
{
    friend class VM;
private:
    #ifdef BETTER_HASH_TABLE
    robin_hood::unordered_flat_map<ObjString*, Value,
        robin_hood::hash<ObjString*>, std::equal_to<ObjString*>, MAX_LOAD_FACTOR_I> m;
    #else
    std::unordered_map<ObjString*, Value> m;
    #endif

    VM* owner = nullptr;
    size_t accountedSlots = 0;

    static size_t slotStorageBytes(size_t slots);
    static size_t slotsForEntries(size_t entries);
    void ensureCapacity(size_t entries);
public:
    explicit Table(VM* owner = nullptr);
    ~Table();
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    bool get(ObjString* key, Value& value);
    bool set(ObjString* key, Value val);
    bool del(ObjString* key);
    size_t size() const;
    void reserve(size_t cap);
};

class MemberTable
{
    friend class VM;
private:
    #ifdef BETTER_HASH_TABLE
    robin_hood::unordered_flat_map<ObjString*, ClassMember,
        robin_hood::hash<ObjString*>, std::equal_to<ObjString*>, MAX_LOAD_FACTOR_I> m;
    #else
    std::unordered_map<ObjString*, ClassMember> m;
    #endif

    VM* owner = nullptr;
    size_t accountedSlots = 0;

    static size_t slotStorageBytes(size_t slots);
    static size_t slotsForEntries(size_t entries);
    void ensureCapacity(size_t entries);
public:
    explicit MemberTable(VM* owner = nullptr);
    ~MemberTable();
    MemberTable(const MemberTable&) = delete;
    MemberTable& operator=(const MemberTable&) = delete;

    bool get(ObjString* key, ClassMember& value);
    bool set(ObjString* key, const ClassMember& val);
    bool del(ObjString* key);
    size_t size() const;
    void reserve(size_t cap);
};

class StringPool
{
    friend class VM;
private:
    #ifdef BETTER_HASH_TABLE
    robin_hood::unordered_flat_set<ObjString*, InternHash, InternEqual, MAX_LOAD_FACTOR_I> m;
    #else
    std::unordered_set<ObjString*, InternHash, InternEqual> m;
    #endif

    VM* owner = nullptr;
    size_t accountedSlots = 0;

    static size_t slotStorageBytes(size_t slots);
    static size_t slotsForEntries(size_t entries);
    void ensureCapacity(size_t entries);
public:
    explicit StringPool(VM* owner = nullptr);
    ~StringPool();
    StringPool(const StringPool&) = delete;
    StringPool& operator=(const StringPool&) = delete;

    ObjString* find(std::string_view chars) const;
    void insert(ObjString* string);
    size_t size() const;
};
