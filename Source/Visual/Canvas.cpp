#include "Canvas.h"

namespace kone::grid
{
using namespace juce::gl;
static const char* vertexSource = R"glsl(
attribute vec2 position;
varying vec2 uv;
void main() { uv = position * 0.5 + 0.5; gl_Position = vec4(position, 0.0, 1.0); }
)glsl";
static const char* fragmentSource = R"glsl(
varying vec2 uv;
uniform float waveform[256];
uniform float time, level, bass, sensitivity, motion, textureAmount;
uniform int mode;
float noise(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main()
{
    float amp = clamp(level * sensitivity * 3.0, 0.0, 1.0);
    vec3 cyan = vec3(0.15, 0.90, 0.85), violet = vec3(0.55, 0.30, 0.95);
    vec3 colour = vec3(0.018, 0.028, 0.048);
    if (mode == 0)
    {
        int index = int(clamp(uv.x * 255.0, 0.0, 255.0));
        float y = 0.5 + waveform[index] * sensitivity * 0.38;
        float line = exp(-abs(uv.y - y) * 230.0);
        float grid = step(0.985, fract(uv.x * 12.0)) + step(0.985, fract(uv.y * 8.0));
        colour += grid * 0.045 + mix(cyan, violet, uv.x) * line * (0.65 + amp);
    }
    else if (mode == 1)
        colour = mix(violet, cyan, 0.5 + 0.5 * sin(time * motion * 0.3 + bass * 8.0)) * (0.1 + amp);
    else
    {
        vec2 p = (uv - 0.5) * 8.0;
        float t = time * motion;
        float waves = sin(p.x + t) + sin(p.y * 1.3 - t * 0.7) + sin(length(p) * 2.0 - t + bass * 8.0);
        colour = mix(violet, cyan, 0.5 + 0.5 * sin(waves)) * (0.18 + amp * 0.82);
    }
    colour += (noise(uv + fract(time)) - 0.5) * textureAmount * 0.18;
    gl_FragColor = vec4(clamp(colour, 0.0, 1.0), 1.0);
}
)glsl";
Canvas::Canvas()
{
    context_.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    context_.setRenderer(this);
    context_.setContinuousRepainting(true);
    context_.setComponentPaintingEnabled(true);
    context_.attachTo(*this);
}
Canvas::~Canvas() { context_.detach(); }
void Canvas::present(AudioFrame frame, int mode, float sensitivity, float speed, float texture)
{
    overlayLevel_ = std::clamp(frame.rms * sensitivity * 3.0f, 0.0f, 1.0f);
    { const juce::ScopedLock lock(frameLock_); frame_ = frame; mode_ = mode;
      sensitivity_ = sensitivity; speed_ = speed; texture_ = texture; }
    repaint();
}
void Canvas::newOpenGLContextCreated()
{
    epoch_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
    shader_ = std::make_unique<juce::OpenGLShaderProgram>(context_);
    if (!(shader_->addVertexShader(juce::OpenGLHelpers::translateVertexShaderToV3(vertexSource))
        && shader_->addFragmentShader(juce::OpenGLHelpers::translateFragmentShaderToV3(fragmentSource)) && shader_->link()))
    {
        const juce::ScopedLock lock(frameLock_);
        shaderError_ = shader_->getLastError(); shader_.reset(); return;
    }
    const float vertices[] = {-1,-1, 1,-1, -1,1, 1,1};
    glGenVertexArrays(1, &vertexArray_); glBindVertexArray(vertexArray_);
    glGenBuffers(1, &vertexBuffer_); glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    { const juce::ScopedLock lock(frameLock_); shaderError_.clear(); }
}
void Canvas::renderOpenGL()
{
    const auto scale = context_.getRenderingScale();
    glViewport(0, 0, juce::roundToInt(getWidth() * scale), juce::roundToInt(getHeight() * scale));
    juce::OpenGLHelpers::clear(juce::Colour(0xff080d18));
    if (!shader_) return;
    AudioFrame frame; int mode; float sensitivity, speed, texture;
    { const juce::ScopedLock lock(frameLock_); frame = frame_; mode = mode_;
      sensitivity = sensitivity_; speed = speed_; texture = texture_; }
    shader_->use();
    const auto program = shader_->getProgramID();
    auto uniform = [program](const char* name) { return glGetUniformLocation(program, name); };
    glUniform1fv(uniform("waveform"), 256, frame.waveform.data());
    glUniform1f(uniform("time"), static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.001 - epoch_));
    glUniform1f(uniform("level"), frame.rms); glUniform1f(uniform("bass"), frame.bass);
    glUniform1f(uniform("sensitivity"), sensitivity); glUniform1f(uniform("motion"), speed);
    glUniform1f(uniform("textureAmount"), texture); glUniform1i(uniform("mode"), mode);
    glBindVertexArray(vertexArray_); glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    const auto attribute = static_cast<unsigned int>(glGetAttribLocation(program, "position"));
    glEnableVertexAttribArray(attribute); glVertexAttribPointer(attribute, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); glDisableVertexAttribArray(attribute);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
}
void Canvas::openGLContextClosing()
{
    shader_.reset();
    if (vertexBuffer_) glDeleteBuffers(1, &vertexBuffer_);
    if (vertexArray_) glDeleteVertexArrays(1, &vertexArray_);
    vertexBuffer_ = vertexArray_ = 0;
}
void Canvas::paint(juce::Graphics& graphics)
{
    if (image_.isValid())
    {
        graphics.setOpacity(0.2f + overlayLevel_ * 0.65f);
        const float inset = 30.0f + (1.0f - overlayLevel_) * 35.0f;
        graphics.drawImageWithin(image_, static_cast<int>(inset), static_cast<int>(inset),
            std::max(1, getWidth() - static_cast<int>(inset * 2)), std::max(1, getHeight() - static_cast<int>(inset * 2)), juce::RectanglePlacement::centred);
    }
    graphics.setOpacity(1.0f); graphics.setColour(juce::Colours::white);
    const float size = 24.0f + overlayLevel_ * 14.0f;
    graphics.setFont(face_ ? juce::Font(juce::FontOptions(face_).withHeight(size)) : juce::Font(juce::FontOptions(size)));
    graphics.drawFittedText(text_, getLocalBounds().reduced(32), juce::Justification::centredBottom, 4);
    juce::String error; { const juce::ScopedLock lock(frameLock_); error = shaderError_; }
    if (error.isNotEmpty())
    { graphics.setFont(juce::FontOptions(14.0f)); graphics.drawFittedText("Renderer error: " + error, getLocalBounds().reduced(24), juce::Justification::centredTop, 5); }
}
}
