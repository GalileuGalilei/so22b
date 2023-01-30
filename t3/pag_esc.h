#ifndef PAG_ESC
#define PAG_ESC

typedef struct pag_ptr pag_ptr;
typedef struct pag_fila pag_fila;

#include "mmu.h"
#include "tab_pag.h"

/// @brief escolhe uma página e a libera da memória principal, salvando na memória secundária caso necessário
/// @return número do quadro que foi liberado
int pag_fila_escalonador(pag_fila* self, mmu_t* mmu);

pag_fila* pag_fila_cria();

pag_ptr* pag_ptr_cria(tab_pag_t* tab, int pag, int* mem_copia);

void pag_fila_insere(pag_fila* self, pag_ptr* ptr);

void pag_fila_libera(pag_fila* self, pag_ptr* ptr, mmu_t* mmu);

int pag_fila_escalonador(pag_fila* self, mmu_t* mmu);

void pag_fila_libera_tabela(pag_fila* self, tab_pag_t* tab, mmu_t* mmu);

//usado para o escalonador NRU, reseta os bits de acessada das paginas válidas
void reseta_bit_acessado(pag_fila* self);

#endif