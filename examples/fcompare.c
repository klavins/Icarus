/*
 * fcompare.c - Compare two floating point numbers
 * Usage: EXEC fcompare, 3.14, 2.71
 */

#include "os.h"
#include "string.h"

int main(int argc, char **argv) {
    if (argc != 3) {
        os_print("Usage: EXEC fcompare, <a>, <b>\n");
        return 1;
    }

    double a = atof(argv[1]);
    double b = atof(argv[2]);

    char buf[80];
    if (a < b)
        snprintf(buf, sizeof(buf), "%g < %g\n", a, b);
    else if (a > b)
        snprintf(buf, sizeof(buf), "%g > %g\n", a, b);
    else
        snprintf(buf, sizeof(buf), "%g = %g\n", a, b);

    os_print(buf);
    return 0;
}
