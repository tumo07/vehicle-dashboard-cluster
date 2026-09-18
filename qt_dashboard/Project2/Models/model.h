#ifndef MODEL_H
#define MODEL_H

#include <QObject>
#include <QTimer>

class model : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool carLocked READ carLocked WRITE setCarLocked NOTIFY carLockedChanged)
    Q_PROPERTY(int outdoorTemp READ outdoorTemp WRITE setOutdoorTemp NOTIFY outdoorTempChanged)
    Q_PROPERTY(QString userName READ userName WRITE setUserName NOTIFY userNameChanged)
    Q_PROPERTY(QString currentTime READ currentTime WRITE setCurrentTime NOTIFY currentTimeChanged)

public:
    explicit model(QObject *parent = nullptr);

    bool carLocked() const;
    int outdoorTemp() const;
    QString userName() const;
    QString currentTime() const;

signals:
    void carLockedChanged(bool carLocked);
    void outdoorTempChanged(int outdoorTemp);
    void userNameChanged(QString userName);
    void currentTimeChanged();

public slots:
    void setCarLocked(bool carLocked);
    void setOutdoorTemp(int outdoorTemp);
    void setUserName(QString userName);
    void setCurrentTime(const QString &newCurrentTime);
    void timerTimeout();

private:
    bool m_carLocked;
    int m_outdoorTemp;
    QString m_userName;
    QString m_currentTime;
    QTimer * m_timer;
};

#endif // MODEL_H
