#ifndef TRANSPILER_H
#define TRANSPILER_H
#include <unordered_map>
#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>

#include "parser.h"

// Valid instructions:
// exit
// add <reg_in_1> <reg_in_2> <reg_out>
// sub <reg_in_1> <reg_in_2> <reg_out>
// mul <reg_in_1> <reg_in_2> <reg_out>
// load <reg_mem_addr> <reg_val>
// store <reg_mem_addr> <reg_val>
// request <reg_mem_addr> <reg_num_cycles>
// li <reg> <imm>
// jmpEqZ <reg_test> <reg_next_instr>
// syscall <reg_syscall_num>
// cmpGT <reg_1> <reg_2> <reg_out>

// TODO: dynamically choose stack placement based on privileged data locations
static const int START_OF_STACK = 9216;
static const int RBP = 7; // base pointer is in register 7
static const int RSP = 6; // stack pointer is in register 6
static const std::string PRIV_PREFIX = "privileged-";
static const std::string LOAD_CYCLES = "30";
static const std::string STORE_CYCLES = "20";
static const uint16_t NUMBER_REGISTERS = 8;

class transpiler {

    std::unordered_map<std::string, bool> privilegedObjects; // maps an identifier to a boolean value that says whether it is priveleged or not
    std::array<bool, NUMBER_REGISTERS> occupiedRegister = {false}; // says whether register i is used currently
    std::unordered_map<std::string, std::string> registers; // maps identifier to registers for non privileged data
    std::unordered_map<std::string, std::string> privilegedAddresses; // maps identifier to address for privileged data

    std::string push_registers(std::array<bool, NUMBER_REGISTERS> occupiedRegister) {
        std::string output_string;
        // push the registers to the stack
        // put 1 into free register to increment RSP
        std::string free_register = get_free_register();
        output_string += "li " + free_register + " 1\n";
        for (int i = 1; i < NUMBER_REGISTERS; i++) {
            if (occupiedRegister[i]) {
                output_string += "store " + std::to_string(RSP) + " " + std::to_string(i) + "\n";
                // increment the stack pointer
                output_string += "add " + std::to_string(RSP) + " " + free_register + " " + std::to_string(RSP) + "\n";
                // increment the base pointer
                // TODO: if many registers used
                output_string += "add " + std::to_string(RBP) + " " + free_register + " " + std::to_string(RBP) + "\n";
            }
        }
        // push the stack pointer
        output_string += "store " + std::to_string(RSP) + " " + std::to_string(RSP) + "\n";
        // increment the stack pointer
        output_string += "add " + std::to_string(RSP) + " " + free_register + " " + std::to_string(RSP) + "\n";
        // push the base pointer
        output_string += "store " + std::to_string(RSP) + " " + std::to_string(RBP) + "\n";
        // set the base pointer to the stack pointer
        output_string += "mul " + std::to_string(RSP) + " " + free_register + " " + std::to_string(RSP) + "\n";
        return output_string;
    }
    std::string pop_registers(std::array<bool, NUMBER_REGISTERS> occupiedRegister, std::string label) {
        std::string output_string;
        // pop the registers to the stack
        // put 1 into free register to subtract RSP
        std::string free_register = get_free_register();
        output_string += "li " + free_register + " 1\n";
        for (int i = NUMBER_REGISTERS - 1; i >= 1; i--) {
            if (occupiedRegister[i]) {
                output_string += "load " + std::to_string(RSP) + " " + std::to_string(i) + "\n";
                // subtract the stack pointer
                output_string += "sub " + std::to_string(RSP) + " " + free_register + " " + std::to_string(RSP) + "\n";
                // increment the base pointer
                // TODO: if many registers used
                output_string += "sub " + std::to_string(RBP) + " " + free_register + " " + std::to_string(RBP) + "\n";
            }
        }
        // pop the stack pointer
        output_string += "store " + std::to_string(RSP) + " " + std::to_string(RSP) + "\n";
        // decrement the stack pointer
        output_string += "sub " + std::to_string(RSP) + " " + free_register + " " + std::to_string(RSP) + "\n";
        // push the base pointer
        output_string += "load " + std::to_string(RSP) + " " + std::to_string(RBP) + "\n";
        // set the base pointer to the stack pointer
        // output_string += "mul " + std::to_string(RSP) + " " + std::to_string(free_register) + " " + std::to_string(RSP) + "\n";
        return output_string;
    }

    // TODO: deal with no free registers
    std::string get_free_register() {
        for (int i = NUMBER_REGISTERS - 1; i >= 0; i--) {
            if (occupiedRegister[i] == false) {
                return std::to_string(i);
            }
        }
        printf("Error: no free registers\n");
        return "Error";
    }

    std::string transpile_func_call(parser::FuncCallNode* funcCall, std::string& output_string) {
        // get the function name
        std::string funcName = funcCall->identifier->value;
        // get the arguments
        std::vector<parser::ExprNode*> args = funcCall->args->args;

        // push the registers to the stack
        output_string += push_registers(occupiedRegister);
        std::string pop_instructions = pop_registers(occupiedRegister, funcName);
        for (int i = 0; i < funcCall->args->args.size(); i++) {
            parser::ExprNode* node = funcCall->args->args.at(i);
            auto result_register = transpile_expr(node, output_string);
            output_string += "li " + std::to_string(i + 2) + " 0\n";
            output_string += "add " + std::to_string(i + 2) + result_register+ std::to_string(i + 2) + "\n";
            occupiedRegister[std::stoi(result_register)] = false;
            occupiedRegister[i + 2] = true;
        }
        // load 0  into register 0 for jump zero
        output_string += "li 0 0\n";
        // put label into register 1
        output_string += "li 1 " + funcName + "\n";
        // jump to label
        output_string += "jmpEqZ 0 1\n";
        // // return to this point
        output_string += pop_instructions;
        return "0";
    }

    std::string transpile_expr(parser::ExprNode* expr, std::string& output_string) {
        switch (expr->type) {
            case parser::EXPR: {
                return transpile_expr(expr->expr, output_string);
            }
            case parser::FUNC_CALL: {
                auto result_register = transpile_func_call(static_cast<parser::FuncCallNode *> (expr), output_string);
                return result_register;
            }
            case parser::SYS_CALL: {
                auto sysCall = static_cast<parser::SysCallNode *> (expr);
                // TODO: push and pop
                std::vector<parser::ExprNode*> args = sysCall->args->args;
                int i = 0;
                for (; i < args.size(); i++) {
                    if (occupiedRegister[i]) {
                        auto val = registers.find(static_cast<parser::IdentifierNode*> (args[i])->value)->second = std::to_string(i);
                        auto free_register = get_free_register();
                        registers[val] = free_register;
                        output_string += "li " + free_register + " 0\n";
                        output_string += "add " + free_register + " " + std::to_string(i) + " " + free_register + "\n";
                    } else {
                        occupiedRegister[i] = true;
                    }
                        output_string += "li " + std::to_string(i) + " 0\n";
                    output_string += "add " + std::to_string(i) + " " + transpile_expr(args[i], output_string) + " " + std::to_string(i) + "\n";
                }
                std::string syscall_number;
                auto syscall_num_reg = get_free_register();
                switch (sysCall->syscall) {
                    case parser::OPEN: {
                        syscall_number = "0";
                        output_string += "li " + syscall_num_reg + " " + syscall_number + "\n";
                        occupiedRegister[std::stoi(syscall_num_reg)] = true;
                        break;
                    }
                    case parser::WRITE: {
                        // write to file
                        syscall_number = "1";
                        auto free_register = get_free_register();
                        output_string += "li " + free_register + " 0\n";
                        occupiedRegister[std::stoi(free_register)] = true;
                        break;
                    }

                    case parser::READ: {
                        // write to file
                        syscall_number = "2";
                        auto free_register = get_free_register();
                        output_string += "li " + free_register + " 0\n";
                        occupiedRegister[std::stoi(free_register)] = true;
                        break;
                    }
                    case parser::IOCTL: {
                        // write to file
                        syscall_number = "3";
                        auto free_register = get_free_register();
                        output_string += "li " + free_register + " 0\n";
                        occupiedRegister[std::stoi(free_register)] = true;
                        break;
                    }
                }
                output_string += "syscall " + syscall_number;
                return "0";
            }
            case parser::NUMBER: {
                auto free_register = get_free_register();
                auto number = static_cast<parser::NumberNode *> (expr);
                output_string += "li " + free_register + " " + std::to_string(number->value) + "\n";
                occupiedRegister[std::stoi(free_register)] = true;
                return free_register;
            }
            case parser::IDENTIFIER: {
                auto value = static_cast<parser::IdentifierNode *> (expr)->value;
                if (privilegedObjects[value]) {
                    return PRIV_PREFIX + privilegedAddresses[value];
                }
                auto value_register = registers[value];
                if (value_register.empty()) {
                    value_register = get_free_register();
                    occupiedRegister[std::stoi(value_register)] = true;
                    registers[value] = value_register;
                }
                return value_register;
            }
            case parser::BIN_OP: {
                parser::BinOpNode *binOpNode = static_cast<parser::BinOpNode *> (expr);
                parser::ExprNode* lhs = binOpNode->lhs;
                parser::ExprNode* rhs = binOpNode->rhs;
                auto lhs_register = transpile_expr(lhs, output_string);
                auto rhs_register = transpile_expr(rhs, output_string);
                switch (binOpNode->op) {
                    case parser::ASS: {
                        // acquire write access
                        if (lhs_register.starts_with(PRIV_PREFIX)) {
                            // also acquire read access
                            if (rhs_register.starts_with(PRIV_PREFIX)) {
                                // store address of rhs privileged data in free register
                                std::string free_register_rhs = get_free_register();
                                // mark local address register as occupied
                                occupiedRegister[std::stoi(free_register_rhs)] = true;
                                output_string += "li " + free_register_rhs + " " +  rhs_register.substr(PRIV_PREFIX.length())+ "\n";
                                // store number of cycles in another free register
                                std::string free_register_cycles = get_free_register();
                                // mark cycle register as occupied
                                occupiedRegister[std::stoi(free_register_cycles)] = true;
                                output_string += "li " + free_register_cycles + " " + LOAD_CYCLES + "\n";
                                // request access for rhs
                                output_string += "request " + free_register_rhs + " " + free_register_cycles + "\n";
                                // load privileged data into the first free register (re-use of register)
                                output_string += "load " + free_register_rhs + free_register_rhs + "\n";

                                // store address of lhs privileged data in free register
                                std::string free_register_lhs = get_free_register();
                                // free the cycles register
                                occupiedRegister[std::stoi(free_register_cycles)] = false;
                                output_string += "li " + free_register_lhs + " " +  lhs_register.substr(PRIV_PREFIX.length()) + "\n";
                                // store number of cycles in second free register
                                output_string += "li " + free_register_cycles + " " + STORE_CYCLES + "\n";
                                // request access for lhs
                                output_string += "request " + free_register_lhs + " " + free_register_cycles + "\n";
                                // store rhs in lhs
                                output_string += "store " + free_register_lhs + " " + free_register_rhs + "\n";
                                registers[static_cast<parser::IdentifierNode*> (lhs)->value] = free_register_rhs;
                                return free_register_rhs;
                            } else {
                                // store address of lhs privileged data in free register
                                std::string free_register_lhs = get_free_register();
                                occupiedRegister[std::stoi(free_register_lhs)] = true;
                                output_string += "li " + free_register_lhs + " " + lhs_register.substr(PRIV_PREFIX.length()) + "\n";
                                // store number of cycles in second free register
                                std::string free_register_cycles = get_free_register();
                                occupiedRegister[std::stoi(free_register_lhs)] = false;
                                output_string += "li " + free_register_cycles + " " + STORE_CYCLES + "\n";
                                // request access for lhs
                                output_string += "request " + free_register_lhs + " " + free_register_cycles + "\n";
                                // store rhs in lhs
                                output_string += "store " + free_register_lhs + " " + rhs_register + "\n";
                                // mark local copy as occupied
                                registers[static_cast<parser::IdentifierNode*> (lhs)->value] = rhs_register;
                                return rhs_register;
                            }
                        }
                        if (rhs_register.starts_with(PRIV_PREFIX)) {
                            // store address of rhs privileged data in free register
                            std::string free_register_rhs = get_free_register();
                            output_string += "li " + free_register_rhs + " " +  rhs_register.substr(PRIV_PREFIX.length())+ "\n";
                            // store number of cycles in another free register
                            std::string free_register_cycles = get_free_register();
                            output_string += "li " + free_register_cycles + " " + LOAD_CYCLES + "\n";
                            // request access for rhs
                            output_string += "request " + free_register_rhs + " " + free_register_cycles + "\n";
                            // load privileged data into the first free register (re-use of register)
                            output_string += "load " + free_register_rhs + lhs_register + "\n";
                            return lhs_register;
                        } else {
                            output_string += "li " + lhs_register + " 0\n";
                            output_string += "add " + lhs_register + " " + rhs_register + " " + lhs_register + "\n";
                            occupiedRegister[std::stoi(rhs_register)] = false;
                            return lhs_register;
                        }
                    }
                    default: {
                        auto left_reg = lhs_register;
                        auto right_reg = rhs_register;
                        if (lhs_register.starts_with(PRIV_PREFIX)) {
                            // get register for load address
                            std::string free_register_lhs = get_free_register();
                            occupiedRegister[std::stoi(free_register_lhs)] = true;
                            output_string += "li " + free_register_lhs + " " +  lhs_register.substr(PRIV_PREFIX.length()) + "\n";
                            // get register with cycles
                            std::string cycles_register = get_free_register();
                            occupiedRegister[std::stoi(free_register_lhs)] = false;
                            output_string += "li " + cycles_register + " " + LOAD_CYCLES + "\n";
                            // request access for lhs
                            output_string += "request " + free_register_lhs + " " + cycles_register + "\n";
                            // replace the address in lhs with the value
                            output_string += "load " + free_register_lhs + " " + free_register_lhs + "\n";
                            // mark local copy as occupied
                            registers[static_cast<parser::IdentifierNode*> (lhs)->value] = free_register_lhs;
                            left_reg = free_register_lhs;
                        } // else value is in lhs_register
                        if (rhs_register.starts_with(PRIV_PREFIX)) {
                            // get register for load address
                            std::string free_register_rhs = get_free_register();
                            occupiedRegister[std::stoi(free_register_rhs)] = true;
                            output_string += "li " + free_register_rhs + " " +  rhs_register.substr(PRIV_PREFIX.length()) + "\n";
                            // get register with cycles
                            std::string cycles_register = get_free_register();
                            occupiedRegister[std::stoi(free_register_rhs)] = false;
                            output_string += "li " + cycles_register + " " + LOAD_CYCLES + "\n";
                            // request access for rhs
                            output_string += "request " + free_register_rhs + " " + cycles_register + "\n";
                            // replace the address in rhs with the value
                            output_string += "load " + free_register_rhs + " " + free_register_rhs + "\n";
                            // mark local copy as occupied
                            registers[static_cast<parser::IdentifierNode*> (rhs)->value] = free_register_rhs;
                            right_reg = free_register_rhs;
                        }
                        switch (binOpNode->op) {
                            case parser::ADD: {
                                // add the two values
                                auto free_register = get_free_register();
                                output_string += "add " + left_reg + " " + right_reg + " " + free_register + "\n";
                                occupiedRegister[std::stoi(free_register)] = true;
                                return free_register;
                            }
                            case parser::SUB: {
                                // sub the two values
                                auto free_register = get_free_register();
                                output_string += "sub " + left_reg + " " + right_reg + " " + free_register + "\n";
                                occupiedRegister[std::stoi(free_register)] = true;
                                return free_register;
                            }
                            case parser::MUL: {
                                // mul the two values
                                auto free_register = get_free_register();
                                output_string += "mul " + left_reg + " " + right_reg + " " + free_register + "\n";
                                occupiedRegister[std::stoi(free_register)] = true;
                                return free_register;
                            }
                            case parser::LT: {
                                // compare if right_reg > left_reg
                                auto free_register = get_free_register();
                                output_string += "cmpGT " + right_reg + " " + left_reg + " " + free_register + "\n";
                                occupiedRegister[std::stoi(free_register)] = true;
                                return free_register;
                            }
                            case parser::GT: {
                                // compare if left_reg > right_reg
                                auto free_register = get_free_register();
                                output_string += "cmpGT " + left_reg + " " + right_reg + " " + free_register + "\n";
                                occupiedRegister[std::stoi(free_register)] = true;
                                return free_register;
                            }
                            case parser::LE: {
                                // compare if not (right_reg < left_reg)
                                auto free_register = get_free_register();
                                occupiedRegister[std::stoi(free_register)] = true;
                                output_string += "cmpGT " + left_reg + " " + right_reg + " " + free_register + "\n";
                                auto free_register_one = get_free_register();
                                output_string += "li " + free_register_one + " 1\n";
                                output_string += "sub " + free_register + " " + free_register_one + " " + free_register + "\n";
                                return free_register;
                            }
                            case parser::GE: {
                                // compare if not (left_reg < right_reg)
                                auto free_register = get_free_register();
                                occupiedRegister[std::stoi(free_register)] = true;
                                output_string += "cmpGT " + right_reg + " " + left_reg + " " + free_register + "\n";
                                auto free_register_one = get_free_register();
                                output_string += "li " + free_register_one + " 1\n";
                                output_string += "sub " + free_register + " " + free_register_one + " " + free_register + "\n";
                                return free_register;
                            }
                            case parser::EQ: {
                                // subtract the two values, if they are zero (0 < 1 == true), else false
                                auto free_register = get_free_register();
                                output_string += "sub " + left_reg + " " + right_reg + " " + free_register + "\n";
                                occupiedRegister[std::stoi(free_register)] = true;
                                auto free_register_one = get_free_register();
                                output_string += "li " + free_register_one + " 1\n";
                                output_string += "cmpGT " + free_register_one + " " + free_register + " " + free_register + "\n";
                                return free_register;
                            }
                            case parser::NE: {
                                // subtract the two values, if they are not zero
                                auto free_register = get_free_register();
                                output_string += "sub " + left_reg + " " + right_reg + " " + free_register + "\n";
                                occupiedRegister[std::stoi(free_register)] = true;
                                return free_register;
                            }
                        }
                    }
                }
            } default: {
                return "Error: Expression type was not found";
            }

        }
    }

    std::string transpile_return(parser::ReturnNode* returnNode, std::string& output_string) {
        if (returnNode->expr) {
            // transpile the expression
            // TODO: return value
            std::string result_register = transpile_expr(returnNode->expr, output_string);
            // move the result to register 0
            output_string += "li 0 0\n";
            output_string += "add " + result_register + " 0 0\n";
            // move 0 to return register 1 for jump zero
            output_string += "li 1 0\n";
            // load 1 into register 2 for jump zero
            //output_string += "li 2 1\n";
            // move the return address from RBP - 1 to register 2
            //output_string += "sub " + std::to_string(RBP) + " 2 2\n";
            // jump to return address
            //output_string += "jmpEqZ 1 2\n";
            // fix out internal state
            occupiedRegister[stoi(result_register)] = false;
            occupiedRegister[0] = true;
        }
        output_string += "exit\n";
        return "0";
    }

    void transpile_branch(parser::BranchNode* branch, std::string& output_string) {
        // transpile the condition and get the register with the resulting value
        std::string reg = transpile_expr(branch->condition->expr, output_string);


        auto free_register_label = get_free_register();
        output_string += "li " + free_register_label + " ELSE_LABEL\n";
        output_string += "jmpEqZ " + reg + " " + free_register_label + " \n";

        switch (branch->statement->type) {
            case parser::RETURN: {
                transpile_return(static_cast<parser::ReturnNode *>(branch->statement), output_string);
                break;
            }
            case parser::SCOPE: {
                transpile_scope(static_cast<parser::ScopeNode *>(branch->statement), output_string);
                break;
            }

            case parser::BRANCH: {
                transpile_branch(static_cast<parser::BranchNode *>(branch->statement), output_string);
                break;
            }
            case parser::EXPR: case parser::CONDITION: {
                transpile_expr(static_cast<parser::ExprNode *>(branch->statement), output_string);
                break;

            }
            default: {
                printf("Error: unknown statement type\n");
                break;
            }
        }

        auto free_register = get_free_register();
        occupiedRegister[std::stoi(free_register)] = true;
        free_register_label = get_free_register();
        occupiedRegister[std::stoi(free_register)] = false;
        output_string += "li " + free_register + " 0\n";
        output_string += "li " + free_register_label + " END_LABEL\n";
        output_string += "jmpEqZ " + free_register + " " + free_register_label + " \n";
        output_string += "ELSE_LABEL:"; // no new_line

        if (branch->else_statement) {
            switch (branch->else_statement->type) {
                case parser::RETURN: {
                    transpile_return(static_cast<parser::ReturnNode *>(branch->else_statement), output_string);
                    break;
                }
                case parser::SCOPE: {
                    transpile_scope(static_cast<parser::ScopeNode *>(branch->else_statement), output_string);
                    break;
                }
                case parser::BRANCH: {
                    transpile_branch(static_cast<parser::BranchNode *>(branch->else_statement), output_string);
                    break;
                }
                case parser::EXPR: case parser::CONDITION: {
                    transpile_expr(static_cast<parser::ExprNode *>(branch->else_statement), output_string);
                    break;

                }
                default: {
                    printf("Error: unknown else_statement type\n");
                    break;
                }
            }
        }
        output_string += "END_LABEL:"; // no new_line

        occupiedRegister[std::stoi(free_register)] = true;
    }

    // TODO: scoping (push/pop registers)
    void transpile_scope(parser::ScopeNode* scope, std::string& output_string) {
        for (parser::StatementNode *statement : scope->statements) {
            switch (statement->type) {
                case parser::SCOPE: {
                    transpile_scope(static_cast<parser::ScopeNode *>(statement), output_string);
                    break;
                }
                case parser::RETURN: {
                    // transpile the return statement
                    transpile_return(static_cast<parser::ReturnNode *>(statement), output_string);
                    break;
                }
                case parser::BRANCH: {
                    // transpile the branch statement
                    transpile_branch(static_cast<parser::BranchNode *>(statement), output_string);
                    break;
                }
                case parser::EXPR: {
                    // transpile the expression
                    transpile_expr(static_cast<parser::ExprNode *>(statement), output_string);
                    break;
                }
                default: {
                    printf("Error: unknown statement type\n");
                    break;
                }
            }


        }
    }

    void replace_jump_labels2(std::string& output_string) {
        // iterate through all lines (\n) and check if at the beginning there is [a-zA-Z0-9_]+:
        std::unordered_map<std::string, uint> labels;
        std::istringstream stream(output_string);
        std::string line;
        uint line_number = 1;

        // Iterate through all lines
        while (std::getline(stream, line)) {
            // Check if the line starts with a label (e.g., "label:")
            std::smatch match;
            if (std::regex_search(line, match, std::regex("^([a-zA-Z0-9_]+):"))) {
                labels[match[1].str()] = line_number;
            }
            line_number++;
        }

        // Replace labels in the output_string
        for (const auto& label : labels) {
            std::string label_str = label.first + ":";
            size_t pos = 0;
            while ((pos = output_string.find(label_str, pos)) != std::string::npos) {
                output_string.replace(pos, label_str.length(), "");
                pos += std::to_string(label.second).length();
            }
        }
    }

    void replace_jump_labels(std::string& output_string) {
        // iterate through all lines (\n) and check if at the beginning there is [a-zA-Z0-9_]+:
        std::unordered_map<std::string, uint> labels;
        std::istringstream stream(output_string);
        std::string line;
        uint line_number = 1;

        // Iterate through all lines
        while (std::getline(stream, line)) {
            // Check if the line starts with a label (e.g., "label:")
            std::smatch match;
            if (std::regex_search(line, match, std::regex("^([a-zA-Z0-9_]+):"))) {
                labels[match[1].str()] = line_number;
            }
            line_number++;
        }

        // Replace labels in the output_string
        for (const auto& label : labels) {
            std::string label_str = label.first + ":";
            size_t pos = 0;
            while ((pos = output_string.find(label_str, pos)) != std::string::npos) {
                output_string.replace(pos, label_str.length(), "");
                pos += std::to_string(label.second).length();
            }

            // Replace labels in jmpEqZ instructions
            std::regex jmp_regex("li\\s+(\\d+)\\s+" + label.first);
            output_string = std::regex_replace(output_string, jmp_regex, "li $1 " + std::to_string(label.second));
        }
    }

public:
    void transpile(const char *outFile, parser::ProgramNode *root) {
        // first determine the privileged objects and their addresses
        for (auto &privObjNode : root->privObjNodes) {
            privilegedObjects[privObjNode->identifier->value] = true;
            privilegedAddresses[privObjNode->identifier->value] = std::to_string(privObjNode->address->value);
        }

        // TODO: start at main, then dynamically transpile necessary functions

        std::string output_string;

        // // start with main by jumping there
        // output_string += "li 0 0\n";
        // output_string += "li 1 main\n";
        // output_string += "jmpEqZ 0 1\n";

        // then transpile the functions to intermediate code using labels
        for (auto &funcDefNode : root->funcDefNodes) {
            std::string funcName = funcDefNode->identifier->value;
            // function label
            output_string += funcName + ":"; // no new-line

            // set RBP and RSP in main
            if (funcName == "main") {
                 output_string += "li " + std::to_string(RBP) + " " + std::to_string(START_OF_STACK) + "\n";
                 output_string += "li " + std::to_string(RSP) + " " + std::to_string(START_OF_STACK) + "\n";
            }

            // reset our register table
            occupiedRegister = {false};
            registers.clear();
            // // RSP and RBP are always occupied
             occupiedRegister[7] = true;
             occupiedRegister[6] = true;
            // get parameters into registers
            // We will simply use 0 to 7 in increasing order
            // If more than NUMBER_REGISTERS registers are needed, the transpilation fails
            int num_param = 0;
            for (auto parameter : funcDefNode->params->params) {
                if (num_param >= NUMBER_REGISTERS) {
                    printf("Error: too many parameters\n");
                    return;
                }
                registers[parameter->value] = num_param;
                occupiedRegister[num_param] = true;
                num_param++;
            }

            // transpile the scope
            parser::ScopeNode * scope = funcDefNode->scope;
            transpile_scope(scope, output_string);
        }

        // TODO: insert permissions

        replace_jump_labels(output_string);

        // write output to file
        std::ofstream out(outFile);
        out << output_string;
        out.close();
    }
};

#endif //TRANSPILER_H
