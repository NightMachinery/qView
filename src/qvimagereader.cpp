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

QFuture<std::unique_ptr<QVImageReader::ReadData>> QVImageReader::readFile(const QString &filePath)
{
    return QtConcurrent::run([this, filePath]() { return doReadFile(filePath, m_displayColorProfileFile); });
}

QFuture<void> QVImageReader::preload(const QString &filePath)
{
    return QtConcurrent::run([this, filePath]() {
        if (QFile(filePath).size() / 1024 > VipsReader::getCacheMaxMemoryUsage() / 2) {
            return;
        }
        QSharedPointer<QTemporaryFile> displayColorProfileFile = m_displayColorProfileFile;
        std::optional<QString> targetIccFileName = displayColorProfileFile ? std::make_optional(displayColorProfileFile->fileName()) : std::nullopt;
        VipsReader::preload(filePath, targetIccFileName);
    });
}

std::unique_ptr<QVImageReader::ReadData> QVImageReader::doReadFile(
        const QString &filePath, QSharedPointer<QTemporaryFile> displayColorProfileFile)
{
    QSize imageSize;
    int errorCode = 0;

    std::optional<QString> targetIccFileName;
    if (displayColorProfileFile)
    {
        targetIccFileName = displayColorProfileFile->fileName();
    }
    auto result = VipsReader::read(filePath, targetIccFileName);
    if (result.images.isEmpty() || result.images[0].isNull()) {
        errorCode = 1;
        return std::make_unique<ReadData>(ErrorData{errorCode, std::move(result.error)});
    }
    QVImageWrapper readImage = QVImageWrapper(std::move(result.images));
    const QImage &image = readImage.currentImage();

    imageSize = image.size();

    // Should have been converted by libvips already
    Q_ASSERT(image.format() == QImage::Format::Format_ARGB32_Premultiplied);

    QFileInfo fileInfo(filePath);

    return std::make_unique<ReadData>(SuccessData{std::move(readImage),
                                                      fileInfo.absoluteFilePath(),
                                                      fileInfo.size(),
                                                      imageSize});
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
