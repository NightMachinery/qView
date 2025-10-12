#include "vipsreader.h"
#include <QtGlobal>

#include <iostream>
#include <vips/vips8>
#include <glib.h>

#include <cstddef>
#include <string>


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

    // Strip away non-standard color channels (non RGB or RGBA)
    if (in.bands() > 4 && in.has_alpha())
    {
        in = in.extract_band(0, vips::VImage::option()->set("n", 4));
    } else if (in.bands() > 3 && !in.has_alpha())
    {
        in = in.extract_band(0, vips::VImage::option()->set("n", 3));
    }

    if (!in.has_alpha())
    {
        in = in.bandjoin_const({255.0});
    }

    return in;
}

void VipsReader::preload(const QString &fileName)
{
    vips::VImage image = createPipeline(fileName, false);
    (void)image.width(); // todo stupid
}

VipsReader::ReadResult VipsReader::read(const QString &fileName, const QByteArray &targetIccProfile)
{
    ReadResult result;
    try
    {
        vips::VImage in = createPipeline(fileName, false);

        // TODO: Open questions:
        // - Can we do this lazily?
        // - Should we do this on preload?

        // If we have no embedded ICC profile, interpret as sRGB
        // if (in.get_typeof("icc-profile-data") == 0 && in.interpretation() != VIPS_INTERPRETATION_sRGB)
        // {
        //     try
        //     {
        //         in = in.colourspace(VIPS_INTERPRETATION_sRGB);
        //     }
        //     catch (const vips::VError &)
        //     {
        //         // ignore errors
        //     }
        // }


        // Transform the image color space from the embedded profile to the target profile
        const auto iccTransformOptions = vips::VImage::option()->set("embedded", true);
        if (!targetIccProfile.isEmpty())
        {
            in = in.icc_transform(targetIccProfile.constData(), iccTransformOptions);
        } else {
            // In the absence of a monitor profile, treat as sRGB
            in = in.icc_transform("sRGB", iccTransformOptions);
        }

        // Write result to buffer
        size_t buffer_size;
        void *buffer = in.write_to_memory(&buffer_size);

        auto cleanup = [](void *info)
        {
            g_free(info);
        };

        result.image = QImage(static_cast<const uchar *>(buffer), in.width(), in.height(), in.width() * in.bands(), QImage::Format_RGBA8888, cleanup, buffer);
    }
    catch (const vips::VError &e)
    {
        // TODO: Is this memory valid?
        result.error = e.what();
    }
    return result;
}
