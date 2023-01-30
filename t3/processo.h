#ifndef _PROCESSO
#define _PROCESSO

#include "so.h"

typedef struct processo processo;
typedef struct tabela_processos tabela_processos;

typedef enum
{
  pross_leitura = 1,
  pross_escrita,
  pross_pagfalt,
} pross_bloqueio;


typedef enum
{
  execucao = 1,
  pronto,
  bloqueado,
} processo_estado;

tabela_processos* pross_tabela_cria(so_metricas* metricas);

processo* pross_cria(int programa, tab_pag_t* tab_pags, int* mem_copia);

void pross_insere(tabela_processos* tabela, processo* pross);

void pross_usa_tabela(mmu_t* mmu, processo* pross);

void pross_libera(tabela_processos* tabela, processo* pross);

void pross_altera_estado(tabela_processos* tabela, processo* pross, processo_estado estado, int relCount);

tab_pag_t* pross_tab_pag(processo* pross);

/// @brief carrega um pagina para dentro da memoria principal
/// @param quadro quadro livre da memória aonde a pagina será carregada
void pross_carrega_pagina(processo* pross, mem_t* mem, int pag, int quadro);

/// @param pross processo que tera sua cpue alterada
/// @param cpue cpue a ser copiada
void pross_copia_cpue(processo* pross, cpu_estado_t* cpue);

/// @brief retorna um ponteiro para toda memoria de um processo
int* pross_copia_memoria(processo* pross);

cpu_estado_t* pross_cpue(processo* pross);

/// @brief encontra o último processo que estava em execução
/// @param tabela tabela de processos
/// @return processo em execução
processo* pross_acha_exec(tabela_processos* tabela);

processo* pross_escalonador(tabela_processos* tabela);

/// @brief bloqueia o último processo que estava em execução
void pross_bloqueia(tabela_processos* tabela, processo* pross, pross_bloqueio motivo, int complemento, int relCount);

//itera uma função de desbloqueio fornecida pelo so em todos os processos bloqueados da lista
void pross_desbloqueia_com_so(tabela_processos* tabela, so_t* so, void (*fp)(so_t*, processo*));

processo* pross_acha_bloqueado(tabela_processos* tabela);

pross_bloqueio pross_motivo_bloqueio(processo* pross, int* complemento);

#endif