/**
 * @file Opcodes.hpp
 * @brief This file contains definitions of heart of the interpreter:
 * Bytecode Instructions and binary operations in it
 *
 */
#pragma once

#include "Types.hpp"
#include <string_view>

// FIXME: Maybe we need to change the names of Opcodes?
//
// NOLINTBEGIN
enum class Opcodes : u8 {
    BINOP_add = 0x01, // BINOP +
    BINOP_sub = 0x02, // BINOP -
    BINOP_mul = 0x03, // BINOP *
    BINOP_div = 0x04, // BINOP /
    BINOP_rem = 0x05, // BINOP %
    BINOP_lt  = 0x06, // BINOP <
    BINOP_le  = 0x07, // BINOP <=
    BINOP_gt  = 0x08, // BINOP >
    BINOP_ge  = 0x09, // BINOP >=
    BINOP_eq  = 0x0A, // BINOP ==
    BINOP_ne  = 0x0B, // BINOP !=
    BINOP_and = 0x0C, // BINOP &&
    BINOP_or  = 0x0D, // BINOP ||

    CONST  = 0x10, // CONST, int
    STRING = 0x11, // STRING, string
    SEXP   = 0x12, // SEXP, string, int
    STI    = 0x13, // STI
    STA    = 0x14, // STA
    JMP    = 0x15, // JMP, int
    END    = 0x16, // END
    RET    = 0x17, // RET
    DROP   = 0x18, // DROP
    DUP    = 0x19, // DUP
    SWAP   = 0x1A, // SWAP
    ELEM   = 0x1B, // ELEM

    LD_G = 0x20, // LD, G(int)
    LD_L = 0x21, // LD, L(int)
    LD_A = 0x22, // LD, A(int)
    LD_C = 0x23, // LD, C(int)

    LDA_G = 0x30, // LDA, G(int)
    LDA_L = 0x31, // LDA, L(int)
    LDA_A = 0x32, // LDA, A(int)
    LDA_C = 0x33, // LDA, C(int)

    ST_G = 0x40, // ST, G(int)
    ST_L = 0x41, // ST, L(int)
    ST_A = 0x42, // ST, A(int)
    ST_C = 0x43, // ST, C(int)

    CJMPz   = 0x50, // CJMPz, int
    CJMPnz  = 0x51, // CJMPnz, int
    BEGIN   = 0x52, // BEGIN, int, int
    CBEGIN  = 0x53, // CBEGIN, int, int
    CLOSURE = 0x54, // CLOSURE, int, n: int, [byte, int][n]
    CALLC   = 0x55, // CALLC, int
    CALL    = 0x56, // CALL, int, int
    TAG     = 0x57, // TAG, string, int
    ARRAY   = 0x58, // ARRAY, int
    FAIL    = 0x59, // FAIL, int, int
    LINE    = 0x5A, // LINE, int

    PATT_str    = 0x60, // PATT =str
    PATT_string = 0x61, // PATT #string
    PATT_array  = 0x62, // PATT #array
    PATT_sexp   = 0x63, // PATT #sexp
    PATT_ref    = 0x64, // PATT #ref
    PATT_val    = 0x65, // PATT #val
    PATT_fun    = 0x66, // PATT #fun

    CALL_Lread   = 0x70, // CALL Lread
    CALL_Lwrite  = 0x71, // CALL Lwrite
    CALL_Llength = 0x72, // CALL Llength
    CALL_Lstring = 0x73, // CALL Lstring
    CALL_Barray  = 0x74, // CALL Barray, int
};
// NOLINTEND

/**
 * @brief This enum class represents all binary operations that could
 * happen in opcode
 */
enum class BinOp : u8 {
    ADD = 0x01, // +
    SUB = 0x02, // -
    MUL = 0x03, // *
    DIV = 0x04, // /
    REM = 0x05, // %
    LT  = 0x06, // <
    LE  = 0x07, // <=
    GT  = 0x08, // >
    GE  = 0x09, // >=
    EQ  = 0x0A, // ==
    NE  = 0x0B, // !=
    AND = 0x0C, // &&
    OR  = 0x0D, // ||
};

/**
 * @brief This enum represents all possible values that can be stored or loaded
 *
 */
enum class VariableType : u8 {
    Global   = 0x0,
    Local    = 0x1,
    Argument = 0x2,
    Captured = 0x3,
};

/**
 * @brief This enum represents all possible pattern matches that could be done
 *  in Lama
 */
enum class PatternType : u8 {
    Str     = 0x0, // =str
    String  = 0x1, // #string
    Array   = 0x2, // #array
    Sexp    = 0x3, // #sexp
    Boxed   = 0x4, // #ref
    Unboxed = 0x5, // #val
    Closure = 0x6, // #fun
};

inline auto toString(Opcodes op) -> std::string_view {
#define toStr(op) \
    case Opcodes::op: return #op
    switch (op) {
        toStr(BINOP_add);
        toStr(BINOP_sub);
        toStr(BINOP_mul);
        toStr(BINOP_div);
        toStr(BINOP_rem);
        toStr(BINOP_lt);
        toStr(BINOP_le);
        toStr(BINOP_gt);
        toStr(BINOP_ge);
        toStr(BINOP_eq);
        toStr(BINOP_ne);
        toStr(BINOP_and);
        toStr(BINOP_or);
        toStr(CONST);
        toStr(STRING);
        toStr(SEXP);
        toStr(STI);
        toStr(STA);
        toStr(JMP);
        toStr(END);
        toStr(RET);
        toStr(DROP);
        toStr(DUP);
        toStr(SWAP);
        toStr(ELEM);
        toStr(LD_G);
        toStr(LD_L);
        toStr(LD_A);
        toStr(LD_C);
        toStr(LDA_G);
        toStr(LDA_L);
        toStr(LDA_A);
        toStr(LDA_C);
        toStr(ST_G);
        toStr(ST_L);
        toStr(ST_A);
        toStr(ST_C);
        toStr(CJMPz);
        toStr(CJMPnz);
        toStr(BEGIN);
        toStr(CBEGIN);
        toStr(CLOSURE);
        toStr(CALLC);
        toStr(CALL);
        toStr(TAG);
        toStr(ARRAY);
        toStr(FAIL);
        toStr(LINE);
        toStr(PATT_str);
        toStr(PATT_string);
        toStr(PATT_array);
        toStr(PATT_sexp);
        toStr(PATT_ref);
        toStr(PATT_val);
        toStr(PATT_fun);
        toStr(CALL_Lread);
        toStr(CALL_Lwrite);
        toStr(CALL_Llength);
        toStr(CALL_Lstring);
        toStr(CALL_Barray);

    default: return "UNKNOWN_OPCODE";
    }

#undef toStr
}
