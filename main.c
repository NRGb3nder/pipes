#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils.h"

#define WORKERS_COUNT 3
#define PROCESS_COUNT (WORKERS_COUNT + 1)
#define PIPE_READ_END 0
#define PIPE_WRITE_END 1
#define PIPE_CLOSED_END -1
#define CHAR_BUF_SIZE 256
#define TIMEOUT_SECS 5
#define MSG_POLL "::POLL::"
#define MSG_REQUEST "::REQUEST::"
#define MSG_CONFIRM "::CONFIRMATION::"
#define MSG_DIE "::DIE::"

struct msgqueue_node_t
{
    char *msg;
    struct msgqueue_node_t *next;
};

int start_interaction();
int run_worker(int process_number, int pipes[][PROCESS_COUNT][2]);
int run_controller(int pipes[][PROCESS_COUNT][2]);
int adjust_pipes(int process_number, int pipes[][PROCESS_COUNT][2]);
int fill_sets(fd_set *rfds, fd_set *wfds, int process_number, int pipes[][PROCESS_COUNT][2],
    struct msgqueue_node_t *receivers[PROCESS_COUNT]);
void msgqueue_queue(struct msgqueue_node_t *head, char *msg);
bool msgqueue_has_request(struct msgqueue_node_t *head);
char *msgqueue_next(struct msgqueue_node_t *head);

char *module;

int main(int argc, char *argv[])
{
    module = argv[0];

    return start_interaction();
}

int start_interaction()
{
    int process_number = 0;

    int pipes[PROCESS_COUNT][PROCESS_COUNT][2];
    for (int i = 0; i < PROCESS_COUNT; i++) {
        for (int j = 0; j < PROCESS_COUNT; j++) {
            if (i != j) {
                if (pipe(pipes[i][j]) == -1) {
                    printerr(module, strerror(errno), "pipe creation");
                    return 1;
                }
            }
        }
    }

    for (int i = 0; i < WORKERS_COUNT; i++) {
        process_number++;
        pid_t child;
        if (child = fork(), child == -1) {
            printerr(module, strerror(errno), "fork");
            return 1;
        }

        if (!child) {
            return run_worker(process_number, pipes);
        }
    }

    int result = run_controller(pipes);

    while (wait(NULL) != -1) {
        /* block the process */
    }

    printf("\nDone.\n");

    return result;
}

int run_worker(int process_number, int pipes[][PROCESS_COUNT][2])
{
    if (adjust_pipes(process_number, pipes) == -1) {
        return 1;
    }

    struct msgqueue_node_t *receivers[PROCESS_COUNT];
    for (int i = 0; i < PROCESS_COUNT; i++) {
        if (i != process_number) {
            receivers[i] = malloc(sizeof(struct msgqueue_node_t));
            receivers[i]->next = NULL;
        }
    }

    bool is_error = false;
    bool is_done = false;
    fd_set rfds;
    fd_set wfds;
    struct timeval tv = {.tv_sec = TIMEOUT_SECS, .tv_usec = 0};
    int max_fd_value;
    int confirmations_count = 0;
    while (!is_error && !is_done) {
        max_fd_value = fill_sets(&rfds, &wfds, process_number, pipes, receivers);

        int ret;
        if (ret = select(max_fd_value + 1, &rfds, &wfds, NULL, &tv), ret == -1) {
            printerr(module, strerror(errno), "select");
            is_error = true;
        } else if (!ret) {
            printerr(module, "Worker timeout expired", NULL);
            is_error = true;
        } else {
            for (int i = 0; i < PROCESS_COUNT && !is_error; i++) {
                if (i != process_number) {
                    if (FD_ISSET(pipes[i][process_number][PIPE_READ_END], &rfds)) {
                        char message[CHAR_BUF_SIZE];
                        if (read(pipes[i][process_number][PIPE_READ_END], message, CHAR_BUF_SIZE) == -1) {
                            printerr(module, strerror(errno), "reading from pipe");
                            is_error = true;
                        } else {
                            report_msg_action(false, process_number, true, message);
                            if (!strcmp(message, MSG_CONFIRM)) {
                                confirmations_count++;
                            } else if (!strcmp(message, MSG_REQUEST)) {
                                msgqueue_queue(receivers[i], MSG_CONFIRM);
                            } else if (!strcmp(message, MSG_POLL)) {
                                for (int recvr = 0; recvr < PROCESS_COUNT; recvr++) {
                                    if (recvr != process_number && recvr != 0) {
                                        msgqueue_queue(receivers[recvr], MSG_REQUEST);
                                    }
                                }
                            } else if (!strcmp(message, MSG_DIE)) {
                                is_done = true;
                            } else {
                                printerr(module, "Unidentified message received by worker", NULL);
                                is_error = true;
                            }
                        }
                    }
                    if (FD_ISSET(pipes[process_number][i][PIPE_WRITE_END], &wfds)) {
                        char *message = msgqueue_next(receivers[i]);
                        if (write(pipes[process_number][i][PIPE_WRITE_END], message, strlen(message) + 1) == -1) {
                            printerr(module, strerror(errno), "writing to pipe");
                            is_error = true;
                        } else {
                            report_msg_action(false, process_number, false, message);
                        }
                    }
                }
            }

            if (confirmations_count == WORKERS_COUNT - 1) {
                msgqueue_queue(receivers[0], MSG_CONFIRM);
                confirmations_count = 0;
            }
        }
    }

    for (int i = 0; i < PROCESS_COUNT; i++) {
        if (i != process_number) {
            free(receivers[i]);
        }
    }

    return is_error;
}

int run_controller(int pipes[][PROCESS_COUNT][2])
{
    if (adjust_pipes(0, pipes) == -1) {
        return 1;
    }

    for (int i = 1; i < PROCESS_COUNT; i++) {
        if (write(pipes[0][i][PIPE_WRITE_END], MSG_POLL, strlen(MSG_POLL) + 1) == -1) {
            printerr(module, strerror(errno), "writing to pipe");
            return 1;
        }

        report_msg_action(true, 0, false, MSG_POLL);

        char buf[CHAR_BUF_SIZE];
        if (read(pipes[i][0][PIPE_READ_END], buf, CHAR_BUF_SIZE) == -1) {
            printerr(module, strerror(errno), "reading from pipe");
            return 1;
        }

        if (strcmp(buf, MSG_CONFIRM)) {
            printerr(module, "Incorrect worker answer", NULL);
            return 1;
        } else {
            report_msg_action(true, 0, true, MSG_CONFIRM);
            report_poll_success(i);
        }
    }

    for (int i = 1; i < PROCESS_COUNT; i++) {
        if (write(pipes[0][i][PIPE_WRITE_END], MSG_DIE, strlen(MSG_DIE) + 1) == -1) {
            printerr(module, strerror(errno), "writing to pipe");
            return 1;
        }
        report_msg_action(true, 0, false, MSG_DIE);
    }

    return 0;
}

int adjust_pipes(int process_number, int pipes[][PROCESS_COUNT][2])
{
    for (int i = 0; i < PROCESS_COUNT; i++) {
        for (int j = 0; j < PROCESS_COUNT; j++) {
            if (i != j) {
                if (i != process_number) {
                    if (close(pipes[i][j][PIPE_WRITE_END]) == -1) {
                        printerr(module, strerror(errno), "closing write end of pipe");
                        return -1;
                    }
                    pipes[i][j][PIPE_WRITE_END] = PIPE_CLOSED_END;
                }
                if (j != process_number) {
                    if (close(pipes[i][j][PIPE_READ_END]) == -1) {
                        printerr(module, strerror(errno), "closing read end of pipe");
                        return -1;
                    }
                    pipes[i][j][PIPE_READ_END] = PIPE_CLOSED_END;
                }
            }
        }
    }

    return 0;
}

int fill_sets(fd_set *rfds, fd_set *wfds, int process_number, int pipes[][PROCESS_COUNT][2],
    struct msgqueue_node_t *receivers[PROCESS_COUNT])
{
    int max_fd_value = -1;

    FD_ZERO(rfds);
    FD_ZERO(wfds);
    for (int i = 0; i < PROCESS_COUNT; i++) {
        if (i != process_number) {
            if (pipes[i][process_number][PIPE_READ_END] != PIPE_CLOSED_END) {
                if (pipes[i][process_number][PIPE_READ_END] > max_fd_value) {
                    max_fd_value = pipes[i][process_number][PIPE_READ_END];
                }
                FD_SET(pipes[i][process_number][PIPE_READ_END], rfds);
            }
            if (msgqueue_has_request(receivers[i])) {
                if (pipes[process_number][i][PIPE_WRITE_END] > max_fd_value) {
                    max_fd_value = pipes[process_number][i][PIPE_WRITE_END];
                }
                FD_SET(pipes[process_number][i][PIPE_WRITE_END], wfds);
            }
        }
    }

    return max_fd_value;
}

void msgqueue_queue(struct msgqueue_node_t *head, char *msg)
{
    struct msgqueue_node_t *current_node = head;

    while (current_node->next) {
        current_node = current_node->next;
    }

    current_node->next = malloc(sizeof(struct msgqueue_node_t));
    current_node = current_node->next;
    current_node->msg = msg;
    current_node->next = NULL;
}

bool msgqueue_has_request(struct msgqueue_node_t *head)
{
    return head->next ? true : false;
}

char *msgqueue_next(struct msgqueue_node_t *head)
{
    char *result = head->next->msg;
    struct msgqueue_node_t *new_next = head->next->next;

    free(head->next);

    head->next = new_next;

    return result;
}
