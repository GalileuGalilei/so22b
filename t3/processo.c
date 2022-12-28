#include <stdlib.h>
#include <stdio.h>
#include "processo.h"
#include "so.h"
#include "tela.h"

#define QUANTUM 4 //4 - pequeno; 32 - grande
#define ESCALONADOR 1 //"round-robin" = 0; "processo mais curto" = 1 

typedef struct pross_metricas
{
    int t_retorno; //tempo total do processo
    int t_bloqueado;
    int t_executando;
    int t_pronto;
    int t_retorno_media; //media do tempo que ficou bloqueado
    int n_bloqueios;
    int n_preempcoes;
    int quantum;
    int quantum_media;

    //auxiliares
    int n_execucao;
    int last_exec;
    int last_bloq;
    int last_pron;
} pross_metricas;

struct processo 
{
  cpu_estado_t *cpue;
  processo_estado estado;
  so_chamada_t motivo_bloqueio;
  int complemento; //complemento do motivo do bloqueio
  int* mem_copia; //vai continuar aqui para quando faltar memória principal
  pross_metricas* metricas;

  int id;
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

pross_metricas* pross_metricas_cria()
{
    pross_metricas* self = (pross_metricas*)malloc(sizeof(pross_metricas));
    self->t_bloqueado = 0;
    self->t_executando = 0;
    self->n_bloqueios = 0;
    self->n_preempcoes = 0;
    self->t_pronto = 0;
    self->t_retorno = 0;
    self->t_retorno_media = 0;
    self->quantum = 0;
    self->quantum_media = QUANTUM; //escalonador do processo mais curto
    self->last_bloq = 0;
    self->last_exec = 0;
    self->last_pron = 0;
    self->n_execucao = 0;

    return self;
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

//escreve as metricas de um processo em um arquivo .txt
void pross_log_metricas(processo* pross)
{
    pross_metricas* i = pross->metricas;
    FILE* fp = fopen("log.txt", "a");
    feof(fp);

    fprintf(fp,
        "** programa: %i **\n"
        "tempo de retorno: %i \n"
        "tempo bloqueado: %i \n"
        "tempo executando: %i \n"
        "tempo pronto: %i \n"
        "tempo medio de retorno: %i \n"
        "numero de bloqueios: %i \n"
        "numero de preempcoes: %i \n\n",

        pross->n_programa, i->t_retorno, i->t_bloqueado,
        i->t_executando, i->t_pronto, i->t_retorno_media,
        i->n_bloqueios, i->n_preempcoes
        );

    fclose(fp);
}

void so_log_metricas(so_metricas* i)
{
    FILE* fp = fopen("log.txt", "a");
    feof(fp);
    fprintf(fp,
        "** metricas gerais **\n"
        "numero de chamadas de sistema: %i \n"
        "tempo total: %i \n"
        "tempo no modo zumbi: %i \n",
        i->n_siop, i->t_total, i->t_zumbi
        );

    fclose(fp);
}

void pross_libera(tabela_processos* tabela, processo* pross)
{
    pross_log_metricas(pross);
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

processo* pross_cria(int programa)
{
  processo* pross = (processo*)malloc(sizeof(processo));
  pross->cpue = cpue_cria();
  pross->mem_copia = (int*)malloc(MEM_TAM * sizeof(int));
  pross->metricas = pross_metricas_cria();
  pross->estado = 0; //estado inválido
  pross->next = NULL;
  pross->n_programa = programa;

  return pross;
}

//*****escalonadores*******

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

        if(i->metricas->quantum < QUANTUM)
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

        i->metricas->quantum = 0;
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

        if(i->metricas->quantum_media < curto->metricas->quantum_media)
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

//essa função faz o teste para todas as métricas de um processo e as atualiza caso seja necessário
void atualiza_metricas(processo* pross, processo_estado estado, int relCount)
{
    pross_metricas* metricas = pross->metricas;

    //primeira chamada
    if(pross->estado == 0 && estado != 0)
    {
        metricas->t_retorno = relCount;
    }

    //última chamada
    if(estado == 0)
    {
        metricas->t_retorno = relCount - metricas->t_retorno;
    }

    if(estado == execucao)
    {
        metricas->last_exec = relCount;
        metricas->n_execucao++;
    }

    if(pross->estado == execucao && estado != execucao)
    {
        metricas->quantum = relCount - metricas->last_exec;
        metricas->t_executando += metricas->quantum;
        metricas->quantum_media = (int)((float)metricas->t_executando / metricas->n_execucao);
    }

    if(pross->estado == execucao && estado == pronto)
    {
        metricas->n_preempcoes++;
    }

    if(estado == bloqueado)
    {
        metricas->n_bloqueios++;
        metricas->last_bloq = relCount;
    }

    if(pross->estado == bloqueado && estado != bloqueado)
    {
        metricas->t_bloqueado += relCount - metricas->last_bloq;
        metricas->t_retorno_media = (int)((float)metricas->t_bloqueado / metricas->n_bloqueios);
    }

    if(estado == pronto)
    {
        metricas->last_pron = relCount;
    }

    if(pross->estado == pronto && estado != pronto)
    {
        metricas->t_pronto += relCount - metricas->last_pron;
    }
}

void pross_altera_estado(tabela_processos* tabela, processo* pross, processo_estado estado, int relCount)
{
    atualiza_metricas(pross, estado, relCount);

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