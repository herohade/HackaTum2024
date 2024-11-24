#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <queue>
#include <memory>

#include "lexer.h"


// Here we get the vector of tokens from the lexer
// From this we will build the AST

// TOKEN_IDENTIFIER: [a-zA-Z_][a-zA-Z0-9_]*
// TOKEN_NUMBER: [0-9]+
// TOKEN_KEYWORD: if, else, return
// TOKEN_OPERATOR: +, -, *, <, >, <=, >=, ==, !=, =
// TOKEN_DELIMITER: (, ), {, }, ;, ,
// TOKEN_EOF: end of file
// TOKEN_INVALID: invalid token
// TOKEN_PRIV_DELIM: // (.*)
// TOKEN_SYS_CALL: open, write, read, ioctl

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
    class parser {

    public:
        enum NodeType {
            FUNCTION, // <function>
            PRIV_OBJ, IDENTIFIER, ADDRESS, // <priv_obj>
            FUNC_DEF, PARAMS, SCOPE, // <function>
            RETURN, BRANCH, CONDITION, // <statement>
            EXPR, FUNC_CALL, NUMBER, BIN_OP, // <expr>
            ARGS, // <args>
            SYS_CALL, // <sys_call>
        };

        enum BinOpType {
            ADD, SUB, MUL, LT, GT, LE, GE, EQ, NE, ASS
        };

        enum SysCall_Type {
            OPEN, WRITE, READ, IOCTL
        };

        class PrivObjNode;
        class FuncDefNode;
        class IdentifierNode;
        class AddressNode;
        class ParamsNode;
        class ScopeNode;
        class ExprNode;
        class ConditionNode;
        class ArgsNode;
        class BinOpNode;

        // basic node structure
        class Node {
        public:
            NodeType type;
        };

        class StatementNode : public Node {};

        class ProgramNode: public Node {
        public:
            std::vector<PrivObjNode *> privObjNodes;
            std::vector<FuncDefNode *> funcDefNodes;

            explicit ProgramNode(std::vector<PrivObjNode *> privObjNodes, std::vector<FuncDefNode *> funcDefNodes) : Node() {
                this->type = FUNCTION;
                this->privObjNodes = std::move(privObjNodes);
                this->funcDefNodes = std::move(funcDefNodes);
            }
        };

        class PrivObjNode : public Node {
        public:
            IdentifierNode *identifier;
            AddressNode *address;

            explicit PrivObjNode(IdentifierNode *identifier, AddressNode *address) : Node() {
                this->type = PRIV_OBJ;
                this->identifier = identifier;
                this->address = address;
            }
        };

        class AddressNode : public Node {
        public:
            uint16_t value; // 2^16-1

            explicit AddressNode(const uint16_t value) : Node() {
                this->type = ADDRESS;
                this->value = value;
            }
        };

        class FuncDefNode : public Node {
        public:
            IdentifierNode *identifier;
            ParamsNode *params;
            ScopeNode *scope;

            explicit FuncDefNode(IdentifierNode *identifier, ParamsNode *params, ScopeNode *scope) : Node() {
                this->type = FUNC_DEF;
                this->identifier = identifier;
                this->params = params;
                this->scope = scope;
            }
        };

        class ParamsNode : public Node {
        public:
            std::vector<IdentifierNode *> params;

            explicit ParamsNode(std::vector<IdentifierNode *> params) : Node() {
                this->type = PARAMS;
                this->params = std::move(params);
            }
        };

        class ScopeNode : public StatementNode {
        public:
            std::vector<StatementNode *> statements;

            explicit ScopeNode(std::vector<StatementNode *> statements) : StatementNode() {
                this->type = SCOPE;
                this->statements = std::move(statements);
            }
        };

        class ReturnNode : public StatementNode {
        public:
            ExprNode *expr;

            explicit ReturnNode(ExprNode *expr) : StatementNode() {
                this->type = RETURN;
                this->expr = expr;
            }
        };

        class BranchNode : public StatementNode {
        public:
            ConditionNode *condition;
            StatementNode *statement;
            StatementNode *else_statement;

            explicit BranchNode(ConditionNode *condition, StatementNode *statement, StatementNode *else_statement) : StatementNode() {
                this->type = BRANCH;
                this->condition = condition;
                this->statement = statement;
                this->else_statement = else_statement;
            }
        };

        class ConditionNode : public StatementNode {
        public:
            ExprNode *expr;

            explicit ConditionNode(ExprNode *expr) : StatementNode() {
                this->type = CONDITION;
                this->expr = expr;
            }
        };

        class ExprNode : public StatementNode {
        public:
            ExprNode *expr;

            explicit ExprNode(ExprNode *expr) : StatementNode() {
                this->type = EXPR;
                this->expr = expr;
            }
        };

        class IdentifierNode : public ExprNode {
        public:
            std::string value;

            explicit IdentifierNode(std::string value) : ExprNode(nullptr) {
                this->type = IDENTIFIER;
                this->value = std::move(value);
            }
        };

        class NumberNode : public ExprNode {
        public:
            uint64_t value;

            explicit NumberNode(uint64_t value) : ExprNode(nullptr) {
                this->type = NUMBER;
                this->value = value;
            }
        };

        class BinOpNode : public ExprNode {
        public:
            ExprNode *lhs;
            ExprNode *rhs;
            BinOpType op;

            explicit BinOpNode(ExprNode *lhs, ExprNode *rhs, BinOpType op) : ExprNode(nullptr) {
                this->type = BIN_OP;
                this->lhs = lhs;
                this->rhs = rhs;
                this->op = op;
            }
        };

        class FuncCallNode : public ExprNode {
        public:
            IdentifierNode *identifier;
            ArgsNode *args;

            explicit FuncCallNode(IdentifierNode *identifier, ArgsNode *args) : ExprNode(nullptr) {
                this->type = FUNC_CALL;
                this->identifier = identifier;
                this->args = args;
            }
        };

        class ArgsNode : public Node {
        public:
            std::vector<ExprNode *> args;

            explicit ArgsNode(std::vector<ExprNode *> args) : Node() {
                this->type = ARGS;
                this->args = std::move(args);
            }
        };

        class SysCallNode : public ExprNode {
        public:
            SysCall_Type syscall;
            ArgsNode *args;

            explicit SysCallNode(const SysCall_Type syscall, ArgsNode *args) : ExprNode(nullptr) {
                this->type = SYS_CALL;
                this->syscall = syscall;
                this->args = args;
            }
        };

        // print an ascii tree of the AST
        std::string to_string(Node *root) {
            std::string result;
            if (!root) {
                return result;
            }
            switch (root->type) {
                case FUNCTION: {
                    auto *programNode = static_cast<ProgramNode *>(root);
                    result += "ProgramNode\n";
                    for (auto &privObjNode : programNode->privObjNodes) {
                        result += to_string(privObjNode);
                    }
                    for (auto &funcDefNode : programNode->funcDefNodes) {
                        result += to_string(funcDefNode);
                    }
                    break;
                }
                case PRIV_OBJ: {
                    auto *privObjNode = static_cast<PrivObjNode *>(root);
                    result += "PrivObjNode\n";
                    result += to_string(privObjNode->identifier);
                    result += to_string(privObjNode->address);
                    break;
                }
                case IDENTIFIER: {
                    auto *identifierNode = static_cast<IdentifierNode *>(root);
                    result += "IdentifierNode: " + identifierNode->value + "\n";
                    break;
                }
                case ADDRESS: {
                    auto *addressNode = static_cast<AddressNode *>(root);
                    result += "AddressNode: " + std::to_string(addressNode->value) + "\n";
                    break;
                }
                case FUNC_DEF: {
                    auto *funcDefNode = static_cast<FuncDefNode *>(root);
                    result += "FuncDefNode\n";
                    result += to_string(funcDefNode->identifier);
                    result += to_string(funcDefNode->params);
                    result += to_string(funcDefNode->scope);
                    break;
                }
                case PARAMS: {
                    auto *paramsNode = static_cast<ParamsNode *>(root);
                    result += "ParamsNode\n";
                    for (auto &param : paramsNode->params) {
                        result += to_string(param);
                    }
                    break;
                }
                case SCOPE: {
                    auto *scopeNode = static_cast<ScopeNode *>(root);
                    result += "ScopeNode\n";
                    for (auto &statement : scopeNode->statements) {
                        result += to_string(statement);
                    }
                    break;
                }
                case RETURN: {
                    auto *returnNode = static_cast<ReturnNode *>(root);
                    result += "ReturnNode\n";
                    if (returnNode->expr != nullptr) {
                        result += to_string(returnNode->expr);
                    }
                    break;
                }
                case BRANCH: {
                    auto *branchNode = static_cast<BranchNode *>(root);
                    result += "BranchNode\n";
                    result += to_string(branchNode->condition);
                    result += to_string(branchNode->statement);
                    if (branchNode->else_statement != nullptr) {
                        result += to_string(branchNode->else_statement);
                    }
                    break;
                }
                case CONDITION: {
                    auto *conditionNode = static_cast<ConditionNode *>(root);
                    result += "ConditionNode\n";
                    result += to_string(conditionNode->expr);
                    break;
                }
                case EXPR: {
                    auto *exprNode = static_cast<ExprNode *>(root);
                    result += "ExprNode\n";
                    if (exprNode->expr != nullptr) {
                        result += to_string(exprNode->expr);
                    }
                    break;
                }
                case NUMBER: {
                    auto *numberNode = static_cast<NumberNode *>(root);
                    result += "NumberNode: " + std::to_string(numberNode->value) + "\n";
                    break;
                }
                case BIN_OP: {
                    auto *binOpNode = static_cast<BinOpNode *>(root);
                    result += "BinOpNode\n";
                    result += to_string(binOpNode->lhs);
                    result += to_string(binOpNode->rhs);
                    break;
                }
                case FUNC_CALL: {
                    auto *funcCallNode = static_cast<FuncCallNode *>(root);
                    result += "FuncCallNode\n";
                    result += to_string(funcCallNode->identifier);
                    result += to_string(funcCallNode->args);
                    break;
                }
                case ARGS: {
                    auto *argsNode = static_cast<ArgsNode *>(root);
                    result += "ArgsNode\n";
                    for (auto &arg : argsNode->args) {
                        result += to_string(arg);
                    }
                    break;
                }
                case SYS_CALL: {
                    auto *sysCallNode = static_cast<SysCallNode *>(root);
                    result += "SysCallNode\n";
                    result += to_string(sysCallNode->args);
                    break;
                }
            }
            return result;
        }

        ParamsNode* getParams(std::queue<lexer::Token>& tokens) {
            std::vector<IdentifierNode*> params;

            if (tokens.front().value == ")") {
                return new ParamsNode(params);
            }

            auto token = tokens.front();
            tokens.pop(); // consume identifier
            auto *identifierNode = new IdentifierNode(token.value);
            params.push_back(identifierNode);

            while (!tokens.empty() && tokens.front().value == ",") {
                tokens.pop(); // consume ","

                token = tokens.front();
                tokens.pop(); // consume identifier

                identifierNode = new IdentifierNode(token.value);
                params.push_back(identifierNode);
            }
            return new ParamsNode(params);
        }

        ScopeNode* getScope(std::queue<lexer::Token>& tokens) {
            std::vector<StatementNode*> statements;

            tokens.pop(); // consume "{"

            while (!tokens.empty() && tokens.front().value != "}") {
                auto statement = getStatement(tokens);
                statements.push_back(statement);
            }

            tokens.pop(); // consume "}"

            return new ScopeNode(statements);
        }

        StatementNode* getStatement(std::queue<lexer::Token>& tokens) {
            StatementNode *statementNode;

            switch (tokens.front().type) {
                case lexer::TOKEN_KEYWORD: {
                    auto token = tokens.front();
                    tokens.pop(); // consume keyword

                    if (token.value == "return") {
                        token = tokens.front();

                        if (token.value == ";") {
                            statementNode = new ReturnNode(nullptr);
                        } else {
                            auto expr = getExpr(tokens);
                            statementNode = new ReturnNode(expr);
                        }

                        tokens.pop(); // consume ";"

                    } else if (token.value == "if") {

                        tokens.pop(); // consume "("

                        auto expr = getExpr(tokens);

                        tokens.pop(); // consume ")"

                        auto statement = getStatement(tokens);

                        if (tokens.front().value == "else") {
                            tokens.pop(); // consume "else"

                            auto else_statement = getStatement(tokens);

                            statementNode = new BranchNode(new ConditionNode(expr), statement, else_statement);
                        } else {
                            return new BranchNode(new ConditionNode(expr), statement, nullptr);
                        }
                    } else {
                        std::cerr << "Invalid keyword: " << lexer::to_string(token) << std::endl;
                        statementNode = nullptr;
                    }
                    break;
                }
                case lexer::TOKEN_DELIMITER: {
                    return getScope(tokens);
                }
                default: {
                    auto expr = getExpr(tokens);
                    statementNode = new ExprNode(expr);
                    tokens.pop(); // consume ";"
                    break;
                }
            }

            return statementNode;
        }

        BinOpType stringToOp(const std::string& op) {
            if (op == "+") {
                return ADD;
            } else if (op == "-") {
                return SUB;
            } else if (op == "*") {
                return MUL;
            } else if (op == "<") {
                return LT;
            } else if (op == ">") {
                return GT;
            } else if (op == "<=") {
                return LE;
            } else if (op == ">=") {
                return GE;
            } else if (op == "==") {
                return EQ;
            } else if (op == "!=") {
                return NE;
            } else {
                return ASS;
            }
        }

        ArgsNode* getArgs(std::queue<lexer::Token>& tokens) {
            std::vector<ExprNode*> args;

            if (tokens.front().value == ")") {
                return new ArgsNode(args);
            }

            auto exprNode = getExpr(tokens);

            args.push_back(exprNode);

            while (!tokens.empty() && tokens.front().value == ",") {
                tokens.pop(); // consume ","

                exprNode = getExpr(tokens);

                args.push_back(exprNode);
            }
            return new ArgsNode(args);
        }

        ExprNode* getExpr(std::queue<lexer::Token>& tokens) {
            ExprNode *exprNode;

            auto token = tokens.front();
            tokens.pop(); // consume token

            switch (token.type) {
                case lexer::TOKEN_DELIMITER: {
                    if (token.value == "(") {
                        auto expr = getExpr(tokens);

                        tokens.pop(); // consume ")"

                        exprNode = new ExprNode(expr);
                    } else {
                        std::cerr << "Invalid delimiter: " << lexer::to_string(token) << std::endl;
                        exprNode = nullptr;
                    }
                    break;
                }
                case lexer::TOKEN_IDENTIFIER: {
                    auto identifier = new IdentifierNode(token.value);

                    if (tokens.front().value == "(") {
                        tokens.pop(); // consume "("

                        auto args = getArgs(tokens);

                        tokens.pop(); // consume ")"

                        exprNode = new FuncCallNode(identifier, args);
                    } else {
                        auto op = tokens.front();
                        if (op.type == lexer::TOKEN_OPERATOR) {
                            tokens.pop(); // consume operator

                            auto rhs = getExpr(tokens);

                            exprNode = new BinOpNode(identifier, rhs, stringToOp(op.value));
                        } else {
                            exprNode = identifier;
                        }
                    }
                    break;
                }
                case lexer::TOKEN_NUMBER: {
                    auto number = new NumberNode(std::stoull(token.value));

                    auto op = tokens.front();
                    if (op.type == lexer::TOKEN_OPERATOR) {
                        tokens.pop(); // consume operator

                        auto rhs = getExpr(tokens);

                        exprNode = new BinOpNode(number, rhs, stringToOp(op.value));
                    } else {
                        exprNode = number;
                    }

                    break;
                }
                case lexer::TOKEN_SYS_CALL: {
                    SysCall_Type syscall;
                    if (token.value == "open") {
                        syscall = OPEN;
                    } else if (token.value == "write") {
                        syscall = WRITE;
                    } else if (token.value == "read") {
                        syscall = READ;
                    } else if (token.value == "ioctl") {
                        syscall = IOCTL;
                    } else {
                        std::cerr << "Invalid syscall: " << lexer::to_string(token) << std::endl;
                        exprNode = nullptr;
                        break;
                    }

                    tokens.pop(); // consume "("

                    auto args = getArgs(tokens);

                    tokens.pop(); // consume ")"

                    exprNode = new SysCallNode(syscall, args);
                    break;
                }
                default: {
                    std::cerr << "Invalid token: " << lexer::to_string(token) << std::endl;
                    exprNode = nullptr;
                    break;
                }
            }

            return exprNode;
        }

        ProgramNode *generateAst(std::queue<lexer::Token>& tokens) {

            // for every token: check type and continue parsing depending on type
            // while not EOF:
            //
            // switch:
            //
            // case (function definition): TOKEN_IDENTIFIER + TOKEN_DELIMITER + recursive(params) + TOKEN_DELIMITER + recursive(scope)
            //
            // case (privileged object definition): TOKEN_PRIV_DELIM + TOKEN_DELIMITER + TOKEN_IDENTIFIER + TOKEN_DELIMITER + TOKEN_NUMBER + TOKEN_DELIMITER
            //
            // helper functions:
            // params(): identifier + while(TOKEN_DELIMITER + identifier)
            // scope(): TOKEN_DELIMITER + while not (TOKEN_DELIMITER) {recursive(statement)}
            // statement(): switch:
            // case (return): TOKEN_KEYWORD + if (TOKEN_DELIMITER) return else {recursive(expr) TOKEN_DELIMITER}
            // case (branch): TOKEN_KEYWORD + TOKEN_DELIMITER + recursive(expr) + TOKEN_DELIMITER + recursive(statement) + if (TOKEN_KEYWORD) {recursive(statement)}
            // case (expr): recursive(expr) + TOKEN_DELIMITER
            // expr(): switch:
            // case (parenthesis): TOKEN_DELIMITER + recursive(expr) + TOKEN_DELIMITER
            // case (function call): (SYS_CALL or TOKEN_IDENTIFIER) + TOKEN_DELIMITER + recursive(args) + TOKEN_DELIMITER
            // case token_identifier: (TOKEN_IDENTIFIER or TOKEN_NUMBER) + if (TOKEN_OPERATOR) {recursive (expression)}
                // todo: how to determine if now comes a nested expr or not
            // args(): recursive(expr) + while (TOKEN_DELIMITER + recursive(expr))

            std::vector<PrivObjNode*> privObjNodes;
            std::vector<FuncDefNode*> funcDefNodes;
            auto *root = new ProgramNode(privObjNodes, funcDefNodes);

            while (!tokens.empty()) {
                auto token = tokens.front();
                tokens.pop();
                switch (token.type) {
                    case lexer::TOKEN_PRIV_DELIM: {
                        tokens.pop(); // skip "("

                        const auto identifier = tokens.front().value; // name of variable
                        tokens.pop(); // consume identifier

                        tokens.pop(); // skip ","

                        const uint16_t address = std::stoi((tokens.front().value)); // address of variable cast to uint16_t
                        tokens.pop(); // skip address

                        auto *privObjNode = new PrivObjNode(new IdentifierNode(identifier), new AddressNode(address));
                        root->privObjNodes.push_back(privObjNode);

                        tokens.pop(); // skip ")"

                        break;
                    }
                    case lexer::TOKEN_IDENTIFIER: {
                        auto *identifierNode = new IdentifierNode(token.value);

                        tokens.pop(); // skip "("

                        // consume params
                        ParamsNode* params = getParams(tokens);

                        tokens.pop(); // skip ")"

                        // consume scope
                        ScopeNode *scope = getScope(tokens);

                        auto *funcDefNode = new FuncDefNode(identifierNode, params, scope);
                        root->funcDefNodes.push_back(funcDefNode);
                        break;
                    }
                    default: {
                        std::cerr << "Invalid token type: " << lexer::to_string(token) << std::endl;
                        break;
                    }
                }
            }

            return root;
        }
        
    };

#endif //PARSER_H
