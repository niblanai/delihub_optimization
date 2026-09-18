# POS UI Redesign - TODO List

## ✅ Done (Phase 2.5)
1. ImagePath added to Product model
2. Database schemas updated (SQLite + PostgreSQL)
3. Migration script added for existing databases

## 🔥 Priority 1: Payment Flow Redesign

### Current Flow (Bad):
```
Cart visible → Click PAY → Dialog pops up → Annoying!
```

### New Flow (Odoo-style):
```
Cart visible → Click PAY → Cart area transforms to payment screen → Select Cash/Visa → Confirm → Show "Print Receipt" button
```

### Implementation Steps:

#### 1. Modify PosWindow cart area to support multiple views:
```cpp
// In pos_window.h
private:
    QStackedWidget* m_cartStack = nullptr;
    QWidget* m_cartView = nullptr;
    QWidget* m_paymentView = nullptr;
    QWidget* m_successView = nullptr;
```

#### 2. Create payment selection widget (NO DIALOG!):
```cpp
// m_paymentView contains:
- Large buttons: [💵 Cash] [💳 Card] [📱 Other]
- Amount display
- Back button
- Confirm button
```

#### 3. Success view with print button:
```cpp
// m_successView contains:
- ✅ Order #123 Completed
- Total: 150.00 ج.م
- [🖨 Print Receipt] button
- [✓ New Order] button
```

#### 4. Remove ALL QMessageBox calls:
```cpp
// Replace with:
- Toast notifications (top-right corner)
- Inline error messages
- Status bar messages
```

---

## 🖼️ Priority 2: Product Images

### 1. Image Upload in Product Dialog:
```cpp
// Add to product_dialog.cpp
QPushButton* m_uploadImageBtn;
QLabel* m_imagePreview;

void onUploadImage() {
    QString filePath = QFileDialog::getOpenFileName(
        this, "Select Product Image",
        QDir::homePath(),
        "Images (*.png *.jpg *.jpeg *.bmp)"
    );
    
    if (!filePath.isEmpty()) {
        // Copy to app images folder
        QDir imagesDir("./images/products");
        if (!imagesDir.exists()) imagesDir.mkpath(".");
        
        QString fileName = QString("%1_%2").arg(product.id()).arg(QFileInfo(filePath).fileName());
        QString destPath = imagesDir.filePath(fileName);
        
        QFile::copy(filePath, destPath);
        product.setImagePath(destPath);
        
        // Show preview
        QPixmap pixmap(destPath);
        m_imagePreview->setPixmap(pixmap.scaled(150, 150, Qt::KeepAspectRatio));
    }
}
```

### 2. Display Images in POS Grid:
```cpp
// In pos_window.cpp loadProducts()
for (const auto& product : m_products) {
    auto* btn = new QPushButton;
    
    // Create vertical layout
    auto* layout = new QVBoxLayout(btn);
    
    // Image
    QLabel* imgLabel = new QLabel;
    if (!product.imagePath().isEmpty() && QFile::exists(product.imagePath())) {
        QPixmap pixmap(product.imagePath());
        imgLabel->setPixmap(pixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // Default placeholder
        imgLabel->setText("📦");
        imgLabel->setStyleSheet("font-size: 48px;");
    }
    imgLabel->setAlignment(Qt::AlignCenter);
    
    // Name + Price
    QLabel* textLabel = new QLabel(QString("%1\n%2").arg(product.name(), formatPrice(product.price())));
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setWordWrap(true);
    
    layout->addWidget(imgLabel);
    layout->addWidget(textLabel);
    
    btn->setLayout(layout);
    btn->setMinimumSize(120, 140);
    // ... add to grid
}
```

### 3. Placeholder Image:
- Create: `resources/images/product_placeholder.png`
- Use when product.imagePath() is empty

---

## 🎨 Priority 3: Odoo-style Clean Design

### Color Scheme:
```css
Primary: #714B67 (purple)
Success: #4CAF50 (green)
Background: #F5F5F5 (light gray)
Card: #FFFFFF (white)
Text: #212121 (dark gray)
Border: #E0E0E0 (light border)
```

### CSS Updates:
```css
/* Product button with image */
.product-btn {
    background: white;
    border: 1px solid #E0E0E0;
    border-radius: 8px;
    padding: 8px;
}

.product-btn:hover {
    border-color: #714B67;
    box-shadow: 0 2px 8px rgba(113, 75, 103, 0.15);
}

.product-btn img {
    border-radius: 4px;
}

/* Payment buttons */
.payment-btn {
    min-height: 120px;
    font-size: 24px;
    border-radius: 12px;
}

.payment-btn-cash {
    background: #4CAF50;
    color: white;
}

.payment-btn-card {
    background: #2196F3;
    color: white;
}

/* Success view */
.success-icon {
    font-size: 64px;
    color: #4CAF50;
}
```

---

## 📝 Migration SQL

### For existing databases:
```sql
-- SQLite
ALTER TABLE Products ADD COLUMN ImagePath TEXT;

-- PostgreSQL
ALTER TABLE products ADD COLUMN imagepath VARCHAR(500);
```

---

## 🚀 Implementation Order

1. ✅ Add ImagePath to Product model (DONE)
2. ✅ Update database schemas (DONE)
3. [ ] Modify POS cart area to use QStackedWidget
4. [ ] Create payment selection view (inline, not dialog)
5. [ ] Create success view with print button
6. [ ] Remove all QMessageBox from POS
7. [ ] Add image upload to Product dialog
8. [ ] Update POS grid to show images
9. [ ] Add placeholder image
10. [ ] Apply Odoo-style CSS

---

## 📦 Files to Modify

### Core:
- ✅ `src/core/product.h` - Add imagePath()

### Infrastructure:
- ✅ `src/infra/database_connection_manager.cpp` - SQLite schema
- ✅ `src/infra/schema_deployer.cpp` - PostgreSQL schema

### Data:
- [ ] `src/data/sqlite_repositories.cpp` - Add ImagePath to save/load
- [ ] `src/data/access_repositories.cpp` - Add ImagePath to save/load

### UI:
- [ ] `src/ui/pos/pos_window.h` - Add QStackedWidget
- [ ] `src/ui/pos/pos_window.cpp` - Redesign payment flow
- [ ] `src/ui/products/product_dialog.h` - Add image upload
- [ ] `src/ui/products/product_dialog.cpp` - Implement upload

### Resources:
- [ ] `resources/images/product_placeholder.png` - Default image

---

## ⏱️ Estimated Time
- Payment flow redesign: 2-3 hours
- Image upload + display: 2-3 hours
- CSS polish: 1 hour
- **Total: 5-7 hours**

---

## 📸 Target Result (Like Odoo POS)

```
┌─────────────────────────────────────────────────────────────────┐
│ Register    Orders    🔍 Search...              📷 💜 ☰         │
├────────────────┬────────────────────────────────────────────────┤
│                │  [📚 Books]  [🎁 Accessories]                  │
│  Cart/Payment  ├────────────────────────────────────────────────┤
│                │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐          │
│  (Stacked)     │  │ 📖   │ │ 📘   │ │ 📗   │ │ 📕   │          │
│                │  │Book A│ │Book B│ │Book C│ │Book D│          │
│                │  │$10.00│ │$15.00│ │$20.00│ │$25.00│          │
│                │  └──────┘ └──────┘ └──────┘ └──────┘          │
│                │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐          │
│                │  │ 📔   │ │ 📓   │ │ 📙   │ │ 📒   │          │
│                │  │Book E│ │Book F│ │Book G│ │Book H│          │
│                │  │$30.00│ │$35.00│ │$40.00│ │$45.00│          │
│                │  └──────┘ └──────┘ └──────┘ └──────┘          │
└────────────────┴────────────────────────────────────────────────┘
```

**When PAY clicked, left side becomes:**
```
┌────────────────┐
│ Select Payment │
│                │
│  ┌──────────┐  │
│  │ 💵 CASH  │  │
│  └──────────┘  │
│                │
│  ┌──────────┐  │
│  │ 💳 CARD  │  │
│  └──────────┘  │
│                │
│  ┌──────────┐  │
│  │ 📱 OTHER │  │
│  └──────────┘  │
│                │
│ Total: $45.00  │
└────────────────┘
```

**After payment:**
```
┌────────────────┐
│  ✅ Success!   │
│                │
│  Order #123    │
│  Total: $45.00 │
│                │
│ ┌────────────┐ │
│ │🖨 Print    │ │
│ │ Receipt    │ │
│ └────────────┘ │
│                │
│ ┌────────────┐ │
│ │✓ New Order │ │
│ └────────────┘ │
└────────────────┘
```
