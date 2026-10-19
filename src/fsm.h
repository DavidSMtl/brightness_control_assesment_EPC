#ifndef FSM_H
#define FSM_H

/* Operating modes that control the LED and sensor acquisition. */
typedef enum {
    FSM_MODE_NORMAL = 0,
    FSM_MODE_BLUE,
    FSM_MODE_OFF,
} fsm_mode_t;

/* Returns the current state machine mode. */
fsm_mode_t fsm_get_mode(void);

#endif /* FSM_H */