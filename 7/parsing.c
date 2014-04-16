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

long eval(mpc_ast_t *t, int print);
long eval_op(long x, char* op, long y);

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

    puts("Lispy Version 0.0.0.0.3");
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
            long val = eval(r.output, 1);
            printf(" = %li\n", val);
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

long eval(mpc_ast_t *t, int print) {
    // Check if number, return value if it is
    if(strstr(t->tag, "number")) {
        if(print) printf(" %i", atoi(t->contents));
        return atoi(t->contents);
    }

    // The first child is always a '(' so we skip it
    // The second child [1] is the operator
    if(print) printf("(%s", t->children[1]->contents);
    // Store the first value in the variable so we can apply the calculations to it
    long val = eval(t->children[2], print);
/*    for(int i = 3; i < t->children_num - 1; i++) {
        switch(*t->children[1]->contents) {
            case '+':
                val = val + eval(t->children[i], print);
                break;
            case '-':
                val = val - eval(t->children[i], print);
                break;
            case '*':
                val = val * eval(t->children[i], print);
                break;
            case '/':
                val = val / eval(t->children[i], print);
                break;
            case '%':
                val = val % eval(t->children[i], print);
                break;
            case '^':
                val = val / eval(t->children[i], print);
                break;
        }
    }
*/

    char *op = t->children[1]->contents;
    // Check if the op is "-" and if there is only one argument
    // If yes then negate the number
    if(strcmp(op, "-") == 0 && t->children_num <= 4) {
        return -val;
    }
    int i = 3;
    while(strstr(t->children[i]->tag, "expr")){
        val = eval_op(val, op, eval(t->children[i], print));
        i++;
    }

    if(print) printf(")");
    return val;
}

long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; } // Divide by zero error fixed in 8
    if (strcmp(op, "%") == 0) { return x % y; }
    if (strcmp(op, "^") == 0) {
        long val = x;
        for(int i = 1; i < y; i++) {
            val = eval_op(val, "*", x);
        }
        return val;
    }
    if (strcmp(op, "min") == 0) {
        if(x <= y) return x;
        else       return y;
    }
    if (strcmp(op, "max") == 0) {
        if(x >= y) return x;
        else       return y;
    }
    return 0;
}
