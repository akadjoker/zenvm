#include "compiler.hpp"
#include "process.hpp"
#include <cctype>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <algorithm>

// ============================================================================
// Construtor
// ============================================================================
Compiler::Compiler(VM& vm) : vm(vm) {}

namespace {
const char* token_name(Token token)
{
    switch (token) {
    case Token::PROGRAM: return "program";
    case Token::PROCESS: return "process";
    case Token::FUNCTION: return "function";
    case Token::GLOBAL: return "global";
    case Token::LOCAL: return "local";
    case Token::PRIVATE: return "private";
    case Token::CONST: return "const";
    case Token::BEGIN: return "begin";
    case Token::END: return "end";
    case Token::IF: return "if";
    case Token::ELSE: return "else";
    case Token::WHILE: return "while";
    case Token::LOOP: return "loop";
    case Token::REPEAT: return "repeat";
    case Token::UNTIL: return "until";
    case Token::FROM: return "from";
    case Token::TO: return "to";
    case Token::STEP: return "step";
    case Token::FOR: return "for";
    case Token::SWITCH: return "switch";
    case Token::CASE: return "case";
    case Token::DEFAULT: return "default";
    case Token::BREAK: return "break";
    case Token::CONTINUE: return "continue";
    case Token::FRAME: return "frame";
    case Token::RETURN: return "return";
    case Token::AND_KW: return "and";
    case Token::OR_KW: return "or";
    case Token::NOT_KW: return "not";
    case Token::INT_KW: return "int";
    case Token::FLOAT_KW: return "float";
    case Token::STRING_KW: return "string";
    case Token::ASSIGN: return "=";
    case Token::ADD_ASSIGN: return "+=";
    case Token::SUB_ASSIGN: return "-=";
    case Token::MUL_ASSIGN: return "*=";
    case Token::DIV_ASSIGN: return "/=";
    case Token::MOD_ASSIGN: return "%=";
    case Token::INC: return "++";
    case Token::DEC: return "--";
    case Token::EQ_OP: return "==";
    case Token::NEQ: return "!=";
    case Token::GT: return ">";
    case Token::LT: return "<";
    case Token::GTE: return ">=";
    case Token::LTE: return "<=";
    case Token::PLUS: return "+";
    case Token::MINUS: return "-";
    case Token::STAR: return "*";
    case Token::SLASH: return "/";
    case Token::MOD_OP: return "%";
    case Token::AND_OP: return "&&";
    case Token::OR_OP: return "||";
    case Token::NOT_OP: return "!";
    case Token::BAND_OP: return "&";
    case Token::BOR_OP: return "|";
    case Token::BXOR_OP: return "^";
    case Token::LPAREN: return "(";
    case Token::RPAREN: return ")";
    case Token::LBRACKET: return "[";
    case Token::RBRACKET: return "]";
    case Token::SEMICOLON: return ";";
    case Token::COMMA: return ",";
    case Token::COLON: return ":";
    case Token::DOTDOT: return "..";
    case Token::DOT: return ".";
    case Token::END_OF_FILE: return "<eof>";
    case Token::NUMBER:
    case Token::FLOAT_NUMBER:
    case Token::STRING_LITERAL:
    case Token::IDENT:
        return nullptr;
    }
    return "<token>";
}
}

void Compiler::register_builtin(const std::string& name, int builtin_index)
{
    Symbol s;
    s.name   = name;
    s.kind   = SymKind::BUILTIN;
    s.offset = builtin_index;
    syms.push_back(s);
    // regista nome para disassembly
    if ((int)vm.builtin_names.size() <= builtin_index)
        vm.builtin_names.resize(builtin_index + 1);
    vm.builtin_names[builtin_index] = name;
}

void Compiler::g1(Op op)
{
    int offset = here();
    record_debug_info(offset);
    vm.code.push_back(static_cast<int>(op));
}

void Compiler::g2(Op op, int val)
{
    int offset = here();
    record_debug_info(offset);
    vm.code.push_back(static_cast<int>(op));
    vm.code.push_back(val);
}

void Compiler::g3(Op op, int v1, int v2)
{
    int offset = here();
    record_debug_info(offset);
    vm.code.push_back(static_cast<int>(op));
    vm.code.push_back(v1);
    vm.code.push_back(v2);
}

int Compiler::hole(Op op)
{
    g2(op, 0);
    return here() - 1;
}

// ============================================================================
// Lexer — equivale ao lexico() do DIV
// Lê o próximo token de *src e avança o cursor.
// Resultado em: pieza, num_val, id_val
// ============================================================================
void Compiler::lexico()
{
    // salta espaços e comentários  //
    while (true) {
        while (*src && std::isspace((unsigned char)*src)) src++;
        if (src[0]=='/' && src[1]=='/') {
            while (*src && *src!='\n') src++;
        } else break;
    }

    token_start = src;
    if (!*src) { pieza = Token::END_OF_FILE; return; }

    // número
    if (std::isdigit((unsigned char)*src)) {
        num_val = 0;
        while (std::isdigit((unsigned char)*src)) num_val = num_val*10 + (*src++ - '0');
        if (*src == '.' && src[1] != '.') {
            std::string text = std::to_string(num_val);
            while (!text.empty() && text.back() == '0') text.pop_back();
            if (!text.empty() && text.back() == '.') text.pop_back();
            text += *src++;
            while (std::isdigit((unsigned char)*src)) text += *src++;
            float_val = std::strtof(text.c_str(), nullptr);
            pieza = Token::FLOAT_NUMBER;
        } else {
            pieza = Token::NUMBER;
        }
        return;
    }

    // string literal
    if (*src == '"') {
        src++;
        str_val.clear();
        while (*src && *src != '"') {
            if (*src == '\\' && src[1]) {
                src++;
                switch (*src) {
                case 'n': str_val += '\n'; break;
                case 't': str_val += '\t'; break;
                case '"': str_val += '"'; break;
                case '\\': str_val += '\\'; break;
                default: str_val += *src; break;
                }
                src++;
                continue;
            }
            str_val += *src++;
        }
        if (*src != '"') error("string sem aspas de fecho");
        src++;
        pieza = Token::STRING_LITERAL;
        return;
    }

    // identificador ou palavra reservada
    if (std::isalpha((unsigned char)*src) || *src=='_') {
        id_val.clear();
        while (std::isalnum((unsigned char)*src) || *src=='_') id_val += *src++;

        // tabela de palavras reservadas (= vhash[] do DIV mas simples)
        struct { const char* w; Token t; } kw[] = {
            {"program",  Token::PROGRAM},  {"process",  Token::PROCESS},
            {"global",   Token::GLOBAL},   {"local",    Token::LOCAL},
            {"private",  Token::PRIVATE},  {"const",    Token::CONST},
            {"begin",    Token::BEGIN},    {"end",      Token::END},
            {"if",       Token::IF},       {"else",     Token::ELSE},
            {"while",    Token::WHILE},    {"loop",     Token::LOOP},
            {"repeat",   Token::REPEAT},   {"until",    Token::UNTIL},
            {"from",     Token::FROM},     {"to",       Token::TO},
            {"step",     Token::STEP},     {"for",      Token::FOR},
            {"switch",   Token::SWITCH},   {"case",     Token::CASE},
            {"default",  Token::DEFAULT},
            {"break",    Token::BREAK},    {"continue", Token::CONTINUE},
            {"frame",    Token::FRAME},    {"return",   Token::RETURN},
            {"int",      Token::INT_KW},
            {"float",    Token::FLOAT_KW},
            {"string",   Token::STRING_KW},
            {"function", Token::FUNCTION},
            {"and",      Token::AND_KW},   {"or",       Token::OR_KW},
            {"not",      Token::NOT_KW},
        };
        for (auto& k : kw)
            if (id_val == k.w) { pieza = k.t; return; }

        pieza = Token::IDENT;
        return;
    }

    // operadores e pontuação
    char c = *src++;
    switch (c) {
        case '=':
            if (*src=='=') { src++; pieza = Token::EQ_OP; }
            else                   pieza = Token::ASSIGN;
            return;
        case '!': if (*src=='=') { src++; pieza = Token::NEQ; } else pieza = Token::NOT_OP;  return;
        case '>': if (*src=='=') { src++; pieza = Token::GTE; } else pieza = Token::GT;      return;
        case '<': if (*src=='=') { src++; pieza = Token::LTE; } else pieza = Token::LT;      return;
        case '+':                                                                              // + += ++
            if      (*src=='+') { src++; pieza = Token::INC;        }
            else if (*src=='=') { src++; pieza = Token::ADD_ASSIGN;  }
            else                         pieza = Token::PLUS;
            return;
        case '-':                                                                              // - -= --
            if      (*src=='-') { src++; pieza = Token::DEC;        }
            else if (*src=='=') { src++; pieza = Token::SUB_ASSIGN;  }
            else                         pieza = Token::MINUS;
            return;
        case '*':
            if (*src=='=') { src++; pieza = Token::MUL_ASSIGN; } else pieza = Token::STAR;   return;
        case '/':
            if (*src=='=') { src++; pieza = Token::DIV_ASSIGN; } else pieza = Token::SLASH;  return;
        case '%':
            if (*src=='=') { src++; pieza = Token::MOD_ASSIGN; } else pieza = Token::MOD_OP; return;
        case '&':
            if (*src=='&') { src++; pieza = Token::AND_OP; } else pieza = Token::BAND_OP;    return;
        case '|':
            if (*src=='|') { src++; pieza = Token::OR_OP;  } else pieza = Token::BOR_OP;     return;
        case '^': pieza = Token::BXOR_OP;  return;
        case '(': pieza = Token::LPAREN;   return;
        case ')': pieza = Token::RPAREN;   return;
        case '[': pieza = Token::LBRACKET; return;
        case ']': pieza = Token::RBRACKET; return;
        case ';': pieza = Token::SEMICOLON; return;
        case ':': pieza = Token::COLON;     return;
        case ',': pieza = Token::COMMA;    return;
        case '.':
            if (*src=='.') { src++; pieza = Token::DOTDOT; }  // range em switch: 1..5
            else pieza = Token::DOT;  // acesso remoto: father.x
            return;
        default:
            error(std::string("char desconhecido: ") + c);
    }
}

// ============================================================================
// Symbol table
// ============================================================================
Symbol* Compiler::find_sym(const std::string& name, SymKind /*scope_kind*/)
{
    // procura primeiro locais do processo corrente, depois globais
    // (equivale ao lookup em vhash[] com bloque_lexico do DIV)
    Symbol* found = nullptr;
    for (auto& s : syms)
        if (s.name == name) { found = &s; /* continua — último encontrado tem prioridade (locais sobrepõem globais) */ }
    return found;
}

Symbol& Compiler::add_global(const std::string& name, int array_len)
{
    Symbol s;
    s.name      = name;
    s.kind      = SymKind::GLOBAL;
    s.offset    = next_global;
    s.array_len = array_len;
    int slots   = (array_len >= 0) ? (array_len + 1) : 1;  // array[N] = N+1 elementos
    next_global += slots;
    syms.push_back(s);
    if ((int)vm.globals.size() < next_global) vm.globals.resize(next_global, Value::make_int(0));
    return syms.back();
}

Symbol& Compiler::add_local(const std::string& name, int array_len)
{
    Symbol s;
    s.name      = name;
    s.kind      = SymKind::LOCAL;
    s.offset    = next_local;
    s.array_len = array_len;
    int slots   = (array_len >= 0) ? (array_len + 1) : 1;
    next_local += slots;
    return syms.push_back(s), syms.back();
}

Symbol& Compiler::add_process(const std::string& name, int entry, int loc_offset, int loc_size, int nparams)
{
    Symbol s;
    s.name       = name;
    s.kind       = SymKind::PROCESS;
    s.offset     = entry;
    s.nparams    = nparams;
    s.loc_offset = loc_offset;
    s.loc_size   = loc_size;
    syms.push_back(s);
    // Regista no VM o mapa entry → (loc_offset, loc_size) para uso no spawn()
    vm.proc_loc_map[entry] = {loc_offset, loc_size};
    vm.entry_name_map[entry] = "process '" + name + "'";
    return syms.back();
}

// ============================================================================
// Helpers
// ============================================================================
void Compiler::error(const std::string& msg)
{
    throw std::runtime_error(
        "[compiler] [" + current_scope_label + "] " + current_source_context() + ": " + msg +
        " (token: '" + current_token_text() + "')");
}

void Compiler::warning(const std::string& msg) const
{
    if (!show_warnings)
        return;
    std::fprintf(stderr, "[compiler warning] [%s] %s: %s (token: '%s')\n",
        current_scope_label.c_str(), current_source_context().c_str(), msg.c_str(), current_token_text().c_str());
}

void Compiler::trace(const std::string& msg) const
{
    if (!trace_parse)
        return;
    std::fprintf(stderr, "[compiler trace] [%s] %s: %s\n",
        current_scope_label.c_str(), current_source_context().c_str(), msg.c_str());
}

std::string Compiler::current_token_text() const
{
    switch (pieza) {
    case Token::IDENT:
        return id_val;
    case Token::NUMBER:
        return std::to_string(num_val);
    case Token::FLOAT_NUMBER: {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%g", float_val);
        return buffer;
    }
    case Token::STRING_LITERAL:
        return str_val;
    default: {
        const char* name = token_name(pieza);
        return name ? name : "<token>";
    }
    }
}

std::string Compiler::current_source_context() const
{
    if (!source_begin || !token_start)
        return "sem localizacao";

    int line = 1;
    int column = 1;
    std::string line_text;
    current_source_position(line, column, &line_text);
    std::string caret(column > 1 ? column - 1 : 0, ' ');
    caret += '^';

    return "linha " + std::to_string(line) + ", coluna " + std::to_string(column) +
           "\n    " + line_text +
           "\n    " + caret;
}

void Compiler::current_source_position(int& line, int& column, std::string* line_text) const
{
    line = 1;
    column = 1;
    for (const char* it = source_begin; it < token_start; ++it) {
        if (*it == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }

    if (!line_text)
        return;

    const char* line_begin = token_start;
    while (line_begin > source_begin && line_begin[-1] != '\n')
        --line_begin;

    const char* line_end = token_start;
    while (*line_end && *line_end != '\n')
        ++line_end;

    *line_text = std::string(line_begin, line_end);
}

void Compiler::record_debug_info(int bytecode_offset) const
{
    int line = 1;
    int column = 1;
    std::string line_text;
    current_source_position(line, column, &line_text);

    VM::DebugInfo info;
    info.line = line;
    info.column = column;
    info.scope = current_scope_label;
    info.line_text = std::move(line_text);
    vm.set_debug_info(bytecode_offset, info);
}

Value Compiler::parse_scalar_literal(const char* msg, bool allow_string, bool allow_float)
{
    if (check(Token::NUMBER)) {
        Value value = Value::make_int(num_val);
        lexico();
        return value;
    }
    if (allow_float && check(Token::FLOAT_NUMBER)) {
        Value value = Value::make_float(float_val);
        lexico();
        return value;
    }
    if (allow_string && check(Token::STRING_LITERAL)) {
        Value value = vm.make_string_value(str_val);
        lexico();
        return value;
    }
    error(msg);
    return Value::none();
}

void Compiler::emit_scalar_literal()
{
    if (check(Token::NUMBER)) {
        g2(Op::PUSH_CONST, num_val);
        lexico();
        return;
    }
    if (check(Token::FLOAT_NUMBER)) {
        Value value = Value::make_float(float_val);
        g2(Op::PUSH_FLOAT, value.payload);
        lexico();
        return;
    }
    if (check(Token::STRING_LITERAL)) {
        g2(Op::PUSH_STRING, vm.add_string_literal(str_val));
        lexico();
        return;
    }
    error("esperava literal escalar");
}

void Compiler::expect(Token t, const char* msg)
{
    if (pieza != t) error(std::string("esperava ") + msg);
    lexico();
}

bool Compiler::match(Token t)
{
    if (pieza != t) {
        // : e ; são intercambiáveis (DIV usa ; mas sintaxe C usa :)
        if (t == Token::SEMICOLON && pieza == Token::COLON) {
            warning("':' aceite no lugar de ';'");
            lexico();
            return true;
        }
        if (t == Token::COLON && pieza == Token::SEMICOLON) {
            warning("';' aceite no lugar de ':'");
            lexico();
            return true;
        }
        return false;
    }
    lexico(); return true;
}

// ============================================================================
// PARSE — ponto de entrada
// ============================================================================
int Compiler::compile(const std::string& source)
{
    source_begin = source.c_str();
    src = source.c_str();
    current_scope_label = "program";
    vm.debug_info.clear();
    trace("inicio da compilacao");
    lexico();           // lê o primeiro token (= primeiro lexico() no DIV)
    parse_program();
    return 0;           // entry point do primeiro processo registado
}

// ============================================================================
// parse_program
// Equivale ao sintactico() do DIV
// ============================================================================
void Compiler::parse_program()
{
    // secção global opcional
    if (check(Token::GLOBAL)) {
        lexico();
        parse_global_section();
    }

    // um ou mais processos e/ou funções (em qualquer ordem)
    while (check(Token::PROCESS) || check(Token::FUNCTION)) {
        if (check(Token::PROCESS)) parse_process();
        else                       parse_function();
    }

    if (!check(Token::END_OF_FILE))
        error("esperava 'process', 'function' ou fim de ficheiro");
}

// ============================================================================
// parse_global_section
// global
//   x = 10;
//   y;
// end
// ============================================================================
void Compiler::parse_global_section()
{
    current_scope_label = "global";
    trace("entrar em global");
    // lê variáveis até encontrar 'end'
    while (!check(Token::END) && !check(Token::END_OF_FILE)) {
        if (check(Token::INT_KW) || check(Token::FLOAT_KW) || check(Token::STRING_KW)) lexico(); // tipo opcional
        if (!check(Token::IDENT)) error("esperava nome de variável global");
        std::string name = id_val; lexico();

        int array_len = -1;
        if (check(Token::LBRACKET)) {  // tabela[N] — = ttglo do DIV
            lexico();
            if (!check(Token::NUMBER)) error("esperava tamanho do array");
            array_len = num_val; lexico();
            expect(Token::RBRACKET, "']'");
        }

        Symbol& s = add_global(name, array_len);
        if (array_len < 0 && match(Token::ASSIGN)) {
            // inicialização escalar tipada: x = 10; y = 2.5; z = "abc";
            vm.globals[s.offset] = parse_scalar_literal("esperava literal escalar em global");
        }
        trace("global declarada: " + name);
        match(Token::SEMICOLON);
    }
    expect(Token::END, "'end' depois de 'global'");
    match(Token::SEMICOLON);
    current_scope_label = "program";
}

// ============================================================================
// parse_process
// process nome()
// [local x = 0; end]
// begin
//   statements
// end
// ============================================================================
void Compiler::parse_process()
{
    expect(Token::PROCESS, "'process'");
    if (!check(Token::IDENT)) error("esperava nome do processo");
    std::string proc_name = id_val; lexico();
    current_process = proc_name;
    current_scope_label = "process '" + proc_name + "'";
    trace("declaracao de processo");

    // reset locais para este processo
    syms.erase(std::remove_if(syms.begin(), syms.end(),
        [](const Symbol& s){ return s.kind == SymKind::LOCAL; }), syms.end());

    // Pré-regista variáveis locais predefinidas (= ltobj.def 'local' declarations)
    auto pre = [&](const char* nm, int off) {
        Symbol s; s.name = nm; s.kind = SymKind::LOCAL; s.offset = off;
        syms.push_back(s);
    };
    pre("x",     LOC_X);
    pre("y",     LOC_Y);
    pre("angle", LOC_ANGLE);
    pre("graph", LOC_GRAPH);
    pre("size",  LOC_SIZE);
    pre("flags", LOC_FLAGS);
    pre("file",  LOC_FILE);
    // = _Father/_Son do DIV: id do processo pai / filho mais recente
    pre("father", LOC_FATHER);
    pre("son",    LOC_SON);
    next_local = LOC_USER_START;

    // Parâmetros do processo no DIV entram como locals públicos do tipo.
    expect(Token::LPAREN, "'('");
    int nparams = 0;
    if (!check(Token::RPAREN)) {
        do {
            if (!check(Token::IDENT)) error("esperava nome de parâmetro do processo");
            add_local(id_val);
            vm.loc_init.push_back(Value::make_int(0));
            nparams++;
            lexico();
        } while (match(Token::COMMA));
    }
    expect(Token::RPAREN, "')'");
    match(Token::SEMICOLON);

    // Marca o início da fatia de loc_init[] para este processo (= iloc no DIV)
    int loc_base = (int)vm.loc_init.size() - nparams;

    // entry point do processo
    int entry = here();

    // secção local → valores vão para loc_init[] (= loc[] do DIV)
    // Sem bytecode gerado aqui. Ao spawn, o runtime copia loc_init para locals.
    if (check(Token::LOCAL))   { lexico(); parse_local_section(); }

    // Tamanho da secção local (= iloc_pub_len do DIV)
    int loc_size = (int)vm.loc_init.size() - loc_base;

    // Regista processo com a fatia de loc_init
    add_process(proc_name, entry, loc_base, loc_size, nparams);
    auto it = forward_holes.find(proc_name);
    if (it != forward_holes.end()) {
        for (int h : it->second) vm.code[h] = entry;
        forward_holes.erase(it);
    }

    // secção private → emite LPRI com literais inline no bytecode (= lpri do DIV)
    if (check(Token::PRIVATE)) { lexico(); parse_private_section(); }

    expect(Token::BEGIN, "'begin'");
    match(Token::SEMICOLON);

    while (!check(Token::END) && !check(Token::END_OF_FILE))
        sentencia();

    expect(Token::END, "'end'");
    match(Token::SEMICOLON);

    g1(Op::RETURN);
    current_scope_label = "program";
}

// ============================================================================
// parse_function
// function nome(a, b)
// begin
//   statements
// end
// Parâmetros tornam-se locals[0], locals[1], ...
// FUNC_CALL guarda/restaura locals e ip automaticamente.
// ============================================================================
void Compiler::parse_function()
{
    expect(Token::FUNCTION, "'function'");
    if (!check(Token::IDENT)) error("esperava nome da função");
    std::string fn_name = id_val; lexico();
    current_process = fn_name;
    current_scope_label = "function '" + fn_name + "'";
    trace("declaracao de funcao");

    // reset locais (funções não têm predefined locals; parâmetros começam em 0)
    syms.erase(std::remove_if(syms.begin(), syms.end(),
        [](const Symbol& s){ return s.kind == SymKind::LOCAL; }), syms.end());
    next_local = 0;

    expect(Token::LPAREN, "'('");
    int nparams = 0;
    if (!check(Token::RPAREN)) {
        do {
            if (!check(Token::IDENT)) error("esperava nome de parâmetro");
            add_local(id_val);
            nparams++;
            lexico();
        } while (match(Token::COMMA));
    }
    expect(Token::RPAREN, "')'");
    match(Token::SEMICOLON);

    // secção local opcional (variáveis adicionais da função)
    if (check(Token::LOCAL)) {
        lexico();
        parse_local_section();
    }

    // regista na symbol table (antes do corpo para suportar recursão)
    int entry = here();
    {
        Symbol s;
        s.name    = fn_name;
        s.kind    = SymKind::FUNCTION;
        s.offset  = entry;
        s.nparams = nparams;
        syms.push_back(s);
    }
    vm.entry_name_map[entry] = "function '" + fn_name + "'";
    vm.function_locals_map[entry] = next_local;

    // resolve forward refs (improvavável mas possível)
    auto it = forward_holes.find(fn_name);
    if (it != forward_holes.end()) {
        for (int h : it->second) vm.code[h] = entry;
        forward_holes.erase(it);
    }

    expect(Token::BEGIN, "'begin'");
    match(Token::SEMICOLON);

    in_function = true;
    while (!check(Token::END) && !check(Token::END_OF_FILE))
        sentencia();
    in_function = false;

    expect(Token::END, "'end'");
    match(Token::SEMICOLON);

    // return 0 implícito
    g2(Op::PUSH_CONST, 0);
    g1(Op::FUNC_RET);
    current_scope_label = "program";
}

// ============================================================================
// parse_local_section
// Fiel ao DIV: valores iniciais vão para vm.loc_init[] (= loc[] do DIV).
// Sem bytecode gerado — a cópia acontece em runtime ao spawnar o processo
// (= memcpy(&mem[id],&mem[iloc],iloc_pub_len<<2) no lcal do DIV).
// ============================================================================
void Compiler::parse_local_section()
{
    trace("entrar em local");
    while (!check(Token::END) && !check(Token::END_OF_FILE)) {
        if (check(Token::INT_KW) || check(Token::FLOAT_KW) || check(Token::STRING_KW)) lexico();
        if (!check(Token::IDENT)) error("esperava nome de variável local");
        std::string name = id_val; lexico();
        int array_len = -1;
        if (check(Token::LBRACKET)) {
            lexico();
            if (!check(Token::NUMBER)) error("esperava tamanho do array");
            array_len = num_val; lexico();
            expect(Token::RBRACKET, "']'");
        }
        add_local(name, array_len);
        int slots = (array_len >= 0) ? (array_len + 1) : 1;
        // Inicializador → primeiro slot; resto a zero
        Value init_val = Value::make_int(0);
        if (array_len < 0 && match(Token::ASSIGN)) {
            init_val = parse_scalar_literal("esperava literal escalar");
        }
        vm.loc_init.push_back(init_val);
        for (int i = 1; i < slots; i++) vm.loc_init.push_back(Value::make_int(0));
        trace("local declarada: " + name);
        match(Token::SEMICOLON);
    }
    expect(Token::END, "'end' depois de 'local'");
    match(Token::SEMICOLON);
}

// ============================================================================
// parse_private_section
// Fiel ao DIV: emite LPRI com os valores literais inline no bytecode.
// Em runtime, VM copia esses valores para me->locals[inicio_privadas..].
// (= lpri do DIV: memcpy(&mem[id+inicio_privadas],&mem[ip+1],(mem[ip]-ip-1)<<2))
// ============================================================================
void Compiler::parse_private_section()
{
    trace("entrar em private");
    // Colecta todos os valores de inicialização antes de emitir
    struct PriVar { std::string name; Value init_val; int array_len; };
    std::vector<PriVar> vars;

    while (!check(Token::END) && !check(Token::END_OF_FILE)) {
        if (check(Token::INT_KW) || check(Token::FLOAT_KW) || check(Token::STRING_KW)) lexico();
        if (!check(Token::IDENT)) error("esperava nome de variável private");
        PriVar v;
        v.name = id_val; lexico();
        v.array_len = -1;
        v.init_val  = Value::make_int(0);
        if (check(Token::LBRACKET)) {
            lexico();
            if (!check(Token::NUMBER)) error("esperava tamanho do array");
            v.array_len = num_val; lexico();
            expect(Token::RBRACKET, "']'");
        }
        if (v.array_len < 0 && match(Token::ASSIGN)) {
            v.init_val = parse_scalar_literal("esperava literal escalar");
        }
        vars.push_back(v);
        trace("private declarada: " + v.name);
        match(Token::SEMICOLON);
    }
    expect(Token::END, "'end' depois de 'private'");
    match(Token::SEMICOLON);

    if (vars.empty()) return;

    // Regista os símbolos na tabela
    for (auto& v : vars) add_local(v.name, v.array_len);

    // Emite LPRI jump_addr [kind0 payload0 kind1 payload1 ...] (= lpri adaptado)
    // jump_addr aponta para a instrução DEPOIS do bloco de literais.
    // hole no índice de jump_addr:
    int hole_idx = here();
    record_debug_info(hole_idx);
    vm.code.push_back(static_cast<int>(Op::LPRI));
    int jump_slot = here();       // posição onde vai o jump_addr
    vm.code.push_back(0);         // placeholder

    // Emite os valores literais
    for (auto& v : vars) {
        int slots = (v.array_len >= 0) ? (v.array_len + 1) : 1;
        for (int i = 0; i < slots; i++) {
            Value lit = (i == 0) ? v.init_val : Value::make_int(0);
            vm.code.push_back(static_cast<int>(lit.kind));
            vm.code.push_back(lit.payload);
        }
    }

    // Patcha jump_addr para apontar para cá (= endereço após o bloco)
    vm.code[jump_slot] = here();
    (void)hole_idx;
}


// ============================================================================
// sentencia — dispatcha para o statement correcto
// Equivale ao case ... dentro de sentencia() no divc.cpp
// ============================================================================
void Compiler::sentencia()
{
    trace("sentencia: " + current_token_text());
    switch (pieza) {
        case Token::IF:       lexico(); stmt_if();       break;
        case Token::WHILE:    lexico(); stmt_while();    break;
        case Token::LOOP:     lexico(); stmt_loop();     break;
        case Token::REPEAT:   lexico(); stmt_repeat();   break;
        case Token::FROM:     lexico(); stmt_from();     break;
        case Token::FOR:      lexico(); stmt_for();      break;
        case Token::SWITCH:   lexico(); stmt_switch();   break;
        case Token::FRAME:    lexico(); stmt_frame();    break;
        case Token::RETURN:   lexico(); stmt_return();   break;
        case Token::BREAK:    lexico(); stmt_break();    break;
        case Token::CONTINUE: lexico(); stmt_continue(); break;
        case Token::SEMICOLON: lexico(); break;          // instrução vazia
        case Token::IDENT:    stmt_assign_or_call();     break;
        default:
            error("instrução desconhecida");
    }
}

// ============================================================================
// if (cond) stmts [else stmts] end
// Equivale ao case p_if do divc.cpp
// ============================================================================
void Compiler::stmt_if()
{
    expect(Token::LPAREN, "'('");
    condicion();                     // gera código da condição
    expect(Token::RPAREN, "')'");
    match(Token::SEMICOLON);

    int h1 = hole(Op::JUMP_FALSE);   // im1 — buraco do if

    while (!check(Token::END) && !check(Token::ELSE) && !check(Token::END_OF_FILE))
        sentencia();

    if (match(Token::ELSE)) {
        match(Token::SEMICOLON);
        int h2 = hole(Op::JUMP);     // im2 — salta sobre o else
        patch(h1);                   // tapa buraco do if → aqui começa o else
        while (!check(Token::END) && !check(Token::END_OF_FILE))
            sentencia();
        patch(h2);                   // tapa buraco do jump pós-if
    } else {
        patch(h1);
    }

    expect(Token::END, "'end' depois de 'if'");
    match(Token::SEMICOLON);
}

// ============================================================================
// while (cond) stmts end
// Equivale ao case p_while do divc.cpp:
//   im1 = imem;  condicion(); g2(ljpf,0); im2=imem-1;
//   sentencia(); g2(ljmp,im1); mem[im2]=imem;
// ============================================================================
void Compiler::stmt_while()
{
    int im1 = here();               // início do while (volta aqui no ljmp)

    // guarda posição dos buracos de break/continue para este loop
    int break_base = (int)break_holes.size();
    int cont_base  = (int)cont_holes.size();
    break_holes.push_back(0); break_holes.pop_back(); // não faz nada, só para marcar o nível

    expect(Token::LPAREN, "'('");
    condicion();
    expect(Token::RPAREN, "')'");
    match(Token::SEMICOLON);

    int h_false = hole(Op::JUMP_FALSE);   // im2 — sai do while se falso

    while (!check(Token::END) && !check(Token::END_OF_FILE))
        sentencia();

    expect(Token::END, "'end'");
    match(Token::SEMICOLON);

    g2(Op::JUMP, im1);              // ljmp im1 — volta ao início
    patch(h_false);                 // mem[im2] = imem

    // resolve break holes → saem aqui (depois do loop)
    for (int i = break_base; i < (int)break_holes.size(); i++)
        patch(break_holes[i]);
    break_holes.resize(break_base);

    // resolve continue holes → voltam ao início
    for (int i = cont_base; i < (int)cont_holes.size(); i++)
        vm.code[cont_holes[i]] = im1;
    cont_holes.resize(cont_base);
}

// ============================================================================
// loop stmts end   — loop infinito
// ============================================================================
void Compiler::stmt_loop()
{
    match(Token::SEMICOLON);
    int im1 = here();

    int break_base = (int)break_holes.size();
    int cont_base  = (int)cont_holes.size();

    while (!check(Token::END) && !check(Token::END_OF_FILE))
        sentencia();

    expect(Token::END, "'end' depois de 'loop'");
    match(Token::SEMICOLON);

    g2(Op::JUMP, im1);

    for (int i = break_base; i < (int)break_holes.size(); i++)
        patch(break_holes[i]);
    break_holes.resize(break_base);

    for (int i = cont_base; i < (int)cont_holes.size(); i++)
        vm.code[cont_holes[i]] = im1;
    cont_holes.resize(cont_base);
}

// ============================================================================
// repeat stmts until (cond)
// Equivale ao case p_repeat do divc.cpp:
//   im1=imem; sentencia(); condicion(); g2(ljpf,im1);
//
// Diferença de while: executa SEMPRE pelo menos uma vez,
// e sai quando condição é VERDADEIRA (oposto do while).
// ============================================================================
void Compiler::stmt_repeat()
{
    match(Token::SEMICOLON);

    int im1 = here();   // início do body

    int break_base = (int)break_holes.size();
    int cont_base  = (int)cont_holes.size();

    while (!check(Token::UNTIL) && !check(Token::END_OF_FILE))
        sentencia();

    expect(Token::UNTIL, "'until' depois de 'repeat'");

    // until (condição) — sai quando verdadeiro, repete quando falso
    expect(Token::LPAREN, "'('");
    condicion();
    expect(Token::RPAREN, "')'");
    match(Token::SEMICOLON);

    g2(Op::JUMP_FALSE, im1);    // ljpf im1 — repete se condição falsa

    // break → aqui (depois do until); continue → im1 (início do body)
    for (int i = break_base; i < (int)break_holes.size(); i++)  patch(break_holes[i]);
    break_holes.resize(break_base);
    for (int i = cont_base;  i < (int)cont_holes.size();  i++)  vm.code[cont_holes[i]] = im1;
    cont_holes.resize(cont_base);
}

// ============================================================================
// for (init; cond; incr) stmts end
// Equivale ao case p_for do divc.cpp.
//
// Layout do bytecode (igual ao DIV):
//   [init]               ← stmt_assign_or_call(); consome o ';' do init
//   [im1]: cond; JUMP_FALSE [end]
//          JUMP [body]   ← 1ª iteração salta o incremento
//   [im4]: incr          ← continue aponta aqui
//          JUMP [im1]
//   [body]: stmts
//           JUMP [im4]
//   [end]:
// ============================================================================
void Compiler::stmt_for()
{
    expect(Token::LPAREN, "'('");

    int break_base = (int)break_holes.size();
    int cont_base  = (int)cont_holes.size();

    // 1. Init — atribuição(ões) ou vazio
    //    stmt_assign_or_call() consome o seu próprio ';' (= o primeiro ';' do for)
    if (check(Token::SEMICOLON)) {
        lexico();  // init vazio — consome ';'
    } else if (check(Token::IDENT)) {
        stmt_assign_or_call();  // consome o ';' que separa init da condição
    }
    // Neste ponto já estamos imediatamente antes da condição

    int im1 = here();   // início da condição

    // 2. Condição — ou vazio (= true)
    if (check(Token::SEMICOLON)) {
        g2(Op::PUSH_CONST, 1);
        lexico();
    } else {
        condicion();
        expect(Token::SEMICOLON, "';' depois de condição");
    }
    int h_end  = hole(Op::JUMP_FALSE);  // im2 — sai se falso
    int h_body = hole(Op::JUMP);        // im3 — salta incremento na 1ª iteração

    // 3. Incremento — sem ';' obrigatório (termina com ')')
    int im4 = here();  // continue aponta aqui
    if (!check(Token::RPAREN) && check(Token::IDENT)) {
        stmt_assign_or_call();  // match(';') é opcional — não encontra ';' aqui
    }
    expect(Token::RPAREN, "')'");
    match(Token::SEMICOLON);  // ';' opcional após ')'

    g2(Op::JUMP, im1);  // depois do incremento → re-verifica condição
    patch(h_body);      // body começa aqui

    // 4. Body
    while (!check(Token::END) && !check(Token::END_OF_FILE))
        sentencia();
    expect(Token::END, "'end' depois de 'for'");
    match(Token::SEMICOLON);

    g2(Op::JUMP, im4);  // fim do body → executa incremento
    patch(h_end);       // end do loop

    for (int i = break_base; i < (int)break_holes.size(); i++)  patch(break_holes[i]);
    break_holes.resize(break_base);
    for (int i = cont_base;  i < (int)cont_holes.size();  i++)  vm.code[cont_holes[i]] = im4;
    cont_holes.resize(cont_base);
}

// ============================================================================
// switch (expr)
//   case V:       stmts end
//   case Lo..Hi:  stmts end    ← range (= lcsr do DIV)
//   default:      stmts end
// end
//
// Equivale ao case p_switch do divc.cpp.
//
// O valor do switch permanece na stack durante toda a execução
// (= a stack do DIV tem o valor ao longo do switch inteiro).
// Cada CASE_EQ/CASE_RNG compara sem consumir o switch_val.
// O POP final (ou no início do body) consome o switch_val.
// ============================================================================
void Compiler::stmt_switch()
{
    // switch (expr) — valor fica na stack
    expect(Token::LPAREN, "'('");
    condicion();
    expect(Token::RPAREN, "')'");
    while (check(Token::SEMICOLON)) lexico();

    int im1 = 0;   // buraco do último CASE_EQ/CASE_RNG não correspondido
    int im2 = 0;   // linked list de JMPs para o end (= im2 do DIV)

    while (!check(Token::END) && !check(Token::END_OF_FILE)) {
        if (check(Token::CASE)) {
            lexico();  // consome 'case'

            // pode ter múltiplos valores: case 1, 3, 5:
            // im3 = linked list de JMPs para o body (multi-value)
            int im3 = 0;
            do {
                if (im1) patch(im1);    // tapa buraco do case anterior (falhou → tenta este)

                // valor inferior / único
                expr();

                if (check(Token::DOTDOT)) {
                    // case Lo..Hi  (= lcsr do DIV)
                    lexico();
                    expr();                        // valor superior
                    int h = (int)vm.code.size();
                    g2(Op::CASE_RNG, 0); im1 = h + 1;
                } else {
                    // case V  (= lcse do DIV)
                    int h = (int)vm.code.size();
                    g2(Op::CASE_EQ, 0); im1 = h + 1;
                }

                if (check(Token::COMMA)) {
                    // múltiplos valores: se match → salta para o body
                    lexico();
                    int h = (int)vm.code.size();
                    vm.code.push_back(static_cast<int>(Op::JUMP));
                    vm.code.push_back(im3);  // linked list
                    im3 = h + 1;
                }
            } while (check(Token::COMMA));

            // resolve os JMPs dos valores multi-case → body
            while (im3) { int nxt = vm.code[im3]; vm.code[im3] = here(); im3 = nxt; }

        } else if (check(Token::DEFAULT)) {
            lexico();
            if (im1) { patch(im1); im1 = 0; }  // default: tapa o buraco do último case
        } else {
            error("esperava 'case' ou 'default' dentro de 'switch'");
        }

        while (check(Token::SEMICOLON) || check(Token::COMMA) || check(Token::COLON)) lexico();

        // POP o switch_val antes do body (= lasp do DIV)
        g1(Op::POP);

        // body do case — até 'end'
        while (!check(Token::END) && !check(Token::END_OF_FILE))
            sentencia();
        expect(Token::END, "'end' dentro de 'switch'");
        match(Token::SEMICOLON);

        // JUMP para o end do switch (break implícito)
        int h = (int)vm.code.size();
        vm.code.push_back(static_cast<int>(Op::JUMP));
        vm.code.push_back(im2);  // linked list de JMPs para end
        im2 = h + 1;
    }

    // fim do switch
    expect(Token::END, "'end' depois de 'switch'");
    match(Token::SEMICOLON);

    // último case não correspondeu e não há default → cai aqui, descarta switch_val
    if (im1) patch(im1);
    g1(Op::POP);   // = lasp: descarta switch_val (sem match ou após default)

    // resolve todos os JMPs de break implícito → aqui
    while (im2) { int nxt = vm.code[im2]; vm.code[im2] = here(); im2 = nxt; }
}

// ============================================================================
// from x = A to B [step S]; stmts end
// Equivale ao case p_from do divc.cpp
// ============================================================================
void Compiler::stmt_from()
{
    if (!check(Token::IDENT)) error("esperava variável em 'from'");
    std::string var = id_val; lexico();

    expect(Token::ASSIGN, "'='");

    // valor inicial
    if (!check(Token::NUMBER)) error("esperava número em 'from'");
    int from_val = num_val; lexico();

    expect(Token::TO, "'to'");

    if (!check(Token::NUMBER)) error("esperava número em 'to'");
    int to_val = num_val; lexico();

    int step_val = (from_val <= to_val) ? 1 : -1;
    if (match(Token::STEP)) {
        if (!check(Token::NUMBER)) error("esperava número em 'step'");
        step_val = num_val; lexico();
    }
    match(Token::SEMICOLON);

    // var = from_val
    Symbol* s = find_sym(var);
    if (!s) error("variável não declarada: " + var);

    g2(Op::PUSH_CONST, from_val);
    if (s->kind == SymKind::GLOBAL) g2(Op::STORE_GLOBAL, s->offset);
    else                            g2(Op::STORE_LOCAL,  s->offset);

    int im1 = here();   // início do loop from

    int break_base = (int)break_holes.size();
    int cont_base  = (int)cont_holes.size();

    // condição: var <= to_val  (ou >= se step negativo)
    if (s->kind == SymKind::GLOBAL) g2(Op::LOAD_GLOBAL, s->offset);
    else                            g2(Op::LOAD_LOCAL,  s->offset);
    g2(Op::PUSH_CONST, to_val);
    if (step_val > 0) g1(Op::LTE); else g1(Op::GTE);

    int h_false = hole(Op::JUMP_FALSE);

    while (!check(Token::END) && !check(Token::END_OF_FILE))
        sentencia();

    expect(Token::END, "'end' depois de 'from'");
    match(Token::SEMICOLON);

    int cont_target = here();   // continue volta aqui (incremento)

    // var += step
    if (s->kind == SymKind::GLOBAL) g2(Op::LOAD_GLOBAL, s->offset);
    else                            g2(Op::LOAD_LOCAL,  s->offset);
    g2(Op::PUSH_CONST, step_val);
    g1(Op::ADD);
    if (s->kind == SymKind::GLOBAL) g2(Op::STORE_GLOBAL, s->offset);
    else                            g2(Op::STORE_LOCAL,  s->offset);

    g2(Op::JUMP, im1);
    patch(h_false);

    for (int i = break_base; i < (int)break_holes.size(); i++)  patch(break_holes[i]);
    break_holes.resize(break_base);
    for (int i = cont_base;  i < (int)cont_holes.size();  i++)  vm.code[cont_holes[i]] = cont_target;
    cont_holes.resize(cont_base);
}

// ============================================================================
// frame;  ou  frame(n);
// ============================================================================
void Compiler::stmt_frame()
{
    if (match(Token::LPAREN)) {
        if (!check(Token::NUMBER)) error("esperava número em frame(n)");
        int n = num_val; lexico();
        expect(Token::RPAREN, "')'");
        g2(Op::FRAME_N, n);   // lfrf
    } else {
        g1(Op::FRAME);        // lfrm
    }
    match(Token::SEMICOLON);
}

// ============================================================================
// return;
// ============================================================================
void Compiler::stmt_return()
{
    if (in_function) {
        // return expr;  ou  return;  (sem valor → retorna 0)
        if (!check(Token::SEMICOLON) && !check(Token::END_OF_FILE) && !check(Token::END))
            expr();
        else
            g2(Op::PUSH_CONST, 0);
        match(Token::SEMICOLON);
        g1(Op::FUNC_RET);
    } else {
        g1(Op::RETURN);
        match(Token::SEMICOLON);
    }
}

// ============================================================================
// break;
// ============================================================================
void Compiler::stmt_break()
{
    int h = hole(Op::JUMP);   // buraco resolvido no end do loop
    break_holes.push_back(h);
    match(Token::SEMICOLON);
}

// ============================================================================
// continue;
// ============================================================================
void Compiler::stmt_continue()
{
    int h = hole(Op::JUMP);
    cont_holes.push_back(h);
    match(Token::SEMICOLON);
}

// ============================================================================
// stmt_assign_or_call — despacha atribuições, chamadas, arrays, ++ --
//
// Suporta (fiel ao DIV):
//   x = expr;           → STORE_GLOBAL / STORE_LOCAL
//   x[i] = expr;        → STORE_GLOBAL_IDX / STORE_LOCAL_IDX
//   x += expr;          → LOAD, expr, ADD, STORE
//   x[i] += expr;       → idx, DUP, LOAD_IDX, expr, ADD, STORE_IDX
//   x++;  x--;          → LOAD, PUSH 1, ADD/SUB, STORE
//   proc();             → SPAWN / CALL_BUILTIN
// ============================================================================
void Compiler::stmt_assign_or_call()
{
    std::string name = id_val; lexico();
    Symbol* s = find_sym(name);

    // --- acesso remoto: ident.field = expr  (= laid + lasi do DIV) ---
    if (check(Token::DOT)) {
        lexico();
        if (!check(Token::IDENT)) error("esperava nome de campo após '.'");
        std::string field = id_val; lexico();
        Symbol* fs = find_sym(field);
        if (!fs || fs->kind != SymKind::LOCAL)
            error("campo remoto não encontrado: " + field);

        if (!s || s->kind != SymKind::LOCAL)
            error("variável '" + name + "' não é local (acesso remoto requer local)");

        // Emite id do processo-alvo
        g2(Op::LOAD_LOCAL, s->offset);

        Token op = pieza;
        if (op == Token::ASSIGN) {
            lexico(); expr(); match(Token::SEMICOLON);
            g2(Op::STORE_ID_LOCAL, fs->offset);
        } else if (op == Token::ADD_ASSIGN || op == Token::SUB_ASSIGN ||
                   op == Token::MUL_ASSIGN || op == Token::DIV_ASSIGN ||
                   op == Token::MOD_ASSIGN) {
            // father.x += expr  →  DUP id, LOAD_ID_LOCAL, expr, op, STORE_ID_LOCAL
            g1(Op::DUP);
            g2(Op::LOAD_ID_LOCAL, fs->offset);
            lexico(); expr(); match(Token::SEMICOLON);
            switch (op) {
                case Token::ADD_ASSIGN: g1(Op::ADD); break;
                case Token::SUB_ASSIGN: g1(Op::SUB); break;
                case Token::MUL_ASSIGN: g1(Op::MUL); break;
                case Token::DIV_ASSIGN: g1(Op::DIV); break;
                case Token::MOD_ASSIGN: g1(Op::MOD); break;
                default: break;
            }
            g2(Op::STORE_ID_LOCAL, fs->offset);
        } else {
            error("operador de atribuição esperado após 'ident.field'");
        }
        return;
    }

    // --- chamada de função/processo ---
    if (check(Token::LPAREN)) {
        lexico();

        if (s && s->kind == SymKind::BUILTIN) {
            int nargs = 0;
            if (!check(Token::RPAREN)) {
                do { expr(); nargs++; } while (match(Token::COMMA));
            }
            expect(Token::RPAREN, "')'");
            match(Token::SEMICOLON);
            g2(Op::CALL_BUILTIN, s->offset);
            return;
        }

        if (s && s->kind == SymKind::FUNCTION) {
            int nargs = 0;
            if (!check(Token::RPAREN)) {
                do { expr(); nargs++; } while (match(Token::COMMA));
            }
            expect(Token::RPAREN, "')'");
            match(Token::SEMICOLON);
            g3(Op::FUNC_CALL, s->offset, nargs);
            g1(Op::POP);  // descarta valor de retorno (chamada como statement)
            return;
        }

        // spawn de processo
        int nargs = 0;
        if (!check(Token::RPAREN)) {
            do { expr(); nargs++; } while (match(Token::COMMA));
        }
        expect(Token::RPAREN, "')'");
        match(Token::SEMICOLON);

        if (s && s->kind == SymKind::PROCESS) {
            if (nargs != s->nparams) {
                error("número de parâmetros inválido para processo '" + name + "'");
            }
            g3(Op::SPAWN, s->offset, 100);
            vm.code.push_back(nargs);
            g1(Op::POP);
        } else {
            g3(Op::SPAWN, 0, 100);
            int h = (int)vm.code.size() - 2;
            vm.code.push_back(nargs);
            g1(Op::POP);
            forward_holes[name].push_back(h);
        }
        return;
    }

    if (!s || (s->kind != SymKind::GLOBAL && s->kind != SymKind::LOCAL))
        error("variável não declarada: " + name);

    bool is_global = (s->kind == SymKind::GLOBAL);
    int  base      = s->offset;

    // --- array: name[idx] ---
    if (check(Token::LBRACKET)) {
        lexico();
        expr();                                     // compila índice → stack
        expect(Token::RBRACKET, "']'");

        Token op = pieza;
        if (op == Token::ASSIGN) {
            // a[i] = expr
            lexico();
            expr();
            match(Token::SEMICOLON);
            // stack: [..., idx, val]
            if (is_global) g2(Op::STORE_GLOBAL_IDX, base);
            else           g2(Op::STORE_LOCAL_IDX,  base);

        } else if (op == Token::ADD_ASSIGN || op == Token::SUB_ASSIGN ||
                   op == Token::MUL_ASSIGN || op == Token::DIV_ASSIGN ||
                   op == Token::MOD_ASSIGN) {
            // a[i] += expr  →  DUP, LOAD_IDX, expr, op, STORE_IDX
            lexico();
            g1(Op::DUP);    // duplica idx: [..., idx, idx]
            if (is_global) g2(Op::LOAD_GLOBAL_IDX, base);
            else           g2(Op::LOAD_LOCAL_IDX,  base);  // [..., idx, a[i]]
            expr();
            match(Token::SEMICOLON);
            switch (op) {
                case Token::ADD_ASSIGN: g1(Op::ADD); break;
                case Token::SUB_ASSIGN: g1(Op::SUB); break;
                case Token::MUL_ASSIGN: g1(Op::MUL); break;
                case Token::DIV_ASSIGN: g1(Op::DIV); break;
                case Token::MOD_ASSIGN: g1(Op::MOD); break;
                default: break;
            }
            // stack: [..., idx, result]
            if (is_global) g2(Op::STORE_GLOBAL_IDX, base);
            else           g2(Op::STORE_LOCAL_IDX,  base);
        } else {
            error("operador de atribuição esperado depois de 'name[i]'");
        }
        return;
    }

    // --- escalar: name ---
    Token op = pieza;

    if (op == Token::ASSIGN) {
        // x = expr
        lexico(); expr();
        match(Token::SEMICOLON);
        if (is_global) g2(Op::STORE_GLOBAL, base);
        else           g2(Op::STORE_LOCAL,  base);

    } else if (op == Token::ADD_ASSIGN || op == Token::SUB_ASSIGN ||
               op == Token::MUL_ASSIGN || op == Token::DIV_ASSIGN ||
               op == Token::MOD_ASSIGN) {
        // x += expr  →  LOAD, expr, op, STORE
        lexico();
        if (is_global) g2(Op::LOAD_GLOBAL, base);
        else           g2(Op::LOAD_LOCAL,  base);
        expr();
        match(Token::SEMICOLON);
        switch (op) {
            case Token::ADD_ASSIGN: g1(Op::ADD); break;
            case Token::SUB_ASSIGN: g1(Op::SUB); break;
            case Token::MUL_ASSIGN: g1(Op::MUL); break;
            case Token::DIV_ASSIGN: g1(Op::DIV); break;
            case Token::MOD_ASSIGN: g1(Op::MOD); break;
            default: break;
        }
        if (is_global) g2(Op::STORE_GLOBAL, base);
        else           g2(Op::STORE_LOCAL,  base);

    } else if (op == Token::INC || op == Token::DEC) {
        // x++  →  LOAD, PUSH 1, ADD, STORE  (= linc/ldec do DIV)
        lexico();
        match(Token::SEMICOLON);
        if (is_global) g2(Op::LOAD_GLOBAL, base);
        else           g2(Op::LOAD_LOCAL,  base);
        g2(Op::PUSH_CONST, 1);
        g1(op == Token::INC ? Op::ADD : Op::SUB);
        if (is_global) g2(Op::STORE_GLOBAL, base);
        else           g2(Op::STORE_LOCAL,  base);

    } else {
        error("operador de atribuição esperado: " + name);
    }
}

// ============================================================================
// condicion — expressão com comparações e lógica AND/OR
// Equivale à condicion() + condicion_and() do DIV
// Deixa 1 ou 0 no topo da stack
// ============================================================================
void Compiler::condicion()
{
    // Nível OR (menor prioridade) — = | ou || no DIV
    expr();
    Token op = pieza;
    if (op==Token::EQ_OP || op==Token::NEQ || op==Token::GT ||
        op==Token::LT    || op==Token::GTE || op==Token::LTE) {
        lexico(); expr();
        switch (op) {
            case Token::EQ_OP: g1(Op::EQ);  break;
            case Token::NEQ:   g1(Op::NEQ); break;
            case Token::GT:    g1(Op::GT);  break;
            case Token::LT:    g1(Op::LT);  break;
            case Token::GTE:   g1(Op::GTE); break;
            case Token::LTE:   g1(Op::LTE); break;
            default: break;
        }
    }
    // AND/OR em cadeias: (a > 0) && (b > 0)  ou  a > 0 and b > 0
    while (check(Token::AND_OP) || check(Token::AND_KW) ||
           check(Token::OR_OP)  || check(Token::OR_KW)  ||
           check(Token::BAND_OP)|| check(Token::BOR_OP) || check(Token::BXOR_OP)) {
        Token lop = pieza; lexico();
        condicion_atom();
        switch (lop) {
            case Token::AND_OP: case Token::AND_KW:  g1(Op::AND);  break;
            case Token::OR_OP:  case Token::OR_KW:   g1(Op::OR);   break;
            case Token::BAND_OP:                     g1(Op::BAND); break;
            case Token::BOR_OP:                      g1(Op::BOR);  break;
            case Token::BXOR_OP:                     g1(Op::BXOR); break;
            default: break;
        }
    }
}

// Un átomo de condição (expr + operador relacional opcional)
void Compiler::condicion_atom()
{
    expr();
    Token op = pieza;
    if (op==Token::EQ_OP || op==Token::NEQ || op==Token::GT ||
        op==Token::LT    || op==Token::GTE || op==Token::LTE) {
        lexico(); expr();
        switch (op) {
            case Token::EQ_OP: g1(Op::EQ);  break;
            case Token::NEQ:   g1(Op::NEQ); break;
            case Token::GT:    g1(Op::GT);  break;
            case Token::LT:    g1(Op::LT);  break;
            case Token::GTE:   g1(Op::GTE); break;
            case Token::LTE:   g1(Op::LTE); break;
            default: break;
        }
    }
}

// ============================================================================
// expr — adição, subtracção, bitwise OR
// ============================================================================
void Compiler::expr()
{
    term();
    while (check(Token::PLUS) || check(Token::MINUS) || check(Token::BOR_OP)) {
        Token op = pieza; lexico();
        term();
        switch (op) {
            case Token::PLUS:   g1(Op::ADD); break;
            case Token::MINUS:  g1(Op::SUB); break;
            case Token::BOR_OP: g1(Op::BOR); break;
            default: break;
        }
    }
}

// ============================================================================
// term — multiplicação, divisão, módulo, bitwise
// ============================================================================
void Compiler::term()
{
    factor();
    while (check(Token::STAR)   || check(Token::SLASH) || check(Token::MOD_OP) ||
           check(Token::BAND_OP)|| check(Token::BXOR_OP)) {
        Token op = pieza; lexico();
        factor();
        switch (op) {
            case Token::STAR:    g1(Op::MUL);  break;
            case Token::SLASH:   g1(Op::DIV);  break;
            case Token::MOD_OP:  g1(Op::MOD);  break;
            case Token::BAND_OP: g1(Op::BAND); break;
            case Token::BXOR_OP: g1(Op::BXOR); break;
            default: break;
        }
    }
}

// ============================================================================
// ============================================================================
// factor — literal, variável, array[idx], (expr), NOT, ~, -
// ============================================================================
void Compiler::factor()
{
    if (check(Token::NUMBER) || check(Token::FLOAT_NUMBER) || check(Token::STRING_LITERAL)) {
        emit_scalar_literal();
        return;
    }
    if (check(Token::MINUS)) {
        lexico(); factor(); g1(Op::NEG);
        return;
    }
    // ! expr  (= lnot do DIV) e  ~ expr  (= bnot)
    if (check(Token::NOT_OP) || check(Token::NOT_KW)) {
        lexico(); factor(); g1(Op::NOT);
        return;
    }
    if (check(Token::BXOR_OP)) {  // ^ usado como ~ (bitwise NOT unário)
        lexico(); factor(); g1(Op::BNOT);
        return;
    }
    if (check(Token::LPAREN)) {
        lexico(); condicion();
        expect(Token::RPAREN, "')'");
        return;
    }
    if (check(Token::IDENT)) {
        std::string name = id_val; lexico();

        // chamada de função ou builtin com retorno: soma(a,b), abs(x), etc.
        if (check(Token::LPAREN)) {
            lexico();
            Symbol* s = find_sym(name);
            if (!s) error("função não declarada: " + name);

            // colecta argumentos separados por vírgula
            int nargs = 0;
            if (!check(Token::RPAREN)) {
                do { expr(); nargs++; } while (match(Token::COMMA));
            }
            expect(Token::RPAREN, "')'");

            if (s->kind == SymKind::BUILTIN) {
                g2(Op::CALL_BUILTIN, s->offset);
            } else if (s->kind == SymKind::FUNCTION) {
                g3(Op::FUNC_CALL, s->offset, nargs);
            } else {
                error("'" + name + "' não é uma função");
            }
            return;
        }

        Symbol* s = find_sym(name);
        if (!s) error("variável não declarada: " + name);

        // array[idx]  — acesso com índice runtime (= lptr do DIV)
        if (check(Token::LBRACKET)) {
            lexico();
            expr();                             // compila índice
            expect(Token::RBRACKET, "']'");
            if (s->kind == SymKind::GLOBAL) g2(Op::LOAD_GLOBAL_IDX, s->offset);
            else                            g2(Op::LOAD_LOCAL_IDX,  s->offset);
            return;
        }

        // escalar
        if (s->kind == SymKind::GLOBAL) g2(Op::LOAD_GLOBAL, s->offset);
        else                            g2(Op::LOAD_LOCAL,  s->offset);

        // acesso remoto: expr.field  (= laid + lptr do DIV)
        // O valor carregado é tratado como id de processo;
        // o campo após '.' é resolvido como offset de local var.
        if (check(Token::DOT)) {
            lexico();
            if (!check(Token::IDENT)) error("esperava nome de campo após '.'" );
            std::string field = id_val; lexico();
            Symbol* fs = find_sym(field);
            if (!fs || fs->kind != SymKind::LOCAL)
                error("campo remoto não encontrado: " + field);
            g2(Op::LOAD_ID_LOCAL, fs->offset);
        }
        return;
    }
    error("factor inválido");
}
