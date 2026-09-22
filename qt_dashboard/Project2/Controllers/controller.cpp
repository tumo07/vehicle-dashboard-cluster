#include "controller.h"

controller::controller(
    Service *service,
    QObject *parent)
    : QObject(parent)
    , m_model(this)
    , m_modelPassenger(this)
    , m_dashboardModel(this)
{
    connect(
        service,
        &Service::canFramesReceived,
        &m_dashboardModel,
        &DashboardModel::processFrames
        );
}

model* controller::getModel()
{
    return &m_model;
}

DashboardModel *controller::getDashboardModel()
{
    return &m_dashboardModel;
}

void controller::incrementOutdoorTemp(const int &increment)
{
    int newTemp = m_model.outdoorTemp() + increment;
    m_model.setHVacTemp(newTemp);
}

void controller::incrementVolume(const int &increment)
{
    int newVolume = m_model.volumeLevel() + increment;
    m_model.setVolumeLevel(newVolume);
}

model* controller::getModelPassenger()
{
    return &m_modelPassenger;
}
