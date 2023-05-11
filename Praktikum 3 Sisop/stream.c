//  gcc -o stream stream.c -lcjson

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/sem.h>

#define MAX_MESSAGE_SIZE 1024
#define MAX_SONGS 1000
#define MAX_SONG_NAME_LENGTH 100

struct message
{
  long mtype;
  char mtext[MAX_MESSAGE_SIZE];
};

// Rot13 decoding function
static char *rot13_decode(const char *input)
{
  int len = strlen(input);
  char *output = malloc(len + 1);
  if (output == NULL)
  {
    return NULL;
  }
  const char *input_ptr = input;
  char *output_ptr = output;
  while (*input_ptr)
  {
    char c = *input_ptr++;
    if (c >= 'a' && c <= 'z')
    {
      c = 'a' + ((c - 'a' + 13) % 26);
    }
    else if (c >= 'A' && c <= 'Z')
    {
      c = 'A' + ((c - 'A' + 13) % 26);
    }
    *output_ptr++ = c;
  }
  *output_ptr = '\0';
  return output;
}

static const unsigned char base64_table[256] = {
    ['A'] = 0, ['B'] = 1, ['C'] = 2, ['D'] = 3, ['E'] = 4, ['F'] = 5, ['G'] = 6, ['H'] = 7, ['I'] = 8, ['J'] = 9, ['K'] = 10, ['L'] = 11, ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15, ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19, ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23, ['Y'] = 24, ['Z'] = 25, ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29, ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33, ['i'] = 34, ['j'] = 35, ['k'] = 36, ['l'] = 37, ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41, ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45, ['u'] = 46, ['v'] = 47, ['w'] = 48, ['x'] = 49, ['y'] = 50, ['z'] = 51, ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55, ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59, ['8'] = 60, ['9'] = 61, ['+'] = 62, ['/'] = 63};

// Base64 decoding function
static char *base64_decode(const char *input)
{
  int len = strlen(input);
  if (len % 4 != 0)
  {
    return NULL;
  }

  int output_len = 3 * (len / 4);
  if (input[len - 1] == '=')
    output_len--;
  if (input[len - 2] == '=')
    output_len--;

  char *output = malloc(output_len + 1);
  if (output == NULL)
  {
    return NULL;
  }

  char *output_ptr = output;
  for (int i = 0; i < len; i += 4)
  {
    unsigned char values[4] = {
        base64_table[(unsigned char)input[i]],
        base64_table[(unsigned char)input[i + 1]],
        base64_table[(unsigned char)input[i + 2]],
        base64_table[(unsigned char)input[i + 3]]};

    *output_ptr++ = (values[0] << 2) | (values[1] >> 4);
    if (input[i + 2] != '=')
      *output_ptr++ = (values[1] << 4) | (values[2] >> 2);
    if (input[i + 3] != '=')
      *output_ptr++ = (values[2] << 6) | values[3];
  }

  *output_ptr = '\0';
  return output;
}

// Hex decoding function
static char *hex_decode(const char *input)
{
  int len = strlen(input);
  if (len % 2 != 0)
  {
    return NULL;
  }
  int output_len = len / 2;
  char *output = malloc(output_len + 1);
  if (output == NULL)
  {
    return NULL;
  }
  const char *input_ptr = input;
  char *output_ptr = output;
  while (*input_ptr && *(input_ptr + 1))
  {
    char hex[3] = {*input_ptr, *(input_ptr + 1), '\0'};
    int value = strtol(hex, NULL, 16);
    *output_ptr++ = (char)value;
    input_ptr += 2;
  }
  *output_ptr = '\0';
  return output;
}

// Function to output the list of songs
void output_song_list()
{
  // Open the playlist file for reading
  FILE *playlist = fopen("playlist.txt", "r");
  if (playlist == NULL)
  {
    printf("Error opening playlist file for reading\n");
    return;
  }

  // Close the file
  fclose(playlist);

  system("sort playlist.txt");
}

void add_song(char *song_name)
{
  // Open the playlist file for reading
  FILE *playlist = fopen("playlist.txt", "r");
  if (playlist == NULL)
  {
    printf("Error opening playlist file for reading\n");
    return;
  }

  // Check if song is already in the playlist
  char line[MAX_SONG_NAME_LENGTH];
  while (fgets(line, MAX_SONG_NAME_LENGTH, playlist) != NULL)
  {
    line[strcspn(line, "\n")] = '\0'; // Remove newline character
    if (strcasecmp(line, song_name) == 0)
    {
      printf("SONG ALREADY ON PLAYLIST\n");
      fclose(playlist);
      return;
    }
  }

  // Close the file
  fclose(playlist);

  // Open the playlist file for appending
  playlist = fopen("playlist.txt", "a");
  if (playlist == NULL)
  {
    printf("Error opening playlist file for appending\n");
    return;
  }

  // Write song to the playlistf
  fprintf(playlist, "%s\n", song_name);

  // Close the file
  fclose(playlist);
}

int main()
{
  const char *filename = "song-playlist.json";

  int msgid;
  key_t key = ftok("stream.c", 'A');
  msgid = msgget(key, 0666 | IPC_CREAT);
  if (msgid == -1)
  {
    perror("Error creating message queue");
    exit(1);
  }

  key_t sem_key = ftok("user.c", 'B');
  int sem_id = semget(sem_key, 1, IPC_CREAT | 0666);
  if (sem_id == -1)
  {
    perror("semget failed");
    exit(1);
  }

  // Initialize semaphore to 2 (maximum number of users)
  union semun
  {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
  } arg;
  arg.val = 2;
  if (semctl(sem_id, 0, SETVAL, arg) == -1)
  {
    perror("semctl failed");
    exit(1);
  }

  while (1)
  {
    // Receive user command from message queue
    struct message message;
    if (msgrcv(msgid, &message, sizeof(message), 0, 0) == -1)
    {
      perror("Error receiving message");
      exit(1);
    }

    // Wait for semaphore
    struct sembuf sb = {0, -1, 0};
    if (semop(sem_id, &sb, 1) == -1)
    {
      perror("semop failed");
      exit(1);
    }

    // Parse user command
    char *command = message.mtext;
    char *param = strchr(command, ' ');
    if (param)
    {
      *param = '\0';
      param++;
      while (isspace(*param))
      {
        param++;
      }
    }

    if (strcmp(command, "DECRYPT") == 0)
    {
      FILE *fp = fopen(filename, "r");
      if (fp == NULL)
      {
        printf("Error opening file: %s\n", filename);
        return 1;
      }
      fseek(fp, 0, SEEK_END);
      long file_size = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      char *json_str = malloc(file_size + 1);
      if (json_str == NULL)
      {
        printf("Error allocating memory\n");
        return 1;
      }
      fread(json_str, 1, file_size, fp);
      fclose(fp);
      json_str[file_size] = '\0';

      cJSON *root = cJSON_Parse(json_str);
      if (root == NULL)
      {
        printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        free(json_str);
        return 1;
      }

      FILE *playlist = fopen("playlist.txt", "w");
      if (playlist == NULL)
      {
        printf("Error opening playlist file for writing\n");
        cJSON_Delete(root);
        free(json_str);
        return 1;
      }

      cJSON *item = NULL;
      cJSON_ArrayForEach(item, root)
      {
        cJSON *method = cJSON_GetObjectItemCaseSensitive(item, "method");
        cJSON *song = cJSON_GetObjectItemCaseSensitive(item, "song");

        if (strcmp(method->valuestring, "base64") == 0)
        {
          char *decoded = base64_decode(song->valuestring);
          if (decoded == NULL)
          {
            printf("Error decoding base64: %s\n", song->valuestring);
            continue;
          }
          printf("Base64 decoded: %s\n", decoded);
          fprintf(playlist, "%s\n", decoded);
          free(decoded);
        }
        else if (strcmp(method->valuestring, "hex") == 0)
        {
          char *decoded = hex_decode(song->valuestring);
          if (decoded == NULL)
          {
            printf("Error decoding hex: %s\n", song->valuestring);
            continue;
          }
          printf("Hex decoded: %s\n", decoded);
          fprintf(playlist, "%s\n", decoded);
          free(decoded);
        }
        else if (strcmp(method->valuestring, "rot13") == 0)
        {
          char *decoded = rot13_decode(song->valuestring);
          if (decoded == NULL)
          {
            printf("Error decoding rot13: %s\n", song->valuestring);
            continue;
          }
          printf("Rot13 decoded: %s\n", decoded);
          fprintf(playlist, "%s\n", decoded);
          free(decoded);
        }
        else
        {
          printf("Unknown method: %s\n", method->valuestring);
          continue;
        }
      }

      fclose(playlist);
      cJSON_Delete(root);
      free(json_str);
    }

    else if (strcmp(command, "LIST") == 0)
    {
      output_song_list();
    }

    else if (strcmp(command, "PLAY") == 0)
    {
      // Search for songs containing the query
      char *query = param;
      int num_matches = 0;
      char matches[MAX_SONGS][MAX_SONG_NAME_LENGTH];
      FILE *playlist = fopen("playlist.txt", "r");
      if (playlist == NULL)
      {
        printf("Error opening playlist file for reading\n");
        return 1;
      }
      char song_name[MAX_SONG_NAME_LENGTH];
      while (fgets(song_name, MAX_SONG_NAME_LENGTH, playlist) != NULL)
      {
        // Remove trailing newline if present
        song_name[strcspn(song_name, "\n")] = '\0';

        // Check if song name contains the query
        char *found = strcasestr(song_name, query);
        if (found != NULL)
        {
          strcpy(matches[num_matches], song_name);
          num_matches++;
        }
      }
      fclose(playlist);

      // Print search results
      if (num_matches == 0)
      {
        printf("THERE IS NO SONG CONTAINING \"%s\"\n", query);
      }
      else if (num_matches == 1)
      {
        // Play the first matching song
        printf("USER <%ld> PLAYING \"%s\"\n", message.mtype, matches[0]);
      }
      else
      {
        printf("THERE ARE \"%d\" SONG(S) CONTAINING \"%s\":\n", num_matches, query);
        for (int i = 0; i < num_matches; i++)
        {
          printf("%d. %s\n", i + 1, matches[i]);
        }
      }
    }
    else if (strcmp(command, "ADD") == 0)
    {
      add_song(param);
      printf("USER <%ld> ADD %s\n", message.mtype, param);
    }

    // printf("Received message from User %ld: %s\n", message.mtype, message.mtext);

    // Signal semaphore
    sb.sem_op = 1;
    if (semop(sem_id, &sb, 1) == -1)
    {
      perror("semop failed");
      exit(1);
    }
  }
  return 0;
}
