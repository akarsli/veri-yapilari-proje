#include "history.h"

void history_init(History *h)
{
    stack_init(&h->undo, sizeof(ClosureOp));
    stack_init(&h->redo, sizeof(ClosureOp));
}

void history_free(History *h)
{
    stack_free(&h->undo);
    stack_free(&h->redo);
}

ApplyResult history_apply(History *h, Network *net, int stop_a, int stop_b, int line, bool close)
{
    int status = network_segment_status(net, stop_a, stop_b, line);
    if (status < 0) return APPLY_NO_SEGMENT;
    if ((status == 1) == close) return APPLY_UNCHANGED;

    network_set_segment_closed(net, stop_a, stop_b, line, close);
    ClosureOp op = {.stop_a = stop_a, .stop_b = stop_b, .line = line, .closed = close};
    stack_push(&h->undo, &op);
    stack_clear(&h->redo);   /* yeni işlem: yinelenecek dal geçersiz */
    return APPLY_OK;
}

/* from stack'inin tepesindeki işlemi tersine (undo) veya aynen (redo)
 * uygulayıp to stack'ine taşır. */
static bool move_op(Stack *from, Stack *to, Network *net, bool invert, ClosureOp *done)
{
    ClosureOp op;
    if (!stack_pop(from, &op)) return false;
    bool closed = invert ? !op.closed : op.closed;
    network_set_segment_closed(net, op.stop_a, op.stop_b, op.line, closed);
    stack_push(to, &op);
    if (done) *done = op;
    return true;
}

bool history_undo(History *h, Network *net, ClosureOp *done)
{
    return move_op(&h->undo, &h->redo, net, true, done);
}

bool history_redo(History *h, Network *net, ClosureOp *done)
{
    return move_op(&h->redo, &h->undo, net, false, done);
}
