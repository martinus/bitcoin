// Copyright 2014 BitPay Inc.
// Copyright 2015-2021 Bitcoin Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef __UNIVALUE_H__
#define __UNIVALUE_H__

// Legacy code that uses UniValue typically makes countless copies of UniValue
// objects. These copies are often unnecessary and costly. To help with
// converting legacy code into more efficient std::move friendly code, enable
// NO_UNIVALUE_COPY_OPERATIONS. This will disable all UniValue methods that
// cause copying of elements, causing compile errors. Each of these compile
// errors can then be fixed with e.g. std::move(), or if a copy is still
// necessary by explicitly calling the .copy() method. Unfortunately it cannot
// detect all cases, as a copy constructor is still necessary for collections.
//
// You can disable copy operations here globally, or in individual files by
// setting it before the first univalue.h include.
//
//#define NO_UNIVALUE_COPY_OPERATIONS

#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>
#include <map>
#include <cassert>

class UniValue {
public:
    enum VType { VNULL, VOBJ, VARR, VSTR, VNUM, VBOOL, };

    UniValue() = default;

#if defined(NO_UNIVALUE_COPY_OPERATIONS)
    // explicit copy constructor and deleted copy assignment forces the use of .copy() in most cases.
    explicit UniValue(const UniValue&) = default;
    UniValue& operator=(const UniValue&) = delete;
#else
    UniValue(const UniValue&) = default;
    UniValue& operator=(const UniValue&) = default;
#endif

    UniValue(UniValue&&) = default;
    UniValue& operator=(UniValue&&) = default;
    ~UniValue() = default;

    // adds an explicit copy() operation that also works even when NO_UNIVALUE_COPY_OPERATIONS is defined.
    UniValue copy() const {
        return UniValue{*this};
    }

    UniValue(UniValue::VType initialType) : typ(initialType) {}
    UniValue(UniValue::VType initialType, const std::string& initialStr) :typ(initialType), val(initialStr) {}
    UniValue(UniValue::VType initialType, std::string&& initialStr) : typ(initialType), val(std::move(initialStr)) {}
    UniValue(uint64_t val_) {
        setInt(val_);
    }
    UniValue(int64_t val_) {
        setInt(val_);
    }
    UniValue(bool val_) {
        setBool(val_);
    }
    UniValue(int val_) {
        setInt(val_);
    }
    UniValue(double val_) {
        setFloat(val_);
    }
    UniValue(const std::string& val_) {
        setStr(val_);
    }
    UniValue(std::string&& val_) {
        setStr(std::move(val_));
    }
    UniValue(const char *val_) {
        setStr(std::string(val_));
    }

    void clear();

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

#if !defined(NO_UNIVALUE_COPY_OPERATIONS)
    bool push_back(const UniValue& val);
#endif
    bool push_back(UniValue&& val);
    bool push_back(const std::string& val_) {
        return push_back(UniValue(VSTR, val_));
    }
    bool push_back(std::string&& val_) {
        return push_back(UniValue(VSTR, std::move(val_)));
    }
    bool push_back(const char *val_) {
        return push_back(std::string(val_));
    }
    bool push_back(uint64_t val_) {
        return push_back(UniValue(val_));
    }
    bool push_back(int64_t val_) {
        return push_back(UniValue(val_));
    }
    bool push_back(bool val_) {
        return push_back(UniValue(val_));
    }
    bool push_back(int val_) {
        return push_back(UniValue(val_));
    }
    bool push_back(double val_) {
        return push_back(UniValue(val_));
    }
#if !defined(NO_UNIVALUE_COPY_OPERATIONS)
    bool push_backV(const std::vector<UniValue>& vec);
#endif
    bool push_backV(std::vector<UniValue>&& vec);

#if !defined(NO_UNIVALUE_COPY_OPERATIONS)
    void __pushKV(const std::string& key, const UniValue& val);
#endif
    void __pushKV(const std::string& key, UniValue&& val);
#if !defined(NO_UNIVALUE_COPY_OPERATIONS)
    void __pushKV(std::string&& key, const UniValue& val);
#endif
    void __pushKV(std::string&& key, UniValue&& val);

#if !defined(NO_UNIVALUE_COPY_OPERATIONS)
    bool pushKV(const std::string& key, const UniValue& val);
#endif
    bool pushKV(const std::string& key, UniValue&& val);
#if !defined(NO_UNIVALUE_COPY_OPERATIONS)
    bool pushKV(std::string&& key, const UniValue& val);
#endif
    bool pushKV(std::string&& key, UniValue&& val);
    
    bool pushKV(const std::string& key, const std::string& val_) {
        return pushKV(key, UniValue(VSTR, val_));
    }
    bool pushKV(std::string&& key, const std::string& val_) {
        return pushKV(std::move(key), UniValue(VSTR, val_));
    }
    bool pushKV(const std::string& key, std::string&& val_) {
        return pushKV(key, UniValue(VSTR, std::move(val_)));
    }
    bool pushKV(std::string&& key, std::string&& val_) {
        return pushKV(std::move(key), UniValue(VSTR, std::move(val_)));
    }
    bool pushKV(const std::string& key, const char *val_) {
        return pushKV(key, std::string(val_));
    }
    bool pushKV(std::string&& key, const char *val_) {
        return pushKV(std::move(key), std::string(val_));
    }
    bool pushKV(const std::string& key, int64_t val_) {
        return pushKV(key, UniValue(val_));
    }
    bool pushKV(std::string&& key, int64_t val_) {
        return pushKV(std::move(key), UniValue(val_));
    }
    bool pushKV(const std::string& key, uint64_t val_) {
        return pushKV(key, UniValue(val_));
    }
    bool pushKV(std::string&& key, uint64_t val_) {
        return pushKV(std::move(key), UniValue(val_));
    }    
    bool pushKV(const std::string& key, bool val_) {
        return pushKV(key, UniValue(val_));
    }
    bool pushKV(std::string&& key, bool val_) {
        return pushKV(std::move(key), UniValue(val_));
    }
    bool pushKV(const std::string& key, int val_) {
        return pushKV(key, UniValue((int64_t)val_));
    }
    bool pushKV(std::string&& key, int val_) {
        return pushKV(std::move(key), UniValue((int64_t)val_));
    }    
    bool pushKV(const std::string& key, double val_) {
        return pushKV(key, UniValue(val_));
    }
    bool pushKV(std::string&& key, double val_) {
        return pushKV(std::move(key), UniValue(val_));
    }    
#if !defined(NO_UNIVALUE_COPY_OPERATIONS)
    bool pushKVs(const UniValue& obj);
#endif
    bool pushKVs(UniValue&& obj);

    std::string write(unsigned int prettyIndent = 0,
                      unsigned int indentLevel = 0) const;

    bool read(const char *raw, size_t len);
    bool read(const char *raw) { return read(raw, strlen(raw)); }
    bool read(const std::string& rawStr) {
        return read(rawStr.data(), rawStr.size());
    }

private:
    UniValue::VType typ = VNULL;
    std::string val;                       // numbers are stored as C++ strings
    std::vector<std::string> keys;
    std::vector<UniValue> values;

    bool findKey(const std::string& key, size_t& retIdx) const;
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
