#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include "Models/model.h"
#include "Models/dashboardmodel.h"
#include "Services/services.h"

class controller : public QObject
{
    Q_OBJECT

    // Expose model for QML through controller
    Q_PROPERTY(model* model READ getModel CONSTANT)
    Q_PROPERTY(model* modelPassenger READ getModelPassenger CONSTANT)
    Q_PROPERTY(DashboardModel* dashboardModel READ getDashboardModel CONSTANT)

public:
    explicit controller(
        Service *service,
        QObject *parent = nullptr
        );
    model* getModel();
    model* getModelPassenger();
    DashboardModel *getDashboardModel();
    Q_INVOKABLE void incrementOutdoorTemp(const int &increment);
    Q_INVOKABLE void incrementVolume(const int &increment);

private:
    model m_model;
    model m_modelPassenger;
    DashboardModel m_dashboardModel;

signals:
};

#endif // CONTROLLER_H
