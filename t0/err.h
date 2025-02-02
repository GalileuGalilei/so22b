#ifndef ERR_H
#define ERR_H

// os tipos de erro que podem acontecer ao tentar executar uma instrução
typedef enum {
  ERR_OK,            // sem erro
  ERR_END_INV,       // endereço inválido
  ERR_OP_INV,        // operação inválida
  ERR_CPU_PARADA,    // CPU está com execução suspensa
  ERR_INSTR_INV,     // instrução inválida
  N_ERR              // número de erros
} err_t;

// retorna o nome de erro
char *err_nome(err_t err);

#endif // ERR_H
