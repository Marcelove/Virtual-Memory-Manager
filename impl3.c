#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

int frame;

int valorProPrint, encontrado = 0, mudar = 0, naoachei = 0;

int FIFOatualTLB = 0, FIFOatualPAGE =0;; //saber onde estou no fifo

int FIFOPAGE = 0, FIFOTLB = 0; //indentificadores de ordenação

int falso = 0; //acabaram os números no txt
int enderecosTraduzidos = 0, pageFaults = 0, tlbHits = 0; //números que precisarão serem contabilizados
 
int TLB[16][2];
int pagetable[128];
signed char memoria_fisica[256*256];

unsigned char contador = 0; //contador da framenumber
int cont = 0; //cont para saber onde estou na remoção em estilo LRU

struct valoris {
        int pagenumber;

};

/*
struct stack {

        int pagenumber;
        struct stack * proximo;
};

struct stack *inicio = NULL;

void remover(struct stack *atual, int pagenumber, struct stack *anterior)
{
        if (atual != NULL) {
                if (atual->pagenumber == pagenumber) {

                        if(cont == 0){
                                inicio = atual->proximo;
                                free(atual);
                        }
                        else {
                                anterior->proximo = atual->proximo;
                                free(atual);
                                cont = 0;
                        }
                                
                    
                } 
                else {
                        cont=1;
                        remover(atual->proximo, pagenumber, atual);
                }
        }

        return;
}


void imprimir(struct stack *atual)
{
        if (atual != NULL) {
                if (atual->proximo == NULL) {
                        printf("%d\n", atual->pagenumber);
                        return;
                }
                printf("%d ", atual->pagenumber);
                imprimir(atual->proximo);
        }

}


//inserir pagenumber para a STACK do LRU
void inserir(struct stack *atual, int pagenumber)
{
        //Crio a nova pagenumber para a stack
        struct stack *novo = malloc(sizeof(struct stack));

        novo->pagenumber = pagenumber;
        novo->proximo = NULL;

        //Se atual for NULL, insere logo nele
        if (atual == NULL)
                atual = novo;
        //Sen?o, quero rodar a lista at? achar o ponteiro pro NULL, ou seja
        //O ultimo elemento
        else {
                while (atual->proximo != NULL)
                        atual = atual->proximo;

                atual->proximo = novo;
        }
        
}
*/


void adiconaTLB (int pagenumber, int frame)
{       
        
        TLB[FIFOatualTLB][0] = pagenumber;
        TLB[FIFOatualTLB][1] = frame;
        
        if ((FIFOatualTLB) == 16)
                FIFOatualTLB = 0;
}


int procuraPageNumberNaPageTable (int pagenumber)
{

        int a;

        //procurar pagenumber na pagetable
        for (a=0; a < 128; a++){

                //se encontrei o pagenumber, quero retornar o índice correspondente
                if (pagetable[a] == pagenumber){
                        return a; //que é justamente o frame na memória física
                }
        }

        return -1;
}

void * procuraPageNumberNaTLB (void * valores)
{

        //struct que me deixa operar com os valores q eu desejo na thread
        struct valoris * novo = (struct valoris*)valores;

        int a;

        for (a=0; a < 16; a++){

                //se tlb do índice [a] valor [0] for igual ao pagenumber
                if (TLB[a][0] == novo->pagenumber){

                        valorProPrint = a;
                        encontrado = 1;

                        frame = TLB[a][1]; //frame recebe o frame correspondente
                        return NULL;
                }
        }

        frame = -1;  //frame n foi encontrado
        return NULL;
}

void peganumero (FILE *fp, FILE * saida)
{
       
        char c;                      //char do txt
        int count = 0;               //contador
        int size = 10;               //tamanho para o buffer
        int offset;                  //offset
        int endereco_virtual;        //endereco virtual
        int pagenumber;              //pagenumber
        char *buf = malloc(size);    //buffer de tamanho 10
        char buf2[256];              //buffer pra salvar os bytes do .bin
        int endereco_fisico;         //endereco físico
        signed char value;


        struct valoris * valores = malloc(sizeof(struct valoris));

        //pego char enquanto houver
        for (c = getc(fp); c != EOF; c = getc(fp)){

                //se tamanho exceder, realloc
                if (count >= size-1){
                        size += 10;
                        buf = realloc(buf, size);
                }

                //cheguei no fim do numero atual
                if (c == '\n'){

                        //incremento endereços traduzidos
                        enderecosTraduzidos++;

                        endereco_virtual = atoi(buf);                 //endereco virtual recebe o atoi do buffer
                        offset = endereco_virtual & 255;              //offset é endereço virtual mascarado na esquerda
                        pagenumber = (endereco_virtual >> 8) & 255;   //pagenumber é endereço virtual mascarado na direita
                    
                        valores->pagenumber = pagenumber; //passo o pagenumber pra uma struct

                        pthread_t novaThread1; //crio a thread

                        pthread_create(&novaThread1, NULL, procuraPageNumberNaTLB, (void*) valores); // a thread procura na TLB
                        pthread_join(novaThread1, NULL); // me certifico de que a thread terminou
                        
                        //encontrei na TLB
                        if (encontrado == 1){
                                tlbHits++;
                        }

                        else {
                                frame = procuraPageNumberNaPageTable(pagenumber); //procuro o pagenumber na pagetable

                                //não encontrei na pagetable
                                if (frame == -1){

                                        pageFaults++;

                                        //abro o arquivo binário em forma binária diferenciada
                                        int backingd = open("BACKING_STORE.bin",  O_RDONLY);

                                        //backing é o conjunto de chars que eu quero
                                        signed char *backing = mmap(0, 256*256, PROT_READ, MAP_PRIVATE, backingd, 0); 

                                        frame = FIFOatualPAGE; //se ele não encontrar nem na TLB nem na PTABLE, frame =
                                        FIFOatualPAGE ++;

                                        if(FIFOatualPAGE == 128)
                                                FIFOatualPAGE = 0;

                                        pagetable[frame] = pagenumber;

                                        //copio o conjunto de bytes específicos para memoria física
                                        memcpy(memoria_fisica + (frame * 256), backing + pagenumber *256, 256);

                                        adiconaTLB(pagenumber, frame); //adiciono na TLB o pagenumber novo e o frame novo
                                }
                                else{
                                        adiconaTLB(pagenumber, frame); //adiciono na TLB o pagenumber novo e o frame novo
                                }
                                
                        }



                        int endereco_fisico = (frame << 8) | offset;  //endereço físico é o frame concatenado com o offset

                        if(encontrado == 1){
                                //printf("Virtual address: %d TLB: %d Physical address: %d Value: %d\n", endereco_virtual, valorProPrint, endereco_fisico, memoria_fisica[frame * 256 + offset]);
                                fprintf(saida, "Virtual address: %d TLB: %d Physical address: %d Value: %d\n", endereco_virtual, valorProPrint, endereco_fisico, memoria_fisica[frame * 256 + offset]);
                                encontrado = 0;
                        }
                        else{
                                //printf("Virtual address: %d TLB: %d Physical address: %d Value: %d\n", endereco_virtual, FIFOatualTLB, endereco_fisico, memoria_fisica[frame * 256 + offset]);
                                fprintf(saida,"Virtual address: %d TLB: %d Physical address: %d Value: %d\n", endereco_virtual, FIFOatualTLB, endereco_fisico, memoria_fisica[frame * 256 + offset]);

                                FIFOatualTLB++;
                                if (FIFOatualTLB == 16)
                                        FIFOatualTLB = 0;
                        }

                        free(valores);
                        memset(buf, 0, sizeof(buf));

                        return;
                }
                //buf recebe o novo char se não é \n
                buf[count] = c;
                //count para regular o buffer
                count++;
        }

        falso = 1; // se o FOR conseguiu terminar sem antes ocorrer um return é porque chegou em EOF
}

void varreArquivo (FILE * fp)
{
        char c;

        for (c = getc(fp); c != EOF; c = getc(fp)){
                
                if ((c != '0') && (c != '1') && (c != '2') && (c != '3') && (c != '4') && (c != '5') && (c != '6') && (c != '7') && (c != '8') && (c != '9') && (c != '\n')){
                        printf("Error at addresses.txt format. Exiting\n");
                        exit(1);
                }
        }
                
}


int main(int argc, char * argv[])
{

        //----------------------------Erros no formato do arquivo-----------------------------------//

        FILE *backingteste = fopen("BACKING_STORE.bin", "r");

        if (backingteste == NULL){
                printf("Error at opening BACKING_STORE.bin. Exiting.\n");
                exit(1);
        }

        fclose(backingteste);
        //-----------------------------------Erros nos argumentos----------------------------------//

        //Erro no número de argumentos
        if (argc != 4){
                printf("Error: invalid number of arguments. Expected 3. Exiting.\n");
                return 0;
        }

        //Erro no primeiro argumento passado.
        if((strcmp(argv[2], "fifo") != 0) && (strcmp(argv[2], "lru") != 0)){

                printf("Error: invalid first command '%s'. Exiting.", argv[2]);
                return 0;
        }

        //Erro no segundo argumento passado
        if((strcmp(argv[3], "fifo") != 0) && (strcmp(argv[3], "lru") != 0)){

                printf("Error: invalid second command '%s'. Exiting.\n", argv[3]);
                return 0;
        }
        //-----------------------------------------------------------------------------------//

        //FIFO na pagetable
        if (strcmp(argv[2], "fifo") == 0){
                FIFOPAGE = 1;
        }
        else {
                printf("Did not implement lru. Exiting.\n");
                exit(1);
        }

        //FIFO na TLB
        if (strcmp(argv[3], "fifo") == 0){
                FIFOTLB = 1;
        }
        else {
                printf("Did not implement lru. Exiting.\n");
                exit(1);
        }

        int a;

        //pagetable e memória física inicialmente vazias
        for (a = 0; a < 128; a++){
                pagetable[a] = -1; 
                memoria_fisica[a] = -1;
        }

        //TLB inicalmente vazia
        for (a = 0; a < 16; a++){
                TLB[a][0] = -1;
                TLB[a][1] = -1;
        } 

        //arquivo de saída correct.txt
        FILE * saida = fopen("correct.txt", "w");

        //arquivo com os endereços
        FILE * endereco = fopen(argv[1], "r");

        if (endereco == NULL){
                printf("Error. Couldn't open the file. Exiting.\n");
                return 0;
        }


        //enquanto houverem números no TXT
        while(falso == 0){

                //pego o número
                peganumero(endereco, saida);
        }

        fprintf(saida, "Number of Translated Addresses = %d\n", enderecosTraduzidos);
        fprintf(saida, "Page Faults = %d\n", pageFaults);
        fprintf(saida, "Page Fault Rate = %0.3f\n", (float)pageFaults / (float)enderecosTraduzidos);
        fprintf(saida, "TLB Hits = %d\n", tlbHits);
        fprintf(saida, "TLB Hit Rate = %0.3f\n", (float)tlbHits / (float)enderecosTraduzidos);

        fclose(saida);
        fclose(endereco);

        return 0;
}