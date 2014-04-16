#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

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

// Different possible lval types
enum Lval_types { LVAL_NUM, LVAL_ERR };
// Different possible lval error types
enum Lval_error_types { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
// this struct can represent either an numerical value or an error
// value (e.g. an divide by zero error)
typedef struct {
    int type;
    union {
        long num;
        int err;
    } value;
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

lval eval(mpc_ast_t *t, int print);
lval eval_op(lval x, char* op, lval y);

lval lval_num(long x);
lval lval_err(int x);
void lval_print(lval v);
void lval_println(lval v);

int main(int argc, char** argv) {

    // Create and define parsers
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispy    = mpc_new("lispy");

    mpca_lang(MPC_LANG_DEFAULT,
            "\
                number   : /-?[0-9]+/ ; \
                operator : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\" ; \
                expr     : <number> | '(' <operator> <expr>+ ')' ; \
                lispy    : /^/ <operator> <expr>+ /$/ ; \
            ",
            Number, Operator, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.4");
    puts("Press Ctrl+c to Exit\n");

    while(1) {
        char* input = readline("lispy> ");
        add_history(input);

        // Try to parse the input
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)) {
            // Print the AST on success
            mpc_ast_print(r.output);
            // Evaluate expression
            lval val = eval(r.output, 1);
            printf(" = ");
            lval_println(val);
            mpc_ast_delete(r.output);
        } else {
            // Otherwise print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    // Undefine and Delete our Parsers
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}

lval eval(mpc_ast_t *t, int print) {
    // Check if number, return value if it is
    if(strstr(t->tag, "number")) {
        // Check for errors in conversion
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        if(errno != ERANGE) {
            if(print) printf(" %li", x);
            return lval_num(x);
        }
        else
            return lval_err(LERR_BAD_NUM);
    }

    // The first child is always a '(' so we skip it
    // The second child [1] is the operator
    if(print) printf("(%s", t->children[1]->contents);
    char *op = t->children[1]->contents;
    // Store the first value in the variable so we can apply the calculations to it
    lval val = eval(t->children[2], print);

    // Check if the op is "-" and if there is only one argument
    // If yes then negate the number
    if(strcmp(op, "-") == 0 && t->children_num <= 4) {
        val.value.num = -val.value.num;
        return val;
    }
    int i = 3;
    while(strstr(t->children[i]->tag, "expr")){
        val = eval_op(val, op, eval(t->children[i], print));
        i++;
    }

    if(print) printf(")");
    return val;
}

lval eval_op(lval x, char* op, lval y) {
    // If either value is an error return it
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    // Otherwise do the math!
    if (strcmp(op, "+") == 0) { return lval_num(x.value.num + y.value.num); }
    if (strcmp(op, "-") == 0) { return lval_num(x.value.num - y.value.num); }
    if (strcmp(op, "*") == 0) { return lval_num(x.value.num - y.value.num); }
    if (strcmp(op, "/") == 0) {
        // If the second operator is zero return an error
        if(y.value.num == 0) return lval_err(LERR_DIV_ZERO);
        return lval_num(x.value.num / y.value.num);
    }
    if (strcmp(op, "%") == 0) {
        // If the second operator is zero return an error
        if(y.value.num == 0) return lval_err(LERR_DIV_ZERO);
        return lval_num(x.value.num % y.value.num);
    }
    if (strcmp(op, "^") == 0) {
        // If the y value is 0 we return 1, because x^0 is always 1
        if(y.value.num == 0) return lval_num(1);
        lval val = lval_num(x.value.num);
        for(int i = 1; i < y.value.num; i++) {
            val = eval_op(val, "*", x);
        }
        return val;
    }
    if (strcmp(op, "min") == 0) {
        if(x.value.num <= y.value.num) return x;
        else               return y;
    }
    if (strcmp(op, "max") == 0) {
        if(x.value.num >= y.value.num) return x;
        else               return y;
    }
    return lval_err(LERR_BAD_OP);
}

lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.value.num = x;
    return v;
}

lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.value.err = x;
    return v;
}

// Print the lval "value"
void lval_print(lval v) {
    switch(v.type) {
        case LVAL_ERR:
            switch(v.value.err){
                case LERR_DIV_ZERO:
                    printf("Error: divide by zero!");
                    break;
                case LERR_BAD_OP:
                    printf("Error: invalid operator!");
                    break;
                case LERR_BAD_NUM:
                    printf("Error: invalid number!");
                    break;
                default:
                    printf("Error: unknown error!");
                    break;
            }
            break;

        case LVAL_NUM:
            printf("%li", v.value.num);
            break;

        default:
            printf("Error: unknown lval type!");
            break;
    }
}

// Print the lval "value" plus a newline char
void lval_println(lval v) { lval_print(v); putchar('\n'); }
