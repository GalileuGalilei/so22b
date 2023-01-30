#include <stdlib.h>
#include "pag_esc.h"
#include "tab_pag.h"
#include "mmu.h"
#include "so.h"
#include "tela.h"

#define ESCALONADOR_PAG 0 // 0 = fifo, 1 = nru

//estrutura auxiliar para manter as páginas em forma de lista
struct pag_ptr
{
  int pag;
  tab_pag_t* tab_pag;
  int* mem_copia;
  int classe; //variável auxiliar para a nru

  struct pag_ptr* next;
};

struct pag_fila
{
  pag_ptr* first;
  pag_ptr* last;
};

pag_fila* pag_fila_cria()
{
    pag_fila* self = (pag_fila*)malloc(sizeof(pag_fila));
    self->first = NULL;
    self->last = NULL;

    return self;
}

pag_ptr* pag_ptr_cria(tab_pag_t* tab, int pag, int* mem_copia)
{
    pag_ptr* self = (pag_ptr*)malloc(sizeof(pag_ptr));
    self->tab_pag = tab;
    self->pag = pag;
    self->mem_copia = mem_copia;
    self->next = NULL;
    
    return self;
}

void pag_fila_insere(pag_fila* self, pag_ptr* ptr)
{
    if(self->first == NULL)
    {
        self->last = ptr;
        ptr->next = NULL;
    }
    else
    {
        ptr->next = self->first;
    }

    self->first = ptr;
}

void salva_pag(pag_ptr* ptr, int* mem_copia, mmu_t* mmu)
{
    tab_pag_t* aux = mmu_tab_atual(mmu);

    int v_end = ptr->pag*PAG_TAM;
    mmu_usa_tab_pag(mmu, ptr->tab_pag);
    
    for (int i = v_end; i < PAG_TAM + v_end; i++)
    {
        err_t err =  mmu_le(mmu, i, &mem_copia[i]);
        if(err != ERR_OK)
        {
            t_printf("ERRO:: %s no enredeco %i ", err_nome(err), err, i);
        }
    }

    mmu_usa_tab_pag(mmu, aux);
}

void pag_fila_libera(pag_fila* self, pag_ptr* ptr, mmu_t* mmu)
{
    //salva o conteúdo da página na "memória secundária", caso ela tenha sido alterada
    if(tab_pag_alterada(ptr->tab_pag, ptr->pag))
    {
        salva_pag(ptr, ptr->mem_copia, mmu);
    }

    if(self->first == ptr)
    {
        if(self->last == self->first)
        {
            self->last = NULL;
        }

        self->first = ptr->next;

        tab_pag_muda_valida(ptr->tab_pag, ptr->pag, false);
        free(ptr);
        return;
    }

    pag_ptr* i;
    int count = 1;
    for(i = self->first; i->next != ptr; i = i->next) {count++;}

    if(self->last == ptr)
    {
        self->last = i;
    }

    i->next = ptr->next;
    tab_pag_muda_valida(ptr->tab_pag, ptr->pag, false);
    free(ptr);
}

void pag_fila_libera_tabela(pag_fila* self, tab_pag_t* tab, mmu_t* mmu)
{
    pag_ptr* aux = NULL;
    for (pag_ptr* i = self->first; i != NULL;)
    {
        if(i->tab_pag == tab)
        {
            aux = i->next;
            pag_fila_libera(self, i, mmu);

            i = aux;
            continue;
        }
        i = i->next;
    }
}



//usado para o escalonador NRU, reseta os bits de acessada das paginas válidas
void reseta_bit_acessado(pag_fila* self)
{
    for(pag_ptr* i = self->first; i != NULL; i = i->next)
    {
        tab_pag_muda_acessada(i->tab_pag, i->pag, false);
    }
}

void calcula_nru_classes(pag_fila* self)
{
    for(pag_ptr* i = self->first; i != NULL; i = i->next)
    {
        bool acessada = tab_pag_acessada(i->tab_pag, i->pag);
        bool alterada = tab_pag_alterada(i->tab_pag, i->pag);
        if(!acessada && !alterada)
        {
            i->classe = 0;
        }

        if(!acessada && alterada)
        {
            i->classe = 1;
        }

        if(acessada && !alterada)
        {
            i->classe = 2;
        }

        if(acessada && alterada)
        {
            i->classe = 3;
        }
    }
}

//baseado na descrição do livro, tenta-se pegar arbitrariamente a página de menor classe
int escalonador_nru(pag_fila* self, mmu_t* mmu)
{
    if(self->first == NULL)
    {
        t_printf("ERRO: não há nenhuma página sendo usada");
        return -1;
    }

    calcula_nru_classes(self);

    pag_ptr* menor = self->first;

    for(pag_ptr* i = self->first; i != NULL; i = i->next)
    {
        if(i->classe <= menor->classe)
        {
            menor = i;
        }
    }

    int quadro = tab_pag_quadro(menor->tab_pag, menor->pag);
    pag_fila_libera(self, menor, mmu);

    return quadro;
}

int escalonador_fifo(pag_fila* self, mmu_t* mmu)
{
    if(self->first == NULL)
    {
        t_printf("ERRO: não há nenhuma página sendo usada");
        return -1;
    }

    pag_ptr* last = self->last;
    
    int quadro = tab_pag_quadro(last->tab_pag, last->pag);
    pag_fila_libera(self, last, mmu);

    return quadro;
}

//por enquanto só pega o ultimo da fila e libera
int pag_fila_escalonador(pag_fila* self, mmu_t* mmu)
{
    if(ESCALONADOR_PAG == 0)
    {
        return escalonador_fifo(self, mmu);
    }
    else
    {
        return escalonador_nru(self, mmu);
    }
}