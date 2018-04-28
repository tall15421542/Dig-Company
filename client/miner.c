#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <openssl/md5.h>



#define Length 2000
#define DIGEST_LENGTH 33
#define WORK 0x00
#define STOP 0x01
#define ACCEPT 0x02
#define REJECT 0x03
#define STATUS 0x04
#define QUIT 0x05
#define MINDFUL 0x06

/* set check environment */
struct timeval tv;
fd_set readset;

void print_hex(unsigned char word[Length], size_t len){
    for(int i = 0 ; i < len ; i++){
        printf("%2x ",word[i]);
    }
    printf("\n");
}

void str_to_md5(unsigned char* word, size_t len, char* digest){
    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext, word, len);
    MD5_Final (c,&mdContext);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf (&digest[i*2], "%02x", c[i]);
}


void CTX_to_str( MD5_CTX* context, char* digest){ 
    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_Final(c, context);
    for(int i = 0 ; i < MD5_DIGEST_LENGTH; i++)
        sprintf (&digest[i*2], "%02x", c[i]);
}

void md5_append( MD5_CTX* context, unsigned char* app_word, int app_len, char *digest){
    MD5_Update(context, app_word, app_len);
    CTX_to_str(context, digest);
}

int md5_zero(MD5_CTX* context,unsigned char* word, size_t len, char* digest){
    md5_append(context, word, len, digest);
    //print_hex(word, len);
    //printf("length:%d %s\n",len ,digest);
    int zero = 0;

    for(int i = 0 ; i < DIGEST_LENGTH ; i++){
        if(digest[i]=='0')
            zero++;
        else
            break;
    }
    return zero;
}

void process(unsigned char* word, int add_len, unsigned long long int try){
    unsigned long long int mask = 0xff;
    for(int i = 0 ; i <= add_len ; i++){
        word[ i ] = try & mask;
        try >>= 8;
    }
}

void read_winner(int deal_fd){
    char treasure[DIGEST_LENGTH];
    int win_len;
    char winner[Length];
    int mission;
    read(deal_fd, &win_len, sizeof(int));
    read(deal_fd, winner, sizeof(char) * win_len);
    read(deal_fd, &mission, sizeof(int));
    read(deal_fd, treasure, sizeof(char) * DIGEST_LENGTH);
    winner[win_len] = '\0';
    printf("%s wins a %d-treasure %s\n", winner, mission, treasure);
} 

void transmit_winner(int deal_fd, int add_len, unsigned char* word, int name_len, char* name, int mission, char* digest ){
    write(deal_fd, &add_len, sizeof(int));
    write(deal_fd, word, sizeof(unsigned char) * add_len);
    write(deal_fd, &name_len, sizeof(int));
    write(deal_fd, name, name_len);
    write(deal_fd, &mission, sizeof(int));
    write(deal_fd, digest, sizeof(char) * DIGEST_LENGTH);
    return;    
}

void read_work(int deal_fd, MD5_CTX* context, int *mission, int* base, int* assign_add){

    //read(deal_fd, add_len, sizeof(int));    //printf("len complete\n");
    //read(deal_fd, &word[initial_len], sizeof(unsigned char) * (*add_len) ); //printf("word complete\n");
    read(deal_fd, context, sizeof(MD5_CTX));
    read(deal_fd, mission, sizeof(int)); //printf("mission complete\n");
    read(deal_fd, base, sizeof(int));    //printf("base complete\n");
    read(deal_fd, assign_add, sizeof(int)); //printf("assign_add complete\n");
}

int accept_win(int input_fd){
    int status;
    read(input_fd, &status, sizeof(int));
    if(status == ACCEPT){
#ifdef DEBUG
        printf("accept\n");
#endif
        return 1;
    }
    else
        return 0;
}
void reopen(int* input_fd, int* output_fd, char* input_pipe, char* output_pipe){
    close(*input_fd);
    close(*output_fd);
    *input_fd = open(input_pipe, O_RDWR);
    assert (*input_fd >= 0);

    *output_fd = open(output_pipe, O_RDWR);
    assert (*output_fd >= 0);
    
}

int find(char* name, MD5_CTX* context, int mission_n, int base, int assign_add, int* input_fd_str, int* output_fd_str, 
         int* command, int* awake, char* input_pipe, char* output_pipe)
{
    /* working on mining*/
    int input_fd = *input_fd_str;
    int output_fd = *output_fd_str;
    unsigned char word[Length];
    int add_len = 1;
    unsigned long long int bound = 256;
    unsigned long long int try = base;
    int complete = 0;
        
    fd_set read_set, working_read;
    FD_ZERO(&read_set);
    FD_SET(input_fd, &read_set);

    while(complete != mission_n){

        /* check message from server */
        memcpy(&working_read, &read_set, sizeof(fd_set));
        int ret = select(input_fd+1, &working_read, NULL, NULL, &tv);
        if(ret==-1)
            perror("select()");
        else if(FD_ISSET(input_fd, &working_read)){
            int protocol;
            read(input_fd, &protocol, sizeof(int));
                
            switch(protocol){
                case STOP :
                {
                    read_winner(input_fd);
                    return 0;
                    break;
                }
                case STATUS :
                    *command = STATUS;
                    break;
                case QUIT :
                    *command = QUIT;
                    *awake = 0;
                    reopen(input_fd_str, output_fd_str, input_pipe, output_pipe);
                    printf("BOSS is at rest.\n");
                    return 1;
                    break;
                default:
                    break;
             }
        }
        

        /* find */
        char digest[DIGEST_LENGTH];
        MD5_CTX working_CTX;
        memcpy(&working_CTX, context, sizeof(MD5_CTX));
        process(word, add_len, try);
        complete = md5_zero(&working_CTX, word, add_len, digest);

        /* command status */
        if( *command == STATUS){
            printf("I'm working on %s\n", digest);
            *command = -1;
        }

        if(complete==mission_n){
            /* transmit message to server that the treasure is found */
            
            int name_len = strlen(name) + 1;
            transmit_winner(output_fd, add_len, word, name_len, name, mission_n, digest);
            if(accept_win(input_fd)){
                printf("I win a %d-treasure! %s\n", mission_n, digest);
                return 1;
            }
        }
        try += assign_add;
        if(try >= bound){
            bound <<= 8;
            add_len++;
            try = base;
        }
#ifdef DEBUG
        if(add_len == 10){
            printf("dangerous\n");
        }
#endif
    }
    return 1;
}

int main(int argc, char** argv){
    /* parse arguments */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    char *name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);
    //assert (!ret);

    ret = mkfifo(output_pipe, 0644);
    //assert (!ret);

    /* open pipes */
    int input_fd = open(input_pipe, O_RDWR);
    assert (input_fd >= 0);

    int output_fd = open(output_pipe, O_RDWR);
    assert (output_fd >= 0);
    /* initialize fd_set) */
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    FD_ZERO(&readset);
    FD_SET(input_fd, &readset);

    fd_set working_read;

    int awake = 0;
    int command = -1;
    while(1){
        /* check input_fd */
        memcpy(&working_read, &readset, sizeof(fd_set));
        int ret = select(input_fd+1, &working_read, NULL, NULL, &tv);
        if(ret == -1)
            perror("select():");
        else if(FD_ISSET(input_fd, &working_read)){  
            int protocol;
            read(input_fd, &protocol, sizeof(int));
            if(protocol == MINDFUL){
                printf("BOSS is mindful.\n");
            }
            else if(protocol == WORK){
                int mission;
                int base;
                int assign_add;
                MD5_CTX context;
                MD5_Init(&context);
                read_work(input_fd, &context, &mission, &base, &assign_add);
                find(name, &context, mission, base, assign_add, &input_fd, &output_fd, &command, &awake, input_pipe, output_pipe);
                continue;
            }
            else if(protocol == STOP){
                read_winner(input_fd);
                continue;
            }
            else if(protocol == STATUS){
                command = STATUS;
            }
            else if(protocol == QUIT){
                command = QUIT;
                printf("BOSS is at rest.\n");
                reopen(&input_fd, &output_fd, input_pipe, output_pipe);
                awake = 0;
            }
            else{
#ifdef DEBUG
                printf("unknown protocol: %d\n", protocol);
#endif
            }
            
        }
    }

    /* TODO write your own (1) communication protocol (2) computation algorithm */
    /* To prevent from blocked read on input_fd, try select() or non-blocking I/O */
    

    return 0;
}

#ifdef DEBUG
int main(int argc, char **argv)
{
    unsigned char num[Length];
    num[0]='B';
    num[1]='0';
    num[2]='4';
    num[3]='7';
    num[4]='0';
    num[5]='3';
    num[6]='0';
    num[7]='0';
    num[8]='1';
    num[9] = 0x36;
    num[10] = 0x06;
    if(find(3, num, 11)){
        printf("yes\n");
    }
#endif

