/* Inline asm operands whose expression is an array.

   tccgen.c deliberately leaves VT_LVAL clear on an array-typed field
   reached with '.'/'->' ("an array is never an lvalue"): the register
   already holds the decayed address of the field.  parse_asm_operands()
   has to know about that, because asm_instr() calls save_regs(0) right
   after parsing the operands, and save_reg_upstack() uses VT_LVAL to
   decide what a spilled entry means.

   Both directions have to be right:
     - a memory constraint on such an expression must address the object
       the pointer points at, not the spill slot the pointer landed in;
     - a register constraint on an array must still pass the array's
       address, not the first bytes of the array.  */

extern int printf(const char *, ...);

struct s {
    unsigned int a[4];
    unsigned int tail;
};

/* "=m" on an array-typed struct member reached through a pointer. */
static void out_m(struct s *p)
{
    __asm__ __volatile__("movl $0x1234, %0" : "=m"(p->a) : : "memory");
}

/* "m" on an array-typed struct member reached through a pointer. */
static unsigned int in_m(struct s *p)
{
    unsigned int r;
    __asm__ __volatile__("movl %1, %0" : "=r"(r) : "m"(p->a));
    return r;
}

/* "r" on a local array: the operand is the decayed address. */
static int r_local(void)
{
    unsigned long a[2];
    unsigned long got;
    a[0] = 0xdeadbeefUL;
    a[1] = 0;
    __asm__ __volatile__("mov %1, %0" : "=r"(got) : "r"(a));
    return got == (unsigned long)a;
}

/* "r" on an array-typed struct member reached through a pointer. */
static int r_member(struct s *p)
{
    unsigned long got;
    p->a[0] = 0xdeadbeefU;
    __asm__ __volatile__("mov %1, %0" : "=r"(got) : "r"(p->a));
    return got == (unsigned long)p->a;
}

int main(void)
{
    struct s s;

    s.a[0] = 0;
    s.tail = 0x9999;
    out_m(&s);
    printf("out_m: a[0]=%x tail=%x\n", s.a[0], s.tail);

    s.a[0] = 0x5678;
    printf("in_m: %x\n", in_m(&s));

    printf("r_local: %s\n", r_local() ? "address" : "value");
    printf("r_member: %s\n", r_member(&s) ? "address" : "value");
    return 0;
}
