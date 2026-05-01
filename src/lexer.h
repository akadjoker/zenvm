#ifndef ZEN_LEXER_H
#define ZEN_LEXER_H

#include "common.h"

namespace zen
{

    enum TokenType : uint8_t
    {
        /* Single-char tokens */
        TOK_LPAREN,
        TOK_RPAREN, /* ( ) */
        TOK_LBRACE,
        TOK_RBRACE, /* { } */
        TOK_LBRACKET,
        TOK_RBRACKET, /* [ ] */
        TOK_COMMA,
        TOK_DOT,
        TOK_SEMICOLON,
        TOK_COLON,
        TOK_PLUS,
        TOK_MINUS,
        TOK_STAR,
        TOK_SLASH,
        TOK_PERCENT,
        TOK_AMP,
        TOK_PIPE,
        TOK_CARET,
        TOK_TILDE,

        /* One or two character tokens */
        TOK_BANG,
        TOK_BANG_EQ, /* ! != */
        TOK_EQ,
        TOK_EQ_EQ, /* = == */
        TOK_LT,
        TOK_LT_EQ,
        TOK_LT_LT, /* < <= << */
        TOK_GT,
        TOK_GT_EQ,
        TOK_GT_GT, /* > >= >> */
        TOK_AMP_AMP,
        TOK_PIPE_PIPE, /* && || */
        TOK_PLUS_EQ,
        TOK_MINUS_EQ, /* += -= */
        TOK_STAR_EQ,
        TOK_SLASH_EQ, /* *= /= */
        TOK_DOT_DOT,  /* .. (range) */

        /* Literals */
        TOK_INT,
        TOK_FLOAT,
        TOK_STRING,
        TOK_VERBATIM_STRING,  /* @"..." — no escapes, "" for literal quote */
        TOK_INTERP_START, /* "text { — first segment of interpolated string */
        TOK_INTERP_MID,   /* } text { — middle segment */
        TOK_INTERP_END,   /* } text" — final segment */

        /* Identifiers + keywords */
        TOK_IDENTIFIER,
        TOK_VAR,
        TOK_DEF,
        TOK_CLASS,
        TOK_RETURN,
        TOK_IF,
        TOK_ELIF,
        TOK_ELSE,
        TOK_WHILE,
        TOK_FOR,
        TOK_FOREACH,
        TOK_IN,
        TOK_LOOP,
        TOK_DO,
        TOK_BREAK,
        TOK_CONTINUE,
        TOK_SWITCH,
        TOK_CASE,
        TOK_DEFAULT,
        TOK_TRUE,
        TOK_FALSE,
        TOK_NIL,
        TOK_AND,
        TOK_OR,
        TOK_NOT,
        TOK_FRAME,
        TOK_PRINT,
        TOK_IMPORT,
        TOK_STRUCT,
        TOK_YIELD,
        TOK_SPAWN,
        TOK_RESUME,
        TOK_SELF,

        /* Special */
        TOK_UNDERSCORE, /* _ (discard) */

        /* Set literal */
        TOK_SET_LBRACE, /* #{ */

        /* Keyword-builtins */
        TOK_LEN,
        TOK_SIN,
        TOK_COS,
        TOK_TAN,
        TOK_ASIN,
        TOK_ACOS,
        TOK_ATAN,
        TOK_ATAN2,
        TOK_SQRT,
        TOK_POW,
        TOK_LOG,
        TOK_ABS,
        TOK_FLOOR,
        TOK_CEIL,
        TOK_DEG,
        TOK_RAD,
        TOK_EXP,
        TOK_CLOCK,

        /* Special */
        TOK_ERROR,
        TOK_EOF,
    };

    struct Token
    {
        TokenType type;
        const char *start; /* pointer into source */
        int length;
        int line;
    };

    struct LexerState
    {
        const char *start;
        const char *current;
        int line;
        Token peeked;
        bool has_peeked;
        bool in_interp;
        int interp_depth;
    };

    class Lexer
    {
    public:
        void init(const char *source);

        Token next_token();
        Token peek_token(); /* lookahead without consuming */

        int current_line() const { return line_; }

        LexerState save_state() const
        {
            return {start_, current_, line_, peeked_, has_peeked_, in_interp_, interp_depth_};
        }
        void restore_state(const LexerState &s)
        {
            start_ = s.start;
            current_ = s.current;
            line_ = s.line;
            peeked_ = s.peeked;
            has_peeked_ = s.has_peeked;
            in_interp_ = s.in_interp;
            interp_depth_ = s.interp_depth;
        }

    private:
        void skip_whitespace();
        Token make_token(TokenType type);
        Token error_token(const char *msg);
        Token string_token();
        Token verbatim_string_token();
        Token interp_string_continue();
        Token number_token();
        Token identifier_token();
        TokenType identifier_type();
        TokenType check_keyword(int start, int length, const char *rest, TokenType type);

        char advance();
        char peek();
        char peek_next();
        bool match(char expected);
        bool is_at_end();

        const char *source_;
        const char *start_;   /* start of current token */
        const char *current_; /* current position */
        int line_;

        /* Lookahead buffer */
        Token peeked_;
        bool has_peeked_;

        /* String interpolation state */
        int interp_depth_; /* nesting depth of { } inside interpolation */
        bool in_interp_;   /* currently lexing inside {expr} */

        /* Pending error from skip_whitespace (e.g. unterminated block comment) */
        const char *pending_error_;
    };

} /* namespace zen */

#endif /* ZEN_LEXER_H */
