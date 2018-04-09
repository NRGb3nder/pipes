#ifndef PIPES_UTILS_H
#define PIPES_UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

void printerr(const char *module, const char *errmsg, const char *comment);
void report_msg_action(bool is_controller, int process_number, bool has_received, char *msg);
void report_poll_success(int poll_number);

#endif //PIPES_UTILS_H
