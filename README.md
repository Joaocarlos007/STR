# Projeto de Sistemas de Tempo Real

## Desenvolvimento de um RTOS para STM32G474RE

### Integrantes

|             Nome            | Matrícula |
| --------------------------  | --------- |
| João Carlos Willy Ziebell   | 23207404  |
| Giancarlo Baldo Cavanus     | 23101636  |
| Gabriel de Medeiros Bettini |  946872   |

---

# Objetivo

Este projeto tem como objetivo o desenvolvimento incremental de um Sistema Operacional de Tempo Real (RTOS) para a plataforma STM32G474RE, baseado no kernel educacional MiROS.

O projeto contempla a implementação de mecanismos de concorrência, sincronização, instrumentação por trace e escalonamento de tarefas periódicas em sistemas embarcados de tempo real.

---

# Ambiente de Desenvolvimento

## Hardware

* Placa STM32 NUCLEO-G474RE
* Microcontrolador STM32G474RE
* ARM Cortex-M4F

## Software

* STM32CubeIDE versão ________
* Compilador GCC ARM Embedded
* Renode versão ________
* Sistema Operacional ________

---

# Estrutura do Projeto

```text
Core/
├── Inc/
├── Src/
│   ├── main.cpp
│   ├── miros.cpp
│   └── ...
│
Startup/
Drivers/
```

## Organização dos Arquivos

### main.cpp

Responsável pela criação e execução das tarefas da aplicação.

### miros.cpp

Implementação do núcleo do RTOS:

* Escalonador
* Gerenciamento de threads
* Troca de contexto
* Temporização
* Serviços do sistema operacional

---

# Arquitetura do RTOS

## Componentes Principais

O RTOS foi estruturado nos seguintes módulos:

* Gerenciamento de tarefas
* Escalonador
* Temporização
* Sincronização
* Instrumentação de eventos (Trace)

---

# Estrutura das Threads

Cada tarefa é representada por uma estrutura do tipo:

```cpp
[PREENCHER COM A ESTRUTURA FINAL]
```

### Estados das Threads

* Ready
* Running
* Blocked
* Idle

---

# Troca de Contexto

A troca de contexto é realizada através da exceção PendSV do Cortex-M4.

## Fluxo

1. Scheduler seleciona a próxima tarefa.
2. PendSV é acionada.
3. Contexto da tarefa atual é salvo.
4. Contexto da próxima tarefa é restaurado.
5. Execução continua na nova tarefa.

---

# Fase 1 — Concorrência Cooperativa

## Objetivos

* Implementação de yield()
* Implementação de semáforos
* Bloqueio passivo de tarefas

## Funcionamento do Yield

[DESCREVER IMPLEMENTAÇÃO]

### Fluxograma

[INSERIR FIGURA]

---

## Semáforos

### Estrutura

```cpp
[PREENCHER]
```

### Operações

#### wait()

[DESCREVER]

#### signal()

[DESCREVER]

---

## Resultados Obtidos

### Testes Realizados

* [ ] Troca cooperativa entre tarefas
* [ ] Bloqueio correto
* [ ] Desbloqueio correto

### Evidências

[INSERIR IMAGENS]

---

# Fase 2 — Instrumentação e Trace

## Objetivos

Integrar o RTOS a uma ferramenta de visualização temporal para análise de eventos do sistema.

## Ferramenta Utilizada

* SEGGER SystemView / Trace Compass
* Método de comunicação: _______

---

## Eventos Instrumentados

* Criação de tarefas
* Troca de contexto
* Bloqueio
* Desbloqueio
* Interrupções
* Chamadas do sistema

---

## Pontos Instrumentados no Kernel

| Evento           | Função    |
| ---------------- | --------- |
| Context Switch   | PREENCHER |
| Semaphore Wait   | PREENCHER |
| Semaphore Signal | PREENCHER |
| Tick             | PREENCHER |

---

## Resultados

### Capturas de Trace

[INSERIR IMAGENS]

### Análise Temporal

[PREENCHER]

---

# Fase 3 — Produtor Consumidor

## Objetivos

Implementar comunicação segura entre tarefas através de um buffer FIFO compartilhado.

---

## Arquitetura

### Produtor

[DESCREVER]

### Consumidor

[DESCREVER]

### FIFO

```cpp
[PREENCHER]
```

---

## Sincronização

Semáforos utilizados:

| Semáforo | Função            |
| -------- | ----------------- |
| mutex    | Exclusão mútua    |
| empty    | Espaços livres    |
| full     | Itens disponíveis |

---

## Fluxograma

[INSERIR DIAGRAMA]

---

## Resultados

### Cenários Testados

* Buffer vazio
* Buffer cheio
* Operação normal

### Evidências

[INSERIR IMAGENS]

---

# Fase 4 — Escalonamento de Tarefas Periódicas

## Objetivos

Implementar um escalonador de tempo real baseado em:

* EDF (Earliest Deadline First)

ou

* RM (Rate Monotonic)

ou

* DM (Deadline Monotonic)

---

## Algoritmo Escolhido

[PREENCHER]

### Justificativa

[PREENCHER]

---

## Estrutura das Tarefas Periódicas

```cpp
[PREENCHER]
```

---

## Sistema de Temporização

### Tick do Sistema

[PREENCHER]

### Controle de Liberação das Tarefas

[PREENCHER]

---

## Scheduler

### Critério de Seleção

[PREENCHER]

### Fluxograma

[INSERIR DIAGRAMA]

---

## Resultados

### Cenários de Teste

* Tarefa periódica única
* Múltiplas tarefas periódicas
* Sobrecarga do processador

### Evidências

[INSERIR IMAGENS]

### Análise dos Resultados

[PREENCHER]

---

# Conclusão

[PREENCHER AO FINAL DO PROJETO]

---

# Referências

QUANTUM LEAPS. MiROS (Minimal Real-Time Operating System) for ARM Cortex-M. GitHub, 2024. Disponível em: [GitHub - QuantumLeaps/MiROS.](https://github.com/QuantumLeaps/MiROS)

STMICROELECTRONICS. RM0440: STM32G4 series advanced Arm®-based 32-bit MCUs. Rev. 9. Geneva: STMicroelectronics, 2025. Disponível em: [STMicroelectronics RM0440.](https://www.st.com/resource/en/reference_manual/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)

BUTTAZO, G. Hard Real-Time Computing Systems - Predictable Scheduling Algorithms and Applications. Kluwer Academic Publishers, 1997. ISBN: 0792399943.
