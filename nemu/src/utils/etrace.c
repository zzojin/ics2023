#include <common.h>
#include "utils.h"

void trace_exception(word_t NO, vaddr_t epc) {	
    log_write("[etrace]: trigger exception, mcause is %u at 0x%x", NO, epc);
}
