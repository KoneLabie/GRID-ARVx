#include "NativeGeometry.h"
namespace kone::grid {
namespace {
constexpr float pi=juce::MathConstants<float>::pi;
float sample(const SceneState& s,int index) {
  return s.audio.scope[(size_t)(index & 2047)];
}
float gain(const SceneState& s) {
  // Preserve the native scope/Plasma gain law; other modes use a different
  // offset. CC remains the public 0–127 representation.
  return std::pow(10.0f,((s.get("in")/127.0f-1)*40+9)/20);
}
float intensity(const SceneState& s) {
  return std::pow((s.get("int")/127.0f)/.49f,2.8f)*.98f+.02f;
}
float energy(const SceneState& s) {
  return s.audio.averageMagnitude*gain(s);
}
juce::Path curve(int points,const std::function<juce::Point<float>(int)>& point) {
  juce::Path p;
  for(int i=0;i<points;++i) {auto q=point(i);if(i==0)p.startNewSubPath(q);else p.lineTo(q);}
  return p;
}
void stroke(juce::Graphics& g,const SceneState& s,const juce::Path& p,float opacity,
            bool plasma=false,bool secondary=false) {
  const auto ink=s.colour(secondary?"color2":"color");
  const float glow=s.get("glow")/127.0f;
  const float scale=plasma?std::pow(s.height/600.0f,1.4f):s.width/600.0f;
  const float widths[]{22,10,4.5f,1.6f,.5f};
  const float shades[]{.28f,.55f,1,1,1};
  const float alpha[]{.55f,.5f,.7f,.85f,.9f};
  for(int i=0;i<5;++i) {
    auto c=i==3?ink.interpolatedWith(juce::Colours::white,.35f):i==4?juce::Colours::white:ink.withMultipliedBrightness(shades[i]);
    const float amount=(i<3?(plasma?glow:glow+1):1)*alpha[i]*opacity;
    g.setColour(c.withAlpha(juce::jlimit(0.0f,1.0f,amount)));
    const float width=juce::jmax(1.0f,widths[i]*scale*(plasma&&i<3?glow:1));
    g.strokePath(p,juce::PathStrokeType(width,juce::PathStrokeType::curved));
  }
}
void dot(juce::Graphics& g,const SceneState& s,float x,float y,float radius,float opacity) {
  g.setColour(s.colour("color").withAlpha(juce::jlimit(0.0f,1.0f,opacity)));
  g.fillEllipse(x-radius,y-radius,radius*2,radius*2);
}
bool oscilloscope(juce::Graphics& g,const SceneState& s) {
  const int v=s.variant();if(v>=9)return false;
  const float w=(float)s.width,h=(float)s.height,m=juce::jmin(w,h),cx=w*.5f,cy=h*.5f;
  const float in=gain(s),it=intensity(s),a=energy(s)*it,t=(float)s.time*1.32f;
  if(v==0||v==8) {
    stroke(g,s,curve(256,[&](int i){return juce::Point<float>(i*w/256,cy-sample(s,i*8)*in*it*h*.75f);}),3);
  } else if(v==1) {
    // The native Lisaj plots the mono history against a 512-sample delay.
    // It is a point cloud, rather than a line joining stereo channels.
    for(int i=0;i<2047;i+=3)dot(g,s,cx+sample(s,i)*in*it*m*.76f,cy+sample(s,i+512)*in*it*m*.76f,1,.85f);
  } else if(v==2) {
    const float r=m*.28f;
    for(int k=0;k<3;++k)stroke(g,s,curve(2048,[&](int i){
      const float q=i/2048.0f*2*pi+k/3.0f*2*pi+t*.3f;
      const float rr=r+sample(s,i)*in*it*r*1.1f+a*r*.25f;
      return juce::Point<float>(cx+std::cos(q)*rr,cy+std::sin(q)*rr);
    }),k?1.05f:3);
    dot(g,s,cx,cy,r*(.08f+a*.18f),.7f);
  } else if(v==3) {
    for(int k=0;k<14;++k) {
      const float phase=std::fmod(t*.18f+k/14.0f,1.0f),r=m*(phase*.54f+.02f);
      stroke(g,s,curve(65,[&](int i){const float u=i/64.0f,q=u*2*pi+phase*pi*.5f;
        const float rr=r+sample(s,(int)(u*2048))*in*it*r*.22f;
        return juce::Point<float>(cx+std::cos(q)*rr,cy+std::sin(q)*rr*.55f);
      }),(a*.4f+.6f)*phase*3);
    }
  } else if(v==4) {
    for(int k=0;k<7;++k)stroke(g,s,curve(121,[&](int i){
      const float u=i/120.0f,q=u*pi;
      const float y=h*(k/6.0f*.7f+.15f)+std::sin(q*(k+3)+t*(k*.18f+.4f))*h*(a*.12f+.04f)
        +std::sin(q*(k*1.3f+5)+t*(k*.22f+.7f))*h*.02f
        +sample(s,(int)(u*2048))*in*it*h*.04f;
      return juce::Point<float>(u*w,y);
    }),juce::jmax(.2f,a*.7f+1-std::abs(k/6.0f-.5f)*.2f)*2);
  } else if(v==5) {
    for(int k=0;k<4;++k)stroke(g,s,curve(683,[&](int i){
      const float u=i*3/2048.0f,sx=k==1||k==3?-1.0f:1.0f,sy=k==2||k==3?-1.0f:1.0f;
      return juce::Point<float>(cx+(u-.5f)*sx*w,cy+sy*sample(s,i*3)*in*it*h*.41f);
    }),k?1.35f:3);
  } else if(v==6) {
    for(int k=0;k<2;++k)stroke(g,s,curve(181,[&](int i){
      const float u=i/180.0f,q=u*pi*5+t*.9f+k*pi;
      return juce::Point<float>(u*w,cy+std::sin(q)*h*.32f*(1+a*.4f)+sample(s,(int)(u*2048))*in*it*h*.08f);
    }),k?1.2f:2);
    for(int k=0;k<18;++k){const float u=k/18.0f,q=u*pi*5+t*.9f,c=std::abs(std::cos(q));if(c<=.15f)continue;
      juce::Path p;p.startNewSubPath(u*w,cy+std::sin(q)*h*.32f*(1+a*.4f));p.lineTo(u*w,cy+std::sin(q+pi)*h*.32f*(1+a*.4f));stroke(g,s,p,c*.8f);
    }
  } else if(v==7) {
    const int points=juce::jmax(2,s.width/2+1);
    for(int k=0;k<6;++k)stroke(g,s,curve(points,[&](int i){
      const float u=i/(float)(points-1),q=u*pi,phase=(k*.7f+1)*t;
      const float y=h*(k/5.0f*.7f+.15f)+std::sin(q*3+phase*1.8f)*a*h*.03f
        +std::sin(q*2+phase*1.1f)*a*h*.06f+std::sin(q)*sample(s,(int)(u*2048))*in*it*h*.18f;
      return juce::Point<float>(u*w,y);
    }),(k/5.0f*.7f+.3f)*3);
  }
  return true;
}
bool plasma(juce::Graphics& g,const SceneState& s) {
  const float w=(float)s.width,h=(float)s.height,cx=w*.5f,cy=h*.5f,aspect=juce::jlimit(1.0f,2.0f,w/h);
  const int v=s.variant();
  constexpr float weights[]{.53f,.53f,.69f,.63f,.72f,.44f,.72f};
  const float level=weights[v]*s.get("int")/127.0f;
  const float it=std::pow(level/.6f,2.8f)*.98f+.02f;
  const float a=energy(s)*120*it,spread=(level*17+4)*a,boost=level*.36f+.4f,t=(float)s.time*1.32f;
  auto draw=[&](juce::Path p,float opacity,int k){stroke(g,s,p,opacity,true,s.secondaryPalette&&k%2!=0&&a>.06f);};
  if(v==0)for(int k=0;k<4;++k) {
    const float row=(k/3.0f*.7f+.15f)*h,mag=h*.18f*aspect*spread*(k/3.0f*.4f+.6f)*boost+h*.006f*aspect;
    for(int mirror=0;mirror<4;++mirror) draw(curve(256,[&](int i){
      const float u=i*2/511.0f,q=u*pi*(k*.8f+2),sx=mirror==1||mirror==3?-1.0f:1.0f,sy=mirror>=2?-1.0f:1.0f;
      return juce::Point<float>(cx+(u-.5f)*sx*w*1.02f,row+sy*(std::sin(q+t*(k*.18f+.55f))+std::sin(q*1.6f+t*(k*.12f+.28f))*.3f)*mag);
    }),mirror?.55f:1,k);
  } else if(v==1) {
    const float mag=h*(spread*.22f*boost+.06f);
    for(int k=0;k<2;++k)draw(curve(201,[&](int i){const float u=i/200.0f,q=u*pi*5+t*.9f+k*pi;
      return juce::Point<float>(u*w,cy+std::sin(q)*mag+std::sin(q*.5f+t*.3f)*mag*.25f);
    }),1,k);
    for(int i=0;i<16;++i){const float u=i/16.0f,q=u*pi*5+t*.9f,c=std::abs(std::cos(q));if(c<=.2f)continue;
      juce::Path p;p.startNewSubPath(u*w,cy+std::sin(q)*mag);p.lineTo(u*w,cy+std::sin(q+pi)*mag);draw(p,c*.4f,0);
    }
  } else if(v==2)for(int k=0;k<5;++k)draw(curve(301,[&](int i){
    const float q=i/300.0f*2*pi+t*(k*.05f+.3f)+k/5.0f*pi*.5f,r=k*.15f+.4f,mod=spread*.35f*boost;
    return juce::Point<float>(cx+std::sin(q)*w*.38f*(r+std::sin(q*3+t)*mod),cy+std::sin(q*2+t*.1f)*h*.36f*(r+std::cos(q*2+t)*mod*.6f));
  }),k*.05f+1.05f,k);
  else if(v==3)for(int k=0;k<7;++k){const float phase=std::fmod(t*.06f+k/7.0f,1.0f),r=juce::jmin(w,h)*phase*.5f;
    draw(curve(181,[&](int i){const float q=i/180.0f*2*pi,rr=r+std::sin(q*(k+3)+t*1.5f)*r*spread*.35f*boost;
      return juce::Point<float>(cx+std::cos(q)*rr,cy+std::sin(q)*rr*.7f);
    }),.7f,k);
  } else if(v==4)for(int k=0;k<5;++k){
    const float phase=t*(k*.12f+.5f),row=(k/4.0f*.7f+.15f)*h,col=(k/4.0f*.7f+.15f)*w;
    const float ym=h*(spread*.12f*boost+.005f)*aspect,xm=w*(spread*.1f*boost+.005f);
    draw(curve(201,[&](int i){float u=i/200.0f;return juce::Point<float>(u*w,row+std::sin(u*pi*4+phase)*ym);}),.85f,k);
    draw(curve(201,[&](int i){float u=i/200.0f;return juce::Point<float>(col+std::sin(u*pi*4+phase)*xm,u*h);}),.75f,k);
  } else if(v==5)for(int k=0;k<4;++k){const float u0=k/3.0f,phase=t*(k*.15f+.4f),mag=h*(spread*(u0*.08f+.14f)*boost+.005f)*aspect;
    draw(curve(251,[&](int i){float u=i/250.0f,q=u*pi;return juce::Point<float>(u*w,cy+(u0-.5f)*h*.35f
      +std::sin(q*3+phase)*mag+std::sin(q*5.3f+phase*1.4f)*mag*.4f+std::sin(q*1.5f+t*.2f)*mag*.25f);
    }),.9f-u0*.1f,k);
  } else if(v==6)for(int k=0;k<4;++k){const float r=juce::jmin(w,h)*.28f,mod=r*spread*.4f*boost;
    draw(curve(301,[&](int i){const float q=i/300.0f*2*pi,phase=q*2+t*.2f,rr=r*(std::cos(q*3+t*.3f+k*pi*.5f)*.55f+.45f);
      return juce::Point<float>(cx+std::cos(phase)*rr+std::sin(q*5+t)*mod*.3f,cy+std::sin(phase)*rr*.65f+std::cos(q*3+t*1.1f)*mod*.3f);
    }),.9f,k);
  }
  return true;
}
}
bool drawNativeGeometry(juce::Graphics& g,const SceneState& s) {
  if(s.category()==0)return oscilloscope(g,s);
  if(s.category()==8)return plasma(g,s);
  return false;
}
}
