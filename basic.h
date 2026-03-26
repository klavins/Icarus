#ifndef BASIC_H
#define BASIC_H

#define MAX_TOKENS 32
#define MAX_TOKEN_LEN 64

enum token_type {
    TOK_NONE,
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENT,
    TOK_STRIDENT,  /* string variable like A$ */
    /* Keywords */
    TOK_PRINT,
    TOK_LET,
    TOK_DIM,
    TOK_IF,
    TOK_THEN,
    TOK_FOR,
    TOK_TO,
    TOK_STEP,
    TOK_NEXT,
    TOK_GOTO,
    TOK_GOSUB,
    TOK_RETURN,
    TOK_ON,
    TOK_READ,
    TOK_DATA,
    TOK_RESTORE,
    TOK_RUN,
    TOK_LIST,
    TOK_CLR,
    TOK_QUIT,
    TOK_SAVE,
    TOK_LOAD,
    TOK_DIR,
    TOK_DELETE,
    TOK_FORMAT,
    TOK_DOS,
    /* Operators */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_EQUAL,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    TOK_NE,
    TOK_COMMA,
    /* End of input */
    TOK_EOL,
    /* Special */
    TOK_COLOR,
    TOK_SOUND,
    TOK_GRAPHICS,
    TOK_PLOT,
    TOK_DRAWTO,
    TOK_INPUT,
    TOK_PAUSE
};

struct token {
    enum token_type type;
    int number_val;
    char string_val[MAX_TOKEN_LEN];
};

int  basic_tokenize(const char *input, struct token *tokens, int max);
void basic_exec(const char *line);
void basic_init(void);

/* Serialize/deserialize program for SAVE/LOAD */
int  basic_program_serialize(char *buf, int max_size);
int  basic_program_deserialize(const char *buf, int size);

#endif
