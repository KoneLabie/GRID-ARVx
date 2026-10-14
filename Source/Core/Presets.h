#pragma once
#include "Lyrics.h"
#include "Plugin/Processor.h"
#include "Visual/Scenes.h"
#include <juce_cryptography/juce_cryptography.h>
#if GRID_USE_VEL_ENGINE
#include "Engine/VelEngine.h"
#endif
namespace kone::grid {
class Presets {
public:
  juce::File root;
  explicit Presets(Processor &p, juce::File customRoot = {})
      : processor(p), root(juce::File::getSpecialLocation(
                               juce::File::userApplicationDataDirectory)
                               .getChildFile("KoneLabie/GRID-ARVx/Presets")) {
    if (customRoot != juce::File())
      root = customRoot;
    for (int c = 0; c < 9; ++c) {
      for (auto folder : {"Defaults", "Factory", "User"})
        directory(c, folder).createDirectory();
      auto revisionFile = directory(c, "Defaults").getChildFile("catalog-version.txt");
      const bool upgrading = revisionFile.loadFileAsString().getIntValue() < templateCatalogRevision;
      bool migrated = true;
      if (upgrading) {
        auto old = directory(c, "Defaults").findChildFiles(juce::File::findFiles, false, "*.json");
        auto backup = directory(c, "Previous Defaults").getChildFile("Before catalog "+juce::String(templateCatalogRevision));
        for (const auto& file : old) {
          backup.createDirectory();
          auto target = backup.getNonexistentChildFile(file.getFileNameWithoutExtension(), ".json");
          if (!file.copyFileTo(target) || !file.deleteFile()) migrated = false;
        }
      }
      auto names = templates(c);
      for (int v = 0; v < names.size(); ++v) {
        if (!offeredTemplate(c, v))
          continue;
        auto f = directory(c, "Defaults").getChildFile(names[v] + ".json");
        if (!f.existsAsFile()) {
          auto *o = new juce::DynamicObject;
          o->setProperty("schema", 2);
          o->setProperty("category", categories[c]);
          o->setProperty("name", names[v]);
          o->setProperty("templateRevision", templateCatalogRevision);
          o->setProperty("catalogRevision", templateCatalogRevision);
          o->setProperty("source", "Visual Lab Pro built-in Defaults");
          auto *params = new juce::DynamicObject;
          for (auto control : controls)
            if (!globalControl(control.id))
              params->setProperty(control.id, control.initial);
          params->setProperty("category", encodeChoice(c, 9));
          params->setProperty("variant", encodeChoice(v, 16));
          params->setProperty("color", defaultTemplateColor(c, v));
          params->setProperty("color2", defaultColor2(c));
          params->setProperty("textColor", defaultTextColor(c, v));
          // Defaults reset effects; native mode-specific intensity/speed/glow
          // defaults are applied below rather than cloning a generic setup.
          params->setProperty("glow", c == 0 ? 98 : c == 8 ? 86 : c == 3 ? 85 : c == 7 ? 24 : 0);
          if (c == 4) {
            static constexpr int intensity[]{76,76,70,64,64,98,64,76,70,127,95};
            static constexpr int speed[]{64,64,108,95,64,76,64,76,70,114,76};
            // Setup byte +12 goes to native colorCycle; byte +13 selects
            // the second palette. They were reversed in catalog revision 3.
            static constexpr bool cycle[]{false,false,false,false,true,false,false,false,true,true,false};
            params->setProperty("int", intensity[v]);
            params->setProperty("speed", speed[v]);
            params->setProperty("colorCycle", cycle[v] ? 127 : 0);
          }
          o->setProperty("parameters", juce::var(params));
          if (c==3 || c==4 || c==8) {
            auto* colours=new juce::DynamicObject;
            const auto rgb=nativeDefaultColors(c,v);
            for(int channel=0;channel<2;++channel) {
              const auto id=channel==0?"color":"color2";
              const auto cc=colorCC(rgb[(size_t)channel]);
              params->setProperty(id,cc);
              auto* entry=new juce::DynamicObject;
              entry->setProperty("cc",cc);
              entry->setProperty("argb",(juce::int64)rgb[(size_t)channel].getARGB());
              colours->setProperty(id,juce::var(entry));
            }
            o->setProperty("nativeColors",juce::var(colours));
          }
          o->setProperty("lyrics",
                         juce::Array<juce::var>{juce::var("GRID-ARVx")});
          juce::TemporaryFile generated(f);
          if (!generated.getFile().replaceWithText(juce::JSON::toString(juce::var(o), false)) ||
              !generated.overwriteTargetFileWithTemporary()) migrated = false;
        }
      }
      if (migrated) revisionFile.replaceWithText(juce::String(templateCatalogRevision));
    }
  }
  juce::File directory(int c, const juce::String &group) const {
    return root.getChildFile(categories[juce::jlimit(0, 8, c)])
        .getChildFile(group);
  }
#if GRID_USE_VEL_ENGINE
  juce::Result refreshNativeProfiles(const VelEngine &engine) {
    for(int category=0;category<9;++category)
      for(const auto& file:files(category,"Defaults")) {
        auto data=juce::JSON::parse(file.loadFileAsString());
        auto *object=data.getDynamicObject();
        auto *parameters=data["parameters"].getDynamicObject();
        if(!object || !parameters)continue;
        if(data["nativeEngineProfileVersion"].toString()=="1.2.0" &&
           data["nativeEngineProfile"]["color"].isObject())continue;
        const int variant=decodeChoice((int)data["parameters"]["variant"],16);
        auto profile=engine.defaultProfile(category,variant);
        if(!profile.isObject())return juce::Result::fail("Native Default setup was unavailable.");
        const auto backup=directory(category,"Previous Defaults").getChildFile("Before native engine profiles");
        backup.createDirectory();
        const auto preserved=backup.getChildFile(file.getFileName());
        if(!preserved.existsAsFile() && !file.copyFileTo(preserved))return juce::Result::fail("Unable to back up Default: "+file.getFileName());
        for(const auto* id:{"in","int","speed","glow"})parameters->setProperty(id,profile[id]["cc"]);
        auto *paletteEntry=new juce::DynamicObject();
        paletteEntry->setProperty("cc",parameters->getProperty("color"));
        paletteEntry->setProperty("palette",profile["nativePalette"]);
        profile.getDynamicObject()->setProperty("color",juce::var(paletteEntry));
        profile.getDynamicObject()->setProperty("category",category);
        profile.getDynamicObject()->setProperty("variant",variant);
        if(category==4)parameters->setProperty("colorCycle",(bool)profile["colourCycle"]?127:0);
        object->setProperty("nativeEngineProfile",profile);
        object->setProperty("nativeEngineProfileVersion","1.2.0");
        juce::TemporaryFile updated(file);
        if(!updated.getFile().replaceWithText(juce::JSON::toString(data)) || !updated.overwriteTargetFileWithTemporary())
          return juce::Result::fail("Unable to update native Default: "+file.getFileName());
      }
    return juce::Result::ok();
  }
#endif
  juce::Array<juce::File> files(int c, const juce::String &group) const {
    auto list = directory(c, group).findChildFiles(juce::File::findFiles, false,
                                                   "*.json");
    list.sort();
    if (c == 7 && group == "Defaults")
      for (int i = list.size(); --i >= 0;)
        if (list[i].getFileNameWithoutExtension() == "Normal")
          list.remove(i); // Leave the legacy file intact, including user edits.
    if (group == "Defaults") {
      const auto order = defaultOrder(c);
      std::stable_sort(
          list.begin(), list.end(),
          [&](const juce::File &a, const juce::File &b) {
            int first = order.indexOf(a.getFileNameWithoutExtension(), true);
            int second = order.indexOf(b.getFileNameWithoutExtension(), true);
            if (first < 0)
              first = 1000;
            if (second < 0)
              second = 1000;
            return first != second
                       ? first < second
                       : a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
          });
    }
    return list;
  }
  juce::Array<juce::File> all(int c) const {
    juce::Array<juce::File> a;
    for (auto group : {"Factory", "User", "Defaults"})
      a.addArray(files(c, group));
    return a;
  }
  juce::File userFile(int c, const juce::String &name) const {
    return directory(c, "User").getChildFile(name + ".json");
  }
  static bool validName(const juce::String &n) {
    return n.isNotEmpty() && n.length() <= 64 && n == n.trim() &&
           n == juce::File::createLegalFileName(n) && n != "." && n != ".." &&
           !n.endsWithChar('.') && !n.containsAnyOf("/\\:");
  }
  juce::Result save(int c, const juce::String &name, bool overwrite) {
    if (!validName(name))
      return juce::Result::fail(
          u8"Use a name of 1–64 characters without path characters.");
    auto f = userFile(c, name);
    if (f.existsAsFile() && !overwrite)
      return juce::Result::fail("exists");
    auto *o = new juce::DynamicObject;
    o->setProperty("schema", 2);
    o->setProperty("category", categories[c]);
    o->setProperty("name", name);
    o->setProperty("catalogRevision", templateCatalogRevision);
    auto *params = new juce::DynamicObject;
    for (auto control : controls)
      if (!globalControl(control.id))
        params->setProperty(control.id, processor.value(control.id));
    o->setProperty("parameters", juce::var(params));
    o->setProperty("nativeColors", juce::JSON::parse(processor.getAssetProperty("nativePresetColors")));
    o->setProperty("nativeEngineProfile",juce::JSON::parse(processor.getAssetProperty("nativeEngineValues")));
    o->setProperty("lyrics",
                   juce::JSON::parse(processor.getAssetProperty("lyrics")));
    for (auto key : {"imagePath", "fontPath"}) {
      juce::File source(processor.getAssetProperty(key));
      if (source.existsAsFile()) {
        if (source.getSize() > 32 * 1024 * 1024)
          return juce::Result::fail(
              "Preset assets must be smaller than 32 MB.");
        auto assets = directory(c, "User").getChildFile("Assets");
        assets.createDirectory();
        auto target = assets.getChildFile(
            juce::SHA256(source).toHexString() +
            source.getFileExtension().substring(0, 16));
        if (!target.existsAsFile() && !source.copyFileTo(target))
          return juce::Result::fail("Unable to copy preset asset.");
        o->setProperty(key, target.getRelativePathFrom(f.getParentDirectory()));
      }
    }
    juce::TemporaryFile temp(f);
    if (!temp.getFile().replaceWithText(
            juce::JSON::toString(juce::var(o), false)) ||
        !temp.overwriteTargetFileWithTemporary())
      return juce::Result::fail("Unable to write preset.");
    processor.setAssetProperty("presetName", name);
    processor.setAssetProperty("presetFile", f.getFullPathName());
    return juce::Result::ok();
  }
  juce::Result load(const juce::File &f) {
    if (f.getSize() > 8 * 1024 * 1024)
      return juce::Result::fail("Preset file exceeds 8 MB.");
    auto data = juce::JSON::parse(f.loadFileAsString());
    if (!data.isObject() || (int)data["schema"] != 2)
      return juce::Result::fail("Unsupported preset schema.");
    auto category = categories.indexOf(data["category"].toString());
    if (category < 0)
      return juce::Result::fail("Unknown preset category.");
    if (!data["lyrics"].isVoid() && !validLyrics(data["lyrics"]))
      return juce::Result::fail(
          "Preset lyrics exceed this engine's line or character limit.");
    const bool keepTextSource =
        category == 7 && decodeChoice(processor.value("category"), 9) == 7 &&
        f.getParentDirectory().getFileName() == "Defaults";
    auto *p = data["parameters"].getDynamicObject();
    if (!p)
      return juce::Result::fail("Missing preset parameters.");
    if ((int)data["catalogRevision"] < 3 && p->hasProperty("variant")) {
      const auto value = p->getProperty("variant");
      if ((value.isInt() || value.isInt64()) && (int)value >= 0 && (int)value <= 127)
        p->setProperty("variant", encodeChoice(migrateLegacyVariant(category, decodeChoice((int)value, 16)), 16));
    }
    for (auto control : controls)
      if (p->hasProperty(control.id)) {
        auto v = p->getProperty(control.id);
        if (!(v.isInt() || v.isInt64()) || (int)v < 0 || (int)v > 127)
          return juce::Result::fail(juce::String(u8"Invalid 0–127 value: ") +
                                    juce::String(control.id));
      }
    auto locked = processor.value("lock") >= 64;
    for (auto control : controls) {
      juce::String id(control.id);
      bool keep =
          (keepTextSource && (id.startsWith("lyric") || id == "imageScale" ||
                              id == "textEffect")) ||
          (locked && (id == "int" || id.startsWith("glow") ||
                      id.startsWith("texture") || id.startsWith("dither") ||
                      id.startsWith("track") || id.startsWith("glass")));
      if (!keep && id != "lock" && !globalControl(control.id) &&
          p->hasProperty(control.id))
        processor.set(control.id, (int)p->getProperty(control.id));
    }
    processor.set("category", encodeChoice(category, 9));
    processor.setAssetProperty("presetName", data["name"].toString());
    processor.setAssetProperty("presetFile", f.getFullPathName());
    processor.setAssetProperty("nativePresetColors", juce::JSON::toString(data["nativeColors"]));
    processor.setAssetProperty("nativeEngineValues",juce::JSON::toString(data["nativeEngineProfile"]));
    if (!keepTextSource && data["lyrics"].isArray())
      processor.setAssetProperty("lyrics",
                                 juce::JSON::toString(data["lyrics"]));
    for (auto key : {"imagePath", "fontPath"}) {
      if (keepTextSource)
        continue;
      auto path = data[key].toString();
      processor.setAssetProperty(
          key,
          path.isEmpty()
              ? juce::String()
              : f.getParentDirectory().getChildFile(path).getFullPathName());
    }
    return juce::Result::ok();
  }

private:
  Processor &processor;
};
} // namespace kone::grid
