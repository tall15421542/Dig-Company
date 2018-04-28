#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/md5.h>

#include "boss.h"

#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory
#define Length 256
#define DIGEST_LENGTH 33
#define MAX_FILE 1200000000
#define WORK 0x00
#define STOP 0x01
#define ACCEPT 0x02
#define REJECT 0x03
#define STATUS 0x04
#define QUIT 0x05
#define MINDFUL 0x06
#define BUFFERSIZE 4096

typedef struct DUMP_LIST{
    int used;
    int fd;
    int len;
}DUMP_LIST;

void print_hex(unsigned char* word, size_t len){
    for(int i = 0 ; i < len ; i++){
        printf("%2x ",word[i]);
    }
    printf("\n");
}

void str_to_md5(unsigned char* word, int len, char* digest){
    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext, word, len);
    MD5_Final (c,&mdContext);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) 
        sprintf(&digest[2*i],"%02x", c[i]);
}

void CTX_to_str(MD5_CTX* context, char* digest){
    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_Final(c, context);
    for(int i = 0 ; i < MD5_DIGEST_LENGTH ; i++)
        sprintf(&digest[i*2], "%02x", c[i]);
}

void MD5_append(MD5_CTX* context, unsigned char* word, int len){
    MD5_Update(context, word, len);
}

void MD5_CHECK_TREA(MD5_CTX* context, unsigned char* word, int len, int len_now, int* win_len, int* mission, int unit, char* treasure){ 
    for(int i = 0 ; i < len ; i+=unit){
        MD5_Update(context, &word[i], unit);
        char check[DIGEST_LENGTH];
        MD5_CTX working_CTX;
        memcpy(&working_CTX, context, sizeof(MD5_CTX));
        CTX_to_str(&working_CTX, check);
        int zero = 0;
        for(int j = 0; j < DIGEST_LENGTH ; j++){
            if(check[j]=='0')
                zero++;
            else
                break;
        } 
        if(zero == *mission){
            *win_len = len_now + i+1;
            memcpy(treasure, check, DIGEST_LENGTH);
#ifdef MD5_DEBUG
            printf("%d\n", *win_len);
            printf("%s\n", check);
#endif
            *mission = *mission + 1;
        }
    }
}

void MD5_MINE(MD5_CTX* context, int fd, int* complete, unsigned char* word, int* len, int* win_len, int* mission, char* treasure){
    int read_len;
    unsigned char read_word[BUFFERSIZE];
    read_len = read(fd, read_word, sizeof(unsigned char) * BUFFERSIZE);

    /* judge if EOF */
    if(read_len == 0){
        close(fd);
        *complete = 1;
#ifdef DEBUG
        printf("mine is read!!\n");
#endif
        return;
    }
    if(read_len < BUFFERSIZE)
        MD5_CHECK_TREA(context, read_word, read_len, *len, win_len, mission, 1, treasure);
    else
        MD5_CHECK_TREA(context, read_word, read_len, *len, win_len, mission, read_len, treasure);
    memcpy(&word[*len], read_word, sizeof(unsigned char) * read_len);
    *len = *len + read_len;
}


int md5_zero(unsigned char* word, size_t len){
    char digest[DIGEST_LENGTH];
    str_to_md5(word, len, digest);
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
int load_config_file(struct server_config *config, char *path)
{
    /* TODO finish your own config file parser */
     FILE* fp = fopen(path, "r");
     assert(fp!=NULL);
     char input[PATH_MAX];
     int mine_num = 0;

     while((fscanf(fp, "%s", &input))!=EOF){
        if (strcmp(input, "MINE:") == 0){
            char* mine_path = (char*)malloc(sizeof(char) * PATH_MAX);
            assert(mine_path != NULL);
            fscanf(fp, "%s", mine_path);
            config->mine_file = mine_path;
        }
        else if(strcmp(input, "MINER:")==0){
            mine_num ++;
            config->pipes = realloc(config->pipes, sizeof(struct pipe_pair)*mine_num);
            char* pipe_in = (char*)malloc(sizeof(char) * PATH_MAX);
            char* pipe_out = (char*)malloc(sizeof(char) * PATH_MAX);
            fscanf(fp, "%s %s", pipe_in, pipe_out);
            config->pipes[mine_num-1].input_pipe = pipe_in;
            config->pipes[mine_num-1].output_pipe = pipe_out; 
        }
     }
    
    config->num_miners = mine_num;/* number of miners */;
    fclose(fp);

    return 0;
}

void transmit_work(int deal_fd , int mission, MD5_CTX* context, int base, int assign_add){
    int protocol = WORK;
    write(deal_fd, &protocol, sizeof(int)); //printf("protocol complete\n");
    //write(deal_fd, &add_len, sizeof(int));      //printf("len complete\n");
    //write(deal_fd, &word[len - add_len], sizeof(unsigned char) * add_len); //printf("word complete\n");
    write(deal_fd, context, sizeof(MD5_CTX));
    write(deal_fd, &mission, sizeof(int));  //printf("mission complete\n");
    write(deal_fd, &base, sizeof(int));     //printf("base complete\n");
    write(deal_fd, &assign_add, sizeof(int)); //printf("assign add complete\n");
    return;
}

int assign_jobs(struct server_config* config, struct fd_pair* fd_pair, MD5_CTX* context, int mission)
{

    /* assign work */
    int assign_add = config->num_miners;
    for(int i = 0 ; i < config->num_miners ; i++){
        int deal_fd = fd_pair[i].input_fd;
        int base = i;
        transmit_work(deal_fd, mission, context, base, assign_add);
    }
}

void broadcast_status(int mine_num, struct fd_pair* fd_pair){
    for(int i = 0 ; i < mine_num ; i++){
        int protocol = STATUS;
        write(fd_pair[i].input_fd, &protocol, sizeof(int));
    }
}

void transmit_winner(int deal_fd, int len, char* name, int mission, char* digest){
    int protocol = STOP;
    write(deal_fd, &protocol, sizeof(int));
    write(deal_fd, &len, sizeof(int));
    write(deal_fd, name, sizeof(char) * len);
    write(deal_fd, &mission, sizeof(int));
    write(deal_fd, digest, sizeof(char) * DIGEST_LENGTH);
    return;
}

void broadcast(int win_mine, int mine_num, struct fd_pair* fd_pair, char* name, int mission, char* digest){
    for(int i = 0 ; i < mine_num ; i++){
        if(i==win_mine)
            continue;
        int deal_fd = fd_pair[i].input_fd;
        int len = strlen(name);
        transmit_winner(deal_fd, len, name, mission, digest);
    }
}
void read_winner(int deal_fd, int *add_len, unsigned char* word, char* name, int* mission, char* treasure ){
    int name_len;
    read(deal_fd, add_len, sizeof(int));
    read(deal_fd, word, sizeof(unsigned char) * (*add_len) );
    read(deal_fd, &name_len, sizeof(int));
    read(deal_fd, name, sizeof(char) * name_len);
    read(deal_fd, mission, sizeof(int));
    read(deal_fd, treasure, sizeof(char) * DIGEST_LENGTH);
    return;
}

void close_all(struct fd_pair* fd_pair, int num){
    for(int i = 0 ; i < num ; i++){
        close(fd_pair[i].input_fd);
        close(fd_pair[i].output_fd);
    }
}

void command_status(int mine_num, struct fd_pair* fd_pair, int mission, char* treasure, int len, int complete){
    if(mission==0 || !complete)
        printf("best 0-treasure in 0 bytes\n");
    else
        printf("best %d-treasure %s in %d bytes\n", mission, treasure, len);
    if(complete)
        broadcast_status(mine_num, fd_pair);
}

void print_DUMP(struct DUMP_LIST dump){
    printf("fd(%d), len = %d, used = %d\n", dump.fd, dump.len, dump.used );    
}

void command_dump(fd_set* writeset, unsigned char* word, int len, DUMP_LIST* dump_list, int* dump_num, int complete){
    char path[PATH_MAX];
    scanf("%s", path);
#ifdef DEBUG
    printf("%s\n", path);
#endif
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, REG_S_FLAG);
    assert( fd >= 0 );
    if(!complete){
        close(fd);
        return;
    }
    FD_SET(fd, writeset);
    dump_list[*dump_num].len = 0;
    dump_list[*dump_num].fd = fd;
    dump_list[*dump_num].used = 1;
#ifdef DEBUG
    print_DUMP(dump_list[*dump_num]);
#endif
    *dump_num = *dump_num + 1;
}

void reset_dump_num(DUMP_LIST* dump_list, int* dump_num){
    int exist = 0;
    for(int i = (*dump_num) - 1 ; i >= 0 ; i--){
        if(dump_list[i].used ){
            exist = 1;
            *dump_num = i+1 ; 
            break;
        }
    }
    if(!exist)
        *dump_num = 0;
#ifdef DEBUG
    printf("max: %d\n", *dump_num);
#endif
}

void broadcast_quit(struct fd_pair* fd_pair, int num){
    int protocol = QUIT;
    for(int i = 0 ; i < num ; i++){
        write(fd_pair[i].input_fd, &protocol, sizeof(int));
    }
}

void command_quit(int len, unsigned char* word, struct server_config* config, struct fd_pair* fd_pair, fd_set* readset, int complete){
    if(complete){
        int fd = open(config->mine_file, O_WRONLY | O_TRUNC);
        write(fd, word, sizeof(unsigned char) * len);
        close(fd);
    }
    broadcast_quit(fd_pair, config->num_miners);
    FD_ZERO(readset);
    close_all(fd_pair, config->num_miners);
}

int max(int a, int b){
    if(a>=b)
        return a;
    else
        return b;
}

void reject_winner(int input_fd){
    int status = REJECT;
    write(input_fd, &status, sizeof(int));
}

void update(int *win_i, int i, int add_len, int*len, unsigned char* word, unsigned char* add_word, char* winner, char* name, char* win_treasure, char* treasure){
    *win_i = i;
    memcpy(&word[*len], add_word, sizeof(unsigned char) * (add_len));
    *len = *len + add_len;
    strcpy(winner, name);
    winner[strlen(name)] = '\0';
    strcpy(win_treasure, treasure);
}

void accept_winner(int fd, int *win_i, int i, int add_len, int*len, unsigned char* word, unsigned char* word_now, char* winner, char* name, char* win_treasure, char* treasure){
    int status = ACCEPT;
    write(fd, &status, sizeof(int));
    update(win_i, i, add_len, len, word, word_now, winner, name, win_treasure, treasure);
}

void broadcast_mindful(struct fd_pair* fd_pair, int num){
    int protocol = MINDFUL;
    for(int i = 0 ; i < num ; i ++){
        write(fd_pair[i].input_fd, &protocol, sizeof(int));
    }
}
     

int main(int argc, char **argv)
{
    /* sanity check on arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    /* load config file */
    struct server_config config;
    config.pipes = (struct pipe_pair*) malloc(sizeof(struct pipe_pair));
    load_config_file(&config, argv[1]);

    /* open the named pipes */
    struct fd_pair client_fds[config.num_miners];

    for (int ind = 0; ind < config.num_miners; ind += 1)
    {
        struct fd_pair *fd_ptr = &client_fds[ind];
        struct pipe_pair *pipe_ptr = &config.pipes[ind];
        
        fd_ptr->input_fd = open(pipe_ptr->input_pipe, O_RDWR);
        assert (fd_ptr->input_fd >= 0);

        fd_ptr->output_fd = open(pipe_ptr->output_pipe, O_RDWR);
        assert (fd_ptr->output_fd >= 0);
    }
    broadcast_mindful(client_fds, config.num_miners);
    
    /* deal with data transmitted later */
    static unsigned char word[MAX_FILE];
    MD5_CTX context_now;
    MD5_Init(&context_now);
    int len = 0;
    char win_treasure[DIGEST_LENGTH];
    int mine_fd = open(config.mine_file, O_RDONLY);
    assert(mine_fd>=0);
    int complete = 0;
    int mission = 1;
    
    /* initialize data for select() */
    int maxfd;
    fd_set readset, working_readset;
    fd_set writeset, working_write;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);

    FD_ZERO(&writeset);
    // TODO add input pipes to readset, setup maxfd
    for(int i = 0 ; i < config.num_miners ; i++){
        FD_SET(client_fds[i].output_fd, &readset);
    }
    maxfd = client_fds[config.num_miners-1].output_fd + 1;

    int dump_num = 0;
    DUMP_LIST dump_list[Length];
    memset(dump_list, 0, sizeof(DUMP_LIST) * Length);
    int quit = 0;
    int win_len = 0;

    while (1)
    {   
        if(!complete && !quit){
            MD5_MINE(&context_now, mine_fd, &complete, word, &len, &win_len, &mission, win_treasure);
            if(complete){
                
                assign_jobs(&config, client_fds, &context_now, mission);
#ifdef NONBLOCK
                printf("assign!\n");
#endif
            }
        }
        if(dump_num == 0 && quit)
            exit(0);

        int loop_fd;
        if(dump_num==0)
            loop_fd = maxfd;
        else
            loop_fd = max(dump_list[dump_num-1].fd+1, maxfd);
        
        memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
        memcpy(&working_write, &writeset, sizeof(writeset));
        int ret = select(loop_fd, &working_readset, &working_write , NULL, &timeout);
        
        if(ret==0)
            continue;

        if (FD_ISSET(STDIN_FILENO, &working_readset))
        {
            /* TODO handle user input here */
            char cmd[20];
            scanf("%s", cmd);
#ifdef NONBLOCK
            printf("command read: %s\n", cmd); 
#endif
            if (strcmp(cmd, "status") == 0){   
                command_status(config.num_miners, client_fds, mission-1, win_treasure, win_len, complete);
            }
            else if(strcmp(cmd, "dump") == 0){
                command_dump(&writeset, word, win_len, dump_list, &dump_num, complete);
            }
            else if(strcmp(cmd, "quit")==0){
                command_quit(win_len, word, &config, client_fds, &readset, complete);
                quit = 1;
                continue;
            }
        }
#ifdef NONBLOCK
        if(quit == 1)
            printf("soon to quit\n");
#endif
        /* check dump_list */
        for(int i = 0 ; i < dump_num ; i++){
            if(!dump_list[i].used)
                continue;
            int deal_fd = dump_list[i].fd;
            if(FD_ISSET(deal_fd, &working_write)){
                int written_len = dump_list[i].len;
                if(win_len - written_len <= BUFFERSIZE){
                    write(deal_fd, &word[written_len], sizeof(unsigned char) * (win_len - written_len));
#ifdef NONBLOCK
                    printf("complete writing\n");
#endif
                    FD_CLR(deal_fd, &writeset);
                    close(deal_fd);
                    dump_list[i].used = 0;
                }
                else{
                    write(deal_fd, &word[written_len], BUFFERSIZE);
                    dump_list[i].len += BUFFERSIZE;
                    break;
                }
            }   
        }

        reset_dump_num(dump_list, &dump_num);
        /* TODO check if any client send me some message
           you may re-assign new jobs to clients */
        char winner[Length];
        int win_i;
        int found = 0;

        for(int i = 0 ; i < config.num_miners ; i++){
            unsigned char add_word[Length];
            int name_len;
            char name[Length];
            int add_len;
            char treasure[DIGEST_LENGTH];
            int mission_now;
            int deal_fd = client_fds[i].output_fd;

            if(FD_ISSET(deal_fd, &working_readset)){
                read_winner(deal_fd, &add_len, add_word, name, &mission_now, treasure);
                if(mission_now != mission || found){
                    reject_winner(client_fds[i].input_fd);
                    continue;
                }
                else{
                    MD5_append(&context_now, add_word, add_len);
                    accept_winner(client_fds[i].input_fd, &win_i, i, add_len, &len, word, add_word, winner, name, win_treasure, treasure);
                    win_len = len;
                    found = 1;
                }
            }
        }

        if(found){
            printf("A %d-treasure discovered! %s\n", mission, win_treasure);
#ifdef DEBUG
            print_hex(word, win_len);
#endif
            broadcast(win_i,config.num_miners, client_fds, winner, mission, win_treasure);
            mission++; 
            assign_jobs(&config, client_fds, &context_now, mission); 
        }
    }

    /* TODO close file descriptors */
    /* free config path */
    return 0;
}
