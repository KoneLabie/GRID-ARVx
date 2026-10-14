#include "Canvas.h"
#include "NativeShaders.h"
namespace kone::grid {
using namespace juce::gl;
static constexpr auto nativeArgbFormat =
    juce::PixelARGB::indexR == 0 ? GL_RGBA : GL_BGRA;
static const char *vertex =
    R"GL(attribute vec2 position;varying vec2 uv;void main(){uv=position*.5+.5;gl_Position=vec4(position,0.,1.);})GL";
static const char *fragment = R"GL(
varying vec2 uv;uniform sampler2D sceneImage,bloomImage;uniform int preRendered,secondaryPalette;uniform float time,amp,bass,glow,nativeTime,nativeIntensity,nativeSpeed,nativeOnset;uniform vec2 resolution;uniform vec3 color,color2,textTint;uniform vec4 nativeBands;
uniform int category,variant,tracing,hasImage,trackType,boxCount,preEffects;uniform float trackAmount;uniform vec4 boxes[16],voronoiSeeds[40];uniform int voronoiLabels[40];
float hash(vec2 p){return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453);}
vec3 hsv(float h){return clamp(abs(mod(h*6.+vec3(0,4,2),6.)-3.)-1.,0.,1.);}
vec3 hsl(float h,float s,float l){vec3 rgb=clamp(abs(mod(h*6.+vec3(0,4,2),6.)-3.)-1.,0.,1.);return l+s*(rgb-.5)*(1.-abs(2.*l-1.));}
// Native Lava pixel renderer 0x140454eb0, including its nine separate fields
// and HSL conversion. The original evaluated these equations in 4x4 blocks.
vec3 lava(vec2 p){
 vec2 q=p*2.-1.;float r=length(q),theta=atan(q.y,q.x);
 float I=pow(nativeIntensity/.325,2.2),T=nativeTime*(nativeSpeed*6.+1.5),lo=nativeBands.x*I,mi=nativeBands.y*I,hi=nativeBands.z*I,hit=nativeOnset*I,rms=nativeBands.w*I;
 float f=0.;
 if(variant==0){vec2 a=p*5.4;f=(sin(length(a)*2.2-T*1.6+hi*5.)+sin(a.y*1.8-T*.9+mi*3.)+sin(a.x*2.1+T*1.1+lo*4.)+sin((a.y+a.x)*1.4+T*1.4)+sin((sin(T*.4)*a.y+cos(T*.3)*a.x)*1.7)+5.)/10.;}
 if(variant==1){f=pow(max(1.-r,0.),lo*2.+1.5)*(.5+.5*sin(theta*5.+T*2.))*.7+(.5+.5*sin(r*14.-T*2.+lo*6.))*(1.-r)*.3+(1.-r*.8)*hit*.5;f=min(f,1.);}
 if(variant==2){f=clamp(sin(q.x*2.+q.y*1.5+T*.6)*.2+sin(q.x*3.+T*.8+lo*4.)*.4+.5+sin(q.x*5.-T*.5+mi*3.)*.25+(hit*.4+.3)*q.y,0.,1.);}
 if(variant==3){vec2 a=p*6.6+vec2(sin(mi*2.+T*.4),cos(mi*2.+T*.35))*.4;float n=sin((a.x+a.y)*2.+T*.9+lo*3.)*.5;f=min(1.,pow(max(0.,(sin(a.x*2.8+T*1.8)*sin(a.y*2.8-T*1.4)+n)*.5+.5),max(.2,1.2-hit*1.5))+hit*.3);}
 if(variant==4){vec2 a=p*7.2+vec2(sin(hi*2.+T*.5),cos(hi*2.+T*.4))*.3;f=sin((sin(a.y*3.-T)+sin(a.x*3.+T)+sin((a.x+a.y)*2.))*3.14159274*(hi*4.+2.))*.5+.5;}
 if(variant==5){f=pow(max(1.-r*(sin(theta*7.+T*.8)*.4+lo*1.4+.8),0.),lo*1.5+1.8)*.7+(.5+.5*sin(r*16.-T*1.8+lo*8.))*(1.-r*.95)*.3+(1.-r*.75)*hit*.55;f=min(f,1.);}
 if(variant==6){vec2 cell=floor(q*4.),local=fract(q*4.)-.5;float id=mod(cell.x*127.+cell.y*311.,17.)/17.,edge=min(abs(local.x),abs(local.y))*2.;f=min(1.,abs(sin((id+.5)*T+id*6.28318548+id*lo*4.))*(1.-edge*.6)+edge*hit*.8);}
 if(variant==7){float angle=abs(fract((theta/3.14159274+1.)*3.)-.5)*6.28318548;vec2 a=vec2(cos(angle),sin(angle))*r;f=(sin(a.x*8.+T*1.3+lo*3.)+sin(a.y*8.-T+mi*2.)+sin((a.x+a.y)*5.6+T*.8)+3.)/6.;}
 if(variant==8){f=min(1.,pow(max(1.-r,0.),lo*1.8+1.2)*(.5+.5*sin((r*4.*(lo*2.+1.)+theta-T*1.2)*3.))*.75+(.5+.5*sin(r*12.-T*2.+lo*5.))*(1.-r)*.25+(1.-r*.85)*hit*.55);}
 float hue=fract((f*(nativeSpeed*300.+60.)+dot(color,vec3(.31,.23,.15))*360.)/360.);
 float light=min(1.,f*.75+.1+rms*.15),sat=mi*.2+.8;
 return clamp(hsl(hue,sat,light),0.,1.);
}

float nativeHash(ivec2 p){int n=p.x*0x165667b1+p.y*0x27d4eb2f;n=(n^(n>>13))*0x4bf19f61;return float((n^(n>>16))&0x7fffffff)*4.65661287e-10;}
float nativeNoise(vec2 p){ivec2 cell=ivec2(floor(p));vec2 f=fract(p);f=f*f*(3.-2.*f);return mix(mix(nativeHash(cell),nativeHash(cell+ivec2(1,0)),f.x),mix(nativeHash(cell+ivec2(0,1)),nativeHash(cell+ivec2(1,1)),f.x),f.y);}
vec3 threeColor(int n){n=(n%3+3)%3;return n==0?color:n==1?(secondaryPalette==1?color2:vec3(1)):(secondaryPalette==1?color2:vec3(0));}
vec3 basic(vec2 p){
 vec2 pixel=floor(p*resolution/3.)*3.,q=pixel/resolution,centre=resolution*.5;
 float I=nativeIntensity,S=nativeSpeed,T=nativeTime*(4.17*S+.4),E=2.*pow(I*2.,2.8)*dot(nativeBands.xz,vec2(.6,.4))/1.44544;
 vec3 alt=secondaryPalette==1?color2:vec3(0);float period=resolution.x/18.,shift=0.,threshold=period*.62;
 if(variant==0){
  float k=min(I*1.8,1.),a=(sin(T*.41*.8+q.y*3.1+.5)*.5+.5)*.4+.4+(sin(T*.73*.5+q.y*1.7+2.3)*.5+.5)*.2;
  a*=k*E*92.+k*67.+1.;
  shift=(sin(q.x*6.8+q.y*1.2+T*2.1)+sin(q.y*4.3-q.x*.8+T*.73*1.7+1.9)*.45+sin(q.x*2.5+q.y*2.1+T*.41*1.1+3.4)*.3)*a;
  shift+=sin(q.y*8.5+T*3.2)*E*k*18.;shift+=((nativeNoise(vec2(q.x*2.5+T*.15,q.y*2.5+.3))-.5)*1.8+(nativeNoise(vec2(q.x*4.25+1.3,q.y*2.5+T*.105))-.5)*1.2)*a*.6;
  shift+=sin(q.y*1.5+nativeTime*(k*E*.3+.06))*a*.25*q.x;
  return mod(pixel.x+shift,period)<threshold?color:alt;
 }
 if(variant==1||variant==8){float stepSize=max(6.,16.-nativeBands.w*I),travel=(S*360.+20.)*nativeTime;vec2 d=abs(pixel-centre);float dist=variant==1?length(d):max(d.x,d.y);return threeColor(int((dist+mod(travel,stepSize))/stepSize)+int(travel/stepSize));}
 if(variant==2){float nearest=1e6,second=1e6;int label=0;float magnitude=.02+I*.133+I*E*.167;
  for(int i=0;i<40;++i){vec4 seed=voronoiSeeds[i];vec2 cell=seed.xy+vec2(sin(T*seed.z*4.+seed.y*6.),cos(T*seed.w*4.+seed.x*6.))*magnitude+vec2(sin(seed.x*5.+T*.42),cos(seed.y*5.+T*.36))*magnitude*.5;float d=dot(q-cell,q-cell);if(d<nearest){second=nearest;nearest=d;label=i;}else second=min(second,d);}
  if(sqrt(second)-sqrt(nearest)<.012)return vec3(0);return secondaryPalette==1&&voronoiLabels[label]==1?alt:color;
 }
 if(variant==3){vec2 d=pixel-centre;float r=length(d)/length(centre),angle=atan(d.y,d.x)+(1.-r)*(I*50.+8.+I*E*67.)*.08+T*.5;
  angle+=(nativeNoise(vec2(q.x*3.+T*.1,q.y*3.))-.5)*.5*I;
  angle+=sin(T*2.3+r*12.)*(I*50.+8.+I*E*67.)*.02+sin(angle*2.+r*7.+T*.9)*(I*50.+8.+I*E*67.)*.015;
  return fract(angle/6.28318548*16.)<.6?color:alt;
 }
 if(variant==4){float cell=(1.+nativeBands.w*I*.55)*48.;vec2 origin=(resolution-(floor(resolution/cell)+3.)*cell)*.5;float checker=mod(floor((pixel.x-origin.x)/cell)+floor((pixel.y-origin.y)/cell),2.);return checker<1.?color:(secondaryPalette==1?color2:vec3(1));}
 if(variant==5){float a=((sin(q.y*3.+q.x*2.+T*.65*.6)*.5+.5)*.6+.4)*(I*50.+8.+I*E*67.);
  vec2 c=vec2(sin(T*.3),cos(T*.23+1.2))*.15+.5;float radial=length((q-c)*vec2(1.,resolution.y/resolution.x))*resolution.x;
  shift=(sin(q.y*3.5-q.x*2.+T*.65*1.3+1.5)*.3+sin(q.x*3.+q.y*5.+T*1.8)*.5+(nativeNoise(vec2(q.x*3.+T*.1,q.y*3.+1.5))-.5)*.6)*a;
  return mod(radial+shift-T*40.*(S+.5),resolution.x/12.)<resolution.x/12.*.58?color:alt;
 }
 if(variant==6){float sector=6.28318548/(3.+16.*S),angle=mod(atan(pixel.y-centre.y,pixel.x-centre.x)-(3.*nativeBands.z+1.)*nativeTime,6.28318548);if(mod(angle,sector)>=(.75+nativeBands.x*I*.6)*sector)return vec3(0);return threeColor(int(angle/sector));}
 if(variant==7){float a=((sin(q.x*1.5+q.y*.4+T*.45*.7)*.5+.5)*.5+.5)*(I*58.+1.+I*E*75.);
  float n=(nativeNoise(vec2(q.x*2.5+T*.12,q.y*2.5+.7))-.5)*a*.5;
  float dx=(sin(q.x*3.8+q.y*.7+T*.45*1.2)*.35+sin(q.x*5.5+q.y*1.3+T*1.6+1.))*a+n;
  float dy=(sin(q.y*3.2-q.x*.9+T*.7*1.4+2.1)*.4+sin(q.x*1.3+q.y*5.5+T*1.8))*a+n;
  float size=resolution.x/16.;bool first=mod(pixel.x+pixel.y+dx+dy,size)<size*.55,other=mod(pixel.x-pixel.y-dx-dy,size)<size*.55;return first!=other?color:alt;
 }
 if(variant==9){if(nativeBands.w<=.001)return color;float phase=fract(nativeTime*(1.+19.*S));return phase<.2?mix(color,secondaryPalette==1?threeColor(int(nativeTime*(1.+19.*S))):vec3(1),I*.8+.2):color;}
 if(variant==10){float k=min(I*1.8,1.),a=((sin(q.x*3.+T*.43*.8+.5)*.5+.5)*.35+.4+(sin(q.x*1.6+q.y*.8+T*.7*.5)*.5+.5)*.25)*(k*E*83.+k*67.+1.);
  shift=((nativeNoise(vec2(q.x*2.5+T*.15,q.y*2.5+.3))+nativeNoise(vec2(q.x*4.+1.3,q.y*4.+T*.07))-.5)*.5+sin(q.x*4.3-q.y*.8+T*.7*1.7+1.9)*.45+sin(q.x*6.8+q.y*1.2+T*2.1)+sin(q.x*2.1+q.y*2.5+T*.43*1.1+3.4)*.3)*a;
  shift+=sin(q.x*8.5+T*3.2)*E*k*18.;return mod(pixel.y+shift,resolution.y/16.)<resolution.y/16.*.6?color:alt;
 }
 return color;
}
vec3 textBase(vec2 p){
 vec4 glyph=texture2D(sceneImage,vec2(p.x,1.-p.y));
 if(variant!=5||hasImage==1)return glyph.rgb;
 vec2 q=(p-.5)*vec2(resolution.x/resolution.y,1.)*5.;float f=sin(q.x+time)+cos(q.y-time*.7)+sin(length(q)*1.6-time);vec3 backdrop=hsv(f*.15+time*.035+dot(color,vec3(.31,.23,.15)))*(.2+.65*smoothstep(-2.,2.,f));return backdrop*(1.-glyph.a)+glyph.rgb;
}
vec3 base(vec2 p){
 if(preRendered==1)return texture2D(sceneImage,p).rgb;
 if(category==7)return textBase(p);
 if(category==1&&tracing==0)return lava(vec2(p.x,1.-p.y));
 if(category==4&&tracing==0)return basic(vec2(p.x,1.-p.y));

 if(category==5&&tracing==0){
 vec2 q=(p-.5)*vec2(resolution.x/resolution.y,1.);float r=length(q),theta=atan(q.y,q.x),n=hash(floor(p*resolution)+floor(time*30.));vec3 c=vec3(n);
 if(variant==0){float band=sin(p.y*28.+time*8.);c=vec3(n,hash(floor(p*resolution)+time*17.),hash(floor(p*resolution)-time*23.))*(.5+.5*band);}
 if(variant==1)c=hsv(p.x*8.+sin(p.y*30.+time)*.3+time*.2)*(.3+.7*n);
 if(variant==2)c=hsv((sin(q.x*7.+time)+cos(q.y*8.-time)+sin(r*14.-time))*.18+time*.04);
 if(variant==3)c=vec3(n)*(.3+.7*step(.5,sin(p.y*50.+time*7.)));
 if(variant==4)c=hsv(theta*.4-r*4.+time*.2)*(.2+.8*pow(abs(sin(theta*3.+r*35.-time*2.)),3.));
 if(variant==5)c=vec3(step(0.,sin(time*14.)))*color;
 if(variant==6){float x=sin(time*.7+q.y*9.),y=sin(time*1.1+q.x*12.);c=hsv(theta+time*.1)*exp(-abs(q.x-x*.35+q.y-y*.35)*30.);}
 if(variant==7)c=hsv(p.x*80.+sin(p.y*12.+time)*2.)*(.25+.75*abs(sin(p.x*220.+time)));
 if(variant==8)c=vec3(n);
 return c*mix(vec3(1),color,.25);}

 return texture2D(sceneImage,vec2(p.x,1.-p.y)).rgb;
}
vec3 illuminated(vec2 p){vec3 b=preRendered==1?texture2D(bloomImage,p).rgb:vec3(0);if(category==7){if(variant==4)b=max(b-vec3(.82),vec3(0));else if(variant!=5&&hasImage==0){vec3 bg=(variant==6||variant==11)?vec3(0):color;vec3 difference=abs(b-bg),span=abs(textTint-bg);float strength=clamp(max(difference.r,max(difference.g,difference.b))/max(.001,max(span.r,max(span.g,span.b))),0.,1.);b=textTint*strength;}}return base(p)+b*glow*2.5;}
// Native image FX run in ReferenceFx between the bloom and presentation passes.
vec3 lens(vec2 p){return preEffects==1?texture2D(sceneImage,p).rgb:illuminated(p);}
float lineDistance(vec2 p,vec2 a,vec2 b){vec2 d=b-a;return length(p-a-d*clamp(dot(p-a,d)/max(dot(d,d),.000001),0.,1.));}
float digit(vec2 p,int number){int bits=63;if(number==1)bits=6;if(number==2)bits=91;if(number==3)bits=79;if(number==4)bits=102;if(number==5)bits=109;if(number==6)bits=125;if(number==7)bits=7;if(number==8)bits=127;if(number==9)bits=111;float ink=0.;for(int s=0;s<7;s++){if((bits&(1<<s))==0)continue;vec2 a=vec2(.15,.1),b=vec2(.85,.1);if(s==1){a=vec2(.85,.1);b=vec2(.85,.5);}if(s==2){a=vec2(.85,.5);b=vec2(.85,.9);}if(s==3){a=vec2(.15,.9);b=vec2(.85,.9);}if(s==4){a=vec2(.15,.5);b=vec2(.15,.9);}if(s==5){a=vec2(.15,.1);b=vec2(.15,.5);}if(s==6){a=vec2(.15,.5);b=vec2(.85,.5);}ink=max(ink,1.-smoothstep(.07,.1,lineDistance(p,a,b)));}return ink;}
void main(){vec3 c=lens(uv);for(int i=0;i<16;i++){if(i>=boxCount)break;vec4 b=boxes[i];vec2 centre=(b.xy+b.zw)*.5;vec2 dist=abs(uv-centre)-(b.zw-b.xy)*.5;float edge=step(max(dist.x,dist.y),.003)*step(-.002,max(dist.x,dist.y));if(trackType==2){float corners=abs(dist.x)<.003?1.-step(.025,min(abs(uv.y-b.y),abs(uv.y-b.w))):1.-step(.025,min(abs(uv.x-b.x),abs(uv.x-b.z)));edge*=corners;}if(trackType==3)edge=exp(-abs(max(dist.x,dist.y))*230.)*step(max(dist.x,dist.y),.03);vec3 tint=trackType==4?vec3(0,1,.3):trackType==1?vec3(1):color;c+=tint*edge*trackAmount;if(trackType==1||trackType==4){if(i>0){vec4 prev=boxes[i-1];vec2 pc=(prev.xy+prev.zw)*.5;c+=tint*.6*(1.-smoothstep(.001,.002,lineDistance(uv,centre,pc)))*trackAmount;}vec2 label=(uv-vec2(b.x,b.w))*resolution/vec2(6.,9.);if(label.y>0.&&label.y<1.&&label.x>0.&&label.x<6.){int place=int(floor(label.x));int value=place<2?i+1:place<4?int(centre.x*99.):int(centre.y*99.);int number=place%2==0?value/10:value%10;c+=tint*digit(fract(label),number)*trackAmount;}if(trackType==4&&uv.x>b.x&&uv.x<b.z&&uv.y>b.y&&uv.y<b.w)c+=vec3(0,.04,0)*trackAmount;}}gl_FragColor=vec4(clamp(c,0.,1.),1.);}
)GL";
Canvas::Canvas() {
  // Native Shatter initializes 40 sites using five MSVC rand() draws each.
  // A renderer-local generator avoids changing unrelated FX random streams.
  unsigned seed=1;
  auto random=[&](){seed=seed*214013u+2531011u;return (seed>>16)&32767u;};
  for(int k=0;k<40;++k) {
    voronoiSeeds[(size_t)k*4]=(random()%10000)/10000.0f;
    voronoiSeeds[(size_t)k*4+1]=(random()%10000)/10000.0f;
    voronoiSeeds[(size_t)k*4+2]=((random()%10000)/10000.0f-.5f)*.3f;
    voronoiSeeds[(size_t)k*4+3]=((random()%10000)/10000.0f-.5f)*.3f;
    voronoiLabels[(size_t)k]=random()%2;
  }
  context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
  context.setRenderer(this);
  context.setContinuousRepainting(false);
  context.setComponentPaintingEnabled(false);
  context.attachTo(*this);
}
Canvas::~Canvas() { context.detach(); }
void Canvas::attachUi(juce::Component &component) {
  context.detach();
  // JUCE composites the whole editor after renderOpenGL. Readback inside
  // renderOpenGL still contains only the scene, before any controls are drawn.
  context.setComponentPaintingEnabled(true);
  context.attachTo(component);
}
void Canvas::attachScene() {
  context.detach();context.setComponentPaintingEnabled(false);context.attachTo(*this);
}
void Canvas::capturePresented(std::function<void(juce::Image)> done) {
#if JUCE_WINDOWS
  context.executeOnGLThread(
      [this, done = std::move(done)](juce::OpenGLContext &ctx) {
        const auto scale = ctx.getRenderingScale();
        const int width = juce::roundToInt(getWidth() * scale),
                  height = juce::roundToInt(getHeight() * scale);
        juce::Image image(juce::Image::ARGB, width, height, false,
                          juce::SoftwareImageType());
        glBindFramebuffer(GL_FRAMEBUFFER, ctx.getFrameBufferID());
        GLint previous = GL_BACK;
        glGetIntegerv(GL_READ_BUFFER, &previous);
        glReadBuffer(GL_FRONT);
        {
          juce::Image::BitmapData pixels(image,
                                         juce::Image::BitmapData::writeOnly);
          glReadPixels(0, 0, width, height, nativeArgbFormat, GL_UNSIGNED_BYTE,
                       pixels.data);
          for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
              pixels.getPixelPointer(x, y)[juce::PixelARGB::indexA] = 255;
        }
        const auto error = glGetError();
        glReadBuffer((GLenum)previous);
        done(error == GL_NO_ERROR ? image : juce::Image());
      },
      false);
#else
  done({});
#endif
}
void Canvas::present(juce::Image im, const SceneState &s) {
  {
    juce::ScopedLock l(lock);
    image = std::move(im);
    scene = s;
  }
  context.triggerRepaint();
}
juce::String Canvas::error() const {
  juce::ScopedLock l(lock);
  return shaderError;
}
void Canvas::paint(juce::Graphics &g) {
  auto e = error();
  if (e.isNotEmpty()) {
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.drawFittedText(e, getLocalBounds().reduced(20),
                     juce::Justification::centred, 8);
  }
}
void Canvas::newOpenGLContextCreated() {
  shader = std::make_unique<juce::OpenGLShaderProgram>(context);
  if (!(shader->addVertexShader(
            juce::OpenGLHelpers::translateVertexShaderToV3(vertex)) &&
        shader->addFragmentShader(
            juce::OpenGLHelpers::translateFragmentShaderToV3(fragment)) &&
        shader->link())) {
    juce::ScopedLock l(lock);
    shaderError = shader->getLastError();
    shader.reset();
    return;
  }
  float points[] = {-1, -1, 1, -1, -1, 1, 1, 1};
  blurShader = std::make_unique<juce::OpenGLShaderProgram>(context);
  if (!(blurShader->addVertexShader(
            juce::OpenGLHelpers::translateVertexShaderToV3(vertex)) &&
        blurShader->addFragmentShader(
            juce::OpenGLHelpers::translateFragmentShaderToV3(nativeShader::gaussianBlur)) &&
        blurShader->link())) {
    juce::ScopedLock guard(lock);
    shaderError = blurShader->getLastError();
    shader.reset();
    return;
  }
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
  glGenTextures(1, &sourceTexture);
  glGenTextures(5, stageTextures.data());
  glGenFramebuffers(5, stageFbos.data());
  glGenTextures(1, &smallTexture);
  glBindTexture(GL_TEXTURE_2D, smallTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 36, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         smallTexture, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, context.getFrameBufferID());
}
void Canvas::renderOpenGL() {
  if (paused.load() || !shader)
    return;
  SceneState s;
  juce::Image im;
  {
    juce::ScopedLock l(lock);
    s = scene;
    im = image;
  }
  if (!im.isValid())
    return;
  int requestedTrackWidth =
      32 * (1 + decodeChoice(s.get("trackingResolution"), 4));
  if (trackWidth != requestedTrackWidth) {
    trackWidth = requestedTrackWidth;
    trackHeight = trackWidth * 9 / 16;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, smallTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, trackWidth, trackHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
  }
  auto now = juce::Time::getMillisecondCounterHiRes();
  if (lastFrame > 0)
    frameMilliseconds.store(now - lastFrame);
  lastFrame = now;
  int vsync = s.get("vsync") >= 64 ? 1 : 0;
  if (vsync != lastVsync) {
    context.setSwapInterval(vsync);
    lastVsync = vsync;
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sourceTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  s.get("antialias") >= 64 ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  s.get("antialias") >= 64 ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  // Reuse the display texture and upload a native frame only when it changes.
  // UI-only repaints must not transfer the same full-resolution image again.
  if(!s.nativeFrame || im!=uploadedImage) {
    juce::Image::BitmapData pixels(im,juce::Image::BitmapData::readOnly);
    if(sourceWidth!=im.getWidth() || sourceHeight!=im.getHeight()) {
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,im.getWidth(),im.getHeight(),0,
                   nativeArgbFormat,GL_UNSIGNED_BYTE,pixels.data);
      sourceWidth=im.getWidth();sourceHeight=im.getHeight();
    } else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,sourceWidth,sourceHeight,
                          nativeArgbFormat,GL_UNSIGNED_BYTE,pixels.data);
    uploadedImage=im;
  }
  const auto amounts = referenceFx.effectiveAmounts(s);
  const bool activeFx = std::any_of(amounts.begin(), amounts.end(), [](float a) { return a > .005f; });
  const int requestedEffectW = activeFx ? ReferenceFx::width : im.getWidth();
  const int requestedEffectH = activeFx ? ReferenceFx::height : im.getHeight();
  if (!s.nativeFrame && (uploadW != im.getWidth() || uploadH != im.getHeight() ||
      effectW != requestedEffectW || effectH != requestedEffectH)) {
    uploadW = im.getWidth(); uploadH = im.getHeight();
    effectW = requestedEffectW; effectH = requestedEffectH;
    for (int k = 0; k < 5; ++k) {
      const int w = k < 3 ? uploadW : effectW, h = k < 3 ? uploadH : effectH;
      glBindTexture(GL_TEXTURE_2D, stageTextures[(size_t)k]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glBindFramebuffer(GL_FRAMEBUFFER, stageFbos[(size_t)k]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, stageTextures[(size_t)k], 0);
    }
  }
  glBindTexture(GL_TEXTURE_2D, sourceTexture);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  shader->use();
  auto p = shader->getProgramID();
  auto u = [p](const char *n) { return glGetUniformLocation(p, n); };
  auto f = [&](const char *n, float v) { glUniform1f(u(n), v); };
  auto i = [&](const char *n, int v) { glUniform1i(u(n), v); };
  i("preEffects", 0);
  i("sceneImage", 0);
  i("bloomImage", 1);
  i("preRendered", 0);
  i("category", s.category());
  i("variant", s.variant());
  i("tracing", s.tracing ? 1 : 0);
  i("hasImage", s.hasImage ? 1 : 0);
  f("time", (float)s.time * (s.get("speed") / 64.0f));
  f("nativeTime",(float)s.time*1.32f);
  f("nativeIntensity",s.get("int")/127.0f);
  f("nativeSpeed",s.get("speed")/127.0f);
  glUniform4f(u("nativeBands"),s.nativeSignal[0],s.nativeSignal[1],s.nativeSignal[2],s.nativeSignal[3]);
  f("nativeOnset",s.nativeSignal[4]);
  i("secondaryPalette",s.secondaryPalette?1:0);
  glUniform4fv(u("voronoiSeeds"),40,voronoiSeeds.data());
  glUniform1iv(u("voronoiLabels"),40,voronoiLabels.data());
  f("amp", s.amplitude());
  f("bass", s.audio.bass * inputGain(s.get("in")));
  f("glow",
    s.get("glow") / 127.0f * (s.get("glowReact") >= 64 ? s.amplitude() : 1));
  glUniform2f(u("resolution"), (float)im.getWidth(), (float)im.getHeight());
  for (auto pair : {std::pair{"color", "color"}, std::pair{"color2", "color2"},
                    std::pair{"textTint", "textColor"}}) {
    auto c = s.colour(pair.second,
                     s.category() == 7 && juce::String(pair.second) == "color");
    glUniform3f(u(pair.first), c.getFloatRed(), c.getFloatGreen(),
                c.getFloatBlue());
  }
  i("trackType", decodeChoice(s.get("trackType"), 5));
  f("trackAmount", 0);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  auto attr = (unsigned)glGetAttribLocation(p, "position");
  glEnableVertexAttribArray(attr);
  glVertexAttribPointer(attr, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  i("boxCount", 0);
  // Vel Engine has already applied every visual effect. Present its
  // completed image directly rather than running five redundant staging passes.
  if (!s.nativeFrame) {
  // First render the base scene; blur it horizontally/vertically, then run the
  // FX using those textures.
  float desiredGlow = 0;
  glGetUniformfv(p, u("glow"), &desiredGlow);
  f("glow", 0);
  i("boxCount", 0);
  glBindFramebuffer(GL_FRAMEBUFFER, stageFbos[0]);
  glViewport(0, 0, uploadW, uploadH);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  blurShader->use();
  auto blurProgram = blurShader->getProgramID();
  auto bu = [&](const char *name) {
    return glGetUniformLocation(blurProgram, name);
  };
  auto ba = (unsigned)glGetAttribLocation(blurProgram, "position");
  glEnableVertexAttribArray(ba);
  glVertexAttribPointer(ba, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glUniform1i(bu("tex"), 0);
  glUniform1f(bu("radius"),desiredGlow*2.5f*(.5f+s.get("bloomQuality")/128.0f));
  for (int pass = 1; pass <= 2; ++pass) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stageTextures[(size_t)pass - 1]);
    glBindFramebuffer(GL_FRAMEBUFFER, stageFbos[(size_t)pass]);
    glUniform2f(bu("dir"),pass==1?1.0f/uploadW:0,pass==2?1.0f/uploadH:0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  shader->use();
  glEnableVertexAttribArray(attr);
  glVertexAttribPointer(attr, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, stageTextures[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  s.get("antialias") >= 64 ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  s.get("antialias") >= 64 ? GL_LINEAR : GL_NEAREST);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, stageTextures[2]);
  i("preRendered", 1);
  f("glow", desiredGlow);
  // Recovered order: Dither -> Texture -> Track -> Glass.
  // CPU operations use the original bounded 600x500 RGBA buffer only while FX
  // are enabled. Full-resolution base rendering is retained when bypassed.
  f("trackAmount", 0); i("boxCount", 0);
  glUniform2f(u("resolution"), (float)effectW, (float)effectH);
  glBindFramebuffer(GL_FRAMEBUFFER, stageFbos[3]);
  glViewport(0, 0, effectW, effectH);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (amounts[0] > .005f || amounts[1] > .005f) {
    referencePixels.resize((size_t)ReferenceFx::width * ReferenceFx::height * 4);
    glReadPixels(0, 0, effectW, effectH, GL_RGBA, GL_UNSIGNED_BYTE, referencePixels.data());
    referenceFx.ditherAndTexture(referencePixels, s, amounts[1], amounts[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stageTextures[3]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, effectW, effectH, GL_RGBA, GL_UNSIGNED_BYTE, referencePixels.data());
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, stageTextures[3]);
  i("preEffects", 1);
  f("trackAmount", amounts[2]);
  if (s.get("track") > 0 &&
      ++frames % juce::jmax(1, 12 - s.get("trackingRate") / 12) == 0) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, trackWidth, trackHeight);
    i("boxCount", 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    std::array<unsigned char, 128 * 72 * 4> map{};
    glReadPixels(0, 0, trackWidth, trackHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 map.data());
    std::array<bool, 128 * 72> used{};
    boxCount = 0;
    for (int y = 0; y < trackHeight && boxCount < 16; ++y)
      for (int x = 0; x < trackWidth && boxCount < 16; ++x) {
        int start = y * trackWidth + x;
        if (used[(size_t)start] ||
            std::max({map[(size_t)start * 4], map[(size_t)start * 4 + 1],
                      map[(size_t)start * 4 + 2]}) < 100)
          continue;
        std::array<int, 128 * 72> queue{};
        int head = 0, tail = 1, count = 0, minx = x, maxx = x, miny = y,
            maxy = y;
        queue[0] = start;
        used[(size_t)start] = true;
        while (head < tail) {
          int cell = queue[(size_t)head++], xx = cell % trackWidth,
              yy = cell / trackWidth;
          ++count;
          minx = std::min(minx, xx);
          maxx = std::max(maxx, xx);
          miny = std::min(miny, yy);
          maxy = std::max(maxy, yy);
          for (int n :
               {cell - 1, cell + 1, cell - trackWidth, cell + trackWidth}) {
            if (n < 0 || n >= trackWidth * trackHeight ||
                std::abs(n % trackWidth - xx) + std::abs(n / trackWidth - yy) !=
                    1 ||
                used[(size_t)n])
              continue;
            if (std::max({map[(size_t)n * 4], map[(size_t)n * 4 + 1],
                          map[(size_t)n * 4 + 2]}) < 100)
              continue;
            used[(size_t)n] = true;
            queue[(size_t)tail++] = n;
          }
        }
        if (count >= 2 && count < trackWidth * trackHeight * .78) {
          boxes[(size_t)boxCount * 4] = minx / (float)trackWidth;
          boxes[(size_t)boxCount * 4 + 1] = miny / (float)trackHeight;
          boxes[(size_t)boxCount * 4 + 2] = (maxx + 1) / (float)trackWidth;
          boxes[(size_t)boxCount * 4 + 3] = (maxy + 1) / (float)trackHeight;
          ++boxCount;
        }
      }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, stageFbos[4]);
  glViewport(0, 0, effectW, effectH);
  i("boxCount", amounts[2] > .005f ? boxCount : 0);
  glUniform4fv(u("boxes"), 16, boxes.data());
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (amounts[3] > .005f) {
    referencePixels.resize((size_t)ReferenceFx::width * ReferenceFx::height * 4);
    glReadPixels(0, 0, effectW, effectH, GL_RGBA, GL_UNSIGNED_BYTE, referencePixels.data());
    referenceFx.glass(referencePixels, s, amounts[3]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, stageTextures[4]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, effectW, effectH, GL_RGBA, GL_UNSIGNED_BYTE, referencePixels.data());
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, stageTextures[4]);
  f("trackAmount", 0); i("boxCount", 0);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, context.getFrameBufferID());
  auto rs = context.getRenderingScale();
  int width = juce::roundToInt(getWidth() * rs),
      height = juce::roundToInt(getHeight() * rs);
  glViewport(0, 0, width, height);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  if (capture.load() && onCapture) {
    juce::Image out(juce::Image::ARGB, width, height, false,
                    juce::SoftwareImageType());
    {
      juce::Image::BitmapData b(out, juce::Image::BitmapData::writeOnly);
      glReadPixels(0, 0, width, height, nativeArgbFormat, GL_UNSIGNED_BYTE,
                   b.data);
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
          b.getPixelPointer(x, y)[juce::PixelARGB::indexA] = 255;
      auto errorCode = glGetError();
      if (errorCode != GL_NO_ERROR) {
        juce::ScopedLock guard(lock);
        shaderError = "OpenGL error " + juce::String((int)errorCode);
      }
    }
    onCapture(out, s.captureTag);
  }
  glDisableVertexAttribArray(attr);
  glBindVertexArray(0);
}
void Canvas::openGLContextClosing() {
  shader.reset();
  blurShader.reset();
  glDeleteTextures(5, stageTextures.data());
  glDeleteFramebuffers(5, stageFbos.data());
  stageTextures = {};
  stageFbos = {};
  uploadW = uploadH = effectW = effectH = 0;
  sourceWidth=sourceHeight=0;uploadedImage={};
  referenceFx.reset(); referencePixels.clear();
  if (vbo)
    glDeleteBuffers(1, &vbo);
  if (vao)
    glDeleteVertexArrays(1, &vao);
  if (sourceTexture)
    glDeleteTextures(1, &sourceTexture);
  if (smallTexture)
    glDeleteTextures(1, &smallTexture);
  if (fbo)
    glDeleteFramebuffers(1, &fbo);
  vbo = vao = sourceTexture = smallTexture = fbo = 0;
  trackWidth = 64;
  trackHeight = 36;
}
} // namespace kone::grid
