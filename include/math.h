/*
 * math.h - Math functions
 */
#ifndef ICARUS_USER_MATH_H
#define ICARUS_USER_MATH_H

#include "base.h"

#define sin(x)      ((double (*)(double))_icarus_api(API_SIN))(x)
#define cos(x)      ((double (*)(double))_icarus_api(API_COS))(x)
#define sqrt(x)     ((double (*)(double))_icarus_api(API_SQRT))(x)

#define PI 3.14159265358979323846

#endif
