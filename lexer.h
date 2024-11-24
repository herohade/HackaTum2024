#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>
#include <ctype.h>
#include <string>
#include <queue>

// Just-in-Time Access Compiler
//
// Just-in-Time (JIT) access is a security practice where the risk of standing privileges is minimized by granting access on an as-needed basis and for limited, predetermined periods of time for a specific purpose, making exploits harder.
//
// In this sub-challange, the goal is to write a compiler that transforms a basic C-like language into a simple form of bytecode. However, for every program there are certain privileged objects which may not just be accessed, rather one has to apply for access beforehand. All writes to privileged objects changing their value are to be considered non-optimisable side-effects and must enhibit sequential consistency.
//
// Since security must always keep performance in mind, the goal of this challange is to optimise the emitted bytecode as much as possible.
// Input Program
//
// Input programs have the following syntax:
//
//     program => <function>*
//     function => <identifier> "(" <params>? ")" <scope>
//     params => <identifier> ("," <identifier>)*
//     scope => "{" <statement>* "}"
//     statement => "return" <expr>? ";" | <scope> | "if" "(" <expr> ")" <statement> ("else" <statement>)? | <expr> ";"
//     expr => "(" <expr> ")" | <identifier> "(" <args>? ")" | <identifier> | <number> | <expr> "+" <expr> | <expr> "-" <expr> | <expr> "*" <expr> | <expr> "<" <expr> | <expr> ">" <expr> | <expr> "<=" <expr> | <expr> ">=" <expr> | <expr> "==" <expr> | <expr> "!=" <expr> | <expr> "=" <expr>
//     args => <expr> ("," <expr> )*
//     identifier => [a-zA-Z_][a-zA-Z0-9_]*
//     number => [0-9]+
//
// Everything works as if it were the equivalent C code. No semantic analysis must be performed, it is guaranteed that the input programs are valid. Futhermore, implementation of short-circuiting logic operators is not required. There exist only 64 bit unsigned numbers. Overflow is defined by 2's complement.
//
// There are 4 special function names corresponding to syscalls: open, write, read and ioctl. These may not be used for user-defined functions and must be translated to the syscall instruction (see below). The calling-order of these syscalls must not be changed.
//
// In addition to that, there are n ∈ ℕ lines at the very top specifying which variables are privileged objects and at what memory location they reside. The format is the following:
//
// // (name,addr)
//
// These privileged objects may only be read and written to after having requested access to the memory location. Access need not be requested for sycalls.
// Instructions
//
// The output program must only contain the following instructions:
//
//     exit
//     add <reg_in_1> <reg_in_2> <reg_out>
//     sub <reg_in_1> <reg_in_2> <reg_out>
//     mul <reg_in_1> <reg_in_2> <reg_out>
//     load <reg_mem_addr> <reg_val>
//     store <reg_mem_addr> <reg_val>
//     request <reg_mem_addr> <reg_num_cycles>
//     li <reg> <imm>
//     jmpEqZ <reg_test> <reg_next_instr>
//     syscall <reg_syscall_num>
//
// Immediates are 64 bit unsigned, there are 8 registers, memory addresses must not exceed 2^16-1.
// Behaviour
//
//     exit: Exits the program. No further instructions will be executed. Takes 0 cycles.
//     add/sub/mul: Adds/Subtracts/Multiplies contents of register <reg_in_1> and register <reg_in_2> and writes the results to register <reg_out>. Takes 1 cycle.
//     load/store: Loads from / Stores to the memory address contained in <reg_mem_addr> the contents of <reg_val>. Load takes 10, store takes 5 cycles.
//     request: Requests access to the privileged object contained at address <\reg_mem_addr> for the next x cycles, contained in register <reg_num_cycles>. Takes 20 + x^2/100 cycles.
//     li: Loads the immediate <imm> to register <reg>. Takes 1 cycle.
//     jmpEqZ: Tests whether the content of <reg_test> equal 0, if so jumps to instruction pointed to by contents of <reg_next_instr>. Otherwise execution carries on as usual. Takes 5 cycles.
//     syscall: Performs a system call. The register <reg_syscall_num> contains the syscall number with the following correspondence: 0 => open, 1 => write, 2 => read, 3 => ioctl, taking 2, 3, 3 and 3 arguments, respectively. The arguments are passed in the registers <0>, <1> and <2>. See the BSD manpages for further information. Mind that you are still only able to pass the unsigned integers - these are cast to the correct types internally. Takes 20 cycles.
//
// Translation Examples
// Trivial example
//
// // (a,200)
//
// main() {
//     d = 0;
//     e = 2;
//     a = d+e;
// }
//
// li 0 0
// li 1 2
// add 0 1 2
// li 3 200
// li 4 20
// request 3 4
// store 3 2
//
// Syscall example
//
// // (a,400)
//
// main() {
//     a = 1;
//     open(4,5);
// }
//
//     li 2 1
//     li 3 400
//     li 4 20
//     request 3 4
//     store 3 2
//     li 0 4
//     li 1 5
//     li 2 0
//     syscall 2
//
// Further valid programs
//
// // (a,4)
// // (ggf,100)
// // (foo,1)
//
// bar() {
//     c = ggf;
//     ggf = ggf+1;
//     if(c) {
//         bar();
//     }
//     foo = a;
// }
//
// main() {
//     a = 1;
//     bar();
//     return 0;
// }
//
// // (b,100)
//
// main() {
//     a = 5;
//     if(a == 5)
//         b = 2;
//     else
//         b = 100;
//     return a;
// }

// This is our language grammar:
// program => <priv_obj>* <function>*
// priv_obj => <comment> "(" <identifier> "," <number> ")"
// comment => "//"
// function => <identifier> "(" <params>? ")" <scope>
// params => <identifier> ("," <identifier>)*
// scope => "{" <statement>* "}"
// statement => "return" <expr>? ";" | <scope> | "if" "(" <expr> ")" <statement> ("else" <statement>)? | <expr> ";"
// expr => "(" <expr> ")" | (<identifier> | <sys_call>) "(" <args>? ")" | <identifier> | <number> | <expr> "+" <expr> | <expr> "-" <expr> | <expr> "*" <expr> | <expr> "<" <expr> | <expr> ">" <expr> | <expr> "<=" <expr> | <expr> ">=" <expr> | <expr> "==" <expr> | <expr> "!=" <expr> | <expr> "=" <expr>
// args => <expr> ("," <expr> )*
// identifier => [a-zA-Z_][a-zA-Z0-9_]*
// sys_call => "open" | "write" | "read" | "ioctl"
// number => [0-9]+

class lexer {
public:
    enum TokenType {
        TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_KEYWORD, TOKEN_OPERATOR,
        TOKEN_DELIMITER, TOKEN_EOF, TOKEN_INVALID, TOKEN_PRIV_DELIM,
        TOKEN_SYS_CALL
    };

    // TOKEN_IDENTIFIER: [a-zA-Z_][a-zA-Z0-9_]*
    // TOKEN_NUMBER: [0-9]+
    // TOKEN_KEYWORD: if, else, return
    // TOKEN_OPERATOR: +, -, *, <, >, <=, >=, ==, !=, =
    // TOKEN_DELIMITER: (, ), {, }, ;, ,
    // TOKEN_EOF: end of file
    // TOKEN_INVALID: invalid token
    // TOKEN_PRIV_DELIM: // (.*)
    // TOKEN_SYS_CALL: open, write, read, ioctl

    struct Token {
        TokenType type;
        std::string value;
    };

    // Token to string function
    static std::string to_string(Token token) {
        std::string result;
        switch (token.type) {
            case TOKEN_IDENTIFIER:
                result += "TOKEN_IDENTIFIER: ";
            break;
            case TOKEN_NUMBER:
                result += "TOKEN_NUMBER: ";
            break;
            case TOKEN_KEYWORD:
                result += "TOKEN_KEYWORD: ";
            break;
            case TOKEN_OPERATOR:
                result += "TOKEN_OPERATOR: ";
            break;
            case TOKEN_DELIMITER:
                result += "TOKEN_DELIMITER: ";
            break;
            case TOKEN_EOF:
                result += "TOKEN_EOF: ";
            break;
            case TOKEN_INVALID:
                result += "TOKEN_INVALID: ";
            break;
            case TOKEN_PRIV_DELIM:
                result += "TOKEN_PRIV_DELIM: ";
            break;
            case TOKEN_SYS_CALL:
                result += "TOKEN_SYS_CALL: ";
            break;
        }
        return result + token.value;
    };
    // queue<Token> to string function
    std::string to_string(std::queue<Token> tokens) {
        std::string result;
        Token token;
        while (!tokens.empty()) {
            token = tokens.front();
            tokens.pop();
            result += lexer::to_string(token) + "\n";
        }
        return result;
    }

    /*
     * This function takes a file and returns a queue<Token> of tokens
     * @param file - the path to the file
     * @return queue<Token> - a queue of tokens
     */
    std::queue<Token> lexer_fct(const char *file) {
        std::queue<Token> tokens;
        FILE *f = fopen(file, "r");
        if (!f) {
            printf("Error: could not open file\n");
            return tokens;
        }

        char c;
        while ((c = fgetc(f)) != EOF) {
            if (isspace(c)) {
                continue;
            }

            // definition of privileged object
            if (c == '/') {
                if ((c = fgetc(f)) != EOF && c == '/') {
                    Token comment;
                    comment.type = TOKEN_PRIV_DELIM;
                    comment.value = "//";
                    tokens.push(comment);

                    // skip whitespace
                    while (isspace(c = fgetc(f)));

                    if (c == '(') {
                        Token delimiter;
                        delimiter.type = TOKEN_DELIMITER;
                        delimiter.value = c;
                        tokens.push(delimiter);

                        std::string identifier, value;
                        while ((c = fgetc(f)) != EOF && c != ',') {
                            identifier += c;
                        }
                        Token token;
                        token.type = TOKEN_IDENTIFIER;
                        token.value = identifier;
                        tokens.push(token);

                        Token comma;
                        comma.type = TOKEN_DELIMITER;
                        comma.value = c;
                        tokens.push(comma);

                        while ((c = fgetc(f)) != EOF && c != ')') {
                            value += c;
                        }
                        Token token2;
                        token2.type = TOKEN_NUMBER;
                        token2.value = value;
                        tokens.push(token2);

                        Token delimiter2;
                        delimiter2.type = TOKEN_DELIMITER;
                        delimiter2.value = c;
                        tokens.push(delimiter2);

                    } else {
                        ungetc(c, f);
                        Token token;
                        token.type = TOKEN_INVALID;
                        token.value = "/";
                        tokens.push(token);
                    }
                } else {
                    ungetc(c, f);
                    Token token;
                    token.type = TOKEN_INVALID;
                    token.value = "/";
                    tokens.push(token);
                }
            } else if (isalpha(c) || c == '_') { // definition of function name
                std::string value;
                value += c;
                while ((c = fgetc(f)) != EOF && (isalnum(c) || c == '_')) {
                    value += c;
                }
                ungetc(c, f);

                // check if keyword
                if (value == "if" || value == "else" || value == "return") {
                    Token token;
                    token.type = TOKEN_KEYWORD;
                    token.value = value;
                    tokens.push(token);
                } else if (value == "open" || value == "write" || value == "read" || value == "ioctl") { // check if syscall
                    Token token;
                    token.type = TOKEN_SYS_CALL;
                    token.value = value;
                    tokens.push(token);
                } else {
                    Token token;
                    token.type = TOKEN_IDENTIFIER;
                    token.value = value;
                    tokens.push(token);
                }
            } else if (isdigit(c)) { // definition of number
                std::string value;
                value += c;
                while ((c = fgetc(f)) != EOF && isdigit(c)) {
                    value += c;
                }
                ungetc(c, f);

                Token token;
                token.type = TOKEN_NUMBER;
                token.value = value;
                tokens.push(token);
            } else if (c == '(' || c == ')' || c == '{' || c == '}' || c == ';' || c == ',') { // definition of delimiter
                Token token;
                token.type = TOKEN_DELIMITER;
                token.value = c;
                tokens.push(token);
            } else if (c == '+' || c == '-' || c == '*' || c == '<' || c == '>' || c == '=') { // definition of operator
                std::string value;
                value += c;
                if ((c == '<' || c == '>' || c == '=') && (c = fgetc(f)) != EOF) {
                    if (c == '=') {
                        value += c;
                    } else {
                        ungetc(c, f);
                    }
                }

                Token token;
                token.type = TOKEN_OPERATOR;
                token.value = value;
                tokens.push(token);
            } else {
                Token token;
                token.type = TOKEN_INVALID;
            }
        }

        return tokens;
    }

};

#endif //LEXER_H
