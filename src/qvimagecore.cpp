#include "qvimagecore.h"
#include "qvapplication.h"
#include "qvwin32functions.h"
#include "qvcocoafunctions.h"
#include "qvlinuxx11functions.h"
#include "vipsreader.h"
#include <cstring>
#include <random>
#include <QMessageBox>
#include <QDir>
#include <QUrl>
#include <QSettings>
#include <QCollator>
#include <QtConcurrent/QtConcurrentRun>
#include <QIcon>
#include <QGuiApplication>
#include <QScreen>

QVImageCore::QVImageCore(QObject *parent) : QObject(parent)
{
    m_imageReader = new QVImageReader(this);
    currentRotation = 0;

    // connect(&loadedMovie, &QMovie::updated, this, &QVImageCore::animatedFrameChanged);

    largestDimension = 0;
    const auto screenList = QGuiApplication::screens();
    for (auto const &screen : screenList) {
        int largerDimension;
        if (screen->size().height() > screen->size().width()) {
            largerDimension = screen->size().height();
        } else {
            largerDimension = screen->size().width();
        }

        if (largerDimension > largestDimension) {
            largestDimension = largerDimension;
        }
    }

    // Connect to settings signal
    connect(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated, this,
            &QVImageCore::settingsUpdated);
    settingsUpdated();
}

void QVImageCore::loadFile(const QString &fileName, bool isReloading)
{
    QString sanitaryFileName = fileName;

    // sanitize file name if necessary
    QUrl sanitaryUrl = QUrl(fileName);
    if (sanitaryUrl.isLocalFile())
        sanitaryFileName = sanitaryUrl.toLocalFile();

    QFileInfo fileInfo(sanitaryFileName);
    sanitaryFileName = fileInfo.absoluteFilePath();

    if (fileInfo.isDir()) {
        updateFolderInfo(sanitaryFileName);
        if (currentFileDetails.folderFileInfoList.isEmpty())
            closeImage();
        else
            loadFile(currentFileDetails.folderFileInfoList.at(0).absoluteFilePath);
        return;
    }

    // Pause playing movie because it feels better that way
    setPaused(true);

    currentFileDetails.isLoadRequested = true;
    // TODO: Cache color space? Expensive to get this on every loadFile call?
    m_imageReader->detectDisplayColorSpace(static_cast<QWidget *>(parent())->window()->windowHandle());

    quint64 requestNumber = ++m_requestCounter;

    auto *watcher = new QFutureWatcher<std::unique_ptr<QVImageReader::ReadData>>(this);

    connect(watcher, &QFutureWatcher<std::unique_ptr<QVImageReader::ReadData>>::finished, this,
            [this, watcher, requestNumber]() {
                std::unique_ptr<QVImageReader::ReadData> readData =
                        std::move(watcher->future().takeResult());

                if (requestNumber > m_lastDisplayedCounter)
                {
                    loadPixmap(std::move(readData));

                    m_lastDisplayedCounter = requestNumber;
                }

                watcher->deleteLater();
            });

    watcher->setFuture(m_imageReader->readFile(sanitaryFileName));
}

void QVImageCore::loadPixmap(std::unique_ptr<QVImageReader::ReadData> readData)
{
    std::visit(
            [this](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, QVImageReader::ErrorData>)
                {
                    currentFileDetails = getEmptyFileDetails();
                    currentFileDetails.errorData = arg;
                    loadEmptyPixmap();
                    return;
                } else if constexpr (std::is_same_v<T, QVImageReader::SuccessData>)
                {
                    currentFileDetails.errorData = std::nullopt;

                    // Do this first so we can keep folder info even when loading errored files
                    currentFileDetails.fileInfo = QFileInfo(arg.absoluteFilePath);
                    currentFileDetails.updateLoadedIndexInFolder();
                    if (currentFileDetails.loadedIndexInFolder == -1)
                        updateFolderInfo();

                    loadedPixmap = QPixmap::fromImage(matchCurrentRotation(arg.image.currentImage()));

    // Set file details
    currentFileDetails.isPixmapLoaded = true;
                    currentFileDetails.baseImageSize = arg.imageSize;
    currentFileDetails.loadedPixmapSize = loadedPixmap.size();
                    if (currentFileDetails.baseImageSize == QSize(-1, -1))
                    {
        currentFileDetails.baseImageSize = currentFileDetails.loadedPixmapSize;
    }

    currentFileDetails.timeSinceLoaded.start();

    emit fileChanged();

    requestPreloading();
                }
            },
            *readData);
}

void QVImageCore::closeImage()
{
    currentFileDetails = getEmptyFileDetails();
    loadEmptyPixmap();
}

void QVImageCore::loadEmptyPixmap()
{
    loadedPixmap = QPixmap();
    // loadedMovie.stop();
    // loadedMovie.setFileName("");

    emit fileChanged();
}

QVImageCore::FileDetails QVImageCore::getEmptyFileDetails()
{
    return { QFileInfo(),
             currentFileDetails.folderFileInfoList,
             currentFileDetails.loadedIndexInFolder,
             false,
             false,
             false,
             QSize(),
             QSize(),
             QElapsedTimer(),
             std::nullopt
    };
}

// All file logic, sorting, etc should be moved to a different class or file
QList<QVImageCore::CompatibleFile> QVImageCore::getCompatibleFiles(const QString &dirPath) const
{
    int sortMode = qvGetSettingInt(SortMode);
    QList<CompatibleFile> fileList;

    QMimeDatabase mimeDb;
    const auto &extensions = qvApp->getFileExtensionList();
    const auto &mimeTypes = qvApp->getMimeTypeNameList();

    QMimeDatabase::MatchMode mimeMatchMode = qvGetSettingBool(AllowMimeContentDetection)
            ? QMimeDatabase::MatchDefault
            : QMimeDatabase::MatchExtension;

    // skip hidden files if user wants to
    QDir::Filters filters = QDir::Files;

    if (!qvGetSettingBool(SkipHidden))
        filters |= QDir::Hidden;

    const QFileInfoList currentFolder = QDir(dirPath).entryInfoList(filters, QDir::Unsorted);
    for (const QFileInfo &fileInfo : currentFolder) {
        bool matched = false;
        const QString absoluteFilePath = fileInfo.absoluteFilePath();
        const QString fileName = fileInfo.fileName();
        for (const QString &extension : extensions) {
            if (fileName.endsWith(extension, Qt::CaseInsensitive)) {
                matched = true;
                break;
            }
        }
        QString mimeType;
        if (!matched || sortMode == 4) {
            mimeType = mimeDb.mimeTypeForFile(absoluteFilePath, mimeMatchMode).name();
            matched |= mimeTypes.contains(mimeType);
        }

        // ignore macOS ._ metadata files
        if (fileName.startsWith("._")) {
            matched = false;
        }

        if (matched) {
            fileList.append({ absoluteFilePath, fileName,
                              sortMode == 1 ? fileInfo.lastModified().toMSecsSinceEpoch() : 0,
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
                              sortMode == 2 ? fileInfo.birthTime().toMSecsSinceEpoch() : 0,
#else
                              sortMode == 2 ? fileInfo.created().toMSecsSinceEpoch() : 0,
#endif
                              sortMode == 3 ? fileInfo.size() : 0,
                              sortMode == 4 ? mimeType : QString() });
        }
    }

    return fileList;
}

void QVImageCore::updateFolderInfo(QString dirPath)
{
    if (dirPath.isEmpty()) {
        dirPath = currentFileDetails.fileInfo.path();

        // No directory specified and a file is not already loaded from which we can infer one
        if (dirPath.isEmpty())
            return;
    }

    currentFileDetails.folderFileInfoList = getCompatibleFiles(dirPath);

    DirInfo dirInfo = { dirPath, currentFileDetails.folderFileInfoList.count(),
                        qvGetSettingInt(SortMode), qvGetSettingBool(SortDescending) };

    // Only sort if the folder has changed
    // TODO: this is redundant
    const bool dirChanged = lastDirInfo != dirInfo;
    lastDirInfo = dirInfo;

    const auto sortFn = [&]() {
        // Sorting
        switch (dirInfo.sortMode) {
        case 0: {
            // Natural sorting
            QCollator collator;
            collator.setNumericMode(true);
            std::sort(currentFileDetails.folderFileInfoList.begin(),
                      currentFileDetails.folderFileInfoList.end(),
                      [&](const CompatibleFile &file1, const CompatibleFile &file2) {
                          if (dirInfo.sortDescending)
                              return collator.compare(file1.fileName, file2.fileName) > 0;
                          else
                              return collator.compare(file1.fileName, file2.fileName) < 0;
                      });
            break;
        }
        case 1:
            // Date modified
            std::sort(currentFileDetails.folderFileInfoList.begin(),
                      currentFileDetails.folderFileInfoList.end(),
                      [&](const CompatibleFile &file1, const CompatibleFile &file2) {
                          if (dirInfo.sortDescending)
                              return file1.lastModified < file2.lastModified;
                          else
                              return file1.lastModified > file2.lastModified;
                      });
            break;
        case 2:
            // Date created
            std::sort(currentFileDetails.folderFileInfoList.begin(),
                      currentFileDetails.folderFileInfoList.end(),
                      [&](const CompatibleFile &file1, const CompatibleFile &file2) {
                          if (dirInfo.sortDescending)
                              return file1.lastCreated < file2.lastCreated;
                          else
                              return file1.lastCreated > file2.lastCreated;
                      });
            break;
        case 3:
            // Size
            std::sort(currentFileDetails.folderFileInfoList.begin(),
                      currentFileDetails.folderFileInfoList.end(),
                      [&](const CompatibleFile &file1, const CompatibleFile &file2) {
                          if (dirInfo.sortDescending)
                              return file1.size < file2.size;
                          else
                              return file1.size > file2.size;
                      });
            break;
        case 4: {
            // Type
            QCollator collator;
            std::sort(currentFileDetails.folderFileInfoList.begin(),
                      currentFileDetails.folderFileInfoList.end(),
                      [&](const CompatibleFile &file1, const CompatibleFile &file2) {
                          if (dirInfo.sortDescending)
                              return collator.compare(file1.mimeType, file2.mimeType) > 0;
                          else
                              return collator.compare(file1.mimeType, file2.mimeType) < 0;
                      });
            break;
        }
        case 5:
            // Random
            std::shuffle(currentFileDetails.folderFileInfoList.begin(),
                         currentFileDetails.folderFileInfoList.end(),
                         std::default_random_engine(
                                 std::chrono::system_clock::now().time_since_epoch().count()));
            break;
        default:
            Q_ASSERT(false);
            break;
        }
    };

    if (dirChanged) {
        sortFn();
    }

    // Set current file index variable
    currentFileDetails.updateLoadedIndexInFolder();
}

void QVImageCore::requestPreloading()
{
    // TODO: this makes no sense at all. Preloading amount should be decided
    // dynamically based on available memory and size of surrounding files.
    // added getMemoryUsage for this
    int preloadingMode = qvGetSettingInt(PreloadingMode);
    if (preloadingMode == 0) {
        return;
    }

    int preloadingDistance = preloadingMode == 1 ? 1 : 4;

    QStringList filesToPreload;
    for (int i = currentFileDetails.loadedIndexInFolder - preloadingDistance;
         i <= currentFileDetails.loadedIndexInFolder + preloadingDistance; i++) {
        int index = i;

        // Don't try to preload the currently loaded image
        if (index == currentFileDetails.loadedIndexInFolder)
            continue;

        // keep within index range
        if (qvGetSettingBool(LoopFoldersEnabled)) {
            if (index > currentFileDetails.folderFileInfoList.length() - 1)
                index = index - (currentFileDetails.folderFileInfoList.length());
            else if (index < 0)
                index = index + (currentFileDetails.folderFileInfoList.length());
        }

        // if still out of range after looping, just cancel the preload for this index
        if (index > currentFileDetails.folderFileInfoList.length() - 1 || index < 0
            || currentFileDetails.folderFileInfoList.isEmpty())
            continue;

        QString filePath = currentFileDetails.folderFileInfoList[index].absoluteFilePath;
        filesToPreload.append(filePath);

        if (preloadFilesInProgress.contains(filePath)) {
            continue;
        }

        preloadFilesInProgress.append(filePath);

        // Send preload request
        QThreadPool::globalInstance()->start([this, filePath]() {
            // Skip preload if file is larger than half of VIPS cache
            // TODO: This should be more intelligent
            m_imageReader->preload(filePath);
        });
    }
    lastFilesPreloaded = filesToPreload;
}

void QVImageCore::jumpToNextFrame()
{
    // if (currentFileDetails.isMovieLoaded)
    //     loadedMovie.jumpToNextFrame();
}

void QVImageCore::setPaused(bool desiredState)
{
    // if (currentFileDetails.isMovieLoaded)
        // loadedMovie.setPaused(desiredState);
}

void QVImageCore::setSpeed(int desiredSpeed)
{
    desiredSpeed = std::clamp(desiredSpeed, 0, 1000);

    // if (currentFileDetails.isMovieLoaded)
    //     loadedMovie.setSpeed(desiredSpeed);
}

void QVImageCore::rotateImage(int rotation)
{
    currentRotation += rotation;

    // normalize between 360 and 0
    currentRotation = (currentRotation % 360 + 360) % 360;
    QTransform transform;

    QImage transformedImage;
    // if (currentFileDetails.isMovieLoaded) {
        // transform.rotate(currentRotation);
        // transformedImage = loadedMovie.currentImage().transformed(transform);
    // } else {
        transform.rotate(rotation);
        transformedImage = loadedPixmap.toImage().transformed(transform);
    // }

    loadedPixmap.convertFromImage(transformedImage);

    currentFileDetails.loadedPixmapSize = QSize(loadedPixmap.width(), loadedPixmap.height());
    emit updateLoadedPixmapItem();
}

QImage QVImageCore::matchCurrentRotation(const QImage &imageToRotate)
{
    if (!currentRotation)
        return imageToRotate;

    QTransform transform;
    transform.rotate(currentRotation);
    return imageToRotate.transformed(transform);
}

// TODO: Remove this function---extremely inefficient
QPixmap QVImageCore::matchCurrentRotation(const QPixmap &pixmapToRotate)
{
    if (!currentRotation)
        return pixmapToRotate;

    return QPixmap::fromImage(matchCurrentRotation(pixmapToRotate.toImage()));
}

// TODO: move expensive functions to vips
// TODO: This whole scheme of scaling qpixmap is leading to very high mem usage
// i guarantee it
QPixmap QVImageCore::scaleExpensively(const int desiredWidth, const int desiredHeight)
{
    return scaleExpensively(QSizeF(desiredWidth, desiredHeight));
}

QPixmap QVImageCore::scaleExpensively(const QSizeF desiredSize)
{
    if (!currentFileDetails.isPixmapLoaded)
        return QPixmap();

    QSize size = QSize(loadedPixmap.width(), loadedPixmap.height());
    size.scale(desiredSize.toSize(), Qt::KeepAspectRatio);

    // Get the current frame of the animation if this is an animation
    QPixmap relevantPixmap;
    if (!currentFileDetails.isMovieLoaded) {
        relevantPixmap = loadedPixmap;
    // } else {
    //     relevantPixmap = loadedMovie.currentPixmap();
    //     relevantPixmap = matchCurrentRotation(relevantPixmap);
    }

    // If we are really close to the original size, just return the original
    if (abs(desiredSize.width() - relevantPixmap.width()) < 1
        && abs(desiredSize.height() - relevantPixmap.height()) < 1) {
        return relevantPixmap;
    }

    return relevantPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ;
}

void QVImageCore::settingsUpdated()
{
    auto &settingsManager = qvApp->getSettingsManager();

    // preloading mode
    // TODO: Handle setting

    // update folder info to reflect new settings (e.g. sort order)
    updateFolderInfo();

    bool changedImagePreprocessing = false;

    // colorspaceconversion
    if (colorSpaceConversion != qvGetSettingInt(ColorSpaceConversion)) {
        colorSpaceConversion = qvGetSettingInt(ColorSpaceConversion);
        changedImagePreprocessing = true;
    }

    if (changedImagePreprocessing && currentFileDetails.isPixmapLoaded)
        loadFile(currentFileDetails.fileInfo.absoluteFilePath());
}

void QVImageCore::FileDetails::updateLoadedIndexInFolder()
{
    const QString targetPath = fileInfo.absoluteFilePath().normalized(QString::NormalizationForm_D);
    for (int i = 0; i < folderFileInfoList.length(); i++) {
        // Compare absoluteFilePath first because it's way faster, but double-check with
        // QFileInfo::operator== because it respects file system case sensitivity rules
        QString candidatePath =
                folderFileInfoList[i].absoluteFilePath.normalized(QString::NormalizationForm_D);
        if (candidatePath.compare(targetPath, Qt::CaseInsensitive) == 0
            && QFileInfo(folderFileInfoList[i].absoluteFilePath) == fileInfo) {
            loadedIndexInFolder = i;
            return;
        }
    }
    loadedIndexInFolder = -1;
}
