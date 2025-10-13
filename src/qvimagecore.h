#ifndef _QVIMAGECORE_H
#define _QVIMAGECORE_H

#include <QCache>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMovie>
#include <QObject>
#include <QPixmap>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>
#include <QTemporaryFile>
#include <optional>

#include "qvimagereader.h"

class QVImageCore : public QObject
{
    Q_OBJECT

public:
// todo remove

    struct CompatibleFile
    {
        QString absoluteFilePath;
        QString fileName;

        // Only populated if needed for sorting
        qint64 lastModified;
        qint64 lastCreated;
        qint64 size;
        QString mimeType;
    };

    struct FileDetails
    {
        QFileInfo fileInfo;
        QList<CompatibleFile> folderFileInfoList;
        int loadedIndexInFolder = -1;
        bool isLoadRequested = false;
        bool isPixmapLoaded = false; // TODO remove move into wrapper
        bool isMovieLoaded = false; // TODO remove move into wrapper
        QSize baseImageSize;
        QSize loadedPixmapSize;
        QElapsedTimer timeSinceLoaded;
        std::optional<QVImageReader::ErrorData> errorData;

        void updateLoadedIndexInFolder();
    };

    struct DirInfo
    {
        QString dirPath;
        qsizetype fileCount;
        int sortMode;
        bool sortDescending;

        bool operator!=(const DirInfo &other) const
        {
            return dirPath != other.dirPath || fileCount != other.fileCount
                    || sortMode != other.sortMode || sortDescending != other.sortDescending;
        }
    };

    explicit QVImageCore(QObject *parent = nullptr);

    void loadFile(const QString &fileName, bool isReloading = false);
    void preloadFile(const QString &fileName);
    void loadPixmap(std::unique_ptr<QVImageReader::ReadData> readData);
    void closeImage();
    QList<CompatibleFile> getCompatibleFiles(const QString &dirPath) const;
    void updateFolderInfo(QString dirPath = QString());
    void requestPreloading();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    static bool removeTinyDataTagsFromIccProfile(QByteArray &profile);
#endif

    void settingsUpdated();

    void jumpToPreviousFrame();
    void jumpToNextFrame();
    void setPaused(bool desiredState);
    void setSpeed(int desiredSpeed);

    void rotateImage(int rotation);
    QImage matchCurrentRotation(const QImage &imageToRotate);
    QPixmap matchCurrentRotation(const QPixmap &pixmapToRotate);

    QPixmap scaleExpensively(const int desiredWidth, const int desiredHeight);
    QPixmap scaleExpensively(const QSizeF desiredSize);

    // returned const reference is read-only
    const QPixmap &getLoadedPixmap() const { return loadedPixmap; }
    bool isPaused() const;
    const FileDetails &getCurrentFileDetails() const { return currentFileDetails; }
    int getCurrentRotation() const { return currentRotation; }

signals:
    void animatedFrameChanged();

    void updateLoadedPixmapItem();

    void fileChanged();

protected:
    void loadEmptyPixmap();
    FileDetails getEmptyFileDetails();

private slots:
    void onAnimatedFrameChanged();
private:
    std::optional<QVImageWrapper> loadedImage;
    QPixmap loadedPixmap; // TODO remove

    FileDetails currentFileDetails;
    int currentRotation;

    int colorSpaceConversion;

    DirInfo lastDirInfo;

    QStringList lastFilesPreloaded;
    QStringList preloadFilesInProgress;
    QString waitingOnPreloadFile;

    int largestDimension;

    quint64 m_requestCounter = 0;
    quint64 m_lastDisplayedCounter = 0;

    QVImageReader *m_imageReader = nullptr;
};

#endif /* _QVIMAGECORE_H */
