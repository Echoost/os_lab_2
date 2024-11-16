#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <malloc.h>

#include "pthread.h"

int port = -1;

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
};

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod, int th) {
  //printf("MultModulo th=%d: a=%lu b=%lu mod=%lu \n", th, a, b, mod);
  uint64_t result = 0;
  a = a % mod;
  while (b > 0) {
    if (b % 2 == 1)
      result = (result + a) % mod;
    a = (a * 2) % mod;
    b /= 2;
  }

  return result % mod;
}

uint64_t Factorial(const struct FactorialArgs *args) {
  uint64_t ans = 1;
  // TODO: your code here
  for (uint64_t i = (*args).begin; i < (*args).end + 1; i++) ans *= i;
  ans %= (*args).mod;
  //printf("Factorial: %lu - %lu result=%lu\n", (*args).begin, (*args).end, ans);
  return ans;
}

void *ThreadFactorial(void *args) {
  struct FactorialArgs *fargs = (struct FactorialArgs *)args;
  //printf("ThreadFactorial: %lu - %lu mod=%lu\n", (*fargs).begin, (*fargs).end, (*fargs).mod);
  return (void *)(uint64_t *)Factorial(fargs);
}

int main(int argc, char **argv) {
  //printf("start\n");
  int tnum = -1;
  while (true) {
    int current_optind = optind ? optind : 1;
    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1)
    {
      //printf("tnum=%d\tbreak\n",tnum);
      break;
    }
    
    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        port = atoi(optarg);
        // TODO: your code here
        if (port < 0) {
            printf("invalid port number.\n");
            return -1;
        }
        break;
      case 1:
        tnum = atoi(optarg);
        // TODO: your code here
        if (tnum <= 0) {
            printf("invalid number of threads.\n");
            return -1;
        }
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;
    case '?':
      printf("Unknown argument\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }
  //printf("port=%d\n",port);
  if (port == -1 || tnum == -1) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

  // Создаём сокет
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    fprintf(stderr, "Can not create server socket!\n");
    return 1;
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);
 
  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

  int err = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
  if (err < 0) {
    fprintf(stderr, "Can not bind to socket!\n");
    return 1;
  }

  err = listen(server_fd, 128); 
  if (err < 0) {
    fprintf(stderr, "Could not listen on socket\n");
    return 1;
  }

  //printf("Server listening at %d\n", port);

  //Слушаем в цикле
  while (true) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    
    //printf("wait connect\n");
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);
    //printf("connect = %d\n", client_fd);
    
    if (client_fd < 0) {
      fprintf(stderr, "Could not establish new connection\n");
      continue;
    }

    while (true) {
      unsigned int buffer_size = sizeof(uint64_t) * 3;
      char from_client[buffer_size];
      
      //Получаем сообщение от клиента
      //printf("wait recv\n");
      int read = recv(client_fd, from_client, buffer_size, 0);
      //printf("read = %d\n", read);

      if (!read)
        break;
      if (read < 0) {
        fprintf(stderr, "Client read failed\n");
        break;
      }
      if (read < buffer_size) {
        fprintf(stderr, "Client send wrong data format\n");
        break;
      }
      
      //Принимаем информацию от клиента
      uint64_t begin = 0;
      uint64_t end = 0;
      uint64_t mod = 0;
      memcpy(&begin, from_client, sizeof(uint64_t));
      memcpy(&end, from_client + sizeof(uint64_t), sizeof(uint64_t));
      memcpy(&mod, from_client + 2 * sizeof(uint64_t), sizeof(uint64_t));

      // Запущенный сервер соединяется не понятно с чем и принимат какие-то данные  
      // (клиент в тот момент не запущен)
      // Эти данный, воспринятые как uint64_t, перегружают доступные ресурсы сайта
      // и программа зависает.
      uint64_t max_value = 1000000;
      if(begin > max_value || end > max_value || mod > max_value)
      {
        //printf("continue\n");
        continue;
      }
      else
      {
        printf("%d receive: %lu %lu %lu\n", port, begin, end, mod);
      }
      pthread_t threads[tnum];
      struct FactorialArgs args[tnum];

      // Разделяем данные между потоками
      for(int i = 0; i < tnum; i++) {
          args[i].mod = mod;
      }
      if(tnum == 1)
      {
        args[0].begin = begin;  
        args[0].end = end;  
      }
      else
      {
        uint64_t length = end - begin + 1;
        uint64_t step = length/tnum;

        args[0].begin = begin;  
        args[0].end = begin + step; 
        
        for(int i = 1; i < tnum - 1; i++) {
          args[i].begin = args[i - 1].end + 1;
          args[i].end = args[i].begin + step;
        }

        args[tnum - 1].begin = args[tnum - 2].end + 1;
		    args[tnum - 1].end = end;
      }
      
      
      for (int i = 0; i < tnum; i++) {
        // TODO: parallel somehow
        //printf("ARGS: thread=%d: %lu - %lu\n", i, args[i].begin, args[i].end);
        if (pthread_create(&threads[i], NULL, ThreadFactorial, (void *)&args[i])) {
          printf("Error: pthread_create failed!\n");
          return 1;
        }
      }
      //printf("all threads start\n");
      
      uint64_t total = 1;
      for (int i = 0; i < tnum; i++) {
        uint64_t result = 0;
        pthread_join(threads[i], (void **)&result);
        //printf("threads %d end; result=%lu\n",i, result);
        total = MultModulo(total, result, mod, i);
      }

      //printf("%d total: %lu\n", port, total);

      char buffer[sizeof(total)];
      memcpy(buffer, &total, sizeof(total));
      err = send(client_fd, buffer, sizeof(total), 0);
      if (err < 0) {
        fprintf(stderr, "Can't send data to client\n");
        break;
      }
      //printf("end recv while\n\n");
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);

    //printf("end connect while\n-------------------------------------------\n\n\n");
  }
  return 0;
}