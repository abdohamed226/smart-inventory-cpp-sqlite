cmake_minimum_required(VERSION 3.15)
project(SmartInventory VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)

include_directories(include)

add_executable(smart_inventory
    src/main.cpp
    src/Product.cpp
    src/DatabaseManager.cpp
    src/InventoryManager.cpp
    src/SalesManager.cpp
)

target_link_libraries(smart_inventory PRIVATE SQLite::SQLite3)