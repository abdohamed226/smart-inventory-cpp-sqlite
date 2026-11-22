#include "SalesManager.h"

#include <stdexcept>
#include <utility>

namespace {

struct ProductStockInfo {
    bool found{false};
    std::string name;
    double price{0.0};
    int quantity{0};
};

ProductStockInfo fetchProductStock(DatabaseManager& db, int productId) {
    const std::string sql = R"(
        SELECT name, price, quantity
        FROM Products
        WHERE id = ?
        LIMIT 1;
    )";

    ProductStockInfo info;
    db.query(
        sql,
        [&info](sqlite3_stmt* stmt) {
            info.found = true;
            const unsigned char* rawName = sqlite3_column_text(stmt, 0);
            info.name = rawName ? reinterpret_cast<const char*>(rawName) : "";
            info.price = sqlite3_column_double(stmt, 1);
            info.quantity = sqlite3_column_int(stmt, 2);
        },
        [productId](sqlite3_stmt* stmt) { sqlite3_bind_int(stmt, 1, productId); });

    return info;
}

std::string fetchSaleTimestamp(DatabaseManager& db, int saleId) {
    const std::string sql = R"(
        SELECT sale_date
        FROM Sales
        WHERE id = ?
        LIMIT 1;
    )";

    std::string timestamp;
    db.query(
        sql,
        [&timestamp](sqlite3_stmt* stmt) {
            const unsigned char* rawDate = sqlite3_column_text(stmt, 0);
            timestamp = rawDate ? reinterpret_cast<const char*>(rawDate) : "";
        },
        [saleId](sqlite3_stmt* stmt) { sqlite3_bind_int(stmt, 1, saleId); });

    return timestamp;
}

} // namespace

SalesManager::SalesManager(DatabaseManager& db) : db_(db) {}

SaleReceipt SalesManager::recordSale(int productId, int quantity) {
    if (quantity <= 0) {
        throw std::invalid_argument("Sale quantity must be greater than zero.");
    }
    return recordMultiItemSale({SaleItemRequest{productId, quantity}});
}

SaleReceipt SalesManager::recordMultiItemSale(const std::vector<SaleItemRequest>& items) {
    if (items.empty()) {
        throw std::invalid_argument("Sale must contain at least one item.");
    }

    DatabaseManager::TransactionGuard tx(db_);

    struct ResolvedItem {
        int productId;
        int quantity;
        double unitPrice;
        double lineTotal;
    };

    std::vector<ResolvedItem> resolvedItems;
    resolvedItems.reserve(items.size());

    double computedTotal = 0.0;
    int totalUnits = 0;

    for (const auto& req : items) {
        if (req.productId <= 0) {
            throw std::invalid_argument("Invalid product ID in sale request.");
        }
        if (req.quantity <= 0) {
            throw std::invalid_argument("Sale quantity must be greater than zero.");
        }

        ProductStockInfo stock = fetchProductStock(db_, req.productId);
        if (!stock.found) {
            throw std::runtime_error("Product ID " + std::to_string(req.productId) + " not found.");
        }
        if (stock.quantity < req.quantity) {
            throw std::runtime_error(
                "Insufficient stock for '" + stock.name + "' (ID: " +
                std::to_string(req.productId) + "). Available: " +
                std::to_string(stock.quantity) + ", Requested: " +
                std::to_string(req.quantity));
        }

        const double lineTotal = stock.price * static_cast<double>(req.quantity);
        computedTotal += lineTotal;
        totalUnits += req.quantity;

        resolvedItems.push_back(ResolvedItem{
            req.productId,
            req.quantity,
            stock.price,
            lineTotal});
    }

    db_.executeUpdate(
        "INSERT INTO Sales (total_amount) VALUES (?);",
        [computedTotal](sqlite3_stmt* stmt) {
            sqlite3_bind_double(stmt, 1, computedTotal);
        });

    const int saleId = static_cast<int>(db_.getLastInsertRowId());

    for (const auto& item : resolvedItems) {
        db_.executeUpdate(
            R"(
                INSERT INTO SaleItems (sale_id, product_id, quantity, unit_price)
                VALUES (?, ?, ?, ?);
            )",
            [saleId, &item](sqlite3_stmt* stmt) {
                sqlite3_bind_int(stmt, 1, saleId);
                sqlite3_bind_int(stmt, 2, item.productId);
                sqlite3_bind_int(stmt, 3, item.quantity);
                sqlite3_bind_double(stmt, 4, item.unitPrice);
            });

        const int updated = db_.executeUpdate(
            R"(
                UPDATE Products
                SET quantity = quantity - ?
                WHERE id = ? AND quantity >= ?;
            )",
            [&item](sqlite3_stmt* stmt) {
                sqlite3_bind_int(stmt, 1, item.quantity);
                sqlite3_bind_int(stmt, 2, item.productId);
                sqlite3_bind_int(stmt, 3, item.quantity);
            });

        if (updated == 0) {
            throw std::runtime_error(
                "Stock concurrency check failed for product ID " + std::to_string(item.productId));
        }
    }

    tx.commit();

    SaleReceipt receipt;
    receipt.saleId = saleId;
    receipt.saleDate = fetchSaleTimestamp(db_, saleId);
    receipt.totalAmount = computedTotal;
    receipt.totalItemsSold = totalUnits;
    return receipt;
}

InventorySummary SalesManager::getInventorySummary() {
    InventorySummary summary;

    const std::string statsSql = R"(
        SELECT
            COUNT(*),
            COALESCE(SUM(quantity), 0),
            COALESCE(SUM(price * quantity), 0.0)
        FROM Products;
    )";

    db_.query(statsSql, [&summary](sqlite3_stmt* stmt) {
        summary.totalProducts = sqlite3_column_int(stmt, 0);
        summary.totalUnits = sqlite3_column_int(stmt, 1);
        summary.totalInventoryValue = sqlite3_column_double(stmt, 2);
    });

    const std::string lowStockSql = R"(
        SELECT COUNT(*)
        FROM Products
        WHERE quantity <= min_stock_level;
    )";

    db_.query(lowStockSql, [&summary](sqlite3_stmt* stmt) {
        summary.lowStockCount = sqlite3_column_int(stmt, 0);
    });

    return summary;
}

SalesSummary SalesManager::getSalesSummary() {
    SalesSummary summary;

    const std::string salesSql = R"(
        SELECT
            COUNT(*),
            COALESCE(SUM(total_amount), 0.0)
        FROM Sales;
    )";

    db_.query(salesSql, [&summary](sqlite3_stmt* stmt) {
        summary.salesCount = sqlite3_column_int(stmt, 0);
        summary.totalRevenue = sqlite3_column_double(stmt, 1);
    });

    const std::string unitsSql = R"(
        SELECT COALESCE(SUM(quantity), 0)
        FROM SaleItems;
    )";

    db_.query(unitsSql, [&summary](sqlite3_stmt* stmt) {
        summary.totalUnitsSold = sqlite3_column_int(stmt, 0);
    });

    return summary;
}

SalesSummary SalesManager::getSalesSummaryByDateRange(const std::string& startDateTime,
                                                      const std::string& endDateTime) {
    if (startDateTime.empty() || endDateTime.empty()) {
        throw std::invalid_argument("Start and end date/time strings cannot be empty.");
    }

    SalesSummary summary;

    const std::string salesSql = R"(
        SELECT
            COUNT(*),
            COALESCE(SUM(total_amount), 0.0)
        FROM Sales
        WHERE sale_date BETWEEN ? AND ?;
    )";

    db_.query(
        salesSql,
        [&summary](sqlite3_stmt* stmt) {
            summary.salesCount = sqlite3_column_int(stmt, 0);
            summary.totalRevenue = sqlite3_column_double(stmt, 1);
        },
        [&startDateTime, &endDateTime](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, startDateTime.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, endDateTime.c_str(), -1, SQLITE_TRANSIENT);
        });

    const std::string unitsSql = R"(
        SELECT COALESCE(SUM(si.quantity), 0)
        FROM SaleItems si
        JOIN Sales s ON si.sale_id = s.id
        WHERE s.sale_date BETWEEN ? AND ?;
    )";

    db_.query(
        unitsSql,
        [&summary](sqlite3_stmt* stmt) {
            summary.totalUnitsSold = sqlite3_column_int(stmt, 0);
        },
        [&startDateTime, &endDateTime](sqlite3_stmt* stmt) {
            sqlite3_bind_text(stmt, 1, startDateTime.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, endDateTime.c_str(), -1, SQLITE_TRANSIENT);
        });

    return summary;
}

std::vector<TopSellingProduct> SalesManager::getTopSellingProducts(int limit) {
    if (limit <= 0) {
        throw std::invalid_argument("Report limit must be greater than zero.");
    }

    const std::string sql = R"(
        SELECT
            si.product_id,
            COALESCE(p.name, 'Deleted Product') AS product_name,
            COALESCE(SUM(si.quantity), 0) AS units_sold,
            COALESCE(SUM(si.quantity * si.unit_price), 0.0) AS revenue
        FROM SaleItems si
        LEFT JOIN Products p ON si.product_id = p.id
        GROUP BY si.product_id, p.name
        ORDER BY units_sold DESC, revenue DESC
        LIMIT ?;
    )";

    std::vector<TopSellingProduct> topProducts;
    db_.query(
        sql,
        [&topProducts](sqlite3_stmt* stmt) {
            TopSellingProduct entry;
            entry.productId = sqlite3_column_int(stmt, 0);
            const unsigned char* rawName = sqlite3_column_text(stmt, 1);
            entry.productName = rawName ? reinterpret_cast<const char*>(rawName) : "";
            entry.unitsSold = sqlite3_column_int(stmt, 2);
            entry.revenueGenerated = sqlite3_column_double(stmt, 3);
            topProducts.push_back(std::move(entry));
        },
        [limit](sqlite3_stmt* stmt) { sqlite3_bind_int(stmt, 1, limit); });

    return topProducts;
}