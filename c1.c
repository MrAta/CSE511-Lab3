//
// Created by sps5394 on 11/13/18.
//

#ifndef __linux__

#include <zconf.h>
#include <memory.h>

#endif

#include "c1.h"

int file_counter = -1;

c1_metadata *metadata;

char *charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int c1_batch_insert(c0_node *nodes[], int size) {
  printf("Called with: filecounter: %d\n", file_counter);
  //
  //
  // LOCAL VARIABLES
  //
  //
  int fd, oldfd = 0, rc;
  char *filename;
  char *keyval;
  asprintf(&filename, "%s/%d", DB_DIR, ++file_counter);
  printf("Start: filecounter: %d\n", file_counter);
  // printf("C1: %s\n", filename);
  fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRWXG);

  if (fd == -1) {
    free(filename);
    perror("Failed to open new file: ");
    return -1;
  }
  printf("Created %s with %d\n", filename, file_counter);
  free(filename);
  filename = NULL;
  for (int i = 0; i < size; i++) {
    keyval = calloc(LINE_SIZE, 1);

    if (( rc = snprintf(keyval, (int) LINE_SIZE, "%s %s %d\n", nodes[i]->key,
                        nodes[i]->value, nodes[i]->flag)) == -1) {
      close(fd);
      fprintf(stderr, "Failed to create db entry\n");
      return -1;
    }
//    bzero(keyval + rc + 1, LINE_SIZE - rc);
    if (write(fd, keyval, LINE_SIZE) == -1) {
      free(keyval);
      close(fd);
      return -1;
    }
    free(keyval);
    keyval = NULL;
  }

  // Trigger a merge
  if (file_counter > 0) {
    // Merge with existing file
    printf("Openning old file with %d\n", file_counter - 1);
    asprintf(&filename, "%s/%d", DB_DIR, file_counter - 1);

    if (( oldfd = open(filename, O_RDONLY, S_IWUSR | S_IRUSR | S_IRWXG)) == -1) {
      perror("Failed to open old file: ");
      free(filename);
      close(fd);
      return -1;
    }
    printf("Opened old file: %s with %d\n", filename, file_counter - 1);
    free(filename);
    filename = NULL;
    FILE *file1 = fdopen(fd, "r");
    if (file1 == NULL) {
      close(fd);
      close(oldfd);
      return -1;
    }
    rewind(file1);
    FILE *file2 = fdopen(oldfd, "r");
    if (file2 == NULL) {
      close(fd);
      close(oldfd);
      fclose(file1);
      return -1;
    }
    rewind(file2);
    if (merge(file1, file2) == -1) {
      close(fd);
      close(oldfd);
      return -1;
    }
  }
  printf("ATA: Trying 1 with filecounter: %d\n", file_counter);
  asprintf(&filename, "%s/%d", DB_DIR, file_counter);
  printf("ATA: Tried 1 with filecounter: %d\n", file_counter);
  FILE *currfile = fopen(filename, "r");
  printf("ATA: Trying 2 with filecounter: %d\n", file_counter);
  if (currfile == NULL) {
    printf("ATA: RIP %s\n", filename);
    free(filename);
    perror("RIPcurrfile");
    fclose(currfile);
    return -1;
  }
  free(filename);
  printf("ATA: Tried 2 with filecounter: %d\n", file_counter);
  // Complete
  printf("Finished: filecounter: %d\n", file_counter);
  printf("Post-Finish: filecounter: %d\n", file_counter);
  // Update SSTable
  update_sstable(currfile, metadata);
  fclose(currfile);
  close(fd);
  if (oldfd != 0) {
    close(oldfd);
  }
  return 0;
}

int merge(FILE *file1, FILE *file2) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  char *filename;
  char *line1 = NULL, *line2 = NULL, *key1, *key2, *val1, *val2;
  int mergefd, valid1, valid2;
  ssize_t size1 = 0, size2 = 0;

  if (file1 == NULL || file2 == NULL) return -1;

  asprintf(&filename, "%s/%d", DB_DIR, ++file_counter);
  if (( mergefd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRWXG)) == -1) {
    perror("Failed to open a new merge file: ");
    free(filename);
    return -1;
  }

  while (1) {
    if (line1 == NULL) {
      line1 = calloc(LINE_SIZE, 1);
      size1 = fread(line1, LINE_SIZE, 1, file1);
      if (size1 == 0) {
        break;
      }
    }
    if (line2 == NULL) {
      line2 = calloc(LINE_SIZE, 1);
      size2 = fread(line2, LINE_SIZE, 1, file2);
      if (size2 == 0) {
        break;
      }
    }
    sscanf(line1, "%ms %ms %d", &key1, &val1, &valid1);
    sscanf(line2, "%ms %ms %d", &key2, &val2, &valid2);
    // Compare
    if (strcmp(key1, key2) < 0) {
      if (!valid1) {
        if (write(mergefd, line1, LINE_SIZE) == -1) {
          perror("Failed to write line: ");
          return -1;
        }
      }
      free(line1);
      line1 = NULL;
    } else if (strcmp(key1, key2) > 0) {
      if (!valid2) {
        if (write(mergefd, line2, LINE_SIZE) == -1) {
          perror("Failed to write line: ");
          return -1;
        }
      }
      free(line2);
      line2 = NULL;
    } else {
      // Equal
      if (!valid2) {
        if (write(mergefd, line2, LINE_SIZE) == -1) {
          perror("Failed to write line: ");
          return -1;
        }
      }
      free(line2);
      line2 = NULL;
      free(line1);
      line1 = NULL;
    }
    free(key1);
    free(key2);
    free(val1);
    free(val2);
  }

  if (size2 == 0) { // File 2 ran out of data. Copy rest of file 1.
    // Guaranteed that line1 is allocated.
    assert(line1 != NULL);
    if (write(mergefd, line1, LINE_SIZE) == -1) {
      perror("Failed to write line: ");
      return -1;
    }
    free(line1);
    line1 = NULL;
    line1 = calloc(LINE_SIZE, 1);
    // Read rest of file1
    while (( size1 = fread(line1, LINE_SIZE, 1, file1)) != 0) {
      if (write(mergefd, line1, LINE_SIZE) == -1) {
        perror("Failed to write line: ");
        return -1;
      }
      free(line1);
      line1 = calloc(LINE_SIZE, 1);
    }
  }

  if (size1 == 0) { // File 1 complete. Copy rest from File 2
    if (line2 != NULL) { // Last key read from file 2 still not added
      if (write(mergefd, line2, LINE_SIZE) == -1) {
        perror("Failed to write line: ");
        return -1;
      }
      free(line2);
      line2 = NULL;
    }
    line2 = calloc(LINE_SIZE, 1);
    while (fread(line2, LINE_SIZE, 1, file2) != 0) {
      if (write(mergefd, line2, LINE_SIZE) == -1) {
        perror("Failed to write line: ");
        return -1;
      }
      free(line2);
      line2 = NULL;
      line2 = calloc(LINE_SIZE, 1);
    }
  }
  if (line1 != NULL) {
    free(line1);
  }
  if (line2 != NULL) {
    free(line2);
  }
  fclose(file1);
  fclose(file2);
  close(mergefd);
  return 0;
}

char *c1_get(char *key) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  FILE *file;
  size_t size;
  int numlines = 0;
  char *fname, *val;

  asprintf(&fname, "%s/%d", DB_DIR, file_counter);
  file = fopen(fname, "r");
  free(fname);
  if (file == NULL) {
    fprintf(stderr, "Failed to open current counter file\n");
    printf("errno: %s\n", strerror(errno));
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  size = (size_t) ftell(file);
  numlines = (int) ( size / LINE_SIZE);

  // Lookup start and end in SSTable
  int startline = (int) ( metadata->ssindex[char_to_idx(key[0])] / LINE_SIZE);
  if (metadata->ssindex[char_to_idx(key[0])] == -1) {
    startline = 0;
  }
  int endline = (int) ( metadata->ssindex[char_to_idx(key[0]) + 1] / LINE_SIZE);
  if (metadata->ssindex[char_to_idx(key[0]) + 1] == -1) {
    endline = numlines;
  }
  if (endline > numlines){
    endline = numlines;
  }
  val = c1_search(file, key, startline, endline - 1);
  fclose(file);
  return val;
}

int marshall_metadata(c1_metadata *md, char **buffer) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  int seek = 0;
  *buffer = malloc(sizeof(c1_metadata));

  memcpy(( *buffer ) + seek, &( md->counter_value ), sizeof(int));
  seek += sizeof(int);
  memcpy(( *buffer ) + seek, md->ssindex, sizeof(md->ssindex));
  seek += sizeof(md->ssindex);
  return seek;
}

int unmarshall_metadata(c1_metadata *md, char *buffer) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  int seek = 0;
  memcpy(&( md->counter_value ), buffer, sizeof(int));
  seek += sizeof(int);
  memcpy(md->ssindex, buffer + seek, sizeof(md->ssindex));
  seek += sizeof(md->ssindex);
  return seek;
}

int dump_metadata(c1_metadata *md) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  FILE *md_file;
  char filename[strlen(DB_DIR) + strlen(SSTABLE) + 3];
  char *buf = NULL;
  int size;

  snprintf(filename, strlen(DB_DIR) + strlen(SSTABLE) + 3, "%s/%s", DB_DIR, SSTABLE);
  md_file = fopen(filename, "w");
  size = marshall_metadata(md, &buf);
  fwrite(buf, size, 1, md_file);
  free(buf);
  fclose(md_file);
  return 0;
}

int load_metadata(c1_metadata **md) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  FILE *md_file;
  char filename[15];
  char *buf = NULL;

  *md = calloc(1, sizeof(c1_metadata));
  buf = calloc(1, sizeof(c1_metadata));
  snprintf(filename, 15, "%s/%s", DB_DIR, SSTABLE);
  md_file = fopen(filename, "r");
  if (md_file == NULL) {
    return 0;
  }
  if (fread(buf, sizeof(( *md )->ssindex) + sizeof(int), 1, md_file) == 0) {
    // First run ever
    free(buf);
    return 0;
  }

  unmarshall_metadata(*md, buf);
  file_counter = (*md)->counter_value;
  free(buf);
  return 0;
}

char *c1_search(FILE *file, char *key, int startline, int endline) {
  char *line, *r_key, *val;
  size_t len;
  int valid;

  if (endline >= startline) {
    int mid = (( endline - startline ) / 2 ) + startline;
    fseek(file, mid * LINE_SIZE, SEEK_SET);
    line = calloc(LINE_SIZE, 1);
    len = fread(line, LINE_SIZE, 1, file);
    sscanf(line, "%ms %ms %d", &r_key, &val, &valid);
    free(line);
    if (strcmp(r_key, key) == 0) {
      free(r_key);
      return val;
    } else if (strcmp(r_key, key) > 0) {
      free(r_key);
      free(val);
      return c1_search(file, key, startline, mid - 1);
    } else {
      free(r_key);
      free(val);
      return c1_search(file, key, mid + 1, endline);
    }
  } else {
    return NULL;
  }
}

void c1_init() {
  //
  //
  // LOCAL VARIABLES
  //
  //
  load_metadata(&metadata);
}

int update_sstable(FILE *dfile, c1_metadata *md) {
  //
  //
  // LOCAL VARIABLES
  //
  //
  char *line;
  int boolmask[INDEX_SIZE] = { 0 };
  int idx;
  long offset;
  size_t bytes_read;

  line = calloc(LINE_SIZE, 1);
  while (( bytes_read = fread(line, LINE_SIZE, 1, dfile)) != 0) {
    idx = char_to_idx(line[0]);
    if (!boolmask[idx]) { // First occurrence of the given character
      offset = ftell(dfile) - ( bytes_read * LINE_SIZE);
      md->ssindex[idx] = offset;
      boolmask[idx] = 1;
    }
    free(line);
    line = calloc(LINE_SIZE, 1);
  }
  for (int i = 0; i < INDEX_SIZE; i++) {
    if (md->ssindex[i] == 0) {
      md->ssindex[i] = -1;
    }
  }
  free(line);
  md->counter_value = file_counter;
  dump_metadata(metadata);
  return 0;
}

int char_to_idx(char c) {
  char *ptr = strchr(charset, c);
  if (ptr) {
    return (int) ( ptr - charset );
  }
  return -1;
}
