#pragma once

#include <Arduino.h>

class BringUpDashboard
{
public:
    void begin();
    void update();

private:
    enum class Mode : uint8_t
    {
        SolidRed,
        SolidGreen,
        SolidBlue,
        SolidWhite,
        Rainbow,
        Chase,
        RelayOff,
        ButtonTest,
        Off
    };

    Mode m_mode = Mode::SolidRed;
    uint8_t m_brightness = 80;
    uint8_t m_hue = 0;
    uint8_t m_chaseIndex = 0;
    unsigned long m_lastFrameMs = 0;
    unsigned long m_lastStatusMs = 0;
    bool m_relayOn = true;

    void printMenu();
    void handleSerial();
    void setMode(Mode mode, const char* label);
    void renderFrame();
    void renderSolid(uint8_t r, uint8_t g, uint8_t b);
    void renderRainbow();
    void renderChase();
    void clearStrip();
    void applyRelay(bool on);
    void applyBrightness();
    void printButtonStatus();
};
