#pragma once
#include <juce_opengl/juce_opengl.h>
#include "Core/AudioAnalysis.h"

namespace kone::grid
{
// Experimental foundation backend. Audio never takes the renderer's lock.
class Canvas final : public juce::Component, private juce::OpenGLRenderer
{
public:
    Canvas();
    ~Canvas() override;
    void present(AudioFrame frame, int mode, float sensitivity, float speed, float texture);
    void setImage(juce::Image image) { image_ = std::move(image); repaint(); }
    void setText(juce::String text) { text_ = std::move(text); repaint(); }
    void setTypeface(juce::Typeface::Ptr face) { face_ = std::move(face); repaint(); }
    void paint(juce::Graphics&) override;
private:
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    juce::OpenGLContext context_;
    std::unique_ptr<juce::OpenGLShaderProgram> shader_;
    unsigned int vertexBuffer_ = 0, vertexArray_ = 0;
    juce::CriticalSection frameLock_;
    AudioFrame frame_;
    int mode_ = 0;
    float sensitivity_ = 1, speed_ = 1, texture_ = 0;
    double epoch_ = 0;
    juce::String shaderError_;
    juce::Image image_;
    juce::String text_;
    juce::Typeface::Ptr face_;
    float overlayLevel_ = 0;
};
}
