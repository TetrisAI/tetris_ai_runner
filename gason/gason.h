//https://github.com/vivkin/gason

#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>

namespace gason
{

    enum : unsigned long long
    {
        JSON_VALUE_PAYLOAD_MASK = 0x00007FFFFFFFFFFFULL,
        JSON_VALUE_NAN_MASK = 0x7FF8000000000000ULL,
        JSON_VALUE_NULL = 0x7FFF800000000000ULL,
        JSON_VALUE_TAG_MASK = 0xF,
        JSON_VALUE_TAG_SHIFT = 47
    };

    enum JsonTag
    {
        JSON_TAG_NUMBER = 0,
        JSON_TAG_STRING,
        JSON_TAG_BOOL,
        JSON_TAG_ARRAY,
        JSON_TAG_OBJECT,
        JSON_TAG_NULL = 0xF
    };

    union JsonValue
    {
        uint64_t ival;
        double fval;

        JsonValue()
            : ival(JSON_VALUE_NULL)
        {
        }
        JsonValue(double x)
            : fval(x)
        {
        }
        JsonValue(JsonTag tag, void const *p)
        {
            uint64_t x = reinterpret_cast<uint64_t>(p);
            assert(tag <= JSON_VALUE_TAG_MASK);
            assert(x <= JSON_VALUE_PAYLOAD_MASK);
            ival = JSON_VALUE_NAN_MASK | ((uint64_t)tag << JSON_VALUE_TAG_SHIFT) | x;
        }
        bool isDouble() const
        {
            return (int64_t)ival <= (int64_t)JSON_VALUE_NAN_MASK;
        }
        JsonTag getTag() const
        {
            return isDouble() ? JSON_TAG_NUMBER : JsonTag((ival >> JSON_VALUE_TAG_SHIFT) & JSON_VALUE_TAG_MASK);
        }
        uint64_t getPayload() const
        {
            assert(!isDouble());
            return ival & JSON_VALUE_PAYLOAD_MASK;
        }
        double toNumber() const
        {
            assert(getTag() == JSON_TAG_NUMBER);
            return fval;
        }
        int toInt() const
        {
            assert(getTag() == JSON_TAG_NUMBER);
            return static_cast<int>(fval);
        }
        bool toBool() const
        {
            assert(getTag() == JSON_TAG_BOOL);
            return getPayload() != 0;
        }
        char const *toString() const
        {
            assert(getTag() == JSON_TAG_STRING);
            return reinterpret_cast<char const *>(getPayload());
        }
        struct JsonNode const *toNode() const
        {
            assert(getTag() == JSON_TAG_ARRAY || getTag() == JSON_TAG_OBJECT);
            return reinterpret_cast<struct JsonNode const *>(getPayload());
        }
        JsonValue const &operator[](char const *key) const;
        JsonValue const &operator[](int pos) const;
        size_t size() const;

        struct JsonIterator begin() const;
        struct JsonIterator end() const;
    };

    struct JsonNode
    {
        JsonValue value;
        JsonNode *next;
    };

    struct JsonObjectNode : public JsonNode
    {
        char const *key;
    };


    struct JsonIterator
    {
        JsonNode const *p;

        void operator++()
        {
            p = p->next;
        }
        bool operator != (const JsonIterator &x) const
        {
            return p != x.p;
        }
        char const *key() const
        {
            return static_cast<JsonObjectNode const *>(p)->key;
        }
        JsonNode const *operator*() const
        {
            return p;
        }
        JsonNode const *operator->() const
        {
            return p;
        }
    };

    inline JsonIterator begin(JsonValue o)
    {
        return JsonIterator{o.toNode()};
    }
    inline JsonIterator end(JsonValue)
    {
        return JsonIterator{nullptr};
    }

    enum JsonParseStatus
    {
        JSON_PARSE_OK,
        JSON_PARSE_BAD_NUMBER,
        JSON_PARSE_BAD_STRING,
        JSON_PARSE_BAD_IDENTIFIER,
        JSON_PARSE_STACK_OVERFLOW,
        JSON_PARSE_STACK_UNDERFLOW,
        JSON_PARSE_MISMATCH_BRACKET,
        JSON_PARSE_UNEXPECTED_CHARACTER,
        JSON_PARSE_UNQUOTED_KEY,
        JSON_PARSE_BREAKING_BAD
    };

    class JsonAllocator
    {
        struct Zone
        {
            Zone *next;
            size_t used;
        } *head = nullptr;

    public:
        ~JsonAllocator();
        void *allocate(size_t size);
        void deallocate();

        template<class Type>
        Type *allocate()
        {
            return reinterpret_cast<Type *>(allocate(sizeof Type));
        }
    };

    JsonParseStatus jsonParse(char *str, char **endptr, JsonValue *value, JsonAllocator &allocator);

}