#pragma once

#include "DatabaseManager.h"
#include "Product.h"

#include <optional>
#include <string>
#include <vector>

enum class SortField {
    Id,
    Name,
    Price,
    Quantity
};

enum class SortOrder {
    Ascending,
    Descending
};

class InventoryManager {
public:
    explicit InventoryManager(DatabaseManager& db);

    Product addProduct(const std::string& name,
                       int categoryId,
                       double price,
                       int quantity,
                       int minStockLevel = 5);

    bool updateProduct(const Product& product);
    bool deleteProduct(int productId);

    std::optional<Product> findProductById(int productId);
    std::vector<Product> searchProductsByName(const std::string& nameQuery);

    std::vector<Product> getAllProductsSorted(SortField field = SortField::Id,
                                              SortOrder order = SortOrder::Ascending);

    bool restockProduct(int productId, int addedQuantity);
    std::vector<Product> getLowStockProducts();

private:
    DatabaseManager& db_;

    static Product mapRowToProduct(sqlite3_stmt* stmt);
    static const char* buildOrderByClause(SortField field, SortOrder order);
};