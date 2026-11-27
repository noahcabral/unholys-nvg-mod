#ifndef ROMEO_NVG_FOCUS_H
#define ROMEO_NVG_FOCUS_H

#include "romeo_nvg_fit.h"

#if defined(SHADER_BUS) && SHADER_BUS >= 7 && !defined(USE_MSAA)
uniform float4 bus_romeo_nvg_fit;
Texture2D<float4> s_romeo_nvg_viewports;

uint RomeoNvgViewportBit(uint layout)
{
    switch (layout)
    {
    case 2u: return 1u;  // Analog mono
    case RomeoNvgBinoBezelLayout: return 2u;
    case RomeoNvgQuadBezelLayout: return 4u;
    case RomeoNvgDigitalBezelLayout: return 8u;
    case RomeoNvgDigitalMonoLayout: return 16u;
    default: return 0u;
    }
}

bool RomeoNvgProtectedViewport(float2 uv, int2 maskSize, uint bit)
{
    float2 relative = (uv - 0.5) * screen_res.xy / screen_res.y;
    int2 pixel = int2(floor(relative * maskSize.y + float2(maskSize) * 0.5));
    if (any(pixel < 0) || any(pixel >= maskSize))
        return false;
    uint packed = uint(round(255.0 * s_romeo_nvg_viewports.Load(int3(pixel, 0)).r));
    return (packed & bit) != 0u;
}

float3 RomeoNvgBlurOutside(Texture2D<float4> frame, float2 uv, int2 frameSize,
    int2 maskSize, uint bit, float radius)
{
    static const float2 offsets[8] = {
        float2(0.47, 0.0), float2(-0.47, 0.0), float2(0.0, 0.47), float2(0.0, -0.47),
        float2(0.70, 0.70), float2(-0.70, 0.70), float2(-0.70, -0.70), float2(0.70, -0.70)
    };
    int2 centre = clamp(int2(uv * frameSize), 0, frameSize - 1);
    float3 original = frame.Load(int3(centre, 0)).rgb;
    float3 color = original * 2.0;
    float weight = 2.0;
    [unroll]
    for (uint i = 0; i < 8u; ++i)
    {
        int2 samplePixel = clamp(centre + int2(round(offsets[i] * radius)), 0, frameSize - 1);
        float2 sampleUv = (float2(samplePixel) + 0.5) / frameSize;
        // Point samples keep each footprint entirely outside the protected
        // apertures. Renormalize on both sides of every bezel.
        if (!RomeoNvgProtectedViewport(sampleUv, maskSize, bit))
        {
            float tap = i < 4u ? 1.25 : 0.75;
            color += frame.Load(int3(samplePixel, 0)).rgb * tap;
            weight += tap;
        }
    }
    return color / weight;
}

float3 RomeoNvgSurroundFocus(Texture2D<float4> frame, float2 uv0, float2 uv1, float2 pixel, float3 original)
{
    float focus = smoothstep(0.0, 1.0, bus_romeo_nvg_fit.y);
    if (focus <= 0.0 || ssfx_issvp.x > 0.5)
        return original;
    uint bit = RomeoNvgViewportBit(uint(round(bus_romeo_nvg.w)));
    if (bit == 0u)
        return original;
    uint w, h;
    s_romeo_nvg_viewports.GetDimensions(w, h);
    int2 maskSize = int2(w, h);
    if (any(maskSize <= 0) || RomeoNvgProtectedViewport(pixel / screen_res.xy, maskSize, bit)
        || RomeoNvgProtectedViewport(uv0, maskSize, bit) || RomeoNvgProtectedViewport(uv1, maskSize, bit))
        return original;
    frame.GetDimensions(w, h);
    int2 frameSize = int2(w, h);
    if (any(frameSize <= 0))
        return original;

    float metres = s_romeo_ecoti_position.Load(int3(int2(pixel), 0)).z;
    float radius = 8.0 * RomeoNvgNearFocus(metres) * focus * (float(h) / 1041.0);
    if (radius < 1.0)
        return original;
    float3 first = RomeoNvgBlurOutside(frame, uv0, frameSize, maskSize, bit, radius);
    if (any(int2(uv0 * frameSize) != int2(uv1 * frameSize)))
        first = 0.5 * (first + RomeoNvgBlurOutside(frame, uv1, frameSize, maskSize, bit, radius));
    return lerp(original, first, smoothstep(1.0, 3.0, radius));
}
#endif

#endif
