

#ifndef __KITEN_PY_INTERPRETER_H__
#define __KITEN_PY_INTERPRETER_H__

#include <Arduino.h>
#include <vector>
#include <memory>
#include <functional>
#include <map>

struct PyValue;
class  PyInterp;

class IPyConsole {
public:
    virtual ~IPyConsole() = default;
    
    virtual void pyPrint(const String &s) = 0;
    
    
    
    virtual String pyInput(const String &prompt) = 0;
    
    
    virtual bool pyTick() = 0;
};

typedef std::function<String(const String &moduleName)> PyModuleLoaderFn;

enum class PyType : uint8_t {
    None,
    Bool,
    Int,
    Float,
    Str,
    List,
    Dict,
    Tuple,
    Func,           
    BuiltinFunc,    
    Module,         
};

struct PyFunc {
    std::vector<String> params;
    std::vector<struct PyNode*> defaults;   
    std::vector<struct PyNode*> body;
    std::map<String, PyValue>  closure;      
    String name;
    bool   isVariadic = false;               
};

typedef std::function<PyValue(PyInterp &, const std::vector<PyValue> &)> PyBuiltinFn;

struct PyListData {
    std::vector<PyValue> items;
};

struct PyDictData {
    
    std::vector<std::pair<String, PyValue>> items;
    PyValue *find(const String &key);
    void set(const String &key, const PyValue &v);
    bool contains(const String &key);
};

struct PyValue {
    PyType type = PyType::None;

    long   iVal = 0;            
    double fVal = 0.0;          
    bool   bVal = false;        
    String sVal;                

    std::shared_ptr<PyListData>   listData;   
    std::shared_ptr<PyDictData>   dictData;   
    std::shared_ptr<PyFunc>       funcData;
    PyBuiltinFn                   builtinFn;  

    
    static PyValue makeNone();
    static PyValue makeBool(bool b);
    static PyValue makeInt(long v);
    static PyValue makeFloat(double v);
    static PyValue makeStr(const String &s);
    static PyValue makeList();
    static PyValue makeDict();
    static PyValue makeFunc(std::shared_ptr<PyFunc> f);
    static PyValue makeBuiltin(const String &name, PyBuiltinFn fn);
    static PyValue makeModule(std::shared_ptr<PyDictData> d);

    
    bool isTruthy() const;

    
    bool equals(const PyValue &other) const;

    
    String repr() const;
    String str()  const;

    
    String typeName() const;
};

enum class PyTok : uint8_t {
    End, Newline, Indent, Dedent,
    Name, Number, String, FString,
    
    Plus, Minus, Star, Slash, DoubleSlash, Percent, DoubleStar,
    Assign,         
    Eq, Ne, Lt, Gt, Le, Ge,
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Comma, Colon, Dot, Semicolon, Arrow,
    PlusEq, MinusEq, StarEq, SlashEq, 
    
    Amp, Pipe, Caret, Tilde,         
    LShift, RShift,                  
    AmpEq, PipeEq, CaretEq,          
    LShiftEq, RShiftEq,              
    PercentEq,                       
    DoubleStarEq,                    
    At,                              
    
    KwIf, KwElif, KwElse, KwFor, KwWhile, KwDef, KwReturn,
    KwBreak, KwContinue, KwPass, KwImport, KwFrom, KwAs,
    KwTrue, KwFalse, KwNone, KwAnd, KwOr, KwNot, KwIn, KwIs,
    KwLambda, KwGlobal, KwDel,
    KwAsync, KwAwait,
};

struct PyToken {
    PyTok    tok;
    String   text;     
    long     iVal = 0; 
    double   fVal = 0; 
    bool     isFloat = false;
    int      line = 0;
    int      col  = 0;
};

std::vector<PyToken> pyLex(const String &src, String &errOut);

enum class PyNodeKind : uint8_t {
    
    NumLit, StrLit, FStrLit, BoolLit, NoneLit, ListLit, DictLit, TupleLit,
    
    Name, BinOp, UnaryOp, Compare, BoolOp, Call, Index, Slice, Attr,
    ListComp, LambdaExpr,
    
    ExprStmt, Assign, AugAssign, If, For, While, Break, Continue, Pass,
    FuncDef, Return, Import, FromImport, Global, Delete,
};

struct PyNode {
    PyNodeKind kind;
    int line = 0;

    
    long   numI = 0;
    double numF = 0;
    bool   numIsFloat = false;
    bool   bVal = false;        
    String str;             
    bool   strIsFString = false;

    
    
    
    String op;

    
    std::vector<PyNode*> children;

    
    
    

    PyNode() = default;
    explicit PyNode(PyNodeKind k) : kind(k) {}
};

struct PyParseResult {
    std::vector<PyNode*> body;
    String error;
    bool ok() const { return error.length() == 0; }
};

PyParseResult pyParse(const std::vector<PyToken> &toks);

class PyInterp {
public:
    PyInterp(IPyConsole *console, PyModuleLoaderFn loader);
    ~PyInterp();

    
    
    bool exec(const String &src, const String &filename = "<script>");

    
    void registerBuiltin(const String &name, PyBuiltinFn fn);

    
    void setGlobal(const String &name, const PyValue &v);
    PyValue getGlobal(const String &name) const;

    const String &lastError() const { return m_lastError; }

    
    PyModuleLoaderFn m_loader;

    
    IPyConsole *m_console;

    
    struct Scope {
        std::map<String, PyValue> vars;
        Scope *parent = nullptr;
        PyValue *find(const String &name);
        void set(const String &name, const PyValue &v);
        void setLocal(const String &name, const PyValue &v);
    };

    
    void pushScope(Scope *s);
    void popScope();

    
    PyValue *lookupName(const String &name);
    void assignName(const String &name, const PyValue &v, bool forceLocal = false);

    
    [[noreturn]] void throwError(const String &msg);

    
    
    void requestAbort() { m_abortRequested = true; }
    bool abortRequested() const { return m_abortRequested; }

private:
    String       m_lastError;
    bool         m_abortRequested = false;

    friend struct PyNode;
    friend PyValue evalNode(PyInterp &, PyNode *, PyInterp::Scope *);
    friend void execStmtsPub(PyInterp &, const std::vector<PyNode*> &, PyInterp::Scope *);

public:
    
    Scope        m_globals;
    Scope       *m_currentScope;
    std::map<String, PyValue> m_modules;   
    std::vector<std::shared_ptr<PyFunc>> m_funcPool;  
};

void pyFreeAst(std::vector<PyNode*> &body);

PyValue evalNode(PyInterp &interp, PyNode *node, PyInterp::Scope *scope);
void execStmtsPub(PyInterp &interp, const std::vector<PyNode*> &body, PyInterp::Scope *scope);

#endif
