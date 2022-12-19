#include "so.h"
#include "tela.h"
#include "processo.h"
#include <stdlib.h>

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  tabela_processos* tabela;
};

/*
* tu parou aqui: fez um tratamento pro tik-tok
* e eu acho que só ^_0   .
*/

//t1 - processos
void carrega_mem(so_t* self, int* mem_copia, int tam_copia)
{
  contr_t* contr = self->contr;
  mem_t* mem = contr_mem(contr);
  int valor;
  t_printf("carregando %i", tam_copia);
  for(int i = 0; i < tam_copia; i++)
  {
    valor = mem_copia[i];
    mem_escreve(mem, i, valor);
  }
}

void carrega_processo(so_t* self, processo* pross)
{
  cpue_muda_modo(self->cpue, usuario);
  pross_altera_estado(self->tabela, pross, execucao);
  cpue_copia(pross_cpue(pross), self->cpue);

  int tam = mem_tam(contr_mem(self->contr));
  carrega_mem(self, pross_copia_memoria(pross), tam);
}

void carrega_pronto(so_t* self)
{
  processo* pross = pross_acha_pronto(self->tabela);

  if(pross == NULL)
  {
    t_printf("nada para executar, modo zumbi");
    cpue_muda_modo(self->cpue, zumbi);
    exec_altera_estado(contr_exec(self->contr), self->cpue);
    return;
  }
  cpue_muda_modo(self->cpue, usuario);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  carrega_processo(self, pross);
}

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
    pross_altera_estado(self->tabela, pross, pronto);
    pross_copia_cpue(pross, self->cpue);
    salva_mem(self, pross_copia_memoria(pross));
  }

  int programa = cpue_A(self->cpue);
  carrega_programa(self, programa);

  pross = pross_cria();
  pross_insere(self->tabela, pross);
  pross_altera_estado(self->tabela, pross, execucao);

  cpue_copia(pross_cpue(pross), self->cpue);
  exec_altera_estado(exec, pross_cpue(pross));
}


// funções auxiliares
static void init_mem(so_t *self);
static void panico(so_t *self);

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->tabela = pross_tabela_cria();
  init_mem(self);
  // coloca a CPU em modo usuário
  /*
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  cpue_muda_modo(self->cpue, usuario);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  */
  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  free(self);
}

// trata chamadas de sistema

// chamada de sistema para leitura de E/S
// recebe em A a identificação do dispositivo
// retorna em X o valor lido
//            A o código de erro
static void so_trata_sisop_le(so_t *self)
{
  // faz leitura assíncrona.
  // deveria ser síncrono, verificar es_pronto() e bloquear o processo
  int disp = cpue_A(self->cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);

  if(!es_pronto(contr_es(self->contr), disp, leitura) || err != ERR_OK)
  {
    processo* pross = pross_acha_exec(self->tabela);

    pross_copia_cpue(pross, self->cpue);
    salva_mem(self, pross_copia_memoria(pross));
    pross_bloqueia(pross, SO_LE, disp);
    carrega_pronto(self);
  }
  else
  {
    cpue_muda_A(self->cpue, err);
    cpue_muda_X(self->cpue, val); 
  }

  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}


// chamada de sistema para escrita de E/S
// recebe em A a identificação do dispositivo
//           X o valor a ser escrito
// retorna em A o código de erro
static void so_trata_sisop_escr(so_t *self)
{
  // faz leitura assíncrona.
  // deveria ser síncrono, verificar es_pronto() e bloquear o processo
  int disp = cpue_A(self->cpue);
  int val = cpue_X(self->cpue);
  err_t err = es_escreve(contr_es(self->contr), disp, val);

  if(!es_pronto(contr_es(self->contr), disp, escrita) || err != ERR_OK)
  {
    processo* pross = pross_acha_exec(self->tabela);

    pross_copia_cpue(pross, self->cpue);
    salva_mem(self, pross_copia_memoria(pross));
    pross_bloqueia(pross, SO_ESCR, disp);
    carrega_pronto(self);
  }
  else
  {
    cpue_muda_A(self->cpue, err);
  }

  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  processo* pross = pross_acha_exec(self->tabela);
  pross_libera(self->tabela, pross);
  carrega_pronto(self);
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self)
{
  // o tipo de chamada está no "complemento" do cpue
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

bool desbloqueia_so_le(so_t* self, cpu_estado_t* cpue)
{
  int disp = cpue_A(cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);

//no futuro, verirfica es_pronto
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

//no futuro, verirfica es_pronto
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
    t_printf("chamada de relógio");
    processo* pross = pross_acha_exec(self->tabela);

    if(pross != NULL)
    {
      pross_altera_estado(self->tabela, pross, pronto);
      pross_copia_cpue(pross, self->cpue);
      salva_mem(self, pross_copia_memoria(pross));
    }
    
    pross = pross_acha_bloqueado(self->tabela);

    if(pross == NULL)
    {
      return;
    }
    cpue_copia(pross_cpue(pross), self->cpue);

    so_chamada_t motivo = pross_motivo_bloqueio(pross);
    cpu_estado_t* cpue = pross_cpue(pross);
    t_printf("dispositivo: %i", cpue_A(cpue));
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
      pross_altera_estado(self->tabela, pross, pronto);
      cpue_muda_PC(cpue, cpue_PC(cpue) + 2);
      t_printf("um processo foi desbloqueado");
    }

    carrega_pronto(self);
    //cpue_muda_PC(self->cpue, cpue_PC(self->cpue) + 2);
    // interrupção da cpu foi atendida
    cpue_muda_erro(self->cpue, ERR_OK, 0);
    exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err)
{
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
