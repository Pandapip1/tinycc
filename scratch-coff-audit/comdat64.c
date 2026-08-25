__attribute__((selectany)) int sel_var = 5;
extern int extfunc(void);
static int helper(void) { return sel_var; }
int cmain(void) { return extfunc() + helper(); }
