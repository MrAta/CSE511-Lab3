/**
 * @file journal.c
 * 
 * Function implementations for journaling.
 */

#include "journal.h"
#include "server-part1.h"

int log_transaction(transaction *tx) {
  pthread_mutex_lock(&journal_mutex);
  FILE *file = fopen("tx_log", "a");
  struct stat st;
  size_t data_len = 0;

  if (file == NULL) {
    perror("Could not open tx_log.");
    return -1;
  }

  tx->txb.txid = rand();
  if (!fwrite(&(tx->txb.txid), 1, sizeof(int), file)) {// issue begin
    perror("Could not write TxB: ");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }
  if (fflush(file) != 0) {
    stat("tx_log", &st);
    ftruncate(fileno(file), st.st_size - sizeof(int));
    perror("Could not flush TxB.");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }

  data_len = tx->db.data_len; // length prefix for easy unmarshalling
  if (!fwrite(&(tx->db.data_len), 1, sizeof(size_t), file) || // TODOD:FIXIXXXXXX al these truncs
      !fwrite(tx->db.data, 1, data_len, file) || 
      !fwrite("\0", 1, 1, file)) { // issue data
    stat("tx_log", &st);
    ftruncate(fileno(file), st.st_size - sizeof(int));
    perror("Could not write db.data: ");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }
  if (fflush(file) != 0) {
    stat("tx_log", &st);
    ftruncate(fileno(file), st.st_size - sizeof(int) - data_len);
    perror("Could not flush data.");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }

  tx->valid = 1; // mark transaction as valid
  if (!fwrite(&( tx->valid ), 1, sizeof(int), file)) {
    stat("tx_log", &st);
    ftruncate(fileno(file), st.st_size - sizeof(int) - data_len);
    perror("Could not write valid bit: ");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  } // issue valid entry indicator
  if (fflush(file) != 0) {
    stat("tx_log", &st);
    ftruncate(fileno(file), st.st_size - sizeof(int) - data_len - sizeof(int));
    perror("Could not flush 'valid'.");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }

  tx->txe.committed = 1; // other writes good, mark committed flag
  if (!fwrite(&( tx->txe.committed ), 1, sizeof(int), file) ||
      !fwrite("\n", 1, 1, file)) { // issue commit, entry delimiter
    stat("tx_log", &st);
    ftruncate(fileno(file), st.st_size - sizeof(int) - data_len - sizeof(int));
    perror("Could not flush 'committed': ");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }
  if (fflush(file) != 0) {
    stat("tx_log", &st);
    ftruncate(fileno(file), st.st_size - sizeof(int) - data_len - sizeof(int) -
                            sizeof(int) - 1);
    perror("Could not flush 'TxE': ");
    fclose(file);
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }

  // log entry successfully written

  fclose(file);

  pthread_mutex_unlock(&journal_mutex);

  // return tx->txb.txid;
  return 0; // this should be fine if not using txid
}

int remove_transaction(int txid) {
  pthread_mutex_lock(&journal_mutex);

  char *tmp_entry = (char *) calloc(MAX_JOURNAL_ENTRY_SIZE, sizeof(char));
  transaction *tmp_transaction = (transaction *) calloc(sizeof(transaction), sizeof(char));
  int temp_txid = 0;
  int offset = 0;

  FILE *file = fopen("tx_log", "r+");

  if (file == NULL) {
    perror("Could not open tx_log.");
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }

  while (1) {
    if (fgets(tmp_entry, MAX_JOURNAL_ENTRY_SIZE, file) != NULL) { // read to newline (including newline char, and advances file position to next entry?)
      unmarshall_journal_entry(tmp_transaction, tmp_entry);
      // tmp_transaction = (transaction *) tmp_entry; // TODO: Possible problem location -- done
      temp_txid = tmp_transaction->txb.txid;
      if (temp_txid == txid) { // found transaction in log
        if (!tmp_transaction->valid) { // if already marked as invalid, ignore
          free(tmp_entry);
          tmp_entry = NULL;
          tmp_transaction = NULL;
          fclose(file);
          pthread_mutex_unlock(&journal_mutex);
          return 0;
        }

        // otherwise, seek back and mark invalid
        // TODO: Possible problem location -- done
        offset = (sizeof(int) + sizeof(int) + 1) * -1; // seek back to position to overwrite 4 byte 'valid' variable
        fseek(file, offset, SEEK_CUR);
        fwrite(0, 1, sizeof(int), file); // mark transaction as invalid (subsequent recovers/removes should ignore it)
        if (fflush(file) != 0) {
          perror("Could not remove transaction.");
          free(tmp_entry);
          tmp_entry = NULL;
          tmp_transaction = NULL;
          fclose(file);
          pthread_mutex_unlock(&journal_mutex);
          return -1;
        }
        free(tmp_entry);
        tmp_entry = NULL;
        tmp_transaction = NULL;
        // rewind(file); // dont need
        fclose(file);
        pthread_mutex_unlock(&journal_mutex);
        return 0;
      }
    } else { // end of file
      break;
    }
  }

  free(tmp_entry);
  tmp_entry = NULL;
  tmp_transaction = NULL;
  fclose(file);

  pthread_mutex_unlock(&journal_mutex);

  return -1; // tx not found
}

int flush_log() {
  int rc;
  pthread_mutex_lock(&journal_mutex);
  rc = remove("tx_log");
  pthread_mutex_unlock(&journal_mutex);
  return rc;
}

int recover() {
  pthread_mutex_lock(&journal_mutex);

  char *tmp_entry = (char *) calloc(MAX_JOURNAL_ENTRY_SIZE, sizeof(char));
  transaction *tmp_transaction = (transaction *) calloc(sizeof(transaction), sizeof(char));
  char *key = NULL;
  char *value = NULL;
  char *request_type;
  char *response = NULL;
  char *save_ptr;
  int response_size;

  FILE *file = fopen("tx_log", "r");

  if (file == NULL) {
    perror("Could not open tx_log.");
    pthread_mutex_unlock(&journal_mutex);
    return -1;
  }

  while (1) { // should we be re-journaling while replaying journal? cant read/write to tx_log at the same time though
    if (fgets(tmp_entry, MAX_JOURNAL_ENTRY_SIZE, file) != NULL) {
      unmarshall_journal_entry(tmp_transaction, tmp_entry);

      if (!tmp_transaction->valid) {
        continue; // ignore
      }

      request_type = strtok_r(tmp_transaction->db.data, " ", &save_ptr);
      key = strtok_r(NULL, " ", &save_ptr);
      value = strtok_r(NULL, " ", &save_ptr);
      if (request_type == NULL || key == NULL) {
        printf("Invalid key/command received\n");
      } else if (strncmp(request_type, "GET", 3) == 0) {
        server_1_get_request(key, &response, &response_size);
      } else if (strncmp(request_type, "PUT", 3) == 0) {
        server_1_put_request(key, value, &response, &response_size);
      } else if (strncmp(request_type, "INSERT", 6) == 0) {
        server_1_insert_request(key, value, &response, &response_size);
      } else if (strncmp(request_type, "DELETE", 6) == 0) {
        server_1_delete_request(key, &response, &response_size);
      } else { // *** incorrectly formatted journal entry(ies), ignore
        continue;
      }

      if (response != NULL) {
        free(response);
      }

      response = NULL;

    } else { // end of file
      break;
    }
  }

  free(tmp_entry);
  fclose(file);

  remove("tx_log"); // flush_log but already have lock

  pthread_mutex_unlock(&journal_mutex);

  return 0;
}

int unmarshall_journal_entry(transaction *tmp_transaction, char *tmp_entry) {
  char *entry_pos = tmp_entry;

  memcpy(&(tmp_transaction->txb.txid), entry_pos, sizeof(int)); // txid
  entry_pos += sizeof(int);

  memcpy(&(tmp_transaction->db.data_len), entry_pos, sizeof(size_t)); // length prefix
  entry_pos += sizeof(size_t);

  memcpy(tmp_transaction->db.data, entry_pos, tmp_transaction->db.data_len + 1); // data and null byte
  entry_pos += tmp_transaction->db.data_len + 1;

  memcpy(&(tmp_transaction->valid), entry_pos, sizeof(int)); // valid
  entry_pos += sizeof(int);

  memcpy(&(tmp_transaction->txe.committed), entry_pos, sizeof(int)); // committed
  entry_pos += sizeof(int);

  // dont need newline char

  return 0;
}
