#include "Interpreter.hpp"

#include "Opcodes.hpp"
#include "Types.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

extern "C" {
#include "LamaRuntime.hpp"
#include "gc.h"
#include "runtime_common.h"
}

constexpr std::string_view NOT_ENOUGH_POP  = "Cannot allocate enough memory on stack: underflow";
constexpr std::string_view NOT_ENOUGH_PUSH = "Cannot allocate enough memory on stack: overflow";

// bytecode header looks like this
// ┌────────────┬────────────┬───────────┬────────────────┬─────────────┬────────────────┐
// │            │            │           │                │             │                │
// │strPool size│globals size│pubSym size│ public Symbols │ string pool │ bytecode       │
// │    (bytes) │   (words)  │           │                │             │                │
// └────────────┴────────────┴───────────┴────────────────┴─────────────┴────────────────┘
//   4 bytes      4 bytes      4 bytes     pubSym * 2     strPool size   rest of the file
//                                         * sizeof(i32)
//                                              ???

auto Bytefile::readBytefile(const char* filename) -> std::variant<DiagnosticsBag, Bytefile> {
    std::ifstream  file(filename, std::ios::binary | std::ios::ate);
    DiagnosticsBag readErrors;

    if (!file) { throw std::runtime_error("Error opening file: " + std::string(std::strerror(errno))); }
    // will throw an exception if cannot read
    file.exceptions(std::ios_base::badbit | std::ios_base::failbit);

    auto fileSize = static_cast<std::streamoff>(file.tellg());
    if (fileSize < 0) { throw std::runtime_error("Error determining file size: " + std::string(std::strerror(errno))); }
    file.seekg(0, std::ios::beg);

    Bytefile result;
    auto*    ptr         = operator new(static_cast<usize>(fileSize), std::align_val_t(alignof(i32)));
    auto     deleter     = [](auto p) { operator delete(p); };
    auto     raiiDeleter = std::unique_ptr<char, decltype(deleter)>(static_cast<char*>(ptr), deleter);

    // exception will be thrown, if file open fails
    file.read(static_cast<char*>(ptr), static_cast<std::streamsize>(fileSize));

    u8* u8ptr = static_cast<u8*>(ptr);
    u32 strPoolSize;
    u32 globalAreaSize;
    u32 publicSymbolsNumber;
    copyValues(&strPoolSize, u8ptr);
    copyValues(&globalAreaSize, u8ptr + sizeof(i32));
    copyValues(&publicSymbolsNumber, u8ptr + 2 * sizeof(i32));
    u32 headerSize = 3 * sizeof(i32);
    {
        // NOTE(zelourses): there could be a problem, if we are not aligned to the int
        // That's why to decrease these chances we allocate with alignment of `i32`
        if (publicSymbolsNumber * 2 + headerSize >= fileSize) {
            std::stringstream ss;
            ss << "public symbols size is " << publicSymbolsNumber * 2 << " bytes, while file size is " << fileSize
               << " bytes";
            readErrors.emplace_back(ss.str());
        } else {
            u32* publicSymbols = reinterpret_cast<u32*>(u8ptr + headerSize);
            assert(reinterpret_cast<uintptr_t>(publicSymbols) % 4 == 0);
            result.publicSymbols = std::span<u32> {publicSymbols, static_cast<usize>(publicSymbolsNumber) * 2};
        }
    }
    {
        if (strPoolSize + publicSymbolsNumber * 2 + headerSize >= fileSize) {
            std::stringstream ss;
            ss << "string pool size is " << strPoolSize << " bytes, while remaining file size is "
               << (fileSize - (publicSymbolsNumber * 2) - headerSize) << " bytes";
            readErrors.emplace_back(ss.str());
        } else {
            u8* strPool    = u8ptr + headerSize + publicSymbolsNumber * 2 * sizeof(i32);
            result.strPool = std::span<u8> {strPool, static_cast<usize>(strPoolSize)};
        }
    }
    { result.globalAreaSize = globalAreaSize; }
    {
        auto bytecodeSize = static_cast<usize>(fileSize - result.publicSymbols.size() * sizeof(i32)
                                               - result.strPool.size() - headerSize);
        if (bytecodeSize == 0 || bytecodeSize >= fileSize) { // if we overflow
            std::stringstream ss;
            ss << " bytecode size is " << bytecodeSize << "bytes, while the whole file size is" << fileSize << " bytes";
            readErrors.emplace_back(ss.str());
        } else {
            u8* bytecode    = u8ptr + headerSize + result.publicSymbols.size() * sizeof(i32) + result.strPool.size();
            result.bytecode = std::span<u8>(bytecode, bytecodeSize);
        }
    }

    if (readErrors.empty()) {
        // Essential, otherwise we will get double free
        raiiDeleter.release();

        result.rawData.reset(static_cast<u8*>(ptr));

        result.ip = result.bytecode.data();

        return result;
    }

    return readErrors;
}

auto Bytefile::getString(usize position) -> std::optional<std::string_view> {
    if (position >= strPool.size()) { return std::nullopt; }
    // SAFETY: cast from unsigned char to signed char pointer
    // is standard-layout type, sign does not change it
    // Standard also guarantees that old and new pointer value
    // is equal
    return reinterpret_cast<char*>(strPool.begin().base() + position);
}

auto Bytefile::getNextInt() noexcept -> i32 {
    i32 result;
    copyValues(&result, ip);
    ip += sizeof(i32);
    return result;
}

auto Bytefile::getNextUnsigned() noexcept -> u32 {
    u32 result;
    copyValues(&result, ip);
    ip += sizeof(u32);
    return result;
}

auto Bytefile::getNextString() -> std::optional<std::string_view> {
    if (!enoughBytes(sizeof(u32))) { return std::nullopt; }
    auto index = getNextUnsigned();

    return getString(index);
}

auto Bytefile::closureArray(u32 n) -> std::span<ClosureArg> {
    auto* args = std::bit_cast<ClosureArg*>(ip);
    ip += (sizeof(i8) + sizeof(u32)) * n;
    return {args, static_cast<u32>(n)};
}

auto Bytefile::trySetAddress(u32 absoluteAddress) noexcept -> bool {
    if (bytecode.size() <= absoluteAddress) { return false; }
    ip = bytecode.data() + absoluteAddress;
    return true;
}

auto Bytefile::address() noexcept -> usize {
    // this will always be >= 0;
    assert(ip >= bytecode.data());
    return static_cast<usize>(ip - bytecode.data());
}

auto Bytefile::enoughBytes(usize bytes) noexcept -> bool {
    assert(ip - bytecode.begin().base() >= 0);
    return bytecode.size() - static_cast<usize>((ip - bytecode.begin().base())) >= bytes;
}

Interpreter::Interpreter(u32 globalsSize) : stack(globalsSize) { __init(); }

Stack::Stack(u32 globalsSizeWords) : globalsSize(globalsSizeWords) {
    __gc_stack_bottom = innerData.data() + innerData.size() - 1;
    __gc_stack_top    = __gc_stack_bottom - globalsSize;

    begin = __gc_stack_top;
    bp    = begin;

    push(BOX(0)); // zero as a result
    push(0);      // stop, i.e. -- nullptr
}

auto Bytefile::getNextCode() noexcept -> u8 {
    prevIP = ip;
    return *ip++;
}

auto Bytefile::peekNextCode() noexcept -> u8 { return *ip; }

void Stack::push(usize value) { *(__gc_stack_top--) = value; }

auto Stack::pop() -> usize { return *(++__gc_stack_top); }

auto Stack::top() -> usize { return *(__gc_stack_top + 1); }

auto Stack::getReference(u32 index, VariableType kind) -> std::optional<usize*> {
    switch (kind) {
    case VariableType::Global: {
        if (index > globalsSize) { return std::nullopt; }

        return {begin + 1 + index};
    }
    case VariableType::Local: {
        if (index >= nLocals) { return std::nullopt; }
        return bp - 1 - index;
    }
    case VariableType::Argument: {
        if (index >= nArgs) { return std::nullopt; }

        return bp + 3 // We push 3 `system` variables on stack -- nArgs, nLocals and bp
             + nArgs  // Arguments are reversed to the given index
             - index; //
    }
    case VariableType::Captured: {
        auto* closure = std::bit_cast<usize*>(*(bp + 3 + nArgs + 1));
        return closure + 1 + index;
    }
    }

    // let's hope that it's impossible
    return std::nullopt;
}

auto Stack::stackBegin() -> usize* { return begin; }

auto Stack::prologue([[maybe_unused]] bool beginInClosure, u32 newNArgs, u32 newNLocals) -> bool {
    // std::cerr << "prologue " << beginInClosure << ", " << newNArgs << ", " << newNLocals << std::endl;
    if (!enoughToPush(satAdd(4, newNLocals))) { return false; }

    // NOLINTNEXTLINE(*-sign-conversion)
    push(BOX(nArgs));               // 1
    push(BOX(nLocals));             // 2
    push(std::bit_cast<usize>(bp)); // 3

    nArgs   = newNArgs;
    nLocals = newNLocals;
    bp      = __gc_stack_top + 1;     // point to the previous bp
    __gc_stack_top -= newNLocals + 1; // For return address // 4 + newNLocals
    // We must place boxed value of zero, otherwise gc will go mad
    std::fill(__gc_stack_top, __gc_stack_top + newNLocals + 2, BOX(0));
    return true;
}

auto Stack::epilogue(bool isClosure) -> u8* {
    u32 valuesToPop = 5 + (isClosure ? 1 : 0);
    if (!enoughToPop(satAdd(valuesToPop, nArgs))) { return nullptr; }

    // NOTE(zelourses): we save boxing here
    auto retval    = pop(); // 1
    u32  nArgsOld  = nArgs;
    __gc_stack_top = bp - 1;

    bp      = std::bit_cast<usize*>(pop()); // 2
    nLocals = UNBOX(pop());                 // 3
    nArgs   = UNBOX(pop());                 // 4

    u8* returnAddress = std::bit_cast<u8*>(pop()); // 5

    __gc_stack_top += nArgsOld; // here are nArgs from `enoughToPop`

    if (isClosure) { pop(); } // possible 6
    push(retval);
    return returnAddress;
}

auto Stack::closureRelativeAddr(u32 args) -> u32 {
    auto* ptr = std::bit_cast<usize*>(*(__gc_stack_top + 1 + args));
    return *ptr;
}

auto Stack::enoughToPop(usize n) noexcept -> bool { return static_cast<usize>(begin - __gc_stack_top) >= n; }

auto Stack::enoughToPush(usize n) noexcept -> bool {
    return static_cast<usize>(innerData.data() - __gc_stack_top) >= n;
}

#define checkStackPush                 \
    if (!stack.enoughToPush()) {       \
        std::cerr << NOT_ENOUGH_PUSH;  \
        return InterpretResult::ERROR; \
    }

#define checkStackPop                  \
    if (!stack.enoughToPop()) {        \
        std::cerr << NOT_ENOUGH_POP;   \
        return InterpretResult::ERROR; \
    }

auto Interpreter::onBegin(bool beginInClosure, u32 nArgs, u32 nLocals) -> InterpretResult {
    if (!stack.prologue(beginInClosure, nArgs, nLocals)) {
        std::cerr << NOT_ENOUGH_PUSH;
        return InterpretResult::ERROR;
    }
    return InterpretResult::CONTINUE;
}

auto Interpreter::onCallLRead() -> InterpretResult {
    checkStackPush;
    stack.push(std::bit_cast<u32>(Lread()));
    return InterpretResult::CONTINUE;
}

auto Interpreter::onLine([[maybe_unused]] u32 line) -> InterpretResult {
    // Well, it's an noop for interpreter, as far as Ocaml interpretation goes
    return InterpretResult::CONTINUE;
}

auto Interpreter::onStore(u32 index, VariableType toSave) -> InterpretResult {
    // top is technically a pop and push operation, so will check for pop
    checkStackPop;

    auto top = stack.top();
    auto res = stack.getReference(index, toSave);
    if (!res.has_value()) {
        std::cerr << "Cannot get reference on index " << index << " for type " << to_underlying(toSave);
        return InterpretResult::ERROR;
    }
    *(res.value()) = top;

    return InterpretResult::CONTINUE;
}

auto Interpreter::onDrop() -> InterpretResult {
    checkStackPop;
    stack.pop();
    return InterpretResult::CONTINUE;
}

auto Interpreter::onLoad(u32 index, VariableType toLoad) -> InterpretResult {
    auto ref = stack.getReference(index, toLoad);
    if (!ref.has_value()) {
        std::cerr << "Cannot create reference with index " << index << "and type " << to_underlying(toLoad);
        return InterpretResult::ERROR;
    }
    // NOTE(zelourses): we check firstly that we got the right value to give
    // better diagnostics for both loadable value and for stack size
    checkStackPush;

    stack.push(*ref.value());
    return InterpretResult::CONTINUE;
}

auto Interpreter::onBinOp(BinOp operation) -> InterpretResult {
    if (!stack.enoughToPop(2)) {
        std::cerr << NOT_ENOUGH_POP;
        return InterpretResult::ERROR;
    }

    i32 rhs = UNBOX(stack.pop());
    i32 lhs = UNBOX(stack.pop());
    i32 result;
#define binop(type, op)      \
    case type: {             \
        result = lhs op rhs; \
        break;               \
    }

    switch (operation) {
        binop(BinOp::ADD, +);
        binop(BinOp::SUB, -);
        binop(BinOp::MUL, *);
        binop(BinOp::DIV, /);
        binop(BinOp::REM, %);
        binop(BinOp::LT, <);
        binop(BinOp::LE, <=);
        binop(BinOp::GT, >);
        binop(BinOp::GE, >=);
        binop(BinOp::EQ, ==);
        binop(BinOp::NE, !=);
        binop(BinOp::AND, &&);
        binop(BinOp::OR, ||);
    default: FAIL();
    }
#undef binop
    // NOLINTNEXTLINE(*-sign-conversion)
    stack.push(BOX(result));

    return InterpretResult::CONTINUE;
}

auto Interpreter::onConst(i32 value) -> InterpretResult {
    checkStackPush;
    // NOLINTNEXTLINE(*-sign-conversion)
    stack.push(BOX(value));
    return InterpretResult::CONTINUE;
}

auto Interpreter::onCallLWrite() -> InterpretResult {
    checkStackPop;

    i32 val = UNBOX(stack.pop());
    // Is this an appropriate place to print?
    std::cout << val << '\n';
    stack.push(BOX(0)); // otherwise it will not work
    return InterpretResult::CONTINUE;
}

auto Interpreter::onEndOrRet() -> u8* {
    u8* result = nullptr;
    if (stack.bp != stack.stackBegin() - 1) { result = stack.epilogue(isClosure); }
    isClosure = false;
    return result;
}

auto Interpreter::onJump(u32 jumpLocation) -> usize {
    // Well, we must somehow inform interpreter about jump.
    return jumpLocation;
}

auto Interpreter::onCondJump([[maybe_unused]] bool isNotEq, u32 jumpLocation, usize originalAddress)
    -> std::optional<usize> {
    if (!stack.enoughToPop()) {
        std::cerr << NOT_ENOUGH_POP;
        return std::nullopt;
    }

    auto top = UNBOX(stack.pop());
    if ((!top && !isNotEq) || (top && isNotEq)) { return jumpLocation; }
    return originalAddress;
}

auto Interpreter::onCall(u32 location, [[maybe_unused]] u32 nArgs, u8* originalIP) -> std::optional<usize> {
    if (!stack.enoughToPush()) {
        std::cerr << NOT_ENOUGH_PUSH;
        return std::nullopt;
    }
    // FIXME: there must be a way to check that we are trying to call a function
    // Maybe by BEGIN opcode?
    auto ip = std::bit_cast<usize>(originalIP);
    // We need to copy value of pointer, not the data underneath it
    // castValues(&ip, &originalIP);
    stack.push(ip);
    return location;
}

auto Interpreter::onString(std::string_view str) -> InterpretResult {
    checkStackPush;

    void* objString = Bstring(const_cast<char*>(str.begin()));

    auto value = std::bit_cast<usize>(objString);
    stack.push(value);

    return InterpretResult::CONTINUE;
}

auto Interpreter::onCallLLength() -> InterpretResult {
    checkStackPop;

    auto  val = stack.pop();
    void* ptr = std::bit_cast<void*>(val);

    auto result = static_cast<u32>(Llength(ptr));
    stack.push(result);

    return InterpretResult::CONTINUE;
}

auto Interpreter::onElem() -> InterpretResult {
    if (!stack.enoughToPop(2)) {
        std::cerr << NOT_ENOUGH_POP;
        return InterpretResult::ERROR;
    }
    auto index    = stack.pop();
    auto valToPtr = stack.pop();

    void* ptr     = std::bit_cast<void*>(valToPtr);
    auto  element = std::bit_cast<usize>(Belem(ptr, index));

    stack.push(element);

    return InterpretResult::CONTINUE;
}

auto Interpreter::onSTA() -> InterpretResult {
    if (!stack.enoughToPop(3)) {
        std::cerr << NOT_ENOUGH_POP;
        return InterpretResult::ERROR;
    }

    void* valuePtr = std::bit_cast<void*>(stack.pop());
    auto  i        = std::bit_cast<i32>(stack.pop());
    void* xPtr     = std::bit_cast<void*>(stack.pop());
    stack.push(std::bit_cast<usize>(Bsta(valuePtr, i, xPtr)));

    return InterpretResult::CONTINUE;
}

auto Interpreter::onCallBArray(u32 n) -> InterpretResult {
    if (!stack.enoughToPop(n)) {
        std::cerr << NOT_ENOUGH_POP;
        return InterpretResult::ERROR;
    }

    data* bArray = static_cast<data*>(alloc_array(n));

    for (; n > 0; --n) {
        auto elem = stack.pop();
        // FIXME: it's very bad. We increase the alignment from 1 to 4 and I don't know how to deal with it
        // NOLINTNEXTLINE
        ((int*)bArray->contents)[n - 1] = elem;
    }
    void* arr = bArray->contents;
    stack.push(std::bit_cast<usize>(arr));

    return InterpretResult::CONTINUE;
}

auto Interpreter::onSexp(std::string_view tag, u32 n) -> InterpretResult {
    if (!stack.enoughToPop(n)) {
        std::cerr << NOT_ENOUGH_POP;
        return InterpretResult::ERROR;
    }

    sexp* sExpArray = static_cast<sexp*>(alloc_sexp(n));
    sExpArray->tag  = 0;

    // Do I need to do n -= 1?
    for (; n > 0; --n) {
        auto value                         = stack.pop();
        ((i32*)sExpArray->contents)[n - 1] = value;
    }
    // SAFETY: this call is safe because the value is actually does not changing
    // (As far as I can see from source code of lama)
    //
    // Also, code inside [`LtagHash`] actually do not use this data, it's just
    // someone forgot to put `const`
    sExpArray->tag = UNBOX(LtagHash(const_cast<char*>(tag.data())));
    stack.push(std::bit_cast<usize>(&(sExpArray->tag)));

    return InterpretResult::CONTINUE;
}

auto Interpreter::onDuplicate() -> InterpretResult {
    checkStackPop;
    checkStackPush;

    stack.push(stack.top());
    return InterpretResult::CONTINUE;
}

auto Interpreter::onTag(std::string_view name, u32 n) -> InterpretResult {
    checkStackPop;

    // SAFETY: this call is safe because the value is actually does not changing
    // (As far as I can see from source code of lama)
    //
    // Also, code inside [`LtagHash`] actually do not use this data, it's just
    // someone forgot to put `const`
    auto hash  = LtagHash(const_cast<char*>(name.data()));
    auto boxed = BOX(n);

    auto value = std::bit_cast<u32>(Btag(std::bit_cast<void*>(stack.pop()), hash, boxed));
    stack.push(value);

    return InterpretResult::CONTINUE;
}

auto Interpreter::onCallLString() -> InterpretResult {
    checkStackPop;
    auto* str = Lstring(std::bit_cast<void*>(stack.pop()));
    stack.push(std::bit_cast<u32>(str));

    return InterpretResult::CONTINUE;
}

auto Interpreter::onLoadAccumulator(u32 index, VariableType toLoad) -> InterpretResult {
    auto ref = stack.getReference(index, toLoad);
    if (!ref.has_value()) {
        std::cerr << "cannot create reference in closure for index" << index << " and value " << to_underlying(toLoad);
        return InterpretResult::ERROR;
    }
    if (!stack.enoughToPush(2)) {
        std::cerr << NOT_ENOUGH_PUSH;
        return InterpretResult::ERROR;
    }
    auto val = std::bit_cast<u32>(ref.value());
    stack.push(val);
    stack.push(val);

    return InterpretResult::CONTINUE;
}

auto Interpreter::onClosure(u32 address, std::span<Bytefile::ClosureArg> args) -> InterpretResult {
    checkStackPush;

    data* closure = static_cast<data*>(alloc_closure(args.size() + 1)); // + address
    if (!closure) {
        std::cerr << "Cannot allocate memory for closure";
        return InterpretResult::CONTINUE;
    }

    // FIXME: it's very bad. We increase the alignment from 1 to 4 and I don't know how to deal with it
    // NOLINTNEXTLINE
    ((u32*)closure->contents)[0] = address;

    u32 i = 1;
    for (auto&& [type, index] : args) {
        auto res = stack.getReference(index, type);
        if (!res.has_value()) {
            std::cerr << "cannot create reference in closure for index" << index << " and value "
                      << to_underlying(type);
            return InterpretResult::ERROR;
        }
        // FIXME: it's very bad. We increase the alignment from 1 to 4 and I don't know how to deal with it
        // NOLINTNEXTLINE
        ((u32*)closure->contents)[i++] = *res.value();
    }
    void* value = closure->contents;
    stack.push(std::bit_cast<u32>(value));
    return InterpretResult::CONTINUE;
}

auto Interpreter::onCallClosure(u8* returnAddress, u32 nArgs) -> std::optional<usize> {
    if (!stack.enoughToPush()) {
        std::cerr << NOT_ENOUGH_PUSH;
        return std::nullopt;
    }

    usize addr = stack.closureRelativeAddr(nArgs);

    stack.push(std::bit_cast<usize>(returnAddress));

    isClosure = true;
    return addr;
}

auto Interpreter::onPattern(PatternType pattern) -> InterpretResult {
    checkStackPop;

#define pattern(pat, func)                          \
    case pat: {                                     \
        auto val = stack.pop();                     \
        result   = func(std::bit_cast<void*>(val)); \
        break;                                      \
    }

    i32 result = 0;

    switch (pattern) {
    case PatternType::Str: {
        checkStackPop;
        auto lhs = stack.pop();
        auto rhs = stack.pop();
        auto res = std::bit_cast<u32>(Bstring_patt(std::bit_cast<void*>(lhs), std::bit_cast<void*>(rhs)));
        stack.push(res);
        return InterpretResult::CONTINUE; // Here we are returning to not cause anything bad in next code
    }
        pattern(PatternType::String, Bstring_tag_patt);
        pattern(PatternType::Array, Barray_tag_patt);
        pattern(PatternType::Sexp, Bsexp_tag_patt);
        pattern(PatternType::Boxed, Bboxed_patt);
        pattern(PatternType::Unboxed, Bunboxed_patt);
        pattern(PatternType::Closure, Bclosure_tag_patt);
    }

    stack.push(std::bit_cast<usize>(result));

    return InterpretResult::CONTINUE;
}

auto Interpreter::onArray(u32 size) -> InterpretResult {
    checkStackPop;
    usize array   = stack.pop();
    auto  isArray = std::bit_cast<u32>(Barray_patt(std::bit_cast<void*>(array), BOX(size)));
    stack.push(isArray);

    return InterpretResult::CONTINUE;
}

auto Interpreter::onSwap() -> InterpretResult {
    if (!stack.enoughToPop(2)) {
        std::cerr << NOT_ENOUGH_POP;
        return InterpretResult::ERROR;
    }
    auto first  = stack.pop();
    auto second = stack.pop();
    stack.push(first);
    stack.push(second);

    return InterpretResult::CONTINUE;
}

auto Interpreter::onFail() -> InterpretResult {
    if (!stack.enoughToPop(2)) {
        std::cerr << "Critical -- not enough values for fail message";
    } else {
        auto first  = stack.pop();
        auto second = stack.pop();
        std::cerr << "Something went wrong: " << first << ", " << second;
    }

    return InterpretResult::ERROR;
}
