#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include "Models/model.h"
#include "Models/dashboardmodel.h"

class controller : public QObject
{
    Q_OBJECT

    // Expose model for QML through controller
    Q_PROPERTY(model* model READ getModel CONSTANT)
    Q_PROPERTY(DashboardModel* dashboardModel READ getDashboardModel CONSTANT)

public:
    explicit controller(QObject *parent = nullptr);
    model* getModel();
    DashboardModel *getDashboardModel() const;

private:
    model m_model;
    DashboardModel *m_dashboardModel = nullptr;

signals:
};

#endif // CONTROLLER_H
