# This GPL module is separate from the MIT SMTVV runtime.
add_library(p5r_safetyhook STATIC
  compat/safetyhook-allocator.cpp compat/safetyhook-inline.cpp compat/safetyhook-os.windows.cpp
  "${SAFETYHOOK_ROOT}/src/easy.cpp" "${SAFETYHOOK_ROOT}/src/mid_hook.cpp"
  "${SAFETYHOOK_ROOT}/src/utility.cpp" "${SAFETYHOOK_ROOT}/src/vmt_hook.cpp")
target_include_directories(p5r_safetyhook PUBLIC "${SAFETYHOOK_ROOT}/include")
target_link_libraries(p5r_safetyhook PUBLIC Zydis)
add_library(P5RFirstPersonCamera SHARED camera.cpp)
target_compile_definitions(P5RFirstPersonCamera PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(P5RFirstPersonCamera PRIVATE /W4 /EHsc /utf-8 /bigobj)
target_link_libraries(P5RFirstPersonCamera PRIVATE p5r_safetyhook user32 xinput comctl32)
foreach(test IN ITEMS camera controls settings)
  add_executable(P5RPublic${test}Tests ${test}_tests.cpp)
  target_compile_definitions(P5RPublic${test}Tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(P5RPublic${test}Tests PRIVATE /W4 /EHsc /utf-8 /bigobj)
  target_link_libraries(P5RPublic${test}Tests PRIVATE p5r_safetyhook user32 xinput comctl32)
  add_test(NAME P5RPublic${test}Tests COMMAND P5RPublic${test}Tests)
endforeach()
