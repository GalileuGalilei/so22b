// montador para o processador de SO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "instr.h"

// auxiliares

// aborta o programa com uma mensagem de erro
void erro_brabo(char *msg)
{
  fprintf(stderr, "ERRO FATAL: %s\n", msg);
  exit(1);
}

// retorna true se tem um número na string s (e retorna o número também)
bool tem_numero(char *s, int *num)
{
  if (isdigit(*s) || (*s == '-' && isdigit(*(s+1)))) {
    *num = atoi(s);
    return true;
  }
  return false;
}

// memória de saída

// representa a memória do programa -- a saída do montador é colocada aqui

#define MEM_TAM 1000    // aumentar para programas maiores
int mem[MEM_TAM];
int mem_pos;        // próxima posiçao livre da memória

// coloca um valor no final da memória
void mem_insere(int val)
{
  if (mem_pos >= MEM_TAM-1) {
    erro_brabo("programa muito grande! Aumente MEM_TAM no montador.");
  }
  mem[mem_pos++] = val;
}

// altera o valor em uma posição já ocupada da memória
void mem_altera(int pos, int val)
{
  if (pos < 0 || pos >= mem_pos) {
    erro_brabo("erro interno, alteração de região não inicializada");
  }
  mem[pos] = val;
}

// imprime o conteúdo da memória
void mem_imprime(void)
{
  for (int i = 0; i < mem_pos; i+=10) {
   // printf("    /*%4d */", i);
    for (int j = i; j < i+10 && j < mem_pos; j++) {
      printf(" %d,", mem[j]);
    }
    printf("\n");
  }
}

// simbolos

// tabela com os símbolos (labels) já definidos pelo programa, e o valor (endereço) deles

#define SIMB_TAM 1000
struct {
  char *nome;
  int valor;
} simbolo[SIMB_TAM];
int simb_num;             // número d símbolos na tabela

// retorna o valor de um símbolo, ou -1 se não existir na tabela
int simb_valor(char *nome)
{
  for (int i=0; i<simb_num; i++) {
    if (strcmp(nome, simbolo[i].nome) == 0) {
      return simbolo[i].valor;
    }
  }
  return -1;
}

// insere um novo símbolo na tabela
void simb_novo(char *nome, int valor)
{
  if (nome == NULL) return;
  if (simb_valor(nome) != -1) {
    fprintf(stderr, "ERRO: redefinicao do simbolo '%s'\n", nome);
    return;
  }
  if (simb_num >= SIMB_TAM) {
    erro_brabo("Excesso de símbolos. Aumente SIMB_TAM no montador.");
  }
  simbolo[simb_num].nome = strdup(nome);
  simbolo[simb_num].valor = valor;
  simb_num++;
}


// referências

// tabela com referências a símbolos
//   contém a linha e o endereço correspondente onde o símbolo foi referenciado

#define REF_TAM 1000
struct {
  char *nome;
  int linha;
  int endereco;
} ref[REF_TAM];
int ref_num;      // numero de referências criadas

// insere uma nova referência na tabela
void ref_nova(char *nome, int linha, int endereco)
{
  if (nome == NULL) return;
  if (ref_num >= REF_TAM) {
    erro_brabo("excesso de referências. Aumente REF_TAM no montador.");
  }
  ref[ref_num].nome = strdup(nome);
  ref[ref_num].linha = linha;
  ref[ref_num].endereco = endereco;
  ref_num++;
}

// resolve as referências -- para cada referência, coloca o valor do símbolo
//   no endereço onde ele é referenciado
void ref_resolve(void)
{
  for (int i=0; i<ref_num; i++) {
    int valor = simb_valor(ref[i].nome);
    if (valor == -1) {
      fprintf(stderr, 
              "ERRO: simbolo '%s' referenciado na linha %d não foi definido\n",
              ref[i].nome, ref[i].linha);
    }
    mem_altera(ref[i].endereco, valor);
  }
}



// montagem

// realiza a montagem de uma instrução (gera o código para ela na memória),
//   tendo opcode da instrução e o argumento
void monta_instrucao(int linha, int opcode, char *arg)
{
  int argn;  // para conter o valor numérico do argumento
  int num_args = instr_num_args(opcode);
  
  // trata pseudo-opcodes antes
  if (opcode == ESPACO) {
    if (!tem_numero(arg, &argn)) {
      argn = simb_valor(arg);
    }
    if (argn < 1) {
      fprintf(stderr, "ERRO: linha %d 'ESPACO' deve ter valor positivo\n",
              linha);
      return;
    }
    for (int i = 0; i < argn; i++) {
      mem_insere(0);
    }
    return;
  } else if (opcode == VALOR) {
    // nao faz nada, vai inserir o valor definido em arg
  } else {
    // instrução real, coloca o opcode da instrução na memória
    mem_insere(opcode);
  }
  if (num_args == 0) {
    return;
  }
  if (tem_numero(arg, &argn)) {
    mem_insere(argn);
  } else {
    // não é número, põe um 0 e insere uma referência para alterar depois
    ref_nova(arg, linha, mem_pos);
    mem_insere(0);
  }
}

void monta_linha(int linha, char *label, char *instrucao, char *arg)
{
  int opcode = instr_opcode(instrucao);
  // pseudo-instrução DEFINE tem que ser tratada antes, porque não pode
  //   definir o label de forma normal
  if (opcode == DEFINE) {
    int argn;  // para conter o valor numérico do argumento
    if (label == NULL) {
      fprintf(stderr, "ERRO: linha %d: 'DEFINE' exige um label\n",
                      linha);
    } else if (!tem_numero(arg, &argn)) {
      fprintf(stderr, "ERRO: linha %d 'DEFINE' exige valor numérico\n",
              linha);
    } else {
      // tudo OK, define o símbolo
      simb_novo(label, argn);
    }
    return;
  }
  
  // cria símbolo correspondente ao label, se for o caso
  if (label != NULL) {
    simb_novo(label, mem_pos);
  }
  
  // verifica a existência de instrução e número correto de argumentos
  if (instrucao == NULL) return;
  if (opcode == -1) {
    fprintf(stderr, "ERRO: linha %d: instrucao '%s' desconhecida\n",
                    linha, instrucao);
    return;
  }
  int num_args = instr_num_args(opcode);
  if (num_args == 0 && arg != NULL) {
    fprintf(stderr, "ERRO: linha %d: instrucao '%s' não tem argumento\n",
                    linha, instrucao);
    return;
  }
  if (num_args == 1 && arg == NULL) {
    fprintf(stderr, "ERRO: linha %d: instrucao '%s' necessita argumento\n",
                    linha, instrucao);
    return;
  }
  // tudo OK, monta a instrução
  monta_instrucao(linha, opcode, arg);
}

// retorna true se o caractere for um espaço (ou tab)
bool espaco(char c)
{
  return c == ' ' || c == '\t';
}

// encontra o primeiro caractere que não seja espaço (ou tab) na string
char *pula_ate_espaco(char *s)
{
  while (!espaco(*s) && *s != '\0') {
    s++;
  }
  return s;
}

// troca espaços por fim de string
char *detona_espacos(char *s)
{
  while (espaco(*s)) {
    *s = '\0';
    s++;
  }
  return s;
}

// faz a string terminar no início de um comentário, se houver
// aproveita e termina se chegar no fim de linha
void tira_comentario(char *s)
{
  while(*s != '\0' && *s != ';' && *s != '\n' && *s != '\r') {
    s++;
  }
  *s = '\0';
}

// uma linha montável é formada por [label][ instrucao[ argumento]]
// o que está entre [] é opcional
// as partes são separadas por espaço(s)
// de ';' em diante, ignora-se (comentário)
// a string é alterada, colocando-se NULs no lugar dos espaços, para separá-la em substrings
// quem precisar guardar essas substrings, deve copiá-las.
void monta_string(int linha, char *str)
{
  char *label = NULL;
  char *instrucao = NULL;
  char *arg = NULL;
  tira_comentario(str);
  if (*str == '\0') return;
  if (!espaco(*str)) {
    label = str;
    str = pula_ate_espaco(str);
  }
  str = detona_espacos(str);
  if (*str != '\0') {
    instrucao = str;
    str = pula_ate_espaco(str);
  }
  str = detona_espacos(str);
  if (*str != '\0') {
    arg = str;
    str = pula_ate_espaco(str);
  }
  str = detona_espacos(str);
  if (*str != '\0') {
    fprintf(stderr, "linha %d: ignorando '%s'\n", linha, str);
  }
  if (label != NULL || instrucao != NULL) {
    monta_linha(linha, label, instrucao, arg);
  }
}

void monta_arquivo(char *nome)
{
  int linha = 1;
  FILE *arq;
  arq = fopen(nome, "r");
  if (arq == NULL) {
    fprintf(stderr, "Não foi possível abrir o arquivo '%s'\n", nome);
    return;
  }
  while (true) {
    char lin[500];
    if (fgets(lin, 500, arq) == NULL) break;
    int n = strlen(lin) - 1;
    if (lin[n] == '\n') lin[n--] = '\0';
    // (se o arquivo passou pelo windows, pode ter \r no final da linha)
    if (lin[n] == '\r') lin[n--] = '\0';
    monta_string(linha, lin);
    linha++;
  }
  fclose(arq);
  ref_resolve();
}

int main(int argc, char *argv[argc])
{
  if (argc != 2) {
    fprintf(stderr, "ERRO: chame como '%s nome_do_arquivo'\n", argv[0]);
    return 1;
  }
  monta_arquivo(argv[1]);
  mem_imprime();
  return 0;
}
