#include "ReferenceFx.h"
#include <cmath>
#include <limits>

namespace kone::grid {
namespace {
using RGB = std::array<float, 3>;
using Buffer = ReferenceFx::Buffer;
constexpr int W = ReferenceFx::width, H = ReferenceFx::height;
juce::uint8 byte(float f) { return (juce::uint8)juce::jlimit(0, 255, juce::roundToInt(f)); }
RGB pixel(const Buffer &b, int x, int y) {
  const auto p = (size_t)(juce::jlimit(0, H - 1, y) * W + juce::jlimit(0, W - 1, x)) * 4;
  return {(float)b[p], (float)b[p + 1], (float)b[p + 2]};
}
void put(Buffer &b, int x, int y, const RGB &c) {
  const auto p = (size_t)(y * W + x) * 4;
  for (int k = 0; k < 3; ++k) b[p + (size_t)k] = byte(c[(size_t)k]);
  b[p + 3] = 255;
}
RGB sample(const Buffer &b, float x, float y) {
  x = juce::jlimit(0.0f, (float)W - 1, x);
  y = juce::jlimit(0.0f, (float)H - 1, y);
  const int ix = (int)x, iy = (int)y;
  const float fx = x - ix, fy = y - iy;
  auto a = pixel(b, ix, iy), c = pixel(b, ix + 1, iy), d = pixel(b, ix, iy + 1), e = pixel(b, ix + 1, iy + 1);
  for (int k = 0; k < 3; ++k) a[(size_t)k] = (a[(size_t)k] * (1 - fx) + c[(size_t)k] * fx) * (1 - fy) + (d[(size_t)k] * (1 - fx) + e[(size_t)k] * fx) * fy;
  return a;
}
RGB mix(RGB a, const RGB &b, float amount) {
  for (int k = 0; k < 3; ++k) a[(size_t)k] += (b[(size_t)k] - a[(size_t)k]) * amount;
  return a;
}
juce::Colour colour(const RGB &c) { return {byte(c[0]), byte(c[1]), byte(c[2])}; }
RGB rgb(juce::Colour c) { return {(float)c.getRed(), (float)c.getGreen(), (float)c.getBlue()}; }
RGB enhance(RGB c, float amount) {
  const float peak = std::max({c[0], c[1], c[2], .001f});
  for (auto &v : c) v = juce::jmin(255.0f, v + v / peak * (255 - v) * amount);
  return c;
}
// Original 24-point Burst outline, recovered at 0x140a0d080.
constexpr std::array<juce::Point<float>, 24> burst{{
    {.594f,.274f},{.787f,.080f},{.920f,.213f},{.726f,.406f},
    {1,.406f},{1,.594f},{.726f,.594f},{.920f,.787f},
    {.787f,.920f},{.594f,.726f},{.594f,1},{.406f,1},
    {.406f,.726f},{.213f,.920f},{.080f,.787f},{.274f,.594f},
    {0,.594f},{0,.406f},{.274f,.406f},{.080f,.213f},
    {.213f,.080f},{.406f,.274f},{.406f,0},{.594f,0}}};
float polygonDistance(juce::Point<float> p) {
  float distance = std::numeric_limits<float>::max(); bool inside = false;
  for (size_t i = 0, j = burst.size() - 1; i < burst.size(); j = i++) {
    const auto a = burst[i], b = burst[j], v = b - a, q = p - a;
    const float t = juce::jlimit(0.0f, 1.0f, (q.x * v.x + q.y * v.y) / (v.x * v.x + v.y * v.y));
    distance = juce::jmin(distance, (q - v * t).getDistanceFromOrigin());
    if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
  }
  return inside ? -distance : distance;
}
}

void ReferenceFx::reset() {
  history.clear(); symbolSelection.clear(); symbolColumns = symbolRows = 0;
  lastSymbols = lastGlass = -1; lastOnset = glassFlash = lastGlassOnset = 0;
  reactive={};amountTime=-1;
  glassPhase=glassReflectionPhase=0;
}
std::array<float,4> ReferenceFx::effectiveAmounts(const SceneState &s) {
  const float dt=amountTime<0?1.0f/60:(float)juce::jlimit(0.0,.25,s.time-amountTime);
  amountTime=s.time;
  std::array<float,4> result{};
  const char* names[]{"texture","dither","track","glass"};
  // Recovered updates at 0x1405780a0/640/c50/0x1405791f0:
  // mean absolute input, +9 dB at center, .02 signal gate, .45 duty cycle.
  const float gain=std::pow(10.0f,((s.get("in")/127.0f-.5f)*40+9)/20);
  for(int i=0;i<4;++i) {
    const float manual=s.get(names[i])/127.0f;
    auto &state=reactive[(size_t)i];
    if(s.get((juce::String(names[i])+"React").toRawUTF8())<64) {
      state={};result[(size_t)i]=manual;continue;
    }
    if(s.audio.averageMagnitude*gain<=.02f || manual<=.005f) {state={};continue;}
    state.phase=std::fmod(state.phase+(1+19*s.get("speed")/127.0f)*dt,1.0f);
    const bool on=state.phase<.45f;
    if(on && !state.wasOn) {
      const float choice=random.nextFloat(),variation=random.nextFloat();
      const bool textureOrGlass=i==0||i==3;
      const float factor=choice<.6f?.9f+.2f*variation:
        textureOrGlass?(choice<.8f?1.2f+.3f*variation:.8f-.3f*variation):
                       (choice<.8f?1.2f+.3f*variation:.5f+.3f*variation);
      state.heldAmount=juce::jlimit(0.0f,1.0f,manual*factor);
    }
    state.wasOn=on;
    result[(size_t)i]=on?state.heldAmount:0;
  }
  return result;
}

void ReferenceFx::ditherAndTexture(Buffer &b, const SceneState &s, float d, float t) {
  if (b.size() != (size_t)W * H * 4) return;
  if (d > .005f) dither(b, s, d);
  if (t > .005f) texture(b, s, t);
}

void ReferenceFx::dither(Buffer &b, const SceneState &s, float amount) {
  const int type = decodeChoice(s.get("ditherType"), 4);
  const int cell = type == 0 ? juce::jmax(4, (int)(4 + 76 * std::pow(amount, 1.8f)))
                             : juce::jmax(10, juce::roundToInt(10 + 60 * std::pow(amount, 1.4f)));
  const int columns = (W + cell - 1) / cell, rows = (H + cell - 1) / cell;
  const bool audioReactive = s.get("ditherReact") >= 64;
  const float onset = s.amplitude();
  const bool changedGrid = symbolColumns != columns || symbolRows != rows;
  const bool onsetRise = onset > lastOnset + .02f && onset > .03f;
  if (changedGrid || lastSymbols < 0 || s.time < lastSymbols ||
      (s.time - lastSymbols >= .15 && (!audioReactive || onsetRise))) {
    symbolSelection.resize((size_t)columns * rows);
    for (auto &r : symbolSelection) r = random.nextFloat();
    symbolColumns = columns; symbolRows = rows; lastSymbols = s.time;
  }
  lastOnset = onset;
  // Exact UTF-8 glyph sets from the three original brightness tables. Repeats
  // are intentional weights, not accidental duplicate atlas slots.
  auto glyphSet = [](std::initializer_list<juce::juce_wchar> points) {
    juce::StringArray result; for (auto point : points) result.add(juce::String::charToString(point)); return result;
  };
  static const juce::StringArray bright = glyphSet({0x2588, 0x24, 0x24, 0x6b7b, 0x6b7b, 0x950, 0x950, 0x2a, 0x2a, 0x416, 0x416, 0x23, 0x23, 0x2b, 0x2b, 0x634, 0x634, 0x414, 0x414, 0x271d, 0x271d, 0x40, 0x2721});
  static const juce::StringArray medium = glyphSet({0x3f, 0x3f, 0x3d, 0x3d, 0x30c0, 0x30c0, 0x262a, 0x262a, 0x42f, 0x42f, 0x2206, 0x2206, 0x3c, 0x3c, 0x3e, 0x3e, 0x2d, 0x2d, 0x3a, 0x3a, 0x2e, 0x2e, 0x634, 0x414, 0x6b7b, 0x40, 0x2721});
  static const juce::StringArray dim = glyphSet({0x2d, 0x3a, 0x2e, 0x3c, 0x3e, 0x2206});
  juce::Image output(juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
  juce::Graphics g(output); g.fillAll(juce::Colours::black);
  g.setFont(juce::FontOptions("Consolas", cell * .80f, juce::Font::plain));
  for (int row = 0; row < rows; ++row) for (int col = 0; col < columns; ++col) {
    const int x = col * cell, top = row * cell;
    auto c = pixel(b, juce::jmin(W - 1, x + cell / 2), H - 1 - juce::jmin(H - 1, top + cell / 2));
    const float luma = (c[0] * .299f + c[1] * .587f + c[2] * .114f) / 255;
    const float strength = std::max({c[0],c[1],c[2]}) / 255 * .7f + luma * .3f;
    if (type == 0) {
      const float threshold = audioReactive && onset > .02f ? .20f : .04f;
      if (strength < threshold) continue;
      const auto &set = strength >= .45f ? bright : strength >= .15f ? medium : dim;
      const int index = juce::jmin(set.size() - 1, (int)(symbolSelection[(size_t)row * columns + col] * set.size()));
      g.setColour(colour(enhance(c, .4f)));
      g.drawText(set[index], x, top, cell, cell, juce::Justification::centred, false);
    } else {
      const float gap = (float)juce::jmax(1, juce::roundToInt(cell * .06f));
      const float radius = (cell - gap) * .5f * (.5f + luma * .5f);
      juce::Path shape;
      const juce::Point<float> centre(x + cell * .5f, top + cell * .5f);
      if (type == 1) shape.addStar(centre, 5, radius * .4f, radius);
      else if (type == 2) {
        for (int n = 0; n < 96; ++n) {
          const float a = juce::MathConstants<float>::twoPi * n / 96;
          const float r = radius * (.65f + .35f * std::cos(a * 6));
          const auto p = centre + juce::Point<float>(std::cos(a) * r, std::sin(a) * r);
          if (n == 0) shape.startNewSubPath(p); else shape.lineTo(p);
        }
        shape.closeSubPath();
      } else shape.addPolygon(centre, 6, radius, 0);
      g.setColour(colour(enhance(c, .6f))); g.fillPath(shape);
    }
  }
  juce::Image::BitmapData out(output, juce::Image::BitmapData::readOnly);
  for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) put(b, x, y, rgb(out.getPixelColour(x, H - 1 - y)));
}

void ReferenceFx::texture(Buffer &b, const SceneState &s, float amount) {
  const int type = decodeChoice(s.get("textureType"), 4);
  // The original UI remaps NOISE/GRAIN/CRT/THRESH to host indices [2,1,0,3]
  // at 0x140a0ca30. Host index zero is the composite CRT chain.
  if (type == 2) { noise(b, amount, s.amplitude()); return; }
  const auto primary = rgb(s.colour("color", s.category() == 7));
  std::array<RGB, 4> colours{{{0,0,0}, primary, {255,255,255}, rgb(s.colour("color2"))}};
  const int count = s.category() == 4 || s.category() == 8 ? 4 : 3;
  auto nearest = [&](const RGB &c) {
    float best = std::numeric_limits<float>::max(); int index = 0;
    for (int n = 0; n < count; ++n) {
      float distance = 0;
      for (int k = 0; k < 3; ++k) { const auto v = c[(size_t)k] - colours[(size_t)n][(size_t)k]; distance += v * v; }
      if (distance < best) { best = distance; index = n; }
    }
    return colours[(size_t)index];
  };
  std::array<RGB, W + 2> error{}, next{};
  // Combined UI/host dispatch: Grain = weighted Floyd-Steinberg,
  // Noise = scalar random perturbation + nearest palette, Thresh = palette.
  for (int y = 0; y < H; ++y) {
    next.fill({0,0,0});
    for (int x = 0; x < W; ++x) {
      auto c = pixel(b, x, y);
      if (type == 1) for (int k = 0; k < 3; ++k) c[(size_t)k] += error[(size_t)x + 1][(size_t)k];
      if (type == 0) { const float n = (random.nextFloat() - .5f) * 255 * amount; for (auto &v : c) v += n; }
      const auto chosen = nearest(c);
      if (type == 1) for (int k = 0; k < 3; ++k) {
        const auto i = (size_t)k; const float e = (c[i] - chosen[i]) * amount;
        error[(size_t)x + 2][i] += e * 7 / 16;
        next[(size_t)x][i] += e * 3 / 16;
        next[(size_t)x + 1][i] += e * 5 / 16;
        next[(size_t)x + 2][i] += e / 16;
      }
      put(b, x, y, chosen);
    }
    error = next;
  }
}

void ReferenceFx::noise(Buffer &b, float amount, float amp) {
  // Default Texture is a chain, not one scrolling bitmap. Derived strengths
  // are recovered at 0x14057dab0 and operations at 0x1405798e0..0x14057af10.
  scratch = b;
  const float k = amount * .2125f * .7f, half = H * .5f;
  const float zoom = 1 / (1 + k * ((W * .5f / half) * (W * .5f / half) + 1));
  for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
    const float qx = (x - W * .5f) / half * zoom, qy = (y - H * .5f) / half * zoom;
    const float f = 1 + k * (qx * qx + qy * qy);
    put(b,x,y,sample(scratch,W*.5f+qx*f*half,H*.5f+qy*f*half));
  }
  scratch = b;
  const float channel = amount * .6f;
  const int split = (int)(channel * 12);
  auto wrap = [](int x) { return (x % W + W) % W; };
  for (int y = 0; y < H; ++y) {
    int shift = (int)((random.nextFloat() - .5f) * channel * 16);
    if (random.nextFloat() < channel * .04f) shift += (int)((random.nextFloat() - .5f) * channel * 80);
    for (int x = 0; x < W; ++x) {
      RGB c{pixel(scratch,wrap(x-split+shift),y)[0],pixel(scratch,wrap(x+shift),y)[1],pixel(scratch,wrap(x+split+shift),y)[2]};
      if ((y & 1) == 0) for (auto &v : c) v = juce::jmax(0.0f,v-channel*.15f*255);
      put(b,x,y,c);
    }
  }
  if (history.size() != b.size()) history = b;
  const float persistence = .3f;
  for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
    auto current = pixel(b,x,y), previous = pixel(history,x,y);
    put(b,x,y,mix(current,previous,persistence));
    if (random.nextFloat() > amount * .55f * .5f) put(history,x,y,current);
  }
  // Native posterisation averages square blocks and quantizes the maximum
  // channel, preserving ratios. Protect the original zero-peak division.
  scratch = b;
  const float poster = amount * .65f;
  const int block = juce::jmax(1,(int)(poster*11+1)), levels = juce::jmax(2,(int)(256-poster*252));
  for (int y = 0; y < H; y += block) for (int x = 0; x < W; x += block) {
    RGB c{}; int count = 0;
    for (int yy = y; yy < juce::jmin(H,y+block); ++yy) for (int xx = x; xx < juce::jmin(W,x+block); ++xx) {
      const auto p = pixel(scratch,xx,yy); for (int i = 0; i < 3; ++i) c[(size_t)i] += p[(size_t)i]; ++count;
    }
    for (auto &v : c) v /= count;
    const float peak = std::max({c[0],c[1],c[2]});
    const float quant = std::round((levels-1)*peak/255)/(levels-1)*255;
    if (peak > 0) for (auto &v : c) v = quant*v/peak;
    for (int yy = y; yy < juce::jmin(H,y+block); ++yy) for (int xx = x; xx < juce::jmin(W,x+block); ++xx) put(b,xx,yy,c);
  }
  if (amp > .15f && random.nextFloat() < amount * .7f) {
    scratch = b; const int y0 = random.nextInt(H), rows = juce::jmax(1,(int)(amount * 6));
    const int shift = (int)((random.nextFloat()-.5f)*amount*W*.6f);
    for (int y = y0; y < juce::jmin(H,y0+rows); ++y) for (int x = 0; x < W; ++x) put(b,x,y,pixel(scratch,wrap(x+shift),y));
  }
}

void ReferenceFx::glass(Buffer &b, const SceneState &s, float amount) {
  if (amount <= .005f || b.size() != (size_t)W * H * 4) return;
  if (!burstReady) {
    burstDepth = .01f;
    for (int y = 0; y < 100; ++y) for (int x = 0; x < 100; ++x) {
      const float d = polygonDistance({(x+.5f)/100,(y+.5f)/100});
      burstDistance[(size_t)y*100+x] = d;
      burstDepth = juce::jmax(burstDepth, -d);
    }
    burstReady = true;
  }
  const int type = decodeChoice(s.get("glassType"), 6);
  const float dt = lastGlass < 0 || s.time < lastGlass || s.time-lastGlass>.25 ? 1.0f/60 : (float)juce::jlimit(0.0, .1, s.time-lastGlass);
  lastGlass = s.time;
  const float onset = juce::jlimit(0.0f,1.0f,s.amplitude());
  if (onset > .15f && onset > lastGlassOnset + .03f && glassFlash < .1f) glassFlash = 1;
  glassFlash = juce::jmax(0.0f,glassFlash-dt*3.6f); lastGlassOnset = onset;
  // Native clocks advance only while Glass renders. Opening a lens must not
  // jump its angle to the app's total running time.
  glassPhase+=dt*2.4f;glassReflectionPhase+=dt*1.44f;
  const float phase = glassPhase, phase2 = glassReflectionPhase;
  const float shapeRotation=(type==3||type==5)?phase*.8f:0;
  const float jx = std::sin(phase*17.3f)*glassFlash*glassFlash*12, jy = std::cos(phase*13.7f)*glassFlash*glassFlash*12;
  const float radius = H*(.12f+amount*.52f+amount*onset*.03f);
  const float strength = .1f+amount*.6f;
  const float xScale = juce::jmin(1.0f, (W / (float)H) / (s.width / (float)juce::jmax(1, s.height)));
  scratch = b;
  for (int y = 0; y < H; y += 2) for (int x = 0; x < W; x += 2) {
    const float worldX = (x-jx-W*.5f)/xScale+W*.5f;
    float cx = W*.5f, cy = H*.5f, r = radius;
    if (type == 1) { cx = worldX < W/2 ? W*.25f : W*.75f; cy = y < H/2 ? H*.25f : H*.75f; r = H*(.04f+amount*.19f+amount*onset*.01f); }
    if (type == 2) { const float cell = W/6.0f; cx = (std::floor(worldX/cell)+.5f)*cell; cy = (std::floor((y-jy)/cell)+.5f)*cell; r = cell*(.15f+amount*.35f); }
    const float dx = worldX-cx, dy = y-jy-cy;
    float normalised = std::sqrt(dx*dx+dy*dy)/r;
    if (type == 3) {
      float petal = std::atan2(dy,dx)-shapeRotation;
      petal = petal/juce::MathConstants<float>::twoPi*8;
      petal -= std::floor(petal);
      const float boundary = r*(.32f+.68f*std::pow(1-std::abs(petal*2-1),.6f));
      normalised = juce::jmax(.01f,std::sqrt(dx*dx+dy*dy)/boundary);
    } else if (type == 4) {
      const float arm = r*.16f;
      // OpenGL row coordinates: the long lower stem extends seven half-widths.
      const bool stem = std::abs(dx)<arm && dy>-arm*7 && dy<arm*3;
      const bool bar = std::abs(dx)<arm*3 && std::abs(dy)<arm;
      if (!stem && !bar) normalised = 2;
      else {
        float inside = 0;
        if (stem) inside = juce::jmin(arm-std::abs(dx),juce::jmin(dy+arm*7,arm*3-dy));
        if (bar) inside = juce::jmax(inside,juce::jmin(arm-std::abs(dy),arm*3-std::abs(dx)));
        normalised = juce::jmax(.01f,1-inside/juce::jmax(arm,arm*1.5f));
      }
    } else if (type == 5) {
      const float cs = std::cos(-shapeRotation), sn = std::sin(-shapeRotation);
      const float u = (r+dx*cs-dy*sn)/(r*2), v = (r+dx*sn+dy*cs)/(r*2);
      if (u<-.05f || u>1.05f || v<-.05f || v>1.05f) normalised = 2;
      else {
        const float fx = juce::jlimit(0.0f,99.0f,u*100-.5f), fy = juce::jlimit(0.0f,99.0f,v*100-.5f);
        const int ix = (int)fx, iy = (int)fy;
        auto at = [&](int xx,int yy) { return burstDistance[(size_t)juce::jmin(99,yy)*100+juce::jmin(99,xx)]; };
        const float tx = fx-ix, ty = fy-iy;
        const float sd = (at(ix,iy)*(1-tx)+at(ix+1,iy)*tx)*(1-ty)+(at(ix,iy+1)*(1-tx)+at(ix+1,iy+1)*tx)*ty;
        normalised = sd>0 ? 1+sd/(amount*.005f+.005f) : juce::jmax(.01f,1+sd/burstDepth);
      }
    }
    RGB c = pixel(scratch,x,y);
    if (normalised < 1) {
      const float r2 = normalised*normalised, z = std::sqrt(juce::jmax(0.0f,1-r2)), rim = 1-z;
      const float magnification = 1-rim*(1+.6f/(z+.15f))*strength;
      const float screenCentre = W*.5f+(cx-W*.5f)*xScale;
      const int sx = juce::roundToInt(screenCentre+(x-screenCentre)*magnification), sy = juce::roundToInt(cy+(y-cy)*magnification);
      const float spread = amount*r2*4;
      c = {pixel(scratch,juce::roundToInt(sx+spread),sy)[0],pixel(scratch,sx,sy)[1],pixel(scratch,juce::roundToInt(sx-spread),sy)[2]};
      const float nx = dx/r, ny = dy/r;
      float caustic = amount*juce::jmax(0.0f,std::sin(ny*7+nx*10+phase*3.5f+z*5)*std::sin(nx*6-ny*11+phase2*2.8f+z*7));
      caustic *= caustic*(onset*120+80);
      const float reflection = rim*rim*rim*rim*amount;
      for (int k = 0; k < 3; ++k) c[(size_t)k] = (c[(size_t)k]+caustic*(k==1?.85f:1)+reflection*(140+k*20))*(1+amount*z*.15f);
    } else if (normalised < 1+amount*.02f) {
      const float edge = 1-(normalised-1)/(amount*.02f+.001f);
      for (int k = 0; k < 3; ++k) c[(size_t)k] += edge*edge*amount*(onset*.3f+.5f)*(180+k*20);
    }
    // User-requested correction: retain the source outside the lenses. Native
    // Glass darkens that area; do not reproduce the previous black-out bug.
    if (normalised >= 1+amount*.02f) continue;
    for (int yy = y; yy < juce::jmin(H,y+2); ++yy) for (int xx = x; xx < juce::jmin(W,x+2); ++xx) put(b,xx,yy,c);
  }
}
} // namespace kone::grid
