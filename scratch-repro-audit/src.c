#include <stdio.h>
static int helper(int x) { return x * 3 + 1; }
int global_arr[16];
const char *msg = "hello reproducible world";
int main(int argc, char **argv) {
    int s = 0;
    for (int i = 0; i < 16; i++) { global_arr[i] = helper(i); s += global_arr[i]; }
    printf("%s %d %d\n", msg, s, argc);
    return 0;
}
