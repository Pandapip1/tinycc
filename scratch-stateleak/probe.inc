int probe_g = 5;
int probe_arr[3] = {1,2,3};
extern int ext_f(int);
int probe_fn(int a, int b) {
    int i, s = 0;
    for (i = 0; i < b; i++) s += a * i;
    if (s > 10) s = ext_f(s);
    switch (a) { case 1: s++; break; case 2: s--; break; default: s ^= 3; }
    return s + probe_g + probe_arr[1];
}
double probe_fd(double x) { return x*2.0 + 1.0; }
