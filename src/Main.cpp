/**
 * @file Main.cpp
 * @brief This file contains entry point of interpreter
 * Also, it is the point which uses interpreter
 *
 */

#include "Interpreter.hpp"
#include "Opcodes.hpp"
#include "Types.hpp"
#include "Utils.hpp"

#include <cassert>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

auto interpretOne(Bytefile& bytefile, Interpreter& interpreter) -> InterpretResult {
#define checkEnoughBytes(bytes)                                                \
    if (!bytefile.enoughBytes(bytes)) {                                        \
        std::cerr << "Bytecode could not read next to" << (bytes) << " bytes"; \
        return InterpretResult::ERROR;                                         \
    }

#define trySetAddr(action, address)                                                                        \
    if (!bytefile.trySetAddress(address)) {                                                                \
        std::cerr << "Cannot " action " to address 0x" << std::hex << address << " -- outside bytecode\n"; \
        return InterpretResult::ERROR;                                                                     \
    }

    checkEnoughBytes(1);

    u8 code = bytefile.getNextCode();
    u8 low  = (code & 0x0F);

    switch (static_cast<Opcodes>(code)) {
    case Opcodes::BINOP_add:
    case Opcodes::BINOP_sub:
    case Opcodes::BINOP_mul:
    case Opcodes::BINOP_div:
    case Opcodes::BINOP_rem:
    case Opcodes::BINOP_lt:
    case Opcodes::BINOP_le:
    case Opcodes::BINOP_gt:
    case Opcodes::BINOP_ge:
    case Opcodes::BINOP_eq:
    case Opcodes::BINOP_ne:
    case Opcodes::BINOP_and:
    case Opcodes::BINOP_or: {
        // SAFETY: this cast could happen only if we got
        // into right opcode
        auto operation = static_cast<BinOp>(low);
        return interpreter.onBinOp(operation);
    }
    case Opcodes::CONST: {
        checkEnoughBytes(sizeof(u32));
        auto val = bytefile.getNextInt();
        return interpreter.onConst(val);
    }
    case Opcodes::STRING: {
        auto val = bytefile.getNextString();
        if (!val.has_value()) { return InterpretResult::ERROR; }
        return interpreter.onString(val.value());
    }
    case Opcodes::SEXP: {
        auto tag = bytefile.getNextString();
        if (!tag.has_value()) { return InterpretResult::ERROR; }

        checkEnoughBytes(sizeof(u32));
        auto n = bytefile.getNextUnsigned();

        return interpreter.onSexp(tag.value(), n);
    }
    case Opcodes::STI: {
        std::cerr << "Non-used bytecode STI\n";
        FAIL();
    }
    case Opcodes::STA: {
        return interpreter.onSTA();
    }
    case Opcodes::JMP: {
        checkEnoughBytes(sizeof(u32));
        auto toJump = bytefile.getNextUnsigned();

        auto absoluteAddress = interpreter.onJump(toJump);
        if (!bytefile.trySetAddress(absoluteAddress)) { return InterpretResult::ERROR; }
        return InterpretResult::CONTINUE;
    }
    case Opcodes::END:
    case Opcodes::RET: {
        auto* nextCode = interpreter.onEndOrRet();

        bytefile.ip = nextCode;
        if (!nextCode) { return InterpretResult::STOP; }

        return InterpretResult::CONTINUE;
    }
    case Opcodes::DROP: {
        return interpreter.onDrop();
    }
    case Opcodes::DUP: {
        return interpreter.onDuplicate();
    }
    case Opcodes::SWAP: {
        return interpreter.onSwap();
    }
    case Opcodes::ELEM: {
        return interpreter.onElem();
    }
    case Opcodes::LD_G:
    case Opcodes::LD_L:
    case Opcodes::LD_A:
    case Opcodes::LD_C: {
        checkEnoughBytes(sizeof(u32));
        auto idx = bytefile.getNextUnsigned();
        // SAFETY: this cast could happen only if we got
        // into right opcode
        auto type = static_cast<VariableType>(low);
        return interpreter.onLoad(idx, type);
    }
    case Opcodes::LDA_G:
    case Opcodes::LDA_L:
    case Opcodes::LDA_A:
    case Opcodes::LDA_C: {
        checkEnoughBytes(sizeof(u32));
        auto idx = bytefile.getNextUnsigned();
        // SAFETY: this cast could happen only if we got
        // into right opcode
        auto type = static_cast<VariableType>(low);
        return interpreter.onLoadAccumulator(idx, type);
    }
    case Opcodes::ST_G:
    case Opcodes::ST_L:
    case Opcodes::ST_A:
    case Opcodes::ST_C: {
        checkEnoughBytes(sizeof(u32));
        auto idx = bytefile.getNextUnsigned();
        // SAFETY: this cast could happen only if we got
        // into right opcode
        auto storeType = static_cast<VariableType>(low);
        return interpreter.onStore(idx, storeType);
    }
    case Opcodes::CJMPz:
    case Opcodes::CJMPnz: {
        checkEnoughBytes(sizeof(u32));
        bool isNotEq      = low & 0x01;
        auto jumpLocation = bytefile.getNextUnsigned();
        auto jump         = interpreter.onCondJump(isNotEq, jumpLocation, bytefile.address());
        if (!jump.has_value()) { return InterpretResult::ERROR; }

        trySetAddr("jump", jump.value());
        return InterpretResult::CONTINUE;
    }
    case Opcodes::BEGIN: // Begin and closure begin are the same(?)
    case Opcodes::CBEGIN: {
        checkEnoughBytes(sizeof(u32) * 2);
        // Begin is even, while Cbegin is odd (because it's next instruction)
        bool isCBegin = low & 0x1;
        auto nArgs    = bytefile.getNextUnsigned();
        auto nLocals  = bytefile.getNextUnsigned();
        return interpreter.onBegin(isCBegin, nArgs, nLocals);
    }
    case Opcodes::CLOSURE: {
        checkEnoughBytes(sizeof(u32) * 2);
        auto address = bytefile.getNextUnsigned();
        auto n       = bytefile.getNextUnsigned();
        return interpreter.onClosure(address, bytefile.closureArray(n));
    }
    case Opcodes::CALLC: {
        checkEnoughBytes(sizeof(u32));
        auto nArgs          = bytefile.getNextUnsigned();
        auto closureAddress = interpreter.onCallClosure(bytefile.ip, nArgs);
        if (!closureAddress.has_value()) { return InterpretResult::ERROR; }

        trySetAddr("jump", closureAddress.value());

        auto nextOpcode = bytefile.peekNextCode();

        if (nextOpcode != to_underlying(Opcodes::CBEGIN) && nextOpcode != to_underlying(Opcodes::BEGIN)) {
            std::cerr << "Cannot call closure to address 0x" << std::hex << closureAddress.value()
                      << " -- next opcode is " << toString(Opcodes(nextOpcode)) << " not (C)BEGIN\n";
            return InterpretResult::ERROR;
        }
        return InterpretResult::CONTINUE;
    }
    case Opcodes::CALL: {
        checkEnoughBytes(sizeof(u32) * 2);
        auto location    = bytefile.getNextUnsigned();
        auto nArgs       = bytefile.getNextUnsigned();
        auto callAddress = interpreter.onCall(location, nArgs, bytefile.ip);
        if (!callAddress.has_value()) { return InterpretResult::ERROR; }

        trySetAddr("call", callAddress.value());

        if (bytefile.peekNextCode() != to_underlying(Opcodes::BEGIN)) {
            std::cerr << "Cannot call to address 0x" << std::hex << callAddress.value() << " -- next opcode is "
                      << toString(Opcodes(bytefile.peekNextCode())) << " not BEGIN\n";
            return InterpretResult::ERROR;
        }
        return InterpretResult::CONTINUE;
    }
    case Opcodes::TAG: {
        auto name = bytefile.getNextString();
        if (!name.has_value()) {
            std::cerr << "could not retrieve a string\n";
            return InterpretResult::ERROR;
        }
        auto n = bytefile.getNextUnsigned();
        return interpreter.onTag(name.value(), n);
    }
    case Opcodes::ARRAY: {
        auto size = bytefile.getNextUnsigned();
        return interpreter.onArray(size);
    }
    case Opcodes::FAIL: {
        return interpreter.onFail();
    }
    case Opcodes::LINE: {
        auto line         = bytefile.getNextUnsigned();
        bytefile.fileLine = line;
        return interpreter.onLine(line);
    }
    case Opcodes::PATT_str:
    case Opcodes::PATT_string:
    case Opcodes::PATT_array:
    case Opcodes::PATT_sexp:
    case Opcodes::PATT_ref:
    case Opcodes::PATT_val:
    case Opcodes::PATT_fun: {
        // SAFETY: this cast could happen only if we got
        // into right opcode
        auto type = static_cast<PatternType>(low);
        return interpreter.onPattern(type);
    }
    case Opcodes::CALL_Lread: {
        return interpreter.onCallLRead();
    }
    case Opcodes::CALL_Lwrite: {
        return interpreter.onCallLWrite();
    }
    case Opcodes::CALL_Llength: {
        return interpreter.onCallLLength();
    }
    case Opcodes::CALL_Lstring: {
        return interpreter.onCallLString();
    }
    case Opcodes::CALL_Barray: {
        auto arg = bytefile.getNextUnsigned();
        return interpreter.onCallBArray(arg);
    }
    default: {
        std::cerr << "unknown opcode " << static_cast<u32>(code) << '\n';
        return InterpretResult::ERROR;
    }
    }
}
} // namespace

// NOLINTNEXTLINE
int main(int argc, char** argv) try {
    if (argc != 2) {
        std::cerr << "Wrong input, support only file to bytecode that will be interpreted\n";
        return EXIT_FAILURE;
    }
    const char* file             = argv[1];
    auto        possibleBytefile = Bytefile::readBytefile(file);
    if (std::holds_alternative<DiagnosticsBag>(possibleBytefile)) {
        auto errors = std::get<DiagnosticsBag>(std::move(possibleBytefile));
        for (auto&& e : errors) { std::cerr << "E " << e << '\n'; }
        return EXIT_FAILURE;
    }
    auto            bytefile = std::get<Bytefile>(std::move(possibleBytefile));
    Interpreter     interpreter {bytefile.globalAreaSize};
    InterpretResult result = InterpretResult::CONTINUE;
    while ((result = interpretOne(bytefile, interpreter)) == InterpretResult::CONTINUE) {}

    if (result == InterpretResult::ERROR) {
        std::cerr << "E while trying to interpret ";
        if (!bytefile.fileLine) {
            std::cerr << "code without line info";
        } else {
            std::cerr << "file line " << bytefile.fileLine;
        }
        if (!bytefile.prevIP) {
            std::cerr << " on very first opcode";
        } else {
            std::cerr << "on 0x" << std::hex << bytefile.relAddr(bytefile.prevIP) << ": "
                      << toString(Opcodes(*bytefile.prevIP));
        }

        std::cerr << std::endl;
    }

} catch (std::exception& e) {
    std::cerr << "Uncaught exception: " << e.what() << '\n';
    return EXIT_FAILURE;
} catch (...) {
    std::cerr << "Unknown exception type thrown.\n";
    return EXIT_FAILURE;
}
