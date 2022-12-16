#ifndef _PROCESSO
#define _PROCESSO

#include "so.h"

typedef struct processo processo;
typedef struct tabela_processos tabela_processos;

typedef enum
{
  execucao = 0,
  pronto,
  bloqueado,
} processo_estado;


tabela_processos* pross_tabela_cria();

processo* pross_cria();

void pross_insere(tabela_processos* tabela, processo* pross);

void pross_libera(tabela_processos* tabela, processo* pross);

void pross_altera_estado(tabela_processos* tabela, processo* pross, processo_estado estado);

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

/// @brief futuro escalonador
processo* pross_acha_pronto(tabela_processos* tabela);

/// @brief bloqueia o último processo que estava em execução
void pross_bloqueia(processo* pross, so_chamada_t motivo, int complemento);

void pross_desbloqueia(processo* pross);

processo* pross_acha_bloqueado(tabela_processos* tabela);

processo_estado pross_motivo_bloqueio(processo* pross);

#endif