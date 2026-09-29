#ifndef DASHBOARDMODEL_H
#define DASHBOARDMODEL_H

#include <QObject>
#include <QVariantMap>
#include <QHash>

#include "../Services/services.h"
#include "can_messages.h"

class DashboardModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap data READ data NOTIFY dataChanged FINAL)

public:
    // explicit DashboardModel(Service *service, QObject *parent = nullptr);
    explicit DashboardModel(QObject *parent = nullptr);

    QVariantMap data() const;

signals:
    void dataChanged();

public slots:
    void processFrames(const Service::CanFrameList &frames);

private:
    using Handler = bool (DashboardModel::*)(const Service::CanFrame &);

    void initHandlers();

    bool processFrame(const Service::CanFrame &frame);
    bool updateCommandAck(const Service::CanFrame &frame);
    bool updateVehicle(const Service::CanFrame &frame);
    bool updateLights(const Service::CanFrame &frame);
    bool updateTurn(const Service::CanFrame &frame);
    bool updateTrunk(const Service::CanFrame &frame);
    bool updateRadar(const Service::CanFrame &frame);
    bool updateDiagnostic(const Service::CanFrame &frame);
    bool updateFault(const Service::CanFrame &frame);
    bool updateFaultClear(const Service::CanFrame &frame);
    bool updateWatchdog(const Service::CanFrame &frame);
    bool updateHeartbeat(const Service::CanFrame &frame);

    static quint8 u8(char value);
    static quint16 u16BE(const QByteArray &data, int offset);

    QVariantMap m_data;
    QHash<quint16, Handler> m_handlers;
};

#endif // DASHBOARDMODEL_H
