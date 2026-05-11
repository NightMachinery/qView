#include "mainwindow.h"
#include "qvapplication.h"
#include "qvipcserver.h"
#include "qvwin32functions.h"

#include <QCommandLineParser>
#include <QTextStream>

QStringList expandStandardInputPaths(const QStringList &paths)
{
    QStringList expandedPaths;
    QTextStream inputStream(stdin, QIODevice::ReadOnly);
    QTextStream errorStream(stderr);

    for (const QString &path : paths) {
        if (path != "-") {
            expandedPaths.append(path);
            continue;
        }

        while (!inputStream.atEnd()) {
            const QString line = inputStream.readLine();
            if (!line.isEmpty())
                expandedPaths.append(line);
        }

        if (inputStream.status() != QTextStream::Ok)
            errorStream << "qView: Failed to read paths from standard input" << Qt::endl;
    }

    return expandedPaths;
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QCoreApplication::setOrganizationName("qView");
    QCoreApplication::setApplicationName("qView");
    QCoreApplication::setApplicationVersion(QString::number(VERSION));
    QVApplication app(argc, argv);

#if defined Q_OS_WIN && WIN32_LOADED && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    QStringList arguments = QVWin32Functions::getCommandLineArgs();
#else
    QStringList arguments = app.arguments();
#endif
    const auto ipcServerOption = QVIPCServer::extractServerOption(&arguments);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(
            "ipc-server",
            QObject::tr("Open a JSON IPC socket. With no value, uses the default per-user socket. "
                        "Use --ipc-server=<socket> or --ipc-server <socket> to set it.")));
    parser.addPositionalArgument(QObject::tr("file"),
                                 QObject::tr("The file(s) or directories to open. Use - to read "
                                             "line-delimited paths from standard input."),
                                 QObject::tr("[file ...]"));
    parser.process(arguments);

    auto *window = QVApplication::newWindow();
    const QStringList inputPaths = expandStandardInputPaths(parser.positionalArguments());
    if (!inputPaths.isEmpty())
        QVApplication::openFileSequence(window, inputPaths, true);

    if (ipcServerOption.enabled)
        app.startIpcServer(ipcServerOption.serverName);

    return QApplication::exec();
}
