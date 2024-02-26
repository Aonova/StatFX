#include "logger.h"
#include "ini.h"
#include "easing.h"

// settings to be filled in from ini file
static class Settings {
    public:
    struct OverlayData {
        std::string editorID;
        int tintRed = 0;
        int tintGreen = 0;
        int tintBlue = 0;
        int tintAlpha = 0;
        RE::NiColorA tint = RE::NiColorA(0,0,0,0);
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
        float maxDelta = 1.0f;
    } stamina, magicka, health;
    int sleepTime = 25;
    std::string iniPath = "StatFX.ini";
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

mINI::INIStructure iniStruct;
// Read settings file and fill out settings struct
void initSettings() {
    logger::info("Reading {} file for settings", std::filesystem::current_path().string() + "\\Data\\SKSE\\Plugins\\" + settings.iniPath);
    try {
        // check if ini file exists
        if (!std::filesystem::exists(std::filesystem::current_path().string() + "\\Data\\SKSE\\Plugins\\" + settings.iniPath)) {
            // throw exception
            std::exception e("Settings file not found");
            throw e;
        }
        mINI::INIFile iniFile(std::filesystem::current_path().string() + "\\Data\\SKSE\\Plugins\\" + settings.iniPath);
        logger::info("Reading into iniStruct");
        iniFile.read(iniStruct);
        // get global settings
        logger::info("Getting global settings");
        logger::info("SleepTime: '{}'", iniStruct.get("Global").get("SleepTime") );
        try { settings.sleepTime = std::stoi( iniStruct.get("Global").get("SleepTime") ); }
        catch (const std::exception& e) {
            logger::error("{}", e.what());
            logger::error("Error reading SleepTime: Using default value of 25");
            settings.sleepTime = 25;
        }

        auto initOverlay = [](Settings::OverlayData *stat, std::string section) {
            logger::info("Initializing overlay for section: '{}'", section);
            logger::info("Editor ID: '{}'", iniStruct.get(section).get("EditorID"));
            stat->editorID = iniStruct.get(section).get("EditorID");
            logger::info("Tint: '{}' '{}' '{}' '{}'", iniStruct.get(section).get("TintRed"), iniStruct.get(section).get("TintGreen"), iniStruct.get(section).get("TintBlue"), iniStruct.get(section).get("TintAlpha"));
            stat->tintRed = std::stoi( iniStruct.get(section).get("TintRed") );
            stat->tintGreen = std::stoi( iniStruct.get(section).get("TintGreen") );
            stat->tintBlue = std::stoi( iniStruct.get(section).get("TintBlue") );
            stat->tintAlpha = std::stoi( iniStruct.get(section).get("TintAlpha") );
            stat->tint = RE::NiColorA(static_cast<float>(stat->tintRed)/255.0f, (static_cast<float>(stat->tintGreen))/255.0f, (static_cast<float>(stat->tintBlue))/255.0f, (static_cast<float>(stat->tintAlpha))/255.0f);
            logger::info("Contrast: '{}' '{}' Brightness: '{}' '{}' Saturation: '{}' '{}'", iniStruct.get(section).get("ContrastAdd"), iniStruct.get(section).get("ContrastMult"), iniStruct.get(section).get("BrightnessAdd"), iniStruct.get(section).get("BrightnessMult"), iniStruct.get(section).get("SaturationAdd"), iniStruct.get(section).get("SaturationMult"));
            stat->contrastAdd = std::stof( iniStruct.get(section).get("ContrastAdd") );
            stat->contrastMult = std::stof( iniStruct.get(section).get("ContrastMult") );
            stat->brightnessAdd = std::stof( iniStruct.get(section).get("BrightnessAdd") );
            stat->brightnessMult = std::stof( iniStruct.get(section).get("BrightnessMult") );
            stat->saturationAdd = std::stof( iniStruct.get(section).get("SaturationAdd") );
            stat->saturationMult = std::stof( iniStruct.get(section).get("SaturationMult") );
            logger::info("Start: '{}' End: '{}' Easing: '{}' MinDelta: '{}' MaxDelta: '{}'", iniStruct.get(section).get("StartFraction"), iniStruct.get(section).get("EndFraction"), iniStruct.get(section).get("EasingFunction"), iniStruct.get(section).get("MinDelta"), iniStruct.get(section).get("MaxDelta"));
            stat->startFraction = std::stof( iniStruct.get(section).get("StartFraction") );
            stat->endFraction = std::stof( iniStruct.get(section).get("EndFraction") );
            stat->easingFunction = easing::getEasingFunctionString( iniStruct.get(section).get("EasingFunction") );
            stat->minDelta = std::stof( iniStruct.get(section).get("MinDelta") );
            stat->maxDelta = std::stof( iniStruct.get(section).get("MaxDelta") );
        };
        // get stamina settings
        logger::info("Getting stamina overlay settings");
        initOverlay(&(settings.stamina), "Stamina");
        // get magicka settings
        logger::info("Getting magicka overlay settings");
        initOverlay(&(settings.magicka), "Magicka");
        // get health settings
        logger::info("Getting health overlay settings");
        initOverlay(&(settings.health), "Health");
        // log test assertions
        logger::info("TEST SETTINGS READ SleepTime: {}", settings.sleepTime);
        logger::info("TEST SETTINGS READ Stamina: EditorID: {} Tint: {} {} {} {} Contrast: {} {} Brightness: {} {} Saturation: {} {} Start: {} End: {} Easing: {} MinDelta: {} MaxDelta: {}", settings.stamina.editorID, settings.stamina.tintRed, settings.stamina.tintGreen, settings.stamina.tintBlue, settings.stamina.tintAlpha, settings.stamina.contrastAdd, settings.stamina.contrastMult, settings.stamina.brightnessAdd, settings.stamina.brightnessMult, settings.stamina.saturationAdd, settings.stamina.saturationMult, settings.stamina.startFraction, settings.stamina.endFraction, easing::getStringEasingFunction(settings.stamina.easingFunction), settings.stamina.minDelta, settings.stamina.maxDelta);
        logger::info("TEST SETTINGS READ Magicka: EditorID: {} Tint: {} {} {} {} Contrast: {} {} Brightness: {} {} Saturation: {} {} Start: {} End: {} Easing: {} MinDelta: {} MaxDelta: {}", settings.magicka.editorID, settings.magicka.tintRed, settings.magicka.tintGreen, settings.magicka.tintBlue, settings.magicka.tintAlpha, settings.magicka.contrastAdd, settings.magicka.contrastMult, settings.magicka.brightnessAdd, settings.magicka.brightnessMult, settings.magicka.saturationAdd, settings.magicka.saturationMult, settings.magicka.startFraction, settings.magicka.endFraction, easing::getStringEasingFunction(settings.magicka.easingFunction), settings.magicka.minDelta, settings.magicka.maxDelta);
        logger::info("TEST SETTINGS READ Health: EditorID: {} Tint: {} {} {} {} Contrast: {} {} Brightness: {} {} Saturation: {} {} Start: {} End: {} Easing: {} MinDelta: {} MaxDelta: {}", settings.health.editorID, settings.health.tintRed, settings.health.tintGreen, settings.health.tintBlue, settings.health.tintAlpha, settings.health.contrastAdd, settings.health.contrastMult, settings.health.brightnessAdd, settings.health.brightnessMult, settings.health.saturationAdd, settings.health.saturationMult, settings.health.startFraction, settings.health.endFraction, easing::getStringEasingFunction(settings.health.easingFunction), settings.health.minDelta, settings.health.maxDelta);
    } catch (const std::exception& e) {
        logger::error("{}", e.what());
        logger::error("Error reading config file (syntax issue?): Disabling plugin");
        state = State::Kill;
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
        logger::error("Cannot run main thread due to kill-state: Disabling plugin");
        return;
    }
    try{
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
        // Initialize image space modifiers
        imodInstances.stamina = RE::ImageSpaceModifierInstanceForm::Trigger(imods.stamina, 0.0f, nullptr);
        imodInstances.magicka = RE::ImageSpaceModifierInstanceForm::Trigger(imods.magicka, 0.0f, nullptr);
        imodInstances.health = RE::ImageSpaceModifierInstanceForm::Trigger(imods.health, 0.0f, nullptr);
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

void MainThread(const int SLEEP_TIME) {
    auto state_current = state;
    // "current" stat percentages. Used to check for changes and adjusted according to configured min and max deltas
    Stats s_current = {s_actual.health, s_actual.stamina, s_actual.magicka};
    logger::info("Main thread initialized. State:{} Health:{:.2f} Stamina:{:.2f} Magicka:{:.2f}", (int)state_current, s_current.health, s_current.stamina, s_current.magicka);
    // define lambda for maxDelta approach for updating current stat percentages
    auto approach = [](float current, float actual, float maxDelta)->float {
        if (std::abs(current - actual) < maxDelta) { return actual; }
        else if (current < actual) { return current + maxDelta; }
        else { return current - maxDelta; }
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
            // update current resource percentage to approach the actual resource percentage
            *current = approach(*current, *actual, statOverlayData.maxDelta);
            // update image space modifiers
            if (imodInstance) {
                RE::ImageSpaceModifierInstanceForm::Stop(imod);
                imodInstance = RE::ImageSpaceModifierInstanceForm::Trigger(imod, easedValue(*current,statOverlayData.startFraction,statOverlayData.endFraction,statOverlayData.easingFunction), nullptr);
            }
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
                    tick(&s_current.health, &s_actual.health, RE::ActorValue::kHealth, settings.health, imodInstances.health, imods.health);
                    tick(&s_current.stamina, &s_actual.stamina, RE::ActorValue::kStamina, settings.stamina, imodInstances.stamina, imods.stamina);
                    tick(&s_current.magicka, &s_actual.magicka, RE::ActorValue::kMagicka, settings.magicka, imodInstances.magicka, imods.magicka);
                } catch (const std::exception& e) {
                    logger::error("{}", e.what());
                    logger::error("Main thread exception: Pausing for ten cycles");
                    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME*9));
                }
            } else {
                // state is PAUSE
                if (state_current != State::Pause) { // log state change from run to pause
                    logger::info("Main thread: Paused");
                    state_current = State::Pause;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
        }
    } catch(const std::exception& e) {
        logger::error("{}", e.what());
        logger::error("Main thread exception: Disabling plugin");
        state = State::Kill;
    }
    logger::info("Main thread stopped: Kill state");
}

// On Data Loaded Callback
void OnDataLoaded() {
    // Load settings
    initSettings();
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
    // start main thread
    main_thread = std::thread(MainThread, settings.sleepTime);

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
        logger::info("Data loaded");
        OnDataLoaded();
    } else if (msg->type == SKSE::MessagingInterface::kNewGame) {
        logger::info("New game started");
        OnNewGame();
    } else if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
        logger::info("Game loaded");
        OnGameLoaded();
    } else if (msg->type == SKSE::MessagingInterface::kPreLoadGame) {
        logger::info("Preload game");
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
