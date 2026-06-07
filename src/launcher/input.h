#ifndef FRIJ_INPUT_H
#define FRIJ_INPUT_H

/*
 * Thin input layer for the Back action.
 *
 *   emulator : a keyboard key (Backspace) calls frij_back()
 *   device   : TODO read the hardware button GPIO
 *
 * The launcher's UX logic never knows where the press came from.
 * Call once, after the launcher has started.
 */
void frij_input_init(void);

#endif  // FRIJ_INPUT_H
