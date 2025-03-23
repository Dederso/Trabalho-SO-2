#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

#define N 4 // Número de slots disponíveis no buffer
#define NUM_PRODUTORES 4 // Número de threads produtoras

// Estrutura para representar um pedido
typedef struct {
    char nome_paciente[50];
    int id_medicamento;
    int quantidade;
} Pedido;

// Buffer circular
Pedido buffer[N];
int writepos = 0; // Posição para escrita no buffer
int readpos = 0;  // Posição para leitura no buffer

// Semáforos
sem_t empty; // Indica slots livres
sem_t full;  // Indica slots ocupados
sem_t lock;  // Controle de acesso ao buffer

// Variável para contar o número de pedidos processados
int pedidos_processados = 0;

// Variável para contar o número de pedidos finalizados
int pedidos_finalizados = 0;

// Função do produtor
void* produtor(void* arg) {
    int id = *(int*)arg;
    char filename[50];
    sprintf(filename, "ordens%d.txt", id + 1);
    //printf("Produtor %d lendo arquivo %s\n", id, filename);
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Erro ao abrir o arquivo %s\n", filename);
        pthread_exit(NULL);
    }

    char linha[100];
    while (fgets(linha, sizeof(linha), file)) {
        Pedido pedido;
        sscanf(linha, "%49[^,],%d,%d", pedido.nome_paciente, &pedido.id_medicamento, &pedido.quantidade);
        
        /* //teste para ver o funcionamento dos produtores
        char temp[100];
        strcpy(temp, pedido.nome_paciente);
        sprintf(pedido.nome_paciente, "%d %s", id + 1, temp);
        /*/

        // Espera por um slot livre
        sem_wait(&empty);

        // Acessa o buffer com exclusão mútua
        sem_wait(&lock);
        buffer[writepos] = pedido;
        writepos = (writepos + 1) % N;
        sem_post(&lock);

        // Notifica que há um item pronto para ser consumido
        sem_post(&full);
    }

    // Insere uma ordem virtual indicando término
    Pedido fim = {"FIM", -1, -1};
    sem_wait(&empty);
    sem_wait(&lock);
    buffer[writepos] = fim;
    writepos = (writepos + 1) % N;
    sem_post(&lock);
    sem_post(&full);

    fclose(file);
    pthread_exit(NULL);
}

// Função do consumidor
void* consumidor(void* arg) {
    while (1) {
        // Espera por um item para consumir
        sem_wait(&full);

        // Acessa o buffer com exclusão mútua
        sem_wait(&lock);
        Pedido pedido = buffer[readpos];
        readpos = (readpos + 1) % N;
        sem_post(&lock);

        // Notifica que o slot foi liberado
        sem_post(&empty);

        // Verifica se é uma ordem de término
        if (strcmp(pedido.nome_paciente, "FIM") == 0) {
            pedidos_finalizados++;
            if (pedidos_finalizados == NUM_PRODUTORES) {// Termina somente quando todos os produtores derem uma ordem de término
                break;
            }
            continue;
        }

        // Processa o pedido (simplesmente imprime no terminal)
        printf("Pedido processado: Paciente=%s, Medicamento=%d, Quantidade=%d\n",
               pedido.nome_paciente, pedido.id_medicamento, pedido.quantidade);
        
        pedidos_processados++;
    }

    printf("Todos os pedidos foram processados. Total: %d\n", pedidos_processados);
    pthread_exit(NULL);
}

int main() {

    pthread_t produtores[NUM_PRODUTORES];
    pthread_t consumidor_thread;

    int ids[NUM_PRODUTORES];
    
    // Inicializa os semáforos
    sem_init(&empty, 0, N);   // Inicializa com N slots livres
    sem_init(&full, 0, 0);    // Nenhum slot ocupado inicialmente
    sem_init(&lock, 0, 1);    // Mutex inicializado como disponível

    // Cria as threads produtoras
    for (int i = 0; i < NUM_PRODUTORES; i++) {
        ids[i] = i;
        pthread_create(&produtores[i], NULL, produtor, &ids[i]);
    }

    // Cria a thread consumidora
    pthread_create(&consumidor_thread, NULL, consumidor, NULL);

    // Aguarda todas as threads produtoras terminarem
    for (int i = 0; i < NUM_PRODUTORES; i++) {
        pthread_join(produtores[i], NULL);
    }

    // Aguarda a thread consumidora terminar
    pthread_join(consumidor_thread, NULL);

    // Destroi os semáforos
    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&lock);

    return 0;
}
