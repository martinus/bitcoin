// Copyright 2014 BitPay Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <iomanip>
#include <stdio.h>
#include "univalue.h"
#include "univalue_escapes.h"

static void json_escape(const std::string& inS, std::string& outS)
{
    // two passes: first determine the size, then copy all data over.
    // This is faster than having a single pass, because std::string's += is slow since it has to check the size
    // every time.
    size_t s = 0;
    for (unsigned char ch : inS) {
        s += escapesLen[ch];
    }

    // now that we know the exact size, prepare the string then directly copy into the data.
    outS.resize(outS.size() + s);
    auto* ptr = &outS[outS.size() - s];

    for (unsigned char ch : inS) {
        auto len = escapesLen[ch];
        if (len == 1) {
            *ptr++ = (char)ch;
        } else {
            ptr = std::copy(escapes[ch], escapes[ch] + len, ptr);
        }
    }
}

std::string UniValue::write(unsigned int prettyIndent,
                            unsigned int indentLevel) const
{
    std::string s;
    write(prettyIndent, indentLevel, s);
    return s;
}

void UniValue::write(unsigned int prettyIndent,
                     unsigned int indentLevel,
                     std::string& s) const
{
    unsigned int modIndent = indentLevel;
    if (modIndent == 0)
        modIndent = 1;

    switch (typ) {
    case VNULL:
        s += "null";
        break;
    case VOBJ:
        writeObject(prettyIndent, modIndent, s);
        break;
    case VARR:
        writeArray(prettyIndent, modIndent, s);
        break;
    case VSTR:
        s += '"';
        json_escape(val, s);
        s += '"';
        break;
    case VNUM:
        s += val;
        break;
    case VBOOL:
        s += (val == "1" ? "true" : "false");
        break;
    }
}

static void indentStr(unsigned int prettyIndent, unsigned int indentLevel, std::string& s)
{
    s.append(prettyIndent * indentLevel, ' ');
}

void UniValue::writeArray(unsigned int prettyIndent, unsigned int indentLevel, std::string& s) const
{
    s += '[';
    if (prettyIndent)
        s += '\n';

    for (unsigned int i = 0; i < values.size(); i++) {
        if (prettyIndent)
            indentStr(prettyIndent, indentLevel, s);
        values[i].write(prettyIndent, indentLevel + 1, s);
        if (i != (values.size() - 1)) {
            s += ',';
        }
        if (prettyIndent)
            s += '\n';
    }

    if (prettyIndent)
        indentStr(prettyIndent, indentLevel - 1, s);
    s += ']';
}

void UniValue::writeObject(unsigned int prettyIndent, unsigned int indentLevel, std::string& s) const
{
    s += '{';
    if (prettyIndent)
        s += '\n';

    for (unsigned int i = 0; i < keys.size(); i++) {
        if (prettyIndent)
            indentStr(prettyIndent, indentLevel, s);
        s += '\"';
        json_escape(keys[i], s);
        s += '"';
        s += ':';
        if (prettyIndent)
            s += ' ';
        values.at(i).write(prettyIndent, indentLevel + 1, s);
        if (i != (values.size() - 1))
            s += ',';
        if (prettyIndent)
            s += '\n';
    }

    if (prettyIndent)
        indentStr(prettyIndent, indentLevel - 1, s);
    s += '}';
}

