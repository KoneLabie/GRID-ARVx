#include "Core/Controls.h"
#include "Core/Graphics.h"
#include "Core/Cycles.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace kone::grid;
static void check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
int main() {
  try {
    BeatCycle cycle;
    cycle.tick(false, 2, 7);
    check(cycle.tick(true, 2, 1) == 0 && cycle.tick(true, 2, 1) == 0 &&
              cycle.tick(true, 2, 1) == 1,
          "Cycle did not wait three complete beats after enabling");
    check(cycle.tick(true, 2, 6) == 2,
          "Missed frame did not retain beat count");
    cycle.tick(false, 2, 0);
    check(cycle.tick(true, 2, 2) == 0 && cycle.tick(true, 0, 1) == 1,
          "Cycle interval change did not reset phase");
    check(globalControl("lock") && globalControl("presetCycle") &&
              globalControl("muteInput") && !globalControl("texture"),
          "Preset recall does not isolate global lock / sequencing / audio "
          "settings");
    for (int count = 2; count <= 16; ++count)
      for (int i = 0; i < count; ++i)
        check(decodeChoice(encodeChoice(i, count), count) == i,
              "Choice round trip failed");
    for (auto c : controls)
      check(c.initial >= 0 && c.initial <= 127, "Default outside CC spectrum");
    check(globalControl("renderResolution") && globalControl("pauseMenuRendering"),
          "Visual presets can overwrite resolution or menu-render preferences");
    check(renderResolution(0).width==854 && renderResolution(0).height==480 &&
          renderResolution(127).width==3840 && renderResolution(127).height==2160,
          "Resolution selector does not span 480p through UHD 4K");
    Binding knob{indexOf("texture"), -1, Hardware::knob};
    Binding toggle{indexOf("textureReact"), -1, Hardware::button};
    Binding profile{indexOf("textureType"), encodeChoice(2, 4),
                    Hardware::button};
    check(compatible(knob) && compatible(toggle) && compatible(profile),
          "Valid binding rejected");
    knob.hardware = Hardware::button;
    check(!compatible(knob), "Button bound to knob");
    toggle.hardware = Hardware::knob;
    check(!compatible(toggle), "Knob bound to button");
    profile.hardware = Hardware::knob;
    check(!compatible(profile), "Knob bound to profile trigger");
    Binding original{indexOf("glassType"), 127, Hardware::button};
    auto restored = unpack(pack(original));
    check(restored.target == original.target && restored.fixed == 127 &&
              restored.hardware == Hardware::button,
          "Binding serialization corrupted");
    check(std::abs(inputGain(64) - 1) < .0001f && inputGain(0) < 1 &&
              inputGain(127) > 1,
          "Input gain mapping failed");
    MidiEvents queue;
    for (int i = 0; i < 255; ++i)
      check(queue.push({1, i % 128, i % 128}), "Queue filled early");
    check(!queue.push({}), "Full queue failed to reject");
    MidiEvent e;
    for (int i = 0; i < 255; ++i) {
      check(queue.pop(e), "Queue lost event");
      check(e.cc == i % 128, "Queue reordered event");
    }
    check(!queue.pop(e), "Empty queue returned event");
    std::atomic<bool> done{false};
    std::thread producer([&] {
      for (int i = 0; i < 100000; ++i)
        while (!queue.push({1, i % 128, i % 128}))
          std::this_thread::yield();
      done = true;
    });
    int received = 0;
    while (!done || received < 100000) {
      if (queue.pop(e)) {
        check(e.value == received % 128, "Concurrent queue event corrupted");
        ++received;
      } else
        std::this_thread::yield();
    }
    producer.join();
    std::cout << "PASS: CC ranges, discrete mapping, strict binding types, "
                 "serialization and concurrent "
                 "MIDI queue\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
