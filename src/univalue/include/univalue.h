// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef __UNIVALUE_H__
#define __UNIVALUE_H__

#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>
#include <map>
#include <cassert>

class UniValue {
public:
    enum VType { VNULL, VOBJ, VARR, VSTR, VNUM, VBOOL, };

    UniValue() : typ(VNULL) {}
    UniValue(UniValue::VType initialType) : typ(initialType) {}
    UniValue(UniValue::VType initialType, const std::string& initialStr) : typ(initialType), val(initialStr) {}
    UniValue(UniValue::VType initialType, std::string&& initialStr) : typ(initialType), val(std::move(initialStr)) {}

    UniValue(uint64_t val_) : typ(VNUM), val(std::to_string(val_)) {}
    UniValue(int64_t val_) : typ(VNUM), val(std::to_string(val_)) {}
    UniValue(bool val_) {
        setBool(val_);
    }
    UniValue(int val_) : typ(VNUM), val(std::to_string(val_)) {}
    UniValue(double val_) {
        setFloat(val_);
    }
    UniValue(const std::string& val_) : typ(VSTR), val(val_) {}
    UniValue(std::string&& val_) : typ(VSTR), val(std::move(val_)) {}
    UniValue(const char *val_) : typ(VSTR), val(val_) {}

    void clear();
    void reserve(size_t n) {
        switch (typ) {
        case VOBJ:
            keys.reserve(n);
            values.reserve(n);
            break;

        case VARR:
            values.reserve(n);
            break;

        case VSTR:
            val.reserve(n);
            break;

        default:
            break;
        }
    }

    bool setNull();
    bool setBool(bool val);
    bool setNumStr(const std::string& val);
    bool setNumStr(std::string&& val);
    bool setInt(uint64_t val);
    bool setInt(int64_t val);
    bool setInt(int val_) { return setInt((int64_t)val_); }
    bool setFloat(double val);
    bool setStr(const std::string& val);
    bool setStr(std::string&& val);
    bool setArray();
    bool setObject();

    enum VType getType() const { return typ; }
    const std::string& getValStr() const { return val; }
    bool empty() const { return (values.size() == 0); }

    size_t size() const { return values.size(); }

    bool getBool() const { return isTrue(); }
    void getObjMap(std::map<std::string,UniValue>& kv) const;
    bool checkObject(const std::map<std::string,UniValue::VType>& memberTypes) const;
    const UniValue& operator[](const std::string& key) const;
    const UniValue& operator[](size_t index) const;
    bool exists(const std::string& key) const { size_t i; return findKey(key, i); }

    bool isNull() const { return (typ == VNULL); }
    bool isTrue() const { return (typ == VBOOL) && (val == "1"); }
    bool isFalse() const { return (typ == VBOOL) && (val != "1"); }
    bool isBool() const { return (typ == VBOOL); }
    bool isStr() const { return (typ == VSTR); }
    bool isNum() const { return (typ == VNUM); }
    bool isArray() const { return (typ == VARR); }
    bool isObject() const { return (typ == VOBJ); }

    template<typename Arg>
    bool push_back(UniValue::VType t, Arg&& arg) {
        if (typ != VARR)
            return false;
        values.emplace_back(t, std::forward<Arg>(arg));
        return true;
    }
    template<typename Arg>
    bool push_back(Arg&& arg) {
        if (typ != VARR)
            return false;
        values.emplace_back(std::forward<Arg>(arg));
        return true;
    }
    bool push_backV(const std::vector<UniValue>& vec);
    bool push_backV(std::vector<UniValue>&& vec);

    template<typename K, typename V>
    void __pushKV(K&& key, V&& val) {
        keys.emplace_back(std::forward<K>(key));
        values.emplace_back(std::forward<V>(val));
    }

    template<typename K, typename V>
    bool pushKV(K&& key, V&& val_) {
        if (typ != VOBJ)
            return false;

        size_t idx;
        if (findKey(key, idx))
            values[idx] = std::forward<V>(val_);
        else
            __pushKV(std::forward<K>(key), std::forward<V>(val_));
        return true;
    }
    bool pushKVs(const UniValue& obj);
    bool pushKVs(UniValue&& obj);

    std::string write(unsigned int prettyIndent = 0,
                      unsigned int indentLevel = 0) const;

    bool read(const char *raw, size_t len);
    bool read(const char *raw) { return read(raw, strlen(raw)); }
    bool read(const std::string& rawStr) {
        return read(rawStr.data(), rawStr.size());
    }

private:
    UniValue::VType typ;
    std::string val;                       // numbers are stored as C++ strings
    std::vector<std::string> keys;
    std::vector<UniValue> values;

    bool findKey(const std::string& key, size_t& retIdx) const;
    void write(unsigned int prettyIndent, unsigned int indentLevel, std::string& s) const;
    void writeArray(unsigned int prettyIndent, unsigned int indentLevel, std::string& s) const;
    void writeObject(unsigned int prettyIndent, unsigned int indentLevel, std::string& s) const;

public:
    // Strict type-specific getters, these throw std::runtime_error if the
    // value is of unexpected type
    const std::vector<std::string>& getKeys() const;
    const std::vector<UniValue>& getValues() const;
    bool get_bool() const;
    const std::string& get_str() const;
    int get_int() const;
    int64_t get_int64() const;
    double get_real() const;
    const UniValue& get_obj() const;
    const UniValue& get_array() const;

    enum VType type() const { return getType(); }
    friend const UniValue& find_value( const UniValue& obj, const std::string& name);
};

enum jtokentype {
    JTOK_ERR        = -1,
    JTOK_NONE       = 0,                           // eof
    JTOK_OBJ_OPEN,
    JTOK_OBJ_CLOSE,
    JTOK_ARR_OPEN,
    JTOK_ARR_CLOSE,
    JTOK_COLON,
    JTOK_COMMA,
    JTOK_KW_NULL,
    JTOK_KW_TRUE,
    JTOK_KW_FALSE,
    JTOK_NUMBER,
    JTOK_STRING,
};

extern enum jtokentype getJsonToken(std::string& tokenVal,
                                    unsigned int& consumed, const char *raw, const char *end);
extern const char *uvTypeName(UniValue::VType t);

static inline bool jsonTokenIsValue(enum jtokentype jtt)
{
    switch (jtt) {
    case JTOK_KW_NULL:
    case JTOK_KW_TRUE:
    case JTOK_KW_FALSE:
    case JTOK_NUMBER:
    case JTOK_STRING:
        return true;

    default:
        return false;
    }

    // not reached
}

static inline bool json_isspace(int ch)
{
    switch (ch) {
    case 0x20:
    case 0x09:
    case 0x0a:
    case 0x0d:
        return true;

    default:
        return false;
    }

    // not reached
}

extern const UniValue NullUniValue;

const UniValue& find_value( const UniValue& obj, const std::string& name);

#endif // __UNIVALUE_H__
