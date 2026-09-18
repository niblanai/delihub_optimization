#ifndef BRANCH_H
#define BRANCH_H

#include <QString>

struct Branch {
    int id = 0;
    QString name;
    QString address;

    bool operator==(const Branch& other) const {
        return id == other.id;
    }
};

#endif // BRANCH_H
