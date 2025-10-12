#include "vipsreader.h"
#include <QtGlobal>

#include <vips/vips8>
#include <glib.h>

#include <cstddef>
#include <string>
#include <utility>

void VipsReader::init()
{
    if (VIPS_INIT(nullptr) != 0)
    {
        qFatal("Failed to initialize VIPS");
    }
    vips_cache_set_max_mem(1024 * 1024 * 1024);
    vips_cache_set_max(10000);
}

void VipsReader::shutdown()
{
    vips_shutdown();
}

// TODO: Lazily load full-res image by using isThumbnail
vips::VImage VipsReader::createPipeline(const QString &fileName, bool isThumbnail)
{
    vips::VImage in = isThumbnail
            ? vips::VImage::thumbnail(fileName.toUtf8().constData(), 256) // TODO: magic number
            : vips::VImage::new_from_file(fileName.toUtf8().constData(),
                                            vips::VImage::option()->set("access", VIPS_ACCESS_SEQUENTIAL));

    if (in.interpretation() == VIPS_INTERPRETATION_ERROR)
    {
        throw vips::VError("Vips Error: Cannot interpret image");
    }

    if (in.bands() > 4 && in.has_alpha())
    {
        in = in.extract_band(0, vips::VImage::option()->set("n", 4));
    } else if (in.bands() > 3 && !in.has_alpha())
    {
        in = in.extract_band(0, vips::VImage::option()->set("n", 3));
    }

    if (in.interpretation() != VIPS_INTERPRETATION_sRGB)
    {
        try
        {
            in = in.colourspace(VIPS_INTERPRETATION_sRGB);
        } catch (const vips::VError&)
        {
            // ignore errors
        }
    }

    if (!in.has_alpha())
    {
        in = in.bandjoin_const({255.0});
    }

    return in;
}

void VipsReader::preload(const QString &fileName)
{
    try
    {
        vips::VImage image = createPipeline(fileName, false);
        (void)image.width(); // todo stupid
    }
    catch (const vips::VError &)
    {
        // Preload failures are not critical.
    }
}

VipsReader::ReadResult VipsReader::read(const QString &fileName)
{
    try
    {
        vips::VImage in = createPipeline(fileName, false);
        size_t buffer_size;
        void *buffer = in.write_to_memory(&buffer_size);

        auto cleanup = [](void *info)
        {
            g_free(info);
        };

        QImage resultImage(static_cast<const uchar *>(buffer), in.width(), in.height(), in.width() * in.bands(), QImage::Format_RGBA8888, cleanup, buffer);

        QByteArray colorProfile;
        if (in.get_typeof("icc-profile-data") != 0) {
            const void *profile_data = nullptr;
            size_t profile_size = 0;
            if (vips_image_get_blob(in.get_image(), "icc-profile-data", &profile_data, &profile_size) == 0) {
                if (profile_size > 0 && profile_data) {
                    colorProfile = QByteArray(static_cast<const char*>(profile_data), static_cast<int>(profile_size));
                }
            }
        }

        return {std::move(resultImage), std::move(colorProfile), QString()};
    }
    catch (const vips::VError &e)
    {
        return {QImage(), QByteArray(), QString::fromStdString(e.what())};
    }
}
