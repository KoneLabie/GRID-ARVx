#include "Scenes.h"
#include "NativeGeometry.h"
namespace kone::grid {
static constexpr float pi = juce::MathConstants<float>::pi;
static float random(int n) {
  return (float)((unsigned)(n * 1664525u + 1013904223u) % 65536u) / 65535.0f;
}
juce::Path traceImage(const juce::Image &input) {
  if (!input.isValid()) return {};
  // Marching squares keeps internal holes and separate components; resampling
  // bounds work and contour size.
  float resize =
      320.0f / juce::jmax(1, juce::jmax(input.getWidth(), input.getHeight()));
  auto im = input.rescaled(
      juce::jmax(1, juce::roundToInt(input.getWidth() * resize)),
      juce::jmax(1, juce::roundToInt(input.getHeight() * resize)));
  juce::Path path;
  bool useAlpha = false;
  int minAlpha=255,maxAlpha=0; float minimum=1,maximum=0;
  for (int y=0;y<im.getHeight();++y) for(int x=0;x<im.getWidth();++x) {
    const auto c=im.getPixelAt(x,y);
    minAlpha=juce::jmin(minAlpha,(int)c.getAlpha()); maxAlpha=juce::jmax(maxAlpha,(int)c.getAlpha());
    minimum=juce::jmin(minimum,c.getBrightness()); maximum=juce::jmax(maximum,c.getBrightness());
  }
  useAlpha = maxAlpha > minAlpha && minAlpha < 240;
  const float threshold = (minimum+maximum)*.5f;
  const int alphaThreshold = (minAlpha+maxAlpha)/2;
  if (maxAlpha==0 || (!useAlpha && maximum-minimum < .001f)) return {};
  bool invert =
      !useAlpha &&
      (im.getPixelAt(0, 0).getBrightness() +
       im.getPixelAt(im.getWidth() - 1, 0).getBrightness() +
       im.getPixelAt(0, im.getHeight() - 1).getBrightness() +
       im.getPixelAt(im.getWidth() - 1, im.getHeight() - 1).getBrightness()) >
          2;
  auto inside = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= im.getWidth() || y >= im.getHeight())
      return false;
    auto c = im.getPixelAt(x, y);
    return useAlpha ? c.getAlpha() > alphaThreshold
           : invert ? c.getBrightness() < threshold
                    : c.getBrightness() > threshold;
  };
  const int lines[16][5] = {
      {0, 0, 0, 0, 0}, {1, 3, 0, 0, 0}, {1, 0, 1, 0, 0}, {1, 3, 1, 0, 0},
      {1, 1, 2, 0, 0}, {2, 0, 1, 2, 3}, {1, 0, 2, 0, 0}, {1, 3, 2, 0, 0},
      {1, 2, 3, 0, 0}, {1, 0, 2, 0, 0}, {2, 0, 3, 1, 2}, {1, 1, 2, 0, 0},
      {1, 1, 3, 0, 0}, {1, 0, 1, 0, 0}, {1, 3, 0, 0, 0}, {0, 0, 0, 0, 0}};
  for (int y = -1; y < im.getHeight(); ++y)
    for (int x = -1; x < im.getWidth(); ++x) {
      int c = (inside(x, y) ? 1 : 0) | (inside(x + 1, y) ? 2 : 0) |
              (inside(x + 1, y + 1) ? 4 : 0) | (inside(x, y + 1) ? 8 : 0);
      juce::Point<float> p[4] = {{x + 0.5f, (float)y},
                                 {x + 1.0f, y + 0.5f},
                                 {x + 0.5f, y + 1.0f},
                                 {(float)x, y + 0.5f}};
      for (int k = 0; k < lines[c][0]; ++k) {
        auto a = p[lines[c][1 + k * 2]], b = p[lines[c][2 + k * 2]];
        path.startNewSubPath(a);
        path.lineTo(b);
      }
    }
  path.applyTransform(
      path.getTransformToScaleToFit(-0.8f, -0.8f, 1.6f, 1.6f, true));
  return path;
}
static juce::Path polyhedron(int shape, float time) {
  struct V {
    float x, y, z;
  };
  std::vector<V> vertices;
  juce::Path path;
  auto project = [&](V p) {
    float x = p.x * std::cos(time * .3f) + p.z * std::sin(time * .3f),
          z = -p.x * std::sin(time * .3f) + p.z * std::cos(time * .3f);
    float y = p.y * std::cos(time * .19f) - z * std::sin(time * .19f);
    z = p.y * std::sin(time * .19f) + z * std::cos(time * .19f);
    float depth = 3 / (3 + z * .5f);
    return juce::Point<float>(x * depth * .65f, y * depth * .65f);
  };
  auto segment = [&](V a, V b) {
    path.startNewSubPath(project(a));
    path.lineTo(project(b));
  };
  if (shape == 10) {
    for (int latitude = -4; latitude <= 4; ++latitude) {
      float y = latitude / 5.0f, r = std::sqrt(1 - y * y);
      for (int i = 0; i < 64; ++i) {
        float q = i / 64.0f * 2 * pi, qq = (i + 1) / 64.0f * 2 * pi;
        segment({std::cos(q) * r, y, std::sin(q) * r},
                {std::cos(qq) * r, y, std::sin(qq) * r});
      }
    }
    for (int longitude = 0; longitude < 12; ++longitude) {
      float q = longitude / 12.0f * 2 * pi;
      for (int i = 0; i < 32; ++i) {
        float a = (i / 32.0f - .5f) * pi, b = ((i + 1) / 32.0f - .5f) * pi;
        segment(
            {std::cos(a) * std::cos(q), std::sin(a), std::cos(a) * std::sin(q)},
            {std::cos(b) * std::cos(q), std::sin(b),
             std::cos(b) * std::sin(q)});
      }
    }
    return path;
  }
  if (shape == 9 || shape == 12)
    for (float x : {-1.0f, 1.0f})
      for (float y : {-1.0f, 1.0f})
        for (float z : {-1.0f, 1.0f})
          vertices.push_back({x, y, z});
  if (shape == 11)
    vertices = {{1, 1, 1}, {-1, -1, 1}, {-1, 1, -1}, {1, -1, -1}};
  if (shape == 14)
    vertices = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  float phi = (1 + std::sqrt(5.0f)) / 2;
  if (shape == 12 || shape == 13)
    for (float a : {-1.0f, 1.0f})
      for (float b : {-1.0f, 1.0f}) {
        float small = shape == 12 ? 1 / phi : 1;
        vertices.push_back({0, a * small, b * phi});
        vertices.push_back({a * small, b * phi, 0});
        vertices.push_back({b * phi, 0, a * small});
      }
  auto distance = [](V a, V b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) +
                     (a.z - b.z) * (a.z - b.z));
  };
  float edge = 100;
  for (size_t i = 0; i < vertices.size(); ++i)
    for (size_t j = i + 1; j < vertices.size(); ++j)
      edge = std::min(edge, distance(vertices[i], vertices[j]));
  for (size_t i = 0; i < vertices.size(); ++i)
    for (size_t j = i + 1; j < vertices.size(); ++j)
      if (distance(vertices[i], vertices[j]) < edge + .01f)
        segment(vertices[i], vertices[j]);
  return path;
}
juce::Image drawScene(const SceneState &s, const Assets &assets) {
  // Text is an independent renderer; no contour or visual-family scene runs
  // here.
  if (s.category() == 7)
    return drawTextScene(s, assets);
  juce::Image image(juce::Image::ARGB, s.width, s.height, true,
                    juce::SoftwareImageType());
  juce::Graphics g(image);
  int cat = s.category(), v = s.variant();
  float w = (float)s.width, h = (float)s.height, a = s.amplitude(),
        t = (float)s.time * (s.get("speed") / 64.0f),
        scale = juce::jmin(w, h) * 0.38f;
  auto color = s.colour("color", cat==7);
  g.fillAll(cat == 8 ? s.colour("color2").withMultipliedBrightness(.12f)
                     : juce::Colours::black);
  g.setColour(color);
  if (assets.contour.isEmpty() && drawNativeGeometry(g,s)) return image;
  auto fit = juce::AffineTransform::scale(scale).translated(w / 2, h / 2);
  juce::Path path;
  if (!assets.contour.isEmpty()) {
    path = assets.contour;
    path.applyTransform(
        juce::AffineTransform::rotation(t * 0.2f).scaled(1.0f + a * 0.15f));
  } else if (cat == 0 || cat == 8) {
    int shape = v;
    if (cat == 8) {
      // Seven original Plasma families, each with a separate path topology.
      const int copies = v==4 ? 12 : v==0 || v==1 || v==5 ? 2 : 1;
      for(int k=0;k<copies;++k) for(int i=0;i<=512;++i) {
        const float u=i/512.0f,q=u*2*pi,sample=s.audio.waveform[(size_t)(i%256)]*inputGain(s.get("in"));
        float x=u*2-1,y=0;
        if(v==0) y=(k? -1.0f:1.0f)*(std::abs(sample)*.65f+std::sin(q*4+t)*.15f);
        if(v==1) y=std::sin(q*3+t+k*pi)*(.5f+a*.1f);
        if(v==2) {x=std::sin(q*2+t*.12f);y=std::sin(q*3)*.65f+sample*.1f;}
        if(v==3) {const float radius=.45f+a*.2f+sample*.12f;x=std::cos(q)*radius;y=std::sin(q)*radius;}
        if(v==4) {x=std::sin(q*3+k*pi/6+t*.2f);y=std::sin(q*2+k*pi/6)*.7f+sample*.1f;}
        if(v==5) {y=std::sin(q*2+t)*.5f+std::sin(q*5-t)*.15f+(k?1.0f:-1.0f)*(.03f+a*.08f);}
        if(v==6) {const float r=.45f+.2f*std::cos(q*3);x=std::cos(q*2+t*.2f)*r;y=std::sin(q*2+t*.2f)*r;}
        if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
      }
    } else
    if (shape >= 9) {
      path = polyhedron(shape, t);
    } else {
      int copies = shape == 3   ? 12
                   : shape == 4 ? 5
                   : shape == 5 ? 2
                   : shape == 6 ? 2
                   : shape == 7 ? 12
                                : 1;
      for (int k = 0; k < copies; ++k)
        for (int i = 0; i < 256; ++i) {
          float u = i / 255.0f, q = u * 2 * pi;
          float sample = s.audio.waveform[(size_t)i] * inputGain(s.get("in")) *
                         s.get("int") / 64.0f;
          float x = u * 2 - 1, y = sample * 0.8f;
          if (shape == 1) {
            x = s.audio.left[(size_t)i] * inputGain(s.get("in")) *
                s.get("int") / 32.0f;
            y = s.audio.right[(size_t)i] * inputGain(s.get("in")) *
                s.get("int") / 32.0f;
            if (cat == 8) {
              x = std::sin(q * 2 + t * .1f) + sample * .1f;
              y = std::sin(q * 3) * .65f + sample * .1f;
            }
          }
          if (shape == 2) {
            x = std::cos(q) * (0.75f + sample * 0.5f);
            y = std::sin(q) * (0.75f + sample * 0.5f);
            if (cat == 8) {
              float radius =
                  .55f + .25f * std::cos(q * 5 + t * .1f) + sample * .1f;
              x = std::cos(q) * radius;
              y = std::sin(q) * radius;
            }
          }
          if (shape == 3) {
            float r = (k + 1) / 12.0f;
            x = std::cos(q) * r;
            y = std::sin(q) * r * 0.6f;
          }
          if (shape == 4) {
            x = u * 2 - 1;
            y = std::sin(q * 3 + t + k * 0.4f) * (0.1f + a * 0.3f) + k * 0.1f -
                0.2f;
          }
          if (shape == 5)
            y = (k == 0 ? 1 : -1) * std::abs(sample) * 0.8f;
          if (shape == 6)
            y = (k == 0 ? 1 : -1) * std::sin(q * 3 + t) * .6f;
          if (shape == 7) {
            x = k / 6.0f - 0.9f;
            y = u * 2 - 1 + sample * 0.1f;
          }
          if (shape == 8)
            y = std::abs(std::sin(q * 4 + t)) * (0.1f + a * 0.8f) - 0.4f;
          if (cat == 8 && shape == 0)
            y = std::sin(q * 4 + t) * .5f + sample * .2f;
          if (i == 0)
            path.startNewSubPath(x, y);
          else
            path.lineTo(x, y);
        }
    }
  } else if (cat == 2) {
    if (v == 2 || v == 3 || v == 5 || v == 7) {
      juce::ColourGradient nebula(color.withAlpha(.2f + a * .1f), w / 2, h / 2,
                                  juce::Colours::black, w / 2, h * .95f, true);
      g.setGradientFill(nebula);
      g.fillAll();
      if (v != 3)
        for (int i = 0; i < 100; ++i) {
          float x = random(i * 17) * w,
                y = std::fmod(random(i * 31) * h + t * (v == 5 ? 35 : 3), h);
          g.setColour(color.withAlpha(.2f + random(i * 7) * .6f));
          if (v == 5)
            g.drawLine(x, y, x, y + 4 + a * 25, .7f);
          else
            g.fillEllipse(x, y, 1.3f, 1.3f);
        }
    }
    int orbitCount = v == 0 || v == 6 ? 28 : v == 1 ? 36 : v == 3 ? 12 : 0;
    for (int k = 0; k < orbitCount; ++k) {
      float r = (k + 1) / 28.0f;
      for (int i = 0; i <= 120; ++i) {
        float q = i / 120.0f * 2 * pi,
              x = std::cos(q+t*.08f) * r * (v == 6 ? 0.7f : 1.6f),
              y = std::sin(q) * r * 0.8f;
        if (v == 6)
          y = y * 0.4f + r * 1.6f - 0.8f;
        else if(v==1) {x=std::cos(q+t*r)*r;y=std::sin(q+t*r)*r*.7f;}
        else
          y += std::sin(q * 2 + t) * 0.07f;
        if (i == 0)
          path.startNewSubPath(x, y);
        else
          path.lineTo(x, y);
      }
    }
    g.setColour(color.withAlpha(0.18f + a * 0.25f));
    g.strokePath(path, juce::PathStrokeType(0.8f), fit);
    path.clear();
    if(v==4) for(int i=0;i<70;++i) {
      const auto x=random(i*17)*w,y=random(i*31)*h;
      g.fillEllipse(x,y,1.5f+a*2,1.5f+a*2);
      if(i%3==0)g.drawLine(x,y,random((i+1)*17)*w,random((i+1)*31)*h,.3f);
    }
    if(v==7) for(int i=0;i<12;++i) {
      const auto q=i*pi/6+t*.2f;
      g.setColour(palette((s.get("color")+i*8)%126).withAlpha(.65f));
      g.drawLine(w/2,h/2,w/2+std::cos(q)*scale*(.3f+a),h/2+std::sin(q)*scale*(.3f+a),1.5f);
    }
    g.setColour(color);
    g.fillEllipse(w / 2 - 4 - a * 6, h / 2 - 4 - a * 6, 8 + a * 12, 8 + a * 12);
  } else if (cat == 3) {
    int columns = 24 + s.detail * 12, rows = v == 6 ? 36 : 20;
    g.setFont(juce::FontOptions(h / rows * 0.75f));
    const juce::String glyphs = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ{}<>/";
    for (int x = 0; x < columns; ++x)
      for (int y = 0; y < rows; ++y) {
        float phase = std::fmod(
            y + random(x * 97) * rows + t * (2 + random(x) * 5), (float)rows);
        if (v == 0 && phase > 5)
          continue;
        if(v==2) {
          const float xx=(x+.5f)/columns*2-1,yy=(y+.5f)/rows*2-1;
          const bool head=xx*xx/.38f+yy*yy/.65f<1;
          const bool eyes=std::abs(std::abs(xx)-.23f)<.13f&&std::abs(yy+.12f)<.14f;
          const bool nose=std::abs(xx)<.06f&&yy>.05f&&yy<.28f;
          if(!head||eyes||nose||(yy>.48f&&x%3==0))continue;
        }
        float bright =
            v == 0 ? 1 - phase / 6
                   : 0.2f + random(x * 31 + y * 17 + (int)(t * 4)) * 0.8f;
        if (v == 4)
          bright *=
              juce::jlimit(.1f, 1.0f, a + .2f * std::sin(t * 4 + x * .2f));
        auto ink=color;
        if(v==3)ink=y<rows/3?color:y<rows*2/3?juce::Colours::white:s.colour("color2");
        if(v==7&&random(y*91+(int)(t*12))<.12f)ink=juce::Colours::white;
        g.setColour(ink.withAlpha(bright));
        auto glyph =
            glyphs.substring((x * 13 + y * 7 + (int)t) % glyphs.length(),
                             (x * 13 + y * 7 + (int)t) % glyphs.length() + 1);
        g.drawText(
            glyph,
            juce::Rectangle<float>(x * w / columns,
                                   y*h/rows+(v==5?std::sin(x*.2f+t)*h*.08f:0),
                                   w / columns, h / rows),
            juce::Justification::centred);
      }
  } else if (cat == 6) {
    static constexpr int legacyShapes[]{0,7,1,2,3,8,4,5,6};
    v=legacyShapes[juce::jlimit(0,8,v)];
    int count = 80 + s.detail * 50;
    for (int i = 0; i < count; ++i) {
      float x = random(i * 3) * w, y = random(i * 11) * h, size = 2.0f + (float)s.detail;
      float phase =
          std::fmod(t * (15 + random(i) * 40) + random(i * 11) * h, h);
      g.setColour(color.withMultipliedBrightness(0.4f + random(i * 5) * 0.6f));
      if (v == 0) {
        float bar = (0.1f + a) * random(i * 7) * h;
        g.fillRect(x, h - bar, w / count * 1.5f, bar);
      } else if (v == 1) {
        float r = (std::sin(x / w * 8 + t) + std::cos(y / h * 7 - t)) * 0.5f;
        g.fillRect(x, y, 8.0f, juce::jmax(2.0f, (r + 1) * 10));
      } else if (v == 2) {
        if (random(i * 13 + (int)(t * 5)) < 0.55f) {
          g.fillRect(x, y, 1.0f, 8.0f);
          g.fillRect(x - 3, y + 3, 7.0f, 1.0f);
        }
      } else if (v == 3) {
        float yy = h / 2 + s.audio.waveform[(size_t)(i % 256)] * h *
                               inputGain(s.get("in")) * 0.5f;
        g.fillRect(i * w / count, yy, 3.0f, juce::jmax(2.0f, a * 20));
      } else if (v == 4) {
        g.fillRect(x, phase, size, 2.0f + a * 35);
      } else if (v == 5) {
        g.drawLine(x - 5, y, x + 5, y);
        g.drawLine(x, y - 5, x, y + 5);
      } else if (v == 6) {
        float r = 8 + random(i * 2) * a * 50;
        g.fillEllipse(x - r, h - phase - r, r * 2, r * 2);
      } else if (v == 7) {
        g.fillRect(x, phase, 3.0f, 15.0f + a * 20);
      } else if (v == 8) {
        float bh = random(i * 8) * h * (0.2f + a);
        g.fillRect(x, h - bh, 10.0f, bh);
        g.setColour(juce::Colours::black);
        for (int j = 0; j < 12; ++j)
          g.fillRect(x + 3, h - bh + j * 8, 3.0f, 3.0f);
      } else
        g.fillRect(x, h - phase, 3.0f, 5 + a * 20);
    }
  }
  if (!path.isEmpty()) {
    g.setColour(color);
    g.strokePath(path,
                 juce::PathStrokeType(cat == 8 ? 2.0f : 1.3f,
                                      juce::PathStrokeType::curved),
                 fit);
  }
  return image;
}
} // namespace kone::grid
