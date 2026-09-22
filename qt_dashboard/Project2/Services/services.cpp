#include "services.h"

#include <QIODevice>

Service::Service(QObject *parent)
    : QObject(parent)
{
    connect(
        &m_serial,
        &QSerialPort::readyRead,
        this,
        &Service::onReadyRead
        );

    m_flushTimer.setInterval(
        FLUSH_INTERVAL_MS
        );

    connect(
        &m_flushTimer,
        &QTimer::timeout,
        this,
        &Service::flushFrames
        );
}

bool Service::open()
{
    if (m_serial.isOpen())
        return true;

    m_serial.setPortName("/dev/ttyUSB0");

    m_serial.setBaudRate(
        QSerialPort::Baud115200
        );

    m_serial.setDataBits(
        QSerialPort::Data8
        );

    m_serial.setParity(
        QSerialPort::NoParity
        );

    m_serial.setStopBits(
        QSerialPort::OneStop
        );

    m_serial.setFlowControl(
        QSerialPort::NoFlowControl
        );

    if (!m_serial.open(QIODevice::ReadOnly))
        return false;

    m_rxBuffer.clear();
    m_pendingFrames.clear();

    m_flushTimer.start();

    return true;
}

void Service::close()
{
    m_flushTimer.stop();

    m_rxBuffer.clear();
    m_pendingFrames.clear();

    if (m_serial.isOpen())
        m_serial.close();
}

void Service::onReadyRead()
{
    /*
     * Luôn đọc UART ngay lập tức.
     *
     * Không delay ở đây để tránh serial buffer bị đầy.
     */
    m_rxBuffer += m_serial.readAll();

    /*
     * Tránh buffer tăng vô hạn nếu nhận garbage
     * nhưng không bao giờ có '\n'.
     */
    if (m_rxBuffer.size() > MAX_RX_BUFFER_SIZE &&
        !m_rxBuffer.contains('\n'))
    {
        m_rxBuffer.clear();
        return;
    }

    while (true)
    {
        const qsizetype newline =
            m_rxBuffer.indexOf('\n');

        if (newline < 0)
            break;

        QByteArray line =
            m_rxBuffer.left(newline);

        m_rxBuffer.remove(
            0,
            newline + 1
            );

        if (line.endsWith('\r'))
            line.chop(1);

        /*
         * ACK / NACK / ERR / garbage
         * không quan tâm.
         */
        if (!line.startsWith("CAN:"))
            continue;

        CanFrame frame;

        if (!parseCanFrame(line, frame))
            continue;

        /*
         * Bounded queue.
         *
         * Nếu Qt thật sự bị chậm thì bỏ frame cũ nhất
         * thay vì để RAM tăng vô hạn.
         */
        if (m_pendingFrames.size() >=
            MAX_PENDING_FRAMES)
        {
            m_pendingFrames.dequeue();
        }

        m_pendingFrames.enqueue(frame);
    }
}

void Service::flushFrames()
{
    if (m_pendingFrames.isEmpty())
        return;

    CanFrameList frames;

    frames.reserve(
        m_pendingFrames.size()
        );

    while (!m_pendingFrames.isEmpty())
    {
        frames.append(
            m_pendingFrames.dequeue()
            );
    }

    /*
     * Mỗi 20 ms chỉ emit 1 lần.
     */
    emit canFramesReceived(frames);
}

bool Service::parseCanFrame(
    const QByteArray &line,
    CanFrame &frame) const
{
    /*
     * STM32:
     *
     * CAN:<ID>:<DLC>:<DATA>
     *
     * Ví dụ:
     *
     * CAN:300:7:03030064505A80
     */

    const QList<QByteArray> parts =
        line.split(':');

    if (parts.size() != 4)
        return false;

    if (parts[0] != "CAN")
        return false;

    /*
     * CAN ID HEX
     */
    bool idOk = false;

    const uint id =
        parts[1].toUInt(
            &idOk,
            16
            );

    if (!idOk || id > 0x7FFU)
        return false;

    /*
     * DLC decimal
     */
    bool dlcOk = false;

    const uint dlc =
        parts[2].toUInt(
            &dlcOk,
            10
            );

    if (!dlcOk ||
        dlc == 0U ||
        dlc > 8U)
    {
        return false;
    }

    /*
     * DLC = N
     * DATA phải có N * 2 HEX chars.
     */
    if (parts[3].size() !=
        static_cast<qsizetype>(
            dlc * 2U
            ))
    {
        return false;
    }

    for (const char c : parts[3])
    {
        const bool valid =
            (c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f');

        if (!valid)
            return false;
    }

    const QByteArray data =
        QByteArray::fromHex(
            parts[3]
            );

    if (data.size() !=
        static_cast<qsizetype>(dlc))
    {
        return false;
    }

    frame.id =
        static_cast<quint16>(id);

    frame.dlc =
        static_cast<quint8>(dlc);

    frame.data = data;

    return true;
}
