#ifndef _VIPSREADER_H
#define _VIPSREADER_H

#include <QByteArray>
#include <QImage>
#include <QString>

namespace vips {
class VImage;
}

class VipsReader
{
public:
    struct ReadResult
    {
        QVector<QImage> images;
        QVector<size_t> delays;
        QString error;
    };

    static void init();
    static void shutdown();

    static size_t getMemoryUsage();
    static size_t getCacheMaxMemoryUsage();
    static int getCacheSize();
    static void clearCache();

    static void preload(const QString &fileName, const std::optional<QString> &targetIccProfileFileName);
    static ReadResult read(const QString &fileName, const std::optional<QString> &targetIccProfileFileName);

private:
    static vips::VImage createReadPipeline(const QString &fileName, const std::optional<QString> &targetIccProfileFileName, bool isThumbnail, bool loadAllPages);
    static vips::VImage finalizePipeline(vips::VImage in, const std::optional<QString> &targetIccProfileFileName);
    static QImage writeToQImage(vips::VImage in);
};

#endif /* _VIPSREADER_H */
