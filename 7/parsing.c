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

int main(int argc, char** argv) {

    // Create and define parsers
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispy    = mpc_new("lispy");

    mpca_lang(MPC_LANG_DEFAULT,
            "\
                number   : /-?[0-9]+/ ; \
                operator : '+' | '-' | '*' | '/' ; \
                expr     : <number> | '(' <operator> <expr>+ ')' ; \
                lispy    : /^/ <operator> <expr>+ /$/ ; \
            ",
            Number, Operator, Expr, Lispy);

    puts("Lispy Version 0.0.0.0.1");
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
    if(print) printf("(%c", *t->children[1]->contents);
    // Store the first value in the variable so we can apply the calculations to it
    long val = eval(t->children[2], print);
    for(int i = 3; i < t->children_num - 1; i++) {
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
        }
    }

    if(print) printf(")");
    return val;
}
