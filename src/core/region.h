#ifndef REGION_H
#define REGION_H

#include <QString>

struct Region {
    int id = 0;
    QString name;
    double distanceKm   = 0.0;   // Task 1: distance from shop to this region
    double deliveryFee  = 0.0;   // Task 1: delivery fee for this region

    bool operator==(const Region& other) const {
        return id == other.id;
    }
};

#endif // REGION_H
