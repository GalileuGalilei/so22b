#include <stdlib.h>
#include "pag_esc.h"
#include "tab_pag.h"
#include "mmu.h"
#include "so.h"
#include "tela.h"

//estrutura auxiliar para manter as páginas em forma de lista
struct pag_ptr
{
  int pag;
  tab_pag_t* tab_pag;
  int* mem_copia;

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
    int v_end = ptr->pag*PAG_TAM;
    mmu_usa_tab_pag(mmu, ptr->tab_pag);

    for (int i = v_end; i < PAG_TAM + v_end; i++)
    {
        mmu_le(mmu, i, &mem_copia[i]);
    }
}

void pag_fila_libera(pag_fila* self, pag_ptr* ptr, mmu_t* mmu)
{
    //salva o conteúdo da página na "memória secundário, caso ela tenha sido alterada"
    if(tab_pag_alterada(ptr->tab_pag, ptr->pag))
    {
        salva_pag(ptr, ptr->mem_copia, mmu);
    }

    if(self->first == ptr)
    {
        self->first = ptr->next;
        tab_pag_muda_valida(ptr->tab_pag, ptr->pag, false);
        free(ptr);
        return;
    }

    pag_ptr* i;
    for(i = self->first; i->next != ptr; i = i->next) {}
    
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
    pag_ptr* anterior = NULL;
    for (pag_ptr* i = self->first; i != NULL; i = i->next)
    {
        if(i->tab_pag == tab)
        {
            pag_fila_libera(self, i, mmu);

            i = anterior;
            if(anterior == NULL)
            {
                i = self->first;

                if(i == NULL)
                {
                    return;
                }
            }
        }
        anterior = i;
    }
}

//por enquanto só pega o ultimo da fila e libera
int pag_fila_escalonador(pag_fila* self, mmu_t* mmu)
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