#include "so.h"
#include "tela.h"
#include "rel.h"
#include "processo.h"
#include <stdlib.h>

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  tabela_processos* tabela;
  int relCount; //número total de interrupções do sistema
  so_metricas* metricas;
};

//t1 - processos

//carrega de um vetor de inteiros para a memória so sistema
void carrega_mem(so_t* self, int* mem_copia, int tam_copia)
{
  contr_t* contr = self->contr;
  mem_t* mem = contr_mem(contr);
  int valor;
  t_printf("DEBUG: carregando %i", tam_copia);
  for(int i = 0; i < tam_copia; i++)
  {
    valor = mem_copia[i];
    mem_escreve(mem, i, valor);
  }
}

void carrega_processo(so_t* self, processo* pross)
{
  pross_altera_estado(self->tabela, pross, execucao, self->relCount);
  cpue_copia(pross_cpue(pross), self->cpue);
  cpue_muda_modo(self->cpue, usuario);

  int tam = mem_tam(contr_mem(self->contr));
  carrega_mem(self, pross_copia_memoria(pross), tam);
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

//copia a memoria para dentro de um ponteiro "mem_copia"
void salva_mem(so_t* self, int* mem_copia)
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

void carrega_programa(so_t* self, int program)
{
    if(program == 0)
    {
      int progr[] = { 
        #include "init.maq" 
        };
      carrega_mem(self, progr, sizeof(progr));
      return;
    }

    if(program == 1)
    {
      int progr[] = { 
        #include "p1.maq" 
        };
      carrega_mem(self, progr, sizeof(progr));
      return;
    }

    if(program == 2)
    {
      int progr[] = { 
        #include "p2.maq" 
        };
      carrega_mem(self, progr, sizeof(progr));
      return;
    }
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
    salva_mem(self, pross_copia_memoria(pross));
  }

  int programa = cpue_A(self->cpue);
  carrega_programa(self, programa);
  t_printf("programa %i", programa);

  pross = pross_cria(programa);
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

    salva_mem(self, pross_copia_memoria(pross));
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

    salva_mem(self, pross_copia_memoria(pross));
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
      salva_mem(self, pross_copia_memoria(pross));
    }
    
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
