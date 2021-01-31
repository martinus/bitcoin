// Copyright 2014 BitPay Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <iomanip>
#include <stdio.h>
#include "univalue.h"
#include "univalue_escapes.h"

constexpr auto myEscapes = std::array<char const*, 256>{{
    "\\u0000",
    "\\u0001",
    "\\u0002",
    "\\u0003",
    "\\u0004",
    "\\u0005",
    "\\u0006",
    "\\u0007",
    "\\b",
    "\\t",
    "\\n",
    "\\u000b",
    "\\f",
    "\\r",
    "\\u000e",
    "\\u000f",
    "\\u0010",
    "\\u0011",
    "\\u0012",
    "\\u0013",
    "\\u0014",
    "\\u0015",
    "\\u0016",
    "\\u0017",
    "\\u0018",
    "\\u0019",
    "\\u001a",
    "\\u001b",
    "\\u001c",
    "\\u001d",
    "\\u001e",
    "\\u001f",
    NULL,
    NULL,
    "\\\"",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "\\\\",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "\\u007f",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
}};

constexpr auto myLen = std::array<uint8_t, 256>{{
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    2,
    2,
    2,
    6,
    2,
    2,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    1,
    1,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    6,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
}};

static char* json_escape(const std::string& inS, char* outS)
{
	for (unsigned char c : inS) {
		if (myLen[c] == 1) {
			*outS++ = (char)c;
		} else {
			memcpy(outS, myEscapes[c], myLen[c]);
			outS += myLen[c];
		}
	}
	return outS;
}

static size_t json_escape_size(const std::string& inS)
{
	size_t s = 0;
	for (unsigned char ch : inS) {
		s += myLen[ch];
    }
	return s;
}


template<int N>
static char* cpy(const char(&ar)[N], char* out) {
	memcpy(out, ar, N-1);
	return out + N - 1;
}


std::string UniValue::write(unsigned int prettyIndent,
                            unsigned int indentLevel) const
{
    std::string s;
	s.resize(calcWriteSize(prettyIndent, indentLevel));
    auto end = write(prettyIndent, indentLevel, &s[0]);
	if (end - s.data() != s.size()) {
		throw std::runtime_error("distance does not match!");
	}
    return s;
}

char* UniValue::write(unsigned int prettyIndent,
                     unsigned int indentLevel,
                     char* s) const
{
    unsigned int modIndent = indentLevel;
    if (modIndent == 0)
        modIndent = 1;

    switch (typ) {
    case VNULL:
        s = cpy("null", s);
        break;
    case VOBJ:
        s = writeObject(prettyIndent, modIndent, s);
        break;
    case VARR:
        s = writeArray(prettyIndent, modIndent, s);
        break;
    case VSTR:
        *s++ = '"';
        s = json_escape(val, s);
        *s++ = '"';
        break;
    case VNUM:
        s = std::copy(val.data(), val.data()+val.size(), s);
        break;
    case VBOOL:
		if (val.empty()) {
			s = cpy("false", s);
		} else {
			s = cpy("true", s);
		}
        break;
    }
	return s;
}

size_t UniValue::calcWriteSize(unsigned int prettyIndent,
                     unsigned int indentLevel) const
{
	size_t s = 0;
    unsigned int modIndent = indentLevel;
    if (modIndent == 0)
        modIndent = 1;

    switch (typ) {
    case VNULL:
        s += 4; // "null";
        break;
    case VOBJ:
        s += calcWriteObjectSize(prettyIndent, modIndent);
        break;
    case VARR:
        s += calcWriteArraySize(prettyIndent, modIndent);
        break;
    case VSTR:
		s += 2 + json_escape_size(val);
        break;
    case VNUM:
        s += val.size();
        break;
    case VBOOL:
        s += (val == "1" ? 4 /*"true"*/ : 5 /*"false"*/);
        break;
    }
	return s;
}

static char* indentStr(unsigned int prettyIndent, unsigned int indentLevel, char* s)
{
	std::fill(s, s + prettyIndent * indentLevel, ' ');
	return s + prettyIndent * indentLevel;
}

char* UniValue::writeArray(unsigned int prettyIndent, unsigned int indentLevel, char* s) const
{
    *s++ = '[';
    if (prettyIndent)
        *s++ = '\n';

    for (unsigned int i = 0; i < values.size(); i++) {
        if (prettyIndent)
            s = indentStr(prettyIndent, indentLevel, s);
        s = values[i].write(prettyIndent, indentLevel + 1, s);
        if (i != (values.size() - 1)) {
            *s++ = ',';
        }
        if (prettyIndent)
            *s++ = '\n';
    }

    if (prettyIndent)
        s = indentStr(prettyIndent, indentLevel - 1, s);
    *s++ = ']';
	return s;
}

size_t UniValue::calcWriteArraySize(unsigned int prettyIndent, unsigned int indentLevel) const
{
	size_t s = 0;
    s += 1 /*'['*/;
    if (prettyIndent)
        s += 1 /*'\n'*/;

    for (unsigned int i = 0; i < values.size(); i++) {
        if (prettyIndent)
			s += prettyIndent * indentLevel;
        s += values[i].calcWriteSize(prettyIndent, indentLevel + 1);
        if (i != (values.size() - 1)) {
            s += 1/*','*/;
        }
        if (prettyIndent)
            s += 1/*'\n'*/;
    }

    if (prettyIndent)
		s += prettyIndent * (indentLevel - 1);
    s += 1/*']'*/;
	return s;
}


char* UniValue::writeObject(unsigned int prettyIndent, unsigned int indentLevel, char* s) const
{
    *s++ = '{';
    if (prettyIndent)
        *s++ = '\n';

    for (unsigned int i = 0; i < keys.size(); i++) {
        if (prettyIndent)
            s = indentStr(prettyIndent, indentLevel, s);
        *s++ = '\"';
        s = json_escape(keys[i], s);
		*s++ = '"';
		*s++ = ':';
        if (prettyIndent)
            *s++ = ' ';
        s = values.at(i).write(prettyIndent, indentLevel + 1, s);
        if (i != (values.size() - 1))
            *s++ = ',';
        if (prettyIndent)
            *s++ = '\n';
    }

    if (prettyIndent)
        s = indentStr(prettyIndent, indentLevel - 1, s);
    *s++ = '}';
	return s;
}

size_t UniValue::calcWriteObjectSize(unsigned int prettyIndent, unsigned int indentLevel) const
{
	size_t s = 0;
    s += 1 /*'{'*/;
    if (prettyIndent)
        s += 1 /*'\n'*/;

    for (unsigned int i = 0; i < keys.size(); i++) {
        if (prettyIndent)
			s += prettyIndent * indentLevel;
        s += 1/*'\"'*/;
        s += json_escape_size(keys[i]); 
		s += 1/*'"'*/;
		s += 1/*':'*/;
        if (prettyIndent)
            s += 1/*' '*/;
        s += values.at(i).calcWriteSize(prettyIndent, indentLevel + 1);
        if (i != (values.size() - 1))
            s += 1/*','*/;
        if (prettyIndent)
            s += 1/*'\n'*/;
    }

    if (prettyIndent)
        s += prettyIndent * (indentLevel - 1);
    s += 1/*'}'*/;
	return s;
}
