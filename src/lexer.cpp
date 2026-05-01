#include "lexer.h"

namespace zen {

void Lexer::init(const char* source) {
    source_     = source;
    start_      = source;
    current_    = source;
    line_       = 1;
    has_peeked_ = false;
    in_interp_    = false;
    interp_depth_ = 0;
    pending_error_ = nullptr;
}

/* =========================================================
** Character helpers
** ========================================================= */

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

/* UTF-8 continuation byte: 10xxxxxx (0x80..0xBF) */
static bool is_utf8_cont(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

/* UTF-8 lead byte: 110xxxxx+ (0xC2..0xF4 for valid Unicode) */
static bool is_utf8_lead(char c) { return static_cast<unsigned char>(c) >= 0xC0; }

/* Alpha: ASCII letters, underscore, or start of a multi-byte UTF-8 sequence.
   This allows identifiers like: posição, nombre, 挨拶, 🚀 */
static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || is_utf8_lead(c);
}

/* Alphanumeric: alpha, digit, or UTF-8 continuation byte (part of multi-byte char) */
static bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c) || is_utf8_cont(c);
}

bool Lexer::is_at_end() { return *current_ == '\0'; }

char Lexer::advance() {
    return *current_++;
}

char Lexer::peek() {
    return *current_;
}

char Lexer::peek_next() {
    if (is_at_end()) return '\0';
    return current_[1];
}

bool Lexer::match(char expected) {
    if (is_at_end()) return false;
    if (*current_ != expected) return false;
    current_++;
    return true;
}

/* =========================================================
** Skip whitespace + comments
** ========================================================= */

void Lexer::skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ': case '\r': case '\t':
                advance();
                break;
            case '\n':
                line_++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    /* Line comment: skip to end of line */
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    /* Block comment: skip until */ 
                    advance(); advance(); /* consume /* */
                    int depth = 1;
                    while (depth > 0 && !is_at_end()) {
                        if (peek() == '/' && peek_next() == '*') {
                            advance(); advance(); depth++;
                        } else if (peek() == '*' && peek_next() == '/') {
                            advance(); advance(); depth--;
                        } else {
                            if (peek() == '\n') line_++;
                            advance();
                        }
                    }
                    if (depth > 0)
                        pending_error_ = "Unterminated block comment.";
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/* =========================================================
** Token constructors
** ========================================================= */

Token Lexer::make_token(TokenType type) {
    Token t;
    t.type   = type;
    t.start  = start_;
    t.length = (int)(current_ - start_);
    t.line   = line_;
    return t;
}

Token Lexer::error_token(const char* msg) {
    Token t;
    t.type   = TOK_ERROR;
    t.start  = msg;
    t.length = (int)strlen(msg);
    t.line   = line_;
    return t;
}

/* =========================================================
** String literal
** Single-quoted: 'text' — no interpolation.
** Double-quoted: "text" — supports {expr} interpolation.
**
** Interpolation tokens:
**   "hello {name}, you are {age}!"
** becomes:
**   INTERP_START("hello ")  <name tokens>
**   INTERP_MID(", you are ") <age tokens>
**   INTERP_END("!")
**
** If no {} found in double-quoted, emits plain TOK_STRING.
** ========================================================= */

Token Lexer::string_token() {
    char quote = start_[0]; /* ' or " */

    /* Single-quoted: no interpolation */
    if (quote == '\'') {
        while (peek() != '\'' && !is_at_end()) {
            if (peek() == '\n') line_++;
            if (peek() == '\\') advance(); /* skip escape */
            advance();
        }
        if (is_at_end()) return error_token("Unterminated string.");
        advance(); /* closing ' */
        return make_token(TOK_STRING);
    }

    /* Double-quoted: scan for { interpolation */
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line_++;
        if (peek() == '\\') {
            advance(); /* skip escape char */
            advance(); /* skip escaped char */
            continue;
        }
        if (peek() == '{') {
            /* Found interpolation start */
            /* Token covers: from start_ to current_ (the text before {) */
            Token t;
            t.type   = TOK_INTERP_START;
            t.start  = start_;
            t.length = (int)(current_ - start_);
            t.line   = line_;
            advance(); /* consume '{' */
            in_interp_ = true;
            interp_depth_ = 1;
            return t;
        }
        advance();
    }

    if (is_at_end()) return error_token("Unterminated string.");
    advance(); /* closing " */
    return make_token(TOK_STRING);
}

/* =========================================================
** Continue scanning an interpolated string after '}'.
** Called when we encounter '}' that closes an interpolation expr.
** Scans until next '{' (emit INTERP_MID) or '"' (emit INTERP_END).
** ========================================================= */

Token Lexer::interp_string_continue() {
    start_ = current_; /* start right after the '}' */

    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line_++;
        if (peek() == '\\') {
            advance();
            advance();
            continue;
        }
        if (peek() == '{') {
            /* Another interpolation segment */
            Token t;
            t.type   = TOK_INTERP_MID;
            t.start  = start_;
            t.length = (int)(current_ - start_);
            t.line   = line_;
            advance(); /* consume '{' */
            in_interp_ = true;
            interp_depth_ = 1;
            return t;
        }
        advance();
    }

    if (is_at_end()) return error_token("Unterminated interpolated string.");

    /* Reached closing " */
    Token t;
    t.type   = TOK_INTERP_END;
    t.start  = start_;
    t.length = (int)(current_ - start_);
    t.line   = line_;
    advance(); /* consume '"' */
    in_interp_ = false;
    return t;
}

/* =========================================================
** Verbatim string literal: @"..."
** No escape processing. Multi-line allowed.
** Use "" for a literal quote inside the string.
** ========================================================= */

Token Lexer::verbatim_string_token() {
    /* current_ is right after @" — start_ points to @ */
    while (!is_at_end()) {
        if (peek() == '"') {
            if (peek_next() == '"') {
                /* Doubled quote — literal " — skip both */
                advance();
                advance();
            } else {
                /* End of verbatim string */
                advance(); /* consume closing " */
                return make_token(TOK_VERBATIM_STRING);
            }
        } else {
            if (peek() == '\n') line_++;
            advance();
        }
    }
    return error_token("Unterminated verbatim string.");
}

/* =========================================================
** Number literal
** ========================================================= */

Token Lexer::number_token() {
    /* Hex */
    if (start_[0] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance(); /* x */
        if (!(is_digit(peek()) || (peek() >= 'a' && peek() <= 'f') || (peek() >= 'A' && peek() <= 'F')))
            return error_token("Invalid hex literal: expected hex digits after '0x'.");
        while (is_digit(peek()) || (peek() >= 'a' && peek() <= 'f') || (peek() >= 'A' && peek() <= 'F'))
            advance();
        return make_token(TOK_INT);
    }

    while (is_digit(peek())) advance();

    /* Float? */
    if (peek() == '.' && is_digit(peek_next())) {
        advance(); /* consume '.' */
        while (is_digit(peek())) advance();
        /* Exponent? */
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (is_digit(peek())) advance();
        }
        return make_token(TOK_FLOAT);
    }

    /* Exponent on integer makes it float */
    if (peek() == 'e' || peek() == 'E') {
        advance();
        if (peek() == '+' || peek() == '-') advance();
        while (is_digit(peek())) advance();
        return make_token(TOK_FLOAT);
    }

    return make_token(TOK_INT);
}

/* =========================================================
** Identifier + keyword matching
** ========================================================= */

TokenType Lexer::check_keyword(int start, int length, const char* rest, TokenType type) {
    if (current_ - start_ == start + length &&
        memcmp(start_ + start, rest, length) == 0) {
        return type;
    }
    return TOK_IDENTIFIER;
}

TokenType Lexer::identifier_type() {
    /* Trie-style keyword check (like Crafting Interpreters) */
    switch (start_[0]) {
        case 'a':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'b': return check_keyword(2, 1, "s", TOK_ABS);
                    case 'c': return check_keyword(2, 2, "os", TOK_ACOS);
                    case 'n': return check_keyword(2, 1, "d", TOK_AND);
                    case 's': return check_keyword(2, 2, "in", TOK_ASIN);
                    case 't':
                        if (current_ - start_ > 4 && start_[4] == '2')
                            return check_keyword(2, 3, "an2", TOK_ATAN2);
                        return check_keyword(2, 2, "an", TOK_ATAN);
                }
            }
            break;
        case 'b': return check_keyword(1, 4, "reak", TOK_BREAK);
        case 'c':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'a': return check_keyword(2, 2, "se", TOK_CASE);
                    case 'e': return check_keyword(2, 2, "il", TOK_CEIL);
                    case 'l':
                        if (current_ - start_ > 2 && start_[2] == 'o')
                            return check_keyword(2, 3, "ock", TOK_CLOCK);
                        return check_keyword(2, 3, "ass", TOK_CLASS);
                    case 'o':
                        if (current_ - start_ == 3 && start_[2] == 's')
                            return TOK_COS;
                        return check_keyword(2, 6, "ntinue", TOK_CONTINUE);
                }
            }
            break;
        case 'd':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'e':
                        if (current_ - start_ == 3 && start_[2] == 'f') return TOK_DEF;
                        if (current_ - start_ == 3 && start_[2] == 'g') return TOK_DEG;
                        if (current_ - start_ > 3 && start_[2] == 'f')
                            return check_keyword(3, 4, "ault", TOK_DEFAULT);
                        break;
                    case 'o': if (current_ - start_ == 2) return TOK_DO;
                              break;
                }
            }
            break;
        case 'e':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'l':
                        if (current_ - start_ == 4 && start_[2] == 'i' && start_[3] == 'f')
                            return TOK_ELIF;
                        return check_keyword(2, 2, "se", TOK_ELSE);
                    case 'x': return check_keyword(2, 1, "p", TOK_EXP);
                }
            }
            break;
        case 'f':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOK_FALSE);
                    case 'l': return check_keyword(2, 3, "oor", TOK_FLOOR);
                    case 'o':
                        if (current_ - start_ > 3 && start_[2] == 'r' && start_[3] == 'e')
                            return check_keyword(4, 3, "ach", TOK_FOREACH);
                        return check_keyword(2, 1, "r", TOK_FOR);
                    case 'r': return check_keyword(2, 3, "ame", TOK_FRAME);
                }
            }
            break;
        case 'i':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'f': if (current_ - start_ == 2) return TOK_IF; break;
                    case 'n': if (current_ - start_ == 2) return TOK_IN;
                              return check_keyword(2, 4, "port", TOK_IMPORT);
                    break;
                }
            }
            break;
        case 'l':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'e': return check_keyword(2, 1, "n", TOK_LEN);
                    case 'o':
                        if (current_ - start_ == 3 && start_[2] == 'g') return TOK_LOG;
                        return check_keyword(2, 2, "op", TOK_LOOP);
                }
            }
            break;
        case 'n':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'i': return check_keyword(2, 1, "l", TOK_NIL);
                    case 'o': return check_keyword(2, 1, "t", TOK_NOT);
                }
            }
            break;
        case 'o': return check_keyword(1, 1, "r", TOK_OR);
        case 'p':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'o': return check_keyword(2, 1, "w", TOK_POW);
                    case 'r': return check_keyword(2, 3, "int", TOK_PRINT);
                }
            }
            break;
        case 'r':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'a': return check_keyword(2, 1, "d", TOK_RAD);
                    case 'e':
                        if (current_ - start_ > 2) {
                            switch (start_[2]) {
                                case 't': return check_keyword(3, 3, "urn", TOK_RETURN);
                                case 's': return check_keyword(3, 3, "ume", TOK_RESUME);
                            }
                        }
                        break;
                }
            }
            break;
        case 's':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'e': return check_keyword(2, 2, "lf", TOK_SELF);
                    case 'i': return check_keyword(2, 1, "n", TOK_SIN);
                    case 'p': return check_keyword(2, 3, "awn", TOK_SPAWN);
                    case 'q': return check_keyword(2, 2, "rt", TOK_SQRT);
                    case 't': return check_keyword(2, 4, "ruct", TOK_STRUCT);
                    case 'w': return check_keyword(2, 4, "itch", TOK_SWITCH);
                }
            }
            break;
        case 't':
            if (current_ - start_ > 1) {
                switch (start_[1]) {
                    case 'a': return check_keyword(2, 1, "n", TOK_TAN);
                    case 'r': return check_keyword(2, 2, "ue", TOK_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(1, 2, "ar", TOK_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOK_WHILE);
        case 'x': return check_keyword(1, 2, "or", TOK_XOR);
        case 'y': return check_keyword(1, 4, "ield", TOK_YIELD);
        case '_':
            if (current_ - start_ == 1) return TOK_UNDERSCORE;
            break;
    }
    return TOK_IDENTIFIER;
}

Token Lexer::identifier_token() {
    while (is_alnum(peek())) advance();
    return make_token(identifier_type());
}

/* =========================================================
** Main tokenizer
** ========================================================= */

Token Lexer::next_token() {
    /* Return peeked token if available */
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }

    skip_whitespace();
    start_ = current_;

    if (pending_error_) {
        const char *err = pending_error_;
        pending_error_ = nullptr;
        return error_token(err);
    }

    if (is_at_end()) return make_token(TOK_EOF);

    char c = advance();

    /* If we're inside an interpolation and hit '}' that closes it,
       continue scanning the rest of the string. */
    if (in_interp_) {
        if (c == '{') {
            interp_depth_++;
            return make_token(TOK_LBRACE);
        }
        if (c == '}') {
            interp_depth_--;
            if (interp_depth_ == 0) {
                /* End of interpolated expression — continue string */
                in_interp_ = false;
                return interp_string_continue();
            }
            return make_token(TOK_RBRACE);
        }
    }

    /* Identifiers */
    if (is_alpha(c)) return identifier_token();

    /* Numbers */
    if (is_digit(c)) return number_token();

    switch (c) {
        case '(': return make_token(TOK_LPAREN);
        case ')': return make_token(TOK_RPAREN);
        case '{': return make_token(TOK_LBRACE);
        case '}': return make_token(TOK_RBRACE);
        case '[': return make_token(TOK_LBRACKET);
        case ']': return make_token(TOK_RBRACKET);
        case ',': return make_token(TOK_COMMA);
        case '.': return match('.') ? make_token(TOK_DOT_DOT) : make_token(TOK_DOT);
        case ';': return make_token(TOK_SEMICOLON);
        case ':': return make_token(TOK_COLON);
        case '~': return make_token(TOK_TILDE);
        case '^': return make_token(TOK_CARET);
        case '%': return make_token(TOK_PERCENT);

        case '+': return match('=') ? make_token(TOK_PLUS_EQ) : make_token(TOK_PLUS);
        case '-': return match('=') ? make_token(TOK_MINUS_EQ) : make_token(TOK_MINUS);
        case '*': return match('=') ? make_token(TOK_STAR_EQ) : make_token(TOK_STAR);
        case '/': return match('=') ? make_token(TOK_SLASH_EQ) : make_token(TOK_SLASH);

        case '!': return match('=') ? make_token(TOK_BANG_EQ) : make_token(TOK_BANG);
        case '=': return match('=') ? make_token(TOK_EQ_EQ) : make_token(TOK_EQ);

        case '<':
            if (match('<')) return make_token(TOK_LT_LT);
            if (match('=')) return make_token(TOK_LT_EQ);
            return make_token(TOK_LT);
        case '>':
            if (match('>')) return make_token(TOK_GT_GT);
            if (match('=')) return make_token(TOK_GT_EQ);
            return make_token(TOK_GT);

        case '&': return match('&') ? make_token(TOK_AMP_AMP) : make_token(TOK_AMP);
        case '|': return match('|') ? make_token(TOK_PIPE_PIPE) : make_token(TOK_PIPE);

        case '"': case '\'': return string_token();

        case '#':
            if (match('{')) return make_token(TOK_SET_LBRACE);
            return error_token("Unexpected '#' (use #{...} for set literals).");

        case '@':
            if (match('"')) return verbatim_string_token();
            return error_token("Unexpected '@' (use @\"...\" for verbatim strings).");
    }

    return error_token("Unexpected character.");
}

Token Lexer::peek_token() {
    if (!has_peeked_) {
        peeked_ = next_token();
        has_peeked_ = true;
    }
    return peeked_;
}

} /* namespace zen */
