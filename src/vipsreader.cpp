#include "vipsreader.h"
#include "qvimagecore.h"
#include <QtGlobal>

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
vips::VImage VipsReader::createReadPipeline(const QString &fileName, const std::optional<QString> &targetIccProfileFileName, bool isThumbnail)
{
    // TODO: magick fallback is not working well, avif for example didn't
    // seem to work
    vips::VImage in = isThumbnail
            ? vips::VImage::thumbnail(fileName.toUtf8().constData(), THUMBNAIL_SIZE) // TODO: magic number
            : vips::VImage::new_from_file(
                      fileName.toUtf8().constData(),
                      vips::VImage::option()->set("access", VIPS_ACCESS_SEQUENTIAL));

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
    vips::VImage a = in.extract_band(3);
    vips::VImage r = in.extract_band(0);
    vips::VImage g = in.extract_band(1);
    vips::VImage b = in.extract_band(2);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    in = in.bandjoin({ b, g, r, a });
#elif Q_BYTE_ORDER == Q_BIG_ENDIAN
    in = in.bandjoin({ a, r, g, b });
#endif

    return in;
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
    QFile nullFile;
#ifdef Q_OS_WIN
    nullFile.setFileName("NUL");
#else
    nullFile.setFileName("/dev/null");
#endif
    nullFile.open(QIODevice::WriteOnly);

    vips::VImage img = createReadPipeline(fileName, targetIccProfileFileName, false);

    img.write_to_file(nullFile.fileName().toUtf8().constData());
}

VipsReader::ReadResult VipsReader::read(const QString &fileName,
                                        const std::optional<QString> &targetIccProfileFileName)
{
    ReadResult result;
    try {
        vips::VImage in = createReadPipeline(fileName, targetIccProfileFileName, false);

        // Write result to buffer
        size_t buffer_size;
        void *buffer = in.write_to_memory(&buffer_size);

        auto cleanup = [](void *info) { g_free(info); };

        result.image = QImage(static_cast<const uchar *>(buffer), in.width(), in.height(),
                              in.width() * 4, QImage::Format_ARGB32_Premultiplied, cleanup, buffer);
        if (result.image.isNull()) {
            throw vips::VError("Produced null QImage during conversion");
        }
    } catch (const vips::VError &e) {
        // TODO: Is this memory valid?
        result.error = e.what();
    }
    return result;
}
