#include "bulba/handlers/cls_handler.h"
#include <stdio.h>
void BLB_CLS(void){printf("\033[2J\033[H");fflush(stdout);}
