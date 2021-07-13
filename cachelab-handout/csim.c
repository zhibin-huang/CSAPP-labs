#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef struct Set{
    int latest;
    struct Line *line;
} Set;

typedef struct Line{
    int valid;
    int cnt;
    unsigned long long tag;
} Line;

int hits, misses, evictions;


Set* init_cache(int set_bits, int line_num){
    int set_num = 1 << set_bits;
    Set *sets = (Set*)malloc(set_num * sizeof(Set));
    assert(sets);
    for(int i = 0; i < set_num; ++i){
        sets[i].line = (Line*)malloc(line_num * sizeof(Line));
        assert(sets[i].line);
        sets[i].latest = 0;
        Line* lptr = sets[i].line;
        for(int i = 0; i < line_num; ++i){
            lptr[i].valid = 0;
            lptr[i].tag = 0;
            lptr[i].cnt = 0;
        }
    }
    return sets;
}


void destory(Set* begin, int set_len){
    for(int i = 0; i < set_len; ++i){
        free(begin[i].line);
        begin[i].line = NULL;
    }
    free(begin);
    begin = NULL;
    return;
}


void eviction(unsigned long long tag, Set* s, char* detail, int line_num){
    int minn = s->latest, id = 0, i = 0;
    for(i = 0; i < line_num; ++i){
        if(minn >= s->line[i].cnt){
            minn = s->line[i].cnt;
            id = i;
        }
        if(s->line[i].valid == 0){
            id = i;
            break;
        }
    }
    s->line[id].cnt = s->latest + 1;
    s->latest++;
    s->line[id].tag = tag;
    s->line[id].valid = 1;
    if(i == line_num) {
        strcat(detail, " eviction");
        evictions++;
    }
}


void load_data(Set* sets, unsigned long long address, char* detail, int set_bits, int block_bits, int line_num){
     int idx = (address >> block_bits) & ((1 << set_bits) - 1);
     unsigned long long tag = address >> (set_bits + block_bits);
     Set *s = &sets[idx];
     int i = 0;
     for(i = 0; i < line_num; ++i){
         if(s->line[i].tag == tag && s->line[i].valid){
             s->line[i].cnt = s->latest + 1;
             s->latest++;
             hits++;
             strcat(detail, " hit");
             break;
         }
     }
    if(i == line_num){
        misses++;
        strcat(detail, " miss");
        eviction(tag, s, detail, line_num);
    } 
}


void store_data(Set* sets, unsigned long long address, char* detail, int set_bits, int block_bits,int line_num){
     int idx = (address >> block_bits) & ((1 << set_bits) - 1);
     unsigned long long tag = address >> (set_bits + block_bits);
     Set *s = &sets[idx];
     int i = 0;
     for(i = 0; i < line_num; ++i){
         if(s->line[i].tag == tag && s->line[i].valid){
             s->line[i].cnt = s->latest + 1;
             s->latest++;
             hits++;
             strcat(detail, " hit");
             break;
         }
     }
    if(i == line_num){
        misses++;
        strcat(detail, " miss");
        eviction(tag, s, detail, line_num);
    } 
}


void modify_data(Set* sets, unsigned long long address, char* detail, int set_bits, int block_bits, int line_num){
     load_data(sets, address, detail, set_bits, block_bits, line_num);
     store_data(sets, address, detail, set_bits, block_bits, line_num);
}


int main(int argc, char* argv[])
{
    int ch;
    int set_bits, block_bits, line_num, output_flag = 0;
    char *filename;
    while((ch = getopt(argc, argv, "s:E:b:t:v")) != -1){
        switch(ch){
            case 's':
                set_bits = atoi(optarg);
                break;
            case 'E':
                line_num = atoi(optarg);
                break;
            case 'b':
                block_bits = atoi(optarg);
                break;
            case 't':
                filename = (char*)malloc(sizeof(char) * strlen(optarg));
                strcpy(filename, optarg);
                break;
            case 'v':
                output_flag = 1;
                break;
            default:
            break;
        }
    }
    Set *sets = init_cache(set_bits, line_num);
    
    FILE *f = fopen(filename, "r");
    char op; 
    char tmp[20];
    int d_size;
    unsigned long long address;
    char *detail = (char*)malloc(80 * sizeof(char));
    while(fscanf(f, " %c%s", &op, tmp) != -1){
        char *p;
        address = strtoul(strtok(tmp, ","), &p, 16);
        d_size = atoi(strtok(NULL, ","));
        switch(op){
            case 'L':
                load_data(sets, address, detail, set_bits, block_bits, line_num);
                if(output_flag) printf("L %llx,%d%s\n", address, d_size, detail);
                break;
            case 'M':
                modify_data(sets, address, detail, set_bits, block_bits, line_num);
                if(output_flag) printf("M %llx,%d%s\n", address, d_size, detail);
                break;
            case 'S':
                store_data(sets, address, detail, set_bits, block_bits, line_num);
                if(output_flag) printf("S %llx,%d%s\n", address, d_size, detail);
                break;
            default: break;
        }
        detail[0] = 0;
    }
    printSummary(hits, misses, evictions);
    free(detail);
    destory(sets, 1 << set_bits);
    fclose(f);
    free(filename);
    return 0;
}
