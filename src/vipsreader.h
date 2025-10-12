#ifndef VIPSREADER_H
#define VIPSREADER_H

#include <QByteArray>
#include <QImage>
#include <QString>


namespace vips
{
class VImage;
}


class VipsReader
{
public:
    struct ReadResult
    {
        QImage image;
        QString error;
    };

    static ReadResult read(const QString &fileName, const QString &targetIccProfileFileName);

    static void init();
    static void preload(const QString &fileName);
    static void shutdown();

private:
    static vips::VImage readFile(const QString &fileName, bool isThumbnail);
};

#endif // VIPSREADER_H
