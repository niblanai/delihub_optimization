#ifndef PHONE_H
#define PHONE_H

#include <QString>

struct Phone {
    QString number;

    bool operator==(const Phone& other) const {
        return number == other.number;
    }
};

#endif // PHONE_H
