# MiROS RTOS — STM32G474RE
## Projeto de Sistemas de Tempo Real — Fases 1 a 4

**Plataforma:** STM32G474RE (NUCLEO-G474RE) — ARM Cortex-M4F @ 170 MHz com FPU  
**Linguagem:** C++20  
**Escalonador base:** MiROS (Minimal Real-Time Operating System) — adaptado e estendido

---

## Sumário

1. [Arquitetura Geral do Kernel](#1-arquitetura-geral-do-kernel)
2. [Estrutura de Pilha e Troca de Contexto](#2-estrutura-de-pilha-e-troca-de-contexto)
3. [Fase 1 — Cooperação Voluntária e Semáforos](#3-fase-1--cooperação-voluntária-e-semáforos)
4. [Fase 2 — Visualizador de Trace](#4-fase-2--visualizador-de-trace)
5. [Fase 3 — Produtor/Consumidor](#5-fase-3--produtorconsumidor)
6. [Fase 4 — Escalonador RM e Tarefas Periódicas](#6-fase-4--escalonador-rm-e-tarefas-periódicas)
7. [Modelo de Temporização Independente de Hardware](#7-modelo-de-temporização-independente-de-hardware)
8. [Análise das Linhas de Tempo](#8-análise-das-linhas-de-tempo)

---

## 1. Arquitetura Geral do Kernel

O kernel é baseado no MiROS, um micronúcleo didático portado para o STM32G474RE. A arquitetura é orientada a eventos de tick com escalonamento por bitmask.

### Componentes principais

```
┌─────────────────────────────────────────────────────┐
│                   Aplicação (main.cpp)               │
├─────────────────────────────────────────────────────┤
│              API do RTOS (miros.h)                   │
│  OS_delay | OS_yield | OS_semWait | OS_semSignal     │
│  OSPeriodicTask_start | OS_taskDone                  │
├──────────────────────┬──────────────────────────────┤
│    Escalonador RM    │     Subsistema de Semáforos   │
│    (OS_sched)        │     (OS_Semaphore)            │
├──────────────────────┴──────────────────────────────┤
│          Gerenciador de Ticks (OS_tick)              │
│     OS_tickCount | timeouts | ativação periódica     │
├─────────────────────────────────────────────────────┤
│         Troca de Contexto (PendSV_Handler)           │
│              Assembly ARM Cortex-M4                  │
├─────────────────────────────────────────────────────┤
│              Hardware STM32G474RE                    │
│          SysTick (100 Hz) | NVIC | FPU               │
└─────────────────────────────────────────────────────┘
```

### Estruturas de dados centrais

| Variável | Tipo | Função |
|---|---|---|
| `OS_readySet` | `uint32_t` (bitmask) | Bit N setado = tarefa N+1 pronta para executar |
| `OS_curr` | `OSThread*` | Ponteiro para a tarefa em execução atual |
| `OS_next` | `OSThread*` | Ponteiro para a tarefa escolhida pelo escalonador |
| `OS_thread[33]` | `OSThread*[]` | Array de TCBs registrados (índice 0 = idle) |
| `OS_tickCount` | `uint32_t` | Contador global de ticks — base do escalonamento periódico |

### TCB — Task Control Block

```cpp
class OSThread {
public:
    void    *sp;       // stack pointer — offset 0, acessado pelo PendSV em asm
    uint32_t timeout;  // contador de delay decrescente
    uint8_t  id;       // índice no array OS_thread[] (1..31); idle = 0
};
```

> **Restrição de layout:** `sp` deve ser o primeiro membro da classe. O `PendSV_Handler` em assembly acessa o stack pointer via `STR sp,[r1,#0x00]` — offset zero do objeto. Qualquer membro virtual ou reordenação quebraria a troca de contexto silenciosamente.

---

## 2. Estrutura de Pilha e Troca de Contexto

### Stack frame inicial (montado por `OSThread_start`)

Quando uma tarefa é criada, `OSThread_start()` monta manualmente um stack frame que imita o estado deixado pelo hardware após uma exceção. Isso permite que a primeira troca de contexto para essa tarefa funcione exatamente como qualquer outra troca subsequente.

### Fluxo de troca de contexto

```

O PendSV é configurado com prioridade `0xFF`. Isso garante que a troca de contexto física nunca interrompe uma ISR em execução. O PendSV fica pendente até que todas as interrupções de maior prioridade terminem. Sem isso, o salvamento de R4-R11 poderia corromper o contexto de uma ISR em andamento.

### `extern "C"` e `__attribute__((naked))`

```cpp
extern "C" __attribute__((naked, optimize("-fno-stack-protector")))
void PendSV_Handler(void) { ... }
```

- **`extern "C"`:** impede o name mangling do C++. Sem isso, o compilador gera o símbolo `_Z14PendSV_Handlerv` em vez de `PendSV_Handler`, quebrando a referência na tabela de vetores de interrupção do arquivo de startup em assembly.
- **`naked`:** suprime prólogo e epílogo automáticos do compilador. Necessário porque o handler gerencia a pilha manualmente em assembly — qualquer instrução automática de `PUSH`/`POP` corromperia o contexto.
- **`optimize("-fno-stack-protector")`:** desabilita o stack canary nessa função, que também quebraria a manipulação manual da pilha.

---

## 3. Fase 1 — Cooperação Voluntária e Semáforos

### 3.1 `OS_yield()`

Permite que uma tarefa ceda voluntariamente a CPU sem sair do `OS_readySet`. Diferentemente do bloqueio, a tarefa permanece pronta e é candidata na próxima decisão do escalonador.

```cpp
void OS_yield(void) {
    __disable_irq();
    OS_sched();      // escolhe próxima tarefa e pende PendSV
    __enable_irq();  // PendSV executa aqui
}
```

**Diferença entre yield e delay:**

| | `OS_yield()` | `OS_delay(n)` |
|---|---|---|
| Sai do `OS_readySet`? | Não | Sim |
| Pode ser escolhida no próximo tick? | Sim | Não (até timeout) |
| Consome CPU enquanto espera? | Não (cede imediatamente) | Não (bloqueada) |

### 3.2 Semáforos sem busy-waiting

A implementação reutiliza a mesma representação bitmask do `OS_readySet`. Cada semáforo possui um `waitSet` representando as tarefas bloqueadas naquele recurso.

```cpp
class OS_Semaphore {
public:
    uint32_t count;    // créditos disponíveis
    uint32_t waitSet;  // bitmask: bit (id-1) = tarefa bloqueada aqui
};
```

### Diagrama de estados da tarefa

```
                    OS_semWait (count=0)
                   ┌─────────────────────┐
                   ▼                     │
┌──────────┐  tick/sched  ┌─────────────┐  OS_semWait   ┌──────────┐
│  PRONTA  │─────────────▶│  EXECUTANDO │──(count=0)───▶│ BLOQUEADA│
│(readySet)│◀─────────────│             │               │(waitSet) │
└──────────┘  preempção   └─────────────┘               └──────────┘
      ▲                         │                              │
      │                         │ OS_delay(n)                  │ OS_semSignal
      │                   ┌─────▼──────┐                       │
      └───────────────────│  BLOQUEADA │◀──────────────────────┘
        timeout zerado     │ (timeout)  │
                           └────────────┘
```

### `OS_semWait()` — caminho de bloqueio

```
tarefa chama OS_semWait(sem)
        │
        ├─ count > 0? ──SIM──▶ count-- → continua executando
        │
        └─ count = 0?
                │
                ├─ bit = (1 << (OS_curr->id - 1))
                ├─ OS_readySet  &= ~bit   ← tarefa sai do readySet
                ├─ sem->waitSet |=  bit   ← tarefa entra no waitSet do semáforo
                └─ OS_sched()            ← outra tarefa é despachada imediatamente
```

### `OS_semSignal()` — desbloqueio

```
tarefa chama OS_semSignal(sem)
        │
        ├─ waitSet != 0? ──SIM──▶ isola bit de menor índice (& -waitSet)
        │                          OS_readySet |= bit_da_tarefa_acordada
        │                          OS_sched()   ← possível troca imediata
        │
        └─ waitSet == 0? ──SIM──▶ count++   ← devolve crédito
```

---

## 4. Fase 2 — Visualizador de Trace

### Mecanismo de coleta

A coleta de trace é feita em memória RAM via buffer circular de 10.000 entradas, sem dependência de periféricos externos. Cada evento grava dois bytes: tipo e id da tarefa.

```cpp
struct TraceEvent {
    uint8_t tipo;  // 0=ctx_switch, 1=isr_enter, 2=isr_exit,
                   // 3=sem_block,  4=sem_unblock, 5=delay, 6=yield
    uint8_t id;    // id da tarefa (0xFF para ISR)
};
TraceEvent log_tarefas[10000];
```

### Pontos de instrumentação

| Ponto | Evento gravado | Localização |
|---|---|---|
| Troca de contexto | `tipo=0, id=OS_next->id` | `OS_sched()` |
| Entrada no SysTick | `tipo=1, id=0xFF` | `SysTick_Handler` |
| Saída do SysTick | `tipo=2, id=0xFF` | `SysTick_Handler` |
| Bloqueio em semáforo | `tipo=3, id=OS_curr->id` | `OS_semWait()` |
| Desbloqueio de semáforo | `tipo=4, id=woken_id` | `OS_semSignal()` |
| Chamada de OS_delay | `tipo=5, id=OS_curr->id` | `OS_delay()` |
| Chamada de OS_yield | `tipo=6, id=OS_curr->id` | `OS_yield()` |

### Procedimento de dump e visualização

1. Rodar o firmware em modo debug no STM32CubeIDE
2. Pausar a execução após alguns segundos
3. Exportar a região de memória de `log_tarefas` como arquivo binário (`meu_trace.bin`)
4. Executar o visualizador: `python plot_rtos.py`

---

## 5. Fase 3 — Produtor/Consumidor

### Ordem obrigatória dos semáforos

A ordem de aquisição é crítica para evitar deadlock:

| Tarefa | Ordem correta | Consequência da inversão |
|---|---|---|
| Produtor | `wait(semVagas)` → `wait(semMutex)` | Dormiria segurando o mutex → deadlock |
| Consumidor | `wait(semItens)` → `wait(semMutex)` | Idem |

### Comportamento dinâmico

- **Produtor mais rápido** (delay 250ms) que o consumidor (delay 500ms): buffer enche progressivamente até `ocupacao = 8`, momento em que o produtor bloqueia em `semVagas`
- **Consumidor drena o buffer**: ao consumir, sinaliza `semVagas`, desbloqueando o produtor
- Ambos os cenários de bloqueio (buffer cheio e buffer vazio) são demonstráveis via `ocupacao` no Live Expressions

---

## 6. Fase 4 — Escalonador RM e Tarefas Periódicas

### Princípio do Rate Monotonic

Rate Monotonic é um algoritmo de escalonamento de prioridade fixa ótimo para tarefas periódicas independentes: a tarefa com menor período recebe a maior prioridade. A prioridade é atribuída na inicialização e nunca muda em tempo de execução.

**Teste de escalonabilidade de Liu & Layland:**

$$U = \sum_{i=1}^{n} \frac{C_i}{T_i} \leq n(2^{1/n} - 1)$$

Para as 3 tarefas implementadas:

| Tarefa | Período (T) | WCET estimado (C) | Utilização (C/T) |
|---|---|---|---|
| T1 | 200 ticks (2s) | ~3ms | ~0,0015 |
| T2 | 300 ticks (3s) | ~3ms | ~0,001  |
| T3 | 500 ticks (5s) | ~3ms | ~0,0006 |
| **Total** | | | **≈ 0,003** |

Utilização muito abaixo do limite de 0,779 (sistema escalonável com folga ampla).

### Seleção de prioridade por bitmask O(1)

A posição do bit em `OS_readySet` codifica diretamente a prioridade RM: tarefas com menor período são registradas com menores índices, ocupando bits menos significativos. A seleção da tarefa de maior prioridade é feita com uma única operação aritmética:

```cpp
uint32_t highestBit = ready & (-ready);  // isola o bit menos significativo
```

Em complemento de dois, `-ready` inverte todos os bits e soma 1, o que tem o efeito de preservar apenas o bit menos significativo de `ready`. Resultado: O(1), sem loop, sem array de prioridades separado.

```
OS_readySet = 0b...00000110  (T2 e T3 prontas, T1 não)
-OS_readySet = 0b...11111010
ready & (-ready) = 0b...00000010  ← bit 1 = T2 (maior prioridade entre as prontas)
```

### `OSPeriodicTask` — estrutura de dados

```cpp
class OSPeriodicTask {
public:
    OSThread thread;          // TCB — DEVE ser o primeiro membro (offset 0 para PendSV)
    uint32_t period;          // período em ticks
    uint32_t nextActivation;  // valor de OS_tickCount na próxima ativação
    uint8_t  priority;        // prioridade RM (= índice de registro)
    bool     active;          // true durante execução do ciclo atual
};
```

### Ciclo de vida de uma tarefa periódica

```
OSPeriodicTask_start()
        │
        ├─ Registra em OS_periodicTasks[]
        ├─ Define period e nextActivation = phase + period
        ├─ Chama OSThread_start() internamente
        └─ Remove do OS_readySet  ← nasce bloqueada

        [OS_tickCount incrementa a cada tick]
        │
OS_tick() detecta OS_tickCount >= nextActivation
        │
        ├─ nextActivation += period   ← agenda próxima ativação
        └─ OS_readySet |= bit         ← tarefa acorda

        [tarefa executa seu trabalho]
        │
OS_taskDone()
        │
        ├─ OS_readySet &= ~bit        ← tarefa dorme
        └─ OS_sched()                 ← outra tarefa assume
```

### Verificação de prioridade RM

Quando T1 e T2 ativam simultaneamente (a cada 6 segundos = LCM(200, 300)):

```
OS_tick() no tick 600:
  T1: OS_tickCount(600) >= nextActivation(600) → OS_readySet |= bit0
  T2: OS_tickCount(600) >= nextActivation(600) → OS_readySet |= bit1

OS_readySet = 0b...00000011

OS_sched():
  ready & (-ready) = 0b...00000001  ← bit0 = T1 escolhida
  T1 executa antes de T2  ✓
```

---

## 7. Modelo de Temporização Independente de Hardware

A restrição arquitetural da Fase 4 exige que nenhum timer físico específico do STM32 seja referenciado no código do escalonador. A solução adotada é uma camada de abstração de tempo por software:

```
Hardware (SysTick — periférico do núcleo ARM, não do STM32)
        │
        │ ISR a cada 10ms
        ▼
SysTick_Handler()
        │
        └─ OS_tick()          ← única interface entre hardware e escalonador
                │
                ├─ OS_tickCount++
                ├─ Verifica nextActivation de cada OSPeriodicTask
                └─ Atualiza OS_readySet
```

`OS_tick()` recebe apenas o incremento de um contador. Para portar para outro microcontrolador, basta garantir que alguma ISR periódica chame `OS_tick()` na frequência desejada. O código de `OS_sched()`, `OSPeriodicTask_start()` e `OS_taskDone()` não muda uma linha.

---

## 8. Análise das Linhas de Tempo

### Fase 2 — Trace de troca de contexto

O gráfico gerado pelo `plot_rtos.py` a partir do `meu_trace.bin` demonstra:

- **blinky1 e blinky2** alternando regularmente, proporcionalmente aos seus delays
- **blinky3** com aparições mais esparsas (delay maior)
- **idle** dominando o início antes das tarefas entrarem em regime
- **ISR (SysTick)** visível como pulsos periódicos entre as trocas de contexto

A linha do tempo confirma a sincronia entre a execução física e o log capturado: cada barra no gráfico corresponde diretamente a um evento de troca de contexto registrado no buffer de memória e descarregado via debugger.

### Fase 4 — Padrão periódico

Com o firmware da Fase 4, o padrão esperado no trace é fundamentalmente diferente:

- **Longos períodos de idle** entre as ativações periódicas
- **Pulsos curtos de T1** a cada 200 eventos de tick
- **Pulsos de T2** a cada 300, **T3** a cada 500
- **Quando T1 e T2 coincidem:** T1 aparece primeiro, confirmando a prioridade RM

---

## 9. Uso de Ferramentas de IA no Desenvolvimento

- **Fase 1:** correção do código de OS_semWait/OS_semSignal e depuração da lógica de bitmask (waitSet, OS_readySet).
- **Fase 2:** geração do script plot_rtos.py e definição da estrutura TraceEvent.
- **Fase 3:** identificação da ordem correta dos semáforos para evitar deadlock e correção do código do produtor/consumidor.
- **Fase 4:** geração da estrutura OSPeriodicTask, código de OS_tick() e verificação do teste de escalonabilidade de Liu & Layland.
- **README:** auxílio na elaboração do documento.

---

## Dependências 

- **Visualizador:** Python 3.x com `matplotlib` e `struct` (stdlib)

```bash
# Instalar dependência do visualizador
pip install matplotlib

# Executar visualizador após dump do trace
python plot_rtos.py
```
