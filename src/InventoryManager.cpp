#include "InventoryManager.h"

#include <stdexcept>

InventoryManager::InventoryManager(DatabaseManager& db) : db_(db) {}

Product InventoryManager::mapRowToProduct(sqlite3_stmt* stmt) {
    int id = sqlite3_column_int(stmt, 0);

    const unsigned char* rawName = sqlite3_column_text(stmt, 1);
    std::string name = rawName ? reinterpret_cast<const char*>(rawName) : "";

    int categoryId = (sqlite3_column_type(stmt, 2) == SQLITE_NULL)
                         ? 0
                         : sqlite3_column_int(stmt, 2);

    double price = sqlite3_column_double(stmt, 3);
    int quantity = sqlite3_column_int(stmt, 4);
    int minStockLevel = sqlite3_column_int(stmt, 5);

    return Product(id, name, categoryId, price, quantity, minStockLevel);
}

const char* InventoryManager::buildOrderByClause(SortField field, SortOrder order) {
    const bool desc = (order == SortOrder::Descending);

    switch (field) {
        case SortField::Id:
            return desc ? "id DESC" : "id ASC";
        case SortField::Name:
            return desc ? "name COLLATE NOCASE DESC, id ASC" : "name COLLATE NOCASE ASC, id ASC";
        case SortField::Price:
            return desc ? "price DESC, id ASC" : "price ASC, id ASC";
        case SortField::Quantity:
            return desc ? "quantity DESC, id ASC" : "quantity ASC, id ASC";
    }
    return "id ASC";
}

Product InventoryManager::addProduct(const std::string& name,
                                   int categoryId,
                                   double price,
                                   int quantity,
                                   int minStockLevel) {
    Product draft(0, name, categoryId, price, quantity, minStockLevel);

    const std::string sql = R"(
        INSERT INTO Products (name, category_id, price, quantity, min_stock_level)
        VALUES (?, ?, ?, ?, ?);
    )";

    db_.executeUpdate(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, draft.getName().c_str(), -1, SQLITE_TRANSIENT);
        if (draft.getCategoryId() > 0) {
            sqlite3_bind_int(stmt, 2, draft.getCategoryId());
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        sqlite3_bind_double(stmt, 3, draft.getPrice());
        sqlite3_bind_int(stmt, 4, draft.getQuantity());
        sqlite3_bind_int(stmt, 5, draft.getMinStockLevel());
    });

    const int newId = static_cast<int>(db_.getLastInsertRowId());
    draft.setId(newId);
    return draft;
}

bool InventoryManager::updateProduct(const Product& product) {
    if (product.getId() <= 0) {
        throw std::invalid_argument("Valid product ID is required for update.");
    }

    const std::string sql = R"(
        UPDATE Products
        SET name = ?,
            category_id = ?,
            price = ?,
            quantity = ?,
            min_stock_level = ?
        WHERE id = ?;
    )";

    const int affectedRows = db_.executeUpdate(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_text(stmt, 1, product.getName().c_str(), -1, SQLITE_TRANSIENT);
        if (product.getCategoryId() > 0) {
            sqlite3_bind_int(stmt, 2, product.getCategoryId());
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        sqlite3_bind_double(stmt, 3, product.getPrice());
        sqlite3_bind_int(stmt, 4, product.getQuantity());
        sqlite3_bind_int(stmt, 5, product.getMinStockLevel());
        sqlite3_bind_int(stmt, 6, product.getId());
    });

    return affectedRows > 0;
}

bool InventoryManager::deleteProduct(int productId) {
    if (productId <= 0) {
        throw std::invalid_argument("Product ID must be positive.");
    }

    const std::string sql = "DELETE FROM Products WHERE id = ?;";
    const int affectedRows = db_.executeUpdate(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_int(stmt, 1, productId);
    });

    return affectedRows > 0;
}

std::optional<Product> InventoryManager::findProductById(int productId) {
    if (productId <= 0) {
        return std::nullopt;
    }

    const std::string sql = R"(
        SELECT id, name, category_id, price, quantity, min_stock_level
        FROM Products
        WHERE id = ?
        LIMIT 1;
    )";

    std::optional<Product> result;
    db_.query(
        sql,
        [&result](sqlite3_stmt* stmt) { result = mapRowToProduct(stmt); },
        [productId](sqlite3_stmt* stmt) { sqlite3_bind_int(stmt, 1, productId); });

    return result;
}

std::vector<Product> InventoryManager::searchProductsByName(const std::string& nameQuery) {
    if (nameQuery.empty()) {
        return getAllProductsSorted(SortField::Id, SortOrder::Ascending);
    }

    const std::string sql = R"(
        SELECT id, name, category_id, price, quantity, min_stock_level
        FROM Products
        WHERE name LIKE ?
        ORDER BY id ASC;
    )";

    const std::string pattern = "%" + nameQuery + "%";
    std::vector<Product> matches;

    db_.query(
        sql,
        [&matches](sqlite3_stmt* stmt) { matches.push_back(mapRowToProduct(stmt)); },
        [&pattern](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        });

    return matches;
}

std::vector<Product> InventoryManager::getAllProductsSorted(SortField field, SortOrder order) {
    const std::string sql = std::string(R"(
        SELECT id, name, category_id, price, quantity, min_stock_level
        FROM Products
        ORDER BY )") +
                          buildOrderByClause(field, order) + ";";

    std::vector<Product> products;
    db_.query(sql, [&products](sqlite3_stmt* stmt) {
        products.push_back(mapRowToProduct(stmt));
    });

    return products;
}

bool InventoryManager::restockProduct(int productId, int addedQuantity) {
    if (productId <= 0) {
        throw std::invalid_argument("Product ID must be positive.");
    }
    if (addedQuantity <= 0) {
        throw std::invalid_argument("Restock quantity must be greater than zero.");
    }

    const std::string sql = R"(
        UPDATE Products
        SET quantity = quantity + ?
        WHERE id = ?;
    )";

    const int affectedRows = db_.executeUpdate(sql, [&](sqlite3_stmt* stmt) {
        sqlite3_bind_int(stmt, 1, addedQuantity);
        sqlite3_bind_int(stmt, 2, productId);
    });

    return affectedRows > 0;
}

std::vector<Product> InventoryManager::getLowStockProducts() {
    const std::string sql = R"(
        SELECT id, name, category_id, price, quantity, min_stock_level
        FROM Products
        WHERE quantity <= min_stock_level
        ORDER BY quantity ASC, id ASC;
    )";

    std::vector<Product> lowStock;
    db_.query(sql, [&lowStock](sqlite3_stmt* stmt) {
        lowStock.push_back(mapRowToProduct(stmt));
    });

    return lowStock;
}