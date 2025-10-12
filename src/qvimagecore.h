#ifndef QVIMAGECORE_H
#define QVIMAGECORE_H

#include <QObject>
#include <QImageReader>
#include <QPixmap>
#include <QMovie>
#include <QFileInfo>
#include <QCache>
#include <QElapsedTimer>
#include <QRunnable>
#include <QThreadPool>
#include <QTimer>

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#  include <QColorSpace>
#else
typedef QString QColorSpace;
#endif

// TODO: Move file handling out of this class
class QVImageCore : public QObject
{
    Q_OBJECT

public:
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

    struct ErrorData
    {
        bool hasError = false;
        int errorNum = 0;
        QString errorString;
    };

    struct FileDetails
    {
        QFileInfo fileInfo;
        QList<CompatibleFile> folderFileInfoList;
        int loadedIndexInFolder = -1;
        bool isLoadRequested = false;
        bool isPixmapLoaded = false;
        bool isMovieLoaded = false;
        QSize baseImageSize;
        QSize loadedPixmapSize;
        QElapsedTimer timeSinceLoaded;
        ErrorData errorData;

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

    struct ReadData
    {
        QImage image;
        QString absoluteFilePath;
        qint64 fileSize;
        QSize imageSize;
        ErrorData errorData;
    };

    explicit QVImageCore(QObject *parent = nullptr);

    void loadFile(const QString &fileName, bool isReloading = false);
    ReadData readFile(const QString &fileName, const QColorSpace &targetColorSpace);
    void preloadFile(const QString &fileName, const QColorSpace &targetColorSpace);
    void loadPixmap(const ReadData &readData);
    void closeImage();
    QList<CompatibleFile> getCompatibleFiles(const QString &dirPath) const;
    void updateFolderInfo(QString dirPath = QString());
    void requestPreloading();
    void requestPreloadingFile(const QString &filePath, const QColorSpace &targetColorSpace);
    QColorSpace getTargetColorSpace() const;
    QColorSpace detectDisplayColorSpace() const;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    static bool removeTinyDataTagsFromIccProfile(QByteArray &profile);
#endif

    void settingsUpdated();

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
    const QMovie &getLoadedMovie() const { return loadedMovie; }
    const FileDetails &getCurrentFileDetails() const { return currentFileDetails; }
    int getCurrentRotation() const { return currentRotation; }

signals:
    void animatedFrameChanged(QRect rect);

    void updateLoadedPixmapItem();

    void fileChanged();

protected:
    void loadEmptyPixmap();
    FileDetails getEmptyFileDetails();

private:
    QPixmap loadedPixmap;
    QMovie loadedMovie;

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
};

#endif // QVIMAGECORE_H
