#ifndef ROMEO_NVG_LENS_H
#define ROMEO_NVG_LENS_H

#include "common.h"
#include "romeo_nvg_fit.h"

Texture2D<float4> s_romeo_lens_scene;
Texture2D<float4> s_romeo_lens_blur;

#if defined(SHADER_BUS) && SHADER_BUS >= 7
#include "romeo_nvg_aim_projection.h"
uniform float4 bus_romeo_nvg;
uniform float4 bus_romeo_nvg_fit;
uniform float4 bus_romeo_nvg_motion;
uniform float4 bus_romeo_nvg_beam_start;
uniform float4 bus_romeo_nvg_beam_finish;
uniform float4 bus_romeo_nvg_halo_1;
uniform float4 bus_romeo_nvg_halo_2;
uniform float4 bus_romeo_nvg_halo_3;
uniform float4 bus_romeo_nvg_halo_4;
uniform float4 shader_param_6;
uniform float4 ssfx_issvp;

#ifdef USE_MSAA
#define ROMEO_NVG_GEOMETRY Texture2DMS<float4>
#define ROMEO_NVG_READ_DEPTH(t, p) t.Load(p, 0).z
#define ROMEO_NVG_READ_RGB(t, p) t.Load(p, 0).rgb
#else
#define ROMEO_NVG_GEOMETRY Texture2D<float4>
#define ROMEO_NVG_READ_DEPTH(t, p) t.Load(int3(p, 0)).z
#define ROMEO_NVG_READ_RGB(t, p) t.Load(int3(p, 0)).rgb
#endif

ROMEO_NVG_GEOMETRY s_romeo_lens_position;
ROMEO_NVG_GEOMETRY s_romeo_lens_albedo;

float RomeoNvgEmitterHalo(float4 source, float2 uv, Texture2D<float4> blurred, SamplerState linearClamp)
{
    if (source.w <= 0.0)
        return 0.0;
    float4 position = mul(m_VP, float4(source.xyz, 1.0));
    if (position.w <= 0.05)
        return 0.0;
    float2 centre = position.xy / position.w * float2(0.5, -0.5) + 0.5;
    if (any(centre < 0.0) || any(centre >= 1.0))
        return 0.0;
    float2 tangent = max(abs(pos_decompression_params.xy), 0.001);
    float2 offset = (uv - centre) * (2.0 * position.w / source.w) * tangent;
    float distanceSquared = dot(offset, offset);
    if (distanceSquared >= 4.0)
        return 0.0;

    int2 pixel = int2(centre * screen_res.xy);
    float depth = ROMEO_NVG_READ_DEPTH(s_romeo_lens_position, pixel);
    if (depth > 0.0 && depth + source.w < position.w)
        return 0.0;
    float3 glow = blurred.SampleLevel(linearClamp, centre, 0).rgb;
    float brightness = max(glow.r, max(glow.g, glow.b));
    return exp(-2.4 * distanceSquared) * (1.0 - smoothstep(3.0, 4.0, distanceSquared))
        * lerp(0.20, 0.55, smoothstep(0.15, 0.90, brightness));
}

float RomeoNvgLensHalo(Texture2D<float4> blurred, SamplerState linearClamp, float2 uv)
{
    return RomeoNvgEmitterHalo(bus_romeo_nvg_halo_1, uv, blurred, linearClamp)
         + RomeoNvgEmitterHalo(bus_romeo_nvg_halo_2, uv, blurred, linearClamp)
         + RomeoNvgEmitterHalo(bus_romeo_nvg_halo_3, uv, blurred, linearClamp)
         + RomeoNvgEmitterHalo(bus_romeo_nvg_halo_4, uv, blurred, linearClamp);
}

float RomeoNvgShadowReturn(int2 pixel, float depth)
{
    if (depth <= 0.0)
        return 0.0;
    float3 surface = ROMEO_NVG_READ_RGB(s_romeo_lens_albedo, pixel);
    float reflectance = dot(surface, float3(0.2126, 0.7152, 0.0722));
    return reflectance * lerp(0.021, 0.006, smoothstep(3.0, 55.0, depth));
}

float RomeoNvgViewFocus(float metres)
{
    return RomeoNvgNearFocus(metres) * smoothstep(0.0, 1.0, bus_romeo_nvg_fit.y);
}

float3 RomeoNvgDepthDefocus(Texture2D<float4> scene, SamplerState samplerState,
    float2 uv, float metres, float2 dimensions, float3 sharp)
{
    // The tubes focus on the distance. Use scene depth rather than optic
    // identity so a nearby weapon and a nearby wall both lose detail.
    float radius = 58.0 * RomeoNvgViewFocus(metres) * dimensions.y / 1080.0;
    if (radius < 1.0)
        return sharp;

    static const float2 disk[12] = {
        float2(-0.19, -0.27), float2(0.43, 0.12), float2(-0.40, 0.32), float2(0.23, -0.62),
        float2(0.16, 0.64), float2(-0.69, -0.15), float2(0.72, -0.28), float2(-0.44, 0.70),
        float2(-0.44, -0.72), float2(0.80, 0.48), float2(-0.91, 0.31), float2(0.51, -0.84)
    };
    float3 filtered = sharp;
    float2 sampleRadius = radius / dimensions;
    [unroll]
    for (uint tap = 0; tap < 12; ++tap)
        filtered += scene.SampleLevel(samplerState, uv + disk[tap] * sampleRadius, 0).rgb;
    return lerp(sharp, filtered / 13.0, smoothstep(1.0, 4.0, radius));
}

#define ROMEO_NVG_DEPTH_TEXTURE ROMEO_NVG_GEOMETRY
#define ROMEO_NVG_LIGHT_TEXTURE Texture2D<float4>
#define ROMEO_NVG_SAMPLE_LIGHT(t, s, uv) RomeoNvgLensHalo(t, s, uv)
#define ROMEO_NVG_BRIGHTNESS clamp(bus_romeo_nvg.y, 0.25, 3.0)
#define ROMEO_NVG_DIGITAL_STREAK bus_romeo_nvg_motion.xy
#define ROMEO_NVG_ALBEDO(p) ROMEO_NVG_READ_RGB(s_romeo_lens_albedo, p)
#define ROMEO_NVG_SHADOW_RETURN(p, depth) RomeoNvgShadowReturn(p, depth)
#define ROMEO_NVG_ENVIRONMENT_LIGHT dot(L_ambient.rgb, float3(0.2126, 0.7152, 0.0722))
#define ROMEO_NVG_DEFOCUS RomeoNvgDepthDefocus
#define ROMEO_NVG_NEAR_BLUR(profile, metres) (2.5 + profile.nearBlurPixels) * RomeoNvgViewFocus(metres)
// Source positions define the halos. Bright scenery does not create a flare.
#define ROMEO_NVG_LOCAL_HALO_ONLY
#include "romeo_nvg_profiles.h"

float RomeoNvgBeam(float2 uv)
{
    // The controller's current toggle takes precedence over the bus's previous frame.
    if (bus_romeo_nvg_beam_start.w <= 0.0 || abs(shader_param_6.y) <= 0.0)
        return 0.0;

    // Sample the untouched world and weather ambient, before the NVG and the native laser.
    float ambient = dot(L_ambient.rgb, float3(0.2126, 0.7152, 0.0722));
    float exposure = max(smoothstep(0.013, 0.055, ambient), RomeoNvgSceneExposure(s_romeo_lens_scene, smp_rtlinear));
    if (exposure >= 1.0)
        return 0.0;

    float4 start = mul(m_VP, float4(bus_romeo_nvg_beam_start.xyz, 1.0));
    float4 finish = mul(m_VP, float4(bus_romeo_nvg_beam_finish.xyz, 1.0));
    const float near = 0.04;
    if (start.w < near && finish.w < near)
        return 0.0;
    if (start.w < near)
        start = lerp(start, finish, (near - start.w) / (finish.w - start.w));
    if (finish.w < near)
        finish = lerp(finish, start, (near - finish.w) / (start.w - finish.w));
    float2 origin = (start.xy / start.w * float2(0.5, -0.5) + 0.5) * screen_res.xy;
    float2 end = (finish.xy / finish.w * float2(0.5, -0.5) + 0.5) * screen_res.xy;
    float2 segment = end - origin;
    float2 samplePixel = uv * screen_res.xy;
    float t = saturate(dot(samplePixel - origin, segment) / max(dot(segment, segment), 0.001));
    float2 offset = samplePixel - (origin + segment * t);
    float radius = max(0.75, screen_res.y / 1080.0);
    float distance = length(offset) / radius;
    if (distance > 7.0)
        return 0.0;

    float metres = rcp(lerp(rcp(start.w), rcp(finish.w), t));
    int2 pixel = min(int2(samplePixel), int2(screen_res.xy) - 1);
    float surface = ROMEO_NVG_READ_DEPTH(s_romeo_lens_position, pixel);
    if (surface > 0.0 && metres > surface + 0.04)
        return 0.0;

    float core = 1.0 - smoothstep(0.8, 2.2, distance);
    float glare = 1.0 - smoothstep(0.9, 7.0, distance);
    float strength = saturate(0.55 + bus_romeo_nvg_beam_start.w * 0.30);
    return saturate(core * 0.90 + glare * 0.28) * (1.0 - exposure) * strength;
}

float2 RomeoNvgLensCentre(float2 pixel, float2 localPosition, out float2 screenRadius)
{
    // Project the center of this mesh opening back into the main camera.
    float2 dx = ddx(localPosition);
    float2 dy = ddy(localPosition);
    float determinant = dx.x * dy.y - dx.y * dy.x;
    screenRadius = float2(0.10, 0.13);
    if (abs(determinant) < 0.0000001)
        return pixel / screen_res.xy;
    screenRadius = (abs(float2(dy.y, dx.y)) + abs(float2(dy.x, dx.x)))
        / (abs(determinant) * screen_res.xy);
    float2 offset = float2(localPosition.x * dy.y - localPosition.y * dy.x,
        dx.x * localPosition.y - dx.y * localPosition.x) / determinant;
    return (pixel - offset) / screen_res.xy;
}

float2 RomeoNvgDigitalCentre(float2 pixel, uint layout)
{
    if (layout == RomeoNvgDigitalMonoLayout)
        return float2(0.5, 0.5);

    // Once powered, each display is in a fixed camera-relative pose. Use
    // one optical origin and one exposure meter across the curved mesh.
    bool right = pixel.x >= 0.5 * screen_res.x;
    float2 centre = RomeoNvgDigitalPivot(right);
    if (layout == RomeoNvgDigitalBezelLayout)
        centre += RomeoNvgDigitalFitDelta(centre, 1.0, right);
    return 0.5 + centre * screen_res.y / screen_res.xy;
}
#endif

float4 RomeoNvgLens(float2 pixel, float2 aperture, bool panorama)
{
    const float3 darkGlass = float3(0.0, 0.0, 0.0);
#if defined(SHADER_BUS) && SHADER_BUS >= 7
    uint generation = uint(round(bus_romeo_nvg.x));
    if (generation == 0 || generation > 5 || ssfx_issvp.x > 0.5)
        return float4(darkGlass, 1.0);

    float2 localPosition = aperture * 2.0 - 1.0;
    uint layout = uint(round(bus_romeo_nvg.w));
    bool digital = RomeoNvgIsDigital(generation);
    bool binoBezel = layout == RomeoNvgBinoBezelLayout;
    float2 screenRadius = 0.0;
    float2 centre = digital && !panorama
        ? RomeoNvgDigitalCentre(pixel, layout) : RomeoNvgLensCentre(pixel, localPosition, screenRadius);
    float2 lensPosition = digital ? localPosition / float2(0.84, 0.63) : localPosition;
    float opening = length(localPosition);
    float edge = digital ? RomeoNvgDigitalEdge(localPosition) : opening;
    if (binoBezel)
        clip(1.0 - opening);
    // The quad's two lobes are cut into the mesh and share a straight edge.
    if (!panorama && edge >= 1.0)
        return float4(darkGlass, 1.0);

    float tubeFov = radians(bus_romeo_nvg.z);
    // The G-buffer reconstruction constants carry the main camera's
    // horizontal and vertical projection tangents, including outside ADS.
    float2 viewTangent = abs(pos_decompression_params.xy);
    float2 radiusUv = 0.5 * tan(tubeFov * 0.5) / max(viewTangent, 0.001);
    if (digital)
        radiusUv.y *= 0.75;
    if (layout == RomeoNvgDigitalBezelLayout)
    {
        RomeoNvgAimView projection = RomeoNvgMakeAimView(layout, 1.0, radiusUv);
        centre = RomeoNvgAimSample(centre, projection);
        radiusUv *= projection.scale;
    }
    // OGF texture coordinates already run down from the top of the screen.
    float2 uv = saturate(centre + lensPosition * radiusUv);
    float2 effectCentre = centre;
    float2 effectPosition = lensPosition;
    float2 effectScale = 1.0 / radiusUv;
    float4 sensor = float4(centre, radiusUv);
    if (panorama)
    {
        // All four windows trace the same scene projection at a shared edge.
        const float cameraTangent = tan(radians(41.5));
        float baseScale = min(1.0, cameraTangent / max(viewTangent.y, 0.001));
        RomeoNvgAimView projection = RomeoNvgMakeAimView(layout, baseScale, 0.0);
        uv = RomeoNvgAimSample(pixel / screen_res.xy, projection);
        // Keep the optical effects continuous across the shared mesh edges.
        effectCentre = RomeoNvgAimSample(0.5, projection);
        effectScale = 2.0 / projection.scale;
        effectPosition = (uv - effectCentre) * effectScale;
        sensor = float4(RomeoNvgAimSample(centre, projection), min(screenRadius * projection.scale, 0.45));
    }
    else if (binoBezel)
    {
        // Common scene coordinates on both sides of the bezel. Meter each
        // tube independently at its projected centre.
        float baseScale = RomeoNvgBinoSceneScale(viewTangent.y, tubeFov);
        RomeoNvgAimView projection = RomeoNvgMakeAimView(layout, baseScale, 0.0);
        float2 radius = RomeoNvgBinoFitRadius * screen_res.y / screen_res.xy;
        uv = RomeoNvgAimSample(pixel / screen_res.xy, projection);
        bool right = pixel.x >= 0.5 * screen_res.x;
        float2 apertureCentre = 0.5 + RomeoNvgBinoOpticalCentre(right) * screen_res.y / screen_res.xy;
        effectCentre = RomeoNvgAimSample(apertureCentre, projection);
        effectScale = 1.0 / (radius * projection.scale);
        effectPosition = (uv - effectCentre) * effectScale;
        sensor = float4(effectCentre, radius * projection.scale);
    }
    uint frame = uint(timers.x * (digital ? 50.0 : 24.0));
    float3 image = RomeoNvgRenderTube(s_romeo_lens_scene, s_romeo_lens_position, s_romeo_lens_blur,
        smp_rtlinear, uv, effectCentre, effectPosition, effectScale, generation, frame, sensor);
    float2 sourceUv = RomeoNvgSampleUv(uv, effectCentre, effectPosition, effectScale, generation);
    float3 beamColor = lerp(RomeoNvgForGeneration(generation).phosphor, 1.0, 0.66);
    image = lerp(image, max(image, beamColor), RomeoNvgBeam(sourceUv));

    float coverage = panorama ? 1.0 : 1.0 - smoothstep(0.976, 1.0, edge);
    return float4(lerp(darkGlass, image, coverage), 1.0);
#else
    return float4(darkGlass, 1.0);
#endif
}

#endif
