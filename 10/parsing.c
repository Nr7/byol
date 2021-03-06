#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }

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

// All the possible lval types
enum Lval_types { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };
typedef struct lval {
    int type;
    long num;

    // Error and Symbol types have some string data
    char *err;
    char *sym;

    // Count and pointer to a list of "lval"
    int count;
    struct lval **cell;
} lval;

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

lval *lval_eval_sexpr(lval *v);
lval *lval_eval(lval *v);
lval *lval_pop(lval *v, int i);
lval *lval_take(lval *v, int i);

lval *builtin(lval *v, char *func);
lval *builtin_op(lval *v, char *op);
lval *builtin_head(lval *v);
lval *builtin_tail(lval *v);
lval *builtin_list(lval *v);
lval *builtin_eval(lval *v);
lval *builtin_join(lval *v);
lval *builtin_cons(lval *v);
lval *builtin_len(lval *v);
lval *builtin_init(lval *v);

lval *lval_join(lval *x, lval *y);

lval *lval_num(long x);
lval *lval_err(char *s);
lval *lval_sym(char *s);
lval *lval_sexpr(void);
lval *lval_qexpr(void);

void lval_print(lval *v);
void lval_expr_print(lval *v, char open, char close);
void lval_println(lval *v);
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
                symbol   : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" | \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | \"cons\" | \"len\" | \"init\" ; \
                sexpr    : '(' <expr>* ')' ; \
                qexpr    : '{' <expr>* '}' ; \
                expr     : <number> | <symbol> | <sexpr> | <qexpr> ; \
                lispy    : /^/ <expr>* /$/ ; \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.6");
    puts("Press Ctrl+c to Exit\n");

    while(1) {
        char* input = readline("lispy> ");
        add_history(input);

        // Try to parse the input
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)) {
            // Print the AST on success
            /*mpc_ast_print(r.output);*/
            // Evaluate expression
            /*lval val = eval(r.output);*/
            lval *val = lval_read(r.output);
            /*lval_println(val);*/
            val = lval_eval(val);
            lval_println(val);
            lval_del(val);
            mpc_ast_delete(r.output);
        } else {
            // Otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    // Undefine and Delete our Parsers
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}

lval *lval_eval_sexpr(lval *v){
    // Empty expression
    if(v->count == 0) return v;

    // Evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->count; i++) {
        if(v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    // Single expression
    if(v->count == 1) return lval_take(v, 0);

    // Ensure first element is a symbol
    lval *f = lval_pop(v, 0);
    if(f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with a symbol!");
    }

    // Call builtin with operator
    lval *result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

lval *lval_eval(lval *v) {
    // Evaluate S-Expressions
    if(v->type == LVAL_SEXPR) return lval_eval_sexpr(v);

    // All other lval types remain the same
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

lval *builtin(lval *v, char *func){
    if (strcmp("list", func) == 0) return builtin_list(v);
    if (strcmp("head", func) == 0) return builtin_head(v);
    if (strcmp("tail", func) == 0) return builtin_tail(v);
    if (strcmp("join", func) == 0) return builtin_join(v);
    if (strcmp("eval", func) == 0) return builtin_eval(v);
    if (strcmp("cons", func) == 0) return builtin_cons(v);
    if (strcmp("len",  func) == 0) return builtin_len(v);
    if (strcmp("init", func) == 0) return builtin_init(v);
    if (strstr("+-/*%minmax", func)) return builtin_op(v, func);
    lval_del(v);
    return lval_err("Unknown function!");
}

lval *builtin_op(lval *v, char *op) {
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

lval *builtin_head(lval *v){
    // Check for errors
    LASSERT(v, (v->count == 1), "Function 'head' passed too many arguments!");
    LASSERT(v, (v->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect type!");
    LASSERT(v, (v->cell[0]->count != 0), "Function 'head' passed \"{}\"!");

    // Input OK, take the first argument
    lval *x = lval_take(v, 0);

    // Delete all elements that are not head and return
    while (x->count > 1) lval_del(lval_pop(x, 1));
    return x;
}

lval *builtin_tail(lval *v){
    // Check for errors
    LASSERT(v, (v->count == 1), "Function 'tail' passed too many arguments!");
    LASSERT(v, (v->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect type!");
    LASSERT(v, (v->cell[0]->count != 0), "Function 'tail' passed \"{}\"!");

    // Input OK, take the first argument
    lval *x = lval_take(v, 0);

    // Delete first element and return
    lval_del(lval_pop(x, 0));
    return x;
}

lval *builtin_list(lval *v) {
    LASSERT(v, (v->type == LVAL_SEXPR), "Function 'list' passed incorrect type!");

    v->type = LVAL_QEXPR;
    return v;
}

lval *builtin_eval(lval *v){
    LASSERT(v, (v->count == 1), "Function 'eval' passed too many arguments!");
    LASSERT(v, (v->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

    lval *x = lval_take(v, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval *builtin_join(lval *v) {
    for(int i = 0; i < v->count; i++) {
        LASSERT(v, (v->cell[i]->type == LVAL_QEXPR), "Function 'join' passed incorrect type!");
    }

    lval *x = lval_pop(v, 0);

    while(v->count) {
        x = lval_join(x, lval_pop(v, 0));
    }

    lval_del(v);
    return x;
}

lval *builtin_cons(lval *v) {
    LASSERT(v, (v->count == 2), "Function 'cons' passed incorrect amount of arguments!");
    LASSERT(v, (v->cell[1]->type == LVAL_QEXPR), "Function 'cons' passed incorrect type!");

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

lval *builtin_len(lval *v) {
    LASSERT(v, (v->count == 1), "Function 'len' passed too many arguments!");
    LASSERT(v, (v->cell[0]->type == LVAL_QEXPR), "Function 'len' passed incorrect type!");

    return lval_num(v->cell[0]->count);
}

lval *builtin_init(lval *v) {
    LASSERT(v, (v->count == 1), "Function 'init' passed too many arguments!");
    LASSERT(v, (v->cell[0]->type == LVAL_QEXPR), "Function 'init' passed incorrect type!");
    LASSERT(v, (v->cell[0]->count != 0), "Function 'init' passed \"{}\"!");

    // Input OK, take the first argument
    lval *x = lval_take(v, 0);

    // Delete the last element and return
    lval_del(lval_pop(x, x->count - 1));
    return x;
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
    return v;
}

lval *lval_err(char *s){
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(s) + 1);
    strcpy(v->err, s);
    v->sym = NULL;
    v->num = 0;
    v->count = 0;
    v->cell = NULL;
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
    return v;
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
void lval_print(lval *v) {
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
            lval_expr_print(v, '(', ')');
            break;

        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;

        default:
            printf("Error: unknown lval type!");
            break;
    }
}

void lval_expr_print(lval *v, char open, char close) {
    putchar(open);
    for(int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        // Don't print trailing space if last element
        if (i != (v->count - 1)) putchar(' ');
    }
    putchar(close);
}

// Print the lval "value" plus a newline char
void lval_println(lval *v) { lval_print(v); putchar('\n'); }

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
    }

    // Finally free the lval struct itself
    free(v);
}
