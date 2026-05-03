/*
** invoke_set.inl — Set method dispatch for OP_INVOKE.
** Included inside CASE(OP_INVOKE) when receiver is OBJ_SET.
**
** Available variables: base, arg_count, receiver, mname, mlen, args, R, K
*/

ObjSet *set = as_set(receiver);

switch (method->set_method_id)
{
case SET_ADD:
{
    /* set.add(val) → returns true if newly added */
    if (arg_count != 1) { runtime_error("add() expects 1 argument"); return; }
    R[base] = val_bool(set_add(&gc_, set, args[0]));
    break;
}
case SET_HAS:
{
    /* set.has(val) → bool */
    if (arg_count != 1) { runtime_error("has() expects 1 argument"); return; }
    R[base] = val_bool(set_contains(set, args[0]));
    break;
}
case SET_DELETE:
{
    /* set.delete(val) → returns true if was present */
    if (arg_count != 1) { runtime_error("delete() expects 1 argument"); return; }
    R[base] = val_bool(set_remove(set, args[0]));
    break;
}
case SET_SIZE:
{
    /* set.size() → number of elements */
    R[base] = val_int(set->count);
    break;
}
case SET_CLEAR:
{
    /* set.clear() → remove all elements */
    set_clear(&gc_, set);
    R[base] = val_nil();
    break;
}
case SET_VALUES:
{
    /* set.values() → array of all values */
    ObjArray *result = new_array(&gc_);
    for (int32_t si = 0; si < set->capacity; si++) {
        if (set->nodes[si].hash != 0xFFFFFFFFu) {
            array_push(&gc_, result, set->nodes[si].key);
        }
    }
    R[base] = val_obj((Obj *)result);
    break;
}
default:
{
    runtime_error("set has no method '%s'", mname);
    return;
}
}
