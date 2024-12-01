/**
 * @file Interpreter.hpp
 * @brief This file contains important definitions of main interpreter subroutine
 * and bytefile structure definition
 *
 */
#pragma once
#include "Opcodes.hpp"
#include "Types.hpp"
#include "Utils.hpp"

#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

using DiagnosticsBag = std::vector<std::string>;

/**
 * @brief Structure that represents Lama bytecode file
 * copied from `byterun.c` bytecode printer in Lama project
 */
struct Bytefile {
    std::span<u8>  strPool;       // Strings table
    std::span<u32> publicSymbols; // public symbols
    std::span<u8>  bytecode;      // bytecode buffer
    u32            globalAreaSize;
    u8*            ip;

    u32 fileLine = 0;

    u8* prevIP = nullptr;

    static auto readBytefile(const char* filename) -> std::variant<DiagnosticsBag, Bytefile>;

    auto getString(usize position) -> std::optional<std::string_view>;
    auto getNextString() -> std::optional<std::string_view>;

    auto getNextInt() noexcept -> i32;
    auto getNextUnsigned() noexcept -> u32;

    auto getNextCode() noexcept -> u8;
    auto peekNextCode() noexcept -> u8;

    auto trySetAddress(u32 absoluteAddress) noexcept -> bool;
    auto address() noexcept -> usize;
    /**
     * @brief checks that bytecode has `bytes` amount of bytes next to read
     *
     * @param bytes amount of bytes that could be read
     * @return true there are enough bytes here
     * @return false there is not enough bytes in bytecode
     */
    auto enoughBytes(usize bytes) noexcept -> bool;

    auto relAddr(u8* ptr) noexcept -> usize {
        assert(ptr - bytecode.data() >= 0);
        return static_cast<usize>(ptr - bytecode.data());
    }

#pragma pack(push, 1)

    struct ClosureArg {
        VariableType type;
        u32          argument;
    };

#pragma pack(pop)

    static_assert(sizeof(ClosureArg) == 5,
                  "ClosureArg must have the size of 5 bytes, otherwise it will miss the closure arguments");

    auto closureArray(u32 n) -> std::span<ClosureArg>;

private:
    std::unique_ptr<u8> rawData;
};

class Stack {
public:
    Stack(u32 globalsSizeWords);

    LI_ALWAYS_INLINE
    void push(usize value);
    LI_ALWAYS_INLINE
    auto pop() -> usize;
    LI_ALWAYS_INLINE
    auto top() -> usize;
    LI_ALWAYS_INLINE
    auto getReference(u32 index, VariableType kind) -> std::optional<usize*>;

    auto stackBegin() -> usize*;
    LI_ALWAYS_INLINE
    auto prologue(bool beginInClosure, u32 newNArgs, u32 newNLocals) -> bool;
    LI_ALWAYS_INLINE
    auto epilogue(bool isClosure) -> u8*;
    LI_ALWAYS_INLINE
    auto closureRelativeAddr(u32 args) -> u32;
    LI_ALWAYS_INLINE
    auto enoughToPop(usize n = 1) noexcept -> bool;
    LI_ALWAYS_INLINE
    auto enoughToPush(usize n = 1) noexcept -> bool;

    usize* bp;
    u32    nArgs   = 2; // Default value from which the program launches (_start)
    u32    nLocals = 0;

private:
    static constexpr usize            MAX_STACK_SIZE = 100'000;
    std::array<usize, MAX_STACK_SIZE> innerData;
    usize*                            begin = nullptr;
    u32                               globalsSize;
};

enum class InterpretResult {
    CONTINUE,
    STOP,
    ERROR,
};

class Interpreter {
public:
    auto onBegin(bool isClosure, u32 nArgs, u32 nLocals) -> InterpretResult;
    auto onCallLRead() -> InterpretResult;
    auto onLine(u32 line) -> InterpretResult;
    auto onStore(u32 index, VariableType toSave) -> InterpretResult;
    auto onDrop() -> InterpretResult;
    auto onLoad(u32 index, VariableType toLoad) -> InterpretResult;
    auto onBinOp(BinOp operation) -> InterpretResult;
    auto onConst(i32 value) -> InterpretResult;
    auto onCallLWrite() -> InterpretResult;

    [[nodiscard("This value is the next IP")]]
    auto onEndOrRet() -> u8*;
    [[nodiscard("This value is the next IP")]]
    auto onJump(u32 jumpLocation) -> usize;

    [[nodiscard("This value is the next IP")]]
    auto onCondJump(bool isNotEq, u32 jumpLocation, usize originalIP) -> std::optional<usize>;

    [[nodiscard("This value is the next IP")]]
    auto onCall(u32 location, u32 nArgs, u8* originalIP) -> std::optional<usize>;
    auto onString(std::string_view str) -> InterpretResult;
    auto onCallLLength() -> InterpretResult;
    auto onElem() -> InterpretResult;
    auto onSTA() -> InterpretResult;
    auto onCallBArray(u32 n) -> InterpretResult;
    auto onSexp(std::string_view tag, u32 n) -> InterpretResult;
    auto onDuplicate() -> InterpretResult;
    auto onTag(std::string_view name, u32 n) -> InterpretResult;
    auto onCallLString() -> InterpretResult;
    auto onLoadAccumulator(u32 index, VariableType toLoad) -> InterpretResult;
    auto onClosure(u32 address, std::span<Bytefile::ClosureArg> args) -> InterpretResult;

    [[nodiscard("This value is the next IP")]]
    auto onCallClosure(u8* returnAddress, u32 nArgs) -> std::optional<usize>;
    auto onPattern(PatternType pattern) -> InterpretResult;

    auto onArray(u32 size) -> InterpretResult;
    auto onSwap() -> InterpretResult;
    auto onFail() -> InterpretResult;

    Interpreter(u32 globalAreaSize);

private:
    bool  isClosure = false;
    Stack stack;
};
