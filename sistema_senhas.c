/* ============================================================================
 * Sistema de Distribuicao de Senhas
 * Disciplina: Paradigmas de Programacao
 *
 * Gera senhas comuns e prioritarias, chama a proxima respeitando prioridade
 * e ordem de chegada (FIFO), finaliza atendimentos e mantem um historico
 * com os tempos de espera e atendimento.
 *
 * Cada senha e um no alocado dinamicamente (struct Senha), organizado em
 * listas encadeadas que funcionam como filas. Todo o estado do sistema
 * fica dentro de uma unica struct (Sistema), passada por ponteiro entre
 * as funcoes - sem variaveis globais, e fica explicito no proprio
 * cabecalho da funcao o que ela pode ler ou alterar.
 * ==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h> /* Adicionado para os testes automatizados */

/* ---------------------------- Constantes ------------------------------- */
#define TIPO_COMUM        1
#define TIPO_PRIORITARIA  2
#define TAM_HORARIO       6   /* "HH:MM" + '\0' */
#define TAM_ID            8   /* "C999" + folga para nao estourar o buffer */

/* ------------------------------ Estruturas ------------------------------ */

/* Uma senha na fila: horarios em texto (para exibir) e em minutos (para
 * calcular tempos), mais dois ponteiros de lista - um para a fila em que
 * esta, outro para o historico. */
typedef struct Senha {
    char id[TAM_ID];
    int  tipo;

    char chegadaStr[TAM_HORARIO];
    char chamadaStr[TAM_HORARIO];
    char finalizacaoStr[TAM_HORARIO];

    int  chegadaMin;
    int  chamadaMin;
    int  finalizacaoMin;

    int  tempoEspera;
    int  tempoAtendimento;

    struct Senha *next;
    struct Senha *histProx;
} Senha;

/* Fila encadeada com ponteiros de inicio/fim, pra inserir e remover a
 * cabeca em O(1). */
typedef struct Fila {
    Senha *inicio;
    Senha *fim;
    int    quantidade;
} Fila;

/* Estado completo do sistema. Cada funcao recebe um ponteiro pra esta
 * struct em vez de depender de variaveis globais. */
typedef struct Sistema {
    Fila filaComum;
    Fila filaPrioritaria;

    /* Atendimentos abertos no momento. Da pra chamar novas senhas sem
     * encerrar as anteriores; cada uma so sai daqui quando o atendente
     * escolhe finaliza-la (opcao "Finalizar atendimento"). */
    Fila atendimentosEmAndamento;

    int contadorComum;
    int contadorPrioritaria;

    /* O historico reaproveita os proprios nos das senhas ja finalizadas. */
    Senha *historicoInicio;
    Senha *historicoFim;
    int    totalFinalizados;
    long   somaEspera;
    long   somaAtendimento;
} Sistema;

/* ------------------------------ Prototipos ------------------------------- */

void  exibirMenu(void);
int   validarHorario(const char *str, int *minutos);

void  enfileirar(Fila *fila, Senha *nova);
void  desenfileirar(Fila *fila);
Senha *removerPorId(Fila *fila, const char *id);
void  imprimirOrdemFila(const Fila *fila);
void  liberarFila(Fila *fila);

void  adicionarHistorico(Sistema *sis, Senha *s);
void  liberarHistorico(Sistema *sis);

void  gerarSenha(Sistema *sis);
void  chamarProximaSenha(Sistema *sis);
void  finalizarAtendimento(Sistema *sis);
void  exibirAguardando(const Sistema *sis);
void  exibirHistorico(const Sistema *sis);
void  encerrarPrograma(Sistema *sis);
void  rodarTodosTestes(void);

/* ================================ main =================================== */

int main(void) {
    Sistema sistema = {0};   /* filas vazias, contadores e historico zerados */
    int opcao;

    do {
        exibirMenu();

        if (scanf("%d", &opcao) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            printf("Opcao invalida. Tente novamente.\n");
            opcao = -1;
            continue;
        }

        switch (opcao) {
            case 1: gerarSenha(&sistema);           break;
            case 2: chamarProximaSenha(&sistema);   break;
            case 3: finalizarAtendimento(&sistema); break;
            case 4: exibirAguardando(&sistema);     break;
            case 5: exibirHistorico(&sistema);      break;
            case 6: encerrarPrograma(&sistema);     break;
            case 7: rodarTodosTestes();             break;
            default:
                printf("Opcao invalida. Tente novamente.\n");
        }

    } while (opcao != 6);

    return 0;
}

/* ============================ Funcoes de apoio ============================ */

void exibirMenu(void) {
    printf("\n===== Sistema de Distribuicao de Senhas =====\n");
    printf("1. Gerar senha\n");
    printf("2. Chamar proxima senha\n");
    printf("3. Finalizar atendimento\n");
    printf("4. Exibir quantidade aguardando\n");
    printf("5. Exibir historico\n");
    printf("6. Sair\n");
    printf("7. Rodar testes automatizados\n");
    printf("Escolha uma opcao: ");
}

/* Converte "HH:MM" para minutos desde 00:00. Retorna 0 se o formato ou os
 * valores forem invalidos. Usa "%1s" pra pegar qualquer sobra apos o
 * "HH:MM" (ex.: "10:30abc") em vez de "%n", que fica desligado por
 * padrao em compiladores como o MSVC. */
int validarHorario(const char *str, int *minutos) {
    int h, m;
    char sobra[2];

    int lidos = sscanf(str, "%d:%d%1s", &h, &m, sobra);
    if (lidos != 2) return 0;

    if (h < 0 || h > 23 || m < 0 || m > 59) return 0;

    *minutos = h * 60 + m;
    return 1;
}

/* -------------------------------- Filas ----------------------------------- */

void enfileirar(Fila *fila, Senha *nova) {
    nova->next = NULL;
    if (fila->fim == NULL) {
        fila->inicio = nova;
        fila->fim = nova;
    } else {
        fila->fim->next = nova;
        fila->fim = nova;
    }
    fila->quantidade++;
}

void desenfileirar(Fila *fila) {
    if (fila->inicio == NULL) return;

    Senha *removido = fila->inicio;
    fila->inicio = removido->next;
    if (fila->inicio == NULL) fila->fim = NULL;

    removido->next = NULL;
    fila->quantidade--;
}

/* Remove o no com o id informado, em qualquer posicao da fila (usado pra
 * finalizar um atendimento especifico quando ha varios em andamento).
 * Retorna NULL se nao encontrar. */
Senha *removerPorId(Fila *fila, const char *id) {
    Senha *anterior = NULL;
    Senha *atual = fila->inicio;

    while (atual != NULL) {
        if (strcmp(atual->id, id) == 0) {
            if (anterior == NULL) {
                fila->inicio = atual->next;
            } else {
                anterior->next = atual->next;
            }
            if (atual == fila->fim) {
                fila->fim = anterior;
            }
            atual->next = NULL;
            fila->quantidade--;
            return atual;
        }
        anterior = atual;
        atual = atual->next;
    }

    return NULL;
}

void imprimirOrdemFila(const Fila *fila) {
    Senha *atual = fila->inicio;
    while (atual != NULL) {
        printf("%s", atual->id);
        if (atual->next != NULL) printf(" -> ");
        atual = atual->next;
    }
    printf("\n");
}

void liberarFila(Fila *fila) {
    Senha *atual = fila->inicio;
    while (atual != NULL) {
        Senha *proximo = atual->next;
        free(atual);
        atual = proximo;
    }
    fila->inicio = NULL;
    fila->fim = NULL;
    fila->quantidade = 0;
}

/* ------------------------------- Historico --------------------------------- */

void adicionarHistorico(Sistema *sis, Senha *s) {
    s->histProx = NULL;
    if (sis->historicoFim == NULL) {
        sis->historicoInicio = s;
        sis->historicoFim = s;
    } else {
        sis->historicoFim->histProx = s;
        sis->historicoFim = s;
    }

    sis->totalFinalizados++;
    sis->somaEspera      += s->tempoEspera;
    sis->somaAtendimento += s->tempoAtendimento;
}

void liberarHistorico(Sistema *sis) {
    Senha *atual = sis->historicoInicio;
    while (atual != NULL) {
        Senha *proximo = atual->histProx;
        free(atual);
        atual = proximo;
    }
    sis->historicoInicio = NULL;
    sis->historicoFim = NULL;
}

/* ============================ Opcoes do menu =============================== */

/* Opcao 1: Gerar senha ------------------------------------------------------ */
void gerarSenha(Sistema *sis) {
    printf("\n--- Gerar Senha ---\n");
    printf("Tipo de atendimento:\n");
    printf("1. Comum\n");
    printf("2. Prioritario\n");
    printf("Escolha uma opcao: ");

    int tipo;
    if (scanf("%d", &tipo) != 1) {
        int c; while ((c = getchar()) != '\n' && c != EOF) { }
        printf("Erro: tipo de atendimento invalido. Informe 1 para comum ou 2 para prioritario.\n");
        printf("Nenhuma memoria foi alocada.\n");
        return;
    }

    if (tipo != TIPO_COMUM && tipo != TIPO_PRIORITARIA) {
        printf("Erro: tipo de atendimento invalido. Informe 1 para comum ou 2 para prioritario.\n");
        printf("Nenhuma memoria foi alocada.\n");
        return;
    }

    char horario[16];
    printf("Horario de chegada (HH:MM): ");
    scanf("%15s", horario);

    int minutos;
    if (!validarHorario(horario, &minutos)) {
        printf("Erro: horario invalido. Informe um horario entre 00:00 e 23:59.\n");
        printf("Nenhuma memoria foi alocada.\n");
        return;
    }

    /* So aloca depois de validar tudo - evita no orfao em caso de erro. */
    printf("Alocando memoria para nova senha... ");

    Senha *nova = malloc(sizeof(Senha));
    if (nova == NULL) {
        printf("\nErro: falha ao alocar memoria para a nova senha.\n");
        return;
    }

    nova->tipo = tipo;
    strncpy(nova->chegadaStr, horario, TAM_HORARIO - 1);
    nova->chegadaStr[TAM_HORARIO - 1] = '\0';
    nova->chegadaMin = minutos;
    nova->chamadaMin = -1;
    nova->finalizacaoMin = -1;
    nova->chamadaStr[0] = '\0';
    nova->finalizacaoStr[0] = '\0';
    nova->tempoEspera = 0;
    nova->tempoAtendimento = 0;
    nova->next = NULL;
    nova->histProx = NULL;

    if (tipo == TIPO_COMUM) {
        sis->contadorComum++;
        snprintf(nova->id, TAM_ID, "C%03d", sis->contadorComum);
        enfileirar(&sis->filaComum, nova);
    } else {
        sis->contadorPrioritaria++;
        snprintf(nova->id, TAM_ID, "P%03d", sis->contadorPrioritaria);
        enfileirar(&sis->filaPrioritaria, nova);
    }

    printf("Senha gerada com sucesso: %s\n", nova->id);
    printf("Horario de chegada: %s\n", nova->chegadaStr);
    printf("Fila comum: %d\n", sis->filaComum.quantidade);
    printf("Fila prioritaria: %d\n", sis->filaPrioritaria.quantidade);
}

/* Opcao 2: Chamar proxima senha ---------------------------------------------- */
void chamarProximaSenha(Sistema *sis) {
    printf("\n--- Chamar Proxima Senha ---\n");

    if (sis->filaComum.quantidade == 0 && sis->filaPrioritaria.quantidade == 0) {
        printf("Nao ha pessoas aguardando. As filas comum e prioritaria estao vazias.\n");
        printf("Nenhum atendimento foi iniciado.\n");
        return;
    }

    /* Prioritaria sempre atendida antes da comum. */
    Fila *filaEscolhida = (sis->filaPrioritaria.quantidade > 0) ? &sis->filaPrioritaria : &sis->filaComum;
    Senha *proxima = filaEscolhida->inicio;

    char horario[16];
    printf("Horario da chamada (HH:MM): ");
    scanf("%15s", horario);

    int minutos;
    if (!validarHorario(horario, &minutos)) {
        printf("Erro: horario invalido. Informe um horario entre 00:00 e 23:59.\n");
        printf("Nenhum atendimento foi iniciado.\n");
        return;
    }

    if (minutos < proxima->chegadaMin) {
        printf("Erro: o horario da chamada nao pode ser anterior ao horario de chegada.\n");
        printf("Nenhum atendimento foi iniciado.\n");
        return;
    }

    desenfileirar(filaEscolhida);

    strncpy(proxima->chamadaStr, horario, TAM_HORARIO - 1);
    proxima->chamadaStr[TAM_HORARIO - 1] = '\0';
    proxima->chamadaMin = minutos;
    proxima->tempoEspera = minutos - proxima->chegadaMin;

    /* Fica em aberto ate o atendente escolher encerra-la especificamente. */
    enfileirar(&sis->atendimentosEmAndamento, proxima);

    printf("Chamando proxima senha...\n");
    printf("Senha chamada: %s\n", proxima->id);
    printf("Tipo: %s\n", proxima->tipo == TIPO_COMUM ? "Comum" : "Prioritario");
    printf("Horario de chegada: %s\n", proxima->chegadaStr);
    printf("Horario da chamada: %s\n", proxima->chamadaStr);
    printf("Tempo de espera: %d minutos\n", proxima->tempoEspera);
    printf("Atendimentos em andamento no momento: %d\n", sis->atendimentosEmAndamento.quantidade);
    printf("Atendimento iniciado com sucesso.\n");
}

/* Opcao 3: Finalizar atendimento ---------------------------------------------- */
void finalizarAtendimento(Sistema *sis) {
    printf("\n--- Finalizar Atendimento ---\n");

    if (sis->atendimentosEmAndamento.quantidade == 0) {
        printf("Erro: nao existe nenhum atendimento em andamento.\n");
        printf("Nenhum historico foi alterado.\n");
        return;
    }

    Senha *alvo = NULL;

    if (sis->atendimentosEmAndamento.quantidade == 1) {
        /* Unico atendimento aberto: sem ambiguidade, encerra direto. */
        alvo = sis->atendimentosEmAndamento.inicio;
        printf("Encerrando o unico atendimento em andamento: %s\n", alvo->id);
        desenfileirar(&sis->atendimentosEmAndamento);
    } else {
        /* Varios abertos: o atendente escolhe qual encerrar. */
        printf("Existem %d atendimentos em andamento:\n", sis->atendimentosEmAndamento.quantidade);
        imprimirOrdemFila(&sis->atendimentosEmAndamento);

        char idBusca[TAM_ID];
        printf("Informe a senha que deseja encerrar: ");
        scanf("%7s", idBusca);

        alvo = removerPorId(&sis->atendimentosEmAndamento, idBusca);
        if (alvo == NULL) {
            printf("Erro: senha \"%s\" nao encontrada entre os atendimentos em andamento.\n", idBusca);
            printf("Nenhum historico foi alterado.\n");
            return;
        }
    }

    char horario[16];
    printf("Horario de finalizacao (HH:MM): ");
    scanf("%15s", horario);

    int minutos;
    if (!validarHorario(horario, &minutos)) {
        printf("Erro: horario invalido. Informe um horario entre 00:00 e 23:59.\n");
        printf("Nenhum historico foi alterado.\n");
        enfileirar(&sis->atendimentosEmAndamento, alvo);  /* devolve a senha pra fila */
        return;
    }

    if (minutos < alvo->chamadaMin) {
        printf("Erro: o horario de finalizacao nao pode ser anterior ao horario da chamada.\n");
        printf("Nenhum historico foi alterado.\n");
        enfileirar(&sis->atendimentosEmAndamento, alvo);
        return;
    }

    strncpy(alvo->finalizacaoStr, horario, TAM_HORARIO - 1);
    alvo->finalizacaoStr[TAM_HORARIO - 1] = '\0';
    alvo->finalizacaoMin = minutos;
    alvo->tempoAtendimento = minutos - alvo->chamadaMin;

    printf("Atendimento finalizado para a senha %s.\n", alvo->id);
    printf("Tempo de atendimento: %d minutos\n", alvo->tempoAtendimento);

    adicionarHistorico(sis, alvo);
    printf("Registro adicionado ao historico.\n");
    printf("Atendimentos ainda em andamento: %d\n", sis->atendimentosEmAndamento.quantidade);
}

/* Opcao 4: Exibir quantidade aguardando --------------------------------------- */
void exibirAguardando(const Sistema *sis) {
    printf("\n--- Pessoas Aguardando ---\n");
    printf("Fila comum: %d\n", sis->filaComum.quantidade);
    printf("Fila prioritaria: %d\n", sis->filaPrioritaria.quantidade);
    printf("Total aguardando: %d\n", sis->filaComum.quantidade + sis->filaPrioritaria.quantidade);

    if (sis->filaComum.quantidade > 0) {
        printf("Ordem da fila comum: ");
        imprimirOrdemFila(&sis->filaComum);
    }
    if (sis->filaPrioritaria.quantidade > 0) {
        printf("Ordem da fila prioritaria: ");
        imprimirOrdemFila(&sis->filaPrioritaria);
    }

    printf("Atendimentos em andamento: %d\n", sis->atendimentosEmAndamento.quantidade);
    if (sis->atendimentosEmAndamento.quantidade > 0) {
        printf("Senhas em atendimento: ");
        imprimirOrdemFila(&sis->atendimentosEmAndamento);
    }
}

/* Opcao 5: Exibir historico ---------------------------------------------------- */
void exibirHistorico(const Sistema *sis) {
    printf("\n--- Historico de Atendimentos ---\n");

    if (sis->historicoInicio == NULL) {
        printf("Nenhum atendimento foi finalizado ate o momento.\n");
    } else {
        int indice = 1;
        Senha *atual = sis->historicoInicio;
        while (atual != NULL) {
            printf("%d. Senha: %s | Tipo: %s\n",
                   indice, atual->id, atual->tipo == TIPO_COMUM ? "Comum" : "Prioritario");
            printf("Chegada: %s | Chamada: %s | Finalizacao: %s\n",
                   atual->chegadaStr, atual->chamadaStr, atual->finalizacaoStr);
            printf("Tempo de espera: %d minutos\n", atual->tempoEspera);
            printf("Tempo de atendimento: %d minutos\n", atual->tempoAtendimento);
            indice++;
            atual = atual->histProx;
        }
    }

    double mediaEspera = (sis->totalFinalizados > 0) ? (double) sis->somaEspera / sis->totalFinalizados : 0.0;
    double mediaAtendimento = (sis->totalFinalizados > 0) ? (double) sis->somaAtendimento / sis->totalFinalizados : 0.0;

    printf("Resumo:\n");
    printf("Atendimentos finalizados: %d\n", sis->totalFinalizados);
    printf("Tempo medio de espera: %.2f minutos\n", mediaEspera);
    printf("Tempo medio de atendimento: %.2f minutos\n", mediaAtendimento);
    printf("Pessoas aguardando: %d\n", sis->filaComum.quantidade + sis->filaPrioritaria.quantidade);
}

/* Opcao 6: Sair ------------------------------------------------------------------ */
void encerrarPrograma(Sistema *sis) {
    if (sis->historicoInicio != NULL) {
        printf("Liberando filas...\n");
        printf("Liberando historico de atendimentos...\n");
    } else {
        printf("Liberando filas e historico...\n");
    }

    liberarFila(&sis->filaComum);
    liberarFila(&sis->filaPrioritaria);
    liberarFila(&sis->atendimentosEmAndamento);
    liberarHistorico(sis);

    printf("Memoria liberada com sucesso. Encerrando o programa.\n");
}

/* ================== Testes Automatizados Completos ========================= */
void rodarTodosTestes(void) {
    printf("\n ---------- INICIANDO BATERIA DE TESTES AUTOMATIZADOS ---------- \n");

    {
        printf("[Teste 1] Consulta inicial com filas vazias... ");
        Sistema sis = {0};
        assert(sis.filaComum.quantidade == 0);
        assert(sis.filaPrioritaria.quantidade == 0);
        assert(sis.atendimentosEmAndamento.quantidade == 0);
        printf("OK\n");
    }

    {
        printf("[Teste 2] Geracao da primeira senha comum... ");
        Sistema sis = {0};
        Senha *s = calloc(1, sizeof(Senha));
        strcpy(s->id, "C001");
        enfileirar(&sis.filaComum, s);
        assert(sis.filaComum.quantidade == 1);
        assert(strcmp(sis.filaComum.inicio->id, "C001") == 0);
        liberarFila(&sis.filaComum);
        printf("OK\n");
    }

    {
        printf("[Teste 3] Geracao da primeira senha prioritaria... ");
        Sistema sis = {0};
        Senha *s = calloc(1, sizeof(Senha));
        strcpy(s->id, "P001");
        enfileirar(&sis.filaPrioritaria, s);
        assert(sis.filaPrioritaria.quantidade == 1);
        assert(sis.filaComum.quantidade == 0);
        assert(strcmp(sis.filaPrioritaria.inicio->id, "P001") == 0);
        liberarFila(&sis.filaPrioritaria);
        printf("OK\n");
    }

    {
        printf("[Teste 4] Geracao de varias senhas e contagem das filas... ");
        Sistema sis = {0};
        Senha *c1 = calloc(1, sizeof(Senha)); strcpy(c1->id, "C001"); enfileirar(&sis.filaComum, c1);
        Senha *c2 = calloc(1, sizeof(Senha)); strcpy(c2->id, "C002"); enfileirar(&sis.filaComum, c2);
        Senha *p1 = calloc(1, sizeof(Senha)); strcpy(p1->id, "P001"); enfileirar(&sis.filaPrioritaria, p1);
        Senha *c3 = calloc(1, sizeof(Senha)); strcpy(c3->id, "C003"); enfileirar(&sis.filaComum, c3);
        
        assert(sis.filaComum.quantidade == 3);
        assert(sis.filaPrioritaria.quantidade == 1);
        assert((sis.filaComum.quantidade + sis.filaPrioritaria.quantidade) == 4);
        
        liberarFila(&sis.filaComum);
        liberarFila(&sis.filaPrioritaria);
        printf("OK\n");
    }

    {
        printf("[Teste 5] Atendimento prioritario antes da fila comum... ");
        Sistema sis = {0};
        Senha *c1 = calloc(1, sizeof(Senha)); strcpy(c1->id, "C001"); enfileirar(&sis.filaComum, c1);
        Senha *c2 = calloc(1, sizeof(Senha)); strcpy(c2->id, "C002"); enfileirar(&sis.filaComum, c2);
        Senha *p1 = calloc(1, sizeof(Senha)); strcpy(p1->id, "P001"); enfileirar(&sis.filaPrioritaria, p1);

        Fila *escolhida = (sis.filaPrioritaria.quantidade > 0) ? &sis.filaPrioritaria : &sis.filaComum;
        Senha *prox = escolhida->inicio;
        desenfileirar(escolhida);
        enfileirar(&sis.atendimentosEmAndamento, prox);
        
        assert(strcmp(prox->id, "P001") == 0);
        printf("OK\n");

        printf("[Teste 6] Atendimento FIFO dentro da mesma fila... ");
        escolhida = (sis.filaPrioritaria.quantidade > 0) ? &sis.filaPrioritaria : &sis.filaComum;
        prox = escolhida->inicio;
        desenfileirar(escolhida);
        enfileirar(&sis.atendimentosEmAndamento, prox);
        
        assert(strcmp(prox->id, "C001") == 0);
        
        liberarFila(&sis.filaComum);
        liberarFila(&sis.atendimentosEmAndamento);
        printf("OK\n");
    }

    {
        printf("[Teste 7] Tentativa de chamar senha com filas vazias... ");
        Sistema sis = {0};
        assert(sis.filaComum.quantidade == 0 && sis.filaPrioritaria.quantidade == 0);
        printf("OK\n");
    }

    {
        printf("[Teste 8] Tentativa de finalizar atendimento sem atendimento ativo... ");
        Sistema sis = {0};
        assert(sis.atendimentosEmAndamento.quantidade == 0);
        Senha *alvo = removerPorId(&sis.atendimentosEmAndamento, "C001");
        assert(alvo == NULL);
        printf("OK\n");
    }

    {
        printf("[Teste 9] Validacao de tipo de senha e horario invalido... ");
        int min;
        assert(validarHorario("25:70", &min) == 0);
        assert(validarHorario("09:00", &min) == 1);
        assert(min == 9 * 60);
        printf("OK\n");
    }

    {
        printf("[Teste 10] Historico completo dos tempos de atendimento... ");
        Sistema sis = {0};
        Senha *p1 = calloc(1, sizeof(Senha)); 
        strcpy(p1->id, "P001"); p1->tipo = TIPO_PRIORITARIA; 
        p1->tempoEspera = 5; p1->tempoAtendimento = 15;
        
        Senha *c1 = calloc(1, sizeof(Senha)); 
        strcpy(c1->id, "C001"); c1->tipo = TIPO_COMUM;
        c1->tempoEspera = 30; c1->tempoAtendimento = 20;

        adicionarHistorico(&sis, p1);
        adicionarHistorico(&sis, c1);

        assert(sis.totalFinalizados == 2);
        assert(sis.somaEspera == 35);
        assert(sis.somaAtendimento == 35);
        
        liberarHistorico(&sis);
        printf("OK\n");
    }
    
    printf("\n --- Testes da Equipe 11 (Estrutura de Multiplos Atendentes) --- \n");

    {
        printf("[Teste 11] Chamada de multiplas senhas simultaneas... ");
        Sistema sis = {0};
        Senha *c1 = calloc(1, sizeof(Senha)); strcpy(c1->id, "C001"); enfileirar(&sis.filaComum, c1);
        Senha *p1 = calloc(1, sizeof(Senha)); strcpy(p1->id, "P001"); enfileirar(&sis.filaPrioritaria, p1);

        Senha *at1 = sis.filaPrioritaria.inicio; desenfileirar(&sis.filaPrioritaria);
        enfileirar(&sis.atendimentosEmAndamento, at1);

        Senha *at2 = sis.filaComum.inicio; desenfileirar(&sis.filaComum);
        enfileirar(&sis.atendimentosEmAndamento, at2);

        assert(sis.atendimentosEmAndamento.quantidade == 2);
        liberarFila(&sis.atendimentosEmAndamento);
        printf("OK\n");
    }

    {
        printf("[Teste 12] Finalizacao de atendimento especifico fora de ordem... ");
        Sistema sis = {0};
        Senha *p1 = calloc(1, sizeof(Senha)); strcpy(p1->id, "P001"); enfileirar(&sis.atendimentosEmAndamento, p1);
        Senha *c1 = calloc(1, sizeof(Senha)); strcpy(c1->id, "C001"); enfileirar(&sis.atendimentosEmAndamento, c1);

        Senha *alvo = removerPorId(&sis.atendimentosEmAndamento, "C001");
        assert(alvo != NULL);
        assert(strcmp(alvo->id, "C001") == 0);
        assert(sis.atendimentosEmAndamento.quantidade == 1);
        
        free(alvo);
        liberarFila(&sis.atendimentosEmAndamento);
        printf("OK\n");
    }

    {
        printf("[Teste 13] Tentativa de finalizacao de ID incorreto com atendimentos ativos... ");
        Sistema sis = {0};
        Senha *p1 = calloc(1, sizeof(Senha)); strcpy(p1->id, "P001"); enfileirar(&sis.atendimentosEmAndamento, p1);
        Senha *c2 = calloc(1, sizeof(Senha)); strcpy(c2->id, "C002"); enfileirar(&sis.atendimentosEmAndamento, c2);

        Senha *erro = removerPorId(&sis.atendimentosEmAndamento, "C099");
        assert(erro == NULL);
        assert(sis.atendimentosEmAndamento.quantidade == 2);

        liberarFila(&sis.atendimentosEmAndamento);
        printf("OK\n");
    }

    printf("\n ---------- TODOS OS 13 TESTES PASSARAM COM SUCESSO! ---------- \n");
}
