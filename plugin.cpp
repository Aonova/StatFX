#include "logger.h"
#include "ini.h"
#include "easing.h"

// settings to be filled in from ini file
static class Settings {
    public:
    struct OverlayData {
        bool enabled = true;
        std::string editorID;
        RE::NiColorA tint = RE::NiColorA(1.0f,1.0f,1.0f,0.0f);
        float contrastAdd = 0.0f;
        float contrastMult = 1.0f;
        float brightnessAdd = 0.0f;
        float brightnessMult = 1.0f;
        float saturationAdd = 0.0f;
        float saturationMult = 1.0f;
        float startFraction = 1.0f;
        float endFraction = 0.0f;
        easing::easingFunction easingFunction = easing::linear;
        float minDelta = 0.01f;
        float maxDelta = 0.015f;
        float maxDeltaPos = 0.015f;
    } stamina, magicka, health, defaultValues;
    const std::vector<OverlayData*> stats = {&stamina, &magicka, &health};
    int sleepTime = 25;
    std::string iniPath = "StatFX.ini";
    bool reload = true;
    const std::map<std::string, std::string> defaultEditorIDs = {
        {"Stamina", "StatFXImodStam"},
        {"Magicka", "StatFXImodMag"},
        {"Health", "StatFXImodHealth"}
    };
} settings;

// state enum for main thread to check
enum class State { Pause, Run, Kill };

// Global state to control the main thread
State state = State::Pause;

 // player actor ref
RE::PlayerCharacter *player = nullptr;

// main thread reference
std::thread main_thread;

// Actual stat value percentages for the player, updated every main thread tick
static struct Stats {
    float health = 1.0f;
    float stamina = 1.0f;
    float magicka = 1.0f;
} s_actual;
// NiColors for each stat, to be set according to ini settings

static struct Imods {
    RE::TESImageSpaceModifier *stamina = nullptr;
    RE::TESImageSpaceModifier *magicka = nullptr;
    RE::TESImageSpaceModifier *health = nullptr;
} imods;

static struct ImodInstances {
    RE::ImageSpaceModifierInstanceForm *stamina = nullptr;
    RE::ImageSpaceModifierInstanceForm *magicka = nullptr;
    RE::ImageSpaceModifierInstanceForm *health = nullptr;
} imodInstances;

static std::vector<std::pair<RE::TESImageSpaceModifier**, RE::ImageSpaceModifierInstanceForm**>> imodAndInstance = {
    {&imods.stamina, &imodInstances.stamina},
    {&imods.magicka, &imodInstances.magicka},
    {&imods.health, &imodInstances.health}
};

mINI::INIStructure iniStruct;
// Read settings file and fill out settings struct
void initSettings() {
    logger::info("INI Config: INITIALIZATION");
    // Common sense short curcuit if state is not pause
    if (state != State::Pause) {
        logger::error("INI Config: Error, trying to read settings while thread is not paused, skipping settings read");
        return;
    }
    // lambda to convert string to lowercase
    auto strLower = [](std::string str) -> std::string {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    };
    // lambda to remove all whitespace and parentheses and convert to lowercase
    auto normalizeStr = [strLower](std::string str) -> std::string {
        str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
        str.erase(std::remove(str.begin(), str.end(), '('), str.end());
        str.erase(std::remove(str.begin(), str.end(), ')'), str.end());
        return strLower(str);
    };
    auto skyrimPath = std::filesystem::current_path().string();
    logger::info("INI Config: Reading '{}' file for settings", "..\\Data\\SKSE\\Plugins\\" + settings.iniPath);
    try {
        // check if ini file exists
        if (!std::filesystem::exists(skyrimPath + "\\Data\\SKSE\\Plugins\\" + settings.iniPath)) {
            // log a warning
            logger::warn("INI Config: File not found: Using default settings (no overlays)");
            return;
        }
        mINI::INIFile iniFile(skyrimPath + "\\Data\\SKSE\\Plugins\\" + settings.iniPath);
        iniStruct.clear(); // clear iniStruct
        iniFile.read(iniStruct);
        // get global settings
        auto iniSleepTime = iniStruct.get("Global").get("SleepTime");
        if (iniSleepTime.empty() ) {
            logger::warn("INI Config: Global Section: SleepTime not found: Using default value");
        } else {
            try { settings.sleepTime = round(std::stof( iniSleepTime )); }
            catch (const std::exception& e) {
                logger::error("{}", e.what());
                logger::warn("INI Config: Global Section: Error reading SleepTime '{}': Using default value", iniSleepTime);
            }
        }
        // Set no reload flag if defined
        auto iniReload = iniStruct.get("Global").get("Reload");
        if (!iniReload.empty() && normalizeStr(iniReload)=="false") {
            logger::info("INI Config: Reload flag set to false");
            settings.reload = false;
        }
        // lambda to init a stat's overlay (stamina, magicka, health)
        auto initOverlay = [strLower, normalizeStr](Settings::OverlayData *stat, std::string section) {
            logger::info("INI Config: Initializing settings for section: '{}'", section);
            // Check if disable flag for this overlay is set in ini
            auto iniDisabled = iniStruct.get(section).get("Disabled");
            if (!iniDisabled.empty() && normalizeStr(iniDisabled)=="true") {
                logger::info("INI Config: Disabling {} overlay: manual disabled flag set to True", section);
                stat->enabled = false;
                return;
            }
            auto iniEditorID = iniStruct.get(section).get("EditorID");
            logger::info("INI Config: Editor ID read: '{}'", iniEditorID);
            if (iniEditorID.empty()) {
                logger::warn("INI Config: EditorID is empty, using default: '{}'", section, settings.defaultEditorIDs.at(section));
                iniEditorID = settings.defaultEditorIDs.at(section);
                return;
            }
            stat->editorID = iniEditorID;
            // Fill in tint color settings from ini if they exist, otherwise keep default
            auto iniTintColor = iniStruct.get(section).get("TintColor"); // this is a hex color code: https://rgbcolorpicker.com for instance
            float tintR=0.0f, tintG=0.0f, tintB=0.0f, tintA=0.0f;
            std::vector<float*> tints = {&tintR, &tintG, &tintB, &tintA};
            if (!iniTintColor.empty()) {
                // remove whitespace and parentheses
                iniTintColor.erase(std::remove(iniTintColor.begin(), iniTintColor.end(), ' '), iniTintColor.end());
                iniTintColor.erase(std::remove(iniTintColor.begin(), iniTintColor.end(), '('), iniTintColor.end());
                iniTintColor.erase(std::remove(iniTintColor.begin(), iniTintColor.end(), ')'), iniTintColor.end());
                // if it starts with hash, remove it and parse the color code
                if (iniTintColor[0] == '#') {
                    iniTintColor.erase(0,1);
                    try { // convert hex code to float color values
                        tintR = static_cast<float>(std::stoi(iniTintColor.substr(0,2), nullptr, 16))/255.0f;
                        tintG = static_cast<float>(std::stoi(iniTintColor.substr(2,2), nullptr, 16))/255.0f;
                        tintB = static_cast<float>(std::stoi(iniTintColor.substr(4,2), nullptr, 16))/255.0f;
                        if (iniTintColor.length() == 8) tintA = static_cast<float>(std::stoi(iniTintColor.substr(6,2), nullptr, 16))/255.0f;
                    } catch (const std::exception& e) {
                        logger::error("{}", e.what());
                        logger::warn("INI Config: {} Section: Could not understand TintColor hex code '{}', using default (no tint)",section,iniTintColor);
                    }
                } else {
                    // since its not a hash, parse it as comma seperated rgba values
                    // remove all alphabet characters
                    iniTintColor.erase(std::remove_if(iniTintColor.begin(), iniTintColor.end(), ::isalpha), iniTintColor.end());
                    // parse 4 comma seperated values as float color values
                    std::vector<std::string> strVals;
                    std::stringstream ss(iniTintColor);
                    while(ss.good()) {
                        std::string substr;
                        std::getline(ss, substr, ',');
                        strVals.push_back(substr);
                    }
                    for (int i=0; i<4; i++) {
                        if (!i<strVals.size()) {
                            logger::warn("INI Config: {} Section: TintColor RGBA '{}' is missing some values. Using default (255,255,255,0) for the missing ones", section, iniTintColor);
                            break;
                        }
                        try { *tints[i] = std::stof(strVals[i]); }
                        catch (const std::exception& e) {
                            logger::error("{}", e.what());
                            logger::warn("INI Config: {} Section: Could not understand TintColor RGBA '{}' value {}, skipping", section, iniTintColor, i+1);
                        }
                    }
                }
            }
            if (!iniStruct.get(section).get("TintStrength").empty()) tintA=std::stof(iniStruct.get(section).get("TintStrength")); // alias for TintAlpha
            // legacy tint key support: override with "TintRed", "TintGreen", "TintBlue", "TintAlpha" if they exist
            if (!iniStruct.get(section).get("TintRed").empty()) tintR=std::stof(iniStruct.get(section).get("TintRed"));
            if (!iniStruct.get(section).get("TintGreen").empty()) tintG=std::stof(iniStruct.get(section).get("TintGreen"));
            if (!iniStruct.get(section).get("TintBlue").empty()) tintB=std::stof(iniStruct.get(section).get("TintBlue"));
            if (!iniStruct.get(section).get("TintAlpha").empty()) tintA=std::stof(iniStruct.get(section).get("TintAlpha"));
            // Any values above 1 will be interpreted as out of 255 range, so divide by 255
            for (auto tint: {&tintR, &tintG, &tintB, &tintA}) { if (*tint>1.0f) *tint/=255.0f;}
            // warn if any of the values are out of range 0 to 1
            if (tintR<0.0f || tintR>1.0f || tintG<0.0f || tintG>1.0f || tintB<0.0f || tintB>1.0f || tintA<0.0f || tintA>1.0f) {
                logger::error("INI Config: {} Section: some tint color value (r{:.2} g{:.2} b{:.2} a{:.2}) is out of range (0.0 to 1.0): Using default (no tint)", section, tintR, tintG, tintB, tintA);
                tintR=0.0f; tintG=0.0f; tintB=0.0f; tintA=0.0f;
            }
            stat->tint = RE::NiColorA(tintR, tintG, tintB, tintA);
            // Fill in cinematic settings from ini if they exist, otherwise keep default
            std::vector<std::pair<float*, std::string>> cinematics = {
                {&stat->contrastAdd, "ContrastAdd"},
                {&stat->contrastMult, "ContrastMult"},
                {&stat->brightnessAdd, "BrightnessAdd"},
                {&stat->brightnessMult, "BrightnessMult"},
                {&stat->saturationAdd, "SaturationAdd"},
                {&stat->saturationMult, "SaturationMult"}
            };
            for (auto [dest, key]: cinematics) {
                if (!iniStruct.get(section).get(key).empty()) {
                    auto iniVal = iniStruct.get(section).get(key);
                    if (iniVal.empty()) continue;
                    try { *dest = std::stof( iniVal ); }
                    catch (const std::exception& e) {
                        logger::error("{}", e.what());
                        logger::warn("INI Config: {} Section: Could not understand {}:'{}': Using default", section, key, iniVal);
                    }
                }
            }
            // Fill in curve range info from ini
            float startF=1.0f, endF=0.0f;
            auto iniRange = iniStruct.get(section).get("Range");
            if (!iniRange.empty()) {
                // remove white space and parantheses
                iniRange.erase(std::remove(iniRange.begin(), iniRange.end(), ' '), iniRange.end());
                iniRange.erase(std::remove(iniRange.begin(), iniRange.end(), '('), iniRange.end());
                iniRange.erase(std::remove(iniRange.begin(), iniRange.end(), ')'), iniRange.end());
                // start is before comma, end is after comma
                auto commaPos = iniRange.find(",");
                if (commaPos == std::string::npos) {
                    logger::warn("INI Config: {} Section: Could not understand Range '{}': Using defaults (Start:1.0, End:0.0)", section, iniRange);
                } else {
                    auto end = iniRange.substr(0, commaPos);
                    auto start = iniRange.substr(commaPos+1);

                    try {
                        startF = std::stof( start ); endF = std::stof( end );
                    } catch (const std::exception& e) {
                        logger::error("{}", e.what());
                        logger::warn("INI Config: {} Section: Could not understand Range '{}': Using defaults (Start: 1.0, End: 0.0)", section, iniRange);
                        startF = 1.0f; endF = 0.0f;
                    }
                    if ( endF>1.0 || endF<0.0 || startF>1.0 || startF<0.0 ) {
                        logger::warn("INI Config: {} Section: Range Start '{:.2}' and End '{:.2}' must be in range 0 to 1: Using defaults (Start: 1.0, End: 0.0)", section, startF, endF);
                        startF = 1.0f; endF = 0.0f;
                    } else {
                        // flip start and end if start is lower than end
                        if (startF < endF) {
                            logger::warn("INI Config: {} Section: Range Start '{:.2}' is lower than End '{:.2}': Flipping values", section, startF, endF);
                            float temp = startF;
                            startF = endF;
                            endF = temp;
                        }
                    }
                }
            }
            // legacy start and end fraction
            auto iniStartFraction = iniStruct.get(section).get("StartFraction");
            auto iniEndFraction = iniStruct.get(section).get("EndFraction");
            if (!iniStartFraction.empty()) {
                try { startF = std::stof( iniStartFraction ); }
                catch (const std::exception& e) {
                    logger::error("{}", e.what());
                    logger::warn("INI Config: {} Section: Could not understand StartFraction '{}': Using default (Start: 1.0)", section, iniStartFraction);
                    startF = 1.0f;
                }
            }
            if (!iniEndFraction.empty()) {
                try { endF = std::stof( iniEndFraction ); }
                catch (const std::exception& e) {
                    logger::error("{}", e.what());
                    logger::warn("INI Config: {} Section: Could not understand EndFraction '{}': Using default (End: 0.0)", section, iniEndFraction);
                    endF = 0.0f;
                }
            }
            if ( endF>1.0 || endF<0.0 || startF>1.0 || startF<0.0 ) {
                logger::warn("INI Config: {} Section: StartFraction '{:.2}' and EndFraction '{:.2}' must be in range 0 to 1: Using defaults (Start: 1.0, End: 0.0)", section, startF, endF);
                startF = 1.0f; endF = 0.0f;
            }
            //  flip start and end if start is lower than end
            if (startF < endF) {
                logger::warn("INI Config: {} Section: StartFraction '{:.2}' is lower than EndFraction '{:.2}': Flipping values", section, startF, endF);
                float temp = startF;
                startF = endF;
                endF = temp;
            }
            stat->startFraction = startF;
            stat->endFraction = endF;
            // get easing function value from ini (try variations on key)
            easing::easingFunction easingFunction;
            std::string iniEasingFunction;
            for (auto key: {"EasingFunction","Ease","Function","Curve","Easing","EasingFunc",
                "CurveFunction","CurveFunc","EaseFunc","EaseFunction","CurveType","EasingCurve","EaseCurve"}) {
                iniEasingFunction = iniStruct.get(section).get(key);
                if (iniEasingFunction.empty()) continue;
                easingFunction = easing::getEasingFunctionString( iniEasingFunction ); break;
            }
            // warn if it defaulted to linear
            if (easingFunction == easing::linear && iniEasingFunction!="linear") {
                logger::warn("INI Config: {} Section: No match for Easing Curve '{}': using 'linear' default", section, iniEasingFunction);
            } else {
                stat->easingFunction = easingFunction;
            }
            // read FadeTime as a high-level setting for maxDelta. This is how many seconds to go from no effect to full effect
            float maxDeltaNeg=settings.defaultValues.maxDelta; float maxDeltaPos=settings.defaultValues.maxDeltaPos; float minDelta=settings.defaultValues.minDelta;
            std::string iniFadeTime = "";
            // try variations on key
            for (auto key: {"FadeTime","TransitionTime","Transition","Fade","Time","FadeDuration","FadeSecs","FadeSeconds","FadeS"}) {
                iniFadeTime = iniStruct.get(section).get(key);
                if (!iniFadeTime.empty()) break;
            }
            { // this weird block with SKIP_FADE_TIME lets me succinctly break processing FadeTime when we know parsing failed somewhere
                if (iniFadeTime.empty()) {
                    logger::warn("INI Config: {} Section: FadeTime not found: Using default deltas", section);
                    goto SKIP_FADE_TIME;
                }
                // remove white space and parantheses
                iniFadeTime = normalizeStr(iniFadeTime);
                auto fadeTimeNeg=-1.0f; auto fadeTimePos=-1.0f;
                // if there is a comma, use the first value as min and second as max
                auto commaPos = iniFadeTime.find(",");
                if (commaPos != std::string::npos) { // read in as (neg, pos)
                    auto first = iniFadeTime.substr(0, commaPos);
                    auto second = iniFadeTime.substr(commaPos+1);
                    try { fadeTimeNeg = std::stof( first ); fadeTimePos = std::stof( second ); }
                    catch (const std::exception& e) {
                        logger::error("{}", e.what());
                        logger::warn("INI Config: {} Section: Could not understand FadeTime '{}': Ignoring", section, iniFadeTime);
                    }
                } else { // single value, use for both neg and pos
                    try { fadeTimeNeg = fadeTimePos = std::stof( iniFadeTime ); }
                    catch (const std::exception& e) {
                        logger::error("{}", e.what());
                        logger::warn("INI Config: {} Section: Could not understand FadeTime '{}': Ignoring", section, iniFadeTime);
                    }
                }
                if (fadeTimeNeg <= 0.0f || fadeTimePos <= 0.0f) goto SKIP_FADE_TIME; //negative value or zero is parse fail token, dont continue
                // convert fade time seconds to delta percentage per tick (sleepTime ms per tick)
                auto secsToTicks = [](float secs)->int { return round(secs*1000.0f/settings.sleepTime); };
                auto curveRange = abs(stat->startFraction - stat->endFraction); // the percentage (between 0 and 1) of the stat which we want to have a duration of fade time seconds
                auto secsToDelta = [secsToTicks,curveRange](float secs)->float { return curveRange/secsToTicks(secs); };
                maxDeltaNeg = secsToDelta(fadeTimeNeg); maxDeltaPos = secsToDelta(fadeTimePos);
                logger::info("INI Config: {} Section: FadeTime '{}' parsed as MaxDeltaNeg: {:.4} MaxDeltaPos: {:.4}", section, iniFadeTime, maxDeltaNeg, maxDeltaPos);


            } SKIP_FADE_TIME: // label to skip the rest of the block if fade time could not be processed (to avoid having to handle useless cascading errors)
            // use the legacy Min- and MaxDelta values as overrides if they are defined in the ini
            auto iniMinDelta = iniStruct.get(section).get("MinDelta");
            try { if (!iniMinDelta.empty()) {minDelta = std::stof( iniMinDelta );} }
            catch (const std::exception& e) {
                logger::error("{}", e.what());
                logger::warn("INI Config: {} Section: Could not understand MinDelta '{}': Using default value", section, iniMinDelta);
            }
            auto iniMaxDelta = iniStruct.get(section).get("MaxDelta");
            try {if (!iniMaxDelta.empty()) {maxDeltaNeg = maxDeltaNeg = std::stof( iniMaxDelta );}} // default both neg and positive maxDelta to same value
            catch (const std::exception& e) {
                logger::error("{}", e.what());
                logger::warn("INI Config: {} Section: Could not understand MaxDelta '{}': Using default value", section, iniMaxDelta);
            }
            auto iniMaxDeltaPos = iniStruct.get(section).get("MaxDeltaPos");
            try {if (!iniMaxDeltaPos.empty()) {maxDeltaPos = std::stof( iniMaxDeltaPos );}} // override maxDeltaPos only if the key is in the ini
            catch (const std::exception& e) {
                logger::error("{}", e.what());
                logger::warn("INI Config: {} Section: Could not understand MaxDeltaPos '{}': Using default value", section, iniMaxDeltaPos);
            }
            // update the actual values
            stat->minDelta = minDelta; stat->maxDelta = maxDeltaNeg; stat->maxDeltaPos = maxDeltaPos;
            // set minDelta to slightly less than max if it is higher than max
            if (stat->minDelta > stat->maxDelta) {
                logger::warn("INI Config: {} Section: MinDelta '{:.4}' is higher than MaxDelta '{:.4}': Setting MinDelta to MaxDelta-0.001", section, stat->minDelta, stat->maxDelta);
                stat->minDelta = stat->maxDelta - 0.001f;
            }
        };
        std::map<std::string, Settings::OverlayData*> sectionToSetting = {
            {"Stamina", &settings.stamina},
            {"Magicka", &settings.magicka},
            {"Health", &settings.health}
        };
        // run init for each stat (health, magicka, stamina)
        for (auto [section, setting]: sectionToSetting) {
            initOverlay(setting, section);
        }
        // log test assertions
        logger::info("INI Config: ALL SETTINGS DONE LOADING FROM INI FILE");
        logger::info("SETTINGS LOADED: [Global] SleepTime:'{}' Reload:'{}'", settings.sleepTime, settings.reload);
        for (auto [section, setting]: sectionToSetting) {
            if (setting->enabled) logger::info("SETTINGS LOADED: [{}] ID:'{}' Tint:({:.2},{:.2},{:.2},{:.2}) Contrast:(x{:.2}+{:.2}) Brightness:(x{:.2}+{:.2}) Saturation:(x{:.2}+{:.2}) Range:({:.2},{:.2}) Curve:'{}' Delta:({:.4},{:.4})", section, setting->editorID, setting->tint.red, setting->tint.green, setting->tint.blue, setting->tint.alpha, setting->contrastMult, setting->contrastAdd, setting->brightnessMult, setting->brightnessAdd, setting->saturationMult, setting->saturationAdd, setting->startFraction, setting->endFraction, easing::getStringEasingFunction(setting->easingFunction), setting->minDelta, setting->maxDelta);
        }

    } catch (const std::exception& e) {
        logger::error("{}", e.what());
        logger::error("Unresolvable error reading settings ini file (syntax issue?): Disabling all overlays");
        for (auto stat: settings.stats) { stat->enabled = false; }
        return;
    }

}

// ================================================================================================
// Some util functions modified from GTS_Plugin - License Apache 2.0:
// https://github.com/QuantumEntangledAndy/GTS_Plugin?tab=Apache-2.0-1-ov-file
float GetMaxActorValue(RE::Actor* actor, RE::ActorValue value) {
    auto baseValue = actor->AsActorValueOwner()->GetBaseActorValue(value);
    auto permMod = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIERS::kPermanent, value);
    auto tempMod = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIERS::kTemporary, value);
    return baseValue + permMod + tempMod;
}

float GetActorValue(RE::Actor* actor, RE::ActorValue value) {
    float baseValue = GetMaxActorValue(actor, value);
    auto damageMod = actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIERS::kDamage, value);
    return baseValue + damageMod;
}
float GetPercentageAV(RE::Actor* actor, RE::ActorValue av) {
    return std::max(std::min((GetActorValue(actor, av)/GetMaxActorValue(actor, av)),1.0f),0.0f);
}
// ================================================================================================

void RunMainThread() {
    // short circuit if kill state
    if (state == State::Kill) {
        logger::error("Cannot start main thread due to kill-state.");
        return;
    }
    try{
        // reload settings and forms if reload flag is set
        if (settings.reload) {
            logger::info("Starting Main Thread: RELOAD FLAG IS SET - reloading settings and forms");
            initSettings();
            initForms();
        }
        // set player character
        player = RE::PlayerCharacter::GetSingleton();
        // log if error getting player
        if (!player) {
            logger::error("Player character not found");
            return;
        }
        // initialize player resource percentages
        s_actual.health = GetPercentageAV(player, RE::ActorValue::kHealth);
        s_actual.stamina = GetPercentageAV(player, RE::ActorValue::kStamina);
        s_actual.magicka = GetPercentageAV(player, RE::ActorValue::kMagicka);
        // log current percentages
        logger::info("Starting Main Thread: Health:{:.2f} Stamina:{:.2f} Magicka:{:.2f}", s_actual.health, s_actual.stamina, s_actual.magicka);
        // imodInstances.stamina = RE::ImageSpaceModifierInstanceForm::Trigger(imods.stamina, 0.0f, nullptr);
        // imodInstances.magicka = RE::ImageSpaceModifierInstanceForm::Trigger(imods.magicka, 0.0f, nullptr);
        // imodInstances.health = RE::ImageSpaceModifierInstanceForm::Trigger(imods.health, 0.0f, nullptr);
        // run main thread
        state = State::Run;
    } catch(const std::exception& e) {
        logger::error("{}", e.what());
        logger::error("Main thread set-up exception: Disabling plugin");
        state = State::Kill;
        return;
    }
}

// On Game Loaded Callback
void OnGameLoaded() {
    RunMainThread();
}

// On New Game Callback
void OnNewGame() {
    RunMainThread();
}

void MainThread() {
    auto state_current = state;
    // "current" stat percentages. Used to check for changes and adjusted according to configured min and max deltas
    Stats s_current = {s_actual.health, s_actual.stamina, s_actual.magicka};
    logger::info("Main thread initialized. State:{} Health:{:.2f} Stamina:{:.2f} Magicka:{:.2f}", (int)state_current, s_current.health, s_current.stamina, s_current.magicka);
    // define lambda for maxDelta approach for updating current stat percentages
    auto approach = [](float current, float actual, float maxDeltaNeg, float maxDeltaPos)->float {
        if (current < actual) {
            if (actual - current > maxDeltaPos) { return current + maxDeltaPos; }
            else { return actual; }
        }
        else {
            if (current - actual > maxDeltaNeg) { return current - maxDeltaNeg; }
            else { return actual; }
        }
    };
    // convert 0 to 1 value percentage into 1 to 0 image modifier strength
    auto easedValue = [](float value, float start, float end, easing::easingFunction easeF) -> float {
        // return 0 when value is higher than start, and 1 when value is lower than end
        if (value > start) { return static_cast<float>(easeF(0.0f)); }
        if (value < end) { return static_cast<float>(easeF(1.0f)); }
        // apply easing function backwards between start and end
        auto range = start - end;
        return static_cast<float>(easeF(((-value+end)/range)+1));
    };
    // define tick function for each stat
    auto tick = [approach,easedValue](float *current, float *actual,
        RE::ActorValue avEnum,
        Settings::OverlayData statOverlayData,
        RE::ImageSpaceModifierInstanceForm *imodInstance,
        RE::TESImageSpaceModifier *imod) {
            // update the actual resource percentage
            *actual = GetPercentageAV(player, avEnum);
            // short circuit if the change is less than minDelta
            if (std::abs(*current - *actual) < statOverlayData.minDelta) { return; }
            // update current resource percentage to approach the actual resource percentage using configured deltas
            *current = approach(*current, *actual, statOverlayData.maxDelta, statOverlayData.maxDeltaPos);
            // update image space modifiers
            if (imodInstance) RE::ImageSpaceModifierInstanceForm::Stop(imod);
            imodInstance = RE::ImageSpaceModifierInstanceForm::Trigger(imod, easedValue(*current,statOverlayData.startFraction,statOverlayData.endFraction,statOverlayData.easingFunction), nullptr);
    };
    // main loop
    try{
        while (state != State::Kill) {
            if (state == State::Run) {
                // state is RUN
                if (state_current != State::Run) { // log state change from pause to run
                    logger::info("Main thread: Running");
                    state_current = State::Run;
                }
                try {
                    // process tick for each stat: update values and image modifiers according to configured deltas
                    if (settings.health.enabled) tick(&s_current.health, &s_actual.health, RE::ActorValue::kHealth, settings.health, imodInstances.health, imods.health);
                    if (settings.stamina.enabled) tick(&s_current.stamina, &s_actual.stamina, RE::ActorValue::kStamina, settings.stamina, imodInstances.stamina, imods.stamina);
                    if (settings.magicka.enabled) tick(&s_current.magicka, &s_actual.magicka, RE::ActorValue::kMagicka, settings.magicka, imodInstances.magicka, imods.magicka);
                } catch (const std::exception& e) {
                    logger::error("{}", e.what());
                    logger::error("Main thread exception: Pausing for 5 seconds");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
            } else {
                // state is PAUSE
                if (state_current != State::Pause) { // log state change from run to pause
                    logger::info("Main thread: Paused");
                    state_current = State::Pause;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(settings.sleepTime));
        }
    } catch(const std::exception& e) {
        logger::error("{}", e.what());
        logger::error("Main thread exception: Disabling plugin");
        state = State::Kill;
    }
    logger::info("Main thread stopped: Kill state");
}

void initForms() { //MUST ONLY BE CALLED AFTER INIT SETTINGS
    // Load ImageSpaceModifier Forms
    logger::info("Loading Imod Forms");
    // auto defaultImod = RE::TESForm::LookupByEditorID<RE::TESImageSpaceModifier>("defaultDesaturateImod");
    imods.stamina = RE::TESForm::LookupByEditorID<RE::TESImageSpaceModifier>(settings.stamina.editorID);
    imods.magicka = RE::TESForm::LookupByEditorID<RE::TESImageSpaceModifier>(settings.magicka.editorID);
    imods.health = RE::TESForm::LookupByEditorID<RE::TESImageSpaceModifier>(settings.health.editorID);
    // for (auto imod: {imods.stamina, imods.magicka, imods.health}) { imod = defaultImod->CreateDuplicateForm(true,imod)->As<RE::TESImageSpaceModifier>(); }
    // update imagespace modifier forms with values from settings
    auto updateImod = [](RE::TESImageSpaceModifier *imod, Settings::OverlayData stat) {
        if (imod) {
            imod->tintColor->colorData->keys[0] = RE::NiColorKey(0.0f, stat.tint);
            imod->cinematic.contrast.add->floatData->keys[0] = RE::NiFloatKey(0.0f, stat.contrastAdd);
            imod->cinematic.contrast.mult->floatData->keys[0] = RE::NiFloatKey(0.0f, stat.contrastMult);
            imod->cinematic.brightness.add->floatData->keys[0] = RE::NiFloatKey(0.0f, stat.brightnessAdd);
            imod->cinematic.brightness.mult->floatData->keys[0] = RE::NiFloatKey(0.0f, stat.brightnessMult);
            imod->cinematic.saturation.add->floatData->keys[0] = RE::NiFloatKey(0.0f, stat.saturationAdd);
            imod->cinematic.saturation.mult->floatData->keys[0] = RE::NiFloatKey(0.0f, stat.saturationMult);
        }
    };
    logger::info("Loading Imod Forms: attempting to updateImods - Stamina");
    updateImod(imods.stamina, settings.stamina);
    logger::info("Loading Imod Forms: attempting to updateImods - Magicka");
    updateImod(imods.magicka, settings.magicka);
    logger::info("Loading Imod Forms: attempting to updateImods - Health");
    updateImod(imods.health, settings.health);
}

// On Data Loaded Callback
void OnDataLoaded() {
    // Load settings
    initSettings();
    // Load forms
    initForms();
    // start main thread, intially paused
    main_thread = std::thread(MainThread);

    logger::info("SKSE OnDataLoaded Completed");
}

// On Preload Game, make sure to pause thread
void OnPreloadGame() {
    state = State::Pause;
}

// On Message Callback
void OnMessage(SKSE::MessagingInterface::Message* msg) {
    if (state == State::Kill) { return; }
    if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
        logger::info("SKSE: Data loaded");
        OnDataLoaded();
    } else if (msg->type == SKSE::MessagingInterface::kNewGame) {
        logger::info("SKSE: New game started");
        OnNewGame();
    } else if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
        logger::info("SKSE: Game loaded");
        OnGameLoaded();
    } else if (msg->type == SKSE::MessagingInterface::kPreLoadGame) {
        logger::info("SKSE: Preload game");
        OnPreloadGame();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
    // Setup logging (e.g. using spdlog)
    SetupLog();
    ///
    // register onMessage
    if (state!=State::Kill) {SKSE::GetMessagingInterface()->RegisterListener(OnMessage);}
    ///
    logger::info("SKSE Plugin Load Completed");
    return true;
}
