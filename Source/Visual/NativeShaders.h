#pragma once
// Extracted shader expressions; only the GLSL 1.20 texture-coordinate interface
// is adapted for GRID-ARVx's OpenGL 3.2 fullscreen vertex shader.
namespace kone::grid::nativeShader {
inline constexpr const char* gaussianBlur=R"NATIVE(
        varying vec2 uv;
        uniform sampler2D tex;
        uniform vec2 dir;
        uniform float radius;
        void main() {
            
            // 9-tap Gaussian kernel (weights sum to ~1.0)
            vec4 color = texture2D(tex, uv) * 0.227027;
            color += texture2D(tex, uv + dir * 1.0 * radius) * 0.194946;
            color += texture2D(tex, uv - dir * 1.0 * radius) * 0.194946;
            color += texture2D(tex, uv + dir * 2.0 * radius) * 0.121622;
            color += texture2D(tex, uv - dir * 2.0 * radius) * 0.121622;
            color += texture2D(tex, uv + dir * 3.0 * radius) * 0.054054;
            color += texture2D(tex, uv - dir * 3.0 * radius) * 0.054054;
            color += texture2D(tex, uv + dir * 4.0 * radius) * 0.016216;
            color += texture2D(tex, uv - dir * 4.0 * radius) * 0.016216;
            gl_FragColor = color;
        }
    )NATIVE";
inline constexpr const char* ringGlow=R"NATIVE(
        varying vec2 uv;
        uniform sampler2D tex;
        uniform vec2 resolution;
        uniform float glowRadius;
        uniform float intensity;

        void main() {
            
            vec2 pixelSize = 1.0 / resolution;

            vec4 center = texture2D(tex, uv);
            vec3 result = center.rgb;

            // Sample in concentric rings around current pixel
            float totalGlow = 0.0;
            vec3 glowColor = vec3(0.0);

            int samples = 24;
            float maxDist = glowRadius;

            for (int ring = 1; ring <= 12; ring++) {
                float d = float(ring) / 12.0 * maxDist;
                float ringPixels = d;
                int ringCount = max(6, int(float(ring) * 4));

                for (int s = 0; s < ringCount; s++) {
                    float angle = float(s) / float(ringCount) * 6.2832;
                    vec2 offset = vec2(cos(angle), sin(angle)) * d * pixelSize;
                    vec4 samp = texture2D(tex, uv + offset);
                    float bright = dot(samp.rgb, vec3(0.299, 0.587, 0.114));

                    if (bright > 0.02) {
                        // Three-layer falloff matching oscilloscope shader
                        float coreFalloff = 0.6 / (d * d / (maxDist * 0.3) + 1.0);
                        float haloFalloff = 0.3 / (d * d / (maxDist * 2.0) + 1.0);
                        float bgFalloff   = 0.04 / (d / (maxDist * 4.0) + 1.0);
                        float glow = (coreFalloff + haloFalloff + bgFalloff) * bright;

                        glowColor += samp.rgb * glow;
                        totalGlow += glow;
                    }
                }
            }

            // Self-glow for center pixel
            float centerBright = dot(center.rgb, vec3(0.299, 0.587, 0.114));
            vec3 selfGlow = center.rgb * centerBright * 0.8;

            vec3 final_color = result + (glowColor * intensity) + (selfGlow * intensity);
            gl_FragColor = vec4(final_color, 1.0);
        }
    )NATIVE";
inline constexpr const char* lineGlow=R"NATIVE(
        
        uniform vec2 lineA;
        uniform vec2 lineB;
        uniform vec2 resolution;
        uniform vec2 viewportOrigin;
        uniform vec3 color;
        uniform float glowRadius;
        uniform float intensity;

        float distToSegment(vec2 p, vec2 a, vec2 b) {
            vec2 ab = b - a;
            float t = clamp(dot(p - a, ab) / dot(ab, ab), 0.0, 1.0);
            return length(p - (a + t * ab));
        }

        void main() {
            // Convert framebuffer-absolute gl_FragCoord to viewport-local
            // coords so this shader is invariant to glViewport origin.
            // In Multiple Mode each box has a non-(0,0) viewport origin;
            // in individual mode viewportOrigin is (0,0) and this is a no-op.
            vec2 fragCoord = gl_FragCoord.xy - viewportOrigin;
            fragCoord.y = resolution.y - fragCoord.y;

            float d = distToSegment(fragCoord, lineA, lineB);

            // Three-layer glow:
            // 1. Tight bright core (the visible line)
            float coreFalloff = intensity * 0.6 / (d * d / (glowRadius * 0.3) + 1.0);
            // 2. Medium halo (close glow around the line)
            float haloFalloff = intensity * 0.3 / (d * d / (glowRadius * 2.0) + 1.0);
            // 3. Wide background wash (lights up the dark areas far from the line)
            float bgFalloff   = intensity * 0.04 / (d / (glowRadius * 4.0) + 1.0);

            float glow = coreFalloff + haloFalloff + bgFalloff;

            // White-hot center — steep falloff so core stays thin
            vec3 coreColor = mix(color, vec3(1.0), exp(-d * 2.0));
            // Background wash uses pure palette color (no white blend)
            vec3 bgColor = color * bgFalloff;
            vec3 coreHaloColor = coreColor * (coreFalloff + haloFalloff);

            vec3 finalColor = coreHaloColor + bgColor;
            float totalAlpha = coreFalloff + haloFalloff + bgFalloff;

            gl_FragColor = vec4(finalColor, totalAlpha);
        }
    )NATIVE";
}