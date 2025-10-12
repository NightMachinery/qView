#ifndef _QVIMAGEREADER_H
#define _QVIMAGEREADER_H

#include <QImage>
#include <QObject>
#include <QSharedPointer>
#include <QTemporaryFile>
#include <QFuture>

class QWindow;

/// Asynchronous file reader abstraction
class QVImageReader : public QObject
{
    Q_OBJECT
public:
    struct ErrorData
    {
        bool hasError = false;
        int errorNum = 0;
        QString errorString;
    };

    struct ReadData
    {
        QImage image;
        QString absoluteFilePath;
        qint64 fileSize;
        QSize imageSize;
        ErrorData errorData;

        ReadData() = delete;
        ReadData(QImage &&image, QString absoluteFilePath, qint64 fileSize, QSize imageSize,
                 ErrorData errorData)
            : image(std::move(image)),
              absoluteFilePath(absoluteFilePath),
              fileSize(fileSize),
              imageSize(imageSize),
              errorData(errorData)
        {
        }

        // move constructor
        ReadData(ReadData &&other) noexcept = default;

        // move assignment
        ReadData &operator=(ReadData &&other) noexcept = default;

        // disable copy and assignment
        ReadData(const ReadData &other) = delete;
        ReadData &operator=(const ReadData &other) = delete;
    };

    explicit QVImageReader(QObject *parent = nullptr);

    /** Asynchronously reads a file
     *  Returns immediately. Result will be emitted
     */
    QFuture<std::unique_ptr<ReadData>> readFile(const QString &fileName);


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
