#include "math.h"

/* Reduce angle to [-PI, PI] */
static double reduce_angle(double x) {
    while (x > K_PI) x -= 2.0 * K_PI;
    while (x < -K_PI) x += 2.0 * K_PI;
    return x;
}

/* sin(x) via Taylor series */
double ksin(double x) {
    x = reduce_angle(x);
    double term = x;
    double sum = x;
    for (int i = 1; i < 12; i++) {
        term *= -x * x / (2.0 * i * (2.0 * i + 1.0));
        sum += term;
    }
    return sum;
}

/* cos(x) via Taylor series */
double kcos(double x) {
    x = reduce_angle(x);
    double term = 1.0;
    double sum = 1.0;
    for (int i = 1; i < 12; i++) {
        term *= -x * x / ((2.0 * i - 1.0) * (2.0 * i));
        sum += term;
    }
    return sum;
}

/* sqrt(x) via Newton's method */
double ksqrt(double x) {
    if (x <= 0.0) return 0.0;
    double guess = x > 1.0 ? x / 2.0 : 1.0;
    for (int i = 0; i < 20; i++)
        guess = (guess + x / guess) * 0.5;
    return guess;
}
