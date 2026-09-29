#include "model.h"
#include <QDateTime>

model::model(QObject *parent)
    : QObject(parent)
    , m_carLocked(true)
    , m_outdoorTemp(20)
    , m_userName("John Doe")
    , m_hVacTemp(22)
    , m_volumeLevel(50)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000); // 1 second interval
    m_timer->setSingleShot(true); // Set the timer to single shot
    connect(m_timer, &QTimer::timeout, this, &model::timerTimeout);
    timerTimeout(); // Call the timeout function immediately to set the initial time
}

bool model::carLocked() const
{
    return m_carLocked;
}

void model::setCarLocked(bool carLocked)
{
    if (m_carLocked == carLocked)
        return;
    m_carLocked = carLocked;
    emit carLockedChanged(m_carLocked);
}

int model::outdoorTemp() const
{
    return m_outdoorTemp;
}

void model::setOutdoorTemp(int newOutdoorTemp)
{
    if (m_outdoorTemp == newOutdoorTemp)
        return;
    m_outdoorTemp = newOutdoorTemp;
    emit outdoorTempChanged(m_outdoorTemp);
}

QString model::userName() const
{
    return m_userName;
}

void model::setUserName(QString userName)
{
    if (m_userName == userName)
        return;
    m_userName = userName;
    emit userNameChanged(m_userName);
}

QString model::currentTime() const
{
    return m_currentTime;
}

void model::setCurrentTime(const QString &newCurrentTime)
{
    if (m_currentTime == newCurrentTime)
        return;
    m_currentTime = newCurrentTime;
    emit currentTimeChanged();
}

void model::timerTimeout()
{
    QString currentTime = QDateTime::currentDateTime().toString("hh:mm AP");
    setCurrentTime(currentTime);
    m_timer->start(); // Restart the timer for the next second
}

int model::hVacTemp() const
{
    return m_hVacTemp;
}

void model::setHVacTemp(int newHVacTemp)
{
    if (newHVacTemp < 17 ||
        newHVacTemp > 29 ||
        m_hVacTemp == newHVacTemp)
    {
        return;
    }
    m_hVacTemp = newHVacTemp;
    emit hVacTempChanged();
}

int model::volumeLevel() const
{
    return m_volumeLevel;
}

void model::setVolumeLevel(int newVolumeLevel)
{
    if (newVolumeLevel < 0)
        newVolumeLevel = 0;
    else if (newVolumeLevel > 100)
        newVolumeLevel = 100;
    m_volumeLevel = newVolumeLevel;
    emit volumeLevelChanged();
}
