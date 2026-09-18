#ifndef ADDRESS_H
#define ADDRESS_H

#include <QString>

struct Address {
    QString text;

    bool operator==(const Address& other) const {
        return text == other.text;
    }
};

#endif // ADDRESS_H
