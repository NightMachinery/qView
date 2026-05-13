#include <QtTest>

#include "qvapplication.h"
#include "qvipcserver.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSettings>
#include <QTemporaryDir>

class ActionManagerTests : public QObject
{
    Q_OBJECT

public:
    ActionManagerTests();
    ~ActionManagerTests();

private slots:
    void testClonedActionsUntracked();
    void testIpcServerOptionDisabledByDefault();
    void testIpcServerOptionUsesDefaultSocket();
    void testIpcServerOptionAcceptsExplicitSocket();
    void testIpcCurrentFilePathResponse();
    void testInputPathSequence();
    void testNextFileWrapsPastMissingFiles();
    void testPreviousFileWrapsPastMissingFiles();
};

ActionManagerTests::ActionManagerTests() { }

ActionManagerTests::~ActionManagerTests() { }

void ActionManagerTests::testClonedActionsUntracked()
{
    // Get initial counts of certain actions
    int fullscreenCount = qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length();
    int openCount = qvApp->getActionManager().getAllInstancesOfAction("open").length();
    qDebug() << fullscreenCount;

    // Have window clone actions
    MainWindow window;
    window.show();
    // Make sure they were cloned
    QVERIFY(qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length()
            != fullscreenCount);
    QVERIFY(qvApp->getActionManager().getAllInstancesOfAction("open").length() != openCount);
    // Untrack them
    window.close();

    // Make sure the count has not changed from the initial
    QCOMPARE(qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length(),
             fullscreenCount);
    QCOMPARE(qvApp->getActionManager().getAllInstancesOfAction("open").length(), openCount);
}

void ActionManagerTests::testIpcServerOptionDisabledByDefault()
{
    QStringList arguments = { "qview", "image.jpg" };
    const auto option = QVIPCServer::extractServerOption(&arguments);

    QVERIFY(!option.enabled);
    QVERIFY(option.serverName.isEmpty());
    QCOMPARE(arguments, QStringList({ "qview", "image.jpg" }));
}

void ActionManagerTests::testIpcServerOptionUsesDefaultSocket()
{
    QStringList arguments = { "qview", "image.jpg", "--ipc-server" };
    const auto option = QVIPCServer::extractServerOption(&arguments);

    QVERIFY(option.enabled);
    QCOMPARE(option.serverName, QVIPCServer::defaultServerName());
    QCOMPARE(arguments, QStringList({ "qview", "image.jpg" }));
}

void ActionManagerTests::testIpcServerOptionAcceptsExplicitSocket()
{
    QStringList arguments = { "qview", "--ipc-server=/tmp/qview-test.sock", "image.jpg" };
    const auto option = QVIPCServer::extractServerOption(&arguments);

    QVERIFY(option.enabled);
    QCOMPARE(option.serverName, QString("/tmp/qview-test.sock"));
    QCOMPARE(arguments, QStringList({ "qview", "image.jpg" }));
}

void ActionManagerTests::testIpcCurrentFilePathResponse()
{
    const QJsonObject request = { { QStringLiteral("method"), QStringLiteral("currentFilePath") } };

    QCOMPARE(QVIPCServer::handleMessage(request, "/tmp/image.jpg").value("ok").toBool(), true);
    QCOMPARE(QVIPCServer::handleMessage(request, "/tmp/image.jpg").value("path").toString(),
             QString("/tmp/image.jpg"));

    const QJsonObject noFileResponse = QVIPCServer::handleMessage(request, QString());
    QCOMPARE(noFileResponse.value("ok").toBool(), false);
    QCOMPARE(noFileResponse.value("error").toString(), QString("no_current_file"));

    const QJsonObject unknownResponse =
            QVIPCServer::handleMessage({ { QStringLiteral("method"), QStringLiteral("missing") } },
                                        "/tmp/image.jpg");
    QCOMPARE(unknownResponse.value("ok").toBool(), false);
    QCOMPARE(unknownResponse.value("error").toString(), QString("unknown_method"));
}

void ActionManagerTests::testInputPathSequence()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString firstFile = tempDir.filePath("1.png");
    const QString secondFile = tempDir.filePath("2.png");
    const QString unsupportedFile = tempDir.filePath("notes.txt");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(firstFile));
    QVERIFY(QImage(3, 3, QImage::Format_ARGB32).save(secondFile));
    QVERIFY(QFile(unsupportedFile).open(QIODevice::WriteOnly));

    QVImageCore imageCore;
    QStringList warnings;
    const auto files = imageCore.getCompatibleFilesForInputs(
            { secondFile, tempDir.path(), unsupportedFile, tempDir.filePath("missing.png") },
            &warnings);

    QCOMPARE(files.length(), 2);
    QCOMPARE(files.at(0).absoluteFilePath, QFileInfo(secondFile).absoluteFilePath());
    QCOMPARE(files.at(1).absoluteFilePath, QFileInfo(firstFile).absoluteFilePath());
    QCOMPARE(warnings.length(), 2);
    QVERIFY(warnings.at(0).contains("unsupported"));
    QVERIFY(warnings.at(1).contains("missing"));
}

void ActionManagerTests::testNextFileWrapsPastMissingFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString firstFile = tempDir.filePath("1.png");
    const QString currentFile = tempDir.filePath("2.png");
    const QString missingNextFile = tempDir.filePath("3.png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(firstFile));
    QVERIFY(QImage(3, 3, QImage::Format_ARGB32).save(currentFile));
    QVERIFY(QImage(4, 4, QImage::Format_ARGB32).save(missingNextFile));

    QWidget parent;
    QVGraphicsView view(&parent);
    view.loadFile(currentFile);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(currentFile).absoluteFilePath());

    QVERIFY(QFile::remove(missingNextFile));
    view.goToFile(QVGraphicsView::GoToFileMode::next);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(firstFile).absoluteFilePath());
}

void ActionManagerTests::testPreviousFileWrapsPastMissingFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString missingPreviousFile = tempDir.filePath("1.png");
    const QString currentFile = tempDir.filePath("2.png");
    const QString lastFile = tempDir.filePath("3.png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(missingPreviousFile));
    QVERIFY(QImage(3, 3, QImage::Format_ARGB32).save(currentFile));
    QVERIFY(QImage(4, 4, QImage::Format_ARGB32).save(lastFile));

    QWidget parent;
    QVGraphicsView view(&parent);
    view.loadFile(currentFile);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(currentFile).absoluteFilePath());

    QVERIFY(QFile::remove(missingPreviousFile));
    view.goToFile(QVGraphicsView::GoToFileMode::previous);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(lastFile).absoluteFilePath());
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("qViewTests");
    QCoreApplication::setApplicationName("qViewTests");
    QSettings settings;
    settings.beginGroup("options");
    settings.setValue("colorspaceconversion", 0);
    settings.endGroup();

    QVApplication app(argc, argv);
    ActionManagerTests actionManagerTests;
    return QTest::qExec(&actionManagerTests, argc, argv);
}

#include "tst_actionmanagertests.moc"
