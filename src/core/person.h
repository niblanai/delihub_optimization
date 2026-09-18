#ifndef PERSON_H
#define PERSON_H

#include <QString>
#include <QList>

class Person {
public:
    virtual ~Person() = default;

    int id() const { return m_id; }
    void setId(int id) { m_id = id; }

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

protected:
    Person() = default;
    Person(int id, const QString& name) : m_id(id), m_name(name) {}

    int m_id = 0;
    QString m_name;
};

#endif // PERSON_H
