#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "Controllers/controller.h"
#include "Services/services.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // Service nhận + parse UART
    Service service;

    // Controller nhận Service để connect sang DashboardModel
    controller controller(&service);

    engine.rootContext()->setContextProperty(
        "controller",
        &controller
        );

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() {
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection
        );

    engine.loadFromModule(
        "Project2",
        "Main"
        );

    // mở UART
    service.open();

    return app.exec();
}
