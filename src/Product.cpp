#include "Product.h"

#include <stdexcept>

void Product::validateName(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Product name cannot be empty.");
    }
}

void Product::validatePrice(double price) {
    if (price < 0.0) {
        throw std::invalid_argument("Product price cannot be negative.");
    }
}

void Product::validateQuantity(int quantity) {
    if (quantity < 0) {
        throw std::invalid_argument("Product quantity cannot be negative.");
    }
}

void Product::validateMinStockLevel(int minStockLevel) {
    if (minStockLevel < 0) {
        throw std::invalid_argument("Minimum stock level cannot be negative.");
    }
}

Product::Product(int id,
                 const std::string& name,
                 int categoryId,
                 double price,
                 int quantity,
                 int minStockLevel) {
    validateName(name);
    validatePrice(price);
    validateQuantity(quantity);
    validateMinStockLevel(minStockLevel);

    id_ = id;
    name_ = name;
    categoryId_ = categoryId;
    price_ = price;
    quantity_ = quantity;
    minStockLevel_ = minStockLevel;
}

int Product::getId() const noexcept {
    return id_;
}

const std::string& Product::getName() const noexcept {
    return name_;
}

int Product::getCategoryId() const noexcept {
    return categoryId_;
}

double Product::getPrice() const noexcept {
    return price_;
}

int Product::getQuantity() const noexcept {
    return quantity_;
}

int Product::getMinStockLevel() const noexcept {
    return minStockLevel_;
}

void Product::setId(int id) noexcept {
    id_ = id;
}

void Product::setName(const std::string& name) {
    validateName(name);
    name_ = name;
}

void Product::setCategoryId(int categoryId) noexcept {
    categoryId_ = categoryId;
}

void Product::setPrice(double price) {
    validatePrice(price);
    price_ = price;
}

void Product::setQuantity(int quantity) {
    validateQuantity(quantity);
    quantity_ = quantity;
}

void Product::setMinStockLevel(int minStockLevel) {
    validateMinStockLevel(minStockLevel);
    minStockLevel_ = minStockLevel;
}

bool Product::isLowStock() const noexcept {
    return quantity_ <= minStockLevel_;
}

double Product::getTotalValue() const noexcept {
    return price_ * static_cast<double>(quantity_);
}