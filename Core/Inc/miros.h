/*
 * miros.h
 *
 *  Created on: Feb 6, 2025
 *      Author: guilh
 */

#ifndef INC_MIROS_H_
#define INC_MIROS_H_

namespace rtos {
/* Thread Control Block (TCB) */
typedef struct {
    void *sp; /* stack pointer */
    uint32_t timeout; /* timeout delay down-counter */
    uint8_t id;  // guardar o índice da tarefa
    /* ... other attributes associated with a thread */
} OSThread;

/* Semáforo de contagem, sem busy-waiting */
typedef struct{
	uint32_t count;   // contador
	uint32_t waitSet; // bitmask das tarefas bloqueadas nesse semáforo
} OS_Semaphore;

const uint16_t TICKS_PER_SEC = 100U;

typedef void (*OSThreadHandler)();

void OS_init(void *stkSto, uint32_t stkSize);

/* callback to handle the idle condition */
void OS_onIdle(void);

/* this function must be called with interrupts DISABLED */
void OS_sched(void);

/* transfer control to the RTOS to run the threads */
void OS_run(void);

/* blocking delay */
void OS_delay(uint32_t ticks);

/* process all timeouts */
void OS_tick(void);

/* callback to configure and start interrupts */
void OS_onStartup(void);

void OSThread_start(
    OSThread *me,
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize);

/* ---------- Fase 1 APIs ---------- */

// cede voluntariamente a CPU para a próxima tarefa pronta
void OS_yield(void);

// inicializa um semáforo com um número inicial no contador
void OS_Semaphore_init(OS_Semaphore *sem, uint32_t initCount);

// tenta adquirir o semáforo e bloqueia a tarefa se count = 0
void OS_semWait(OS_Semaphore *sem);

// libera o semáforo e se houver tarefa bloqueada, desbloqueia uma
void OS_semSignal(OS_Semaphore *sem);

}

#endif /* INC_MIROS_H_ */
