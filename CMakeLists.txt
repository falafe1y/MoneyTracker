cmake_minimum_required(VERSION 3.16)

project(MoneyTracker VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Sql)

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
        Main.qml
        BankComboBox.qml
        CategoryPicker.qml
        SOURCES core/Account.h
        SOURCES core/Asset.h
        SOURCES core/Category.h
        SOURCES core/Currency.h
        SOURCES core/Currency.cpp
        SOURCES core/Money.h
        SOURCES core/Transaction.h
        SOURCES services/CurrencyConverter.h
        SOURCES services/CurrencyConverter.cpp
        SOURCES services/BalanceCalculator.h
        SOURCES services/BalanceCalculator.cpp
        SOURCES services/CurrencyRateProvider.h
        SOURCES services/TestCurrencyRateProvider.h
        SOURCES interface/FinanceController.h
        SOURCES interface/FinanceController.cpp
        SOURCES persistence/FinanceRepository.h
        SOURCES persistence/FinanceRepository.cpp
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
)

install(TARGETS appMoneyTracker
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

enable_testing()

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
