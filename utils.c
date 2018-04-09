#include "utils.h"

void printerr(const char *module, const char *errmsg, const char *comment)
{
    fprintf(stderr, "%s: %s ", module, errmsg);
    if (comment) {
        fprintf(stderr, "(%s)", comment);
    }
    fprintf(stderr, "\n");
}

void report_msg_action (bool is_controller, int process_number, bool has_received, char *msg)
{
    char *actor = is_controller ? "CONTROLLER" : "WORKER";
    char *performed_operation = has_received ? "received" : "sent";

    printf("%s (%d): has %s \"%s\"\n", actor, process_number, performed_operation, msg);
}
void report_poll_success(int poll_number)
{
    printf("Poll #%d has been successfully accomplished\n", poll_number);
}