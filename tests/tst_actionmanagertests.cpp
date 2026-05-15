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
    void testBackgroundDetailsRectSelection();
    void testBackgroundDetailsText();
    void testRecoverNtagPath();
    void testInputPathSequence();
    void testInputPathSequenceRecoversNtagPath();
    void testNextFileWrapsPastMissingFiles();
    void testPreviousFileWrapsPastMissingFiles();
    void testNextFileRecoversRetaggedFile();
    void testPreviousFileWorksAfterRecoveredNavigation();
    void testCurrentFileInfoRecoversNtagPath();
    void testCurrentFileInfoRecoversUntaggedPathAfterTagRemoval();
    void testIpcCurrentFilePathRecoversNtagPath();
    void testIpcCurrentFilePathRecoversUntaggedPathAfterTagRemoval();
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

void ActionManagerTests::testBackgroundDetailsRectSelection()
{
    const QRect viewportRect(0, 0, 800, 600);
    const QSize textSize(120, 20);
    const int padding = 10;

    QCOMPARE(MainWindow::selectBackgroundDetailsRect(viewportRect, QRect(0, 200, 800, 200),
                                                     textSize, padding),
             QRect(0, 0, 800, 200));
    QCOMPARE(MainWindow::selectBackgroundDetailsRect(viewportRect, QRect(300, 0, 200, 600),
                                                     textSize, padding),
             QRect(0, 0, 300, 600));
    QVERIFY(MainWindow::selectBackgroundDetailsRect(viewportRect, viewportRect, textSize, padding)
                    .isNull());
    QVERIFY(MainWindow::selectBackgroundDetailsRect(viewportRect, QRect(0, 12, 800, 576),
                                                    textSize, padding)
                    .isNull());
}

void ActionManagerTests::testBackgroundDetailsText()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString imageFile = tempDir.filePath("image..gray..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(imageFile));

    const QString text =
            MainWindow::backgroundDetailsText(QFileInfo(imageFile), 1, 7, QSize(3840, 2160));
    QVERIFY(text.contains("2/7"));
    QVERIFY(text.contains("image..gray..png"));
    QVERIFY(text.contains("3840x2160"));
    QVERIFY(text.contains(QVInfoDialog::formatBytes(QFileInfo(imageFile).size())));
}

void ActionManagerTests::testRecoverNtagPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString untaggedFile = tempDir.filePath("image.png");
    const QString taggedFile = tempDir.filePath("image..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(taggedFile));

    bool recovered = false;
    QCOMPARE(QVImageCore::recoverNtagPath(untaggedFile, &recovered),
             QFileInfo(taggedFile).absoluteFilePath());
    QVERIFY(recovered);

    QVERIFY(QImage(3, 3, QImage::Format_ARGB32).save(untaggedFile));
    recovered = true;
    QCOMPARE(QVImageCore::recoverNtagPath(untaggedFile, &recovered), untaggedFile);
    QVERIFY(!recovered);

    QVERIFY(QFile::remove(untaggedFile));
    const QString firstTaggedFile = tempDir.filePath("image..blue..png");
    QVERIFY(QImage(4, 4, QImage::Format_ARGB32).save(firstTaggedFile));
    recovered = false;
    QCOMPARE(QVImageCore::recoverNtagPath(untaggedFile, &recovered),
             QFileInfo(firstTaggedFile).absoluteFilePath());
    QVERIFY(recovered);

    QVERIFY(QFile::remove(taggedFile));
    QVERIFY(QFile::remove(firstTaggedFile));
    QVERIFY(QImage(5, 5, QImage::Format_ARGB32).save(untaggedFile));
    recovered = false;
    QCOMPARE(QVImageCore::recoverNtagPath(taggedFile, &recovered),
             QFileInfo(untaggedFile).absoluteFilePath());
    QVERIFY(recovered);

    QVERIFY(QImage(6, 6, QImage::Format_ARGB32).save(firstTaggedFile));
    recovered = false;
    QCOMPARE(QVImageCore::recoverNtagPath(taggedFile, &recovered),
             QFileInfo(untaggedFile).absoluteFilePath());
    QVERIFY(recovered);
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

void ActionManagerTests::testInputPathSequenceRecoversNtagPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString untaggedFile = tempDir.filePath("image.png");
    const QString taggedFile = tempDir.filePath("image..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(taggedFile));

    QVImageCore imageCore;
    QStringList warnings;
    const auto files = imageCore.getCompatibleFilesForInputs({ untaggedFile }, &warnings);

    QCOMPARE(files.length(), 1);
    QCOMPARE(files.at(0).absoluteFilePath, QFileInfo(taggedFile).absoluteFilePath());
    QVERIFY(warnings.isEmpty());
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

void ActionManagerTests::testNextFileRecoversRetaggedFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString currentFile = tempDir.filePath("1.png");
    const QString nextFile = tempDir.filePath("2.png");
    const QString taggedNextFile = tempDir.filePath("2..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(currentFile));
    QVERIFY(QImage(3, 3, QImage::Format_ARGB32).save(nextFile));

    QWidget parent;
    QVGraphicsView view(&parent);
    view.loadFile(currentFile);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(currentFile).absoluteFilePath());

    QVERIFY(QFile::rename(nextFile, taggedNextFile));
    view.goToFile(QVGraphicsView::GoToFileMode::next);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(taggedNextFile).absoluteFilePath());
}

void ActionManagerTests::testPreviousFileWorksAfterRecoveredNavigation()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString firstFile = tempDir.filePath("1.png");
    const QString nextFile = tempDir.filePath("2.png");
    const QString taggedNextFile = tempDir.filePath("2..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(firstFile));
    QVERIFY(QImage(3, 3, QImage::Format_ARGB32).save(nextFile));

    QWidget parent;
    QVGraphicsView view(&parent);
    view.loadFile(firstFile);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(firstFile).absoluteFilePath());

    QVERIFY(QFile::rename(nextFile, taggedNextFile));
    view.goToFile(QVGraphicsView::GoToFileMode::next);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(taggedNextFile).absoluteFilePath());

    view.goToFile(QVGraphicsView::GoToFileMode::previous);
    QTRY_COMPARE(view.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(firstFile).absoluteFilePath());
}

void ActionManagerTests::testCurrentFileInfoRecoversNtagPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString currentFile = tempDir.filePath("image.png");
    const QString taggedCurrentFile = tempDir.filePath("image..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(currentFile));

    MainWindow window;
    window.show();
    window.openFile(currentFile);
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(currentFile).absoluteFilePath());

    QVERIFY(QFile::rename(currentFile, taggedCurrentFile));
    QCOMPARE(window.recoverCurrentFileInfo().absoluteFilePath(),
             QFileInfo(taggedCurrentFile).absoluteFilePath());
    window.close();
    qvApp->deleteFromLastActiveWindows(&window);
}

void ActionManagerTests::testCurrentFileInfoRecoversUntaggedPathAfterTagRemoval()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString currentFile = tempDir.filePath("image.png");
    const QString taggedCurrentFile = tempDir.filePath("image..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(currentFile));

    MainWindow window;
    window.show();
    window.openFile(currentFile);
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(currentFile).absoluteFilePath());

    QVERIFY(QFile::rename(currentFile, taggedCurrentFile));
    QCOMPARE(window.recoverCurrentFileInfo().absoluteFilePath(),
             QFileInfo(taggedCurrentFile).absoluteFilePath());

    QVERIFY(QFile::rename(taggedCurrentFile, currentFile));
    QCOMPARE(window.recoverCurrentFileInfo().absoluteFilePath(),
             QFileInfo(currentFile).absoluteFilePath());
    window.close();
    qvApp->deleteFromLastActiveWindows(&window);
}

void ActionManagerTests::testIpcCurrentFilePathRecoversNtagPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString currentFile = tempDir.filePath("image.png");
    const QString taggedCurrentFile = tempDir.filePath("image..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(currentFile));

    MainWindow window;
    window.show();
    qvApp->addToLastActiveWindows(&window);
    window.openFile(currentFile);
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(currentFile).absoluteFilePath());

    QVERIFY(QFile::rename(currentFile, taggedCurrentFile));
    QCOMPARE(qvApp->currentFilePath(), QFileInfo(taggedCurrentFile).absoluteFilePath());
    window.close();
    qvApp->deleteFromLastActiveWindows(&window);
}

void ActionManagerTests::testIpcCurrentFilePathRecoversUntaggedPathAfterTagRemoval()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString currentFile = tempDir.filePath("image.png");
    const QString taggedCurrentFile = tempDir.filePath("image..red..png");
    QVERIFY(QImage(2, 2, QImage::Format_ARGB32).save(currentFile));

    MainWindow window;
    window.show();
    qvApp->addToLastActiveWindows(&window);
    window.openFile(currentFile);
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.absoluteFilePath(),
                 QFileInfo(currentFile).absoluteFilePath());

    QVERIFY(QFile::rename(currentFile, taggedCurrentFile));
    QCOMPARE(qvApp->currentFilePath(), QFileInfo(taggedCurrentFile).absoluteFilePath());

    QVERIFY(QFile::rename(taggedCurrentFile, currentFile));
    QCOMPARE(qvApp->currentFilePath(), QFileInfo(currentFile).absoluteFilePath());
    window.close();
    qvApp->deleteFromLastActiveWindows(&window);
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
