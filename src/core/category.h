#ifndef CATEGORY_H
#define CATEGORY_H

#include <QString>

struct Category {
    int id = 0;
    QString name;

    bool operator==(const Category& other) const {
        return id == other.id;
    }
};

#endif // CATEGORY_H
