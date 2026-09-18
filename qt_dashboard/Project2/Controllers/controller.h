#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include "Models/model.h"

class controller : public QObject
{
    Q_OBJECT

    // Expose model for QML through controller
    Q_PROPERTY(model* model READ getModel CONSTANT)

public:
    explicit controller(QObject *parent = nullptr);
    model* getModel();

private:
    model m_model;

signals:
};

#endif // CONTROLLER_H
