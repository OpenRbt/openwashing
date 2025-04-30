#ifndef WIRINGPI_MOCK_H
#define WIRINGPI_MOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <mutex>
#include <bits/this_thread_sleep.h>

#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0

extern int COIN_PIN;
extern int BANKNOTE_PIN;

namespace {
    std::map<int, int> g_mockPinStates;
    std::mutex g_pinMutex;
}

inline void mockSetPinState(int pin, int value) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    g_mockPinStates[pin] = value;
    printf("[MOCK] Setting pin %d to state %d\n", pin, value);
}

inline void mockInjectCoin() {
    mockSetPinState(COIN_PIN, HIGH);
    mockSetPinState(COIN_PIN, LOW);
}

inline void mockInjectBanknote() {
    mockSetPinState(BANKNOTE_PIN, HIGH);
    mockSetPinState(BANKNOTE_PIN, LOW);
}

inline void mockPressButton(int buttonNum) {
    if (buttonNum < 1 || buttonNum > 7) {
        printf("[MOCK] Invalid button number: %d\n", buttonNum);
        return;
    }
    
    int buttonPins[] = {-1, 13, 14, 21, 22, 23, 24, 25};
    int pin = buttonPins[buttonNum];
    
    mockSetPinState(pin, LOW);
}

inline void mockReleaseButton(int buttonNum) {
    if (buttonNum < 1 || buttonNum > 7) {
        printf("[MOCK] Invalid button number: %d\n", buttonNum);
        return;
    }
    
    int buttonPins[] = {-1, 13, 14, 21, 22, 23, 24, 25};
    int pin = buttonPins[buttonNum];
    
    mockSetPinState(pin, HIGH);
}

inline int wiringPiSetup() {
    printf("[MOCK] WiringPi setup initialized\n");
    return 0;
}

inline void pinMode(int pin, int mode) {
    printf("[MOCK] Set pin %d to mode %s\n", pin, mode == INPUT ? "INPUT" : "OUTPUT");
    
    if (mode == INPUT) {
        mockSetPinState(pin, HIGH);
    }
}

inline void digitalWrite(int pin, int value) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    g_mockPinStates[pin] = value;
    printf("[MOCK] Write %s to pin %d\n", value == HIGH ? "HIGH" : "LOW", pin);
}

inline int digitalRead(int pin) {
    std::lock_guard<std::mutex> lock(g_pinMutex);
    auto it = g_mockPinStates.find(pin);
    int value = (it != g_mockPinStates.end()) ? it->second : HIGH;
    return value;
}

inline void delay(unsigned int howLong) {
    printf("[MOCK] Delaying for %d ms\n", howLong);
    std::this_thread::sleep_for(std::chrono::milliseconds(howLong));
}

#endif