#ifndef ROMEO_NVG_REFLEX_H
#define ROMEO_NVG_REFLEX_H

#if defined(SHADER_BUS) && SHADER_BUS >= 7
#include "romeo_nvg_aim_projection.h"
uniform float4 bus_romeo_nvg;
#endif

uint RomeoNvgReflexLayout()
{
#if defined(SHADER_BUS) && SHADER_BUS >= 7
    uint layout = uint(round(bus_romeo_nvg.w));
    if (bus_romeo_nvg.x > 0.5 && bus_romeo_nvg_aim.x > 0.0
        && layout >= RomeoNvgBinoBezelLayout && layout <= RomeoNvgDigitalBezelLayout)
        return layout;
#endif
    return 0u;
}

float2 RomeoNvgReflexUv(float4 clipPosition)
{
    return clipPosition.xy / clipPosition.w * float2(0.5, -0.5) + 0.5;
}

float4 RomeoNvgReflexPosition(float4 clipPosition)
{
#if defined(SHADER_BUS) && SHADER_BUS >= 7
    uint layout = RomeoNvgReflexLayout();
    if (layout != 0u)
    {
        float tubeFov = radians(bus_romeo_nvg.z);
        float2 tangent = max(abs(pos_decompression_params.xy), 0.001);
        float baseScale = 1.0;
        float2 digitalRadius = 0.0;
        if (layout == RomeoNvgBinoBezelLayout)
            baseScale = RomeoNvgBinoSceneScale(tangent.y, tubeFov);
        else if (layout == RomeoNvgQuadBezelLayout)
            baseScale = min(1.0, tan(radians(41.5)) / tangent.y);
        else
            digitalRadius = (0.5 * tan(tubeFov * 0.5) / tangent) * float2(1.0, 0.75);

        RomeoNvgAimView projection = RomeoNvgMakeAimView(layout, baseScale, digitalRadius);
        float2 source = RomeoNvgReflexUv(clipPosition);
        float2 destination = 0.5 + (source - 0.5 + projection.offset) / projection.scale;
        clipPosition.xy = (destination - 0.5) * float2(2.0, -2.0) * clipPosition.w;
        // Native source depth is checked in the pixel shader. This layer draws
        // after the goggle geometry at the reprojected screen location.
        clipPosition.z = 0.0;
    }
#endif
    return clipPosition;
}

#ifdef ROMEO_NVG_REFLEX_PIXEL
Texture2D<float4> s_romeo_reflex_viewports;
#ifdef USE_MSAA
Texture2DMS<float4> s_romeo_reflex_position;
#else
Texture2D<float4> s_romeo_reflex_position;
#endif

void RomeoNvgReflexClip(float2 pixel, float4 originalClip, float weaponDepth)
{
#if defined(SHADER_BUS) && SHADER_BUS >= 7
    uint layout = RomeoNvgReflexLayout();
    if (layout == 0u)
        return;
    clip(pixel.x - screen_res.x * 0.5);
    uint viewportBit = 1u << (layout - 5u);
    uint width, height;
    s_romeo_reflex_viewports.GetDimensions(width, height);
    float2 relative = (pixel - screen_res.xy * 0.5) / screen_res.y;
    int2 size = int2(width, height);
    int2 samplePixel = int2(floor(relative * height + float2(size) * 0.5));
    if (any(samplePixel < 0) || any(samplePixel >= size))
        discard;
    uint apertures = uint(round(255.0 * s_romeo_reflex_viewports.Load(int3(samplePixel, 0)).r));
    clip((apertures & viewportBit) != 0 ? 1.0 : -1.0);

    float2 source = RomeoNvgReflexUv(originalClip);
    if (any(source < 0.0) || any(source >= 1.0))
        discard;
    int2 p = int2(source * screen_res.xy);
#ifdef USE_MSAA
    float depth = s_romeo_reflex_position.Load(p, 0).z;
#else
    float depth = s_romeo_reflex_position.Load(int3(p, 0)).z;
#endif
    if (depth > 0.0 && depth + 0.01 < weaponDepth)
        discard;
#endif
}
#endif
#endif
