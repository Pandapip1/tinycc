extern int extfunc(void);
int gdata = 42;
int commvar;
int start64(void) { return extfunc() + gdata; }
