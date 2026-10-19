#ifndef FSM_H
#define FSM_H

/* Modos operativos que gobiernan el LED y la adquisición del sensor. */
typedef enum {
    FSM_MODE_NORMAL = 0,
    FSM_MODE_BLUE,
    FSM_MODE_OFF,
} fsm_mode_t;

/* Devuelve el modo actual de la máquina de estados. */
fsm_mode_t fsm_get_mode(void);

#endif /* FSM_H */