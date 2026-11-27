#ifndef ROMEO_NVG_FIT_H
#define ROMEO_NVG_FIT_H

static const uint RomeoNvgBinoBezelLayout = 6u;
static const uint RomeoNvgQuadBezelLayout = 7u;
static const uint RomeoNvgDigitalBezelLayout = 8u;
static const uint RomeoNvgDigitalMonoLayout = 9u;
static const float RomeoNvgBinoFitScale = 1.39555662;
static const float RomeoNvgQuadFitScale = 1.07736719;
static const float RomeoNvgBinoFitRadius = 346.70326 / 1041.0;

float RomeoNvgNearFocus(float metres)
{
    if (metres <= 0.0)
        return 0.0;
    return 1.0 - smoothstep(0.08, 0.9144, metres);
}

float2 RomeoNvgBinoOpticalCentre(bool right)
{
    float direction = right ? 1.0 : -1.0;
    return float2(direction * (1.01 * RomeoNvgBinoFitRadius) + 0.05339 / 1041.0,
        0.69645 / 1041.0);
}

// Work in viewport-height units, with screen Y increasing downward. Taking
// homogeneous coordinates lets the vertex shader keep near-plane clipping.
float2 RomeoNvgFitDelta(float2 position, float depth, bool quad, bool right)
{
    float direction = right ? 1.0 : -1.0;
    float2 pivot;
    float2 shift = 0.0;
    float scale;
    if (quad)
    {
        pivot = float2(direction * 950.0 / 1041.0, 0.0);
        scale = RomeoNvgQuadFitScale;
    }
    else
    {
        pivot = float2(right ? 627.58628 : -627.47950,
            right ? -0.41532 : 1.80823) / 1041.0;
        shift = float2(direction * 0.20 * RomeoNvgBinoFitRadius,
            direction * 1.11179 / 1041.0);
        scale = RomeoNvgBinoFitScale;
    }
    return (scale - 1.0) * (position - pivot * depth) + shift * depth;
}

float RomeoNvgBinoSceneScale(float verticalTangent, float tubeFov)
{
    return min(1.0, tan(tubeFov * 0.5) / max(2.0 * RomeoNvgBinoFitRadius * verticalTangent, 0.001));
}

float2 RomeoNvgDigitalPivot(bool right)
{
    return float2(right ? 384.02051 : -383.91236,
        right ? -0.42074 : 1.83144) / 1041.0;
}

float2 RomeoNvgDigitalFitDelta(float2 position, float depth, bool right)
{
    float direction = right ? 1.0 : -1.0;
    float2 shift = float2(-direction * 66.29256, direction * 1.12609) / 1041.0;
    return (RomeoNvgBinoFitScale - 1.0) * (position - RomeoNvgDigitalPivot(right) * depth) + shift * depth;
}

float RomeoNvgDigitalEdge(float2 position)
{
    float2 screen = abs(position) / float2(0.84, 0.63);
    return max(length(position), max(screen.x, screen.y));
}

#endif
