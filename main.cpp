#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "transpiler.h"

// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or
// click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.
int main(int argc, char** argv) {
    lexer lex;
    parser parse;
    transpiler tran;

    const char* IN_FILE = "../test.txt";
    const char* OUT_FILE = argv[1] ? argv[1] : "../output.in";

    auto token_queue = lex.lexer_fct(IN_FILE);
    std::cout << lex.to_string(token_queue) << std::endl;

    auto ast = parse.generateAst(token_queue);
    std::cout << parse.to_string(ast) << std::endl;

    tran.transpile(OUT_FILE, ast);

    return 0;
}

// TIP See CLion help at <a
// href="https://www.jetbrains.com/help/clion/">jetbrains.com/help/clion/</a>.
//  Also, you can try interactive lessons for CLion by selecting
//  'Help | Learn IDE Features' from the main menu.