# Crank up the warnings to the maximum. These flags should all work with
# relatively modern compilers.
# https://gcc.gnu.org/onlinedocs/gcc-13.1.0/gcc/Warning-Options.html
# https://clang.llvm.org/docs/DiagnosticsReference.html
function(enable_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} /W4)
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    target_compile_options(
      ${target_name}
      PRIVATE -Wall
              -Wextra
              -pedantic
              -Wshadow
              -Wnon-virtual-dtor
              -Wcast-align
              -Wunused
              -Woverloaded-virtual
              -Wpedantic
              -Wnull-dereference
              -Wformat=2
              -Wimplicit-fallthrough
              -Wno-missing-field-initializers
              -Wpessimizing-move
              -Wno-nullability-extension
              -Wno-c99-extensions)
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    target_compile_options(
      ${target_name}
      PRIVATE -Wall
              -Wextra
              -pedantic
              -Wshadow
              -Wnon-virtual-dtor
              -Wcast-align
              -Wunused
              -Woverloaded-virtual
              -Wpedantic
              -Wmisleading-indentation
              -Wduplicated-cond
              -Wduplicated-branches
              -Wlogical-op
              -Wnull-dereference
              -Wformat=2
              -Wimplicit-fallthrough
              -Wpessimizing-move
              -Wno-missing-field-initializers
              -Wno-error=attributes)
  endif()
endfunction(enable_warnings)
