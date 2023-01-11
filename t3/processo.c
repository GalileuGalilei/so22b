#include <stdlib.h>
#include <stdio.h>
#include "processo.h"
#include "so.h"
#include "tela.h"
#include "metricas.h"

#define QUANTUM 32 //4 - pequeno; 32 - grande
#define ESCALONADOR 0 //"round-robin" = 0; "processo mais curto" = 1 

struct processo 
{
  cpu_estado_t *cpue;
  processo_estado estado;
  so_chamada_t motivo_bloqueio;
  int complemento; //complemento do motivo do bloqueio(por enquanto não serve pra nada)
  int* mem_copia; //no futuro, será usada para simular a memória secundária
  pross_metricas* metricas;
  tab_pag_t* tab_pags;

  int id; //num tenho certeza se vou usar isso
  int n_programa;
  struct processo* next;
};

struct tabela_processos
{
    processo* first;
    processo* last;
    processo* exec; //processo que está em execução no momento

    so_metricas* metricas; //metricas gerais do so
};


typedef struct ponteiro_pagina
{
    tab_pag_t* ptr;
    int pagina;
    struct ponteiro_pagina* next;
}ponteiro_pagina;

tabela_processos* pross_tabela_cria(so_metricas* metricas)
{
    tabela_processos* tabela = (tabela_processos*)malloc(sizeof(tabela_processos));
    tabela->metricas = metricas;
    tabela->exec = NULL;
    tabela->first = NULL;
    tabela->last = NULL;

    //limpa o arquivo de log a cada nova sessão
    FILE* fp = fopen("log.txt", "w");
    fclose(fp);

    return tabela;
}

processo* pross_cria(int programa, tab_pag_t* tab_pags)
{
  processo* pross = (processo*)malloc(sizeof(processo));
  pross->cpue = cpue_cria();
  pross->mem_copia = (int*)malloc(MEM_TAM * sizeof(int));
  pross->metricas = pross_metricas_cria(QUANTUM);
  pross->tab_pags = tab_pags;
  pross->estado = 0; //estado inválido
  pross->next = NULL;
  pross->n_programa = programa;

  return pross;
}

//insere um processo no inicio da fila
void pross_insere(tabela_processos* tabela, processo* pross)
{
    if(tabela->first == NULL)
    {
        tabela->last = pross;
        pross->id = 0;
        pross->next = NULL;
    }
    else
    {
        pross->id = tabela->first->id + 1;
        pross->next = tabela->first;
    }

    tabela->first = pross;
}

void pross_usa_tabela(mmu_t* mmu, processo* pross)
{
    mmu_usa_tab_pag(mmu, pross->tab_pags);
}

void pross_libera(tabela_processos* tabela, processo* pross)
{
    pross_log_metricas(pross, pross->n_programa);
    tab_pag_destroi(pross->tab_pags);
    free(pross->mem_copia);
    free(pross->cpue);
    free(pross->metricas);

    if(tabela->exec == pross)
    {
        tabela->exec = NULL;
    }

    if(tabela->first == pross)
    {
        tabela->first = pross->next;
        free(pross);
        return;
    }

    processo* i;
    for(i = tabela->first; i->next != pross; i = i->next) {}
    
    if(tabela->last == pross)
    {
        tabela->last = pross->next;
    }

    i->next = pross->next;
    free(pross);
}

//reseta o bit de escrita e acesso de todas as páginas
void reseta_bit_pags()
{
    
}
//*****escalonadores*******]



processo* round_robin(tabela_processos* tabela)
{
    processo* ant = NULL; //processo antes de i
    for(processo* i = tabela->first; i != NULL; i = i->next)
    {
        if(i->estado != pronto)
        {
            ant = i;
            continue;
        }

        if(pross_metricas_quantum(i) < QUANTUM)
        {
            return i;
        }

        //processos que tenham ultrapassodo o quantum, são jogados para o fim da fila
        if(i->next != NULL)
        {
            if(ant == NULL)
            {
                tabela->first = i->next;
            }
            else
            {
                ant->next = i->next;   
            }
            tabela->last->next = i;
            tabela->last = i;
            i->next = NULL;
        }

        pross_metricas_quantum_reseta(i);
    }

    return NULL;
}

processo* processo_mais_curto(tabela_processos* tabela)
{
    processo* curto = NULL;

    for (processo* i = tabela->first; i != NULL; i = i->next)
    {
        if(i->estado != pronto)
        {
            continue;
        }

        if(curto == NULL)
        {
            curto = i;
        }

        if(pross_metricas_quantum_media(i) < pross_metricas_quantum_media(curto))
        {
            curto = i;
        }
    }
    
    return curto;
}

processo* pross_acha_exec(tabela_processos* tabela)
{
    return tabela->exec;
}

processo* pross_escalonador(tabela_processos* tabela)
{
    processo* pross;

    if(ESCALONADOR == 0)
    {
        pross = round_robin(tabela);
    }
    else
    {
        pross = processo_mais_curto(tabela);
    }

    if(pross == NULL && tabela->first == NULL)
    {
        t_printf("Sem processos para executar");

        if(tabela->metricas != NULL)
        {
            so_log_metricas(tabela->metricas);
            tabela->metricas = NULL;
        }
    }

    return pross;
}

void pross_altera_estado(tabela_processos* tabela, processo* pross, processo_estado estado, int relCount)
{
    atualiza_metricas(pross->metricas ,pross->estado, estado, relCount);

    if(estado == execucao)
    {
        tabela->exec = pross;
    }
    else if(pross->estado == execucao)
    {
        tabela->exec = NULL;
    }

    pross->estado = estado;
}

void pross_copia_cpue(processo* pross, cpu_estado_t* cpue)
{
    cpue_copia(cpue, pross->cpue);
}

int* pross_copia_memoria(processo* pross)
{
    return pross->mem_copia;
}

cpu_estado_t* pross_cpue(processo* pross)
{
    return pross->cpue;
}

void pross_bloqueia(tabela_processos* tabela, processo* pross, so_chamada_t motivo, int complemento, int relCount)
{
  pross_altera_estado(tabela, pross, bloqueado, relCount);
  pross->motivo_bloqueio = motivo;
  pross->complemento = complemento;
}

//itera uma função de desbloqueio fornecida pelo so em todos os processos bloqueados da lista
void pross_desbloqueia_com_so(tabela_processos* tabela, so_t* so, void (*fp)(processo*, so_t*))
{
    for (processo* i = tabela->first; i != NULL; i = i->next)
    {
        if(i->estado != bloqueado)
        {
            continue;
        }

        fp(i, so);   
    }
}

processo* pross_acha_bloqueado(tabela_processos* tabela)
{
    for(processo* i = tabela->first; i != NULL; i = i->next)
    {
        if(i->estado == bloqueado)
        {
            return i;
        }
    }

    return NULL;
}

processo_estado pross_motivo_bloqueio(processo* pross)
{
    return pross->motivo_bloqueio;
}