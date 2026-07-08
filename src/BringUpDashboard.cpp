#include "BringUpDashboard.h"

#include <FastLED.h>

// Confirmed working Dig2Go hardware configuration.
static constexpr uint8_t LED_DATA_PIN = 16;
static constexpr uint8_t LED_RELAY_PIN = 12;
static constexpr uint8_t BUTTON_PIN = 0;
static constexpr uint16_t NUM_LEDS = 15;
static constexpr uint16_t FRAME_MS = 30;

CRGB leds[NUM_LEDS];

void BringUpDashboard::begin()
{
    Serial.begin(115200);
    delay(800);

    pinMode(LED_RELAY_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    applyRelay(true);
    delay(100);

    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    applyBrightness();

    renderSolid(255, 0, 0);
    FastLED.show();

    Serial.println();
    Serial.println("====================================");
    Serial.println(" DOBETTERLED BRING-UP DASHBOARD");
    Serial.println("====================================");
    Serial.println("Hardware: Dig2Go, GPIO16 data, GPIO12 relay, 15 WS2812B GRB LEDs");
    printMenu();
}

void BringUpDashboard::update()
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
}

void BringUpDashboard::printMenu()
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
    Serial.println();
    Serial.print("Current brightness: ");
    Serial.println(m_brightness);
    Serial.println();
}

void BringUpDashboard::handleSerial()
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

void BringUpDashboard::setMode(Mode mode, const char* label)
{
    m_mode = mode;
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

void BringUpDashboard::renderFrame()
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
    }
}

void BringUpDashboard::renderSolid(uint8_t r, uint8_t g, uint8_t b)
{
    fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
    FastLED.show();
}

void BringUpDashboard::renderRainbow()
{
    fill_rainbow(leds, NUM_LEDS, m_hue, 12);
    FastLED.show();
    ++m_hue;
}

void BringUpDashboard::renderChase()
{
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    leds[m_chaseIndex % NUM_LEDS] = CRGB::White;
    FastLED.show();
    m_chaseIndex = (m_chaseIndex + 1) % NUM_LEDS;
}

void BringUpDashboard::clearStrip()
{
    fill_solid(leds, NUM_LEDS, CRGB::Black);
}

void BringUpDashboard::applyRelay(bool on)
{
    m_relayOn = on;
    digitalWrite(LED_RELAY_PIN, on ? HIGH : LOW);
}

void BringUpDashboard::applyBrightness()
{
    FastLED.setBrightness(m_brightness);
}

void BringUpDashboard::printButtonStatus()
{
    const bool pressed = (digitalRead(BUTTON_PIN) == LOW);
    Serial.print("Button: ");
    Serial.println(pressed ? "PRESSED" : "released");
}
