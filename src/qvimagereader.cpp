#include "qvimagereader.h"
#include "vipsreader.h"
#include "qvwin32functions.h"
#include "qvcocoafunctions.h"
#include "qvlinuxx11functions.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QFileInfo>
#include <QWindow>

QVImageReader::QVImageReader(QObject *parent)
    : QObject(parent)
{
}

QFuture<std::unique_ptr<QVImageReader::ReadData>> QVImageReader::readFile(const QString &fileName)
{
    return QtConcurrent::run([this, fileName]() { return doReadFile(fileName, m_displayColorProfileFile); });
}

std::unique_ptr<QVImageReader::ReadData> QVImageReader::doReadFile(
        const QString &fileName, QSharedPointer<QTemporaryFile> displayColorProfileFile)
{
    QImage readImage;
    QSize imageSize;
    int errorCode = 0;
    QString errorString;

    QString targetIccFileName;
    if (displayColorProfileFile)
    {
        targetIccFileName = displayColorProfileFile->fileName();
    }
    auto result = VipsReader::read(fileName, targetIccFileName);
    readImage = std::move(result.image);
    errorString = std::move(result.error);

    if (!readImage.isNull())
    {
        imageSize = readImage.size();
    } else
    {
        errorCode = 1;
    }

    // Should have been converted by libvips already
    Q_ASSERT(readImage.format() == QImage::Format::Format_ARGB32_Premultiplied);

    QFileInfo fileInfo(fileName);

    if (readImage.isNull())
    {
        return std::make_unique<ReadData>(ErrorData{errorCode, errorString});
    } else
    {
        return std::make_unique<ReadData>(SuccessData{std::move(readImage),
                                                      fileInfo.absoluteFilePath(),
                                                      fileInfo.size(),
                                                      imageSize});
    }
}

void QVImageReader::detectDisplayColorSpace(QWindow *window)
{
    QByteArray profileData;
#ifdef WIN32_LOADED
    profileData = QVWin32Functions::getIccProfileForWindow(window);
#endif
#ifdef COCOA_LOADED
    profileData = QVCocoaFunctions::getIccProfileForWindow(window);
#endif
#ifdef X11_LOADED
    profileData = QVLinuxX11Functions::getIccProfileForWindow(window);
#endif

    if (!profileData.isEmpty()) {
        m_displayColorProfileFile.reset(new QTemporaryFile());
        m_displayColorProfileFile->open();
        m_displayColorProfileFile->write(profileData);
        m_displayColorProfileFile->close();
    }
}
