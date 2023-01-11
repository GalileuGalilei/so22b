#include "stdlib.h"
#include "stdio.h"
#include "metricas.h"
#include "processo.h"

struct pross_metricas
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
};

pross_metricas* pross_metricas_cria(int quantum)
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
    self->quantum_media = quantum; //escalonador do processo mais curto
    self->last_bloq = 0;
    self->last_exec = 0;
    self->last_pron = 0;
    self->n_execucao = 0;

    return self;
}

//escreve as metricas de um processo em um arquivo .txt
void pross_log_metricas(pross_metricas* i, int programa)
{
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

        programa, i->t_retorno, i->t_bloqueado,
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

//essa função faz o teste para todas as métricas de um processo e as atualiza caso seja necessário
void atualiza_metricas(pross_metricas* metricas, processo_estado estado_atual, processo_estado estado_novo, int relCount)
{
    //primeira chamada
    if(estado_atual == 0 && estado_novo != 0)
    {
        metricas->t_retorno = relCount;
    }

    //última chamada
    if(estado_novo == 0)
    {
        metricas->t_retorno = relCount - metricas->t_retorno;
    }

    if(estado_novo == execucao)
    {
        metricas->last_exec = relCount;
        metricas->n_execucao++;
    }

    if(estado_atual == execucao && estado_novo != execucao)
    {
        metricas->quantum = relCount - metricas->last_exec;
        metricas->t_executando += metricas->quantum;
        metricas->quantum_media = (int)((float)metricas->t_executando / metricas->n_execucao);
    }

    if(estado_atual == execucao && estado_novo == pronto)
    {
        metricas->n_preempcoes++;
    }

    if(estado_novo == bloqueado)
    {
        metricas->n_bloqueios++;
        metricas->last_bloq = relCount;
    }

    if(estado_atual == bloqueado && estado_novo != bloqueado)
    {
        metricas->t_bloqueado += relCount - metricas->last_bloq;
        metricas->t_retorno_media = (int)((float)metricas->t_bloqueado / metricas->n_bloqueios);
    }

    if(estado_novo == pronto)
    {
        metricas->last_pron = relCount;
    }

    if(estado_atual == pronto && estado_novo != pronto)
    {
        metricas->t_pronto += relCount - metricas->last_pron;
    }
}

int pross_metricas_quantum(pross_metricas* metricas)
{
    return metricas->quantum;
}

int pross_metricas_quantum_reseta(pross_metricas* metricas)
{
    metricas->quantum = 0;
}

int pross_metricas_quantum_media(pross_metricas* metricas)
{
    return metricas->quantum_media;
}