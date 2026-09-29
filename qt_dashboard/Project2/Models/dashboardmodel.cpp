#include "dashboardmodel.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

DashboardModel::DashboardModel(QObject *parent)
    : QObject(parent)
{
    /*
     * Default state
     */
    m_data = {
        {"commandId", 0},
        {"commandAckStatus", ACK_STATUS_PENDING},
        {"validationResult", VALIDATION_OK},

        {"gear", GEAR_UNKNOWN},
        {"engineRunning", false},
        {"vehicleMoving", false},
        {"parkBrakeOn", false},
        {"seatbeltOk", false},
        {"trunkAjar", false},
        {"dtcActive", false},
        {"hazardActive", false},
        {"reverseActive", false},

        {"speedKmh", 50},
        {"fuelPercent", 23},
        {"coolantTempC", 73},
        {"batteryVoltage", 12.6},

        {"drlOn", false},
        {"headlightOn", false},
        {"highBeamOn", false},
        {"fogOn", false},
        {"turnLeftOn", false},
        {"turnRightOn", false},
        {"hazardOn", false},
        {"brakeOn", false},

        {"wiperMode", CMD_WIPER_OFF},

        {"leftBlinkOn", false},
        {"rightBlinkOn", false},

        {"trunkOpenPercent", 0},
        {"trunkMotorState", TRUNK_UNKNOWN},

        {"radarDistanceCm", DIST_NO_OBJECT},
        {"parkingLevel", PARKING_CLEAR},
        {"radarObjectDetected", false},

        {"diagCommand", 0},
        {"diagFrameIndex", 0},
        {"diagTotalFrames", 0},
        {"diagPayload", QString()},

        {"faultActive", false},
        {"faultSourceNode", NODE_ID_CENTRAL_ECU},
        {"faultSeverity", DTC_SEVERITY_INFO},
        {"faultDtcCode", DTC_B0000},
        {"faultOccurrenceCounter", 0},
        {"faultErrorCode", ERR_NONE},

        {"lastClearedDtc", 0},

        {"watchdogNodeId", 0},
        {"watchdogSeconds", 0},

        {"centralHeartbeatCounter", 0},
        {"centralInitOk", false},
        {"centralCanOk", false},
        {"centralSensorsOk", false},
        {"centralDtcActive", false},

        {"frontHeartbeatCounter", 0},
        {"frontInitOk", false},
        {"frontCanOk", false},
        {"frontSensorsOk", false},
        {"frontDtcActive", false},

        {"rearHeartbeatCounter", 0},
        {"rearInitOk", false},
        {"rearCanOk", false},
        {"rearSensorsOk", false},
        {"rearDtcActive", false}
    };

    initHandlers();    
}

QVariantMap DashboardModel::data() const
{
    return m_data;
}

/* =========================================================
 * CAN ID -> handler mapping
 *
 * Tất cả ID lấy trực tiếp từ can_messages.h
 * ========================================================= */

void DashboardModel::initHandlers()
{
    m_handlers = {
        {
            CAN_ID_CMD_ACK,
            &DashboardModel::updateCommandAck
        },
        {
            CAN_ID_STATUS_VEHICLE_STATE,
            &DashboardModel::updateVehicle
        },
        {
            CAN_ID_STATUS_LIGHTS_STATE,
            &DashboardModel::updateLights
        },
        {
            CAN_ID_STATUS_TURN_BLINK,
            &DashboardModel::updateTurn
        },
        {
            CAN_ID_STATUS_TRUNK_STATE,
            &DashboardModel::updateTrunk
        },
        {
            CAN_ID_STATUS_REVERSE_RADAR,
            &DashboardModel::updateRadar
        },
        {
            CAN_ID_STATUS_DIAG_RESPONSE,
            &DashboardModel::updateDiagnostic
        },
        {
            CAN_ID_BANNER_FAULT,
            &DashboardModel::updateFault
        },
        {
            CAN_ID_BANNER_CLEAR,
            &DashboardModel::updateFaultClear
        },
        {
            CAN_ID_WATCHDOG_ALERT,
            &DashboardModel::updateWatchdog
        },

        // 3 heartbeat dùng chung decoder
        {
            CAN_ID_HEARTBEAT_CENTRAL,
            &DashboardModel::updateHeartbeat
        },
        {
            CAN_ID_HEARTBEAT_FRONT_BCM,
            &DashboardModel::updateHeartbeat
        },
        {
            CAN_ID_HEARTBEAT_REAR_BCM,
            &DashboardModel::updateHeartbeat
        }
    };
}

/* =========================================================
 * Batch từ Service
 * ========================================================= */

void DashboardModel::processFrames(
    const Service::CanFrameList &frames)
{
    bool changed = false;

    for (const auto &frame : frames)
    {
        if (processFrame(frame))
            changed = true;

            qDebug().noquote()
                << "[DECODED FRAME]"
                << "ID:"
                << QString("0x%1")
                       .arg(frame.id, 3, 16, QChar('0'))
                       .toUpper()
                << "\n"
                << QJsonDocument::fromVariant(m_data)
                       .toJson(QJsonDocument::Indented);
    }

    /*
     * 20 frame tới cũng chỉ notify QML 1 lần.
     * Data cuối cùng là latest data.
     */
    if (changed)
        emit dataChanged();
}

/* =========================================================
 * Không còn switch-case
 * ========================================================= */

bool DashboardModel::processFrame(
    const Service::CanFrame &frame)
{
    const auto it =
        m_handlers.constFind(frame.id);

    if (it == m_handlers.constEnd())
        return false;

    const Handler handler =
        it.value();

    return (this->*handler)(frame);
}

/* =========================================================
 * 0x105 Command ACK
 * ========================================================= */

bool DashboardModel::updateCommandAck(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 3U)
        return false;

    m_data["commandId"] =
        u8(frame.data[0]);

    m_data["commandAckStatus"] =
        u8(frame.data[1]);

    m_data["validationResult"] =
        u8(frame.data[2]);

    return true;
}

/* =========================================================
 * 0x300 Vehicle
 * ========================================================= */

bool DashboardModel::updateVehicle(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 7U)
        return false;

    const quint8 flags =
        u8(frame.data[1]);

    m_data["gear"] =
        u8(frame.data[0]);

    m_data["engineRunning"] =
        (flags & STATE_ENGINE_RUNNING) != 0U;

    m_data["vehicleMoving"] =
        (flags & STATE_VEHICLE_MOVING) != 0U;

    m_data["parkBrakeOn"] =
        (flags & STATE_PARK_BRAKE_ON) != 0U;

    m_data["seatbeltOk"] =
        (flags & STATE_SEATBELT_OK) != 0U;

    m_data["trunkAjar"] =
        (flags & STATE_TRUNK_AJAR) != 0U;

    m_data["dtcActive"] =
        (flags & STATE_DTC_ACTIVE) != 0U;

    m_data["hazardActive"] =
        (flags & STATE_HAZARD_ACTIVE) != 0U;

    m_data["reverseActive"] =
        (flags & STATE_REVERSE_ACTIVE) != 0U;

    const quint16 speed =
        u16BE(frame.data, 2);

    m_data["speedKmh"] =
        DECODE_SPEED(speed);

    m_data["fuelPercent"] =
        u8(frame.data[4]);

    m_data["coolantTempC"] =
        DECODE_TEMP(
            u8(frame.data[5])
            );

    m_data["batteryVoltage"] =
        DECODE_VOLTAGE(
            u8(frame.data[6])
            );

    return true;
}

/* =========================================================
 * 0x301 Light + Wiper
 * ========================================================= */

bool DashboardModel::updateLights(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 2U)
        return false;

    const quint8 flags =
        u8(frame.data[0]);

    m_data["drlOn"] =
        (flags & STATUS_DRL_ON) != 0U;

    m_data["headlightOn"] =
        (flags & STATUS_HEADLIGHT_ON) != 0U;

    m_data["highBeamOn"] =
        (flags & STATUS_HIGH_BEAM_ON) != 0U;

    m_data["fogOn"] =
        (flags & STATUS_FOG_ON) != 0U;

    m_data["turnLeftOn"] =
        (flags & STATUS_TURN_LEFT_ON) != 0U;

    m_data["turnRightOn"] =
        (flags & STATUS_TURN_RIGHT_ON) != 0U;

    m_data["hazardOn"] =
        (flags & STATUS_HAZARD_ON) != 0U;

    m_data["brakeOn"] =
        (flags & STATUS_BRAKE_ON) != 0U;

    m_data["wiperMode"] =
        u8(frame.data[1]);

    return true;
}


/* =========================================================
 * 0x302 Turn blink
 * ========================================================= */

bool DashboardModel::updateTurn(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 1U)
        return false;

    const quint8 value =
        u8(frame.data[0]);

    m_data["leftBlinkOn"] =
        (value & 0x01U) != 0U;

    m_data["rightBlinkOn"] =
        (value & 0x02U) != 0U;

    return true;
}


/* =========================================================
 * 0x303 Trunk
 * ========================================================= */

bool DashboardModel::updateTrunk(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 2U)
        return false;

    m_data["trunkOpenPercent"] =
        u8(frame.data[0]);

    m_data["trunkMotorState"] =
        u8(frame.data[1]);

    return true;
}

/* =========================================================
 * 0x304 Radar
 * ========================================================= */

bool DashboardModel::updateRadar(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 4U)
        return false;

    const quint16 distance =
        u16BE(frame.data, 0);

    m_data["radarDistanceCm"] =
        distance;

    m_data["parkingLevel"] =
        u8(frame.data[2]);

    m_data["radarObjectDetected"] =
        distance != DIST_NO_OBJECT;

    return true;
}

/* =========================================================
 * 0x305 Diagnostic
 * ========================================================= */

bool DashboardModel::updateDiagnostic(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 8U)
        return false;

    m_data["diagCommand"] =
        u8(frame.data[0]);

    m_data["diagFrameIndex"] =
        u8(frame.data[1]);

    m_data["diagTotalFrames"] =
        u8(frame.data[2]);

    m_data["diagPayload"] =
        QString::fromLatin1(
            frame.data
                .mid(3, 5)
                .toHex()
                .toUpper()
            );

    return true;
}

/* =========================================================
 * 0x600 Fault
 * ========================================================= */

bool DashboardModel::updateFault(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 6U)
        return false;

    m_data["faultActive"] = true;

    m_data["faultSourceNode"] =
        u8(frame.data[0]);

    m_data["faultSeverity"] =
        u8(frame.data[1]);

    m_data["faultDtcCode"] =
        u16BE(frame.data, 2);

    m_data["faultOccurrenceCounter"] =
        u8(frame.data[4]);

    m_data["faultErrorCode"] =
        u8(frame.data[5]);

    return true;
}

/* =========================================================
 * 0x601 Fault clear
 * ========================================================= */

bool DashboardModel::updateFaultClear(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 2U)
        return false;

    const quint16 dtc =
        u16BE(frame.data, 0);

    m_data["lastClearedDtc"] =
        dtc;

    const quint16 current =
        static_cast<quint16>(
            m_data["faultDtcCode"].toUInt()
            );

    if (dtc == 0U ||
        dtc == current)
    {
        m_data["faultActive"] =
            false;
    }

    return true;
}

/* =========================================================
 * 0x610 Watchdog
 * ========================================================= */

bool DashboardModel::updateWatchdog(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 2U)
        return false;

    m_data["watchdogNodeId"] =
        u8(frame.data[0]);

    m_data["watchdogSeconds"] =
        u8(frame.data[1]);

    return true;
}

/* =========================================================
 * Heartbeat
 * ========================================================= */

bool DashboardModel::updateHeartbeat(
    const Service::CanFrame &frame)
{
    if (frame.dlc != 2U)
        return false;

    const quint8 counter =
        u8(frame.data[0]);

    const quint8 flags =
        u8(frame.data[1]);

    QString prefix;

    if (frame.id ==
        CAN_ID_HEARTBEAT_CENTRAL)
    {
        prefix = "central";
    }
    else if (frame.id ==
             CAN_ID_HEARTBEAT_FRONT_BCM)
    {
        prefix = "front";
    }
    else
    {
        prefix = "rear";
    }

    m_data[prefix + "HeartbeatCounter"] =
        counter;

    m_data[prefix + "InitOk"] =
        (flags & HB_INIT_OK) != 0U;

    m_data[prefix + "CanOk"] =
        (flags & HB_CAN_OK) != 0U;

    m_data[prefix + "SensorsOk"] =
        (flags & HB_SENSORS_OK) != 0U;

    m_data[prefix + "DtcActive"] =
        (flags & HB_DTC_ACTIVE) != 0U;

    return true;
}

/* =========================================================
 * Helpers
 * ========================================================= */

quint8 DashboardModel::u8(char value)
{
    return static_cast<quint8>(
        static_cast<unsigned char>(
            value
            )
        );
}

quint16 DashboardModel::u16BE(
    const QByteArray &data,
    int offset)
{
    if (offset < 0 ||
        offset + 1 >= data.size())
    {
        return 0U;
    }

    return PACK_U16(
        u8(data[offset]),
        u8(data[offset + 1])
        );
}
