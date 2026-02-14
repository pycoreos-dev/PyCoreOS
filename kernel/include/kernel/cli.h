#ifndef KERNEL_CLI_H
#define KERNEL_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cli_action {
    CLI_ACTION_NONE = 0,
    CLI_ACTION_LAUNCH_DOOM = 1,
    CLI_ACTION_RESTART = 2,
    CLI_ACTION_SHUTDOWN = 3,
} cli_action;

void cli_init(void);
cli_action cli_execute(const char* line);

#ifdef __cplusplus
}
#endif

#endif
