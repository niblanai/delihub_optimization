#ifndef PRODUCT_SUPPLIER_REPOSITORY_H
#define PRODUCT_SUPPLIER_REPOSITORY_H

#include "core/product_supplier.h"
#include <vector>
#include <QString>

class IProductSupplierRepository {
public:
    virtual ~IProductSupplierRepository() = default;
    
    // Add or update product-supplier relationship
    virtual bool save(const ProductSupplier& ps) = 0;
    
    // Remove product-supplier relationship
    virtual bool remove(int productId, int supplierId) = 0;
    
    // Get all suppliers for a product (ordered by isPreferred DESC, purchasePrice ASC)
    virtual std::vector<ProductSupplier> getByProductId(int productId) = 0;
    
    // Get all products for a supplier (ordered by productName ASC)
    virtual std::vector<ProductSupplier> getBySupplierId(int supplierId) = 0;
    
    // Get preferred supplier for a product (if any)
    virtual ProductSupplier getPreferredSupplier(int productId) = 0;
    
    // Set a supplier as preferred for a product (unsets other preferred suppliers for same product)
    virtual bool setPreferred(int productId, int supplierId) = 0;
};

#endif // PRODUCT_SUPPLIER_REPOSITORY_H
