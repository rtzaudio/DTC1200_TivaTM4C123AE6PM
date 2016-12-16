/*
 * TransportTask.h
 *
 */

#ifndef DTC1200_TIVATM4C123AE6PMI_TRANSPORTTASK_H_
#define DTC1200_TIVATM4C123AE6PMI_TRANSPORTTASK_H_

/* Transport Message Command Structure */
typedef struct _CMDMSG {
    uint8_t cmd;	/* command code   */
    uint8_t op;		/* operation code */
} CMDMSG;

/* Transport Control Command Codes */
#define CMD_TRANSPORT_MODE		1	/* set the current transport mode */
#define CMD_PUNCH				2	/* op=1 punch-in, op=0 punch out */
#define CMD_TOGGLE_LIFTER		3	/* toggle tape lifter state */

/* Transport Controller Function Prototypes */

Void TransportCommandTask(UArg a0, UArg a1);
void TransportControllerTask(UArg a0, UArg a1);

void QueueTransportCommand(uint8_t cmd, uint8_t op);

#endif /* DTC1200_TIVATM4C123AE6PMI_TRANSPORTTASK_H_ */
