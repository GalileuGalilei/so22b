#ifndef METRICAS
#define METRICAS
#include "so.h"
#include "processo.h"

typedef struct pross_metricas pross_metricas;

pross_metricas* pross_metricas_cria(int quantum);

void pross_log_metricas(pross_metricas* i, int programa);

void so_log_metricas(so_metricas* i);

int pross_metricas_quantum(pross_metricas* metricas);

void pross_metricas_quantum_reseta(pross_metricas* metricas);

int pross_metricas_quantum_media(pross_metricas* metricas);

void atualiza_metricas(pross_metricas* metricas, processo_estado estado_atual, processo_estado estado_novo, int relCount);

#endif