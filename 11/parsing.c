#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval *err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_NUM(func_name, args, arg_count) \
    LASSERT(args, args->count != arg_count, \
        "Function '%s' passed incorrect number of arguments. Got %i, Expected %i", func_name, args->count, arg_count);

#define LASSERT_TYPE(func_name, args, index, type) \
    LASSERT(args, args->cell[index]->type == type, \
        "Function '%s' passed incorrect type for argument %i. Got %s, expected %s", func_name, index, ltype_name(args->cell[index]->type), ltype_name(type));

#define LASSERT_NOT_EMPTY(func_name, args, index) \
    LASSERT(args, args->cell[index]->count != 0, "Function '%s' passed {} for argument %i", func_name, index);

#ifdef _WIN32

#include <string.h>

static char buffer[2048]

/* Fake readline function */
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
}

/* Fake add_history function */
void add_history(char* unused) {}

#else

#include <editline/readline.h>
#include <editline/history.h>

#endif

// Forward declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// Function pointer definition for builtin functions
typedef lval*(*lbuiltin)(lenv*, lval*);

// All the possible lval types
enum Lval_types { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };
// Lisp value struct
struct lval {
    int type;
    long num;

    // Error and Symbol types have some string data
    char *err;
    char *sym;

    // A builtin function pointer
    lbuiltin fun;
    // Count and pointer to a list of "lval"
    int count;
    lval **cell;
};

// Environment structure
struct lenv {
    int run;    // Used to exit the program, set to 0 in builtin_exit
    int count;
    lval **vals;
    char **syms;
};

int number_of_nodes(mpc_ast_t *ast) {
    if(ast->children_num <= 0) return 1;
    else {
        int i = 0;
        int count = 1;
        for(; i < ast->children_num; i++) {
            count = count + number_of_nodes(ast->children[i]);
        }
        return count;
    }
}

char *ltype_name(int t);

lval *lval_eval_sexpr(lenv *e, lval *v);
lval *lval_eval(lenv *e, lval *v);
lval *lval_pop(lval *v, int i);
lval *lval_take(lval *v, int i);
lval *lval_copy(lval *v);

lval *builtin(lenv *e, lval *v, char *func);
lval *builtin_add(lenv* e, lval* a);
lval *builtin_sub(lenv* e, lval* a);
lval *builtin_mul(lenv* e, lval* a);
lval *builtin_div(lenv* e, lval* a);
lval *builtin_op(lenv *e, lval *v, char *op);
lval *builtin_head(lenv *e, lval *v);
lval *builtin_tail(lenv *e, lval *v);
lval *builtin_list(lenv *e, lval *v);
lval *builtin_eval(lenv *e, lval *v);
lval *builtin_join(lenv *e, lval *v);
lval *builtin_cons(lenv *e, lval *v);
lval *builtin_len(lenv *e, lval *v);
lval *builtin_init(lenv *e, lval *v);
lval *builtin_def(lenv *e, lval *v);
lval *builtin_exit(lenv *e, lval *v);
lval *builtin_printenv(lenv *e, lval *v);

lval *lval_join(lval *x, lval *y);

lval *lval_num(long x);
lval *lval_err(char *s, ...);
lval *lval_sym(char *s);
lval *lval_sexpr(void);
lval *lval_qexpr(void);
lval *lval_fun(lbuiltin func);

lenv *lenv_new(void);
void lenv_del(lenv *e);
lval *lenv_get(lenv *e, lval *k);
void lenv_put(lenv *e, lval *k, lval *v);
void lenv_add_builtin(lenv *e, char *name, lbuiltin func);
void lenv_add_builtins(lenv *e);

void lval_print(lenv *e, lval *v);
void lval_expr_print(lenv *e, lval *v, char open, char close);
void lval_println(lenv *e, lval *v);
lval *lval_add(lval *v, lval *x);
lval *lval_read_num(mpc_ast_t *t);
lval *lval_read(mpc_ast_t *t);
void lval_del(lval *v);

int main(int argc, char** argv) {

    // Create and define parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");

    mpca_lang(MPC_LANG_DEFAULT,
            "\
                number   : /-?[0-9]+/ ; \
                symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%]+/ ; \
                sexpr    : '(' <expr>* ')' ; \
                qexpr    : '{' <expr>* '}' ; \
                expr     : <number> | <symbol> | <sexpr> | <qexpr> ; \
                lispy    : /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.7");
    puts("Press Ctrl+c to Exit\n");

    // Create the environment
    lenv *e = lenv_new();
    lenv_add_builtins(e);

    while(e->run) {
        char* input = readline("lispy> ");
        add_history(input);

        // Try to parse the input
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)) {

            lval *val = lval_read(r.output);
            val = lval_eval(e, val);
            lval_println(e, val);
            lval_del(val);

            mpc_ast_delete(r.output);
        } else {
            // Otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    // Delete the environment
    lenv_del(e);

    // Undefine and Delete our Parsers
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}

char *ltype_name(int t) {
    switch(t) {
        case LVAL_ERR: return "Error";
        case LVAL_NUM: return "Number";
        case LVAL_FUN: return "Function";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

lval *lval_eval_sexpr(lenv *e, lval *v){
    // Empty expression
    if(v->count == 0) return v;

    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->count; i++) {
        if(v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    // Single expression (ignore "exit" & "printenv" functions, they should have no arguments)
    if(v->count == 1 && v->cell[0]->fun != builtin_exit && v->cell[0]->fun != builtin_printenv) {
        return lval_take(v, 0);
    }

    // Ensure first element is a function after evaluation
    lval *f = lval_pop(v, 0);
    if(f->type != LVAL_FUN) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with a function!");
    }

    // Call function
    lval *result = f->fun(e, v);
    lval_del(f);
    return result;
}

lval *lval_eval(lenv *e, lval *v) {
    if(v->type == LVAL_SYM) {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if(v->type == LVAL_SEXPR) return lval_eval_sexpr(e, v);
    return v;
}

lval *lval_pop(lval *v, int i) {
    // Check if there are enough lvals in the array
    if(i >= v->count) return lval_err("lval_pop index out of bounds!");
    // Copy the item at "i"
    lval *val = v->cell[i];

    // Move back all the pointers after the taken value
    for(; i < v->count - 1; i++) {
        v->cell[i] = v->cell[i+1];
    }

    // Decrement the counter
    v->count -= 1;
    // Reallocate memory for the cell array
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    // Return the popped value
    return val;
}

lval *lval_take(lval *v, int i){
    // Check if there are enough lvals in the array
    if(i >= v->count) {
        lval_del(v);
        return lval_err("lval_take index out of bounds!");
    }

    // Pop the item at "i"
    lval *val = lval_pop(v, i);
    // Delete the old lval completely
    lval_del(v);
    return val;
}

lval *lval_copy(lval *v){
    lval *x;

    switch(v->type) {
        case LVAL_FUN:
            x = lval_fun(v->fun);
            break;

        case LVAL_NUM:
            x = lval_num(v->num);
            break;

        case LVAL_ERR:
            x = lval_err(v->err);
            break;

        case LVAL_SYM:
            x = lval_sym(v->sym);
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x = malloc(sizeof(lval));
            x->type = v->type;
            x->count = v->count;
            for(int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            x->err = NULL;
            x->sym = NULL;
            x->num = 0;
            x->fun = NULL;
            break;

        default:
            x = lval_err("Unknown type found when copying lval!");
            break;
    }
    return x;
}
lval *builtin(lenv *e, lval *v, char *func){
    if (strcmp("list", func) == 0) return builtin_list(e, v);
    if (strcmp("head", func) == 0) return builtin_head(e, v);
    if (strcmp("tail", func) == 0) return builtin_tail(e, v);
    if (strcmp("join", func) == 0) return builtin_join(e, v);
    if (strcmp("eval", func) == 0) return builtin_eval(e, v);
    if (strcmp("cons", func) == 0) return builtin_cons(e, v);
    if (strcmp("len",  func) == 0) return builtin_len(e, v);
    if (strcmp("init", func) == 0) return builtin_init(e, v);
    if (strstr("+-/*%minmax", func)) return builtin_op(e, v, func);
    lval_del(v);
    return lval_err("Unknown function!");
}

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }
lval* builtin_rem(lenv* e, lval* a) { return builtin_op(e, a, "%"); }
lval* builtin_min(lenv* e, lval* a) { return builtin_op(e, a, "min"); }
lval* builtin_max(lenv* e, lval* a) { return builtin_op(e, a, "max"); }


lval *builtin_op(lenv *e, lval *v, char *op) {
    // Check that all inputted values are numbers
    for (int i = 0; i < v->count; i++) {
        if(v->cell[i]->type != LVAL_NUM) {
            return lval_err("Not a number!");
        }
    }

    lval *x = lval_pop(v, 0);

    if(strcmp(op, "-") == 0 && v->count == 0) {
        x->num = -x->num;
    }

    while(v->count) {
        // Pop the next value
        lval *y = lval_pop(v, 0);

        if (strcmp(op, "+") == 0) x->num += y->num;
        if (strcmp(op, "-") == 0) x->num -= y->num;
        if (strcmp(op, "*") == 0) x->num *= y->num;
        if (strcmp(op, "/") == 0) {
            // If the second operator is zero return an error
            if(y->num == 0) {
                lval_del(v);
                lval_del(x);
                lval_del(y);
                return lval_err("Divide by zero");
            }
            x->num /= y->num;
        }
        if (strcmp(op, "%") == 0) {
            // If the second operator is zero return an error
            if(y->num == 0) {
                lval_del(v);
                lval_del(x);
                lval_del(y);
                return lval_err("Divide by zero");
            }
            x->num %= y->num;
        }
        if (strcmp(op, "^") == 0) {
            // If the y value is 0 we set the x value to 1,
            // because x^0 is always 1
            if(y->num == 0) x->num = 1;
            else {
                long temp = x->num;
                for(int i = 1; i < y->num; i++) {
                    x->num *= temp;
                }
            }
        }
        if (strcmp(op, "min") == 0) {
            if(x->num > y->num) x->num = y->num;
        }
        if (strcmp(op, "max") == 0) {
            if(x->num < y->num) x->num = y->num;
        }
        // Delete the temporary lval
        lval_del(y);
    }

    // Delete the old lval and return the result
    lval_del(v);
    return x;
}

lval *builtin_head(lenv *e, lval *v){
    // Check for errors
    LASSERT_NUM("head", v, 1);
    LASSERT_TYPE("head", v, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", v, 0);

    // Input OK, take the first argument
    lval *x = lval_take(v, 0);

    // Delete all elements that are not head and return
    while (x->count > 1) lval_del(lval_pop(x, 1));
    return x;
}

lval *builtin_tail(lenv *e, lval *v){
    // Check for errors
    LASSERT_NUM("tail", v, 1);
    LASSERT_TYPE("tail", v, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", v, 0);

    // Input OK, take the first argument
    lval *x = lval_take(v, 0);

    // Delete first element and return
    lval_del(lval_pop(x, 0));
    return x;
}

lval *builtin_list(lenv *e, lval *v) {
    LASSERT(v, (v->type == LVAL_SEXPR), "Function 'list' passed incorrect type. Got %s, expected %s",
            ltype_name(v->type), ltype_name(LVAL_SEXPR));

    v->type = LVAL_QEXPR;
    return v;
}

lval *builtin_eval(lenv *e, lval *v){
    LASSERT_NUM("eval", v, 1);
    LASSERT_TYPE("eval", v, 0, LVAL_QEXPR);

    lval *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval *builtin_join(lenv *e, lval *v) {
    for(int i = 0; i < v->count; i++) {
        LASSERT_TYPE("join", v, i, LVAL_QEXPR);
    }

    lval *x = lval_pop(v, 0);

    while(v->count) {
        x = lval_join(x, lval_pop(v, 0));
    }

    lval_del(v);
    return x;
}

lval *builtin_cons(lenv *e, lval *v) {
    LASSERT_NUM("cons", v, 2);
    LASSERT_TYPE("cons", v, 1, LVAL_QEXPR);

    // Pop the first argument
    lval *x = lval_pop(v, 0);

    // Take the second argument (v deleted)
    lval *y = lval_take(v, 0);

    // Create a new QExpr
    lval *z = lval_qexpr();
    // Add the first argument to it as is
    z = lval_add(z, x);

    // Then join the z & y lvals (y deleted)
    z = lval_join(z, y);

    return z;
}

lval *builtin_len(lenv *e, lval *v) {
    LASSERT_NUM("len", v, 1);
    LASSERT_TYPE("len", v, 0, LVAL_QEXPR);

    return lval_num(v->cell[0]->count);
}

lval *builtin_init(lenv *e, lval *v) {
    LASSERT_NUM("init", v, 1);
    LASSERT_TYPE("init", v, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("init", v, 0);

    // Input OK, take the first argument
    lval *x = lval_take(v, 0);

    // Delete the last element and return
    lval_del(lval_pop(x, x->count - 1));
    return x;
}

lval *builtin_def(lenv *e, lval *v){
    LASSERT_TYPE("def", v, 0, LVAL_QEXPR);

    // Check that the first argument a symbol list
    lval *syms = v->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(v, (syms->cell[i]->type == LVAL_SYM), "Function 'def' cannot define non-symbol. Argument %i was a %s, expected %s", i + 1, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    // Check that there are the same amount of symbols and values
    LASSERT(v, (syms->count == v->count-1), "Function 'def' the amount of symbols passed don't match the amount of values. Got %i symbols and %i values", syms->count, v->count-1);

    for (int i = 0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], v->cell[i+1]);
    }

    lval_del(v);
    // Return empty expression "()"
    return lval_sexpr();
}

lval *builtin_exit(lenv *e, lval *v) {
    e->run = 0;
    lval_del(v);
    return lval_sym("Exiting");
}

lval *builtin_printenv(lenv *e, lval *v){
    for(int i = 0; i < e->count; i++) {
        printf("%s\n", e->syms[i]);
    }
    return lval_sexpr();
}

lval *lval_join(lval *x, lval *y) {
    // For each cell in 'y' add it to 'x'
    while(y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    // Delete the empty 'y' and return 'x'
    lval_del(y);
    return x;
}

lval *lval_num(long x){
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    v->err = NULL;
    v->sym = NULL;
    v->count = 0;
    v->cell = NULL;
    v->fun = NULL;
    return v;
}

lval *lval_err(char *s, ...){
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->sym = NULL;
    v->num = 0;
    v->count = 0;
    v->cell = NULL;
    v->fun = NULL;

    // Create a va list and initialize it
    va_list va;
    va_start(va, s);

    // Allocate 512 bytes for the error string
    v->err = malloc(512);

    // Create the error string from the arguments passed
    vsnprintf(v->err, 511, s, va);

    // Reallocate the string to the actual size
    v->err = realloc(v->err, strlen(v->err)+1);

    // Cleanup the va list
    va_end(va);

    return v;
}

lval *lval_sym(char *s){
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    v->err = NULL;
    v->num = 0;
    v->count = 0;
    v->cell = NULL;
    v->fun = NULL;
    return v;
}

lval *lval_sexpr(void){
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->err = NULL;
    v->sym = NULL;
    v->num = 0;
    v->count = 0;
    v->cell = NULL;
    v->fun = NULL;
    return v;
}

lval *lval_qexpr(void){
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->err = NULL;
    v->sym = NULL;
    v->num = 0;
    v->count = 0;
    v->cell = NULL;
    v->fun = NULL;
    return v;
}

lval *lval_fun(lbuiltin func){
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    v->err = NULL;
    v->sym = NULL;
    v->num = 0;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lenv *lenv_new(void){
    lenv *e = malloc(sizeof(lenv));
    e->run = 1;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv *e){
    for(int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval *lenv_get(lenv *e, lval *k) {
    // Iterate over all the symbols in the environment and check if any
    // of them matches to the given lval
    for(int i = 0; i < e->count; i++) {
        // Return a copy of the matching lval
        if(strcmp(e->syms[i], k->sym) == 0) return lval_copy(e->vals[i]);
    }
    // No match, return error
    return lval_err("Unbound symbol '%s'", k->sym);
}

void lenv_put(lenv *e, lval *k, lval *v){
    // Iterate over all the symbols in the environment and check if any
    // of them matches to the given lval
    for(int i = 0; i < e->count; i++) {
        if(strcmp(e->syms[i], k->sym) == 0) {
            // Replace the the value with the new one and return
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    // Value not found, add the new value to the environment
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);
    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func){
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv *e) {
    // List functions
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "cons", builtin_cons);
    lenv_add_builtin(e, "init", builtin_init);
    lenv_add_builtin(e, "len",  builtin_len);

    // Variable functions
    lenv_add_builtin(e, "def",  builtin_def);

    // Math functions
    lenv_add_builtin(e, "+",  builtin_add);
    lenv_add_builtin(e, "-",  builtin_sub);
    lenv_add_builtin(e, "*",  builtin_mul);
    lenv_add_builtin(e, "/",  builtin_div);
    lenv_add_builtin(e, "%",  builtin_rem);
    lenv_add_builtin(e, "max",  builtin_max);
    lenv_add_builtin(e, "min",  builtin_min);

    // Other
    lenv_add_builtin(e, "exit", builtin_exit);
    lenv_add_builtin(e, "printenv", builtin_printenv);
}

lval *lval_read_num(mpc_ast_t *t){
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    if(errno != ERANGE) {
        return lval_num(x);
    }
    else return lval_err("Invalid number");
}

// Print the lval "value"
void lval_print(lenv *e, lval *v) {
    switch(v->type) {
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;

        case LVAL_NUM:
            printf("%li", v->num);
            break;

        case LVAL_SYM:
            printf("%s", v->sym);
            break;

        case LVAL_SEXPR:
            lval_expr_print(e, v, '(', ')');
            break;

        case LVAL_QEXPR:
            lval_expr_print(e, v, '{', '}');
            break;

        case LVAL_FUN:
            for(int i = 0; i < e->count; i++) {
                if(v->fun == e->vals[i]->fun) {
                    printf("Function name: %s", e->syms[i]);
                    break;
                }
            }
            break;

        default:
            printf("Error: unknown lval type!");
            break;
    }
}

void lval_expr_print(lenv *e, lval *v, char open, char close) {
    putchar(open);
    for(int i = 0; i < v->count; i++) {
        lval_print(e, v->cell[i]);
        // Don't print trailing space if last element
        if (i != (v->count - 1)) putchar(' ');
    }
    putchar(close);
}

// Print the lval "value" plus a newline char
void lval_println(lenv *e, lval *v) { lval_print(e, v); putchar('\n'); }

lval *lval_add(lval *v, lval *x){
    v->count += 1;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval *lval_read(mpc_ast_t *t) {
    // If the input is a symbol or a number return a conversion to that type
    if (strstr(t->tag, "number")) return lval_read_num(t);
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

    // If the input is root (>) or a sexpr then create an empty list
    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
    if (strstr(t->tag, "sexpr"))  x = lval_sexpr();
    if (strstr(t->tag, "qexpr"))  x = lval_qexpr();

    // Fill this list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if(strcmp(t->children[i]->contents, "(") == 0) continue;
        if(strcmp(t->children[i]->contents, ")") == 0) continue;
        if(strcmp(t->children[i]->contents, "}") == 0) continue;
        if(strcmp(t->children[i]->contents, "{") == 0) continue;
        if(strcmp(t->children[i]->tag,  "regex") == 0) continue;
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_del(lval *v) {
    switch(v->type) {
        case LVAL_NUM:
            // Nothing extra to free with the number type
            break;

        case LVAL_ERR:
            // Free the error string
            free(v->err);
            break;

        case LVAL_SYM:
            // Free the symbol string
            free(v->sym);
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            // Free all the sub-Expressions
            for(int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            // Free the pointer array as well
            free(v->cell);
            break;

        case LVAL_FUN: break;
    }

    // Finally free the lval struct itself
    free(v);
}
