#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define _UTHREAD_PRIVATE
#include "context.h"
#include "palloc.h"
#include "preempt.h"
#include "queue.h"
#include "tls.h"
#include "uthread.h"

struct uthread_tcb {
	/* TODO: PART 1 - Phase 2 */
	/* TODO: PART 2 - Phase 3 */
};

void uthread_yield(void)
{
	/* TODO: PART 1 - Phase 2 */
}

int uthread_create(uthread_func_t func, void *arg)
{
	/* TODO: PART 1 - Phase 2 */
}

void uthread_exit(void)
{
	/* TODO: PART 1 - Phase 2 */
}

void uthread_block(void)
{
	/* TODO: PART 1 - Phase 2 */
}

void uthread_unblock(struct uthread_tcb *uthread)
{
	/* TODO: PART 1 - Phase 2 */
}

void *uthread_get_tls(void)
{
	/* TODO: PART 2 - Phase 3 */
}

void uthread_set_tls(void *tls)
{
	/* TODO: PART 2 - Phase 3 */
}

struct uthread_tcb *uthread_current(void)
{
	/* TODO: PART 1 - Phase 2 */
}

void uthread_start(uthread_func_t start, void *arg)
{
	/* TODO: PART 1 - Phase 2 */
	/* TODO: PART 2 - Phase 3 */
}

void uthread_mem_config(size_t npages)
{
	palloc_configure(npages);
}
