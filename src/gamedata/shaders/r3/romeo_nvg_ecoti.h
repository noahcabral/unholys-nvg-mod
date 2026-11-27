#ifndef ROMEO_NVG_ECOTI_H
#define ROMEO_NVG_ECOTI_H

#include "romeo_nvg_fit.h"

#if defined(SHADER_BUS) && SHADER_BUS >= 7 && !defined(USE_MSAA)
#include "romeo_nvg_aim_projection.h"
uniform float4 bus_romeo_nvg;
Texture2D<float4> s_romeo_ecoti_position;
Texture2D<float4> s_romeo_ecoti_heat;

#define ROMEO_NVG_DEPTH_TEXTURE Texture2D<float4>
#define ROMEO_NVG_READ_DEPTH(t, p) t.Load(int3(p, 0)).z
#include "romeo_nvg_profiles.h"

float3 RomeoNvgEcoti(float2 pixel, float3 image)
{
    if (uint(round(bus_romeo_nvg.x)) != 3u)
        return image;

    // These coordinates describe the fully deployed camera attachment in
    // viewport-height units. The active layout comes from the equipped item.
    uint layout = uint(round(bus_romeo_nvg.w));
    float2 view = (pixel - screen_res.xy * 0.5) / screen_res.y;
    float2 centre;
    float2 radius;
    bool quadBezel = layout == RomeoNvgQuadBezelLayout;
    if (layout == 5u || quadBezel)
    {
        bool right = view.x >= 0.0;
        centre = float2(right ? 0.31497 : -0.31497, -0.00931);
        radius = float2(0.15990, 0.16054);
        if (quadBezel)
        {
            centre += RomeoNvgFitDelta(centre, 1.0, true, right);
            radius *= RomeoNvgQuadFitScale;
        }
    }
    else if (layout == 4u)
    {
        centre = float2(view.x < 0.0 ? -0.24887 : 0.24887, -0.00714);
        radius = float2(0.12530, 0.12720);
    }
    else if (layout == 3u)
    {
        centre = float2(view.x < 0.0 ? -0.303 : 0.303, -0.0022);
        radius = 0.159;
    }
    else if (layout == 1u)
    {
        centre = float2(0.364, 0.0);
        radius = 0.234;
    }
    else if (layout == 2u)
    {
        centre = float2(0.0, 0.0);
        radius = 0.236;
    }
    else if (layout == RomeoNvgBinoBezelLayout)
    {
        centre = RomeoNvgRightOpticalCentre(layout);
        radius = RomeoNvgBinoFitRadius;
    }
    else
        return image;

    float2 lens = (view - centre) / radius;
    float tubeFov = radians(bus_romeo_nvg.z);
    // The corrected quad's projected conic already accounts for 30/40 degrees.
    float range = layout == 5u || quadBezel ? 1.0 : RomeoNvgThermalRadius(tubeFov);
    float mask = 1.0 - smoothstep(range * 0.985, range, length(lens));
    if (mask <= 0.0)
        return image;

    float2 viewTangent = max(abs(pos_decompression_params.xy), 0.001);
    float2 uv;
    if (layout == RomeoNvgBinoBezelLayout)
    {
        float baseScale = RomeoNvgBinoSceneScale(viewTangent.y, tubeFov);
        uv = RomeoNvgAimSample(pixel / screen_res.xy, RomeoNvgMakeAimView(layout, baseScale, 0.0));
    }
    else if (layout >= 3u)
    {
        float baseScale = min(1.0, tan(radians(41.5)) / viewTangent.y);
        uv = RomeoNvgAimSample(pixel / screen_res.xy, RomeoNvgMakeAimView(layout, baseScale, 0.0));
    }
    else
    {
        float2 centreUv = 0.5 + centre * screen_res.y / screen_res.xy;
        uv = centreUv + lens * (0.5 * tan(tubeFov * 0.5) / viewTangent);
    }

    uint width, height;
    s_romeo_ecoti_heat.GetDimensions(width, height);
    if (width == 0 || height == 0)
        return image;
    int2 lastPixel = int2(width, height) - 1;
    int2 sourcePixel = min(int2(saturate(uv) * float2(width, height)), lastPixel);
    float contour = RomeoNvgThermalOutline(s_romeo_ecoti_heat, s_romeo_ecoti_position, sourcePixel, lastPixel);
    return lerp(image, float3(0.72, 1.0, 0.94), contour * mask);
}
#endif

#endif
