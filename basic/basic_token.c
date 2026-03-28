#include "basic_internal.h"
#include "klib.h"

int is_alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

char to_upper(char c) {
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

static enum token_type keyword_type(const char *word) {
    if (strcmp(word, "REM") == 0)   return TOK_REM;
    if (strcmp(word, "POKE") == 0)  return TOK_POKE;
    if (strcmp(word, "DELAY") == 0)   return TOK_DELAY;
    if (strcmp(word, "SHOW") == 0)    return TOK_SHOW;
    if (strcmp(word, "PRINT") == 0) return TOK_PRINT;
    if (strcmp(word, "LET") == 0)   return TOK_LET;
    if (strcmp(word, "DIM") == 0)   return TOK_DIM;
    if (strcmp(word, "IF") == 0)    return TOK_IF;
    if (strcmp(word, "THEN") == 0)  return TOK_THEN;
    if (strcmp(word, "FOR") == 0)   return TOK_FOR;
    if (strcmp(word, "TO") == 0)    return TOK_TO;
    if (strcmp(word, "STEP") == 0)  return TOK_STEP;
    if (strcmp(word, "MOD") == 0)   return TOK_MOD;
    if (strcmp(word, "NEXT") == 0)  return TOK_NEXT;
    if (strcmp(word, "GOTO") == 0)   return TOK_GOTO;
    if (strcmp(word, "GOSUB") == 0)  return TOK_GOSUB;
    if (strcmp(word, "RETURN") == 0) return TOK_RETURN;
    if (strcmp(word, "ON") == 0)     return TOK_ON;
    if (strcmp(word, "READ") == 0)   return TOK_READ;
    if (strcmp(word, "DATA") == 0)   return TOK_DATA;
    if (strcmp(word, "RESTORE") == 0) return TOK_RESTORE;
    if (strcmp(word, "RUN") == 0)   return TOK_RUN;
    if (strcmp(word, "LIST") == 0)  return TOK_LIST;
    if (strcmp(word, "CLR") == 0)   return TOK_CLR;
    if (strcmp(word, "CLS") == 0)   return TOK_CLS;
    if (strcmp(word, "QUIT") == 0)   return TOK_QUIT;
    if (strcmp(word, "SAVE") == 0)   return TOK_SAVE;
    if (strcmp(word, "LOAD") == 0)   return TOK_LOAD;
    if (strcmp(word, "DIR") == 0)    return TOK_DIR;
    if (strcmp(word, "DELETE") == 0)  return TOK_DELETE;
    if (strcmp(word, "FORMAT") == 0)  return TOK_FORMAT;
    if (strcmp(word, "DOS") == 0)     return TOK_DOS;
    if (strcmp(word, "COLOR") == 0) return TOK_COLOR;
    if (strcmp(word, "SOUND") == 0)    return TOK_SOUND;
    if (strcmp(word, "GRAPHICS") == 0) return TOK_GRAPHICS;
    if (strcmp(word, "GR") == 0)       return TOK_GRAPHICS;
    if (strcmp(word, "PLOT") == 0)     return TOK_PLOT;
    if (strcmp(word, "DRAWTO") == 0)   return TOK_DRAWTO;
    if (strcmp(word, "FILLTO") == 0)   return TOK_FILLTO;
    if (strcmp(word, "INPUT") == 0)    return TOK_INPUT;
    if (strcmp(word, "PAUSE") == 0)    return TOK_PAUSE;
    if (strcmp(word, "POS") == 0)      return TOK_POS;
    if (strcmp(word, "TEXT") == 0)     return TOK_TEXT;
    return TOK_IDENT;
}

int basic_tokenize(const char *input, struct token *tokens, int max) {
    int count = 0;
    const char *p = input;

    while (*p && count < max - 1) {
        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '\0')
            break;

        struct token *t = &tokens[count];
        t->number_val = 0;
        t->string_val[0] = '\0';

        if (is_digit(*p)) {
            t->type = TOK_NUMBER;
            double val = 0;
            if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
                /* Hex literal: 0xFF */
                p += 2;
                int ival = 0;
                while (is_digit(*p) || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                    int d;
                    if (*p >= '0' && *p <= '9') d = *p - '0';
                    else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
                    else d = *p - 'A' + 10;
                    ival = ival * 16 + d;
                    p++;
                }
                val = ival;
            } else {
                while (is_digit(*p))
                    val = val * 10 + (*p++ - '0');
                if (*p == '.') {
                    p++;
                    double frac = 0.1;
                    while (is_digit(*p)) {
                        val += (*p++ - '0') * frac;
                        frac *= 0.1;
                    }
                }
            }
            t->number_val = val;
        } else if (is_alpha(*p)) {
            int i = 0;
            while (is_alnum(*p) && i < MAX_TOKEN_LEN - 1)
                t->string_val[i++] = to_upper(*p++);
            t->string_val[i] = '\0';
            if (*p == '$') {
                /* String variable: single letter + $ */
                t->type = TOK_STRIDENT;
                p++;
            } else {
                t->type = keyword_type(t->string_val);
                if (t->type == TOK_REM) {
                    count++;
                    goto done; /* skip rest of line */
                }
            }
        } else if (*p == '"') {
            p++;
            int i = 0;
            while (*p && *p != '"' && i < MAX_TOKEN_LEN - 1)
                t->string_val[i++] = *p++;
            t->string_val[i] = '\0';
            if (*p == '"')
                p++;
            t->type = TOK_STRING;
        } else if (*p == '<') {
            p++;
            if (*p == '=')      { t->type = TOK_LE; p++; }
            else if (*p == '>') { t->type = TOK_NE; p++; }
            else                { t->type = TOK_LT; }
        } else if (*p == '>') {
            p++;
            if (*p == '=')      { t->type = TOK_GE; p++; }
            else                { t->type = TOK_GT; }
        } else {
            switch (*p) {
            case '+': t->type = TOK_PLUS;   break;
            case '-': t->type = TOK_MINUS;  break;
            case '*': t->type = TOK_STAR;   break;
            case '/': t->type = TOK_SLASH;  break;
            case '=': t->type = TOK_EQUAL;  break;
            case '(': t->type = TOK_LPAREN; break;
            case ')': t->type = TOK_RPAREN; break;
            case ',': t->type = TOK_COMMA;  break;
            default:
                os_printf("?ILLEGAL CHARACTER: %c\n", *p);
                return -1;
            }
            p++;
        }
        count++;
    }

done:
    tokens[count].type = TOK_EOL;
    tokens[count].string_val[0] = '\0';
    tokens[count].number_val = 0;
    count++;

    return count;
}
