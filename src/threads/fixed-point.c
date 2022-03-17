#include "threads/fixed-point.h"
#include <stdint.h>

static int fraction = 1<<Q; 

int int_to_fixed_point (int n)
{
    return n * fraction;
}

int fixed_point_to_int (int x)
{
    return x / fraction;
}

int fixed_point_round_to_int (int x)
{
    if (x >= 0)
    {
        return (x + fraction / 2) / fraction;
    }
    else
    {
        return (x - fraction / 2) / fraction;
    }
}

int fixed_point_add (int x, int y)
{
    return x + y;
}

int fixed_point_sub (int x, int y)
{
    return x - y;
}

int fixed_point_mul (int x, int y)
{
    return ((int64_t)x * y / fraction);
}

int fixed_point_div (int x, int y)
{
    return ((int64_t)x * fraction / y);
}

int fixed_point_addn (int x, int n)
{
    return fixed_point_add(x, int_to_fixed_point(n));
}

int fixed_point_subn (int x, int n)
{
    return fixed_point_sub(x, int_to_fixed_point(n));
}

int fixed_point_muln (int x, int n)
{
    return x * n;
}

int fixed_point_divn (int x, int n)
{
    return x / n;
}
