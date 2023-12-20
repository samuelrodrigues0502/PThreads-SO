#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/unistd.h>
#include <stdbool.h>

//cores.
#define RED    "\033[31m"
#define NONE   "\033[0m"
#define BLUE   "\033[34m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"

#define usoMax 2 //Quantidade de usos do microondas por pessoa

pthread_mutex_t muM = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t muF = PTHREAD_MUTEX_INITIALIZER;
bool deuRuim = false;//Indicador de deadlock

int proximo(int i) {//Função que pega o próximo da fila circular de pessoas
    return (i + 1) % 7;
}

typedef struct pessoa {
    int id; //Identificador da pessoa
    char* nome; //Nome
    int prio; //Prioridade
    int enve; //Envelhecimento
    int qtduso; //quantidade de usos
    int qtdfurafila; //quantidade de vezes que a pessoa foi passada para trás
    pthread_cond_t sema; //Cond Semáforo
} Pessoa;

Pessoa* vetP[7]; //Fila de pessoas
int posI = 0, posF = 0; //Controladores da fila

void usar(Pessoa* pes) {//Função de uso do microondas
    pthread_mutex_lock(&muF); //Tranca a fila

    if (posF == posI) {//Verifica se a fila está vazia.
        vetP[posF] = pes; //Insere a pessoa na úttima posição
        posF = proximo(posF); //Setando a nova posição final
    } else {
        int pos = proximo(posI);
        
        //Caso a pessoa a ser inserida for um calouro, a inserção ocorre
        //de forma diferente. Por causa da prioridade circular.
        if (pes->prio == 1) {
            //while que define a posição de inserção da pessoa de acordo com sua prioridade + envelhecimento
            while (pos != posF && (vetP[pos]->prio + vetP[pos]->enve) % 3 >= pes->prio) {
                pos = proximo(pos); //Garante a fila circular e escolhe a posição
            }

            //laço que abre espaço para a pessoa entrar na fila e empura os demais uma posição pra trás
            for (int i = posF; i != pos; i = (i + 7 - 1) % 7) {
                vetP[i] = vetP[(i + 7 - 1) % 7];
                vetP[i]->enve += (vetP[i]->qtdfurafila + 1) / 2; //envelhecimento a cada dois empurrões
                vetP[i]->qtdfurafila = (vetP[i]->qtdfurafila + 1) % 2;
            }
            vetP[pos] = pes; //Inserção da pessoa na fila
            posF = proximo(posF);//Setando a nova posição final
        } else {
            //while que define a posição de inserção da pessoa de acordo com sua prioridade + envelhecimento
            while (pos != posF && vetP[pos]->prio + vetP[pos]->enve >= pes->prio) {
                pos = proximo(pos); //Garante a fila circular e escolhe a posição
            }

            //laço que abre espaço para a pessoa entrar na fila e empura os demais uma posição pra trás
            for (int i = posF; i != pos; i = (i + 7 - 1) % 7) {
                vetP[i] = vetP[(i + 7 - 1) % 7];
                vetP[i]->enve += (vetP[i]->qtdfurafila + 1) / 2; //envelhecimento a cada dois empurrões
                vetP[i]->qtdfurafila = (vetP[i]->qtdfurafila + 1) % 2;
            }
            vetP[pos] = pes; //Inserção da pessoa na fila
            posF = proximo(posF);//Setando a nova posição final
        }

        bool verif[3] = {false, false, false}; //verifica existência das tres categorias de pessoas.

        for (int i = proximo(posI); i != posF; i = proximo(i)) {
            //verifica se há uma pessoa com prioridade de calouro
            if ((vetP[i]->prio + vetP[i]->enve) % 3 == 1) {
                verif[0] = true;
            }
            //verifica se há uma pessoa com prioridade de veterano
            if ((vetP[i]->prio + vetP[i]->enve) % 3 == 2) {
                verif[1] = true;
            }
            //verifica se há uma pessoa com prioridade de senior
            if ((vetP[i]->prio + vetP[i]->enve) % 3 == 0) {
                verif[2] = true;
            }
        }
        //Caso na fila tenha as três categorias de pessoas,
        //provavelmente há um possível deadlock
        if (verif[0] && verif[1] && verif[2]) {
            //Então a variável global é setada como true
            deuRuim = true;
        }
    }

    //enquanto não for a vez da pessoa ela espera na fila
    while (vetP[posI]->id != pes->id) {
        printf("%s entra na fila.\n", pes->nome);

        //espera a sinalização da thread que terminou de usar
        //o microondas para entrar em execução
        pthread_cond_wait(&pes->sema, &muF);
        //printf("%s foi avisado\n", pes->nome);
    }

    pthread_mutex_unlock(&muF); //Libera a fila
    pthread_mutex_lock(&muM); //Tranca o microondas

    printf(YELLOW "\n%s está usando o microondas.\n" NONE, pes->nome), fflush(stdout);

    sleep(1); //tempo durante o uso do microondas

    //printf(GREEN "%s terminou de usar\n" NONE, pes->nome), fflush(stdout);
    pes->qtduso++;
    pthread_mutex_unlock(&muM); //Libera o microondas

    printf(GREEN "%s liberou o microondas.\n" NONE, pes->nome);

    pthread_mutex_lock(&muF); //Tranca a fila
    //printf("%s vai se remover\n", pes->nome);

    pes->enve = 0;//zerando as váriaveis de controle de envelhecimento
    pes->qtdfurafila = 0;
    posI = proximo(posI);
    
    if (posI != posF) {
        //printf("%s sinalizou %s\n", pes->nome, vetP[posI]->nome);
        
        //Caso não haja nenhum deadlock a thread que perdeu a posse do
        //microondas pode sinalizar a próxima pessoa da fila
        if (deuRuim != true) {
            //sinaliza a primeira pessoa da fila para usar o microoondas,
            pthread_cond_signal(&vetP[posI]->sema);
        }
    }
    //printf("%s se removeu\n", pes->nome);

    pthread_mutex_unlock(&muF); //Libera a fila
}

void *monitor(void *dummy) {
    while (1) {     
        sleep(5);//Monitor verifica a existência de deadlock a cada 5seg
        pthread_mutex_lock(&muF);//Tranca a fila de pessoas
        
        if (deuRuim) {//Se há deadlocck ocorre o sorteio:
            int qtd = 0;
            //Laço para contar quantas pessoas há na fila
            for (int i = posI; i != posF; i = proximo(i)) {
                qtd++;
            }

            srand((unsigned) time(NULL));
            int aleat = rand() % qtd;
            //Variável auxiliar que guarda a pessoa escolhida
            Pessoa *escolhida = vetP[(posI + aleat) % 7];
            //Troca entre a primeira pessoa da fila e a pessoa escolhida
            vetP[(posI + aleat) % 7] = vetP[posI];
            vetP[posI] = escolhida;

            pthread_cond_signal(&vetP[posI]->sema);//Sinalização para a nova primeira pessoa da fila

            printf("\nMonitor detectou" RED " DEADLOCK "  NONE "e sorteou %s para usar o microondas.\n", escolhida->nome);
        }
        pthread_mutex_unlock(&muF);//Destranca a fila após o sorteio da pessoa
        deuRuim = false;//Setando a variável global que indica deadlock como false
    }
}

void *saulo(void *dummy) {
    srand((unsigned) time(NULL));

    Pessoa pessoa;
    pessoa.id = 1;
    pessoa.nome = "Saulo";
    pessoa.prio = 3;
    pessoa.enve = 0;
    pessoa.qtduso = 0;
    pessoa.sema = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    while (pessoa.qtduso < usoMax) {
        int aleat = rand() % 3;
        sleep(aleat); //Entrada na fila após um tempo aleatório      
        usar(&pessoa);
    }

}

void *kelvin(void *dummy) {
    srand((unsigned) time(NULL));
    
    Pessoa pessoa;
    pessoa.id = 2;
    pessoa.nome = "Kelvin";
    pessoa.prio = 1;
    pessoa.enve = 0;
    pessoa.qtduso = 0;
    pessoa.sema = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    while (pessoa.qtduso < usoMax) {
        int aleat = rand() % 3;
        sleep(aleat); //Entrada na fila após um tempo aleatório       
        usar(&pessoa);
    }
}

void *vanir(void *dummy) {
    srand((unsigned) time(NULL));

    Pessoa pessoa;
    pessoa.id = 3;
    pessoa.nome = "Vanir";
    pessoa.prio = 2;
    pessoa.enve = 0;
    pessoa.qtduso = 0;
    pessoa.sema = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    while (pessoa.qtduso < usoMax) {
        int aleat = rand() % 3;
        sleep(aleat); //Entrada na fila após um tempo aleatório       
        usar(&pessoa);
    }
}

void *samir(void *dummy) {
    srand((unsigned) time(NULL));

    Pessoa pessoa;
    pessoa.id = 4;
    pessoa.nome = "Samir";
    pessoa.prio = 3;
    pessoa.enve = 0;
    pessoa.qtduso = 0;
    pessoa.sema = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    while (pessoa.qtduso < usoMax) {      
        int aleat = rand() % 3;
        sleep(aleat); //Entrada na fila após um tempo aleatório    
        usar(&pessoa);
    }
}

void *kamila(void *dummy) {
    srand((unsigned) time(NULL));

    Pessoa pessoa;
    pessoa.id = 5;
    pessoa.nome = "Kamila";
    pessoa.prio = 1;
    pessoa.enve = 0;
    pessoa.qtduso = 0;
    pessoa.sema = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    while (pessoa.qtduso < usoMax) {
        int aleat = rand() % 3;
        sleep(aleat); //Entrada na fila após um tempo aleatório        
        usar(&pessoa);
    }
}

void *vitor(void *dummy) {
    srand((unsigned) time(NULL));

    Pessoa pessoa;
    pessoa.id = 6;
    pessoa.nome = "Vitor";
    pessoa.prio = 2;
    pessoa.enve = 0;
    pessoa.qtduso = 0;
    pessoa.sema = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    while (pessoa.qtduso < usoMax) {
        int aleat = rand() % 3;
        sleep(aleat); //Entrada na fila após um tempo aleatório       
        usar(&pessoa);
    }
}

int main() {
    int rc, i;
    pthread_t t[7];
    
    /* Criação das threads(pessoas que vão utilizar o microondas). */
    if ((rc = pthread_create(&t[0], NULL, saulo, NULL)))
        printf("Error creating the saulo thread..\n");
    if ((rc = pthread_create(&t[1], NULL, vanir, NULL)))
        printf("Error creating the vanir thread..\n");
    if ((rc = pthread_create(&t[2], NULL, kelvin, NULL)))
        printf("Error creating the kelvin thread..\n");
    if ((rc = pthread_create(&t[3], NULL, samir, NULL)))
        printf("Error creating the samir thread..\n");
    if ((rc = pthread_create(&t[4], NULL, vitor, NULL)))
        printf("Error creating the vitor thread..\n");
    if ((rc = pthread_create(&t[5], NULL, kamila, NULL)))
        printf("Error creating the kamila thread..\n");
    if ((rc = pthread_create(&t[6], NULL, monitor, NULL)))
        printf("Error creating the monitor thread..\n");

    //Laço que espera a finalização de todas as threads.
    for (i = 0; i < 6; i++)
        pthread_join(t[i], NULL); 
    pthread_cancel(t[6]);//Aposenta o monitor

    printf("\nThreads encerradas.\n");
    return 0;
}