#pragma once

#include <string>

class Product {
private:
    int id_{0};
    std::string name_;
    int categoryId_{0};
    double price_{0.0};
    int quantity_{0};
    int minStockLevel_{5};

    void validateName(const std::string& name);
    void validatePrice(double price);
    void validateQuantity(int quantity);
    void validateMinStockLevel(int minStockLevel);

public:
    Product() = default;

    Product(int id,
            const std::string& name,
            int categoryId,
            double price,
            int quantity,
            int minStockLevel = 5);

    int getId() const noexcept;
    const std::string& getName() const noexcept;
    int getCategoryId() const noexcept;
    double getPrice() const noexcept;
    int getQuantity() const noexcept;
    int getMinStockLevel() const noexcept;

    void setId(int id) noexcept;
    void setName(const std::string& name);
    void setCategoryId(int categoryId) noexcept;
    void setPrice(double price);
    void setQuantity(int quantity);
    void setMinStockLevel(int minStockLevel);

    bool isLowStock() const noexcept;
    double getTotalValue() const noexcept;
};