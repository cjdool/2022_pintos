#ifndef THREADSFIXED-POINT_H
#define THREADSFIXED-POINT_H

#define P 17
#define Q 14

int int_to_fixed_point (int n);
int fixed_point_to_int (int x);
int fixed_point_round_to_int (int x);
int fixed_point_add (int x, int y);
int fixed_point_sub (int x, int y);
int fixed_point_mul (int x, int y);
int fixed_point_div (int x, int y);
int fixed_point_addn (int x, int n);
int fixed_point_subn (int x, int n);
int fixed_point_muln (int x, int n);
int fixed_point_divn (int x, int n);

#endif /* threads/fixed-point.h */
