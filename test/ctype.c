/*
 * Tests for ctype functions (from klib.c).
 */

#include <stdio.h>
#include <assert.h>

/* Functions under test (from klib.c) */

static int icarus_isdigit(int c) { return c >= '0' && c <= '9'; }
static int icarus_isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int icarus_isalnum(int c) { return icarus_isalpha(c) || icarus_isdigit(c); }
static int icarus_isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
static int icarus_isupper(int c) { return c >= 'A' && c <= 'Z'; }
static int icarus_islower(int c) { return c >= 'a' && c <= 'z'; }
static int icarus_toupper(int c) { return icarus_islower(c) ? c - 32 : c; }
static int icarus_tolower(int c) { return icarus_isupper(c) ? c + 32 : c; }

int main(void) {
    /* ---- isdigit ---- */

    /* 1. Digits return true */
    for (char c = '0'; c <= '9'; c++)
        assert(icarus_isdigit(c));
    printf("  1. isdigit 0-9 = OK\n");

    /* 2. Letters return false */
    assert(!icarus_isdigit('A'));
    assert(!icarus_isdigit('z'));
    assert(!icarus_isdigit(' '));
    printf("  2. isdigit rejects non-digits = OK\n");

    /* 3. Boundary: characters adjacent to '0' and '9' */
    assert(!icarus_isdigit('0' - 1));
    assert(!icarus_isdigit('9' + 1));
    printf("  3. isdigit boundaries = OK\n");

    /* ---- isalpha ---- */

    /* 4. All uppercase */
    for (char c = 'A'; c <= 'Z'; c++)
        assert(icarus_isalpha(c));
    printf("  4. isalpha A-Z = OK\n");

    /* 5. All lowercase */
    for (char c = 'a'; c <= 'z'; c++)
        assert(icarus_isalpha(c));
    printf("  5. isalpha a-z = OK\n");

    /* 6. Non-alpha rejected */
    assert(!icarus_isalpha('0'));
    assert(!icarus_isalpha(' '));
    assert(!icarus_isalpha('@'));  /* just before 'A' */
    assert(!icarus_isalpha('['));  /* just after 'Z' */
    assert(!icarus_isalpha('`'));  /* just before 'a' */
    assert(!icarus_isalpha('{'));  /* just after 'z' */
    printf("  6. isalpha rejects non-alpha = OK\n");

    /* ---- isalnum ---- */

    /* 7. Digits and letters accepted */
    assert(icarus_isalnum('0'));
    assert(icarus_isalnum('9'));
    assert(icarus_isalnum('A'));
    assert(icarus_isalnum('z'));
    printf("  7. isalnum accepts digits+letters = OK\n");

    /* 8. Punctuation rejected */
    assert(!icarus_isalnum(' '));
    assert(!icarus_isalnum('!'));
    assert(!icarus_isalnum('\n'));
    printf("  8. isalnum rejects punctuation = OK\n");

    /* ---- isspace ---- */

    /* 9. All whitespace characters */
    assert(icarus_isspace(' '));
    assert(icarus_isspace('\t'));
    assert(icarus_isspace('\n'));
    assert(icarus_isspace('\r'));
    assert(icarus_isspace('\f'));
    assert(icarus_isspace('\v'));
    printf("  9. isspace all whitespace = OK\n");

    /* 10. Non-whitespace rejected */
    assert(!icarus_isspace('A'));
    assert(!icarus_isspace('0'));
    assert(!icarus_isspace('\0'));
    printf(" 10. isspace rejects non-space = OK\n");

    /* ---- isupper / islower ---- */

    /* 11. isupper */
    assert(icarus_isupper('A'));
    assert(icarus_isupper('Z'));
    assert(!icarus_isupper('a'));
    assert(!icarus_isupper('0'));
    printf(" 11. isupper = OK\n");

    /* 12. islower */
    assert(icarus_islower('a'));
    assert(icarus_islower('z'));
    assert(!icarus_islower('A'));
    assert(!icarus_islower('0'));
    printf(" 12. islower = OK\n");

    /* ---- toupper / tolower ---- */

    /* 13. toupper converts lowercase */
    assert(icarus_toupper('a') == 'A');
    assert(icarus_toupper('z') == 'Z');
    printf(" 13. toupper converts = OK\n");

    /* 14. toupper leaves non-lowercase unchanged */
    assert(icarus_toupper('A') == 'A');
    assert(icarus_toupper('5') == '5');
    assert(icarus_toupper(' ') == ' ');
    printf(" 14. toupper passthrough = OK\n");

    /* 15. tolower converts uppercase */
    assert(icarus_tolower('A') == 'a');
    assert(icarus_tolower('Z') == 'z');
    printf(" 15. tolower converts = OK\n");

    /* 16. tolower leaves non-uppercase unchanged */
    assert(icarus_tolower('a') == 'a');
    assert(icarus_tolower('5') == '5');
    assert(icarus_tolower(' ') == ' ');
    printf(" 16. tolower passthrough = OK\n");

    printf("\nAll tests passed.\n");
    return 0;
}
