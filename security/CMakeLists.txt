set(argon2_root "${CMAKE_CURRENT_LIST_DIR}/../third_party/argon2")
add_library(vexa_argon2 STATIC
    ${argon2_root}/src/argon2.c
    ${argon2_root}/src/core.c
    ${argon2_root}/src/encoding.c
    ${argon2_root}/src/ref.c
    ${argon2_root}/src/blake2/blake2b.c)
target_include_directories(vexa_argon2 PUBLIC ${argon2_root}/include)
target_compile_definitions(vexa_argon2 PRIVATE ARGON2_NO_THREADS)
set_target_properties(vexa_argon2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(vexa_access STATIC
    PasswordStore.cpp PasswordStore.h PasswordLock.cpp PasswordLock.h
    AppAccess.cpp AppAccess.h SystemLockMonitor.cpp SystemLockMonitor.h)
set_target_properties(vexa_access PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(vexa_access PUBLIC cxx_std_17)
target_include_directories(vexa_access PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(vexa_access PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent PRIVATE vexa_argon2)
if(WIN32)
    target_link_libraries(vexa_access PRIVATE advapi32 wtsapi32)
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_link_libraries(vexa_access PUBLIC Qt6::DBus)
endif()
