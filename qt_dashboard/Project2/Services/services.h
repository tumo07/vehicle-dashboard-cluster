#ifndef SERVICE_H
#define SERVICE_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QQueue>
#include <QTimer>
#include <QList>

class Service : public QObject
{
    Q_OBJECT

public:
    struct CanFrame
    {
        quint16 id = 0;
        quint8 dlc = 0;
        QByteArray data;
    };

    using CanFrameList = QList<CanFrame>;

    explicit Service(QObject *parent = nullptr);

    bool open();
    void close();

signals:
    void canFramesReceived(
        const Service::CanFrameList &frames
        );

private slots:
    void onReadyRead();
    void flushFrames();

private:
    bool parseCanFrame(
        const QByteArray &line,
        CanFrame &frame
        ) const;

private:
    QSerialPort m_serial;

    QByteArray m_rxBuffer;

    QQueue<CanFrame> m_pendingFrames;

    QTimer m_flushTimer;

    static constexpr int FLUSH_INTERVAL_MS = 20;

    static constexpr int MAX_PENDING_FRAMES = 256;

    static constexpr int MAX_RX_BUFFER_SIZE = 4096;
};

Q_DECLARE_METATYPE(Service::CanFrame)
Q_DECLARE_METATYPE(Service::CanFrameList)

#endif // SERVICE_H
