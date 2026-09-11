cmake_minimum_required(VERSION 3.16)

project(MoneyTracker VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Sql Network LinguistTools)

qt_standard_project_setup(REQUIRES 6.10)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appMoneyTracker
    main.cpp
)

qt_add_qml_module(appMoneyTracker
    URI MoneyTracker
    QML_FILES
        Desktop.qml
        Mobile.qml
        # BankComboBox.qml
        # CategoryPicker.qml
        SOURCES core/Account.h
        SOURCES core/Asset.h
        SOURCES core/Category.h
        SOURCES core/Currency.h
        SOURCES core/Currency.cpp
        SOURCES core/Money.h
        SOURCES core/Transaction.h
        SOURCES core/CryptoWallet.h
        SOURCES core/CryptoTransaction.h
        SOURCES services/CurrencyConverter.h
        SOURCES services/CurrencyConverter.cpp
        SOURCES services/BalanceCalculator.h
        SOURCES services/BalanceCalculator.cpp
        SOURCES services/TransactionDateFilter.h
        SOURCES services/TransactionDateFilter.cpp
        SOURCES services/DateSliceCalculator.h
        SOURCES services/DateSliceCalculator.cpp
        SOURCES services/CsvCodec.h
        SOURCES services/CsvCodec.cpp
        SOURCES services/CurrencyRateProvider.h
        SOURCES services/CurrencyRateCache.h
        SOURCES services/CurrencyRateCache.cpp
        SOURCES services/CbrCurrencyRateProvider.h
        SOURCES services/CbrCurrencyRateProvider.cpp
        SOURCES services/TronUsdtParser.h
        SOURCES services/TronUsdtParser.cpp
        SOURCES services/CryptoParser.h
        SOURCES services/CryptoParser.cpp
        SOURCES services/CryptoProvider.h
        SOURCES services/CryptoProvider.cpp
        SOURCES interface/FinanceController.h
        SOURCES interface/FinanceController.cpp
        SOURCES persistence/FinanceRepository.h
        SOURCES persistence/FinanceRepository.cpp
)

qt_add_translations(
    TARGETS appMoneyTracker
    SOURCE_TARGETS appMoneyTracker
    TS_FILES translations/qml_en.ts
    RESOURCE_PREFIX /i18n
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appMoneyTracker PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appMoneyTracker
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appMoneyTracker
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
        Qt6::Network
)

install(TARGETS appMoneyTracker
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

enable_testing()

if (NOT ANDROID)
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(CurrencyConverterTest
    tests/CurrencyConverterTest.cpp
    core/Currency.cpp
    services/CurrencyConverter.cpp
)

target_link_libraries(CurrencyConverterTest
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME CurrencyConverterTest
    COMMAND CurrencyConverterTest
)

add_executable(CurrencyRateCacheTest
    tests/CurrencyRateCacheTest.cpp
    core/Currency.cpp
    services/CurrencyConverter.cpp
    services/CurrencyRateCache.cpp
    services/CbrCurrencyRateProvider.cpp
)

target_link_libraries(CurrencyRateCacheTest
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
)

add_test(
    NAME CurrencyRateCacheTest
    COMMAND CurrencyRateCacheTest
)

add_executable(BalanceCalculatorTest
    tests/BalanceCalculatorTest.cpp
    core/Currency.cpp
    services/CurrencyConverter.cpp
    services/BalanceCalculator.cpp
)

target_link_libraries(BalanceCalculatorTest
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME BalanceCalculatorTest
    COMMAND BalanceCalculatorTest
)

add_executable(TransactionDateFilterTest
    tests/TransactionDateFilterTest.cpp
    services/TransactionDateFilter.cpp
    services/DateSliceCalculator.cpp
)

target_link_libraries(TransactionDateFilterTest
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME TransactionDateFilterTest
    COMMAND TransactionDateFilterTest
)

add_executable(FinanceRepositoryTest
    tests/FinanceRepositoryTest.cpp
    core/Currency.cpp
    persistence/FinanceRepository.cpp
)

target_link_libraries(FinanceRepositoryTest
    PRIVATE
        Qt6::Core
        Qt6::Sql
        Qt6::Test
)

add_test(
    NAME FinanceRepositoryTest
    COMMAND FinanceRepositoryTest
)

add_executable(CsvCodecTest
    tests/CsvCodecTest.cpp
    services/CsvCodec.cpp
)

target_link_libraries(CsvCodecTest
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME CsvCodecTest
    COMMAND CsvCodecTest
)

add_executable(TronUsdtParserTest
    tests/TronUsdtParserTest.cpp
    services/TronUsdtParser.cpp
)

target_link_libraries(TronUsdtParserTest
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME TronUsdtParserTest
    COMMAND TronUsdtParserTest
)

add_executable(CryptoParserTest
    tests/CryptoParserTest.cpp
    services/CryptoParser.cpp
)

target_link_libraries(CryptoParserTest
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(
    NAME CryptoParserTest
    COMMAND CryptoParserTest
)
endif()
