

#include "py_interpreter.h"
#include "config.h"
#include "settings.h"
#include "keyboard.h"
#include "ui.h"
#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

PyValue PyValue::makeNone()        { PyValue v; v.type = PyType::None;  return v; }
PyValue PyValue::makeBool(bool b)  { PyValue v; v.type = PyType::Bool;  v.bVal = b; return v; }
PyValue PyValue::makeInt(long i)   { PyValue v; v.type = PyType::Int;   v.iVal = i; return v; }
PyValue PyValue::makeFloat(double f){PyValue v; v.type = PyType::Float; v.fVal = f; return v; }
PyValue PyValue::makeStr(const String &s){ PyValue v; v.type = PyType::Str; v.sVal = s; return v; }

PyValue PyValue::makeList() {
    PyValue v; v.type = PyType::List;
    v.listData = std::make_shared<PyListData>();
    return v;
}
PyValue PyValue::makeDict() {
    PyValue v; v.type = PyType::Dict;
    v.dictData = std::make_shared<PyDictData>();
    return v;
}
PyValue PyValue::makeFunc(std::shared_ptr<PyFunc> f) {
    PyValue v; v.type = PyType::Func;
    v.funcData = f;
    return v;
}
PyValue PyValue::makeBuiltin(const String &name, PyBuiltinFn fn) {
    PyValue v; v.type = PyType::BuiltinFunc;
    v.sVal = name;
    v.builtinFn = fn;
    return v;
}
PyValue PyValue::makeModule(std::shared_ptr<PyDictData> d) {
    PyValue v; v.type = PyType::Module;
    v.dictData = d;
    return v;
}

bool PyValue::isTruthy() const {
    switch (type) {
        case PyType::None:    return false;
        case PyType::Bool:    return bVal;
        case PyType::Int:     return iVal != 0;
        case PyType::Float:   return fVal != 0.0;
        case PyType::Str:     return sVal.length() != 0;
        case PyType::List:
        case PyType::Tuple:   return listData && !listData->items.empty();
        case PyType::Dict:
        case PyType::Module:  return dictData && !dictData->items.empty();
        default:              return true;   
    }
}

bool PyValue::equals(const PyValue &o) const {
    if (type == PyType::None) return o.type == PyType::None;
    if (o.type == PyType::None) return false;
    if ((type == PyType::Int || type == PyType::Float) &&
        (o.type == PyType::Int || o.type == PyType::Float)) {
        double a = (type == PyType::Int) ? (double)iVal : fVal;
        double b = (o.type == PyType::Int) ? (double)o.iVal : o.fVal;
        return a == b;
    }
    if (type != o.type) return false;
    switch (type) {
        case PyType::Bool: return bVal == o.bVal;
        case PyType::Int:  return iVal == o.iVal;
        case PyType::Float:return fVal == o.fVal;
        case PyType::Str:  return sVal == o.sVal;
        case PyType::List:
        case PyType::Tuple: {
            if (!listData || !o.listData) return false;
            if (listData->items.size() != o.listData->items.size()) return false;
            for (size_t i = 0; i < listData->items.size(); i++)
                if (!listData->items[i].equals(o.listData->items[i])) return false;
            return true;
        }
        default: return false;
    }
}

String PyValue::repr() const {
    switch (type) {
        case PyType::None:    return "None";
        case PyType::Bool:    return bVal ? "True" : "False";
        case PyType::Int:     return String(iVal);
        case PyType::Float: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%g", fVal);
            return String(buf);
        }
        case PyType::Str: {
            String out = "'";
            for (char c : sVal) {
                if (c == '\'') out += "\\'";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\t') out += "\\t";
                else out += c;
            }
            out += "'";
            return out;
        }
        case PyType::List: {
            String out = "[";
            if (listData) {
                for (size_t i = 0; i < listData->items.size(); i++) {
                    if (i) out += ", ";
                    out += listData->items[i].repr();
                }
            }
            out += "]";
            return out;
        }
        case PyType::Tuple: {
            String out = "(";
            if (listData) {
                for (size_t i = 0; i < listData->items.size(); i++) {
                    if (i) out += ", ";
                    out += listData->items[i].repr();
                }
            }
            if (listData && listData->items.size() == 1) out += ",";
            out += ")";
            return out;
        }
        case PyType::Dict: {
            String out = "{";
            if (dictData) {
                for (size_t i = 0; i < dictData->items.size(); i++) {
                    if (i) out += ", ";
                    out += "'" + dictData->items[i].first + "': " + dictData->items[i].second.repr();
                }
            }
            out += "}";
            return out;
        }
        case PyType::Func:        return "<function " + (funcData ? funcData->name : "?") + ">";
        case PyType::BuiltinFunc: return "<built-in function " + sVal + ">";
        case PyType::Module:      return "<module>";
    }
    return "?";
}

String PyValue::str() const {
    
    if (type == PyType::Str) return sVal;
    return repr();
}

String PyValue::typeName() const {
    switch (type) {
        case PyType::None:        return "NoneType";
        case PyType::Bool:        return "bool";
        case PyType::Int:         return "int";
        case PyType::Float:       return "float";
        case PyType::Str:         return "str";
        case PyType::List:        return "list";
        case PyType::Tuple:       return "tuple";
        case PyType::Dict:        return "dict";
        case PyType::Func:        return "function";
        case PyType::BuiltinFunc: return "builtin_function_or_method";
        case PyType::Module:      return "module";
    }
    return "?";
}

PyValue *PyDictData::find(const String &key) {
    for (auto &kv : items) if (kv.first == key) return &kv.second;
    return nullptr;
}
void PyDictData::set(const String &key, const PyValue &v) {
    if (PyValue *ex = find(key)) { *ex = v; return; }
    items.push_back({key, v});
}
bool PyDictData::contains(const String &key) {
    return find(key) != nullptr;
}

static bool isIdentStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool isIdentChar(char c)  { return isIdentStart(c) || (c >= '0' && c <= '9'); }
static bool isDigit(char c)      { return c >= '0' && c <= '9'; }
static bool isHexDigit(char c)   { return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

std::vector<PyToken> pyLex(const String &src, String &errOut)
{
    std::vector<PyToken> out;
    errOut = "";

    std::vector<int> indentStack;
    indentStack.push_back(0);
    int i = 0, line = 1, col = 1;
    int n = src.length();
    bool atLineStart = true;
    bool sawNonBlank = false;
    int parenDepth = 0;   

    auto emit = [&](PyTok t, int l, int c) {
        PyToken tok; tok.tok = t; tok.line = l; tok.col = c;
        out.push_back(tok);
    };
    auto emitText = [&](PyTok t, const String &s, int l, int c) {
        PyToken tok; tok.tok = t; tok.text = s; tok.line = l; tok.col = c;
        out.push_back(tok);
    };

    while (i < n) {
        
        if (atLineStart && parenDepth == 0) {
            int indent = 0;
            int startLine = line;
            while (i < n && (src[i] == ' ' || src[i] == '\t')) {
                indent += (src[i] == ' ') ? 1 : 8;   
                i++; col++;
            }
            
            if (i >= n || src[i] == '\n' || src[i] == '#') {
                
                while (i < n && src[i] != '\n') { i++; col++; }
                if (i < n) { i++; line++; col = 1; }
                continue;
            }
            atLineStart = false;
            sawNonBlank = true;

            if (indent > indentStack.back()) {
                indentStack.push_back(indent);
                emit(PyTok::Indent, startLine, 1);
            } else {
                while (indent < indentStack.back()) {
                    indentStack.pop_back();
                    emit(PyTok::Dedent, startLine, 1);
                }
                if (indent != indentStack.back()) {
                    errOut = "IndentationError at line " + String(startLine);
                    return out;
                }
            }
            continue;
        }

        char c = src[i];

        
        if (c == ' ' || c == '\t') { i++; col++; continue; }

        
        if (c == '\\' && i + 1 < n && src[i + 1] == '\n') {
            i += 2; line++; col = 1; continue;
        }

        
        if (c == '\n') {
            if (parenDepth == 0) {
                
                if (sawNonBlank) {
                    emit(PyTok::Newline, line, col);
                    sawNonBlank = false;
                }
                atLineStart = true;
            }
            i++; line++; col = 1;
            continue;
        }

        
        if (c == '#') {
            while (i < n && src[i] != '\n') { i++; col++; }
            continue;
        }

        
        
        auto parseHexEscape = [&](String &s) -> bool {
            if (i + 3 < n && src[i + 1] == 'x' &&
                isHexDigit(src[i + 2]) && isHexDigit(src[i + 3])) {
                char hex[3] = { src[i + 2], src[i + 3], 0 };
                s += (char)strtol(hex, nullptr, 16);
                i += 4; col += 4;
                return true;
            }
            return false;
        };

        
        if (c == '"' || c == '\'') {
            int startLine = line, startCol = col;
            char quote = c;
            bool triple = (i + 2 < n && src[i + 1] == quote && src[i + 2] == quote);
            String s;
            if (triple) {
                i += 3; col += 3;
                while (i < n) {
                    if (i + 2 < n && src[i] == quote && src[i + 1] == quote && src[i + 2] == quote) {
                        i += 3; col += 3; break;
                    }
                    if (src[i] == '\n') { s += '\n'; line++; col = 1; i++; continue; }
                    if (src[i] == '\\' && i + 1 < n) {
                        if (parseHexEscape(s)) continue;
                        char nx = src[i + 1];
                        if (nx == 'n') s += '\n';
                        else if (nx == 't') s += '\t';
                        else if (nx == 'r') s += '\r';
                        else if (nx == '\\') s += '\\';
                        else if (nx == quote) s += quote;
                        else s += nx;
                        i += 2; col += 2; continue;
                    }
                    s += src[i]; i++; col++;
                }
            } else {
                i++; col++;
                while (i < n && src[i] != quote && src[i] != '\n') {
                    if (src[i] == '\\' && i + 1 < n) {
                        if (parseHexEscape(s)) continue;
                        char nx = src[i + 1];
                        if (nx == 'n') s += '\n';
                        else if (nx == 't') s += '\t';
                        else if (nx == 'r') s += '\r';
                        else if (nx == '\\') s += '\\';
                        else if (nx == quote) s += quote;
                        else if (nx == '0') s += '\0';
                        else s += nx;
                        i += 2; col += 2; continue;
                    }
                    s += src[i]; i++; col++;
                }
                if (i < n && src[i] == quote) { i++; col++; }
                else { errOut = "Unterminated string at line " + String(startLine); return out; }
            }
            
            bool isFString = false;
            
            
            (void)isFString;
            (void)startCol;
            emitText(PyTok::String, s, startLine, startCol);
            continue;
        }

        
        if ((c == 'f' || c == 'F' || c == 'r' || c == 'R' || c == 'b' || c == 'B')
            && i + 1 < n && (src[i + 1] == '"' || src[i + 1] == '\'')) {
            
            if (i + 2 >= n || !isIdentChar(src[i + 2])) {
                int startLine = line, startCol = col;
                bool isF = (c == 'f' || c == 'F');
                char quote = src[i + 1];
                bool triple = (i + 3 < n && src[i + 2] == quote && src[i + 3] == quote);
                i += 2; col += 2;
                if (triple) { i++; col++; }
                String s;
                while (i < n) {
                    if (triple) {
                        if (i + 2 < n && src[i] == quote && src[i + 1] == quote && src[i + 2] == quote) {
                            i += 3; col += 3; break;
                        }
                    } else {
                        if (i < n && src[i] == quote) { i++; col++; break; }
                        if (i < n && src[i] == '\n') { errOut = "Unterminated f-string at line " + String(startLine); return out; }
                    }
                    if (src[i] == '\\' && i + 1 < n) {
                        if (parseHexEscape(s)) continue;
                        char nx = src[i + 1];
                        if (nx == 'n') s += '\n';
                        else if (nx == 't') s += '\t';
                        else if (nx == 'r') s += '\r';
                        else if (nx == '\\') s += '\\';
                        else if (nx == quote) s += quote;
                        else s += nx;
                        i += 2; col += 2; continue;
                    }
                    s += src[i]; i++; col++;
                }
                PyToken tok;
                tok.tok = isF ? PyTok::FString : PyTok::String;
                tok.text = s;
                tok.line = startLine; tok.col = startCol;
                out.push_back(tok);
                continue;
            }
        }

        
        if (isIdentStart(c)) {
            int startCol = col;
            String s; s += c; i++; col++;
            while (i < n && isIdentChar(src[i])) { s += src[i]; i++; col++; }

            
            static const char *kw[] = {
                "if","elif","else","for","while","def","return","break",
                "continue","pass","import","from","as","True","False","None",
                "and","or","not","in","is","lambda","global","del",
                "async","await"
            };
            static const PyTok kwTok[] = {
                PyTok::KwIf, PyTok::KwElif, PyTok::KwElse, PyTok::KwFor,
                PyTok::KwWhile, PyTok::KwDef, PyTok::KwReturn, PyTok::KwBreak,
                PyTok::KwContinue, PyTok::KwPass, PyTok::KwImport, PyTok::KwFrom,
                PyTok::KwAs, PyTok::KwTrue, PyTok::KwFalse, PyTok::KwNone,
                PyTok::KwAnd, PyTok::KwOr, PyTok::KwNot, PyTok::KwIn, PyTok::KwIs,
                PyTok::KwLambda, PyTok::KwGlobal, PyTok::KwDel,
                PyTok::KwAsync, PyTok::KwAwait
            };
            bool isKw = false;
            for (int k = 0; k < (int)(sizeof(kw)/sizeof(kw[0])); k++) {
                if (s.equals(kw[k])) {
                    emit(kwTok[k], line, startCol);
                    isKw = true;
                    break;
                }
            }
            if (!isKw) emitText(PyTok::Name, s, line, startCol);
            continue;
        }

        
        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(src[i + 1]))) {
            int startCol = col;
            String s;
            bool isFloat = false;
            
            if (c == '0' && i + 1 < n && (src[i + 1] == 'x' || src[i + 1] == 'X')) {
                s += src[i++]; s += src[i++]; col += 2;
                while (i < n && (isDigit(src[i]) || (src[i] >= 'a' && src[i] <= 'f') || (src[i] >= 'A' && src[i] <= 'F'))) {
                    s += src[i++]; col++;
                }
                PyToken tok; tok.tok = PyTok::Number;
                tok.text = s; tok.iVal = strtoll(s.c_str() + 2, nullptr, 16);
                tok.isFloat = false; tok.line = line; tok.col = startCol;
                out.push_back(tok);
                continue;
            }
            if (c == '0' && i + 1 < n && (src[i + 1] == 'b' || src[i + 1] == 'B')) {
                s += src[i++]; s += src[i++]; col += 2;
                while (i < n && (src[i] == '0' || src[i] == '1')) { s += src[i++]; col++; }
                PyToken tok; tok.tok = PyTok::Number;
                tok.text = s; tok.iVal = strtoll(s.c_str() + 2, nullptr, 2);
                tok.isFloat = false; tok.line = line; tok.col = startCol;
                out.push_back(tok);
                continue;
            }
            while (i < n && isDigit(src[i])) { s += src[i++]; col++; }
            if (i < n && src[i] == '.') {
                isFloat = true; s += src[i++]; col++;
                while (i < n && isDigit(src[i])) { s += src[i++]; col++; }
            }
            if (i < n && (src[i] == 'e' || src[i] == 'E')) {
                isFloat = true; s += src[i++]; col++;
                if (i < n && (src[i] == '+' || src[i] == '-')) { s += src[i++]; col++; }
                while (i < n && isDigit(src[i])) { s += src[i++]; col++; }
            }
            PyToken tok; tok.tok = PyTok::Number; tok.text = s;
            if (isFloat) { tok.fVal = strtod(s.c_str(), nullptr); tok.isFloat = true; }
            else         { tok.iVal = strtoll(s.c_str(), nullptr, 10); tok.isFloat = false; }
            tok.line = line; tok.col = startCol;
            out.push_back(tok);
            continue;
        }

        
        int startCol = col;
        auto match2 = [&](char a, char b, PyTok t) -> bool {
            if (i + 1 < n && src[i] == a && src[i + 1] == b) {
                emit(t, line, startCol); i += 2; col += 2; return true;
            }
            return false;
        };
        auto match3 = [&](char a, char b, char c2, PyTok t) -> bool {
            if (i + 2 < n && src[i] == a && src[i + 1] == b && src[i + 2] == c2) {
                emit(t, line, startCol); i += 3; col += 3; return true;
            }
            return false;
        };

        
        if (match3('*','*','=', PyTok::DoubleStarEq)) continue;
        if (match3('<','<','=', PyTok::LShiftEq)) continue;
        if (match3('>','>','=', PyTok::RShiftEq)) continue;

        
        if (match2('*','*', PyTok::DoubleStar)) continue;
        if (match2('/','/', PyTok::DoubleSlash)) continue;
        if (match2('=','=', PyTok::Eq)) continue;
        if (match2('!','=', PyTok::Ne)) continue;
        if (match2('<','<', PyTok::LShift)) continue;
        if (match2('>','>', PyTok::RShift)) continue;
        if (match2('<','=', PyTok::Le)) continue;
        if (match2('>','=', PyTok::Ge)) continue;
        if (match2('+','=', PyTok::PlusEq)) continue;
        if (match2('-','=', PyTok::MinusEq)) continue;
        if (match2('*','=', PyTok::StarEq)) continue;
        if (match2('/','=', PyTok::SlashEq)) continue;
        if (match2('%','=', PyTok::PercentEq)) continue;
        if (match2('&','=', PyTok::AmpEq)) continue;
        if (match2('|','=', PyTok::PipeEq)) continue;
        if (match2('^','=', PyTok::CaretEq)) continue;
        if (match2('-','>', PyTok::Arrow)) continue;

        
        switch (c) {
            case '+': emit(PyTok::Plus, line, startCol);     i++; col++; continue;
            case '-': emit(PyTok::Minus, line, startCol);    i++; col++; continue;
            case '*': emit(PyTok::Star, line, startCol);     i++; col++; continue;
            case '/': emit(PyTok::Slash, line, startCol);    i++; col++; continue;
            case '%': emit(PyTok::Percent, line, startCol);  i++; col++; continue;
            case '=': emit(PyTok::Assign, line, startCol);   i++; col++; continue;
            case '<': emit(PyTok::Lt, line, startCol);       i++; col++; continue;
            case '>': emit(PyTok::Gt, line, startCol);       i++; col++; continue;
            case '(': emit(PyTok::LParen, line, startCol);   i++; col++; parenDepth++; continue;
            case ')': emit(PyTok::RParen, line, startCol);   i++; col++; parenDepth--; continue;
            case '[': emit(PyTok::LBracket, line, startCol); i++; col++; parenDepth++; continue;
            case ']': emit(PyTok::RBracket, line, startCol); i++; col++; parenDepth--; continue;
            case '{': emit(PyTok::LBrace, line, startCol);   i++; col++; parenDepth++; continue;
            case '}': emit(PyTok::RBrace, line, startCol);   i++; col++; parenDepth--; continue;
            case ',': emit(PyTok::Comma, line, startCol);    i++; col++; continue;
            case ':': emit(PyTok::Colon, line, startCol);    i++; col++; continue;
            case '.': emit(PyTok::Dot, line, startCol);      i++; col++; continue;
            case ';': emit(PyTok::Semicolon, line, startCol);i++; col++; continue;
            case '&': emit(PyTok::Amp, line, startCol);      i++; col++; continue;
            case '|': emit(PyTok::Pipe, line, startCol);     i++; col++; continue;
            case '^': emit(PyTok::Caret, line, startCol);    i++; col++; continue;
            case '~': emit(PyTok::Tilde, line, startCol);    i++; col++; continue;
            case '@': emit(PyTok::At, line, startCol);       i++; col++; continue;
        }
        
        
        
        
        
        i++; col++;
    }

    
    if (sawNonBlank) emit(PyTok::Newline, line, 1);
    while (indentStack.size() > 1) {
        indentStack.pop_back();
        emit(PyTok::Dedent, line, 1);
    }
    emit(PyTok::End, line, 1);
    return out;
}

struct PyParser {
    const std::vector<PyToken> &toks;
    size_t pos = 0;
    String error;

    PyParser(const std::vector<PyToken> &t) : toks(t) {}

    const PyToken &peek(int off = 0) const {
        static PyToken sentinel;
        size_t idx = pos + off;
        if (idx >= toks.size()) return sentinel;
        return toks[idx];
    }
    const PyToken &advance() {
        static PyToken sentinel;
        if (pos >= toks.size()) return sentinel;
        return toks[pos++];
    }
    bool check(PyTok t) const { return peek().tok == t; }
    bool accept(PyTok t) { if (check(t)) { advance(); return true; } return false; }
    bool expect(PyTok t, const char *what) {
        if (!accept(t)) {
            error = "Parse error line " + String(peek().line) + ": expected " + what;
            return false;
        }
        return true;
    }

    PyNode *makeNode(PyNodeKind k) {
        PyNode *n = new PyNode();
        n->kind = k;
        n->line = peek().line;
        return n;
    }

    
    std::vector<PyNode*> parseFile() {
        std::vector<PyNode*> body;
        while (!check(PyTok::End) && error.length() == 0) {
            while (accept(PyTok::Newline)) {}
            if (check(PyTok::End)) break;
            size_t prevPos = pos;    
            PyNode *s = parseStmt();
            if (s) {
                body.push_back(s);
            } else {
                
                
                
                if (error.length() == 0) break;  
                while (!check(PyTok::Newline) && !check(PyTok::End)) advance();
                while (accept(PyTok::Newline)) {}
            }
            
            if (pos == prevPos && !check(PyTok::End)) advance();
        }
        return body;
    }

    
    PyNode *parseStmt() {
        const PyToken &t = peek();
        
        
        if (t.tok == PyTok::At) {
            while (!check(PyTok::Newline) && !check(PyTok::End)) advance();
            if (check(PyTok::Newline)) advance();
            
            return parseStmt();
        }
        
        if (t.tok == PyTok::KwAsync) {
            advance();
            if (check(PyTok::KwDef))  return parseDef();   
            if (check(PyTok::KwFor))  return parseFor();   
            
        }
        
        if (t.tok == PyTok::KwAwait) {
            advance();
            
            return parseSimpleStmt();
        }
        if (t.tok == PyTok::KwIf)      return parseIf();
        if (t.tok == PyTok::KwWhile)   return parseWhile();
        if (t.tok == PyTok::KwFor)     return parseFor();
        if (t.tok == PyTok::KwDef)     return parseDef();
        if (t.tok == PyTok::KwReturn)  return parseReturn();
        if (t.tok == PyTok::KwBreak)   { advance(); PyNode *n = makeNode(PyNodeKind::Break); accept(PyTok::Newline); return n; }
        if (t.tok == PyTok::KwContinue){ advance(); PyNode *n = makeNode(PyNodeKind::Continue); accept(PyTok::Newline); return n; }
        if (t.tok == PyTok::KwPass)    { advance(); PyNode *n = makeNode(PyNodeKind::Pass); accept(PyTok::Newline); return n; }
        if (t.tok == PyTok::KwImport)  return parseImport();
        if (t.tok == PyTok::KwFrom)    return parseFromImport();
        if (t.tok == PyTok::KwGlobal)  return parseGlobal();
        if (t.tok == PyTok::KwDel)     return parseDel();
        return parseSimpleStmt();
    }

    
    PyNode *parseSimpleStmt() {
        PyNode *lhs = parseExpr();
        if (!lhs) return nullptr;

        const PyToken &t = peek();
        if (t.tok == PyTok::Assign) {
            advance();
            PyNode *rhs = parseExpr();
            if (!rhs) return nullptr;
            PyNode *n = makeNode(PyNodeKind::Assign);
            
            n->children.push_back(lhs);
            n->children.push_back(rhs);
            
            while (check(PyTok::Assign)) {
                advance();
                PyNode *more = parseExpr();
                if (!more) return nullptr;
                n->children.push_back(more);
            }
            accept(PyTok::Newline);
            return n;
        }
        if (t.tok == PyTok::PlusEq || t.tok == PyTok::MinusEq ||
            t.tok == PyTok::StarEq || t.tok == PyTok::SlashEq ||
            t.tok == PyTok::PercentEq ||
            t.tok == PyTok::AmpEq || t.tok == PyTok::PipeEq ||
            t.tok == PyTok::CaretEq || t.tok == PyTok::LShiftEq ||
            t.tok == PyTok::RShiftEq || t.tok == PyTok::DoubleStarEq) {
            String op = "+=";
            if (t.tok == PyTok::MinusEq)      op = "-=";
            if (t.tok == PyTok::StarEq)       op = "*=";
            if (t.tok == PyTok::SlashEq)      op = "/=";
            if (t.tok == PyTok::PercentEq)    op = "%=";
            if (t.tok == PyTok::AmpEq)        op = "&=";
            if (t.tok == PyTok::PipeEq)       op = "|=";
            if (t.tok == PyTok::CaretEq)      op = "^=";
            if (t.tok == PyTok::LShiftEq)     op = "<<=";
            if (t.tok == PyTok::RShiftEq)     op = ">>=";
            if (t.tok == PyTok::DoubleStarEq) op = "**=";
            advance();
            PyNode *rhs = parseExpr();
            if (!rhs) return nullptr;
            PyNode *n = makeNode(PyNodeKind::AugAssign);
            n->op = op;
            n->children.push_back(lhs);
            n->children.push_back(rhs);
            accept(PyTok::Newline);
            return n;
        }
        
        PyNode *n = makeNode(PyNodeKind::ExprStmt);
        n->children.push_back(lhs);
        accept(PyTok::Newline);
        return n;
    }

    
    PyNode *parseIf() {
        advance(); 
        PyNode *cond = parseExpr();
        if (!cond) return nullptr;
        if (!expect(PyTok::Colon, "':' after if condition")) return nullptr;
        std::vector<PyNode*> body = parseBlock();
        PyNode *n = makeNode(PyNodeKind::If);
        n->children.push_back(cond);
        
        for (auto *s : body) n->children.push_back(s);
        
        while (check(PyTok::KwElif)) {
            advance();
            PyNode *ec = parseExpr();
            if (!ec) return nullptr;
            if (!expect(PyTok::Colon, "':' after elif condition")) return nullptr;
            std::vector<PyNode*> eb = parseBlock();
            
            PyNode *sub = makeNode(PyNodeKind::If);
            sub->children.push_back(ec);
            for (auto *s : eb) sub->children.push_back(s);
            n->children.push_back(sub);
        }
        if (check(PyTok::KwElse)) {
            advance();
            if (!expect(PyTok::Colon, "':' after else")) return nullptr;
            std::vector<PyNode*> eb = parseBlock();
            
            PyNode *elseCond = makeNode(PyNodeKind::BoolLit);
            elseCond->bVal = true;
            PyNode *sub = makeNode(PyNodeKind::If);
            sub->children.push_back(elseCond);
            for (auto *s : eb) sub->children.push_back(s);
            n->children.push_back(sub);
        }
        return n;
    }

    PyNode *parseWhile() {
        advance();
        PyNode *cond = parseExpr();
        if (!cond) return nullptr;
        if (!expect(PyTok::Colon, "':' after while condition")) return nullptr;
        std::vector<PyNode*> body = parseBlock();
        PyNode *n = makeNode(PyNodeKind::While);
        n->children.push_back(cond);
        for (auto *s : body) n->children.push_back(s);
        return n;
    }

    PyNode *parseFor() {
        advance(); 
        PyNode *target = parseTargetList();
        if (!target) return nullptr;
        if (!expect(PyTok::KwIn, "'in' after for-target")) return nullptr;
        PyNode *iter = parseExpr();
        if (!iter) return nullptr;
        if (!expect(PyTok::Colon, "':' after for-iterable")) return nullptr;
        std::vector<PyNode*> body = parseBlock();
        PyNode *n = makeNode(PyNodeKind::For);
        n->children.push_back(target);
        n->children.push_back(iter);
        for (auto *s : body) n->children.push_back(s);
        return n;
    }

    PyNode *parseTargetList() {
        
        PyNode *first = parseTarget();
        if (!first) return nullptr;
        if (!check(PyTok::Comma)) return first;
        PyNode *n = makeNode(PyNodeKind::TupleLit);
        n->children.push_back(first);
        while (accept(PyTok::Comma)) {
            if (check(PyTok::KwIn) || check(PyTok::Assign) || check(PyTok::Colon)) break;
            PyNode *t = parseTarget();
            if (!t) return nullptr;
            n->children.push_back(t);
        }
        return n;
    }

    PyNode *parseTarget() {
        if (check(PyTok::LParen)) {
            advance();
            PyNode *n = parseTargetList();
            expect(PyTok::RParen, "')'");
            return n;
        }
        if (check(PyTok::LBracket)) {
            advance();
            PyNode *n = parseTargetList();
            expect(PyTok::RBracket, "']'");
            return n;
        }
        if (check(PyTok::Name)) {
            PyNode *n = makeNode(PyNodeKind::Name);
            n->str = advance().text;
            return n;
        }
        error = "Expected target name at line " + String(peek().line);
        return nullptr;
    }

    PyNode *parseDef() {
        advance(); 
        if (!check(PyTok::Name)) { error = "Expected function name"; return nullptr; }
        String name = advance().text;
        if (!expect(PyTok::LParen, "'(' after function name")) return nullptr;
        auto f = std::make_shared<PyFunc>();
        f->name = name;
        while (!check(PyTok::RParen) && error.length() == 0) {
            if (check(PyTok::Star)) {
                advance();
                if (check(PyTok::Name)) {
                    f->params.push_back(advance().text);
                    f->isVariadic = true;
                }
            } else if (check(PyTok::Name)) {
                String pname = advance().text;
                f->params.push_back(pname);
                if (accept(PyTok::Assign)) {
                    PyNode *def = parseExpr();
                    f->defaults.push_back(def);
                } else {
                    
                    
                    if (!f->defaults.empty()) {
                        error = "Non-default argument follows default argument";
                        return nullptr;
                    }
                }
            }
            if (!accept(PyTok::Comma)) break;
        }
        if (!expect(PyTok::RParen, "')' after function parameters")) return nullptr;
        if (!expect(PyTok::Colon, "':' after function header")) return nullptr;
        std::vector<PyNode*> body = parseBlock();
        PyNode *n = makeNode(PyNodeKind::FuncDef);
        n->str = name;
        
        
        
        n->numI = reinterpret_cast<long>(new std::shared_ptr<PyFunc>(f));
        for (auto *s : body) n->children.push_back(s);
        return n;
    }

    PyNode *parseReturn() {
        advance();
        if (check(PyTok::Newline) || check(PyTok::Dedent)) {
            PyNode *n = makeNode(PyNodeKind::Return);
            n->children.push_back(nullptr);   
            accept(PyTok::Newline);
            return n;
        }
        PyNode *v = parseExpr();
        if (!v) return nullptr;
        PyNode *n = makeNode(PyNodeKind::Return);
        n->children.push_back(v);
        accept(PyTok::Newline);
        return n;
    }

    PyNode *parseImport() {
        advance(); 
        PyNode *n = makeNode(PyNodeKind::Import);
        while (true) {
            String name;
            while (check(PyTok::Name) || check(PyTok::Dot)) {
                name += peek().text;
                advance();
            }
            PyNode *m = makeNode(PyNodeKind::StrLit);
            m->str = name;
            if (accept(PyTok::KwAs)) {
                String alias = advance().text;
                PyNode *a = makeNode(PyNodeKind::Name);
                a->str = alias;
                m->children.push_back(a);
            }
            n->children.push_back(m);
            if (!accept(PyTok::Comma)) break;
        }
        accept(PyTok::Newline);
        return n;
    }

    PyNode *parseFromImport() {
        advance(); 
        String modName;
        while (check(PyTok::Name) || check(PyTok::Dot)) {
            modName += peek().text;
            advance();
        }
        if (!expect(PyTok::KwImport, "'import' after module name")) return nullptr;
        PyNode *n = makeNode(PyNodeKind::FromImport);
        n->str = modName;
        bool paren = accept(PyTok::LParen);
        if (check(PyTok::Star)) {
            advance();
            PyNode *star = makeNode(PyNodeKind::StrLit);
            star->str = "*";
            n->children.push_back(star);
        } else {
            while (true) {
                String name = advance().text;
                PyNode *item = makeNode(PyNodeKind::Name);
                item->str = name;
                if (accept(PyTok::KwAs)) {
                    String alias = advance().text;
                    PyNode *a = makeNode(PyNodeKind::Name);
                    a->str = alias;
                    item->children.push_back(a);
                }
                n->children.push_back(item);
                if (!accept(PyTok::Comma)) break;
                if (paren && check(PyTok::RParen)) break;
            }
        }
        if (paren) expect(PyTok::RParen, "')'");
        accept(PyTok::Newline);
        return n;
    }

    PyNode *parseGlobal() {
        advance();
        PyNode *n = makeNode(PyNodeKind::Global);
        while (check(PyTok::Name)) {
            PyNode *nm = makeNode(PyNodeKind::Name);
            nm->str = advance().text;
            n->children.push_back(nm);
            if (!accept(PyTok::Comma)) break;
        }
        accept(PyTok::Newline);
        return n;
    }

    PyNode *parseDel() {
        advance();
        PyNode *n = makeNode(PyNodeKind::Delete);
        while (check(PyTok::Name)) {
            PyNode *nm = makeNode(PyNodeKind::Name);
            nm->str = advance().text;
            n->children.push_back(nm);
            if (!accept(PyTok::Comma)) break;
        }
        accept(PyTok::Newline);
        return n;
    }

    
    std::vector<PyNode*> parseBlock() {
        std::vector<PyNode*> body;
        if (!accept(PyTok::Newline)) {
            
            PyNode *s = parseSimpleStmt();
            if (s) body.push_back(s);
            return body;
        }
        if (!expect(PyTok::Indent, "INDENT after ':'")) return body;
        while (!check(PyTok::Dedent) && !check(PyTok::End) && error.length() == 0) {
            while (accept(PyTok::Newline)) {}
            if (check(PyTok::Dedent) || check(PyTok::End)) break;
            size_t prevPos = pos;
            PyNode *s = parseStmt();
            if (s) {
                body.push_back(s);
            } else {
                
                if (error.length() == 0) break;
                while (!check(PyTok::Newline) && !check(PyTok::Dedent) && !check(PyTok::End)) advance();
                while (accept(PyTok::Newline)) {}
            }
            if (pos == prevPos && !check(PyTok::Dedent) && !check(PyTok::End)) advance();
        }
        accept(PyTok::Dedent);
        return body;
    }

    
    PyNode *parseExpr() { return parseTernary(); }

    PyNode *parseTernary() {
        PyNode *e = parseLambda();
        if (!e) return nullptr;
        if (check(PyTok::KwIf)) {
            
            advance();
            PyNode *cond = parseLambda();
            if (!cond) return nullptr;
            if (!expect(PyTok::KwElse, "'else' in ternary")) return nullptr;
            PyNode *elseE = parseLambda();
            if (!elseE) return nullptr;
            
            PyNode *n = makeNode(PyNodeKind::If);
            n->children.push_back(cond);
            n->children.push_back(e);
            PyNode *elseCond = makeNode(PyNodeKind::BoolLit); elseCond->bVal = true;
            PyNode *sub = makeNode(PyNodeKind::If);
            sub->children.push_back(elseCond);
            sub->children.push_back(elseE);
            n->children.push_back(sub);
            return n;
        }
        return e;
    }

    PyNode *parseLambda() {
        if (check(PyTok::KwLambda)) {
            advance();
            
            std::vector<String> params;
            while (check(PyTok::Name)) {
                params.push_back(advance().text);
                if (!accept(PyTok::Comma)) break;
            }
            if (!expect(PyTok::Colon, "':' in lambda")) return nullptr;
            PyNode *body = parseExpr();
            if (!body) return nullptr;
            PyNode *n = makeNode(PyNodeKind::LambdaExpr);
            auto f = std::make_shared<PyFunc>();
            f->name = "<lambda>";
            f->params = params;
            f->body.push_back(body);
            n->numI = reinterpret_cast<long>(new std::shared_ptr<PyFunc>(f));
            return n;
        }
        return parseOr();
    }

    PyNode *parseOr() {
        PyNode *l = parseAnd();
        if (!l) return nullptr;
        while (check(PyTok::KwOr)) {
            advance();
            PyNode *r = parseAnd();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BoolOp);
            n->op = "or";
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseAnd() {
        PyNode *l = parseNot();
        if (!l) return nullptr;
        while (check(PyTok::KwAnd)) {
            advance();
            PyNode *r = parseNot();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BoolOp);
            n->op = "and";
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseNot() {
        if (check(PyTok::KwNot)) {
            advance();
            PyNode *e = parseNot();
            if (!e) return nullptr;
            PyNode *n = makeNode(PyNodeKind::UnaryOp);
            n->op = "not";
            n->children.push_back(e);
            return n;
        }
        return parseCompare();
    }

    PyNode *parseCompare() {
        PyNode *l = parseBitOr();
        if (!l) return nullptr;
        while (true) {
            PyTok t = peek().tok;
            String op;
            if (t == PyTok::Eq) op = "==";
            else if (t == PyTok::Ne) op = "!=";
            else if (t == PyTok::Lt) op = "<";
            else if (t == PyTok::Gt) op = ">";
            else if (t == PyTok::Le) op = "<=";
            else if (t == PyTok::Ge) op = ">=";
            else if (t == PyTok::KwIn) op = "in";
            else if (t == PyTok::KwNot && peek(1).tok == PyTok::KwIn) {
                advance(); op = "not in";
            }
            else if (t == PyTok::KwIs) {
                if (peek(1).tok == PyTok::KwNot) { advance(); op = "is not"; }
                else op = "is";
            }
            else break;
            advance();
            if (op == "is not" || op == "not in") advance(); 
            PyNode *r = parseBitOr();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::Compare);
            n->op = op;
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseBitOr() {
        PyNode *l = parseBitXor();
        if (!l) return nullptr;
        while (check(PyTok::Pipe)) {
            advance();
            PyNode *r = parseBitXor();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BinOp);
            n->op = "|";
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseBitXor() {
        PyNode *l = parseBitAnd();
        if (!l) return nullptr;
        while (check(PyTok::Caret)) {
            advance();
            PyNode *r = parseBitAnd();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BinOp);
            n->op = "^";
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseBitAnd() {
        PyNode *l = parseShift();
        if (!l) return nullptr;
        while (check(PyTok::Amp)) {
            advance();
            PyNode *r = parseShift();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BinOp);
            n->op = "&";
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseShift() {
        PyNode *l = parseAddSub();
        if (!l) return nullptr;
        while (check(PyTok::LShift) || check(PyTok::RShift)) {
            String op = (peek().tok == PyTok::LShift) ? "<<" : ">>";
            advance();
            PyNode *r = parseAddSub();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BinOp);
            n->op = op;
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseAddSub() {
        PyNode *l = parseMulDiv();
        if (!l) return nullptr;
        while (check(PyTok::Plus) || check(PyTok::Minus)) {
            String op = (peek().tok == PyTok::Plus) ? "+" : "-";
            advance();
            PyNode *r = parseMulDiv();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BinOp);
            n->op = op;
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseMulDiv() {
        PyNode *l = parseUnary();
        if (!l) return nullptr;
        while (check(PyTok::Star) || check(PyTok::Slash) || check(PyTok::DoubleSlash) || check(PyTok::Percent)) {
            String op = "*";
            if (peek().tok == PyTok::Slash) op = "/";
            else if (peek().tok == PyTok::DoubleSlash) op = "//";
            else if (peek().tok == PyTok::Percent) op = "%";
            advance();
            PyNode *r = parseUnary();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BinOp);
            n->op = op;
            n->children.push_back(l);
            n->children.push_back(r);
            l = n;
        }
        return l;
    }

    PyNode *parseUnary() {
        if (check(PyTok::Minus)) {
            advance();
            PyNode *e = parseUnary();
            if (!e) return nullptr;
            PyNode *n = makeNode(PyNodeKind::UnaryOp);
            n->op = "-";
            n->children.push_back(e);
            return n;
        }
        if (check(PyTok::Plus)) {
            advance();
            return parseUnary();
        }
        if (check(PyTok::Tilde)) {
            advance();
            PyNode *e = parseUnary();
            if (!e) return nullptr;
            PyNode *n = makeNode(PyNodeKind::UnaryOp);
            n->op = "~";
            n->children.push_back(e);
            return n;
        }
        
        
        if (check(PyTok::KwAwait)) {
            advance();
            return parseUnary();
        }
        return parsePower();
    }

    PyNode *parsePower() {
        PyNode *l = parsePostfix();
        if (!l) return nullptr;
        if (check(PyTok::DoubleStar)) {
            advance();
            PyNode *r = parseUnary();
            if (!r) return nullptr;
            PyNode *n = makeNode(PyNodeKind::BinOp);
            n->op = "**";
            n->children.push_back(l);
            n->children.push_back(r);
            return n;
        }
        return l;
    }

    PyNode *parsePostfix() {
        PyNode *e = parsePrimary();
        if (!e) return nullptr;
        while (true) {
            if (check(PyTok::LParen)) {
                advance();
                PyNode *n = makeNode(PyNodeKind::Call);
                n->children.push_back(e);
                while (!check(PyTok::RParen) && error.length() == 0) {
                    
                    if (check(PyTok::Star) || check(PyTok::DoubleStar)) advance();
                    PyNode *a = parseExpr();
                    if (!a) return nullptr;
                    
                    
                    
                    if (check(PyTok::KwFor)) {
                        advance(); 
                        PyNode *target = parseTargetList();
                        if (!target) return nullptr;
                        if (!expect(PyTok::KwIn, "'in' in generator expression")) return nullptr;
                        PyNode *iter = parseExpr();
                        if (!iter) return nullptr;
                        PyNode *comp = makeNode(PyNodeKind::ListComp);
                        comp->children.push_back(a);     
                        comp->children.push_back(target);
                        comp->children.push_back(iter);
                        
                        while (check(PyTok::KwIf)) {
                            advance();
                            PyNode *cond = parseOr();
                            if (!cond) return nullptr;
                            comp->children.push_back(cond);
                        }
                        n->children.push_back(comp);
                        
                        break;
                    }
                    n->children.push_back(a);
                    if (!accept(PyTok::Comma)) break;
                }
                if (!expect(PyTok::RParen, "')' after call arguments")) return nullptr;
                e = n;
            } else if (check(PyTok::LBracket)) {
                advance();
                PyNode *n = makeNode(PyNodeKind::Index);
                n->children.push_back(e);
                PyNode *idx = parseSliceOrIndex();
                if (!idx) return nullptr;
                n->children.push_back(idx);
                if (!expect(PyTok::RBracket, "']' after subscript")) return nullptr;
                e = n;
            } else if (check(PyTok::Dot)) {
                advance();
                if (!check(PyTok::Name)) { error = "Expected attribute name after '.'"; return nullptr; }
                PyNode *n = makeNode(PyNodeKind::Attr);
                n->children.push_back(e);
                n->str = advance().text;
                e = n;
            } else {
                break;
            }
        }
        return e;
    }

    PyNode *parseSliceOrIndex() {
        
        
        
        PyNode *start = nullptr;
        if (!check(PyTok::Colon)) {
            start = parseExpr();
            if (!start) return nullptr;
        }
        if (!check(PyTok::Colon)) return start;   
        
        advance(); 
        PyNode *stop = nullptr;
        if (!check(PyTok::Colon) && !check(PyTok::RBracket)) {
            stop = parseExpr();
            if (!stop) return nullptr;
        }
        PyNode *step = nullptr;
        if (accept(PyTok::Colon)) {
            if (!check(PyTok::RBracket)) {
                step = parseExpr();
                if (!step) return nullptr;
            }
        }
        PyNode *n = makeNode(PyNodeKind::Slice);
        if (start) n->children.push_back(start); else n->children.push_back(nullptr);
        if (stop)  n->children.push_back(stop);  else n->children.push_back(nullptr);
        if (step)  n->children.push_back(step);  else n->children.push_back(nullptr);
        return n;
    }

    PyNode *parsePrimary() {
        const PyToken &t = peek();
        if (t.tok == PyTok::Number) {
            advance();
            PyNode *n = makeNode(PyNodeKind::NumLit);
            n->numI = t.iVal;
            n->numF = t.fVal;
            n->numIsFloat = t.isFloat;
            return n;
        }
        if (t.tok == PyTok::String) {
            advance();
            PyNode *n = makeNode(PyNodeKind::StrLit);
            n->str = t.text;
            
            while (check(PyTok::String)) {
                n->str += advance().text;
            }
            return n;
        }
        if (t.tok == PyTok::FString) {
            advance();
            PyNode *n = makeNode(PyNodeKind::FStrLit);
            n->str = t.text;
            n->strIsFString = true;
            return n;
        }
        if (t.tok == PyTok::KwTrue || t.tok == PyTok::KwFalse) {
            advance();
            PyNode *n = makeNode(PyNodeKind::BoolLit);
            n->bVal = (t.tok == PyTok::KwTrue);
            return n;
        }
        if (t.tok == PyTok::KwNone) {
            advance();
            return makeNode(PyNodeKind::NoneLit);
        }
        if (t.tok == PyTok::Name) {
            advance();
            PyNode *n = makeNode(PyNodeKind::Name);
            n->str = t.text;
            return n;
        }
        if (t.tok == PyTok::LParen) {
            advance();
            
            if (accept(PyTok::RParen)) {
                PyNode *n = makeNode(PyNodeKind::TupleLit);
                return n;
            }
            PyNode *e = parseExpr();
            if (!e) return nullptr;
            if (check(PyTok::Comma)) {
                
                PyNode *n = makeNode(PyNodeKind::TupleLit);
                n->children.push_back(e);
                while (accept(PyTok::Comma)) {
                    if (check(PyTok::RParen)) break;
                    PyNode *more = parseExpr();
                    if (!more) return nullptr;
                    n->children.push_back(more);
                }
                if (!expect(PyTok::RParen, "')' after tuple")) return nullptr;
                return n;
            }
            if (!expect(PyTok::RParen, "')' after parenthesised expr")) return nullptr;
            return e;
        }
        if (t.tok == PyTok::LBracket) {
            advance();
            PyNode *n = makeNode(PyNodeKind::ListLit);
            if (!check(PyTok::RBracket)) {
                
                PyNode *first = parseExpr();
                if (!first) return nullptr;
                if (check(PyTok::KwFor)) {
                    
                    advance();
                    PyNode *target = parseTargetList();
                    if (!target) return nullptr;
                    if (!expect(PyTok::KwIn, "'in' in comprehension")) return nullptr;
                    PyNode *iter = parseExpr();
                    if (!iter) return nullptr;
                    PyNode *comp = makeNode(PyNodeKind::ListComp);
                    comp->children.push_back(first);     
                    comp->children.push_back(target);
                    comp->children.push_back(iter);
                    
                    while (check(PyTok::KwIf)) {
                        advance();
                        PyNode *cond = parseOr();
                        if (!cond) return nullptr;
                        comp->children.push_back(cond);
                    }
                    if (!expect(PyTok::RBracket, "']' after comprehension")) return nullptr;
                    return comp;
                }
                n->children.push_back(first);
                while (accept(PyTok::Comma)) {
                    if (check(PyTok::RBracket)) break;
                    PyNode *e = parseExpr();
                    if (!e) return nullptr;
                    n->children.push_back(e);
                }
            }
            if (!expect(PyTok::RBracket, "']' after list")) return nullptr;
            return n;
        }
        if (t.tok == PyTok::LBrace) {
            advance();
            PyNode *n = makeNode(PyNodeKind::DictLit);
            if (!check(PyTok::RBrace)) {
                while (true) {
                    PyNode *k = parseExpr();
                    if (!k) return nullptr;
                    if (!expect(PyTok::Colon, "':' in dict")) return nullptr;
                    PyNode *v = parseExpr();
                    if (!v) return nullptr;
                    PyNode *pair = makeNode(PyNodeKind::TupleLit);
                    pair->children.push_back(k);
                    pair->children.push_back(v);
                    n->children.push_back(pair);
                    if (!accept(PyTok::Comma)) break;
                }
            }
            if (!expect(PyTok::RBrace, "'}' after dict")) return nullptr;
            return n;
        }
        error = "Unexpected token at line " + String(t.line) + ": tok=" + String((int)t.tok);
        return nullptr;
    }
};

PyParseResult pyParse(const std::vector<PyToken> &toks)
{
    PyParseResult r;
    PyParser p(toks);
    r.body = p.parseFile();
    r.error = p.error;
    return r;
}

PyValue evalExpr(PyInterp &interp, PyNode *node, PyInterp::Scope *scope);
void execStmts(PyInterp &interp, const std::vector<PyNode*> &body, PyInterp::Scope *scope);
void execStmt(PyInterp &interp, PyNode *node, PyInterp::Scope *scope);

struct PySignal {
    enum Kind { Return, Break, Continue, Error } kind;
    PyValue value;
    String  errMsg;
    PySignal(Kind k) : kind(k) {}
    PySignal(Kind k, const PyValue &v) : kind(k), value(v) {}
    PySignal(Kind k, const String &e) : kind(k), errMsg(e) {}
};

void PyInterp::throwError(const String &msg) {
    PySignal s(PySignal::Error, msg);
    throw s;
}

static double toFloat(const PyValue &v, PyInterp &interp) {
    if (v.type == PyType::Int) return (double)v.iVal;
    if (v.type == PyType::Float) return v.fVal;
    if (v.type == PyType::Bool) return v.bVal ? 1.0 : 0.0;
    interp.throwError("TypeError: cannot convert " + v.typeName() + " to float");
    return 0;
}
static long toInt(const PyValue &v, PyInterp &interp) {
    if (v.type == PyType::Int) return v.iVal;
    if (v.type == PyType::Float) return (long)v.fVal;
    if (v.type == PyType::Bool) return v.bVal ? 1 : 0;
    if (v.type == PyType::Str) {
        
        char *end = nullptr;
        long n = strtol(v.sVal.c_str(), &end, 10);
        if (end == v.sVal.c_str()) interp.throwError("ValueError: invalid literal for int(): '" + v.sVal + "'");
        return n;
    }
    interp.throwError("TypeError: cannot convert " + v.typeName() + " to int");
    return 0;
}
static String toStr(const PyValue &v) { return v.str(); }

static PyValue doImport(PyInterp &interp, const String &modName);

static void assignTarget(PyInterp &interp, PyNode *target, const PyValue &value, PyInterp::Scope *scope);

static PyValue getAttr(const PyValue &obj, const String &name, PyInterp &interp);

static PyValue doIndex(const PyValue &obj, const PyValue &idx, PyInterp &interp);
static PyValue doSlice(const PyValue &obj, PyNode *startNode, PyNode *stopNode, PyNode *stepNode,
                       PyInterp &interp, PyInterp::Scope *scope);

static PyValue binOp(const String &op, const PyValue &a, const PyValue &b, PyInterp &interp)
{
    
    bool aNum = (a.type == PyType::Int || a.type == PyType::Float || a.type == PyType::Bool);
    bool bNum = (b.type == PyType::Int || b.type == PyType::Float || b.type == PyType::Bool);
    if (aNum && bNum) {
        bool useFloat = (a.type == PyType::Float || b.type == PyType::Float ||
                         op == "/" || op == "//");
        if (op == "+") {
            if (useFloat) return PyValue::makeFloat(toFloat(a, interp) + toFloat(b, interp));
            return PyValue::makeInt(toInt(a, interp) + toInt(b, interp));
        }
        if (op == "-") {
            if (useFloat) return PyValue::makeFloat(toFloat(a, interp) - toFloat(b, interp));
            return PyValue::makeInt(toInt(a, interp) - toInt(b, interp));
        }
        if (op == "*") {
            if (useFloat) return PyValue::makeFloat(toFloat(a, interp) * toFloat(b, interp));
            return PyValue::makeInt(toInt(a, interp) * toInt(b, interp));
        }
        if (op == "/") {
            double bv = toFloat(b, interp);
            if (bv == 0) interp.throwError("ZeroDivisionError: division by zero");
            return PyValue::makeFloat(toFloat(a, interp) / bv);
        }
        if (op == "//") {
            double bv = toFloat(b, interp);
            if (bv == 0) interp.throwError("ZeroDivisionError: integer division by zero");
            double r = toFloat(a, interp) / bv;
            r = (r < 0) ? floor(r) : floor(r);
            if (useFloat) return PyValue::makeFloat(r);
            return PyValue::makeInt((long)r);
        }
        if (op == "%") {
            long bv = toInt(b, interp);
            if (bv == 0) interp.throwError("ZeroDivisionError: modulo by zero");
            long av = toInt(a, interp);
            long r = av % bv;
            if (r != 0 && ((r < 0) != (bv < 0))) r += bv;
            return PyValue::makeInt(r);
        }
        if (op == "**") {
            double av = toFloat(a, interp);
            double bv = toFloat(b, interp);
            double r = pow(av, bv);
            if (useFloat) return PyValue::makeFloat(r);
            return PyValue::makeInt((long)r);
        }
        
        if (op == "&")  return PyValue::makeInt(toInt(a, interp) & toInt(b, interp));
        if (op == "|")  return PyValue::makeInt(toInt(a, interp) | toInt(b, interp));
        if (op == "^")  return PyValue::makeInt(toInt(a, interp) ^ toInt(b, interp));
        if (op == "<<") return PyValue::makeInt(toInt(a, interp) << toInt(b, interp));
        if (op == ">>") return PyValue::makeInt(toInt(a, interp) >> toInt(b, interp));
    }

    
    if (op == "+" && a.type == PyType::Str && b.type == PyType::Str)
        return PyValue::makeStr(a.sVal + b.sVal);
    
    if (op == "%" && a.type == PyType::Str) {
        String fmt = a.sVal;
        String result;
        
        std::vector<PyValue> vals;
        if (b.type == PyType::Tuple && b.listData) {
            for (auto &v : b.listData->items) vals.push_back(v);
        } else {
            vals.push_back(b);
        }
        int vi = 0;
        for (int fi = 0; fi < (int)fmt.length(); fi++) {
            if (fmt[fi] == '%' && fi + 1 < (int)fmt.length()) {
                char spec = fmt[fi + 1];
                if (spec == '%') { result += '%'; fi++; continue; }
                
                PyValue val = (vi < (int)vals.size()) ? vals[vi++] : PyValue::makeNone();
                String valStr;
                if (spec == 's')        valStr = val.str();
                else if (spec == 'r')   valStr = val.repr();
                else if (spec == 'd' || spec == 'i') {
                    valStr = (val.type == PyType::Int) ? String(val.iVal) :
                             (val.type == PyType::Bool) ? String(val.bVal ? 1 : 0) : val.str();
                } else if (spec == 'f') {
                    char buf[32]; snprintf(buf, sizeof(buf), "%f", toFloat(val, interp));
                    valStr = buf;
                } else if (spec == 'x') {
                    char buf[16]; snprintf(buf, sizeof(buf), "%x", (unsigned long)toInt(val, interp));
                    valStr = buf;
                } else if (spec == 'X') {
                    char buf[16]; snprintf(buf, sizeof(buf), "%X", (unsigned long)toInt(val, interp));
                    valStr = buf;
                } else if (spec == 'o') {
                    char buf[16]; snprintf(buf, sizeof(buf), "%o", (unsigned long)toInt(val, interp));
                    valStr = buf;
                } else {
                    
                    result += '%'; result += spec; fi++; continue;
                }
                result += valStr;
                fi++; 
            } else {
                result += fmt[fi];
            }
        }
        return PyValue::makeStr(result);
    }
    if (op == "*" && a.type == PyType::Str && b.type == PyType::Int) {
        String r;
        long n = b.iVal;
        for (long i = 0; i < n; i++) r += a.sVal;
        return PyValue::makeStr(r);
    }
    if (op == "*" && a.type == PyType::Int && b.type == PyType::Str) {
        String r;
        long n = a.iVal;
        for (long i = 0; i < n; i++) r += b.sVal;
        return PyValue::makeStr(r);
    }
    
    if (op == "+" && a.type == PyType::List && b.type == PyType::List) {
        PyValue r = PyValue::makeList();
        if (a.listData) for (auto &x : a.listData->items) r.listData->items.push_back(x);
        if (b.listData) for (auto &x : b.listData->items) r.listData->items.push_back(x);
        return r;
    }
    if (op == "*" && a.type == PyType::List && b.type == PyType::Int) {
        PyValue r = PyValue::makeList();
        long n = b.iVal;
        for (long i = 0; i < n; i++)
            if (a.listData) for (auto &x : a.listData->items) r.listData->items.push_back(x);
        return r;
    }

    interp.throwError("TypeError: unsupported operand type(s) for " + op + ": '" +
                      a.typeName() + "' and '" + b.typeName() + "'");
    return PyValue::makeNone();
}

static PyValue doCompare(const String &op, const PyValue &a, const PyValue &b, PyInterp &interp)
{
    if (op == "==") return PyValue::makeBool(a.equals(b));
    if (op == "!=") return PyValue::makeBool(!a.equals(b));
    if (op == "is") return PyValue::makeBool(a.equals(b));
    if (op == "is not") return PyValue::makeBool(!a.equals(b));
    if (op == "in" || op == "not in") {
        bool found = false;
        if (b.type == PyType::List || b.type == PyType::Tuple) {
            if (b.listData) for (auto &x : b.listData->items) if (x.equals(a)) { found = true; break; }
        } else if (b.type == PyType::Str) {
            if (a.type == PyType::Str) found = b.sVal.indexOf(a.sVal) >= 0;
        } else if (b.type == PyType::Dict) {
            if (a.type == PyType::Str && b.dictData) found = b.dictData->contains(a.sVal);
        }
        return PyValue::makeBool(op == "not in" ? !found : found);
    }
    
    bool aNum = (a.type == PyType::Int || a.type == PyType::Float || a.type == PyType::Bool);
    bool bNum = (b.type == PyType::Int || b.type == PyType::Float || b.type == PyType::Bool);
    if (aNum && bNum) {
        double av = toFloat(a, interp), bv = toFloat(b, interp);
        if (op == "<")  return PyValue::makeBool(av <  bv);
        if (op == ">")  return PyValue::makeBool(av >  bv);
        if (op == "<=") return PyValue::makeBool(av <= bv);
        if (op == ">=") return PyValue::makeBool(av >= bv);
    }
    if (a.type == PyType::Str && b.type == PyType::Str) {
        int c = strcmp(a.sVal.c_str(), b.sVal.c_str());
        if (op == "<")  return PyValue::makeBool(c <  0);
        if (op == ">")  return PyValue::makeBool(c >  0);
        if (op == "<=") return PyValue::makeBool(c <= 0);
        if (op == ">=") return PyValue::makeBool(c >= 0);
    }
    interp.throwError("TypeError: unsupported comparison: " + op + " between " +
                      a.typeName() + " and " + b.typeName());
    return PyValue::makeNone();
}

PyValue evalExpr(PyInterp &interp, PyNode *node, PyInterp::Scope *scope)
{
    if (!node) return PyValue::makeNone();
    if (interp.abortRequested()) interp.throwError("KeyboardInterrupt");

    switch (node->kind) {
        case PyNodeKind::NumLit:
            if (node->numIsFloat) return PyValue::makeFloat(node->numF);
            return PyValue::makeInt(node->numI);

        case PyNodeKind::StrLit:
            return PyValue::makeStr(node->str);

        case PyNodeKind::FStrLit: {
            
            
            String out;
            const String &tpl = node->str;
            int i = 0, n = tpl.length();
            while (i < n) {
                char c = tpl[i];
                if (c == '{' && i + 1 < n && tpl[i + 1] == '{') { out += '{'; i += 2; continue; }
                if (c == '}' && i + 1 < n && tpl[i + 1] == '}') { out += '}'; i += 2; continue; }
                if (c == '{') {
                    i++;
                    String exprSrc;
                    int depth = 1;
                    while (i < n && depth > 0) {
                        char cc = tpl[i];
                        if (cc == '{') depth++;
                        else if (cc == '}') { depth--; if (depth == 0) break; }
                        exprSrc += cc; i++;
                    }
                    if (i < n) i++; 
                    
                    String err;
                    auto toks = pyLex(exprSrc, err);
                    if (err.length() == 0) {
                        auto pr = pyParse(toks);
                        if (pr.ok() && !pr.body.empty()) {
                            
                            PyInterp::Scope inner;
                            inner.parent = scope;
                            try {
                                PyValue v = evalExpr(interp, pr.body[0], &inner);
                                out += v.str();
                            } catch (PySignal &) {
                                pyFreeAst(pr.body);
                                throw;
                            }
                            pyFreeAst(pr.body);
                        } else {
                            pyFreeAst(pr.body);
                        }
                    }
                    continue;
                }
                out += c;
                i++;
            }
            return PyValue::makeStr(out);
        }

        case PyNodeKind::BoolLit:
            return PyValue::makeBool(node->bVal);

        case PyNodeKind::NoneLit:
            return PyValue::makeNone();

        case PyNodeKind::Name: {
            PyValue *v = interp.lookupName(node->str);
            if (v) return *v;
            
            PyValue *b = interp.lookupName("__builtins__");
            if (b && b->type == PyType::Dict && b->dictData) {
                PyValue *fn = b->dictData->find(node->str);
                if (fn) return *fn;
            }
            interp.throwError("NameError: name '" + node->str + "' is not defined");
            return PyValue::makeNone();
        }

        case PyNodeKind::ListLit: {
            PyValue r = PyValue::makeList();
            for (auto *c : node->children) {
                r.listData->items.push_back(evalExpr(interp, c, scope));
            }
            return r;
        }

        case PyNodeKind::TupleLit: {
            PyValue r; r.type = PyType::Tuple;
            r.listData = std::make_shared<PyListData>();
            for (auto *c : node->children) {
                r.listData->items.push_back(evalExpr(interp, c, scope));
            }
            return r;
        }

        case PyNodeKind::DictLit: {
            PyValue r = PyValue::makeDict();
            for (auto *pair : node->children) {
                if (pair->kind == PyNodeKind::TupleLit && pair->children.size() == 2) {
                    PyValue k = evalExpr(interp, pair->children[0], scope);
                    PyValue v = evalExpr(interp, pair->children[1], scope);
                    String key = k.type == PyType::Str ? k.sVal : k.repr();
                    r.dictData->set(key, v);
                }
            }
            return r;
        }

        case PyNodeKind::BinOp: {
            if (node->children.size() < 2) return PyValue::makeNone();
            PyValue a = evalExpr(interp, node->children[0], scope);
            
            PyValue b = evalExpr(interp, node->children[1], scope);
            return binOp(node->op, a, b, interp);
        }

        case PyNodeKind::UnaryOp: {
            if (node->children.empty()) return PyValue::makeNone();
            PyValue v = evalExpr(interp, node->children[0], scope);
            if (node->op == "-") {
                if (v.type == PyType::Int) return PyValue::makeInt(-v.iVal);
                if (v.type == PyType::Float) return PyValue::makeFloat(-v.fVal);
                if (v.type == PyType::Bool) return PyValue::makeInt(v.bVal ? -1 : 0);
                interp.throwError("TypeError: bad operand for unary -: " + v.typeName());
            }
            if (node->op == "~") {
                if (v.type == PyType::Int)  return PyValue::makeInt(~v.iVal);
                if (v.type == PyType::Bool) return PyValue::makeInt(~(long)v.bVal);
                interp.throwError("TypeError: bad operand for unary ~: " + v.typeName());
            }
            if (node->op == "not") return PyValue::makeBool(!v.isTruthy());
            interp.throwError("SyntaxError: unknown unary op " + node->op);
            return PyValue::makeNone();
        }

        case PyNodeKind::Compare: {
            if (node->children.size() < 2) return PyValue::makeNone();
            PyValue a = evalExpr(interp, node->children[0], scope);
            PyValue b = evalExpr(interp, node->children[1], scope);
            return doCompare(node->op, a, b, interp);
        }

        case PyNodeKind::BoolOp: {
            if (node->children.size() < 2) return PyValue::makeNone();
            PyValue a = evalExpr(interp, node->children[0], scope);
            if (node->op == "and") {
                if (!a.isTruthy()) return a;
                return evalExpr(interp, node->children[1], scope);
            } else {
                if (a.isTruthy()) return a;
                return evalExpr(interp, node->children[1], scope);
            }
        }

        case PyNodeKind::Call: {
            if (node->children.empty()) return PyValue::makeNone();
            PyValue fn = evalExpr(interp, node->children[0], scope);
            std::vector<PyValue> args;
            for (size_t i = 1; i < node->children.size(); i++) {
                args.push_back(evalExpr(interp, node->children[i], scope));
            }
            if (fn.type == PyType::BuiltinFunc) {
                return fn.builtinFn(interp, args);
            }
            if (fn.type == PyType::Func && fn.funcData) {
                auto &f = *fn.funcData;
                PyInterp::Scope callScope;
                callScope.parent = &interp.m_globals; 
                
                size_t nParams = f.params.size();
                size_t nArgs = args.size();
                for (size_t i = 0; i < nParams; i++) {
                    if (i < nArgs) {
                        callScope.vars[f.params[i]] = args[i];
                    } else if (i - (nArgs) < f.defaults.size()) {
                        
                        
                        
                        PyNode *def = f.defaults[i - nArgs];
                        if (def) callScope.vars[f.params[i]] = evalExpr(interp, def, &callScope);
                    } else if (f.isVariadic && i == nParams - 1) {
                        callScope.vars[f.params[i]] = PyValue::makeList();
                    }
                }
                if (f.isVariadic && nParams > 0) {
                    PyValue varArgs = PyValue::makeList();
                    size_t fixed = nParams - 1;
                    for (size_t i = fixed; i < nArgs; i++) varArgs.listData->items.push_back(args[i]);
                    callScope.vars[f.params[nParams - 1]] = varArgs;
                }
                interp.pushScope(&callScope);
                try {
                    execStmts(interp, f.body, &callScope);
                } catch (PySignal &s) {
                    interp.popScope();
                    if (s.kind == PySignal::Return) return s.value;
                    throw;
                }
                interp.popScope();
                return PyValue::makeNone();
            }
            interp.throwError("TypeError: '" + fn.typeName() + "' object is not callable");
            return PyValue::makeNone();
        }

        case PyNodeKind::Index: {
            if (node->children.size() < 2) return PyValue::makeNone();
            PyValue obj = evalExpr(interp, node->children[0], scope);
            PyNode *idxNode = node->children[1];
            if (idxNode && idxNode->kind == PyNodeKind::Slice) {
                return doSlice(obj, idxNode->children.size() > 0 ? idxNode->children[0] : nullptr,
                               idxNode->children.size() > 1 ? idxNode->children[1] : nullptr,
                               idxNode->children.size() > 2 ? idxNode->children[2] : nullptr,
                               interp, scope);
            }
            PyValue idx = evalExpr(interp, idxNode, scope);
            return doIndex(obj, idx, interp);
        }

        case PyNodeKind::Slice:
            
            return PyValue::makeNone();

        case PyNodeKind::Attr: {
            if (node->children.empty()) return PyValue::makeNone();
            PyValue obj = evalExpr(interp, node->children[0], scope);
            return getAttr(obj, node->str, interp);
        }

        case PyNodeKind::ListComp: {
            
            
            
            
            PyValue result = PyValue::makeList();
            PyValue iter = evalExpr(interp, node->children[2], scope);
            std::vector<PyValue> items;
            if (iter.type == PyType::List || iter.type == PyType::Tuple) {
                if (iter.listData) items = iter.listData->items;
            } else if (iter.type == PyType::Str) {
                for (char c : iter.sVal) items.push_back(PyValue::makeStr(String(c)));
            } else if (iter.type == PyType::Dict) {
                if (iter.dictData) for (auto &kv : iter.dictData->items) items.push_back(PyValue::makeStr(kv.first));
            }
            PyNode *target = node->children[1];
            PyNode *elem = node->children[0];
            for (auto &item : items) {
                PyInterp::Scope compScope;
                compScope.parent = scope;
                assignTarget(interp, target, item, &compScope);
                bool pass = true;
                for (size_t i = 3; i < node->children.size(); i++) {
                    PyValue cond = evalExpr(interp, node->children[i], &compScope);
                    if (!cond.isTruthy()) { pass = false; break; }
                }
                if (pass) {
                    result.listData->items.push_back(evalExpr(interp, elem, &compScope));
                }
            }
            return result;
        }

        case PyNodeKind::LambdaExpr: {
            if (node->numI == 0) return PyValue::makeNone();  
            auto f = *reinterpret_cast<std::shared_ptr<PyFunc>*>((void*)node->numI);
            return PyValue::makeFunc(f);
        }

        default:
            interp.throwError("SyntaxError: cannot evaluate node kind " + String((int)node->kind));
    }
    return PyValue::makeNone();
}

static PyValue doIndex(const PyValue &obj, const PyValue &idx, PyInterp &interp)
{
    if (obj.type == PyType::Str || obj.type == PyType::List || obj.type == PyType::Tuple) {
        long n = 0;
        if (obj.type == PyType::Str) n = obj.sVal.length();
        else if (obj.listData) n = obj.listData->items.size();
        long i = toInt(idx, interp);
        if (i < 0) i += n;
        if (i < 0 || i >= n) interp.throwError("IndexError: index out of range");
        if (obj.type == PyType::Str) return PyValue::makeStr(String(obj.sVal[(int)i]));
        return obj.listData->items[(size_t)i];
    }
    if (obj.type == PyType::Dict) {
        if (idx.type != PyType::Str) interp.throwError("TypeError: dict keys must be str in this build");
        if (!obj.dictData || !obj.dictData->find(idx.sVal))
            interp.throwError("KeyError: '" + idx.sVal + "'");
        return *obj.dictData->find(idx.sVal);
    }
    interp.throwError("TypeError: '" + obj.typeName() + "' is not subscriptable");
    return PyValue::makeNone();
}

static PyValue doSlice(const PyValue &obj, PyNode *startNode, PyNode *stopNode, PyNode *stepNode,
                       PyInterp &interp, PyInterp::Scope *scope)
{
    long n = 0;
    if (obj.type == PyType::Str) n = obj.sVal.length();
    else if ((obj.type == PyType::List || obj.type == PyType::Tuple) && obj.listData) n = obj.listData->items.size();
    else interp.throwError("TypeError: '" + obj.typeName() + "' is not sliceable");

    long start = 0, stop = n, step = 1;
    if (startNode) start = toInt(evalExpr(interp, startNode, scope), interp);
    if (stopNode)  stop  = toInt(evalExpr(interp, stopNode, scope), interp);
    if (stepNode)  step  = toInt(evalExpr(interp, stepNode, scope), interp);
    if (step == 0) interp.throwError("ValueError: slice step cannot be zero");

    if (start < 0) start += n;
    if (stop  < 0) stop  += n;
    if (step > 0) {
        if (start < 0) start = 0;
        if (stop > n) stop = n;
    } else {
        if (start >= n) start = n - 1;
        if (stop < 0) stop = -1;
    }

    if (obj.type == PyType::Str) {
        String r;
        if (step > 0) for (long i = start; i < stop; i += step) r += obj.sVal[(int)i];
        else          for (long i = start; i > stop; i += step) r += obj.sVal[(int)i];
        return PyValue::makeStr(r);
    }
    PyValue r;
    if (obj.type == PyType::Tuple) {
        r.type = PyType::Tuple;
        r.listData = std::make_shared<PyListData>();
    } else {
        r = PyValue::makeList();
    }
    if (step > 0) for (long i = start; i < stop; i += step) r.listData->items.push_back(obj.listData->items[(size_t)i]);
    else          for (long i = start; i > stop; i += step) r.listData->items.push_back(obj.listData->items[(size_t)i]);
    return r;
}

static PyValue getAttr(const PyValue &obj, const String &name, PyInterp &interp)
{
    
    if (obj.type == PyType::Module && obj.dictData) {
        PyValue *v = obj.dictData->find(name);
        if (v) return *v;
        interp.throwError("AttributeError: module has no attribute '" + name + "'");
    }
    
    if (obj.type == PyType::Str) {
        if (name == "upper")   return PyValue::makeBuiltin("str.upper",   [obj](PyInterp&, const std::vector<PyValue>&) { return PyValue::makeStr(obj.sVal); }); 
        
        
        
        if (name == "lower")   return PyValue::makeBuiltin("str.lower",   [obj](PyInterp&, const std::vector<PyValue>&) -> PyValue { String s = obj.sVal; s.toLowerCase(); return PyValue::makeStr(s); });
        if (name == "upper")   return PyValue::makeBuiltin("str.upper",   [obj](PyInterp&, const std::vector<PyValue>&) -> PyValue { String s = obj.sVal; s.toUpperCase(); return PyValue::makeStr(s); });
        if (name == "strip" || name == "lstrip" || name == "rstrip")
            return PyValue::makeBuiltin("str." + name, [obj, name](PyInterp&, const std::vector<PyValue>&) -> PyValue {
                String s = obj.sVal;
                if (name == "strip" || name == "lstrip") {
                    int i = 0; while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
                    s = s.substring(i);
                }
                if (name == "strip" || name == "rstrip") {
                    int i = s.length() - 1;
                    while (i >= 0 && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i--;
                    s = s.substring(0, i + 1);
                }
                return PyValue::makeStr(s);
            });
        if (name == "split")
            return PyValue::makeBuiltin("str.split", [obj](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                PyValue r = PyValue::makeList();
                String s = obj.sVal;
                String sep = " ";
                if (!args.empty()) sep = args[0].str();
                if (sep.length() == 0) {
                    
                    int i = 0;
                    while (i < (int)s.length()) {
                        while (i < (int)s.length() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
                        if (i >= (int)s.length()) break;
                        int start = i;
                        while (i < (int)s.length() && !(s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
                        r.listData->items.push_back(PyValue::makeStr(s.substring(start, i)));
                    }
                } else {
                    int from = 0;
                    while (true) {
                        int pos = s.indexOf(sep, from);
                        if (pos < 0) {
                            r.listData->items.push_back(PyValue::makeStr(s.substring(from)));
                            break;
                        }
                        r.listData->items.push_back(PyValue::makeStr(s.substring(from, pos)));
                        from = pos + sep.length();
                    }
                }
                (void)interp;
                return r;
            });
        if (name == "join")
            return PyValue::makeBuiltin("str.join", [obj](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) interp.throwError("TypeError: join() requires an iterable");
                const PyValue &lst = args[0];
                String r;
                if (lst.type == PyType::List || lst.type == PyType::Tuple) {
                    if (lst.listData) for (size_t i = 0; i < lst.listData->items.size(); i++) {
                        if (i) r += obj.sVal;
                        r += lst.listData->items[i].str();
                    }
                }
                return PyValue::makeStr(r);
            });
        if (name == "replace")
            return PyValue::makeBuiltin("str.replace", [obj](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                if (args.size() < 2) interp.throwError("TypeError: replace() requires 2 args");
                String s = obj.sVal;
                String from = args[0].str(), to = args[1].str();
                String r; r.reserve(s.length());
                int i = 0;
                while (i < (int)s.length()) {
                    if (s.substring(i).startsWith(from) && from.length() > 0) {
                        r += to;
                        i += from.length();
                    } else {
                        r += s[i];
                        i++;
                    }
                }
                return PyValue::makeStr(r);
            });
        if (name == "find")
            return PyValue::makeBuiltin("str.find", [obj](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) return PyValue::makeInt(-1);
                return PyValue::makeInt(obj.sVal.indexOf(args[0].str()));
            });
        if (name == "startswith")
            return PyValue::makeBuiltin("str.startswith", [obj](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) return PyValue::makeBool(false);
                return PyValue::makeBool(obj.sVal.startsWith(args[0].str()));
            });
        if (name == "endswith")
            return PyValue::makeBuiltin("str.endswith", [obj](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) return PyValue::makeBool(false);
                return PyValue::makeBool(obj.sVal.endsWith(args[0].str()));
            });
        if (name == "format")
            return PyValue::makeBuiltin("str.format", [obj](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                
                String out;
                int argIdx = 0;
                for (int i = 0; i < (int)obj.sVal.length(); i++) {
                    char c = obj.sVal[i];
                    if (c == '{' && i + 1 < obj.sVal.length() && obj.sVal[i + 1] == '{') { out += '{'; i++; continue; }
                    if (c == '}' && i + 1 < obj.sVal.length() && obj.sVal[i + 1] == '}') { out += '}'; i++; continue; }
                    if (c == '{') {
                        
                        i++;
                        while (i < (int)obj.sVal.length() && obj.sVal[i] != '}') i++;
                        if (argIdx < (int)args.size()) out += args[argIdx++].str();
                        continue;
                    }
                    out += c;
                }
                (void)interp;
                return PyValue::makeStr(out);
            });
        if (name == "count")
            return PyValue::makeBuiltin("str.count", [obj](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) return PyValue::makeInt(0);
                String needle = args[0].str();
                if (needle.length() == 0) return PyValue::makeInt(obj.sVal.length() + 1);
                int cnt = 0, from = 0;
                while ((from = obj.sVal.indexOf(needle, from)) >= 0) { cnt++; from += needle.length(); }
                return PyValue::makeInt(cnt);
            });
        interp.throwError("AttributeError: 'str' has no attribute '" + name + "'");
    }
    
    if (obj.type == PyType::List && obj.listData) {
        
        
        
        auto lstData = obj.listData;
        if (name == "append")
            return PyValue::makeBuiltin("list.append", [lstData](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (!args.empty()) lstData->items.push_back(args[0]);
                return PyValue::makeNone();
            });
        if (name == "pop")
            return PyValue::makeBuiltin("list.pop", [lstData](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                if (lstData->items.empty()) interp.throwError("IndexError: pop from empty list");
                long idx = -1;
                if (!args.empty()) idx = toInt(args[0], interp);
                long n = lstData->items.size();
                if (idx < 0) idx += n;
                if (idx < 0 || idx >= n) interp.throwError("IndexError: pop index out of range");
                PyValue v = lstData->items[(size_t)idx];
                lstData->items.erase(lstData->items.begin() + idx);
                return v;
            });
        if (name == "insert")
            return PyValue::makeBuiltin("list.insert", [lstData](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                if (args.size() < 2) interp.throwError("TypeError: insert() requires 2 args");
                long idx = toInt(args[0], interp);
                long n = lstData->items.size();
                if (idx < 0) idx += n;
                if (idx < 0) idx = 0;
                if (idx > n) idx = n;
                lstData->items.insert(lstData->items.begin() + idx, args[1]);
                return PyValue::makeNone();
            });
        if (name == "remove")
            return PyValue::makeBuiltin("list.remove", [lstData](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) interp.throwError("TypeError: remove() requires 1 arg");
                for (size_t i = 0; i < lstData->items.size(); i++) {
                    if (lstData->items[i].equals(args[0])) {
                        lstData->items.erase(lstData->items.begin() + i);
                        return PyValue::makeNone();
                    }
                }
                interp.throwError("ValueError: list.remove(x): x not in list");
                return PyValue::makeNone();
            });
        if (name == "index")
            return PyValue::makeBuiltin("list.index", [lstData](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) interp.throwError("TypeError: index() requires 1 arg");
                for (size_t i = 0; i < lstData->items.size(); i++)
                    if (lstData->items[i].equals(args[0])) return PyValue::makeInt(i);
                interp.throwError("ValueError: " + args[0].repr() + " is not in list");
                return PyValue::makeNone();
            });
        if (name == "count")
            return PyValue::makeBuiltin("list.count", [lstData](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) return PyValue::makeInt(0);
                long cnt = 0;
                for (auto &x : lstData->items) if (x.equals(args[0])) cnt++;
                return PyValue::makeInt(cnt);
            });
        if (name == "sort")
            return PyValue::makeBuiltin("list.sort", [lstData](PyInterp&, const std::vector<PyValue>&) -> PyValue {
                std::sort(lstData->items.begin(), lstData->items.end(),
                          [](const PyValue &a, const PyValue &b) -> bool {
                              if (a.type == PyType::Int && b.type == PyType::Int) return a.iVal < b.iVal;
                              if (a.type == PyType::Str && b.type == PyType::Str) return strcmp(a.sVal.c_str(), b.sVal.c_str()) < 0;
                              return false;
                          });
                return PyValue::makeNone();
            });
        if (name == "reverse")
            return PyValue::makeBuiltin("list.reverse", [lstData](PyInterp&, const std::vector<PyValue>&) -> PyValue {
                std::reverse(lstData->items.begin(), lstData->items.end());
                return PyValue::makeNone();
            });
        if (name == "extend")
            return PyValue::makeBuiltin("list.extend", [lstData](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (!args.empty() && args[0].listData) {
                    for (auto &x : args[0].listData->items) lstData->items.push_back(x);
                }
                return PyValue::makeNone();
            });
        interp.throwError("AttributeError: 'list' has no attribute '" + name + "'");
    }
    
    if (obj.type == PyType::Dict && obj.dictData) {
        auto d = obj.dictData;
        if (name == "keys")
            return PyValue::makeBuiltin("dict.keys", [d](PyInterp&, const std::vector<PyValue>&) -> PyValue {
                PyValue r = PyValue::makeList();
                for (auto &kv : d->items) r.listData->items.push_back(PyValue::makeStr(kv.first));
                return r;
            });
        if (name == "values")
            return PyValue::makeBuiltin("dict.values", [d](PyInterp&, const std::vector<PyValue>&) -> PyValue {
                PyValue r = PyValue::makeList();
                for (auto &kv : d->items) r.listData->items.push_back(kv.second);
                return r;
            });
        if (name == "items")
            return PyValue::makeBuiltin("dict.items", [d](PyInterp&, const std::vector<PyValue>&) -> PyValue {
                PyValue r = PyValue::makeList();
                for (auto &kv : d->items) {
                    PyValue pair; pair.type = PyType::Tuple;
                    pair.listData = std::make_shared<PyListData>();
                    pair.listData->items.push_back(PyValue::makeStr(kv.first));
                    pair.listData->items.push_back(kv.second);
                    r.listData->items.push_back(pair);
                }
                return r;
            });
        if (name == "get")
            return PyValue::makeBuiltin("dict.get", [d](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) return PyValue::makeNone();
                String key = args[0].type == PyType::Str ? args[0].sVal : args[0].repr();
                if (PyValue *v = d->find(key)) return *v;
                if (args.size() > 1) return args[1];
                return PyValue::makeNone();
            });
        if (name == "pop")
            return PyValue::makeBuiltin("dict.pop", [d](PyInterp &interp, const std::vector<PyValue> &args) -> PyValue {
                if (args.empty()) interp.throwError("TypeError: pop() requires 1 arg");
                String key = args[0].type == PyType::Str ? args[0].sVal : args[0].repr();
                for (size_t i = 0; i < d->items.size(); i++) {
                    if (d->items[i].first == key) {
                        PyValue v = d->items[i].second;
                        d->items.erase(d->items.begin() + i);
                        return v;
                    }
                }
                if (args.size() > 1) return args[1];
                interp.throwError("KeyError: '" + key + "'");
                return PyValue::makeNone();
            });
        if (name == "update")
            return PyValue::makeBuiltin("dict.update", [d](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
                if (!args.empty() && args[0].dictData) {
                    for (auto &kv : args[0].dictData->items) d->set(kv.first, kv.second);
                }
                return PyValue::makeNone();
            });
        interp.throwError("AttributeError: 'dict' has no attribute '" + name + "'");
    }
    interp.throwError("AttributeError: '" + obj.typeName() + "' has no attribute '" + name + "'");
    return PyValue::makeNone();
}

static void assignTarget(PyInterp &interp, PyNode *target, const PyValue &value, PyInterp::Scope *scope)
{
    if (!target) return;
    if (target->kind == PyNodeKind::Name) {
        interp.assignName(target->str, value);
        return;
    }
    if (target->kind == PyNodeKind::TupleLit || target->kind == PyNodeKind::ListLit) {
        
        if (!(value.type == PyType::List || value.type == PyType::Tuple) || !value.listData) {
            interp.throwError("TypeError: cannot unpack non-iterable " + value.typeName());
        }
        auto &items = value.listData->items;
        if (items.size() < target->children.size()) {
            interp.throwError("ValueError: not enough values to unpack");
        }
        for (size_t i = 0; i < target->children.size(); i++) {
            assignTarget(interp, target->children[i], items[i], scope);
        }
        return;
    }
    if (target->kind == PyNodeKind::Attr) {
        
        PyValue obj = evalExpr(interp, target->children[0], scope);
        if ((obj.type == PyType::Dict || obj.type == PyType::Module) && obj.dictData) {
            obj.dictData->set(target->str, value);
            return;
        }
        interp.throwError("AttributeError: cannot set attribute on " + obj.typeName());
        return;
    }
    if (target->kind == PyNodeKind::Index) {
        PyValue obj = evalExpr(interp, target->children[0], scope);
        PyNode *idxNode = target->children[1];
        if (idxNode && idxNode->kind == PyNodeKind::Slice) {
            interp.throwError("TypeError: slice assignment not supported in this build");
        }
        PyValue idx = evalExpr(interp, idxNode, scope);
        if (obj.type == PyType::List && obj.listData) {
            long i = toInt(idx, interp);
            long n = obj.listData->items.size();
            if (i < 0) i += n;
            if (i < 0 || i >= n) interp.throwError("IndexError: list assignment out of range");
            obj.listData->items[(size_t)i] = value;
            return;
        }
        if (obj.type == PyType::Dict && obj.dictData) {
            String key = idx.type == PyType::Str ? idx.sVal : idx.repr();
            obj.dictData->set(key, value);
            return;
        }
        interp.throwError("TypeError: '" + obj.typeName() + "' does not support item assignment");
        return;
    }
    interp.throwError("SyntaxError: invalid assignment target");
}

void execStmt(PyInterp &interp, PyNode *node, PyInterp::Scope *scope)
{
    if (!node) return;
    if (interp.abortRequested()) interp.throwError("KeyboardInterrupt");

    switch (node->kind) {
        case PyNodeKind::ExprStmt:
            if (!node->children.empty()) evalExpr(interp, node->children[0], scope);
            return;

        case PyNodeKind::Assign: {
            
            PyValue v = evalExpr(interp, node->children[1], scope);
            assignTarget(interp, node->children[0], v, scope);
            for (size_t i = 2; i < node->children.size(); i++) {
                
                
                
                
                
                
                PyValue chained = (i == node->children.size() - 1)
                                  ? evalExpr(interp, node->children[i], scope)
                                  : v;
                assignTarget(interp, node->children[i - 1], chained, scope);
                v = chained;
            }
            return;
        }

        case PyNodeKind::AugAssign: {
            if (node->children.size() < 2) return;
            PyNode *target = node->children[0];
            PyValue cur = evalExpr(interp, target, scope);
            PyValue rhs  = evalExpr(interp, node->children[1], scope);
            
            
            String op = node->op;
            if (op.endsWith("=")) op.remove(op.length() - 1);
            PyValue result = binOp(op, cur, rhs, interp);
            assignTarget(interp, target, result, scope);
            return;
        }

        case PyNodeKind::If: {
            
            
            
            
            
            bool isTernary = (node->children.size() == 2 &&
                              node->children[1] &&
                              (node->children[1]->kind == PyNodeKind::NumLit ||
                               node->children[1]->kind == PyNodeKind::StrLit ||
                               node->children[1]->kind == PyNodeKind::Name ||
                               node->children[1]->kind == PyNodeKind::BinOp ||
                               node->children[1]->kind == PyNodeKind::Call));
            if (isTernary) {
                
                
            }
            
            PyValue cond = evalExpr(interp, node->children[0], scope);
            if (cond.isTruthy()) {
                
                for (size_t i = 1; i < node->children.size(); i++) {
                    PyNode *c = node->children[i];
                    if (c && c->kind == PyNodeKind::If) break;
                    execStmt(interp, c, scope);
                }
                return;
            }
            
            for (size_t i = 1; i < node->children.size(); i++) {
                PyNode *c = node->children[i];
                if (c && c->kind == PyNodeKind::If) {
                    execStmt(interp, c, scope);
                    return;
                }
            }
            return;
        }

        case PyNodeKind::While: {
            PyNode *cond = node->children[0];
            while (evalExpr(interp, cond, scope).isTruthy()) {
                if (interp.abortRequested()) interp.throwError("KeyboardInterrupt");
                try {
                    for (size_t i = 1; i < node->children.size(); i++) {
                        execStmt(interp, node->children[i], scope);
                    }
                } catch (PySignal &s) {
                    if (s.kind == PySignal::Break) break;
                    if (s.kind == PySignal::Continue) continue;
                    throw;
                }
            }
            return;
        }

        case PyNodeKind::For: {
            PyNode *target = node->children[0];
            PyValue iter = evalExpr(interp, node->children[1], scope);
            std::vector<PyValue> items;
            if (iter.type == PyType::List || iter.type == PyType::Tuple) {
                if (iter.listData) items = iter.listData->items;
            } else if (iter.type == PyType::Str) {
                for (char c : iter.sVal) items.push_back(PyValue::makeStr(String(c)));
            } else if (iter.type == PyType::Dict) {
                if (iter.dictData) for (auto &kv : iter.dictData->items) items.push_back(PyValue::makeStr(kv.first));
            } else if (iter.type == PyType::Int) {
                
            }
            for (auto &item : items) {
                if (interp.abortRequested()) interp.throwError("KeyboardInterrupt");
                assignTarget(interp, target, item, scope);
                try {
                    for (size_t i = 2; i < node->children.size(); i++) {
                        execStmt(interp, node->children[i], scope);
                    }
                } catch (PySignal &s) {
                    if (s.kind == PySignal::Break) break;
                    if (s.kind == PySignal::Continue) continue;
                    throw;
                }
            }
            return;
        }

        case PyNodeKind::Break:    throw PySignal(PySignal::Break);
        case PyNodeKind::Continue: throw PySignal(PySignal::Continue);
        case PyNodeKind::Pass:     return;

        case PyNodeKind::FuncDef: {
            if (node->numI == 0) return;  
            auto f = *reinterpret_cast<std::shared_ptr<PyFunc>*>((void*)node->numI);
            
            f->body.clear();
            for (auto *c : node->children) f->body.push_back(c);
            interp.m_funcPool.push_back(f);   
            
            
            interp.assignName(node->str, PyValue::makeFunc(f));
            return;
        }

        case PyNodeKind::Return: {
            PyValue v = (node->children.empty() || !node->children[0])
                        ? PyValue::makeNone()
                        : evalExpr(interp, node->children[0], scope);
            throw PySignal(PySignal::Return, v);
        }

        case PyNodeKind::Import: {
            for (auto *m : node->children) {
                String modName = m->str;
                String alias = modName;
                if (!m->children.empty() && m->children[0]->kind == PyNodeKind::Name)
                    alias = m->children[0]->str;
                PyValue mod = doImport(interp, modName);
                
                
                interp.assignName(alias, mod);
            }
            return;
        }

        case PyNodeKind::FromImport: {
            PyValue mod = doImport(interp, node->str);
            if (mod.type != PyType::Module || !mod.dictData) return;
            for (auto *item : node->children) {
                if (item->kind == PyNodeKind::StrLit && item->str == "*") {
                    
                    for (auto &kv : mod.dictData->items) {
                        if (!kv.first.startsWith("_")) interp.assignName(kv.first, kv.second);
                    }
                } else if (item->kind == PyNodeKind::Name) {
                    String name = item->str;
                    String alias = name;
                    if (!item->children.empty() && item->children[0]->kind == PyNodeKind::Name)
                        alias = item->children[0]->str;
                    PyValue *v = mod.dictData->find(name);
                    if (v) interp.assignName(alias, *v);
                    else interp.throwError("ImportError: cannot import name '" + name + "' from '" + node->str + "'");
                }
            }
            return;
        }

        case PyNodeKind::Global: {
            
            
            
            
            
            
            return;
        }

        case PyNodeKind::Delete: {
            for (auto *c : node->children) {
                if (c->kind == PyNodeKind::Name) {
                    if (scope->vars.count(c->str)) scope->vars.erase(c->str);
                }
            }
            return;
        }

        default:
            
            interp.throwError("SyntaxError: cannot execute node kind " + String((int)node->kind));
    }
}

void execStmts(PyInterp &interp, const std::vector<PyNode*> &body, PyInterp::Scope *scope)
{
    for (auto *s : body) {
        execStmt(interp, s, scope);
    }
}

static PyValue doImport(PyInterp &interp, const String &modName)
{
    
    {
        auto it = interp.m_modules.find(modName);
        if (it != interp.m_modules.end()) return it->second;
    }
    if (!interp.m_loader) interp.throwError("ImportError: no module loader configured");
    String src = interp.m_loader(modName);
    if (src.length() == 0) interp.throwError("ModuleNotFoundError: No module named '" + modName + "'");

    
    String err;
    auto toks = pyLex(src, err);
    if (err.length() > 0) interp.throwError("SyntaxError in module '" + modName + "': " + err);
    auto pr = pyParse(toks);
    if (!pr.ok()) interp.throwError("SyntaxError in module '" + modName + "': " + pr.error);

    auto modDict = std::make_shared<PyDictData>();
    PyValue modVal = PyValue::makeModule(modDict);
    interp.m_modules[modName] = modVal;

    
    PyInterp::Scope modScope;
    modScope.parent = &interp.m_globals;
    
    modScope.vars["__builtins__"] = interp.m_globals.vars["__builtins__"];

    interp.pushScope(&modScope);
    try {
        execStmts(interp, pr.body, &modScope);
    } catch (PySignal &s) {
        interp.popScope();
        pyFreeAst(pr.body);
        if (s.kind == PySignal::Error) interp.throwError(s.errMsg);
        interp.throwError("ImportError: error while executing module '" + modName + "'");
    } catch (std::exception &e) {
        interp.popScope();
        pyFreeAst(pr.body);
        interp.throwError(String("ImportError: internal error in '") + modName + "': " + e.what());
    } catch (...) {
        interp.popScope();
        pyFreeAst(pr.body);
        interp.throwError("ImportError: unknown error in '" + modName + "'");
    }
    interp.popScope();

    
    for (auto &kv : modScope.vars) {
        if (kv.first == "__builtins__") continue;
        modDict->set(kv.first, kv.second);
    }
    pyFreeAst(pr.body);
    return modVal;
}

static void registerBuiltins(PyInterp &interp)
{
    auto &b = interp.m_globals.vars["__builtins__"];
    b = PyValue::makeDict();

    auto add = [&](const String &name, PyBuiltinFn fn) {
        b.dictData->set(name, PyValue::makeBuiltin(name, fn));
    };

    add("print", [&interp](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        (void)ip;
        String line;
        for (size_t i = 0; i < args.size(); i++) {
            if (i) line += " ";
            line += args[i].str();
        }
        interp.m_console->pyPrint(line + "\n");
        return PyValue::makeNone();
    });

    add("input", [&interp](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        (void)ip;
        String prompt;
        if (!args.empty()) prompt = args[0].str();
        return PyValue::makeStr(interp.m_console->pyInput(prompt));
    });

    add("len", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: len() requires 1 arg");
        const PyValue &v = args[0];
        if (v.type == PyType::Str) return PyValue::makeInt(v.sVal.length());
        if ((v.type == PyType::List || v.type == PyType::Tuple) && v.listData)
            return PyValue::makeInt(v.listData->items.size());
        if (v.type == PyType::Dict && v.dictData) return PyValue::makeInt(v.dictData->items.size());
        ip.throwError("TypeError: object of type '" + v.typeName() + "' has no len()");
        return PyValue::makeNone();
    });

    add("range", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        long start = 0, stop = 0, step = 1;
        if (args.size() == 1) { stop = toInt(args[0], ip); }
        else if (args.size() == 2) { start = toInt(args[0], ip); stop = toInt(args[1], ip); }
        else if (args.size() >= 3) { start = toInt(args[0], ip); stop = toInt(args[1], ip); step = toInt(args[2], ip); }
        if (step == 0) ip.throwError("ValueError: range() arg 3 must not be zero");
        PyValue r = PyValue::makeList();
        if (step > 0) for (long i = start; i < stop; i += step) r.listData->items.push_back(PyValue::makeInt(i));
        else          for (long i = start; i > stop; i += step) r.listData->items.push_back(PyValue::makeInt(i));
        return r;
    });

    add("int", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeInt(0);
        if (args.size() >= 2) {
            
            long base = toInt(args[1], ip);
            String s = args[0].str();
            return PyValue::makeInt(strtoll(s.c_str(), nullptr, (int)base));
        }
        if (args[0].type == PyType::Str) return PyValue::makeInt(toInt(args[0], ip));
        if (args[0].type == PyType::Float) return PyValue::makeInt((long)args[0].fVal);
        if (args[0].type == PyType::Bool) return PyValue::makeInt(args[0].bVal ? 1 : 0);
        if (args[0].type == PyType::Int) return args[0];
        ip.throwError("TypeError: int() argument must be a number or string");
        return PyValue::makeNone();
    });

    add("float", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeFloat(0.0);
        if (args[0].type == PyType::Str) {
            char *end = nullptr;
            double d = strtod(args[0].sVal.c_str(), &end);
            if (end == args[0].sVal.c_str()) ip.throwError("ValueError: could not convert string to float");
            return PyValue::makeFloat(d);
        }
        return PyValue::makeFloat(toFloat(args[0], ip));
    });

    add("str", [](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeStr("");
        return PyValue::makeStr(args[0].str());
    });

    add("bool", [](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeBool(false);
        return PyValue::makeBool(args[0].isTruthy());
    });

    add("list", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeList();
        const PyValue &v = args[0];
        PyValue r = PyValue::makeList();
        if ((v.type == PyType::List || v.type == PyType::Tuple) && v.listData) {
            for (auto &x : v.listData->items) r.listData->items.push_back(x);
        } else if (v.type == PyType::Str) {
            for (char c : v.sVal) r.listData->items.push_back(PyValue::makeStr(String(c)));
        } else if (v.type == PyType::Dict && v.dictData) {
            for (auto &kv : v.dictData->items) r.listData->items.push_back(PyValue::makeStr(kv.first));
        }
        (void)ip;
        return r;
    });

    add("dict", [](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        PyValue r = PyValue::makeDict();
        for (auto &a : args) {
            if ((a.type == PyType::List || a.type == PyType::Tuple) && a.listData) {
                for (auto &kv : a.listData->items) {
                    if (kv.type == PyType::Tuple && kv.listData && kv.listData->items.size() == 2) {
                        String k = kv.listData->items[0].str();
                        r.dictData->set(k, kv.listData->items[1]);
                    }
                }
            }
        }
        return r;
    });

    add("tuple", [](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        PyValue r; r.type = PyType::Tuple;
        r.listData = std::make_shared<PyListData>();
        if (!args.empty() && args[0].listData) {
            for (auto &x : args[0].listData->items) r.listData->items.push_back(x);
        }
        return r;
    });

    add("type", [](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeStr("NoneType");
        return PyValue::makeStr(args[0].typeName());
    });

    add("abs", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: abs() requires 1 arg");
        const PyValue &v = args[0];
        if (v.type == PyType::Int) return PyValue::makeInt(v.iVal < 0 ? -v.iVal : v.iVal);
        if (v.type == PyType::Float) return PyValue::makeFloat(v.fVal < 0 ? -v.fVal : v.fVal);
        ip.throwError("TypeError: bad operand for abs(): " + v.typeName());
        return PyValue::makeNone();
    });

    add("min", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        std::vector<PyValue> items;
        if (args.size() == 1 && (args[0].type == PyType::List || args[0].type == PyType::Tuple) && args[0].listData) {
            items = args[0].listData->items;
        } else {
            items = args;
        }
        if (items.empty()) ip.throwError("ValueError: min() arg is empty");
        PyValue best = items[0];
        for (size_t i = 1; i < items.size(); i++) {
            if (doCompare("<", items[i], best, ip).bVal) best = items[i];
        }
        return best;
    });

    add("max", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        std::vector<PyValue> items;
        if (args.size() == 1 && (args[0].type == PyType::List || args[0].type == PyType::Tuple) && args[0].listData) {
            items = args[0].listData->items;
        } else {
            items = args;
        }
        if (items.empty()) ip.throwError("ValueError: max() arg is empty");
        PyValue best = items[0];
        for (size_t i = 1; i < items.size(); i++) {
            if (doCompare(">", items[i], best, ip).bVal) best = items[i];
        }
        return best;
    });

    add("sum", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: sum() requires 1 arg");
        PyValue acc = args.size() > 1 ? args[1] : PyValue::makeInt(0);
        const PyValue &v = args[0];
        std::vector<PyValue> items;
        if ((v.type == PyType::List || v.type == PyType::Tuple) && v.listData) items = v.listData->items;
        for (auto &x : items) acc = binOp("+", acc, x, ip);
        return acc;
    });

    add("sorted", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: sorted() requires 1 arg");
        const PyValue &v = args[0];
        if (!(v.type == PyType::List || v.type == PyType::Tuple) || !v.listData)
            ip.throwError("TypeError: sorted() expects a list");
        PyValue r = PyValue::makeList();
        r.listData->items = v.listData->items;
        std::sort(r.listData->items.begin(), r.listData->items.end(),
                  [&ip](const PyValue &a, const PyValue &b) -> bool {
                      return doCompare("<", a, b, ip).bVal;
                  });
        return r;
    });

    add("reversed", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: reversed() requires 1 arg");
        const PyValue &v = args[0];
        if ((v.type == PyType::List || v.type == PyType::Tuple) && v.listData) {
            PyValue r = PyValue::makeList();
            r.listData->items = v.listData->items;
            std::reverse(r.listData->items.begin(), r.listData->items.end());
            return r;
        }
        if (v.type == PyType::Str) {
            String r;
            for (int i = v.sVal.length() - 1; i >= 0; i--) r += v.sVal[i];
            return PyValue::makeStr(r);
        }
        ip.throwError("TypeError: argument is not reversible");
        return PyValue::makeNone();
    });

    add("ord", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty() || args[0].type != PyType::Str || args[0].sVal.length() != 1)
            ip.throwError("TypeError: ord() expected a single character string");
        return PyValue::makeInt((long)args[0].sVal[0]);
    });

    add("chr", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: chr() requires 1 arg");
        long n = toInt(args[0], ip);
        char buf[2] = {(char)n, 0};
        return PyValue::makeStr(String(buf));
    });

    add("hex", [](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeStr("0x0");
        String s = "0x" + String(args[0].iVal, HEX);
        return PyValue::makeStr(s);
    });

    add("bin", [](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeStr("0b0");
        long n = args[0].iVal;
        if (n == 0) return PyValue::makeStr("0b0");
        String s = "0b";
        long m = n < 0 ? -n : n;
        String bits;
        while (m) { bits = (m & 1 ? "1" : "0") + bits; m >>= 1; }
        s += bits;
        if (n < 0) s = "-" + s;
        return PyValue::makeStr(s);
    });

    add("round", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: round() requires 1 arg");
        double v = toFloat(args[0], ip);
        double r = (v >= 0) ? floor(v + 0.5) : ceil(v - 0.5);
        if (args[0].type == PyType::Float && !(args.size() > 1)) return PyValue::makeFloat(r);
        return PyValue::makeInt((long)r);
    });

    add("isinstance", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.size() < 2) ip.throwError("TypeError: isinstance() requires 2 args");
        String t = args[1].str();
        return PyValue::makeBool(args[0].typeName() == t);
    });

    
    b.dictData->set("True",  PyValue::makeBool(true));
    b.dictData->set("False", PyValue::makeBool(false));
    b.dictData->set("None",  PyValue::makeNone());
    b.dictData->set("PI",    PyValue::makeFloat(M_PI));
    b.dictData->set("E",     PyValue::makeFloat(M_E));

    
    
    
    auto mathFn = [&b](const String &name, double (*fn)(double)) {
        auto impl = [fn, name](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
            if (args.empty()) ip.throwError("TypeError: " + name + "() requires 1 arg");
            return PyValue::makeFloat(fn(toFloat(args[0], ip)));
        };
        b.dictData->set(name, PyValue::makeBuiltin(name, impl));
        b.dictData->set("_" + name, PyValue::makeBuiltin("_" + name, impl));
    };
    mathFn("sqrt", sqrt);
    mathFn("sin",  sin);
    mathFn("cos",  cos);
    mathFn("tan",  tan);
    mathFn("log",  log);
    mathFn("log2", log2);
    mathFn("log10",log10);
    mathFn("exp",  exp);
    mathFn("floor",floor);
    mathFn("ceil", ceil);

    add("pow", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.size() < 2) ip.throwError("TypeError: pow() requires 2 args");
        return PyValue::makeFloat(pow(toFloat(args[0], ip), toFloat(args[1], ip)));
    });
    
    add("_pow", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.size() < 2) ip.throwError("TypeError: pow() requires 2 args");
        return PyValue::makeFloat(pow(toFloat(args[0], ip), toFloat(args[1], ip)));
    });

    add("time", [](PyInterp&, const std::vector<PyValue>&) -> PyValue {
        return PyValue::makeFloat(millis() / 1000.0);
    });

    add("sleep", [&interp](PyInterp&, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) return PyValue::makeNone();
        double secs = toFloat(args[0], interp);
        unsigned long ms = (unsigned long)(secs * 1000);
        unsigned long start = millis();
        while (millis() - start < ms) {
            if (!interp.m_console->pyTick()) interp.requestAbort();
            delay(5);
        }
        return PyValue::makeNone();
    });

    
    
    
    add("_randint", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.size() < 2) ip.throwError("TypeError: randint() requires 2 args");
        long lo = toInt(args[0], ip);
        long hi = toInt(args[1], ip);
        if (lo > hi) ip.throwError("ValueError: empty range for randint()");
        long range = hi - lo + 1;
        
        long r = lo + (long)(esp_random() % (uint32_t)range);
        return PyValue::makeInt(r);
    });

    add("_choice", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.empty()) ip.throwError("TypeError: choice() requires 1 arg");
        const PyValue &v = args[0];
        if ((v.type == PyType::List || v.type == PyType::Tuple) && v.listData) {
            if (v.listData->items.empty()) ip.throwError("IndexError: choice() from empty sequence");
            long idx = (long)(esp_random() % (uint32_t)v.listData->items.size());
            return v.listData->items[(size_t)idx];
        }
        if (v.type == PyType::Str) {
            if (v.sVal.length() == 0) ip.throwError("IndexError: choice() from empty string");
            long idx = (long)(esp_random() % (uint32_t)v.sVal.length());
            return PyValue::makeStr(String(v.sVal[(int)idx]));
        }
        ip.throwError("TypeError: choice() requires a sequence");
        return PyValue::makeNone();
    });

    add("_random_float", [](PyInterp&, const std::vector<PyValue>&) -> PyValue {
        
        return PyValue::makeFloat((double)esp_random() / (double)0xFFFFFFFF);
    });

    
    add("randint", [](PyInterp &ip, const std::vector<PyValue> &args) -> PyValue {
        if (args.size() < 2) ip.throwError("TypeError: randint() requires 2 args");
        long lo = toInt(args[0], ip);
        long hi = toInt(args[1], ip);
        if (lo > hi) ip.throwError("ValueError: empty range for randint()");
        long range = hi - lo + 1;
        long r = lo + (long)(esp_random() % (uint32_t)range);
        return PyValue::makeInt(r);
    });
}

PyInterp::PyInterp(IPyConsole *console, PyModuleLoaderFn loader)
    : m_loader(loader), m_console(console), m_currentScope(nullptr)
{
    m_globals.parent = nullptr;
    registerBuiltins(*this);
    m_currentScope = &m_globals;
}

PyInterp::~PyInterp()
{
    
}

bool PyInterp::exec(const String &src, const String &filename)
{
    (void)filename;
    String err;
    auto toks = pyLex(src, err);
    if (err.length() > 0) {
        m_lastError = err;
        return false;
    }
    auto pr = pyParse(toks);
    if (!pr.ok()) {
        m_lastError = pr.error;
        return false;
    }
    m_abortRequested = false;
    try {
        execStmts(*this, pr.body, &m_globals);
    } catch (PySignal &s) {
        if (s.kind == PySignal::Error) {
            m_lastError = s.errMsg;
            pyFreeAst(pr.body);
            return false;
        }
        
    } catch (std::exception &e) {
        
        
        m_lastError = String("InternalError: ") + e.what();
        pyFreeAst(pr.body);
        return false;
    } catch (...) {
        
        
        m_lastError = "InternalError: unknown exception";
        pyFreeAst(pr.body);
        return false;
    }
    pyFreeAst(pr.body);
    return true;
}

void PyInterp::registerBuiltin(const String &name, PyBuiltinFn fn)
{
    PyValue *b = lookupName("__builtins__");
    if (b && b->type == PyType::Dict && b->dictData) {
        b->dictData->set(name, PyValue::makeBuiltin(name, fn));
    }
}

void PyInterp::setGlobal(const String &name, const PyValue &v) { m_globals.vars[name] = v; }
PyValue PyInterp::getGlobal(const String &name) const {
    auto it = m_globals.vars.find(name);
    return it != m_globals.vars.end() ? it->second : PyValue::makeNone();
}

void PyInterp::pushScope(Scope *s) { s->parent = m_currentScope; m_currentScope = s; }
void PyInterp::popScope()          { if (m_currentScope) m_currentScope = m_currentScope->parent; }

PyValue *PyInterp::lookupName(const String &name)
{
    for (Scope *s = m_currentScope; s; s = s->parent) {
        auto it = s->vars.find(name);
        if (it != s->vars.end()) return &it->second;
    }
    return nullptr;
}

void PyInterp::assignName(const String &name, const PyValue &v, bool forceLocal)
{
    if (forceLocal && m_currentScope) {
        m_currentScope->vars[name] = v;
        return;
    }
    
    for (Scope *s = m_currentScope; s; s = s->parent) {
        if (s->vars.count(name)) {
            s->vars[name] = v;
            return;
        }
    }
    
    if (m_currentScope) m_currentScope->vars[name] = v;
    else m_globals.vars[name] = v;
}

void pyFreeAst(std::vector<PyNode*> &body)
{
    for (auto *n : body) {
        if (!n) continue;
        if (n->kind == PyNodeKind::FuncDef || n->kind == PyNodeKind::LambdaExpr) {
            
            auto *holder = reinterpret_cast<std::shared_ptr<PyFunc>*>((void*)n->numI);
            if (holder) { delete holder; n->numI = 0; }
        }
        pyFreeAst(n->children);
        delete n;
    }
    body.clear();
}

PyValue evalNode(PyInterp &interp, PyNode *node, PyInterp::Scope *scope) {
    return evalExpr(interp, node, scope);
}
void execStmtsPub(PyInterp &interp, const std::vector<PyNode*> &body, PyInterp::Scope *scope) {
    execStmts(interp, body, scope);
}
