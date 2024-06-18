#include <fixedptc.h>
#include <stdio.h>

int main() {
    fixedpt A = fixedpt_rconst(-0.5), B = fixedpt_rconst(0.5), C = fixedpt_rconst(1.5), D = fixedpt_rconst(-1.5), E = fixedpt_rconst(10.0), F = fixedpt_rconst(-9.0);
    printf("floor(-0.5) = %d, ceil(-0.5)=%d\n", fixedpt_toint(fixedpt_floor(A)),  fixedpt_toint(fixedpt_ceil(A)));
    printf("floor(0.5) = %d, ceil(0.5)=%d\n", fixedpt_toint(fixedpt_floor(B)),  fixedpt_toint(fixedpt_ceil(B)));
    printf("floor(-1.5) = %d, ceil(-1.5)=%d\n", fixedpt_toint(fixedpt_floor(D)),  fixedpt_toint(fixedpt_ceil(D)));
    printf("floor(1.5) = %d, ceil(1.5)=%d\n", fixedpt_toint(fixedpt_floor(C)),  fixedpt_toint(fixedpt_ceil(C)));
    printf("floor(10.0) = %d, ceil(10.0)=%d\n", fixedpt_toint(fixedpt_floor(E)),  fixedpt_toint(fixedpt_ceil(E)));
    printf("floor(-9.0) = %d, ceil(-9.0)=%d\n", fixedpt_toint(fixedpt_floor(F)),  fixedpt_toint(fixedpt_ceil(F)));

}
