#include "vipsreader.h"
#include "qvimagecore.h"
#include <QtGlobal>

#include <iostream>
#include <vips/vips8>
#include <vips/memory.h>
#include <vips/operation.h>
#include <glib.h>

#include <cstddef>
#include <string>

namespace {
static const size_t THUMBNAIL_SIZE = 4096;
static const size_t CACHE_MAX_MEM = 1024 * 1024 * 1024;
static const size_t CACHE_MAX_FILES = 100;
static const size_t CACHE_MAX_OPERATIONS = 10000;
}

void VipsReader::init()
{
    if (VIPS_INIT(nullptr) != 0) {
        qFatal("Failed to initialize VIPS");
    }
    // TODO: Tweak caching depending on preloading mode, maybe fully
    // configurable in future
    vips_cache_set_max_mem(CACHE_MAX_MEM);
    vips_cache_set_max_files(CACHE_MAX_FILES);
    vips_cache_set_max(CACHE_MAX_OPERATIONS);
}

void VipsReader::shutdown()
{
    vips_shutdown();
}

// TODO: Lazily load full-res image by using isThumbnail
vips::VImage VipsReader::createReadPipeline(const QString &fileName, const std::optional<QString> &targetIccProfileFileName, bool isThumbnail, bool loadAllPages)
{
    auto options = vips::VImage::option()->set("access", VIPS_ACCESS_SEQUENTIAL);
    if (loadAllPages) {
        options = options->set("n", -1);
    }

    vips::VImage in = vips::VImage::new_from_file(
                      fileName.toUtf8().constData(),
                      options);

    return in;
}

vips::VImage VipsReader::finalizePipeline(vips::VImage in, const std::optional<QString> &targetIccProfileFileName)
{
        if (in.interpretation() == VIPS_INTERPRETATION_ERROR) {
        throw vips::VError("Vips Error: Cannot interpret image");
    }

    // If we have no embedded ICC profile, interpret as sRGB
    const bool hasEmbeddedProfile = in.get_typeof("icc-profile-data") != 0;
    if (!hasEmbeddedProfile && in.interpretation() != VIPS_INTERPRETATION_sRGB) {
        in = in.colourspace(VIPS_INTERPRETATION_sRGB);
    }

    // Transform the image color space from the embedded profile to the target profile
    const auto iccTransformOptions = vips::VImage::option()->set("embedded", true);
    if (targetIccProfileFileName.has_value()) {
        in = in.icc_transform(targetIccProfileFileName->toUtf8().constData(),
                                iccTransformOptions);
    } else {
        // In the absence of a monitor profile, treat as sRGB
        in = in.icc_transform("sRGB", iccTransformOptions);
    }

    // Strip away non-standard color channels (non RGB or RGBA)
    if (in.bands() > 4 && in.has_alpha()) {
        in = in.extract_band(0, vips::VImage::option()->set("n", 4));
    } else if (in.bands() > 3 && !in.has_alpha()) {
        in = in.extract_band(0, vips::VImage::option()->set("n", 3));
    }

    // Add const alpha channel if it is not present
    if (!in.has_alpha()) {
        in = in.bandjoin_const({ 255.0 });
    }

    // convert to correct format for QImage::Format_ARGB32_Premultiplied
    // Already premultiplied for some reason

    // RGBA -> ARGB
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    in = in.bandjoin({
        in.extract_band(2), // b
        in.extract_band(1), // g
        in.extract_band(0), // r
        in.extract_band(3), // a
    });
#elif Q_BYTE_ORDER == Q_BIG_ENDIAN
    in = in.bandjoin({
        in.extract_band(3), // a
        in.extract_band(0), // r
        in.extract_band(1), // g
        in.extract_band(2), // b
    });
#endif

    return in;
}

QImage VipsReader::writeToQImage(vips::VImage in)
{
    size_t buffer_size;
    void *buffer = in.write_to_memory(&buffer_size);
    auto cleanupFn = [](void *info) { g_free(info); };

    QImage image = QImage(static_cast<const uchar *>(buffer), in.width(), in.height(),
                          in.width() * 4, QImage::Format_ARGB32_Premultiplied, cleanupFn, buffer);
    if (image.isNull()) {
        throw vips::VError("Produced null QImage during conversion");
    }
    return image;
}

size_t VipsReader::getMemoryUsage()
{
    return vips_tracked_get_mem();
}

size_t VipsReader::getCacheMaxMemoryUsage()
{
    return vips_cache_get_max_mem();
}

int VipsReader::getCacheSize()
{
    return vips_cache_get_size();
}

void VipsReader::clearCache()
{
    vips_cache_drop_all();
}

void VipsReader::preload(const QString &fileName, const std::optional<QString> &targetIccProfileFileName)
{
    try {
    QFile nullFile;
#ifdef Q_OS_WIN
    nullFile.setFileName("NUL");
#else
    nullFile.setFileName("/dev/null");
#endif
    nullFile.open(QIODevice::WriteOnly);

    vips::VImage img = createReadPipeline(fileName, targetIccProfileFileName, false, false);
    img = finalizePipeline(img, targetIccProfileFileName);

    // TODO: this doesn't work i think
    img.write_to_file(nullFile.fileName().toUtf8().constData());
    } catch (const vips::VError &e) {
        qWarning() << "Failed to preload image" << fileName << e.what();
    }
}

// TODO: Handle embedded orientation data
VipsReader::ReadResult VipsReader::read(const QString &fileName,
                                        const std::optional<QString> &targetIccProfileFileName)
{
    ReadResult result;
    try {
        // TODO: Lazily load
        vips::VImage in = createReadPipeline(fileName, targetIccProfileFileName, false, true);
        in = finalizePipeline(in, targetIccProfileFileName);

        const int nPages = vips_image_get_n_pages(in.get_image());
        if (nPages > 1) {
            int pageHeight = vips_image_get_page_height(in.get_image());

            QVector<QImage> frames;
            frames.reserve(nPages);

            for (int i = 0; i < nPages; ++i) {
                // std::cout << "processing page " << i << std::endl;
                vips::VImage page = in.extract_area(0, i * pageHeight, in.width(), pageHeight);


                frames.append(std::move(writeToQImage(page)));
            }
            result.images = std::move(frames);
            return result;
        }

        result.images.append(writeToQImage(in));
    } catch (const vips::VError &e) {
        // TODO: Is this memory valid?
        result.error = e.what();
    }
    return result;
}
