#ifndef _QVIMAGEREADER_H
#define _QVIMAGEREADER_H

#include "qvimagewrapper.h"
#include <QImage>
#include <QObject>
#include <QSharedPointer>
#include <QTemporaryFile>
#include <QFuture>

#include <variant>

class QWindow;

/// Asynchronous file reader abstraction
class QVImageReader : public QObject
{
    Q_OBJECT
public:
    struct ErrorData
    {
        int errorCode = 0;
        QString errorString;
    };

    struct SuccessData
    {
        QVImageWrapper image;
        QString absoluteFilePath;
        qint64 fileSize;
        QSize imageSize;
    };

    using ReadData = std::variant<SuccessData, ErrorData>;

    explicit QVImageReader(QObject *parent = nullptr);

    /** Asynchronously reads a file
     *  Returns immediately a future that will hold the result.
     */
    QFuture<std::unique_ptr<ReadData>> readFile(const QString &fileName);

    /** Preloads a file into memory
     *  Result will be discarded, and hopefully cached by libvips.
     *  Returns immediately a future containing no result.
     */
    QFuture<void> preload(const QString &fileName);


    /** Detects the display color space and stores it in a temporary file
     *  The result of this is stored until the next invocation
     */
    void detectDisplayColorSpace(QWindow *window);

private:
    /// Routine to do actual reading work
    static std::unique_ptr<ReadData> doReadFile(const QString &fileName, QSharedPointer<QTemporaryFile> displayColorProfileFile);

    QSharedPointer<QTemporaryFile> m_displayColorProfileFile;
};

#endif /* _QVIMAGEREADER_H */
