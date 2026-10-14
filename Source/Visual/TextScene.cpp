#include "Scenes.h"
namespace kone::grid {
static float textNoise(int n) {
  return (float)((unsigned)(n * 1664525u + 1013904223u) % 65536u) / 65535.0f;
}
static juce::Path shapeText(const juce::Path &source, float size, int style,
                            float time) {
  juce::Path output;
  output.setUsingNonZeroWinding(source.isUsingNonZeroWinding());
  auto bounds = source.getBounds();
  auto point = [&](float x, float y) {
    float u = (x - bounds.getX()) / juce::jmax(1.0f, size),
          v = (y - bounds.getY()) / juce::jmax(1.0f, size), dx = 0, dy = 0;
    if (style == 0) {
      dx = .008f * std::sin(v * 15 + u * 7);
      dy = .012f * std::sin(u * 19);
    }
    if (style == 2) {
      dx = .010f * std::sin(u * 17 + v * 11);
      dy = .010f * std::cos(u * 21);
    }
    if (style == 5) {
      dx = .060f * std::sin(v * 13 + u * 4 + time * 2);
      dy = .065f * std::sin(u * 12 - time * 1.5f);
    }
    if (style == 9) {
      dx = .020f * std::sin(u * 17 + v * 9);
      dy = .035f * std::sin(u * 19) + .015f * std::cos(v * 13);
    }
    return juce::Point<float>(x + dx * size, y + dy * size);
  };
  juce::Path::Iterator item(source);
  while (item.next()) {
    auto a = point(item.x1, item.y1), b = point(item.x2, item.y2),
         c = point(item.x3, item.y3);
    switch (item.elementType) {
    case juce::Path::Iterator::startNewSubPath:
      output.startNewSubPath(a);
      break;
    case juce::Path::Iterator::lineTo:
      output.lineTo(a);
      break;
    case juce::Path::Iterator::quadraticTo:
      output.quadraticTo(a, b);
      break;
    case juce::Path::Iterator::cubicTo:
      output.cubicTo(a, b, c);
      break;
    case juce::Path::Iterator::closePath:
      output.closeSubPath();
      break;
    }
  }
  return output;
}
juce::Image drawTextScene(const SceneState &s, const Assets &assets) {
  juce::Image image(juce::Image::ARGB, s.width, s.height, true,
                    juce::SoftwareImageType());
  juce::Graphics g(image);
  const int rawVariant = s.variant(), v = rawVariant == 1 ? 8 : rawVariant;
  const float w = (float)s.width, h = (float)s.height, a = s.amplitude(),
              t = (float)s.time * s.get("speed") / 64.0f;
  g.fillAll(v == 5 && !assets.image.isValid() ? juce::Colours::transparentBlack
                                              : s.colour("color", true));
  if (assets.image.isValid()) {
    float factor = 0.15f + s.get("imageScale") / 64.0f;
    auto bounds = juce::Rectangle<float>(
        w * (1 - factor) / 2, h * (1 - factor) / 2, w * factor, h * factor);
    g.drawImage(assets.image, bounds, juce::RectanglePlacement::centred);
  }
  // These backgrounds belong to Text styles, independently of other modes.
  if (!assets.image.isValid() && v == 11) {
    g.fillAll(juce::Colours::black);
    auto tint = s.colour("color");
    for (int y = 0; y < (int)h; y += 12)
      for (int x = 0; x < (int)w; x += 12) {
        if (textNoise(x * 13 + y * 47) < .48f)
          continue;
        g.setColour(tint.withAlpha(.35f + .6f * textNoise(x + y * 3)));
        g.drawLine((float)x - 2, (float)y, (float)x + 2, (float)y);
        g.drawLine((float)x, (float)y - 2, (float)x, (float)y + 2);
      }
  }
  if (!assets.image.isValid() && v == 6) {
    g.fillAll(juce::Colours::black);
    for (int i = 0; i < 70; ++i) {
      g.setColour(palette((int)(textNoise(i * 13) * 126)).withAlpha(.25f));
      g.fillRect(textNoise(i * 23) * w, textNoise(i * 47) * h, 1.0f, 1.0f);
    }
  }
  auto line =
      assets.lyrics.isEmpty()
          ? juce::String()
          : assets.lyrics[decodeChoice(s.get("lyric"), assets.lyrics.size())];
  if (v == 3) {
    g.setColour(juce::Colours::white.withAlpha(.13f));
    for (int i = 0; i < 900; ++i)
      g.fillRect(textNoise(i * 17 + (int)(t * 8)) * w,
                 textNoise(i * 31 + (int)(t * 8)) * h, 2.0f, 1.0f);
    for (int i = 0; i < 8; ++i)
      g.drawHorizontalLine((int)(std::fmod(i * h / 8 + t * 3, h)), 0, w);
    g.setFont(juce::FontOptions("Courier New", 12, juce::Font::bold));
    g.drawText("PLAY >  SP", 20, 90, 150, 40, juce::Justification::topLeft);
    g.drawText("00:" + juce::String((int)s.time / 60).paddedLeft('0', 2) + ":" +
                   juce::String((int)s.time % 60).paddedLeft('0', 2),
               (int)w - 180, (int)h - 180, 160, 30,
               juce::Justification::centredRight);
  }
  juce::Rectangle<int> textArea(0, 0, (int)w, (int)h);
  if (v == 4) {
    const float uiScale = juce::jmin(w / 960.0f, h / 540.0f);
    auto window = juce::Rectangle<float>(w * .25f, h * .40f, w * .5f, h * .22f);
    g.setColour(juce::Colour(0xffc5c5c5));
    g.fillRect(window);
    g.setColour(juce::Colour(0xff293c85));
    g.fillRect(window.withHeight(22 * uiScale));
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12 * uiScale));
    g.drawText("System message",
               window.withHeight(22 * uiScale).reduced(5 * uiScale, 0),
               juce::Justification::centredLeft);
    g.setColour(juce::Colours::red);
    g.fillEllipse(window.getX() + 15 * uiScale, window.getY() + 45 * uiScale,
                  24 * uiScale, 24 * uiScale);
    g.setColour(juce::Colours::white);
    g.drawLine(window.getX() + 20 * uiScale, window.getY() + 50 * uiScale,
               window.getX() + 34 * uiScale, window.getY() + 64 * uiScale,
               2 * uiScale);
    g.drawLine(window.getX() + 34 * uiScale, window.getY() + 50 * uiScale,
               window.getX() + 20 * uiScale, window.getY() + 64 * uiScale,
               2 * uiScale);
    textArea = window.toNearestInt()
                   .withTrimmedLeft(juce::roundToInt(55 * uiScale))
                   .withTrimmedTop(juce::roundToInt(25 * uiScale))
                   .withTrimmedBottom(juce::roundToInt(24 * uiScale));
  }
  float size = h * (v == 4 ? .045f : v == 11 ? .28f : .32f);
  const auto styleFace = assets.styleFonts[(size_t)v];
  const bool referenceFace = !assets.font && styleFace;
  auto face = assets.font ? assets.font : styleFace;
  const char *family = v == 11             ? "Times New Roman"
                       : v == 9 || v == 10 ? "Georgia"
                       : v == 2 || v == 5  ? "Arial Black"
                       : v == 3 || v == 6  ? "Lucida Console"
                       : v == 0            ? "Arial Rounded MT Bold"
                       : v == 8            ? "Segoe Print"
                                           : "Arial";
  auto font = face ? juce::Font(juce::FontOptions(face).withHeight(size))
                   : juce::Font(juce::FontOptions(
                         family, size,
                         v == 8    ? juce::Font::plain
                         : v == 9  ? juce::Font::bold | juce::Font::italic
                         : v == 11 ? juce::Font::italic
                                   : juce::Font::bold));
  if (v == 0)
    font.setHorizontalScale(.58f);
  size *= juce::jmin(
      1.0f,
      w * 0.8f /
          juce::jmax(1.0f, juce::GlyphArrangement::getStringWidth(font, line)));
  font.setHeight(size);
  g.setFont(font);
  auto tc = s.colour("textColor");
  int fx = decodeChoice(s.get("textEffect"), 7);
  juce::AffineTransform tr = juce::AffineTransform::translation(-w / 2, -h / 2);
  if (fx == 0)
    tr = tr.sheared(std::sin(t * 3) * a * 0.2f, 0);
  if (fx == 1)
    tr = tr.scaled(1 + a * 0.35f);
  if (fx == 2)
    tr = tr.rotated(t * 0.5f);
  if (fx == 4)
    tr = tr.translated(0, std::sin(t * 5) * a * 30);
  if (fx == 5)
    tr = tr.translated(std::sin(t * 43) * a * 15, std::cos(t * 37) * a * 15);
  tr = tr.translated(w / 2, h / 2);
  g.addTransform(tr);
  if (fx == 6 && std::sin(t * 12) < 0)
    tc = tc.withAlpha(0.1f);
  g.setColour(tc);
  auto drawLetters = [&](const juce::String &letters,
                         juce::Rectangle<float> area) {
    if (referenceFace && v != 4) {
      // Font outlines already contain the recorded ornamental, cutout or pixel
      // treatment. Do not add a second simulated outline over those glyphs.
      g.setColour(tc);
      g.drawText(letters, area, juce::Justification::centred);
      return;
    }
    if (v == 6 || v == 3) {
      const float pixelSize = v == 6 ? 8.0f : 3.0f;
      auto small = juce::Image(
          juce::Image::ARGB, juce::jmax(1, (int)(area.getWidth() / pixelSize)),
          juce::jmax(1, (int)(area.getHeight() / pixelSize)), true,
          juce::SoftwareImageType());
      {
        juce::Graphics pixels(small);
        pixels.setColour(tc);
        pixels.setFont(font.withHeight(font.getHeight() / pixelSize));
        pixels.drawText(letters, small.getBounds(),
                        juce::Justification::centred);
      }
      g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
      g.drawImage(small, area);
      return;
    }
    if (v == 7) {
      float total = juce::GlyphArrangement::getStringWidth(font, letters),
            x = area.getCentreX() - total / 2;
      for (int i = 0; i < letters.length(); ++i) {
        auto ch = letters.substring(i, i + 1);
        float width = juce::GlyphArrangement::getStringWidth(font, ch);
        juce::Graphics::ScopedSaveState saved(g);
        auto card = juce::Rectangle<float>(x, area.getCentreY() - size * .5f,
                                           width + 4, size);
        g.addTransform(juce::AffineTransform::rotation(
            (textNoise(i * 37) - .5f) * .3f, card.getCentreX(),
            card.getCentreY()));
        g.setColour(i % 3 == 0 ? s.colour("color")
                               : juce::Colour(0xffe0e0da));
        g.fillRect(card);
        g.setColour(i % 3 == 0
                        ? tc
                        : juce::Colours::black.withAlpha(tc.getFloatAlpha()));
        auto cutout = face
                          ? font
                          : juce::Font(juce::FontOptions(
                                i % 2 ? "Georgia" : "Arial", size,
                                i % 2 ? juce::Font::bold : juce::Font::italic));
        juce::GlyphArrangement letter;
        letter.addFittedText(cutout, ch, card.getX(), card.getY(),
                             card.getWidth(), card.getHeight(),
                             juce::Justification::centred, 1, .1f);
        letter.draw(g);
        x += width;
      }
      g.setFont(font);
      return;
    }
    if (v == 0 || v == 2 || v == 5 || v == 9 || v == 10) {
      juce::GlyphArrangement glyphs;
      glyphs.addFittedText(font, letters, area.getX(), area.getY(),
                           area.getWidth(), area.getHeight(),
                           juce::Justification::centred, 1);
      juce::Path ink;
      glyphs.createPath(ink);
      ink = shapeText(ink, size, v, t);
      g.setColour(tc);
      if (v == 2)
        g.strokePath(ink, juce::PathStrokeType(juce::jmax(1.0f, size * .014f),
                                               juce::PathStrokeType::curved));
      else if (v != 10)
        g.fillPath(ink);
      if (v == 9) {
        // Rounded, uneven letter edges shown by the Picnic style.
        g.strokePath(ink, juce::PathStrokeType(size * .012f,
                                               juce::PathStrokeType::curved));
      }
      if (v == 10) {
        auto bounds = ink.getBounds();
        g.strokePath(ink, juce::PathStrokeType(size * .032f));
        g.setColour(
            s.colour("color", true).withAlpha(tc.getFloatAlpha()));
        g.strokePath(ink, juce::PathStrokeType(size * .017f));
        g.setColour(tc);
        g.strokePath(ink, juce::PathStrokeType(size * .006f));
        for (int i = 0; i < 180; ++i) {
          auto point = juce::Point<float>(
              bounds.getX() + textNoise(i * 17) * bounds.getWidth(),
              bounds.getY() + textNoise(i * 29) * bounds.getHeight());
          if (ink.contains(point))
            g.drawEllipse(point.x, point.y, size * .009f, size * .009f,
                          size * .003f);
        }
      }
      return;
    }
    if (v == 4) {
      juce::GlyphArrangement message;
      message.addFittedText(font, letters, area.getX(), area.getY(),
                            area.getWidth(), area.getHeight(),
                            juce::Justification::centred, 1);
      juce::Path ink;
      message.createPath(ink);
      g.setColour(juce::Colours::black.withAlpha(tc.getFloatAlpha()));
      g.strokePath(ink, juce::PathStrokeType(juce::jmax(.6f, size * .08f)));
      g.setColour(tc);
      g.fillPath(ink);
      return;
    }
    g.drawText(letters, area, juce::Justification::centred);
  };
  if (fx == 4) {
    float total = juce::GlyphArrangement::getStringWidth(font, line),
          x = (w - total) / 2;
    for (int i = 0; i < line.length(); ++i) {
      auto ch = line.substring(i, i + 1);
      float width = juce::GlyphArrangement::getStringWidth(font, ch);
      drawLetters(ch,
                  juce::Rectangle<float>(
                      x, h / 2 - size / 2 + std::sin(t * 6 + i * .4f) * a * 18,
                      width + 2, size * 1.4f));
      x += width;
    }
  } else
    drawLetters(line, textArea.toFloat());
  if (fx == 3) {
    g.setColour(tc.withAlpha(0.4f));
    tc = tc.withAlpha(.4f);
    drawLetters(line,
                textArea.toFloat().translated(std::sin(t * 60) * a * 15, 0));
  }
  return image;
}
} // namespace kone::grid
