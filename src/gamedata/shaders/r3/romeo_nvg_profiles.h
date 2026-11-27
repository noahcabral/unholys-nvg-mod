#ifndef ROMEO_NVG_PROFILES_H
#define ROMEO_NVG_PROFILES_H

#ifndef ROMEO_NVG_DEPTH_TEXTURE
#define ROMEO_NVG_DEPTH_TEXTURE Texture2D<float>
#define ROMEO_NVG_READ_DEPTH(t, p) t.Load(int3(p, 0))
#endif
#ifndef ROMEO_NVG_HEAT_TEXTURE
#define ROMEO_NVG_HEAT_TEXTURE Texture2D<float4>
#define ROMEO_NVG_READ_HEAT(t, p) t.Load(int3(p, 0)).rgb
#define ROMEO_NVG_HEAT_SIZE(t, w, h) t.GetDimensions(w, h)
#endif
#ifndef ROMEO_NVG_LIGHT_TEXTURE
#define ROMEO_NVG_LIGHT_TEXTURE Texture2D<float>
#define ROMEO_NVG_SAMPLE_LIGHT(t, s, uv) t.SampleLevel(s, uv, 0)
#endif
#ifndef ROMEO_NVG_BRIGHTNESS
#define ROMEO_NVG_BRIGHTNESS 1.0
#endif
#ifndef ROMEO_NVG_DIGITAL_STREAK
#define ROMEO_NVG_DIGITAL_STREAK float2(0.0, 0.0)
#endif
#ifndef ROMEO_NVG_SHADOW_RETURN
#define ROMEO_NVG_SHADOW_RETURN(p, depth) 0.0
#endif
#ifndef ROMEO_NVG_NEAR_BLUR
#define ROMEO_NVG_NEAR_BLUR(profile, metres) ((metres) > 0.0 ? \
    (1.0 - smoothstep(0.0, (profile).nearBlurMetres, (metres))) * (profile).nearBlurPixels : 0.0)
#endif

struct RomeoNvgProfile
{
    float3 phosphor;
    float target;
    float maxGain;
    float blackLevel;
    float fisheye;
    float blurPixels;
    float nearBlurMetres;
    float nearBlurPixels;
    float grain;
    float haloGain;
};

static const float RomeoNvgAnalogPeakGrain = 0.42;

bool RomeoNvgIsDigital(uint generation)
{
    return generation == 4u || generation == 5u;
}

#ifndef ROMEO_NVG_ALBEDO
#define ROMEO_NVG_ALBEDO(pixel) float3(0.0, 0.0, 0.0)
#endif
// Strength of the estimated near-IR contribution to the color sensor.
#ifndef ROMEO_NVG_NIR_STRENGTH
#define ROMEO_NVG_NIR_STRENGTH 0.80
#endif
#ifndef ROMEO_NVG_ENVIRONMENT_LIGHT
#define ROMEO_NVG_ENVIRONMENT_LIGHT 0.03
#endif

float RomeoNvgColorSignal(float sceneLight)
{
    return smoothstep(0.00025, 0.0045, sceneLight);
}

float3 RomeoNvgDigitalColor(float3 image, float3 albedo, float meteredGain, float signal)
{
    const float3 weights = float3(0.2126, 0.7152, 0.0722);
    float3 visible = max(image, 0.0);
    float sceneLight = dot(visible, weights);
    float materialLight = dot(albedo, weights);
    float illumination = sceneLight / max(materialLight, 0.08);
    float localDarkness = 1.0 - smoothstep(0.045, 0.36, illumination);
    float tubeDarkness = smoothstep(1.4, 8.0, meteredGain);
    // This simulated sensor has no IR-cut filter. Its color cast persists
    // in sunlight and under lamps, with more visible RGB mixed in there.
    float infrared = lerp(0.88, 1.0, localDarkness * tubeDarkness)
        * saturate(ROMEO_NVG_NIR_STRENGTH);
    float3 chroma = (visible - sceneLight) / max(sceneLight, 0.012);

    float foliage = max(0.0, albedo.g - max(albedo.r, albedo.b));
    float longWave = max(0.0, dot(albedo, float3(0.65, 0.28, 0.07)) + foliage * 1.4);
    float response = 0.72 * sqrt(saturate(longWave))
        + 0.28 * smoothstep(0.002, 0.095, sceneLight);
    // The weather's interpolated ambient light drives a gradual hue change.
    // A local lamp can reveal material variation without switching the palette.
    float daylight = smoothstep(0.012, 0.052, ROMEO_NVG_ENVIRONMENT_LIGHT);
    float violet = smoothstep(0.08, 0.60, response) * lerp(0.28, 1.0, daylight);
    float magenta = smoothstep(0.48, 0.88, response + foliage * 0.45) * daylight * daylight;
    float3 cobalt = lerp(float3(0.34, 0.46, 0.97), float3(0.13, 0.32, 1.00), daylight);
    float3 lavender = lerp(float3(0.52, 0.43, 1.00), float3(0.78, 0.30, 1.00), daylight);
    float3 sensorColor = lerp(cobalt, lavender, violet);
    sensorColor = lerp(sensorColor, float3(1.00, 0.32, 0.86), magenta);
    float3 sensorChroma = sensorColor / dot(sensorColor, weights) - 1.0;
    chroma = lerp(chroma, sensorChroma, infrared) * RomeoNvgColorSignal(sceneLight);
    // Scale chroma into display gamut while retaining the metered brightness.
    float upper = max(0.0001, max(chroma.r, max(chroma.g, chroma.b)));
    float lower = max(0.0001, -min(chroma.r, min(chroma.g, chroma.b)));
    return signal + chroma * min(signal, min((1.0 - signal) / upper, signal / lower));
}

RomeoNvgProfile RomeoNvgForGeneration(uint generation)
{
    RomeoNvgProfile profile;
    if (generation == 1)
    {
        profile.phosphor = float3(0.10, 0.75, 0.10);
        profile.target = 0.42;
        profile.maxGain = 15.0;
        profile.blackLevel = 0.027;
        profile.fisheye = 0.30;
        profile.blurPixels = 3.4;
        profile.nearBlurMetres = 8.0;
        profile.nearBlurPixels = 3.5;
        profile.grain = 0.0;
        profile.haloGain = 4.0;
    }
    else if (generation == 3)
    {
        profile.phosphor = float3(0.35, 0.65, 1.00);
        profile.target = 0.68;
        profile.maxGain = 171.6;
        profile.blackLevel = 0.0;
        profile.fisheye = 0.0;
        profile.blurPixels = 0.0;
        profile.nearBlurMetres = 2.0;
        profile.nearBlurPixels = 1.6;
        profile.grain = RomeoNvgAnalogPeakGrain;
        profile.haloGain = 2.4;
    }
    else if (RomeoNvgIsDigital(generation))
    {
        profile.phosphor = float3(0.94, 0.94, 0.94);
        profile.target = 0.33;
        profile.maxGain = generation == 5u ? 24.0 : 30.0;
        profile.blackLevel = 0.015;
        profile.fisheye = 0.07;
        profile.blurPixels = 0.0;
        profile.nearBlurMetres = 0.0;
        profile.nearBlurPixels = 0.0;
        profile.grain = 0.80;
        profile.haloGain = 3.5;
    }
    else
    {
        profile.phosphor = float3(0.25, 0.85, 0.35);
        profile.target = 0.54;
        profile.maxGain = 114.4;
        profile.blackLevel = 0.006;
        profile.fisheye = 0.0;
        profile.blurPixels = 0.0;
        profile.nearBlurMetres = 3.0;
        profile.nearBlurPixels = 2.0;
        profile.grain = RomeoNvgAnalogPeakGrain;
        profile.haloGain = 3.2;
    }
    return profile;
}

static const float RomeoNvgCentreSpacing = 0.91;
static const float RomeoNvgHousingOuterEdge = 1.105;

// Centers and radii are in viewport-height units. The spacing leaves a shared
// width of 9% of a screen diameter, with no toe-in between the two views.
float2 RomeoNvgTubeCentre(uint tube, float radius)
{
    return float2((tube == 0 ? -1.0 : 1.0) * RomeoNvgCentreSpacing * radius, 0.0);
}

float RomeoNvgApertureRadius(float aspect)
{
    return min(0.44, (aspect * 0.5 - 0.015)
        / (RomeoNvgCentreSpacing + RomeoNvgHousingOuterEdge));
}

float2 RomeoNvgFisheyeUv(float2 viewUv, float2 opticalCentre, float2 lensPosition, float strength)
{
    // Preserve the lens center and rim. The middle of the image expands while
    // the outer portion compresses, as in equidistant fisheye projection.
    float radiusSquared = saturate(dot(lensPosition, lensPosition));
    return opticalCentre + (viewUv - opticalCentre) * lerp(1.0 - strength, 1.0, radiusSquared);
}

float RomeoNvgVisibleHeat(ROMEO_NVG_HEAT_TEXTURE heat, ROMEO_NVG_DEPTH_TEXTURE depth, int2 pixel)
{
    float3 signal = ROMEO_NVG_READ_HEAT(heat, pixel);
    float depthMetres = ROMEO_NVG_READ_DEPTH(depth, pixel);
    return (depthMetres > 0.0) * step(0.08, max(signal.r, max(signal.g, signal.b)));
}

float RomeoNvgNeighbourHeat(
    ROMEO_NVG_HEAT_TEXTURE heat, ROMEO_NVG_DEPTH_TEXTURE depth, int2 pixel, int2 lastPixel, int stride)
{
    float left = RomeoNvgVisibleHeat(heat, depth, max(pixel - int2(stride, 0), 0));
    float right = RomeoNvgVisibleHeat(heat, depth, min(pixel + int2(stride, 0), lastPixel));
    float above = RomeoNvgVisibleHeat(heat, depth, max(pixel - int2(0, stride), 0));
    float below = RomeoNvgVisibleHeat(heat, depth, min(pixel + int2(0, stride), lastPixel));
    return min(min(left, right), min(above, below));
}

float RomeoNvgThermalOutline(
    ROMEO_NVG_HEAT_TEXTURE heat, ROMEO_NVG_DEPTH_TEXTURE depth, int2 pixel, int2 lastPixel)
{
    float center = RomeoNvgVisibleHeat(heat, depth, pixel);
    if (center <= 0.0)
        return 0.0;

    // Only light the warm side of the boundary. Foreground cold pixels and
    // uniform hot interiors keep their normal phosphor image.
    int stroke = max(1, int(round(1.65 * (lastPixel.y + 1) / 1080.0)));
    float core = center - RomeoNvgNeighbourHeat(heat, depth, pixel, lastPixel, stroke);
    float feather = center - RomeoNvgNeighbourHeat(heat, depth, pixel, lastPixel, stroke + 1);
    return max(saturate(core) * 0.88, saturate(feather) * 0.20);
}

float RomeoNvgThermalRadius(float tubeFovRadians)
{
    // Convert the 75% angular aperture to a radius in rectilinear lens space.
    float halfFov = tubeFovRadians * 0.5;
    return tan(halfFov * 0.75) / tan(halfFov);
}

// Extract glare from linear HDR goggle color before bloom or phosphor.
// Render to ceil(viewSize / 4) and use a threshold above diffuse white.
float RomeoNvgExtractLights(Texture2D<float4> color, uint2 lightPixel, float threshold)
{
    uint width, height;
    color.GetDimensions(width, height);
    uint2 lastPixel = uint2(width, height) - 1u;
    float sum = 0.0;
    float peak = 0.0;
    threshold = max(threshold, 0.001);
    [unroll]
    for (uint y = 0; y < 4u; ++y)
    {
        [unroll]
        for (uint x = 0; x < 4u; ++x)
        {
            uint2 pixel = min(lightPixel * 4u + uint2(x, y), lastPixel);
            float3 rgb = color.Load(int3(pixel, 0)).rgb;
            float intensity = max(dot(rgb, float3(0.2126, 0.7152, 0.0722)),
                max(rgb.r, max(rgb.g, rgb.b)) * 0.35);
            float excess = max(intensity - threshold, 0.0) / threshold;
            float light = excess / (1.0 + excess);
            sum += light;
            peak = max(peak, light);
        }
    }
    // Keep subpixel sources visible after downsampling.
    return max(sum / 16.0, peak * 0.25);
}

// Run once per axis, alternating input and output at quarter resolution.
// axis is (1, 0) for the horizontal pass and (0, 1) for the vertical pass.
float RomeoNvgBlurLights(Texture2D<float> lights, SamplerState linearClamp, float2 uv, float2 axis)
{
    uint width, height;
    lights.GetDimensions(width, height);
    float2 stepUv = axis / float2(width, height) * (height / 270.0);
    float halo = lights.SampleLevel(linearClamp, uv, 0) * 0.227027;
    float2 nearOffset = stepUv * 1.384615;
    float2 farOffset = stepUv * 3.230769;
    halo += (lights.SampleLevel(linearClamp, uv + nearOffset, 0)
        + lights.SampleLevel(linearClamp, uv - nearOffset, 0)) * 0.316216;
    halo += (lights.SampleLevel(linearClamp, uv + farOffset, 0)
        + lights.SampleLevel(linearClamp, uv - farOffset, 0)) * 0.070270;
    return halo;
}

float RomeoNvgGhost(ROMEO_NVG_LIGHT_TEXTURE halo, SamplerState linearClamp,
    float2 uv, float2 opticalCentre, float2 lensUvScale, float scale)
{
    float2 sourceOffset = -(uv - opticalCentre) / scale;
    float2 sourceUv = opticalCentre + sourceOffset;
    float2 sourceLens = sourceOffset * lensUvScale;
    float sourceRadiusSquared = dot(sourceLens, sourceLens);
    if (any(sourceUv < 0.0) || any(sourceUv > 1.0) || sourceRadiusSquared >= 1.0)
        return 0.0;

    float lensFade = 1.0 - smoothstep(0.81, 1.0, sourceRadiusSquared);
    return ROMEO_NVG_SAMPLE_LIGHT(halo, linearClamp, sourceUv) * lensFade;
}

float RomeoNvgSceneExposure(Texture2D<float4> color, SamplerState linearClamp)
{
    float3 light = color.SampleLevel(linearClamp, float2(0.25, 0.38), 0).rgb
        + color.SampleLevel(linearClamp, float2(0.75, 0.38), 0).rgb
        + color.SampleLevel(linearClamp, float2(0.25, 0.62), 0).rgb
        + color.SampleLevel(linearClamp, float2(0.75, 0.62), 0).rgb;
    return smoothstep(0.024, 0.10, dot(max(light, 0.0), float3(0.2126, 0.7152, 0.0722)) * 0.25);
}

float RomeoNvgAnalogGain(Texture2D<float4> color, ROMEO_NVG_DEPTH_TEXTURE depth,
    SamplerState linearClamp, float4 sensor, float2 dimensions, float night, RomeoNvgProfile profile)
{
    float samples[9];
    [unroll]
    for (int y = 0; y < 3; ++y)
    {
        [unroll]
        for (int x = 0; x < 3; ++x)
        {
            float2 uv = saturate(sensor.xy + (float2(x, y) - 1.0) * sensor.zw * 0.48);
            float3 rgb = color.SampleLevel(linearClamp, uv, 0).rgb;
            float luminance = dot(max(rgb, 0.0), float3(0.2126, 0.7152, 0.0722));
            if (night > 0.01)
            {
                int2 pixel = min(int2(uv * dimensions), int2(dimensions) - 1);
                float metres = ROMEO_NVG_READ_DEPTH(depth, pixel);
                luminance = max(luminance, ROMEO_NVG_SHADOW_RETURN(pixel, metres) * night);
            }
            samples[y * 3 + x] = luminance;
        }
    }
    // Find the median. A bright light in part of one tube cannot dim that tube
    // or affect the exposure of its neighbours.
    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        [unroll]
        for (int j = i + 1; j < 9; ++j)
        {
            float low = min(samples[i], samples[j]);
            samples[j] = max(samples[i], samples[j]);
            samples[i] = low;
        }
    }
    float level = max(samples[4] - profile.blackLevel * (1.0 - night), 0.0);
    float targetDensity = -log(1.0 - profile.target) * ROMEO_NVG_BRIGHTNESS;
    float gain = level > 0.0 ? targetDensity / level : targetDensity;
    return profile.maxGain > 0.0 ? min(gain, profile.maxGain) : gain;
}

float RomeoNvgRegionalAnalogGain(Texture2D<float4> color, SamplerState linearClamp,
    float2 uv, float2 dimensions, float luminance, float shadowReturn, float daylight,
    float tubeGain, RomeoNvgProfile profile, uint generation)
{
    // Meter a soft region around each point so a bright wall does not set
    // the exposure of a dark doorway. Mix gains geometrically to keep local contrast.
    const float3 luminanceWeights = float3(0.2126, 0.7152, 0.0722);
    float2 offset = (24.0 * dimensions.y / 1080.0) / dimensions;
    float3 diagonal = color.SampleLevel(linearClamp, saturate(uv + offset), 0).rgb
        + color.SampleLevel(linearClamp, saturate(uv - offset), 0).rgb;
    offset.y = -offset.y;
    diagonal += color.SampleLevel(linearClamp, saturate(uv + offset), 0).rgb
        + color.SampleLevel(linearClamp, saturate(uv - offset), 0).rgb;
    float localLevel = max(0.58 * luminance + 0.105 * dot(diagonal, luminanceWeights), shadowReturn);
    localLevel -= min(profile.blackLevel * daylight, localLevel * 0.18);
    if (localLevel <= 0.0)
        return tubeGain;

    float localGain = (-log(1.0 - profile.target) * ROMEO_NVG_BRIGHTNESS) / localLevel;
    if (localGain <= tubeGain)
        return tubeGain;
    float localWeight = generation == 3u ? 0.52 : 0.44;
    float gain = tubeGain * pow(localGain / tubeGain, localWeight);
    return profile.maxGain > 0.0 ? min(gain, profile.maxGain) : gain;
}

float RomeoNvgHighlightSignal(float exposure, float knee)
{
    if (exposure <= knee)
        return 1.0 - exp(-exposure);

    // Match the ordinary response through the knee and keep contrast in
    // bright surfaces that would otherwise converge to the phosphor colour.
    return 1.0 - exp(-knee) / (1.0 + exposure - knee);
}

float RomeoNvgDigitalSignal(float luminance, float gain)
{
    return RomeoNvgHighlightSignal(max(luminance, 0.0) * gain, 0.8);
}

float RomeoNvgDigitalGain(Texture2D<float4> color, ROMEO_NVG_DEPTH_TEXTURE depth,
    SamplerState linearClamp, float2 centre, float2 lensUvScale, float2 dimensions, float night,
    RomeoNvgProfile profile)
{
    // Each sample represents one of 25 equal areas in the display.
    float luminance[32];
    [unroll]
    for (int y = 0; y < 5; ++y)
    {
        [unroll]
        for (int x = 0; x < 5; ++x)
        {
            float2 sensorOffset = (float2(x, y) + 0.5) * 0.4 - 1.0;
            float2 uv = saturate(centre + sensorOffset / lensUvScale);
            float3 rgb = color.SampleLevel(linearClamp, uv, 0).rgb;
            float luma = dot(max(rgb, 0.0), float3(0.2126, 0.7152, 0.0722));
            if (night > 0.01)
            {
                int2 pixel = min(int2(uv * dimensions), int2(dimensions) - 1);
                float metres = ROMEO_NVG_READ_DEPTH(depth, pixel);
                luma = max(luma, ROMEO_NVG_SHADOW_RETURN(pixel, metres) * night);
            }
            luminance[y * 5 + x] = luma;
        }
    }
    [unroll]
    for (int padding = 25; padding < 32; ++padding)
        luminance[padding] = 65504.0;

    // Sort the samples from dark to bright.
    [unroll]
    for (int span = 2; span <= 32; span *= 2)
    {
        [unroll]
        for (int stride = span / 2; stride > 0; stride /= 2)
        {
            [unroll]
            for (int sampleIndex = 0; sampleIndex < 32; ++sampleIndex)
            {
                int partner = sampleIndex ^ stride;
                if (partner > sampleIndex)
                {
                    float lower = min(luminance[sampleIndex], luminance[partner]);
                    float upper = max(luminance[sampleIndex], luminance[partner]);
                    bool ascending = (sampleIndex & span) == 0;
                    luminance[sampleIndex] = ascending ? lower : upper;
                    luminance[partner] = ascending ? upper : lower;
                }
            }
        }
    }
    // Select the most consistent brightness band that covers a majority.
    // Use the band to meter a bright wall across its whole lit surface.
    float bestContrast = 2.0;
    int majorityStart = 0;
    [unroll]
    for (int first = 0; first < 13; ++first)
    {
        float high = max(luminance[first + 12], 0.0005);
        float contrast = (high - max(luminance[first], 0.0005)) / high;
        if (contrast < bestContrast)
        {
            bestContrast = contrast;
            majorityStart = first;
        }
    }
    // Solve for the brightness after the sensor's contrast curve and clip.
    // At the upper bound even the darkest sample can reach the target if the
    // sensor has enough gain. At the lower bound the brightest sample is there.
    float daylight = smoothstep(0.022, 0.075, luminance[majorityStart + 6]);
    float exposureTarget = lerp(profile.target, 0.22, daylight);
    float targetDensity = -log(1.0 - exposureTarget);
    float upper = min(profile.maxGain, targetDensity / max(luminance[majorityStart], 0.0005));
    float lower = min(upper, targetDensity / max(luminance[majorityStart + 12], 0.0005));
    [loop]
    for (int iteration = 0; iteration < 8; ++iteration)
    {
        float trial = (lower + upper) * 0.5;
        float displayed = 0.0;
        [loop]
        for (int index = 0; index < 13; ++index)
            displayed += RomeoNvgDigitalSignal(luminance[majorityStart + index], trial);
        if (displayed < exposureTarget * 13.0)
            lower = trial;
        else
            upper = trial;
    }
    return (lower + upper) * 0.5;
}

float2 RomeoNvgSampleUv(float2 viewUv, float2 opticalCentre, float2 lensPosition,
    float2 lensUvScale, uint generation)
{
    RomeoNvgProfile profile = RomeoNvgForGeneration(generation);
    float2 uv = saturate(RomeoNvgFisheyeUv(viewUv, opticalCentre, lensPosition, profile.fisheye));
    if (RomeoNvgIsDigital(generation))
    {
        float2 sensorSize = float2(800, 600) * 0.5 * lensUvScale;
        uv = saturate(opticalCentre + (floor((uv - opticalCentre) * sensorSize) + 0.5) / sensorSize);
    }
    return uv;
}

float3 RomeoNvgRenderTube(
    Texture2D<float4> color, ROMEO_NVG_DEPTH_TEXTURE depth, ROMEO_NVG_LIGHT_TEXTURE halo,
    SamplerState linearClamp, float2 viewUv, float2 opticalCentre, float2 lensPosition,
    float2 lensUvScale, uint generation, uint frameIndex, float4 sensor)
{
    bool digital = RomeoNvgIsDigital(generation);
    RomeoNvgProfile profile = RomeoNvgForGeneration(generation);
    float2 uv = RomeoNvgSampleUv(viewUv, opticalCentre, lensPosition, lensUvScale, generation);
    uint width, height;
    color.GetDimensions(width, height);
    int2 lastPixel = int2(width, height) - 1;
    float2 dimensions = float2(width, height);
    int2 pixel = min(int2(uv * dimensions), lastPixel);

    float depthMetres = ROMEO_NVG_READ_DEPTH(depth, pixel);
    float3 image = color.SampleLevel(linearClamp, uv, 0).rgb;
    if (!digital)
    {
        float nearBlur = ROMEO_NVG_NEAR_BLUR(profile, depthMetres);
        float blur = (profile.blurPixels + nearBlur) * (height / 1080.0);
        if (blur > 0.05)
        {
            float2 stepUv = blur / dimensions;
            image = (image * 4.0
                + color.SampleLevel(linearClamp, uv + float2(stepUv.x, 0), 0).rgb
                + color.SampleLevel(linearClamp, uv - float2(stepUv.x, 0), 0).rgb
                + color.SampleLevel(linearClamp, uv + float2(0, stepUv.y), 0).rgb
                + color.SampleLevel(linearClamp, uv - float2(0, stepUv.y), 0).rgb) / 8.0;
        }
#ifdef ROMEO_NVG_DEFOCUS
        image = ROMEO_NVG_DEFOCUS(color, linearClamp, uv, depthMetres, dimensions, image);
#endif
    }
    else
    {
        float2 streak = ROMEO_NVG_DIGITAL_STREAK;
        if (dot(streak, streak) > 0.000001)
        {
            image = (image * 4.0
                + color.SampleLevel(linearClamp, saturate(uv + streak * 0.33), 0).rgb * 2.0
                + color.SampleLevel(linearClamp, saturate(uv + streak * 0.66), 0).rgb
                + color.SampleLevel(linearClamp, saturate(uv + streak), 0).rgb) / 8.0;
        }
    }

    float luminance = dot(max(image, 0.0), float3(0.2126, 0.7152, 0.0722));
    float sceneLight = luminance;
    float daylight = digital ? 0.0 : RomeoNvgSceneExposure(color, linearClamp);
    float night = 1.0 - daylight;
    float shadowReturn = night > 0.01 ? ROMEO_NVG_SHADOW_RETURN(pixel, depthMetres) * night : 0.0;
    luminance = max(luminance, shadowReturn);
    float gain;
    float signal;
    if (digital)
    {
        gain = RomeoNvgDigitalGain(color, depth, linearClamp, opticalCentre, lensUvScale, dimensions, night, profile);
        signal = RomeoNvgDigitalSignal(luminance, gain);
        if (generation == 5u)
            signal *= lerp(0.42, 1.0, RomeoNvgColorSignal(sceneLight));
    }
    else
    {
        bool regional = generation == 2u || generation == 3u;
        gain = RomeoNvgAnalogGain(color, depth, linearClamp, sensor, dimensions, night, profile);
        if (regional)
            gain = RomeoNvgRegionalAnalogGain(color, linearClamp, uv, dimensions, luminance,
                shadowReturn, daylight, gain, profile, generation);
        float black = profile.blackLevel * daylight;
        if (regional)
            black = min(black, luminance * 0.18);
        float exposure = max(luminance - black, 0.0) * gain;
        // The Gen 3 knee begins above its metered target, keeping the
        // low-light response while preserving detail on brighter subjects.
        signal = generation == 3u
            ? RomeoNvgHighlightSignal(exposure, 1.20) : 1.0 - exp(-exposure);
    }
    if (profile.grain > 0.0)
    {
        int2 grainPixel = pixel;
        float gainFraction = saturate(gain / profile.maxGain);
        float response = gainFraction * gainFraction;
        float grainFloor = 0.045;
        if (generation == 3u)
        {
            // Same peak as Gen 2, with less scintillation at intermediate gains.
            response *= response;
            grainFloor = 0.018;
        }
        float grainStrength = profile.grain * lerp(grainFloor, 1.0, response);
        if (generation == 2u || generation == 3u)
        {
            // Use the same phosphor grain size for both analog generations.
            float grainSize = max(1.0, round(height / 550.0));
            grainPixel = int2(float2(pixel) / grainSize);
        }
        uint noise = uint(grainPixel.x) * 1664525u + uint(grainPixel.y) * 1013904223u + frameIndex * 374761393u;
        noise = (noise ^ (noise >> 16)) * 2246822519u;
        noise ^= noise >> 13;
        float grain = float(noise & 65535u) / 65535.0 - 0.5;
        signal = saturate(signal + grain * grainStrength);
    }

    float haloSignal = ROMEO_NVG_SAMPLE_LIGHT(halo, linearClamp, uv);
    float ghosts = 0.0;
#ifndef ROMEO_NVG_LOCAL_HALO_ONLY
    // A digital sensor retains local glare. Reflected copies of the scene
    // create circular overlays even when the bright source is a wall or sky.
    if (!digital)
    {
        float ghostFade = smoothstep(0.025, 0.16, length(lensPosition));
        ghosts = (RomeoNvgGhost(halo, linearClamp, uv, opticalCentre, lensUvScale, 0.35) * 0.12
            + RomeoNvgGhost(halo, linearClamp, uv, opticalCentre, lensUvScale, 0.65) * 0.05) * ghostFade;
    }
#endif
    float glare = 1.0 - exp(-(haloSignal + ghosts) * profile.haloGain);
    float3 phosphor = signal * profile.phosphor;
    if (generation == 5u)
    {
        float3 albedo = depthMetres > 0.0 ? ROMEO_NVG_ALBEDO(pixel) : 0.0;
        phosphor = RomeoNvgDigitalColor(image, albedo, gain, signal);
    }
    float3 amplified = phosphor + (1.0 - phosphor) * glare * lerp(profile.phosphor, 1.0, glare * 0.6);
    if (generation == 1)
    {
        float overload = max(daylight, smoothstep(0.15, 0.65, luminance)) * 0.97;
        amplified = lerp(amplified, float3(0.78, 1.0, 0.69), overload);
    }
    return amplified;
}

float3 RomeoNvgHousing(
    Texture2D<float4> mainView, SamplerState linearClamp, float3 background, float2 uv,
    float2 nearLens, float2 otherLens, float screenAspect)
{
    float distance = length(nearLens);
    // Round the case joint where the two curved rims meet.
    float joint = saturate(1.0 - abs(distance - length(otherLens)) / 0.10);
    float hullDistance = distance - joint * joint * 0.012;
    float housing = 1.0 - smoothstep(1.068, RomeoNvgHousingOuterEdge, hullDistance);
    if (housing <= 0.0)
        return background;

    // The display sits below a wider front opening. The inner wall darkens
    // toward the display and the front edge catches a little overhead light.
    float front = smoothstep(1.012, 1.060, distance);
    float upper = saturate(0.5 - nearLens.y * 0.44);
    float bevelDistance = (distance - 1.045) / 0.028;
    float bevel = exp(-bevelDistance * bevelDistance);
    float3 rim = lerp(float3(0.014, 0.019, 0.021), float3(0.043, 0.048, 0.052), front);
    rim += bevel * (0.011 + 0.040 * upper);

    float2 offset = (3.0 / 1080.0) * float2(1.0 / screenAspect, 1.0);
    float3 throughHousing = (background * 4.0
        + mainView.SampleLevel(linearClamp, uv + float2(offset.x, 0.0), 0).rgb
        + mainView.SampleLevel(linearClamp, uv - float2(offset.x, 0.0), 0).rgb
        + mainView.SampleLevel(linearClamp, uv + float2(0.0, offset.y), 0).rgb
        + mainView.SampleLevel(linearClamp, uv - float2(0.0, offset.y), 0).rgb) / 8.0;
    float opacity = lerp(0.37, 0.55, front);
    return lerp(background, lerp(throughHousing, rim, opacity), housing);
}

// Composite the independent goggle view over the main camera. The two lenses
// share the view but have their own optical centers for distortion.
// thermalTube is the index of the pod with the ECOTI fitted. Its FOV is in radians.
// Color, depth and heat must match in size, camera projection and frame.
// Depth is linear metres, and heat must be depth-tested in that view. Pass the
// blurred quarter-resolution light map from this same view as viewHalo.
float3 RomeoNvgComposite(
    Texture2D<float4> mainView, float2 screenUv, float screenAspect,
    Texture2D<float4> viewColor, ROMEO_NVG_DEPTH_TEXTURE viewDepth, ROMEO_NVG_HEAT_TEXTURE viewHeat,
    ROMEO_NVG_LIGHT_TEXTURE viewHalo,
    SamplerState linearClamp, uint generation, uint frameIndex, uint thermalTube, float tubeFovRadians)
{
    float radius = RomeoNvgApertureRadius(screenAspect);
    float2 aspectScale = float2(screenAspect, 1.0);
    float2 screenPosition = (screenUv - 0.5) * aspectScale;
    float2 leftCentre = RomeoNvgTubeCentre(0, radius);
    float2 rightCentre = RomeoNvgTubeCentre(1, radius);
    float2 left = (screenPosition - leftCentre) / radius;
    float2 right = (screenPosition - rightCentre) / radius;
    float leftDistance = length(left);
    float rightDistance = length(right);
    float leftCoverage = 1.0 - smoothstep(0.993, 1.0, leftDistance);
    float rightCoverage = 1.0 - smoothstep(0.993, 1.0, rightDistance);
    float coverage = max(leftCoverage, rightCoverage);

    float3 background = mainView.SampleLevel(linearClamp, screenUv, 0).rgb;
    if (coverage < 1.0)
    {
        bool closerToLeft = leftDistance <= rightDistance;
        background = RomeoNvgHousing(mainView, linearClamp, background, screenUv,
            closerToLeft ? left : right, closerToLeft ? right : left, screenAspect);
    }
    if (coverage <= 0.0)
        return background;

    float rightBias = smoothstep(-radius * 0.10, radius * 0.10, screenPosition.x);
    float leftWeight = leftCoverage * lerp(1.0, 0.02, rightBias);
    float rightWeight = rightCoverage * lerp(0.02, 1.0, rightBias);
    float weight = rightWeight / (leftWeight + rightWeight);
    uint tube = weight < 0.5 ? 0u : 1u;
    float2 centre = RomeoNvgTubeCentre(tube, radius);
    float2 lensUvScale = aspectScale / radius;
    float2 opticalUv = 0.5 + centre / aspectScale;
    float3 image = RomeoNvgRenderTube(viewColor, viewDepth, viewHalo, linearClamp,
        screenUv, opticalUv, (screenPosition - centre) / radius,
        lensUvScale, generation, frameIndex, float4(opticalUv, 1.0 / lensUvScale));

    // Blend the two screens only where both contribute, keeping the recessed
    // lip outside their union and the brightness consistent through the join.
    if (weight > 0.0 && weight < 1.0)
    {
        float2 otherCentre = RomeoNvgTubeCentre(1u - tube, radius);
        float2 otherUv = 0.5 + otherCentre / aspectScale;
        float3 otherImage = RomeoNvgRenderTube(viewColor, viewDepth, viewHalo, linearClamp,
            screenUv, otherUv, (screenPosition - otherCentre) / radius,
            lensUvScale, generation, frameIndex, float4(otherUv, 1.0 / lensUvScale));
        image = lerp(image, otherImage, tube == 0 ? weight : 1.0 - weight);
    }

    if (generation == 3 && thermalTube <= 1u)
    {
        float thermalRadius = RomeoNvgThermalRadius(tubeFovRadians);
        float thermalDistance = length(screenPosition - RomeoNvgTubeCentre(thermalTube, radius)) / radius;
        if (thermalDistance < thermalRadius)
        {
            uint width, height;
            ROMEO_NVG_HEAT_SIZE(viewHeat, width, height);
            int2 lastPixel = int2(width, height) - 1;
            int2 pixel = min(int2(screenUv * float2(width, height)), lastPixel);
            float edge = RomeoNvgThermalOutline(viewHeat, viewDepth, pixel, lastPixel);
            float thermalCoverage = 1.0 - smoothstep(thermalRadius * 0.985, thermalRadius, thermalDistance);
            image = lerp(image, float3(0.72, 1.0, 0.94), thermalCoverage * edge);
        }
    }
    return lerp(background, image, coverage);
}

#endif
