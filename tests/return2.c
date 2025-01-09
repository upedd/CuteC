/* A function with internal linkage can be declared multiple times */
static int my_fun(void);

static int my_fun(void) {
    return 0;
}