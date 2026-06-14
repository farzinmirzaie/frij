#ifndef FRIJ_INPUT_H
#define FRIJ_INPUT_H

/*
 * Thin input layer for the hardware buttons, emulator side.
 *
 *   emulator : polls SDL keys — Backspace/Esc = Back (tap) / home (hold),
 *              Space = Frij AI push-to-talk.
 *   device   : the same actions come from M5.BtnA/M5.BtnB in src/main.cpp,
 *              not this file.
 *
 * The launcher's UX logic never knows where the press came from.
 * Call once, after the launcher has started.
 */
void frij_input_init(void);

#endif  // FRIJ_INPUT_H
