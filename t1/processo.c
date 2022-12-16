#include "processo.h"
#include "so.h"
#include <stdlib.h>

struct processo 
{
  cpu_estado_t *cpue;
  processo_estado estado;
  so_chamada_t motivo_bloqueio;
  int complemento; //complemento do motivo do bloqueio
  int* mem_copia;

  int id;
  struct processo* next;
};

struct tabela_processos
{
    processo* lista;
    processo* exec; //processo que está em execução no momento;
};


tabela_processos* pross_tabela_cria()
{
    tabela_processos* tabela = (tabela_processos*)malloc(sizeof(tabela_processos));
    tabela->lista = NULL;

    return tabela;
}

void pross_insere(tabela_processos* tabela, processo* pross)
{
    if(tabela->lista == NULL)
    {
        tabela->lista = pross;
        return;
    }

    processo* i;
    for (i = tabela->lista; i->next != NULL; i = i->next) {}

    i->next = pross;
    pross->id = i->id + 1;
}

void pross_libera(tabela_processos* tabela, processo* pross)
{
    free(pross->mem_copia);
    free(pross->cpue);

    if(tabela->exec == pross)
    {
        tabela->exec = NULL;
    }

    if(tabela->lista == pross)
    {
        tabela->lista = pross->next;
        free(pross);
        return;
    }

    processo* i;
    for(i = tabela->lista; i->next != pross; i = i->next) {}
    
    i->next = pross->next;
    free(pross);
}

processo* pross_cria()
{
  processo* pross = (processo*)malloc(sizeof(processo));
  pross->cpue = cpue_cria();
  pross->mem_copia = (int*)malloc(MEM_TAM * sizeof(int));
  pross->estado = execucao;
  pross->next = NULL;

  return pross;
}

processo* pross_acha_exec(tabela_processos* tabela)
{
    return tabela->exec;
}

processo* pross_acha_pronto(tabela_processos* tabela)
{
  for(processo* i = tabela->lista; i->next != NULL; i = i->next)
  {
    if(i->estado == pronto)
    {
        return i;
    }
  } 

  return NULL; 
}

void pross_altera_estado(tabela_processos* tabela, processo* pross, processo_estado estado)
{
    pross->estado = estado;

    if(estado == execucao)
    {
        tabela->exec = pross;
        return;
    }

    if(tabela->exec == pross && estado != execucao)
    {
        tabela->exec = NULL;
    }
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

void pross_bloqueia(processo* pross, so_chamada_t motivo, int complemento)
{
  pross->estado = bloqueado;
  pross->motivo_bloqueio = motivo;
  pross->complemento = complemento;
}

void pross_desbloqueia(processo* pross)
{
    pross->estado = pronto;
}

processo* pross_acha_bloqueado(tabela_processos* tabela)
{
    for(processo* i = tabela->lista; i->next != NULL; i = i->next)
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
    return pross->estado;
}