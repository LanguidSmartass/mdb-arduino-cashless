#ifndef BIT_REG_DEFS_H
#define BIT_REG_DEFS_H

#define SET_BIT(r, b)    (r |=  (1 << b))    // set bit in register
#define CLR_BIT(r, b)    (r &= ~(1 << b))    // clear bit in register
#define TGL_BIT(r, b)    (r ^=  (1 << b))    // toggle bit in register
#define CHK_BIT(r, b)    (r &   (1 << b))    // check if bit in register is set

#endif