/*
 * compare.c - Compare two integers
 * Usage: EXEC compare, 3, 7
 */

#include "os.h"
#include "string.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        os_print("Usage: EXEC compare, <a>, <b>\n");
        return 1;
    }

    int a = atoi(argv[1]);
    int b = atoi(argv[2]);

    char buf[80];
    if (a < b)
        snprintf(buf, sizeof(buf), "%d < %d\n", a, b);
    else if (a > b)
        snprintf(buf, sizeof(buf), "%d > %d\n", a, b);
    else
        snprintf(buf, sizeof(buf), "%d = %d\n", a, b);

    os_print(buf);
    return 0;
}
