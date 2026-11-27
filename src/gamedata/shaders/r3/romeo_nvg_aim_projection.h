#ifndef ROMEO_NVG_AIM_PROJECTION_H
#define ROMEO_NVG_AIM_PROJECTION_H

#include "romeo_nvg_fit.h"
uniform float4 bus_romeo_nvg_aim;

float2 RomeoNvgRightOpticalCentre(uint layout)
{
    switch (layout)
    {
    case RomeoNvgBinoBezelLayout:
        return RomeoNvgBinoOpticalCentre(true);
    case RomeoNvgQuadBezelLayout:
        return float2(245.90 / 1041.0, 0.0);
    case RomeoNvgDigitalBezelLayout:
        return RomeoNvgDigitalPivot(true) + RomeoNvgDigitalFitDelta(RomeoNvgDigitalPivot(true), 1.0, true);
    default:
        return 0.0;
    }
}

struct RomeoNvgAimView
{
    float2 offset;
    float scale;
};

RomeoNvgAimView RomeoNvgMakeAimView(uint layout, float baseScale, float2 digitalRadius)
{
    RomeoNvgAimView projection;
    float2 heightToUv = screen_res.y / screen_res.xy;
    float2 rightCentre = RomeoNvgRightOpticalCentre(layout);
    float2 shift = rightCentre * heightToUv * saturate(bus_romeo_nvg_aim.x);
    projection.offset = 0.0;
    projection.scale = baseScale;
    if (!any(shift))
        return projection;

    // Leave enough source image for the apertures on the opposite side.
    // One scale for every tube keeps the scene continuous at the bezel.
    float2 halfExtent = 0.5;
    if (layout == RomeoNvgBinoBezelLayout)
        halfExtent = min(0.5, (abs(rightCentre) + RomeoNvgBinoFitRadius) * heightToUv);
    else if (layout == RomeoNvgDigitalBezelLayout)
    {
        float2 leftCentre = RomeoNvgDigitalPivot(false);
        leftCentre += RomeoNvgDigitalFitDelta(leftCentre, 1.0, false);
        halfExtent = max(abs(leftCentre), abs(rightCentre)) * heightToUv + digitalRadius;
    }
    float2 scaleLimit = (0.5 - 0.5 / screen_res.xy) / (halfExtent + abs(shift));
    projection.scale = min(baseScale, min(scaleLimit.x, scaleLimit.y));
    projection.offset = shift * projection.scale;
    return projection;
}

float2 RomeoNvgAimSample(float2 uv, RomeoNvgAimView projection)
{
    return 0.5 + (uv - 0.5) * projection.scale - projection.offset;
}

#endif
