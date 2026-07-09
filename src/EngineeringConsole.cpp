#include "EngineeringConsole.h"

#include <FastLED.h>

namespace
{
    // Confirmed working Dig2Go hardware configuration -- identical
    // values to BringUpDashboard.cpp. Kept in this anonymous namespace
    // (internal linkage) so nothing here can collide with symbols in
    // BringUpDashboard.cpp, which remains in the build, untouched and
    // simply unreferenced by main.cpp after this milestone.
    constexpr uint8_t LED_DATA_PIN = 16;
    constexpr uint8_t LED_RELAY_PIN = 12;
    constexpr uint8_t BUTTON_PIN = 0;
    constexpr uint16_t NUM_LEDS = 15;
    constexpr uint16_t FRAME_MS = 30;

    // Uniquely named, and internal-linkage via the anonymous namespace,
    // specifically to avoid any duplicate-symbol collision with
    // BringUpDashboard.cpp's own file-scope `CRGB leds[NUM_LEDS];`,
    // which is NOT static and stays in the link because that file is
    // left in place per this milestone's rollback requirement.
    CRGB g_engineeringConsoleLeds[NUM_LEDS];
}

void EngineeringConsole::begin()
{
    Serial.begin(115200);
    delay(800);

    pinMode(LED_RELAY_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    applyRelay(true);
    delay(100);

    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(g_engineeringConsoleLeds, NUM_LEDS);
    applyBrightness();

    renderSolid(255, 0, 0);
    FastLED.show();

    // Milestone 2: resets internal gesture-detection state only. Touches
    // no hardware -- pinMode(BUTTON_PIN, ...) above already configured
    // the pin this class will keep reading every update().
    m_gestureEngine.begin();

    // Milestone 3: resets internal effect/palette state only. Touches
    // no hardware -- never called until a gesture switches into
    // Mode::EffectEngineMode.
    m_effectEngine.begin();

    // Milestone 4A: starts the I2S mic peripheral. Independent pins
    // (SCK=18, WS=4, SD=19) from LED/relay/button -- no conflict. Safe
    // to fail: begin() logs a warning and leaves isAvailable() false
    // rather than blocking or crashing boot.
    m_audioInput.begin();

    printBanner();
    printMenu();
}

void EngineeringConsole::update()
{
    handleSerial();

    const unsigned long now = millis();
    if (now - m_lastFrameMs >= FRAME_MS)
    {
        m_lastFrameMs = now;
        renderFrame();
    }

    if (m_mode == Mode::ButtonTest && now - m_lastStatusMs >= 250)
    {
        m_lastStatusMs = now;
        printButtonStatus();
    }

    // Milestone 2: gesture detection and Serial reporting (unchanged).
    // Milestone 3: the returned Gesture is now also acted on -- see
    // handleGesture(). Reuses the same raw button reading Button Test
    // mode already uses -- read-only, no new GPIO access pattern.
    const ButtonGestureEngine::Gesture gesture = m_gestureEngine.update(digitalRead(BUTTON_PIN) == LOW);
    handleGesture(gesture);

    // Milestone 4A: drains any newly available mic samples. Non-
    // blocking (see AudioInput::update()) -- does not affect frame
    // timing or button responsiveness.
    m_audioInput.update();

    if (m_continuousAudioDiag && now - m_lastAudioDiagMs >= 300)
    {
        m_lastAudioDiagMs = now;
        printAudioDiagnostics();
    }
}

void EngineeringConsole::handleGesture(ButtonGestureEngine::Gesture gesture)
{
    switch (gesture)
    {
        case ButtonGestureEngine::Gesture::SinglePress:
            enterEffectEngineMode();
            m_effectEngine.nextEffect();
            Serial.print("EffectEngine: Next Effect -> ");
            Serial.println(m_effectEngine.currentEffectName());
            break;

        case ButtonGestureEngine::Gesture::DoublePress:
            enterEffectEngineMode();
            m_effectEngine.previousEffect();
            Serial.print("EffectEngine: Previous Effect -> ");
            Serial.println(m_effectEngine.currentEffectName());
            break;

        case ButtonGestureEngine::Gesture::TriplePress:
            enterEffectEngineMode();
            m_effectEngine.nextPalette();
            Serial.print("EffectEngine: Next Palette -> ");
            Serial.println(m_effectEngine.currentPaletteName());
            break;

        case ButtonGestureEngine::Gesture::FourPress:
            enterEffectEngineMode();
            m_effectEngine.nextMode();
            Serial.print("EffectEngine: Next Mode -> ");
            Serial.println(m_effectEngine.currentModeName());
            Serial.print("EffectEngine: Effect -> ");
            Serial.println(m_effectEngine.currentEffectName());
            break;

        case ButtonGestureEngine::Gesture::DoublePressHold:
            // Global on/off modifier, not a separate effect -- persists
            // across effect/palette/Mode changes (this flag is never
            // touched by nextEffect()/previousEffect()/nextPalette()/
            // nextMode()), never blocks 1-4 press navigation (see
            // docs/PRODUCT_SPEC.md Section 10). Milestone 4B: now
            // modulates whichever effect is selected -- see renderFrame().
            m_audioReactiveOverlay = !m_audioReactiveOverlay;
            Serial.print("Audio Reactive Overlay: ");
            Serial.print(m_audioReactiveOverlay ? "ON" : "OFF");
            Serial.print("  (level: ");
            Serial.print(m_audioInput.level());
            Serial.println(")");
            break;

        case ButtonGestureEngine::Gesture::SixPressPowerCandidate:
        case ButtonGestureEngine::Gesture::TenPressFactoryResetPending:
        case ButtonGestureEngine::Gesture::LongHold:
        case ButtonGestureEngine::Gesture::Unclassified:
        case ButtonGestureEngine::Gesture::None:
        default:
            // Candidate-only or out-of-scope gestures. Already Serial-
            // reported by ButtonGestureEngine itself; no action taken
            // here -- no power toggle, no settings erase, no menu, per
            // this milestone's explicit scope limits.
            break;
    }
}

void EngineeringConsole::enterEffectEngineMode()
{
    if (m_mode == Mode::EffectEngineMode)
    {
        return; // Already active -- don't re-flicker the relay every press.
    }

    m_mode = Mode::EffectEngineMode;
    m_modeLabel = "Effect Engine";
    Serial.println("Mode: Effect Engine (button-gesture controlled)");
    applyRelay(true);
    delay(20);
}

void EngineeringConsole::printBanner()
{
    Serial.println();
    Serial.println("====================================");
    Serial.println(" DOBETTERLED ENGINEERING CONSOLE");
    Serial.println(" v0.2.0-alpha");
    Serial.println("====================================");
    Serial.println("Hardware: Dig2Go, GPIO16 data, GPIO12 relay, 15 WS2812B GRB LEDs");
}

void EngineeringConsole::printMenu()
{
    Serial.println();
    Serial.println("Commands:");
    Serial.println("  1 = Solid RED");
    Serial.println("  2 = Solid GREEN");
    Serial.println("  3 = Solid BLUE");
    Serial.println("  4 = Solid WHITE");
    Serial.println("  5 = Moving RAINBOW");
    Serial.println("  6 = CHASE test");
    Serial.println("  7 = Relay OFF test");
    Serial.println("  8 = Button test mode");
    Serial.println("  9 = LEDs OFF, relay ON");
    Serial.println("  + = Brightness up");
    Serial.println("  - = Brightness down");
    Serial.println("  m = Print this menu again");
    Serial.println("  s = Print Engineering Console status");
    Serial.println("  a = Print audio diagnostics (Milestone 4A, mic bring-up)");
    Serial.println("  A = Toggle continuous audio diagnostics (~300ms)");
    Serial.println();
    Serial.println("Button Gesture Engine + Effect Engine: active (Milestone 4B)");
    Serial.println("  1 Press        = Next Effect (within current Mode category)");
    Serial.println("  2 Presses      = Previous Effect (within current Mode category)");
    Serial.println("  3 Presses      = Next Palette");
    Serial.println("  4 Presses      = Next Mode (Static -> Motion -> Reactive)");
    Serial.println("  2 Presses+Hold = Toggle Audio Reactive Overlay (modulates current effect)");
    Serial.println("  Long Hold, 6 Presses, 10 Presses = detected/reported only, no action");
    Serial.println("  Any of 1/2/3/4-press switches live LEDs into Effect Engine mode.");
    Serial.println();
    Serial.print("Current brightness: ");
    Serial.println(m_brightness);
    Serial.println();
}

void EngineeringConsole::handleSerial()
{
    while (Serial.available() > 0)
    {
        const char c = static_cast<char>(Serial.read());

        switch (c)
        {
            case '1': setMode(Mode::SolidRed, "Solid RED"); break;
            case '2': setMode(Mode::SolidGreen, "Solid GREEN"); break;
            case '3': setMode(Mode::SolidBlue, "Solid BLUE"); break;
            case '4': setMode(Mode::SolidWhite, "Solid WHITE"); break;
            case '5': setMode(Mode::Rainbow, "Moving RAINBOW"); break;
            case '6': setMode(Mode::Chase, "CHASE test"); break;
            case '7': setMode(Mode::RelayOff, "Relay OFF test"); break;
            case '8': setMode(Mode::ButtonTest, "Button test mode"); break;
            case '9': setMode(Mode::Off, "LEDs OFF, relay ON"); break;

            case '+':
                if (m_brightness <= 240) m_brightness += 15;
                else m_brightness = 255;
                applyBrightness();
                Serial.print("Brightness: ");
                Serial.println(m_brightness);
                break;

            case '-':
                if (m_brightness >= 20) m_brightness -= 15;
                else m_brightness = 4;
                applyBrightness();
                Serial.print("Brightness: ");
                Serial.println(m_brightness);
                break;

            case 'm':
            case 'M':
                printMenu();
                break;

            case 's':
            case 'S':
                printEngineeringStatus();
                break;

            case 'a':
                printAudioDiagnostics();
                break;

            case 'A':
                m_continuousAudioDiag = !m_continuousAudioDiag;
                Serial.print("Continuous Audio Diagnostics: ");
                Serial.println(m_continuousAudioDiag ? "ON (every ~300ms)" : "OFF");
                break;

            case '\r':
            case '\n':
            case ' ':
                break;

            default:
                Serial.print("Unknown command: ");
                Serial.println(c);
                Serial.println("Type m to print the menu.");
                break;
        }
    }
}

void EngineeringConsole::setMode(Mode mode, const char* label)
{
    m_mode = mode;
    m_modeLabel = label;
    Serial.print("Mode: ");
    Serial.println(label);

    if (mode == Mode::RelayOff)
    {
        clearStrip();
        FastLED.show();
        applyRelay(false);
        Serial.println("Relay is OFF. LEDs should be dark. Press 1-6, 8, or 9 to re-enable relay.");
    }
    else
    {
        applyRelay(true);
        delay(20);
    }

    renderFrame();
}

void EngineeringConsole::renderFrame()
{
    switch (m_mode)
    {
        case Mode::SolidRed:   renderSolid(255, 0, 0); break;
        case Mode::SolidGreen: renderSolid(0, 255, 0); break;
        case Mode::SolidBlue:  renderSolid(0, 0, 255); break;
        case Mode::SolidWhite: renderSolid(255, 255, 255); break;
        case Mode::Rainbow:    renderRainbow(); break;
        case Mode::Chase:      renderChase(); break;
        case Mode::ButtonTest:
            if (digitalRead(BUTTON_PIN) == LOW) renderSolid(255, 255, 255);
            else renderSolid(0, 0, 40);
            break;
        case Mode::Off:
            clearStrip();
            FastLED.show();
            break;
        case Mode::RelayOff:
            // Do nothing while relay is off.
            break;
        case Mode::EffectEngineMode:
            // Milestone 4B: Audio Overlay ON modulates whichever effect
            // is selected; OFF renders identically to before this
            // milestone (see EffectEngine.cpp's per-effect audioActive
            // branches).
            m_effectEngine.render(g_engineeringConsoleLeds, NUM_LEDS,
                                   m_audioReactiveOverlay, m_audioInput.level());
            FastLED.show();
            break;
    }
}

void EngineeringConsole::renderSolid(uint8_t r, uint8_t g, uint8_t b)
{
    fill_solid(g_engineeringConsoleLeds, NUM_LEDS, CRGB(r, g, b));
    FastLED.show();
}

void EngineeringConsole::renderRainbow()
{
    fill_rainbow(g_engineeringConsoleLeds, NUM_LEDS, m_hue, 12);
    FastLED.show();
    ++m_hue;
}

void EngineeringConsole::renderChase()
{
    fill_solid(g_engineeringConsoleLeds, NUM_LEDS, CRGB::Black);
    g_engineeringConsoleLeds[m_chaseIndex % NUM_LEDS] = CRGB::White;
    FastLED.show();
    m_chaseIndex = (m_chaseIndex + 1) % NUM_LEDS;
}

void EngineeringConsole::clearStrip()
{
    fill_solid(g_engineeringConsoleLeds, NUM_LEDS, CRGB::Black);
}

void EngineeringConsole::applyRelay(bool on)
{
    m_relayOn = on;
    digitalWrite(LED_RELAY_PIN, on ? HIGH : LOW);
}

void EngineeringConsole::applyBrightness()
{
    FastLED.setBrightness(m_brightness);
}

void EngineeringConsole::printButtonStatus()
{
    const bool pressed = (digitalRead(BUTTON_PIN) == LOW);
    Serial.print("Button: ");
    Serial.println(pressed ? "PRESSED" : "released");
}

void EngineeringConsole::printEngineeringStatus()
{
    // Read-only: no GPIO writes, no FastLED buffer writes, no relay
    // calls. Only reads already-tracked member state plus millis(),
    // digitalRead(), and ESP.getFreeHeap().
    const bool rawPressed = (digitalRead(BUTTON_PIN) == LOW);

    Serial.println();
    Serial.println("================================================");
    Serial.println(" DOBETTERLED Engineering Console -- Status");
    Serial.println("================================================");
    Serial.println("Hardware");
    Serial.println("  Board             QuinLED Dig2Go");
    Serial.println("  Firmware          DOBETTERLED");
    Serial.println("  Version           v0.2.0-alpha");
    Serial.print("  Uptime (ms)       "); Serial.println(millis());
    Serial.println();
    Serial.println("LED System");
    Serial.print("  Brightness        "); Serial.println(m_brightness);
    Serial.print("  Pixels            "); Serial.println(NUM_LEDS);
    Serial.print("  Current Effect    "); Serial.println(m_modeLabel);
    Serial.println();
    Serial.println("Button");
    Serial.print("  Raw State         "); Serial.println(rawPressed ? "PRESSED" : "released");
    Serial.println("  Gesture Engine    active (Milestone 2 detection + Milestone 3 wiring)");
    Serial.println();
    Serial.println("Effect Engine");
    Serial.print("  Mode Category     "); Serial.println(m_effectEngine.currentModeName());
    Serial.print("  Effect            "); Serial.println(m_effectEngine.currentEffectName());
    Serial.print("  Palette           "); Serial.println(m_effectEngine.currentPaletteName());
    Serial.println();
    Serial.println("Audio");
    Serial.print("  Reactive Overlay  "); Serial.println(m_audioReactiveOverlay ? "ON (modulating current effect)" : "OFF");
    Serial.print("  Level             "); Serial.println(m_audioInput.level());
    Serial.println("  Mode              (not implemented -- SoundManager is dormant)");
    Serial.println();
    Serial.println("Memory");
    Serial.print("  Free Heap         "); Serial.println(ESP.getFreeHeap());
    Serial.println("================================================");
    Serial.println();
}

void EngineeringConsole::printAudioDiagnostics()
{
    // Read-only, same as printEngineeringStatus() -- no GPIO writes, no
    // FastLED buffer writes, no relay calls. Milestone 4A: reports
    // AudioInput's raw hardware readings only, no effect modulation.
    Serial.println();
    Serial.println("Audio Diagnostics");
    Serial.print("  Raw:          "); Serial.println(m_audioInput.raw());
    Serial.print("  Level:        "); Serial.println(m_audioInput.level());
    Serial.print("  Peak:         "); Serial.println(m_audioInput.peak());
    Serial.print("  Average:      "); Serial.println(m_audioInput.average());
    Serial.print("  Noise Floor:  "); Serial.println(m_audioInput.noiseFloor());
    Serial.print("  Available:    "); Serial.println(m_audioInput.isAvailable() ? "YES" : "NO (mic init failed)");
}
