# SQLCipher is built into the application; the stock QSQLITE plugin is not used.
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
find_package(Threads REQUIRED)
add_library(vexa_sqlcipher STATIC ../third_party/sqlcipher/sqlite3.c)
target_include_directories(vexa_sqlcipher PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../third_party/sqlcipher)
target_compile_definitions(vexa_sqlcipher PUBLIC SQLITE_HAS_CODEC PRIVATE
    SQLITE_THREADSAFE=1 SQLITE_TEMP_STORE=2 SQLCIPHER_CRYPTO_OPENSSL
    SQLITE_EXTRA_INIT=sqlcipher_extra_init SQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown HAVE_STDINT_H=1
    SQLITE_ENABLE_COLUMN_METADATA SQLITE_ENABLE_FTS5 SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE SQLITE_USE_URI SQLITE_DEFAULT_MEMSTATUS=0)
target_link_libraries(vexa_sqlcipher PUBLIC OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(vexa_sqlcipher PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32)
    target_link_libraries(vexa_sqlcipher PUBLIC ws2_32 crypt32)
endif()

add_library(vexa_storage STATIC
    CipherDriver.cpp CipherDriver.h DatabaseEncryption.cpp DatabaseEncryption.h
    DatabaseProtection.cpp DatabaseProtection.h PortableBackup.cpp PortableBackup.h)
set_target_properties(vexa_storage PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
target_compile_features(vexa_storage PUBLIC cxx_std_17)
target_include_directories(vexa_storage PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(vexa_storage PUBLIC Qt6::Core Qt6::Sql Qt6::Concurrent PRIVATE vexa_sqlcipher)
if(WIN32)
    target_link_libraries(vexa_storage PRIVATE advapi32)
endif()
