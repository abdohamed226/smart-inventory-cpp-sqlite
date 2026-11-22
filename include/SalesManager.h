#pragma once

#include "DatabaseManager.h"

#include <string>
#include <vector>

struct SaleItemRequest {
    int productId{0};
    int quantity{0};
};

struct SaleReceipt {
    int saleId{0};
    std::string saleDate;
    double totalAmount{0.0};
    int totalItemsSold{0};
};

struct InventorySummary {
    int totalProducts{0};
    int totalUnits{0};
    double totalInventoryValue{0.0};
    int lowStockCount{0};
};

struct SalesSummary {
    int salesCount{0};
    int totalUnitsSold{0};
    double totalRevenue{0.0};
};

struct TopSellingProduct {
    int productId{0};
    std::string productName;
    int unitsSold{0};
    double revenueGenerated{0.0};
};

class SalesManager {
public:
    explicit SalesManager(DatabaseManager& db);

    SaleReceipt recordSale(int productId, int quantity);
    SaleReceipt recordMultiItemSale(const std::vector<SaleItemRequest>& items);

    InventorySummary getInventorySummary();
    SalesSummary getSalesSummary();
    SalesSummary getSalesSummaryByDateRange(const std::string& startDateTime,
                                            const std::string& endDateTime);
    std::vector<TopSellingProduct> getTopSellingProducts(int limit = 5);

private:
    DatabaseManager& db_;
};