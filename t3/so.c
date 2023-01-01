#include "so.h"
#include "tela.h"
#include "rel.h"
#include "processo.h"
#include <stdlib.h>
#define PAG_TAM 32 //num sei

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  tabela_processos* tabela;
  int relCount; //número total de interrupções do sistema
  so_metricas* metricas;
  char* quadros; //array de booleano que indica se um quadro está livre
};

//t1 - processos

void carrega_pagina(so_t* self, tab_pag_t* tab, int pag, int* data)
{
  mmu_t* mmu = contr_mmu(self->contr);
  mmu_usa_tab_pag(mmu, tab);
  int end_virtual = pag * PAG_TAM;

  for(int i = end_virtual; i < end_virtual + PAG_TAM; i++)
  {
    mmu_escreve(mmu, i, data[i - end_virtual]);
  }
}

//cria uma nova tabela de páginas e carrega um programa para dentro dela
tab_pag_t* carrega_tabela(so_t* self, int* programa, int tam)
{
  int num_pag = (int)(tam / PAG_TAM + 1);
  int num_quadros = (int)(MEM_TAM / PAG_TAM + 1);
  tab_pag_t* tab = tab_pag_cria(num_pag, PAG_TAM);

  int i;
  int j;
  for(i = 0, j = 0; i < num_quadros && j < num_pag; i++)
  {
    //esse loop parte da assumição de que haverá quadros livres o suficiente
    if(self->quadros[i] == 0)
    {
      tab_pag_muda_quadro(tab, j, i);
      self->quadros[i] = 1;
      j++;
    }
  }

  t_printf("DEBUG: carregando programa de tam %i de %i paginas", tam, num_pag);
  for(int i = 0; i < num_pag; i++)
  {
    carrega_pagina(self, tab, i, programa + i * PAG_TAM);
  }

  return tab;
}

void carrega_processo(so_t* self, processo* pross)
{
  pross_altera_estado(self->tabela, pross, execucao, self->relCount);
  cpue_copia(pross_cpue(pross), self->cpue);
  cpue_muda_modo(self->cpue, usuario);

  mmu_t* mmu = contr_mmu(self->contr);

  //por enquanto, só isso chega
  pross_usa_tabela(mmu, pross);
}

//procura um processo para executar, se não achar, inicia o modo zumbi(e anota isso nas metricas)
void carrega_pronto(so_t* self)
{
  processo* pross = pross_escalonador(self->tabela);

  if(pross == NULL)
  {
    if(cpue_modo(self->cpue) != zumbi) 
    {
      self->metricas->last_zumbi = self->relCount;
    }

    t_printf("DEBUG: modo zumbi");
    cpue_muda_modo(self->cpue, zumbi);
    exec_altera_estado(contr_exec(self->contr), self->cpue);
    return;
  }

    if(cpue_modo(self->cpue) == zumbi)
    {
      self->metricas->t_zumbi += self->relCount - self->metricas->last_zumbi;
    }

  carrega_processo(self, pross);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

//todo: refatorar isso aqui pra salvar uma página pra dentro da memória de um processo(eu acho)
void salva_memx(so_t* self, int* mem_copia)
{
  contr_t* contr = self->contr;
  mem_t* mem = contr_mem(contr);
  int tam = mem_tam(mem);
  int valor;

  for(int i = 0; i < tam; i++)
  {
    mem_le(mem, i, &valor);
    mem_copia[i] = valor;
  }
}

tab_pag_t* carrega_programa(so_t* self, int program)
{
    if(program == 0)
    {
      int progr[] = { 
        #include "init.maq" 
        };
      return carrega_tabela(self, progr, sizeof(progr));
    }

    if(program == 1)
    {
      int progr[] = { 
        #include "p1.maq" 
        };
      return carrega_tabela(self, progr, sizeof(progr));
    }

    if(program == 2)
    {
      int progr[] = { 
        #include "p2.maq" 
        };
      return carrega_tabela(self, progr, sizeof(progr));
    }

  t_printf("ERRO: tentativa de carregar um programa inexistente");
  return NULL;
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self)
{
  processo* pross = pross_acha_exec(self->tabela);
  contr_t* contr = self->contr;
  exec_t* exec = contr_exec(contr);

  if(pross != NULL)
  {
    pross_altera_estado(self->tabela, pross, pronto, self->relCount);
    cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
    pross_copia_cpue(pross, self->cpue);
    //salva_mem(self, pross_copia_memoria(pross));
  }

  int programa = cpue_A(self->cpue);
  tab_pag_t* tab = carrega_programa(self, programa);
  t_printf("programa %i", programa);

  pross = pross_cria(programa, tab);
  pross_insere(self->tabela, pross);
  pross_altera_estado(self->tabela, pross, execucao, self->relCount);

  cpue_copia(pross_cpue(pross), self->cpue);
  exec_altera_estado(exec, pross_cpue(pross));
}

// funções auxiliares
static void init_mem(so_t *self);
static void panico(so_t *self);

so_metricas* so_metricas_cria()
{
  so_metricas* metricas = (so_metricas*)malloc(sizeof(so_metricas));

  metricas->last_zumbi = 0;
  metricas->t_total = 0;
  metricas->n_siop = 0;
  metricas->t_zumbi = 0;

  return metricas;
}

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->relCount = 0;
  self->metricas = so_metricas_cria();
  self->tabela = pross_tabela_cria(self->metricas);

  int n_quadros = (int)(MEM_TAM / PAG_TAM + 1);
  self->quadros = (char*)malloc(sizeof(char) * n_quadros);

  for(int i = 0; i < n_quadros; i++)
  {
    //começa com todos os quadros livres
    self->quadros[i] = 0;
  }

  init_mem(self);
  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  free(self);
}

// trata chamadas de sistema
static void so_trata_sisop_le(so_t *self)
{
  int disp = cpue_A(self->cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);

  //!es_pronto(contr_es(self->contr), disp, escrita)
  //até onde entendi, es_pronto não está implementado
  if(err != ERR_OK)
  {
    processo* pross = pross_acha_exec(self->tabela);

    //salva_mem(self, pross_copia_memoria(pross));
    pross_bloqueia(self->tabela, pross, SO_LE, disp, self->relCount);
    pross_copia_cpue(pross, self->cpue);
    carrega_pronto(self);
  }
  else
  {
    cpue_muda_A(self->cpue, err);
    cpue_muda_X(self->cpue, val); 
    cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  }

  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  //só incrementa o PC, se não houver trocas de processos
}


// chamada de sistema para escrita de E/S
static void so_trata_sisop_escr(so_t *self)
{
  int disp = cpue_A(self->cpue);
  int val = cpue_X(self->cpue);
  err_t err = es_escreve(contr_es(self->contr), disp, val);
  //!es_pronto(contr_es(self->contr), disp, escrita)
  //até onde entendi, es_pronto não está implementado
  if(err != ERR_OK) //bloqueio do processo
  {
    processo* pross = pross_acha_exec(self->tabela);

    //salva_mem(self, pross_copia_memoria(pross));
    pross_bloqueia(self->tabela, pross, SO_ESCR, disp, self->relCount);
    pross_copia_cpue(pross, self->cpue);
    carrega_pronto(self);
  }
  else
  {
    cpue_muda_A(self->cpue, err);
    cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  }

  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  //só incrementa o PC, se não houver trocas de processos
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  t_printf("DEBUG: terminando processo");

  processo* pross = pross_acha_exec(self->tabela);
  pross_altera_estado(self->tabela, pross, 0, self->relCount); //estado inválido
  pross_libera(self->tabela, pross);

  carrega_pronto(self);
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

bool desbloqueia_so_le(so_t* self, cpu_estado_t* cpue)
{
  int disp = cpue_A(cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);

  //no futuro, verirficar es_pronto
  if(err == ERR_OK)
  {
    cpue_muda_A(cpue, err);
    cpue_muda_X(cpue, val);
    return true;
  }

  return false;
}

bool desbloqueia_so_escr(so_t* self, cpu_estado_t* cpue)
{
  int disp = cpue_A(cpue);
  int val = cpue_X(cpue);
  err_t err = es_escreve(contr_es(self->contr), disp, val);

  //no futuro, verirficar es_pronto
  if(err == ERR_OK)
  {
    cpue_muda_A(cpue, err);
    return true;
  }

  return false;
}

// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self)
{
    t_printf("DEBUG: chamada de relogio");
    processo* pross = pross_acha_exec(self->tabela);

    if(pross != NULL)
    {
      pross_altera_estado(self->tabela, pross, pronto, self->relCount);
      exec_copia_estado(contr_exec(self->contr), pross_cpue(pross));
      //salva_mem(self, pross_copia_memoria(pross));
    }

    //se houver vários processos bloqueados, pode ser que um processo impeça que outro desbloqueie
    pross = pross_acha_bloqueado(self->tabela);

    if(pross == NULL)
    {
      carrega_pronto(self);
      cpue_muda_erro(self->cpue, ERR_OK, 0);
      exec_altera_estado(contr_exec(self->contr), self->cpue);
      return;
    }

    so_chamada_t motivo = pross_motivo_bloqueio(pross);
    cpu_estado_t* cpue = pross_cpue(pross);
    bool debug = false;

    switch (motivo)
    {
    case SO_LE:
      debug = desbloqueia_so_le(self, cpue);
      break;
    
    case SO_ESCR:
      debug = desbloqueia_so_escr(self, cpue);
      break;
    
    default:
      t_printf("algo de muito errado não está certo");
      break;
    }

    if(debug)
    {
      pross_altera_estado(self->tabela, pross, pronto, self->relCount);
      cpue_muda_PC(cpue, cpue_PC(cpue) + 2);
      t_printf("DEBUG: processo desbloqueado");
    }

    carrega_pronto(self);

    // interrupção da cpu foi atendida
    cpue_muda_erro(self->cpue, ERR_OK, 0);
    exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self)
{
  // o tipo de chamada está no "complemento" do cpue
  self->metricas->n_siop++;
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  so_chamada_t chamada = cpue_complemento(self->cpue);
  switch (chamada) {
    case SO_LE:
      so_trata_sisop_le(self);
      break;
    case SO_ESCR:
      so_trata_sisop_escr(self);
      break;
    case SO_FIM:
      so_trata_sisop_fim(self);
      break;
    case SO_CRIA:
      so_trata_sisop_cria(self);
      break;
    default:
      t_printf("so: chamada de sistema não reconhecida %d\n", chamada);
      panico(self);
  }
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err)
{
  self->relCount = rel_pega_tempo(self->contr);
  self->metricas->t_total = self->relCount;
  switch (err) {
    case ERR_SISOP:
      so_trata_sisop(self);
      break;
    case ERR_TIC:
      so_trata_tic(self);
      break;
    default:
      t_printf("SO: interrupção não tratada [%s]", err_nome(err));
      self->paniquei = true;
  }
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
}

// carrega um programa na memória
static void init_mem(so_t *self)
{
  cpu_estado_t* cpue = self->cpue;
  cpue_muda_A(cpue, 0);
  so_trata_sisop_cria(self);
}
  
static void panico(so_t *self) 
{
  t_printf("Problema irrecuperável no SO");
  self->paniquei = true;
}
