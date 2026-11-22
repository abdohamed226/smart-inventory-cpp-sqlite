#include "DatabaseManager.h"
#include "InventoryManager.h"
#include "Product.h"
#include "SalesManager.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void clearInputStream() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int readInt(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        int value = 0;
        if (std::cin >> value) {
            clearInputStream();
            return value;
        }
        std::cout << "[Error] Please enter a valid integer.\n";
        clearInputStream();
    }
}

double readDouble(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        double value = 0.0;
        if (std::cin >> value) {
            clearInputStream();
            return value;
        }
        std::cout << "[Error] Please enter a valid number.\n";
        clearInputStream();
    }
}

std::string readLine(const std::string& prompt, bool allowEmpty = false) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        if (allowEmpty || !line.empty()) {
            return line;
        }
        std::cout << "[Error] Input cannot be empty.\n";
    }
}

void printProductsTable(const std::vector<Product>& products) {
    if (products.empty()) {
        std::cout << "No products found.\n";
        return;
    }

    std::cout << "\n"
              << std::left
              << std::setw(6) << "ID"
              << std::setw(24) << "Name"
              << std::setw(8) << "CatID"
              << std::setw(12) << "Price"
              << std::setw(10) << "Qty"
              << std::setw(10) << "MinStock"
              << std::setw(14) << "StockValue"
              << "Status\n";
    std::cout << std::string(86, '-') << "\n";

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& p : products) {
        std::cout << std::left
                  << std::setw(6) << p.getId()
                  << std::setw(24) << p.getName().substr(0, 23)
                  << std::setw(8) << p.getCategoryId()
                  << std::setw(12) << p.getPrice()
                  << std::setw(10) << p.getQuantity()
                  << std::setw(10) << p.getMinStockLevel()
                  << std::setw(14) << p.getTotalValue()
                  << (p.isLowStock() ? "LOW STOCK" : "OK")
                  << "\n";
    }
}

void handleAddProduct(InventoryManager& inv) {
    std::cout << "\n--- Add New Product ---\n";
    std::string name = readLine("Product Name: ");
    int categoryId = readInt("Category ID (0 for none): ");
    double price = readDouble("Price: ");
    int quantity = readInt("Initial Quantity: ");
    int minStock = readInt("Minimum Stock Alert Level (e.g. 5): ");

    Product created = inv.addProduct(name, categoryId, price, quantity, minStock);
    std::cout << "[Success] Product added with ID: " << created.getId() << "\n";
    if (created.isLowStock()) {
        std::cout << "[Alert] This product is already at or below minimum stock level!\n";
    }
}

void handleViewAndSort(InventoryManager& inv) {
    std::cout << "\n--- View & Sort Products ---\n";
    std::cout << "Sort by: 1) ID  2) Name  3) Price  4) Quantity\n";
    int fieldChoice = readInt("Choose field (1-4): ");

    std::cout << "Order: 1) Ascending  2) Descending\n";
    int orderChoice = readInt("Choose order (1-2): ");

    SortField field = SortField::Id;
    switch (fieldChoice) {
        case 2: field = SortField::Name; break;
        case 3: field = SortField::Price; break;
        case 4: field = SortField::Quantity; break;
        default: field = SortField::Id; break;
    }

    SortOrder order = (orderChoice == 2) ? SortOrder::Descending : SortOrder::Ascending;
    auto products = inv.getAllProductsSorted(field, order);
    printProductsTable(products);
}

void handleSearchProduct(InventoryManager& inv) {
    std::cout << "\n--- Search Product ---\n";
    std::cout << "1) Search by ID\n2) Search by Name\n";
    int choice = readInt("Choose option (1-2): ");

    if (choice == 1) {
        int id = readInt("Enter Product ID: ");
        auto found = inv.findProductById(id);
        if (found.has_value()) {
            printProductsTable({*found});
        } else {
            std::cout << "No product found with ID " << id << ".\n";
        }
    } else {
        std::string query = readLine("Enter Name or keyword: ");
        auto matches = inv.searchProductsByName(query);
        printProductsTable(matches);
    }
}

void handleUpdateProduct(InventoryManager& inv) {
    std::cout << "\n--- Update Product ---\n";
    int id = readInt("Enter Product ID to update: ");
    auto existing = inv.findProductById(id);
    if (!existing.has_value()) {
        std::cout << "[Error] Product ID " << id << " not found.\n";
        return;
    }

    Product updated = *existing;
    std::cout << "Current Name: " << updated.getName() << "\n";
    std::string newName = readLine("New Name (or press Enter to keep): ", true);
    if (!newName.empty()) {
        updated.setName(newName);
    }

    int newCat = readInt("New Category ID (current: " + std::to_string(updated.getCategoryId()) + ", 0 for none): ");
    updated.setCategoryId(newCat);

    double newPrice = readDouble("New Price (current: " + std::to_string(updated.getPrice()) + "): ");
    updated.setPrice(newPrice);

    int newQty = readInt("New Quantity (current: " + std::to_string(updated.getQuantity()) + "): ");
    updated.setQuantity(newQty);

    int newMin = readInt("New Min Stock Level (current: " + std::to_string(updated.getMinStockLevel()) + "): ");
    updated.setMinStockLevel(newMin);

    if (inv.updateProduct(updated)) {
        std::cout << "[Success] Product updated.\n";
    } else {
        std::cout << "[Warning] No changes saved.\n";
    }
}

void handleDeleteProduct(InventoryManager& inv) {
    std::cout << "\n--- Delete Product ---\n";
    int id = readInt("Enter Product ID to delete: ");
    std::string confirm = readLine("Are you sure? Type 'yes' to confirm: ");
    if (confirm != "yes") {
        std::cout << "Deletion cancelled.\n";
        return;
    }

    if (inv.deleteProduct(id)) {
        std::cout << "[Success] Product deleted.\n";
    } else {
        std::cout << "[Error] Product ID " << id << " not found.\n";
    }
}

void handleRestock(InventoryManager& inv) {
    std::cout << "\n--- Restock Product ---\n";
    int id = readInt("Enter Product ID: ");
    int qtyToAdd = readInt("Quantity to add: ");

    if (inv.restockProduct(id, qtyToAdd)) {
        std::cout << "[Success] Stock updated.\n";
        if (auto p = inv.findProductById(id); p.has_value()) {
            printProductsTable({*p});
        }
    } else {
        std::cout << "[Error] Product ID " << id << " not found.\n";
    }
}

void handleSell(SalesManager& sales, InventoryManager& inv) {
    std::cout << "\n--- Record Sale ---\n";
    std::vector<SaleItemRequest> cart;

    while (true) {
        int productId = readInt("Enter Product ID: ");
        int qty = readInt("Enter Quantity to sell: ");
        cart.push_back(SaleItemRequest{productId, qty});

        std::string more = readLine("Add another item to this sale? (y/n): ");
        if (more != "y" && more != "Y") {
            break;
        }
    }

    SaleReceipt receipt = sales.recordMultiItemSale(cart);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n[Success] Sale #" << receipt.saleId << " completed!\n"
              << "  Date:       " << receipt.saleDate << "\n"
              << "  Items Sold: " << receipt.totalItemsSold << "\n"
              << "  Total Due:  " << receipt.totalAmount << "\n";

    for (const auto& item : cart) {
        if (auto p = inv.findProductById(item.productId); p.has_value() && p->isLowStock()) {
            std::cout << "[Low Stock Alert] '" << p->getName()
                      << "' has only " << p->getQuantity()
                      << " left (min: " << p->getMinStockLevel() << ")!\n";
        }
    }
}

void handleLowStockAlerts(InventoryManager& inv) {
    std::cout << "\n--- Low Stock Alerts ---\n";
    auto lowStock = inv.getLowStockProducts();
    printProductsTable(lowStock);
}

void handleReports(SalesManager& sales) {
    std::cout << "\n--- Reports ---\n";
    std::cout << "1) Inventory Summary\n"
              << "2) Sales Summary (All Time)\n"
              << "3) Sales Summary by Date Range (YYYY-MM-DD HH:MM:SS)\n"
              << "4) Top Selling Products\n";
    int choice = readInt("Choose report (1-4): ");

    std::cout << std::fixed << std::setprecision(2);
    if (choice == 1) {
        InventorySummary invSum = sales.getInventorySummary();
        std::cout << "\n[Inventory Summary]\n"
                  << "  Total Products:       " << invSum.totalProducts << "\n"
                  << "  Total Units in Stock: " << invSum.totalUnits << "\n"
                  << "  Total Inventory Value:" << invSum.totalInventoryValue << "\n"
                  << "  Low Stock Items:      " << invSum.lowStockCount << "\n";
    } else if (choice == 2) {
        SalesSummary sSum = sales.getSalesSummary();
        std::cout << "\n[All-Time Sales Summary]\n"
                  << "  Completed Sales: " << sSum.salesCount << "\n"
                  << "  Total Units Sold:" << sSum.totalUnitsSold << "\n"
                  << "  Total Revenue:   " << sSum.totalRevenue << "\n";
    } else if (choice == 3) {
        std::string startDt = readLine("Start DateTime (e.g. 2026-11-01 00:00:00): ");
        std::string endDt = readLine("End DateTime   (e.g. 2026-11-30 23:59:59): ");
        SalesSummary rangeSum = sales.getSalesSummaryByDateRange(startDt, endDt);
        std::cout << "\n[Sales Summary (" << startDt << " to " << endDt << ")]\n"
                  << "  Completed Sales: " << rangeSum.salesCount << "\n"
                  << "  Total Units Sold:" << rangeSum.totalUnitsSold << "\n"
                  << "  Total Revenue:   " << rangeSum.totalRevenue << "\n";
    } else if (choice == 4) {
        int limit = readInt("How many top products to show? (e.g. 5): ");
        auto top = sales.getTopSellingProducts(limit);
        if (top.empty()) {
            std::cout << "No sales recorded yet.\n";
            return;
        }
        std::cout << "\n"
                  << std::left
                  << std::setw(6) << "ID"
                  << std::setw(24) << "Product Name"
                  << std::setw(14) << "Units Sold"
                  << std::setw(14) << "Revenue\n";
        std::cout << std::string(58, '-') << "\n";
        for (const auto& entry : top) {
            std::cout << std::left
                      << std::setw(6) << entry.productId
                      << std::setw(24) << entry.productName.substr(0, 23)
                      << std::setw(14) << entry.unitsSold
                      << std::setw(14) << entry.revenueGenerated << "\n";
        }
    } else {
        std::cout << "[Error] Invalid report choice.\n";
    }
}

void printMainMenu() {
    std::cout << "\n========================================\n"
              << "   SMART INVENTORY MANAGEMENT SYSTEM\n"
              << "========================================\n"
              << "1. Add Product\n"
              << "2. View / Sort Products\n"
              << "3. Search Product\n"
              << "4. Update Product\n"
              << "5. Delete Product\n"
              << "6. Restock Product\n"
              << "7. Sell Product\n"
              << "8. Low Stock Alerts\n"
              << "9. Reports\n"
              << "0. Exit\n"
              << "----------------------------------------\n";
}

} // namespace

int main() {
    try {
        fs::create_directories("database");
        const std::string dbPath = "database/inventory.db";

        DatabaseManager db(dbPath);
        db.open();
        db.initializeSchema();

        InventoryManager inventory(db);
        SalesManager sales(db);

        std::cout << "Connected to SQLite database: " << dbPath << "\n";

        while (true) {
            printMainMenu();
            int choice = readInt("Select an option: ");

            try {
                switch (choice) {
                    case 1: handleAddProduct(inventory); break;
                    case 2: handleViewAndSort(inventory); break;
                    case 3: handleSearchProduct(inventory); break;
                    case 4: handleUpdateProduct(inventory); break;
                    case 5: handleDeleteProduct(inventory); break;
                    case 6: handleRestock(inventory); break;
                    case 7: handleSell(sales, inventory); break;
                    case 8: handleLowStockAlerts(inventory); break;
                    case 9: handleReports(sales); break;
                    case 0:
                        std::cout << "Closing database and exiting. Goodbye!\n";
                        db.close();
                        return 0;
                    default:
                        std::cout << "[Error] Please choose a number between 0 and 9.\n";
                        break;
                }
            } catch (const std::invalid_argument& ex) {
                std::cout << "[Validation Error] " << ex.what() << "\n";
            } catch (const std::runtime_error& ex) {
                std::cout << "[Operation Error] " << ex.what() << "\n";
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "[Fatal Error] " << ex.what() << "\n";
        return 1;
    }
}