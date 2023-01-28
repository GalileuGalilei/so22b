#include <stdlib.h>
#include <stdio.h>
#include "so.h"
#include "tela.h"
#include "rel.h"
#include "processo.h"
#include "pag_esc.h"

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  tabela_processos* tabela;
  int relCount; //número total de interrupções do sistema
  so_metricas* metricas;
  pag_fila* pag_validas;
  char* quadros; //array de booleano que indica se um quadro está livre
};

//cria uma tabela nova tendo todas as paginas como inválidas
tab_pag_t* cria_tabela(so_t* self, int tam)
{
  int num_pag = (int)(tam / PAG_TAM + 1);
  //int num_quadros = (int)(MEM_TAM / PAG_TAM + 1);
  tab_pag_t* tab = tab_pag_cria(num_pag, PAG_TAM);

  int i;
  for(i = 0; i < num_pag; i++)
  {
    //todas as páginas começam como inválidas
    tab_pag_muda_valida(tab, i, false);

    //self->quadros[i] = 1;
  }

  t_printf("DEBUG: criando tabela de tam %i de %i paginas", tam, num_pag);
  /*
  for(int i = 0; i < num_pag; i++)
  {
    carrega_pagina(self, tab, i, programa + i * PAG_TAM);
  }
  */
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

//me livrei daqueles #include "init.maq", mó feio.
int* carrega_arquivo(const char* nome, int* tam)
{
  FILE* file = fopen(nome, "r");

  char c;
  int i = 0;
  while ((c = fgetc(file)) != EOF) 
  {
    if (c == ',') 
    {
      i++;
    }
  }

  int* vector = (int*) malloc((i + 1) * sizeof(int));

  rewind(file);
  *tam = i;
  i = 0;
  while (fscanf(file, "%d,", &vector[i]) != EOF) 
  {
    i++;
  }

  fclose(file);
  return vector;
}

int* carrega_programa(int program, int* tam_prog)
{
  switch (program)
  {
  case 0:
    return carrega_arquivo("init.maq", tam_prog);
  case 1:
    return carrega_arquivo("p1.maq", tam_prog);
  case 2:
    return carrega_arquivo("p2.maq", tam_prog);

  default:
    t_printf("ERRO: tentativa de carregar um programa inexistente");
    return NULL;
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
  }

  int programa = cpue_A(self->cpue);
  int tam_prog;
  int* prog = carrega_programa(programa, &tam_prog);
  tab_pag_t* tab = cria_tabela(self, tam_prog);

  t_printf("programa %i de tamanho %i", programa, tam_prog);

  pross = pross_cria(programa, tab, prog);
  pross_insere(self->tabela, pross);
  pross_usa_tabela(contr_mmu(self->contr), pross);
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
  self->pag_validas = pag_fila_cria();

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
    pross_copia_cpue(pross, self->cpue);
    pross_bloqueia(self->tabela, pross, pross_leitura, disp, self->relCount);
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
    pross_bloqueia(self->tabela, pross, pross_escrita, disp, self->relCount);
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
  //só incrementa o PC, se não houver trocas de processos ??? ?????
}

void so_trata_falha_pagina(so_t *self)
{
  mmu_t* mmu = contr_mmu(self->contr);
  int pag = mmu_ultimo_endereco(mmu) / PAG_TAM;

  processo* pross = pross_acha_exec(self->tabela);
  pross_copia_cpue(pross, self->cpue);
  pross_bloqueia(self->tabela, pross, pross_pagfalt, pag, self->relCount);

  //todo: fazer o que o Benhas pediu
  carrega_pronto(self);

  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

//se não houver quadros livres, chama o escalonador
int acha_quadro_livre(so_t* self)
{
  int num_quadros = (int)(MEM_TAM / PAG_TAM + 1);
  for(int i = 0; i < num_quadros; i++)
  {
    if(self->quadros[i] == 0)
    {
      self->quadros[i] = 1;
      return i;
    }
  }

  return pag_fila_escalonador(self->pag_validas, contr_mmu(self->contr));
}

void libera_quadros_tab(so_t* self, tab_pag_t* tab)
{
  int n_pags = tab_pag_num_pag(tab);
  for(int pag = 0; pag < n_pags; pag++)
  {
    if(tab_pag_valida(tab, pag))
    {
      int n_quadro = tab_pag_quadro(tab, pag);
      self->quadros[n_quadro] = 0;
    }
  }
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  t_printf("DEBUG: terminando processo");

  processo* pross = pross_acha_exec(self->tabela);
  libera_quadros_tab(self, pross_tab_pag(pross));
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

bool desbloqueia_so_falha_pag(so_t* self, processo* pross, int pag)
{
  mem_t* mem = contr_mem(self->contr);
  cpu_estado_t* cpue = pross_cpue(pross);
  int quadro = acha_quadro_livre(self);
  
  pross_carrega_pagina(pross, mem, pag, quadro);
  pag_ptr* ptr = pag_ptr_cria(pross_tab_pag(pross), pag, pross_copia_memoria(pross));
  pag_fila_insere(self->pag_validas, ptr);

  //volta no pc anterior para repetir o incremento da instrução
  cpue_muda_PC(cpue, cpue_PC(cpue) - 2);
  cpue_muda_A(cpue, ERR_OK);
  return true;
}

//tenta desbloquear um processo
void desbloqueia_processo(so_t* self, processo* pross)
{
    int complemento = 0;
    pross_bloqueio motivo = pross_motivo_bloqueio(pross, &complemento);
    cpu_estado_t* cpue = pross_cpue(pross);
    bool alterado = false;

    switch (motivo)
    {
    case pross_leitura:
      alterado = desbloqueia_so_le(self, cpue);
      break;
    
    case pross_escrita:
      alterado = desbloqueia_so_escr(self, cpue);
      break;
    
    case pross_pagfalt:
      alterado = desbloqueia_so_falha_pag(self, pross, complemento);
      break;

    default:
      t_printf("algo de muito errado não está certo");
      break;
    }

    if(alterado)
    {
      pross_altera_estado(self->tabela, pross, pronto, self->relCount);
      cpue_muda_PC(cpue, cpue_PC(cpue) + 2);
      t_printf("DEBUG: processo desbloqueado");
    }
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
    }

    pross = pross_acha_bloqueado(self->tabela);

    if(pross == NULL)
    {
      carrega_pronto(self);
      cpue_muda_erro(self->cpue, ERR_OK, 0);
      exec_altera_estado(contr_exec(self->contr), self->cpue);
      return;
    }

    pross_desbloqueia_com_so(self->tabela, self, desbloqueia_processo);
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
    case ERR_FALPAG:
      so_trata_falha_pagina(self);
      break;
    case ERR_PAGINV:
      t_printf("DEBUG: Tentativa de acesso ilegal a memoria");
      self->paniquei = true;
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
