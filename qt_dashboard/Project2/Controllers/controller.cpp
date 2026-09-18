#include "controller.h"

controller::controller(QObject *parent)
    : QObject(parent)
    , m_model(this)
{
}

model* controller::getModel()
{
    return &m_model;
}
