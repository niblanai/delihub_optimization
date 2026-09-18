#ifndef STOCK_MOVEMENT_H
#define STOCK_MOVEMENT_H

#include <QString>
#include <QDateTime>

struct StockMovement {
    int      id          = 0;
    int      productId   = 0;
    QString  productName;          // helper for display
    
    // Phase 1: Unified movement tracking
    QString  movementType;         // "Sale", "Purchase", "Return", "Stocktake", "Manual"
    int      quantity    = 0;      // Always positive - the quantity moved
    QString  referenceType;        // "Order", "PurchaseInvoice", "Return", "StockCount", "Manual"
    int      referenceId = 0;      // ID of the related record (order, invoice, etc.)
    
    QDateTime dateTime;
    int      userId      = 0;
    QString  notes;
    
    // Backward compatibility (deprecated - use movementType instead)
    int quantityChange() const { 
        // Convert to old format: positive for in, negative for out
        if (movementType == "Purchase" || movementType == "Return" || movementType == "StocktakeAdjustmentIn")
            return quantity;
        else if (movementType == "Sale" || movementType == "StocktakeAdjustmentOut")
            return -quantity;
        return quantity;
    }
    
    QString reason() const { return movementType + " " + referenceType; }
    int orderId() const { return (referenceType == "Order") ? referenceId : 0; }
    QDateTime timestamp() const { return dateTime; }

    // Convenience
    bool isInbound()  const { 
        return movementType == "Purchase" || movementType == "Return" || 
               movementType == "StocktakeAdjustmentIn" || movementType == "ManualIn";
    }
    bool isOutbound() const { 
        return movementType == "Sale" || movementType == "StocktakeAdjustmentOut" || 
               movementType == "ManualOut";
    }
};

#endif // STOCK_MOVEMENT_H
