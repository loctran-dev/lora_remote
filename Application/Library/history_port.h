/**
 * @file    history_port.h
 * @brief   Flash geometry of the history journal on the remote.
 * @ingroup history
 */
#ifndef HISTORY_PORT_H
#define HISTORY_PORT_H
#define HJ_REMOTE 1               ///< Build the journal for the remote (larger header).
#define HJ_BASE 0x0803A000u       ///< Start address of the journal in flash.
#define HJ_SIZE 0x4000u           ///< Journal size [bytes].
#define HJ_PAGE_SIZE 2048u        ///< Flash page size [bytes].
#endif
